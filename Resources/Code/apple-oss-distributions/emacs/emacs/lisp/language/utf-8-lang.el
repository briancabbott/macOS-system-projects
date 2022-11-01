;;; utf-8-lang.el --- generic UTF-8 language environment -*- no-byte-compile: t -*-

;; Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006, 2007
;;   Free Software Foundation, Inc.

;; Author: Dave Love <fx@gnu.org>
;; Keywords: i18n

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

;;; Code:

(set-language-info-alist
 "UTF-8" `((coding-system mule-utf-8)
	   (coding-priority mule-utf-8)
	   (setup-function
	    . (lambda ()
		;; Use Unicode font under Windows.  Jason Rumney fecit.
		(if (and (fboundp 'w32-add-charset-info)
			 (not (boundp 'w32-unicode-charset-defined)))
		    (w32-add-charset-info "iso10646-1" 'w32-charset-ansi t))))
;; Is this appropriate?
;; 	   (exit-function
;; 	    . (lambda ()
;; 		(if (and (fboundp 'w32-add-charset-info)
;; 			 (not (boundp 'w32-unicode-charset-defined)))
;; 		    (setq w32-charset-info-alist
;; 			  (delete (assoc "iso10646-1")
;; 				  w32-charset-info-alist)))))
	   (input-method . "rfc1345")	; maybe not the best choice
	   (documentation . "\
This language environment is a generic one for a subset of the Unicode
character set encoded in UTF-8."))
 nil)

(provide 'utf-8-lang)

;;; arch-tag: dfa339e1-296f-4b1e-9fe8-2b65279ec813
;;; utf-8-lang.el ends here
