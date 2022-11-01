;;; select.el --- lisp portion of standard selection support

;; Maintainer: FSF
;; Keywords: internal

;; Copyright (C) 1993, 1994, 2001, 2002, 2003, 2004,
;;   2005, 2006, 2007 Free Software Foundation, Inc.
;; Based partially on earlier release by Lucid.

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

;; This is for temporary compatibility with pre-release Emacs 19.
(defalias 'x-selection 'x-get-selection)
(defun x-get-selection (&optional type data-type)
  "Return the value of an X Windows selection.
The argument TYPE (default `PRIMARY') says which selection,
and the argument DATA-TYPE (default `STRING') says
how to convert the data.

TYPE may be any symbol \(but nil stands for `PRIMARY').  However,
only a few symbols are commonly used.  They conventionally have
all upper-case names.  The most often used ones, in addition to
`PRIMARY', are `SECONDARY' and `CLIPBOARD'.

DATA-TYPE is usually `STRING', but can also be one of the symbols
in `selection-converter-alist', which see."
  (let ((data (x-get-selection-internal (or type 'PRIMARY)
					(or data-type 'STRING)))
	coding)
    (when (and (stringp data)
	       (setq data-type (get-text-property 0 'foreign-selection data)))
      (setq coding (if (eq data-type 'UTF8_STRING)
		       'utf-8
		     (or next-selection-coding-system
			 selection-coding-system))
	    data (decode-coding-string data coding))
      (put-text-property 0 (length data) 'foreign-selection data-type data))
    data))

(defun x-get-clipboard ()
  "Return text pasted to the clipboard."
  (x-get-selection-internal 'CLIPBOARD 'STRING))

(defun x-set-selection (type data)
  "Make an X Windows selection of type TYPE and value DATA.
The argument TYPE (nil means `PRIMARY') says which selection, and
DATA specifies the contents.  TYPE must be a symbol.  \(It can also
be a string, which stands for the symbol with that name, but this
is considered obsolete.)  DATA may be a string, a symbol, an
integer (or a cons of two integers or list of two integers).

The selection may also be a cons of two markers pointing to the same buffer,
or an overlay.  In these cases, the selection is considered to be the text
between the markers *at whatever time the selection is examined*.
Thus, editing done in the buffer after you specify the selection
can alter the effective value of the selection.

The data may also be a vector of valid non-vector selection values.

The return value is DATA.

Interactively, this command sets the primary selection.  Without
prefix argument, it reads the selection in the minibuffer.  With
prefix argument, it uses the text of the region as the selection value ."
  (interactive (if (not current-prefix-arg)
		   (list 'PRIMARY (read-string "Set text for pasting: "))
		 (list 'PRIMARY (buffer-substring (region-beginning) (region-end)))))
  ;; This is for temporary compatibility with pre-release Emacs 19.
  (if (stringp type)
      (setq type (intern type)))
  (or (x-valid-simple-selection-p data)
      (and (vectorp data)
	   (let ((valid t)
		 (i (1- (length data))))
	     (while (>= i 0)
	       (or (x-valid-simple-selection-p (aref data i))
		   (setq valid nil))
	       (setq i (1- i)))
	     valid))
      (signal 'error (list "invalid selection" data)))
  (or type (setq type 'PRIMARY))
  (if data
      (x-own-selection-internal type data)
    (x-disown-selection-internal type))
  data)

(defun x-valid-simple-selection-p (data)
  (or (stringp data)
      (symbolp data)
      (integerp data)
      (and (consp data)
	   (integerp (car data))
	   (or (integerp (cdr data))
	       (and (consp (cdr data))
		    (integerp (car (cdr data))))))
      (overlayp data)
      (and (consp data)
	   (markerp (car data))
	   (markerp (cdr data))
	   (marker-buffer (car data))
	   (marker-buffer (cdr data))
	   (eq (marker-buffer (car data))
	       (marker-buffer (cdr data)))
	   (buffer-name (marker-buffer (car data)))
	   (buffer-name (marker-buffer (cdr data))))))

;;; Cut Buffer support

(defun x-get-cut-buffer (&optional which-one)
  "Returns the value of one of the 8 X server cut-buffers.
Optional arg WHICH-ONE should be a number from 0 to 7, defaulting to 0.
Cut buffers are considered obsolete; you should use selections instead."
  (x-get-cut-buffer-internal
   (if which-one
       (aref [CUT_BUFFER0 CUT_BUFFER1 CUT_BUFFER2 CUT_BUFFER3
	      CUT_BUFFER4 CUT_BUFFER5 CUT_BUFFER6 CUT_BUFFER7]
	     which-one)
     'CUT_BUFFER0)))

(defun x-set-cut-buffer (string &optional push)
  "Store STRING into the X server's primary cut buffer.
If PUSH is non-nil, also rotate the cut buffers:
this means the previous value of the primary cut buffer moves to the second
cut buffer, and the second to the third, and so on (there are 8 buffers.)
Cut buffers are considered obsolete; you should use selections instead."
  (or (stringp string) (signal 'wrong-type-argument (list 'string string)))
  (if push
      (x-rotate-cut-buffers-internal 1))
  (x-store-cut-buffer-internal 'CUT_BUFFER0 string))


;;; Functions to convert the selection into various other selection types.
;;; Every selection type that Emacs handles is implemented this way, except
;;; for TIMESTAMP, which is a special case.

(eval-when-compile (require 'ccl))

(define-ccl-program ccl-check-utf-8
  '(0
    ((r0 = 1)
     (loop
      (read-if (r1 < #x80) (repeat)
	((r0 = 0)
	 (if (r1 < #xC2) (end))
	 (read r2)
	 (if ((r2 & #xC0) != #x80) (end))
	 (if (r1 < #xE0) ((r0 = 1) (repeat)))
	 (read r2)
	 (if ((r2 & #xC0) != #x80) (end))
	 (if (r1 < #xF0) ((r0 = 1) (repeat)))
	 (read r2)
	 (if ((r2 & #xC0) != #x80) (end))
	 (if (r1 < #xF8) ((r0 = 1) (repeat)))
	 (read r2)
	 (if ((r2 & #xC0) != #x80) (end))
	 (if (r1 == #xF8) ((r0 = 1) (repeat)))
	 (end))))))
  "Check if the input unibyte string is a valid UTF-8 sequence or not.
If it is valid, set the register `r0' to 1, else set it to 0.")

(defun string-utf-8-p (string)
  "Return non-nil iff STRING is a unibyte string of valid UTF-8 sequence."
  (if (or (not (stringp string))
	  (multibyte-string-p string))
      (error "Not a unibyte string: %s" string))
  (let ((status (make-vector 9 0)))
    (ccl-execute-on-string ccl-check-utf-8 status string)
    (= (aref status 0) 1)))


(defun xselect-convert-to-string (selection type value)
  (let (str coding)
    ;; Get the actual string from VALUE.
    (cond ((stringp value)
	   (setq str value))

	  ((overlayp value)
	   (save-excursion
	     (or (buffer-name (overlay-buffer value))
		 (error "selection is in a killed buffer"))
	     (set-buffer (overlay-buffer value))
	     (setq str (buffer-substring (overlay-start value)
					 (overlay-end value)))))
	  ((and (consp value)
		(markerp (car value))
		(markerp (cdr value)))
	   (or (eq (marker-buffer (car value)) (marker-buffer (cdr value)))
	       (signal 'error
		       (list "markers must be in the same buffer"
			     (car value) (cdr value))))
	   (save-excursion
	     (set-buffer (or (marker-buffer (car value))
			     (error "selection is in a killed buffer")))
	     (setq str (buffer-substring (car value) (cdr value))))))

    (when str
      ;; If TYPE is nil, this is a local request, thus return STR as
      ;; is.  Otherwise, encode STR.
      (if (not type)
	  str
	(setq coding (or next-selection-coding-system selection-coding-system))
	(if coding
	    (setq coding (coding-system-base coding))
	  (setq coding 'raw-text))
	(let ((inhibit-read-only t))
	  ;; Suppress producing escape sequences for compositions.
	  (remove-text-properties 0 (length str) '(composition nil) str)
	  (cond
	   ((eq type 'TEXT)
	    (if (not (multibyte-string-p str))
		;; Don't have to encode unibyte string.
		(setq type 'STRING)
	      ;; If STR contains only ASCII, Latin-1, and raw bytes,
	      ;; encode STR by iso-latin-1, and return it as type
	      ;; `STRING'.  Otherwise, encode STR by CODING.  In that
	      ;; case, the returing type depends on CODING.
	      (let ((charsets (find-charset-string str)))
		(setq charsets
		      (delq 'ascii
			    (delq 'latin-iso8859-1
				  (delq 'eight-bit-control
					(delq 'eight-bit-graphic charsets)))))
		(if charsets
		    (setq str (encode-coding-string str coding)
			  type (if (memq coding '(compound-text
						  compound-text-with-extensions))
				   'COMPOUND_TEXT
				 'STRING))
		  (setq type 'STRING
			str (encode-coding-string str 'iso-latin-1))))))

	   ((eq type 'COMPOUND_TEXT)
	    (setq str (encode-coding-string str coding)))

	   ((eq type 'STRING)
	    (if (memq coding '(compound-text
			       compound-text-with-extensions))
		(setq str (string-make-unibyte str))
	      (setq str (encode-coding-string str coding))))

	   ((eq type 'UTF8_STRING)
	    (if (multibyte-string-p str)
		(setq str (encode-coding-string str 'utf-8)))
	    (if (not (string-utf-8-p str))
		(setq str nil))) ;; Decline request as we don't have UTF-8 data.
	   (t
	    (error "Unknow selection type: %S" type))
	   )))

      (setq next-selection-coding-system nil)
      (cons type str))))


(defun xselect-convert-to-length (selection type value)
  (let ((value
	 (cond ((stringp value)
		(length value))
	       ((overlayp value)
		(abs (- (overlay-end value) (overlay-start value))))
	       ((and (consp value)
		     (markerp (car value))
		     (markerp (cdr value)))
		(or (eq (marker-buffer (car value))
			(marker-buffer (cdr value)))
		    (signal 'error
			    (list "markers must be in the same buffer"
				  (car value) (cdr value))))
		(abs (- (car value) (cdr value)))))))
    (if value ; force it to be in 32-bit format.
	(cons (ash value -16) (logand value 65535))
      nil)))

(defun xselect-convert-to-targets (selection type value)
  ;; return a vector of atoms, but remove duplicates first.
  (let* ((all (cons 'TIMESTAMP (mapcar 'car selection-converter-alist)))
	 (rest all))
    (while rest
      (cond ((memq (car rest) (cdr rest))
	     (setcdr rest (delq (car rest) (cdr rest))))
	    ((eq (car (cdr rest)) '_EMACS_INTERNAL)  ; shh, it's a secret
	     (setcdr rest (cdr (cdr rest))))
	    (t
	     (setq rest (cdr rest)))))
    (apply 'vector all)))

(defun xselect-convert-to-delete (selection type value)
  (x-disown-selection-internal selection)
  ;; A return value of nil means that we do not know how to do this conversion,
  ;; and replies with an "error".  A return value of NULL means that we have
  ;; done the conversion (and any side-effects) but have no value to return.
  'NULL)

(defun xselect-convert-to-filename (selection type value)
  (cond ((overlayp value)
	 (buffer-file-name (or (overlay-buffer value)
			       (error "selection is in a killed buffer"))))
	((and (consp value)
	      (markerp (car value))
	      (markerp (cdr value)))
	 (buffer-file-name (or (marker-buffer (car value))
			       (error "selection is in a killed buffer"))))
	(t nil)))

(defun xselect-convert-to-charpos (selection type value)
  (let (a b tmp)
    (cond ((cond ((overlayp value)
		  (setq a (overlay-start value)
			b (overlay-end value)))
		 ((and (consp value)
		       (markerp (car value))
		       (markerp (cdr value)))
		  (setq a (car value)
			b (cdr value))))
	   (setq a (1- a) b (1- b)) ; zero-based
	   (if (< b a) (setq tmp a a b b tmp))
	   (cons 'SPAN
		 (vector (cons (ash a -16) (logand a 65535))
			 (cons (ash b -16) (logand b 65535))))))))

(defun xselect-convert-to-lineno (selection type value)
  (let (a b buf tmp)
    (cond ((cond ((and (consp value)
		       (markerp (car value))
		       (markerp (cdr value)))
		  (setq a (marker-position (car value))
			b (marker-position (cdr value))
			buf (marker-buffer (car value))))
		 ((overlayp value)
		  (setq buf (overlay-buffer value)
			a (overlay-start value)
			b (overlay-end value)))
		 )
	   (save-excursion
	     (set-buffer buf)
	     (setq a (count-lines 1 a)
		   b (count-lines 1 b)))
	   (if (< b a) (setq tmp a a b b tmp))
	   (cons 'SPAN
		 (vector (cons (ash a -16) (logand a 65535))
			 (cons (ash b -16) (logand b 65535))))))))

(defun xselect-convert-to-colno (selection type value)
  (let (a b buf tmp)
    (cond ((cond ((and (consp value)
		       (markerp (car value))
		       (markerp (cdr value)))
		  (setq a (car value)
			b (cdr value)
			buf (marker-buffer a)))
		 ((overlayp value)
		  (setq buf (overlay-buffer value)
			a (overlay-start value)
			b (overlay-end value)))
		 )
	   (save-excursion
	     (set-buffer buf)
	     (goto-char a)
	     (setq a (current-column))
	     (goto-char b)
	     (setq b (current-column)))
	   (if (< b a) (setq tmp a a b b tmp))
	   (cons 'SPAN
		 (vector (cons (ash a -16) (logand a 65535))
			 (cons (ash b -16) (logand b 65535))))))))

(defun xselect-convert-to-os (selection type size)
  (symbol-name system-type))

(defun xselect-convert-to-host (selection type size)
  (system-name))

(defun xselect-convert-to-user (selection type size)
  (user-full-name))

(defun xselect-convert-to-class (selection type size)
  "Convert selection to class.
This function returns the string \"Emacs\"."
  "Emacs")

;; We do not try to determine the name Emacs was invoked with,
;; because it is not clean for a program's behavior to depend on that.
(defun xselect-convert-to-name (selection type size)
  "Convert selection to name.
This function returns the string \"emacs\"."
  "emacs")

(defun xselect-convert-to-integer (selection type value)
  (and (integerp value)
       (cons (ash value -16) (logand value 65535))))

(defun xselect-convert-to-atom (selection type value)
  (and (symbolp value) value))

(defun xselect-convert-to-identity (selection type value) ; used internally
  (vector value))

(setq selection-converter-alist
      '((TEXT . xselect-convert-to-string)
	(COMPOUND_TEXT . xselect-convert-to-string)
	(STRING . xselect-convert-to-string)
	(UTF8_STRING . xselect-convert-to-string)
	(TARGETS . xselect-convert-to-targets)
	(LENGTH . xselect-convert-to-length)
	(DELETE . xselect-convert-to-delete)
	(FILE_NAME . xselect-convert-to-filename)
	(CHARACTER_POSITION . xselect-convert-to-charpos)
	(LINE_NUMBER . xselect-convert-to-lineno)
	(COLUMN_NUMBER . xselect-convert-to-colno)
	(OWNER_OS . xselect-convert-to-os)
	(HOST_NAME . xselect-convert-to-host)
	(USER . xselect-convert-to-user)
	(CLASS . xselect-convert-to-class)
	(NAME . xselect-convert-to-name)
	(ATOM . xselect-convert-to-atom)
	(INTEGER . xselect-convert-to-integer)
	(_EMACS_INTERNAL . xselect-convert-to-identity)
	))

(provide 'select)

;;; arch-tag: bb634f97-8a3b-4b0a-b940-f6e09982328c
;;; select.el ends here
