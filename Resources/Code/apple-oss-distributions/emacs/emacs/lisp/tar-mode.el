;;; tar-mode.el --- simple editing of tar files from GNU emacs

;; Copyright (C) 1990, 1991, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
;;   2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007 Free Software Foundation, Inc.

;; Author: Jamie Zawinski <jwz@lucid.com>
;; Maintainer: FSF
;; Created: 04 Apr 1990
;; Keywords: unix

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

;; This package attempts to make dealing with Unix 'tar' archives easier.
;; When this code is loaded, visiting a file whose name ends in '.tar' will
;; cause the contents of that archive file to be displayed in a Dired-like
;; listing.  It is then possible to use the customary Dired keybindings to
;; extract sub-files from that archive, either by reading them into their own
;; editor buffers, or by copying them directly to arbitrary files on disk.
;; It is also possible to delete sub-files from within the tar file and write
;; the modified archive back to disk, or to edit sub-files within the archive
;; and re-insert the modified files into the archive.  See the documentation
;; string of tar-mode for more info.

;; This code now understands the extra fields that GNU tar adds to tar files.

;; This interacts correctly with "uncompress.el" in the Emacs library,
;; which you get with
;;
;;  (autoload 'uncompress-while-visiting "uncompress")
;;  (setq auto-mode-alist (cons '("\\.Z$" . uncompress-while-visiting)
;;			   auto-mode-alist))
;;
;; Do not attempt to use tar-mode.el with crypt.el, you will lose.

;;    ***************   TO DO   ***************
;;
;; o  chmod should understand "a+x,og-w".
;;
;; o  It's not possible to add a NEW file to a tar archive; not that
;;    important, but still...
;;
;; o  The code is less efficient that it could be - in a lot of places, I
;;    pull a 512-character string out of the buffer and parse it, when I could
;;    be parsing it in place, not garbaging a string.  Should redo that.
;;
;; o  I'd like a command that searches for a string/regexp in every subfile
;;    of an archive, where <esc> would leave you in a subfile-edit buffer.
;;    (Like the Meta-R command of the Zmacs mail reader.)
;;
;; o  Sometimes (but not always) reverting the tar-file buffer does not
;;    re-grind the listing, and you are staring at the binary tar data.
;;    Typing 'g' again immediately after that will always revert and re-grind
;;    it, though.  I have no idea why this happens.
;;
;; o  Tar-mode interacts poorly with crypt.el and zcat.el because the tar
;;    write-file-hook actually writes the file.  Instead it should remove the
;;    header (and conspire to put it back afterwards) so that other write-file
;;    hooks which frob the buffer have a chance to do their dirty work.  There
;;    might be a problem if the tar write-file-hook does not come *first* on
;;    the list.
;;
;; o  Block files, sparse files, continuation files, and the various header
;;    types aren't editable.  Actually I don't know that they work at all.

;; Rationale:

;; Why does tar-mode edit the file itself instead of using tar?

;; That means that you can edit tar files which you don't have room for
;; on your local disk.

;; I don't know about recent features in gnu tar, but old versions of tar
;; can't replace a file in the middle of a tar file with a new version.
;; Tar-mode can.  I don't think tar can do things like chmod the subfiles.
;; An implementation which involved unpacking and repacking the file into
;; some scratch directory would be very wasteful, and wouldn't be able to
;; preserve the file owners.

;;; Code:

(defgroup tar nil
  "Simple editing of tar files."
  :prefix "tar-"
  :group 'data)

(defcustom tar-anal-blocksize 20
  "The blocksize of tar files written by Emacs, or nil, meaning don't care.
The blocksize of a tar file is not really the size of the blocks; rather, it is
the number of blocks written with one system call.  When tarring to a tape,
this is the size of the *tape* blocks, but when writing to a file, it doesn't
matter much.  The only noticeable difference is that if a tar file does not
have a blocksize of 20, tar will tell you that; all this really controls is
how many null padding bytes go on the end of the tar file."
  :type '(choice integer (const nil))
  :group 'tar)

(defcustom tar-update-datestamp nil
  "Non-nil means Tar mode should play fast and loose with sub-file datestamps.
If this is true, then editing and saving a tar file entry back into its
tar file will update its datestamp.  If false, the datestamp is unchanged.
You may or may not want this - it is good in that you can tell when a file
in a tar archive has been changed, but it is bad for the same reason that
editing a file in the tar archive at all is bad - the changed version of
the file never exists on disk."
  :type 'boolean
  :group 'tar)

(defcustom tar-mode-show-date nil
  "Non-nil means Tar mode should show the date/time of each subfile.
This information is useful, but it takes screen space away from file names."
  :type 'boolean
  :group 'tar)

(defvar tar-parse-info nil)
;; Be sure that this variable holds byte position, not char position.
(defvar tar-header-offset nil)
(defvar tar-superior-buffer nil)
(defvar tar-superior-descriptor nil)
(defvar tar-subfile-mode nil)

(put 'tar-parse-info 'permanent-local t)
(put 'tar-header-offset 'permanent-local t)
(put 'tar-superior-buffer 'permanent-local t)
(put 'tar-superior-descriptor 'permanent-local t)

(defmacro tar-setf (form val)
  "A mind-numbingly simple implementation of setf."
  (let ((mform (macroexpand form (and (boundp 'byte-compile-macro-environment)
				      byte-compile-macro-environment))))
    (cond ((symbolp mform) (list 'setq mform val))
	  ((not (consp mform)) (error "can't setf %s" form))
	  ((eq (car mform) 'aref)
	   (list 'aset (nth 1 mform) (nth 2 mform) val))
	  ((eq (car mform) 'car)
	   (list 'setcar (nth 1 mform) val))
	  ((eq (car mform) 'cdr)
	   (list 'setcdr (nth 1 mform) val))
	  (t (error "don't know how to setf %s" form)))))

;;; down to business.

(defmacro make-tar-header (name mode uid git size date ck lt ln
			   magic uname gname devmaj devmin)
  (list 'vector name mode uid git size date ck lt ln
	magic uname gname devmaj devmin))

(defmacro tar-header-name (x) (list 'aref x 0))
(defmacro tar-header-mode (x) (list 'aref x 1))
(defmacro tar-header-uid  (x) (list 'aref x 2))
(defmacro tar-header-gid  (x) (list 'aref x 3))
(defmacro tar-header-size (x) (list 'aref x 4))
(defmacro tar-header-date (x) (list 'aref x 5))
(defmacro tar-header-checksum  (x) (list 'aref x 6))
(defmacro tar-header-link-type (x) (list 'aref x 7))
(defmacro tar-header-link-name (x) (list 'aref x 8))
(defmacro tar-header-magic (x) (list 'aref x 9))
(defmacro tar-header-uname (x) (list 'aref x 10))
(defmacro tar-header-gname (x) (list 'aref x 11))
(defmacro tar-header-dmaj (x) (list 'aref x 12))
(defmacro tar-header-dmin (x) (list 'aref x 13))

(defmacro make-tar-desc (data-start tokens)
  (list 'cons data-start tokens))

(defmacro tar-desc-data-start (x) (list 'car x))
(defmacro tar-desc-tokens     (x) (list 'cdr x))

(defconst tar-name-offset 0)
(defconst tar-mode-offset (+ tar-name-offset 100))
(defconst tar-uid-offset  (+ tar-mode-offset 8))
(defconst tar-gid-offset  (+ tar-uid-offset 8))
(defconst tar-size-offset (+ tar-gid-offset 8))
(defconst tar-time-offset (+ tar-size-offset 12))
(defconst tar-chk-offset  (+ tar-time-offset 12))
(defconst tar-linkp-offset (+ tar-chk-offset 8))
(defconst tar-link-offset (+ tar-linkp-offset 1))
;;; GNU-tar specific slots.
(defconst tar-magic-offset (+ tar-link-offset 100))
(defconst tar-uname-offset (+ tar-magic-offset 8))
(defconst tar-gname-offset (+ tar-uname-offset 32))
(defconst tar-dmaj-offset (+ tar-gname-offset 32))
(defconst tar-dmin-offset (+ tar-dmaj-offset 8))
(defconst tar-end-offset (+ tar-dmin-offset 8))

(defun tar-header-block-tokenize (string)
  "Return a `tar-header' structure.
This is a list of name, mode, uid, gid, size,
write-date, checksum, link-type, and link-name."
  (cond ((< (length string) 512) nil)
	(;(some 'plusp string)		 ; <-- oops, massive cycle hog!
	 (or (not (= 0 (aref string 0))) ; This will do.
	     (not (= 0 (aref string 101))))
	 (let* ((name-end (1- tar-mode-offset))
		(link-end (1- tar-magic-offset))
		(uname-end (1- tar-gname-offset))
		(gname-end (1- tar-dmaj-offset))
		(link-p (aref string tar-linkp-offset))
		(magic-str (substring string tar-magic-offset (1- tar-uname-offset)))
		(uname-valid-p (or (string= "ustar  " magic-str) (string= "GNUtar " magic-str)))
		name linkname
		(nulsexp   "[^\000]*\000"))
	   (when (string-match nulsexp string tar-name-offset)
	     (setq name-end (min name-end (1- (match-end 0)))))
	   (when (string-match nulsexp string tar-link-offset)
	     (setq link-end (min link-end (1- (match-end 0)))))
	   (when (string-match nulsexp string tar-uname-offset)
	     (setq uname-end (min uname-end (1- (match-end 0)))))
	   (when (string-match nulsexp string tar-gname-offset)
	     (setq gname-end (min gname-end (1- (match-end 0)))))
	   (setq name (substring string tar-name-offset name-end)
		 link-p (if (or (= link-p 0) (= link-p ?0))
			    nil
			  (- link-p ?0)))
	   (setq linkname (substring string tar-link-offset link-end))
	   (if default-enable-multibyte-characters
	       (setq name
		     (decode-coding-string name
					   (or file-name-coding-system
					       default-file-name-coding-system
					       'undecided))
		     linkname
		     (decode-coding-string linkname
					   (or file-name-coding-system
					       default-file-name-coding-system
					       'undecided))))
	   (if (and (null link-p) (string-match "/\\'" name)) (setq link-p 5)) ; directory
	   (make-tar-header
	     name
	     (tar-parse-octal-integer string tar-mode-offset tar-uid-offset)
	     (tar-parse-octal-integer string tar-uid-offset tar-gid-offset)
	     (tar-parse-octal-integer string tar-gid-offset tar-size-offset)
	     (tar-parse-octal-integer string tar-size-offset tar-time-offset)
	     (tar-parse-octal-long-integer string tar-time-offset tar-chk-offset)
	     (tar-parse-octal-integer string tar-chk-offset tar-linkp-offset)
	     link-p
	     linkname
	     uname-valid-p
	     (and uname-valid-p (substring string tar-uname-offset uname-end))
	     (and uname-valid-p (substring string tar-gname-offset gname-end))
	     (tar-parse-octal-integer string tar-dmaj-offset tar-dmin-offset)
	     (tar-parse-octal-integer string tar-dmin-offset tar-end-offset)
	     )))
	(t 'empty-tar-block)))


(defun tar-parse-octal-integer (string &optional start end)
  (if (null start) (setq start 0))
  (if (null end) (setq end (length string)))
  (if (= (aref string start) 0)
      0
    (let ((n 0))
      (while (< start end)
	(setq n (if (< (aref string start) ?0) n
		  (+ (* n 8) (- (aref string start) ?0)))
	      start (1+ start)))
      n)))

(defun tar-parse-octal-long-integer (string &optional start end)
  (if (null start) (setq start 0))
  (if (null end) (setq end (length string)))
  (if (= (aref string start) 0)
      (list 0 0)
    (let ((lo 0)
	  (hi 0))
      (while (< start end)
	(if (>= (aref string start) ?0)
	    (setq lo (+ (* lo 8) (- (aref string start) ?0))
		  hi (+ (* hi 8) (ash lo -16))
		  lo (logand lo 65535)))
	(setq start (1+ start)))
      (list hi lo))))

(defun tar-parse-octal-integer-safe (string)
  (if (zerop (length string)) (error "empty string"))
  (mapc (lambda (c)
	  (if (or (< c ?0) (> c ?7))
	      (error "`%c' is not an octal digit" c)))
	string)
  (tar-parse-octal-integer string))


(defun tar-header-block-checksum (string)
  "Compute and return a tar-acceptable checksum for this block."
  (let* ((chk-field-start tar-chk-offset)
	 (chk-field-end (+ chk-field-start 8))
	 (sum 0)
	 (i 0))
    ;; Add up all of the characters except the ones in the checksum field.
    ;; Add that field as if it were filled with spaces.
    (while (< i chk-field-start)
      (setq sum (+ sum (aref string i))
	    i (1+ i)))
    (setq i chk-field-end)
    (while (< i 512)
      (setq sum (+ sum (aref string i))
	    i (1+ i)))
    (+ sum (* 32 8))))

(defun tar-header-block-check-checksum (hblock desired-checksum file-name)
  "Beep and print a warning if the checksum doesn't match."
  (if (not (= desired-checksum (tar-header-block-checksum hblock)))
      (progn (beep) (message "Invalid checksum for file %s!" file-name))))

(defun tar-clip-time-string (time)
  (let ((str (current-time-string time)))
    (concat " " (substring str 4 16) (substring str 19 24))))

(defun tar-grind-file-mode (mode)
  "Construct a `-rw--r--r--' string indicating MODE.
MODE should be an integer which is a file mode value."
  (string
   (if (zerop (logand 256 mode)) ?- ?r)
   (if (zerop (logand 128 mode)) ?- ?w)
   (if (zerop (logand 1024 mode)) (if (zerop (logand  64 mode)) ?- ?x) ?s)
   (if (zerop (logand  32 mode)) ?- ?r)
   (if (zerop (logand  16 mode)) ?- ?w)
   (if (zerop (logand 2048 mode)) (if (zerop (logand   8 mode)) ?- ?x) ?s)
   (if (zerop (logand   4 mode)) ?- ?r)
   (if (zerop (logand   2 mode)) ?- ?w)
   (if (zerop (logand   1 mode)) ?- ?x)))

(defun tar-header-block-summarize (tar-hblock &optional mod-p)
  "Return a line similar to the output of `tar -vtf'."
  (let ((name (tar-header-name tar-hblock))
	(mode (tar-header-mode tar-hblock))
	(uid (tar-header-uid tar-hblock))
	(gid (tar-header-gid tar-hblock))
	(uname (tar-header-uname tar-hblock))
	(gname (tar-header-gname tar-hblock))
	(size (tar-header-size tar-hblock))
	(time (tar-header-date tar-hblock))
	;; (ck (tar-header-checksum tar-hblock))
	(type (tar-header-link-type tar-hblock))
	(link-name (tar-header-link-name tar-hblock)))
    (format "%c%c%s%8s/%-8s%7s%s %s%s"
	    (if mod-p ?* ? )
	    (cond ((or (eq type nil) (eq type 0)) ?-)
		  ((eq type 1) ?h)	; link
		  ((eq type 2) ?l)	; symlink
		  ((eq type 3) ?c)	; char special
		  ((eq type 4) ?b)	; block special
		  ((eq type 5) ?d)	; directory
		  ((eq type 6) ?p)	; FIFO/pipe
		  ((eq type 20) ?*)	; directory listing
		  ((eq type 28) ?L)	; next has longname
		  ((eq type 29) ?M)	; multivolume continuation
		  ((eq type 35) ?S)	; sparse
		  ((eq type 38) ?V)	; volume header
		  (t ?\s)
		  )
	    (tar-grind-file-mode mode)
	    (if (= 0 (length uname)) uid uname)
	    (if (= 0 (length gname)) gid gname)
	    size
	    (if tar-mode-show-date (tar-clip-time-string time) "")
	    (propertize name
			'mouse-face 'highlight
			'help-echo "mouse-2: extract this file into a buffer")
	    (if (or (eq type 1) (eq type 2))
		(concat (if (= type 1) " ==> " " --> ") link-name)
	      ""))))

(defun tar-untar-buffer ()
  "Extract all archive members in the tar-file into the current directory."
  (interactive)
  (let ((multibyte enable-multibyte-characters))
    (unwind-protect
	(save-restriction
	  (widen)
	  (set-buffer-multibyte nil)
	  (dolist (descriptor tar-parse-info)
	    (let* ((tokens (tar-desc-tokens descriptor))
		   (name (tar-header-name tokens))
		   (dir (file-name-directory name))
		   (start (+ (tar-desc-data-start descriptor)
			     (- tar-header-offset (point-min))))
		   (end (+ start (tar-header-size tokens))))
	      (unless (file-directory-p name)
		(message "Extracting %s" name)
		(if (and dir (not (file-exists-p dir)))
		    (make-directory dir t))
		(unless (file-directory-p name)
		  (write-region start end name))
		(set-file-modes name (tar-header-mode tokens))))))
      (set-buffer-multibyte multibyte))))

(defun tar-summarize-buffer ()
  "Parse the contents of the tar file in the current buffer.
Place a dired-like listing on the front;
then narrow to it, so that only that listing
is visible (and the real data of the buffer is hidden)."
  (let ((modified (buffer-modified-p)))
    (set-buffer-multibyte nil)
    (let* ((result '())
           (pos (point-min))
           (progress-reporter
            (make-progress-reporter "Parsing tar file..."
                                    (point-min) (max 1 (- (buffer-size) 1024))))
           tokens)
      (while (and (<= (+ pos 512) (point-max))
                  (not (eq 'empty-tar-block
                           (setq tokens
                                 (tar-header-block-tokenize
                                  (buffer-substring pos (+ pos 512)))))))
        (setq pos (+ pos 512))
        (progress-reporter-update progress-reporter pos)
        (if (eq (tar-header-link-type tokens) 20)
            ;; Foo.  There's an extra empty block after these.
            (setq pos (+ pos 512)))
        (let ((size (tar-header-size tokens)))
          (if (< size 0)
              (error "%s has size %s - corrupted"
                     (tar-header-name tokens) size))
                                        ;
                                        ; This is just too slow.  Don't really need it anyway....
                                        ;(tar-header-block-check-checksum
                                        ;  hblock (tar-header-block-checksum hblock)
                                        ;  (tar-header-name tokens))

          (push (make-tar-desc pos tokens) result)

          (and (null (tar-header-link-type tokens))
               (> size 0)
               (setq pos
                     (+ pos 512 (ash (ash (1- size) -9) 9)) ; this works
                                        ;(+ pos (+ size (- 512 (rem (1- size) 512)))) ; this doesn't
                     ))))
      (make-local-variable 'tar-parse-info)
      (setq tar-parse-info (nreverse result))
      ;; A tar file should end with a block or two of nulls,
      ;; but let's not get a fatal error if it doesn't.
      (if (eq tokens 'empty-tar-block)
          (progress-reporter-done progress-reporter)
        (message "Warning: premature EOF parsing tar file")))
    (set-buffer-multibyte default-enable-multibyte-characters)
    (goto-char (point-min))
    (let ((inhibit-read-only t))
      ;; Collect summary lines and insert them all at once since tar files
      ;; can be pretty big.
      (let ((total-summaries
	     (mapconcat
	      (lambda (tar-desc)
		(tar-header-block-summarize (tar-desc-tokens tar-desc)))
	      tar-parse-info
	      "\n")))
	(insert total-summaries "\n"))
      (narrow-to-region (point-min) (point))
      (set (make-local-variable 'tar-header-offset) (position-bytes (point)))
      (goto-char (point-min))
      (restore-buffer-modified-p modified))))

(defvar tar-mode-map
  (let ((map (make-keymap)))
    (suppress-keymap map)
    (define-key map " " 'tar-next-line)
    (define-key map "C" 'tar-copy)
    (define-key map "d" 'tar-flag-deleted)
    (define-key map "\^D" 'tar-flag-deleted)
    (define-key map "e" 'tar-extract)
    (define-key map "f" 'tar-extract)
    (define-key map "\C-m" 'tar-extract)
    (define-key map [mouse-2] 'tar-mouse-extract)
    (define-key map "g" 'revert-buffer)
    (define-key map "h" 'describe-mode)
    (define-key map "n" 'tar-next-line)
    (define-key map "\^N" 'tar-next-line)
    (define-key map [down] 'tar-next-line)
    (define-key map "o" 'tar-extract-other-window)
    (define-key map "p" 'tar-previous-line)
    (define-key map "q" 'quit-window)
    (define-key map "\^P" 'tar-previous-line)
    (define-key map [up] 'tar-previous-line)
    (define-key map "R" 'tar-rename-entry)
    (define-key map "u" 'tar-unflag)
    (define-key map "v" 'tar-view)
    (define-key map "x" 'tar-expunge)
    (define-key map "\177" 'tar-unflag-backwards)
    (define-key map "E" 'tar-extract-other-window)
    (define-key map "M" 'tar-chmod-entry)
    (define-key map "G" 'tar-chgrp-entry)
    (define-key map "O" 'tar-chown-entry)

    ;; Make menu bar items.

    ;; Get rid of the Edit menu bar item to save space.
    (define-key map [menu-bar edit] 'undefined)

    (define-key map [menu-bar immediate]
      (cons "Immediate" (make-sparse-keymap "Immediate")))

    (define-key map [menu-bar immediate view]
      '("View This File" . tar-view))
    (define-key map [menu-bar immediate display]
      '("Display in Other Window" . tar-display-other-window))
    (define-key map [menu-bar immediate find-file-other-window]
      '("Find in Other Window" . tar-extract-other-window))
    (define-key map [menu-bar immediate find-file]
      '("Find This File" . tar-extract))

    (define-key map [menu-bar mark]
      (cons "Mark" (make-sparse-keymap "Mark")))

    (define-key map [menu-bar mark unmark-all]
      '("Unmark All" . tar-clear-modification-flags))
    (define-key map [menu-bar mark deletion]
      '("Flag" . tar-flag-deleted))
    (define-key map [menu-bar mark unmark]
      '("Unflag" . tar-unflag))

    (define-key map [menu-bar operate]
      (cons "Operate" (make-sparse-keymap "Operate")))

    (define-key map [menu-bar operate chown]
      '("Change Owner..." . tar-chown-entry))
    (define-key map [menu-bar operate chgrp]
      '("Change Group..." . tar-chgrp-entry))
    (define-key map [menu-bar operate chmod]
      '("Change Mode..." . tar-chmod-entry))
    (define-key map [menu-bar operate rename]
      '("Rename to..." . tar-rename-entry))
    (define-key map [menu-bar operate copy]
      '("Copy to..." . tar-copy))
    (define-key map [menu-bar operate expunge]
      '("Expunge Marked Files" . tar-expunge))

    map)
  "Local keymap for Tar mode listings.")


;; tar mode is suitable only for specially formatted data.
(put 'tar-mode 'mode-class 'special)
(put 'tar-subfile-mode 'mode-class 'special)

;;;###autoload
(define-derived-mode tar-mode nil "Tar"
  "Major mode for viewing a tar file as a dired-like listing of its contents.
You can move around using the usual cursor motion commands.
Letters no longer insert themselves.
Type `e' to pull a file out of the tar file and into its own buffer;
or click mouse-2 on the file's line in the Tar mode buffer.
Type `c' to copy an entry from the tar file into another file on disk.

If you edit a sub-file of this archive (as with the `e' command) and
save it with \\[save-buffer], the contents of that buffer will be
saved back into the tar-file buffer; in this way you can edit a file
inside of a tar archive without extracting it and re-archiving it.

See also: variables `tar-update-datestamp' and `tar-anal-blocksize'.
\\{tar-mode-map}"
  ;; this is not interactive because you shouldn't be turning this
  ;; mode on and off.  You can corrupt things that way.
  ;; rms: with permanent locals, it should now be possible to make this work
  ;; interactively in some reasonable fashion.
  (make-local-variable 'tar-header-offset)
  (make-local-variable 'tar-parse-info)
  (set (make-local-variable 'require-final-newline) nil) ; binary data, dude...
  (set (make-local-variable 'revert-buffer-function) 'tar-mode-revert)
  (set (make-local-variable 'local-enable-local-variables) nil)
  (set (make-local-variable 'next-line-add-newlines) nil)
  ;; Prevent loss of data when saving the file.
  (set (make-local-variable 'file-precious-flag) t)
  (auto-save-mode 0)
  (set (make-local-variable 'write-contents-functions) '(tar-mode-write-file))
  (buffer-disable-undo)
  (widen)
  (if (and (boundp 'tar-header-offset) tar-header-offset)
      (narrow-to-region (point-min) (byte-to-position tar-header-offset))
    (tar-summarize-buffer)
    (tar-next-line 0)))


(defun tar-subfile-mode (p)
  "Minor mode for editing an element of a tar-file.
This mode arranges for \"saving\" this buffer to write the data
into the tar-file buffer that it came from.  The changes will actually
appear on disk when you save the tar-file's buffer."
  (interactive "P")
  (or (and (boundp 'tar-superior-buffer) tar-superior-buffer)
      (error "This buffer is not an element of a tar file"))
  ;; Don't do this, because it is redundant and wastes mode line space.
  ;;  (or (assq 'tar-subfile-mode minor-mode-alist)
  ;;      (setq minor-mode-alist (append minor-mode-alist
  ;;				     (list '(tar-subfile-mode " TarFile")))))
  (make-local-variable 'tar-subfile-mode)
  (setq tar-subfile-mode
	(if (null p)
	    (not tar-subfile-mode)
	    (> (prefix-numeric-value p) 0)))
  (cond (tar-subfile-mode
	 (add-hook 'write-file-functions 'tar-subfile-save-buffer nil t)
	 ;; turn off auto-save.
	 (auto-save-mode -1)
	 (setq buffer-auto-save-file-name nil)
	 (run-hooks 'tar-subfile-mode-hook))
	(t
	 (remove-hook 'write-file-functions 'tar-subfile-save-buffer t))))


;; Revert the buffer and recompute the dired-like listing.
(defun tar-mode-revert (&optional no-auto-save no-confirm)
  (let ((revert-buffer-function nil)
	(old-offset tar-header-offset)
	success)
    (setq tar-header-offset nil)
    (unwind-protect
	(and (revert-buffer t no-confirm)
	     (progn (widen)
		    (setq success t)
		    (tar-mode)))
      ;; If the revert was canceled,
      ;; put back the old value of tar-header-offset.
      (or success
	  (setq tar-header-offset old-offset)))))


(defun tar-next-line (arg)
  "Move cursor vertically down ARG lines and to the start of the filename."
  (interactive "p")
  (forward-line arg)
  (if (eobp) nil (forward-char (if tar-mode-show-date 54 36))))

(defun tar-previous-line (arg)
  "Move cursor vertically up ARG lines and to the start of the filename."
  (interactive "p")
  (tar-next-line (- arg)))

(defun tar-current-descriptor (&optional noerror)
  "Return the tar-descriptor of the current line, or signals an error."
  ;; I wish lines had plists, like in ZMACS...
  (or (nth (count-lines (point-min)
			(save-excursion (beginning-of-line) (point)))
	   tar-parse-info)
      (if noerror
	  nil
	  (error "This line does not describe a tar-file entry"))))

(defun tar-get-descriptor ()
  (let* ((descriptor (tar-current-descriptor))
	 (tokens (tar-desc-tokens descriptor))
	 (size (tar-header-size tokens))
	 (link-p (tar-header-link-type tokens)))
    (if link-p
	(error "This is a %s, not a real file"
	       (cond ((eq link-p 5) "directory")
		     ((eq link-p 20) "tar directory header")
		     ((eq link-p 28) "next has longname")
		     ((eq link-p 29) "multivolume-continuation")
		     ((eq link-p 35) "sparse entry")
		     ((eq link-p 38) "volume header")
		     (t "link"))))
    (if (zerop size) (error "This is a zero-length file"))
    descriptor))

(defun tar-mouse-extract (event)
  "Extract a file whose tar directory line you click on."
  (interactive "e")
  (save-excursion
    (set-buffer (window-buffer (posn-window (event-end event))))
    (save-excursion
      (goto-char (posn-point (event-end event)))
      ;; Just make sure this doesn't get an error.
      (tar-get-descriptor)))
  (select-window (posn-window (event-end event)))
  (goto-char (posn-point (event-end event)))
  (tar-extract))

(defun tar-file-name-handler (op &rest args)
  "Helper function for `tar-extract'."
  (or (eq op 'file-exists-p)
      (let ((file-name-handler-alist nil))
	(apply op args))))

(defun tar-extract (&optional other-window-p)
  "In Tar mode, extract this entry of the tar file into its own buffer."
  (interactive)
  (let* ((view-p (eq other-window-p 'view))
	 (descriptor (tar-get-descriptor))
	 (tokens (tar-desc-tokens descriptor))
	 (name (tar-header-name tokens))
	 (size (tar-header-size tokens))
	 (start (+ (tar-desc-data-start descriptor)
		   (- tar-header-offset (point-min))))
	 (end (+ start size)))
    (let* ((tar-buffer (current-buffer))
	   (tar-buffer-multibyte enable-multibyte-characters)
	   (tarname (buffer-name))
	   (bufname (concat (file-name-nondirectory name)
			    " ("
			     tarname
			     ")"))
	   (read-only-p (or buffer-read-only view-p))
	   (new-buffer-file-name (expand-file-name
				  ;; `:' is not allowed on Windows
				  (concat tarname "!" name)))
	   (buffer (get-file-buffer new-buffer-file-name))
	   (just-created nil))
      (unless buffer
	(setq buffer (generate-new-buffer bufname))
	(setq bufname (buffer-name buffer))
	(setq just-created t)
	(unwind-protect
	    (progn
	      (widen)
	      (set-buffer-multibyte nil)
	      (save-excursion
		(set-buffer buffer)
		(let ((buffer-undo-list t))
		  (if enable-multibyte-characters
		      (progn
			;; We must avoid unibyte->multibyte conversion.
			(set-buffer-multibyte nil)
			(insert-buffer-substring tar-buffer start end)
			(set-buffer-multibyte t))
		    (insert-buffer-substring tar-buffer start end))
		  (goto-char (point-min))
		  (setq buffer-file-name new-buffer-file-name)
		  (setq buffer-file-truename
			(abbreviate-file-name buffer-file-name))
		  ;; We need to mimic the parts of insert-file-contents
		  ;; which determine the coding-system and decode the text.
		  (let ((coding
			 (or coding-system-for-read
			     (and set-auto-coding-function
				  (save-excursion
				    (funcall set-auto-coding-function
					     name (- (point-max) (point)))))
			     ;; The following binding causes
			     ;; find-buffer-file-type-coding-system
			     ;; (defined on dos-w32.el) to act as if
			     ;; the file being extracted existed, so
			     ;; that the file's contents' encoding and
			     ;; EOL format are auto-detected.
			     (let ((file-name-handler-alist
				    (if (featurep 'dos-w32)
					'(("" . tar-file-name-handler))
				      file-name-handler-alist)))
			       (car (find-operation-coding-system
				     'insert-file-contents
				     (cons name (current-buffer)) t)))))
			(multibyte enable-multibyte-characters)
			(detected (detect-coding-region
				   (point-min)
				   (min (+ (point-min) 16384) (point-max)) t)))
		    (if coding
			(or (numberp (coding-system-eol-type coding))
			    (vectorp (coding-system-eol-type detected))
			    (setq coding (coding-system-change-eol-conversion
					  coding
					  (coding-system-eol-type detected))))
		      (setq coding
			    (find-new-buffer-file-coding-system detected)))
		    (if (or (eq coding 'no-conversion)
			    (eq (coding-system-type coding) 5))
			(setq multibyte (set-buffer-multibyte nil)))
		    (or multibyte
			(setq coding
			      (coding-system-change-text-conversion
			       coding 'raw-text)))
		    (decode-coding-region (point-min) (point-max) coding)
		    ;; Force buffer-file-coding-system to what
		    ;; decode-coding-region actually used.
		    (set-buffer-file-coding-system last-coding-system-used t))
		  ;; Set the default-directory to the dir of the
		  ;; superior buffer.
		  (setq default-directory
			(save-excursion
			  (set-buffer tar-buffer)
			  default-directory))
		  (normal-mode)  ; pick a mode.
		  (rename-buffer bufname)
		  (make-local-variable 'tar-superior-buffer)
		  (make-local-variable 'tar-superior-descriptor)
		  (setq tar-superior-buffer tar-buffer)
		  (setq tar-superior-descriptor descriptor)
		  (setq buffer-read-only read-only-p)
		  (set-buffer-modified-p nil))
		(tar-subfile-mode 1))
	      (set-buffer tar-buffer))
	  (narrow-to-region (point-min) tar-header-offset)
	  (set-buffer-multibyte tar-buffer-multibyte)))
      (if view-p
	  (view-buffer buffer (and just-created 'kill-buffer))
	(if (eq other-window-p 'display)
	    (display-buffer buffer)
	  (if other-window-p
	      (switch-to-buffer-other-window buffer)
	    (switch-to-buffer buffer)))))))


(defun tar-extract-other-window ()
  "In Tar mode, find this entry of the tar file in another window."
  (interactive)
  (tar-extract t))

(defun tar-display-other-window ()
  "In Tar mode, display this entry of the tar file in another window."
  (interactive)
  (tar-extract 'display))

(defun tar-view ()
  "In Tar mode, view the tar file entry on this line."
  (interactive)
  (tar-extract 'view))


(defun tar-read-file-name (&optional prompt)
  "Read a file name with this line's entry as the default."
  (or prompt (setq prompt "Copy to: "))
  (let* ((default-file (expand-file-name
			(tar-header-name (tar-desc-tokens
					  (tar-current-descriptor)))))
	 (target (expand-file-name
		  (read-file-name prompt
				  (file-name-directory default-file)
				  default-file nil))))
    (if (or (string= "" (file-name-nondirectory target))
	    (file-directory-p target))
	(setq target (concat (if (string-match "/$" target)
				 (substring target 0 (1- (match-end 0)))
				 target)
			     "/"
			     (file-name-nondirectory default-file))))
    target))


(defun tar-copy (&optional to-file)
  "In Tar mode, extract this entry of the tar file into a file on disk.
If TO-FILE is not supplied, it is prompted for, defaulting to the name of
the current tar-entry."
  (interactive (list (tar-read-file-name)))
  (let* ((descriptor (tar-get-descriptor))
	 (tokens (tar-desc-tokens descriptor))
	 (name (tar-header-name tokens))
	 (size (tar-header-size tokens))
	 (start (+ (tar-desc-data-start descriptor)
		   (- tar-header-offset (point-min))))
	 (end (+ start size))
	 (multibyte enable-multibyte-characters)
	 (inhibit-file-name-handlers inhibit-file-name-handlers)
	 (inhibit-file-name-operation inhibit-file-name-operation))
    (save-restriction
      (widen)
      ;; Inhibit compressing a subfile again if *both* name and
      ;; to-file are handled by jka-compr
      (if (and (eq (find-file-name-handler name 'write-region) 'jka-compr-handler)
	       (eq (find-file-name-handler to-file 'write-region) 'jka-compr-handler))
	  (setq inhibit-file-name-handlers
		(cons 'jka-compr-handler
		      (and (eq inhibit-file-name-operation 'write-region)
			   inhibit-file-name-handlers))
		inhibit-file-name-operation 'write-region))
      (unwind-protect
	  (let ((coding-system-for-write 'no-conversion))
	    (set-buffer-multibyte nil)
	    (write-region start end to-file nil nil nil t))
	(set-buffer-multibyte multibyte)))
    (message "Copied tar entry %s to %s" name to-file)))

(defun tar-flag-deleted (p &optional unflag)
  "In Tar mode, mark this sub-file to be deleted from the tar file.
With a prefix argument, mark that many files."
  (interactive "p")
  (beginning-of-line)
  (dotimes (i (abs p))
    (if (tar-current-descriptor unflag) ; barf if we're not on an entry-line.
	(progn
	  (delete-char 1)
	  (insert (if unflag " " "D"))))
    (forward-line (if (< p 0) -1 1)))
  (if (eobp) nil (forward-char 36)))

(defun tar-unflag (p)
  "In Tar mode, un-mark this sub-file if it is marked to be deleted.
With a prefix argument, un-mark that many files forward."
  (interactive "p")
  (tar-flag-deleted p t))

(defun tar-unflag-backwards (p)
  "In Tar mode, un-mark this sub-file if it is marked to be deleted.
With a prefix argument, un-mark that many files backward."
  (interactive "p")
  (tar-flag-deleted (- p) t))


;; When this function is called, it is sure that the buffer is unibyte.
(defun tar-expunge-internal ()
  "Expunge the tar-entry specified by the current line."
  (let* ((descriptor (tar-current-descriptor))
	 (tokens (tar-desc-tokens descriptor))
	 ;; (line (tar-desc-data-start descriptor))
	 (name (tar-header-name tokens))
	 (size (tar-header-size tokens))
	 (link-p (tar-header-link-type tokens))
	 (start (tar-desc-data-start descriptor))
	 (following-descs (cdr (memq descriptor tar-parse-info))))
    (if link-p (setq size 0)) ; size lies for hard-links.
    ;;
    ;; delete the current line...
    (beginning-of-line)
    (let ((line-start (point)))
      (end-of-line) (forward-char)
      ;; decrement the header-pointer to be in sync...
      (setq tar-header-offset (- tar-header-offset (- (point) line-start)))
      (delete-region line-start (point)))
    ;;
    ;; delete the data pointer...
    (setq tar-parse-info (delq descriptor tar-parse-info))
    ;;
    ;; delete the data from inside the file...
    (widen)
    (let* ((data-start (+ start (- tar-header-offset (point-min)) -512))
	   (data-end (+ data-start 512 (ash (ash (+ size 511) -9) 9))))
      (delete-region data-start data-end)
      ;;
      ;; and finally, decrement the start-pointers of all following
      ;; entries in the archive.  This is a pig when deleting a bunch
      ;; of files at once - we could optimize this to only do the
      ;; iteration over the files that remain, or only iterate up to
      ;; the next file to be deleted.
      (let ((data-length (- data-end data-start)))
	(dolist (desc following-descs)
	  (tar-setf (tar-desc-data-start desc)
		    (- (tar-desc-data-start desc) data-length))))
      ))
  (narrow-to-region (point-min) tar-header-offset))


(defun tar-expunge (&optional noconfirm)
  "In Tar mode, delete all the archived files flagged for deletion.
This does not modify the disk image; you must save the tar file itself
for this to be permanent."
  (interactive)
  (if (or noconfirm
	  (y-or-n-p "Expunge files marked for deletion? "))
      (let ((n 0)
	    (multibyte enable-multibyte-characters))
	(save-excursion
          (widen)
          (set-buffer-multibyte nil)
	  (goto-char (point-min))
	  (while (not (eobp))
	    (if (looking-at "D")
		(progn (tar-expunge-internal)
		       (setq n (1+ n)))
		(forward-line 1)))
	  ;; after doing the deletions, add any padding that may be necessary.
	  (tar-pad-to-blocksize)
          (widen)
          (set-buffer-multibyte multibyte)
	  (narrow-to-region (point-min) tar-header-offset))
	(if (zerop n)
	    (message "Nothing to expunge.")
	    (message "%s files expunged.  Be sure to save this buffer." n)))))


(defun tar-clear-modification-flags ()
  "Remove the stars at the beginning of each line."
  (interactive)
  (save-excursion
    (goto-char (point-min))
    (while (< (position-bytes (point)) tar-header-offset)
      (if (not (eq (following-char) ?\s))
	  (progn (delete-char 1) (insert " ")))
      (forward-line 1))))


(defun tar-chown-entry (new-uid)
  "Change the user-id associated with this entry in the tar file.
If this tar file was written by GNU tar, then you will be able to edit
the user id as a string; otherwise, you must edit it as a number.
You can force editing as a number by calling this with a prefix arg.
This does not modify the disk image; you must save the tar file itself
for this to be permanent."
  (interactive (list
		 (let ((tokens (tar-desc-tokens (tar-current-descriptor))))
		   (if (or current-prefix-arg
			   (not (tar-header-magic tokens)))
		       (let (n)
			 (while (not (numberp (setq n (read-minibuffer
							"New UID number: "
							(format "%s" (tar-header-uid tokens)))))))
			 n)
		       (read-string "New UID string: " (tar-header-uname tokens))))))
  (cond ((stringp new-uid)
	 (tar-setf (tar-header-uname (tar-desc-tokens (tar-current-descriptor)))
		   new-uid)
	 (tar-alter-one-field tar-uname-offset (concat new-uid "\000")))
	(t
	 (tar-setf (tar-header-uid (tar-desc-tokens (tar-current-descriptor)))
		   new-uid)
	 (tar-alter-one-field tar-uid-offset
	   (concat (substring (format "%6o" new-uid) 0 6) "\000 ")))))


(defun tar-chgrp-entry (new-gid)
  "Change the group-id associated with this entry in the tar file.
If this tar file was written by GNU tar, then you will be able to edit
the group id as a string; otherwise, you must edit it as a number.
You can force editing as a number by calling this with a prefix arg.
This does not modify the disk image; you must save the tar file itself
for this to be permanent."
  (interactive (list
		 (let ((tokens (tar-desc-tokens (tar-current-descriptor))))
		   (if (or current-prefix-arg
			   (not (tar-header-magic tokens)))
		       (let (n)
			 (while (not (numberp (setq n (read-minibuffer
							"New GID number: "
							(format "%s" (tar-header-gid tokens)))))))
			 n)
		       (read-string "New GID string: " (tar-header-gname tokens))))))
  (cond ((stringp new-gid)
	 (tar-setf (tar-header-gname (tar-desc-tokens (tar-current-descriptor)))
		   new-gid)
	 (tar-alter-one-field tar-gname-offset
	   (concat new-gid "\000")))
	(t
	 (tar-setf (tar-header-gid (tar-desc-tokens (tar-current-descriptor)))
		   new-gid)
	 (tar-alter-one-field tar-gid-offset
	   (concat (substring (format "%6o" new-gid) 0 6) "\000 ")))))

(defun tar-rename-entry (new-name)
  "Change the name associated with this entry in the tar file.
This does not modify the disk image; you must save the tar file itself
for this to be permanent."
  (interactive
    (list (read-string "New name: "
	    (tar-header-name (tar-desc-tokens (tar-current-descriptor))))))
  (if (string= "" new-name) (error "zero length name"))
  (if (> (length new-name) 98) (error "name too long"))
  (tar-setf (tar-header-name (tar-desc-tokens (tar-current-descriptor)))
	    new-name)
  (if (multibyte-string-p new-name)
      (setq new-name (encode-coding-string new-name
					   (or file-name-coding-system
					       default-file-name-coding-system))))
  (tar-alter-one-field 0
    (substring (concat new-name (make-string 99 0)) 0 99)))


(defun tar-chmod-entry (new-mode)
  "Change the protection bits associated with this entry in the tar file.
This does not modify the disk image; you must save the tar file itself
for this to be permanent."
  (interactive (list (tar-parse-octal-integer-safe
		       (read-string "New protection (octal): "))))
  (tar-setf (tar-header-mode (tar-desc-tokens (tar-current-descriptor)))
	    new-mode)
  (tar-alter-one-field tar-mode-offset
    (concat (substring (format "%6o" new-mode) 0 6) "\000 ")))


(defun tar-alter-one-field (data-position new-data-string)
  (let* ((descriptor (tar-current-descriptor))
	 (tokens (tar-desc-tokens descriptor))
	 (multibyte enable-multibyte-characters))
    (unwind-protect
	(save-excursion
	  ;;
	  ;; update the header-line.
	  (beginning-of-line)
	  (let ((p (point)))
	    (forward-line 1)
	    (delete-region p (point))
	    (insert (tar-header-block-summarize tokens) "\n")
	    (setq tar-header-offset (position-bytes (point-max))))

	  (widen)
	  (set-buffer-multibyte nil)
	  (let* ((start (+ (tar-desc-data-start descriptor)
			   (- tar-header-offset (point-min))
                           -512)))
	    ;;
	    ;; delete the old field and insert a new one.
	    (goto-char (+ start data-position))
	    (delete-region (point) (+ (point) (length new-data-string))) ; <--
	    (insert new-data-string) ; <--
	    ;;
	    ;; compute a new checksum and insert it.
	    (let ((chk (tar-header-block-checksum
			(buffer-substring start (+ start 512)))))
	      (goto-char (+ start tar-chk-offset))
	      (delete-region (point) (+ (point) 8))
	      (insert (format "%6o" chk))
	      (insert 0)
	      (insert ? )
	      (tar-setf (tar-header-checksum tokens) chk)
	      ;;
	      ;; ok, make sure we didn't botch it.
	      (tar-header-block-check-checksum
	        (buffer-substring start (+ start 512))
	        chk (tar-header-name tokens))
	      )))
      (narrow-to-region (point-min) tar-header-offset)
      (set-buffer-multibyte multibyte)
      (tar-next-line 0))))


(defun tar-octal-time (timeval)
  ;; Format a timestamp as 11 octal digits.  Ghod, I hope this works...
  (let ((hibits (car timeval)) (lobits (car (cdr timeval))))
    (format "%05o%01o%05o"
	    (lsh hibits -2)
	    (logior (lsh (logand 3 hibits) 1)
		    (if (> (logand lobits 32768) 0) 1 0))
	    (logand 32767 lobits)
	    )))

(defun tar-subfile-save-buffer ()
  "In tar subfile mode, save this buffer into its parent tar-file buffer.
This doesn't write anything to disk; you must save the parent tar-file buffer
to make your changes permanent."
  (interactive)
  (if (not (and (boundp 'tar-superior-buffer) tar-superior-buffer))
    (error "This buffer has no superior tar file buffer"))
  (if (not (and (boundp 'tar-superior-descriptor) tar-superior-descriptor))
    (error "This buffer doesn't have an index into its superior tar file!"))
  (save-excursion
  (let ((subfile (current-buffer))
	(subfile-multibyte enable-multibyte-characters)
	(coding buffer-file-coding-system)
	(descriptor tar-superior-descriptor)
	subfile-size)
    ;; We must make the current buffer unibyte temporarily to avoid
    ;; multibyte->unibyte conversion in `insert-buffer-substring'.
    (set-buffer-multibyte nil)
    (setq subfile-size (buffer-size))
    (set-buffer tar-superior-buffer)
    (let* ((tokens (tar-desc-tokens descriptor))
	   (start (tar-desc-data-start descriptor))
	   (name (tar-header-name tokens))
	   (size (tar-header-size tokens))
	   (size-pad (ash (ash (+ size 511) -9) 9))
	   (head (memq descriptor tar-parse-info))
	   (following-descs (cdr head))
	   (tar-buffer-multibyte enable-multibyte-characters))
      (if (not head)
	(error "Can't find this tar file entry in its parent tar file!"))
      (unwind-protect
       (save-excursion
	(widen)
	(set-buffer-multibyte nil)
	;; delete the old data...
	(let* ((data-start (+ start (- tar-header-offset (point-min))))
	       (data-end (+ data-start (ash (ash (+ size 511) -9) 9))))
	  (delete-region data-start data-end)
	  ;; insert the new data...
	  (goto-char data-start)
	  (insert-buffer-substring subfile)
	  (setq subfile-size
		(encode-coding-region
		 data-start (+ data-start subfile-size) coding))
	  ;;
	  ;; pad the new data out to a multiple of 512...
	  (let ((subfile-size-pad (ash (ash (+ subfile-size 511) -9) 9)))
	    (goto-char (+ data-start subfile-size))
	    (insert (make-string (- subfile-size-pad subfile-size) 0))
	    ;;
	    ;; update the data pointer of this and all following files...
	    (tar-setf (tar-header-size tokens) subfile-size)
	    (let ((difference (- subfile-size-pad size-pad)))
	      (dolist (desc following-descs)
		(tar-setf (tar-desc-data-start desc)
			  (+ (tar-desc-data-start desc) difference))))
	    ;;
	    ;; Update the size field in the header block.
	    (let ((header-start (- data-start 512)))
	      (goto-char (+ header-start tar-size-offset))
	      (delete-region (point) (+ (point) 12))
	      (insert (format "%11o" subfile-size))
	      (insert ? )
	      ;;
	      ;; Maybe update the datestamp.
	      (if (not tar-update-datestamp)
		  nil
		(goto-char (+ header-start tar-time-offset))
		(delete-region (point) (+ (point) 12))
		(insert (tar-octal-time (current-time)))
		(insert ? ))
	      ;;
	      ;; compute a new checksum and insert it.
	      (let ((chk (tar-header-block-checksum
			  (buffer-substring header-start data-start))))
		(goto-char (+ header-start tar-chk-offset))
		(delete-region (point) (+ (point) 8))
		(insert (format "%6o" chk))
		(insert 0)
		(insert ? )
		(tar-setf (tar-header-checksum tokens) chk)))
	    ;;
	    ;; alter the descriptor-line...
	    ;;
	    (let ((position (- (length tar-parse-info) (length head))))
	      (goto-char (point-min))
	      (next-line position)
	      (beginning-of-line)
	      (let ((p (point))
		    after
		    (m (set-marker (make-marker) tar-header-offset)))
		(forward-line 1)
		(setq after (point))
		;; Insert the new text after the old, before deleting,
		;; to preserve the window start.
		(let ((line (tar-header-block-summarize tokens t)))
		  (insert-before-markers (string-as-unibyte line) "\n"))
		(delete-region p after)
		(setq tar-header-offset (marker-position m)))
	      )))
	;; after doing the insertion, add any final padding that may be necessary.
	(tar-pad-to-blocksize))
       (narrow-to-region (point-min) tar-header-offset)
       (set-buffer-multibyte tar-buffer-multibyte)))
    (set-buffer-modified-p t)   ; mark the tar file as modified
    (tar-next-line 0)
    (set-buffer subfile)
    ;; Restore the buffer multibyteness.
    (set-buffer-multibyte subfile-multibyte)
    (set-buffer-modified-p nil) ; mark the tar subfile as unmodified
    (message "Saved into tar-buffer `%s'.  Be sure to save that buffer!"
	     (buffer-name tar-superior-buffer))
    ;; Prevent basic-save-buffer from changing our coding-system.
    (setq last-coding-system-used buffer-file-coding-system)
    ;; Prevent ordinary saving from happening.
    t)))


;; When this function is called, it is sure that the buffer is unibyte.
(defun tar-pad-to-blocksize ()
  "If we are being anal about tar file blocksizes, fix up the current buffer.
Leaves the region wide."
  (if (null tar-anal-blocksize)
      nil
    (widen)
    (let* ((last-desc (nth (1- (length tar-parse-info)) tar-parse-info))
	   (start (tar-desc-data-start last-desc))
	   (tokens (tar-desc-tokens last-desc))
	   (link-p (tar-header-link-type tokens))
	   (size (if link-p 0 (tar-header-size tokens)))
	   (data-end (+ start size))
	   (bbytes (ash tar-anal-blocksize 9))
	   (pad-to (+ bbytes (* bbytes (/ (- data-end (point-min)) bbytes))))
	   (inhibit-read-only t) ; ##
	   )
      ;; If the padding after the last data is too long, delete some;
      ;; else insert some until we are padded out to the right number of blocks.
      ;;
      (let ((goal-end (+ (or tar-header-offset 0) pad-to)))
        (if (> (point-max) goal-end)
            (delete-region goal-end (point-max))
          (goto-char (point-max))
	  (insert (make-string (- goal-end (point-max)) ?\0)))))))


;; Used in write-file-hook to write tar-files out correctly.
(defun tar-mode-write-file ()
  (unwind-protect
      (save-excursion
	(widen)
	;; Doing this here confuses things - the region gets left too wide!
	;; I suppose this is run in a context where changing the buffer is bad.
	;; (tar-pad-to-blocksize)
	;; tar-header-offset turns out to be null for files fetched with W3,
	;; at least.
	(let ((coding-system-for-write 'no-conversion))
	  (write-region (if tar-header-offset
			    (byte-to-position tar-header-offset)
			  (point-min))
			(point-max)
			buffer-file-name nil t))
	(tar-clear-modification-flags)
	(set-buffer-modified-p nil))
    (narrow-to-region (point-min) (byte-to-position tar-header-offset)))
  ;; Return t because we've written the file.
  t)

(provide 'tar-mode)

;; arch-tag: 8a585a4a-340e-42c2-89e7-d3b1013a4b78
;;; tar-mode.el ends here
