;;; view.el --- peruse file or buffer without editing

;; Copyright (C) 1985, 1989, 1994, 1995, 1997, 2000, 2001, 2002,
;;   2003, 2004, 2005, 2006, 2007 Free Software Foundation, Inc.

;; Author: K. Shane Hartman
;; Maintainer: Inge Frick <inge@nada.kth.se>
;; Keywords: files

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

;; This package provides the `view' minor mode documented in the Emacs
;; user's manual.
;; View mode entry and exit is done through the functions view-mode-enter
;; and view-mode-exit.  Use these functions to enter or exit view-mode from
;; emacs lisp programs.
;; We use both view- and View- as prefix for symbols.  View- is used as
;; prefix for commands that have a key binding.  view- is used for commands
;; without key binding.  The purpose of this is to make it easier for a
;; user to use command name completion.

;;; Suggested key bindings:
;;
;; (define-key ctl-x-4-map "v" 'view-file-other-window)  ; ^x4v
;; (define-key ctl-x-5-map "v" 'view-file-other-frame)  ; ^x5v
;;
;; You could also bind view-file, view-buffer, view-buffer-other-window and
;; view-buffer-other-frame to keys.

;;; Code:

(defgroup view nil
  "Peruse file or buffer without editing."
  :link '(function-link view-mode)
  :link '(custom-manual "(emacs)Misc File Ops")
  :group 'wp
  :group 'editing)

(defcustom view-highlight-face 'highlight
   "*The face used for highlighting the match found by View mode search."
   :type 'face
   :group 'view)

;; `view-mode-auto-exit' is replaced by the following option variable which
;; only says if scrolling past buffer end should leave view mode or not, it
;; doesn't say if leaving view mode should restore windows or not.  The latter
;; is now controlled by the presence of a value in `view-return-to-alist'.
(defcustom view-scroll-auto-exit nil
  "*Non-nil means scrolling past the end of buffer exits View mode.
A value of nil means attempting to scroll past the end of the buffer,
only rings the bell and gives a message on how to leave."
  :type 'boolean
  :group 'view)

(defcustom view-try-extend-at-buffer-end nil
 "*Non-nil means try to load more of file when reaching end of buffer.
This variable is mainly intended to be temporarily set to non-nil by
the F command in view-mode, but you can set it to t if you want the action
for all scroll commands in view mode."
  :type 'boolean
  :group 'view)

(defcustom view-remove-frame-by-deleting nil
  "*Determine how View mode removes a frame no longer needed.
If nil, make an icon of the frame.  If non-nil, delete the frame."
  :type 'boolean
  :group 'view)

(defcustom view-exits-all-viewing-windows nil
  "*Non-nil means restore all windows used to view buffer.
Commands that restore windows when finished viewing a buffer, apply to all
windows that display the buffer and have restore information in
`view-return-to-alist'.
If `view-exits-all-viewing-windows' is nil, only the selected window is
considered for restoring."
  :type 'boolean
  :group 'view)

(defcustom view-inhibit-help-message nil
  "*Non-nil inhibits the help message shown upon entering View mode."
  :type 'boolean
  :group 'view
  :version "22.1")

;;;###autoload
(defvar view-mode nil
  "Non-nil if View mode is enabled.
Don't change this variable directly, you must change it by one of the
functions that enable or disable view mode.")
;;;###autoload
(make-variable-buffer-local 'view-mode)

(defcustom view-mode-hook nil
  "Normal hook run when starting to view a buffer or file."
  :type 'hook
  :group 'view)

(defvar view-old-buffer-read-only nil)
(make-variable-buffer-local 'view-old-buffer-read-only)

(defvar view-old-Helper-return-blurb)
(make-variable-buffer-local 'view-old-Helper-return-blurb)

;; Just to avoid warnings.
(defvar Helper-return-blurb)

(defvar view-page-size nil
  "Default number of lines to scroll by View page commands.
If nil that means use the window size.")
(make-variable-buffer-local 'view-page-size)

(defvar view-half-page-size nil
  "Default number of lines to scroll by View half page commands.
If nil that means use half the window size.")
(make-variable-buffer-local 'view-half-page-size)

(defvar view-last-regexp nil)
(make-variable-buffer-local 'view-last-regexp) ; Global is better???

(defvar view-return-to-alist nil
  "What to do with used windows and where to go when finished viewing buffer.
This is local in each buffer being viewed.
It is added to by `view-mode-enter' when starting to view a buffer and
subtracted from by `view-mode-exit' when finished viewing the buffer.

See RETURN-TO-ALIST argument of function `view-mode-exit' for the format of
`view-return-to-alist'.")
(make-variable-buffer-local 'view-return-to-alist)
(put 'view-return-to-alist 'permanent-local t)

(defvar view-exit-action nil
  "nil or a function with one argument (a buffer) called when finished viewing.
This is local in each buffer being viewed.
The \\[view-file] and \\[view-file-other-window] commands may set this to
`kill-buffer'.")
(make-variable-buffer-local 'view-exit-action)

(defvar view-no-disable-on-exit nil
  "If non-nil, View mode \"exit\" commands don't actually disable View mode.
Instead, these commands just switch buffers or windows.
This is set in certain buffers by specialized features such as help commands
that use View mode automatically.")

(defvar view-overlay nil
  "Overlay used to display where a search operation found its match.
This is local in each buffer, once it is used.")
(make-variable-buffer-local 'view-overlay)

(unless (assq 'view-mode minor-mode-alist)
  (setq minor-mode-alist
	(cons (list 'view-mode
		    (propertize " View"
				'local-map mode-line-minor-mode-keymap
				'help-echo "mouse-3: minor mode menu"))
	      minor-mode-alist)))

;; Define keymap inside defvar to make it easier to load changes.
;; Some redundant "less"-like key bindings below have been commented out.
(defvar view-mode-map
  (let ((map (make-sparse-keymap)))
    (define-key map "C" 'View-kill-and-leave)
    (define-key map "c" 'View-leave)
    (define-key map "Q" 'View-quit-all)
    (define-key map "E" 'View-exit-and-edit)
;    (define-key map "v" 'View-exit)
    (define-key map "e" 'View-exit)
    (define-key map "q" 'View-quit)
;    (define-key map "N" 'View-search-last-regexp-backward)
    (define-key map "p" 'View-search-last-regexp-backward)
    (define-key map "n" 'View-search-last-regexp-forward)
;    (define-key map "?" 'View-search-regexp-backward) ; Less does this.
    (define-key map "\\" 'View-search-regexp-backward)
    (define-key map "/" 'View-search-regexp-forward)
    (define-key map "r" 'isearch-backward)
    (define-key map "s" 'isearch-forward)
    (define-key map "m" 'point-to-register)
    (define-key map "'" 'register-to-point)
    (define-key map "x" 'exchange-point-and-mark)
    (define-key map "@" 'View-back-to-mark)
    (define-key map "." 'set-mark-command)
    (define-key map "%" 'View-goto-percent)
;    (define-key map "G" 'View-goto-line-last)
    (define-key map "g" 'View-goto-line)
    (define-key map "=" 'what-line)
    (define-key map "F" 'View-revert-buffer-scroll-page-forward)
;    (define-key map "k" 'View-scroll-line-backward)
    (define-key map "y" 'View-scroll-line-backward)
;    (define-key map "j" 'View-scroll-line-forward)
    (define-key map "\n" 'View-scroll-line-forward)
    (define-key map "\r" 'View-scroll-line-forward)
    (define-key map "u" 'View-scroll-half-page-backward)
    (define-key map "d" 'View-scroll-half-page-forward)
    (define-key map "z" 'View-scroll-page-forward-set-page-size)
    (define-key map "w" 'View-scroll-page-backward-set-page-size)
;    (define-key map "b" 'View-scroll-page-backward)
    (define-key map "\C-?" 'View-scroll-page-backward)
;    (define-key map "f" 'View-scroll-page-forward)
    (define-key map " " 'View-scroll-page-forward)
    (define-key map "o" 'View-scroll-to-buffer-end)
    (define-key map ">" 'end-of-buffer)
    (define-key map "<" 'beginning-of-buffer)
    (define-key map "-" 'negative-argument)
    (define-key map "9" 'digit-argument)
    (define-key map "8" 'digit-argument)
    (define-key map "7" 'digit-argument)
    (define-key map "6" 'digit-argument)
    (define-key map "5" 'digit-argument)
    (define-key map "4" 'digit-argument)
    (define-key map "3" 'digit-argument)
    (define-key map "2" 'digit-argument)
    (define-key map "1" 'digit-argument)
    (define-key map "0" 'digit-argument)
    (define-key map "H" 'describe-mode)
    (define-key map "?" 'describe-mode)	; Maybe do as less instead? See above.
    (define-key map "h" 'describe-mode)
    map))

(or (assq 'view-mode minor-mode-map-alist)
    (setq minor-mode-map-alist
	  (cons (cons 'view-mode view-mode-map) minor-mode-map-alist)))

;;; Commands that enter or exit view mode.

;;;###autoload
(defun view-file (file)
  "View FILE in View mode, returning to previous buffer when done.
Emacs commands editing the buffer contents are not available; instead,
a special set of commands (mostly letters and punctuation)
are defined for moving around in the buffer.
Space scrolls forward, Delete scrolls backward.
For list of all View commands, type H or h while viewing.

This command runs the normal hook `view-mode-hook'."
  (interactive "fView file: ")
  (unless (file-exists-p file) (error "%s does not exist" file))
  (let ((had-a-buf (get-file-buffer file))
	(buffer (find-file-noselect file)))
    (if (eq (with-current-buffer buffer
	      (get major-mode 'mode-class))
	    'special)
	(progn
	  (switch-to-buffer buffer)
	  (message "Not using View mode because the major mode is special"))
      (view-buffer buffer (and (not had-a-buf) 'kill-buffer)))))

;;;###autoload
(defun view-file-other-window (file)
  "View FILE in View mode in another window.
Return that window to its previous buffer when done.
Emacs commands editing the buffer contents are not available; instead,
a special set of commands (mostly letters and punctuation)
are defined for moving around in the buffer.
Space scrolls forward, Delete scrolls backward.
For list of all View commands, type H or h while viewing.

This command runs the normal hook `view-mode-hook'."
  (interactive "fIn other window view file: ")
  (unless (file-exists-p file) (error "%s does not exist" file))
  (let ((had-a-buf (get-file-buffer file)))
    (view-buffer-other-window (find-file-noselect file) nil
			      (and (not had-a-buf) 'kill-buffer))))

;;;###autoload
(defun view-file-other-frame (file)
  "View FILE in View mode in another frame.
Maybe delete other frame and/or return to previous buffer when done.
Emacs commands editing the buffer contents are not available; instead,
a special set of commands (mostly letters and punctuation)
are defined for moving around in the buffer.
Space scrolls forward, Delete scrolls backward.
For list of all View commands, type H or h while viewing.

This command runs the normal hook `view-mode-hook'."
  (interactive "fIn other frame view file: ")
  (unless (file-exists-p file) (error "%s does not exist" file))
  (let ((had-a-buf (get-file-buffer file)))
    (view-buffer-other-frame (find-file-noselect file) nil
			     (and (not had-a-buf) 'kill-buffer))))


;;;###autoload
(defun view-buffer (buffer &optional exit-action)
  "View BUFFER in View mode, returning to previous buffer when done.
Emacs commands editing the buffer contents are not available; instead,
a special set of commands (mostly letters and punctuation)
are defined for moving around in the buffer.
Space scrolls forward, Delete scrolls backward.
For list of all View commands, type H or h while viewing.

This command runs the normal hook `view-mode-hook'.

Optional argument EXIT-ACTION is either nil or a function with buffer as
argument.  This function is called when finished viewing buffer.
Use this argument instead of explicitly setting `view-exit-action'."

  (interactive "bView buffer: ")
  (let ((undo-window (list (window-buffer) (window-start) (window-point))))
    (switch-to-buffer buffer)
    (view-mode-enter (cons (selected-window) (cons nil undo-window))
		     exit-action)))

;;;###autoload
(defun view-buffer-other-window (buffer &optional not-return exit-action)
  "View BUFFER in View mode in another window.
Return to previous buffer when done, unless optional NOT-RETURN is non-nil.
Emacs commands editing the buffer contents are not available; instead,
a special set of commands (mostly letters and punctuation)
are defined for moving around in the buffer.
Space scrolls forward, Delete scrolls backward.
For list of all View commands, type H or h while viewing.

This command runs the normal hook `view-mode-hook'.

Optional argument EXIT-ACTION is either nil or a function with buffer as
argument.  This function is called when finished viewing buffer.
Use this argument instead of explicitly setting `view-exit-action'."
  (interactive "bIn other window view buffer:\nP")
  (let* ((win				; This window will be selected by
	  (get-lru-window))		; switch-to-buffer-other-window below.
	 (return-to
	  (and (not not-return)
	       (cons (selected-window)
		     (if (eq win (selected-window))
			 t			; Has to make new window.
		       (list
			(window-buffer win)	; Other windows old buffer.
			(window-start win)
			(window-point win)))))))
    (switch-to-buffer-other-window buffer)
    (view-mode-enter (and return-to (cons (selected-window) return-to))
		     exit-action)))

;;;###autoload
(defun view-buffer-other-frame (buffer &optional not-return exit-action)
  "View BUFFER in View mode in another frame.
Return to previous buffer when done, unless optional NOT-RETURN is non-nil.
Emacs commands editing the buffer contents are not available; instead,
a special set of commands (mostly letters and punctuation)
are defined for moving around in the buffer.
Space scrolls forward, Delete scrolls backward.
For list of all View commands, type H or h while viewing.

This command runs the normal hook `view-mode-hook'.

Optional argument EXIT-ACTION is either nil or a function with buffer as
argument.  This function is called when finished viewing buffer.
Use this argument instead of explicitly setting `view-exit-action'."
  (interactive "bView buffer in other frame: \nP")
  (let ((return-to
	 (and (not not-return) (cons (selected-window) t)))) ; Old window.
    (switch-to-buffer-other-frame buffer)
    (view-mode-enter (and return-to (cons (selected-window) return-to))
		     exit-action)))

;;;###autoload
(defun view-mode (&optional arg)
  ;; In the following documentation string we have to use some explicit key
  ;; bindings instead of using the \\[] construction.  The reason for this
  ;; is that most commands have more than one key binding.
  "Toggle View mode, a minor mode for viewing text but not editing it.
With ARG, turn View mode on iff ARG is positive.

Emacs commands that do not change the buffer contents are available as usual.
Kill commands insert text in kill buffers but do not delete.  Other commands
\(among them most letters and punctuation) beep and tell that the buffer is
read-only.
\\<view-mode-map>
The following additional commands are provided.  Most commands take prefix
arguments.  Page commands default to \"page size\" lines which is almost a whole
window full, or number of lines set by \\[View-scroll-page-forward-set-page-size] or \\[View-scroll-page-backward-set-page-size].  Half page commands default to
and set \"half page size\" lines which initially is half a window full.  Search
commands default to a repeat count of one.

H, h, ?	 This message.
Digits	provide prefix arguments.
\\[negative-argument]	negative prefix argument.
\\[beginning-of-buffer]	move to the beginning of buffer.
>	move to the end of buffer.
\\[View-scroll-to-buffer-end]	scroll so that buffer end is at last line of window.
SPC	scroll forward \"page size\" lines.
	  With prefix scroll forward prefix lines.
DEL	scroll backward \"page size\" lines.
	  With prefix scroll backward prefix lines.
\\[View-scroll-page-forward-set-page-size]	like  \\[View-scroll-page-forward]  but with prefix sets \"page size\" to prefix.
\\[View-scroll-page-backward-set-page-size]	like  \\[View-scroll-page-backward]  but with prefix sets \"page size\" to prefix.
\\[View-scroll-half-page-forward]	scroll forward \"half page size\" lines.  With prefix, sets
	  \"half page size\" to prefix lines and scrolls forward that much.
\\[View-scroll-half-page-backward]	scroll backward \"half page size\" lines.  With prefix, sets
	  \"half page size\" to prefix lines and scrolls backward that much.
RET, LFD  scroll forward one line.  With prefix scroll forward prefix line(s).
y	scroll backward one line.  With prefix scroll backward prefix line(s).
\\[View-revert-buffer-scroll-page-forward]	revert-buffer if necessary and scroll forward.
	  Use this to view a changing file.
\\[what-line]	prints the current line number.
\\[View-goto-percent]	goes prefix argument (default 100) percent into buffer.
\\[View-goto-line]	goes to line given by prefix argument (default first line).
.	set the mark.
x	exchanges point and mark.
\\[View-back-to-mark]	return to mark and pops mark ring.
	  Mark ring is pushed at start of every successful search and when
	  jump to line occurs.  The mark is set on jump to buffer start or end.
\\[point-to-register]	save current position in character register.
'	go to position saved in character register.
s	do forward incremental search.
r	do reverse incremental search.
\\[View-search-regexp-forward]	searches forward for regular expression, starting after current page.
	  ! and @ have a special meaning at the beginning of the regexp.
	  ! means search for a line with no match for regexp.  @ means start
	  search at beginning (end for backward search) of buffer.
\\	searches backward for regular expression, starting before current page.
\\[View-search-last-regexp-forward]	searches forward for last regular expression.
p	searches backward for last regular expression.
\\[View-quit]	quit View mode, restoring this window and buffer to previous state.
	  \\[View-quit] is the normal way to leave view mode.
\\[View-exit]	exit View mode but stay in current buffer.  Use this if you started
	  viewing a buffer (file) and find out you want to edit it.
	  This command restores the previous read-only status of the buffer.
\\[View-exit-and-edit]	exit View mode, and make the current buffer editable
	  even if it was not editable before entry to View mode.
\\[View-quit-all]	quit View mode, restoring all windows to previous state.
\\[View-leave]	quit View mode and maybe switch buffers, but don't kill this buffer.
\\[View-kill-and-leave]	quit View mode, kill current buffer and go back to other buffer.

The effect of \\[View-leave], \\[View-quit] and \\[View-kill-and-leave] depends on how view-mode was entered.  If it was
entered by view-file, view-file-other-window, view-file-other-frame, or
\\[dired-view-file] \(\\[view-file], \\[view-file-other-window],
\\[view-file-other-frame], or the Dired mode v command),
then \\[View-quit] will try to kill the current buffer.
If view-mode was entered from another buffer, by \\[view-buffer],
\\[view-buffer-other-window], \\[view-buffer-other frame], \\[view-file],
\\[view-file-other-window], or \\[view-file-other-frame],
then \\[View-leave], \\[View-quit] and \\[View-kill-and-leave] will return to that buffer.

Entry to view-mode runs the normal hook `view-mode-hook'."
  (interactive "P")
  (unless (and arg			; Do nothing if already OK.
	       (if (> (prefix-numeric-value arg) 0) view-mode (not view-mode)))
    (if view-mode (view-mode-disable)
      (view-mode-enable))))

(defun view-mode-enable ()
  "Turn on View mode."
  ;; Always leave view mode before changing major mode.
  ;; This is to guarantee that the buffer-read-only variable is restored.
  (add-hook 'change-major-mode-hook 'view-mode-disable nil t)
  (setq view-mode t
	view-page-size nil
	view-half-page-size nil
	view-old-buffer-read-only buffer-read-only
	buffer-read-only t
	view-old-Helper-return-blurb (and (boundp 'Helper-return-blurb)
					  Helper-return-blurb)
	Helper-return-blurb
	(format "continue viewing %s"
		(if (buffer-file-name)
		    (file-name-nondirectory (buffer-file-name))
		  (buffer-name))))
  (force-mode-line-update)
  (run-hooks 'view-mode-hook))

(defun view-mode-disable ()
  "Turn off View mode."
  (remove-hook 'change-major-mode-hook 'view-mode-disable t)
  (and view-overlay (delete-overlay view-overlay))
  (force-mode-line-update)
  ;; Calling toggle-read-only while View mode is enabled
  ;; sets view-read-only to t as a buffer-local variable
  ;; after exiting View mode.  That arranges that the next toggle-read-only
  ;; will reenable View mode.
  ;; Cancelling View mode in any other way should cancel that, too,
  ;; so that View mode stays off if toggle-read-only is called.
  (if (local-variable-p 'view-read-only)
      (kill-local-variable 'view-read-only))
  (setq view-mode nil
	Helper-return-blurb view-old-Helper-return-blurb)
  (if buffer-read-only
      (setq buffer-read-only view-old-buffer-read-only)))

;;;###autoload
(defun view-mode-enter (&optional return-to exit-action) "\
Enter View mode and set up exit from view mode depending on optional arguments.
If RETURN-TO is non-nil it is added as an element to the buffer local alist
`view-return-to-alist'.
Save EXIT-ACTION in buffer local variable `view-exit-action'.
It should be either nil or a function that takes a buffer as argument.
This function will be called by `view-mode-exit'.

RETURN-TO is either nil, meaning do nothing when exiting view mode, or
it has the format (WINDOW OLD-WINDOW . OLD-BUF-INFO).
WINDOW is a window used for viewing.
OLD-WINDOW is nil or the window to select after viewing.
OLD-BUF-INFO tells what to do with WINDOW when exiting.  It is one of:
1) nil       Do nothing.
2) t         Delete WINDOW or, if it is the only window, its frame.
3) (OLD-BUFF START POINT)  Display buffer OLD-BUFF with displayed text
                           starting at START and point at POINT in WINDOW.
4) quit-window   Do `quit-window' in WINDOW.

For list of all View commands, type H or h while viewing.

This function runs the normal hook `view-mode-hook'."
  (if return-to
      (let ((entry (assq (car return-to) view-return-to-alist)))
	(if entry (setcdr entry (cdr return-to))
	  (setq view-return-to-alist (cons return-to view-return-to-alist)))))
  (if exit-action (setq view-exit-action exit-action))
  (unless view-mode			; Do nothing if already in view mode.
    (view-mode-enable)
    (force-mode-line-update)
    (unless view-inhibit-help-message
      (message "%s"
	       (substitute-command-keys "\
View mode: type \\[help-command] for help, \\[describe-mode] for commands, \\[View-quit] to quit.")))))

(defun view-mode-exit (&optional return-to-alist exit-action all-win)
  "Exit View mode in various ways, depending on optional arguments.
RETURN-TO-ALIST, EXIT-ACTION and ALL-WIN determine what to do after exit.
EXIT-ACTION is nil or a function that is called with current buffer as
argument.
RETURN-TO-ALIST is an alist that for some of the windows displaying the
current buffer, associate information on what to do with those windows.
If ALL-WIN or the variable `view-exits-all-viewing-windows' is non-nil,
then all windows on RETURN-TO-ALIST are restored to their old state.
Otherwise only the selected window is affected (if it is on RETURN-TO-ALIST).

Elements of RETURN-TO-ALIST have the format (WINDOW OLD-WINDOW . OLD-BUF-INFO).
WINDOW is a window displaying the current buffer.
OLD-WINDOW is nil or a window to select after viewing.
OLD-BUF-INFO is information on what to do with WINDOW and is one of:
1) nil       Do nothing.
2) t         Delete WINDOW and, if it is the only window, its frame.
3) (OLD-BUF START POINT)  Display buffer OLD-BUF with displayed text
                          starting at START and point at POINT in WINDOW.
4) quit-window   Do `quit-window' in WINDOW.

If one of the WINDOW in RETURN-TO-ALIST is the selected window and the
corresponding OLD-WINDOW is a live window, then select OLD-WINDOW."
  (setq all-win
	(and return-to-alist (or all-win view-exits-all-viewing-windows)))
  (if view-mode		; Only do something if in view mode.
      (let* ((buffer (current-buffer))
	     window notlost
	     (sel-old (assq (selected-window) return-to-alist))
	     (alist (cond
		     (all-win		; Try to restore all windows.
		      (append return-to-alist nil)) ; Copy.
		     (sel-old		; Only selected window.
		      (list sel-old))))
	     (old-window (if sel-old (car (cdr sel-old)))))
	(if all-win			; Follow chains of old-windows.
	    (let ((c (length alist)) a)
	      (while (and (> c 0)	; Safety if mutually refering windows.
			  (or (not (window-live-p old-window))
			      (eq buffer (window-buffer old-window)))
			  (setq a (assq old-window alist)))
		(setq c (1- c))
		(setq old-window (car (cdr a))))
	      (if (or (zerop c) (not (window-live-p old-window)))
		  (setq old-window (selected-window)))))
	(or view-no-disable-on-exit
	    (view-mode-disable))
	(while alist			; Restore windows with info.
	  (setq notlost nil)
	  (if (and (window-live-p (setq window (car (car alist))))
		   (eq buffer (window-buffer window)))
	      (let ((frame (window-frame window))
		    (old-buf-info (cdr (cdr (car alist)))))
		(if all-win (select-window window))
		(cond
		 ((and (consp old-buf-info) ; Case 3.
		       (buffer-live-p (car old-buf-info)))
		  (set-window-buffer window (car old-buf-info)) ; old-buf
		  (set-window-start window (car (cdr old-buf-info)))
		  (set-window-point window (car (cdr (cdr old-buf-info)))))
		 ((eq old-buf-info 'quit-window)
		  (quit-window))	; Case 4.
		 ((not (eq old-buf-info t)) nil) ; Not case 2, do nothing.
		 ((not (one-window-p t)) (delete-window))
		 ((not (eq frame (next-frame)))
		  ;; Not the only frame, so can safely be removed.
		  (if view-remove-frame-by-deleting
		      (delete-frame frame)
		    (setq notlost t)	; Keep the window. See below.
		    (iconify-frame frame))))))
	  ;; If a frame is removed by iconifying it, then the window is not
	  ;; really lost.  In this case we keep the entry in
	  ;; view-return-to-alist so that if the user deiconifies the frame
	  ;; and then press q, then the frame is iconified again.
	  (unless notlost
	    (setq view-return-to-alist
		  (delete (car alist) view-return-to-alist)))
	  (setq alist (cdr alist)))
	(if (window-live-p old-window)	; still existing window
	    (select-window old-window))
	(when exit-action
	  (setq view-exit-action nil)
	  (funcall exit-action buffer))
	(force-mode-line-update))))

(defun View-exit ()
  "Exit View mode but stay in current buffer."
  (interactive)
  (view-mode-exit))

;;;###autoload
(defun View-exit-and-edit ()
  "Exit View mode and make the current buffer editable."
  (interactive)
  (let ((view-old-buffer-read-only nil)
	(view-no-disable-on-exit nil))
    (view-mode-exit)))

(defun View-leave ()
  "Quit View mode and maybe switch buffers, but don't kill this buffer."
  (interactive)
  (view-mode-exit view-return-to-alist))

(defun View-quit ()
  "Quit View mode, trying to restore window and buffer to previous state.
Maybe kill this buffer.  Try to restore selected window to previous state
and go to previous buffer or window."
  (interactive)
  (view-mode-exit view-return-to-alist view-exit-action))

(defun View-quit-all ()
  "Quit View mode, trying to restore windows and buffers to previous state.
Maybe kill current buffer.  Try to restore all windows viewing buffer to
previous state and go to previous buffer or window."
  (interactive)
  (view-mode-exit view-return-to-alist view-exit-action t))

(defun View-kill-and-leave ()
  "Quit View mode, kill current buffer and return to previous buffer."
  (interactive)
  (view-mode-exit view-return-to-alist (or view-exit-action 'kill-buffer) t))


;;; Some help routines.

(defun view-window-size ()
  ;; Window height excluding mode line.
  (1- (window-height)))

;(defun view-last-command (&optional who what)
;  (setq view-last-command-entry this-command)
;  (setq view-last-command who)
;  (setq view-last-command-argument what))

;(defun View-repeat-last-command ()
;  "Repeat last command issued in View mode."
;  (interactive)
;  (if (and view-last-command
;	   (eq view-last-command-entry last-command))
;      (funcall view-last-command view-last-command-argument))
;  (setq this-command view-last-command-entry))

(defun view-recenter ()
  ;; Center point in window.
  (recenter (/ (view-window-size) 2)))

(defun view-page-size-default (lines)
  ;; Get page size.
  (let ((default (- (view-window-size) next-screen-context-lines)))
    (if (or (null lines) (zerop (setq lines (prefix-numeric-value lines))))
	default
      (min (abs lines) default))))

(defun view-set-half-page-size-default (lines)
  ;; Get and maybe set half page size.
  (if (not lines) (or view-half-page-size
		      (/ (view-window-size) 2))
    (setq view-half-page-size
	  (if (zerop (setq lines (prefix-numeric-value lines)))
	      (/ (view-window-size) 2)
	    (view-page-size-default lines)))))


;;; Commands for moving around in the buffer.

(defun View-goto-percent (&optional percent)
  "Move to end (or prefix PERCENT) of buffer in View mode.
Display is centered at point.
Also set the mark at the position where point was."
  (interactive "P")
  (push-mark)
  (goto-char
   (if percent
       (+ (point-min)
	  (floor (* (- (point-max) (point-min)) 0.01
		    (max 0 (min 100 (prefix-numeric-value percent))))))
     (point-max)))
  (view-recenter))

;(defun View-goto-line-last (&optional line)
;"Move to last (or prefix LINE) line in View mode.
;Display is centered at LINE.
;Sets mark at starting position and pushes mark ring."
;  (interactive "P")
;  (push-mark)
;  (if line (goto-line (prefix-numeric-value line))
;    (goto-char (point-max))
;    (beginning-of-line))
;  (view-recenter))

(defun View-goto-line (&optional line)
  "Move to first (or prefix LINE) line in View mode.
Display is centered at LINE.
Also set the mark at the position where point was."
  (interactive "p")
  (push-mark)
  (goto-line line)
  (view-recenter))

(defun View-back-to-mark (&optional ignore)
  "Return to last mark set in View mode, else beginning of file.
Display that line at the center of the window.
This command pops the mark ring, so that successive
invocations return to earlier marks."
  (interactive)
  (goto-char (or (mark t) (point-min)))
  (pop-mark)
  (view-recenter))

(defun view-scroll-lines (lines backward default maxdefault)
  ;; This function does the job for all the scrolling commands.
  ;; Scroll forward LINES lines.  If BACKWARD is true scroll backwards.
  ;; If LINES is negative scroll in the other direction.  If LINES is 0 or nil,
  ;; scroll DEFAULT lines.  If MAXDEFAULT is true then scroll no more than a
  ;; window full.
  (if (or (null lines) (zerop (setq lines (prefix-numeric-value lines))))
      (setq lines default))
  (when (< lines 0)
    (setq backward (not backward)) (setq lines (- lines)))
  (setq default (view-page-size-default nil)) ; Max scrolled at a time.
  (if maxdefault (setq lines (min lines default)))
  (cond
   (backward (scroll-down lines))
   ((view-really-at-end)
    (if view-scroll-auto-exit (View-quit)
      (ding)
      (view-end-message)))
   (t (while (> lines default)
	(scroll-up default)
	(setq lines (- lines default))
	(if (view-really-at-end) (setq lines 0)))
      (scroll-up lines)
      (if (view-really-at-end) (view-end-message))
      (move-to-window-line -1)
      (beginning-of-line))))

(defun view-really-at-end ()
  ;; Return true if buffer end visible.  Maybe revert buffer and test.
  (and (pos-visible-in-window-p (point-max))
       (let ((buf (current-buffer))
	     (bufname (buffer-name))
	     (file (buffer-file-name)))
	 (or (not view-try-extend-at-buffer-end)
	     (null file)
	     (verify-visited-file-modtime buf)
	     (not (file-exists-p file))
	     (when (buffer-modified-p buf)
	       (setq file (file-name-nondirectory file))
	       (not (yes-or-no-p
		     (format
		      "File %s changed on disk.  Discard your edits%s? "
		      file
		      (if (string= bufname file) ""
			(concat " in " bufname))))))
	     (progn
	       (revert-buffer t t t)
	       (pos-visible-in-window-p (point-max)))))))

(defun view-end-message ()
  ;; Tell that we are at end of buffer.
  (goto-char (point-max))
  (if view-return-to-alist
      (message "End of buffer.  Type %s to quit viewing."
	       (substitute-command-keys
		(if view-scroll-auto-exit "\\[View-scroll-page-forward]"
		  "\\[View-quit]")))
    (message "End of buffer")))

(defun View-scroll-to-buffer-end ()
  "Scroll backward or forward so that buffer end is at last line of window."
  (interactive)
  (let ((p (if (pos-visible-in-window-p (point-max)) (point))))
    (goto-char (point-max))
    (recenter -1)
    (and p (goto-char p))))

(defun View-scroll-page-forward (&optional lines)
  "Scroll \"page size\" or prefix LINES lines forward in View mode.
Exit if end of text is visible and `view-scroll-auto-exit' is non-nil.
\"page size\" is whole window full, or number of lines set by
\\[View-scroll-page-forward-set-page-size] or
\\[View-scroll-page-backward-set-page-size].
If LINES is more than a window-full, only the last window-full is shown."
  (interactive "P")
  (view-scroll-lines lines nil (view-page-size-default view-page-size) nil))

(defun View-scroll-page-backward (&optional lines)
  "Scroll \"page size\" or prefix LINES lines backward in View mode.
See also `View-scroll-page-forward'."
  (interactive "P")
  (view-scroll-lines lines t (view-page-size-default view-page-size) nil))

(defun View-scroll-page-forward-set-page-size (&optional lines)
  "Scroll forward LINES lines in View mode, setting the \"page size\".
This is the number of lines which \\[View-scroll-page-forward] and
\\[View-scroll-page-backward] scroll by default.
If LINES is omitted or = 0, sets \"page size\" to window height and
scrolls forward that much, otherwise scrolls forward LINES lines and sets
\"page size\" to the minimum of window height and the absolute value of LINES.
See also `View-scroll-page-forward'."
  (interactive "P")
  (view-scroll-lines lines nil
		     (setq view-page-size (view-page-size-default lines))
		     nil))

(defun View-scroll-page-backward-set-page-size (&optional lines)
  "Scroll backward prefix LINES lines in View mode, setting the \"page size\".
See also `View-scroll-page-forward-set-page-size'."
  (interactive "P")
  (view-scroll-lines lines t
		     (setq view-page-size (view-page-size-default lines))
		     nil))

(defun View-scroll-line-forward (&optional lines)
  "Scroll forward one line (or prefix LINES lines) in View mode.
See also `View-scroll-page-forward,' but note that scrolling is limited
to minimum of LINES and one window-full."
  (interactive "P")
  (view-scroll-lines lines nil 1 t))

(defun View-scroll-line-backward (&optional lines)
  "Scroll backward one line (or prefix LINES lines) in View mode.
See also `View-scroll-line-forward'."
  (interactive "P")
  (view-scroll-lines lines t 1 t))

(defun View-scroll-half-page-forward (&optional lines)
  "Scroll forward a \"half page\" (or prefix LINES) lines in View mode.
If LINES is not omitted, the \"half page size\" is set to the minimum of
window height and the absolute value of LINES.
LINES=0 resets \"half page size\" to half window height."
  (interactive "P")
  (view-scroll-lines lines nil (view-set-half-page-size-default lines) t))

(defun View-scroll-half-page-backward (&optional lines)
  "Scroll backward a \"half page\" (or prefix LINES) lines in View mode.
See also `View-scroll-half-page-forward'."
  (interactive "P")
  (view-scroll-lines lines t (view-set-half-page-size-default lines) t))

(defun View-revert-buffer-scroll-page-forward (&optional lines)
  "Scroll forward, reverting buffer if needed, in View mode.
If buffer has not been changed and the corresponding file is newer, first
revert the buffer, then scroll.
This command is useful if you are viewing a changing file.

The prefix argument LINES says how many lines to scroll.
If you don't specify a prefix argument, it uses the number of lines set by
\\[View-scroll-page-forward-set-page-size] or
\\[View-scroll-page-backward-set-page-size].
If LINES is more than a window-full, only the last window-full is shown."
  (interactive "P")
  (let ((view-scroll-auto-exit nil)
	(view-try-extend-at-buffer-end t))
    (view-scroll-lines lines nil (view-page-size-default view-page-size) nil)))

(defun View-search-regexp-forward (n regexp)
  "Search forward for first (or prefix Nth) occurrence of REGEXP in View mode.

Displays line found at center of window.  Sets mark at starting position and
pushes mark ring.

Characters @ and ! are special at the beginning of REGEXP.  They modify
the search rather than become part of the pattern searched for.
@ means search all the buffer i.e. start search at the beginning of buffer.
! means search for a line that contains no match for the pattern.
If REGEXP is empty or only consist of these control characters, then
an earlier remembered REGEXP is used, otherwise REGEXP is remembered
for use by later search commands.

The variable `view-highlight-face' controls the face that is used
for highlighting the match that is found."
  (interactive "p\nsSearch forward (regexp): ")
  (view-search n regexp))

(defun View-search-regexp-backward (n regexp)
  "Search backward for first (or prefix Nth) occurrence of REGEXP in View mode.

Displays line found at center of window.  Sets mark at starting position and
pushes mark ring.

Characters @ and ! are special at the beginning of REGEXP.  They modify
the search rather than become part of the pattern searched for.
@ means search all the buffer i.e. start search at the end of buffer.
! means search for a line that contains no match for the pattern.
If REGEXP is empty or only consist of these control characters, then
an earlier remembered REGEXP is used, otherwise REGEXP is remembered
for use by later search commands.

The variable `view-highlight-face' controls the face that is used
for highlighting the match that is found."
  (interactive "p\nsSearch backward (regexp): ")
  (view-search (- n) regexp))

(defun View-search-last-regexp-forward (n) "\
Search forward for first (or prefix Nth) instance of last regexp in View mode.
Displays line found at center of window.  Sets mark at starting position and
pushes mark ring.

The variable `view-highlight-face' controls the face that is used
for highlighting the match that is found."
  (interactive "p")
  (view-search n nil))

(defun View-search-last-regexp-backward (n) "\
Search backward for first (or prefix Nth) instance of last regexp in View mode.
Displays line found at center of window.  Sets mark at starting position and
pushes mark ring.

The variable `view-highlight-face' controls the face that is used
for highlighting the match that is found."
  (interactive "p")
  (view-search (- n) nil))

(defun view-search (times regexp)
  ;; This function does the job for all the View-search- commands.
  ;; Search for the TIMESt match for REGEXP. If TIMES is negative
  ;; search backwards. If REGEXP is nil use `view-last-regexp'.
  ;; Charcters "!" and "@" have a special meaning at the beginning of
  ;; REGEXP and are removed from REGEXP before the search "!" means
  ;; search for lines with no match for REGEXP.  "@" means search in
  ;; the whole buffer, don't start searching from the present point.
  (let (where no end ln)
    (cond
     ((and regexp (> (length regexp) 0)
	   (or (not (memq (string-to-char regexp) '(?! ?@)))
	       (progn
		 (if (member (substring regexp 0 2) '("!@" "@!"))
		     (setq end t no t ln 2)
		   (setq no (not (setq end (eq ?@ (string-to-char regexp))))
			 ln 1))
		 (> (length (setq regexp (substring regexp ln))) 0))))
      (setq view-last-regexp (if no (list regexp) regexp)))
     ((consp view-last-regexp)
      (setq regexp (car view-last-regexp))
      (unless (setq no (not no)) (setq view-last-regexp regexp)))
     (view-last-regexp (setq regexp view-last-regexp)
		       (if no (setq view-last-regexp (list regexp))))
     (t (error "No previous View-mode search")))
    (save-excursion
      (if end (goto-char (if (< times 0) (point-max) (point-min)))
	(move-to-window-line (if (< times 0) 0 -1)))
      (if (if no (view-search-no-match-lines times regexp)
	    (re-search-forward regexp nil t times))
	  (setq where (point))))
    (if where
	(progn
	  (push-mark)
	  (goto-char where)
	  (if view-overlay
	      (move-overlay view-overlay (match-beginning 0) (match-end 0))
	    (setq view-overlay
		  (make-overlay (match-beginning 0) (match-end 0))))
	  (overlay-put view-overlay 'face view-highlight-face)
	  (beginning-of-line)
	  (view-recenter))
      (message "Can't find occurrence %d of %s%s"
	       times (if no "no " "") regexp)
      (sit-for 4))))

(defun view-search-no-match-lines (times regexp)
  ;; Search for the TIMESt occurrence of line with no match for REGEXP.
  (let ((back (and (< times 0) (setq times (- times)) -1))
	n)
    (while (> times 0)
      (save-excursion (beginning-of-line (if back (- times) (1+ times)))
		      (setq n (point)))
      (setq times
	    (cond
	     ((< (count-lines (point) n) times) -1) ; Not enough lines.
	     ((or (null (re-search-forward regexp nil t back))
		  (if back (and (< (match-end 0) n)
				(> (count-lines (match-end 0) n) 1))
		    (and (< n (match-beginning 0))
			 (> (count-lines n (match-beginning 0)) 1))))
	      0)			; No match within lines.
	     (back (count-lines (max n (match-beginning 0)) (match-end 0)))
	     (t (count-lines (match-beginning 0) (min n (match-end 0))))))
      (goto-char n))
    (and (zerop times) (looking-at "^.*$"))))


(provide 'view)

;;; arch-tag: 6d0ace36-1d12-4de3-8de3-1fa3231636d7
;;; view.el ends here
