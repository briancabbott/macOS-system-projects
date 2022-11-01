;;; help-fns.el --- Complex help functions

;; Copyright (C) 1985, 1986, 1993, 1994, 1998, 1999, 2000, 2001,
;;   2002, 2003, 2004, 2005, 2006, 2007 Free Software Foundation, Inc.

;; Maintainer: FSF
;; Keywords: help, internal

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

;; This file contains those help commands which are complicated, and
;; which may not be used in every session.  For example
;; `describe-function' will probably be heavily used when doing elisp
;; programming, but not if just editing C files.  Simpler help commands
;; are in help.el

;;; Code:

(require 'help-mode)

;; Functions

;;;###autoload
(defun describe-function (function)
  "Display the full documentation of FUNCTION (a symbol)."
  (interactive
   (let ((fn (function-called-at-point))
	 (enable-recursive-minibuffers t)
	 val)
     (setq val (completing-read (if fn
				    (format "Describe function (default %s): " fn)
				  "Describe function: ")
				obarray 'fboundp t nil nil
				(and fn (symbol-name fn))))
     (list (if (equal val "")
	       fn (intern val)))))
  (if (null function)
      (message "You didn't specify a function")
    (help-setup-xref (list #'describe-function function) (interactive-p))
    (save-excursion
      (with-output-to-temp-buffer (help-buffer)
	(prin1 function)
	;; Use " is " instead of a colon so that
	;; it is easier to get out the function name using forward-sexp.
	(princ " is ")
	(describe-function-1 function)
	(print-help-return-message)
	(with-current-buffer standard-output
	  ;; Return the text we displayed.
	  (buffer-string))))))

(defun help-split-fundoc (docstring def)
  "Split a function DOCSTRING into the actual doc and the usage info.
Return (USAGE . DOC) or nil if there's no usage info.
DEF is the function whose usage we're looking for in DOCSTRING."
  ;; Functions can get the calling sequence at the end of the doc string.
  ;; In cases where `function' has been fset to a subr we can't search for
  ;; function's name in the doc string so we use `fn' as the anonymous
  ;; function name instead.
  (when (and docstring (string-match "\n\n(fn\\(\\( .*\\)?)\\)\\'" docstring))
    (cons (format "(%s%s"
		  ;; Replace `fn' with the actual function name.
		  (if (consp def) "anonymous" def)
		  (match-string 1 docstring))
	  (substring docstring 0 (match-beginning 0)))))

(defun help-add-fundoc-usage (docstring arglist)
  "Add the usage info to DOCSTRING.
If DOCSTRING already has a usage info, then just return it unchanged.
The usage info is built from ARGLIST.  DOCSTRING can be nil.
ARGLIST can also be t or a string of the form \"(FUN ARG1 ARG2 ...)\"."
  (unless (stringp docstring) (setq docstring "Not documented"))
  (if (or (string-match "\n\n(fn\\(\\( .*\\)?)\\)\\'" docstring) (eq arglist t))
      docstring
    (concat docstring
	    (if (string-match "\n?\n\\'" docstring)
		(if (< (- (match-end 0) (match-beginning 0)) 2) "\n" "")
	      "\n\n")
	    (if (and (stringp arglist)
		     (string-match "\\`([^ ]+\\(.*\\))\\'" arglist))
		(concat "(fn" (match-string 1 arglist) ")")
	      (format "%S" (help-make-usage 'fn arglist))))))

(defun help-function-arglist (def)
  ;; Handle symbols aliased to other symbols.
  (if (and (symbolp def) (fboundp def)) (setq def (indirect-function def)))
  ;; If definition is a macro, find the function inside it.
  (if (eq (car-safe def) 'macro) (setq def (cdr def)))
  (cond
   ((byte-code-function-p def) (aref def 0))
   ((eq (car-safe def) 'lambda) (nth 1 def))
   ((and (eq (car-safe def) 'autoload) (not (eq (nth 4 def) 'keymap)))
    "[Arg list not available until function definition is loaded.]")
   (t t)))

(defun help-make-usage (function arglist)
  (cons (if (symbolp function) function 'anonymous)
	(mapcar (lambda (arg)
		  (if (not (symbolp arg))
		      (if (and (consp arg) (symbolp (car arg)))
			  ;; CL style default values for optional args.
			  (cons (intern (upcase (symbol-name (car arg))))
				(cdr arg))
			arg)
		    (let ((name (symbol-name arg)))
		      (if (string-match "\\`&" name) arg
			(intern (upcase name))))))
		arglist)))

;; Could be this, if we make symbol-file do the work below.
;; (defun help-C-file-name (subr-or-var kind)
;;   "Return the name of the C file where SUBR-OR-VAR is defined.
;; KIND should be `var' for a variable or `subr' for a subroutine."
;;   (symbol-file (if (symbolp subr-or-var) subr-or-var
;; 		 (subr-name subr-or-var))
;; 	       (if (eq kind 'var) 'defvar 'defun)))
;;;###autoload
(defun help-C-file-name (subr-or-var kind)
  "Return the name of the C file where SUBR-OR-VAR is defined.
KIND should be `var' for a variable or `subr' for a subroutine."
  (let ((docbuf (get-buffer-create " *DOC*"))
	(name (if (eq 'var kind)
		  (concat "V" (symbol-name subr-or-var))
		(concat "F" (subr-name subr-or-var)))))
    (with-current-buffer docbuf
      (goto-char (point-min))
      (if (eobp)
	  (insert-file-contents-literally
	   (expand-file-name internal-doc-file-name doc-directory)))
      (let ((file (catch 'loop
		    (while t
		      (let ((pnt (search-forward (concat "" name "\n"))))
			(re-search-backward "S\\(.*\\)")
			(let ((file (match-string 1)))
			  (if (member file build-files)
			      (throw 'loop file)
			    (goto-char pnt))))))))
	(if (string-match "\\.\\(o\\|obj\\)\\'" file)
	    (setq file (replace-match ".c" t t file)))
	(if (string-match "\\.c\\'" file)
	    (concat "src/" file)
	  file)))))

(defface help-argument-name '((((supports :slant italic)) :inherit italic))
  "Face to highlight argument names in *Help* buffers."
  :group 'help)

(defun help-default-arg-highlight (arg)
  "Default function to highlight arguments in *Help* buffers.
It returns ARG in face `help-argument-name'; ARG is also
downcased if it displays differently than the default
face (according to `face-differs-from-default-p')."
  (propertize (if (face-differs-from-default-p 'help-argument-name)
                  (downcase arg)
                arg)
              'face 'help-argument-name))

(defun help-do-arg-highlight (doc args)
  (with-syntax-table (make-syntax-table emacs-lisp-mode-syntax-table)
    (modify-syntax-entry ?\- "w")
    (dolist (arg args doc)
      (setq doc (replace-regexp-in-string
                 ;; This is heuristic, but covers all common cases
                 ;; except ARG1-ARG2
                 (concat "\\<"                   ; beginning of word
                         "\\(?:[a-z-]*-\\)?"     ; for xxx-ARG
                         "\\("
                         (regexp-quote arg)
                         "\\)"
                         "\\(?:es\\|s\\|th\\)?"  ; for ARGth, ARGs
                         "\\(?:-[a-z0-9-]+\\)?"  ; for ARG-xxx, ARG-n
                         "\\(?:-[{([<`\"].*?\\)?"; for ARG-{x}, (x), <x>, [x], `x'
                         "\\>")                  ; end of word
                 (help-default-arg-highlight arg)
                 doc t t 1)))))

(defun help-highlight-arguments (usage doc &rest args)
  (when usage
    (with-temp-buffer
      (insert usage)
      (goto-char (point-min))
      (let ((case-fold-search nil)
            (next (not (or args (looking-at "\\["))))
            (opt nil))
        ;; Make a list of all arguments
        (skip-chars-forward "^ ")
        (while next
          (or opt (not (looking-at " &")) (setq opt t))
          (if (not (re-search-forward " \\([\\[(]*\\)\\([^] &)\.]+\\)" nil t))
              (setq next nil)
            (setq args (cons (match-string 2) args))
            (when (and opt (string= (match-string 1) "("))
              ;; A pesky CL-style optional argument with default value,
              ;; so let's skip over it
              (search-backward "(")
              (goto-char (scan-sexps (point) 1)))))
        ;; Highlight aguments in the USAGE string
        (setq usage (help-do-arg-highlight (buffer-string) args))
        ;; Highlight arguments in the DOC string
        (setq doc (and doc (help-do-arg-highlight doc args))))))
  ;; Return value is like the one from help-split-fundoc, but highlighted
  (cons usage doc))

;;;###autoload
(defun describe-simplify-lib-file-name (file)
  "Simplify a library name FILE to a relative name, and make it a source file."
  (if file
      ;; Try converting the absolute file name to a library name.
      (let ((libname (file-name-nondirectory file)))
	;; Now convert that back to a file name and see if we get
	;; the original one.  If so, they are equivalent.
	(if (equal file (locate-file libname load-path '("")))
	    (if (string-match "[.]elc\\'" libname)
		(substring libname 0 -1)
	      libname)
	  file))))

;;;###autoload
(defun describe-function-1 (function)
  (let* ((def (if (symbolp function)
		  (symbol-function function)
		function))
	 file-name string
	 (beg (if (commandp def) "an interactive " "a ")))
    (setq string
	  (cond ((or (stringp def)
		     (vectorp def))
		 "a keyboard macro")
		((subrp def)
		 (if (eq 'unevalled (cdr (subr-arity def)))
		     (concat beg "special form")
		   (concat beg "built-in function")))
		((byte-code-function-p def)
		 (concat beg "compiled Lisp function"))
		((symbolp def)
		 (while (symbolp (symbol-function def))
		   (setq def (symbol-function def)))
		 (format "an alias for `%s'" def))
		((eq (car-safe def) 'lambda)
		 (concat beg "Lisp function"))
		((eq (car-safe def) 'macro)
		 "a Lisp macro")
		((eq (car-safe def) 'autoload)
		 (setq file-name (nth 1 def))
		 (format "%s autoloaded %s"
			 (if (commandp def) "an interactive" "an")
			 (if (eq (nth 4 def) 'keymap) "keymap"
			   (if (nth 4 def) "Lisp macro" "Lisp function"))
			 ))
                ((keymapp def)
                 (let ((is-full nil)
                       (elts (cdr-safe def)))
                   (while elts
                     (if (char-table-p (car-safe elts))
                         (setq is-full t
                               elts nil))
                     (setq elts (cdr-safe elts)))
                   (if is-full
                       "a full keymap"
                     "a sparse keymap")))
		(t "")))
    (princ string)
    (with-current-buffer standard-output
      (save-excursion
	(save-match-data
	  (if (re-search-backward "alias for `\\([^`']+\\)'" nil t)
	      (help-xref-button 1 'help-function def)))))
    (or file-name
	(setq file-name (symbol-file function 'defun)))
    (setq file-name (describe-simplify-lib-file-name file-name))
    (when (equal file-name "loaddefs.el")
      ;; Find the real def site of the preloaded function.
      ;; This is necessary only for defaliases.
      (let ((location
	     (condition-case nil
		 (find-function-search-for-symbol function nil "loaddefs.el")
	       (error nil))))
	(when location
	  (with-current-buffer (car location)
	    (goto-char (cdr location))
	    (when (re-search-backward
		   "^;;; Generated autoloads from \\(.*\\)" nil t)
	      (setq file-name (match-string 1)))))))
    (when (and (null file-name) (subrp def))
      ;; Find the C source file name.
      (setq file-name (if (get-buffer " *DOC*")
			  (help-C-file-name def 'subr)
			'C-source)))
    (when file-name
      (princ " in `")
      ;; We used to add .el to the file name,
      ;; but that's completely wrong when the user used load-file.
      (princ (if (eq file-name 'C-source) "C source code" file-name))
      (princ "'")
      ;; Make a hyperlink to the library.
      (with-current-buffer standard-output
        (save-excursion
	  (re-search-backward "`\\([^`']+\\)'" nil t)
	  (help-xref-button 1 'help-function-def function file-name))))
    (princ ".")
    (terpri)
    (when (commandp function)
      (if (and (eq function 'self-insert-command)
	       (eq (key-binding "a") 'self-insert-command)
	       (eq (key-binding "b") 'self-insert-command)
	       (eq (key-binding "c") 'self-insert-command))
	  (princ "It is bound to many ordinary text characters.\n")
	(let* ((remapped (command-remapping function))
	       (keys (where-is-internal
		      (or remapped function) overriding-local-map nil nil))
	       non-modified-keys)
	  ;; Which non-control non-meta keys run this command?
	  (dolist (key keys)
	    (if (member (event-modifiers (aref key 0)) '(nil (shift)))
		(push key non-modified-keys)))
	  (when remapped
	    (princ "It is remapped to `")
	    (princ (symbol-name remapped))
	    (princ "'"))

	  (when keys
	    (princ (if remapped " which is bound to " "It is bound to "))
	    ;; If lots of ordinary text characters run this command,
	    ;; don't mention them one by one.
	    (if (< (length non-modified-keys) 10)
		(princ (mapconcat 'key-description keys ", "))
	      (dolist (key non-modified-keys)
		(setq keys (delq key keys)))
	      (if keys
		  (progn
		    (princ (mapconcat 'key-description keys ", "))
		    (princ ", and many ordinary text characters"))
		(princ "many ordinary text characters"))))
	  (when (or remapped keys non-modified-keys)
	    (princ ".")
	    (terpri)))))
    (let* ((arglist (help-function-arglist def))
	   (doc (documentation function))
	   (usage (help-split-fundoc doc function)))
      (with-current-buffer standard-output
        ;; If definition is a keymap, skip arglist note.
        (unless (keymapp def)
          (let* ((use (cond
                        (usage (setq doc (cdr usage)) (car usage))
                        ((listp arglist)
                         (format "%S" (help-make-usage function arglist)))
                        ((stringp arglist) arglist)
                        ;; Maybe the arglist is in the docstring of the alias.
                        ((let ((fun function))
                           (while (and (symbolp fun)
                                       (setq fun (symbol-function fun))
                                       (not (setq usage (help-split-fundoc
                                                         (documentation fun)
                                                         function)))))
                           usage)
                         (car usage))
                        ((or (stringp def)
                             (vectorp def))
                         (format "\nMacro: %s" (format-kbd-macro def)))
                        (t "[Missing arglist.  Please make a bug report.]")))
                 (high (help-highlight-arguments use doc)))
            (let ((fill-begin (point)))
	      (insert (car high) "\n")
	      (fill-region fill-begin (point)))
            (setq doc (cdr high))))
        (let ((obsolete (and
                         ;; function might be a lambda construct.
                         (symbolp function)
                         (get function 'byte-obsolete-info))))
          (when obsolete
            (princ "\nThis function is obsolete")
            (when (nth 2 obsolete)
              (insert (format " since %s" (nth 2 obsolete))))
            (insert ";\n"
                    (if (stringp (car obsolete)) (car obsolete)
                      (format "use `%s' instead." (car obsolete)))
                    "\n"))
          (insert "\n"
                  (or doc "Not documented.")))))))


;; Variables

;;;###autoload
(defun variable-at-point (&optional any-symbol)
  "Return the bound variable symbol found at or before point.
Return 0 if there is no such symbol.
If ANY-SYMBOL is non-nil, don't insist the symbol be bound."
  (or (condition-case ()
	  (with-syntax-table emacs-lisp-mode-syntax-table
	    (save-excursion
	      (or (not (zerop (skip-syntax-backward "_w")))
		  (eq (char-syntax (following-char)) ?w)
		  (eq (char-syntax (following-char)) ?_)
		  (forward-sexp -1))
	      (skip-chars-forward "'")
	      (let ((obj (read (current-buffer))))
		(and (symbolp obj) (boundp obj) obj))))
	(error nil))
      (let* ((str (find-tag-default))
	     (sym (if str (intern-soft str))))
	(if (and sym (or any-symbol (boundp sym)))
	    sym
	  (save-match-data
	    (when (and str (string-match "\\`\\W*\\(.*?\\)\\W*\\'" str))
	      (setq sym (intern-soft (match-string 1 str)))
	      (and (or any-symbol (boundp sym)) sym)))))
      0))

;;;###autoload
(defun describe-variable (variable &optional buffer)
  "Display the full documentation of VARIABLE (a symbol).
Returns the documentation as a string, also.
If VARIABLE has a buffer-local value in BUFFER (default to the current buffer),
it is displayed along with the global value."
  (interactive
   (let ((v (variable-at-point))
	 (enable-recursive-minibuffers t)
	 val)
     (setq val (completing-read (if (symbolp v)
				    (format
				     "Describe variable (default %s): " v)
				  "Describe variable: ")
				obarray
				'(lambda (vv)
				   (or (boundp vv)
				       (get vv 'variable-documentation)))
				t nil nil
				(if (symbolp v) (symbol-name v))))
     (list (if (equal val "")
	       v (intern val)))))
  (unless (buffer-live-p buffer) (setq buffer (current-buffer)))
  (if (not (symbolp variable))
      (message "You did not specify a variable")
    (save-excursion
      (let* ((valvoid (not (with-current-buffer buffer (boundp variable))))
	     ;; Extract the value before setting up the output buffer,
	     ;; in case `buffer' *is* the output buffer.
	     (val (unless valvoid (buffer-local-value variable buffer)))
	     val-start-pos)
	(help-setup-xref (list #'describe-variable variable buffer)
			 (interactive-p))
	(with-output-to-temp-buffer (help-buffer)
	  (with-current-buffer buffer
	    (prin1 variable)
	    ;; Make a hyperlink to the library if appropriate.  (Don't
	    ;; change the format of the buffer's initial line in case
	    ;; anything expects the current format.)
	    (let ((file-name (symbol-file variable 'defvar)))
	      (setq file-name (describe-simplify-lib-file-name file-name))
	      (when (equal file-name "loaddefs.el")
		;; Find the real def site of the preloaded variable.
		(let ((location
		       (condition-case nil
			   (find-variable-noselect variable file-name)
			 (error nil))))
		  (when location
		    (with-current-buffer (car location)
		      (when (cdr location)
			(goto-char (cdr location)))
		      (when (re-search-backward
			     "^;;; Generated autoloads from \\(.*\\)" nil t)
			(setq file-name (match-string 1)))))))
	      (when (and (null file-name)
			 (integerp (get variable 'variable-documentation)))
		;; It's a variable not defined in Elisp but in C.
		(setq file-name
		      (if (get-buffer " *DOC*")
			  (help-C-file-name variable 'var)
			'C-source)))
	      (if file-name
		  (progn
		    (princ " is a variable defined in `")
		    (princ (if (eq file-name 'C-source) "C source code" file-name))
		    (princ "'.\n")
		    (with-current-buffer standard-output
		      (save-excursion
			(re-search-backward "`\\([^`']+\\)'" nil t)
			(help-xref-button 1 'help-variable-def
					  variable file-name)))
		    (if valvoid
			(princ "It is void as a variable.")
		      (princ "Its ")))
		(if valvoid
		    (princ " is void as a variable.")
		  (princ "'s "))))
	    (if valvoid
		nil
	      (with-current-buffer standard-output
		(setq val-start-pos (point))
		(princ "value is ")
		(terpri)
		(let ((from (point)))
		  (pp val)
		  ;; Hyperlinks in variable's value are quite frequently
		  ;; inappropriate e.g C-h v <RET> features <RET>
		  ;; (help-xref-on-pp from (point))
		  (if (< (point) (+ from 20))
		      (delete-region (1- from) from)))))
	    (terpri)

	    (when (local-variable-p variable)
	      (princ (format "%socal in buffer %s; "
			     (if (get variable 'permanent-local)
				 "Permanently l" "L")
			     (buffer-name)))
	      (if (not (default-boundp variable))
		  (princ "globally void")
		(let ((val (default-value variable)))
		  (with-current-buffer standard-output
		    (princ "global value is ")
		    (terpri)
		    ;; Fixme: pp can take an age if you happen to
		    ;; ask for a very large expression.  We should
		    ;; probably print it raw once and check it's a
		    ;; sensible size before prettyprinting.  -- fx
		    (let ((from (point)))
		      (pp val)
		      ;; See previous comment for this function.
		      ;; (help-xref-on-pp from (point))
		      (if (< (point) (+ from 20))
			  (delete-region (1- from) from)))))))
	    ;; Add a note for variables that have been make-var-buffer-local.
	    (when (and (local-variable-if-set-p variable)
		       (or (not (local-variable-p variable))
			   (with-temp-buffer
			     (local-variable-if-set-p variable))))
	      (princ "\nAutomatically becomes buffer-local when set in any fashion.\n"))
	    (terpri)

	    ;; If the value is large, move it to the end.
	    (with-current-buffer standard-output
	      (when (> (count-lines (point-min) (point-max)) 10)
		;; Note that setting the syntax table like below
		;; makes forward-sexp move over a `'s' at the end
		;; of a symbol.
		(set-syntax-table emacs-lisp-mode-syntax-table)
		(goto-char val-start-pos)
		;; The line below previously read as
		;; (delete-region (point) (progn (end-of-line) (point)))
		;; which suppressed display of the buffer local value for
		;; large values.
		(when (looking-at "value is") (replace-match ""))
		(save-excursion
		  (insert "\n\nValue:")
		  (set (make-local-variable 'help-button-cache)
		       (point-marker)))
		(insert "value is shown ")
		(insert-button "below"
			       'action help-button-cache
			       'follow-link t
			       'help-echo "mouse-2, RET: show value")
		(insert ".\n")))

 	    ;; Mention if it's an alias
            (let* ((alias (condition-case nil
                             (indirect-variable variable)
                           (error variable)))
                   (obsolete (get variable 'byte-obsolete-variable))
		   (safe-var (get variable 'safe-local-variable))
                   (doc (or (documentation-property variable 'variable-documentation)
                            (documentation-property alias 'variable-documentation))))
              (unless (eq alias variable)
                (princ (format "\nThis variable is an alias for `%s'.\n" alias)))
	      (if (or obsolete safe-var)
		  (terpri))

              (when obsolete
                (princ "This variable is obsolete")
                (if (cdr obsolete) (princ (format " since %s" (cdr obsolete))))
                (princ ";") (terpri)
                (princ (if (stringp (car obsolete)) (car obsolete)
                         (format "use `%s' instead." (car obsolete))))
                (terpri))
	      (when safe-var
		(princ "This variable is safe as a file local variable ")
		(princ "if its value\nsatisfies the predicate ")
		(princ (if (byte-code-function-p safe-var)
			   "which is byte-compiled expression.\n"
			 (format "`%s'.\n" safe-var))))
	      (princ "\nDocumentation:\n")
              (princ (or doc "Not documented as a variable.")))
	    ;; Make a link to customize if this variable can be customized.
	    (if (custom-variable-p variable)
		(let ((customize-label "customize"))
		  (terpri)
		  (terpri)
		  (princ (concat "You can " customize-label " this variable."))
		  (with-current-buffer standard-output
		    (save-excursion
		      (re-search-backward
		       (concat "\\(" customize-label "\\)") nil t)
		      (help-xref-button 1 'help-customize-variable variable)))))
	    (print-help-return-message)
	    (save-excursion
	      (set-buffer standard-output)
	      ;; Return the text we displayed.
	      (buffer-string))))))))


;;;###autoload
(defun describe-syntax (&optional buffer)
  "Describe the syntax specifications in the syntax table of BUFFER.
The descriptions are inserted in a help buffer, which is then displayed.
BUFFER defaults to the current buffer."
  (interactive)
  (setq buffer (or buffer (current-buffer)))
  (help-setup-xref (list #'describe-syntax buffer) (interactive-p))
  (with-output-to-temp-buffer (help-buffer)
    (let ((table (with-current-buffer buffer (syntax-table))))
      (with-current-buffer standard-output
	(describe-vector table 'internal-describe-syntax-value)
	(while (setq table (char-table-parent table))
	  (insert "\nThe parent syntax table is:")
	  (describe-vector table 'internal-describe-syntax-value))))))

(defun help-describe-category-set (value)
  (insert (cond
	   ((null value) "default")
	   ((char-table-p value) "deeper char-table ...")
	   (t (condition-case err
		  (category-set-mnemonics value)
		(error "invalid"))))))

;;;###autoload
(defun describe-categories (&optional buffer)
  "Describe the category specifications in the current category table.
The descriptions are inserted in a buffer, which is then displayed.
If BUFFER is non-nil, then describe BUFFER's category table instead.
BUFFER should be a buffer or a buffer name."
  (interactive)
  (setq buffer (or buffer (current-buffer)))
  (help-setup-xref (list #'describe-categories buffer) (interactive-p))
  (with-output-to-temp-buffer (help-buffer)
    (let ((table (with-current-buffer buffer (category-table))))
      (with-current-buffer standard-output
	(describe-vector table 'help-describe-category-set)
	(let ((docs (char-table-extra-slot table 0)))
	  (if (or (not (vectorp docs)) (/= (length docs) 95))
	      (insert "Invalid first extra slot in this char table\n")
	    (insert "Meanings of mnemonic characters are:\n")
	    (dotimes (i 95)
	      (let ((elt (aref docs i)))
		(when elt
		  (insert (+ i ?\s) ": " elt "\n"))))
	    (while (setq table (char-table-parent table))
	      (insert "\nThe parent category table is:")
	      (describe-vector table 'help-describe-category-set))))))))

(provide 'help-fns)

;; arch-tag: 9e10331c-ae81-4d13-965d-c4819aaab0b3
;;; help-fns.el ends here
