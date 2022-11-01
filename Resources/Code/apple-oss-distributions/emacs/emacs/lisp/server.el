;;; server.el --- Lisp code for GNU Emacs running as server process

;; Copyright (C) 1986, 1987, 1992, 1994, 1995, 1996, 1997, 1998, 1999, 2000,
;;   2001, 2002, 2003, 2004, 2005, 2006, 2007 Free Software Foundation, Inc.

;; Author: William Sommerfeld <wesommer@athena.mit.edu>
;; Maintainer: FSF
;; Keywords: processes

;; Changes by peck@sun.com and by rms.

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

;; This Lisp code is run in Emacs when it is to operate as
;; a server for other processes.

;; Load this library and do M-x server-edit to enable Emacs as a server.
;; Emacs opens up a socket for communication with clients.  If there are no
;; client buffers to edit, server-edit acts like (switch-to-buffer
;; (other-buffer))

;; When some other program runs "the editor" to edit a file,
;; "the editor" can be the Emacs client program ../lib-src/emacsclient.
;; This program transmits the file names to Emacs through
;; the server subprocess, and Emacs visits them and lets you edit them.

;; Note that any number of clients may dispatch files to emacs to be edited.

;; When you finish editing a Server buffer, again call server-edit
;; to mark that buffer as done for the client and switch to the next
;; Server buffer.  When all the buffers for a client have been edited
;; and exited with server-edit, the client "editor" will return
;; to the program that invoked it.

;; Your editing commands and Emacs's display output go to and from
;; the terminal in the usual way.  Thus, server operation is possible
;; only when Emacs can talk to the terminal at the time you invoke
;; the client.  This is possible in four cases:

;; 1. On a window system, where Emacs runs in one window and the
;; program that wants to use "the editor" runs in another.

;; 2. On a multi-terminal system, where Emacs runs on one terminal and the
;; program that wants to use "the editor" runs on another.

;; 3. When the program that wants to use "the editor" is running
;; as a subprocess of Emacs.

;; 4. On a system with job control, when Emacs is suspended, the program
;; that wants to use "the editor" will stop and display
;; "Waiting for Emacs...".  It can then be suspended, and Emacs can be
;; brought into the foreground for editing.  When done editing, Emacs is
;; suspended again, and the client program is brought into the foreground.

;; The buffer local variable "server-buffer-clients" lists
;; the clients who are waiting for this buffer to be edited.
;; The global variable "server-clients" lists all the waiting clients,
;; and which files are yet to be edited for each.

;;; Code:

(eval-when-compile (require 'cl))

(defgroup server nil
  "Emacs running as a server process."
  :group 'external)

(defcustom server-use-tcp nil
  "If non-nil, use TCP sockets instead of local sockets."
  :set #'(lambda (sym val)
           (unless (featurep 'make-network-process '(:family local))
             (setq val t)
             (unless load-in-progress
               (message "Local sockets unsupported, using TCP sockets")))
           (when val (random t))
           (set-default sym val))
  :group 'server
  :type 'boolean
  :version "22.1")

(defcustom server-host nil
  "The name or IP address to use as host address of the server process.
If set, the server accepts remote connections; otherwise it is local."
  :group 'server
  :type '(choice
          (string :tag "Name or IP address")
          (const :tag "Local" nil))
  :version "22.1")
(put 'server-host 'risky-local-variable t)

(defcustom server-auth-dir "~/.emacs.d/server/"
  "Directory for server authentication files."
  :group 'server
  :type 'directory
  :version "22.1")
(put 'server-auth-dir 'risky-local-variable t)

(defcustom server-raise-frame t
  "If non-nil, raise frame when switching to a buffer."
  :group 'server
  :type 'boolean
  :version "22.1")

(defcustom server-visit-hook nil
  "Hook run when visiting a file for the Emacs server."
  :group 'server
  :type 'hook)

(defcustom server-switch-hook nil
  "Hook run when switching to a buffer for the Emacs server."
  :group 'server
  :type 'hook)

(defcustom server-done-hook nil
  "Hook run when done editing a buffer for the Emacs server."
  :group 'server
  :type 'hook)

(defvar server-process nil
  "The current server process.")

(defvar server-clients nil
  "List of current server clients.
Each element is (CLIENTID BUFFERS...) where CLIENTID is a string
that can be given to the server process to identify a client.
When a buffer is marked as \"done\", it is removed from this list.")

(defvar server-buffer-clients nil
  "List of client ids for clients requesting editing of current buffer.")
(make-variable-buffer-local 'server-buffer-clients)
;; Changing major modes should not erase this local.
(put 'server-buffer-clients 'permanent-local t)

(defcustom server-window nil
  "Specification of the window to use for selecting Emacs server buffers.
If nil, use the selected window.
If it is a function, it should take one argument (a buffer) and
display and select it.  A common value is `pop-to-buffer'.
If it is a window, use that.
If it is a frame, use the frame's selected window.

It is not meaningful to set this to a specific frame or window with Custom.
Only programs can do so."
  :group 'server
  :version "22.1"
  :type '(choice (const :tag "Use selected window"
			:match (lambda (widget value)
				 (not (functionp value)))
			nil)
		 (function-item :tag "Use pop-to-buffer" pop-to-buffer)
		 (function :tag "Other function")))

(defcustom server-temp-file-regexp "^/tmp/Re\\|/draft$"
  "Regexp matching names of temporary files.
These are deleted and reused after each edit by the programs that
invoke the Emacs server."
  :group 'server
  :type 'regexp)

(defcustom server-kill-new-buffers t
  "Whether to kill buffers when done with them.
If non-nil, kill a buffer unless it already existed before editing
it with Emacs server.  If nil, kill only buffers as specified by
`server-temp-file-regexp'.
Please note that only buffers are killed that still have a client,
i.e. buffers visited which \"emacsclient --no-wait\" are never killed in
this way."
  :group 'server
  :type 'boolean
  :version "21.1")

(or (assq 'server-buffer-clients minor-mode-alist)
    (push '(server-buffer-clients " Server") minor-mode-alist))

(defvar server-existing-buffer nil
  "Non-nil means the buffer existed before the server was asked to visit it.
This means that the server should not kill the buffer when you say you
are done with it in the server.")
(make-variable-buffer-local 'server-existing-buffer)

(defvar server-name "server")

(defvar server-socket-dir
  (format "/tmp/emacs%d" (user-uid)))

(defun server-log (string &optional client)
  "If a *server* buffer exists, write STRING to it for logging purposes."
  (when (get-buffer "*server*")
    (with-current-buffer "*server*"
      (goto-char (point-max))
      (insert (current-time-string)
	      (if client (format " %s:" client) " ")
	      string)
      (or (bolp) (newline)))))

(defun server-sentinel (proc msg)
  (let ((client (assq proc server-clients)))
    ;; Remove PROC from the list of clients.
    (when client
      (setq server-clients (delq client server-clients))
      (dolist (buf (cdr client))
	(with-current-buffer buf
	  ;; Remove PROC from the clients of each buffer.
	  (setq server-buffer-clients (delq proc server-buffer-clients))
	  ;; Kill the buffer if necessary.
	  (when (and (null server-buffer-clients)
		     (or (and server-kill-new-buffers
			      (not server-existing-buffer))
			 (server-temp-file-p)))
	    (kill-buffer (current-buffer)))))))
  ;; If this is a new client process, set the query-on-exit flag to nil
  ;; for this process (it isn't inherited from the server process).
  (when (and (eq (process-status proc) 'open)
	     (process-query-on-exit-flag proc))
    (set-process-query-on-exit-flag proc nil))
  ;; Delete the associated connection file, if applicable.
  ;; This is actually problematic: the file may have been overwritten by
  ;; another Emacs server in the mean time, so it's not ours any more.
  ;; (and (process-contact proc :server)
  ;;      (eq (process-status proc) 'closed)
  ;;      (ignore-errors (delete-file (process-get proc :server-file))))
  (server-log (format "Status changed to %s" (process-status proc)) proc))

(defun server-select-display (display)
  ;; If the current frame is on `display' we're all set.
  (unless (equal (frame-parameter (selected-frame) 'display) display)
    ;; Otherwise, look for an existing frame there and select it.
    (dolist (frame (frame-list))
      (when (equal (frame-parameter frame 'display) display)
	(select-frame frame)))
    ;; If there's no frame on that display yet, create and select one.
    (unless (equal (frame-parameter (selected-frame) 'display) display)
      (let* ((buffer (generate-new-buffer " *server-dummy*"))
             (frame (make-frame-on-display
                     display
                     ;; Make it display (and remember) some dummy buffer, so
                     ;; we can detect later if the frame is in use or not.
                     `((server-dummmy-buffer . ,buffer)
                       ;; This frame may be deleted later (see
                       ;; server-unselect-display) so we want it to be as
                       ;; unobtrusive as possible.
                       (visibility . nil)))))
        (select-frame frame)
        (set-window-buffer (selected-window) buffer)))))

(defun server-unselect-display (frame)
  ;; If the temporary frame is in use (displays something real), make it
  ;; visible.  If not (which can happen if the user's customizations call
  ;; pop-to-buffer etc.), delete it to avoid preserving the connection after
  ;; the last real frame is deleted.
  (if (and (eq (frame-first-window frame)
               (next-window (frame-first-window frame) 'nomini))
           (eq (window-buffer (frame-first-window frame))
               (frame-parameter frame 'server-dummy-buffer)))
      ;; The temp frame still only shows one buffer, and that is the
      ;; internal temp buffer.
      (delete-frame frame)
    (set-frame-parameter frame 'visibility t))
  (kill-buffer (frame-parameter frame 'server-dummy-buffer))
  (set-frame-parameter frame 'server-dummy-buffer nil))

(defun server-unquote-arg (arg)
  (replace-regexp-in-string
   "&." (lambda (s)
	  (case (aref s 1)
	    (?& "&")
	    (?- "-")
	    (?n "\n")
	    (t " ")))
   arg t t))

(defun server-ensure-safe-dir (dir)
  "Make sure DIR is a directory with no race-condition issues.
Creates the directory if necessary and makes sure:
- there's no symlink involved
- it's owned by us
- it's not readable/writable by anybody else."
  (setq dir (directory-file-name dir))
  (let ((attrs (file-attributes dir)))
    (unless attrs
      (letf (((default-file-modes) ?\700)) (make-directory dir t))
      (setq attrs (file-attributes dir)))
    ;; Check that it's safe for use.
    (unless (and (eq t (car attrs)) (eql (nth 2 attrs) (user-uid))
                 (or (eq system-type 'windows-nt)
                     (zerop (logand ?\077 (file-modes dir)))))
      (error "The directory %s is unsafe" dir))))

;;;###autoload
(defun server-start (&optional leave-dead)
  "Allow this Emacs process to be a server for client processes.
This starts a server communications subprocess through which
client \"editors\" can send your editing commands to this Emacs job.
To use the server, set up the program `emacsclient' in the
Emacs distribution as your standard \"editor\".

Optional argument LEAVE-DEAD (interactively, a prefix arg) means just
kill any existing server communications subprocess."
  (interactive "P")
  (when server-process
    ;; kill it dead!
    (ignore-errors (delete-process server-process)))
  ;; If this Emacs already had a server, clear out associated status.
  (while server-clients
    (let ((buffer (nth 1 (car server-clients))))
      (server-buffer-done buffer)))
  ;; Now any previous server is properly stopped.
  (unless leave-dead
    (let* ((server-dir (if server-use-tcp server-auth-dir server-socket-dir))
           (server-file (expand-file-name server-name server-dir)))
      ;; Make sure there is a safe directory in which to place the socket.
      (server-ensure-safe-dir server-dir)
      ;; Remove any leftover socket or authentication file.
      (ignore-errors (delete-file server-file))
      (when server-process
        (server-log (message "Restarting server")))
      (letf (((default-file-modes) ?\700))
        (setq server-process
              (apply #'make-network-process
                     :name server-name
                     :server t
                     :noquery t
                     :sentinel 'server-sentinel
                     :filter 'server-process-filter
                     ;; We must receive file names without being decoded.
                     ;; Those are decoded by server-process-filter according
                     ;; to file-name-coding-system.
                     :coding 'raw-text
                     ;; The rest of the args depends on the kind of socket used.
                     (if server-use-tcp
                         (list :family nil
                               :service t
                               :host (or server-host 'local)
                               :plist '(:authenticated nil))
                       (list :family 'local
                             :service server-file
                             :plist '(:authenticated t)))))
        (unless server-process (error "Could not start server process"))
        (when server-use-tcp
          (let ((auth-key
                 (loop
                    ;; The auth key is a 64-byte string of random chars in the
                    ;; range `!'..`~'.
                    for i below 64
                    collect (+ 33 (random 94)) into auth
                    finally return (concat auth))))
            (process-put server-process :auth-key auth-key)
            (with-temp-file server-file
              (set-buffer-multibyte nil)
              (setq buffer-file-coding-system 'no-conversion)
              (insert (format-network-address
                       (process-contact server-process :local))
                      " " (int-to-string (emacs-pid))
                      "\n" auth-key))))))))

;;;###autoload
(define-minor-mode server-mode
  "Toggle Server mode.
With ARG, turn Server mode on if ARG is positive, off otherwise.
Server mode runs a process that accepts commands from the
`emacsclient' program.  See `server-start' and Info node `Emacs server'."
  :global t
  :group 'server
  :version "22.1"
  ;; Fixme: Should this check for an existing server socket and do
  ;; nothing if there is one (for multiple Emacs sessions)?
  (server-start (not server-mode)))

(defun* server-process-filter (proc string)
  "Process a request from the server to edit some files.
PROC is the server process.  Format of STRING is \"PATH PATH PATH... \\n\"."
  ;; First things first: let's check the authentication
  (unless (process-get proc :authenticated)
    (if (and (string-match "-auth \\(.*?\\)\n" string)
             (equal (match-string 1 string) (process-get proc :auth-key)))
        (progn
          (setq string (substring string (match-end 0)))
          (process-put proc :authenticated t)
          (server-log "Authentication successful" proc))
      (server-log "Authentication failed" proc)
      (process-send-string proc "Authentication failed")
      (delete-process proc)
      ;; We return immediately
      (return-from server-process-filter)))
  (server-log string proc)
  (let ((prev (process-get proc :previous-string)))
    (when prev
      (setq string (concat prev string))
      (process-put proc :previous-string nil)))
  (when (> (recursion-depth) 0)
    ;; We're inside a minibuffer already, so if the emacs-client is trying
    ;; to open a frame on a new display, we might end up with an unusable
    ;; frame because input from that display will be blocked (until exiting
    ;; the minibuffer).  Better exit this minibuffer right away.
    ;; Similarly with recursive-edits such as the splash screen.
    (process-put proc :previous-string string)
    (run-with-timer 0 nil (lexical-let ((proc proc))
                            (lambda () (server-process-filter proc ""))))
    (top-level))
  (condition-case nil
      ;; If we're running isearch, we must abort it to allow Emacs to
      ;; display the buffer and switch to it.
      (mapc #'(lambda (buffer)
		(with-current-buffer buffer
		  (when (bound-and-true-p isearch-mode)
		    (isearch-cancel))))
	    (buffer-list))
    ;; Signaled by isearch-cancel
    (quit (message nil)))
  ;; If the input is multiple lines,
  ;; process each line individually.
  (while (string-match "\n" string)
    (let ((request (substring string 0 (match-beginning 0)))
	  (coding-system (and default-enable-multibyte-characters
			      (or file-name-coding-system
				  default-file-name-coding-system)))
	  client nowait eval
	  (files nil)
	  (lineno 1)
	  (tmp-frame nil) ;; Sometimes used to embody the selected display.
	  (columnno 0))
      ;; Remove this line from STRING.
      (setq string (substring string (match-end 0)))
      (setq client (cons proc nil))
      (while (string-match "[^ ]* " request)
	(let ((arg (substring request (match-beginning 0) (1- (match-end 0)))))
	  (setq request (substring request (match-end 0)))
	  (cond
            ((equal "-nowait" arg) (setq nowait t))
            ((equal "-eval" arg) (setq eval t))
            ((and (equal "-display" arg) (string-match "\\([^ ]*\\) " request))
             (let ((display (server-unquote-arg (match-string 1 request))))
               (setq request (substring request (match-end 0)))
               (condition-case err
                   (setq tmp-frame (server-select-display display))
                 (error (process-send-string proc (nth 1 err))
                        (setq request "")))))
            ;; ARG is a line number option.
            ((string-match "\\`\\+[0-9]+\\'" arg)
             (setq lineno (string-to-number (substring arg 1))))
            ;; ARG is line number:column option.
            ((string-match "\\`+\\([0-9]+\\):\\([0-9]+\\)\\'" arg)
             (setq lineno (string-to-number (match-string 1 arg))
                   columnno (string-to-number (match-string 2 arg))))
            (t
             ;; Undo the quoting that emacsclient does
             ;; for certain special characters.
             (setq arg (server-unquote-arg arg))
             ;; Now decode the file name if necessary.
             (when coding-system
               (setq arg (decode-coding-string arg coding-system)))
             (if eval
                 (let* (errorp
                        (v (condition-case errobj
                               (eval (car (read-from-string arg)))
                             (error (setq errorp t) errobj))))
                   (when v
                     (with-temp-buffer
                       (let ((standard-output (current-buffer)))
                         (when errorp (princ "error: "))
                         (pp v)
                         (ignore-errors
                           (process-send-region proc (point-min) (point-max)))
                         ))))
               ;; ARG is a file name.
               ;; Collapse multiple slashes to single slashes.
               (setq arg (command-line-normalize-file-name arg))
               (push (list arg lineno columnno) files))
             (setq lineno 1)
             (setq columnno 0)))))
      (when files
	(run-hooks 'pre-command-hook)
	(server-visit-files files client nowait)
	(run-hooks 'post-command-hook))
      ;; CLIENT is now a list (CLIENTNUM BUFFERS...)
      (if (null (cdr client))
	  ;; This client is empty; get rid of it immediately.
	  (progn
	    (delete-process proc)
	    (server-log "Close empty client" proc))
	;; We visited some buffer for this client.
	(or nowait (push client server-clients))
	(unless (or isearch-mode (minibufferp))
	  (server-switch-buffer (nth 1 client))
	  (run-hooks 'server-switch-hook)
	  (unless nowait
	    (message "%s" (substitute-command-keys
                           "When done with a buffer, type \\[server-edit]")))))
      (when (frame-live-p tmp-frame)
        ;; Delete tmp-frame or make it visible depending on whether it's
        ;; been used or not.
        (server-unselect-display tmp-frame))))
  ;; Save for later any partial line that remains.
  (when (> (length string) 0)
    (process-put proc :previous-string string)))

(defun server-goto-line-column (file-line-col)
  (goto-line (nth 1 file-line-col))
  (let ((column-number (nth 2 file-line-col)))
    (when (> column-number 0)
      (move-to-column (1- column-number)))))

(defun server-visit-files (files client &optional nowait)
  "Find FILES and return the list CLIENT with the buffers nconc'd.
FILES is an alist whose elements are (FILENAME LINENUMBER COLUMNNUMBER).
NOWAIT non-nil means this client is not waiting for the results,
so don't mark these buffers specially, just visit them normally."
  ;; Bind last-nonmenu-event to force use of keyboard, not mouse, for queries.
  (let ((last-nonmenu-event t) client-record)
    ;; Restore the current buffer afterward, but not using save-excursion,
    ;; because we don't want to save point in this buffer
    ;; if it happens to be one of those specified by the server.
    (save-current-buffer
      (dolist (file files)
	;; If there is an existing buffer modified or the file is
	;; modified, revert it.  If there is an existing buffer with
	;; deleted file, offer to write it.
	(let* ((minibuffer-auto-raise (or server-raise-frame
                                          minibuffer-auto-raise))
	       (filen (car file))
	       (obuf (get-file-buffer filen)))
	  (add-to-history 'file-name-history filen)
	  (if (and obuf (set-buffer obuf))
	      (progn
		(cond ((file-exists-p filen)
		       (when (not (verify-visited-file-modtime obuf))
                         (revert-buffer t nil)))
		      (t
		       (when (y-or-n-p
                              (concat "File no longer exists: "
                                      filen
                                      ", write buffer to file? "))
                         (write-file filen))))
		(setq server-existing-buffer t)
		(server-goto-line-column file))
	    (set-buffer (find-file-noselect filen))
	    (server-goto-line-column file)
	    (run-hooks 'server-visit-hook)))
	(unless nowait
	  ;; When the buffer is killed, inform the clients.
	  (add-hook 'kill-buffer-hook 'server-kill-buffer nil t)
	  (push (car client) server-buffer-clients))
	(push (current-buffer) client-record)))
    (nconc client client-record)))

(defun server-buffer-done (buffer &optional for-killing)
  "Mark BUFFER as \"done\" for its client(s).
This buries the buffer, then returns a list of the form (NEXT-BUFFER KILLED).
NEXT-BUFFER is another server buffer, as a suggestion for what to select next,
or nil.  KILLED is t if we killed BUFFER (typically, because it was visiting
a temp file).
FOR-KILLING if non-nil indicates that we are called from `kill-buffer'."
  (let ((next-buffer nil)
	(killed nil)
	(old-clients server-clients))
    (while old-clients
      (let ((client (car old-clients)))
	(or next-buffer
	    (setq next-buffer (nth 1 (memq buffer client))))
	(delq buffer client)
	;; Delete all dead buffers from CLIENT.
	(let ((tail client))
	  (while tail
	    (and (bufferp (car tail))
		 (null (buffer-name (car tail)))
		 (delq (car tail) client))
	    (setq tail (cdr tail))))
	;; If client now has no pending buffers,
	;; tell it that it is done, and forget it entirely.
	(unless (cdr client)
	  (delete-process (car client))
	  (server-log "Close" (car client))
	  (setq server-clients (delq client server-clients))))
      (setq old-clients (cdr old-clients)))
    (when (and (bufferp buffer) (buffer-name buffer))
      ;; We may or may not kill this buffer;
      ;; if we do, do not call server-buffer-done recursively
      ;; from kill-buffer-hook.
      (let ((server-kill-buffer-running t))
	(with-current-buffer buffer
	  (setq server-buffer-clients nil)
	  (run-hooks 'server-done-hook))
	;; Notice whether server-done-hook killed the buffer.
	(if (null (buffer-name buffer))
	    (setq killed t)
	  ;; Don't bother killing or burying the buffer
	  ;; when we are called from kill-buffer.
	  (unless for-killing
	    (when (and (not killed)
		       server-kill-new-buffers
		       (with-current-buffer buffer
			 (not server-existing-buffer)))
	      (setq killed t)
	      (bury-buffer buffer)
	      (kill-buffer buffer))
	    (unless killed
	      (if (server-temp-file-p buffer)
		  (progn
		    (kill-buffer buffer)
		    (setq killed t))
		(bury-buffer buffer)))))))
    (list next-buffer killed)))

(defun server-temp-file-p (&optional buffer)
  "Return non-nil if BUFFER contains a file considered temporary.
These are files whose names suggest they are repeatedly
reused to pass information to another program.

The variable `server-temp-file-regexp' controls which filenames
are considered temporary."
  (and (buffer-file-name buffer)
       (string-match server-temp-file-regexp (buffer-file-name buffer))))

(defun server-done ()
  "Offer to save current buffer, mark it as \"done\" for clients.
This kills or buries the buffer, then returns a list
of the form (NEXT-BUFFER KILLED).  NEXT-BUFFER is another server buffer,
as a suggestion for what to select next, or nil.
KILLED is t if we killed BUFFER, which happens if it was created
specifically for the clients and did not exist before their request for it."
  (when server-buffer-clients
    (if (server-temp-file-p)
	;; For a temp file, save, and do make a non-numeric backup
	;; (unless make-backup-files is nil).
	(let ((version-control nil)
	      (buffer-backed-up nil))
	  (save-buffer))
      (when (and (buffer-modified-p)
		 buffer-file-name
		 (y-or-n-p (concat "Save file " buffer-file-name "? ")))
	(save-buffer)))
    (server-buffer-done (current-buffer))))

;; Ask before killing a server buffer.
;; It was suggested to release its client instead,
;; but I think that is dangerous--the client would proceed
;; using whatever is on disk in that file. -- rms.
(defun server-kill-buffer-query-function ()
  (or (not server-buffer-clients)
      (yes-or-no-p (format "Buffer `%s' still has clients; kill it? "
			   (buffer-name (current-buffer))))))

(add-hook 'kill-buffer-query-functions
 	  'server-kill-buffer-query-function)

(defun server-kill-emacs-query-function ()
  (let (live-client
	(tail server-clients))
    ;; See if any clients have any buffers that are still alive.
    (while tail
      (when (memq t (mapcar 'stringp (mapcar 'buffer-name (cdr (car tail)))))
	(setq live-client t))
      (setq tail (cdr tail)))
    (or (not live-client)
	(yes-or-no-p "Server buffers still have clients; exit anyway? "))))

(add-hook 'kill-emacs-query-functions 'server-kill-emacs-query-function)

(defvar server-kill-buffer-running nil
  "Non-nil while `server-kill-buffer' or `server-buffer-done' is running.")

(defun server-kill-buffer ()
  ;; Prevent infinite recursion if user has made server-done-hook
  ;; call kill-buffer.
  (or server-kill-buffer-running
      (and server-buffer-clients
	   (let ((server-kill-buffer-running t))
	     (when server-process
	       (server-buffer-done (current-buffer) t))))))

(defun server-edit (&optional arg)
  "Switch to next server editing buffer; say \"Done\" for current buffer.
If a server buffer is current, it is marked \"done\" and optionally saved.
The buffer is also killed if it did not exist before the clients asked for it.
When all of a client's buffers are marked as \"done\", the client is notified.

Temporary files such as MH <draft> files are always saved and backed up,
no questions asked.  (The variable `make-backup-files', if nil, still
inhibits a backup; you can set it locally in a particular buffer to
prevent a backup for it.)  The variable `server-temp-file-regexp' controls
which filenames are considered temporary.

If invoked with a prefix argument, or if there is no server process running,
starts server process and that is all.  Invoked by \\[server-edit]."
  (interactive "P")
  (cond
    ((or arg
         (not server-process)
         (memq (process-status server-process) '(signal exit)))
     (server-mode 1))
    (server-clients (apply 'server-switch-buffer (server-done)))
    (t (message "No server editing buffers exist"))))

(defun server-switch-buffer (&optional next-buffer killed-one)
  "Switch to another buffer, preferably one that has a client.
Arg NEXT-BUFFER is a suggestion; if it is a live buffer, use it."
  ;; KILLED-ONE is t in a recursive call
  ;; if we have already killed one temp-file server buffer.
  ;; This means we should avoid the final "switch to some other buffer"
  ;; since we've already effectively done that.
  (if (null next-buffer)
      (if server-clients
	  (server-switch-buffer (nth 1 (car server-clients)) killed-one)
	(unless (or killed-one (window-dedicated-p (selected-window)))
	  (switch-to-buffer (other-buffer))
	  (message "No server buffers remain to edit")))
    (if (not (buffer-name next-buffer))
	;; If NEXT-BUFFER is a dead buffer, remove the server records for it
	;; and try the next surviving server buffer.
	(apply 'server-switch-buffer (server-buffer-done next-buffer))
      ;; OK, we know next-buffer is live, let's display and select it.
      (if (functionp server-window)
	  (funcall server-window next-buffer)
	(let ((win (get-buffer-window next-buffer 0)))
	  (if (and win (not server-window))
	      ;; The buffer is already displayed: just reuse the window.
              (progn
                (select-window win)
                (set-buffer next-buffer))
	    ;; Otherwise, let's find an appropriate window.
	    (cond ((and (windowp server-window)
			(window-live-p server-window))
		   (select-window server-window))
		  ((framep server-window)
		   (unless (frame-live-p server-window)
		     (setq server-window (make-frame)))
		   (select-window (frame-selected-window server-window))))
	    (when (window-minibuffer-p (selected-window))
	      (select-window (next-window nil 'nomini 0)))
	    ;; Move to a non-dedicated window, if we have one.
	    (when (window-dedicated-p (selected-window))
	      (select-window
	       (get-window-with-predicate
		(lambda (w)
		  (and (not (window-dedicated-p w))
		       (equal (frame-parameter (window-frame w) 'display)
			      (frame-parameter (selected-frame) 'display))))
		'nomini 'visible (selected-window))))
	    (condition-case nil
		(switch-to-buffer next-buffer)
	      ;; After all the above, we might still have ended up with
	      ;; a minibuffer/dedicated-window (if there's no other).
	      (error (pop-to-buffer next-buffer)))))))
    (when server-raise-frame
      (select-frame-set-input-focus (window-frame (selected-window))))))

(define-key ctl-x-map "#" 'server-edit)

(defun server-unload-hook ()
  (server-mode -1)
  (remove-hook 'kill-buffer-query-functions 'server-kill-buffer-query-function)
  (remove-hook 'kill-emacs-query-functions 'server-kill-emacs-query-function)
  (remove-hook 'kill-buffer-hook 'server-kill-buffer))

(add-hook 'kill-emacs-hook (lambda () (server-mode -1))) ;Cleanup upon exit.
(add-hook 'server-unload-hook 'server-unload-hook)

(provide 'server)

;; arch-tag: 1f7ecb42-f00a-49f8-906d-61995d84c8d6
;;; server.el ends here
