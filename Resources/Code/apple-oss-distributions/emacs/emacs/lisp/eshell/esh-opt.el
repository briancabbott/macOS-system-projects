;;; esh-opt.el --- command options processing

;; Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004,
;;   2005, 2006, 2007 Free Software Foundation, Inc.

;; Author: John Wiegley <johnw@gnu.org>

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

(provide 'esh-opt)

(eval-when-compile (require 'esh-maint))

(defgroup eshell-opt nil
  "The options processing code handles command argument parsing for
Eshell commands implemented in Lisp."
  :tag "Command options processing"
  :group 'eshell)

;;; Commentary:

;;; User Functions:

(defmacro eshell-eval-using-options (name macro-args
					  options &rest body-forms)
  "Process NAME's MACRO-ARGS using a set of command line OPTIONS.
After doing so, settings will be stored in local symbols as declared
by OPTIONS; FORMS will then be evaluated -- assuming all was OK.

The syntax of OPTIONS is:

  '((?C  nil         nil multi-column    \"multi-column display\")
    (nil \"help\"      nil nil             \"show this usage display\")
    (?r  \"reverse\"   nil reverse-list    \"reverse order while sorting\")
    :external \"ls\"
    :usage \"[OPTION]... [FILE]...
  List information about the FILEs (the current directory by default).
  Sort entries alphabetically across.\")

`eshell-eval-using-options' returns the value of the last form in
BODY-FORMS.  If instead an external command is run, the tag
`eshell-external' will be thrown with the new process for its value.

Lastly, any remaining arguments will be available in a locally
interned variable `args' (created using a `let' form)."
  `(let ((temp-args
	  ,(if (memq ':preserve-args (cadr options))
	       macro-args
	     (list 'eshell-stringify-list
		   (list 'eshell-flatten-list macro-args)))))
     (let ,(append (mapcar (function
			    (lambda (opt)
			      (or (and (listp opt) (nth 3 opt))
				  'eshell-option-stub)))
			   (cadr options))
		   '(usage-msg last-value ext-command args))
       (eshell-do-opt ,name ,options (quote ,body-forms)))))

;;; Internal Functions:

(eval-when-compile
  (defvar temp-args)
  (defvar last-value)
  (defvar usage-msg)
  (defvar ext-command)
  (defvar args))

(defun eshell-do-opt (name options body-forms)
  "Helper function for `eshell-eval-using-options'.
This code doesn't really need to be macro expanded everywhere."
  (setq args temp-args)
  (if (setq
       ext-command
       (catch 'eshell-ext-command
	 (when (setq
		usage-msg
		(catch 'eshell-usage
		  (setq last-value nil)
		  (if (and (= (length args) 0)
			   (memq ':show-usage options))
		      (throw 'eshell-usage
			     (eshell-show-usage name options)))
		  (setq args (eshell-process-args name args options)
			last-value (eval (append (list 'progn)
						 body-forms)))
		  nil))
	   (error "%s" usage-msg))))
      (throw 'eshell-external
	     (eshell-external-command ext-command args))
    last-value))

(defun eshell-show-usage (name options)
  "Display the usage message for NAME, using OPTIONS."
  (let ((usage (format "usage: %s %s\n\n" name
		       (cadr (memq ':usage options))))
	(extcmd (memq ':external options))
	(post-usage (memq ':post-usage options))
	had-option)
    (while options
      (when (listp (car options))
	(let ((opt (car options)))
	  (setq had-option t)
	  (cond ((and (nth 0 opt)
		      (nth 1 opt))
		 (setq usage
		       (concat usage
			       (format "    %-20s %s\n"
				       (format "-%c, --%s" (nth 0 opt)
					       (nth 1 opt))
				       (nth 4 opt)))))
		((nth 0 opt)
		 (setq usage
		       (concat usage
			       (format "    %-20s %s\n"
				       (format "-%c" (nth 0 opt))
				       (nth 4 opt)))))
		((nth 1 opt)
		 (setq usage
		       (concat usage
			       (format "    %-20s %s\n"
				       (format "    --%s" (nth 1 opt))
				       (nth 4 opt)))))
		(t (setq had-option nil)))))
      (setq options (cdr options)))
    (if post-usage
	(setq usage (concat usage (and had-option "\n")
			    (cadr post-usage))))
    (when extcmd
      (setq extcmd (eshell-search-path (cadr extcmd)))
      (if extcmd
	  (setq usage
		(concat usage
			(format "
This command is implemented in Lisp.  If an unrecognized option is
passed to this command, the external version '%s'
will be called instead." extcmd)))))
    (throw 'eshell-usage usage)))

(defun eshell-set-option (name ai opt options)
  "Using NAME's remaining args (index AI), set the OPT within OPTIONS.
If the option consumes an argument for its value, the argument list
will be modified."
  (if (not (nth 3 opt))
      (eshell-show-usage name options)
    (if (eq (nth 2 opt) t)
	(if (> ai (length args))
	    (error "%s: missing option argument" name)
	  (set (nth 3 opt) (nth ai args))
	  (if (> ai 0)
	      (setcdr (nthcdr (1- ai) args) (nthcdr (1+ ai) args))
	    (setq args (cdr args))))
      (set (nth 3 opt) (or (nth 2 opt) t)))))

(defun eshell-process-option (name switch kind ai options)
  "For NAME, process SWITCH (of type KIND), from args at index AI.
The SWITCH will be looked up in the set of OPTIONS.

SWITCH should be either a string or character.  KIND should be the
integer 0 if it's a character, or 1 if it's a string.

The SWITCH is then be matched against OPTIONS.  If no matching handler
is found, and an :external command is defined (and available), it will
be called; otherwise, an error will be triggered to say that the
switch is unrecognized."
  (let* ((opts options)
	 found)
    (while opts
      (if (and (listp (car opts))
		 (nth kind (car opts))
		 (if (= kind 0)
		     (eq switch (nth kind (car opts)))
		   (string= switch (nth kind (car opts)))))
	  (progn
	    (eshell-set-option name ai (car opts) options)
	    (setq found t opts nil))
	(setq opts (cdr opts))))
    (unless found
      (let ((extcmd (memq ':external options)))
	(when extcmd
	  (setq extcmd (eshell-search-path (cadr extcmd)))
	  (if extcmd
	      (throw 'eshell-ext-command extcmd)
	    (if (char-valid-p switch)
		(error "%s: unrecognized option -%c" name switch)
	      (error "%s: unrecognized option --%s" name switch))))))))

(defun eshell-process-args (name args options)
  "Process the given ARGS using OPTIONS.
This assumes that symbols have been intern'd by `eshell-with-options'."
  (let ((ai 0) arg)
    (while (< ai (length args))
      (setq arg (nth ai args))
      (if (not (and (stringp arg)
		    (string-match "^-\\(-\\)?\\(.*\\)" arg)))
	  (setq ai (1+ ai))
	(let* ((dash (match-string 1 arg))
	       (switch (match-string 2 arg)))
	  (if (= ai 0)
	      (setq args (cdr args))
	    (setcdr (nthcdr (1- ai) args) (nthcdr (1+ ai) args)))
	  (if dash
	      (if (> (length switch) 0)
		  (eshell-process-option name switch 1 ai options)
		(setq ai (length args)))
	    (let ((len (length switch))
		  (index 0))
	      (while (< index len)
		(eshell-process-option name (aref switch index) 0 ai options)
		(setq index (1+ index)))))))))
  args)

;;; Code:

;;; arch-tag: 45c6c2d0-8091-46a1-a205-2f4bafd8230c
;;; esh-opt.el ends here
