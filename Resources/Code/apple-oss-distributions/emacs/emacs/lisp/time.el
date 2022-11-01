;;; time.el --- display time, load and mail indicator in mode line of Emacs -*-coding: utf-8 -*-

;; Copyright (C) 1985, 1986, 1987, 1993, 1994, 1996, 2000, 2001, 2002,
;;   2003, 2004, 2005, 2006, 2007 Free Software Foundation, Inc.

;; Maintainer: FSF

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

;; Facilities to display current time/date and a new-mail indicator
;; in the Emacs mode line.  The single entry point is `display-time'.

;;; Code:

(defgroup display-time nil
  "Display time and load in mode line of Emacs."
  :group 'mode-line
  :group 'mail)


(defcustom display-time-mail-file nil
  "*File name of mail inbox file, for indicating existence of new mail.
Non-nil and not a string means don't check for mail; nil means use
default, which is system-dependent, and is the same as used by Rmail."
  :type '(choice (const :tag "None" none)
		 (const :tag "Default" nil)
		 (file :format "%v"))
  :group 'display-time)

(defcustom display-time-mail-directory nil
  "*Name of mail inbox directory, for indicating existence of new mail.
Any nonempty regular file in the directory is regarded as newly arrived mail.
If nil, do not check a directory for arriving mail."
  :type '(choice (const :tag "None" nil)
		 (directory :format "%v"))
  :group 'display-time)

(defcustom display-time-mail-function nil
  "*Function to call, for indicating existence of new mail.
If nil, that means use the default method: check that the file
specified by `display-time-mail-file' is nonempty or that the
directory `display-time-mail-directory' contains nonempty files."
  :type '(choice (const :tag "Default" nil)
		 (function))
  :group 'display-time)

(defcustom display-time-default-load-average 0
  "*Which load average value will be shown in the mode line.
Almost every system can provide values of load for past 1 minute, past 5 or
past 15 minutes.  The default is to display 1 minute load average."
  :type '(choice (const :tag "1 minute load" 0)
		 (const :tag "5 minutes load" 1)
		 (const :tag "15 minutes load" 2)
		 (const :tag "None" nil))
  :group 'display-time)

(defvar display-time-load-average nil
  "Load average currently being shown in mode line.")

(defcustom display-time-load-average-threshold 0.1
  "*Load-average values below this value won't be shown in the mode line."
  :type 'number
  :group 'display-time)

;;;###autoload
(defcustom display-time-day-and-date nil "\
*Non-nil means \\[display-time] should display day and date as well as time."
  :type 'boolean
  :group 'display-time)

(defvar display-time-timer nil)

(defcustom display-time-interval 60
  "*Seconds between updates of time in the mode line."
  :type 'integer
  :group 'display-time)

(defcustom display-time-24hr-format nil
  "*Non-nil indicates time should be displayed as hh:mm, 0 <= hh <= 23.
A value of nil means 1 <= hh <= 12, and an AM/PM suffix is used."
  :type 'boolean
  :group 'display-time)

(defvar display-time-string nil)

(defcustom display-time-hook nil
  "*List of functions to be called when the time is updated on the mode line."
  :type 'hook
  :group 'display-time)

(defvar display-time-server-down-time nil
   "Time when mail file's file system was recorded to be down.
If that file system seems to be up, the value is nil.")

;;;###autoload
(defun display-time ()
  "Enable display of time, load level, and mail flag in mode lines.
This display updates automatically every minute.
If `display-time-day-and-date' is non-nil, the current day and date
are displayed as well.
This runs the normal hook `display-time-hook' after each update."
  (interactive)
  (display-time-mode 1))

;; This business used to be simpler when all mode lines had the same
;; face and the image could just be pbm.  Now we try to rely on an xpm
;; image with a transparent background.  Otherwise, set the background
;; for pbm.

(defcustom display-time-mail-face nil
  "Face to use for `display-time-mail-string'.
If `display-time-use-mail-icon' is non-nil, the image's
background color is the background of this face.  Set this to
make the mail indicator stand out on a color display."
  :group 'mode-line-faces
  :group 'display-time
  :version "22.1"
  :type '(choice (const :tag "None" nil) face))

(defvar display-time-mail-icon
  (find-image '((:type xpm :file "letter.xpm" :ascent center)
		(:type pbm :file "letter.pbm" :ascent center)))
  "Image specification to offer as the mail indicator on a graphic display.
See `display-time-use-mail-icon' and `display-time-mail-face'.")

;; Fixme: Default to icon on graphical display?
(defcustom display-time-use-mail-icon nil
  "Non-nil means use an icon as mail indicator on a graphic display.
Otherwise use `display-time-mail-string'.  The icon may consume less
of the mode line.  It is specified by `display-time-mail-icon'."
  :group 'display-time
  :type 'boolean)

;; Fixme: maybe default to the character if we can display Unicode.
(defcustom display-time-mail-string "Mail"
  "String to use as the mail indicator in `display-time-string-forms'.
This can use the Unicode letter character if you can display it."
  :group 'display-time
  :version "22.1"
  :type '(choice (const "Mail")
		 ;; Use :tag here because the Lucid menu won't display
		 ;; multibyte text.
		 (const :tag "Unicode letter character" "✉")
		 string))

(defcustom display-time-format nil
  "*String specifying format for displaying the time in the mode line.
See the function `format-time-string' for an explanation of
how to write this string.  If this is nil, the defaults
depend on `display-time-day-and-date' and `display-time-24hr-format'."
  :type '(choice (const :tag "Default" nil)
		 string)
  :group 'display-time)

(defcustom display-time-string-forms
  '((if (and (not display-time-format) display-time-day-and-date)
	(format-time-string "%a %b %e " now)
      "")
    (propertize
     (format-time-string (or display-time-format
			     (if display-time-24hr-format "%H:%M" "%-I:%M%p"))
			 now)
     'help-echo (format-time-string "%a %b %e, %Y" now))
    load
    (if mail
	;; Build the string every time to act on customization.
	;; :set-after doesn't help for `customize-option'.  I think it
	;; should.
	(concat
	 " "
	 (propertize
	  display-time-mail-string
	  'display `(when (and display-time-use-mail-icon
			       (display-graphic-p))
		      ,@display-time-mail-icon
		      ,@(if (and display-time-mail-face
				 (memq (plist-get (cdr display-time-mail-icon)
						  :type)
				       '(pbm xbm)))
			    (let ((bg (face-attribute display-time-mail-face
						      :background)))
			      (if (stringp bg)
				  (list :background bg)))))
	  'face display-time-mail-face
	  'help-echo "You have new mail; mouse-2: Read mail"
	  'mouse-face 'mode-line-highlight
	  'local-map (make-mode-line-mouse-map 'mouse-2
					       read-mail-command)))
      ""))
  "*List of expressions governing display of the time in the mode line.
For most purposes, you can control the time format using `display-time-format'
which is a more standard interface.

This expression is a list of expressions that can involve the keywords
`load', `day', `month', and `year', `12-hours', `24-hours', `minutes',
`seconds', all numbers in string form, and `monthname', `dayname', `am-pm',
and `time-zone' all alphabetic strings, and `mail' a true/nil value.

For example, the form

  '((substring year -2) \"/\" month \"/\" day
    \" \" 24-hours \":\" minutes \":\" seconds
    (if time-zone \" (\") time-zone (if time-zone \")\")
    (if mail \" Mail\" \"\"))

would give mode line times like `94/12/30 21:07:48 (UTC)'."
  :type 'sexp
  :group 'display-time)

(defun display-time-event-handler ()
  (display-time-update)
  ;; Do redisplay right now, if no input pending.
  (sit-for 0)
  (let* ((current (current-time))
	 (timer display-time-timer)
	 ;; Compute the time when this timer will run again, next.
	 (next-time (timer-relative-time
		     (list (aref timer 1) (aref timer 2) (aref timer 3))
		     (* 5 (aref timer 4)) 0)))
    ;; If the activation time is far in the past,
    ;; skip executions until we reach a time in the future.
    ;; This avoids a long pause if Emacs has been suspended for hours.
    (or (> (nth 0 next-time) (nth 0 current))
	(and (= (nth 0 next-time) (nth 0 current))
	     (> (nth 1 next-time) (nth 1 current)))
	(and (= (nth 0 next-time) (nth 0 current))
	     (= (nth 1 next-time) (nth 1 current))
	     (> (nth 2 next-time) (nth 2 current)))
	(progn
	  (timer-set-time timer (timer-next-integral-multiple-of-time
				 current display-time-interval)
			  display-time-interval)
	  (timer-activate timer)))))

(defun display-time-next-load-average ()
  (interactive)
  (if (= 3 (setq display-time-load-average (1+ display-time-load-average)))
      (setq display-time-load-average 0))
  (display-time-update)
  (sit-for 0))

(defun display-time-mail-check-directory ()
  (let ((mail-files (directory-files display-time-mail-directory t))
	(size 0))
    (while (and mail-files (= size 0))
      ;; Count size of regular files only.
      (setq size (+ size (or (and (file-regular-p (car mail-files))
				  (nth 7 (file-attributes (car mail-files))))
			     0)))
      (setq mail-files (cdr mail-files)))
    (if (> size 0)
	size
      nil)))

(defun display-time-update ()
  "Update the display-time info for the mode line.
However, don't redisplay right now.

This is used for things like Rmail `g' that want to force an
update which can wait for the next redisplay."
  (let* ((now (current-time))
	 (time (current-time-string now))
         (load (if (null display-time-load-average)
		   ""
		 (condition-case ()
		     ;; Do not show values less than
		     ;; `display-time-load-average-threshold'.
		     (if (> (* display-time-load-average-threshold 100)
			    (nth display-time-load-average (load-average)))
			 ""
		       ;; The load average number is mysterious, so
		       ;; provide some help.
		       (let ((str (format " %03d"
					  (nth display-time-load-average
					       (load-average)))))
			 (propertize
			  (concat (substring str 0 -2) "." (substring str -2))
			  'local-map (make-mode-line-mouse-map
				      'mouse-2 'display-time-next-load-average)
			  'mouse-face 'mode-line-highlight
			  'help-echo (concat
				      "System load average for past "
				      (if (= 0 display-time-load-average)
					  "1 minute"
					(if (= 1 display-time-load-average)
					    "5 minutes"
					  "15 minutes"))
				      "; mouse-2: next"))))
		   (error ""))))
         (mail-spool-file (or display-time-mail-file
                              (getenv "MAIL")
                              (concat rmail-spool-directory
                                      (user-login-name))))
	 (mail (or (and display-time-mail-function
			(funcall display-time-mail-function))
		   (and display-time-mail-directory
			(display-time-mail-check-directory))
		   (and (stringp mail-spool-file)
			(or (null display-time-server-down-time)
			    ;; If have been down for 20 min, try again.
			    (> (- (nth 1 now) display-time-server-down-time)
			       1200)
			    (and (< (nth 1 now) display-time-server-down-time)
				 (> (- (nth 1 now)
				       display-time-server-down-time)
				    -64336)))
			(let ((start-time (current-time)))
			  (prog1
			      (display-time-file-nonempty-p mail-spool-file)
			    (if (> (- (nth 1 (current-time))
				      (nth 1 start-time))
				   20)
				;; Record that mail file is not accessible.
				(setq display-time-server-down-time
				      (nth 1 (current-time)))
			      ;; Record that mail file is accessible.
			      (setq display-time-server-down-time nil)))))))
         (24-hours (substring time 11 13))
         (hour (string-to-number 24-hours))
         (12-hours (int-to-string (1+ (% (+ hour 11) 12))))
         (am-pm (if (>= hour 12) "pm" "am"))
         (minutes (substring time 14 16))
         (seconds (substring time 17 19))
         (time-zone (car (cdr (current-time-zone now))))
         (day (substring time 8 10))
         (year (substring time 20 24))
         (monthname (substring time 4 7))
         (month
          (cdr
           (assoc
            monthname
            '(("Jan" . "1") ("Feb" . "2") ("Mar" . "3") ("Apr" . "4")
              ("May" . "5") ("Jun" . "6") ("Jul" . "7") ("Aug" . "8")
              ("Sep" . "9") ("Oct" . "10") ("Nov" . "11") ("Dec" . "12")))))
         (dayname (substring time 0 3)))
    (setq display-time-string
          (mapconcat 'eval display-time-string-forms ""))
    ;; This is inside the let binding, but we are not going to document
    ;; what variables are available.
    (run-hooks 'display-time-hook))
  (force-mode-line-update))

(defun display-time-file-nonempty-p (file)
  (and (file-exists-p file)
       (< 0 (nth 7 (file-attributes (file-chase-links file))))))

;;;###autoload
(define-minor-mode display-time-mode
  "Toggle display of time, load level, and mail flag in mode lines.
With a numeric arg, enable this display if arg is positive.

When this display is enabled, it updates automatically every minute.
If `display-time-day-and-date' is non-nil, the current day and date
are displayed as well.
This runs the normal hook `display-time-hook' after each update."
  :global t :group 'display-time
  (and display-time-timer (cancel-timer display-time-timer))
  (setq display-time-timer nil)
  (setq display-time-string "")
  (or global-mode-string (setq global-mode-string '("")))
  (setq display-time-load-average display-time-default-load-average)
  (if display-time-mode
      (progn
	(or (memq 'display-time-string global-mode-string)
	    (setq global-mode-string
		  (append global-mode-string '(display-time-string))))
	;; Set up the time timer.
	(setq display-time-timer
	      (run-at-time t display-time-interval
			   'display-time-event-handler))
	;; Make the time appear right away.
	(display-time-update)
	;; When you get new mail, clear "Mail" from the mode line.
	(add-hook 'rmail-after-get-new-mail-hook
		  'display-time-event-handler))
    (remove-hook 'rmail-after-get-new-mail-hook
		 'display-time-event-handler)))

(provide 'time)

;;; arch-tag: b9c1623f-b5cb-48e4-b650-482a4d23c5a6
;;; time.el ends here
