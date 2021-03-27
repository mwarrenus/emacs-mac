;;; slovak.el --- Quail package for inputting Slovak  -*-coding: utf-8;-*-

;; Copyright (C) 1998, 2001-2021 Free Software Foundation, Inc.

;; Authors: Tibor Šimko <tibor.simko@fmph.uniba.sk>
;;	Milan Zamazal <pdm@zamazal.org>
;; Maintainer: Pavel Janík <Pavel@Janik.cz>
;; Keywords: i18n, multilingual, input method, Slovak

;; This file is part of GNU Emacs.

;; GNU Emacs is free software: you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation, either version 3 of the License, or
;; (at your option) any later version.

;; GNU Emacs is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with GNU Emacs.  If not, see <https://www.gnu.org/licenses/>.

;;; Commentary:

;; This file defines the following Slovak keyboards:
;; - standard Slovak keyboard
;; - three Slovak keyboards for programmers

;;; Code:

(require 'quail)


(quail-define-package
 "slovak" "Slovak" "SK" t
 "Standard Slovak keyboard."
 nil t nil nil t nil nil nil nil nil t)

(quail-define-rules
 ("1" ?+)
 ("2" ?ľ)
 ("3" ?š)
 ("4" ?č)
 ("5" ?ť)
 ("6" ?ž)
 ("7" ?ý)
 ("8" ?á)
 ("9" ?í)
 ("0" ?é)
 ("!" ?1)
 ("@" ?2)
 ("#" ?3)
 ("$" ?4)
 ("%" ?5)
 ("^" ?6)
 ("&" ?7)
 ("*" ?8)
 ("(" ?9)
 (")" ?0)
 ("-" ?=)
 ("_" ?%)
 ("=" ?')
 ("[" ?ú)
 ("{" ?/)
 ("]" ?ä)
 ("}" ?\()
 ("\\" ?ň)
 ("|" ?\))
 (";" ?ô)
 (":" ?\")
 ("'" ?§)
 ("\"" ?!)
 ("<" ??)
 (">" ?:)
 ("/" ?-)
 ("?" ?_)
 ("`" ?\;)
 ("~" ?^)
 ("y" ?z)
 ("z" ?y)
 ("Y" ?Z)
 ("Z" ?Y)
 ("=a" ?á)
 ("+a" ?ä)
 ("+=a" ?ä)
 ("+c" ?č)
 ("+d" ?ď)
 ("=e" ?é)
 ("+e" ?ě)
 ("=i" ?í)
 ("=l" ?ĺ)
 ("+l" ?ľ)
 ("+n" ?ň)
 ("=o" ?ó)
 ("+o" ?ô)
 ("~o" ?ô)
 ("+=o" ?ö)
 ("=r" ?ŕ)
 ("+r" ?ř)
 ("=s" ?ß)
 ("+s" ?š)
 ("+t" ?ť)
 ("=u" ?ú)
 ("+u" ?ů)
 ("+=u" ?ü)
 ("=z" ?ý)
 ("+y" ?ž)
 ("=A" ?Á)
 ("+A" ?Ä)
 ("+=A" ?Ä)
 ("+C" ?Č)
 ("+D" ?Ď)
 ("=E" ?É)
 ("+E" ?Ě)
 ("=I" ?Í)
 ("=L" ?Ĺ)
 ("+L" ?Ľ)
 ("+N" ?Ň)
 ("=O" ?Ó)
 ("+O" ?Ô)
 ("~O" ?Ô)
 ("+=O" ?Ö)
 ("=R" ?Ŕ)
 ("+R" ?Ř)
 ("=S" ?ß)
 ("+S" ?Š)
 ("+T" ?Ť)
 ("=U" ?Ú)
 ("+U" ?Ů)
 ("+=U" ?Ü)
 ("=Z" ?Ý)
 ("+Y" ?Ž)
 ("=q" ?`)
 ("=2" ?@)
 ("=3" ?#)
 ("=4" ?$)
 ("=5" ?%)
 ("=6" ?^)
 ("=7" ?&)
 ("=8" ?*)
 ("=9" ?\()
 ("=0" ?\))
 ("+1" ?!)
 ("+2" ?@)
 ("+3" ?#)
 ("+4" ?$)
 ("+5" ?%)
 ("+6" ?^)
 ("+7" ?&)
 ("+8" ?*)
 ("+9" ?\()
 ("+0" ?\)))


(quail-define-package
 "slovak-prog-1" "Slovak" "SK" t
 "Slovak (non-standard) keyboard for programmers #1.

All digits except of `1' are replaced by Slovak characters as on the standard
Slovak keyboard.
Dead keys are on `[', `[[', and `[[['.
All other keys are the same as on standard US keyboard."
 nil t nil nil t nil nil nil nil nil t)

(quail-define-rules
 ("[[[[" ?\[)
 ("2" ?ľ)
 ("3" ?š)
 ("4" ?č)
 ("5" ?ť)
 ("6" ?ž)
 ("7" ?ý)
 ("8" ?á)
 ("9" ?í)
 ("0" ?é)
 ("[a" ?á)
 ("[[a" ?ä)
 ("[[[a" ?ä)
 ("[c" ?č)
 ("[[c" ?č)
 ("[d" ?ď)
 ("[[d" ?ď)
 ("[e" ?é)
 ("[[e" ?ě)
 ("[i" ?í)
 ("[l" ?ĺ)
 ("[[l" ?ľ)
 ("[n" ?ň)
 ("[[n" ?ň)
 ("[o" ?ó)
 ("[[o" ?ô)
 ("[[[o" ?ö)
 ("[r" ?ŕ)
 ("[[r" ?ř)
 ("[s" ?š)
 ("[[s" ?š)
 ("[[[s" ?ß)
 ("[t" ?ť)
 ("[[t" ?ť)
 ("[u" ?ú)
 ("[[u" ?ů)
 ("[[[u" ?ü)
 ("[y" ?ý)
 ("[z" ?ž)
 ("[[z" ?ž)
 ("[A" ?Á)
 ("[[A" ?Ä)
 ("[[[A" ?Ä)
 ("[C" ?Č)
 ("[[C" ?Č)
 ("[D" ?Ď)
 ("[[D" ?Ď)
 ("[E" ?É)
 ("[[E" ?Ě)
 ("[I" ?Í)
 ("[L" ?Ĺ)
 ("[[L" ?Ľ)
 ("[N" ?Ň)
 ("[[N" ?Ň)
 ("[O" ?Ó)
 ("[[O" ?Ô)
 ("[[[O" ?Ö)
 ("[R" ?Ŕ)
 ("[[R" ?Ř)
 ("[S" ?Š)
 ("[[S" ?Š)
 ("[[[S" ?ß)
 ("[T" ?Ť)
 ("[[T" ?Ť)
 ("[U" ?Ú)
 ("[[U" ?Ů)
 ("[[[U" ?Ü)
 ("[Y" ?Ý)
 ("[Z" ?Ž)
 ("[[Z" ?Ž))


(quail-define-package
 "slovak-prog-2" "Slovak" "SK" t
 "Slovak (non-standard) keyboard for programmers #2.

All digits except of `1' are replaced by Slovak characters as on the standard
Slovak keyboard.
Dead keys are on `=' and `+'.
All other keys are the same as on standard US keyboard."
 nil t nil nil t nil nil nil nil nil t)

(quail-define-rules
 ("2" ?ľ)
 ("3" ?š)
 ("4" ?č)
 ("5" ?ť)
 ("6" ?ž)
 ("7" ?ý)
 ("8" ?á)
 ("9" ?í)
 ("0" ?é)
 ("==" ?=)
 ("++" ?+)
 ("=+" ?+)
 ("=2" ?2)
 ("=3" ?3)
 ("=4" ?4)
 ("=5" ?5)
 ("=6" ?6)
 ("=7" ?7)
 ("=8" ?8)
 ("=9" ?9)
 ("=0" ?0)
 ("+2" ?2)
 ("+3" ?3)
 ("+4" ?4)
 ("+5" ?5)
 ("+6" ?6)
 ("+7" ?7)
 ("+8" ?8)
 ("+9" ?9)
 ("+0" ?0)
 ("=a" ?á)
 ("+a" ?ä)
 ("+=a" ?ä)
 ("+c" ?č)
 ("+d" ?ď)
 ("=e" ?é)
 ("+e" ?ě)
 ("=i" ?í)
 ("=l" ?ĺ)
 ("+l" ?ľ)
 ("+n" ?ň)
 ("=o" ?ó)
 ("+o" ?ô)
 ("+=o" ?ö)
 ("=r" ?ŕ)
 ("+r" ?ř)
 ("=s" ?ß)
 ("+s" ?š)
 ("+t" ?ť)
 ("=u" ?ú)
 ("+u" ?ů)
 ("+=u" ?ü)
 ("=y" ?ý)
 ("+z" ?ž)
 ("=A" ?Á)
 ("+A" ?Ä)
 ("+=A" ?Ä)
 ("+C" ?Č)
 ("+D" ?Ď)
 ("=E" ?É)
 ("+E" ?Ě)
 ("=I" ?Í)
 ("=L" ?Ĺ)
 ("+L" ?Ľ)
 ("+N" ?Ň)
 ("=O" ?Ó)
 ("+O" ?Ô)
 ("+=O" ?Ö)
 ("=R" ?Ŕ)
 ("+R" ?Ř)
 ("=S" ?ß)
 ("+S" ?Š)
 ("+T" ?Ť)
 ("=U" ?Ú)
 ("+U" ?Ů)
 ("+=U" ?Ü)
 ("=Y" ?Ý)
 ("+Z" ?Ž))


(quail-define-package
 "slovak-prog-3" "Slovak" "SK" t
 "Slovak (non-standard) keyboard for programmers #3.

Dead keys are on `[', `[[', `[[[', and `]'.
All other keys are the same as on standard US keyboard."
 nil t nil nil t nil nil nil nil nil t)

(quail-define-rules
 ("[[[[" ?\[)
 ("[]" ?\])
 ("][" ?\[)
 ("]]" ?\])
 ("[a" ?á)
 ("[[a" ?ä)
 ("[[[a" ?ä)
 ("]a" ?ä)
 ("][a" ?ä)
 ("[c" ?č)
 ("[[c" ?č)
 ("]c" ?č)
 ("[d" ?ď)
 ("[[d" ?ď)
 ("]d" ?ď)
 ("[e" ?é)
 ("[[e" ?ě)
 ("]e" ?ě)
 ("[i" ?í)
 ("[l" ?ĺ)
 ("[[l" ?ľ)
 ("]l" ?ľ)
 ("[n" ?ň)
 ("[[n" ?ň)
 ("]n" ?ň)
 ("[[o" ?ô)
 ("[[[o" ?ö)
 ("[o" ?ó)
 ("]o" ?ô)
 ("][o" ?ö)
 ("[r" ?ŕ)
 ("[[r" ?ř)
 ("]r" ?ř)
 ("[s" ?š)
 ("[[s" ?š)
 ("[[[s" ?ß)
 ("]s" ?š)
 ("[t" ?ť)
 ("[[t" ?ť)
 ("]t" ?ť)
 ("[u" ?ú)
 ("[[u" ?ů)
 ("[[[u" ?ü)
 ("]u" ?ů)
 ("][u" ?ü)
 ("[y" ?ý)
 ("[z" ?ž)
 ("[[z" ?ž)
 ("]z" ?ž)
 ("[A" ?Á)
 ("[[A" ?Ä)
 ("[[[A" ?Ä)
 ("]A" ?Ä)
 ("][A" ?Ä)
 ("[C" ?Č)
 ("[[C" ?Č)
 ("]C" ?Č)
 ("[D" ?Ď)
 ("[[D" ?Ď)
 ("]D" ?Ď)
 ("[E" ?É)
 ("[[E" ?Ě)
 ("]E" ?Ě)
 ("[I" ?Í)
 ("[L" ?Ĺ)
 ("[[L" ?Ľ)
 ("]L" ?Ľ)
 ("[N" ?Ň)
 ("[[N" ?Ň)
 ("]N" ?Ň)
 ("[O" ?Ó)
 ("[[O" ?Ô)
 ("[[[O" ?Ö)
 ("]O" ?Ô)
 ("][O" ?Ö)
 ("[R" ?Ŕ)
 ("[[R" ?Ř)
 ("]R" ?Ř)
 ("[S" ?Š)
 ("[[S" ?Š)
 ("[[[S" ?ß)
 ("]S" ?Š)
 ("[T" ?Ť)
 ("[[T" ?Ť)
 ("]T" ?Ť)
 ("[U" ?Ú)
 ("[[U" ?Ů)
 ("[[[U" ?Ü)
 ("]U" ?Ů)
 ("][U" ?Ü)
 ("[Y" ?Ý)
 ("[Z" ?Ž)
 ("[[Z" ?Ž)
 ("]Z" ?Ž))

;;; slovak.el ends here
