;;; english.el --- support for English -*- no-byte-compile: t -*-

;; Copyright (C) 1997, 2001, 2002, 2003, 2004, 2005, 2006, 2007
;;   Free Software Foundation, Inc.
;; Copyright (C) 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007
;;   National Institute of Advanced Industrial Science and Technology (AIST)
;;   Registration Number H14PRO021

;; Keywords: multibyte character, character set, syntax, category

;; This file is part of GNU Emacs.

;; GNU Emacs is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 2, or (at your option)
;; any later version.

;; GNU Emacs is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with GNU Emacs; see the file COPYING.  If not, write to the
;; Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
;; Boston, MA 02110-1301, USA.

;;; Commentary:

;; We need nothing special to support English on Emacs.  Selecting
;; English as a language environment is one of the ways to reset
;; various multilingual environment to the original settting.

;;; Code:

(set-language-info-alist
 "English" '((tutorial . "TUTORIAL")
	     (charset ascii)
	     (sample-text . "Hello!, Hi!, How are you?")
	     (documentation . "\
Nothing special is needed to handle English.")
	     ))

;; Make "ASCII" an alias of "English" language environment.
(set-language-info-alist
 "ASCII" (cdr (assoc "English" language-info-alist)))

;;; arch-tag: e440bdb0-91b0-4fb4-ae38-425780f8f745
;;; english.el ends here
