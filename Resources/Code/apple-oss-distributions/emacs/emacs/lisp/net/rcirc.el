;;; rcirc.el --- default, simple IRC client.

;; Copyright (C) 2005, 2006, 2007 Free Software Foundation, Inc.

;; Author: Ryan Yeske
;; URL: http://www.nongnu.org/rcirc
;; Keywords: comm

;; This file is part of GNU Emacs.

;; This file is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 2, or (at your option)
;; any later version.

;; This file is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with GNU Emacs; see the file COPYING.  If not, write to the
;; Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
;; Boston, MA 02110-1301, USA.

;;; Commentary:

;; Internet Relay Chat (IRC) is a form of instant communication over
;; the Internet. It is mainly designed for group (many-to-many)
;; communication in discussion forums called channels, but also allows
;; one-to-one communication.

;; Rcirc has simple defaults and clear and consistent behaviour.
;; Message arrival timestamps, activity notification on the modeline,
;; message filling, nick completion, and keepalive pings are all
;; enabled by default, but can easily be adjusted or turned off.  Each
;; discussion takes place in its own buffer and there is a single
;; server buffer per connection.

;; Open a new irc connection with:
;; M-x irc RET

;;; Todo:

;;; Code:

(require 'ring)
(require 'time-date)
(eval-when-compile (require 'cl))

(defgroup rcirc nil
  "Simple IRC client."
  :version "22.1"
  :prefix "rcirc-"
  :link '(custom-manual "(rcirc)")
  :group 'applications)

(defcustom rcirc-default-server "irc.freenode.net"
  "The default server to connect to."
  :type 'string
  :group 'rcirc)

(defcustom rcirc-default-port 6667
  "The default port to connect to."
  :type 'integer
  :group 'rcirc)

(defcustom rcirc-default-nick (user-login-name)
  "Your nick."
  :type 'string
  :group 'rcirc)

(defcustom rcirc-default-user-name (user-login-name)
  "Your user name sent to the server when connecting."
  :type 'string
  :group 'rcirc)

(defcustom rcirc-default-user-full-name (if (string= (user-full-name) "")
					    rcirc-default-user-name
					  (user-full-name))
  "The full name sent to the server when connecting."
  :type 'string
  :group 'rcirc)

(defcustom rcirc-startup-channels-alist '(("^irc.freenode.net$" "#rcirc"))
  "Alist of channels to join at startup.
Each element looks like (SERVER-REGEXP . CHANNEL-LIST)."
  :type '(alist :key-type string :value-type (repeat string))
  :group 'rcirc)

(defcustom rcirc-fill-flag t
  "*Non-nil means line-wrap messages printed in channel buffers."
  :type 'boolean
  :group 'rcirc)

(defcustom rcirc-fill-column nil
  "*Column beyond which automatic line-wrapping should happen.
If nil, use value of `fill-column'.
If `window-width', use the window's width as maximum.
If `frame-width', use the frame's width as maximum."
  :type '(choice (const :tag "Value of `fill-column'")
		 (const :tag "Full window width" window-width)
		 (const :tag "Full frame width" frame-width)
		 (integer :tag "Number of columns"))
  :group 'rcirc)

(defcustom rcirc-fill-prefix nil
  "*Text to insert before filled lines.
If nil, calculate the prefix dynamically to line up text
underneath each nick."
  :type '(choice (const :tag "Dynamic" nil)
		 (string :tag "Prefix text"))
  :group 'rcirc)

(defvar rcirc-ignore-buffer-activity-flag nil
  "If non-nil, ignore activity in this buffer.")
(make-variable-buffer-local 'rcirc-ignore-buffer-activity-flag)

(defvar rcirc-low-priority-flag nil
  "If non-nil, activity in this buffer is considered low priority.")
(make-variable-buffer-local 'rcirc-low-priority-flag)

(defcustom rcirc-time-format "%H:%M "
  "*Describes how timestamps are printed.
Used as the first arg to `format-time-string'."
  :type 'string
  :group 'rcirc)

(defcustom rcirc-input-ring-size 1024
  "*Size of input history ring."
  :type 'integer
  :group 'rcirc)

(defcustom rcirc-read-only-flag t
  "*Non-nil means make text in IRC buffers read-only."
  :type 'boolean
  :group 'rcirc)

(defcustom rcirc-buffer-maximum-lines nil
  "*The maximum size in lines for rcirc buffers.
Channel buffers are truncated from the top to be no greater than this
number.  If zero or nil, no truncating is done."
  :type '(choice (const :tag "No truncation" nil)
		 (integer :tag "Number of lines"))
  :group 'rcirc)

(defcustom rcirc-scroll-show-maximum-output t
  "*If non-nil, scroll buffer to keep the point at the bottom of the window."
  :type 'boolean
  :group 'rcirc)

(defcustom rcirc-authinfo nil
  "List of authentication passwords.
Each element of the list is a list with a SERVER-REGEXP string
and a method symbol followed by method specific arguments.

The valid METHOD symbols are `nickserv', `chanserv' and
`bitlbee'.

The required ARGUMENTS for each METHOD symbol are:
  `nickserv': NICK PASSWORD
  `chanserv': NICK CHANNEL PASSWORD
  `bitlbee': NICK PASSWORD

Example:
 ((\"freenode\" nickserv \"bob\" \"p455w0rd\")
  (\"freenode\" chanserv \"bob\" \"#bobland\" \"passwd99\")
  (\"bitlbee\" bitlbee \"robert\" \"sekrit\"))"
  :type '(alist :key-type (string :tag "Server")
		:value-type (choice (list :tag "NickServ"
					  (const nickserv)
					  (string :tag "Nick")
					  (string :tag "Password"))
				    (list :tag "ChanServ"
					  (const chanserv)
					  (string :tag "Nick")
					  (string :tag "Channel")
					  (string :tag "Password"))
				    (list :tag "BitlBee"
					  (const bitlbee)
					  (string :tag "Nick")
					  (string :tag "Password"))))
  :group 'rcirc)

(defcustom rcirc-auto-authenticate-flag t
  "*Non-nil means automatically send authentication string to server.
See also `rcirc-authinfo'."
  :type 'boolean
  :group 'rcirc)

(defcustom rcirc-prompt "> "
  "Prompt string to use in IRC buffers.

The following replacements are made:
%n is your nick.
%s is the server.
%t is the buffer target, a channel or a user.

Setting this alone will not affect the prompt;
use either M-x customize or also call `rcirc-update-prompt'."
  :type 'string
  :set 'rcirc-set-changed
  :initialize 'custom-initialize-default
  :group 'rcirc)

(defcustom rcirc-keywords nil
  "List of keywords to highlight in message text."
  :type '(repeat string)
  :group 'rcirc)

(defcustom rcirc-ignore-list ()
  "List of ignored nicks.
Use /ignore to list them, use /ignore NICK to add or remove a nick."
  :type '(repeat string)
  :group 'rcirc)

(defvar rcirc-ignore-list-automatic ()
  "List of ignored nicks added to `rcirc-ignore-list' because of renaming.
When an ignored person renames, their nick is added to both lists.
Nicks will be removed from the automatic list on follow-up renamings or
parts.")

(defcustom rcirc-bright-nicks nil
  "List of nicks to be emphasized.
See `rcirc-bright-nick' face."
  :type '(repeat string)
  :group 'rcirc)

(defcustom rcirc-dim-nicks nil
  "List of nicks to be deemphasized.
See `rcirc-dim-nick' face."
  :type '(repeat string)
  :group 'rcirc)

(defcustom rcirc-print-hooks nil
  "Hook run after text is printed.
Called with 5 arguments, PROCESS, SENDER, RESPONSE, TARGET and TEXT."
  :type 'hook
  :group 'rcirc)

(defcustom rcirc-always-use-server-buffer-flag nil
  "Non-nil means messages without a channel target will go to the server buffer."
  :type 'boolean
  :group 'rcirc)

(defcustom rcirc-decode-coding-system 'utf-8
  "Coding system used to decode incoming irc messages."
  :type 'coding-system
  :group 'rcirc)

(defcustom rcirc-encode-coding-system 'utf-8
  "Coding system used to encode outgoing irc messages."
  :type 'coding-system
  :group 'rcirc)

(defcustom rcirc-coding-system-alist nil
  "Alist to decide a coding system to use for a channel I/O operation.
The format is ((PATTERN . VAL) ...).
PATTERN is either a string or a cons of strings.
If PATTERN is a string, it is used to match a target.
If PATTERN is a cons of strings, the car part is used to match a
target, and the cdr part is used to match a server.
VAL is either a coding system or a cons of coding systems.
If VAL is a coding system, it is used for both decoding and encoding
messages.
If VAL is a cons of coding systems, the car part is used for decoding,
and the cdr part is used for encoding."
  :type '(alist :key-type (choice (string :tag "Channel Regexp")
					  (cons (string :tag "Channel Regexp")
						(string :tag "Server Regexp")))
		:value-type (choice coding-system
				    (cons (coding-system :tag "Decode")
					  (coding-system :tag "Encode"))))
  :group 'rcirc)

(defcustom rcirc-multiline-major-mode 'fundamental-mode
  "Major-mode function to use in multiline edit buffers."
  :type 'function
  :group 'rcirc)

(defvar rcirc-nick nil)

(defvar rcirc-prompt-start-marker nil)
(defvar rcirc-prompt-end-marker nil)

(defvar rcirc-nick-table nil)

(defvar rcirc-nick-syntax-table
  (let ((table (make-syntax-table text-mode-syntax-table)))
    (mapc (lambda (c) (modify-syntax-entry c "w" table))
          "[]\\`_^{|}-")
    (modify-syntax-entry ?' "_" table)
    table)
  "Syntax table which includes all nick characters as word constituents.")

;; each process has an alist of (target . buffer) pairs
(defvar rcirc-buffer-alist nil)

(defvar rcirc-activity nil
  "List of buffers with unviewed activity.")

(defvar rcirc-activity-string ""
  "String displayed in modeline representing `rcirc-activity'.")
(put 'rcirc-activity-string 'risky-local-variable t)

(defvar rcirc-server-buffer nil
  "The server buffer associated with this channel buffer.")

(defvar rcirc-target nil
  "The channel or user associated with this buffer.")

(defvar rcirc-urls nil
  "List of urls seen in the current buffer.")
(put 'rcirc-urls 'permanent-local t)

(defvar rcirc-timeout-seconds 600
  "Kill connection after this many seconds if there is no activity.")

(defconst rcirc-id-string (concat "rcirc on GNU Emacs " emacs-version))

(defvar rcirc-startup-channels nil)
;;;###autoload
(defun rcirc (arg)
  "Connect to IRC.
If ARG is non-nil, prompt for a server to connect to."
  (interactive "P")
  (if arg
      (let* ((server (read-string "IRC Server: " rcirc-default-server))
	     (port (read-string "IRC Port: " (number-to-string rcirc-default-port)))
	     (nick (read-string "IRC Nick: " rcirc-default-nick))
	     (channels (split-string
			(read-string "IRC Channels: "
				     (mapconcat 'identity (rcirc-startup-channels server) " "))
			"[, ]+" t)))
	(rcirc-connect server port nick rcirc-default-user-name rcirc-default-user-full-name
		       channels))
    ;; make new connection using defaults unless already connected to
    ;; the default rcirc-server
    (let (connected)
      (dolist (p (rcirc-process-list))
	(when (string= rcirc-default-server (process-name p))
	  (setq connected p)))
      (if (not connected)
	  (rcirc-connect rcirc-default-server rcirc-default-port
			 rcirc-default-nick rcirc-default-user-name
			 rcirc-default-user-full-name
			 (rcirc-startup-channels rcirc-default-server))
	(switch-to-buffer (process-buffer connected))
	(message "Connected to %s"
		 (process-contact (get-buffer-process (current-buffer))
				  :host))))))
;;;###autoload
(defalias 'irc 'rcirc)


(defvar rcirc-process-output nil)
(defvar rcirc-topic nil)
(defvar rcirc-keepalive-timer nil)
(defvar rcirc-last-server-message-time nil)
(defvar rcirc-server nil)		; server provided by server
(defvar rcirc-server-name nil)		; server name given by 001 response
(defvar rcirc-timeout-timer nil)
(defvar rcirc-user-disconnect nil)
(defvar rcirc-connecting nil)
(defvar rcirc-process nil)

;;;###autoload
(defun rcirc-connect (&optional server port nick user-name full-name startup-channels)
  (save-excursion
    (message "Connecting to %s..." server)
    (let* ((inhibit-eol-conversion)
           (port-number (if port
			    (if (stringp port)
				(string-to-number port)
			      port)
			  rcirc-default-port))
	   (server (or server rcirc-default-server))
	   (nick (or nick rcirc-default-nick))
	   (user-name (or user-name rcirc-default-user-name))
	   (full-name (or full-name rcirc-default-user-full-name))
	   (startup-channels startup-channels)
           (process (make-network-process :name server :host server :service port-number)))
      ;; set up process
      (set-process-coding-system process 'raw-text 'raw-text)
      (switch-to-buffer (rcirc-generate-new-buffer-name process nil))
      (set-process-buffer process (current-buffer))
      (rcirc-mode process nil)
      (set-process-sentinel process 'rcirc-sentinel)
      (set-process-filter process 'rcirc-filter)
      (make-local-variable 'rcirc-process)
      (setq rcirc-process process)
      (make-local-variable 'rcirc-server)
      (setq rcirc-server server)
      (make-local-variable 'rcirc-server-name)
      (setq rcirc-server-name server)	; update when we get 001 response
      (make-local-variable 'rcirc-buffer-alist)
      (setq rcirc-buffer-alist nil)
      (make-local-variable 'rcirc-nick-table)
      (setq rcirc-nick-table (make-hash-table :test 'equal))
      (make-local-variable 'rcirc-nick)
      (setq rcirc-nick nick)
      (make-local-variable 'rcirc-process-output)
      (setq rcirc-process-output nil)
      (make-local-variable 'rcirc-startup-channels)
      (setq rcirc-startup-channels startup-channels)
      (make-local-variable 'rcirc-last-server-message-time)
      (setq rcirc-last-server-message-time (current-time))
      (make-local-variable 'rcirc-timeout-timer)
      (setq rcirc-timeout-timer nil)
      (make-local-variable 'rcirc-user-disconnect)
      (setq rcirc-user-disconnect nil)
      (make-local-variable 'rcirc-connecting)
      (setq rcirc-connecting t)

      ;; identify
      (rcirc-send-string process (concat "NICK " nick))
      (rcirc-send-string process (concat "USER " user-name
                                      " hostname servername :"
                                      full-name))

      ;; setup ping timer if necessary
      (unless rcirc-keepalive-timer
	(setq rcirc-keepalive-timer
	      (run-at-time 0 (/ rcirc-timeout-seconds 2) 'rcirc-keepalive)))

      (message "Connecting to %s...done" server)

      ;; return process object
      process)))

(defmacro with-rcirc-process-buffer (process &rest body)
  (declare (indent 1) (debug t))
  `(with-current-buffer (process-buffer ,process)
     ,@body))

(defmacro with-rcirc-server-buffer (&rest body)
  (declare (indent 0) (debug t))
  `(with-current-buffer rcirc-server-buffer
     ,@body))

(defun rcirc-keepalive ()
  "Send keep alive pings to active rcirc processes.
Kill processes that have not received a server message since the
last ping."
  (if (rcirc-process-list)
      (mapc (lambda (process)
	      (with-rcirc-process-buffer process
		(when (not rcirc-connecting)
		  (rcirc-send-string process (concat "PING " (rcirc-server-name process))))))
            (rcirc-process-list))
    ;; no processes, clean up timer
    (cancel-timer rcirc-keepalive-timer)
    (setq rcirc-keepalive-timer nil)))

(defvar rcirc-debug-buffer " *rcirc debug*")
(defvar rcirc-debug-flag nil
  "If non-nil, write information to `rcirc-debug-buffer'.")
(defun rcirc-debug (process text)
  "Add an entry to the debug log including PROCESS and TEXT.
Debug text is written to `rcirc-debug-buffer' if `rcirc-debug-flag'
is non-nil."
  (when rcirc-debug-flag
    (save-excursion
      (save-window-excursion
        (set-buffer (get-buffer-create rcirc-debug-buffer))
        (goto-char (point-max))
        (insert (concat
                 "["
                 (format-time-string "%Y-%m-%dT%T ") (process-name process)
                 "] "
                 text))))))

(defvar rcirc-sentinel-hooks nil
  "Hook functions called when the process sentinel is called.
Functions are called with PROCESS and SENTINEL arguments.")

(defun rcirc-sentinel (process sentinel)
  "Called when PROCESS receives SENTINEL."
  (let ((sentinel (replace-regexp-in-string "\n" "" sentinel)))
    (rcirc-debug process (format "SENTINEL: %S %S\n" process sentinel))
    (with-rcirc-process-buffer process
      (dolist (buffer (cons nil (mapcar 'cdr rcirc-buffer-alist)))
	(with-current-buffer (or buffer (current-buffer))
	  (rcirc-print process "rcirc.el" "ERROR" rcirc-target
		       (format "%s: %s (%S)"
			       (process-name process)
			       sentinel
			       (process-status process)) (not rcirc-target))
	  ;; remove the prompt from buffers
	  (let ((inhibit-read-only t))
	    (delete-region rcirc-prompt-start-marker
			   rcirc-prompt-end-marker))))
      (run-hook-with-args 'rcirc-sentinel-hooks process sentinel))))

(defun rcirc-process-list ()
  "Return a list of rcirc processes."
  (let (ps)
    (mapc (lambda (p)
            (when (buffer-live-p (process-buffer p))
              (with-rcirc-process-buffer p
                (when (eq major-mode 'rcirc-mode)
                  (setq ps (cons p ps))))))
          (process-list))
    ps))

(defvar rcirc-receive-message-hooks nil
  "Hook functions run when a message is received from server.
Function is called with PROCESS, COMMAND, SENDER, ARGS and LINE.")
(defun rcirc-filter (process output)
  "Called when PROCESS receives OUTPUT."
  (rcirc-debug process output)
  (rcirc-reschedule-timeout process)
  (with-rcirc-process-buffer process
    (setq rcirc-last-server-message-time (current-time))
    (setq rcirc-process-output (concat rcirc-process-output output))
    (when (= (aref rcirc-process-output
                   (1- (length rcirc-process-output))) ?\n)
      (mapc (lambda (line)
              (rcirc-process-server-response process line))
            (split-string rcirc-process-output "[\n\r]" t))
      (setq rcirc-process-output nil))))

(defun rcirc-reschedule-timeout (process)
  (with-rcirc-process-buffer process
    (when (not rcirc-connecting)
      (with-rcirc-process-buffer process
	(when rcirc-timeout-timer (cancel-timer rcirc-timeout-timer))
	(setq rcirc-timeout-timer (run-at-time rcirc-timeout-seconds nil
					       'rcirc-delete-process
					       process))))))

(defun rcirc-delete-process (process)
  (message "delete process %S" process)
  (delete-process process))

(defvar rcirc-trap-errors-flag t)
(defun rcirc-process-server-response (process text)
  (if rcirc-trap-errors-flag
      (condition-case err
          (rcirc-process-server-response-1 process text)
        (error
         (rcirc-print process "RCIRC" "ERROR" nil
                      (format "\"%s\" %s" text err) t)))
    (rcirc-process-server-response-1 process text)))

(defun rcirc-process-server-response-1 (process text)
  (if (string-match "^\\(:\\([^ ]+\\) \\)?\\([^ ]+\\) \\(.+\\)$" text)
      (let* ((user (match-string 2 text))
	     (sender (rcirc-user-nick user))
             (cmd (match-string 3 text))
             (args (match-string 4 text))
             (handler (intern-soft (concat "rcirc-handler-" cmd))))
        (string-match "^\\([^:]*\\):?\\(.+\\)?$" args)
        (let* ((args1 (match-string 1 args))
               (args2 (match-string 2 args))
               (args (delq nil (append (split-string args1 " " t)
				       (list args2)))))
        (if (not (fboundp handler))
            (rcirc-handler-generic process cmd sender args text)
          (funcall handler process sender args text))
        (run-hook-with-args 'rcirc-receive-message-hooks
                            process cmd sender args text)))
    (message "UNHANDLED: %s" text)))

(defvar rcirc-responses-no-activity '("305" "306")
  "Responses that don't trigger activity in the mode-line indicator.")

(defun rcirc-handler-generic (process response sender args text)
  "Generic server response handler."
  (rcirc-print process sender response nil
               (mapconcat 'identity (cdr args) " ")
	       (not (member response rcirc-responses-no-activity))))

(defun rcirc-send-string (process string)
  "Send PROCESS a STRING plus a newline."
  (let ((string (concat (encode-coding-string string rcirc-encode-coding-system)
                        "\n")))
    (unless (eq (process-status process) 'open)
      (error "Network connection to %s is not open"
             (process-name process)))
    (rcirc-debug process string)
    (process-send-string process string)))

(defun rcirc-buffer-process (&optional buffer)
  "Return the process associated with channel BUFFER.
With no argument or nil as argument, use the current buffer."
  (or (get-buffer-process (if buffer
			      (with-current-buffer buffer
				rcirc-server-buffer)
			    rcirc-server-buffer))
      rcirc-process))

(defun rcirc-server-name (process)
  "Return PROCESS server name, given by the 001 response."
  (with-rcirc-process-buffer process
    (or rcirc-server-name rcirc-default-server)))

(defun rcirc-nick (process)
  "Return PROCESS nick."
  (with-rcirc-process-buffer process
    (or rcirc-nick rcirc-default-nick)))

(defun rcirc-buffer-nick (&optional buffer)
  "Return the nick associated with BUFFER.
With no argument or nil as argument, use the current buffer."
  (with-current-buffer (or buffer (current-buffer))
    (with-current-buffer rcirc-server-buffer
      (or rcirc-nick rcirc-default-nick))))

(defvar rcirc-max-message-length 420
  "Messages longer than this value will be split.")

(defun rcirc-send-message (process target message &optional noticep)
  "Send TARGET associated with PROCESS a privmsg with text MESSAGE.
If NOTICEP is non-nil, send a notice instead of privmsg."
  ;; max message length is 512 including CRLF
  (let* ((response (if noticep "NOTICE" "PRIVMSG"))
         (oversize (> (length message) rcirc-max-message-length))
         (text (if oversize
                   (substring message 0 rcirc-max-message-length)
                 message))
         (text (if (string= text "")
                   " "
                 text))
         (more (if oversize
                   (substring message rcirc-max-message-length))))
    (rcirc-get-buffer-create process target)
    (rcirc-print process (rcirc-nick process) response target text)
    (rcirc-send-string process (concat response " " target " :" text))
    (when more (rcirc-send-message process target more noticep))))

(defvar rcirc-input-ring nil)
(defvar rcirc-input-ring-index 0)
(defun rcirc-prev-input-string (arg)
  (ring-ref rcirc-input-ring (+ rcirc-input-ring-index arg)))

(defun rcirc-insert-prev-input (arg)
  (interactive "p")
  (when (<= rcirc-prompt-end-marker (point))
    (delete-region rcirc-prompt-end-marker (point-max))
    (insert (rcirc-prev-input-string 0))
    (setq rcirc-input-ring-index (1+ rcirc-input-ring-index))))

(defun rcirc-insert-next-input (arg)
  (interactive "p")
  (when (<= rcirc-prompt-end-marker (point))
    (delete-region rcirc-prompt-end-marker (point-max))
    (setq rcirc-input-ring-index (1- rcirc-input-ring-index))
    (insert (rcirc-prev-input-string -1))))

(defvar rcirc-nick-completions nil)
(defvar rcirc-nick-completion-start-offset nil)

(defun rcirc-complete-nick ()
  "Cycle through nick completions from list of nicks in channel."
  (interactive)
  (if (eq last-command this-command)
      (setq rcirc-nick-completions
            (append (cdr rcirc-nick-completions)
                    (list (car rcirc-nick-completions))))
    (setq rcirc-nick-completion-start-offset
          (- (save-excursion
               (if (re-search-backward " " rcirc-prompt-end-marker t)
                   (1+ (point))
                 rcirc-prompt-end-marker))
             rcirc-prompt-end-marker))
    (setq rcirc-nick-completions
          (let ((completion-ignore-case t))
            (all-completions
	     (buffer-substring
	      (+ rcirc-prompt-end-marker
		 rcirc-nick-completion-start-offset)
	      (point))
	     (mapcar (lambda (x) (cons x nil))
		     (rcirc-channel-nicks (rcirc-buffer-process)
					  rcirc-target))))))
  (let ((completion (car rcirc-nick-completions)))
    (when completion
      (rcirc-put-nick-channel (rcirc-buffer-process) completion rcirc-target)
      (delete-region (+ rcirc-prompt-end-marker
			rcirc-nick-completion-start-offset)
		     (point))
      (insert (concat completion
                      (if (= (+ rcirc-prompt-end-marker
                                rcirc-nick-completion-start-offset)
                             rcirc-prompt-end-marker)
                          ": "))))))

(defun set-rcirc-decode-coding-system (coding-system)
  "Set the decode coding system used in this channel."
  (interactive "zCoding system for incoming messages: ")
  (setq rcirc-decode-coding-system coding-system))

(defun set-rcirc-encode-coding-system (coding-system)
  "Set the encode coding system used in this channel."
  (interactive "zCoding system for outgoing messages: ")
  (setq rcirc-encode-coding-system coding-system))

(defvar rcirc-mode-map (make-sparse-keymap)
  "Keymap for rcirc mode.")

(define-key rcirc-mode-map (kbd "RET") 'rcirc-send-input)
(define-key rcirc-mode-map (kbd "M-p") 'rcirc-insert-prev-input)
(define-key rcirc-mode-map (kbd "M-n") 'rcirc-insert-next-input)
(define-key rcirc-mode-map (kbd "TAB") 'rcirc-complete-nick)
(define-key rcirc-mode-map (kbd "C-c C-b") 'rcirc-browse-url)
(define-key rcirc-mode-map (kbd "C-c C-c") 'rcirc-edit-multiline)
(define-key rcirc-mode-map (kbd "C-c C-j") 'rcirc-cmd-join)
(define-key rcirc-mode-map (kbd "C-c C-k") 'rcirc-cmd-kick)
(define-key rcirc-mode-map (kbd "C-c C-l") 'rcirc-toggle-low-priority)
(define-key rcirc-mode-map (kbd "C-c C-d") 'rcirc-cmd-mode)
(define-key rcirc-mode-map (kbd "C-c C-m") 'rcirc-cmd-msg)
(define-key rcirc-mode-map (kbd "C-c C-r") 'rcirc-cmd-nick) ; rename
(define-key rcirc-mode-map (kbd "C-c C-o") 'rcirc-cmd-oper)
(define-key rcirc-mode-map (kbd "C-c C-p") 'rcirc-cmd-part)
(define-key rcirc-mode-map (kbd "C-c C-q") 'rcirc-cmd-query)
(define-key rcirc-mode-map (kbd "C-c C-t") 'rcirc-cmd-topic)
(define-key rcirc-mode-map (kbd "C-c C-n") 'rcirc-cmd-names)
(define-key rcirc-mode-map (kbd "C-c C-w") 'rcirc-cmd-whois)
(define-key rcirc-mode-map (kbd "C-c C-x") 'rcirc-cmd-quit)
(define-key rcirc-mode-map (kbd "C-c TAB") ; C-i
  'rcirc-toggle-ignore-buffer-activity)
(define-key rcirc-mode-map (kbd "C-c C-s") 'rcirc-switch-to-server-buffer)
(define-key rcirc-mode-map (kbd "C-c C-a") 'rcirc-jump-to-first-unread-line)

(defvar rcirc-browse-url-map (make-sparse-keymap)
  "Keymap used for browsing URLs in `rcirc-mode'.")

(define-key rcirc-browse-url-map (kbd "RET") 'rcirc-browse-url-at-point)
(define-key rcirc-browse-url-map (kbd "<mouse-2>") 'rcirc-browse-url-at-mouse)

(defvar rcirc-short-buffer-name nil
  "Generated abbreviation to use to indicate buffer activity.")

(defvar rcirc-mode-hook nil
  "Hook run when setting up rcirc buffer.")

(defvar rcirc-last-post-time nil)

(defun rcirc-mode (process target)
  "Major mode for IRC channel buffers.

\\{rcirc-mode-map}"
  (kill-all-local-variables)
  (use-local-map rcirc-mode-map)
  (setq mode-name "rcirc")
  (setq major-mode 'rcirc-mode)

  (make-local-variable 'rcirc-input-ring)
  (setq rcirc-input-ring (make-ring rcirc-input-ring-size))
  (make-local-variable 'rcirc-server-buffer)
  (setq rcirc-server-buffer (process-buffer process))
  (make-local-variable 'rcirc-target)
  (setq rcirc-target target)
  (make-local-variable 'rcirc-topic)
  (setq rcirc-topic nil)
  (make-local-variable 'rcirc-last-post-time)
  (setq rcirc-last-post-time (current-time))

  (make-local-variable 'rcirc-short-buffer-name)
  (setq rcirc-short-buffer-name nil)
  (make-local-variable 'rcirc-urls)
  (setq use-hard-newlines t)

  (make-local-variable 'rcirc-decode-coding-system)
  (make-local-variable 'rcirc-encode-coding-system)
  (dolist (i rcirc-coding-system-alist)
    (let ((chan (if (consp (car i)) (caar i) (car i)))
	  (serv (if (consp (car i)) (cdar i) "")))
      (when (and (string-match chan (or target ""))
		 (string-match serv (rcirc-server-name process)))
	(setq rcirc-decode-coding-system (if (consp (cdr i)) (cadr i) (cdr i))
	      rcirc-encode-coding-system (if (consp (cdr i)) (cddr i) (cdr i))))))

  ;; setup the prompt and markers
  (make-local-variable 'rcirc-prompt-start-marker)
  (setq rcirc-prompt-start-marker (make-marker))
  (set-marker rcirc-prompt-start-marker (point-max))
  (make-local-variable 'rcirc-prompt-end-marker)
  (setq rcirc-prompt-end-marker (make-marker))
  (set-marker rcirc-prompt-end-marker (point-max))
  (rcirc-update-prompt)
  (goto-char rcirc-prompt-end-marker)
  (make-local-variable 'overlay-arrow-position)
  (setq overlay-arrow-position (make-marker))
  (set-marker overlay-arrow-position nil)

  ;; if the user changes the major mode or kills the buffer, there is
  ;; cleanup work to do
  (add-hook 'change-major-mode-hook 'rcirc-change-major-mode-hook nil t)
  (add-hook 'kill-buffer-hook 'rcirc-kill-buffer-hook nil t)

  ;; add to buffer list, and update buffer abbrevs
  (when target				; skip server buffer
    (let ((buffer (current-buffer)))
      (with-rcirc-process-buffer process
	(setq rcirc-buffer-alist (cons (cons target buffer)
				       rcirc-buffer-alist))))
    (rcirc-update-short-buffer-names))

  (run-hooks 'rcirc-mode-hook))

(defun rcirc-update-prompt (&optional all)
  "Reset the prompt string in the current buffer.

If ALL is non-nil, update prompts in all IRC buffers."
  (if all
      (mapc (lambda (process)
	      (mapc (lambda (buffer)
		      (with-current-buffer buffer
			(rcirc-update-prompt)))
		    (with-rcirc-process-buffer process
		      (mapcar 'cdr rcirc-buffer-alist))))
	    (rcirc-process-list))
    (let ((inhibit-read-only t)
	  (prompt (or rcirc-prompt "")))
      (mapc (lambda (rep)
	      (setq prompt
		    (replace-regexp-in-string (car rep) (cdr rep) prompt)))
	    (list (cons "%n" (rcirc-buffer-nick))
		  (cons "%s" (with-rcirc-server-buffer rcirc-server-name))
		  (cons "%t" (or rcirc-target ""))))
      (save-excursion
	(delete-region rcirc-prompt-start-marker rcirc-prompt-end-marker)
	(goto-char rcirc-prompt-start-marker)
	(let ((start (point)))
	  (insert-before-markers prompt)
	  (set-marker rcirc-prompt-start-marker start)
	  (when (not (zerop (- rcirc-prompt-end-marker
			       rcirc-prompt-start-marker)))
	    (add-text-properties rcirc-prompt-start-marker
				 rcirc-prompt-end-marker
				 (list 'face 'rcirc-prompt
				       'read-only t 'field t
				       'front-sticky t 'rear-nonsticky t))))))))

(defun rcirc-set-changed (option value)
  "Set OPTION to VALUE and do updates after a customization change."
  (set-default option value)
  (cond ((eq option 'rcirc-prompt)
	 (rcirc-update-prompt 'all))
	(t
	 (error "Bad option %s" option))))

(defun rcirc-channel-p (target)
  "Return t if TARGET is a channel name."
  (and target
       (not (zerop (length target)))
       (or (eq (aref target 0) ?#)
           (eq (aref target 0) ?&))))

(defun rcirc-kill-buffer-hook ()
  "Part the channel when killing an rcirc buffer."
  (when (eq major-mode 'rcirc-mode)
    (rcirc-clean-up-buffer "Killed buffer")))

(defun rcirc-change-major-mode-hook ()
  "Part the channel when changing the major-mode."
  (rcirc-clean-up-buffer "Changed major mode"))

(defun rcirc-clean-up-buffer (reason)
  (let ((buffer (current-buffer)))
    (rcirc-clear-activity buffer)
    (when (and (rcirc-buffer-process)
	       (eq (process-status (rcirc-buffer-process)) 'open))
      (with-rcirc-server-buffer
       (setq rcirc-buffer-alist
	     (rassq-delete-all buffer rcirc-buffer-alist)))
      (rcirc-update-short-buffer-names)
      (if (rcirc-channel-p rcirc-target)
	  (rcirc-send-string (rcirc-buffer-process)
			     (concat "PART " rcirc-target " :" reason))
	(when rcirc-target
	  (rcirc-remove-nick-channel (rcirc-buffer-process)
				     (rcirc-buffer-nick)
				     rcirc-target))))))

(defun rcirc-generate-new-buffer-name (process target)
  "Return a buffer name based on PROCESS and TARGET.
This is used for the initial name given to IRC buffers."
  (if target
      (concat target "@" (process-name process))
    (concat "*" (process-name process) "*")))

(defun rcirc-get-buffer (process target &optional server)
  "Return the buffer associated with the PROCESS and TARGET.

If optional argument SERVER is non-nil, return the server buffer
if there is no existing buffer for TARGET, otherwise return nil."
  (with-rcirc-process-buffer process
    (if (null target)
	(current-buffer)
      (let ((buffer (cdr (assoc-string target rcirc-buffer-alist t))))
	(or buffer (when server (current-buffer)))))))

(defun rcirc-get-buffer-create (process target)
  "Return the buffer associated with the PROCESS and TARGET.
Create the buffer if it doesn't exist."
  (let ((buffer (rcirc-get-buffer process target)))
    (if (and buffer (buffer-live-p buffer))
	(with-current-buffer buffer
	  (when (not rcirc-target)
 	    (setq rcirc-target target))
	  buffer)
	;; create the buffer
	(with-rcirc-process-buffer process
	  (let ((new-buffer (get-buffer-create
			     (rcirc-generate-new-buffer-name process target))))
	    (with-current-buffer new-buffer
	      (rcirc-mode process target))
	    (rcirc-put-nick-channel process (rcirc-nick process) target)
	    new-buffer)))))

(defun rcirc-send-input ()
  "Send input to target associated with the current buffer."
  (interactive)
  (if (< (point) rcirc-prompt-end-marker)
      ;; copy the line down to the input area
      (progn
	(forward-line 0)
	(let ((start (if (eq (point) (point-min))
			 (point)
		       (if (get-text-property (1- (point)) 'hard)
			   (point)
			 (previous-single-property-change (point) 'hard))))
	      (end (next-single-property-change (1+ (point)) 'hard)))
	  (goto-char (point-max))
	  (insert (replace-regexp-in-string
		   "\n\\s-+" " "
		   (buffer-substring-no-properties start end)))))
    ;; process input
    (goto-char (point-max))
    (when (not (equal 0 (- (point) rcirc-prompt-end-marker)))
      ;; delete a trailing newline
      (when (eq (point) (point-at-bol))
	(delete-backward-char 1))
      (let ((input (buffer-substring-no-properties
		    rcirc-prompt-end-marker (point))))
	(dolist (line (split-string input "\n"))
	  (rcirc-process-input-line line))
	;; add to input-ring
	(save-excursion
	  (ring-insert rcirc-input-ring input)
	  (setq rcirc-input-ring-index 0))))))

(defun rcirc-process-input-line (line)
  (if (string-match "^/\\([^ ]+\\) ?\\(.*\\)$" line)
      (rcirc-process-command (match-string 1 line)
			     (match-string 2 line)
			     line)
    (rcirc-process-message line)))

(defun rcirc-process-message (line)
  (if (not rcirc-target)
      (message "Not joined (no target)")
    (delete-region rcirc-prompt-end-marker (point))
    (rcirc-send-message (rcirc-buffer-process) rcirc-target line)
    (setq rcirc-last-post-time (current-time))))

(defun rcirc-process-command (command args line)
  (if (eq (aref command 0) ?/)
      ;; "//text" will send "/text" as a message
      (rcirc-process-message (substring line 1))
    (let ((fun (intern-soft (concat "rcirc-cmd-" command)))
	  (process (rcirc-buffer-process)))
      (newline)
      (with-current-buffer (current-buffer)
	(delete-region rcirc-prompt-end-marker (point))
	(if (string= command "me")
	    (rcirc-print process (rcirc-buffer-nick)
			 "ACTION" rcirc-target args)
	  (rcirc-print process (rcirc-buffer-nick)
		       "COMMAND" rcirc-target line))
	(set-marker rcirc-prompt-end-marker (point))
	(if (fboundp fun)
	    (funcall fun args process rcirc-target)
	  (rcirc-send-string process
			     (concat command " :" args)))))))

(defvar rcirc-parent-buffer nil)
(defvar rcirc-window-configuration nil)
(defun rcirc-edit-multiline ()
  "Move current edit to a dedicated buffer."
  (interactive)
  (let ((pos (1+ (- (point) rcirc-prompt-end-marker))))
    (goto-char (point-max))
    (let ((text (buffer-substring rcirc-prompt-end-marker (point)))
          (parent (buffer-name)))
      (delete-region rcirc-prompt-end-marker (point))
      (setq rcirc-window-configuration (current-window-configuration))
      (pop-to-buffer (concat "*multiline " parent "*"))
      (funcall rcirc-multiline-major-mode)
      (rcirc-multiline-minor-mode 1)
      (setq rcirc-parent-buffer parent)
      (insert text)
      (and (> pos 0) (goto-char pos))
      (message "Type C-c C-c to return text to %s, or C-c C-k to cancel" parent))))

(defvar rcirc-multiline-minor-mode-map (make-sparse-keymap)
  "Keymap for multiline mode in rcirc.")
(define-key rcirc-multiline-minor-mode-map
  (kbd "C-c C-c") 'rcirc-multiline-minor-submit)
(define-key rcirc-multiline-minor-mode-map
  (kbd "C-x C-s") 'rcirc-multiline-minor-submit)
(define-key rcirc-multiline-minor-mode-map
  (kbd "C-c C-k") 'rcirc-multiline-minor-cancel)
(define-key rcirc-multiline-minor-mode-map
  (kbd "ESC ESC ESC") 'rcirc-multiline-minor-cancel)

(define-minor-mode rcirc-multiline-minor-mode
  "Minor mode for editing multiple lines in rcirc."
  :init-value nil
  :lighter " rcirc-mline"
  :keymap rcirc-multiline-minor-mode-map
  :global nil
  :group 'rcirc
  (make-local-variable 'rcirc-parent-buffer)
  (put 'rcirc-parent-buffer 'permanent-local t)
  (setq fill-column rcirc-max-message-length))

(defun rcirc-multiline-minor-submit ()
  "Send the text in buffer back to parent buffer."
  (interactive)
  (assert rcirc-parent-buffer)
  (untabify (point-min) (point-max))
  (let ((text (buffer-substring (point-min) (point-max)))
        (buffer (current-buffer))
        (pos (point)))
    (set-buffer rcirc-parent-buffer)
    (goto-char (point-max))
    (insert text)
    (kill-buffer buffer)
    (set-window-configuration rcirc-window-configuration)
    (goto-char (+ rcirc-prompt-end-marker (1- pos)))))

(defun rcirc-multiline-minor-cancel ()
  "Cancel the multiline edit."
  (interactive)
  (kill-buffer (current-buffer))
  (set-window-configuration rcirc-window-configuration))

(defun rcirc-any-buffer (process)
  "Return a buffer for PROCESS, either the one selected or the process buffer."
  (if rcirc-always-use-server-buffer-flag
      (process-buffer process)
    (let ((buffer (window-buffer (selected-window))))
      (if (and buffer
	       (with-current-buffer buffer
		 (and (eq major-mode 'rcirc-mode)
		      (eq (rcirc-buffer-process) process))))
	  buffer
	(process-buffer process)))))

(defcustom rcirc-response-formats
  '(("PRIVMSG" . "%T<%N> %m")
    ("NOTICE"  . "%T-%N- %m")
    ("ACTION"  . "%T[%N %m]")
    ("COMMAND" . "%T%m")
    ("ERROR"   . "%T%fw!!! %m")
    (t         . "%T%fp*** %fs%n %r %m"))
  "An alist of formats used for printing responses.
The format is looked up using the response-type as a key;
if no match is found, the default entry (with a key of `t') is used.

The entry's value part should be a string, which is inserted with
the of the following escape sequences replaced by the described values:

  %m        The message text
  %n        The sender's nick
  %N        The sender's nick (with face `rcirc-my-nick' or `rcirc-other-nick')
  %r        The response-type
  %T        The timestamp (with face `rcirc-timestamp')
  %t        The target
  %fw       Following text uses the face `font-lock-warning-face'
  %fp       Following text uses the face `rcirc-server-prefix'
  %fs       Following text uses the face `rcirc-server'
  %f[FACE]  Following text uses the face FACE
  %f-       Following text uses the default face
  %%        A literal `%' character"
  :type '(alist :key-type (choice (string :tag "Type")
				  (const :tag "Default" t))
		:value-type string)
  :group 'rcirc)

(defun rcirc-format-response-string (process sender response target text)
  "Return a nicely-formatted response string, incorporating TEXT
\(and perhaps other arguments).  The specific formatting used
is found by looking up RESPONSE in `rcirc-response-formats'."
  (let ((chunks
	 (split-string (or (cdr (assoc response rcirc-response-formats))
			   (cdr (assq t rcirc-response-formats)))
		       "%"))
	(sender (or sender ""))
	(result "")
	(face nil)
	key face-key repl)
    (when (equal (car chunks) "")
      (pop chunks))
    (dolist (chunk chunks)
      (if (equal chunk "")
	  (setq key ?%)
	(setq key (aref chunk 0))
	(setq chunk (substring chunk 1)))
      (setq repl
	    (cond ((eq key ?%)
		   ;; %% -- literal % character
		   "%")
		  ((or (eq key ?n) (eq key ?N))
		   ;; %n/%N -- nick
		   (let ((nick (concat (if (string= (rcirc-server-name process)
						    sender)
					   ""
					 sender)
				       (and target (concat "," target)))))
		     (rcirc-facify nick
				   (if (eq key ?n)
				       face
				     (cond ((string= sender (rcirc-nick process))
					    'rcirc-my-nick)
					   ((and rcirc-bright-nicks
						 (string-match
						  (regexp-opt rcirc-bright-nicks)
						  sender))
					    'rcirc-bright-nick)
					   ((and rcirc-dim-nicks
						 (string-match
						  (regexp-opt rcirc-dim-nicks)
						  sender))
					    'rcirc-dim-nick)
					   (t
					    'rcirc-other-nick))))))
		   ((eq key ?T)
		   ;; %T -- timestamp
		   (rcirc-facify
		    (format-time-string rcirc-time-format (current-time))
		    'rcirc-timestamp))
		  ((eq key ?m)
		   ;; %m -- message text
		   (rcirc-markup-text process sender response (rcirc-facify text face)))
		  ((eq key ?t)
		   ;; %t -- target
		   (rcirc-facify (or rcirc-target "") face))
		  ((eq key ?r)
		   ;; %r -- response
		   (rcirc-facify response face))
		  ((eq key ?f)
		   ;; %f -- change face
		   (setq face-key (aref chunk 0))
		   (setq chunk (substring chunk 1))
		   (cond ((eq face-key ?w)
			  ;; %fw -- warning face
			  (setq face 'font-lock-warning-face))
			 ((eq face-key ?p)
			  ;; %fp -- server-prefix face
			  (setq face 'rcirc-server-prefix))
			 ((eq face-key ?s)
			  ;; %fs -- warning face
			  (setq face 'rcirc-server))
			 ((eq face-key ?-)
			  ;; %fs -- warning face
			  (setq face nil))
			 ((and (eq face-key ?\[)
			       (string-match "^\\([^]]*\\)[]]" chunk)
			       (facep (match-string 1 chunk)))
			  ;; %f[...] -- named face
			  (setq face (intern (match-string 1 chunk)))
			  (setq chunk (substring chunk (match-end 0)))))
		   "")))
      (setq result (concat result repl (rcirc-facify chunk face))))
    result))

(defun rcirc-target-buffer (process sender response target text)
  "Return a buffer to print the server response."
  (assert (not (bufferp target)))
  (with-rcirc-process-buffer process
    (cond ((not target)
	   (rcirc-any-buffer process))
	  ((not (rcirc-channel-p target))
	   ;; message from another user
	   (if (string= response "PRIVMSG")
	       (rcirc-get-buffer-create process (if (string= sender rcirc-nick)
						    target
						  sender))
	     (rcirc-get-buffer process target t)))
	  ((or (rcirc-get-buffer process target)
	       (rcirc-any-buffer process))))))

(defvar rcirc-activity-types nil)
(make-variable-buffer-local 'rcirc-activity-types)
(defvar rcirc-last-sender nil)
(make-variable-buffer-local 'rcirc-last-sender)

(defun rcirc-print (process sender response target text &optional activity)
  "Print TEXT in the buffer associated with TARGET.
Format based on SENDER and RESPONSE.  If ACTIVITY is non-nil,
record activity."
  (or text (setq text ""))
  (unless (or (member sender rcirc-ignore-list)
	      (member (with-syntax-table rcirc-nick-syntax-table
			(when (string-match "^\\([^/]\\w*\\)[:,]" text)
			  (match-string 1 text)))
		      rcirc-ignore-list))
    (let* ((buffer (rcirc-target-buffer process sender response target text))
	   (inhibit-read-only t))
      (with-current-buffer buffer
	(let ((moving (= (point) rcirc-prompt-end-marker))
	      (old-point (point-marker))
	      (fill-start (marker-position rcirc-prompt-start-marker)))

	  (unless (string= sender (rcirc-nick process))
	    ;; only decode text from other senders, not ours
	    (setq text (decode-coding-string text rcirc-decode-coding-system))
	    ;; mark the line with overlay arrow
	    (unless (or (marker-position overlay-arrow-position)
			(get-buffer-window (current-buffer)))
	      (set-marker overlay-arrow-position
			  (marker-position rcirc-prompt-start-marker))))

	  ;; temporarily set the marker insertion-type because
	  ;; insert-before-markers results in hidden text in new buffers
	  (goto-char rcirc-prompt-start-marker)
	  (set-marker-insertion-type rcirc-prompt-start-marker t)
	  (set-marker-insertion-type rcirc-prompt-end-marker t)

	  (let ((fmted-text
		 (rcirc-format-response-string process sender response nil
					       text)))

	    (insert fmted-text (propertize "\n" 'hard t))
	    (set-marker-insertion-type rcirc-prompt-start-marker nil)
	    (set-marker-insertion-type rcirc-prompt-end-marker nil)

	    (let ((text-start (make-marker)))
	      (set-marker text-start
			  (or (next-single-property-change fill-start
							   'rcirc-text)
			      rcirc-prompt-end-marker))
	      ;; squeeze spaces out of text before rcirc-text
	      (fill-region fill-start (1- text-start))

	      ;; fill the text we just inserted, maybe
	      (when (and rcirc-fill-flag
			 (not (string= response "372"))) ;/motd
		(let ((fill-prefix
		       (or rcirc-fill-prefix
			   (make-string (- text-start fill-start) ?\s)))
		      (fill-column (cond ((eq rcirc-fill-column 'frame-width)
					  (1- (frame-width)))
					 ((eq rcirc-fill-column 'window-width)
					  (1- (window-width)))
					 (rcirc-fill-column
					  rcirc-fill-column)
					 (t fill-column))))
		  (fill-region fill-start rcirc-prompt-start-marker 'left t)))))

	  ;; set inserted text to be read-only
	  (when rcirc-read-only-flag
	    (put-text-property rcirc-prompt-start-marker fill-start 'read-only t)
	    (let ((inhibit-read-only t))
	      (put-text-property rcirc-prompt-start-marker fill-start
				 'front-sticky t)
	      (put-text-property (1- (point)) (point) 'rear-nonsticky t)))

	  ;; truncate buffer if it is very long
	  (save-excursion
	    (when (and rcirc-buffer-maximum-lines
		       (> rcirc-buffer-maximum-lines 0)
		       (= (forward-line (- rcirc-buffer-maximum-lines)) 0))
	      (delete-region (point-min) (point))))

	  ;; set the window point for buffers show in windows
	  (walk-windows (lambda (w)
			  (when (and (not (eq (selected-window) w))
				     (eq (current-buffer)
					 (window-buffer w))
				     (>= (window-point w)
					 rcirc-prompt-end-marker))
			    (set-window-point w (point-max))))
			nil t)

	  ;; restore the point
	  (goto-char (if moving rcirc-prompt-end-marker old-point))

        ;; keep window on bottom line if it was already there
	  (when rcirc-scroll-show-maximum-output
	    (walk-windows (lambda (w)
			    (when (eq (window-buffer w) (current-buffer))
			      (with-current-buffer (window-buffer w)
				(when (eq major-mode 'rcirc-mode)
				  (with-selected-window w
 				    (when (<= (- (window-height)
 						 (count-screen-lines
						  (window-point)
						  (window-start))
						 1)
					      0)
				      (recenter -1)))))))
			  nil t))

	  ;; flush undo (can we do something smarter here?)
	  (buffer-disable-undo)
	  (buffer-enable-undo))

	;; record modeline activity
	(when (and activity
		   (not rcirc-ignore-buffer-activity-flag)
		   (not (and rcirc-dim-nicks sender
			     (string-match (regexp-opt rcirc-dim-nicks) sender))))
	      (rcirc-record-activity (current-buffer)
				     (when (not (rcirc-channel-p rcirc-target))
				       'nick)))

	(sit-for 0)			; displayed text before hook
	(run-hook-with-args 'rcirc-print-hooks
			    process sender response target text)))))

(defun rcirc-startup-channels (server)
  "Return the list of startup channels for SERVER."
  (let (channels)
    (dolist (i rcirc-startup-channels-alist)
      (if (string-match (car i) server)
          (setq channels (append channels (cdr i)))))
    channels))

(defun rcirc-join-channels (process channels)
  "Join CHANNELS."
  (save-window-excursion
    (dolist (channel channels)
      (with-rcirc-process-buffer process
	(rcirc-cmd-join channel process)))))

;;; nick management
(defvar rcirc-nick-prefix-chars "~&@%+")
(defun rcirc-user-nick (user)
  "Return the nick from USER.  Remove any non-nick junk."
  (save-match-data
    (if (string-match (concat "^[" rcirc-nick-prefix-chars
			      "]?\\([^! ]+\\)!?") (or user ""))
	(match-string 1 user)
      user)))

(defun rcirc-nick-channels (process nick)
  "Return list of channels for NICK."
  (with-rcirc-process-buffer process
    (mapcar (lambda (x) (car x))
	    (gethash nick rcirc-nick-table))))

(defun rcirc-put-nick-channel (process nick channel)
  "Add CHANNEL to list associated with NICK."
  (let ((nick (rcirc-user-nick nick)))
    (with-rcirc-process-buffer process
      (let* ((chans (gethash nick rcirc-nick-table))
	     (record (assoc-string channel chans t)))
	(if record
	    (setcdr record (current-time))
	  (puthash nick (cons (cons channel (current-time))
			      chans)
		   rcirc-nick-table))))))

(defun rcirc-nick-remove (process nick)
  "Remove NICK from table."
  (with-rcirc-process-buffer process
    (remhash nick rcirc-nick-table)))

(defun rcirc-remove-nick-channel (process nick channel)
  "Remove the CHANNEL from list associated with NICK."
  (with-rcirc-process-buffer process
    (let* ((chans (gethash nick rcirc-nick-table))
           (newchans
	    ;; instead of assoc-string-delete-all:
	    (let ((record (assoc-string channel chans t)))
	      (when record
		(setcar record 'delete)
		(assq-delete-all 'delete chans)))))
      (if newchans
          (puthash nick newchans rcirc-nick-table)
        (remhash nick rcirc-nick-table)))))

(defun rcirc-channel-nicks (process target)
  "Return the list of nicks associated with TARGET sorted by last activity."
  (when target
    (if (rcirc-channel-p target)
	(with-rcirc-process-buffer process
	  (let (nicks)
	    (maphash
	     (lambda (k v)
	       (let ((record (assoc-string target v t)))
		 (if record
		     (setq nicks (cons (cons k (cdr record)) nicks)))))
	     rcirc-nick-table)
	    (mapcar (lambda (x) (car x))
		    (sort nicks (lambda (x y) (time-less-p (cdr y) (cdr x)))))))
      (list target))))

(defun rcirc-ignore-update-automatic (nick)
  "Remove NICK from `rcirc-ignore-list'
if NICK is also on `rcirc-ignore-list-automatic'."
  (when (member nick rcirc-ignore-list-automatic)
      (setq rcirc-ignore-list-automatic
	    (delete nick rcirc-ignore-list-automatic)
	    rcirc-ignore-list
	    (delete nick rcirc-ignore-list))))

;;; activity tracking
(defvar rcirc-track-minor-mode-map (make-sparse-keymap)
  "Keymap for rcirc track minor mode.")

(define-key rcirc-track-minor-mode-map (kbd "C-c `") 'rcirc-next-active-buffer)
(define-key rcirc-track-minor-mode-map (kbd "C-c C-@") 'rcirc-next-active-buffer)
(define-key rcirc-track-minor-mode-map (kbd "C-c C-SPC") 'rcirc-next-active-buffer)

;;;###autoload
(define-minor-mode rcirc-track-minor-mode
  "Global minor mode for tracking activity in rcirc buffers."
  :init-value nil
  :lighter ""
  :keymap rcirc-track-minor-mode-map
  :global t
  :group 'rcirc
  (or global-mode-string (setq global-mode-string '("")))
  ;; toggle the mode-line channel indicator
  (if rcirc-track-minor-mode
      (progn
	(and (not (memq 'rcirc-activity-string global-mode-string))
	     (setq global-mode-string
		   (append global-mode-string '(rcirc-activity-string))))
	(add-hook 'window-configuration-change-hook
		  'rcirc-window-configuration-change))
    (setq global-mode-string
	  (delete 'rcirc-activity-string global-mode-string))
    (remove-hook 'window-configuration-change-hook
		 'rcirc-window-configuration-change)))

(or (assq 'rcirc-ignore-buffer-activity-flag minor-mode-alist)
    (setq minor-mode-alist
          (cons '(rcirc-ignore-buffer-activity-flag " Ignore") minor-mode-alist)))
(or (assq 'rcirc-low-priority-flag minor-mode-alist)
    (setq minor-mode-alist
          (cons '(rcirc-low-priority-flag " LowPri") minor-mode-alist)))

(defun rcirc-toggle-ignore-buffer-activity ()
  "Toggle the value of `rcirc-ignore-buffer-activity-flag'."
  (interactive)
  (setq rcirc-ignore-buffer-activity-flag
	(not rcirc-ignore-buffer-activity-flag))
  (message (if rcirc-ignore-buffer-activity-flag
	       "Ignore activity in this buffer"
	     "Notice activity in this buffer"))
  (force-mode-line-update))

(defun rcirc-toggle-low-priority ()
  "Toggle the value of `rcirc-low-priority-flag'."
  (interactive)
  (setq rcirc-low-priority-flag
	(not rcirc-low-priority-flag))
  (message (if rcirc-low-priority-flag
	       "Activity in this buffer is low priority"
	     "Activity in this buffer is normal priority"))
  (force-mode-line-update))

(defvar rcirc-switch-to-buffer-function 'switch-to-buffer
  "Function to use when switching buffers.
Possible values are `switch-to-buffer', `pop-to-buffer', and
`display-buffer'.")

(defun rcirc-switch-to-server-buffer ()
  "Switch to the server buffer associated with current channel buffer."
  (interactive)
  (funcall rcirc-switch-to-buffer-function rcirc-server-buffer))

(defun rcirc-jump-to-first-unread-line ()
  "Move the point to the first unread line in this buffer."
  (interactive)
  (when (marker-position overlay-arrow-position)
    (goto-char overlay-arrow-position)))

(defvar rcirc-last-non-irc-buffer nil
  "The buffer to switch to when there is no more activity.")

(defun rcirc-next-active-buffer (arg)
  "Go to the next rcirc buffer with activity.
With prefix ARG, go to the next low priority buffer with activity.
The function given by `rcirc-switch-to-buffer-function' is used to
show the buffer."
  (interactive "P")
  (let* ((pair (rcirc-split-activity rcirc-activity))
	 (lopri (car pair))
	 (hipri (cdr pair)))
    (if (or (and (not arg) hipri)
	    (and arg lopri))
	(progn
	  (unless (eq major-mode 'rcirc-mode)
	    (setq rcirc-last-non-irc-buffer (current-buffer)))
	  (funcall rcirc-switch-to-buffer-function
		   (car (if arg lopri hipri))))
      (if (eq major-mode 'rcirc-mode)
	  (if (not (and rcirc-last-non-irc-buffer
			(buffer-live-p rcirc-last-non-irc-buffer)))
	      (message "No IRC activity.  Start something.")
	    (message "No more IRC activity.  Go back to work.")
	    (funcall rcirc-switch-to-buffer-function rcirc-last-non-irc-buffer)
	    (setq rcirc-last-non-irc-buffer nil))
	(message (concat
		  "No IRC activity."
		  (when lopri
		    (concat
		     "  Type C-u "
		     (key-description (this-command-keys))
		     " for low priority activity."))))))))

(defvar rcirc-activity-hooks nil
  "Hook to be run when there is channel activity.

Functions are called with a single argument, the buffer with the
activity.  Only run if the buffer is not visible and
`rcirc-ignore-buffer-activity-flag' is non-nil.")

(defun rcirc-record-activity (buffer &optional type)
  "Record BUFFER activity with TYPE."
  (with-current-buffer buffer
    (when (not (get-buffer-window (current-buffer) t))
      (setq rcirc-activity
	    (sort (add-to-list 'rcirc-activity (current-buffer))
		  (lambda (b1 b2)
		    (let ((t1 (with-current-buffer b1 rcirc-last-post-time))
			  (t2 (with-current-buffer b2 rcirc-last-post-time)))
		      (time-less-p t2 t1)))))
      (pushnew type rcirc-activity-types)
      (rcirc-update-activity-string)))
  (run-hook-with-args 'rcirc-activity-hooks buffer))

(defun rcirc-clear-activity (buffer)
  "Clear the BUFFER activity."
  (setq rcirc-activity (delete buffer rcirc-activity))
  (with-current-buffer buffer
    (setq rcirc-activity-types nil)))

(defun rcirc-split-activity (activity)
  "Return a cons cell with ACTIVITY split into (lopri . hipri)."
  (let (lopri hipri)
    (dolist (buf rcirc-activity)
      (with-current-buffer buf
	(if (and rcirc-low-priority-flag
		 (not (member 'nick rcirc-activity-types)))
	    (add-to-list 'lopri buf t)
	  (add-to-list 'hipri buf t))))
    (cons lopri hipri)))

;; TODO: add mouse properties
(defun rcirc-update-activity-string ()
  "Update mode-line string."
  (let* ((pair (rcirc-split-activity rcirc-activity))
	 (lopri (car pair))
	 (hipri (cdr pair)))
    (setq rcirc-activity-string
	  (cond ((or hipri lopri)
		 (concat "-"
			 (and hipri "[")
			 (rcirc-activity-string hipri)
			 (and hipri lopri ",")
			 (and lopri
			      (concat "("
				      (rcirc-activity-string lopri)
				      ")"))
			 (and hipri "]")
			 "-"))
		((not (null (rcirc-process-list)))
		 "-[]-")
		(t "")))))

(defun rcirc-activity-string (buffers)
  (mapconcat (lambda (b)
	       (let ((s (substring-no-properties (rcirc-short-buffer-name b))))
		 (with-current-buffer b
		   (dolist (type rcirc-activity-types)
		     (rcirc-add-face 0 (length s)
				     (case type
				       (nick 'rcirc-track-nick)
				       (keyword 'rcirc-track-keyword))
				     s)))
		 s))
	     buffers ","))

(defun rcirc-short-buffer-name (buffer)
  "Return a short name for BUFFER to use in the modeline indicator."
  (with-current-buffer buffer
    (or rcirc-short-buffer-name (buffer-name))))

(defvar rcirc-current-buffer nil)
(defun rcirc-window-configuration-change ()
  "Go through visible windows and remove buffers from activity list.
Also, clear the overlay arrow if the current buffer is now hidden."
  (let ((current-now-hidden t))
    (walk-windows (lambda (w)
		    (let ((buf (window-buffer w)))
		      (with-current-buffer buf
			(when (eq major-mode 'rcirc-mode)
			  (rcirc-clear-activity buf)))
			(when (eq buf rcirc-current-buffer)
			  (setq current-now-hidden nil)))))
    ;; add overlay arrow if the buffer isn't displayed
    (when (and current-now-hidden
	       rcirc-current-buffer
	       (buffer-live-p rcirc-current-buffer))
      (with-current-buffer rcirc-current-buffer
	(when (and (eq major-mode 'rcirc-mode)
		   (marker-position overlay-arrow-position))
	  (set-marker overlay-arrow-position nil)))))

  ;; remove any killed buffers from list
  (setq rcirc-activity
	(delq nil (mapcar (lambda (buf) (when (buffer-live-p buf) buf))
			  rcirc-activity)))
  (rcirc-update-activity-string)
  (setq rcirc-current-buffer (current-buffer)))


;;; buffer name abbreviation
(defun rcirc-update-short-buffer-names ()
  (let ((bufalist
	 (apply 'append (mapcar (lambda (process)
				  (with-rcirc-process-buffer process
				    rcirc-buffer-alist))
				(rcirc-process-list)))))
    (dolist (i (rcirc-abbreviate bufalist))
      (when (buffer-live-p (cdr i))
	(with-current-buffer (cdr i)
	  (setq rcirc-short-buffer-name (car i)))))))

(defun rcirc-abbreviate (pairs)
  (apply 'append (mapcar 'rcirc-rebuild-tree (rcirc-make-trees pairs))))

(defun rcirc-rebuild-tree (tree &optional acc)
  (let ((ch (char-to-string (car tree))))
    (dolist (x (cdr tree))
      (if (listp x)
	  (setq acc (append acc
			   (mapcar (lambda (y)
				     (cons (concat ch (car y))
					   (cdr y)))
				   (rcirc-rebuild-tree x))))
	(setq acc (cons (cons ch x) acc))))
    acc))

(defun rcirc-make-trees (pairs)
  (let (alist)
    (mapc (lambda (pair)
	    (if (consp pair)
		(let* ((str (car pair))
		       (data (cdr pair))
		       (char (unless (zerop (length str))
			       (aref str 0)))
		       (rest (unless (zerop (length str))
			       (substring str 1)))
		       (part (if char (assq char alist))))
		  (if part
		      ;; existing partition
		      (setcdr part (cons (cons rest data) (cdr part)))
		    ;; new partition
		    (setq alist (cons (if char
					  (list char (cons rest data))
					data)
				      alist))))
	      (setq alist (cons pair alist))))
	  pairs)
    ;; recurse into cdrs of alist
    (mapc (lambda (x)
	    (when (and (listp x) (listp (cadr x)))
	      (setcdr x (if (> (length (cdr x)) 1)
			    (rcirc-make-trees (cdr x))
			  (setcdr x (list (cdadr x)))))))
	  alist)))

;;; /commands these are called with 3 args: PROCESS, TARGET, which is
;; the current buffer/channel/user, and ARGS, which is a string
;; containing the text following the /cmd.

(defmacro defun-rcirc-command (command argument docstring interactive-form
                                       &rest body)
  "Define a command."
  `(defun ,(intern (concat "rcirc-cmd-" (symbol-name command)))
     (,@argument &optional process target)
     ,(concat docstring "\n\nNote: If PROCESS or TARGET are nil, the values given"
              "\nby `rcirc-buffer-process' and `rcirc-target' will be used.")
     ,interactive-form
     (let ((process (or process (rcirc-buffer-process)))
           (target (or target rcirc-target)))
       ,@body)))

(defun-rcirc-command msg (message)
  "Send private MESSAGE to TARGET."
  (interactive "i")
  (if (null message)
      (progn
        (setq target (completing-read "Message nick: "
                                      (with-rcirc-server-buffer
					rcirc-nick-table)))
        (when (> (length target) 0)
          (setq message (read-string (format "Message %s: " target)))
          (when (> (length message) 0)
            (rcirc-send-message process target message))))
    (if (not (string-match "\\([^ ]+\\) \\(.+\\)" message))
        (message "Not enough args, or something.")
      (setq target (match-string 1 message)
            message (match-string 2 message))
      (rcirc-send-message process target message))))

(defun-rcirc-command query (nick)
  "Open a private chat buffer to NICK."
  (interactive (list (completing-read "Query nick: "
                                      (with-rcirc-server-buffer rcirc-nick-table))))
  (let ((existing-buffer (rcirc-get-buffer process nick)))
    (switch-to-buffer (or existing-buffer
			  (rcirc-get-buffer-create process nick)))
    (when (not existing-buffer)
      (rcirc-cmd-whois nick))))

(defun-rcirc-command join (channel)
  "Join CHANNEL."
  (interactive "sJoin channel: ")
  (let ((buffer (rcirc-get-buffer-create process
                                         (car (split-string channel)))))
    (rcirc-send-string process (concat "JOIN " channel))
    (when (not (eq (selected-window) (minibuffer-window)))
      (funcall rcirc-switch-to-buffer-function buffer))))

(defun-rcirc-command part (channel)
  "Part CHANNEL."
  (interactive "sPart channel: ")
  (let ((channel (if (> (length channel) 0) channel target)))
    (rcirc-send-string process (concat "PART " channel " :" rcirc-id-string))))

(defun-rcirc-command quit (reason)
  "Send a quit message to server with REASON."
  (interactive "sQuit reason: ")
  (rcirc-send-string process (concat "QUIT :"
				     (if (not (zerop (length reason)))
					 reason
				       rcirc-id-string))))

(defun-rcirc-command nick (nick)
  "Change nick to NICK."
  (interactive "i")
  (when (null nick)
    (setq nick (read-string "New nick: " (rcirc-nick process))))
  (rcirc-send-string process (concat "NICK " nick)))

(defun-rcirc-command names (channel)
  "Display list of names in CHANNEL or in current channel if CHANNEL is nil.
If called interactively, prompt for a channel when prefix arg is supplied."
  (interactive "P")
  (if (interactive-p)
      (if channel
          (setq channel (read-string "List names in channel: " target))))
  (let ((channel (if (> (length channel) 0)
                     channel
                   target)))
    (rcirc-send-string process (concat "NAMES " channel))))

(defun-rcirc-command topic (topic)
  "List TOPIC for the TARGET channel.
With a prefix arg, prompt for new topic."
  (interactive "P")
  (if (and (interactive-p) topic)
      (setq topic (read-string "New Topic: " rcirc-topic)))
  (rcirc-send-string process (concat "TOPIC " target
                                     (when (> (length topic) 0)
                                       (concat " :" topic)))))

(defun-rcirc-command whois (nick)
  "Request information from server about NICK."
  (interactive (list
                (completing-read "Whois: "
                                 (with-rcirc-server-buffer rcirc-nick-table))))
  (rcirc-send-string process (concat "WHOIS " nick)))

(defun-rcirc-command mode (args)
  "Set mode with ARGS."
  (interactive (list (concat (read-string "Mode nick or channel: ")
                             " " (read-string "Mode: "))))
  (rcirc-send-string process (concat "MODE " args)))

(defun-rcirc-command list (channels)
  "Request information on CHANNELS from server."
  (interactive "sList Channels: ")
  (rcirc-send-string process (concat "LIST " channels)))

(defun-rcirc-command oper (args)
  "Send operator command to server."
  (interactive "sOper args: ")
  (rcirc-send-string process (concat "OPER " args)))

(defun-rcirc-command quote (message)
  "Send MESSAGE literally to server."
  (interactive "sServer message: ")
  (rcirc-send-string process message))

(defun-rcirc-command kick (arg)
  "Kick NICK from current channel."
  (interactive (list
                (concat (completing-read "Kick nick: "
                                         (rcirc-channel-nicks
					  (rcirc-buffer-process)
					  rcirc-target))
                        (read-from-minibuffer "Kick reason: "))))
  (let* ((arglist (split-string arg))
         (argstring (concat (car arglist) " :"
                            (mapconcat 'identity (cdr arglist) " "))))
    (rcirc-send-string process (concat "KICK " target " " argstring))))

(defun rcirc-cmd-ctcp (args &optional process target)
  (if (string-match "^\\([^ ]+\\)\\s-+\\(.+\\)$" args)
      (let ((target (match-string 1 args))
            (request (match-string 2 args)))
        (rcirc-send-string process
			   (format "PRIVMSG %s \C-a%s\C-a"
				   target (upcase request))))
    (rcirc-print process (rcirc-nick process) "ERROR" nil
                 "usage: /ctcp NICK REQUEST")))

(defun rcirc-cmd-me (args &optional process target)
  (rcirc-send-string process (format "PRIVMSG %s :\C-aACTION %s\C-a"
                                     target args)))

(defun rcirc-add-or-remove (set &optional elt)
  (if (and elt (not (string= "" elt)))
      (if (member-ignore-case elt set)
	  (delete elt set)
	(cons elt set))
    set))

(defun-rcirc-command ignore (nick)
  "Manage the ignore list.
Ignore NICK, unignore NICK if already ignored, or list ignored
nicks when no NICK is given.  When listing ignored nicks, the
ones added to the list automatically are marked with an asterisk."
  (interactive "sToggle ignoring of nick: ")
  (setq rcirc-ignore-list (rcirc-add-or-remove rcirc-ignore-list nick))
  (rcirc-print process nil "IGNORE" target
	       (mapconcat
		(lambda (nick)
		  (concat nick
			  (if (member nick rcirc-ignore-list-automatic)
			      "*" "")))
		rcirc-ignore-list " ")))

(defun-rcirc-command bright (nick)
  "Manage the bright nick list."
  (interactive "sToggle emphasis of nick: ")
  (setq rcirc-bright-nicks (rcirc-add-or-remove rcirc-bright-nicks nick))
  (rcirc-print process nil "BRIGHT" target
	       (mapconcat 'identity rcirc-bright-nicks " ")))

(defun-rcirc-command dim (nick)
  "Manage the dim nick list."
  (interactive "sToggle deemphasis of nick: ")
  (setq rcirc-dim-nicks (rcirc-add-or-remove rcirc-dim-nicks nick))
  (rcirc-print process nil "DIM" target
	       (mapconcat 'identity rcirc-dim-nicks " ")))

(defun-rcirc-command keyword (keyword)
  "Manage the keyword list.
Mark KEYWORD, unmark KEYWORD if already marked, or list marked
keywords when no KEYWORD is given."
  (interactive "sToggle highlighting of keyword: ")
  (setq rcirc-keywords (rcirc-add-or-remove rcirc-keywords keyword))
  (rcirc-print process nil "KEYWORD" target
	       (mapconcat 'identity rcirc-keywords " ")))


(defun rcirc-add-face (start end name &optional object)
  "Add face NAME to the face text property of the text from START to END."
  (when name
    (let ((pos start)
	  next prop)
      (while (< pos end)
	(setq prop (get-text-property pos 'face object)
	      next (next-single-property-change pos 'face object end))
	(unless (member name (get-text-property pos 'face object))
	  (add-text-properties pos next (list 'face (cons name prop)) object))
	(setq pos next)))))

(defun rcirc-facify (string face)
  "Return a copy of STRING with FACE property added."
  (let ((string (or string "")))
    (rcirc-add-face 0 (length string) face string)
    string))

(defvar rcirc-url-regexp
  (rx-to-string
   `(and word-boundary
	 (or (and
	      (or (and (or "http" "https" "ftp" "file" "gopher" "news"
			   "telnet" "wais" "mailto")
		       "://")
		  "www.")
	      (1+ (char "-a-zA-Z0-9_."))
	      (1+ (char "-a-zA-Z0-9_"))
	      (optional ":" (1+ (char "0-9"))))
	     (and (1+ (char "-a-zA-Z0-9_."))
		  (or ".com" ".net" ".org")
		  word-boundary))
	 (optional
	  (and "/"
	       (1+ (char "-a-zA-Z0-9_=!?#$\@~`%&*+|\\/:;.,{}[]()"))
	       (char "-a-zA-Z0-9_=#$\@~`%&*+|\\/:;{}[]()")))))
  "Regexp matching URLs.  Set to nil to disable URL features in rcirc.")

(defun rcirc-browse-url (&optional arg)
  "Prompt for URL to browse based on URLs in buffer."
  (interactive "P")
  (let ((completions (mapcar (lambda (x) (cons x nil)) rcirc-urls))
        (initial-input (car rcirc-urls))
        (history (cdr rcirc-urls)))
    (browse-url (completing-read "rcirc browse-url: "
                                 completions nil nil initial-input 'history)
                arg)))

(defun rcirc-browse-url-at-point (point)
  "Send URL at point to `browse-url'."
  (interactive "d")
  (let ((beg (previous-single-property-change (1+ point) 'mouse-face))
	(end (next-single-property-change point 'mouse-face)))
    (browse-url (buffer-substring-no-properties beg end))))

(defun rcirc-browse-url-at-mouse (event)
  "Send URL at mouse click to `browse-url'."
  (interactive "e")
  (let ((position (event-end event)))
    (with-current-buffer (window-buffer (posn-window position))
      (rcirc-browse-url-at-point (posn-point position)))))


(defvar rcirc-markup-text-functions
  '(rcirc-markup-body-text
    rcirc-markup-attributes
    rcirc-markup-my-nick
    rcirc-markup-urls
    rcirc-markup-keywords
    rcirc-markup-bright-nicks)
  "List of functions used to manipulate text before it is printed.

Each function takes three arguments, PROCESS, SENDER, RESPONSE
and CHANNEL-BUFFER.  The current buffer is temporary buffer that
contains the text to manipulate.  Each function works on the text
in this buffer.")

(defun rcirc-markup-text (process sender response text)
  "Return TEXT with properties added based on various patterns."
  (let ((channel-buffer (current-buffer)))
    (with-temp-buffer
      (insert text)
      (goto-char (point-min))
      (dolist (fn rcirc-markup-text-functions)
	(save-excursion
	  (funcall fn process sender response channel-buffer)))
      (buffer-substring (point-min) (point-max)))))

(defun rcirc-markup-body-text (process sender response channel-buffer)
  ;; We add the text property `rcirc-text' to identify this as the
  ;; body text.
  (add-text-properties (point-min) (point-max)
		       (list 'rcirc-text (buffer-substring-no-properties
					  (point-min) (point-max)))))

(defun rcirc-markup-attributes (process sender response channel-buffer)
  (while (re-search-forward "\\([\C-b\C-_\C-v]\\).*?\\(\\1\\|\C-o\\)" nil t)
    (rcirc-add-face (match-beginning 0) (match-end 0)
		    (case (char-after (match-beginning 1))
		      (?\C-b 'bold)
		      (?\C-v 'italic)
		      (?\C-_ 'underline)))
    ;; keep the ^O since it could terminate other attributes
    (when (not (eq ?\C-o (char-before (match-end 2))))
      (delete-region (match-beginning 2) (match-end 2)))
    (delete-region (match-beginning 1) (match-end 1))
    (goto-char (1+ (match-beginning 1))))
  ;; remove the ^O characters now
  (while (re-search-forward "\C-o+" nil t)
    (delete-region (match-beginning 0) (match-end 0))))

(defun rcirc-markup-my-nick (process sender response channel-buffer)
  (with-syntax-table rcirc-nick-syntax-table
    (while (re-search-forward (concat "\\b"
				      (regexp-quote (rcirc-nick process))
				      "\\b")
			      nil t)
      (rcirc-add-face (match-beginning 0) (match-end 0)
		      'rcirc-nick-in-message)
      (when (string= response "PRIVMSG")
	(rcirc-add-face (point-min) (point-max) 'rcirc-nick-in-message-full-line)
	(rcirc-record-activity channel-buffer 'nick)))))

(defun rcirc-markup-urls (process sender response channel-buffer)
  (while (re-search-forward rcirc-url-regexp nil t)
    (let ((start (match-beginning 0))
	  (end (match-end 0)))
      (rcirc-add-face start end 'rcirc-url)
      (add-text-properties start end (list 'mouse-face 'highlight
					   'keymap rcirc-browse-url-map))
      ;; record the url
      (let ((url (buffer-substring-no-properties start end)))
	(with-current-buffer channel-buffer
	  (push url rcirc-urls))))))

(defun rcirc-markup-keywords (process sender response channel-buffer)
  (let* ((target (with-current-buffer channel-buffer (or rcirc-target "")))
	 (keywords (delq nil (mapcar (lambda (keyword)
				      (when (not (string-match keyword target))
					keyword))
				    rcirc-keywords))))
    (when keywords
      (while (re-search-forward (regexp-opt keywords 'words) nil t)
	(rcirc-add-face (match-beginning 0) (match-end 0) 'rcirc-keyword)
	(when (and (string= response "PRIVMSG")
		   (not (string= sender (rcirc-nick process))))
	  (rcirc-record-activity channel-buffer 'keyword))))))

(defun rcirc-markup-bright-nicks (process sender response channel-buffer)
  (when (and rcirc-bright-nicks
	     (string= response "NAMES"))
    (with-syntax-table rcirc-nick-syntax-table
      (while (re-search-forward (regexp-opt rcirc-bright-nicks 'words) nil t)
	(rcirc-add-face (match-beginning 0) (match-end 0)
			'rcirc-bright-nick)))))

;;; handlers
;; these are called with the server PROCESS, the SENDER, which is a
;; server or a user, depending on the command, the ARGS, which is a
;; list of strings, and the TEXT, which is the original server text,
;; verbatim
(defun rcirc-handler-001 (process sender args text)
  (rcirc-handler-generic process "001" sender args text)
  ;; set the real server name
  (with-rcirc-process-buffer process
    (setq rcirc-connecting nil)
    (rcirc-reschedule-timeout process)
    (setq rcirc-server-name sender)
    (setq rcirc-nick (car args))
    (rcirc-update-prompt)
    (when rcirc-auto-authenticate-flag (rcirc-authenticate))
    (rcirc-join-channels process rcirc-startup-channels)))

(defun rcirc-handler-PRIVMSG (process sender args text)
  (let ((target (if (rcirc-channel-p (car args))
                    (car args)
                  sender))
        (message (or (cadr args) "")))
    (if (string-match "^\C-a\\(.*\\)\C-a$" message)
        (rcirc-handler-CTCP process target sender (match-string 1 message))
      (rcirc-print process sender "PRIVMSG" target message t))
    ;; update nick timestamp
    (if (member target (rcirc-nick-channels process sender))
        (rcirc-put-nick-channel process sender target))))

(defun rcirc-handler-NOTICE (process sender args text)
  (let ((target (car args))
        (message (cadr args)))
    (if (string-match "^\C-a\\(.*\\)\C-a$" message)
        (rcirc-handler-CTCP-response process target sender
				     (match-string 1 message))
      (rcirc-print process sender "NOTICE"
		   (cond ((rcirc-channel-p target)
			  target)
			 ;;; -ChanServ- [#gnu] Welcome...
			 ((string-match "\\[\\(#[^\] ]+\\)\\]" message)
			  (match-string 1 message))
			 (sender
			  (if (string= sender (rcirc-server-name process))
			      nil	; server notice
			    sender)))
                 message t))))

(defun rcirc-handler-WALLOPS (process sender args text)
  (rcirc-print process sender "WALLOPS" sender (car args) t))

(defun rcirc-handler-JOIN (process sender args text)
  (let ((channel (car args)))
    (rcirc-get-buffer-create process channel)
    (rcirc-print process sender "JOIN" channel "")

    ;; print in private chat buffer if it exists
    (when (rcirc-get-buffer (rcirc-buffer-process) sender)
      (rcirc-print process sender "JOIN" sender channel))

    (rcirc-put-nick-channel process sender channel)))

;; PART and KICK are handled the same way
(defun rcirc-handler-PART-or-KICK (process response channel sender nick args)
  (rcirc-ignore-update-automatic nick)
  (if (not (string= nick (rcirc-nick process)))
      ;; this is someone else leaving
      (rcirc-remove-nick-channel process nick channel)
    ;; this is us leaving
    (mapc (lambda (n)
	    (rcirc-remove-nick-channel process n channel))
	  (rcirc-channel-nicks process channel))

    ;; if the buffer is still around, make it inactive
    (let ((buffer (rcirc-get-buffer process channel)))
      (when buffer
	(with-current-buffer buffer
	  (setq rcirc-target nil))))))

(defun rcirc-handler-PART (process sender args text)
  (let* ((channel (car args))
	 (reason (cadr args))
	 (message (concat channel " " reason)))
    (rcirc-print process sender "PART" channel message)
    ;; print in private chat buffer if it exists
    (when (rcirc-get-buffer (rcirc-buffer-process) sender)
      (rcirc-print process sender "PART" sender message))

    (rcirc-handler-PART-or-KICK process "PART" channel sender sender reason)))

(defun rcirc-handler-KICK (process sender args text)
  (let* ((channel (car args))
	 (nick (cadr args))
	 (reason (caddr args))
	 (message (concat nick " " channel " " reason)))
    (rcirc-print process sender "KICK" channel message t)
    ;; print in private chat buffer if it exists
    (when (rcirc-get-buffer (rcirc-buffer-process) nick)
      (rcirc-print process sender "KICK" nick message))

    (rcirc-handler-PART-or-KICK process "KICK" channel sender nick reason)))

(defun rcirc-handler-QUIT (process sender args text)
  (rcirc-ignore-update-automatic sender)
  (mapc (lambda (channel)
	  (rcirc-print process sender "QUIT" channel (apply 'concat args)))
	(rcirc-nick-channels process sender))

  ;; print in private chat buffer if it exists
  (when (rcirc-get-buffer (rcirc-buffer-process) sender)
    (rcirc-print process sender "QUIT" sender (apply 'concat args)))

  (rcirc-nick-remove process sender))

(defun rcirc-handler-NICK (process sender args text)
  (let* ((old-nick sender)
         (new-nick (car args))
         (channels (rcirc-nick-channels process old-nick)))
    ;; update list of ignored nicks
    (rcirc-ignore-update-automatic old-nick)
    (when (member old-nick rcirc-ignore-list)
      (add-to-list 'rcirc-ignore-list new-nick)
      (add-to-list 'rcirc-ignore-list-automatic new-nick))
    ;; print message to nick's channels
    (dolist (target channels)
      (rcirc-print process sender "NICK" target new-nick))
    ;; update private chat buffer, if it exists
    (let ((chat-buffer (rcirc-get-buffer process old-nick)))
      (when chat-buffer
	(with-current-buffer chat-buffer
	  (rcirc-print process sender "NICK" old-nick new-nick)
	  (setq rcirc-target new-nick)
	  (rename-buffer (rcirc-generate-new-buffer-name process new-nick)))))
    ;; remove old nick and add new one
    (with-rcirc-process-buffer process
      (let ((v (gethash old-nick rcirc-nick-table)))
        (remhash old-nick rcirc-nick-table)
        (puthash new-nick v rcirc-nick-table))
      ;; if this is our nick...
      (when (string= old-nick rcirc-nick)
        (setq rcirc-nick new-nick)
	(rcirc-update-prompt t)
        ;; reauthenticate
        (when rcirc-auto-authenticate-flag (rcirc-authenticate))))))

(defun rcirc-handler-PING (process sender args text)
  (rcirc-send-string process (concat "PONG " (car args))))

(defun rcirc-handler-PONG (process sender args text)
  ;; do nothing
  )

(defun rcirc-handler-TOPIC (process sender args text)
  (let ((topic (cadr args)))
    (rcirc-print process sender "TOPIC" (car args) topic)
    (with-current-buffer (rcirc-get-buffer process (car args))
      (setq rcirc-topic topic))))

(defvar rcirc-nick-away-alist nil)
(defun rcirc-handler-301 (process sender args text)
  "RPL_AWAY"
  (let* ((nick (cadr args))
	 (rec (assoc-string nick rcirc-nick-away-alist))
	 (away-message (caddr args)))
    (when (or (not rec)
	      (not (string= (cdr rec) away-message)))
      ;; away message has changed
      (rcirc-handler-generic process "AWAY" nick (cdr args) text)
      (if rec
	  (setcdr rec away-message)
	(setq rcirc-nick-away-alist (cons (cons nick away-message)
					  rcirc-nick-away-alist))))))

(defun rcirc-handler-332 (process sender args text)
  "RPL_TOPIC"
  (let ((buffer (or (rcirc-get-buffer process (cadr args))
		    (rcirc-get-temp-buffer-create process (cadr args)))))
    (with-current-buffer buffer
      (setq rcirc-topic (caddr args)))))

(defun rcirc-handler-333 (process sender args text)
  "Not in rfc1459.txt"
  (let ((buffer (or (rcirc-get-buffer process (cadr args))
		    (rcirc-get-temp-buffer-create process (cadr args)))))
    (with-current-buffer buffer
      (let ((setter (caddr args))
	    (time (current-time-string
		   (seconds-to-time
		    (string-to-number (cadddr args))))))
	(rcirc-print process sender "TOPIC" (cadr args)
		     (format "%s (%s on %s)" rcirc-topic setter time))))))

(defun rcirc-handler-477 (process sender args text)
  "ERR_NOCHANMODES"
  (rcirc-print process sender "477" (cadr args) (caddr args)))

(defun rcirc-handler-MODE (process sender args text)
  (let ((target (car args))
        (msg (mapconcat 'identity (cdr args) " ")))
    (rcirc-print process sender "MODE"
                 (if (string= target (rcirc-nick process))
                     nil
                   target)
                 msg)

    ;; print in private chat buffers if they exist
    (mapc (lambda (nick)
	    (when (rcirc-get-buffer process nick)
	      (rcirc-print process sender "MODE" nick msg)))
	  (cddr args))))

(defun rcirc-get-temp-buffer-create (process channel)
  "Return a buffer based on PROCESS and CHANNEL."
  (let ((tmpnam (concat " " (downcase channel) "TMP" (process-name process))))
    (get-buffer-create tmpnam)))

(defun rcirc-handler-353 (process sender args text)
  "RPL_NAMREPLY"
  (let ((channel (caddr args)))
    (mapc (lambda (nick)
            (rcirc-put-nick-channel process nick channel))
          (split-string (cadddr args) " " t))
    (with-current-buffer (rcirc-get-temp-buffer-create process channel)
      (goto-char (point-max))
      (insert (car (last args)) " "))))

(defun rcirc-handler-366 (process sender args text)
  "RPL_ENDOFNAMES"
  (let* ((channel (cadr args))
         (buffer (rcirc-get-temp-buffer-create process channel)))
    (with-current-buffer buffer
      (rcirc-print process sender "NAMES" channel
                   (buffer-substring (point-min) (point-max))))
    (kill-buffer buffer)))

(defun rcirc-handler-433 (process sender args text)
  "ERR_NICKNAMEINUSE"
  (rcirc-handler-generic process "433" sender args text)
  (let* ((new-nick (concat (cadr args) "`")))
    (with-rcirc-process-buffer process
      (rcirc-cmd-nick new-nick nil process))))

(defun rcirc-authenticate ()
  "Send authentication to process associated with current buffer.
Passwords are stored in `rcirc-authinfo' (which see)."
  (interactive)
  (with-rcirc-server-buffer
    (dolist (i rcirc-authinfo)
      (let ((process (rcirc-buffer-process))
	    (server (car i))
	    (nick (caddr i))
	    (method (cadr i))
	    (args (cdddr i)))
	(when (and (string-match server rcirc-server)
		   (string-match nick rcirc-nick))
	  (cond ((equal method 'nickserv)
		 (rcirc-send-string
		  process
		  (concat
		   "PRIVMSG nickserv :identify "
		   (car args))))
		((equal method 'chanserv)
		 (rcirc-send-string
		  process
		  (concat
		   "PRIVMSG chanserv :identify "
		   (cadr args) " " (car args))))
		((equal method 'bitlbee)
		 (rcirc-send-string
		  process
		  (concat "PRIVMSG &bitlbee :identify " (car args))))
		(t
		 (message "No %S authentication method defined"
			  method))))))))

(defun rcirc-handler-INVITE (process sender args text)
  (rcirc-print process sender "INVITE" nil (mapconcat 'identity args " ") t))

(defun rcirc-handler-ERROR (process sender args text)
  (rcirc-print process sender "ERROR" nil (mapconcat 'identity args " ")))

(defun rcirc-handler-CTCP (process target sender text)
  (if (string-match "^\\([^ ]+\\) *\\(.*\\)$" text)
      (let* ((request (upcase (match-string 1 text)))
             (args (match-string 2 text))
             (handler (intern-soft (concat "rcirc-handler-ctcp-" request))))
        (if (not (fboundp handler))
            (rcirc-print process sender "ERROR" target
                         (format "%s sent unsupported ctcp: %s" sender text)
			 t)
          (funcall handler process target sender args)
          (if (not (string= request "ACTION"))
              (rcirc-print process sender "CTCP" target
			   (format "%s" text) t))))))

(defun rcirc-handler-ctcp-VERSION (process target sender args)
  (rcirc-send-string process
                     (concat "NOTICE " sender
                             " :\C-aVERSION " rcirc-id-string
                             "\C-a")))

(defun rcirc-handler-ctcp-ACTION (process target sender args)
  (rcirc-print process sender "ACTION" target args t))

(defun rcirc-handler-ctcp-TIME (process target sender args)
  (rcirc-send-string process
                     (concat "NOTICE " sender
                             " :\C-aTIME " (current-time-string) "\C-a")))

(defun rcirc-handler-CTCP-response (process target sender message)
  (rcirc-print process sender "CTCP" nil message t))

(defgroup rcirc-faces nil
  "Faces for rcirc."
  :group 'rcirc
  :group 'faces)

(defface rcirc-my-nick			; font-lock-function-name-face
  '((((class color) (min-colors 88) (background light)) (:foreground "Blue1"))
    (((class color) (min-colors 88) (background dark)) (:foreground "LightSkyBlue"))
    (((class color) (min-colors 16) (background light)) (:foreground "Blue"))
    (((class color) (min-colors 16) (background dark)) (:foreground "LightSkyBlue"))
    (((class color) (min-colors 8)) (:foreground "blue" :weight bold))
    (t (:inverse-video t :weight bold)))
  "The face used to highlight my messages."
  :group 'rcirc-faces)

(defface rcirc-other-nick	     ; font-lock-variable-name-face
  '((((class grayscale) (background light))
     (:foreground "Gray90" :weight bold :slant italic))
    (((class grayscale) (background dark))
     (:foreground "DimGray" :weight bold :slant italic))
    (((class color) (min-colors 88) (background light)) (:foreground "DarkGoldenrod"))
    (((class color) (min-colors 88) (background dark)) (:foreground "LightGoldenrod"))
    (((class color) (min-colors 16) (background light)) (:foreground "DarkGoldenrod"))
    (((class color) (min-colors 16) (background dark)) (:foreground "LightGoldenrod"))
    (((class color) (min-colors 8)) (:foreground "yellow" :weight light))
    (t (:weight bold :slant italic)))
  "The face used to highlight other messages."
  :group 'rcirc-faces)

(defface rcirc-bright-nick
  '((((class grayscale) (background light))
     (:foreground "LightGray" :weight bold :underline t))
    (((class grayscale) (background dark))
     (:foreground "Gray50" :weight bold :underline t))
    (((class color) (min-colors 88) (background light)) (:foreground "CadetBlue"))
    (((class color) (min-colors 88) (background dark)) (:foreground "Aquamarine"))
    (((class color) (min-colors 16) (background light)) (:foreground "CadetBlue"))
    (((class color) (min-colors 16) (background dark)) (:foreground "Aquamarine"))
    (((class color) (min-colors 8)) (:foreground "magenta"))
    (t (:weight bold :underline t)))
  "Face used for nicks matched by `rcirc-bright-nicks'."
  :group 'rcirc-faces)

(defface rcirc-dim-nick
  '((t :inherit default))
  "Face used for nicks in `rcirc-dim-nicks'."
  :group 'rcirc-faces)

(defface rcirc-server			; font-lock-comment-face
  '((((class grayscale) (background light))
     (:foreground "DimGray" :weight bold :slant italic))
    (((class grayscale) (background dark))
     (:foreground "LightGray" :weight bold :slant italic))
    (((class color) (min-colors 88) (background light))
     (:foreground "Firebrick"))
    (((class color) (min-colors 88) (background dark))
     (:foreground "chocolate1"))
    (((class color) (min-colors 16) (background light))
     (:foreground "red"))
    (((class color) (min-colors 16) (background dark))
     (:foreground "red1"))
    (((class color) (min-colors 8) (background light))
     )
    (((class color) (min-colors 8) (background dark))
     )
    (t (:weight bold :slant italic)))
  "The face used to highlight server messages."
  :group 'rcirc-faces)

(defface rcirc-server-prefix	 ; font-lock-comment-delimiter-face
  '((default :inherit rcirc-server)
    (((class grayscale)))
    (((class color) (min-colors 16)))
    (((class color) (min-colors 8) (background light))
     :foreground "red")
    (((class color) (min-colors 8) (background dark))
     :foreground "red1"))
  "The face used to highlight server prefixes."
  :group 'rcirc-faces)

(defface rcirc-timestamp
  '((t (:inherit default)))
  "The face used to highlight timestamps."
  :group 'rcirc-faces)

(defface rcirc-nick-in-message		; font-lock-keyword-face
  '((((class grayscale) (background light)) (:foreground "LightGray" :weight bold))
    (((class grayscale) (background dark)) (:foreground "DimGray" :weight bold))
    (((class color) (min-colors 88) (background light)) (:foreground "Purple"))
    (((class color) (min-colors 88) (background dark)) (:foreground "Cyan1"))
    (((class color) (min-colors 16) (background light)) (:foreground "Purple"))
    (((class color) (min-colors 16) (background dark)) (:foreground "Cyan"))
    (((class color) (min-colors 8)) (:foreground "cyan" :weight bold))
    (t (:weight bold)))
  "The face used to highlight instances of your nick within messages."
  :group 'rcirc-faces)

(defface rcirc-nick-in-message-full-line
  '((t (:bold t)))
  "The face used emphasize the entire message when your nick is mentioned."
  :group 'rcirc-faces)

(defface rcirc-prompt			; comint-highlight-prompt
  '((((min-colors 88) (background dark)) (:foreground "cyan1"))
    (((background dark)) (:foreground "cyan"))
    (t (:foreground "dark blue")))
  "The face used to highlight prompts."
  :group 'rcirc-faces)

(defface rcirc-track-nick
  '((((type tty)) (:inherit default))
    (t (:inverse-video t)))
  "The face used in the mode-line when your nick is mentioned."
  :group 'rcirc-faces)

(defface rcirc-track-keyword
  '((t (:bold t )))
  "The face used in the mode-line when keywords are mentioned."
  :group 'rcirc-faces)

(defface rcirc-url
  '((t (:bold t)))
  "The face used to highlight urls."
  :group 'rcirc-faces)

(defface rcirc-keyword
  '((t (:inherit highlight)))
  "The face used to highlight keywords."
  :group 'rcirc-faces)


;; When using M-x flyspell-mode, only check words after the prompt
(put 'rcirc-mode 'flyspell-mode-predicate 'rcirc-looking-at-input)
(defun rcirc-looking-at-input ()
  "Returns true if point is past the input marker."
  (>= (point) rcirc-prompt-end-marker))


(provide 'rcirc)

;; arch-tag: b471b7e8-6b5a-4399-b2c6-a3c78dfc8ffb
;;; rcirc.el ends here
