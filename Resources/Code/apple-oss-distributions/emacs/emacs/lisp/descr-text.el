;;; descr-text.el --- describe text mode

;; Copyright (C) 1994, 1995, 1996, 2001, 2002, 2003, 2004,
;;   2005, 2006, 2007 Free Software Foundation, Inc.

;; Author: Boris Goldowsky <boris@gnu.org>
;; Maintainer: FSF
;; Keywords: faces, i18n, Unicode, multilingual

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

;;; Describe-Text Mode.

;;; Code:

(eval-when-compile (require 'quail))
(require 'help-fns)

;;; Describe-Text Utilities.

(defun describe-text-widget (widget)
  "Insert text to describe WIDGET in the current buffer."
  (insert-text-button
   (symbol-name (if (symbolp widget) widget (car widget)))
   'action `(lambda (&rest ignore)
	      (widget-browse ',widget))
   'help-echo "mouse-2, RET: browse this widget")
  (insert " ")
  (insert-text-button
   "(widget)Top" 'type 'help-info 'help-args '("(widget)Top")))

(defun describe-text-sexp (sexp)
  "Insert a short description of SEXP in the current buffer."
  (let ((pp (condition-case signal
		(pp-to-string sexp)
	      (error (prin1-to-string signal)))))
    (when (string-match "\n\\'" pp)
      (setq pp (substring pp 0 (1- (length pp)))))
    (if (cond ((string-match "\n" pp)
	       nil)
	      ((> (length pp) (- (window-width) (current-column)))
	       nil)
	      (t t))
	(insert pp)
      (insert-text-button
       "[Show]" 'action `(lambda (&rest ignore)
			(with-output-to-temp-buffer
			    "*Pp Eval Output*"
			  (princ ',pp)))
       'help-echo "mouse-2, RET: pretty print value in another buffer"))))

(defun describe-property-list (properties)
  "Insert a description of PROPERTIES in the current buffer.
PROPERTIES should be a list of overlay or text properties.
The `category', `face' and `font-lock-face' properties are made
into help buttons that call `describe-text-category' or
`describe-face' when pushed."
  ;; Sort the properties by the size of their value.
  (dolist (elt (sort (let (ret)
		       (while properties
			 (push (list (pop properties) (pop properties)) ret))
		       ret)
		     (lambda (a b) (string< (prin1-to-string (nth 0 a) t)
					    (prin1-to-string (nth 0 b) t)))))
    (let ((key (nth 0 elt))
	  (value (nth 1 elt)))
      (insert (propertize (format "  %-20s " key)
			  'face 'help-argument-name))
      (cond ((eq key 'category)
	     (insert-text-button
	      (symbol-name value)
	      'action `(lambda (&rest ignore)
			 (describe-text-category ',value))
	      'help-echo "mouse-2, RET: describe this category"))
            ((memq key '(face font-lock-face mouse-face))
	     (insert-text-button
	      (format "%S" value)
	      'type 'help-face 'help-args (list value)))
            ((widgetp value)
	     (describe-text-widget value))
	    (t
	     (describe-text-sexp value))))
    (insert "\n")))

;;; Describe-Text Commands.

(defun describe-text-category (category)
  "Describe a text property category."
  (interactive "SCategory: ")
  (help-setup-xref (list #'describe-text-category category) (interactive-p))
  (save-excursion
    (with-output-to-temp-buffer "*Help*"
      (set-buffer standard-output)
      (insert "Category " (format "%S" category) ":\n\n")
      (describe-property-list (symbol-plist category))
      (goto-char (point-min)))))

;;;###autoload
(defun describe-text-properties (pos &optional output-buffer)
  "Describe widgets, buttons, overlays and text properties at POS.
Interactively, describe them for the character after point.
If optional second argument OUTPUT-BUFFER is non-nil,
insert the output into that buffer, and don't initialize or clear it
otherwise."
  (interactive "d")
  (if (>= pos (point-max))
      (error "No character follows specified position"))
  (if output-buffer
      (describe-text-properties-1 pos output-buffer)
    (if (not (or (text-properties-at pos) (overlays-at pos)))
	(message "This is plain text.")
      (let ((buffer (current-buffer))
	    (target-buffer "*Help*"))
	(when (eq buffer (get-buffer target-buffer))
	  (setq target-buffer "*Help*<2>"))
	(save-excursion
	  (with-output-to-temp-buffer target-buffer
	    (set-buffer standard-output)
	    (setq output-buffer (current-buffer))
	    (insert "Text content at position " (format "%d" pos) ":\n\n")
	    (with-current-buffer buffer
	      (describe-text-properties-1 pos output-buffer))
	    (goto-char (point-min))))))))

(defun describe-text-properties-1 (pos output-buffer)
  (let* ((properties (text-properties-at pos))
	 (overlays (overlays-at pos))
	 (wid-field (get-char-property pos 'field))
	 (wid-button (get-char-property pos 'button))
	 (wid-doc (get-char-property pos 'widget-doc))
	 ;; If button.el is not loaded, we have no buttons in the text.
	 (button (and (fboundp 'button-at) (button-at pos)))
	 (button-type (and button (button-type button)))
	 (button-label (and button (button-label button)))
	 (widget (or wid-field wid-button wid-doc)))
    (with-current-buffer output-buffer
      ;; Widgets
      (when (widgetp widget)
	(newline)
	(insert (cond (wid-field "This is an editable text area")
		      (wid-button "This is an active area")
		      (wid-doc "This is documentation text")))
	(insert " of a ")
	(describe-text-widget widget)
	(insert ".\n\n"))
      ;; Buttons
      (when (and button (not (widgetp wid-button)))
	(newline)
	(insert "Here is a `" (format "%S" button-type)
		"' button labeled `" button-label "'.\n\n"))
      ;; Overlays
      (when overlays
	(newline)
	(if (eq (length overlays) 1)
	    (insert "There is an overlay here:\n")
	  (insert "There are " (format "%d" (length overlays))
			 " overlays here:\n"))
	(dolist (overlay overlays)
	  (insert " From " (format "%d" (overlay-start overlay))
			 " to " (format "%d" (overlay-end overlay)) "\n")
	  (describe-property-list (overlay-properties overlay)))
	(insert "\n"))
      ;; Text properties
      (when properties
	(newline)
	(insert "There are text properties here:\n")
	(describe-property-list properties)))))

(defcustom describe-char-unicodedata-file nil
  "Location of Unicode data file.
This is the UnicodeData.txt file from the Unicode Consortium, used for
diagnostics.  If it is non-nil `describe-char' will print data
looked up from it.  This facility is mostly of use to people doing
multilingual development.

This is a fairly large file, not typically present on GNU systems.
At the time of writing it is at the URL
`http://www.unicode.org/Public/UNIDATA/UnicodeData.txt'."
  :group 'mule
  :version "22.1"
  :type '(choice (const :tag "None" nil)
		 file))

;; We could convert the unidata file into a Lispy form once-for-all
;; and distribute it for loading on demand.  It might be made more
;; space-efficient by splitting strings word-wise and replacing them
;; with lists of symbols interned in a private obarray, e.g.
;; "LATIN SMALL LETTER A" => '(LATIN SMALL LETTER A).

;; Fixme: Check whether this needs updating for Unicode 4.
(defun describe-char-unicode-data (char)
  "Return a list of Unicode data for unicode CHAR.
Each element is a list of a property description and the property value.
The list is null if CHAR isn't found in `describe-char-unicodedata-file'."
  (when describe-char-unicodedata-file
    (unless (file-exists-p describe-char-unicodedata-file)
      (error "`unicodedata-file' %s not found" describe-char-unicodedata-file))
    (with-current-buffer (get-buffer-create " *Unicode Data*")
      (when (zerop (buffer-size))
	;; Don't use -literally in case of DOS line endings.
	(insert-file-contents describe-char-unicodedata-file))
      (goto-char (point-min))
      (let ((hex (format "%04X" char))
	    found first last)
	(if (re-search-forward (concat "^" hex) nil t)
	    (setq found t)
	  ;; It's not listed explicitly.  Look for ranges, e.g. CJK
	  ;; ideographs, and check whether it's in one of them.
	  (while (and (re-search-forward "^\\([^;]+\\);[^;]+First>;" nil t)
		      (>= char (setq first
				     (string-to-number (match-string 1) 16)))
		      (progn
			(forward-line 1)
			(looking-at "^\\([^;]+\\);[^;]+Last>;")
			(> char
			   (setq last
				 (string-to-number (match-string 1) 16))))))
	  (if (and (>= char first)
		   (<= char last))
	      (setq found t)))
	(if found
	    (let ((fields (mapcar (lambda (elt)
				    (if (> (length elt) 0)
					elt))
				  (cdr (split-string
					(buffer-substring
					 (line-beginning-position)
					 (line-end-position))
					";")))))
	      ;; The length depends on whether the last field was empty.
	      (unless (or (= 13 (length fields))
			  (= 14 (length fields)))
		(error "Invalid contents in %s" describe-char-unicodedata-file))
	      ;; The field names and values lists are slightly
	      ;; modified from Mule-UCS unidata.el.
	      (list
	       (list "Name" (let ((name (nth 0 fields)))
			      ;; Check for <..., First>, <..., Last>
			      (if (string-match "\\`\\(<[^,]+\\)," name)
				  (concat (match-string 1 name) ">")
				name)))
	       (list "Category"
		     (cdr (assoc
			   (nth 1 fields)
			   '(("Lu" . "uppercase letter")
			     ("Ll" . "lowercase letter")
			     ("Lt" . "titlecase letter")
			     ("Mn" . "non-spacing mark")
			     ("Mc" . "spacing-combining mark")
			     ("Me" . "enclosing mark")
			     ("Nd" . "decimal digit")
			     ("Nl" . "letter number")
			     ("No" . "other number")
			     ("Zs" . "space separator")
			     ("Zl" . "line separator")
			     ("Zp" . "paragraph separator")
			     ("Cc" . "other control")
			     ("Cf" . "other format")
			     ("Cs" . "surrogate")
			     ("Co" . "private use")
			     ("Cn" . "not assigned")
			     ("Lm" . "modifier letter")
			     ("Lo" . "other letter")
			     ("Pc" . "connector punctuation")
			     ("Pd" . "dash punctuation")
			     ("Ps" . "open punctuation")
			     ("Pe" . "close punctuation")
			     ("Pi" . "initial-quotation punctuation")
			     ("Pf" . "final-quotation punctuation")
			     ("Po" . "other punctuation")
			     ("Sm" . "math symbol")
			     ("Sc" . "currency symbol")
			     ("Sk" . "modifier symbol")
			     ("So" . "other symbol")))))
	       (list "Combining class"
		     (cdr (assoc
			   (string-to-number (nth 2 fields))
			   '((0 . "Spacing")
			     (1 . "Overlays and interior")
			     (7 . "Nuktas")
			     (8 . "Hiragana/Katakana voicing marks")
			     (9 . "Viramas")
			     (10 . "Start of fixed position classes")
			     (199 . "End of fixed position classes")
			     (200 . "Below left attached")
			     (202 . "Below attached")
			     (204 . "Below right attached")
			     (208 . "Left attached (reordrant around \
single base character)")
			     (210 . "Right attached")
			     (212 . "Above left attached")
			     (214 . "Above attached")
			     (216 . "Above right attached")
			     (218 . "Below left")
			     (220 . "Below")
			     (222 . "Below right")
			     (224 . "Left (reordrant around single base \
character)")
			     (226 . "Right")
			     (228 . "Above left")
			     (230 . "Above")
			     (232 . "Above right")
			     (233 . "Double below")
			     (234 . "Double above")
			     (240 . "Below (iota subscript)")))))
	       (list "Bidi category"
		     (cdr (assoc
			   (nth 3 fields)
			   '(("L" . "Left-to-Right")
			     ("LRE" . "Left-to-Right Embedding")
			     ("LRO" . "Left-to-Right Override")
			     ("R" . "Right-to-Left")
			     ("AL" . "Right-to-Left Arabic")
			     ("RLE" . "Right-to-Left Embedding")
			     ("RLO" . "Right-to-Left Override")
			     ("PDF" . "Pop Directional Format")
			     ("EN" . "European Number")
			     ("ES" . "European Number Separator")
			     ("ET" . "European Number Terminator")
			     ("AN" . "Arabic Number")
			     ("CS" . "Common Number Separator")
			     ("NSM" . "Non-Spacing Mark")
			     ("BN" . "Boundary Neutral")
			     ("B" . "Paragraph Separator")
			     ("S" . "Segment Separator")
			     ("WS" . "Whitespace")
			     ("ON" . "Other Neutrals")))))
	       (list
		"Decomposition"
		(if (nth 4 fields)
		    (let* ((parts (split-string (nth 4 fields)))
			   (info (car parts)))
		      (if (string-match "\\`<\\(.+\\)>\\'" info)
			  (setq info (match-string 1 info))
			(setq info nil))
		      (if info (setq parts (cdr parts)))
		      ;; Maybe printing ? for unrepresentable unicodes
		      ;; here and below should be changed?
		      (setq parts (mapconcat
				   (lambda (arg)
				     (string (or (decode-char
						  'ucs
						  (string-to-number arg 16))
						 ??)))
				   parts " "))
		      (concat info parts))))
	       (list "Decimal digit value"
		     (nth 5 fields))
	       (list "Digit value"
		     (nth 6 fields))
	       (list "Numeric value"
		     (nth 7 fields))
	       (list "Mirrored"
		     (if (equal "Y" (nth 8 fields))
			 "yes"))
	       (list "Old name" (nth 9 fields))
	       (list "ISO 10646 comment" (nth 10 fields))
	       (list "Uppercase" (and (nth 11 fields)
				      (string (or (decode-char
						   'ucs
						   (string-to-number
						    (nth 11 fields) 16))
						  ??))))
	       (list "Lowercase" (and (nth 12 fields)
				      (string (or (decode-char
						   'ucs
						   (string-to-number
						    (nth 12 fields) 16))
						  ??))))
	       (list "Titlecase" (and (nth 13 fields)
				      (string (or (decode-char
						   'ucs
						   (string-to-number
						    (nth 13 fields) 16))
						  ??)))))))))))

;; Return information about how CHAR is displayed at the buffer
;; position POS.  If the selected frame is on a graphic display,
;; return a cons (FONTNAME . GLYPH-CODE).  Otherwise, return a string
;; describing the terminal codes for the character.
(defun describe-char-display (pos char)
  (if (display-graphic-p (selected-frame))
      (internal-char-font pos char)
    (let* ((coding (terminal-coding-system))
	   (encoded (encode-coding-char char coding)))
      (if encoded
	  (encoded-string-description encoded coding)))))


;;;###autoload
(defun describe-char (pos)
  "Describe the character after POS (interactively, the character after point).
The information includes character code, charset and code points in it,
syntax, category, how the character is encoded in a file,
character composition information (if relevant),
as well as widgets, buttons, overlays, and text properties."
  (interactive "d")
  (if (>= pos (point-max))
      (error "No character follows specified position"))
  (let* ((char (char-after pos))
	 (charset (char-charset char))
	 (composition (find-composition pos nil nil t))
	 (component-chars nil)
	 (display-table (or (window-display-table)
			    buffer-display-table
			    standard-display-table))
	 (disp-vector (and display-table (aref display-table char)))
	 (multibyte-p enable-multibyte-characters)
	 (overlays (mapcar #'(lambda (o) (overlay-properties o))
			   (overlays-at pos)))
	 (char-description (if (not multibyte-p)
			       (single-key-description char)
			     (if (< char 128)
				 (single-key-description char)
			       (string-to-multibyte
				(char-to-string char)))))
	 (text-props-desc
	  (let ((tmp-buf (generate-new-buffer " *text-props*")))
	    (unwind-protect
		(progn
		  (describe-text-properties pos tmp-buf)
		  (with-current-buffer tmp-buf (buffer-string)))
	      (kill-buffer tmp-buf))))
	 item-list max-width unicode)

    (if (or (< char 256)
	    (memq 'mule-utf-8 (find-coding-systems-region pos (1+ pos)))
	    (get-char-property pos 'untranslated-utf-8))
	(setq unicode (or (get-char-property pos 'untranslated-utf-8)
			  (encode-char char 'ucs))))
    (setq item-list
	  `(("character"
	     ,(format "%s (%d, #o%o, #x%x%s)"
		      (apply 'propertize char-description
			     (text-properties-at pos))
		      char char char
		      (if unicode
			  (format ", U+%04X" unicode)
			"")))
	    ("charset"
	     ,`(insert-text-button
		,(symbol-name charset)
		'type 'help-character-set 'help-args '(,charset))
	     ,(format "(%s)" (charset-description charset)))
	    ("code point"
	     ,(let ((split (split-char char)))
		`(insert-text-button
		  ,(if (= (charset-dimension charset) 1)
		       (format "#x%02X" (nth 1 split))
		     (format "#x%02X #x%02X" (nth 1 split)
			     (nth 2 split)))
		  'action (lambda (&rest ignore)
			    (list-charset-chars ',charset)
			    (with-selected-window
				(get-buffer-window "*Character List*" 0)
			      (goto-char (point-min))
			      (forward-line 2) ;Skip the header.
			      (let ((case-fold-search nil))
				(search-forward ,(char-to-string char)
						nil t))))
		  'help-echo
		  "mouse-2, RET: show this character in its character set")))
	    ("syntax"
	     ,(let ((syntax (syntax-after pos)))
		(with-temp-buffer
		  (internal-describe-syntax-value syntax)
		  (buffer-string))))
	    ("category"
	     ,@(let ((category-set (char-category-set char)))
		 (if (not category-set)
		     '("-- none --")
		   (mapcar #'(lambda (x) (format "%c:%s"
						 x (category-docstring x)))
			   (category-set-mnemonics category-set)))))
	    ,@(let ((props (aref char-code-property-table char))
		    ps)
		(when props
		  (while props
		    (push (format "%s:" (pop props)) ps)
		    (push (format "%s;" (pop props)) ps))
		  (list (cons "Properties" (nreverse ps)))))
	    ("to input"
	     ,@(let ((key-list (and (eq input-method-function
					'quail-input-method)
				    (quail-find-key char))))
		 (if (consp key-list)
		     (list "type"
			   (mapconcat #'(lambda (x) (concat "\"" x "\""))
				      key-list " or ")
			   "with"
			   `(insert-text-button
			     ,current-input-method
			     'type 'help-input-method
			     'help-args '(,current-input-method))))))
	    ("buffer code"
	     ,(encoded-string-description
	       (string-as-unibyte (char-to-string char)) nil))
	    ("file code"
	     ,@(let* ((coding buffer-file-coding-system)
		      (encoded (encode-coding-char char coding)))
		 (if encoded
		     (list (encoded-string-description encoded coding)
			   (format "(encoded by coding system %S)" coding))
		   (list "not encodable by coding system"
			 (symbol-name coding)))))
	    ("display"
	     ,(cond
	       (disp-vector
		(setq disp-vector (copy-sequence disp-vector))
		(dotimes (i (length disp-vector))
		  (setq char (aref disp-vector i))
		  (aset disp-vector i
			(cons char (describe-char-display
				    pos (glyph-char char)))))
		(format "by display table entry [%s] (see below)"
			(mapconcat
			 #'(lambda (x)
			     (format "?%c" (glyph-char (car x))))
			 disp-vector " ")))
	       (composition
		(let ((from (car composition))
		      (to (nth 1 composition))
		      (next (1+ pos))
		      (components (nth 2 composition))
		      ch)
		  (setcar composition
			  (and (< from pos) (buffer-substring from pos)))
		  (setcar (cdr composition)
			  (and (< next to) (buffer-substring next to)))
		  (dotimes (i (length components))
		    (if (integerp (setq ch (aref components i)))
			(push (cons ch (describe-char-display pos ch))
			      component-chars)))
		  (setq component-chars (nreverse component-chars))
		  (format "composed to form \"%s\" (see below)"
			  (buffer-substring from to))))
	       (t
		(let ((display (describe-char-display pos char)))
		  (if (display-graphic-p (selected-frame))
		      (if display
			  (concat
			   "by this font (glyph code)\n"
			   (format "     %s (#x%02X)"
				   (car display) (cdr display)))
			"no font available")
		    (if display
			(format "terminal code %s" display)
		      "not encodable for terminal"))))))
	    ,@(let ((face
		     (if (not (or disp-vector composition))
			 (cond
			  ((and show-trailing-whitespace
				(save-excursion (goto-char pos)
						(looking-at "[ \t]+$")))
			   'trailing-whitespace)
			  ((and nobreak-char-display unicode (eq unicode '#xa0))
			   'nobreak-space)
			  ((and nobreak-char-display unicode (eq unicode '#xad))
			   'escape-glyph)
			  ((and (< char 32) (not (memq char '(9 10))))
			   'escape-glyph)))))
		(if face (list (list "hardcoded face"
				     `(insert-text-button
				       ,(symbol-name face)
				       'type 'help-face 'help-args '(,face))))))
	    ,@(let ((unicodedata (and unicode
				      (describe-char-unicode-data unicode))))
		(if unicodedata
		    (cons (list "Unicode data" " ") unicodedata)))))
    (setq max-width (apply #'max (mapcar #'(lambda (x)
					     (if (cadr x) (length (car x)) 0))
					 item-list)))
    (help-setup-xref nil (interactive-p))
    (with-output-to-temp-buffer (help-buffer)
      (with-current-buffer standard-output
	(set-buffer-multibyte multibyte-p)
	(let ((formatter (format "%%%ds:" max-width)))
	  (dolist (elt item-list)
	    (when (cadr elt)
	      (insert (format formatter (car elt)))
	      (dolist (clm (cdr elt))
		(if (eq (car-safe clm) 'insert-text-button)
		    (progn (insert " ") (eval clm))
		  (when (>= (+ (current-column)
			       (or (string-match "\n" clm)
				   (string-width clm))
			       1)
			    (window-width))
		    (insert "\n")
		    (indent-to (1+ max-width)))
		  (insert " " clm)))
	      (insert "\n"))))

	(when overlays
	  (save-excursion
	    (goto-char (point-min))
	    (re-search-forward "character:[ \t\n]+")
	    (let* ((end (+ (point) (length char-description))))
	      (mapc #'(lambda (props)
			(let ((o (make-overlay (point) end)))
			  (while props
			    (overlay-put o (car props) (nth 1 props))
			    (setq props (cddr props)))))
		    overlays))))

	(when disp-vector
	  (insert
	   "\nThe display table entry is displayed by ")
	  (if (display-graphic-p (selected-frame))
	      (progn
		(insert "these fonts (glyph codes):\n")
		(dotimes (i (length disp-vector))
		  (insert (glyph-char (car (aref disp-vector i))) ?:
			  (propertize " " 'display '(space :align-to 5))
			  (if (cdr (aref disp-vector i))
			      (format "%s (#x%02X)" (cadr (aref disp-vector i))
				      (cddr (aref disp-vector i)))
			    "-- no font --")
			  "\n")
		  (let ((face (glyph-face (car (aref disp-vector i)))))
		    (when face
		      (insert (propertize " " 'display '(space :align-to 5))
			      "face: ")
		      (insert (concat "`" (symbol-name face) "'"))
		      (insert "\n")))))
	    (insert "these terminal codes:\n")
	    (dotimes (i (length disp-vector))
	      (insert (car (aref disp-vector i))
		      (propertize " " 'display '(space :align-to 5))
		      (or (cdr (aref disp-vector i)) "-- not encodable --")
		      "\n"))))

	(when composition
	  (insert "\nComposed")
	  (if (car composition)
	      (if (cadr composition)
		  (insert " with the surrounding characters \""
			  (car composition) "\" and \""
			  (cadr composition) "\"")
		(insert " with the preceding character(s) \""
			(car composition) "\""))
	    (if (cadr composition)
		(insert " with the following character(s) \""
			(cadr composition) "\"")))
	  (insert " by the rule:\n\t("
		  (mapconcat (lambda (x)
			       (format (if (consp x) "%S" "?%c") x))
			     (nth 2 composition)
			     " ")
		  ")")
	  (insert  "\nThe component character(s) are displayed by ")
	  (if (display-graphic-p (selected-frame))
	      (progn
		(insert "these fonts (glyph codes):")
		(dolist (elt component-chars)
		  (insert "\n " (car elt) ?:
			  (propertize " " 'display '(space :align-to 5))
			  (if (cdr elt)
			      (format "%s (#x%02X)" (cadr elt) (cddr elt))
			    "-- no font --"))))
	    (insert "these terminal codes:")
	    (dolist (elt component-chars)
	      (insert "\n  " (car elt) ":"
		      (propertize " " 'display '(space :align-to 5))
		      (or (cdr elt) "-- not encodable --"))))
	  (insert "\nSee the variable `reference-point-alist' for "
		  "the meaning of the rule.\n"))

        (if text-props-desc (insert text-props-desc))
	(setq help-xref-stack-item (list 'help-insert-string (buffer-string)))
	(toggle-read-only 1)
	(print-help-return-message)))))

(defalias 'describe-char-after 'describe-char)
(make-obsolete 'describe-char-after 'describe-char "22.1")

(provide 'descr-text)

;; arch-tag: fc55a498-f3e9-4312-b5bd-98cc02480af1
;;; descr-text.el ends here
