;;; rmailout.el --- "RMAIL" mail reader for Emacs: output message to a file

;; Copyright (C) 1985, 1987, 1993, 1994, 2001, 2002, 2003, 2004,
;;   2005, 2006, 2007 Free Software Foundation, Inc.

;; Maintainer: FSF
;; Keywords: mail

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

(require 'rmail)
(provide 'rmailout)

;;;###autoload
(defcustom rmail-output-file-alist nil
  "*Alist matching regexps to suggested output Rmail files.
This is a list of elements of the form (REGEXP . NAME-EXP).
The suggestion is taken if REGEXP matches anywhere in the message buffer.
NAME-EXP may be a string constant giving the file name to use,
or more generally it may be any kind of expression that returns
a file name as a string."
  :type '(repeat (cons regexp
		       (choice :value ""
			       (string :tag "File Name")
			       sexp)))
  :group 'rmail-output)

(defun rmail-output-read-rmail-file-name ()
  "Read the file name to use for `rmail-output-to-rmail-file'.
Set `rmail-default-rmail-file' to this name as well as returning it."
  (let ((default-file
	  (let (answer tail)
	    (setq tail rmail-output-file-alist)
	    ;; Suggest a file based on a pattern match.
	    (while (and tail (not answer))
	      (save-excursion
		(set-buffer rmail-buffer)
		(goto-char (point-min))
		(if (re-search-forward (car (car tail)) nil t)
		    (setq answer (eval (cdr (car tail)))))
		(setq tail (cdr tail))))
	    ;; If no suggestions, use same file as last time.
	    (expand-file-name (or answer rmail-default-rmail-file)))))
    (let ((read-file
	   (expand-file-name
	    (read-file-name
	     (concat "Output message to Rmail file (default "
		     (file-name-nondirectory default-file)
		     "): ")
	     (file-name-directory default-file)
	     (abbreviate-file-name default-file))
	    (file-name-directory default-file))))
      ;; If the user enters just a directory,
      ;; use the name within that directory chosen by the default.
      (setq rmail-default-rmail-file
	    (if (file-directory-p read-file)
		(expand-file-name (file-name-nondirectory default-file)
				  read-file)
	      read-file)))))

(defun rmail-output-read-file-name ()
  "Read the file name to use for `rmail-output'.
Set `rmail-default-file' to this name as well as returning it."
  (let ((default-file
	  (let (answer tail)
	    (setq tail rmail-output-file-alist)
	    ;; Suggest a file based on a pattern match.
	    (while (and tail (not answer))
	      (save-excursion
		(goto-char (point-min))
		(if (re-search-forward (car (car tail)) nil t)
		    (setq answer (eval (cdr (car tail)))))
		(setq tail (cdr tail))))
	    ;; If no suggestion, use same file as last time.
	    (or answer rmail-default-file))))
    (let ((read-file
	   (expand-file-name
	    (read-file-name
	     (concat "Output message to Unix mail file (default "
		     (file-name-nondirectory default-file)
		     "): ")
	     (file-name-directory default-file)
	     (abbreviate-file-name default-file))
	    (file-name-directory default-file))))
      (setq rmail-default-file
	    (if (file-directory-p read-file)
		(expand-file-name (file-name-nondirectory default-file)
				  read-file)
	      (expand-file-name
	       (or read-file (file-name-nondirectory default-file))
	       (file-name-directory default-file)))))))

;;; There are functions elsewhere in Emacs that use this function;
;;; look at them before you change the calling method.
;;;###autoload
(defun rmail-output-to-rmail-file (file-name &optional count stay)
  "Append the current message to an Rmail file named FILE-NAME.
If the file does not exist, ask if it should be created.
If file is being visited, the message is appended to the Emacs
buffer visiting that file.
If the file exists and is not an Rmail file, the message is
appended in inbox format, the same way `rmail-output' does it.

The default file name comes from `rmail-default-rmail-file',
which is updated to the name you use in this command.

A prefix argument COUNT says to output that many consecutive messages,
starting with the current one.  Deleted messages are skipped and don't count.

If the optional argument STAY is non-nil, then leave the last filed
message up instead of moving forward to the next non-deleted message."
  (interactive
   (list (rmail-output-read-rmail-file-name)
	 (prefix-numeric-value current-prefix-arg)))
  (or count (setq count 1))
  (setq file-name
	(expand-file-name file-name
			  (file-name-directory rmail-default-rmail-file)))
  (if (and (file-readable-p file-name) (not (mail-file-babyl-p file-name)))
      (rmail-output file-name count)
    (rmail-maybe-set-message-counters)
    (setq file-name (abbreviate-file-name file-name))
    (or (find-buffer-visiting file-name)
	(file-exists-p file-name)
	(if (yes-or-no-p
	     (concat "\"" file-name "\" does not exist, create it? "))
	    (let ((file-buffer (create-file-buffer file-name)))
	      (save-excursion
		(set-buffer file-buffer)
		(rmail-insert-rmail-file-header)
		(let ((require-final-newline nil)
		      (coding-system-for-write
		       (or rmail-file-coding-system
			   'emacs-mule-unix)))
		  (write-region (point-min) (point-max) file-name t 1)))
	      (kill-buffer file-buffer))
	  (error "Output file does not exist")))
    (while (> count 0)
      (let (redelete)
	(unwind-protect
	    (progn
	      (set-buffer rmail-buffer)
	      ;; Temporarily turn off Deleted attribute.
	      ;; Do this outside the save-restriction, since it would
	      ;; shift the place in the buffer where the visible text starts.
	      (if (rmail-message-deleted-p rmail-current-message)
		  (progn (setq redelete t)
			 (rmail-set-attribute "deleted" nil)))
	      (save-restriction
		(widen)
		;; Decide whether to append to a file or to an Emacs buffer.
		(save-excursion
		  (let ((buf (find-buffer-visiting file-name))
			(cur (current-buffer))
			(beg (1+ (rmail-msgbeg rmail-current-message)))
			(end (1+ (rmail-msgend rmail-current-message)))
			(coding-system-for-write
			 (or rmail-file-coding-system
			     'emacs-mule-unix)))
		    (if (not buf)
			;; Output to a file.
			(if rmail-fields-not-to-output
			    ;; Delete some fields while we output.
			    (let ((obuf (current-buffer)))
			      (set-buffer (get-buffer-create " rmail-out-temp"))
			      (insert-buffer-substring obuf beg end)
			      (rmail-delete-unwanted-fields)
			      (append-to-file (point-min) (point-max) file-name)
			      (set-buffer obuf)
			      (kill-buffer (get-buffer " rmail-out-temp")))
			  (append-to-file beg end file-name))
		      (if (eq buf (current-buffer))
			  (error "Can't output message to same file it's already in"))
		      ;; File has been visited, in buffer BUF.
		      (set-buffer buf)
		      (let ((buffer-read-only nil)
			    (msg (and (boundp 'rmail-current-message)
				      rmail-current-message)))
			;; If MSG is non-nil, buffer is in RMAIL mode.
			(if msg
			    (progn
			      ;; Turn on auto save mode, if it's off in this
			      ;; buffer but enabled by default.
			      (and (not buffer-auto-save-file-name)
				   auto-save-default
				   (auto-save-mode t))
			      (rmail-maybe-set-message-counters)
			      (widen)
			      (narrow-to-region (point-max) (point-max))
			      (insert-buffer-substring cur beg end)
			      (goto-char (point-min))
			      (widen)
			      (search-backward "\n\^_")
			      (narrow-to-region (point) (point-max))
			      (rmail-delete-unwanted-fields)
			      (rmail-count-new-messages t)
			      (if (rmail-summary-exists)
				  (rmail-select-summary
				    (rmail-update-summary)))
			      (rmail-show-message msg))
			  ;; Output file not in rmail mode => just insert at the end.
			  (narrow-to-region (point-min) (1+ (buffer-size)))
			  (goto-char (point-max))
			  (insert-buffer-substring cur beg end)
			  (rmail-delete-unwanted-fields)))))))
	      (rmail-set-attribute "filed" t))
	  (if redelete (rmail-set-attribute "deleted" t))))
      (setq count (1- count))
      (if rmail-delete-after-output
	  (unless
	      (if (and (= count 0) stay)
		  (rmail-delete-message)
		(rmail-delete-forward))
	    (setq count 0))
	(if (> count 0)
	    (unless
		(if (not stay) (rmail-next-undeleted-message 1))
	      (setq count 0)))))))

;;;###autoload
(defcustom rmail-fields-not-to-output nil
  "*Regexp describing fields to exclude when outputting a message to a file."
  :type '(choice (const :tag "None" nil)
		 regexp)
  :group 'rmail-output)

;; Delete from the buffer header fields we don't want output.
;; NOT-RMAIL if t means this buffer does not have the full header
;; and *** EOOH *** that a message in an Rmail file has.
(defun rmail-delete-unwanted-fields (&optional not-rmail)
  (if rmail-fields-not-to-output
      (save-excursion
	(goto-char (point-min))
	;; Find the end of the header.
	(if (and (or not-rmail (search-forward "\n*** EOOH ***\n" nil t))
		 (search-forward "\n\n" nil t))
	    (let ((end (point-marker)))
	      (goto-char (point-min))
	      (while (re-search-forward rmail-fields-not-to-output end t)
		(beginning-of-line)
		(delete-region (point)
			       (progn (forward-line 1) (point)))))))))

;;; There are functions elsewhere in Emacs that use this function;
;;; look at them before you change the calling method.
;;;###autoload
(defun rmail-output (file-name &optional count noattribute from-gnus)
  "Append this message to system-inbox-format mail file named FILE-NAME.
A prefix argument COUNT says to output that many consecutive messages,
starting with the current one.  Deleted messages are skipped and don't count.
When called from lisp code, COUNT may be omitted and defaults to 1.

If the pruned message header is shown on the current message, then
messages will be appended with pruned headers; otherwise, messages
will be appended with their original headers.

The default file name comes from `rmail-default-file',
which is updated to the name you use in this command.

The optional third argument NOATTRIBUTE, if non-nil, says not
to set the `filed' attribute, and not to display a message.

The optional fourth argument FROM-GNUS is set when called from GNUS."
  (interactive
   (list (rmail-output-read-file-name)
	 (prefix-numeric-value current-prefix-arg)))
  (or count (setq count 1))
  (setq file-name
	(expand-file-name file-name
			  (and rmail-default-file
			       (file-name-directory rmail-default-file))))
  (if (and (file-readable-p file-name) (mail-file-babyl-p file-name))
      (rmail-output-to-rmail-file file-name count)
    (set-buffer rmail-buffer)
    (let ((orig-count count)
	  (rmailbuf (current-buffer))
	  (case-fold-search t)
	  (tembuf (get-buffer-create " rmail-output"))
	  (original-headers-p
	   (and (not from-gnus)
		(save-excursion
		  (save-restriction
		    (narrow-to-region (rmail-msgbeg rmail-current-message) (point-max))
		    (goto-char (point-min))
		    (forward-line 1)
		    (= (following-char) ?0)))))
	  header-beginning
	  mail-from mime-version content-type)
      (while (> count 0)
	;; Preserve the Mail-From and MIME-Version fields
	;; even if they have been pruned.
	(or from-gnus
	    (save-excursion
	      (save-restriction
		(widen)
		(goto-char (rmail-msgbeg rmail-current-message))
		(setq header-beginning (point))
		(search-forward "\n*** EOOH ***\n")
		(narrow-to-region header-beginning (point))
		(setq mail-from (mail-fetch-field "Mail-From"))
		(unless rmail-enable-mime
		  (setq mime-version (mail-fetch-field "MIME-Version")
			content-type (mail-fetch-field "Content-type"))))))
	(save-excursion
	  (set-buffer tembuf)
	  (erase-buffer)
	  (insert-buffer-substring rmailbuf)
	  (when rmail-enable-mime
	    (if original-headers-p
		(delete-region (goto-char (point-min))
			       (if (search-forward "\n*** EOOH ***\n")
				   (match-end 0)))
	      (goto-char (point-min))
	      (forward-line 2)
	      (delete-region (point-min)(point))
	      (search-forward "\n*** EOOH ***\n")
	      (delete-region (match-beginning 0)
			     (if (search-forward "\n\n")
				 (1- (match-end 0)))))
	    (setq buffer-file-coding-system (or rmail-file-coding-system
						'raw-text)))
	  (rmail-delete-unwanted-fields t)
	  (or (bolp) (insert "\n"))
	  (goto-char (point-min))
	  (if mail-from
	      (insert mail-from "\n")
	    (insert "From "
		    (mail-strip-quoted-names (or (mail-fetch-field "from")
						 (mail-fetch-field "really-from")
						 (mail-fetch-field "sender")
						 "unknown"))
		    " " (current-time-string) "\n"))
	  (when mime-version
	    (insert "MIME-Version: " mime-version)
	    ;; Some malformed MIME messages set content-type to nil.
	    (when content-type
	      (insert "\nContent-type: " content-type "\n")))
	  ;; ``Quote'' "\nFrom " as "\n>From "
	  ;;  (note that this isn't really quoting, as there is no requirement
	  ;;   that "\n[>]+From " be quoted in the same transparent way.)
	  (let ((case-fold-search nil))
	    (while (search-forward "\nFrom " nil t)
	      (forward-char -5)
	      (insert ?>)))
	  (write-region (point-min) (point-max) file-name t
			(if noattribute 'nomsg)))
	(or noattribute
	    (if (equal major-mode 'rmail-mode)
		(rmail-set-attribute "filed" t)))
	(setq count (1- count))
	(or from-gnus
	    (let ((next-message-p
		   (if rmail-delete-after-output
		       (rmail-delete-forward)
		     (if (> count 0)
			 (rmail-next-undeleted-message 1))))
		  (num-appended (- orig-count count)))
	      (if (and next-message-p original-headers-p)
		  (rmail-toggle-header))
	      (if (and (> count 0) (not next-message-p))
		  (progn
		    (error
		     (save-excursion
		       (set-buffer rmailbuf)
		       (format "Only %d message%s appended" num-appended
			       (if (= num-appended 1) "" "s"))))
		    (setq count 0))))))
      (kill-buffer tembuf))))

;;;###autoload
(defun rmail-output-body-to-file (file-name)
  "Write this message body to the file FILE-NAME.
FILE-NAME defaults, interactively, from the Subject field of the message."
  (interactive
   (let ((default-file
	   (or (mail-fetch-field "Subject")
	       rmail-default-body-file)))
     (list (setq rmail-default-body-file
		 (read-file-name
		  "Output message body to file: "
		  (and default-file (file-name-directory default-file))
		  default-file
		  nil default-file)))))
  (setq file-name
	(expand-file-name file-name
			  (and rmail-default-body-file
			       (file-name-directory rmail-default-body-file))))
  (save-excursion
    (goto-char (point-min))
    (search-forward "\n\n")
    (and (file-exists-p file-name)
	 (not (y-or-n-p (format "File %s exists; overwrite? " file-name)))
	 (error "Operation aborted"))
    (write-region (point) (point-max) file-name)
    (if (equal major-mode 'rmail-mode)
	(rmail-set-attribute "stored" t)))
  (if rmail-delete-after-output
      (rmail-delete-forward)))

;;; arch-tag: 447117c6-1a9a-4b88-aa43-3101b043e3a4
;;; rmailout.el ends here
