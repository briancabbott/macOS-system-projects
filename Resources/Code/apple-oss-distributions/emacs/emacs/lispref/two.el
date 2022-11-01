;; Auxiliary functions for preparing a two volume manual.

;; Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006, 2007
;;   Free Software Foundation, Inc.

;; --rjc 30mar92

;; This file is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 2, or (at your option)
;; any later version.

;; This file is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with this file; see the file COPYING.  If not, write to
;; the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
;; Boston, MA 02110-1301, USA.

(defun volume-aux-markup (arg)
  "Append `vol. NUMBER' to page number.
Apply to aux file that you save.
Then insert marked file into other volume's .aux file."
  (interactive "sType volume number, 1 or 2: " )
  (goto-char (point-min))
  (while (search-forward "-pg" nil t)
    (end-of-line 1)
    (delete-backward-char 1 nil)
    (insert ", vol.'tie" arg "}")))

(defun volume-index-markup (arg)
  "Prepend  `NUMBER:' to page number.  Use Roman Numeral.
Apply only to unsorted index file,
Then insert marked file into other volume's unsorted index file.
Then run texindex on that file and save."
  (interactive
   "sType volume number,  roman number I or II: " )
  (goto-char (point-min))
  (while (search-forward "\\entry" nil t)
    (search-forward "}{" (save-excursion (end-of-line) (point)) nil)
    (insert arg ":")))

(defun volume-numbers-toc-markup (arg)
  (interactive
   "sType volume number,  roman number I or II: " )
  (goto-char (point-min))
  (while (search-forward "chapentry" nil t)
    (end-of-line)
    (search-backward "{" nil t)
    (forward-char 1)
    (insert arg ":")))

(defun volume-header-toc-markup ()
  "Insert Volume I and Volume II text into .toc file.
NOTE: this auxilary function is file specific.
This is for the *Elisp Ref Manual*"
  (interactive)
  (goto-char (point-min))
  (insert "\\unnumbchapentry {Volume 1}{}\n\\unnumbchapentry {}{}\n")
  (search-forward "\\unnumbchapentry {Index}")
  (forward-line 1)
  (insert
   "\\unnumbchapentry {}{}\n\\unnumbchapentry {}{}\n\\unnumbchapentry {}{}\n\\unnumbchapentry {}{}\n\\unnumbchapentry {Volume 2}{}\n\\unnumbchapentry {}{}\n"))


;;; In batch mode, you cannot call functions with args; hence this kludge:

(defun volume-aux-markup-1 () (volume-aux-markup "1"))
(defun volume-aux-markup-2 () (volume-aux-markup "2"))

(defun volume-index-markup-I () (volume-index-markup "I"))
(defun volume-index-markup-II () (volume-index-markup "II"))

(defun volume-numbers-toc-markup-I () (volume-numbers-toc-markup "I"))
(defun volume-numbers-toc-markup-II () (volume-numbers-toc-markup "II"))

;;; arch-tag: 848955fe-e9cf-45e7-a2f1-570ef156d6a5
