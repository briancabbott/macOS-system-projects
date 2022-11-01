;;; cal-bahai.el --- calendar functions for the Baha'i calendar.

;; Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006, 2007
;;   Free Software Foundation, Inc.

;; Author: John Wiegley <johnw@gnu.org>
;; Keywords: calendar
;; Human-Keywords: Baha'i calendar, Baha'i, Bahai, calendar, diary

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

;; This collection of functions implements the features of calendar.el
;; and diary.el that deal with the Baha'i calendar.

;; The Baha'i (http://www.bahai.org) calendar system is based on a
;; solar cycle of 19 months with 19 days each.  The four remaining
;; "intercalary" days are called the Ayyam-i-Ha (days of Ha), and are
;; placed between the 18th and 19th months.  They are meant as a time
;; of festivals preceding the 19th month, which is the month of
;; fasting.  In Gregorian leap years, there are 5 of these days (Ha
;; has the numerical value of 5 in the arabic abjad, or
;; letter-to-number, reckoning).

;; Each month is named after an attribute of God, as are the 19 days
;; -- which have the same names as the months.  There is also a name
;; for each year in every 19 year cycle.  These cycles are called
;; Vahids.  A cycle of 19 Vahids (361 years) is called a Kullu-Shay,
;; which means "all things".

;; The calendar was named the "Badi calendar" by its author, the Bab.
;; It uses a week of seven days, corresponding to the Gregorian week,
;; each of which has its own name, again patterned after the
;; attributes of God.

;; Note: The days of Ayyam-i-Ha are encoded as zero and negative
;; offsets from the first day of the final month.  So, (19 -3 157) is
;; the first day of Ayyam-i-Ha, in the year 157 BE.

;;; Code:

(defvar date)
(defvar displayed-month)
(defvar displayed-year)
(defvar number)
(defvar original-date)

(require 'cal-julian)

(defvar bahai-calendar-month-name-array
  ["Baha" "Jalal" "Jamal" "`Azamat" "Nur" "Rahmat" "Kalimat" "Kamal"
   "Asma" "`Izzat" "Mashiyyat" "`Ilm" "Qudrat" "Qawl" "Masa'il"
   "Sharaf" "Sultan" "Mulk" "`Ala"])

(defvar calendar-bahai-epoch (calendar-absolute-from-gregorian '(3 21 1844))
  "Absolute date of start of Baha'i calendar = March 19, 622 A.D. (Julian).")

(defun bahai-calendar-leap-year-p (year)
  "True if YEAR is a leap year on the Baha'i calendar."
  (calendar-leap-year-p (+ year 1844)))

(defvar bahai-calendar-leap-base
  (+ (/ 1844 4) (- (/ 1844 100)) (/ 1844 400)))

(defun calendar-absolute-from-bahai (date)
  "Compute absolute date from Baha'i date DATE.
The absolute date is the number of days elapsed since the (imaginary)
Gregorian date Sunday, December 31, 1 BC."
  (let* ((month (extract-calendar-month date))
	 (day (extract-calendar-day date))
	 (year (extract-calendar-year date))
	 (prior-years (+ (1- year) 1844))
	 (leap-days (- (+ (/ prior-years 4) ; Leap days in prior years.
			  (- (/ prior-years 100))
			  (/ prior-years 400))
		       bahai-calendar-leap-base)))
    (+ (1- calendar-bahai-epoch)	; Days before epoch
       (* 365 (1- year))		; Days in prior years.
       leap-days
       (calendar-sum m 1 (< m month) 19)
       (if (= month 19) 4 0)
       day)))				; Days so far this month.

(defun calendar-bahai-from-absolute (date)
  "Baha'i year corresponding to the absolute DATE."
  (if (< date calendar-bahai-epoch)
      (list 0 0 0) ;; pre-Baha'i date
    (let* ((greg (calendar-gregorian-from-absolute date))
	   (year (+ (- (extract-calendar-year greg) 1844)
		    (if (or (> (extract-calendar-month greg) 3)
			    (and (= (extract-calendar-month greg) 3)
				 (>= (extract-calendar-day greg) 21)))
			1 0)))
           (month ;; Search forward from Baha.
            (1+ (calendar-sum m 1
			      (> date
				 (calendar-absolute-from-bahai
				  (list m 19 year)))
			      1)))
           (day	;; Calculate the day by subtraction.
            (- date
               (1- (calendar-absolute-from-bahai (list month 1 year))))))
      (list month day year))))

(defun calendar-bahai-date-string (&optional date)
  "String of Baha'i date of Gregorian DATE.
Defaults to today's date if DATE is not given."
  (let* ((bahai-date (calendar-bahai-from-absolute
                       (calendar-absolute-from-gregorian
                        (or date (calendar-current-date)))))
         (y (extract-calendar-year bahai-date))
         (m (extract-calendar-month bahai-date))
         (d (extract-calendar-day bahai-date)))
    (let ((monthname
	   (if (and (= m 19)
		    (<= d 0))
	       "Ayyam-i-Ha"
	     (aref bahai-calendar-month-name-array (1- m))))
	  (day (int-to-string
		(if (<= d 0)
		    (if (bahai-calendar-leap-year-p y)
			(+ d 5)
		      (+ d 4))
		  d)))
	  (dayname nil)
	  (month (int-to-string m))
	  (year (int-to-string y)))
      (mapconcat 'eval calendar-date-display-form ""))))

(defun calendar-print-bahai-date ()
  "Show the Baha'i calendar equivalent of the selected date."
  (interactive)
  (message "Baha'i date: %s"
           (calendar-bahai-date-string (calendar-cursor-to-date t))))

(defun calendar-goto-bahai-date (date &optional noecho)
  "Move cursor to Baha'i date DATE.
Echo Baha'i date unless NOECHO is t."
  (interactive (bahai-prompt-for-date))
  (calendar-goto-date (calendar-gregorian-from-absolute
                       (calendar-absolute-from-bahai date)))
  (or noecho (calendar-print-bahai-date)))

(defun bahai-prompt-for-date ()
  "Ask for a Baha'i date."
  (let* ((today (calendar-current-date))
         (year (calendar-read
                "Baha'i calendar year (not 0): "
                '(lambda (x) (/= x 0))
                (int-to-string
                 (extract-calendar-year
                  (calendar-bahai-from-absolute
                   (calendar-absolute-from-gregorian today))))))
         (completion-ignore-case t)
         (month (cdr (assoc
                       (completing-read
                        "Baha'i calendar month name: "
                        (mapcar 'list
                                (append bahai-calendar-month-name-array nil))
                        nil t)
                      (calendar-make-alist bahai-calendar-month-name-array
                                           1))))
         (day (calendar-read "Baha'i calendar day (1-19): "
			     '(lambda (x) (and (< 0 x) (<= x 19))))))
    (list (list month day year))))

(defun diary-bahai-date ()
  "Baha'i calendar equivalent of date diary entry."
  (format "Baha'i date: %s" (calendar-bahai-date-string date)))

(defun holiday-bahai (month day string)
  "Holiday on MONTH, DAY (Baha'i) called STRING.
If MONTH, DAY (Baha'i) is visible, the value returned is corresponding
Gregorian date in the form of the list (((month day year) STRING)).  Returns
nil if it is not visible in the current calendar window."
  (let* ((bahai-date (calendar-bahai-from-absolute
		      (calendar-absolute-from-gregorian
		       (list displayed-month 15 displayed-year))))
         (m (extract-calendar-month bahai-date))
         (y (extract-calendar-year bahai-date))
	 (date))
    (if (< m 1)
        nil ;;   Baha'i calendar doesn't apply.
      (increment-calendar-month m y (- 10 month))
      (if (> m 7) ;;  Baha'i date might be visible
          (let ((date (calendar-gregorian-from-absolute
                       (calendar-absolute-from-bahai (list month day y)))))
            (if (calendar-date-is-visible-p date)
                (list (list date string))))))))

(defun list-bahai-diary-entries ()
  "Add any Baha'i date entries from the diary file to `diary-entries-list'.
Baha'i date diary entries must be prefaced by an
`bahai-diary-entry-symbol' (normally a `B').  The same diary date
forms govern the style of the Baha'i calendar entries, except that the
Baha'i month names must be given numerically.  The Baha'i months are
numbered from 1 to 19 with Baha being 1 and 19 being `Ala.  If a
Baha'i date diary entry begins with a `diary-nonmarking-symbol', the
entry will appear in the diary listing, but will not be marked in the
calendar.  This function is provided for use with the
`nongregorian-diary-listing-hook'."
  (if (< 0 number)
      (let ((buffer-read-only nil)
            (diary-modified (buffer-modified-p))
            (gdate original-date)
            (mark (regexp-quote diary-nonmarking-symbol)))
        (calendar-for-loop i from 1 to number do
           (let* ((d diary-date-forms)
                  (bdate (calendar-bahai-from-absolute
                          (calendar-absolute-from-gregorian gdate)))
                  (month (extract-calendar-month bdate))
                  (day (extract-calendar-day bdate))
                  (year (extract-calendar-year bdate)))
             (while d
               (let*
                   ((date-form (if (equal (car (car d)) 'backup)
                                   (cdr (car d))
                                 (car d)))
                    (backup (equal (car (car d)) 'backup))
                    (dayname
                     (concat
                      (calendar-day-name gdate) "\\|"
                      (substring (calendar-day-name gdate) 0 3) ".?"))
                    (calendar-month-name-array
                     bahai-calendar-month-name-array)
                    (monthname
                     (concat
                      "\\*\\|"
                      (calendar-month-name month)))
                    (month (concat "\\*\\|0*" (int-to-string month)))
                    (day (concat "\\*\\|0*" (int-to-string day)))
                    (year
                     (concat
                      "\\*\\|0*" (int-to-string year)
                      (if abbreviated-calendar-year
                          (concat "\\|" (int-to-string (% year 100)))
                        "")))
                    (regexp
                     (concat
                      "\\(\\`\\|\^M\\|\n\\)" mark "?"
                      (regexp-quote bahai-diary-entry-symbol)
                      "\\("
                      (mapconcat 'eval date-form "\\)\\(")
                      "\\)"))
                    (case-fold-search t))
                 (goto-char (point-min))
                 (while (re-search-forward regexp nil t)
                   (if backup (re-search-backward "\\<" nil t))
                   (if (and (or (char-equal (preceding-char) ?\^M)
                                (char-equal (preceding-char) ?\n))
                            (not (looking-at " \\|\^I")))
                       ;;  Diary entry that consists only of date.
                       (backward-char 1)
                     ;;  Found a nonempty diary entry--make it visible and
                     ;;  add it to the list.
                     (let ((entry-start (point))
                           (date-start))
                       (re-search-backward "\^M\\|\n\\|\\`")
                       (setq date-start (point))
                       (re-search-forward "\^M\\|\n" nil t 2)
                       (while (looking-at " \\|\^I")
                         (re-search-forward "\^M\\|\n" nil t))
                       (backward-char 1)
                       (subst-char-in-region date-start (point) ?\^M ?\n t)
                       (add-to-diary-list
                        gdate
                        (buffer-substring-no-properties entry-start (point))
                        (buffer-substring-no-properties
                         (1+ date-start) (1- entry-start)))))))
               (setq d (cdr d))))
           (setq gdate
                 (calendar-gregorian-from-absolute
                  (1+ (calendar-absolute-from-gregorian gdate)))))
           (set-buffer-modified-p diary-modified))
        (goto-char (point-min))))

(defun mark-bahai-diary-entries ()
  "Mark days in the calendar window that have Baha'i date diary entries.
Each entry in diary-file (or included files) visible in the calendar
window is marked.  Baha'i date entries are prefaced by a
bahai-diary-entry-symbol \(normally a B`I').  The same
diary-date-forms govern the style of the Baha'i calendar entries,
except that the Baha'i month names must be spelled in full.  The
Baha'i months are numbered from 1 to 12 with Baha being 1 and 12 being
`Ala.  Baha'i date diary entries that begin with a
diary-nonmarking-symbol will not be marked in the calendar.  This
function is provided for use as part of the
nongregorian-diary-marking-hook."
  (let ((d diary-date-forms))
    (while d
      (let*
          ((date-form (if (equal (car (car d)) 'backup)
                          (cdr (car d))
                        (car d)));; ignore 'backup directive
           (dayname (diary-name-pattern calendar-day-name-array))
           (monthname
            (concat
             (diary-name-pattern bahai-calendar-month-name-array t)
             "\\|\\*"))
           (month "[0-9]+\\|\\*")
           (day "[0-9]+\\|\\*")
           (year "[0-9]+\\|\\*")
           (l (length date-form))
           (d-name-pos (- l (length (memq 'dayname date-form))))
           (d-name-pos (if (/= l d-name-pos) (+ 2 d-name-pos)))
           (m-name-pos (- l (length (memq 'monthname date-form))))
           (m-name-pos (if (/= l m-name-pos) (+ 2 m-name-pos)))
           (d-pos (- l (length (memq 'day date-form))))
           (d-pos (if (/= l d-pos) (+ 2 d-pos)))
           (m-pos (- l (length (memq 'month date-form))))
           (m-pos (if (/= l m-pos) (+ 2 m-pos)))
           (y-pos (- l (length (memq 'year date-form))))
           (y-pos (if (/= l y-pos) (+ 2 y-pos)))
           (regexp
            (concat
             "\\(\\`\\|\^M\\|\n\\)"
             (regexp-quote bahai-diary-entry-symbol)
             "\\("
             (mapconcat 'eval date-form "\\)\\(")
             "\\)"))
           (case-fold-search t))
        (goto-char (point-min))
        (while (re-search-forward regexp nil t)
          (let* ((dd-name
                  (if d-name-pos
                      (buffer-substring
                       (match-beginning d-name-pos)
                       (match-end d-name-pos))))
                 (mm-name
                  (if m-name-pos
                      (buffer-substring
                       (match-beginning m-name-pos)
                       (match-end m-name-pos))))
                 (mm (string-to-number
                      (if m-pos
                          (buffer-substring
                           (match-beginning m-pos)
                           (match-end m-pos))
                        "")))
                 (dd (string-to-number
                      (if d-pos
                          (buffer-substring
                           (match-beginning d-pos)
                           (match-end d-pos))
                        "")))
                 (y-str (if y-pos
                            (buffer-substring
                             (match-beginning y-pos)
                             (match-end y-pos))))
                 (yy (if (not y-str)
                         0
                       (if (and (= (length y-str) 2)
                                abbreviated-calendar-year)
                           (let* ((current-y
                                   (extract-calendar-year
                                    (calendar-bahai-from-absolute
                                     (calendar-absolute-from-gregorian
                                      (calendar-current-date)))))
                                  (y (+ (string-to-number y-str)
                                        (* 100 (/ current-y 100)))))
                             (if (> (- y current-y) 50)
                                 (- y 100)
                               (if (> (- current-y y) 50)
                                   (+ y 100)
                                 y)))
                         (string-to-number y-str)))))
            (if dd-name
                (mark-calendar-days-named
                 (cdr (assoc-string (substring dd-name 0 3)
                                    (calendar-make-alist
                                     calendar-day-name-array
                                     0
                                     '(lambda (x) (substring x 0 3)))
                                    t)))
              (if mm-name
                  (if (string-equal mm-name "*")
                      (setq mm 0)
                    (setq mm
                          (cdr (assoc-string
                                mm-name
                                (calendar-make-alist
                                  bahai-calendar-month-name-array)
                                t)))))
              (mark-bahai-calendar-date-pattern mm dd yy)))))
      (setq d (cdr d)))))

(defun mark-bahai-calendar-date-pattern (month day year)
  "Mark dates in calendar window that conform to Baha'i date MONTH/DAY/YEAR.
A value of 0 in any position is a wildcard."
  (save-excursion
    (set-buffer calendar-buffer)
    (if (and (/= 0 month) (/= 0 day))
        (if (/= 0 year)
            ;; Fully specified Baha'i date.
            (let ((date (calendar-gregorian-from-absolute
                         (calendar-absolute-from-bahai
                          (list month day year)))))
              (if (calendar-date-is-visible-p date)
                  (mark-visible-calendar-date date)))
          ;; Month and day in any year--this taken from the holiday stuff.
          (let* ((bahai-date (calendar-bahai-from-absolute
                                (calendar-absolute-from-gregorian
                                 (list displayed-month 15 displayed-year))))
                 (m (extract-calendar-month bahai-date))
                 (y (extract-calendar-year bahai-date))
                 (date))
            (if (< m 1)
                nil;;   Baha'i calendar doesn't apply.
              (increment-calendar-month m y (- 10 month))
              (if (> m 7);;  Baha'i date might be visible
                  (let ((date (calendar-gregorian-from-absolute
                               (calendar-absolute-from-bahai
                                (list month day y)))))
                    (if (calendar-date-is-visible-p date)
                        (mark-visible-calendar-date date)))))))
      ;; Not one of the simple cases--check all visible dates for match.
      ;; Actually, the following code takes care of ALL of the cases, but
      ;; it's much too slow to be used for the simple (common) cases.
      (let ((m displayed-month)
            (y displayed-year)
            (first-date)
            (last-date))
        (increment-calendar-month m y -1)
        (setq first-date
              (calendar-absolute-from-gregorian
               (list m 1 y)))
        (increment-calendar-month m y 2)
        (setq last-date
              (calendar-absolute-from-gregorian
               (list m (calendar-last-day-of-month m y) y)))
        (calendar-for-loop date from first-date to last-date do
          (let* ((b-date (calendar-bahai-from-absolute date))
                 (i-month (extract-calendar-month b-date))
                 (i-day (extract-calendar-day b-date))
                 (i-year (extract-calendar-year b-date)))
            (and (or (zerop month)
                     (= month i-month))
                 (or (zerop day)
                     (= day i-day))
                 (or (zerop year)
                     (= year i-year))
                 (mark-visible-calendar-date
                  (calendar-gregorian-from-absolute date)))))))))

(defun insert-bahai-diary-entry (arg)
  "Insert a diary entry.
For the Baha'i date corresponding to the date indicated by point.
Prefix arg will make the entry nonmarking."
  (interactive "P")
  (let* ((calendar-month-name-array bahai-calendar-month-name-array))
    (make-diary-entry
     (concat
      bahai-diary-entry-symbol
      (calendar-date-string
       (calendar-bahai-from-absolute
        (calendar-absolute-from-gregorian
         (calendar-cursor-to-date t)))
       nil t))
     arg)))

(defun insert-monthly-bahai-diary-entry (arg)
  "Insert a monthly diary entry.
For the day of the Baha'i month corresponding to the date indicated by point.
Prefix arg will make the entry nonmarking."
  (interactive "P")
  (let* ((calendar-date-display-form
          (if european-calendar-style '(day " * ") '("* " day )))
         (calendar-month-name-array bahai-calendar-month-name-array))
    (make-diary-entry
     (concat
      bahai-diary-entry-symbol
      (calendar-date-string
       (calendar-bahai-from-absolute
        (calendar-absolute-from-gregorian
         (calendar-cursor-to-date t)))))
     arg)))

(defun insert-yearly-bahai-diary-entry (arg)
  "Insert an annual diary entry.
For the day of the Baha'i year corresponding to the date indicated by point.
Prefix arg will make the entry nonmarking."
  (interactive "P")
  (let* ((calendar-date-display-form
          (if european-calendar-style
              '(day " " monthname)
            '(monthname " " day)))
         (calendar-month-name-array bahai-calendar-month-name-array))
    (make-diary-entry
     (concat
      bahai-diary-entry-symbol
      (calendar-date-string
       (calendar-bahai-from-absolute
        (calendar-absolute-from-gregorian
         (calendar-cursor-to-date t)))))
     arg)))

(provide 'cal-bahai)

;;; arch-tag: c1cb1d67-862a-4264-a01c-41cb4df01f14
;;; cal-bahai.el ends here
