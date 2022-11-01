;;; mh-gnus.el --- make MH-E compatible with various versions of Gnus

;; Copyright (C) 2003, 2004, 2006, 2007 Free Software Foundation, Inc.

;; Author: Satyaki Das <satyaki@theforce.stanford.edu>
;; Maintainer: Bill Wohler <wohler@newt.com>
;; Keywords: mail
;; See: mh-e.el

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

;;; Change Log:

;;; Code:

(require 'mh-e)

(mh-require 'gnus-util nil t)
(mh-require 'mm-bodies nil t)
(mh-require 'mm-decode nil t)
(mh-require 'mm-view nil t)
(mh-require 'mml nil t)

;; Copy of function from gnus-util.el.
(defun-mh mh-gnus-local-map-property gnus-local-map-property (map)
  "Return a list suitable for a text property list specifying keymap MAP."
  (cond (mh-xemacs-flag (list 'keymap map))
        ((>= emacs-major-version 21) (list 'keymap map))
        (t (list 'local-map map))))

;; Copy of function from mm-decode.el.
(defun-mh mh-mm-merge-handles mm-merge-handles (handles1 handles2)
  (append (if (listp (car handles1)) handles1 (list handles1))
          (if (listp (car handles2)) handles2 (list handles2))))

;; Copy of function from mm-decode.el.
(defun-mh mh-mm-set-handle-multipart-parameter
  mm-set-handle-multipart-parameter (handle parameter value)
  ;; HANDLE could be a CTL.
  (if handle
      (put-text-property 0 (length (car handle)) parameter value
                         (car handle))))

;; Copy of function from mm-view.el.
(defun-mh mh-mm-inline-text-vcard mm-inline-text-vcard (handle)
  (let (buffer-read-only)
    (mm-insert-inline
     handle
     (concat "\n-- \n"
             (ignore-errors
               (if (fboundp 'vcard-pretty-print)
                   (vcard-pretty-print (mm-get-part handle))
                 (vcard-format-string
                  (vcard-parse-string (mm-get-part handle)
                                      'vcard-standard-filter))))))))

;; Function from mm-decode.el used in PGP messages. Just define it with older
;; Gnus to avoid compiler warning.
(defun-mh mh-mm-possibly-verify-or-decrypt
  mm-possibly-verify-or-decrypt (parts ctl)
  nil)

;; Copy of macro in mm-decode.el.
(defmacro-mh mh-mm-handle-multipart-ctl-parameter
  mm-handle-multipart-ctl-parameter (handle parameter)
  `(get-text-property 0 ,parameter (car ,handle)))

;; Copy of function in mm-decode.el.
(defun-mh mh-mm-readable-p mm-readable-p (handle)
  "Say whether the content of HANDLE is readable."
  (and (< (with-current-buffer (mm-handle-buffer handle)
            (buffer-size)) 10000)
       (mm-with-unibyte-buffer
         (mm-insert-part handle)
         (and (eq (mm-body-7-or-8) '7bit)
              (not (mh-mm-long-lines-p 76))))))

;; Copy of function in mm-bodies.el.
(defun-mh mh-mm-long-lines-p mm-long-lines-p (length)
  "Say whether any of the lines in the buffer is longer than LENGTH."
  (save-excursion
    (goto-char (point-min))
    (end-of-line)
    (while (and (not (eobp))
                (not (> (current-column) length)))
      (forward-line 1)
      (end-of-line))
    (and (> (current-column) length)
         (current-column))))

(defun-mh mh-mm-keep-viewer-alive-p mm-keep-viewer-alive-p (handle)
  ;; Released Gnus doesn't keep handles associated with externally displayed
  ;; MIME parts. So this will always return nil.
  nil)

(defun-mh mh-mm-destroy-parts mm-destroy-parts (list)
  "Older versions of Emacs don't have this function."
  nil)

(defun-mh mh-mm-uu-dissect-text-parts mm-uu-dissect-text-parts (handles)
  "Emacs 21 and XEmacs don't have this function."
  nil)

;; Copy of function in mml.el.
(defun-mh mh-mml-minibuffer-read-disposition
  mml-minibuffer-read-disposition (type &optional default)
  (unless default (setq default
                        (if (and (string-match "\\`text/" type)
                                 (not (string-match "\\`text/rtf\\'" type)))
                            "inline"
                          "attachment")))
  (let ((disposition (completing-read
                      (format "Disposition (default %s): " default)
                      '(("attachment") ("inline") (""))
                      nil t nil nil default)))
    (if (not (equal disposition ""))
        disposition
      default)))

;; This is mm-save-part from Gnus 5.10 since that function in emacs21.2 is
;; buggy (the args to read-file-name are incorrect). When all supported
;; versions of Emacs come with at least Gnus 5.10, we can delete this
;; function and rename calls to mh-mm-save-part to mm-save-part.
(defun mh-mm-save-part (handle)
  "Write HANDLE to a file."
  (let ((name (mail-content-type-get (mm-handle-type handle) 'name))
        (filename (mail-content-type-get
                   (mm-handle-disposition handle) 'filename))
        file)
    (when filename
      (setq filename (file-name-nondirectory filename)))
    (setq file (read-file-name "Save MIME part to: "
                               (or mm-default-directory
                                   default-directory)
                               nil nil (or filename name "")))
    (setq mm-default-directory (file-name-directory file))
    (and (or (not (file-exists-p file))
             (yes-or-no-p (format "File %s already exists; overwrite? "
                                  file)))
         (mm-save-part-to-file handle file))))

(defun mh-mm-text-html-renderer ()
  "Find the renderer Gnus is using to display text/html MIME parts."
  (or (and (boundp 'mm-inline-text-html-renderer) mm-inline-text-html-renderer)
      (and (boundp 'mm-text-html-renderer) mm-text-html-renderer)))

(provide 'mh-gnus)

;; Local Variables:
;; no-byte-compile: t
;; no-update-autoloads: t
;; indent-tabs-mode: nil
;; sentence-end-double-space: nil
;; End:

;; arch-tag: 1e3638af-cad3-4c69-8427-bc8eb6e5e4fa
;;; mh-gnus.el ends here
