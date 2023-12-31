/* Implementation of GUI terminal on the Mac OS.
   Copyright (C) 2000, 2001, 2002, 2003, 2004,
                 2005, 2006, 2007, 2008 Free Software Foundation, Inc.

This file is part of GNU Emacs.

GNU Emacs is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3, or (at your option)
any later version.

GNU Emacs is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Emacs; see the file COPYING.  If not, write to
the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
Boston, MA 02110-1301, USA.  */

/* Contributed by Andrew Choi (akochoi@mac.com).  */

#include <config.h>
#include <signal.h>

#include <stdio.h>

#include "lisp.h"
#include "blockinput.h"

#include "macterm.h"

#ifndef MAC_OSX
#include <alloca.h>
#endif

#if !TARGET_API_MAC_CARBON
#include <Quickdraw.h>
#include <ToolUtils.h>
#include <Sound.h>
#include <Events.h>
#include <Script.h>
#include <Resources.h>
#include <Fonts.h>
#include <TextUtils.h>
#include <LowMem.h>
#include <Controls.h>
#include <Windows.h>
#include <Displays.h>
#if defined (__MRC__) || (__MSL__ >= 0x6000)
#include <ControlDefinitions.h>
#endif

#if __profile__
#include <profiler.h>
#endif
#endif /* not TARGET_API_MAC_CARBON */

#include "systty.h"
#include "systime.h"

#include <ctype.h>
#include <errno.h>
#include <setjmp.h>
#include <sys/stat.h>

#include "charset.h"
#include "coding.h"
#include "frame.h"
#include "dispextern.h"
#include "fontset.h"
#include "termhooks.h"
#include "termopts.h"
#include "termchar.h"
#include "disptab.h"
#include "buffer.h"
#include "window.h"
#include "keyboard.h"
#include "intervals.h"
#include "atimer.h"
#include "keymap.h"



/* Non-nil means Emacs uses toolkit scroll bars.  */

Lisp_Object Vx_toolkit_scroll_bars;

/* If non-zero, the text will be rendered using Core Graphics text
   rendering which may anti-alias the text.  */
int mac_use_core_graphics;

/* Non-zero means make use of UNDERLINE_POSITION font properties.
   (Not yet supported.)  */
int x_use_underline_position_properties;

/* Non-zero means to draw the underline at the same place as the descent line.  */

int x_underline_at_descent_line;

/* This is a chain of structures for all the X displays currently in
   use.  */

struct x_display_info *x_display_list;

/* This is a list of cons cells, each of the form (NAME
   FONT-LIST-CACHE . RESOURCE-DATABASE), one for each element of
   x_display_list and in the same order.  NAME is the name of the
   frame.  FONT-LIST-CACHE records previous values returned by
   x-list-fonts.  RESOURCE-DATABASE preserves the X Resource Database
   equivalent, which is implemented with a Lisp object, for the
   display. */

Lisp_Object x_display_name_list;

/* This is display since Mac does not support multiple ones.  */
struct mac_display_info one_mac_display_info;

/* Frame being updated by update_frame.  This is declared in term.c.
   This is set by update_begin and looked at by all the XT functions.
   It is zero while not inside an update.  In that case, the XT
   functions assume that `selected_frame' is the frame to apply to.  */

extern struct frame *updating_frame;

/* This is a frame waiting to be auto-raised, within XTread_socket.  */

struct frame *pending_autoraise_frame;

/* Mouse movement.

   Formerly, we used PointerMotionHintMask (in standard_event_mask)
   so that we would have to call XQueryPointer after each MotionNotify
   event to ask for another such event.  However, this made mouse tracking
   slow, and there was a bug that made it eventually stop.

   Simply asking for MotionNotify all the time seems to work better.

   In order to avoid asking for motion events and then throwing most
   of them away or busy-polling the server for mouse positions, we ask
   the server for pointer motion hints.  This means that we get only
   one event per group of mouse movements.  "Groups" are delimited by
   other kinds of events (focus changes and button clicks, for
   example), or by XQueryPointer calls; when one of these happens, we
   get another MotionNotify event the next time the mouse moves.  This
   is at least as efficient as getting motion events when mouse
   tracking is on, and I suspect only negligibly worse when tracking
   is off.  */

/* Where the mouse was last time we reported a mouse event.  */

Rect last_mouse_glyph;
FRAME_PTR last_mouse_glyph_frame;

/* The scroll bar in which the last X motion event occurred.

   If the last X motion event occurred in a scroll bar, we set this so
   XTmouse_position can know whether to report a scroll bar motion or
   an ordinary motion.

   If the last X motion event didn't occur in a scroll bar, we set
   this to Qnil, to tell XTmouse_position to return an ordinary motion
   event.  */

Lisp_Object last_mouse_scroll_bar;

/* This is a hack.  We would really prefer that XTmouse_position would
   return the time associated with the position it returns, but there
   doesn't seem to be any way to wrest the time-stamp from the server
   along with the position query.  So, we just keep track of the time
   of the last movement we received, and return that in hopes that
   it's somewhat accurate.  */

Time last_mouse_movement_time;

/* Incremented by XTread_socket whenever it really tries to read
   events.  */

#ifdef __STDC__
int volatile input_signal_count;
#else
int input_signal_count;
#endif

/* Non-nil means update doesn't reset vscroll.  */
int mac_redisplay_dont_reset_vscroll;

extern Lisp_Object Vsystem_name;

extern Lisp_Object Qeql;

/* A mask of extra modifier bits to put into every keyboard char.  */

extern EMACS_INT extra_keyboard_modifiers;

/* The keysyms to use for the various modifiers.  */

static Lisp_Object Qalt, Qhyper, Qsuper, Qcontrol, Qmeta, Qmodifier_value;

extern int inhibit_window_system;

#if __MRC__ && !TARGET_API_MAC_CARBON
QDGlobals qd;  /* QuickDraw global information structure.  */
#endif

struct mac_display_info *mac_display_info_for_display (Display *);
static void x_update_window_end P_ ((struct window *, int, int));
int x_catch_errors P_ ((Display *));
void x_uncatch_errors P_ ((Display *, int));
void x_lower_frame P_ ((struct frame *));
void x_scroll_bar_clear P_ ((struct frame *));
int x_had_errors_p P_ ((Display *));
void x_wm_set_size_hint P_ ((struct frame *, long, int));
void x_raise_frame P_ ((struct frame *));
void x_set_window_size P_ ((struct frame *, int, int, int));
void x_wm_set_window_state P_ ((struct frame *, int));
void x_wm_set_icon_pixmap P_ ((struct frame *, int));
void mac_initialize P_ ((void));
static void x_font_min_bounds P_ ((XFontStruct *, int *, int *));
static int x_compute_min_glyph_bounds P_ ((struct frame *));
static void x_update_end P_ ((struct frame *));
static void XTframe_up_to_date P_ ((struct frame *));
static void XTset_terminal_modes P_ ((void));
static void XTreset_terminal_modes P_ ((void));
static void x_clear_frame P_ ((void));
static void frame_highlight P_ ((struct frame *));
static void frame_unhighlight P_ ((struct frame *));
static void x_new_focus_frame P_ ((struct x_display_info *, struct frame *));
static void XTframe_rehighlight P_ ((struct frame *));
static void x_frame_rehighlight P_ ((struct x_display_info *));
static void x_draw_hollow_cursor P_ ((struct window *, struct glyph_row *));
static void x_draw_bar_cursor P_ ((struct window *, struct glyph_row *, int,
				   enum text_cursor_kinds));

static void x_clip_to_row P_ ((struct window *, struct glyph_row *, int, GC));
static void x_update_begin P_ ((struct frame *));
static void x_update_window_begin P_ ((struct window *));
static void x_after_update_window_line P_ ((struct glyph_row *));

static XCharStruct *mac_per_char_metric P_ ((XFontStruct *, XChar2b *, int));
static void XSetFont P_ ((Display *, GC, XFontStruct *));

#if !USE_APPKIT
extern void mac_toolbox_initialize P_ ((void));
extern void x_scroll_bar_report_motion P_ ((struct frame **, Lisp_Object *,
					    enum scroll_bar_part *,
					    Lisp_Object *, Lisp_Object *,
					    unsigned long *));
#if USE_CG_DRAWING
extern void mac_flush_display_optional P_ ((struct frame *));
#endif
#endif

#if USE_QUICKDRAW
#define GC_FORE_COLOR(gc)	(&(gc)->fore_color)
#define GC_BACK_COLOR(gc)	(&(gc)->back_color)
#elif !USE_MAC_IMAGE_IO
static RGBColor *
GC_FORE_COLOR (gc)
     GC gc;
{
  static RGBColor color;

  color.red = RED16_FROM_ULONG (gc->xgcv.foreground);
  color.green = GREEN16_FROM_ULONG (gc->xgcv.foreground);
  color.blue = BLUE16_FROM_ULONG (gc->xgcv.foreground);

  return &color;
}

static RGBColor *
GC_BACK_COLOR (gc)
     GC gc;
{
  static RGBColor color;

  color.red = RED16_FROM_ULONG (gc->xgcv.background);
  color.green = GREEN16_FROM_ULONG (gc->xgcv.background);
  color.blue = BLUE16_FROM_ULONG (gc->xgcv.background);

  return &color;
}
#endif
#define GC_FONT(gc)		((gc)->xgcv.font)
#define FRAME_NORMAL_GC(f)	((f)->output_data.mac->normal_gc)

#define CG_SET_FILL_COLOR(context, color)				\
  CGContextSetRGBFillColor (context,					\
			    RED_FROM_ULONG (color) / 255.0f,		\
			    GREEN_FROM_ULONG (color) / 255.0f,		\
			    BLUE_FROM_ULONG (color) / 255.0f, 1.0f)
#if USE_CG_DRAWING && MAC_OS_X_VERSION_MAX_ALLOWED >= 1030
#if MAC_OS_X_VERSION_MIN_REQUIRED == 1020
#define CG_SET_FILL_COLOR_MAYBE_WITH_CGCOLOR(context, color, cg_color) \
  do {								       \
    if (CGColorGetTypeID != NULL)				       \
      CGContextSetFillColorWithColor (context, cg_color);	       \
    else							       \
      CG_SET_FILL_COLOR (context, color);			       \
  } while (0)
#else
#define CG_SET_FILL_COLOR_MAYBE_WITH_CGCOLOR(context, color, cg_color)	\
  CGContextSetFillColorWithColor (context, cg_color)
#endif
#else
#define CG_SET_FILL_COLOR_MAYBE_WITH_CGCOLOR(context, color, cg_color)	\
  CG_SET_FILL_COLOR (context, color)
#endif
#define CG_SET_FILL_COLOR_WITH_GC_FOREGROUND(context, gc)		\
  CG_SET_FILL_COLOR_MAYBE_WITH_CGCOLOR (context, (gc)->xgcv.foreground,	\
					(gc)->cg_fore_color)
#define CG_SET_FILL_COLOR_WITH_GC_BACKGROUND(context, gc)		\
  CG_SET_FILL_COLOR_MAYBE_WITH_CGCOLOR (context, (gc)->xgcv.background,	\
					(gc)->cg_back_color)


#define CG_SET_STROKE_COLOR(context, color)				\
  CGContextSetRGBStrokeColor (context,					\
			      RED_FROM_ULONG (color) / 255.0f,		\
			      GREEN_FROM_ULONG (color) / 255.0f,	\
			      BLUE_FROM_ULONG (color) / 255.0f, 1.0f)
#if USE_CG_DRAWING && MAC_OS_X_VERSION_MAX_ALLOWED >= 1030
#if MAC_OS_X_VERSION_MIN_REQUIRED == 1020
#define CG_SET_STROKE_COLOR_MAYBE_WITH_CGCOLOR(context, color, cg_color) \
  do {								       \
    if (CGColorGetTypeID != NULL)				       \
      CGContextSetStrokeColorWithColor (context, cg_color);	       \
    else							       \
      CG_SET_STROKE_COLOR (context, color);			       \
  } while (0)
#else
#define CG_SET_STROKE_COLOR_MAYBE_WITH_CGCOLOR(context, color, cg_color) \
  CGContextSetStrokeColorWithColor (context, cg_color)
#endif
#else
#define CG_SET_STROKE_COLOR_MAYBE_WITH_CGCOLOR(context, color, cg_color) \
  CG_SET_STROKE_COLOR (context, color)
#endif
#define CG_SET_STROKE_COLOR_WITH_GC_FOREGROUND(context, gc) \
  CG_SET_STROKE_COLOR_MAYBE_WITH_CGCOLOR (context, (gc)->xgcv.foreground, \
					  (gc)->cg_fore_color)

#if USE_CG_DRAWING
/* Fringe bitmaps.  */

static int max_fringe_bmp = 0;
static CGImageRef *fringe_bmp = 0;

CGColorSpaceRef mac_cg_color_space_rgb;
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1030
static CGColorRef mac_cg_color_black;
#endif

static void
init_cg_color ()
{
  mac_cg_color_space_rgb = CGColorSpaceCreateDeviceRGB ();
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1030
#if MAC_OS_X_VERSION_MIN_REQUIRED == 1020
  /* Don't check the availability of CGColorCreate; this symbol is
     defined even in Mac OS X 10.1.  */
  if (CGColorGetTypeID != NULL)
#endif
    {
      CGFloat rgba[] = {0.0f, 0.0f, 0.0f, 1.0f};

      mac_cg_color_black = CGColorCreate (mac_cg_color_space_rgb, rgba);
    }
#endif
}
#endif	/* USE_CG_DRAWING */

/* X display function emulation */

#if !USE_APPKIT
/* Mac version of XDrawLine.  */

static void
mac_draw_line (f, gc, x1, y1, x2, y2)
     struct frame *f;
     GC gc;
     int x1, y1, x2, y2;
{
#if USE_CG_DRAWING
  CGContextRef context;
  CGFloat gx1 = x1, gy1 = y1, gx2 = x2, gy2 = y2;

  if (y1 != y2)
    gx1 += 0.5f, gx2 += 0.5f;
  if (x1 != x2)
    gy1 += 0.5f, gy2 += 0.5f;

  context = mac_begin_cg_clip (f, gc);
  CG_SET_STROKE_COLOR_WITH_GC_FOREGROUND (context, gc);
  CGContextBeginPath (context);
  CGContextMoveToPoint (context, gx1, gy1);
  CGContextAddLineToPoint (context, gx2, gy2);
  CGContextClosePath (context);
  CGContextStrokePath (context);
  mac_end_cg_clip (f);
#else
  if (x1 == x2)
    {
      if (y1 > y2)
	y1--;
      else if (y2 > y1)
	y2--;
    }
  else if (y1 == y2)
    {
      if (x1 > x2)
	x1--;
      else
	x2--;
    }

  mac_begin_clip (f, gc);
  RGBForeColor (GC_FORE_COLOR (gc));
  MoveTo (x1, y1);
  LineTo (x2, y2);
  mac_end_clip (f, gc);
#endif
}
#endif

/* Mac version of XDrawLine (to Pixmap).  */

void
XDrawLine (display, p, gc, x1, y1, x2, y2)
     Display *display;
     Pixmap p;
     GC gc;
     int x1, y1, x2, y2;
{
#if USE_MAC_IMAGE_IO
  CGContextRef context;
  XImagePtr ximg = p;
  CGColorSpaceRef color_space;
  CGImageAlphaInfo alpha_info;
  CGFloat gx1 = x1, gy1 = y1, gx2 = x2, gy2 = y2;

  if (y1 != y2)
    gx1 += 0.5f, gx2 += 0.5f;
  if (x1 != x2)
    gy1 += 0.5f, gy2 += 0.5f;

  if (ximg->bits_per_pixel == 32)
    {
      color_space = mac_cg_color_space_rgb;
      alpha_info = kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Host;
    }
  else
    {
      color_space = NULL;
      alpha_info = kCGImageAlphaOnly;
    }
  context = CGBitmapContextCreate (ximg->data, ximg->width, ximg->height, 8,
				   ximg->bytes_per_line, color_space,
				   alpha_info);
  if (ximg->bits_per_pixel == 32)
    CG_SET_STROKE_COLOR_WITH_GC_FOREGROUND (context, gc);
  else
    CGContextSetGrayStrokeColor (context, gc->xgcv.foreground / 255.0f, 1.0);
  CGContextMoveToPoint (context, gx1, gy1);
  CGContextAddLineToPoint (context, gx2, gy2);
  CGContextClosePath (context);
  CGContextStrokePath (context);
  CGContextRelease (context);
#else
  CGrafPtr old_port;
  GDHandle old_gdh;

  if (x1 == x2)
    {
      if (y1 > y2)
	y1--;
      else if (y2 > y1)
	y2--;
    }
  else if (y1 == y2)
    {
      if (x1 > x2)
	x1--;
      else
	x2--;
    }

  GetGWorld (&old_port, &old_gdh);
  SetGWorld (p, NULL);

  RGBForeColor (GC_FORE_COLOR (gc));

  LockPixels (GetGWorldPixMap (p));
  MoveTo (x1, y1);
  LineTo (x2, y2);
  UnlockPixels (GetGWorldPixMap (p));

  SetGWorld (old_port, old_gdh);
#endif
}


static void
mac_erase_rectangle (f, gc, x, y, width, height)
     struct frame *f;
     GC gc;
     int x, y;
     unsigned int width, height;
{
#if USE_CG_DRAWING
  CGContextRef context;

  context = mac_begin_cg_clip (f, gc);
  CG_SET_FILL_COLOR_WITH_GC_BACKGROUND (context, gc);
  CGContextFillRect (context, mac_rect_make (f, x, y, width, height));
  mac_end_cg_clip (f);
#else
  Rect r;

  mac_begin_clip (f, gc);
  RGBBackColor (GC_BACK_COLOR (gc));
  SetRect (&r, x, y, x + width, y + height);
  EraseRect (&r);
  RGBBackColor (GC_BACK_COLOR (FRAME_NORMAL_GC (f)));
  mac_end_clip (f, gc);
#endif
}


/* Mac version of XClearArea.  */

void
mac_clear_area (f, x, y, width, height)
     struct frame *f;
     int x, y;
     unsigned int width, height;
{
  mac_erase_rectangle (f, FRAME_NORMAL_GC (f), x, y, width, height);
}

/* Mac version of XClearWindow.  */

static void
mac_clear_window (f)
     struct frame *f;
{
#if USE_CG_DRAWING
  CGContextRef context;
  GC gc = FRAME_NORMAL_GC (f);

  context = mac_begin_cg_clip (f, NULL);
  CG_SET_FILL_COLOR_WITH_GC_BACKGROUND (context, gc);
  CGContextFillRect (context, CGRectMake (0, 0, FRAME_PIXEL_WIDTH (f),
					  FRAME_PIXEL_HEIGHT (f)));
  mac_end_cg_clip (f);
#else  /* !USE_CG_DRAWING */
  mac_begin_clip (f, NULL);
  RGBBackColor (GC_BACK_COLOR (FRAME_NORMAL_GC (f)));
#if TARGET_API_MAC_CARBON
  {
    Rect r;

    GetWindowPortBounds (FRAME_MAC_WINDOW (f), &r);
    EraseRect (&r);
  }
#else /* not TARGET_API_MAC_CARBON */
  EraseRect (&(FRAME_MAC_WINDOW (f)->portRect));
#endif /* not TARGET_API_MAC_CARBON */
  mac_end_clip (f, NULL);
#endif
}


/* Mac replacement for XCopyArea.  */

#if USE_CG_DRAWING
static void
mac_draw_cg_image (image, f, gc, src_x, src_y, width, height,
		   dest_x, dest_y, overlay_p)
     CGImageRef image;
     struct frame *f;
     GC gc;
     int src_x, src_y;
     unsigned int width, height;
     int dest_x, dest_y, overlay_p;
{
  CGContextRef context;
  CGRect dest_rect, bounds;

  context = mac_begin_cg_clip (f, gc);
  dest_rect = mac_rect_make (f, dest_x, dest_y, width, height);
  bounds = mac_rect_make (f, dest_x - src_x, dest_y - src_y,
			  CGImageGetWidth (image), CGImageGetHeight (image));
  if (!overlay_p)
    {
      CG_SET_FILL_COLOR_WITH_GC_BACKGROUND (context, gc);
      CGContextFillRect (context, dest_rect);
    }
  CGContextClipToRect (context, dest_rect);
  CGContextTranslateCTM (context,
			 CGRectGetMinX (bounds), CGRectGetMaxY (bounds));
  CGContextScaleCTM (context, 1, -1);
  if (CGImageIsMask (image))
    CG_SET_FILL_COLOR_WITH_GC_FOREGROUND (context, gc);
  bounds.origin = CGPointZero;
  CGContextDrawImage (context, bounds, image);
  mac_end_cg_clip (f);
}

#else  /* !USE_CG_DRAWING */

static void
mac_draw_bitmap (f, gc, x, y, width, height, bits, overlay_p)
     struct frame *f;
     GC gc;
     int x, y, width, height;
     unsigned short *bits;
     int overlay_p;
{
  BitMap bitmap;
  Rect r;

  bitmap.rowBytes = sizeof(unsigned short);
  bitmap.baseAddr = (char *)bits;
  SetRect (&(bitmap.bounds), 0, 0, width, height);

  mac_begin_clip (f, gc);
  RGBForeColor (GC_FORE_COLOR (gc));
  RGBBackColor (GC_BACK_COLOR (gc));
  SetRect (&r, x, y, x + width, y + height);
#if TARGET_API_MAC_CARBON
  {
    CGrafPtr port;

    GetPort (&port);
    LockPortBits (port);
    CopyBits (&bitmap, GetPortBitMapForCopyBits (port),
	      &(bitmap.bounds), &r, overlay_p ? srcOr : srcCopy, 0);
    UnlockPortBits (port);
  }
#else /* not TARGET_API_MAC_CARBON */
  CopyBits (&bitmap, &(FRAME_MAC_WINDOW (f)->portBits), &(bitmap.bounds), &r,
	    overlay_p ? srcOr : srcCopy, 0);
#endif /* not TARGET_API_MAC_CARBON */
  RGBBackColor (GC_BACK_COLOR (FRAME_NORMAL_GC (f)));
  mac_end_clip (f, gc);
}
#endif	/* !USE_CG_DRAWING */


/* Mac replacement for XCreateBitmapFromBitmapData.  */

static void
mac_create_bitmap_from_bitmap_data (bitmap, bits, w, h)
     BitMap *bitmap;
     char *bits;
     int w, h;
{
  static const unsigned char swap_nibble[16]
    = { 0x0, 0x8, 0x4, 0xc,    /* 0000 1000 0100 1100 */
	0x2, 0xa, 0x6, 0xe,    /* 0010 1010 0110 1110 */
	0x1, 0x9, 0x5, 0xd,    /* 0001 1001 0101 1101 */
	0x3, 0xb, 0x7, 0xf };  /* 0011 1011 0111 1111 */
  int i, j, w1;
  char *p;

  w1 = (w + 7) / 8;         /* nb of 8bits elt in X bitmap */
  bitmap->rowBytes = ((w + 15) / 16) * 2; /* nb of 16bits elt in Mac bitmap */
  bitmap->baseAddr = xmalloc (bitmap->rowBytes * h);
  bzero (bitmap->baseAddr, bitmap->rowBytes * h);
  for (i = 0; i < h; i++)
    {
      p = bitmap->baseAddr + i * bitmap->rowBytes;
      for (j = 0; j < w1; j++)
	{
	  /* Bitswap XBM bytes to match how Mac does things.  */
	  unsigned char c = *bits++;
	  *p++ = (unsigned char)((swap_nibble[c & 0xf] << 4)
				 | (swap_nibble[(c>>4) & 0xf]));
	}
    }

  SetRect (&(bitmap->bounds), 0, 0, w, h);
}


static void
mac_free_bitmap (bitmap)
     BitMap *bitmap;
{
  xfree (bitmap->baseAddr);
}


Pixmap
XCreatePixmap (display, w, width, height, depth)
     Display *display;
     Window w;
     unsigned int width, height;
     unsigned int depth;
{
#if USE_MAC_IMAGE_IO
  XImagePtr ximg;

  ximg = xmalloc (sizeof (*ximg));
  ximg->width = width;
  ximg->height = height;
  ximg->bits_per_pixel = depth == 1 ? 8 : 32;
  ximg->bytes_per_line = width * (ximg->bits_per_pixel / 8);
  ximg->data = xmalloc (ximg->bytes_per_line * height);
  return ximg;
#else
  Pixmap pixmap;
  Rect r;
  QDErr err;

#ifdef MAC_OS8
  SetPortWindowPort (w);
#endif
  SetRect (&r, 0, 0, width, height);
#if !defined (WORDS_BIG_ENDIAN) && USE_CG_DRAWING
  if (depth == 1)
#endif
    err = NewGWorld (&pixmap, depth, &r, NULL, NULL, 0);
#if !defined (WORDS_BIG_ENDIAN) && USE_CG_DRAWING
  else
    /* CreateCGImageFromPixMaps requires ARGB format.  */
    err = QTNewGWorld (&pixmap, k32ARGBPixelFormat, &r, NULL, NULL, 0);
#endif
  if (err != noErr)
    return NULL;
  return pixmap;
#endif
}


Pixmap
XCreatePixmapFromBitmapData (display, w, data, width, height, fg, bg, depth)
     Display *display;
     Window w;
     char *data;
     unsigned int width, height;
     unsigned long fg, bg;
     unsigned int depth;
{
  Pixmap pixmap;
  BitMap bitmap;
#if USE_MAC_IMAGE_IO
  CGDataProviderRef provider;
  CGImageRef image_mask;
  CGContextRef context;

  pixmap = XCreatePixmap (display, w, width, height, depth);
  if (pixmap == NULL)
    return NULL;

  mac_create_bitmap_from_bitmap_data (&bitmap, data, width, height);
  provider = CGDataProviderCreateWithData (NULL, bitmap.baseAddr,
					   bitmap.rowBytes * height, NULL);
  image_mask = CGImageMaskCreate (width, height, 1, 1, bitmap.rowBytes,
				  provider, NULL, 0);
  CGDataProviderRelease (provider);

  context = CGBitmapContextCreate (pixmap->data, width, height, 8,
				   pixmap->bytes_per_line,
				   mac_cg_color_space_rgb,
				   kCGImageAlphaNoneSkipFirst
				   | kCGBitmapByteOrder32Host);

  CG_SET_FILL_COLOR (context, fg);
  CGContextFillRect (context, CGRectMake (0, 0, width, height));
  CG_SET_FILL_COLOR (context, bg);
  CGContextDrawImage (context, CGRectMake (0, 0, width, height), image_mask);
  CGContextRelease (context);
  CGImageRelease (image_mask);
#else
  CGrafPtr old_port;
  GDHandle old_gdh;
  static GC gc = NULL;

  if (gc == NULL)
    gc = XCreateGC (display, w, 0, NULL);

  pixmap = XCreatePixmap (display, w, width, height, depth);
  if (pixmap == NULL)
    return NULL;

  GetGWorld (&old_port, &old_gdh);
  SetGWorld (pixmap, NULL);
  mac_create_bitmap_from_bitmap_data (&bitmap, data, width, height);
  XSetForeground (display, gc, fg);
  XSetBackground (display, gc, bg);
  RGBForeColor (GC_FORE_COLOR (gc));
  RGBBackColor (GC_BACK_COLOR (gc));
  LockPixels (GetGWorldPixMap (pixmap));
#if TARGET_API_MAC_CARBON
  CopyBits (&bitmap, GetPortBitMapForCopyBits (pixmap),
	    &bitmap.bounds, &bitmap.bounds, srcCopy, 0);
#else /* not TARGET_API_MAC_CARBON */
  CopyBits (&bitmap, &(((GrafPtr)pixmap)->portBits),
	    &bitmap.bounds, &bitmap.bounds, srcCopy, 0);
#endif /* not TARGET_API_MAC_CARBON */
  UnlockPixels (GetGWorldPixMap (pixmap));
  SetGWorld (old_port, old_gdh);
#endif
  mac_free_bitmap (&bitmap);

  return pixmap;
}


void
XFreePixmap (display, pixmap)
     Display *display;
     Pixmap pixmap;
{
#if USE_MAC_IMAGE_IO
  if (pixmap)
    {
      if (pixmap->data)
	xfree (pixmap->data);
      xfree (pixmap);
    }
#else
  DisposeGWorld (pixmap);
#endif
}


/* Mac replacement for XFillRectangle.  */

static void
mac_fill_rectangle (f, gc, x, y, width, height)
     struct frame *f;
     GC gc;
     int x, y;
     unsigned int width, height;
{
#if USE_CG_DRAWING
  CGContextRef context;

  context = mac_begin_cg_clip (f, gc);
  CG_SET_FILL_COLOR_WITH_GC_FOREGROUND (context, gc);
  CGContextFillRect (context, mac_rect_make (f, x, y, width, height));
  mac_end_cg_clip (f);
#else
  Rect r;

  mac_begin_clip (f, gc);
  RGBForeColor (GC_FORE_COLOR (gc));
  SetRect (&r, x, y, x + width, y + height);
  PaintRect (&r); /* using foreground color of gc */
  mac_end_clip (f, gc);
#endif
}


/* Mac replacement for XDrawRectangle: dest is a window.  */

static void
mac_draw_rectangle (f, gc, x, y, width, height)
     struct frame *f;
     GC gc;
     int x, y;
     unsigned int width, height;
{
#if USE_CG_DRAWING
  CGContextRef context;
  CGRect rect;

  context = mac_begin_cg_clip (f, gc);
  CG_SET_STROKE_COLOR_WITH_GC_FOREGROUND (context, gc);
  rect = mac_rect_make (f, x, y, width + 1, height + 1);
  CGContextStrokeRect (context, CGRectInset (rect, 0.5f, 0.5f));
  mac_end_cg_clip (f);
#else
  Rect r;

  mac_begin_clip (f, gc);
  RGBForeColor (GC_FORE_COLOR (gc));
  SetRect (&r, x, y, x + width + 1, y + height + 1);
  FrameRect (&r); /* using foreground color of gc */
  mac_end_clip (f, gc);
#endif
}


#if USE_APPKIT
static void
mac_fill_trapezoid_for_relief (f, gc, x, y, width, height, top_p)
     struct frame *f;
     GC gc;
     int x, y;
     unsigned int width, height;
     int top_p;
{
  CGContextRef context;
  CGRect rect = mac_rect_make (f, x, y, width, height);
  CGPoint points[4];

  points[0].x = points[1].x = CGRectGetMinX (rect);
  points[2].x = points[3].x = CGRectGetMaxX (rect);
  points[0].y = points[3].y = CGRectGetMinY (rect);
  points[1].y = points[2].y = CGRectGetMaxY (rect);

  if (top_p)
    points[2].x -= CGRectGetHeight (rect);
  else
    points[0].x += CGRectGetHeight (rect);

  context = mac_begin_cg_clip (f, gc);
  CG_SET_FILL_COLOR_WITH_GC_FOREGROUND (context, gc);
  CGContextBeginPath (context);
  CGContextAddLines (context, points, 4);
  CGContextClosePath (context);
  CGContextFillPath (context);
  mac_end_cg_clip (f);
}
#endif


static void
mac_invert_rectangle (f, x, y, width, height)
     struct frame *f;
     int x, y;
     unsigned int width, height;
{
#if USE_CG_DRAWING && MAC_OS_X_VERSION_MAX_ALLOWED >= 1040
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1040 && MAC_OS_X_VERSION_MIN_REQUIRED >= 1020
  if (CGContextSetBlendMode != NULL)
#endif
    {
      CGContextRef context;

      context = mac_begin_cg_clip (f, NULL);
      CGContextSetRGBFillColor (context, 1.0f, 1.0f, 1.0f, 1.0f);
      CGContextSetBlendMode (context, kCGBlendModeDifference);
      CGContextFillRect (context, mac_rect_make (f, x, y, width, height));
      mac_end_cg_clip (f);
    }
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1040 && MAC_OS_X_VERSION_MIN_REQUIRED >= 1020
  else				/* CGContextSetBlendMode == NULL */
#endif
#endif	/* USE_CG_DRAWING && MAC_OS_X_VERSION_MAX_ALLOWED >= 1040 */
#if !USE_CG_DRAWING || MAC_OS_X_VERSION_MAX_ALLOWED < 1040 || (MAC_OS_X_VERSION_MIN_REQUIRED < 1040 && MAC_OS_X_VERSION_MIN_REQUIRED >= 1020)
    {
#if USE_QUICKDRAW
      Rect r;

      mac_begin_clip (f, NULL);
      SetRect (&r, x, y, x + width, y + height);
      InvertRect (&r);
      mac_end_clip (f, NULL);
#elif USE_APPKIT
      extern void mac_appkit_invert_rectangle P_ ((struct frame *, int, int,
						   unsigned int, unsigned int));

      mac_appkit_invert_rectangle (f, x, y, width, height);
#endif
    }
#endif
}

/* Invert rectangles RECTANGLES[0], ..., RECTANGLES[N-1] in the frame F,
   excluding scroll bar area.  */

static void
mac_invert_rectangles (f, rectangles, n)
     struct frame *f;
     const Rect *rectangles;
     int n;
{
  int i;

  for (i = 0; i < n; i++)
    mac_invert_rectangle (f, rectangles[i].left, rectangles[i].top,
			  rectangles[i].right - rectangles[i].left,
			  rectangles[i].bottom - rectangles[i].top);
  if (FRAME_HAS_VERTICAL_SCROLL_BARS (f))
    {
      Lisp_Object bar;

      for (bar = FRAME_SCROLL_BARS (f); !NILP (bar);
	   bar = XSCROLL_BAR (bar)->next)
	{
	  struct scroll_bar *b = XSCROLL_BAR (bar);
	  Rect bar_rect, r;

	  SetRect (&bar_rect, XINT (b->left), XINT (b->top),
		   XINT (b->left) + XINT (b->width),
		   XINT (b->top) + XINT (b->height));
	  for (i = 0; i < n; i++)
	    if (SectRect (rectangles + i, &bar_rect, &r))
	      mac_invert_rectangle (f, r.left, r.top,
				    r.right - r.left, r.bottom - r.top);
	}
    }
}


#if USE_ATSUI
static OSStatus
atsu_get_text_layout_with_text_ptr (text, text_length, style, text_layout)
     ConstUniCharArrayPtr text;
     UniCharCount text_length;
     ATSUStyle style;
     ATSUTextLayout *text_layout;
{
  OSStatus err;
  static ATSUTextLayout saved_text_layout = NULL;

  if (saved_text_layout == NULL)
    {
      static const UniCharCount lengths[] = {kATSUToTextEnd};
      static const ATSUAttributeTag tags[] = {kATSULineLayoutOptionsTag};
      static const ByteCount sizes[] = {sizeof (ATSLineLayoutOptions)};
      static ATSLineLayoutOptions line_layout =
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1020
	kATSLineDisableAllLayoutOperations | kATSLineUseDeviceMetrics
	| kATSLineUseQDRendering
#else
	kATSLineIsDisplayOnly | kATSLineFractDisable
#endif
	;
      static const ATSUAttributeValuePtr values[] = {&line_layout};

      err = ATSUCreateTextLayoutWithTextPtr (text,
					     kATSUFromTextBeginning,
					     kATSUToTextEnd,
					     text_length,
					     1, lengths, &style,
					     &saved_text_layout);
      if (err == noErr)
	err = ATSUSetLayoutControls (saved_text_layout,
				     sizeof (tags) / sizeof (tags[0]),
				     tags, sizes, values);
      if (err == noErr)
	err = ATSUSetTransientFontMatching (saved_text_layout, true);
    }
  else
    {
      err = ATSUSetRunStyle (saved_text_layout, style,
			     kATSUFromTextBeginning, kATSUToTextEnd);
      if (err == noErr)
	err = ATSUSetTextPointerLocation (saved_text_layout, text,
					  kATSUFromTextBeginning,
					  kATSUToTextEnd,
					  text_length);
    }

  if (err == noErr)
    *text_layout = saved_text_layout;
  return err;
}


static void
mac_draw_image_string_atsui (f, gc, x, y, buf, nchars, bg_width,
			     overstrike_p, bytes_per_char)
     struct frame *f;
     GC gc;
     int x, y;
     char *buf;
     int nchars, bg_width, overstrike_p, bytes_per_char;
{
  OSStatus err;
  ATSUTextLayout text_layout;

  xassert (bytes_per_char == 2);

#ifndef WORDS_BIG_ENDIAN
  {
    int i;
    UniChar *text = (UniChar *)buf;

    for (i = 0; i < nchars; i++)
      text[i] = EndianU16_BtoN (text[i]);
  }
#endif
  err = atsu_get_text_layout_with_text_ptr ((ConstUniCharArrayPtr)buf,
					    nchars,
					    GC_FONT (gc)->mac_style,
					    &text_layout);
  if (err != noErr)
    return;

#if USE_QUICKDRAW
#ifdef MAC_OSX
  if (!mac_use_core_graphics)
    {
#endif
      mac_begin_clip (f, gc);
      RGBForeColor (GC_FORE_COLOR (gc));
      if (bg_width)
	{
	  Rect r;

	  SetRect (&r, x, y - FONT_BASE (GC_FONT (gc)),
		   x + bg_width, y + FONT_DESCENT (GC_FONT (gc)));
	  RGBBackColor (GC_BACK_COLOR (gc));
	  EraseRect (&r);
	  RGBBackColor (GC_BACK_COLOR (FRAME_NORMAL_GC (f)));
	}
      MoveTo (x, y);
      ATSUDrawText (text_layout,
		    kATSUFromTextBeginning, kATSUToTextEnd,
		    kATSUUseGrafPortPenLoc, kATSUUseGrafPortPenLoc);
      if (overstrike_p)
	{
	  MoveTo (x + 1, y);
	  ATSUDrawText (text_layout,
			kATSUFromTextBeginning, kATSUToTextEnd,
			kATSUUseGrafPortPenLoc, kATSUUseGrafPortPenLoc);
	}
      mac_end_clip (f, gc);
#ifdef MAC_OSX
    }
  else
#endif
#endif	/* USE_QUICKDRAW */
#ifdef MAC_OSX
    {
      static CGContextRef context;
      static const ATSUAttributeTag tags[] = {kATSUCGContextTag};
      static const ByteCount sizes[] = {sizeof (CGContextRef)};
      static const ATSUAttributeValuePtr values[] = {&context};

#if USE_CG_DRAWING
      context = mac_begin_cg_clip (f, gc);
#else
      CGrafPtr port;

      GetPort (&port);
      QDBeginCGContext (port, &context);
      CGContextTranslateCTM (context, 0, FRAME_PIXEL_HEIGHT (f));
      if (gc->n_clip_rects || bg_width)
	{
	  CGContextScaleCTM (context, 1, -1);
	  if (gc->n_clip_rects)
	    CGContextClipToRects (context, gc->clip_rects,
				  gc->n_clip_rects);
#endif
	  if (bg_width)
	    {
	      CG_SET_FILL_COLOR_WITH_GC_BACKGROUND (context, gc);
	      CGContextFillRect	(context,
				 mac_rect_make (f,
						x, y - FONT_BASE (GC_FONT (gc)),
						bg_width,
						FONT_HEIGHT (GC_FONT (gc))));
	    }
	  CGContextScaleCTM (context, 1, -1);
#if !USE_CG_DRAWING
	}
#endif
      CG_SET_FILL_COLOR_WITH_GC_FOREGROUND (context, gc);
      err = ATSUSetLayoutControls (text_layout,
				   sizeof (tags) / sizeof (tags[0]),
				   tags, sizes, values);
      if (err == noErr)
	{
	  ATSUDrawText (text_layout,
			kATSUFromTextBeginning, kATSUToTextEnd,
			Long2Fix (x), Long2Fix (-y));
	  if (overstrike_p)
	    ATSUDrawText (text_layout,
			  kATSUFromTextBeginning, kATSUToTextEnd,
			  Long2Fix (x + 1), Long2Fix (-y));
	}
#if USE_CG_DRAWING
      mac_end_cg_clip (f);
      context = NULL;
#else
      CGContextSynchronize (context);
      QDEndCGContext (port, &context);
#endif
#if 0
      /* This doesn't work on Mac OS X 10.1.  */
      ATSUClearLayoutControls (text_layout,
			       sizeof (tags) / sizeof (tags[0]), tags);
#else
      ATSUSetLayoutControls (text_layout,
			     sizeof (tags) / sizeof (tags[0]),
			     tags, sizes, values);
#endif
    }
#endif	/* MAC_OSX */
}
#endif	/* USE_ATSUI */


#if USE_QUICKDRAW
static void
mac_draw_image_string_qd (f, gc, x, y, buf, nchars, bg_width,
			  overstrike_p, bytes_per_char)
     struct frame *f;
     GC gc;
     int x, y;
     char *buf;
     int nchars, bg_width, overstrike_p, bytes_per_char;
{
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1020
  UInt32 savedFlags;
#endif

  mac_begin_clip (f, gc);
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1020
  if (mac_use_core_graphics)
    savedFlags = SwapQDTextFlags (kQDUseCGTextRendering);
#endif
  RGBForeColor (GC_FORE_COLOR (gc));
#ifdef MAC_OS8
  if (bg_width)
    {
      RGBBackColor (GC_BACK_COLOR (gc));
      TextMode (srcCopy);
    }
  else
    TextMode (srcOr);
#else
  /* We prefer not to use srcCopy text transfer mode on Mac OS X
     because:
     - Screen is double-buffered.  (In srcCopy mode, a text is drawn
       into an offscreen graphics world first.  So performance gain
       cannot be expected.)
     - It lowers rendering quality.
     - Some fonts leave garbage on cursor movement.  */
  if (bg_width)
    {
      Rect r;

      RGBBackColor (GC_BACK_COLOR (gc));
      SetRect (&r, x, y - FONT_BASE (GC_FONT (gc)),
	       x + bg_width, y + FONT_DESCENT (GC_FONT (gc)));
      EraseRect (&r);
    }
  TextMode (srcOr);
#endif
  TextFont (GC_FONT (gc)->mac_fontnum);
  TextSize (GC_FONT (gc)->mac_fontsize);
  TextFace (GC_FONT (gc)->mac_fontface);
  MoveTo (x, y);
  DrawText (buf, 0, nchars * bytes_per_char);
  if (overstrike_p)
    {
      TextMode (srcOr);
      MoveTo (x + 1, y);
      DrawText (buf, 0, nchars * bytes_per_char);
    }
  if (bg_width)
    RGBBackColor (GC_BACK_COLOR (FRAME_NORMAL_GC (f)));
  mac_end_clip (f, gc);

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1020
  if (mac_use_core_graphics)
    SwapQDTextFlags(savedFlags);
#endif
}
#endif	/* USE_QUICKDRAW */


static INLINE void
mac_draw_string_common (f, gc, x, y, buf, nchars, bg_width,
			overstrike_p, bytes_per_char)
     struct frame *f;
     GC gc;
     int x, y;
     char *buf;
     int nchars, bg_width, overstrike_p, bytes_per_char;
{
#if USE_ATSUI
  if (GC_FONT (gc)->mac_style)
    mac_draw_image_string_atsui (f, gc, x, y, buf, nchars, bg_width,
				 overstrike_p, bytes_per_char);
  else
#endif	/* USE_ATSUI */
    {
#if !USE_QUICKDRAW
      abort ();
#else  /* USE_QUICKDRAW */
      mac_draw_image_string_qd (f, gc, x, y, buf, nchars, bg_width,
				overstrike_p, bytes_per_char);
#endif	/* USE_QUICKDRAW */
    }
}


/* Mac replacement for XDrawImageString.  */

static void
mac_draw_image_string (f, gc, x, y, buf, nchars, bg_width, overstrike_p)
     struct frame *f;
     GC gc;
     int x, y;
     char *buf;
     int nchars, bg_width, overstrike_p;
{
  mac_draw_string_common (f, gc, x, y, buf, nchars, bg_width,
			  overstrike_p, 1);
}


/* Mac replacement for XDrawImageString16.  */

static void
mac_draw_image_string_16 (f, gc, x, y, buf, nchars, bg_width, overstrike_p)
     struct frame *f;
     GC gc;
     int x, y;
     XChar2b *buf;
     int nchars, bg_width, overstrike_p;
{
  mac_draw_string_common (f, gc, x, y, (char *) buf, nchars, bg_width,
			  overstrike_p, 2);
}


/* Mac replacement for XQueryTextExtents, but takes a character.  If
   STYLE is NULL, measurement is done by QuickDraw Text routines for
   the font of the current graphics port.  If CG_GLYPH is not NULL,
   *CG_GLYPH is set to the glyph ID or 0 if it cannot be obtained.  */

static OSStatus
mac_query_char_extents (style, c,
			font_ascent_return, font_descent_return,
			overall_return, cg_glyph)
#if USE_ATSUI
     ATSUStyle style;
#else
     void *style;
#endif
     int c;
     int *font_ascent_return, *font_descent_return;
     XCharStruct *overall_return;
#if USE_CG_TEXT_DRAWING
     CGGlyph *cg_glyph;
#else
     void *cg_glyph;
#endif
{
  OSStatus err = noErr;
  int width;
  Rect char_bounds;

#if USE_ATSUI
  if (style)
    {
      ATSUTextLayout text_layout;
      UniChar ch = c;

      err = atsu_get_text_layout_with_text_ptr (&ch, 1, style, &text_layout);
      if (err == noErr
	  && (font_ascent_return || font_descent_return || overall_return))
	{
	  ATSTrapezoid glyph_bounds;

	  err = ATSUGetGlyphBounds (text_layout, 0, 0,
				    kATSUFromTextBeginning, kATSUToTextEnd,
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1020
				    kATSUseFractionalOrigins,
#else
				    kATSUseDeviceOrigins,
#endif
				    1, &glyph_bounds, NULL);
	  if (err == noErr)
	    {
	      xassert (glyph_bounds.lowerRight.x - glyph_bounds.lowerLeft.x
		       == glyph_bounds.upperRight.x - glyph_bounds.upperLeft.x);

	      width = Fix2Long (glyph_bounds.upperRight.x
				- glyph_bounds.upperLeft.x);
	      if (font_ascent_return)
		*font_ascent_return = -Fix2Long (glyph_bounds.upperLeft.y);
	      if (font_descent_return)
		*font_descent_return = Fix2Long (glyph_bounds.lowerLeft.y);
	    }
	}
      if (err == noErr && overall_return)
	{
	  err = ATSUMeasureTextImage (text_layout,
				      kATSUFromTextBeginning, kATSUToTextEnd,
				      0, 0, &char_bounds);
	  if (err == noErr)
	    STORE_XCHARSTRUCT (*overall_return, width, char_bounds);
#if USE_CG_TEXT_DRAWING
	  if (err == noErr && cg_glyph)
	    {
	      OSStatus err1;
	      ATSUGlyphInfoArray glyph_info_array;
	      ByteCount count = sizeof (ATSUGlyphInfoArray);

	      err1 = ATSUMatchFontsToText (text_layout, kATSUFromTextBeginning,
					   kATSUToTextEnd, NULL, NULL, NULL);
	      if (err1 == noErr)
		err1 = ATSUGetGlyphInfo (text_layout, kATSUFromTextBeginning,
					 kATSUToTextEnd, &count,
					 &glyph_info_array);
	      if (err1 == noErr
		  /* Make sure that we don't have to make layout
		     adjustments.  */
		  && glyph_info_array.glyphs[0].deltaY == 0.0f
		  && glyph_info_array.glyphs[0].idealX == 0.0f
		  && glyph_info_array.glyphs[0].screenX == 0)
		{
		  xassert (glyph_info_array.glyphs[0].glyphID);
		  *cg_glyph = glyph_info_array.glyphs[0].glyphID;
		}
	      else
		*cg_glyph = 0;
	    }
#endif
	}
    }
  else
#endif
    {
#if !USE_QUICKDRAW
      abort ();
#else  /* USE_QUICKDRAW */
      if (font_ascent_return || font_descent_return)
	{
	  FontInfo font_info;

	  GetFontInfo (&font_info);
	  if (font_ascent_return)
	    *font_ascent_return = font_info.ascent;
	  if (font_descent_return)
	    *font_descent_return = font_info.descent;
	}
      if (overall_return)
	{
	  char ch = c;

	  width = CharWidth (ch);
	  QDTextBounds (1, &ch, &char_bounds);
	  STORE_XCHARSTRUCT (*overall_return, width, char_bounds);
	}
#endif   /* USE_QUICKDRAW */
    }

  return err;
}


/* Mac replacement for XTextExtents16.  Only sets horizontal metrics.  */

static int
mac_text_extents_16 (font_struct, string, nchars, overall_return)
     XFontStruct *font_struct;
     XChar2b *string;
     int nchars;
     XCharStruct *overall_return;
{
  int i;
  short width = 0, lbearing = 0, rbearing = 0;
  XCharStruct *pcm;

  for (i = 0; i < nchars; i++)
    {
      pcm = mac_per_char_metric (font_struct, string, 0);
      if (pcm == NULL)
	width += FONT_WIDTH (font_struct);
      else
	{
	  lbearing = min (lbearing, width + pcm->lbearing);
	  rbearing = max (rbearing, width + pcm->rbearing);
	  width += pcm->width;
	}
      string++;
    }

  overall_return->lbearing = lbearing;
  overall_return->rbearing = rbearing;
  overall_return->width = width;

  /* What's the meaning of the return value of XTextExtents16?  */
}


#if USE_CG_TEXT_DRAWING
static int cg_text_anti_aliasing_threshold = 8;

static void
init_cg_text_anti_aliasing_threshold ()
{
  int threshold;
  Boolean valid_p;

  threshold =
    CFPreferencesGetAppIntegerValue (CFSTR ("AppleAntiAliasingThreshold"),
				     kCFPreferencesCurrentApplication,
				     &valid_p);
  if (valid_p)
    cg_text_anti_aliasing_threshold = threshold;
}

static int
mac_draw_image_string_cg (f, gc, x, y, buf, nchars, bg_width, overstrike_p)
     struct frame *f;
     GC gc;
     int x, y;
     XChar2b *buf;
     int nchars, bg_width, overstrike_p;
{
  CGFloat gx, gy;
  int i;
  CGContextRef context;
  CGGlyph *glyphs;
  CGSize *advances;

  if (!mac_use_core_graphics || GC_FONT (gc)->cg_font == NULL)
    return 0;

  gx = x;
  gy = -y;
  glyphs = (CGGlyph *)buf;
  advances = alloca (sizeof (CGSize) * nchars);
  if (advances == NULL)
    return 0;
  for (i = 0; i < nchars; i++)
    {
      XCharStruct *pcm = mac_per_char_metric (GC_FONT (gc), buf, 0);

      advances[i].width = pcm->width;
      advances[i].height = 0;
      glyphs[i] = GC_FONT (gc)->cg_glyphs[buf->byte2];
      buf++;
    }

#if USE_CG_DRAWING
  context = mac_begin_cg_clip (f, gc);
#else
  QDBeginCGContext (GetWindowPort (FRAME_MAC_WINDOW (f)), &context);
  CGContextTranslateCTM (context, 0, FRAME_PIXEL_HEIGHT (f));
  if (gc->n_clip_rects || bg_width)
    {
      CGContextScaleCTM (context, 1, -1);
      if (gc->n_clip_rects)
	CGContextClipToRects (context, gc->clip_rects, gc->n_clip_rects);
#endif
      if (bg_width)
	{
	  CG_SET_FILL_COLOR_WITH_GC_BACKGROUND (context, gc);
	  CGContextFillRect
	    (context,
	     mac_rect_make (f, gx, y - FONT_BASE (GC_FONT (gc)),
			    bg_width, FONT_HEIGHT (GC_FONT (gc))));
	}
      CGContextScaleCTM (context, 1, -1);
#if !USE_CG_DRAWING
    }
#endif
  CG_SET_FILL_COLOR_WITH_GC_FOREGROUND (context, gc);
  CGContextSetFont (context, GC_FONT (gc)->cg_font);
  CGContextSetFontSize (context, GC_FONT (gc)->mac_fontsize);
  CGContextSetTextMatrix (context, CGAffineTransformIdentity);
  if (GC_FONT (gc)->mac_fontsize <= cg_text_anti_aliasing_threshold)
    CGContextSetShouldAntialias (context, false);
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1030
#if MAC_OS_X_VERSION_MIN_REQUIRED == 1020
  if (CGContextShowGlyphsWithAdvances != NULL)
#endif
    {
      CGContextSetTextPosition (context, gx, gy);
      CGContextShowGlyphsWithAdvances (context, glyphs, advances, nchars);
      if (overstrike_p)
	{
	  CGContextSetTextPosition (context, gx + 1.0f, gy);
	  CGContextShowGlyphsWithAdvances (context, glyphs, advances, nchars);
	}
    }
#if MAC_OS_X_VERSION_MIN_REQUIRED == 1020
  else			 /* CGContextShowGlyphsWithAdvances == NULL */
#endif
#endif	/* MAC_OS_X_VERSION_MAX_ALLOWED >= 1030  */
#if MAC_OS_X_VERSION_MAX_ALLOWED < 1030 || MAC_OS_X_VERSION_MIN_REQUIRED == 1020
    {
      for (i = 0; i < nchars; i++)
	{
	  CGContextShowGlyphsAtPoint (context, gx, gy, glyphs + i, 1);
	  if (overstrike_p)
	    CGContextShowGlyphsAtPoint (context, gx + 1.0f, gy, glyphs + i, 1);
	  gx += advances[i].width;
	}
    }
#endif
#if USE_CG_DRAWING
  mac_end_cg_clip (f);
#else
  CGContextSynchronize (context);
  QDEndCGContext (GetWindowPort (FRAME_MAC_WINDOW (f)), &context);
#endif

  return 1;
}
#endif


#if !USE_CG_DRAWING
/* Mac replacement for XCopyArea: dest must be window.  */

static void
mac_copy_area (src, f, gc, src_x, src_y, width, height, dest_x, dest_y)
     Pixmap src;
     struct frame *f;
     GC gc;
     int src_x, src_y;
     unsigned int width, height;
     int dest_x, dest_y;
{
  Rect src_r, dest_r;

  mac_begin_clip (f, gc);

  SetRect (&src_r, src_x, src_y, src_x + width, src_y + height);
  SetRect (&dest_r, dest_x, dest_y, dest_x + width, dest_y + height);

  ForeColor (blackColor);
  BackColor (whiteColor);

  LockPixels (GetGWorldPixMap (src));
#if TARGET_API_MAC_CARBON
  {
    CGrafPtr port;

    GetPort (&port);
    LockPortBits (port);
    CopyBits (GetPortBitMapForCopyBits (src),
	      GetPortBitMapForCopyBits (port),
	      &src_r, &dest_r, srcCopy, 0);
    UnlockPortBits (port);
  }
#else /* not TARGET_API_MAC_CARBON */
  CopyBits (&(((GrafPtr)src)->portBits), &(FRAME_MAC_WINDOW (f)->portBits),
	    &src_r, &dest_r, srcCopy, 0);
#endif /* not TARGET_API_MAC_CARBON */
  UnlockPixels (GetGWorldPixMap (src));

  RGBBackColor (GC_BACK_COLOR (FRAME_NORMAL_GC (f)));

  mac_end_clip (f, gc);
}


static void
mac_copy_area_with_mask (src, mask, f, gc, src_x, src_y,
			 width, height, dest_x, dest_y)
     Pixmap src, mask;
     struct frame *f;
     GC gc;
     int src_x, src_y;
     unsigned int width, height;
     int dest_x, dest_y;
{
  Rect src_r, dest_r;

  mac_begin_clip (f, gc);

  SetRect (&src_r, src_x, src_y, src_x + width, src_y + height);
  SetRect (&dest_r, dest_x, dest_y, dest_x + width, dest_y + height);

  ForeColor (blackColor);
  BackColor (whiteColor);

  LockPixels (GetGWorldPixMap (src));
  LockPixels (GetGWorldPixMap (mask));
#if TARGET_API_MAC_CARBON
  {
    CGrafPtr port;

    GetPort (&port);
    LockPortBits (port);
    CopyMask (GetPortBitMapForCopyBits (src), GetPortBitMapForCopyBits (mask),
	      GetPortBitMapForCopyBits (port),
	      &src_r, &src_r, &dest_r);
    UnlockPortBits (port);
  }
#else /* not TARGET_API_MAC_CARBON */
  CopyMask (&(((GrafPtr)src)->portBits), &(((GrafPtr)mask)->portBits),
	    &(FRAME_MAC_WINDOW (f)->portBits), &src_r, &src_r, &dest_r);
#endif /* not TARGET_API_MAC_CARBON */
  UnlockPixels (GetGWorldPixMap (mask));
  UnlockPixels (GetGWorldPixMap (src));

  RGBBackColor (GC_BACK_COLOR (FRAME_NORMAL_GC (f)));

  mac_end_clip (f, gc);
}
#endif	/* !USE_CG_DRAWING */


/* Mac replacement for XCopyArea: used only for scrolling.  */

#if TARGET_API_MAC_CARBON
/* Defined in either mactoolbox.c or macappkit.m.  */
extern void mac_scroll_area P_ ((struct frame *, GC, int, int,
				 unsigned int, unsigned int, int, int));
#else  /* not TARGET_API_MAC_CARBON */
static void
mac_scroll_area (f, gc, src_x, src_y, width, height, dest_x, dest_y)
     struct frame *f;
     GC gc;
     int src_x, src_y;
     unsigned int width, height;
     int dest_x, dest_y;
{
  Rect src_r, dest_r;
  WindowRef w = FRAME_MAC_WINDOW (f);

  mac_begin_clip (f, gc);

  SetRect (&src_r, src_x, src_y, src_x + width, src_y + height);
  SetRect (&dest_r, dest_x, dest_y, dest_x + width, dest_y + height);

  /* In Color QuickDraw, set ForeColor and BackColor as follows to avoid
     color mapping in CopyBits.  Otherwise, it will be slow.  */
  ForeColor (blackColor);
  BackColor (whiteColor);
  CopyBits (&(w->portBits), &(w->portBits), &src_r, &dest_r, srcCopy, 0);

  RGBBackColor (GC_BACK_COLOR (FRAME_NORMAL_GC (f)));

  mac_end_clip (f, gc);
}
#endif	/* not TARGET_API_MAC_CARBON */


/* Mac replacement for XChangeGC.  */

static void
XChangeGC (display, gc, mask, xgcv)
     Display *display;
     GC gc;
     unsigned long mask;
     XGCValues *xgcv;
{
  if (mask & GCForeground)
    XSetForeground (display, gc, xgcv->foreground);
  if (mask & GCBackground)
    XSetBackground (display, gc, xgcv->background);
  if (mask & GCFont)
    XSetFont (display, gc, xgcv->font);
}


/* Mac replacement for XCreateGC.  */

GC
XCreateGC (display, d, mask, xgcv)
     Display *display;
     void *d;
     unsigned long mask;
     XGCValues *xgcv;
{
  GC gc = xmalloc (sizeof (*gc));

  bzero (gc, sizeof (*gc));
#if USE_CG_DRAWING && MAC_OS_X_VERSION_MAX_ALLOWED >= 1030
#if MAC_OS_X_VERSION_MIN_REQUIRED == 1020
  if (CGColorGetTypeID != NULL)
#endif
    {
      gc->cg_fore_color = gc->cg_back_color = mac_cg_color_black;
      CGColorRetain (gc->cg_fore_color);
      CGColorRetain (gc->cg_back_color);
    }
#endif
  XChangeGC (display, gc, mask, xgcv);

  return gc;
}


/* Used in xfaces.c.  */

void
XFreeGC (display, gc)
     Display *display;
     GC gc;
{
#if USE_QUICKDRAW
  if (gc->clip_region)
    DisposeRgn (gc->clip_region);
#endif
#if USE_CG_DRAWING && MAC_OS_X_VERSION_MAX_ALLOWED >= 1030
#if MAC_OS_X_VERSION_MIN_REQUIRED == 1020
  if (CGColorGetTypeID != NULL)
#endif
    {
      CGColorRelease (gc->cg_fore_color);
      CGColorRelease (gc->cg_back_color);
    }
#endif
  xfree (gc);
}


/* Mac replacement for XGetGCValues.  */

static void
XGetGCValues (display, gc, mask, xgcv)
     Display *display;
     GC gc;
     unsigned long mask;
     XGCValues *xgcv;
{
  if (mask & GCForeground)
    xgcv->foreground = gc->xgcv.foreground;
  if (mask & GCBackground)
    xgcv->background = gc->xgcv.background;
  if (mask & GCFont)
    xgcv->font = gc->xgcv.font;
}


/* Mac replacement for XSetForeground.  */

void
XSetForeground (display, gc, color)
     Display *display;
     GC gc;
     unsigned long color;
{
  if (gc->xgcv.foreground != color)
    {
      gc->xgcv.foreground = color;
#if USE_QUICKDRAW
      gc->fore_color.red = RED16_FROM_ULONG (color);
      gc->fore_color.green = GREEN16_FROM_ULONG (color);
      gc->fore_color.blue = BLUE16_FROM_ULONG (color);
#endif
#if USE_CG_DRAWING && MAC_OS_X_VERSION_MAX_ALLOWED >= 1030
#if MAC_OS_X_VERSION_MIN_REQUIRED == 1020
      if (CGColorGetTypeID != NULL)
#endif
	{
	  CGColorRelease (gc->cg_fore_color);
	  if (color == 0)
	    {
	      gc->cg_fore_color = mac_cg_color_black;
	      CGColorRetain (gc->cg_fore_color);
	    }
	  else
	    {
	      CGFloat rgba[4];

	      rgba[0] = RED_FROM_ULONG (color) / 255.0f;
	      rgba[1] = GREEN_FROM_ULONG (color) / 255.0f;
	      rgba[2] = BLUE_FROM_ULONG (color) / 255.0f;
	      rgba[3] = 1.0f;
	      gc->cg_fore_color = CGColorCreate (mac_cg_color_space_rgb, rgba);
	    }
	}
#endif
    }
}


/* Mac replacement for XSetBackground.  */

void
XSetBackground (display, gc, color)
     Display *display;
     GC gc;
     unsigned long color;
{
  if (gc->xgcv.background != color)
    {
      gc->xgcv.background = color;
#if USE_QUICKDRAW
      gc->back_color.red = RED16_FROM_ULONG (color);
      gc->back_color.green = GREEN16_FROM_ULONG (color);
      gc->back_color.blue = BLUE16_FROM_ULONG (color);
#endif
#if USE_CG_DRAWING && MAC_OS_X_VERSION_MAX_ALLOWED >= 1030
#if MAC_OS_X_VERSION_MIN_REQUIRED == 1020
      if (CGColorGetTypeID != NULL)
#endif
	{
	  CGColorRelease (gc->cg_back_color);
	  if (color == 0)
	    {
	      gc->cg_back_color = mac_cg_color_black;
	      CGColorRetain (gc->cg_back_color);
	    }
	  else
	    {
	      CGFloat rgba[4];

	      rgba[0] = RED_FROM_ULONG (color) / 255.0f;
	      rgba[1] = GREEN_FROM_ULONG (color) / 255.0f;
	      rgba[2] = BLUE_FROM_ULONG (color) / 255.0f;
	      rgba[3] = 1.0f;
	      gc->cg_back_color = CGColorCreate (mac_cg_color_space_rgb, rgba);
	    }
	}
#endif
    }
}


/* Mac replacement for XSetFont.  */

static void
XSetFont (display, gc, font)
     Display *display;
     GC gc;
     XFontStruct *font;
{
  gc->xgcv.font = font;
}


/* Mac replacement for XSetClipRectangles.  */

static void
mac_set_clip_rectangles (f, gc, rectangles, n)
     struct frame *f;
     GC gc;
     Rect *rectangles;
     int n;
{
  int i;

  xassert (n >= 0 && n <= MAX_CLIP_RECTS);

  gc->n_clip_rects = n;
#if USE_QUICKDRAW
  if (n > 0)
    {
      if (gc->clip_region == NULL)
	gc->clip_region = NewRgn ();
      RectRgn (gc->clip_region, rectangles);
      if (n > 1)
	{
	  RgnHandle region = NewRgn ();

	  for (i = 1; i < n; i++)
	    {
	      RectRgn (region, rectangles + i);
	      UnionRgn (gc->clip_region, region, gc->clip_region);
	    }
	  DisposeRgn (region);
	}
    }
#endif
#if defined (MAC_OSX) && (USE_ATSUI || USE_CG_DRAWING)
  for (i = 0; i < n; i++)
    {
      Rect *rect = rectangles + i;

      gc->clip_rects[i] = mac_rect_make (f, rect->left, rect->top,
					 rect->right - rect->left,
					 rect->bottom - rect->top);
    }
#endif
}


/* Mac replacement for XSetClipMask.  */

static INLINE void
mac_reset_clip_rectangles (f, gc)
     struct frame *f;
     GC gc;
{
  gc->n_clip_rects = 0;
}

/* Remove calls to XFlush by defining XFlush to an empty replacement.
   Calls to XFlush should be unnecessary because the X output buffer
   is flushed automatically as needed by calls to XPending,
   XNextEvent, or XWindowEvent according to the XFlush man page.
   XTread_socket calls XPending.  Removing XFlush improves
   performance.  */

#define XFlush(DISPLAY)	(void) 0


/***********************************************************************
		    Starting and ending an update
 ***********************************************************************/

/* Start an update of frame F.  This function is installed as a hook
   for update_begin, i.e. it is called when update_begin is called.
   This function is called prior to calls to x_update_window_begin for
   each window being updated.  */

static void
x_update_begin (f)
     struct frame *f;
{
  BLOCK_INPUT;
  mac_update_begin (f);
  UNBLOCK_INPUT;
}


/* Start update of window W.  Set the global variable updated_window
   to the window being updated and set output_cursor to the cursor
   position of W.  */

static void
x_update_window_begin (w)
     struct window *w;
{
  struct frame *f = XFRAME (WINDOW_FRAME (w));
  struct mac_display_info *display_info = FRAME_MAC_DISPLAY_INFO (f);

  updated_window = w;
  set_output_cursor (&w->cursor);

  BLOCK_INPUT;

  if (f == display_info->mouse_face_mouse_frame)
    {
      /* Don't do highlighting for mouse motion during the update.  */
      display_info->mouse_face_defer = 1;

      /* If F needs to be redrawn, simply forget about any prior mouse
	 highlighting.  */
      if (FRAME_GARBAGED_P (f))
	display_info->mouse_face_window = Qnil;

#if 0 /* Rows in a current matrix containing glyphs in mouse-face have
	 their mouse_face_p flag set, which means that they are always
	 unequal to rows in a desired matrix which never have that
	 flag set.  So, rows containing mouse-face glyphs are never
	 scrolled, and we don't have to switch the mouse highlight off
	 here to prevent it from being scrolled.  */

      /* Can we tell that this update does not affect the window
	 where the mouse highlight is?  If so, no need to turn off.
	 Likewise, don't do anything if the frame is garbaged;
	 in that case, the frame's current matrix that we would use
	 is all wrong, and we will redisplay that line anyway.  */
      if (!NILP (display_info->mouse_face_window)
	  && w == XWINDOW (display_info->mouse_face_window))
	{
	  int i;

	  for (i = 0; i < w->desired_matrix->nrows; ++i)
	    if (MATRIX_ROW_ENABLED_P (w->desired_matrix, i))
	      break;

	  if (i < w->desired_matrix->nrows)
	    clear_mouse_face (display_info);
	}
#endif /* 0 */
    }

  UNBLOCK_INPUT;
}


/* Draw a vertical window border from (x,y0) to (x,y1)  */

static void
mac_draw_vertical_window_border (w, x, y0, y1)
     struct window *w;
     int x, y0, y1;
{
  struct frame *f = XFRAME (WINDOW_FRAME (w));
  struct face *face;

  face = FACE_FROM_ID (f, VERTICAL_BORDER_FACE_ID);
  if (face)
    XSetForeground (FRAME_MAC_DISPLAY (f), f->output_data.mac->normal_gc,
		    face->foreground);

#if USE_APPKIT
  mac_fill_rectangle (f, f->output_data.mac->normal_gc, x, y0, 1, y1 - y0);
#else
  mac_draw_line (f, f->output_data.mac->normal_gc, x, y0, x, y1);
#endif
}

/* End update of window W (which is equal to updated_window).

   Draw vertical borders between horizontally adjacent windows, and
   display W's cursor if CURSOR_ON_P is non-zero.

   MOUSE_FACE_OVERWRITTEN_P non-zero means that some row containing
   glyphs in mouse-face were overwritten.  In that case we have to
   make sure that the mouse-highlight is properly redrawn.

   W may be a menu bar pseudo-window in case we don't have X toolkit
   support.  Such windows don't have a cursor, so don't display it
   here.  */

static void
x_update_window_end (w, cursor_on_p, mouse_face_overwritten_p)
     struct window *w;
     int cursor_on_p, mouse_face_overwritten_p;
{
  struct mac_display_info *dpyinfo = FRAME_MAC_DISPLAY_INFO (XFRAME (w->frame));

  if (!w->pseudo_window_p)
    {
      BLOCK_INPUT;

      if (cursor_on_p)
	display_and_set_cursor (w, 1, output_cursor.hpos,
				output_cursor.vpos,
				output_cursor.x, output_cursor.y);

      if (draw_window_fringes (w, 1))
	x_draw_vertical_border (w);

      UNBLOCK_INPUT;
    }

  /* If a row with mouse-face was overwritten, arrange for
     XTframe_up_to_date to redisplay the mouse highlight.  */
  if (mouse_face_overwritten_p)
    {
      dpyinfo->mouse_face_beg_row = dpyinfo->mouse_face_beg_col = -1;
      dpyinfo->mouse_face_end_row = dpyinfo->mouse_face_end_col = -1;
      dpyinfo->mouse_face_window = Qnil;
    }

  updated_window = NULL;
}


/* End update of frame F.  This function is installed as a hook in
   update_end.  */

static void
x_update_end (f)
     struct frame *f;
{
  /* Mouse highlight may be displayed again.  */
  FRAME_MAC_DISPLAY_INFO (f)->mouse_face_defer = 0;

  BLOCK_INPUT;
  mac_update_end (f);
  XFlush (FRAME_MAC_DISPLAY (f));
  UNBLOCK_INPUT;
}


/* This function is called from various places in xdisp.c whenever a
   complete update has been performed.  The global variable
   updated_window is not available here.  */

static void
XTframe_up_to_date (f)
     struct frame *f;
{
  if (FRAME_MAC_P (f))
    {
      struct mac_display_info *dpyinfo = FRAME_MAC_DISPLAY_INFO (f);

      if (dpyinfo->mouse_face_deferred_gc
	  || f == dpyinfo->mouse_face_mouse_frame)
	{
	  BLOCK_INPUT;
	  if (dpyinfo->mouse_face_mouse_frame)
	    note_mouse_highlight (dpyinfo->mouse_face_mouse_frame,
				  dpyinfo->mouse_face_mouse_x,
				  dpyinfo->mouse_face_mouse_y);
	  dpyinfo->mouse_face_deferred_gc = 0;
	  UNBLOCK_INPUT;
	}

      mac_frame_up_to_date (f);
    }
}


/* Draw truncation mark bitmaps, continuation mark bitmaps, overlay
   arrow bitmaps, or clear the fringes if no bitmaps are required
   before DESIRED_ROW is made current.  The window being updated is
   found in updated_window.  This function is called from
   update_window_line only if it is known that there are differences
   between bitmaps to be drawn between current row and DESIRED_ROW.  */

static void
x_after_update_window_line (desired_row)
     struct glyph_row *desired_row;
{
  struct window *w = updated_window;
  struct frame *f;
  int width, height;

  xassert (w);

  if (!desired_row->mode_line_p && !w->pseudo_window_p)
    desired_row->redraw_fringe_bitmaps_p = 1;

  /* When a window has disappeared, make sure that no rest of
     full-width rows stays visible in the internal border.  Could
     check here if updated_window is the leftmost/rightmost window,
     but I guess it's not worth doing since vertically split windows
     are almost never used, internal border is rarely set, and the
     overhead is very small.  */
  if (windows_or_buffers_changed
      && desired_row->full_width_p
      && (f = XFRAME (w->frame),
	  width = FRAME_INTERNAL_BORDER_WIDTH (f),
	  width != 0)
      && (height = desired_row->visible_height,
	  height > 0))
    {
      int y = WINDOW_TO_FRAME_PIXEL_Y (w, max (0, desired_row->y));

      /* Internal border is drawn below the tool bar.  */
      if (WINDOWP (f->tool_bar_window)
	  && w == XWINDOW (f->tool_bar_window))
	y -= width;

      BLOCK_INPUT;
      mac_clear_area (f, 0, y, width, height);
      mac_clear_area (f, FRAME_PIXEL_WIDTH (f) - width, y, width, height);
      UNBLOCK_INPUT;
    }
}


/* Draw the bitmap WHICH in one of the left or right fringes of
   window W.  ROW is the glyph row for which to display the bitmap; it
   determines the vertical position at which the bitmap has to be
   drawn.  */

static void
x_draw_fringe_bitmap (w, row, p)
     struct window *w;
     struct glyph_row *row;
     struct draw_fringe_bitmap_params *p;
{
  struct frame *f = XFRAME (WINDOW_FRAME (w));
  Display *display = FRAME_MAC_DISPLAY (f);
  struct face *face = p->face;
  int overlay_p = p->overlay_p;

#ifdef MAC_OSX
  if (!overlay_p)
    {
      int bx = p->bx, nx = p->nx;

#if 0  /* MAC_TODO: stipple */
      /* In case the same realized face is used for fringes and
	 for something displayed in the text (e.g. face `region' on
	 mono-displays, the fill style may have been changed to
	 FillSolid in x_draw_glyph_string_background.  */
      if (face->stipple)
	XSetFillStyle (FRAME_X_DISPLAY (f), face->gc, FillOpaqueStippled);
      else
	XSetForeground (FRAME_X_DISPLAY (f), face->gc, face->background);
#endif

      /* If the fringe is adjacent to the left (right) scroll bar of a
	 leftmost (rightmost, respectively) window, then extend its
	 background to the gap between the fringe and the bar.  */
      if ((WINDOW_LEFTMOST_P (w)
	   && WINDOW_HAS_VERTICAL_SCROLL_BAR_ON_LEFT (w))
	  || (WINDOW_RIGHTMOST_P (w)
	      && WINDOW_HAS_VERTICAL_SCROLL_BAR_ON_RIGHT (w)))
	{
	  int sb_width = WINDOW_CONFIG_SCROLL_BAR_WIDTH (w);

	  if (sb_width > 0)
	    {
	      int left = WINDOW_SCROLL_BAR_AREA_X (w);
	      int width = (WINDOW_CONFIG_SCROLL_BAR_COLS (w)
			   * FRAME_COLUMN_WIDTH (f));

	      if (bx < 0
		  && (left + width == p->x
		      || p->x + p->wd == left))
		{
		  /* Bitmap fills the fringe and we need background
		     extension.  */
		  bx = p->x;
		  nx = p->wd;
		}

	      if (bx >= 0)
		{
		  if (left + width == bx)
		    {
		      bx = left + sb_width;
		      nx += width - sb_width;
		    }
		  else if (bx + nx == left)
		    nx += width - sb_width;
		}
	    }
	}

      if (bx >= 0)
	{
	  mac_erase_rectangle (f, face->gc, bx, p->by, nx, p->ny);
	  /* The fringe background has already been filled.  */
	  overlay_p = 1;
	}

#if 0  /* MAC_TODO: stipple */
      if (!face->stipple)
	XSetForeground (FRAME_X_DISPLAY (f), face->gc, face->foreground);
#endif
    }
#endif	/* MAC_OSX */

  /* Must clip because of partially visible lines.  */
  x_clip_to_row (w, row, -1, face->gc);

#ifndef MAC_OSX
  if (p->bx >= 0 && !p->overlay_p)
    {
#if 0  /* MAC_TODO: stipple */
      /* In case the same realized face is used for fringes and
	 for something displayed in the text (e.g. face `region' on
	 mono-displays, the fill style may have been changed to
	 FillSolid in x_draw_glyph_string_background.  */
      if (face->stipple)
	XSetFillStyle (FRAME_X_DISPLAY (f), face->gc, FillOpaqueStippled);
      else
	XSetForeground (FRAME_X_DISPLAY (f), face->gc, face->background);
#endif

      mac_erase_rectangle (f, face->gc, p->bx, p->by, p->nx, p->ny);

#if 0  /* MAC_TODO: stipple */
      if (!face->stipple)
	XSetForeground (FRAME_X_DISPLAY (f), face->gc, face->foreground);
#endif
    }
#endif	/* !MAC_OSX */

  if (p->which
#if USE_CG_DRAWING
      && p->which < max_fringe_bmp
#endif
      )
    {
      XGCValues gcv;

      XGetGCValues (display, face->gc, GCForeground, &gcv);
      XSetForeground (display, face->gc,
		      (p->cursor_p
		       ? (p->overlay_p ? face->background
			  : f->output_data.mac->cursor_pixel)
		       : face->foreground));
#if USE_CG_DRAWING
      mac_draw_cg_image (fringe_bmp[p->which], f, face->gc, 0, p->dh,
			 p->wd, p->h, p->x, p->y, overlay_p);
#else
      mac_draw_bitmap (f, face->gc, p->x, p->y,
		       p->wd, p->h, p->bits + p->dh, overlay_p);
#endif
      XSetForeground (display, face->gc, gcv.foreground);
    }

  mac_reset_clip_rectangles (f, face->gc);
}

#if USE_CG_DRAWING
static void
mac_define_fringe_bitmap (which, bits, h, wd)
     int which;
     unsigned short *bits;
     int h, wd;
{
  int i;
  CGDataProviderRef provider;

  if (which >= max_fringe_bmp)
    {
      i = max_fringe_bmp;
      max_fringe_bmp = which + 20;
      fringe_bmp = (CGImageRef *) xrealloc (fringe_bmp, max_fringe_bmp * sizeof (CGImageRef));
      while (i < max_fringe_bmp)
	fringe_bmp[i++] = 0;
    }

  for (i = 0; i < h; i++)
    bits[i] = ~bits[i];

  BLOCK_INPUT;

  provider = CGDataProviderCreateWithData (NULL, bits,
					   sizeof (unsigned short) * h, NULL);
  if (provider)
    {
      fringe_bmp[which] = CGImageMaskCreate (wd, h, 1, 1,
					     sizeof (unsigned short),
					     provider, NULL, 0);
      CGDataProviderRelease (provider);
    }

  UNBLOCK_INPUT;
}

static void
mac_destroy_fringe_bitmap (which)
     int which;
{
  if (which >= max_fringe_bmp)
    return;

  if (fringe_bmp[which])
    {
      BLOCK_INPUT;
      CGImageRelease (fringe_bmp[which]);
      UNBLOCK_INPUT;
    }
  fringe_bmp[which] = 0;
}
#endif


/* This is called when starting Emacs and when restarting after
   suspend.  When starting Emacs, no window is mapped.  And nothing
   must be done to Emacs's own window if it is suspended (though that
   rarely happens).  */

static void
XTset_terminal_modes ()
{
}

/* This is called when exiting or suspending Emacs.  Exiting will make
   the windows go away, and suspending requires no action.  */

static void
XTreset_terminal_modes ()
{
}



/***********************************************************************
			   Display Iterator
 ***********************************************************************/

/* Function prototypes of this page.  */

static XCharStruct *x_per_char_metric P_ ((XFontStruct *, XChar2b *));
static int mac_encode_char P_ ((int, XChar2b *, struct font_info *, int *));


static void
pcm_init (pcm, count)
     XCharStruct *pcm;
     int count;
{
  bzero (pcm, sizeof (XCharStruct) * count);
  while (--count >= 0)
    {
      pcm->descent = PCM_INVALID;
      pcm++;
    }
}

static enum pcm_status
pcm_get_status (pcm)
     const XCharStruct *pcm;
{
  int height = pcm->ascent + pcm->descent;

  /* Negative height means some special status.  */
  return height >= 0 ? PCM_VALID : height;
}

/* Get metrics of character CHAR2B in FONT.  Value is null if CHAR2B
   is not contained in the font.  */

static INLINE XCharStruct *
x_per_char_metric (font, char2b)
     XFontStruct *font;
     XChar2b *char2b;
{
  /* The result metric information.  */
  XCharStruct *pcm = NULL;

  xassert (font && char2b);

#if USE_ATSUI
  if (font->mac_style)
    {
      XCharStruct **row = font->bounds.rows + char2b->byte1;

      if (*row == NULL)
	{
	  *row = xmalloc (sizeof (XCharStruct) * 0x100);
	  pcm_init (*row, 0x100);
	}
      pcm = *row + char2b->byte2;
      if (pcm_get_status (pcm) != PCM_VALID)
	{
	  BLOCK_INPUT;
	  mac_query_char_extents (font->mac_style,
				  (char2b->byte1 << 8) + char2b->byte2,
				  NULL, NULL, pcm, NULL);
	  UNBLOCK_INPUT;
	}
    }
  else
    {
#endif
  if (font->bounds.per_char != NULL)
    {
      if (font->min_byte1 == 0 && font->max_byte1 == 0)
	{
	  /* min_char_or_byte2 specifies the linear character index
	     corresponding to the first element of the per_char array,
	     max_char_or_byte2 is the index of the last character.  A
	     character with non-zero CHAR2B->byte1 is not in the font.
	     A character with byte2 less than min_char_or_byte2 or
	     greater max_char_or_byte2 is not in the font.  */
	  if (char2b->byte1 == 0
	      && char2b->byte2 >= font->min_char_or_byte2
	      && char2b->byte2 <= font->max_char_or_byte2)
	    pcm = font->bounds.per_char
	      + (char2b->byte2 - font->min_char_or_byte2);
	}
      else
	{
	  /* If either min_byte1 or max_byte1 are nonzero, both
	     min_char_or_byte2 and max_char_or_byte2 are less than
	     256, and the 2-byte character index values corresponding
	     to the per_char array element N (counting from 0) are:

	     byte1 = N/D + min_byte1
	     byte2 = N\D + min_char_or_byte2

	     where:

	     D = max_char_or_byte2 - min_char_or_byte2 + 1
	     / = integer division
	     \ = integer modulus  */
	  if (char2b->byte1 >= font->min_byte1
	      && char2b->byte1 <= font->max_byte1
	      && char2b->byte2 >= font->min_char_or_byte2
	      && char2b->byte2 <= font->max_char_or_byte2)
	    {
	      pcm = (font->bounds.per_char
		     + ((font->max_char_or_byte2 - font->min_char_or_byte2 + 1)
			* (char2b->byte1 - font->min_byte1))
		     + (char2b->byte2 - font->min_char_or_byte2));
	    }
	}
    }
  else
    {
      /* If the per_char pointer is null, all glyphs between the first
	 and last character indexes inclusive have the same
	 information, as given by both min_bounds and max_bounds.  */
      if (char2b->byte2 >= font->min_char_or_byte2
	  && char2b->byte2 <= font->max_char_or_byte2)
	pcm = &font->max_bounds;
    }
#if USE_ATSUI
    }
#endif

  return ((pcm == NULL
	   || (pcm->width == 0
#if 0 /* Show hollow boxes for zero-width glyphs such as combining diacritics.  */
	       && (pcm->rbearing - pcm->lbearing) == 0
#endif
	       ))
	  ? NULL : pcm);
}

/* RIF:
 */

static XCharStruct *
mac_per_char_metric (font, char2b, font_type)
     XFontStruct *font;
     XChar2b *char2b;
     int font_type;
{
  return x_per_char_metric (font, char2b);
}

/* RIF:
   Encode CHAR2B using encoding information from FONT_INFO.  CHAR2B is
   the two-byte form of C.  Encoding is returned in *CHAR2B.  */

static int
mac_encode_char (c, char2b, font_info, two_byte_p)
     int c;
     XChar2b *char2b;
     struct font_info *font_info;
     int *two_byte_p;
{
  int charset = CHAR_CHARSET (c);
  XFontStruct *font = font_info->font;

  /* FONT_INFO may define a scheme by which to encode byte1 and byte2.
     This may be either a program in a special encoder language or a
     fixed encoding.  */
  if (font_info->font_encoder)
    {
      /* It's a program.  */
      struct ccl_program *ccl = font_info->font_encoder;

      check_ccl_update (ccl);
      if (CHARSET_DIMENSION (charset) == 1)
	{
	  ccl->reg[0] = charset;
	  ccl->reg[1] = char2b->byte2;
	  ccl->reg[2] = -1;
	}
      else
	{
	  ccl->reg[0] = charset;
	  ccl->reg[1] = char2b->byte1;
	  ccl->reg[2] = char2b->byte2;
	}

      ccl_driver (ccl, NULL, NULL, 0, 0, NULL);

      /* We assume that MSBs are appropriately set/reset by CCL
	 program.  */
      if (font->max_byte1 == 0)	/* 1-byte font */
	char2b->byte1 = 0, char2b->byte2 = ccl->reg[1];
      else
	char2b->byte1 = ccl->reg[1], char2b->byte2 = ccl->reg[2];
    }
  else if (font_info->encoding[charset])
    {
      /* Fixed encoding scheme.  See fontset.h for the meaning of the
	 encoding numbers.  */
      int enc = font_info->encoding[charset];

      if ((enc == 1 || enc == 2)
	  && CHARSET_DIMENSION (charset) == 2)
	char2b->byte1 |= 0x80;

      if (enc == 1 || enc == 3)
	char2b->byte2 |= 0x80;

      if (enc == 4)
        {
          int sjis1, sjis2;

          ENCODE_SJIS (char2b->byte1, char2b->byte2, sjis1, sjis2);
          char2b->byte1 = sjis1;
          char2b->byte2 = sjis2;
        }
    }

  if (two_byte_p)
    *two_byte_p = ((XFontStruct *) (font_info->font))->max_byte1 > 0;

  return FONT_TYPE_UNKNOWN;
}



/***********************************************************************
			    Glyph display
 ***********************************************************************/



static void x_set_glyph_string_clipping P_ ((struct glyph_string *));
static void x_set_glyph_string_gc P_ ((struct glyph_string *));
static void x_draw_glyph_string_background P_ ((struct glyph_string *,
						int));
static void x_draw_glyph_string_foreground P_ ((struct glyph_string *));
static void x_draw_composite_glyph_string_foreground P_ ((struct glyph_string *));
static void x_draw_glyph_string_box P_ ((struct glyph_string *));
static void x_draw_glyph_string  P_ ((struct glyph_string *));
static void mac_compute_glyph_string_overhangs P_ ((struct glyph_string *));
static void x_set_cursor_gc P_ ((struct glyph_string *));
static void x_set_mode_line_face_gc P_ ((struct glyph_string *));
static void x_set_mouse_face_gc P_ ((struct glyph_string *));
/*static int x_alloc_lighter_color P_ ((struct frame *, Display *, Colormap,
  unsigned long *, double, int));*/
static void x_setup_relief_color P_ ((struct frame *, struct relief *,
				      double, int, unsigned long));
static void x_setup_relief_colors P_ ((struct glyph_string *));
static void x_draw_image_glyph_string P_ ((struct glyph_string *));
static void x_draw_image_relief P_ ((struct glyph_string *));
static void x_draw_image_foreground P_ ((struct glyph_string *));
static void x_clear_glyph_string_rect P_ ((struct glyph_string *, int,
					   int, int, int));
static void x_draw_relief_rect P_ ((struct frame *, int, int, int, int,
				    int, int, int, int, int, int,
				    Rect *));
static void x_draw_box_rect P_ ((struct glyph_string *, int, int, int, int,
				 int, int, int, Rect *));

#if GLYPH_DEBUG
static void x_check_font P_ ((struct frame *, XFontStruct *));
#endif


/* Set S->gc to a suitable GC for drawing glyph string S in cursor
   face.  */

static void
x_set_cursor_gc (s)
     struct glyph_string *s;
{
  if (s->font == FRAME_FONT (s->f)
      && s->face->background == FRAME_BACKGROUND_PIXEL (s->f)
      && s->face->foreground == FRAME_FOREGROUND_PIXEL (s->f)
      && !s->cmp)
    s->gc = s->f->output_data.mac->cursor_gc;
  else
    {
      /* Cursor on non-default face: must merge.  */
      XGCValues xgcv;
      unsigned long mask;

      xgcv.background = s->f->output_data.mac->cursor_pixel;
      xgcv.foreground = s->face->background;

      /* If the glyph would be invisible, try a different foreground.  */
      if (xgcv.foreground == xgcv.background)
	xgcv.foreground = s->face->foreground;
      if (xgcv.foreground == xgcv.background)
	xgcv.foreground = s->f->output_data.mac->cursor_foreground_pixel;
      if (xgcv.foreground == xgcv.background)
	xgcv.foreground = s->face->foreground;

      /* Make sure the cursor is distinct from text in this face.  */
      if (xgcv.background == s->face->background
	  && xgcv.foreground == s->face->foreground)
	{
	  xgcv.background = s->face->foreground;
	  xgcv.foreground = s->face->background;
	}

      IF_DEBUG (x_check_font (s->f, s->font));
      xgcv.font = s->font;
      mask = GCForeground | GCBackground | GCFont;

      if (FRAME_MAC_DISPLAY_INFO (s->f)->scratch_cursor_gc)
	XChangeGC (s->display, FRAME_MAC_DISPLAY_INFO (s->f)->scratch_cursor_gc,
		   mask, &xgcv);
      else
	FRAME_MAC_DISPLAY_INFO (s->f)->scratch_cursor_gc
	  = XCreateGC (s->display, s->window, mask, &xgcv);

      s->gc = FRAME_MAC_DISPLAY_INFO (s->f)->scratch_cursor_gc;
    }
}


/* Set up S->gc of glyph string S for drawing text in mouse face.  */

static void
x_set_mouse_face_gc (s)
     struct glyph_string *s;
{
  int face_id;
  struct face *face;

  /* What face has to be used last for the mouse face?  */
  face_id = FRAME_X_DISPLAY_INFO (s->f)->mouse_face_face_id;
  face = FACE_FROM_ID (s->f, face_id);
  if (face == NULL)
    face = FACE_FROM_ID (s->f, MOUSE_FACE_ID);

  if (s->first_glyph->type == CHAR_GLYPH)
    face_id = FACE_FOR_CHAR (s->f, face, s->first_glyph->u.ch);
  else
    face_id = FACE_FOR_CHAR (s->f, face, 0);
  s->face = FACE_FROM_ID (s->f, face_id);
  PREPARE_FACE_FOR_DISPLAY (s->f, s->face);

  /* If font in this face is same as S->font, use it.  */
  if (s->font == s->face->font)
    s->gc = s->face->gc;
  else
    {
      /* Otherwise construct scratch_cursor_gc with values from FACE
	 but font FONT.  */
      XGCValues xgcv;
      unsigned long mask;

      xgcv.background = s->face->background;
      xgcv.foreground = s->face->foreground;
      IF_DEBUG (x_check_font (s->f, s->font));
      xgcv.font = s->font;
      mask = GCForeground | GCBackground | GCFont;

      if (FRAME_MAC_DISPLAY_INFO (s->f)->scratch_cursor_gc)
	XChangeGC (s->display, FRAME_MAC_DISPLAY_INFO (s->f)->scratch_cursor_gc,
		   mask, &xgcv);
      else
	FRAME_MAC_DISPLAY_INFO (s->f)->scratch_cursor_gc
	  = XCreateGC (s->display, s->window, mask, &xgcv);

      s->gc = FRAME_MAC_DISPLAY_INFO (s->f)->scratch_cursor_gc;
    }

  xassert (s->gc != 0);
}


/* Set S->gc of glyph string S to a GC suitable for drawing a mode line.
   Faces to use in the mode line have already been computed when the
   matrix was built, so there isn't much to do, here.  */

static INLINE void
x_set_mode_line_face_gc (s)
     struct glyph_string *s;
{
  s->gc = s->face->gc;
}


/* Set S->gc of glyph string S for drawing that glyph string.  Set
   S->stippled_p to a non-zero value if the face of S has a stipple
   pattern.  */

static INLINE void
x_set_glyph_string_gc (s)
     struct glyph_string *s;
{
  PREPARE_FACE_FOR_DISPLAY (s->f, s->face);

  if (s->hl == DRAW_NORMAL_TEXT)
    {
      s->gc = s->face->gc;
      s->stippled_p = s->face->stipple != 0;
    }
  else if (s->hl == DRAW_INVERSE_VIDEO)
    {
      x_set_mode_line_face_gc (s);
      s->stippled_p = s->face->stipple != 0;
    }
  else if (s->hl == DRAW_CURSOR)
    {
      x_set_cursor_gc (s);
      s->stippled_p = 0;
    }
  else if (s->hl == DRAW_MOUSE_FACE)
    {
      x_set_mouse_face_gc (s);
      s->stippled_p = s->face->stipple != 0;
    }
  else if (s->hl == DRAW_IMAGE_RAISED
	   || s->hl == DRAW_IMAGE_SUNKEN)
    {
      s->gc = s->face->gc;
      s->stippled_p = s->face->stipple != 0;
    }
  else
    {
      s->gc = s->face->gc;
      s->stippled_p = s->face->stipple != 0;
    }

  /* GC must have been set.  */
  xassert (s->gc != 0);
}


/* Set clipping for output of glyph string S.  S may be part of a mode
   line or menu if we don't have X toolkit support.  */

static INLINE void
x_set_glyph_string_clipping (s)
     struct glyph_string *s;
{
  Rect rects[MAX_CLIP_RECTS];
  int n;

  n = get_glyph_string_clip_rects (s, rects, MAX_CLIP_RECTS);
  mac_set_clip_rectangles (s->f, s->gc, rects, n);
}


/* RIF:
   Compute left and right overhang of glyph string S.  If S is a glyph
   string for a composition, assume overhangs don't exist.  */

static void
mac_compute_glyph_string_overhangs (s)
     struct glyph_string *s;
{
  if (!(s->cmp == NULL
	&& s->first_glyph->type == CHAR_GLYPH))
    return;

  if (!s->two_byte_p
#if USE_ATSUI
      || s->font->mac_style
#endif
      )
    {
      XCharStruct cs;

      mac_text_extents_16 (s->font, s->char2b, s->nchars, &cs);
      s->right_overhang = cs.rbearing > cs.width ? cs.rbearing - cs.width : 0;
      s->left_overhang = cs.lbearing < 0 ? -cs.lbearing : 0;
    }
  else
    {
#if !USE_QUICKDRAW
      abort ();
#else  /* USE_QUICKDRAW */
      Rect r;
      MacFontStruct *font = s->font;

      mac_begin_clip (s->f, NULL);

      TextFont (font->mac_fontnum);
      TextSize (font->mac_fontsize);
      TextFace (font->mac_fontface);

      QDTextBounds (s->nchars * 2, (char *)s->char2b, &r);

      mac_end_clip (s->f, NULL);

      s->right_overhang = r.right > s->width ? r.right - s->width : 0;
      s->left_overhang = r.left < 0 ? -r.left : 0;
#endif	/* USE_QUICKDRAW */
    }
}


/* Fill rectangle X, Y, W, H with background color of glyph string S.  */

static INLINE void
x_clear_glyph_string_rect (s, x, y, w, h)
     struct glyph_string *s;
     int x, y, w, h;
{
  mac_erase_rectangle (s->f, s->gc, x, y, w, h);
}


/* Draw the background of glyph_string S.  If S->background_filled_p
   is non-zero don't draw it.  FORCE_P non-zero means draw the
   background even if it wouldn't be drawn normally.  This is used
   when a string preceding S draws into the background of S, or S
   contains the first component of a composition.  */

static void
x_draw_glyph_string_background (s, force_p)
     struct glyph_string *s;
     int force_p;
{
  /* Nothing to do if background has already been drawn or if it
     shouldn't be drawn in the first place.  */
  if (!s->background_filled_p)
    {
      int box_line_width = max (s->face->box_line_width, 0);

#if 0 /* MAC_TODO: stipple */
      if (s->stippled_p)
	{
	  /* Fill background with a stipple pattern.  */
	  XSetFillStyle (s->display, s->gc, FillOpaqueStippled);
	  XFillRectangle (s->display, s->window, s->gc, s->x,
			  s->y + box_line_width,
			  s->background_width,
			  s->height - 2 * box_line_width);
	  XSetFillStyle (s->display, s->gc, FillSolid);
	  s->background_filled_p = 1;
	}
      else
#endif
        if (FONT_HEIGHT (s->font) < s->height - 2 * box_line_width
	       || s->font_not_found_p
	       || s->extends_to_end_of_line_p
	       || force_p)
	{
	  x_clear_glyph_string_rect (s, s->x, s->y + box_line_width,
				     s->background_width,
				     s->height - 2 * box_line_width);
	  s->background_filled_p = 1;
	}
    }
}


/* Draw the foreground of glyph string S.  */

static void
x_draw_glyph_string_foreground (s)
     struct glyph_string *s;
{
  int i, x, bg_width;

  /* If first glyph of S has a left box line, start drawing the text
     of S to the right of that box line.  */
  if (s->face->box != FACE_NO_BOX
      && s->first_glyph->left_box_line_p)
    x = s->x + abs (s->face->box_line_width);
  else
    x = s->x;

  /* Draw characters of S as rectangles if S's font could not be
     loaded.  */
  if (s->font_not_found_p)
    {
      for (i = 0; i < s->nchars; ++i)
	{
	  struct glyph *g = s->first_glyph + i;
	  mac_draw_rectangle (s->f, s->gc, x, s->y,
			      g->pixel_width - 1, s->height - 1);
	  x += g->pixel_width;
	}
    }
  else
    {
      char *char1b = (char *) s->char2b;
      int boff = s->font_info->baseline_offset;

      if (s->font_info->vertical_centering)
	boff = VCENTER_BASELINE_OFFSET (s->font, s->f) - boff;

      /* If we can use 8-bit functions, condense S->char2b.  */
      if (!s->two_byte_p
#if USE_ATSUI
	  && GC_FONT (s->gc)->mac_style == NULL
#endif
	  )
	for (i = 0; i < s->nchars; ++i)
	  char1b[i] = s->char2b[i].byte2;

      /* Draw text with XDrawString if background has already been
	 filled.  Otherwise, use XDrawImageString.  (Note that
	 XDrawImageString is usually faster than XDrawString.)  Always
	 use XDrawImageString when drawing the cursor so that there is
	 no chance that characters under a box cursor are invisible.  */
      if (s->for_overlaps
	  || (s->background_filled_p && s->hl != DRAW_CURSOR))
	bg_width = 0;		/* Corresponds to XDrawString.  */
      else
	bg_width = s->background_width; /* Corresponds to XDrawImageString.  */

      if (s->two_byte_p
#if USE_ATSUI
	  || GC_FONT (s->gc)->mac_style
#endif
	  )
#if USE_CG_TEXT_DRAWING
	if (!s->two_byte_p
	    && mac_draw_image_string_cg (s->f, s->gc, x, s->ybase - boff,
					 s->char2b, s->nchars, bg_width,
					 s->face->overstrike))
	  ;
	else
#endif
	  mac_draw_image_string_16 (s->f, s->gc, x, s->ybase - boff,
				    s->char2b, s->nchars, bg_width,
				    s->face->overstrike);
      else
	mac_draw_image_string (s->f, s->gc, x, s->ybase - boff,
			       char1b, s->nchars, bg_width,
			       s->face->overstrike);
    }
}

/* Draw the foreground of composite glyph string S.  */

static void
x_draw_composite_glyph_string_foreground (s)
     struct glyph_string *s;
{
  int i, x;

  /* If first glyph of S has a left box line, start drawing the text
     of S to the right of that box line.  */
  if (s->face->box != FACE_NO_BOX
      && s->first_glyph->left_box_line_p)
    x = s->x + abs (s->face->box_line_width);
  else
    x = s->x;

  /* S is a glyph string for a composition.  S->gidx is the index of
     the first character drawn for glyphs of this composition.
     S->gidx == 0 means we are drawing the very first character of
     this composition.  */

  /* Draw a rectangle for the composition if the font for the very
     first character of the composition could not be loaded.  */
  if (s->font_not_found_p)
    {
      if (s->gidx == 0)
	mac_draw_rectangle (s->f, s->gc, x, s->y,
			    s->width - 1, s->height - 1);
    }
  else
    {
      for (i = 0; i < s->nchars; i++, ++s->gidx)
	if (mac_per_char_metric (GC_FONT (s->gc), s->char2b + i, 0) == NULL)
	  /* This is a nonexistent or zero-width glyph such as a
	     combining diacritic.  Draw a rectangle.  */
	  mac_draw_rectangle (s->f, s->gc,
			      x + s->cmp->offsets[s->gidx * 2], s->y,
			      FONT_WIDTH (GC_FONT (s->gc)) - 1, s->height - 1);
	else
	  mac_draw_image_string_16 (s->f, s->gc,
				    x + s->cmp->offsets[s->gidx * 2],
				    s->ybase - s->cmp->offsets[s->gidx * 2 + 1],
				    s->char2b + i, 1, 0, s->face->overstrike);
    }
}


#ifdef USE_X_TOOLKIT

static struct frame *x_frame_of_widget P_ ((Widget));


/* Return the frame on which widget WIDGET is used.. Abort if frame
   cannot be determined.  */

static struct frame *
x_frame_of_widget (widget)
     Widget widget;
{
  struct x_display_info *dpyinfo;
  Lisp_Object tail;
  struct frame *f;

  dpyinfo = x_display_info_for_display (XtDisplay (widget));

  /* Find the top-level shell of the widget.  Note that this function
     can be called when the widget is not yet realized, so XtWindow
     (widget) == 0.  That's the reason we can't simply use
     x_any_window_to_frame.  */
  while (!XtIsTopLevelShell (widget))
    widget = XtParent (widget);

  /* Look for a frame with that top-level widget.  Allocate the color
     on that frame to get the right gamma correction value.  */
  for (tail = Vframe_list; GC_CONSP (tail); tail = XCDR (tail))
    if (GC_FRAMEP (XCAR (tail))
	&& (f = XFRAME (XCAR (tail)),
	    (f->output_data.nothing != 1
	     && FRAME_X_DISPLAY_INFO (f) == dpyinfo))
	&& f->output_data.x->widget == widget)
      return f;

  abort ();
}


/* Allocate the color COLOR->pixel on the screen and display of
   widget WIDGET in colormap CMAP.  If an exact match cannot be
   allocated, try the nearest color available.  Value is non-zero
   if successful.  This is called from lwlib.  */

int
x_alloc_nearest_color_for_widget (widget, cmap, color)
     Widget widget;
     Colormap cmap;
     XColor *color;
{
  struct frame *f = x_frame_of_widget (widget);
  return x_alloc_nearest_color (f, cmap, color);
}


#endif /* USE_X_TOOLKIT */

#if 0 /* MAC_TODO */

/* Allocate the color COLOR->pixel on SCREEN of DISPLAY, colormap
   CMAP.  If an exact match can't be allocated, try the nearest color
   available.  Value is non-zero if successful.  Set *COLOR to the
   color allocated.  */

int
x_alloc_nearest_color (f, cmap, color)
     struct frame *f;
     Colormap cmap;
     XColor *color;
{
  Display *display = FRAME_X_DISPLAY (f);
  Screen *screen = FRAME_X_SCREEN (f);
  int rc;

  gamma_correct (f, color);
  rc = XAllocColor (display, cmap, color);
  if (rc == 0)
    {
      /* If we got to this point, the colormap is full, so we're going
	 to try to get the next closest color.  The algorithm used is
	 a least-squares matching, which is what X uses for closest
	 color matching with StaticColor visuals.  */
      int nearest, i;
      unsigned long nearest_delta = ~0;
      int ncells = XDisplayCells (display, XScreenNumberOfScreen (screen));
      XColor *cells = (XColor *) alloca (ncells * sizeof *cells);

      for (i = 0; i < ncells; ++i)
	cells[i].pixel = i;
      XQueryColors (display, cmap, cells, ncells);

      for (nearest = i = 0; i < ncells; ++i)
	{
	  long dred   = (color->red   >> 8) - (cells[i].red   >> 8);
	  long dgreen = (color->green >> 8) - (cells[i].green >> 8);
	  long dblue  = (color->blue  >> 8) - (cells[i].blue  >> 8);
	  unsigned long delta = dred * dred + dgreen * dgreen + dblue * dblue;

	  if (delta < nearest_delta)
	    {
	      nearest = i;
	      nearest_delta = delta;
	    }
	}

      color->red   = cells[nearest].red;
      color->green = cells[nearest].green;
      color->blue  = cells[nearest].blue;
      rc = XAllocColor (display, cmap, color);
    }

#ifdef DEBUG_X_COLORS
  if (rc)
    register_color (color->pixel);
#endif /* DEBUG_X_COLORS */

  return rc;
}


/* Allocate color PIXEL on frame F.  PIXEL must already be allocated.
   It's necessary to do this instead of just using PIXEL directly to
   get color reference counts right.  */

unsigned long
x_copy_color (f, pixel)
     struct frame *f;
     unsigned long pixel;
{
  XColor color;

  color.pixel = pixel;
  BLOCK_INPUT;
  XQueryColor (FRAME_X_DISPLAY (f), FRAME_X_COLORMAP (f), &color);
  XAllocColor (FRAME_X_DISPLAY (f), FRAME_X_COLORMAP (f), &color);
  UNBLOCK_INPUT;
#ifdef DEBUG_X_COLORS
  register_color (pixel);
#endif
  return color.pixel;
}


/* Allocate color PIXEL on display DPY.  PIXEL must already be allocated.
   It's necessary to do this instead of just using PIXEL directly to
   get color reference counts right.  */

unsigned long
x_copy_dpy_color (dpy, cmap, pixel)
     Display *dpy;
     Colormap cmap;
     unsigned long pixel;
{
  XColor color;

  color.pixel = pixel;
  BLOCK_INPUT;
  XQueryColor (dpy, cmap, &color);
  XAllocColor (dpy, cmap, &color);
  UNBLOCK_INPUT;
#ifdef DEBUG_X_COLORS
  register_color (pixel);
#endif
  return color.pixel;
}

#endif /* MAC_TODO */


/* Brightness beyond which a color won't have its highlight brightness
   boosted.

   Nominally, highlight colors for `3d' faces are calculated by
   brightening an object's color by a constant scale factor, but this
   doesn't yield good results for dark colors, so for colors who's
   brightness is less than this value (on a scale of 0-255) have to
   use an additional additive factor.

   The value here is set so that the default menu-bar/mode-line color
   (grey75) will not have its highlights changed at all.  */
#define HIGHLIGHT_COLOR_DARK_BOOST_LIMIT 187


/* Allocate a color which is lighter or darker than *COLOR by FACTOR
   or DELTA.  Try a color with RGB values multiplied by FACTOR first.
   If this produces the same color as COLOR, try a color where all RGB
   values have DELTA added.  Return the allocated color in *COLOR.
   DISPLAY is the X display, CMAP is the colormap to operate on.
   Value is non-zero if successful.  */

static int
mac_alloc_lighter_color (f, color, factor, delta)
     struct frame *f;
     unsigned long *color;
     double factor;
     int delta;
{
  unsigned long new;
  long bright;

  /* On Mac, RGB values are 0-255, not 0-65535, so scale delta. */
  delta /= 256;

  /* Change RGB values by specified FACTOR.  Avoid overflow!  */
  xassert (factor >= 0);
  new = RGB_TO_ULONG (min (0xff, (int) (factor * RED_FROM_ULONG (*color))),
                    min (0xff, (int) (factor * GREEN_FROM_ULONG (*color))),
                    min (0xff, (int) (factor * BLUE_FROM_ULONG (*color))));

  /* Calculate brightness of COLOR.  */
  bright = (2 * RED_FROM_ULONG (*color) + 3 * GREEN_FROM_ULONG (*color)
            + BLUE_FROM_ULONG (*color)) / 6;

  /* We only boost colors that are darker than
     HIGHLIGHT_COLOR_DARK_BOOST_LIMIT.  */
  if (bright < HIGHLIGHT_COLOR_DARK_BOOST_LIMIT)
    /* Make an additive adjustment to NEW, because it's dark enough so
       that scaling by FACTOR alone isn't enough.  */
    {
      /* How far below the limit this color is (0 - 1, 1 being darker).  */
      double dimness = 1 - (double)bright / HIGHLIGHT_COLOR_DARK_BOOST_LIMIT;
      /* The additive adjustment.  */
      int min_delta = delta * dimness * factor / 2;

      if (factor < 1)
        new = RGB_TO_ULONG (max (0, min (0xff, (int) (RED_FROM_ULONG (*color)) - min_delta)),
			    max (0, min (0xff, (int) (GREEN_FROM_ULONG (*color)) - min_delta)),
			    max (0, min (0xff, (int) (BLUE_FROM_ULONG (*color)) - min_delta)));
      else
        new = RGB_TO_ULONG (max (0, min (0xff, (int) (min_delta + RED_FROM_ULONG (*color)))),
			    max (0, min (0xff, (int) (min_delta + GREEN_FROM_ULONG (*color)))),
			    max (0, min (0xff, (int) (min_delta + BLUE_FROM_ULONG (*color)))));
    }

  if (new == *color)
    new = RGB_TO_ULONG (max (0, min (0xff, (int) (delta + RED_FROM_ULONG (*color)))),
                      max (0, min (0xff, (int) (delta + GREEN_FROM_ULONG (*color)))),
                      max (0, min (0xff, (int) (delta + BLUE_FROM_ULONG (*color)))));

  /* MAC_TODO: Map to palette and retry with delta if same? */
  /* MAC_TODO: Free colors (if using palette)? */

  if (new == *color)
    return 0;

  *color = new;

  return 1;
}


/* Set up the foreground color for drawing relief lines of glyph
   string S.  RELIEF is a pointer to a struct relief containing the GC
   with which lines will be drawn.  Use a color that is FACTOR or
   DELTA lighter or darker than the relief's background which is found
   in S->f->output_data.x->relief_background.  If such a color cannot
   be allocated, use DEFAULT_PIXEL, instead.  */

static void
x_setup_relief_color (f, relief, factor, delta, default_pixel)
     struct frame *f;
     struct relief *relief;
     double factor;
     int delta;
     unsigned long default_pixel;
{
  XGCValues xgcv;
  struct mac_output *di = f->output_data.mac;
  unsigned long mask = GCForeground;
  unsigned long pixel;
  unsigned long background = di->relief_background;
  struct mac_display_info *dpyinfo = FRAME_MAC_DISPLAY_INFO (f);

  /* MAC_TODO: Free colors (if using palette)? */

  /* Allocate new color.  */
  xgcv.foreground = default_pixel;
  pixel = background;
  if (dpyinfo->n_planes != 1
      && mac_alloc_lighter_color (f, &pixel, factor, delta))
    {
      relief->allocated_p = 1;
      xgcv.foreground = relief->pixel = pixel;
    }

  if (relief->gc == 0)
    {
#if 0 /* MAC_TODO: stipple */
      xgcv.stipple = dpyinfo->gray;
      mask |= GCStipple;
#endif
      relief->gc = XCreateGC (NULL, FRAME_MAC_WINDOW (f), mask, &xgcv);
    }
  else
    XChangeGC (NULL, relief->gc, mask, &xgcv);
}


/* Set up colors for the relief lines around glyph string S.  */

static void
x_setup_relief_colors (s)
     struct glyph_string *s;
{
  struct mac_output *di = s->f->output_data.mac;
  unsigned long color;

  if (s->face->use_box_color_for_shadows_p)
    color = s->face->box_color;
  else if (s->first_glyph->type == IMAGE_GLYPH
	   && s->img->pixmap
	   && !IMAGE_BACKGROUND_TRANSPARENT (s->img, s->f, 0))
    color = IMAGE_BACKGROUND (s->img, s->f, 0);
  else
    {
      XGCValues xgcv;

      /* Get the background color of the face.  */
      XGetGCValues (s->display, s->gc, GCBackground, &xgcv);
      color = xgcv.background;
    }

  if (di->white_relief.gc == 0
      || color != di->relief_background)
    {
      di->relief_background = color;
      x_setup_relief_color (s->f, &di->white_relief, 1.2, 0x8000,
			    WHITE_PIX_DEFAULT (s->f));
      x_setup_relief_color (s->f, &di->black_relief, 0.6, 0x4000,
			    BLACK_PIX_DEFAULT (s->f));
    }
}


/* Draw a relief on frame F inside the rectangle given by LEFT_X,
   TOP_Y, RIGHT_X, and BOTTOM_Y.  WIDTH is the thickness of the relief
   to draw, it must be >= 0.  RAISED_P non-zero means draw a raised
   relief.  LEFT_P non-zero means draw a relief on the left side of
   the rectangle.  RIGHT_P non-zero means draw a relief on the right
   side of the rectangle.  CLIP_RECT is the clipping rectangle to use
   when drawing.  */

static void
x_draw_relief_rect (f, left_x, top_y, right_x, bottom_y, width,
		    raised_p, top_p, bot_p, left_p, right_p, clip_rect)
     struct frame *f;
     int left_x, top_y, right_x, bottom_y, width;
     int top_p, bot_p, left_p, right_p, raised_p;
     Rect *clip_rect;
{
#if USE_APPKIT
  GC top_left_gc, bottom_right_gc;

  if (raised_p)
    {
      top_left_gc = f->output_data.mac->white_relief.gc;
      bottom_right_gc = f->output_data.mac->black_relief.gc;
    }
  else
    {
      top_left_gc = f->output_data.mac->black_relief.gc;
      bottom_right_gc = f->output_data.mac->white_relief.gc;
    }

  mac_set_clip_rectangles (f, top_left_gc, clip_rect, 1);
  mac_set_clip_rectangles (f, bottom_right_gc, clip_rect, 1);

  if (left_p)
    mac_fill_rectangle (f, top_left_gc, left_x, top_y,
			width, bottom_y + 1 - top_y);
  if (right_p)
    mac_fill_rectangle (f, bottom_right_gc, right_x + 1 - width, top_y,
			width, bottom_y + 1 - top_y);
  if (top_p)
    {
      if (!right_p)
	mac_fill_rectangle (f, top_left_gc, left_x, top_y,
			    right_x + 1 - left_x, width);
      else
	mac_fill_trapezoid_for_relief (f, top_left_gc, left_x, top_y,
				       right_x + 1 - left_x, width, 1);
    }
  if (bot_p)
    {
      if (!left_p)
	mac_fill_rectangle (f, bottom_right_gc, left_x, bottom_y + 1 - width,
			    right_x + 1 - left_x, width);
      else
	mac_fill_trapezoid_for_relief (f, bottom_right_gc,
				       left_x, bottom_y + 1 - width,
				       right_x + 1 - left_x, width, 0);
    }

  mac_set_clip_rectangles (f, top_left_gc, clip_rect, 1);
  mac_set_clip_rectangles (f, bottom_right_gc, clip_rect, 1);
#else
  int i;
  GC gc;

  if (raised_p)
    gc = f->output_data.mac->white_relief.gc;
  else
    gc = f->output_data.mac->black_relief.gc;
  mac_set_clip_rectangles (f, gc, clip_rect, 1);

  /* Top.  */
  if (top_p)
    for (i = 0; i < width; ++i)
      mac_draw_line (f, gc,
		     left_x + i * left_p, top_y + i,
		     right_x + 1 - i * right_p, top_y + i);

  /* Left.  */
  if (left_p)
    for (i = 0; i < width; ++i)
      mac_draw_line (f, gc,
		     left_x + i, top_y + i, left_x + i, bottom_y - i + 1);

  mac_reset_clip_rectangles (f, gc);
  if (raised_p)
    gc = f->output_data.mac->black_relief.gc;
  else
    gc = f->output_data.mac->white_relief.gc;
  mac_set_clip_rectangles (f, gc, clip_rect, 1);

  /* Bottom.  */
  if (bot_p)
    for (i = 0; i < width; ++i)
      mac_draw_line (f, gc,
		     left_x + i * left_p, bottom_y - i,
		     right_x + 1 - i * right_p, bottom_y - i);

  /* Right.  */
  if (right_p)
    for (i = 0; i < width; ++i)
      mac_draw_line (f, gc,
		     right_x - i, top_y + i + 1, right_x - i, bottom_y - i);

  mac_reset_clip_rectangles (f, gc);
#endif
}


/* Draw a box on frame F inside the rectangle given by LEFT_X, TOP_Y,
   RIGHT_X, and BOTTOM_Y.  WIDTH is the thickness of the lines to
   draw, it must be >= 0.  LEFT_P non-zero means draw a line on the
   left side of the rectangle.  RIGHT_P non-zero means draw a line
   on the right side of the rectangle.  CLIP_RECT is the clipping
   rectangle to use when drawing.  */

static void
x_draw_box_rect (s, left_x, top_y, right_x, bottom_y, width,
		 left_p, right_p, clip_rect)
     struct glyph_string *s;
     int left_x, top_y, right_x, bottom_y, width, left_p, right_p;
     Rect *clip_rect;
{
  XGCValues xgcv;

  XGetGCValues (s->display, s->gc, GCForeground, &xgcv);
  XSetForeground (s->display, s->gc, s->face->box_color);
  mac_set_clip_rectangles (s->f, s->gc, clip_rect, 1);

  /* Top.  */
  mac_fill_rectangle (s->f, s->gc, left_x, top_y,
		      right_x - left_x + 1, width);

  /* Left.  */
  if (left_p)
    mac_fill_rectangle (s->f, s->gc, left_x, top_y,
			width, bottom_y - top_y + 1);

  /* Bottom.  */
  mac_fill_rectangle (s->f, s->gc, left_x, bottom_y - width + 1,
		      right_x - left_x + 1, width);

  /* Right.  */
  if (right_p)
    mac_fill_rectangle (s->f, s->gc, right_x - width + 1,
			top_y, width, bottom_y - top_y + 1);

  XSetForeground (s->display, s->gc, xgcv.foreground);
  mac_reset_clip_rectangles (s->f, s->gc);
}


/* Draw a box around glyph string S.  */

static void
x_draw_glyph_string_box (s)
     struct glyph_string *s;
{
  int width, left_x, right_x, top_y, bottom_y, last_x, raised_p;
  int left_p, right_p;
  struct glyph *last_glyph;
  Rect clip_rect;

  last_x = ((s->row->full_width_p && !s->w->pseudo_window_p)
	    ? WINDOW_RIGHT_EDGE_X (s->w)
	    : window_box_right (s->w, s->area));

  /* The glyph that may have a right box line.  */
  last_glyph = (s->cmp || s->img
		? s->first_glyph
		: s->first_glyph + s->nchars - 1);

  width = abs (s->face->box_line_width);
  raised_p = s->face->box == FACE_RAISED_BOX;
  left_x = s->x;
  right_x = (s->row->full_width_p && s->extends_to_end_of_line_p
	     ? last_x - 1
	     : min (last_x, s->x + s->background_width) - 1);
  top_y = s->y;
  bottom_y = top_y + s->height - 1;

  left_p = (s->first_glyph->left_box_line_p
	    || (s->hl == DRAW_MOUSE_FACE
		&& (s->prev == NULL
		    || s->prev->hl != s->hl)));
  right_p = (last_glyph->right_box_line_p
	     || (s->hl == DRAW_MOUSE_FACE
		 && (s->next == NULL
		     || s->next->hl != s->hl)));

  get_glyph_string_clip_rect (s, &clip_rect);

  if (s->face->box == FACE_SIMPLE_BOX)
    x_draw_box_rect (s, left_x, top_y, right_x, bottom_y, width,
		     left_p, right_p, &clip_rect);
  else
    {
      x_setup_relief_colors (s);
      x_draw_relief_rect (s->f, left_x, top_y, right_x, bottom_y,
			  width, raised_p, 1, 1, left_p, right_p, &clip_rect);
    }
}


/* Draw foreground of image glyph string S.  */

static void
x_draw_image_foreground (s)
     struct glyph_string *s;
{
  int x = s->x;
  int y = s->ybase - image_ascent (s->img, s->face, &s->slice);

  /* If first glyph of S has a left box line, start drawing it to the
     right of that line.  */
  if (s->face->box != FACE_NO_BOX
      && s->first_glyph->left_box_line_p
      && s->slice.x == 0)
    x += abs (s->face->box_line_width);

  /* If there is a margin around the image, adjust x- and y-position
     by that margin.  */
  if (s->slice.x == 0)
    x += s->img->hmargin;
  if (s->slice.y == 0)
    y += s->img->vmargin;

  if (s->img->pixmap)
    {
      x_set_glyph_string_clipping (s);

#if USE_CG_DRAWING
      mac_draw_cg_image (s->img->data.ptr_val,
			 s->f, s->gc, s->slice.x, s->slice.y,
			 s->slice.width, s->slice.height, x, y, 1);
#endif
      if (s->img->mask)
#if !USE_CG_DRAWING
	mac_copy_area_with_mask (s->img->pixmap, s->img->mask,
				 s->f, s->gc, s->slice.x, s->slice.y,
				 s->slice.width, s->slice.height, x, y);
#else
	;
#endif
      else
	{
#if !USE_CG_DRAWING
	  mac_copy_area (s->img->pixmap,
			 s->f, s->gc, s->slice.x, s->slice.y,
			 s->slice.width, s->slice.height, x, y);
#endif

	  /* When the image has a mask, we can expect that at
	     least part of a mouse highlight or a block cursor will
	     be visible.  If the image doesn't have a mask, make
	     a block cursor visible by drawing a rectangle around
	     the image.  I believe it's looking better if we do
	     nothing here for mouse-face.  */
	  if (s->hl == DRAW_CURSOR)
	    {
	      int r = s->img->relief;
	      if (r < 0) r = -r;
	      mac_draw_rectangle (s->f, s->gc, x - r, y - r,
				  s->slice.width + r*2 - 1,
				  s->slice.height + r*2 - 1);
	    }
	}
    }
  else
    /* Draw a rectangle if image could not be loaded.  */
    mac_draw_rectangle (s->f, s->gc, x, y,
			s->slice.width - 1, s->slice.height - 1);
}


/* Draw a relief around the image glyph string S.  */

static void
x_draw_image_relief (s)
     struct glyph_string *s;
{
  int x0, y0, x1, y1, thick, raised_p;
  Rect r;
  int x = s->x;
  int y = s->ybase - image_ascent (s->img, s->face, &s->slice);

  /* If first glyph of S has a left box line, start drawing it to the
     right of that line.  */
  if (s->face->box != FACE_NO_BOX
      && s->first_glyph->left_box_line_p
      && s->slice.x == 0)
    x += abs (s->face->box_line_width);

  /* If there is a margin around the image, adjust x- and y-position
     by that margin.  */
  if (s->slice.x == 0)
    x += s->img->hmargin;
  if (s->slice.y == 0)
    y += s->img->vmargin;

  if (s->hl == DRAW_IMAGE_SUNKEN
      || s->hl == DRAW_IMAGE_RAISED)
    {
      thick = tool_bar_button_relief >= 0 ? tool_bar_button_relief : DEFAULT_TOOL_BAR_BUTTON_RELIEF;
      raised_p = s->hl == DRAW_IMAGE_RAISED;
    }
  else
    {
      thick = abs (s->img->relief);
      raised_p = s->img->relief > 0;
    }

  x0 = x - thick;
  y0 = y - thick;
  x1 = x + s->slice.width + thick - 1;
  y1 = y + s->slice.height + thick - 1;

  x_setup_relief_colors (s);
  get_glyph_string_clip_rect (s, &r);
  x_draw_relief_rect (s->f, x0, y0, x1, y1, thick, raised_p,
		      s->slice.y == 0,
		      s->slice.y + s->slice.height == s->img->height,
		      s->slice.x == 0,
		      s->slice.x + s->slice.width == s->img->width,
		      &r);
}


/* Draw part of the background of glyph string S.  X, Y, W, and H
   give the rectangle to draw.  */

static void
x_draw_glyph_string_bg_rect (s, x, y, w, h)
     struct glyph_string *s;
     int x, y, w, h;
{
#if 0 /* MAC_TODO: stipple */
  if (s->stippled_p)
    {
      /* Fill background with a stipple pattern.  */
      XSetFillStyle (s->display, s->gc, FillOpaqueStippled);
      XFillRectangle (s->display, s->window, s->gc, x, y, w, h);
      XSetFillStyle (s->display, s->gc, FillSolid);
    }
  else
#endif /* MAC_TODO */
    x_clear_glyph_string_rect (s, x, y, w, h);
}


/* Draw image glyph string S.

            s->y
   s->x      +-------------------------
	     |   s->face->box
	     |
	     |     +-------------------------
	     |     |  s->img->margin
	     |     |
	     |     |       +-------------------
	     |     |       |  the image

 */

static void
x_draw_image_glyph_string (s)
     struct glyph_string *s;
{
  int x, y;
  int box_line_hwidth = abs (s->face->box_line_width);
  int box_line_vwidth = max (s->face->box_line_width, 0);
  int height;

  height = s->height - 2 * box_line_vwidth;


  /* Fill background with face under the image.  Do it only if row is
     taller than image or if image has a clip mask to reduce
     flickering.  */
  s->stippled_p = s->face->stipple != 0;
  if (height > s->slice.height
      || s->img->hmargin
      || s->img->vmargin
      || s->img->mask
      || s->img->pixmap == 0
      || s->width != s->background_width)
    {
      x = s->x;
      if (s->first_glyph->left_box_line_p
	  && s->slice.x == 0)
	x += box_line_hwidth;

      y = s->y;
      if (s->slice.y == 0)
	y += box_line_vwidth;

      x_draw_glyph_string_bg_rect (s, x, y, s->background_width, height);

      s->background_filled_p = 1;
    }

  /* Draw the foreground.  */
  x_draw_image_foreground (s);

  /* If we must draw a relief around the image, do it.  */
  if (s->img->relief
      || s->hl == DRAW_IMAGE_RAISED
      || s->hl == DRAW_IMAGE_SUNKEN)
    x_draw_image_relief (s);
}


/* Draw stretch glyph string S.  */

static void
x_draw_stretch_glyph_string (s)
     struct glyph_string *s;
{
  xassert (s->first_glyph->type == STRETCH_GLYPH);

  if (s->hl == DRAW_CURSOR
      && !x_stretch_cursor_p)
    {
      /* If `x-stretch-block-cursor' is nil, don't draw a block cursor
	 as wide as the stretch glyph.  */
      int width, background_width = s->background_width;
      int x = s->x, left_x = window_box_left_offset (s->w, TEXT_AREA);

      if (x < left_x)
	{
	  background_width -= left_x - x;
	  x = left_x;
	}
      width = min (FRAME_COLUMN_WIDTH (s->f), background_width);

      /* Draw cursor.  */
      x_draw_glyph_string_bg_rect (s, x, s->y, width, s->height);

      /* Clear rest using the GC of the original non-cursor face.  */
      if (width < background_width)
	{
	  int y = s->y;
	  int w = background_width - width, h = s->height;
	  Rect r;
	  GC gc;

	  x += width;
	  if (s->row->mouse_face_p
	      && cursor_in_mouse_face_p (s->w))
	    {
	      x_set_mouse_face_gc (s);
	      gc = s->gc;
	    }
	  else
	    gc = s->face->gc;

	  get_glyph_string_clip_rect (s, &r);
	  mac_set_clip_rectangles (s->f, gc, &r, 1);

#if 0 /* MAC_TODO: stipple */
	  if (s->face->stipple)
	    {
	      /* Fill background with a stipple pattern.  */
	      XSetFillStyle (s->display, gc, FillOpaqueStippled);
	      XFillRectangle (s->display, s->window, gc, x, y, w, h);
	      XSetFillStyle (s->display, gc, FillSolid);
	    }
	  else
#endif /* MAC_TODO */
	    mac_erase_rectangle (s->f, gc, x, y, w, h);
	}
    }
  else if (!s->background_filled_p)
    {
      int background_width = s->background_width;
      int x = s->x, left_x = window_box_left_offset (s->w, TEXT_AREA);

      /* Don't draw into left margin, fringe or scrollbar area
         except for header line and mode line.  */
      if (x < left_x && !s->row->mode_line_p)
	{
	  background_width -= left_x - x;
	  x = left_x;
	}
      if (background_width > 0)
	x_draw_glyph_string_bg_rect (s, x, s->y, background_width, s->height);
    }

  s->background_filled_p = 1;
}


/* Draw glyph string S.  */

static void
x_draw_glyph_string (s)
     struct glyph_string *s;
{
  int relief_drawn_p = 0;

  /* If S draws into the background of its successor that does not
     draw a cursor, draw the background of the successor first so that
     S can draw into it.  This makes S->next use XDrawString instead
     of XDrawImageString.  */
  if (s->next && s->right_overhang && !s->for_overlaps
      && s->next->first_glyph->type != IMAGE_GLYPH
      && s->next->hl != DRAW_CURSOR)
    {
      x_set_glyph_string_gc (s->next);
      x_set_glyph_string_clipping (s->next);
      if (s->next->first_glyph->type == STRETCH_GLYPH)
	x_draw_stretch_glyph_string (s->next);
      else
	x_draw_glyph_string_background (s->next, 1);
    }

  /* Set up S->gc, set clipping and draw S.  */
  x_set_glyph_string_gc (s);

  /* Draw relief (if any) in advance for char/composition so that the
     glyph string can be drawn over it.  */
  if (!s->for_overlaps
      && s->face->box != FACE_NO_BOX
      && (s->first_glyph->type == CHAR_GLYPH
	  || s->first_glyph->type == COMPOSITE_GLYPH))

    {
      x_set_glyph_string_clipping (s);
      x_draw_glyph_string_background (s, 1);
      x_draw_glyph_string_box (s);
      x_set_glyph_string_clipping (s);
      relief_drawn_p = 1;
    }
  else
    x_set_glyph_string_clipping (s);

  switch (s->first_glyph->type)
    {
    case IMAGE_GLYPH:
      x_draw_image_glyph_string (s);
      break;

    case STRETCH_GLYPH:
      x_draw_stretch_glyph_string (s);
      break;

    case CHAR_GLYPH:
      if (s->for_overlaps)
	s->background_filled_p = 1;
      else
	x_draw_glyph_string_background (s, 0);
      x_draw_glyph_string_foreground (s);
      break;

    case COMPOSITE_GLYPH:
      if (s->for_overlaps || s->gidx > 0)
	s->background_filled_p = 1;
      else
	x_draw_glyph_string_background (s, 1);
      x_draw_composite_glyph_string_foreground (s);
      break;

    default:
      abort ();
    }

  if (!s->for_overlaps)
    {
      /* Draw underline.  */
      if (s->face->underline_p)
	{
	  unsigned long tem, h;
	  int y;

#if 0
	  /* Get the underline thickness.  Default is 1 pixel.  */
	  if (!XGetFontProperty (s->font, XA_UNDERLINE_THICKNESS, &h))
#endif
	    h = 1;

	  y = s->y + s->height - h;
	  if (!x_underline_at_descent_line)
            {
	      /* Get the underline position.  This is the recommended
                 vertical offset in pixels from the baseline to the top of
                 the underline.  This is a signed value according to the
                 specs, and its default is

	         ROUND ((maximum descent) / 2), with
	         ROUND(x) = floor (x + 0.5)  */

#if 0
              if (x_use_underline_position_properties
                  && XGetFontProperty (s->font, XA_UNDERLINE_POSITION, &tem))
                y = s->ybase + (long) tem;
              else
#endif
	      if (s->face->font)
                y = s->ybase + (s->face->font->max_bounds.descent + 1) / 2;
            }

	  if (s->face->underline_defaulted_p)
	    mac_fill_rectangle (s->f, s->gc, s->x, y,
				s->background_width, h);
	  else
	    {
	      XGCValues xgcv;
	      XGetGCValues (s->display, s->gc, GCForeground, &xgcv);
	      XSetForeground (s->display, s->gc, s->face->underline_color);
	      mac_fill_rectangle (s->f, s->gc, s->x, y,
				  s->background_width, h);
	      XSetForeground (s->display, s->gc, xgcv.foreground);
	    }
	}

      /* Draw overline.  */
      if (s->face->overline_p)
	{
	  unsigned long dy = 0, h = 1;

	  if (s->face->overline_color_defaulted_p)
	    mac_fill_rectangle (s->f, s->gc, s->x, s->y + dy,
				s->background_width, h);
	  else
	    {
	      XGCValues xgcv;
	      XGetGCValues (s->display, s->gc, GCForeground, &xgcv);
	      XSetForeground (s->display, s->gc, s->face->overline_color);
	      mac_fill_rectangle (s->f, s->gc, s->x, s->y + dy,
				  s->background_width, h);
	      XSetForeground (s->display, s->gc, xgcv.foreground);
	    }
	}

      /* Draw strike-through.  */
      if (s->face->strike_through_p)
	{
	  unsigned long h = 1;
	  unsigned long dy = (s->height - h) / 2;

	  if (s->face->strike_through_color_defaulted_p)
	    mac_fill_rectangle (s->f, s->gc, s->x, s->y + dy,
				s->width, h);
	  else
	    {
	      XGCValues xgcv;
	      XGetGCValues (s->display, s->gc, GCForeground, &xgcv);
	      XSetForeground (s->display, s->gc, s->face->strike_through_color);
	      mac_fill_rectangle (s->f, s->gc, s->x, s->y + dy,
				  s->width, h);
	      XSetForeground (s->display, s->gc, xgcv.foreground);
	    }
	}

      /* Draw relief if not yet drawn.  */
      if (!relief_drawn_p && s->face->box != FACE_NO_BOX)
	x_draw_glyph_string_box (s);
    }

  /* Reset clipping.  */
  mac_reset_clip_rectangles (s->f, s->gc);
}

/* Shift display to make room for inserted glyphs.   */

void
mac_shift_glyphs_for_insert (f, x, y, width, height, shift_by)
     struct frame *f;
     int x, y, width, height, shift_by;
{
  mac_scroll_area (f, f->output_data.mac->normal_gc,
		   x, y, width, height,
		   x + shift_by, y);
}

/* Delete N glyphs at the nominal cursor position.  Not implemented
   for X frames.  */

static void
x_delete_glyphs (n)
     register int n;
{
  abort ();
}


/* Clear entire frame.  If updating_frame is non-null, clear that
   frame.  Otherwise clear the selected frame.  */

static void
x_clear_frame ()
{
  struct frame *f;

  if (updating_frame)
    f = updating_frame;
  else
    f = SELECTED_FRAME ();

  /* Clearing the frame will erase any cursor, so mark them all as no
     longer visible.  */
  mark_window_cursors_off (XWINDOW (FRAME_ROOT_WINDOW (f)));
  output_cursor.hpos = output_cursor.vpos = 0;
  output_cursor.x = -1;

  /* We don't set the output cursor here because there will always
     follow an explicit cursor_to.  */
  BLOCK_INPUT;
  mac_clear_window (f);

  /* We have to clear the scroll bars, too.  If we have changed
     colors or something like that, then they should be notified.  */
  x_scroll_bar_clear (f);

  XFlush (FRAME_MAC_DISPLAY (f));
  UNBLOCK_INPUT;
}



/* Invert the middle quarter of the frame for .15 sec.  */

/* We use the select system call to do the waiting, so we have to make
   sure it's available.  If it isn't, we just won't do visual bells.  */

#if defined (HAVE_TIMEVAL) && defined (HAVE_SELECT)


/* Subtract the `struct timeval' values X and Y, storing the result in
   *RESULT.  Return 1 if the difference is negative, otherwise 0.  */

static int
timeval_subtract (result, x, y)
     struct timeval *result, x, y;
{
  /* Perform the carry for the later subtraction by updating y.  This
     is safer because on some systems the tv_sec member is unsigned.  */
  if (x.tv_usec < y.tv_usec)
    {
      int nsec = (y.tv_usec - x.tv_usec) / 1000000 + 1;
      y.tv_usec -= 1000000 * nsec;
      y.tv_sec += nsec;
    }

  if (x.tv_usec - y.tv_usec > 1000000)
    {
      int nsec = (y.tv_usec - x.tv_usec) / 1000000;
      y.tv_usec += 1000000 * nsec;
      y.tv_sec -= nsec;
    }

  /* Compute the time remaining to wait.  tv_usec is certainly
     positive.  */
  result->tv_sec = x.tv_sec - y.tv_sec;
  result->tv_usec = x.tv_usec - y.tv_usec;

  /* Return indication of whether the result should be considered
     negative.  */
  return x.tv_sec < y.tv_sec;
}

void
XTflash (f)
     struct frame *f;
{
  /* Get the height not including a menu bar widget.  */
  int height = FRAME_TEXT_LINES_TO_PIXEL_HEIGHT (f, FRAME_LINES (f));
  /* Height of each line to flash.  */
  int flash_height = FRAME_LINE_HEIGHT (f);
  /* These will be the left and right margins of the rectangles.  */
  int flash_left = FRAME_INTERNAL_BORDER_WIDTH (f);
  int flash_right = FRAME_PIXEL_WIDTH (f) - FRAME_INTERNAL_BORDER_WIDTH (f);
  int width;
  Rect rects[2];
  int nrects;

  /* Don't flash the area between a scroll bar and the frame
     edge it is next to.  */
  switch (FRAME_VERTICAL_SCROLL_BAR_TYPE (f))
    {
    case vertical_scroll_bar_left:
      flash_left += VERTICAL_SCROLL_BAR_WIDTH_TRIM;
      break;

    case vertical_scroll_bar_right:
      flash_right -= VERTICAL_SCROLL_BAR_WIDTH_TRIM;
      break;

    default:
      break;
    }

  width = flash_right - flash_left;

  if (height > 3 * FRAME_LINE_HEIGHT (f))
    {
      /* If window is tall, flash top and bottom line.  */
      rects[0].left = rects[1].left = flash_left;
      rects[0].top = (FRAME_INTERNAL_BORDER_WIDTH (f)
		      + FRAME_TOOL_BAR_LINES (f) * FRAME_LINE_HEIGHT (f));
      rects[0].right = rects[1].right = flash_left + width;
      rects[0].bottom = rects[0].top + flash_height;
      rects[1].top = height - flash_height - FRAME_INTERNAL_BORDER_WIDTH (f);
      rects[1].bottom = rects[1].top + flash_height;
      nrects = 2;
    }
  else
    {
      /* If it is short, flash it all.  */
      rects[0].left = flash_left;
      rects[0].top = FRAME_INTERNAL_BORDER_WIDTH (f);
      rects[0].right = rects[0].left + width;
      rects[0].bottom = rects[0].top + (height
					- 2 * FRAME_INTERNAL_BORDER_WIDTH (f));
      nrects = 1;
    }

  BLOCK_INPUT;

  mac_invert_rectangles (f, rects, nrects);

  x_flush (f);

  {
    struct timeval wakeup;

    EMACS_GET_TIME (wakeup);

    /* Compute time to wait until, propagating carry from usecs.  */
    wakeup.tv_usec += 150000;
    wakeup.tv_sec += (wakeup.tv_usec / 1000000);
    wakeup.tv_usec %= 1000000;

    /* Keep waiting until past the time wakeup or any input gets
       available.  */
    while (! detect_input_pending ())
      {
	struct timeval current;
	struct timeval timeout;

	EMACS_GET_TIME (current);

	/* Break if result would be negative.  */
	if (timeval_subtract (&current, wakeup, current))
	  break;

	/* How long `select' should wait.  */
	timeout.tv_sec = 0;
	timeout.tv_usec = 10000;

	/* Try to wait that long--but we might wake up sooner.  */
	select (0, NULL, NULL, NULL, &timeout);
      }
  }

  mac_invert_rectangles (f, rects, nrects);

  x_flush (f);

  UNBLOCK_INPUT;
}

#endif /* defined (HAVE_TIMEVAL) && defined (HAVE_SELECT) */


/* Make audible bell.  */

void
XTring_bell ()
{
  struct frame *f = SELECTED_FRAME ();

#if defined (HAVE_TIMEVAL) && defined (HAVE_SELECT)
  if (visible_bell)
    XTflash (f);
  else
#endif
    {
      BLOCK_INPUT;
      mac_alert_sound_play ();
      XFlush (FRAME_MAC_DISPLAY (f));
      UNBLOCK_INPUT;
    }
}


/* Specify how many text lines, from the top of the window,
   should be affected by insert-lines and delete-lines operations.
   This, and those operations, are used only within an update
   that is bounded by calls to x_update_begin and x_update_end.  */

static void
XTset_terminal_window (n)
     register int n;
{
  /* This function intentionally left blank.  */
}



/***********************************************************************
			      Line Dance
 ***********************************************************************/

/* Perform an insert-lines or delete-lines operation, inserting N
   lines or deleting -N lines at vertical position VPOS.  */

static void
x_ins_del_lines (vpos, n)
     int vpos, n;
{
  abort ();
}


/* Scroll part of the display as described by RUN.  */

static void
x_scroll_run (w, run)
     struct window *w;
     struct run *run;
{
  struct frame *f = XFRAME (w->frame);
  int x, y, width, height, from_y, to_y, bottom_y;

  /* Get frame-relative bounding box of the text display area of W,
     without mode lines.  Include in this box the left and right
     fringe of W.  */
  window_box (w, -1, &x, &y, &width, &height);

  from_y = WINDOW_TO_FRAME_PIXEL_Y (w, run->current_y);
  to_y = WINDOW_TO_FRAME_PIXEL_Y (w, run->desired_y);
  bottom_y = y + height;

  if (to_y < from_y)
    {
      /* Scrolling up.  Make sure we don't copy part of the mode
	 line at the bottom.  */
      if (from_y + run->height > bottom_y)
	height = bottom_y - from_y;
      else
	height = run->height;
    }
  else
    {
      /* Scolling down.  Make sure we don't copy over the mode line.
	 at the bottom.  */
      if (to_y + run->height > bottom_y)
	height = bottom_y - to_y;
      else
	height = run->height;
    }

  BLOCK_INPUT;

  /* Cursor off.  Will be switched on again in x_update_window_end.  */
  updated_window = w;
  x_clear_cursor (w);

  mac_scroll_area (f, f->output_data.mac->normal_gc,
		   x, from_y,
		   width, height,
		   x, to_y);

  UNBLOCK_INPUT;
}



/***********************************************************************
			   Exposure Events
 ***********************************************************************/


static void
frame_highlight (f)
     struct frame *f;
{
  x_update_cursor (f, 1);
}

static void
frame_unhighlight (f)
     struct frame *f;
{
  x_update_cursor (f, 1);
}

/* The focus has changed.  Update the frames as necessary to reflect
   the new situation.  Note that we can't change the selected frame
   here, because the Lisp code we are interrupting might become confused.
   Each event gets marked with the frame in which it occurred, so the
   Lisp code can tell when the switch took place by examining the events.  */

static void
x_new_focus_frame (dpyinfo, frame)
     struct x_display_info *dpyinfo;
     struct frame *frame;
{
  struct frame *old_focus = dpyinfo->x_focus_frame;

  if (frame != dpyinfo->x_focus_frame)
    {
      /* Set this before calling other routines, so that they see
	 the correct value of x_focus_frame.  */
      dpyinfo->x_focus_frame = frame;

      if (old_focus && old_focus->auto_lower)
	x_lower_frame (old_focus);

#if 0
      selected_frame = frame;
      XSETFRAME (XWINDOW (selected_frame->selected_window)->frame,
		 selected_frame);
      Fselect_window (selected_frame->selected_window, Qnil);
      choose_minibuf_frame ();
#endif /* ! 0 */

      if (dpyinfo->x_focus_frame && dpyinfo->x_focus_frame->auto_raise)
	pending_autoraise_frame = dpyinfo->x_focus_frame;
      else
	pending_autoraise_frame = 0;

#if USE_MAC_FONT_PANEL
      if (frame)
	mac_set_font_info_for_selection (frame, DEFAULT_FACE_ID, 0);
#endif
    }

  x_frame_rehighlight (dpyinfo);
}

/* Handle FocusIn and FocusOut state changes for FRAME.
   If FRAME has focus and there exists more than one frame, puts
   a FOCUS_IN_EVENT into *BUFP.  */

void
mac_focus_changed (type, dpyinfo, frame, bufp)
     int type;
     struct mac_display_info *dpyinfo;
     struct frame *frame;
     struct input_event *bufp;
{
  if (type == activeFlag)
    {
      if (dpyinfo->x_focus_event_frame != frame)
        {
          x_new_focus_frame (dpyinfo, frame);
          dpyinfo->x_focus_event_frame = frame;

          /* Don't stop displaying the initial startup message
             for a switch-frame event we don't need.  */
          if (GC_NILP (Vterminal_frame)
              && GC_CONSP (Vframe_list)
              && !GC_NILP (XCDR (Vframe_list)))
            {
              bufp->kind = FOCUS_IN_EVENT;
              XSETFRAME (bufp->frame_or_window, frame);
            }
        }
    }
  else
    {
      if (dpyinfo->x_focus_event_frame == frame)
        {
          dpyinfo->x_focus_event_frame = 0;
          x_new_focus_frame (dpyinfo, 0);
        }
    }
}

/* Handle an event saying the mouse has moved out of an Emacs frame.  */

void
x_mouse_leave (dpyinfo)
     struct x_display_info *dpyinfo;
{
  x_new_focus_frame (dpyinfo, dpyinfo->x_focus_event_frame);
}

/* The focus has changed, or we have redirected a frame's focus to
   another frame (this happens when a frame uses a surrogate
   mini-buffer frame).  Shift the highlight as appropriate.

   The FRAME argument doesn't necessarily have anything to do with which
   frame is being highlighted or un-highlighted; we only use it to find
   the appropriate X display info.  */

static void
XTframe_rehighlight (frame)
     struct frame *frame;
{
  x_frame_rehighlight (FRAME_X_DISPLAY_INFO (frame));
}

static void
x_frame_rehighlight (dpyinfo)
     struct x_display_info *dpyinfo;
{
  struct frame *old_highlight = dpyinfo->x_highlight_frame;

  if (dpyinfo->x_focus_frame)
    {
      dpyinfo->x_highlight_frame
	= ((GC_FRAMEP (FRAME_FOCUS_FRAME (dpyinfo->x_focus_frame)))
	   ? XFRAME (FRAME_FOCUS_FRAME (dpyinfo->x_focus_frame))
	   : dpyinfo->x_focus_frame);
      if (! FRAME_LIVE_P (dpyinfo->x_highlight_frame))
	{
	  FRAME_FOCUS_FRAME (dpyinfo->x_focus_frame) = Qnil;
	  dpyinfo->x_highlight_frame = dpyinfo->x_focus_frame;
	}
    }
  else
    dpyinfo->x_highlight_frame = 0;

  if (dpyinfo->x_highlight_frame != old_highlight)
    {
      if (old_highlight)
	frame_unhighlight (old_highlight);
      if (dpyinfo->x_highlight_frame)
	frame_highlight (dpyinfo->x_highlight_frame);
    }
}



/* Convert a keysym to its name.  */

char *
x_get_keysym_name (keysym)
     int keysym;
{
  char *value;

  BLOCK_INPUT;
#if 0
  value = XKeysymToString (keysym);
#else
  value = 0;
#endif
  UNBLOCK_INPUT;

  return value;
}



/* Function to report a mouse movement to the mainstream Emacs code.
   The input handler calls this.

   We have received a mouse movement event, which is given in *event.
   If the mouse is over a different glyph than it was last time, tell
   the mainstream emacs code by setting mouse_moved.  If not, ask for
   another motion event, so we can check again the next time it moves.  */

static Point last_mouse_motion_position;
static Lisp_Object last_mouse_motion_frame;

int
note_mouse_movement (frame, pos)
     FRAME_PTR frame;
     Point *pos;
{
  struct mac_display_info *dpyinfo = FRAME_MAC_DISPLAY_INFO (frame);
  Rect r;

  last_mouse_movement_time = TickCount () * (1000 / 60);  /* to milliseconds */
  last_mouse_motion_position = *pos;
  XSETFRAME (last_mouse_motion_frame, frame);

  if (frame == dpyinfo->mouse_face_mouse_frame
      && !PtInRect (*pos, mac_get_frame_bounds (frame, &r)))
    {
      /* This case corresponds to LeaveNotify in X11.  If we move
	 outside the frame, then we're certainly no longer on any text
	 in the frame.  */
      clear_mouse_face (dpyinfo);
      dpyinfo->mouse_face_mouse_frame = 0;
      if (!dpyinfo->grabbed)
	rif->define_frame_cursor (frame,
				  frame->output_data.mac->nontext_cursor);
    }

  /* Has the mouse moved off the glyph it was on at the last sighting?  */
  if (frame != last_mouse_glyph_frame
      || !PtInRect (*pos, &last_mouse_glyph))
    {
      frame->mouse_moved = 1;
      last_mouse_scroll_bar = Qnil;
      note_mouse_highlight (frame, pos->h, pos->v);
      /* Remember which glyph we're now on.  */
      remember_mouse_glyph (frame, pos->h, pos->v, &last_mouse_glyph);
      last_mouse_glyph_frame = frame;
      return 1;
    }

  return 0;
}


/************************************************************************
			      Mouse Face
 ************************************************************************/

/* MAC TODO:  This should be called from somewhere (or removed)  ++KFS */

static void
redo_mouse_highlight ()
{
  if (!NILP (last_mouse_motion_frame)
      && FRAME_LIVE_P (XFRAME (last_mouse_motion_frame)))
    note_mouse_highlight (XFRAME (last_mouse_motion_frame),
			  last_mouse_motion_position.h,
			  last_mouse_motion_position.v);
}


struct frame *
mac_focus_frame (dpyinfo)
     struct mac_display_info *dpyinfo;
{
  if (dpyinfo->x_focus_frame)
    return dpyinfo->x_focus_frame;
  else
    /* Mac version may get events, such as a menu bar click, even when
       all the frames are invisible.  In this case, we regard the
       event came to the selected frame.  */
    return SELECTED_FRAME ();
}


/* Return the current position of the mouse.
   *FP should be a frame which indicates which display to ask about.

   If the mouse movement started in a scroll bar, set *FP, *BAR_WINDOW,
   and *PART to the frame, window, and scroll bar part that the mouse
   is over.  Set *X and *Y to the portion and whole of the mouse's
   position on the scroll bar.

   If the mouse movement started elsewhere, set *FP to the frame the
   mouse is on, *BAR_WINDOW to nil, and *X and *Y to the character cell
   the mouse is over.

   Set *TIME to the server time-stamp for the time at which the mouse
   was at this position.

   Don't store anything if we don't have a valid set of values to report.

   This clears the mouse_moved flag, so we can wait for the next mouse
   movement.  */

static void
XTmouse_position (fp, insist, bar_window, part, x, y, time)
     FRAME_PTR *fp;
     int insist;
     Lisp_Object *bar_window;
     enum scroll_bar_part *part;
     Lisp_Object *x, *y;
     unsigned long *time;
{
  FRAME_PTR f1;

  BLOCK_INPUT;

#if !USE_APPKIT
  if (! NILP (last_mouse_scroll_bar) && insist == 0)
    x_scroll_bar_report_motion (fp, bar_window, part, x, y, time);
  else
#endif	/* !USE_APPKIT */
    {
      Lisp_Object frame, tail;

      /* Clear the mouse-moved flag for every frame on this display.  */
      FOR_EACH_FRAME (tail, frame)
	XFRAME (frame)->mouse_moved = 0;

      last_mouse_scroll_bar = Qnil;

      if (FRAME_MAC_DISPLAY_INFO (*fp)->grabbed && last_mouse_frame
	  && FRAME_LIVE_P (last_mouse_frame))
	f1 = last_mouse_frame;
      else
	f1 = mac_focus_frame (FRAME_MAC_DISPLAY_INFO (*fp));

      if (f1)
	{
	  /* Ok, we found a frame.  Store all the values.
	     last_mouse_glyph is a rectangle used to reduce the
	     generation of mouse events.  To not miss any motion
	     events, we must divide the frame into rectangles of the
	     size of the smallest character that could be displayed
	     on it, i.e. into the same rectangles that matrices on
	     the frame are divided into.  */
	  Point mouse_pos;

	  mac_get_frame_mouse (f1, &mouse_pos);
	  remember_mouse_glyph (f1, mouse_pos.h, mouse_pos.v,
				&last_mouse_glyph);
	  last_mouse_glyph_frame = f1;

	  *bar_window = Qnil;
	  *part = 0;
	  *fp = f1;
	  XSETINT (*x, mouse_pos.h);
	  XSETINT (*y, mouse_pos.v);
	  *time = last_mouse_movement_time;
	}
    }

  UNBLOCK_INPUT;
}


/************************************************************************
			 Scroll bars, general
 ************************************************************************/

/* Create a scroll bar and return the scroll bar vector for it.  W is
   the Emacs window on which to create the scroll bar. TOP, LEFT,
   WIDTH and HEIGHT are the pixel coordinates and dimensions of the
   scroll bar. */

static struct scroll_bar *
x_scroll_bar_create (w, top, left, width, height, disp_top, disp_height)
     struct window *w;
     int top, left, width, height, disp_top, disp_height;
{
  struct frame *f = XFRAME (w->frame);
  struct scroll_bar *bar
    = XSCROLL_BAR (Fmake_vector (make_number (SCROLL_BAR_VEC_SIZE), Qnil));
  Rect r;

  BLOCK_INPUT;

  XSETWINDOW (bar->window, w);
  XSETINT (bar->top, top);
  XSETINT (bar->left, left);
  XSETINT (bar->width, width);
  XSETINT (bar->height, height);
#ifdef MAC_OSX
  bar->fringe_extended_p = Qnil;
#endif
  bar->redraw_needed_p = Qnil;

  SetRect (&r, left, disp_top, left + width, disp_top + disp_height);
  mac_create_scroll_bar (bar, &r,
#ifdef USE_TOOLKIT_SCROLL_BARS
			 false
#else
			 width < disp_height
#endif
			 );

  /* Add bar to its frame's list of scroll bars.  */
  bar->next = FRAME_SCROLL_BARS (f);
  bar->prev = Qnil;
  XSETVECTOR (FRAME_SCROLL_BARS (f), bar);
  if (!NILP (bar->next))
    XSETVECTOR (XSCROLL_BAR (bar->next)->prev, bar);

  UNBLOCK_INPUT;
  return bar;
}


/* Destroy scroll bar BAR, and set its Emacs window's scroll bar to
   nil.  */

static void
x_scroll_bar_remove (bar)
     struct scroll_bar *bar;
{
  BLOCK_INPUT;

  /* Destroy the Mac scroll bar control  */
  mac_dispose_scroll_bar (bar);

  /* Disassociate this scroll bar from its window.  */
  XWINDOW (bar->window)->vertical_scroll_bar = Qnil;

  UNBLOCK_INPUT;
}


/* Set the handle of the vertical scroll bar for WINDOW to indicate
   that we are displaying PORTION characters out of a total of WHOLE
   characters, starting at POSITION.  If WINDOW has no scroll bar,
   create one.  */

static void
XTset_vertical_scroll_bar (w, portion, whole, position)
     struct window *w;
     int portion, whole, position;
{
  struct frame *f = XFRAME (w->frame);
  struct scroll_bar *bar;
  int top, height, left, sb_left, width, sb_width, disp_top, disp_height;
  int window_y, window_height;
#ifdef MAC_OSX
  int fringe_extended_p;
#endif

  /* Get window dimensions.  */
  window_box (w, -1, 0, &window_y, 0, &window_height);
  top = window_y;
  width = WINDOW_CONFIG_SCROLL_BAR_COLS (w) * FRAME_COLUMN_WIDTH (f);
  height = window_height;

  /* Compute the left edge of the scroll bar area.  */
  left = WINDOW_SCROLL_BAR_AREA_X (w);

  /* Compute the width of the scroll bar which might be less than
     the width of the area reserved for the scroll bar.  */
  if (WINDOW_CONFIG_SCROLL_BAR_WIDTH (w) > 0)
    sb_width = WINDOW_CONFIG_SCROLL_BAR_WIDTH (w);
  else
    sb_width = width;

  /* Compute the left edge of the scroll bar.  */
  if (WINDOW_HAS_VERTICAL_SCROLL_BAR_ON_RIGHT (w))
    sb_left = left + (WINDOW_RIGHTMOST_P (w) ? width - sb_width : 0);
  else
    sb_left = left + (WINDOW_LEFTMOST_P (w) ? 0 : width - sb_width);

  /* Adjustments according to Inside Macintosh to make it look nice */
  disp_top = top;
  disp_height = height;
#ifdef MAC_OS8
  if (disp_top == 0)
    {
      disp_top = -1;
      disp_height++;
    }
  else if (disp_top == FRAME_PIXEL_HEIGHT (f) - 16)
    {
      disp_top++;
      disp_height--;
    }

  if (sb_left + sb_width == FRAME_PIXEL_WIDTH (f))
    sb_left++;
#endif

#ifdef MAC_OSX
  if (WINDOW_HAS_VERTICAL_SCROLL_BAR_ON_LEFT (w))
    fringe_extended_p = (WINDOW_LEFTMOST_P (w)
			 && WINDOW_LEFT_FRINGE_WIDTH (w)
			 && (WINDOW_HAS_FRINGES_OUTSIDE_MARGINS (w)
			     || WINDOW_LEFT_MARGIN_COLS (w) == 0));
  else
    fringe_extended_p = (WINDOW_RIGHTMOST_P (w)
			 && WINDOW_RIGHT_FRINGE_WIDTH (w)
			 && (WINDOW_HAS_FRINGES_OUTSIDE_MARGINS (w)
			     || WINDOW_RIGHT_MARGIN_COLS (w) == 0));
#endif

  /* Does the scroll bar exist yet?  */
  if (NILP (w->vertical_scroll_bar))
    {
      BLOCK_INPUT;
#ifdef MAC_OSX
      if (fringe_extended_p)
	mac_clear_area (f, sb_left, top, sb_width, height);
      else
#endif
	mac_clear_area (f, left, top, width, height);
      UNBLOCK_INPUT;
      bar = x_scroll_bar_create (w, top, sb_left, sb_width, height, disp_top,
				 disp_height);
      XSETVECTOR (w->vertical_scroll_bar, bar);
    }
  else
    {
      /* It may just need to be moved and resized.  */
      bar = XSCROLL_BAR (w->vertical_scroll_bar);

      BLOCK_INPUT;

      /* If already correctly positioned, do nothing.  */
      if (XINT (bar->left) == sb_left
	  && XINT (bar->top) == top
	  && XINT (bar->width) == sb_width
	  && XINT (bar->height) == height
#ifdef MAC_OSX
	  && !NILP (bar->fringe_extended_p) == fringe_extended_p
#endif
	  )
	{
	  if (!NILP (bar->redraw_needed_p))
	    mac_redraw_scroll_bar (bar);
	}
      else
	{
	  Rect r;

	  /* Since toolkit scroll bars are smaller than the space reserved
	     for them on the frame, we have to clear "under" them.  */
#ifdef MAC_OSX
	  if (fringe_extended_p)
	    mac_clear_area (f, sb_left, top, sb_width, height);
	  else
#endif
	    mac_clear_area (f, left, top, width, height);

	  SetRect (&r, sb_left + VERTICAL_SCROLL_BAR_WIDTH_TRIM, disp_top,
		   sb_left + sb_width - VERTICAL_SCROLL_BAR_WIDTH_TRIM,
		   disp_top + disp_height);
	  mac_set_scroll_bar_bounds (bar, &r);

          /* Remember new settings.  */
          XSETINT (bar->left, sb_left);
          XSETINT (bar->top, top);
          XSETINT (bar->width, sb_width);
          XSETINT (bar->height, height);
        }

      UNBLOCK_INPUT;
    }

#ifdef MAC_OSX
  bar->fringe_extended_p = fringe_extended_p ? Qt : Qnil;
#endif
  bar->redraw_needed_p = Qnil;

#ifdef USE_TOOLKIT_SCROLL_BARS
  x_set_toolkit_scroll_bar_thumb (bar, portion, position, whole);
#else /* not USE_TOOLKIT_SCROLL_BARS */
  /* Set the scroll bar's current state, unless we're currently being
     dragged.  */
  if (NILP (bar->dragging))
    {
      int top_range = VERTICAL_SCROLL_BAR_TOP_RANGE (f, height);

      if (whole == 0)
	x_scroll_bar_set_handle (bar, 0, top_range, 0);
      else
	{
	  int start = ((double) position * top_range) / whole;
	  int end = ((double) (position + portion) * top_range) / whole;
	  x_scroll_bar_set_handle (bar, start, end, 0);
	}
    }
#endif /* not USE_TOOLKIT_SCROLL_BARS */
}


/* The following three hooks are used when we're doing a thorough
   redisplay of the frame.  We don't explicitly know which scroll bars
   are going to be deleted, because keeping track of when windows go
   away is a real pain - "Can you say set-window-configuration, boys
   and girls?"  Instead, we just assert at the beginning of redisplay
   that *all* scroll bars are to be removed, and then save a scroll bar
   from the fiery pit when we actually redisplay its window.  */

/* Arrange for all scroll bars on FRAME to be removed at the next call
   to `*judge_scroll_bars_hook'.  A scroll bar may be spared if
   `*redeem_scroll_bar_hook' is applied to its window before the judgment.  */

static void
XTcondemn_scroll_bars (frame)
     FRAME_PTR frame;
{
  /* Transfer all the scroll bars to FRAME_CONDEMNED_SCROLL_BARS.  */
  while (! NILP (FRAME_SCROLL_BARS (frame)))
    {
      Lisp_Object bar;
      bar = FRAME_SCROLL_BARS (frame);
      FRAME_SCROLL_BARS (frame) = XSCROLL_BAR (bar)->next;
      XSCROLL_BAR (bar)->next = FRAME_CONDEMNED_SCROLL_BARS (frame);
      XSCROLL_BAR (bar)->prev = Qnil;
      if (! NILP (FRAME_CONDEMNED_SCROLL_BARS (frame)))
	XSCROLL_BAR (FRAME_CONDEMNED_SCROLL_BARS (frame))->prev = bar;
      FRAME_CONDEMNED_SCROLL_BARS (frame) = bar;
    }
}


/* Un-mark WINDOW's scroll bar for deletion in this judgment cycle.
   Note that WINDOW isn't necessarily condemned at all.  */

static void
XTredeem_scroll_bar (window)
     struct window *window;
{
  struct scroll_bar *bar;
  struct frame *f;

  /* We can't redeem this window's scroll bar if it doesn't have one.  */
  if (NILP (window->vertical_scroll_bar))
    abort ();

  bar = XSCROLL_BAR (window->vertical_scroll_bar);

  /* Unlink it from the condemned list.  */
  f = XFRAME (WINDOW_FRAME (window));
  if (NILP (bar->prev))
    {
      /* If the prev pointer is nil, it must be the first in one of
	 the lists.  */
      if (EQ (FRAME_SCROLL_BARS (f), window->vertical_scroll_bar))
	/* It's not condemned.  Everything's fine.  */
	return;
      else if (EQ (FRAME_CONDEMNED_SCROLL_BARS (f),
		   window->vertical_scroll_bar))
	FRAME_CONDEMNED_SCROLL_BARS (f) = bar->next;
      else
	/* If its prev pointer is nil, it must be at the front of
	   one or the other!  */
	abort ();
    }
  else
    XSCROLL_BAR (bar->prev)->next = bar->next;

  if (! NILP (bar->next))
    XSCROLL_BAR (bar->next)->prev = bar->prev;

  bar->next = FRAME_SCROLL_BARS (f);
  bar->prev = Qnil;
  XSETVECTOR (FRAME_SCROLL_BARS (f), bar);
  if (! NILP (bar->next))
    XSETVECTOR (XSCROLL_BAR (bar->next)->prev, bar);
}

/* Remove all scroll bars on FRAME that haven't been saved since the
   last call to `*condemn_scroll_bars_hook'.  */

static void
XTjudge_scroll_bars (f)
     FRAME_PTR f;
{
  Lisp_Object bar, next;

  bar = FRAME_CONDEMNED_SCROLL_BARS (f);

  /* Clear out the condemned list now so we won't try to process any
     more events on the hapless scroll bars.  */
  FRAME_CONDEMNED_SCROLL_BARS (f) = Qnil;

  for (; ! NILP (bar); bar = next)
    {
      struct scroll_bar *b = XSCROLL_BAR (bar);

      x_scroll_bar_remove (b);

      next = b->next;
      b->next = b->prev = Qnil;
    }

  /* Now there should be no references to the condemned scroll bars,
     and they should get garbage-collected.  */
}

/* The screen has been cleared so we may have changed foreground or
   background colors, and the scroll bars may need to be redrawn.
   Clear out the scroll bars, and ask for expose events, so we can
   redraw them.  */

void
x_scroll_bar_clear (f)
     FRAME_PTR f;
{
  Lisp_Object bar;

  /* We can have scroll bars even if this is 0,
     if we just turned off scroll bar mode.
     But in that case we should not clear them.  */
  if (FRAME_HAS_VERTICAL_SCROLL_BARS (f))
    for (bar = FRAME_SCROLL_BARS (f); VECTORP (bar);
	 bar = XSCROLL_BAR (bar)->next)
      XSCROLL_BAR (bar)->redraw_needed_p = Qt;
}


/***********************************************************************
			       Tool-bars
 ***********************************************************************/
#if USE_MAC_TOOLBAR
void
mac_move_window_with_gravity (f, win_gravity, left, top)
     struct frame *f;
     int win_gravity;
     short left, top;
{
  Rect inner, outer;

  mac_get_window_bounds (f, &inner, &outer);

  switch (win_gravity)
    {
    case NorthWestGravity:
    case WestGravity:
    case SouthWestGravity:
      left += inner.left - outer.left;
      break;

    case NorthGravity:
    case CenterGravity:
    case SouthGravity:
      left += ((inner.left - outer.left) + (inner.right - outer.right)) / 2;
      break;

    case NorthEastGravity:
    case EastGravity:
    case SouthEastGravity:
      left += inner.right - outer.right;
      break;
    }

  switch (win_gravity)
    {
    case NorthWestGravity:
    case NorthGravity:
    case NorthEastGravity:
      top += inner.top - outer.top;
      break;

    case WestGravity:
    case CenterGravity:
    case EastGravity:
      top += ((inner.top - outer.top) + (inner.bottom - outer.bottom)) / 2;
      break;

    case SouthWestGravity:
    case SouthGravity:
    case SouthEastGravity:
      top += inner.bottom - outer.bottom;
      break;
    }

  mac_move_window (FRAME_MAC_WINDOW (f), left, top, false);
}

void
mac_get_window_origin_with_gravity (f, win_gravity, left, top)
     struct frame *f;
     int win_gravity;
     short *left, *top;
{
  Rect inner, outer;

  mac_get_window_bounds (f, &inner, &outer);

  switch (win_gravity)
    {
    case NorthWestGravity:
    case WestGravity:
    case SouthWestGravity:
      *left = outer.left;
      break;

    case NorthGravity:
    case CenterGravity:
    case SouthGravity:
      *left = outer.left + ((outer.right - outer.left)
			    - (inner.right - inner.left)) / 2;
      break;

    case NorthEastGravity:
    case EastGravity:
    case SouthEastGravity:
      *left = outer.right - (inner.right - inner.left);
      break;
    }

  switch (win_gravity)
    {
    case NorthWestGravity:
    case NorthGravity:
    case NorthEastGravity:
      *top = outer.top;
      break;

    case WestGravity:
    case CenterGravity:
    case EastGravity:
      *top = outer.top + ((outer.bottom - outer.top)
			  - (inner.bottom - inner.top)) / 2;
      break;

    case SouthWestGravity:
    case SouthGravity:
    case SouthEastGravity:
      *top = outer.bottom - (inner.bottom - inner.top);
      break;
    }
}

CGImageRef
mac_image_spec_to_cg_image (f, image)
     struct frame *f;
     Lisp_Object image;
{
  if (!valid_image_p (image))
    return NULL;
  else
    {
      int img_id = lookup_image (f, image);
      struct image *img = IMAGE_FROM_ID (f, img_id);

      prepare_image_for_display (f, img);

      return img->data.ptr_val;
    }
}
#endif	/* USE_MAC_TOOLBAR */


/***********************************************************************
			     Text Cursor
 ***********************************************************************/

/* Set clipping for output in glyph row ROW.  W is the window in which
   we operate.  GC is the graphics context to set clipping in.

   ROW may be a text row or, e.g., a mode line.  Text rows must be
   clipped to the interior of the window dedicated to text display,
   mode lines must be clipped to the whole window.  */

static void
x_clip_to_row (w, row, area, gc)
     struct window *w;
     struct glyph_row *row;
     int area;
     GC gc;
{
  struct frame *f = XFRAME (WINDOW_FRAME (w));
  Rect clip_rect;
  int window_x, window_y, window_width;

  window_box (w, area, &window_x, &window_y, &window_width, 0);

  clip_rect.left = window_x;
  clip_rect.top = WINDOW_TO_FRAME_PIXEL_Y (w, row->y);
  clip_rect.top = max (clip_rect.top, window_y);
  clip_rect.right = clip_rect.left + window_width;
  clip_rect.bottom = clip_rect.top + row->visible_height;

  mac_set_clip_rectangles (f, gc, &clip_rect, 1);
}


/* Draw a hollow box cursor on window W in glyph row ROW.  */

static void
x_draw_hollow_cursor (w, row)
     struct window *w;
     struct glyph_row *row;
{
  struct frame *f = XFRAME (WINDOW_FRAME (w));
  struct mac_display_info *dpyinfo = FRAME_MAC_DISPLAY_INFO (f);
  Display *dpy = FRAME_MAC_DISPLAY (f);
  int x, y, wd, h;
  XGCValues xgcv;
  struct glyph *cursor_glyph;
  GC gc;

  /* Get the glyph the cursor is on.  If we can't tell because
     the current matrix is invalid or such, give up.  */
  cursor_glyph = get_phys_cursor_glyph (w);
  if (cursor_glyph == NULL)
    return;

  /* Compute frame-relative coordinates for phys cursor.  */
  get_phys_cursor_geometry (w, row, cursor_glyph, &x, &y, &h);
  wd = w->phys_cursor_width;

  /* The foreground of cursor_gc is typically the same as the normal
     background color, which can cause the cursor box to be invisible.  */
  xgcv.foreground = f->output_data.mac->cursor_pixel;
  if (dpyinfo->scratch_cursor_gc)
    XChangeGC (dpy, dpyinfo->scratch_cursor_gc, GCForeground, &xgcv);
  else
    dpyinfo->scratch_cursor_gc = XCreateGC (dpy, FRAME_MAC_WINDOW (f),
					    GCForeground, &xgcv);
  gc = dpyinfo->scratch_cursor_gc;

  /* Set clipping, draw the rectangle, and reset clipping again.  */
  x_clip_to_row (w, row, TEXT_AREA, gc);
  mac_draw_rectangle (f, gc, x, y, wd, h - 1);
  mac_reset_clip_rectangles (f, gc);
}


/* Draw a bar cursor on window W in glyph row ROW.

   Implementation note: One would like to draw a bar cursor with an
   angle equal to the one given by the font property XA_ITALIC_ANGLE.
   Unfortunately, I didn't find a font yet that has this property set.
   --gerd.  */

static void
x_draw_bar_cursor (w, row, width, kind)
     struct window *w;
     struct glyph_row *row;
     int width;
     enum text_cursor_kinds kind;
{
  struct frame *f = XFRAME (w->frame);
  struct glyph *cursor_glyph;

  /* If cursor is out of bounds, don't draw garbage.  This can happen
     in mini-buffer windows when switching between echo area glyphs
     and mini-buffer.  */
  cursor_glyph = get_phys_cursor_glyph (w);
  if (cursor_glyph == NULL)
    return;

  /* If on an image, draw like a normal cursor.  That's usually better
     visible than drawing a bar, esp. if the image is large so that
     the bar might not be in the window.  */
  if (cursor_glyph->type == IMAGE_GLYPH)
    {
      struct glyph_row *row;
      row = MATRIX_ROW (w->current_matrix, w->phys_cursor.vpos);
      draw_phys_cursor_glyph (w, row, DRAW_CURSOR);
    }
  else
    {
      Display *dpy = FRAME_MAC_DISPLAY (f);
      Window window = FRAME_MAC_WINDOW (f);
      GC gc = FRAME_MAC_DISPLAY_INFO (f)->scratch_cursor_gc;
      unsigned long mask = GCForeground | GCBackground;
      struct face *face = FACE_FROM_ID (f, cursor_glyph->face_id);
      XGCValues xgcv;

      /* If the glyph's background equals the color we normally draw
	 the bar cursor in, the bar cursor in its normal color is
	 invisible.  Use the glyph's foreground color instead in this
	 case, on the assumption that the glyph's colors are chosen so
	 that the glyph is legible.  */
      if (face->background == f->output_data.mac->cursor_pixel)
	xgcv.background = xgcv.foreground = face->foreground;
      else
	xgcv.background = xgcv.foreground = f->output_data.mac->cursor_pixel;

      if (gc)
	XChangeGC (dpy, gc, mask, &xgcv);
      else
	{
	  gc = XCreateGC (dpy, window, mask, &xgcv);
	  FRAME_MAC_DISPLAY_INFO (f)->scratch_cursor_gc = gc;
	}

      if (width < 0)
	width = FRAME_CURSOR_WIDTH (f);
      width = min (cursor_glyph->pixel_width, width);

      w->phys_cursor_width = width;
      x_clip_to_row (w, row, TEXT_AREA, gc);

      if (kind == BAR_CURSOR)
	mac_fill_rectangle (f, gc,
			    WINDOW_TEXT_TO_FRAME_PIXEL_X (w, w->phys_cursor.x),
			    WINDOW_TO_FRAME_PIXEL_Y (w, w->phys_cursor.y),
			    width, row->height);
      else
	mac_fill_rectangle (f, gc,
			    WINDOW_TEXT_TO_FRAME_PIXEL_X (w, w->phys_cursor.x),
			    WINDOW_TO_FRAME_PIXEL_Y (w, w->phys_cursor.y +
						     row->height - width),
			    cursor_glyph->pixel_width,
			    width);

      mac_reset_clip_rectangles (f, gc);
    }
}


/* RIF: Define cursor CURSOR on frame F.  */

static void
mac_define_frame_cursor (f, cursor)
     struct frame *f;
     Cursor cursor;
{
  struct mac_display_info *dpyinfo = FRAME_MAC_DISPLAY_INFO (f);

  if (dpyinfo->x_focus_frame == f)
    SetThemeCursor (cursor);
}


/* RIF: Clear area on frame F.  */

static void
mac_clear_frame_area (f, x, y, width, height)
     struct frame *f;
     int x, y, width, height;
{
  mac_clear_area (f, x, y, width, height);
}


/* RIF: Draw cursor on window W.  */

static void
mac_draw_window_cursor (w, glyph_row, x, y, cursor_type, cursor_width, on_p, active_p)
     struct window *w;
     struct glyph_row *glyph_row;
     int x, y;
     int cursor_type, cursor_width;
     int on_p, active_p;
{
  if (on_p)
    {
      w->phys_cursor_type = cursor_type;
      w->phys_cursor_on_p = 1;

      if (glyph_row->exact_window_width_line_p
	  && w->phys_cursor.hpos >= glyph_row->used[TEXT_AREA])
	{
	  glyph_row->cursor_in_fringe_p = 1;
	  draw_fringe_bitmap (w, glyph_row, 0);
	}
      else
      switch (cursor_type)
	{
	case HOLLOW_BOX_CURSOR:
	  x_draw_hollow_cursor (w, glyph_row);
	  break;

	case FILLED_BOX_CURSOR:
	  draw_phys_cursor_glyph (w, glyph_row, DRAW_CURSOR);
	  break;

	case BAR_CURSOR:
	  x_draw_bar_cursor (w, glyph_row, cursor_width, BAR_CURSOR);
	  break;

	case HBAR_CURSOR:
	  x_draw_bar_cursor (w, glyph_row, cursor_width, HBAR_CURSOR);
	  break;

	case NO_CURSOR:
	  w->phys_cursor_width = 0;
	  break;

	default:
	  abort ();
	}
    }
}


/* Icons.  */

#if 0 /* MAC_TODO: no icon support yet.  */
int
x_bitmap_icon (f, icon)
     struct frame *f;
     Lisp_Object icon;
{
  HANDLE hicon;

  if (FRAME_W32_WINDOW (f) == 0)
    return 1;

  if (NILP (icon))
    hicon = LoadIcon (hinst, EMACS_CLASS);
  else if (STRINGP (icon))
    hicon = LoadImage (NULL, (LPCTSTR) SDATA (icon), IMAGE_ICON, 0, 0,
		       LR_DEFAULTSIZE | LR_LOADFROMFILE);
  else if (SYMBOLP (icon))
    {
      LPCTSTR name;

      if (EQ (icon, intern ("application")))
	name = (LPCTSTR) IDI_APPLICATION;
      else if (EQ (icon, intern ("hand")))
	name = (LPCTSTR) IDI_HAND;
      else if (EQ (icon, intern ("question")))
	name = (LPCTSTR) IDI_QUESTION;
      else if (EQ (icon, intern ("exclamation")))
	name = (LPCTSTR) IDI_EXCLAMATION;
      else if (EQ (icon, intern ("asterisk")))
	name = (LPCTSTR) IDI_ASTERISK;
      else if (EQ (icon, intern ("winlogo")))
	name = (LPCTSTR) IDI_WINLOGO;
      else
	return 1;

      hicon = LoadIcon (NULL, name);
    }
  else
    return 1;

  if (hicon == NULL)
    return 1;

  PostMessage (FRAME_W32_WINDOW (f), WM_SETICON, (WPARAM) ICON_BIG,
               (LPARAM) hicon);

  return 0;
}
#endif /* MAC_TODO */

/************************************************************************
			  Handling X errors
 ************************************************************************/

/* Display Error Handling functions not used on W32. Listing them here
   helps diff stay in step when comparing w32term.c with xterm.c.

x_error_catcher (display, error)
x_catch_errors (dpy)
x_catch_errors_unwind (old_val)
x_check_errors (dpy, format)
x_had_errors_p (dpy)
x_clear_errors (dpy)
x_uncatch_errors (dpy, count)
x_trace_wire ()
x_connection_signal (signalnum)
x_connection_closed (dpy, error_message)
x_error_quitter (display, error)
x_error_handler (display, error)
x_io_error_quitter (display)

 */


/* Changing the font of the frame.  */

/* Give frame F the font named FONTNAME as its default font, and
   return the full name of that font.  FONTNAME may be a wildcard
   pattern; in that case, we choose some font that fits the pattern.
   The return value shows which font we chose.  */

Lisp_Object
x_new_font (f, fontname)
     struct frame *f;
     register char *fontname;
{
  struct font_info *fontp
    = FS_LOAD_FONT (f, 0, fontname, -1);

  if (!fontp)
    return Qnil;

  FRAME_FONT (f) = (XFontStruct *) (fontp->font);
  FRAME_BASELINE_OFFSET (f) = fontp->baseline_offset;
  FRAME_FONTSET (f) = -1;

  FRAME_COLUMN_WIDTH (f) = fontp->average_width;
  FRAME_SPACE_WIDTH (f) = fontp->space_width;
  FRAME_LINE_HEIGHT (f) = FONT_HEIGHT (FRAME_FONT (f));

  compute_fringe_widths (f, 1);

  /* Compute the scroll bar width in character columns.  */
  if (FRAME_CONFIG_SCROLL_BAR_WIDTH (f) > 0)
    {
      int wid = FRAME_COLUMN_WIDTH (f);
      FRAME_CONFIG_SCROLL_BAR_COLS (f)
	= (FRAME_CONFIG_SCROLL_BAR_WIDTH (f) + wid-1) / wid;
    }
  else
    {
      int wid = FRAME_COLUMN_WIDTH (f);
      FRAME_CONFIG_SCROLL_BAR_COLS (f) = (14 + wid - 1) / wid;
    }

  /* Now make the frame display the given font.  */
  if (FRAME_MAC_WINDOW (f) != 0)
    {
      XSetFont (FRAME_MAC_DISPLAY (f), f->output_data.mac->normal_gc,
		FRAME_FONT (f));
      XSetFont (FRAME_MAC_DISPLAY (f), f->output_data.mac->reverse_gc,
		FRAME_FONT (f));
      XSetFont (FRAME_MAC_DISPLAY (f), f->output_data.mac->cursor_gc,
		FRAME_FONT (f));

      /* Don't change the size of a tip frame; there's no point in
	 doing it because it's done in Fx_show_tip, and it leads to
	 problems because the tip frame has no widget.  */
      if (NILP (tip_frame) || XFRAME (tip_frame) != f)
        x_set_window_size (f, 0, FRAME_COLS (f), FRAME_LINES (f));
    }

  return build_string (fontp->full_name);
}

/* Give frame F the fontset named FONTSETNAME as its default font, and
   return the full name of that fontset.  FONTSETNAME may be a wildcard
   pattern; in that case, we choose some fontset that fits the pattern.
   The return value shows which fontset we chose.  */

Lisp_Object
x_new_fontset (f, fontsetname)
     struct frame *f;
     char *fontsetname;
{
  int fontset = fs_query_fontset (build_string (fontsetname), 0);
  Lisp_Object result;

  if (fontset < 0)
    return Qnil;

  if (FRAME_FONTSET (f) == fontset)
    /* This fontset is already set in frame F.  There's nothing more
       to do.  */
    return fontset_name (fontset);

  result = x_new_font (f, (SDATA (fontset_ascii (fontset))));

  if (!STRINGP (result))
    /* Can't load ASCII font.  */
    return Qnil;

  /* Since x_new_font doesn't update any fontset information, do it now.  */
  FRAME_FONTSET (f) = fontset;

  return build_string (fontsetname);
}


/***********************************************************************
	TODO: W32 Input Methods
 ***********************************************************************/
/* Listing missing functions from xterm.c helps diff stay in step.

xim_destroy_callback (xim, client_data, call_data)
xim_open_dpy (dpyinfo, resource_name)
struct xim_inst_t
xim_instantiate_callback (display, client_data, call_data)
xim_initialize (dpyinfo, resource_name)
xim_close_dpy (dpyinfo)

 */


void
mac_handle_origin_change (f)
     struct frame *f;
{
  x_real_positions (f, &f->left_pos, &f->top_pos);
}

void
mac_handle_size_change (f, pixelwidth, pixelheight)
     struct frame *f;
     int pixelwidth, pixelheight;
{
  int cols, rows;

  cols = FRAME_PIXEL_WIDTH_TO_TEXT_COLS (f, pixelwidth);
  rows = FRAME_PIXEL_HEIGHT_TO_TEXT_LINES (f, pixelheight);

  if (cols != FRAME_COLS (f)
      || rows != FRAME_LINES (f)
      || pixelwidth != FRAME_PIXEL_WIDTH (f)
      || pixelheight != FRAME_PIXEL_HEIGHT (f))
    {
      /* We pass 1 for DELAY since we can't run Lisp code inside of
	 a BLOCK_INPUT.  */
      change_frame_size (f, rows, cols, 0, 1, 0);
      FRAME_PIXEL_WIDTH (f) = pixelwidth;
      FRAME_PIXEL_HEIGHT (f) = pixelheight;

      /* If cursor was outside the new size, mark it as off.  */
      mark_window_cursors_off (XWINDOW (f->root_window));

      /* Clear out any recollection of where the mouse highlighting
	 was, since it might be in a place that's outside the new
	 frame size.  Actually checking whether it is outside is a
	 pain in the neck, so don't try--just let the highlighting be
	 done afresh with new size.  */
      cancel_mouse_face (f);

#if TARGET_API_MAC_CARBON
      mac_reposition_hourglass (f);
#endif
    }
}


/* Calculate the absolute position in frame F
   from its current recorded position values and gravity.  */

void
x_calc_absolute_position (f)
     struct frame *f;
{
  int flags = f->size_hint_flags;
  Rect inner, outer;

  /* We have nothing to do if the current position
     is already for the top-left corner.  */
  if (! ((flags & XNegative) || (flags & YNegative)))
    return;

  /* Find the offsets of the outside upper-left corner of
     the inner window, with respect to the outer window.  */
  BLOCK_INPUT;
  mac_get_window_bounds (f, &inner, &outer);
  UNBLOCK_INPUT;

  /* Treat negative positions as relative to the leftmost bottommost
     position that fits on the screen.  */
  if (flags & XNegative)
    f->left_pos += (FRAME_MAC_DISPLAY_INFO (f)->width
		    - (outer.right - outer.left));

  if (flags & YNegative)
    f->top_pos += (FRAME_MAC_DISPLAY_INFO (f)->height
		   - (outer.bottom - outer.top));

  /* The left_pos and top_pos
     are now relative to the top and left screen edges,
     so the flags should correspond.  */
  f->size_hint_flags &= ~ (XNegative | YNegative);
}

/* CHANGE_GRAVITY is 1 when calling from Fset_frame_position,
   to really change the position, and 0 when calling from
   x_make_frame_visible (in that case, XOFF and YOFF are the current
   position values).  It is -1 when calling from x_set_frame_parameters,
   which means, do adjust for borders but don't change the gravity.  */

void
x_set_offset (f, xoff, yoff, change_gravity)
     struct frame *f;
     register int xoff, yoff;
     int change_gravity;
{
  if (change_gravity > 0)
    {
      f->top_pos = yoff;
      f->left_pos = xoff;
      f->size_hint_flags &= ~ (XNegative | YNegative);
      if (xoff < 0)
	f->size_hint_flags |= XNegative;
      if (yoff < 0)
	f->size_hint_flags |= YNegative;
      f->win_gravity = NorthWestGravity;
    }
  x_calc_absolute_position (f);

  BLOCK_INPUT;
  x_wm_set_size_hint (f, (long) 0, 0);

#if TARGET_API_MAC_CARBON
  mac_move_window_structure (FRAME_MAC_WINDOW (f), f->left_pos, f->top_pos);
  /* If the title bar is completely outside the screen, adjust the
     position. */
#if !USE_APPKIT
  ConstrainWindowToScreen (FRAME_MAC_WINDOW (f), kWindowTitleBarRgn,
			   kWindowConstrainMoveRegardlessOfFit
			   | kWindowConstrainAllowPartial, NULL, NULL);
  if (!NILP (tip_frame) && XFRAME (tip_frame) == f)
    mac_handle_origin_change (f);
#endif	/* !USE_APPKIT */
#else
  {
    Rect inner, outer, screen_rect, dummy;
    RgnHandle region = NewRgn ();

    mac_get_window_bounds (f, &inner, &outer);
    f->x_pixels_diff = inner.left - outer.left;
    f->y_pixels_diff = inner.top - outer.top;
    MoveWindow (FRAME_MAC_WINDOW (f), f->left_pos + f->x_pixels_diff,
		f->top_pos + f->y_pixels_diff, false);

    /* If the title bar is completely outside the screen, adjust the
       position.  The variable `outer' holds the title bar rectangle.
       The variable `inner' holds slightly smaller one than `outer',
       so that the calculation of overlapping may not become too
       strict.  */
    GetWindowRegion (FRAME_MAC_WINDOW (f), kWindowTitleBarRgn, region);
    outer = (*region)->rgnBBox;
    DisposeRgn (region);
    inner = outer;
    InsetRect (&inner, 8, 8);
    screen_rect = qd.screenBits.bounds;
    screen_rect.top += GetMBarHeight ();

    if (!SectRect (&inner, &screen_rect, &dummy))
      {
	if (inner.right <= screen_rect.left)
	  f->left_pos = screen_rect.left;
	else if (inner.left >= screen_rect.right)
	  f->left_pos = screen_rect.right - (outer.right - outer.left);

	if (inner.bottom <= screen_rect.top)
	  f->top_pos = screen_rect.top;
	else if (inner.top >= screen_rect.bottom)
	  f->top_pos = screen_rect.bottom - (outer.bottom - outer.top);

	MoveWindow (FRAME_MAC_WINDOW (f), f->left_pos + f->x_pixels_diff,
		    f->top_pos + f->y_pixels_diff, false);
      }
  }
#endif

  UNBLOCK_INPUT;
}

/* Call this to change the size of frame F's x-window.
   If CHANGE_GRAVITY is 1, we change to top-left-corner window gravity
   for this size change and subsequent size changes.
   Otherwise we leave the window gravity unchanged.  */

void
x_set_window_size (f, change_gravity, cols, rows)
     struct frame *f;
     int change_gravity;
     int cols, rows;
{
  int pixelwidth, pixelheight;

  BLOCK_INPUT;

  check_frame_size (f, &rows, &cols);
  f->scroll_bar_actual_width
    = FRAME_SCROLL_BAR_COLS (f) * FRAME_COLUMN_WIDTH (f);

  compute_fringe_widths (f, 0);

  pixelwidth = FRAME_TEXT_COLS_TO_PIXEL_WIDTH (f, cols);
  pixelheight = FRAME_TEXT_LINES_TO_PIXEL_HEIGHT (f, rows);

  f->win_gravity = NorthWestGravity;
  x_wm_set_size_hint (f, (long) 0, 0);

  mac_size_window (FRAME_MAC_WINDOW (f), pixelwidth, pixelheight, 0);

#if TARGET_API_MAC_CARBON
  if (!NILP (tip_frame) && f == XFRAME (tip_frame))
#endif
    mac_handle_size_change (f, pixelwidth, pixelheight);

  if (f->output_data.mac->internal_border_width
      != FRAME_INTERNAL_BORDER_WIDTH (f))
    {
      if (f->async_visible)
	mac_clear_window (f);
      f->output_data.mac->internal_border_width
	= FRAME_INTERNAL_BORDER_WIDTH (f);
    }

  SET_FRAME_GARBAGED (f);

  UNBLOCK_INPUT;
}

/* Mouse warping.  */

void x_set_mouse_pixel_position (struct frame *f, int pix_x, int pix_y);

void
x_set_mouse_position (f, x, y)
     struct frame *f;
     int x, y;
{
  int pix_x, pix_y;

  pix_x = FRAME_COL_TO_PIXEL_X (f, x) + FRAME_COLUMN_WIDTH (f) / 2;
  pix_y = FRAME_LINE_TO_PIXEL_Y (f, y) + FRAME_LINE_HEIGHT (f) / 2;

  if (pix_x < 0) pix_x = 0;
  if (pix_x > FRAME_PIXEL_WIDTH (f)) pix_x = FRAME_PIXEL_WIDTH (f);

  if (pix_y < 0) pix_y = 0;
  if (pix_y > FRAME_PIXEL_HEIGHT (f)) pix_y = FRAME_PIXEL_HEIGHT (f);

  x_set_mouse_pixel_position (f, pix_x, pix_y);
}

void
x_set_mouse_pixel_position (f, pix_x, pix_y)
     struct frame *f;
     int pix_x, pix_y;
{
#ifdef MAC_OSX
  BLOCK_INPUT;
  mac_convert_frame_point_to_global (f, &pix_x, &pix_y);
  CGWarpMouseCursorPosition (CGPointMake (pix_x, pix_y));
  UNBLOCK_INPUT;
#else
#if 0 /* MAC_TODO: LMSetMouseLocation and CursorDeviceMoveTo are non-Carbon */
  BLOCK_INPUT;

  XWarpPointer (FRAME_X_DISPLAY (f), None, FRAME_X_WINDOW (f),
		0, 0, 0, 0, pix_x, pix_y);
  UNBLOCK_INPUT;
#endif
#endif
}

/* focus shifting, raising and lowering.  */

void
x_focus_on_frame (f)
     struct frame *f;
{
#if 0  /* This proves to be unpleasant.  */
  x_raise_frame (f);
#endif
#if 0
  /* I don't think that the ICCCM allows programs to do things like this
     without the interaction of the window manager.  Whatever you end up
     doing with this code, do it to x_unfocus_frame too.  */
  XSetInputFocus (FRAME_X_DISPLAY (f), FRAME_X_WINDOW (f),
		  RevertToPointerRoot, CurrentTime);
#endif /* ! 0 */
}

void
x_unfocus_frame (f)
     struct frame *f;
{
}

/* Raise frame F.  */

void
x_raise_frame (f)
     struct frame *f;
{
  if (f->async_visible)
    {
      BLOCK_INPUT;
      mac_bring_window_to_front (FRAME_MAC_WINDOW (f));
      UNBLOCK_INPUT;
    }
}

/* Lower frame F.  */

void
x_lower_frame (f)
     struct frame *f;
{
  if (f->async_visible)
    {
      BLOCK_INPUT;
      mac_send_window_behind (FRAME_MAC_WINDOW (f), NULL);
      UNBLOCK_INPUT;
    }
}

static void
XTframe_raise_lower (f, raise_flag)
     FRAME_PTR f;
     int raise_flag;
{
  if (raise_flag)
    x_raise_frame (f);
  else
    x_lower_frame (f);
}

/* Change of visibility.  */

void
mac_handle_visibility_change (f)
     struct frame *f;
{
  Window wp = FRAME_MAC_WINDOW (f);
  int visible = 0, iconified = 0;
  struct input_event buf;

  if (mac_is_window_visible (wp))
    {
      if (mac_is_window_collapsed (wp))
	iconified = 1;
      else
	visible = 1;
    }

  if (!f->async_visible && visible)
    {
      if (f->iconified)
	{
	  /* wait_reading_process_output will notice this and update
	     the frame's display structures.  If we were made
	     invisible, we should not set garbaged, because that stops
	     redrawing on Update events.  */
	  SET_FRAME_GARBAGED (f);

	  EVENT_INIT (buf);
	  buf.kind = DEICONIFY_EVENT;
	  XSETFRAME (buf.frame_or_window, f);
	  buf.arg = Qnil;
	  kbd_buffer_store_event (&buf);
	}
      else if (! NILP (Vframe_list) && ! NILP (XCDR (Vframe_list)))
	/* Force a redisplay sooner or later to update the
	   frame titles in case this is the second frame.  */
	record_asynch_buffer_change ();
    }
  else if (f->async_visible && !visible)
    if (iconified)
      {
	EVENT_INIT (buf);
	buf.kind = ICONIFY_EVENT;
	XSETFRAME (buf.frame_or_window, f);
	buf.arg = Qnil;
	kbd_buffer_store_event (&buf);
      }

  f->async_visible = visible;
  f->async_iconified = iconified;
}

/* This tries to wait until the frame is really visible.
   However, if the window manager asks the user where to position
   the frame, this will return before the user finishes doing that.
   The frame will not actually be visible at that time,
   but it will become visible later when the window manager
   finishes with it.  */

void
x_make_frame_visible (f)
     struct frame *f;
{
  BLOCK_INPUT;

  if (! FRAME_VISIBLE_P (f))
    {
      /* We test FRAME_GARBAGED_P here to make sure we don't
	 call x_set_offset a second time
	 if we get to x_make_frame_visible a second time
	 before the window gets really visible.  */
      if (! FRAME_ICONIFIED_P (f)
	  && ! f->output_data.mac->asked_for_visible)
	x_set_offset (f, f->left_pos, f->top_pos, 0);

      f->output_data.mac->asked_for_visible = 1;

      mac_collapse_window (FRAME_MAC_WINDOW (f), false);
      mac_show_window (FRAME_MAC_WINDOW (f));
    }

  XFlush (FRAME_MAC_DISPLAY (f));

  /* Synchronize to ensure Emacs knows the frame is visible
     before we do anything else.  We do this loop with input not blocked
     so that incoming events are handled.  */
  {
    Lisp_Object frame;
    int count;

    /* This must come after we set COUNT.  */
    UNBLOCK_INPUT;

    XSETFRAME (frame, f);

    /* Wait until the frame is visible.  Process X events until a
       MapNotify event has been seen, or until we think we won't get a
       MapNotify at all..  */
    for (count = input_signal_count + 10;
	 input_signal_count < count && !FRAME_VISIBLE_P (f);)
      {
	/* Force processing of queued events.  */
	x_sync (f);

	/* Machines that do polling rather than SIGIO have been
	   observed to go into a busy-wait here.  So we'll fake an
	   alarm signal to let the handler know that there's something
	   to be read.  We used to raise a real alarm, but it seems
	   that the handler isn't always enabled here.  This is
	   probably a bug.  */
	if (input_polling_used ())
	  {
	    /* It could be confusing if a real alarm arrives while
	       processing the fake one.  Turn it off and let the
	       handler reset it.  */
	    extern void poll_for_input_1 P_ ((void));
	    int old_poll_suppress_count = poll_suppress_count;
	    poll_suppress_count = 1;
	    poll_for_input_1 ();
	    poll_suppress_count = old_poll_suppress_count;
	  }

	/* See if a MapNotify event has been processed.  */
	FRAME_SAMPLE_VISIBILITY (f);
      }
  }
}

/* Change from mapped state to withdrawn state.  */

/* Make the frame visible (mapped and not iconified).  */

void
x_make_frame_invisible (f)
     struct frame *f;
{
  /* A deactivate event does not occur when the last visible frame is
     made invisible.  So if we clear the highlight here, it will not
     be rehighlighted when it is made visible.  */
#if 0
  /* Don't keep the highlight on an invisible frame.  */
  if (FRAME_MAC_DISPLAY_INFO (f)->x_highlight_frame == f)
    FRAME_MAC_DISPLAY_INFO (f)->x_highlight_frame = 0;
#endif

  BLOCK_INPUT;

#if !TARGET_API_MAC_CARBON || USE_APPKIT
  /* Before unmapping the window, update the WM_SIZE_HINTS property to claim
     that the current position of the window is user-specified, rather than
     program-specified, so that when the window is mapped again, it will be
     placed at the same location, without forcing the user to position it
     by hand again (they have already done that once for this window.)  */
  x_wm_set_size_hint (f, (long) 0, 1);
#endif

  mac_hide_window (FRAME_MAC_WINDOW (f));

  UNBLOCK_INPUT;

#if !TARGET_API_MAC_CARBON || USE_APPKIT
  mac_handle_visibility_change (f);
#endif
}

/* Change window state from mapped to iconified.  */

void
x_iconify_frame (f)
     struct frame *f;
{
  OSStatus err;

  /* A deactivate event does not occur when the last visible frame is
     iconified.  So if we clear the highlight here, it will not be
     rehighlighted when it is deiconified.  */
#if 0
  /* Don't keep the highlight on an invisible frame.  */
  if (FRAME_MAC_DISPLAY_INFO (f)->x_highlight_frame == f)
    FRAME_MAC_DISPLAY_INFO (f)->x_highlight_frame = 0;
#endif

  if (f->async_iconified)
    return;

  BLOCK_INPUT;

  FRAME_SAMPLE_VISIBILITY (f);

  if (! FRAME_VISIBLE_P (f))
    mac_show_window (FRAME_MAC_WINDOW (f));

  err = mac_collapse_window (FRAME_MAC_WINDOW (f), true);

  UNBLOCK_INPUT;

  if (err != noErr)
    error ("Can't notify window manager of iconification");

#if !TARGET_API_MAC_CARBON || USE_APPKIT
  mac_handle_visibility_change (f);
#endif
}


/* Free X resources of frame F.  */

void
x_free_frame_resources (f)
     struct frame *f;
{
  struct mac_display_info *dpyinfo = FRAME_MAC_DISPLAY_INFO (f);
  Window wp = FRAME_MAC_WINDOW (f);

  BLOCK_INPUT;

  /* AppKit version of mac_dispose_frame_window, which is implemented
     as -[NSWindow close], will change the focus to the next window
     during its call.  So, unlike other platforms, we clean up the
     focus-related variables before calling mac_dispose_frame_window.  */
  if (f == dpyinfo->x_focus_frame)
    {
      dpyinfo->x_focus_frame = 0;
#if USE_MAC_FONT_PANEL
      mac_set_font_info_for_selection (NULL, DEFAULT_FACE_ID, 0);
#endif
    }
  if (f == dpyinfo->x_focus_event_frame)
    dpyinfo->x_focus_event_frame = 0;
  if (f == dpyinfo->x_highlight_frame)
    dpyinfo->x_highlight_frame = 0;

  if (f == dpyinfo->mouse_face_mouse_frame)
    {
      dpyinfo->mouse_face_beg_row
	= dpyinfo->mouse_face_beg_col = -1;
      dpyinfo->mouse_face_end_row
	= dpyinfo->mouse_face_end_col = -1;
      dpyinfo->mouse_face_window = Qnil;
      dpyinfo->mouse_face_deferred_gc = 0;
      dpyinfo->mouse_face_mouse_frame = 0;
    }

  mac_dispose_frame_window (f);
  if (wp == tip_window)
    /* Neither WaitNextEvent nor ReceiveNextEvent receives `window
       closed' event.  So we reset tip_window here.  */
    tip_window = NULL;

  free_frame_menubar (f);

  if (FRAME_FACE_CACHE (f))
    free_frame_faces (f);

  x_free_gcs (f);

  if (FRAME_SIZE_HINTS (f))
    xfree (FRAME_SIZE_HINTS (f));

  xfree (f->output_data.mac);
  f->output_data.mac = NULL;

  UNBLOCK_INPUT;
}


/* Destroy the X window of frame F.  */

void
x_destroy_window (f)
     struct frame *f;
{
  struct mac_display_info *dpyinfo = FRAME_MAC_DISPLAY_INFO (f);

  x_free_frame_resources (f);

  dpyinfo->reference_count--;
}


/* Setting window manager hints.  */

/* Set the normal size hints for the window manager, for frame F.
   FLAGS is the flags word to use--or 0 meaning preserve the flags
   that the window now has.
   If USER_POSITION is nonzero, we set the USPosition
   flag (this is useful when FLAGS is 0).  */
void
x_wm_set_size_hint (f, flags, user_position)
     struct frame *f;
     long flags;
     int user_position;
{
  int base_width, base_height, width_inc, height_inc;
  int min_rows = 0, min_cols = 0;
  XSizeHints *size_hints;

  base_width = FRAME_TEXT_COLS_TO_PIXEL_WIDTH (f, 0);
  base_height = FRAME_TEXT_LINES_TO_PIXEL_HEIGHT (f, 0);
  width_inc = FRAME_COLUMN_WIDTH (f);
  height_inc = FRAME_LINE_HEIGHT (f);

  check_frame_size (f, &min_rows, &min_cols);

  size_hints = FRAME_SIZE_HINTS (f);
  if (size_hints == NULL)
    {
      size_hints = FRAME_SIZE_HINTS (f) = xmalloc (sizeof (XSizeHints));
      bzero (size_hints, sizeof (XSizeHints));
    }

  size_hints->flags |= PResizeInc | PMinSize | PBaseSize ;
  size_hints->width_inc  = width_inc;
  size_hints->height_inc = height_inc;
  size_hints->min_width  = base_width + min_cols * width_inc;
  size_hints->min_height = base_height + min_rows * height_inc;
  size_hints->base_width  = base_width;
  size_hints->base_height = base_height;

  if (flags)
    size_hints->flags = flags;
  else if (user_position)
    {
      size_hints->flags &= ~ PPosition;
      size_hints->flags |= USPosition;
    }
}

#if 0 /* MAC_TODO: hide application instead of iconify? */
/* Used for IconicState or NormalState */

void
x_wm_set_window_state (f, state)
     struct frame *f;
     int state;
{
#ifdef USE_X_TOOLKIT
  Arg al[1];

  XtSetArg (al[0], XtNinitialState, state);
  XtSetValues (f->output_data.x->widget, al, 1);
#else /* not USE_X_TOOLKIT */
  Window window = FRAME_X_WINDOW (f);

  f->output_data.x->wm_hints.flags |= StateHint;
  f->output_data.x->wm_hints.initial_state = state;

  XSetWMHints (FRAME_X_DISPLAY (f), window, &f->output_data.x->wm_hints);
#endif /* not USE_X_TOOLKIT */
}

void
x_wm_set_icon_pixmap (f, pixmap_id)
     struct frame *f;
     int pixmap_id;
{
  Pixmap icon_pixmap;

#ifndef USE_X_TOOLKIT
  Window window = FRAME_X_WINDOW (f);
#endif

  if (pixmap_id > 0)
    {
      icon_pixmap = x_bitmap_pixmap (f, pixmap_id);
      f->output_data.x->wm_hints.icon_pixmap = icon_pixmap;
    }
  else
    {
      /* It seems there is no way to turn off use of an icon pixmap.
	 The following line does it, only if no icon has yet been created,
	 for some window managers.  But with mwm it crashes.
	 Some people say it should clear the IconPixmapHint bit in this case,
	 but that doesn't work, and the X consortium said it isn't the
	 right thing at all.  Since there is no way to win,
	 best to explicitly give up.  */
#if 0
      f->output_data.x->wm_hints.icon_pixmap = None;
#else
      return;
#endif
    }

#ifdef USE_X_TOOLKIT /* same as in x_wm_set_window_state.  */

  {
    Arg al[1];
    XtSetArg (al[0], XtNiconPixmap, icon_pixmap);
    XtSetValues (f->output_data.x->widget, al, 1);
  }

#else /* not USE_X_TOOLKIT */

  f->output_data.x->wm_hints.flags |= IconPixmapHint;
  XSetWMHints (FRAME_X_DISPLAY (f), window, &f->output_data.x->wm_hints);

#endif /* not USE_X_TOOLKIT */
}

#endif /* MAC_TODO */

void
x_wm_set_icon_position (f, icon_x, icon_y)
     struct frame *f;
     int icon_x, icon_y;
{
#if 0 /* MAC_TODO: no icons on Mac */
#ifdef USE_X_TOOLKIT
  Window window = XtWindow (f->output_data.x->widget);
#else
  Window window = FRAME_X_WINDOW (f);
#endif

  f->output_data.x->wm_hints.flags |= IconPositionHint;
  f->output_data.x->wm_hints.icon_x = icon_x;
  f->output_data.x->wm_hints.icon_y = icon_y;

  XSetWMHints (FRAME_X_DISPLAY (f), window, &f->output_data.x->wm_hints);
#endif /* MAC_TODO */
}


/***********************************************************************
			  XLFD Pattern Match
 ***********************************************************************/

/* An XLFD pattern is divided into blocks delimited by '*'.  This
   structure holds information for each block.  */
struct xlfdpat_block
{
  /* Length of the pattern string in this block.  Non-zero except for
     the first and the last blocks.  */
  int len;

  /* Pattern string except the last character in this block.  The last
     character is replaced with NUL in order to use it as a
     sentinel.  */
  unsigned char *pattern;

  /* Last character of the pattern string.  Must not be '?'.  */
  unsigned char last_char;

  /* One of the tables for the Boyer-Moore string search.  It
     specifies the number of positions to proceed for each character
     with which the match fails.  */
  int skip[256];

  /* The skip value for the last character in the above `skip' is
     assigned to `infinity' in order to simplify a loop condition.
     The original value is saved here.  */
  int last_char_skip;
};

struct xlfdpat
{
  /* Normalized pattern string.  "Normalized" means that capital
     letters are lowered, blocks are not empty except the first and
     the last ones, and trailing '?'s in a block that is not the last
     one are moved to the next one.  The last character in each block
     is replaced with NUL.  */
  unsigned char *buf;

  /* Number of characters except '*'s and trailing '?'s in the
     normalized pattern string.  */
  int nchars;

  /* Number of trailing '?'s in the normalized pattern string.  */
  int trailing_anychars;

  /* Number of blocks and information for each block.  The latter is
     NULL if the pattern is exact (no '*' or '?' in it).  */
  int nblocks;
  struct xlfdpat_block *blocks;
};

static void
xlfdpat_destroy (pat)
     struct xlfdpat *pat;
{
  if (pat)
    {
      if (pat->buf)
	{
	  if (pat->blocks)
	    xfree (pat->blocks);
	  xfree (pat->buf);
	}
      xfree (pat);
    }
}

static struct xlfdpat *
xlfdpat_create (pattern)
     const char *pattern;
{
  struct xlfdpat *pat;
  int nblocks, i, skip;
  unsigned char last_char, *p, *q, *anychar_head;
  const unsigned char *ptr;
  struct xlfdpat_block *blk;

  pat = xmalloc (sizeof (struct xlfdpat));
  pat->buf = xmalloc (strlen (pattern) + 1);

  /* Normalize the pattern string and store it to `pat->buf'.  */
  nblocks = 0;
  anychar_head = NULL;
  q = pat->buf;
  last_char = '\0';
  for (ptr = pattern; *ptr; ptr++)
    {
      unsigned char c = *ptr;

      if (c == '*')
	if (last_char == '*')
	  /*  ...a** -> ...a*  */
	  continue;
	else
	  {
	    if (last_char == '?')
	      {
		if (anychar_head > pat->buf && *(anychar_head - 1) == '*')
		  /*  ...*??* -> ...*??  */
		  continue;
		else
		  /*  ...a??* -> ...a*??  */
		  {
		    *anychar_head++ = '*';
		    c = '?';
		  }
	      }
	    nblocks++;
	  }
      else if (c == '?')
	{
	  if (last_char != '?')
	    anychar_head = q;
	}
      else
	/* On Mac OS X 10.3, tolower also converts non-ASCII
	   characters for some locales.  */
	if (isascii (c))
	  c = tolower (c);

      *q++ = last_char = c;
    }
  *q = '\0';
  nblocks++;
  pat->nblocks = nblocks;
  if (last_char != '?')
    pat->trailing_anychars = 0;
  else
    {
      pat->trailing_anychars = q - anychar_head;
      q = anychar_head;
    }
  pat->nchars = q - pat->buf - (nblocks - 1);

  if (anychar_head == NULL && nblocks == 1)
    {
      /* The pattern is exact.  */
      pat->blocks = NULL;
      return pat;
    }

  pat->blocks = xmalloc (sizeof (struct xlfdpat_block) * nblocks);

  /* Divide the normalized pattern into blocks.  */
  p = pat->buf;
  for (blk = pat->blocks; blk < pat->blocks + nblocks - 1; blk++)
    {
      blk->pattern = p;
      while (*p != '*')
	p++;
      blk->len = p - blk->pattern;
      p++;
    }
  blk->pattern = p;
  blk->len = q - blk->pattern;

  /* Setup a table for the Boyer-Moore string search.  */
  for (blk = pat->blocks; blk < pat->blocks + nblocks; blk++)
    if (blk->len != 0)
      {
	blk->last_char = blk->pattern[blk->len - 1];
	blk->pattern[blk->len - 1] = '\0';

	for (skip = 1; skip < blk->len; skip++)
	  if (blk->pattern[blk->len - skip - 1] == '?')
	    break;

	for (i = 0; i < 256; i++)
	  blk->skip[i] = skip;

	p = blk->pattern + (blk->len - skip);
	while (--skip > 0)
	  blk->skip[*p++] = skip;

	blk->last_char_skip = blk->skip[blk->last_char];
      }

  return pat;
}

static INLINE int
xlfdpat_exact_p (pat)
     struct xlfdpat *pat;
{
  return pat->blocks == NULL;
}

/* Return the first string in STRING + 0, ..., STRING + START_MAX such
   that the pattern in *BLK matches with its prefix.  Return NULL
   there is no such strings.  STRING must be lowered in advance.  */

static const char *
xlfdpat_block_match_1 (blk, string, start_max)
     struct xlfdpat_block *blk;
     const unsigned char *string;
     int start_max;
{
  int start, infinity;
  unsigned char *p;
  const unsigned char *s;

  xassert (blk->len > 0);
  xassert (start_max + blk->len <= strlen (string));
  xassert (blk->last_char != '?');

  /* See the comments in the function `boyer_moore' (search.c) for the
     use of `infinity'.  */
  infinity = start_max + blk->len + 1;
  blk->skip[blk->last_char] = infinity;

  start = 0;
  do
    {
      /* Check the last character of the pattern. */
      s = string + blk->len - 1;
      do
	{
	  start += blk->skip[*(s + start)];
	}
      while (start <= start_max);

      if (start < infinity)
	/* Couldn't find the last character.  */
	return NULL;

      /* No less than `infinity' means we could find the last
	 character at `s[start - infinity]'.  */
      start -= infinity;

      /* Check the remaining characters.  We prefer making no-'?'
	 cases faster because the use of '?' is really rare.  */
      p = blk->pattern;
      s = string + start;
      do
	{
	  while (*p++ == *s++)
	    ;
	}
      while (*(p - 1) == '?');

      if (*(p - 1) == '\0')
	/* Matched.  */
	return string + start;

      /* Didn't match.  */
      start += blk->last_char_skip;
    }
  while (start <= start_max);

  return NULL;
}

#define xlfdpat_block_match(b, s, m) \
  ((b)->len == 1 ? memchr ((s), (b)->last_char, (m) + 1) \
   : xlfdpat_block_match_1 (b, s, m))

/* Check if XLFD pattern PAT, which is generated by `xlfdpat_create',
   matches with STRING.  STRING must be lowered in advance.  */

static int
xlfdpat_match (pat, string)
     struct xlfdpat *pat;
     const unsigned char *string;
{
  int str_len, nblocks, i, start_max;
  struct xlfdpat_block *blk;
  const unsigned char *s;

  xassert (pat->nblocks > 0);

  if (xlfdpat_exact_p (pat))
    return strcmp (pat->buf, string) == 0;

  /* The number of the characters in the string must not be smaller
     than that in the pattern.  */
  str_len = strlen (string);
  if (str_len < pat->nchars + pat->trailing_anychars)
    return 0;

  /* Chop off the trailing '?'s.  */
  str_len -= pat->trailing_anychars;

  /* The last block.  When it is non-empty, it must match at the end
     of the string.  */
  nblocks = pat->nblocks;
  blk = pat->blocks + (nblocks - 1);
  if (nblocks == 1)
    /* The last block is also the first one.  */
    return (str_len == blk->len
	    && (blk->len == 0 || xlfdpat_block_match (blk, string, 0)));
  else if (blk->len != 0)
    if (!xlfdpat_block_match (blk, string + (str_len - blk->len), 0))
      return 0;

  /* The first block.  When it is non-empty, it must match at the
     beginning of the string.  */
  blk = pat->blocks;
  if (blk->len != 0)
    {
      s = xlfdpat_block_match (blk, string, 0);
      if (s == NULL)
	return 0;
      string = s + blk->len;
    }

  /* The rest of the blocks.  */
  start_max = str_len - pat->nchars;
  for (i = 1, blk++; i < nblocks - 1; i++, blk++)
    {
      s = xlfdpat_block_match (blk, string, start_max);
      if (s == NULL)
	return 0;
      start_max -= s - string;
      string = s + blk->len;
    }

  return 1;
}


/***********************************************************************
				Fonts
 ***********************************************************************/

/* Return a pointer to struct font_info of font FONT_IDX of frame F.  */

struct font_info *
x_get_font_info (f, font_idx)
     FRAME_PTR f;
     int font_idx;
{
  return (FRAME_MAC_FONT_TABLE (f) + font_idx);
}

/* the global font name table */
static char **font_name_table = NULL;
static int font_name_table_size = 0;
static int font_name_count = 0;

/* Alist linking font family names to Font Manager font family
   references (which can also be used as QuickDraw font IDs).  We use
   an alist because hash tables are not ready when the terminal frame
   for Mac OS Classic is created.  */
static Lisp_Object fm_font_family_alist;
#if USE_ATSUI
/* Hash table linking font family names to ATSU font IDs.  */
static Lisp_Object atsu_font_id_hash;
/* Alist linking Font Manager style to face attributes.  */
static Lisp_Object fm_style_face_attributes_alist;
extern Lisp_Object QCfamily, QCweight, QCslant, Qnormal, Qbold, Qitalic;
#endif
#if USE_MAC_FONT_PANEL
Lisp_Object Qpanel_closed, Qselection;
#endif

/* Alist linking character set strings to Mac text encoding and Emacs
   coding system. */
static Lisp_Object Vmac_charset_info_alist;

static Lisp_Object
create_text_encoding_info_alist ()
{
  Lisp_Object result = Qnil, rest;

  for (rest = Vmac_charset_info_alist; CONSP (rest); rest = XCDR (rest))
    {
      Lisp_Object charset_info = XCAR (rest);
      Lisp_Object charset, coding_system, text_encoding;
      Lisp_Object existing_info;

      if (!(CONSP (charset_info)
	    && (charset = XCAR (charset_info),
		STRINGP (charset))
	    && CONSP (XCDR (charset_info))
	    && (text_encoding = XCAR (XCDR (charset_info)),
		INTEGERP (text_encoding))
	    && CONSP (XCDR (XCDR (charset_info)))
	    && (coding_system = XCAR (XCDR (XCDR (charset_info))),
		SYMBOLP (coding_system))))
	continue;

      existing_info = assq_no_quit (text_encoding, result);
      if (NILP (existing_info))
	result = Fcons (list3 (text_encoding, coding_system, charset),
			result);
      else
	if (NILP (Fmember (charset, XCDR (XCDR (existing_info)))))
	  XSETCDR (XCDR (existing_info),
		   Fcons (charset, XCDR (XCDR (existing_info))));
    }

  return result;
}


static void
decode_mac_font_name (name, size, coding_system)
     char *name;
     int size;
     Lisp_Object coding_system;
{
  struct coding_system coding;
  char *buf, *p;

  if (!NILP (coding_system) && !NILP (Fcoding_system_p (coding_system)))
    {
      for (p = name; *p; p++)
	if (!isascii (*p) || iscntrl (*p))
	  break;

      if (*p)
	{
	  setup_coding_system (coding_system, &coding);
	  coding.src_multibyte = 0;
	  coding.dst_multibyte = 1;
	  coding.mode |= CODING_MODE_LAST_BLOCK;
	  coding.composing = COMPOSITION_DISABLED;
	  buf = (char *) alloca (size);

	  decode_coding (&coding, name, buf, strlen (name), size - 1);
	  bcopy (buf, name, coding.produced);
	  name[coding.produced] = '\0';
	}
    }

  /* If there's just one occurrence of '-' in the family name, it is
     replaced with '_'.  (More than one occurrence of '-' means a
     "FOUNDRY-FAMILY-CHARSET"-style name.)  */
  p = strchr (name, '-');
  if (p && strchr (p + 1, '-') == NULL)
    *p = '_';

  for (p = name; *p; p++)
    /* On Mac OS X 10.3, tolower also converts non-ASCII characters
       for some locales.  */
    if (isascii (*p))
      *p = tolower (*p);
}


static char *
mac_to_x_fontname (name, size, style, charset)
     const char *name;
     int size;
     Style style;
     char *charset;
{
  Str31 foundry, cs;
  Str255 family;
  char xf[256], *result;
  unsigned char *p;

  if (sscanf (name, "%31[^-]-%255[^-]-%31s", foundry, family, cs) == 3)
    charset = cs;
  else
    {
      strcpy(foundry, "Apple");
      strcpy(family, name);
    }

  sprintf (xf, "%s-%c-normal--%d-%d-%d-%d-m-%d-%s",
	   style & bold ? "bold" : "medium", style & italic ? 'i' : 'r',
	   size, size * 10, size ? 72 : 0, size ? 72 : 0, size * 10, charset);

  result = xmalloc (strlen (foundry) + strlen (family) + strlen (xf) + 3 + 1);
  sprintf (result, "-%s-%s-%s", foundry, family, xf);
  for (p = result; *p; p++)
    /* On Mac OS X 10.3, tolower also converts non-ASCII characters
       for some locales.  */
    if (isascii (*p))
      *p = tolower (*p);
  return result;
}


/* Parse fully-specified and instantiated X11 font spec XF, and store
   the results to FAMILY, *SIZE, *STYLE, and CHARSET.  Return 1 if the
   parsing succeeded, and 0 otherwise.  For FAMILY and CHARSET, the
   caller must allocate at least 256 and 32 bytes respectively.  For
   ordinary Mac fonts, the value stored to FAMILY should just be their
   names, like "monaco", "Taipei", etc.  Fonts converted from the GNU
   intlfonts collection contain their charset designation in their
   names, like "ETL-Fixed-iso8859-1", "ETL-Fixed-koi8-r", etc.  Both
   types of font names are handled accordingly.  */

const int kDefaultFontSize = 12;

static int
parse_x_font_name (xf, family, size, style, charset)
     const char *xf;
     char *family;
     int *size;
     Style *style;
     char *charset;
{
  Str31 foundry, weight;
  int point_size, avgwidth;
  char slant[2], *p;

  if (sscanf (xf, "-%31[^-]-%255[^-]-%31[^-]-%1[^-]-%*[^-]-%*[^-]-%d-%d-%*[^-]-%*[^-]-%*c-%d-%31s",
              foundry, family, weight, slant, size,
	      &point_size, &avgwidth, charset) != 8
      && sscanf (xf, "-%31[^-]-%255[^-]-%31[^-]-%1[^-]-%*[^-]--%d-%d-%*[^-]-%*[^-]-%*c-%d-%31s",
		 foundry, family, weight, slant, size,
		 &point_size, &avgwidth, charset) != 8)
    return 0;

  if (*size == 0)
    {
      if (point_size > 0)
	*size = point_size / 10;
      else if (avgwidth > 0)
	*size = avgwidth / 10;
    }
  if (*size == 0)
    *size = kDefaultFontSize;

  *style = normal;
  if (strcmp (weight, "bold") == 0)
    *style |= bold;
  if (*slant == 'i')
    *style |= italic;

  if (NILP (Fassoc (build_string (charset), Vmac_charset_info_alist)))
    {
      int foundry_len = strlen (foundry), family_len = strlen (family);

      if (foundry_len + family_len + strlen (charset) + 2 < sizeof (Str255))
	{
	  /* Like sprintf (family, "%s-%s-%s", foundry, family, charset),
	     but take overlap into account.  */
	  memmove (family + foundry_len + 1, family, family_len);
	  memcpy (family, foundry, foundry_len);
	  family[foundry_len] = '-';
	  family[foundry_len + 1 + family_len] = '-';
	  strcpy (family + foundry_len + 1 + family_len + 1, charset);
	}
      else
	return 0;
    }

  for (p = family; *p; p++)
    /* On Mac OS X 10.3, tolower also converts non-ASCII characters
       for some locales.  */
    if (isascii (*p))
      *p = tolower (*p);

  return 1;
}


static void
add_font_name_table_entry (char *font_name)
{
  if (font_name_table_size == 0)
    {
      font_name_table_size = 256;
      font_name_table = (char **)
	xmalloc (font_name_table_size * sizeof (char *));
    }
  else if (font_name_count + 1 >= font_name_table_size)
    {
      font_name_table_size *= 2;
      font_name_table = (char **)
	xrealloc (font_name_table,
		  font_name_table_size * sizeof (char *));
    }

  font_name_table[font_name_count++] = font_name;
}

static void
add_mac_font_name (name, size, style, charset)
     const char *name;
     int size;
     Style style;
     const char *charset;
{
  if (size > 0)
    add_font_name_table_entry (mac_to_x_fontname (name, size, style, charset));
  else
    {
      add_font_name_table_entry (mac_to_x_fontname (name, 0, style, charset));
      add_font_name_table_entry (mac_to_x_fontname (name, 0, italic, charset));
      add_font_name_table_entry (mac_to_x_fontname (name, 0, bold, charset));
      add_font_name_table_entry (mac_to_x_fontname (name, 0, italic | bold,
						    charset));
    }
}

#if USE_ATSUI
static FMFontStyle
fm_get_style_from_font (font)
     FMFont font;
{
  OSStatus err;
  FMFontStyle style = normal;
  ByteCount len;
  UInt16 mac_style;
  FMFontFamily font_family;
#define FONT_HEADER_MAC_STYLE_OFFSET (4*4 + 2*2 + 8*2 + 2*4)

#if __LP64__
  err = ATSFontGetTable (FMGetATSFontRefFromFont (font),
			 'head', FONT_HEADER_MAC_STYLE_OFFSET,
			 sizeof (mac_style), &mac_style, &len);
  if (err == noErr
      && len >= FONT_HEADER_MAC_STYLE_OFFSET + sizeof (mac_style))
    style = EndianU16_BtoN (mac_style);
#else
  /* FMGetFontFamilyInstanceFromFont returns `normal' as the style of
     some font (e.g., Optima) even if it is `bold'.  */
  err = FMGetFontTable (font, 'head', FONT_HEADER_MAC_STYLE_OFFSET,
			sizeof (mac_style), &mac_style, &len);
  if (err == noErr
      && len >= FONT_HEADER_MAC_STYLE_OFFSET + sizeof (mac_style))
    style = EndianU16_BtoN (mac_style);
  else
    FMGetFontFamilyInstanceFromFont (font, &font_family, &style);
#endif

  return style;
}

static ATSUFontID
atsu_find_font_from_family_name (family)
     const char *family;
{
  struct Lisp_Hash_Table *h = XHASH_TABLE (atsu_font_id_hash);
  unsigned hash_code;
  int i;
  Lisp_Object rest, best;
  FMFontStyle min_style, style;

  i = hash_lookup (h, make_unibyte_string (family, strlen (family)),
		   &hash_code);
  if (i < 0)
    return kATSUInvalidFontID;

  rest = HASH_VALUE (h, i);
  if (INTEGERP (rest) || (CONSP (rest) && INTEGERP (XCDR (rest))))
    return cons_to_long (rest);

  rest = Fnreverse (rest);
  best = XCAR (rest);
  rest = XCDR (rest);
  if (!NILP (rest)
      && (min_style = fm_get_style_from_font (cons_to_long (best))) != normal)
    do
      {
	style = fm_get_style_from_font (cons_to_long (XCAR (rest)));
	if (style < min_style)
	  {
	    best = XCAR (rest);
	    if (style == normal)
	      break;
	    else
	      min_style = style;
	  }
	rest = XCDR (rest);
      }
    while (!NILP (rest));

  HASH_VALUE (h, i) = best;
  return cons_to_long (best);
}

static Lisp_Object
fm_style_to_face_attributes (fm_style)
     FMFontStyle fm_style;
{
  Lisp_Object tem;

  fm_style &= (bold | italic);
  tem = assq_no_quit (make_number (fm_style),
		      fm_style_face_attributes_alist);
  if (!NILP (tem))
    return XCDR (tem);

  tem = list4 (QCweight, fm_style & bold ? Qbold : Qnormal,
	       QCslant, fm_style & italic ? Qitalic : Qnormal);
  fm_style_face_attributes_alist =
    Fcons (Fcons (make_number (fm_style), tem),
	   fm_style_face_attributes_alist);

  return tem;
}

static Lisp_Object
atsu_find_font_family_name (font_id)
     ATSUFontID font_id;
{
  OSStatus err;
  ByteCount len;
  Lisp_Object family = Qnil;

  err = ATSUFindFontName (font_id, kFontFamilyName,
			  kFontMacintoshPlatform, kFontNoScript,
			  kFontNoLanguage, 0, NULL, &len, NULL);
  if (err == noErr)
    {
      family = make_uninit_string (len);
      err = ATSUFindFontName (font_id, kFontFamilyName,
			      kFontMacintoshPlatform, kFontNoScript,
			      kFontNoLanguage, len, SDATA (family),
			      NULL, NULL);
    }
  if (err == noErr)
    decode_mac_font_name (SDATA (family), len + 1, Qnil);

  return family;
}

Lisp_Object
mac_atsu_font_face_attributes (font_id)
     ATSUFontID font_id;
{
  Lisp_Object family, style_attrs;

  family = atsu_find_font_family_name (font_id);
  if (NILP (family))
    return Qnil;
  style_attrs = fm_style_to_face_attributes (fm_get_style_from_font (font_id));
  return Fcons (QCfamily, Fcons (family, style_attrs));
}
#endif

/* Sets up the table font_name_table to contain the list of all fonts
   in the system the first time the table is used so that the Resource
   Manager need not be accessed every time this information is
   needed.  */

static void
init_font_name_table ()
{
#if TARGET_API_MAC_CARBON
  FMFontFamilyIterator ffi;
  FMFontFamilyInstanceIterator ffii;
  FMFontFamily ff;
  Lisp_Object text_encoding_info_alist;
  struct gcpro gcpro1;

  text_encoding_info_alist = create_text_encoding_info_alist ();

#if USE_ATSUI
#if USE_CG_TEXT_DRAWING
  init_cg_text_anti_aliasing_threshold ();
#endif
  if (!NILP (assq_no_quit (make_number (kTextEncodingMacUnicode),
			   text_encoding_info_alist)))
    {
      OSStatus err;
      struct Lisp_Hash_Table *h;
      unsigned hash_code;
      ItemCount nfonts, i;
      ATSUFontID *font_ids = NULL;
      Lisp_Object prev_family = Qnil;
      int j;

      atsu_font_id_hash =
	make_hash_table (Qequal, make_number (DEFAULT_HASH_SIZE),
			 make_float (DEFAULT_REHASH_SIZE),
			 make_float (DEFAULT_REHASH_THRESHOLD),
			 Qnil, Qnil, Qnil);
      h = XHASH_TABLE (atsu_font_id_hash);

      err = ATSUFontCount (&nfonts);
      if (err == noErr)
	{
	  font_ids = xmalloc (sizeof (ATSUFontID) * nfonts);
	  err = ATSUGetFontIDs (font_ids, nfonts, NULL);
	}
      if (err == noErr)
	for (i = 0; i < nfonts; i++)
	  {
	    Lisp_Object family;

	    family = atsu_find_font_family_name (font_ids[i]);
	    if (NILP (family) || SREF (family, 0) == '.')
	      continue;
	    if (!NILP (Fequal (prev_family, family)))
	      family = prev_family;
	    else
	      j = hash_lookup (h, family, &hash_code);
	    if (j < 0)
	      {
		add_mac_font_name (SDATA (family), 0, normal, "iso10646-1");
		j = hash_put (h, family, Fcons (long_to_cons (font_ids[i]),
						Qnil), hash_code);
	      }
	    else if (EQ (prev_family, family))
	      HASH_VALUE (h, j) = Fcons (long_to_cons (font_ids[i]),
					 HASH_VALUE (h, j));
	    prev_family = family;
	  }
      if (font_ids)
	xfree (font_ids);
    }
#endif

#if USE_QUICKDRAW
  /* Create a dummy instance iterator here to avoid creating and
     destroying it in the loop.  */
  if (FMCreateFontFamilyInstanceIterator (0, &ffii) != noErr)
    return;
  /* Create an iterator to enumerate the font families.  */
  if (FMCreateFontFamilyIterator (NULL, NULL, kFMDefaultOptions, &ffi)
      != noErr)
    {
      FMDisposeFontFamilyInstanceIterator (&ffii);
      return;
    }

  GCPRO1 (text_encoding_info_alist);

  while (FMGetNextFontFamily (&ffi, &ff) == noErr)
    {
      Str255 name;
      FMFont font;
      FMFontStyle style;
      FMFontSize size;
      TextEncoding encoding;
      TextEncodingBase sc;
      Lisp_Object text_encoding_info, family;

      if (FMGetFontFamilyName (ff, name) != noErr)
	continue;
      p2cstr (name);
      if (*name == '.')
	continue;

      if (FMGetFontFamilyTextEncoding (ff, &encoding) != noErr)
	continue;
      sc = GetTextEncodingBase (encoding);
      text_encoding_info = assq_no_quit (make_number (sc),
					 text_encoding_info_alist);
      if (NILP (text_encoding_info))
	text_encoding_info = assq_no_quit (make_number (kTextEncodingMacRoman),
					   text_encoding_info_alist);
      decode_mac_font_name (name, sizeof (name),
			    XCAR (XCDR (text_encoding_info)));
      family = build_string (name);
      if (!NILP (Fassoc (family, fm_font_family_alist)))
	continue;
      fm_font_family_alist = Fcons (Fcons (family, make_number (ff)),
				    fm_font_family_alist);

      /* Point the instance iterator at the current font family.  */
      if (FMResetFontFamilyInstanceIterator (ff, &ffii) != noErr)
	continue;

      while (FMGetNextFontFamilyInstance (&ffii, &font, &style, &size)
	     == noErr)
	{
	  Lisp_Object rest = XCDR (XCDR (text_encoding_info));

	  if (size > 0 || style == normal)
	    for (; !NILP (rest); rest = XCDR (rest))
	      add_mac_font_name (name, size, style, SDATA (XCAR (rest)));
	}
    }

  UNGCPRO;

  /* Dispose of the iterators.  */
  FMDisposeFontFamilyIterator (&ffi);
  FMDisposeFontFamilyInstanceIterator (&ffii);
#endif	/* USE_QUICKDRAW */
#else  /* !TARGET_API_MAC_CARBON */
  GrafPtr port;
  SInt16 fontnum, old_fontnum;
  int num_mac_fonts = CountResources('FOND');
  int i, j;
  Handle font_handle, font_handle_2;
  short id, scriptcode;
  ResType type;
  Str255 name;
  struct FontAssoc *fat;
  struct AsscEntry *assc_entry;
  Lisp_Object text_encoding_info_alist, text_encoding_info, family;
  struct gcpro gcpro1;

  GetPort (&port);  /* save the current font number used */
  old_fontnum = port->txFont;

  text_encoding_info_alist = create_text_encoding_info_alist ();

  GCPRO1 (text_encoding_info_alist);

  for (i = 1; i <= num_mac_fonts; i++)  /* get all available fonts */
    {
      font_handle = GetIndResource ('FOND', i);
      if (!font_handle)
	continue;

      GetResInfo (font_handle, &id, &type, name);
      GetFNum (name, &fontnum);
      p2cstr (name);
      if (fontnum == 0 || *name == '.')
	continue;

      TextFont (fontnum);
      scriptcode = FontToScript (fontnum);
      text_encoding_info = assq_no_quit (make_number (scriptcode),
					 text_encoding_info_alist);
      if (NILP (text_encoding_info))
	text_encoding_info = assq_no_quit (make_number (smRoman),
					   text_encoding_info_alist);
      decode_mac_font_name (name, sizeof (name),
			    XCAR (XCDR (text_encoding_info)));
      family = build_string (name);
      if (!NILP (Fassoc (family, fm_font_family_alist)))
	continue;
      fm_font_family_alist = Fcons (Fcons (family, make_number (fontnum)),
				    fm_font_family_alist);
      do
	{
	  HLock (font_handle);

	  if (GetResourceSizeOnDisk (font_handle)
	      >= sizeof (struct FamRec))
	    {
	      fat = (struct FontAssoc *) (*font_handle
					  + sizeof (struct FamRec));
	      assc_entry
		= (struct AsscEntry *) (*font_handle
					+ sizeof (struct FamRec)
					+ sizeof (struct FontAssoc));

	      for (j = 0; j <= fat->numAssoc; j++, assc_entry++)
		{
		  Lisp_Object rest = XCDR (XCDR (text_encoding_info));

		  for (; !NILP (rest); rest = XCDR (rest))
		    add_mac_font_name (name, assc_entry->fontSize,
				       assc_entry->fontStyle,
				       SDATA (XCAR (rest)));
		}
	    }

	  HUnlock (font_handle);
	  font_handle_2 = GetNextFOND (font_handle);
	  ReleaseResource (font_handle);
	  font_handle = font_handle_2;
	}
      while (ResError () == noErr && font_handle);
    }

  UNGCPRO;

  TextFont (old_fontnum);
#endif  /* !TARGET_API_MAC_CARBON */
}


void
mac_clear_font_name_table ()
{
  int i;

  for (i = 0; i < font_name_count; i++)
    xfree (font_name_table[i]);
  xfree (font_name_table);
  font_name_table = NULL;
  font_name_table_size = font_name_count = 0;
  fm_font_family_alist = Qnil;
}


enum xlfd_scalable_field_index
  {
    XLFD_SCL_PIXEL_SIZE,
    XLFD_SCL_POINT_SIZE,
    XLFD_SCL_AVGWIDTH,
    XLFD_SCL_LAST
  };

static const int xlfd_scalable_fields[] =
  {
    6,				/* PIXEL_SIZE */
    7,				/* POINT_SIZE */
    11,				/* AVGWIDTH */
    -1
  };

static Lisp_Object
mac_do_list_fonts (pattern, maxnames)
     const char *pattern;
     int maxnames;
{
  int i, n_fonts = 0;
  Lisp_Object font_list = Qnil;
  struct xlfdpat *pat;
  char *scaled;
  const char *ptr;
  int scl_val[XLFD_SCL_LAST], *val;
  const int *field;
  int exact;

  if (font_name_table == NULL)  /* Initialize when first used.  */
    init_font_name_table ();

  for (i = 0; i < XLFD_SCL_LAST; i++)
    scl_val[i] = -1;

  /* If the pattern contains 14 dashes and one of PIXEL_SIZE,
     POINT_SIZE, and AVGWIDTH fields is explicitly specified, scalable
     fonts are scaled according to the specified size.  */
  ptr = pattern;
  i = 0;
  field = xlfd_scalable_fields;
  val = scl_val;
  if (*ptr == '-')
    do
      {
	ptr++;
	if (i == *field)
	  {
	    if ('0' <= *ptr && *ptr <= '9')
	      {
		*val = *ptr++ - '0';
		while ('0' <= *ptr && *ptr <= '9' && *val < 10000)
		  *val = *val * 10 + *ptr++ - '0';
		if (*ptr != '-')
		  *val = -1;
	      }
	    field++;
	    val++;
	  }
	ptr = strchr (ptr, '-');
	i++;
      }
    while (ptr && i < 14);

  if (i == 14 && ptr == NULL)
    {
      if (scl_val[XLFD_SCL_PIXEL_SIZE] < 0)
	scl_val[XLFD_SCL_PIXEL_SIZE] =
	  (scl_val[XLFD_SCL_POINT_SIZE] > 0 ? scl_val[XLFD_SCL_POINT_SIZE] / 10
	   : (scl_val[XLFD_SCL_AVGWIDTH] > 0 ? scl_val[XLFD_SCL_AVGWIDTH] / 10
	      : -1));
      if (scl_val[XLFD_SCL_POINT_SIZE] < 0)
	scl_val[XLFD_SCL_POINT_SIZE] =
	  (scl_val[XLFD_SCL_PIXEL_SIZE] > 0 ? scl_val[XLFD_SCL_PIXEL_SIZE] * 10
	   : (scl_val[XLFD_SCL_AVGWIDTH] > 0 ? scl_val[XLFD_SCL_AVGWIDTH]
	      : -1));
      if (scl_val[XLFD_SCL_AVGWIDTH] < 0)
	scl_val[XLFD_SCL_AVGWIDTH] =
	  (scl_val[XLFD_SCL_PIXEL_SIZE] > 0 ? scl_val[XLFD_SCL_PIXEL_SIZE] * 10
	   : (scl_val[XLFD_SCL_POINT_SIZE] > 0 ? scl_val[XLFD_SCL_POINT_SIZE]
	      : -1));
    }
  else
    scl_val[XLFD_SCL_PIXEL_SIZE] = -1;

  pat = xlfdpat_create (pattern);
  if (pat == NULL)
    return Qnil;

  exact = xlfdpat_exact_p (pat);

  for (i = 0; i < font_name_count; i++)
    {
      if (xlfdpat_match (pat, font_name_table[i]))
	{
	  font_list = Fcons (build_string (font_name_table[i]), font_list);
	  if (exact || (maxnames > 0 && ++n_fonts >= maxnames))
	    break;
	}
      else if (scl_val[XLFD_SCL_PIXEL_SIZE] > 0
	       && (ptr = strstr (font_name_table[i], "-0-0-0-0-m-0-")))
	{
	  int former_len = ptr - font_name_table[i];

	  scaled = xmalloc (strlen (font_name_table[i]) + 20 + 1);
	  memcpy (scaled, font_name_table[i], former_len);
	  sprintf (scaled + former_len,
		   "-%d-%d-72-72-m-%d-%s",
		   scl_val[XLFD_SCL_PIXEL_SIZE],
		   scl_val[XLFD_SCL_POINT_SIZE],
		   scl_val[XLFD_SCL_AVGWIDTH],
		   ptr + sizeof ("-0-0-0-0-m-0-") - 1);

	  if (xlfdpat_match (pat, scaled))
	    {
	      font_list = Fcons (build_string (scaled), font_list);
	      xfree (scaled);
	      if (exact || (maxnames > 0 && ++n_fonts >= maxnames))
		  break;
	    }
	  else
	    xfree (scaled);
	}
    }

  xlfdpat_destroy (pat);

  return font_list;
}

/* Return a list of names of available fonts matching PATTERN on frame F.

   Frame F null means we have not yet created any frame on Mac, and
   consult the first display in x_display_list.  MAXNAMES sets a limit
   on how many fonts to match.  */

Lisp_Object
x_list_fonts (f, pattern, size, maxnames)
     struct frame *f;
     Lisp_Object pattern;
     int size, maxnames;
{
  Lisp_Object list = Qnil, patterns, tem, key;
  struct mac_display_info *dpyinfo
    = f ? FRAME_MAC_DISPLAY_INFO (f) : x_display_list;

  xassert (size <= 0);

  patterns = Fassoc (pattern, Valternate_fontname_alist);
  if (NILP (patterns))
    patterns = Fcons (pattern, Qnil);

  for (; CONSP (patterns); patterns = XCDR (patterns))
    {
      pattern = XCAR (patterns);

      if (!STRINGP (pattern))
        continue;

      tem = XCAR (XCDR (dpyinfo->name_list_element));
      key = Fcons (pattern, make_number (maxnames));

      list = Fassoc (key, tem);
      if (!NILP (list))
	{
	  list = Fcdr_safe (list);
	  /* We have a cashed list.  Don't have to get the list again.  */
	  goto label_cached;
	}

      BLOCK_INPUT;
      list = mac_do_list_fonts (SDATA (pattern), maxnames);
      UNBLOCK_INPUT;

      /* MAC_TODO: add code for matching outline fonts here */

      /* Now store the result in the cache.  */
      XSETCAR (XCDR (dpyinfo->name_list_element),
	       Fcons (Fcons (key, list),
		      XCAR (XCDR (dpyinfo->name_list_element))));

    label_cached:
      if (NILP (list)) continue; /* Try the remaining alternatives.  */
    }

  return list;
}


#if GLYPH_DEBUG

/* Check that FONT is valid on frame F.  It is if it can be found in F's
   font table.  */

static void
x_check_font (f, font)
     struct frame *f;
     XFontStruct *font;
{
  int i;
  struct x_display_info *dpyinfo = FRAME_X_DISPLAY_INFO (f);

  xassert (font != NULL);

  for (i = 0; i < dpyinfo->n_fonts; i++)
    if (dpyinfo->font_table[i].name
	&& font == dpyinfo->font_table[i].font)
      break;

  xassert (i < dpyinfo->n_fonts);
}

#endif /* GLYPH_DEBUG != 0 */

/* Set *W to the minimum width, *H to the minimum font height of FONT.
   Note: There are (broken) X fonts out there with invalid XFontStruct
   min_bounds contents.  For example, handa@etl.go.jp reports that
   "-adobe-courier-medium-r-normal--*-180-*-*-m-*-iso8859-1" fonts
   have font->min_bounds.width == 0.  */

static INLINE void
x_font_min_bounds (font, w, h)
     MacFontStruct *font;
     int *w, *h;
{
  *h = FONT_HEIGHT (font);
  *w = font->min_bounds.width;
}


/* Compute the smallest character width and smallest font height over
   all fonts available on frame F.  Set the members smallest_char_width
   and smallest_font_height in F's x_display_info structure to
   the values computed.  Value is non-zero if smallest_font_height or
   smallest_char_width become smaller than they were before.  */

static int
x_compute_min_glyph_bounds (f)
     struct frame *f;
{
  int i;
  struct mac_display_info *dpyinfo = FRAME_MAC_DISPLAY_INFO (f);
  MacFontStruct *font;
  int old_width = dpyinfo->smallest_char_width;
  int old_height = dpyinfo->smallest_font_height;

  dpyinfo->smallest_font_height = 100000;
  dpyinfo->smallest_char_width = 100000;

  for (i = 0; i < dpyinfo->n_fonts; ++i)
    if (dpyinfo->font_table[i].name)
      {
	struct font_info *fontp = dpyinfo->font_table + i;
	int w, h;

	font = (MacFontStruct *) fontp->font;
	xassert (font != (MacFontStruct *) ~0);
	x_font_min_bounds (font, &w, &h);

	dpyinfo->smallest_font_height = min (dpyinfo->smallest_font_height, h);
	dpyinfo->smallest_char_width = min (dpyinfo->smallest_char_width, w);
      }

  xassert (dpyinfo->smallest_char_width > 0
	   && dpyinfo->smallest_font_height > 0);

  return (dpyinfo->n_fonts == 1
	  || dpyinfo->smallest_char_width < old_width
	  || dpyinfo->smallest_font_height < old_height);
}


/* Determine whether given string is a fully-specified XLFD: all 14
   fields are present, none is '*'.  */

static int
is_fully_specified_xlfd (p)
     const char *p;
{
  int i;
  char *q;

  if (*p != '-')
    return 0;

  for (i = 0; i < 13; i++)
    {
      q = strchr (p + 1, '-');
      if (q == NULL)
        return 0;
      if (q - p == 2 && *(p + 1) == '*')
        return 0;
      p = q;
    }

  if (strchr (p + 1, '-') != NULL)
    return 0;

  if (*(p + 1) == '*' && *(p + 2) == '\0')
    return 0;

  return 1;
}


/* mac_load_query_font creates and returns an internal representation
   for a font in a MacFontStruct struct.  There is really no concept
   corresponding to "loading" a font on the Mac.  But we check its
   existence and find the font number and all other information for it
   and store them in the returned MacFontStruct.  */

static MacFontStruct *
mac_load_query_font (f, fontname)
     struct frame *f;
     char *fontname;
{
  int size;
  char *name;
  Str255 family;
  Str31 charset;
  SInt16 fontnum;
#if USE_ATSUI
  static ATSUFontID font_id;
  ATSUStyle mac_style = NULL;
#endif
  Style fontface;
#if TARGET_API_MAC_CARBON
  TextEncoding encoding;
  int scriptcode;
#else
  short scriptcode;
#endif
  MacFontStruct *font;
  XCharStruct *space_bounds = NULL, *pcm;

  if (is_fully_specified_xlfd (fontname))
    name = fontname;
  else
    {
      Lisp_Object matched_fonts;

      matched_fonts = mac_do_list_fonts (fontname, 1);
      if (NILP (matched_fonts))
	return NULL;
      name = SDATA (XCAR (matched_fonts));
    }

  if (parse_x_font_name (name, family, &size, &fontface, charset) == 0)
    return NULL;

#if USE_ATSUI
  if (strcmp (charset, "iso10646-1") == 0) /* XXX */
    {
      OSStatus err;
      static const ATSUAttributeTag tags[] =
	{kATSUFontTag, kATSUSizeTag,
	 kATSUQDBoldfaceTag, kATSUQDItalicTag};
      static const ByteCount sizes[] =
	{sizeof (ATSUFontID), sizeof (Fixed),
	 sizeof (Boolean), sizeof (Boolean)};
      static Fixed size_fixed;
      static Boolean bold_p, italic_p;
      static const ATSUAttributeValuePtr values[] =
	{&font_id, &size_fixed,
	 &bold_p, &italic_p};
      static const ATSUFontFeatureType types[] =
	{kAllTypographicFeaturesType, kDiacriticsType};
      static const ATSUFontFeatureSelector selectors[] =
	{kAllTypeFeaturesOffSelector, kDecomposeDiacriticsSelector};
      FMFontStyle style;

      font_id = atsu_find_font_from_family_name (family);
      if (font_id == kATSUInvalidFontID)
	return NULL;
      size_fixed = Long2Fix (size);
      bold_p = (fontface & bold) != 0;
      italic_p = (fontface & italic) != 0;
      err = ATSUCreateStyle (&mac_style);
      if (err != noErr)
	return NULL;
      err = ATSUSetFontFeatures (mac_style, sizeof (types) / sizeof (types[0]),
      				 types, selectors);
      if (err != noErr)
	return NULL;
      err = ATSUSetAttributes (mac_style, sizeof (tags) / sizeof (tags[0]),
			       tags, sizes, values);
      if (err != noErr)
	return NULL;
#if !__LP64__
      err = FMGetFontFamilyInstanceFromFont (font_id, &fontnum, &style);
      if (err != noErr)
#endif
	fontnum = -1;
      scriptcode = kTextEncodingMacUnicode;
    }
  else
#endif
    {
#if USE_QUICKDRAW
      Lisp_Object tmp = Fassoc (build_string (family), fm_font_family_alist);

      if (NILP (tmp))
	return NULL;
      fontnum = XINT (XCDR (tmp));
#if TARGET_API_MAC_CARBON
      if (FMGetFontFamilyTextEncoding (fontnum, &encoding) != noErr)
	return NULL;
      scriptcode = GetTextEncodingBase (encoding);
#else
      scriptcode = FontToScript (fontnum);
#endif
#else	/* !USE_QUICKDRAW */
      return NULL;
#endif	/* !USE_QUICKDRAW */
    }

  font = (MacFontStruct *) xmalloc (sizeof (struct MacFontStruct));

  font->mac_fontnum = fontnum;
  font->mac_fontsize = size;
  font->mac_fontface = fontface;
  font->mac_scriptcode = scriptcode;
#if USE_ATSUI
  font->mac_style = mac_style;
#if USE_CG_TEXT_DRAWING
  font->cg_font = NULL;
  font->cg_glyphs = NULL;
#endif
#endif

  /* Apple Japanese (SJIS) font is listed as both
     "*-jisx0208.1983-sjis" (Japanese script) and "*-jisx0201.1976-0"
     (Roman script) in init_font_name_table ().  The latter should be
     treated as a one-byte font.  */
  if (scriptcode == smJapanese && strcmp (charset, "jisx0201.1976-0") == 0)
    font->mac_scriptcode = smRoman;

  font->full_name = mac_to_x_fontname (family, size, fontface, charset);

#if USE_ATSUI
  if (font->mac_style)
    {
      OSStatus err;
      UniChar c;

      font->min_byte1 = 0;
      font->max_byte1 = 0xff;
      font->min_char_or_byte2 = 0;
      font->max_char_or_byte2 = 0xff;

      font->bounds.rows = xmalloc (sizeof (XCharStruct *) * 0x100);
      bzero (font->bounds.rows, sizeof (XCharStruct *) * 0x100);
      font->bounds.rows[0] = xmalloc (sizeof (XCharStruct) * 0x100);
      pcm_init (font->bounds.rows[0], 0x100);

#if USE_CG_TEXT_DRAWING
      if (fontnum != -1)
	{
	  FMFontStyle style;
	  ATSFontRef ats_font;

	  err = FMGetFontFromFontFamilyInstance (fontnum, fontface,
						 &font_id, &style);
	  /* Use CG text drawing if italic/bold is not synthesized.  */
	  if (err == noErr && style == fontface)
	    {
	      ats_font = FMGetATSFontRefFromFont (font_id);
	      font->cg_font = CGFontCreateWithPlatformFont (&ats_font);
	    }
	}

      if (font->cg_font)
	{
	  font->cg_glyphs = xmalloc (sizeof (CGGlyph) * 0x100);
	  bzero (font->cg_glyphs, sizeof (CGGlyph) * 0x100);
	}
#endif
      space_bounds = font->bounds.rows[0] + 0x20;
      err = mac_query_char_extents (font->mac_style, 0x20,
				    &font->ascent, &font->descent,
				    space_bounds,
#if USE_CG_TEXT_DRAWING
				    (font->cg_glyphs ? font->cg_glyphs + 0x20
				     : NULL)
#else
				    NULL
#endif
				    );
      if (err != noErr
	  || space_bounds->width <= 0 || FONT_HEIGHT (font) <= 0)
	{
	  mac_unload_font (&one_mac_display_info, font);
	  return NULL;
	}

      pcm = font->bounds.rows[0];
      for (c = 0x21; c <= 0xff; c++)
	{
	  if (c == 0xad)
	    /* Soft hyphen is not supported in ATSUI.  */
	    continue;
	  else if (c == 0x7f)
	    {
#if USE_CG_TEXT_DRAWING
	      if (font->cg_glyphs)
		{
		  c = 0x9f;
		  pcm = NULL;
		  continue;
		}
#endif
	      break;
	    }

	  mac_query_char_extents (font->mac_style, c, NULL, NULL,
				  pcm ? pcm + c : NULL,
#if USE_CG_TEXT_DRAWING
				  (font->cg_glyphs ? font->cg_glyphs + c
				   : NULL)
#else
				    NULL
#endif
				  );

#if USE_CG_TEXT_DRAWING
	  if (font->cg_glyphs && font->cg_glyphs[c] == 0)
	    {
	      /* Don't use CG text drawing if font substitution occurs in
		 ASCII or Latin-1 characters.  */
	      CGFontRelease (font->cg_font);
	      font->cg_font = NULL;
	      xfree (font->cg_glyphs);
	      font->cg_glyphs = NULL;
	      if (pcm == NULL)
		break;
	    }
#endif
	}
    }
  else
#endif
    {
#if !USE_QUICKDRAW
      abort ();
#else  /* USE_QUICKDRAW */
      OSStatus err;
      FontInfo the_fontinfo;
      int is_two_byte_font;

      mac_begin_clip (f, NULL);

      TextFont (fontnum);
      TextSize (size);
      TextFace (fontface);

      GetFontInfo (&the_fontinfo);

      font->ascent = the_fontinfo.ascent;
      font->descent = the_fontinfo.descent;

      is_two_byte_font = (font->mac_scriptcode == smJapanese
			  || font->mac_scriptcode == smTradChinese
			  || font->mac_scriptcode == smSimpChinese
			  || font->mac_scriptcode == smKorean);

      if (is_two_byte_font)
	{
	  int char_width;

	  font->min_byte1 = 0xa1;
	  font->max_byte1 = 0xfe;
	  font->min_char_or_byte2 = 0xa1;
	  font->max_char_or_byte2 = 0xfe;

	  /* Use the width of an "ideographic space" of that font
	     because the_fontinfo.widMax returns the wrong width for
	     some fonts.  */
	  switch (font->mac_scriptcode)
	    {
	    case smJapanese:
	      font->min_byte1 = 0x81;
	      font->max_byte1 = 0xfc;
	      font->min_char_or_byte2 = 0x40;
	      font->max_char_or_byte2 = 0xfc;
	      char_width = StringWidth("\p\x81\x40");
	      break;
	    case smTradChinese:
	      font->min_char_or_byte2 = 0x40;
	      char_width = StringWidth("\p\xa1\x40");
	      break;
	    case smSimpChinese:
	      char_width = StringWidth("\p\xa1\xa1");
	      break;
	    case smKorean:
	      char_width = StringWidth("\p\xa1\xa1");
	      break;
	    }

	  font->bounds.per_char = NULL;

	  if (fontface & italic)
	    font->max_bounds.rbearing = char_width + 1;
	  else
	    font->max_bounds.rbearing = char_width;
	  font->max_bounds.lbearing = 0;
	  font->max_bounds.width = char_width;
	  font->max_bounds.ascent = the_fontinfo.ascent;
	  font->max_bounds.descent = the_fontinfo.descent;

	  font->min_bounds = font->max_bounds;
	}
      else
	{
	  int c;

	  font->min_byte1 = font->max_byte1 = 0;
	  font->min_char_or_byte2 = 0x20;
	  font->max_char_or_byte2 = 0xff;

	  font->bounds.per_char =
	    xmalloc (sizeof (XCharStruct) * (0xff - 0x20 + 1));
	  bzero (font->bounds.per_char,
		 sizeof (XCharStruct) * (0xff - 0x20 + 1));

	  space_bounds = font->bounds.per_char;
	  err = mac_query_char_extents (NULL, 0x20, &font->ascent,
					&font->descent, space_bounds, NULL);
	  if (err != noErr || space_bounds->width <= 0)
	    {
	      mac_unload_font (&one_mac_display_info, font);
	      return NULL;
	    }

	  for (c = 0x21, pcm = space_bounds + 1; c <= 0xff; c++, pcm++)
	    mac_query_char_extents (NULL, c, NULL, NULL, pcm, NULL);
	}

      mac_end_clip (f, NULL);
#endif	/* USE_QUICKDRAW */
    }

  if (space_bounds)
    {
      int c;

      font->min_bounds = font->max_bounds = *space_bounds;
      for (c = 0x21, pcm = space_bounds + 1; c <= 0x7f; c++, pcm++)
	if (pcm->width > 0)
	  {
	    font->min_bounds.lbearing = min (font->min_bounds.lbearing,
					     pcm->lbearing);
	    font->min_bounds.rbearing = min (font->min_bounds.rbearing,
					     pcm->rbearing);
	    font->min_bounds.width    = min (font->min_bounds.width,
					     pcm->width);
	    font->min_bounds.ascent   = min (font->min_bounds.ascent,
					     pcm->ascent);
	    font->min_bounds.descent  = min (font->min_bounds.descent,
					     pcm->descent);

	    font->max_bounds.lbearing = max (font->max_bounds.lbearing,
					     pcm->lbearing);
	    font->max_bounds.rbearing = max (font->max_bounds.rbearing,
					     pcm->rbearing);
	    font->max_bounds.width    = max (font->max_bounds.width,
					     pcm->width);
	    font->max_bounds.ascent   = max (font->max_bounds.ascent,
					     pcm->ascent);
	    font->max_bounds.descent  = max (font->max_bounds.descent,
					     pcm->descent);
	  }
      if (
#if USE_ATSUI
	  font->mac_style == NULL &&
#endif
	  font->max_bounds.width == font->min_bounds.width
	  && font->min_bounds.lbearing >= 0
	  && font->max_bounds.rbearing <= font->max_bounds.width)
	{
	  /* Fixed width and no overhangs.  */
	  xfree (font->bounds.per_char);
	  font->bounds.per_char = NULL;
	}
    }

#if !defined (MAC_OS8) || USE_ATSUI
  /* AppKit and WebKit do some adjustment to the heights of Courier,
     Helvetica, and Times.  This only works on the environments where
     srcCopy text transfer mode is never used.  */
  if (
#ifdef MAC_OS8			/* implies USE_ATSUI */
      font->mac_style &&
#endif
      (strcmp (family, "courier") == 0 || strcmp (family, "helvetica") == 0
       || strcmp (family, "times") == 0))
    font->ascent += (font->ascent + font->descent) * .15 + 0.5;
#endif

  return font;
}


void
mac_unload_font (dpyinfo, font)
     struct mac_display_info *dpyinfo;
     XFontStruct *font;
{
  xfree (font->full_name);
#if USE_ATSUI
  if (font->mac_style)
    {
      int i;

      for (i = font->min_byte1; i <= font->max_byte1; i++)
	if (font->bounds.rows[i])
	  xfree (font->bounds.rows[i]);
      xfree (font->bounds.rows);
      ATSUDisposeStyle (font->mac_style);
    }
  else
#endif
    if (font->bounds.per_char)
      xfree (font->bounds.per_char);
#if USE_CG_TEXT_DRAWING
  if (font->cg_font)
    CGFontRelease (font->cg_font);
  if (font->cg_glyphs)
    xfree (font->cg_glyphs);
#endif
  xfree (font);
}


/* Load font named FONTNAME of the size SIZE for frame F, and return a
   pointer to the structure font_info while allocating it dynamically.
   If SIZE is 0, load any size of font.
   If loading is failed, return NULL.  */

struct font_info *
x_load_font (f, fontname, size)
     struct frame *f;
     register char *fontname;
     int size;
{
  struct mac_display_info *dpyinfo = FRAME_MAC_DISPLAY_INFO (f);
  Lisp_Object font_names;

  /* Get a list of all the fonts that match this name.  Once we
     have a list of matching fonts, we compare them against the fonts
     we already have by comparing names.  */
  font_names = x_list_fonts (f, build_string (fontname), size, 1);

  if (!NILP (font_names))
    {
      Lisp_Object tail;
      int i;

      for (i = 0; i < dpyinfo->n_fonts; i++)
	for (tail = font_names; CONSP (tail); tail = XCDR (tail))
	  if (dpyinfo->font_table[i].name
	      && (!strcmp (dpyinfo->font_table[i].name,
			   SDATA (XCAR (tail)))
		  || !strcmp (dpyinfo->font_table[i].full_name,
			      SDATA (XCAR (tail)))))
	    return (dpyinfo->font_table + i);
    }
  else
    return NULL;

  /* Load the font and add it to the table.  */
  {
    struct MacFontStruct *font;
    struct font_info *fontp;
    int i;

    fontname = (char *) SDATA (XCAR (font_names));

    BLOCK_INPUT;
    font = mac_load_query_font (f, fontname);
    UNBLOCK_INPUT;
    if (!font)
      return NULL;

    /* Find a free slot in the font table.  */
    for (i = 0; i < dpyinfo->n_fonts; ++i)
      if (dpyinfo->font_table[i].name == NULL)
	break;

    /* If no free slot found, maybe enlarge the font table.  */
    if (i == dpyinfo->n_fonts
	&& dpyinfo->n_fonts == dpyinfo->font_table_size)
      {
	int sz;
	dpyinfo->font_table_size = max (16, 2 * dpyinfo->font_table_size);
	sz = dpyinfo->font_table_size * sizeof *dpyinfo->font_table;
	dpyinfo->font_table
	  = (struct font_info *) xrealloc (dpyinfo->font_table, sz);
      }

    fontp = dpyinfo->font_table + i;
    if (i == dpyinfo->n_fonts)
      ++dpyinfo->n_fonts;

    /* Now fill in the slots of *FONTP.  */
    BLOCK_INPUT;
    bzero (fontp, sizeof (*fontp));
    fontp->font = font;
    fontp->font_idx = i;
    fontp->name = (char *) xmalloc (strlen (fontname) + 1);
    bcopy (fontname, fontp->name, strlen (fontname) + 1);

    if (font->min_bounds.width == font->max_bounds.width)
      {
	/* Fixed width font.  */
	fontp->average_width = fontp->space_width = font->min_bounds.width;
      }
    else
      {
	XChar2b char2b;
	XCharStruct *pcm;

	char2b.byte1 = 0x00, char2b.byte2 = 0x20;
	pcm = mac_per_char_metric (font, &char2b, 0);
	if (pcm)
	  fontp->space_width = pcm->width;
	else
	  fontp->space_width = FONT_WIDTH (font);

	if (pcm)
	  {
	    int width = pcm->width;
	    for (char2b.byte2 = 33; char2b.byte2 <= 126; char2b.byte2++)
	      if ((pcm = mac_per_char_metric (font, &char2b, 0)) != NULL)
		width += pcm->width;
	    fontp->average_width = width / 95.0 + 0.5;
	  }
	else
	  fontp->average_width = FONT_WIDTH (font);
      }

    fontp->full_name = (char *) xmalloc (strlen (font->full_name) + 1);
    bcopy (font->full_name, fontp->full_name, strlen (font->full_name) + 1);

    fontp->size = font->max_bounds.width;
    fontp->height = FONT_HEIGHT (font);
    {
      /* For some font, ascent and descent in max_bounds field is
	 larger than the above value.  */
      int max_height = font->max_bounds.ascent + font->max_bounds.descent;
      if (max_height > fontp->height)
	fontp->height = max_height;
    }

    /* The slot `encoding' specifies how to map a character
       code-points (0x20..0x7F or 0x2020..0x7F7F) of each charset to
       the font code-points (0:0x20..0x7F, 1:0xA0..0xFF), or
       (0:0x2020..0x7F7F, 1:0xA0A0..0xFFFF, 3:0x20A0..0x7FFF,
       2:0xA020..0xFF7F).  For the moment, we don't know which charset
       uses this font.  So, we set information in fontp->encoding[1]
       which is never used by any charset.  If mapping can't be
       decided, set FONT_ENCODING_NOT_DECIDED.  */
    if (font->mac_scriptcode == smJapanese)
      fontp->encoding[1] = 4;
    else
      {
        fontp->encoding[1]
           = (font->max_byte1 == 0
	      /* 1-byte font */
	      ? (font->min_char_or_byte2 < 0x80
	         ? (font->max_char_or_byte2 < 0x80
	            ? 0		/* 0x20..0x7F */
	            : FONT_ENCODING_NOT_DECIDED) /* 0x20..0xFF */
	         : 1)		/* 0xA0..0xFF */
	      /* 2-byte font */
	      : (font->min_byte1 < 0x80
	         ? (font->max_byte1 < 0x80
	            ? (font->min_char_or_byte2 < 0x80
		       ? (font->max_char_or_byte2 < 0x80
		          ? 0		/* 0x2020..0x7F7F */
		          : FONT_ENCODING_NOT_DECIDED) /* 0x2020..0x7FFF */
		       : 3)		/* 0x20A0..0x7FFF */
	            : FONT_ENCODING_NOT_DECIDED) /* 0x20??..0xA0?? */
	         : (font->min_char_or_byte2 < 0x80
	            ? (font->max_char_or_byte2 < 0x80
		       ? 2		/* 0xA020..0xFF7F */
		       : FONT_ENCODING_NOT_DECIDED) /* 0xA020..0xFFFF */
	            : 1)));		/* 0xA0A0..0xFFFF */
      }

#if 0 /* MAC_TODO: fill these out with more reasonably values */
    fontp->baseline_offset
      = (XGetFontProperty (font, dpyinfo->Xatom_MULE_BASELINE_OFFSET, &value)
	 ? (long) value : 0);
    fontp->relative_compose
      = (XGetFontProperty (font, dpyinfo->Xatom_MULE_RELATIVE_COMPOSE, &value)
	 ? (long) value : 0);
    fontp->default_ascent
      = (XGetFontProperty (font, dpyinfo->Xatom_MULE_DEFAULT_ASCENT, &value)
	 ? (long) value : 0);
#else
    fontp->baseline_offset = 0;
    fontp->relative_compose = 0;
    fontp->default_ascent = 0;
#endif

    /* Set global flag fonts_changed_p to non-zero if the font loaded
       has a character with a smaller width than any other character
       before, or if the font loaded has a smaller height than any
       other font loaded before.  If this happens, it will make a
       glyph matrix reallocation necessary.  */
    fonts_changed_p |= x_compute_min_glyph_bounds (f);
    UNBLOCK_INPUT;
    return fontp;
  }
}


/* Return a pointer to struct font_info of a font named FONTNAME for
   frame F.  If no such font is loaded, return NULL.  */

struct font_info *
x_query_font (f, fontname)
     struct frame *f;
     register char *fontname;
{
  struct mac_display_info *dpyinfo = FRAME_MAC_DISPLAY_INFO (f);
  int i;

  for (i = 0; i < dpyinfo->n_fonts; i++)
    if (dpyinfo->font_table[i].name
	&& (!xstricmp (dpyinfo->font_table[i].name, fontname)
	    || !xstricmp (dpyinfo->font_table[i].full_name, fontname)))
      return (dpyinfo->font_table + i);
  return NULL;
}


/* Find a CCL program for a font specified by FONTP, and set the member
 `encoder' of the structure.  */

void
x_find_ccl_program (fontp)
     struct font_info *fontp;
{
  Lisp_Object list, elt;

  for (list = Vfont_ccl_encoder_alist; CONSP (list); list = XCDR (list))
    {
      elt = XCAR (list);
      if (CONSP (elt)
	  && STRINGP (XCAR (elt))
	  && (fast_c_string_match_ignore_case (XCAR (elt), fontp->name)
	      >= 0))
	break;
    }
  if (! NILP (list))
    {
      struct ccl_program *ccl
	= (struct ccl_program *) xmalloc (sizeof (struct ccl_program));

      if (setup_ccl_program (ccl, XCDR (elt)) < 0)
	xfree (ccl);
      else
	fontp->font_encoder = ccl;
    }
}


/* The Mac Event loop code */

#if !TARGET_API_MAC_CARBON
#include <Events.h>
#include <Quickdraw.h>
#include <Balloons.h>
#include <Devices.h>
#include <Fonts.h>
#include <Gestalt.h>
#include <Menus.h>
#include <Processes.h>
#include <Sound.h>
#include <ToolUtils.h>
#include <TextUtils.h>
#include <Dialogs.h>
#include <Script.h>
#include <Types.h>
#include <Resources.h>

#if __MWERKS__
#include <unix.h>
#endif
#endif /* ! TARGET_API_MAC_CARBON */

/* Contains the string "reverse", which is a constant for mouse button emu.*/
Lisp_Object Qreverse;


/* Modifier associated with the control key, or nil to ignore. */
Lisp_Object Vmac_control_modifier;

/* Modifier associated with the option key, or nil to ignore. */
Lisp_Object Vmac_option_modifier;

/* Modifier associated with the command key, or nil to ignore. */
Lisp_Object Vmac_command_modifier;

/* Modifier associated with the function key, or nil to ignore. */
Lisp_Object Vmac_function_modifier;

/* True if the option and command modifiers should be used to emulate
   a three button mouse */
Lisp_Object Vmac_emulate_three_button_mouse;

#if TARGET_API_MAC_CARBON
/* Non-zero if the mouse wheel button (i.e. button 4) should map to
   mouse-2, instead of mouse-3.  */
int mac_wheel_button_is_mouse_2;

/* If non-zero, the Mac "Command" key is passed on to the Mac Toolbox
   for processing before Emacs sees it.  */
int mac_pass_command_to_system;

/* If non-zero, the Mac "Control" key is passed on to the Mac Toolbox
   for processing before Emacs sees it.  */
int mac_pass_control_to_system;
#endif

/* Whether or not the screen configuration has changed.  */
int mac_screen_config_changed = 0;

/* Apple Events */
#if TARGET_API_MAC_CARBON
#if !USE_APPKIT
Lisp_Object Qhi_command;
#ifdef MAC_OSX
Lisp_Object Qtoolbar_switch_mode;
#endif
#endif
#if USE_MAC_TSM || USE_APPKIT
Lisp_Object Qtext_input;
Lisp_Object Vmac_ts_active_input_overlay, Vmac_ts_active_input_buf;
static Lisp_Object Vmac_ts_script_language_on_focus;
static Lisp_Object saved_ts_script_language_on_focus;
static ScriptLanguageRecord saved_ts_language;
static Component saved_ts_component;
#endif
#if USE_MAC_TSM
Lisp_Object Qupdate_active_input_area, Qunicode_for_key_event;
#endif
#if USE_APPKIT
Lisp_Object Qinsert_text, Qset_marked_text;
Lisp_Object Qaction, Qmac_action_key_paths;
#endif
#ifdef MAC_OSX
Lisp_Object Qservice, Qpaste, Qperform;
Lisp_Object Qmouse_drag_overlay;
#endif
#endif	/* TARGET_API_MAC_CARBON */
extern Lisp_Object Qundefined;
extern int XTread_socket P_ ((int, int, struct input_event *));
#if !USE_APPKIT
extern void init_apple_event_handler P_ ((void));
#endif
extern void mac_find_apple_event_spec P_ ((AEEventClass, AEEventID,
					   Lisp_Object *, Lisp_Object *,
					   Lisp_Object *));
extern OSErr init_coercion_handler P_ ((void));

/* Table for translating Mac keycode to X keysym values.  Contributed
   by Sudhir Shenoy.
   Mapping for special keys is now identical to that in Apple X11
   except `clear' (-> <clear>) on the KeyPad, `enter' (-> <kp-enter>)
   on the right of the Cmd key on laptops, and fn + `enter' (->
   <linefeed>). */
const unsigned char keycode_to_xkeysym_table[] = {
  /*0x00*/ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  /*0x10*/ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  /*0x20*/ 0, 0, 0, 0, 0x0d /*return*/, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

  /*0x30*/ 0x09 /*tab*/, 0 /*0x0020 space*/, 0, 0x08 /*backspace*/,
  /*0x34*/ 0x8d /*enter on laptops*/, 0x1b /*escape*/, 0, 0,
  /*0x38*/ 0, 0, 0, 0,
  /*0x3C*/ 0, 0, 0, 0,

  /*0x40*/ 0xce /*f17*/, 0xae /*kp-decimal*/, 0, 0xaa /*kp-multiply*/,
  /*0x44*/ 0, 0xab /*kp-add*/, 0, 0x0b /*clear*/,
  /*0x48*/ 0, 0, 0, 0xaf /*kp-divide*/,
  /*0x4C*/ 0x8d /*kp-enter*/, 0, 0xad /*kp-subtract*/, 0xcf /*f18*/,

  /*0x50*/ 0xd0 /*f19*/, 0xbd /*kp-equal*/, 0xb0 /*kp-0*/, 0xb1 /*kp-1*/,
  /*0x54*/ 0xb2 /*kp-2*/, 0xb3 /*kp-3*/, 0xb4 /*kp-4*/, 0xb5 /*kp-5*/,
  /*0x58*/ 0xb6 /*kp-6*/, 0xb7 /*kp-7*/, 0xd1 /*f20*/, 0xb8 /*kp-8*/,
  /*0x5C*/ 0xb9 /*kp-9*/, 0, 0, 0xac /*kp-separator*/,

  /*0x60*/ 0xc2 /*f5*/, 0xc3 /*f6*/, 0xc4 /*f7*/, 0xc0 /*f3*/,
  /*0x64*/ 0xc5 /*f8*/, 0xc6 /*f9*/, 0, 0xc8 /*f11*/,
  /*0x68*/ 0, 0xca /*f13*/, 0xcd /*f16*/, 0xcb /*f14*/,
  /*0x6C*/ 0, 0xc7 /*f10*/, 0x0a /*fn+enter on laptops*/, 0xc9 /*f12*/,

  /*0x70*/ 0, 0xcc /*f15*/, 0x6a /*help*/, 0x50 /*home*/,
  /*0x74*/ 0x55 /*pgup*/, 0xff /*delete*/, 0xc1 /*f4*/, 0x57 /*end*/,
  /*0x78*/ 0xbf /*f2*/, 0x56 /*pgdown*/, 0xbe /*f1*/, 0x51 /*left*/,
  /*0x7C*/ 0x53 /*right*/, 0x54 /*down*/, 0x52 /*up*/, 0
};

#ifdef MAC_OSX
/* Table for translating Mac keycode with the laptop `fn' key to that
   without it.  Destination symbols in comments are keys on US
   keyboard, and they may not be the same on other types of keyboards.
   If the destination is identical to the source, it doesn't map `fn'
   key to a modifier.  */
static const unsigned char fn_keycode_to_keycode_table[] = {
  /*0x00*/ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  /*0x10*/ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  /*0x20*/ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

  /*0x30*/ 0, 0, 0, 0,
  /*0x34*/ 0, 0, 0, 0,
  /*0x38*/ 0, 0, 0, 0,
  /*0x3C*/ 0, 0, 0, 0,

  /*0x40*/ 0x40 /*f17 = f17*/, 0x2f /*kp-decimal -> '.'*/, 0, 0x23 /*kp-multiply -> 'p'*/,
  /*0x44*/ 0, 0x2c /*kp-add -> '/'*/, 0, 0x16 /*clear -> '6'*/,
  /*0x48*/ 0, 0, 0, 0x1d /*kp-/ -> '0'*/,
  /*0x4C*/ 0x24 /*kp-enter -> return*/, 0, 0x29 /*kp-subtract -> ';'*/, 0x4f /*f18 = f18*/,

  /*0x50*/ 0x50 /*f19 = f19*/, 0x1b /*kp-equal -> '-'*/, 0x2e /*kp-0 -> 'm'*/, 0x26 /*kp-1 -> 'j'*/,
  /*0x54*/ 0x28 /*kp-2 -> 'k'*/, 0x25 /*kp-3 -> 'l'*/, 0x20 /*kp-4 -> 'u'*/, 0x22 /*kp-5 ->'i'*/,
  /*0x58*/ 0x1f /*kp-6 -> 'o'*/, 0x1a /*kp-7 -> '7'*/, 0x5a /*f20 = f20*/, 0x1c /*kp-8 -> '8'*/,
  /*0x5C*/ 0x19 /*kp-9 -> '9'*/, 0, 0, 0,

  /*0x60*/ 0x60 /*f5 = f5*/, 0x61 /*f6 = f6*/, 0x62 /*f7 = f7*/, 0x63 /*f3 = f3*/,
  /*0x64*/ 0x64 /*f8 = f8*/, 0x65 /*f9 = f9*/, 0, 0x67 /*f11 = f11*/,
  /*0x68*/ 0, 0x69 /*f13 = f13*/, 0x6a /*f16 = f16*/, 0x6b /*f14 = f14*/,
  /*0x6C*/ 0, 0x6d /*f10 = f10*/, 0, 0x6f /*f12 = f12*/,

  /*0x70*/ 0, 0x71 /*f15 = f15*/, 0x72 /*help = help*/, 0x7b /*home -> left*/,
  /*0x74*/ 0x7e /*pgup -> up*/, 0x33 /*delete -> backspace*/, 0x76 /*f4 = f4*/, 0x7c /*end -> right*/,
  /*0x78*/ 0x78 /*f2 = f2*/, 0x7d /*pgdown -> down*/, 0x7a /*f1 = f1*/, 0x7b /*left = left*/,
  /*0x7C*/ 0x7c /*right = right*/, 0x7d /*down = down*/, 0x7e /*up = up*/, 0
};
#endif	/* MAC_OSX */

int
#if TARGET_API_MAC_CARBON
mac_to_emacs_modifiers (UInt32 mods, UInt32 unmapped_mods)
#else
mac_to_emacs_modifiers (EventModifiers mods, EventModifiers unmapped_mods)
#endif
{
  unsigned int result = 0;
  if ((mods | unmapped_mods) & shiftKey)
    result |= shift_modifier;

  /* Deactivated to simplify configuration:
     if Vmac_option_modifier is non-NIL, we fully process the Option
     key. Otherwise, we only process it if an additional Ctrl or Command
     is pressed. That way the system may convert the character to a
     composed one.
     if ((mods & optionKey) &&
      (( !NILP(Vmac_option_modifier) ||
      ((mods & cmdKey) || (mods & controlKey))))) */

  if (!NILP (Vmac_option_modifier) && (mods & optionKey)) {
    Lisp_Object val = Fget(Vmac_option_modifier, Qmodifier_value);
    if (INTEGERP(val))
      result |= XUINT(val);
  }
  if (!NILP (Vmac_command_modifier) && (mods & cmdKey)) {
    Lisp_Object val = Fget(Vmac_command_modifier, Qmodifier_value);
    if (INTEGERP(val))
      result |= XUINT(val);
  }
  if (!NILP (Vmac_control_modifier) && (mods & controlKey)) {
    Lisp_Object val = Fget(Vmac_control_modifier, Qmodifier_value);
    if (INTEGERP(val))
      result |= XUINT(val);
  }

#ifdef MAC_OSX
  if (!NILP (Vmac_function_modifier) && (mods & kEventKeyModifierFnMask)) {
    Lisp_Object val = Fget(Vmac_function_modifier, Qmodifier_value);
    if (INTEGERP(val))
      result |= XUINT(val);
  }
#endif

  return result;
}

UInt32
mac_mapped_modifiers (modifiers, key_code)
     UInt32 modifiers, key_code;
{
  UInt32 mapped_modifiers_all =
    (NILP (Vmac_control_modifier) ? 0 : controlKey)
    | (NILP (Vmac_option_modifier) ? 0 : optionKey)
    | (NILP (Vmac_command_modifier) ? 0 : cmdKey);

#ifdef MAC_OSX
  mapped_modifiers_all |=
    (NILP (Vmac_function_modifier) ? 0 : kEventKeyModifierFnMask);

  /* The meaning of kEventKeyModifierFnMask has changed in Mac OS X
     10.5, and it now behaves much like Cocoa's NSFunctionKeyMask.  It
     no longer means laptop's `fn' key is down for the following keys:
     F1, F2, and so on, Help, Forward Delete, Home, End, Page Up, Page
     Down, the arrow keys, and Clear.  We ignore the corresponding bit
     if that key can be entered without the `fn' key on laptops.  */
  if (modifiers & kEventKeyModifierFnMask
      && key_code <= 0x7f
      && fn_keycode_to_keycode_table[key_code] == key_code
      && key_code != 0)		/* kVK_ANSI_A */
    modifiers &= ~kEventKeyModifierFnMask;
#endif

  return mapped_modifiers_all & modifiers;
}

int
mac_get_emulated_btn ( UInt32 modifiers )
{
  int result = 0;
  if (!NILP (Vmac_emulate_three_button_mouse)) {
    int cmdIs3 = !EQ (Vmac_emulate_three_button_mouse, Qreverse);
    if (modifiers & cmdKey)
      result = cmdIs3 ? 2 : 1;
    else if (modifiers & optionKey)
      result = cmdIs3 ? 1 : 2;
  }
  return result;
}

#ifdef MAC_OSX
void
mac_get_selected_range (w, range)
     struct window *w;
     CFRange *range;
{
  Lisp_Object overlay = find_symbol_value (Qmouse_drag_overlay);
  struct buffer *b = XBUFFER (w->buffer);
  int begv = BUF_BEGV (b), zv = BUF_ZV (b);
  int start, end;

  if (OVERLAYP (overlay)
      && EQ (Foverlay_buffer (overlay), w->buffer)
      && (start = XINT (Foverlay_start (overlay)),
	  end = XINT (Foverlay_end (overlay)),
	  start != end))
    ;
  else
    {
      if (w == XWINDOW (selected_window) && b == current_buffer)
	start = PT;
      else
	start = marker_position (w->pointm);

      if (NILP (Vtransient_mark_mode) || NILP (b->mark_active))
	end = start;
      else
	{
	  int mark_pos = marker_position (b->mark);

	  if (start <= mark_pos)
	    end = mark_pos;
	  else
	    {
	      end = start;
	      start = mark_pos;
	    }
	}
    }

  if (start != end)
    {
      if (start < begv)
	start = begv;
      else if (start > zv)
	start = zv;

      if (end < begv)
	end = begv;
      else if (end > zv)
	end = zv;
    }

  range->location = start - begv;
  range->length = end - start;
}

/* Store the text of the buffer BUF from START to END as Unicode
   characters in CHARACTERS.  Return non-zero if successful.  */

int
mac_store_buffer_text_to_unicode_chars (buf, start, end, characters)
     struct buffer *buf;
     int start, end;
     UniChar *characters;
{
  int start_byte, end_byte, char_count, byte_count;
  struct coding_system coding;
  unsigned char *dst = (unsigned char *) characters;

  start_byte = buf_charpos_to_bytepos (buf, start);
  end_byte = buf_charpos_to_bytepos (buf, end);
  char_count = end - start;
  byte_count = end_byte - start_byte;

  if (setup_coding_system (
#ifdef WORDS_BIG_ENDIAN
			   intern ("utf-16be")
#else
			   intern ("utf-16le")
#endif
			   , &coding) < 0)
    return 0;

  coding.src_multibyte = !NILP (buf->enable_multibyte_characters);
  coding.dst_multibyte = 0;
  coding.mode |= CODING_MODE_LAST_BLOCK;
  coding.composing = COMPOSITION_DISABLED;

  if (BUF_GPT_BYTE (buf) <= start_byte || end_byte <= BUF_GPT_BYTE (buf))
    encode_coding (&coding, BUF_BYTE_ADDRESS (buf, start_byte), dst,
		   byte_count, char_count * sizeof (UniChar));
  else
    {
      int first_byte_count = BUF_GPT_BYTE (buf) - start_byte;

      encode_coding (&coding, BUF_BYTE_ADDRESS (buf, start_byte), dst,
		     first_byte_count, char_count * sizeof (UniChar));
      if (coding.result == CODING_FINISH_NORMAL)
	encode_coding (&coding,
		       BUF_BYTE_ADDRESS (buf, start_byte + first_byte_count),
		       dst + coding.produced,
		       byte_count - first_byte_count,
		       char_count * sizeof (UniChar) - coding.produced);
    }

  if (coding.result != CODING_FINISH_NORMAL)
    return 0;

  return 1;
}

void
mac_ax_selected_text_range (f, range)
     struct frame *f;
     CFRange *range;
{
  mac_get_selected_range (XWINDOW (f->selected_window), range);
}

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1030
unsigned int
mac_ax_number_of_characters (f)
     struct frame *f;
{
  struct buffer *b = XBUFFER (XWINDOW (f->selected_window)->buffer);

  return BUF_ZV (b) - BUF_BEGV (b);
}
#endif
#endif

#if USE_MAC_TSM || USE_APPKIT
OSStatus
mac_restore_keyboard_input_source ()
{
  OSStatus err = noErr;
#if !__LP64__
  ScriptLanguageRecord slrec, *slptr = NULL;

  if (EQ (Vmac_ts_script_language_on_focus, Qt)
      && EQ (saved_ts_script_language_on_focus, Qt))
    slptr = &saved_ts_language;
  else if (CONSP (Vmac_ts_script_language_on_focus)
	   && INTEGERP (XCAR (Vmac_ts_script_language_on_focus))
	   && INTEGERP (XCDR (Vmac_ts_script_language_on_focus))
	   && CONSP (saved_ts_script_language_on_focus)
	   && EQ (XCAR (saved_ts_script_language_on_focus),
		  XCAR (Vmac_ts_script_language_on_focus))
	   && EQ (XCDR (saved_ts_script_language_on_focus),
		  XCDR (Vmac_ts_script_language_on_focus)))
    {
      slrec.fScript = XINT (XCAR (Vmac_ts_script_language_on_focus));
      slrec.fLanguage = XINT (XCDR (Vmac_ts_script_language_on_focus));
      slptr = &slrec;
    }

  if (slptr)
    {
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1020
      err = SetDefaultInputMethodOfClass (saved_ts_component, slptr,
					  kKeyboardInputMethodClass);
#else
      err = SetDefaultInputMethod (saved_ts_component, slptr);
#endif
      if (err == noErr)
	err = SetTextServiceLanguage (slptr);

      /* Seems to be needed on Mac OS X 10.2.  */
      if (err == noErr)
	KeyScript (slptr->fScript | smKeyForceKeyScriptMask);
    }
#endif	/* !__LP64__ */

  return err;
}

void
mac_save_keyboard_input_source ()
{
#if !__LP64__
  OSStatus err;
  ScriptLanguageRecord slrec, *slptr = NULL;

  saved_ts_script_language_on_focus = Vmac_ts_script_language_on_focus;

  if (EQ (Vmac_ts_script_language_on_focus, Qt))
    {
      err = GetTextServiceLanguage (&saved_ts_language);
      if (err == noErr)
	slptr = &saved_ts_language;
    }
  else if (CONSP (Vmac_ts_script_language_on_focus)
	   && INTEGERP (XCAR (Vmac_ts_script_language_on_focus))
	   && INTEGERP (XCDR (Vmac_ts_script_language_on_focus)))
    {
      slrec.fScript = XINT (XCAR (Vmac_ts_script_language_on_focus));
      slrec.fLanguage = XINT (XCDR (Vmac_ts_script_language_on_focus));
      slptr = &slrec;
    }

  if (slptr)
    {
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1020
      GetDefaultInputMethodOfClass (&saved_ts_component, slptr,
				    kKeyboardInputMethodClass);
#else
      GetDefaultInputMethod (&saved_ts_component, slptr);
#endif
    }
#endif	/* !__LP64__ */
}
#endif

#if TARGET_API_MAC_CARBON
/***** Code to handle C-g testing  *****/
extern int quit_char;
extern int make_ctrl_char P_ ((int));

int
mac_quit_char_key_p (modifiers, key_code)
     UInt32 modifiers, key_code;
{
  UInt32 char_code, mapped_modifiers;
  unsigned long some_state = 0;
  Ptr kchr_ptr = (Ptr) GetScriptManagerVariable (smKCHRCache);
  int c, emacs_modifiers;

  /* Mask off modifier keys that are mapped to some Emacs modifiers.  */
  mapped_modifiers = mac_mapped_modifiers (modifiers, key_code);
  key_code |= (modifiers & ~mapped_modifiers);
  char_code = KeyTranslate (kchr_ptr, key_code, &some_state);
  if (char_code & ~0xff)
    return 0;

  c = char_code;
  emacs_modifiers = mac_to_emacs_modifiers (mapped_modifiers, modifiers);
  if (emacs_modifiers & ctrl_modifier)
    c = make_ctrl_char (c);

  c |= (emacs_modifiers
	& (meta_modifier | alt_modifier
	   | hyper_modifier | super_modifier));

  return c == quit_char;
}
#endif

static void
mac_set_unicode_keystroke_event (code, buf)
     UniChar code;
     struct input_event *buf;
{
  int charset_id, c1, c2;

  if (code < 0x80)
    {
      buf->kind = ASCII_KEYSTROKE_EVENT;
      buf->code = code;
    }
  else if (code < 0x100)
    {
      if (code < 0xA0)
	charset_id = CHARSET_8_BIT_CONTROL;
      else
	charset_id = charset_latin_iso8859_1;
      buf->kind = MULTIBYTE_CHAR_KEYSTROKE_EVENT;
      buf->code = MAKE_CHAR (charset_id, code, 0);
    }
  else
    {
      if (code < 0x2500)
	charset_id = charset_mule_unicode_0100_24ff,
	  code -= 0x100;
      else if (code < 0x33FF)
	charset_id = charset_mule_unicode_2500_33ff,
	  code -= 0x2500;
      else if (code >= 0xE000)
	charset_id = charset_mule_unicode_e000_ffff,
	  code -= 0xE000;
      c1 = (code / 96) + 32, c2 = (code % 96) + 32;
      buf->kind = MULTIBYTE_CHAR_KEYSTROKE_EVENT;
      buf->code = MAKE_CHAR (charset_id, c1, c2);
    }
}

void
do_keystroke (action, char_code, key_code, modifiers, timestamp, buf)
     EventKind action;
     unsigned char char_code;
     UInt32 key_code, modifiers;
     unsigned long timestamp;
     struct input_event *buf;
{
  static SInt16 last_key_script = -1;
  SInt16 current_key_script = GetScriptManagerVariable (smKeyScript);
  UInt32 mapped_modifiers = mac_mapped_modifiers (modifiers, key_code);

#ifdef MAC_OSX
  if (mapped_modifiers & kEventKeyModifierFnMask
      && key_code <= 0x7f
      && fn_keycode_to_keycode_table[key_code])
    key_code = fn_keycode_to_keycode_table[key_code];
#endif

  if (key_code <= 0x7f && keycode_to_xkeysym_table[key_code])
    {
      buf->kind = NON_ASCII_KEYSTROKE_EVENT;
      buf->code = 0xff00 | keycode_to_xkeysym_table[key_code];
    }
  else if (mapped_modifiers)
    {
      /* translate the keycode back to determine the original key */
#ifdef MAC_OSX
      UCKeyboardLayout *uchr_ptr = NULL;
#if __LP64__
      TISInputSourceRef source = TISCopyCurrentKeyboardLayoutInputSource ();
      CFDataRef uchr_data = NULL;

      if (source)
	uchr_data =
	  TISGetInputSourceProperty (source, kTISPropertyUnicodeKeyLayoutData);
      if (uchr_data)
	uchr_ptr = (UCKeyboardLayout *) CFDataGetBytePtr (uchr_data);
#elif MAC_OS_X_VERSION_MAX_ALLOWED >= 1020
      OSStatus err;
      KeyboardLayoutRef layout;

      err = KLGetCurrentKeyboardLayout (&layout);
      if (err == noErr)
	err = KLGetKeyboardLayoutProperty (layout, kKLuchrData,
					   (const void **) &uchr_ptr);
#else
      static SInt16 last_key_layout_id = 0;
      static Handle uchr_handle = (Handle)-1;
      SInt16 current_key_layout_id =
	GetScriptVariable (current_key_script, smScriptKeys);

      if (uchr_handle == (Handle)-1
	  || last_key_layout_id != current_key_layout_id)
	{
	  uchr_handle = GetResource ('uchr', current_key_layout_id);
	  last_key_layout_id = current_key_layout_id;
	}
      if (uchr_handle)
	uchr_ptr = (UCKeyboardLayout *)*uchr_handle;
#endif

      if (uchr_ptr)
	{
	  OSStatus status;
	  UInt16 key_action = action - keyDown;
	  UInt32 modifier_key_state = (modifiers & ~mapped_modifiers) >> 8;
	  UInt32 keyboard_type = LMGetKbdType ();
	  SInt32 dead_key_state = 0;
	  UniChar code;
	  UniCharCount actual_length;

	  status = UCKeyTranslate (uchr_ptr, key_code, key_action,
				   modifier_key_state, keyboard_type,
				   kUCKeyTranslateNoDeadKeysMask,
				   &dead_key_state,
				   1, &actual_length, &code);
	  if (status == noErr && actual_length == 1)
	    mac_set_unicode_keystroke_event (code, buf);
	}
#if __LP64__
      if (source)
	CFRelease (source);
#endif
#endif	/* MAC_OSX */

      if (buf->kind == NO_EVENT)
	{
	  /* This code comes from Keyboard Resource, Appendix C of IM
	     - Text.  This is necessary since shift is ignored in KCHR
	     table translation when option or command is pressed.  It
	     also does not translate correctly control-shift chars
	     like C-% so mask off shift here also.  */
	  /* Mask off modifier keys that are mapped to some Emacs
	     modifiers.  */
	  int new_modifiers = modifiers & ~mapped_modifiers;
	  /* set high byte of keycode to modifier high byte*/
	  int new_key_code = key_code | new_modifiers;
	  Ptr kchr_ptr = (Ptr) GetScriptManagerVariable (smKCHRCache);
	  unsigned long some_state = 0;
	  UInt32 new_char_code;

	  new_char_code = KeyTranslate (kchr_ptr, new_key_code, &some_state);
	  if (new_char_code == 0)
	    /* Seems like a dead key.  Append up-stroke.  */
	    new_char_code = KeyTranslate (kchr_ptr, new_key_code | 0x80,
					  &some_state);
	  if (new_char_code)
	    {
	      buf->kind = ASCII_KEYSTROKE_EVENT;
	      buf->code = new_char_code & 0xff;
	    }
	}
    }

  if (buf->kind == NO_EVENT)
    {
      buf->kind = ASCII_KEYSTROKE_EVENT;
      buf->code = char_code;
    }

  buf->modifiers = mac_to_emacs_modifiers (mapped_modifiers, modifiers);
  buf->modifiers |= (extra_keyboard_modifiers
		     & (meta_modifier | alt_modifier
			| hyper_modifier | super_modifier));

#if TARGET_API_MAC_CARBON
  if (buf->kind == ASCII_KEYSTROKE_EVENT
      && buf->code >= 0x80 && buf->modifiers)
    {
      OSStatus err;
      TextEncoding encoding = kTextEncodingMacRoman;
      TextToUnicodeInfo ttu_info;

      UpgradeScriptInfoToTextEncoding (current_key_script,
				       kTextLanguageDontCare,
				       kTextRegionDontCare,
				       NULL, &encoding);
      err = CreateTextToUnicodeInfoByEncoding (encoding, &ttu_info);
      if (err == noErr)
	{
	  UniChar code;
	  Str255 pstr;
	  ByteCount unicode_len;

	  pstr[0] = 1;
	  pstr[1] = buf->code;
	  err = ConvertFromPStringToUnicode (ttu_info, pstr,
					     sizeof (UniChar),
					     &unicode_len, &code);
	  if (err == noErr && unicode_len == sizeof (UniChar))
	    mac_set_unicode_keystroke_event (code, buf);
	  DisposeTextToUnicodeInfo (&ttu_info);
	}
    }
#endif

  if (buf->kind == ASCII_KEYSTROKE_EVENT
      && buf->code >= 0x80
      && last_key_script != current_key_script)
    {
      struct input_event event;

      EVENT_INIT (event);
      event.kind = LANGUAGE_CHANGE_EVENT;
      event.arg = Qnil;
      event.code = current_key_script;
      event.timestamp = timestamp;
      kbd_buffer_store_event (&event);
      last_key_script = current_key_script;
    }
}

void
mac_store_apple_event (class, id, desc)
     Lisp_Object class, id;
     const AEDesc *desc;
{
  struct input_event buf;

  EVENT_INIT (buf);

  buf.kind = MAC_APPLE_EVENT;
  buf.x = class;
  buf.y = id;
  XSETFRAME (buf.frame_or_window,
	     mac_focus_frame (&one_mac_display_info));
  /* Now that Lisp object allocations are protected by BLOCK_INPUT, it
     is safe to use them during read_socket_hook.  */
  buf.arg = mac_aedesc_to_lisp (desc);
  kbd_buffer_store_event (&buf);
}

#if TARGET_API_MAC_CARBON
OSStatus
mac_store_event_ref_as_apple_event (class, id, class_key, id_key,
				    event, num_params, names, types)
     AEEventClass class;
     AEEventID id;
     Lisp_Object class_key, id_key;
     EventRef event;
     UInt32 num_params;
     const EventParamName *names;
     const EventParamType *types;
{
  OSStatus err = eventNotHandledErr;
  Lisp_Object binding;

  mac_find_apple_event_spec (class, id, &class_key, &id_key, &binding);
  if (!NILP (binding) && !EQ (binding, Qundefined))
    {
      if (INTEGERP (binding))
	err = XINT (binding);
      else
	{
	  struct input_event buf;

	  EVENT_INIT (buf);

	  buf.kind = MAC_APPLE_EVENT;
	  buf.x = class_key;
	  buf.y = id_key;
	  XSETFRAME (buf.frame_or_window,
		     mac_focus_frame (&one_mac_display_info));
	  /* Now that Lisp object allocations are protected by
	     BLOCK_INPUT, it is safe to use them during
	     read_socket_hook.  */
	  buf.arg = Fcons (build_string ("aevt"),
			   mac_event_parameters_to_lisp (event, num_params,
							 names, types));
	  kbd_buffer_store_event (&buf);
	  mac_wakeup_from_rne ();
	  err = noErr;
	}
    }

  return err;
}
#endif	/* TARGET_API_MAC_CARBON */

static pascal void
mac_handle_dm_notification (event)
     AppleEvent *event;
{
  mac_screen_config_changed = 1;
}

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1030
static void
mac_handle_cg_display_reconfig (display, flags, user_info)
     CGDirectDisplayID display;
     CGDisplayChangeSummaryFlags flags;
     void *user_info;
{
  mac_screen_config_changed = 1;
}
#endif

static OSErr
init_dm_notification_handler ()
{
  OSErr err = noErr;

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1030
#if MAC_OS_X_VERSION_MIN_REQUIRED == 1020
  if (CGDisplayRegisterReconfigurationCallback != NULL)
#endif
    {
      CGDisplayRegisterReconfigurationCallback (mac_handle_cg_display_reconfig,
						NULL);
    }
#if MAC_OS_X_VERSION_MIN_REQUIRED == 1020
  else		/* CGDisplayRegisterReconfigurationCallback == NULL */
#endif
#endif	/* MAC_OS_X_VERSION_MAX_ALLOWED >= 1030 */
#if MAC_OS_X_VERSION_MAX_ALLOWED < 1030 || MAC_OS_X_VERSION_MIN_REQUIRED == 1020
    {
      static DMNotificationUPP handle_dm_notificationUPP = NULL;
      ProcessSerialNumber psn;

      if (handle_dm_notificationUPP == NULL)
	handle_dm_notificationUPP =
	  NewDMNotificationUPP (mac_handle_dm_notification);

      err = GetCurrentProcess (&psn);
      if (err == noErr)
	err = DMRegisterNotifyProc (handle_dm_notificationUPP, &psn);
    }
#endif

  return err;
}

void
mac_get_screen_info (dpyinfo)
     struct mac_display_info *dpyinfo;
{
#ifdef MAC_OSX
  /* HasDepth returns true if it is possible to have a 32 bit display,
     but this may not be what is actually used.  Mac OSX can do better.  */
  dpyinfo->color_p = CGDisplaySamplesPerPixel (kCGDirectMainDisplay) > 1;
  dpyinfo->n_planes = CGDisplayBitsPerPixel (kCGDirectMainDisplay);
  {
    CGDisplayErr err;
    CGDisplayCount ndisps;
    CGDirectDisplayID *displays;

    err = CGGetActiveDisplayList (0, NULL, &ndisps);
    if (err == noErr)
      {
	displays = alloca (sizeof (CGDirectDisplayID) * ndisps);
	err = CGGetActiveDisplayList (ndisps, displays, &ndisps);
      }
    if (err == noErr)
      {
	CGRect bounds = CGRectZero;

	while (ndisps-- > 0)
	  bounds = CGRectUnion (bounds, CGDisplayBounds (displays[ndisps]));
	dpyinfo->height = CGRectGetHeight (bounds);
	dpyinfo->width = CGRectGetWidth (bounds);
      }
    else
      {
	dpyinfo->height = CGDisplayPixelsHigh (kCGDirectMainDisplay);
	dpyinfo->width = CGDisplayPixelsWide (kCGDirectMainDisplay);
      }
  }
#else  /* !MAC_OSX */
  {
    GDHandle gdh = GetMainDevice ();
    Rect rect = (**gdh).gdRect;

    dpyinfo->color_p = TestDeviceAttribute (gdh, gdDevType);
    for (dpyinfo->n_planes = 32; dpyinfo->n_planes > 0; dpyinfo->n_planes >>= 1)
      if (HasDepth (gdh, dpyinfo->n_planes, gdDevType, dpyinfo->color_p))
	break;

    for (gdh = DMGetFirstScreenDevice (dmOnlyActiveDisplays); gdh;
	 gdh = DMGetNextScreenDevice (gdh, dmOnlyActiveDisplays))
      UnionRect (&rect, &(**gdh).gdRect, &rect);

    dpyinfo->height = rect.bottom - rect.top;
    dpyinfo->width = rect.right - rect.left;
  }
#endif  /* !MAC_OSX */
}


/***********************************************************************
		   Initialization (Mac OS Classic)
 ***********************************************************************/

#ifdef MAC_OS8
extern void init_emacs_passwd_dir ();
extern int emacs_main (int, char **, char **);

extern void initialize_applescript();
extern void terminate_applescript();

static void
do_get_menus (void)
{
  Handle menubar_handle;
  MenuRef menu;

  menubar_handle = GetNewMBar (128);
  if(menubar_handle == NULL)
    abort ();
  SetMenuBar (menubar_handle);
  DrawMenuBar ();

#if !TARGET_API_MAC_CARBON
  menu = GetMenuRef (M_APPLE);
  if (menu != NULL)
    AppendResMenu (menu, 'DRVR');
  else
    abort ();
#endif
}

static void
do_init_managers (void)
{
#if !TARGET_API_MAC_CARBON
  InitGraf (&qd.thePort);
  InitFonts ();
  FlushEvents (everyEvent, 0);
  InitWindows ();
  InitMenus ();
  TEInit ();
  InitDialogs (NULL);
#endif /* !TARGET_API_MAC_CARBON */
  InitCursor ();

#if !TARGET_API_MAC_CARBON
  /* set up some extra stack space for use by emacs */
  SetApplLimit ((Ptr) ((long) GetApplLimit () - EXTRA_STACK_ALLOC));

  /* MaxApplZone must be called for AppleScript to execute more
     complicated scripts */
  MaxApplZone ();
  MoreMasters ();
#endif /* !TARGET_API_MAC_CARBON */
}

static void
do_check_ram_size (void)
{
  SInt32 physical_ram_size, logical_ram_size;

  if (Gestalt (gestaltPhysicalRAMSize, &physical_ram_size) != noErr
      || Gestalt (gestaltLogicalRAMSize, &logical_ram_size) != noErr
      || physical_ram_size > (1 << VALBITS)
      || logical_ram_size > (1 << VALBITS))
    {
      StopAlert (RAM_TOO_LARGE_ALERT_ID, NULL);
      exit (1);
    }
}

#if __profile__
void
profiler_exit_proc ()
{
  ProfilerDump ("\pEmacs.prof");
  ProfilerTerm ();
}
#endif

/* These few functions implement Emacs as a normal Mac application
   (almost): set up the heap and the Toolbox, handle necessary system
   events plus a few simple menu events.  They also set up Emacs's
   access to functions defined in the rest of this file.  Emacs uses
   function hooks to perform all its terminal I/O.  A complete list of
   these functions appear in termhooks.h.  For what they do, read the
   comments there and see also w32term.c and xterm.c.  What's
   noticeably missing here is the event loop, which is normally
   present in most Mac application.  After performing the necessary
   Mac initializations, main passes off control to emacs_main
   (corresponding to main in emacs.c).  Emacs_main calls XTread_socket
   (defined further below) to read input.  This is where
   WaitNextEvent/ReceiveNextEvent is called to process Mac events.  */

#undef main
int
main (void)
{
#if __profile__  /* is the profiler on? */
  if (ProfilerInit(collectDetailed, bestTimeBase, 5000, 200))
    exit(1);
#endif

#if __MWERKS__
  /* set creator and type for files created by MSL */
  _fcreator = MAC_EMACS_CREATOR_CODE;
  _ftype = 'TEXT';
#endif

  do_init_managers ();

  do_get_menus ();

#ifndef USE_LSB_TAG
  do_check_ram_size ();
#endif

  init_emacs_passwd_dir ();

  init_environ ();

  init_coercion_handler ();

  initialize_applescript ();

  init_apple_event_handler ();

  init_dm_notification_handler ();

  {
    char **argv;
    int argc = 0;

    /* set up argv array from STR# resource */
    get_string_list (&argv, ARGV_STRING_LIST_ID);
    while (argv[argc])
      argc++;

    /* free up AppleScript resources on exit */
    atexit (terminate_applescript);

#if __profile__  /* is the profiler on? */
    atexit (profiler_exit_proc);
#endif

    /* 3rd param "envp" never used in emacs_main */
    (void) emacs_main (argc, argv, 0);
  }

  /* Never reached - real exit in Fkill_emacs */
  return 0;
}

/* Need to override CodeWarrior's input function so no conversion is
   done on newlines Otherwise compiled functions in .elc files will be
   read incorrectly.  Defined in ...:MSL C:MSL
   Common:Source:buffer_io.c.  */
#ifdef __MWERKS__
void
__convert_to_newlines (unsigned char * p, size_t * n)
{
#pragma unused(p,n)
}

void
__convert_from_newlines (unsigned char * p, size_t * n)
{
#pragma unused(p,n)
}
#endif

void
make_mac_terminal_frame (struct frame *f)
{
  Lisp_Object frame;
  Rect r;

  XSETFRAME (frame, f);

  f->output_method = output_mac;
  f->output_data.mac = (struct mac_output *)
    xmalloc (sizeof (struct mac_output));
  bzero (f->output_data.mac, sizeof (struct mac_output));

  XSETFRAME (FRAME_KBOARD (f)->Vdefault_minibuffer_frame, f);

  FRAME_COLS (f) = 96;
  FRAME_LINES (f) = 4;

  FRAME_CAN_HAVE_SCROLL_BARS (f) = 1;
  FRAME_VERTICAL_SCROLL_BAR_TYPE (f) = vertical_scroll_bar_right;

  FRAME_DESIRED_CURSOR (f) = FILLED_BOX_CURSOR;

  f->output_data.mac->cursor_pixel = 0;
  f->output_data.mac->border_pixel = 0x00ff00;
  f->output_data.mac->mouse_pixel = 0xff00ff;
  f->output_data.mac->cursor_foreground_pixel = 0x0000ff;

  f->output_data.mac->text_cursor = kThemeIBeamCursor;
  f->output_data.mac->nontext_cursor = kThemeArrowCursor;
  f->output_data.mac->modeline_cursor = kThemeArrowCursor;
  f->output_data.mac->hand_cursor = kThemePointingHandCursor;
  f->output_data.mac->hourglass_cursor = kThemeWatchCursor;
  f->output_data.mac->horizontal_drag_cursor = kThemeResizeLeftRightCursor;

  FRAME_FONTSET (f) = -1;
  f->output_data.mac->explicit_parent = 0;
  f->left_pos = 8;
  f->top_pos = 32;
  f->border_width = 0;

  f->internal_border_width = 0;

  f->auto_raise = 1;
  f->auto_lower = 1;

  f->new_text_cols = 0;
  f->new_text_lines = 0;

  SetRect (&r, f->left_pos, f->top_pos,
           f->left_pos + FRAME_PIXEL_WIDTH (f),
           f->top_pos + FRAME_PIXEL_HEIGHT (f));

  BLOCK_INPUT;

  if (!(FRAME_MAC_WINDOW (f) =
	NewCWindow (NULL, &r, "\p", true, dBoxProc,
		    (WindowRef) -1, 1, (long) f->output_data.mac)))
    abort ();
  /* so that update events can find this mac_output struct */
  f->output_data.mac->mFP = f;  /* point back to emacs frame */

  UNBLOCK_INPUT;

  x_make_gc (f);

  /* Need to be initialized for unshow_buffer in window.c.  */
  selected_window = f->selected_window;

  Fmodify_frame_parameters (frame,
                            Fcons (Fcons (Qfont,
                                          build_string ("-*-monaco-medium-r-*--*-90-*-*-*-*-mac-roman")), Qnil));
  Fmodify_frame_parameters (frame,
                            Fcons (Fcons (Qforeground_color,
                                          build_string ("black")), Qnil));
  Fmodify_frame_parameters (frame,
                            Fcons (Fcons (Qbackground_color,
                                          build_string ("white")), Qnil));
}
#endif	/* MAC_OS8 */


/***********************************************************************
			    Initialization
 ***********************************************************************/

static int mac_initialized = 0;

static XrmDatabase
mac_make_rdb (xrm_option)
     const char *xrm_option;
{
  XrmDatabase database;

  database = xrm_get_preference_database (NULL);
  if (xrm_option)
    xrm_merge_string_database (database, xrm_option);

  return database;
}

struct mac_display_info *
mac_term_init (display_name, xrm_option, resource_name)
     Lisp_Object display_name;
     char *xrm_option;
     char *resource_name;
{
  struct mac_display_info *dpyinfo;

  BLOCK_INPUT;

  if (!mac_initialized)
    {
      mac_initialize ();
      mac_initialized = 1;
    }

  if (x_display_list)
    error ("Sorry, this version can only handle one display");

  dpyinfo = &one_mac_display_info;
  bzero (dpyinfo, sizeof (*dpyinfo));

#ifdef MAC_OSX
  dpyinfo->mac_id_name
    = (char *) xmalloc (SBYTES (Vinvocation_name)
			+ SBYTES (Vsystem_name)
			+ 2);
  sprintf (dpyinfo->mac_id_name, "%s@%s",
	   SDATA (Vinvocation_name), SDATA (Vsystem_name));
#else
  dpyinfo->mac_id_name = (char *) xmalloc (strlen ("Mac Display") + 1);
  strcpy (dpyinfo->mac_id_name, "Mac Display");
#endif

  dpyinfo->reference_count = 0;
  dpyinfo->resx = 72.0;
  dpyinfo->resy = 72.0;

  mac_get_screen_info (dpyinfo);

  dpyinfo->grabbed = 0;
  dpyinfo->root_window = NULL;
  dpyinfo->image_cache = make_image_cache ();

  dpyinfo->mouse_face_beg_row = dpyinfo->mouse_face_beg_col = -1;
  dpyinfo->mouse_face_end_row = dpyinfo->mouse_face_end_col = -1;
  dpyinfo->mouse_face_face_id = DEFAULT_FACE_ID;
  dpyinfo->mouse_face_window = Qnil;
  dpyinfo->mouse_face_overlay = Qnil;
  dpyinfo->mouse_face_hidden = 0;

  dpyinfo->xrdb = mac_make_rdb (xrm_option);

  /* Put this display on the chain.  */
  dpyinfo->next = x_display_list;
  x_display_list = dpyinfo;

  /* Put it on x_display_name_list.  */
  x_display_name_list = Fcons (Fcons (display_name,
				      Fcons (Qnil, dpyinfo->xrdb)),
                               x_display_name_list);
  dpyinfo->name_list_element = XCAR (x_display_name_list);

  UNBLOCK_INPUT;

  return dpyinfo;
}

/* Get rid of display DPYINFO, assuming all frames are already gone.  */

void
x_delete_display (dpyinfo)
     struct mac_display_info *dpyinfo;
{
  int i;

  /* Discard this display from x_display_name_list and x_display_list.
     We can't use Fdelq because that can quit.  */
  if (! NILP (x_display_name_list)
      && EQ (XCAR (x_display_name_list), dpyinfo->name_list_element))
    x_display_name_list = XCDR (x_display_name_list);
  else
    {
      Lisp_Object tail;

      tail = x_display_name_list;
      while (CONSP (tail) && CONSP (XCDR (tail)))
	{
	  if (EQ (XCAR (XCDR (tail)), dpyinfo->name_list_element))
	    {
	      XSETCDR (tail, XCDR (XCDR (tail)));
	      break;
	    }
	  tail = XCDR (tail);
	}
    }

  if (x_display_list == dpyinfo)
    x_display_list = dpyinfo->next;
  else
    {
      struct x_display_info *tail;

      for (tail = x_display_list; tail; tail = tail->next)
	if (tail->next == dpyinfo)
	  tail->next = tail->next->next;
    }

  /* Free the font names in the font table.  */
  for (i = 0; i < dpyinfo->n_fonts; i++)
    if (dpyinfo->font_table[i].name)
      {
	if (dpyinfo->font_table[i].name != dpyinfo->font_table[i].full_name)
	  xfree (dpyinfo->font_table[i].full_name);
	xfree (dpyinfo->font_table[i].name);
      }

  if (dpyinfo->font_table)
    {
      if (dpyinfo->font_table->font_encoder)
	xfree (dpyinfo->font_table->font_encoder);
      xfree (dpyinfo->font_table);
    }
  if (dpyinfo->mac_id_name)
    xfree (dpyinfo->mac_id_name);

  if (x_display_list == 0)
    {
      mac_clear_font_name_table ();
      bzero (dpyinfo, sizeof (*dpyinfo));
    }
}


#ifdef MAC_OSX
void
mac_handle_user_signal (sig)
     int sig;
{
  extern void mac_wakeup_from_run_loop_run_once P_ ((void));

  mac_wakeup_from_run_loop_run_once ();
}
#endif	/* MAC_OSX */

/* Set up use of X before we make the first connection.  */

extern frame_parm_handler mac_frame_parm_handlers[];

static struct redisplay_interface x_redisplay_interface =
{
  mac_frame_parm_handlers,
  x_produce_glyphs,
  x_write_glyphs,
  x_insert_glyphs,
  x_clear_end_of_line,
  x_scroll_run,
  x_after_update_window_line,
  x_update_window_begin,
  x_update_window_end,
  x_cursor_to,
  x_flush,
#if USE_CG_DRAWING && !USE_APPKIT
  mac_flush_display_optional,
#else
  0, /* flush_display_optional */
#endif
  x_clear_window_mouse_face,
  x_get_glyph_overhangs,
  x_fix_overlapping_area,
  x_draw_fringe_bitmap,
#if USE_CG_DRAWING
  mac_define_fringe_bitmap,
  mac_destroy_fringe_bitmap,
#else
  0, /* define_fringe_bitmap */
  0, /* destroy_fringe_bitmap */
#endif
  mac_per_char_metric,
  mac_encode_char,
  mac_compute_glyph_string_overhangs,
  x_draw_glyph_string,
  mac_define_frame_cursor,
  mac_clear_frame_area,
  mac_draw_window_cursor,
  mac_draw_vertical_window_border,
  mac_shift_glyphs_for_insert
};

void
mac_initialize ()
{
  rif = &x_redisplay_interface;

  clear_frame_hook = x_clear_frame;
  ins_del_lines_hook = x_ins_del_lines;
  delete_glyphs_hook = x_delete_glyphs;
  ring_bell_hook = XTring_bell;
  reset_terminal_modes_hook = XTreset_terminal_modes;
  set_terminal_modes_hook = XTset_terminal_modes;
  update_begin_hook = x_update_begin;
  update_end_hook = x_update_end;
  set_terminal_window_hook = XTset_terminal_window;
  read_socket_hook = XTread_socket;
  frame_up_to_date_hook = XTframe_up_to_date;
  mouse_position_hook = XTmouse_position;
  frame_rehighlight_hook = XTframe_rehighlight;
  frame_raise_lower_hook = XTframe_raise_lower;

  set_vertical_scroll_bar_hook = XTset_vertical_scroll_bar;
  condemn_scroll_bars_hook = XTcondemn_scroll_bars;
  redeem_scroll_bar_hook = XTredeem_scroll_bar;
  judge_scroll_bars_hook = XTjudge_scroll_bars;

  scroll_region_ok = 1;         /* we'll scroll partial frames */
  char_ins_del_ok = 1;
  line_ins_del_ok = 1;          /* we'll just blt 'em */
  fast_clear_end_of_line = 1;   /* X does this well */
  memory_below_frame = 0;       /* we don't remember what scrolls
				   off the bottom */
  baud_rate = 19200;

  last_tool_bar_item = -1;

  /* Try to use interrupt input; if we can't, then start polling.  */
  Fset_input_mode (Qt, Qnil, Qt, Qnil);

  BLOCK_INPUT;

#if TARGET_API_MAC_CARBON
#ifdef MAC_OSX
  if (init_wakeup_fds () < 0)
    abort ();

  handle_user_signal_hook = mac_handle_user_signal;

  init_coercion_handler ();

  init_dm_notification_handler ();
#endif

  install_application_handler ();

#if !USE_APPKIT
  mac_toolbox_initialize ();
#endif
#endif

#if USE_CG_DRAWING
  init_cg_color ();

  mac_init_fringe ();
#endif

  UNBLOCK_INPUT;
}


void
syms_of_macterm ()
{
#if 0
  staticpro (&x_error_message_string);
  x_error_message_string = Qnil;
#endif

  Qcontrol = intern ("control");	staticpro (&Qcontrol);
  Qmeta    = intern ("meta");		staticpro (&Qmeta);
  Qalt     = intern ("alt");		staticpro (&Qalt);
  Qhyper   = intern ("hyper");		staticpro (&Qhyper);
  Qsuper   = intern ("super");		staticpro (&Qsuper);
  Qmodifier_value = intern ("modifier-value");
  staticpro (&Qmodifier_value);

  Fput (Qcontrol, Qmodifier_value, make_number (ctrl_modifier));
  Fput (Qmeta,    Qmodifier_value, make_number (meta_modifier));
  Fput (Qalt,     Qmodifier_value, make_number (alt_modifier));
  Fput (Qhyper,   Qmodifier_value, make_number (hyper_modifier));
  Fput (Qsuper,   Qmodifier_value, make_number (super_modifier));

#if TARGET_API_MAC_CARBON
#if !USE_APPKIT
  Qhi_command   = intern ("hi-command");    staticpro (&Qhi_command);
#ifdef MAC_OSX
  Qtoolbar_switch_mode = intern ("toolbar-switch-mode");
  staticpro (&Qtoolbar_switch_mode);
#endif
#endif
#if USE_MAC_FONT_PANEL
  Qpanel_closed = intern ("panel-closed");  staticpro (&Qpanel_closed);
  Qselection    = intern ("selection");     staticpro (&Qselection);
#endif

#ifdef MAC_OSX
  Qservice     = intern ("service");	  staticpro (&Qservice);
  Qpaste       = intern ("paste");	  staticpro (&Qpaste);
  Qperform     = intern ("perform");	  staticpro (&Qperform);

  Qmouse_drag_overlay = intern ("mouse-drag-overlay");
  staticpro (&Qmouse_drag_overlay);
#endif
#if USE_MAC_TSM || USE_APPKIT
  Qtext_input = intern ("text-input");	staticpro (&Qtext_input);
#endif
#if USE_MAC_TSM
  Qupdate_active_input_area = intern ("update-active-input-area");
  staticpro (&Qupdate_active_input_area);
  Qunicode_for_key_event = intern ("unicode-for-key-event");
  staticpro (&Qunicode_for_key_event);
#endif
#if USE_APPKIT
  Qinsert_text = intern ("insert-text");	staticpro (&Qinsert_text);
  Qset_marked_text = intern ("set-marked-text"); staticpro (&Qset_marked_text);

  Qaction = intern ("action");		staticpro (&Qaction);
  Qmac_action_key_paths = intern ("mac-action-key-paths");
  staticpro (&Qmac_action_key_paths);
#endif
#endif

#ifdef MAC_OSX
  Fprovide (intern ("mac-carbon"), Qnil);
#endif

  staticpro (&Qreverse);
  Qreverse = intern ("reverse");

  staticpro (&x_display_name_list);
  x_display_name_list = Qnil;

  staticpro (&last_mouse_scroll_bar);
  last_mouse_scroll_bar = Qnil;

  staticpro (&fm_font_family_alist);
  fm_font_family_alist = Qnil;

#if USE_ATSUI
  staticpro (&atsu_font_id_hash);
  atsu_font_id_hash = Qnil;

  staticpro (&fm_style_face_attributes_alist);
  fm_style_face_attributes_alist = Qnil;
#endif

#if USE_MAC_TSM || USE_APPKIT
  staticpro (&saved_ts_script_language_on_focus);
  saved_ts_script_language_on_focus = Qnil;
#endif

  /* We don't yet support this, but defining this here avoids whining
     from cus-start.el and other places, like "M-x set-variable".  */
  DEFVAR_BOOL ("x-use-underline-position-properties",
	       &x_use_underline_position_properties,
     doc: /* *Non-nil means make use of UNDERLINE_POSITION font properties.
A value of nil means ignore them.  If you encounter fonts with bogus
UNDERLINE_POSITION font properties, for example 7x13 on XFree prior
to 4.1, set this to nil.

NOTE: Not supported on Mac yet.  */);
  x_use_underline_position_properties = 0;

  DEFVAR_BOOL ("x-underline-at-descent-line",
	       &x_underline_at_descent_line,
     doc: /* *Non-nil means to draw the underline at the same place as the descent line.
A value of nil means to draw the underline according to the value of the
variable `x-use-underline-position-properties', which is usually at the
baseline level.  The default value is nil.  */);
  x_underline_at_descent_line = 0;

  DEFVAR_LISP ("x-toolkit-scroll-bars", &Vx_toolkit_scroll_bars,
    doc: /* If not nil, Emacs uses toolkit scroll bars.  */);
#ifdef USE_TOOLKIT_SCROLL_BARS
  Vx_toolkit_scroll_bars = Qt;
#else
  Vx_toolkit_scroll_bars = Qnil;
#endif

  staticpro (&last_mouse_motion_frame);
  last_mouse_motion_frame = Qnil;

  DEFVAR_BOOL ("mac-redisplay-dont-reset-vscroll", &mac_redisplay_dont_reset_vscroll,
	       doc: /* Non-nil means update doesn't reset vscroll.  */);
  mac_redisplay_dont_reset_vscroll = 0;

/* Variables to configure modifier key assignment.  */

  DEFVAR_LISP ("mac-control-modifier", &Vmac_control_modifier,
    doc: /* *Modifier key assumed when the Mac control key is pressed.
The value can be `control', `meta', `alt', `hyper', or `super' for the
respective modifier.  The default is `control'.  */);
  Vmac_control_modifier = Qcontrol;

  DEFVAR_LISP ("mac-option-modifier", &Vmac_option_modifier,
    doc: /* *Modifier key assumed when the Mac alt/option key is pressed.
The value can be `control', `meta', `alt', `hyper', or `super' for the
respective modifier.  If the value is nil then the key will act as the
normal Mac control modifier, and the option key can be used to compose
characters depending on the chosen Mac keyboard setting.  */);
  Vmac_option_modifier = Qnil;

  DEFVAR_LISP ("mac-command-modifier", &Vmac_command_modifier,
    doc: /* *Modifier key assumed when the Mac command key is pressed.
The value can be `control', `meta', `alt', `hyper', or `super' for the
respective modifier.  The default is `meta'.  */);
  Vmac_command_modifier = Qmeta;

  DEFVAR_LISP ("mac-function-modifier", &Vmac_function_modifier,
    doc: /* *Modifier key assumed when the Mac function key is pressed.
The value can be `control', `meta', `alt', `hyper', or `super' for the
respective modifier.  Note that remapping the function key may lead to
unexpected results for some keys on non-US/GB keyboards.  */);
  Vmac_function_modifier = Qnil;

  DEFVAR_LISP ("mac-emulate-three-button-mouse",
	       &Vmac_emulate_three_button_mouse,
    doc: /* *Specify a way of three button mouse emulation.
The value can be nil, t, or the symbol `reverse'.
A value of nil means that no emulation should be done and the modifiers
should be placed on the mouse-1 event.
t means that when the option-key is held down while pressing the mouse
button, the click will register as mouse-2 and while the command-key
is held down, the click will register as mouse-3.
The symbol `reverse' means that the option-key will register for
mouse-3 and the command-key will register for mouse-2.  */);
  Vmac_emulate_three_button_mouse = Qnil;

#if TARGET_API_MAC_CARBON
  DEFVAR_BOOL ("mac-wheel-button-is-mouse-2", &mac_wheel_button_is_mouse_2,
    doc: /* *Non-nil if the wheel button is mouse-2 and the right click mouse-3.
Otherwise, the right click will be treated as mouse-2 and the wheel
button will be mouse-3.  */);
  mac_wheel_button_is_mouse_2 = 1;

  DEFVAR_BOOL ("mac-pass-command-to-system", &mac_pass_command_to_system,
    doc: /* *Non-nil if command key presses are passed on to the Mac Toolbox.  */);
  mac_pass_command_to_system = 1;

  DEFVAR_BOOL ("mac-pass-control-to-system", &mac_pass_control_to_system,
    doc: /* *Non-nil if control key presses are passed on to the Mac Toolbox.  */);
  mac_pass_control_to_system = 1;

#endif

  DEFVAR_BOOL ("mac-allow-anti-aliasing", &mac_use_core_graphics,
    doc: /* *If non-nil, allow anti-aliasing.
The text will be rendered using Core Graphics text rendering which
may anti-alias the text.  */);
#if USE_CG_DRAWING
  mac_use_core_graphics = 1;
#else
  mac_use_core_graphics = 0;
#endif

  /* Register an entry for `mac-roman' so that it can be used when
     creating the terminal frame on Mac OS 9 before loading
     term/mac-win.elc.  */
  DEFVAR_LISP ("mac-charset-info-alist", &Vmac_charset_info_alist,
    doc: /* Alist of Emacs character sets vs text encodings and coding systems.
Each entry should be of the form:

   (CHARSET-NAME TEXT-ENCODING CODING-SYSTEM)

where CHARSET-NAME is a string used in font names to identify the
charset, TEXT-ENCODING is a TextEncodingBase value in Mac, and
CODING_SYSTEM is a coding system corresponding to TEXT-ENCODING.  */);
  Vmac_charset_info_alist =
    Fcons (list3 (build_string ("mac-roman"),
		  make_number (smRoman), Qnil), Qnil);

#if USE_MAC_TSM || USE_APPKIT
  DEFVAR_LISP ("mac-ts-active-input-overlay", &Vmac_ts_active_input_overlay,
    doc: /* Overlay used to display Mac TSM active input area.  */);
  Vmac_ts_active_input_overlay = Qnil;

  DEFVAR_LISP ("mac-ts-active-input-buf", &Vmac_ts_active_input_buf,
    doc: /* Byte sequence of the current Mac TSM active input area.  */);
  /* `empty_string' is not ready yet on Mac OS Classic.  */
  Vmac_ts_active_input_buf = build_string ("");

  DEFVAR_LISP ("mac-ts-script-language-on-focus", &Vmac_ts_script_language_on_focus,
    doc: /* *How to change Mac TSM script/language when a frame gets focus.
If the value is t, the input script and language are restored to those
used in the last focus frame.  If the value is a pair of integers, the
input script and language codes, which are defined in the Script
Manager, are set to its car and cdr parts, respectively.  Otherwise,
Emacs doesn't set them and thus follows the system default behavior.  */);
  Vmac_ts_script_language_on_focus = Qnil;
#endif
}

/* arch-tag: f2259165-4454-4c04-a029-a133c8af7b5b
   (do not change this comment) */
