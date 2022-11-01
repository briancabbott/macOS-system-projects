;;; syntax.el --- helper functions to find syntactic context

;; Copyright (C) 2000, 2001, 2002, 2003, 2004,
;;   2005, 2006, 2007 Free Software Foundation, Inc.

;; Maintainer: FSF
;; Keywords: internal

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

;; The main exported function is `syntax-ppss'.  You might also need
;; to call `syntax-ppss-flush-cache' or to add it to
;; before-change-functions'(although this is automatically done by
;; syntax-ppss when needed, but that might fail if syntax-ppss is
;; called in a context where before-change-functions is temporarily
;; let-bound to nil).

;;; Todo:

;; - do something about the case where the syntax-table is changed.
;;   This typically happens with tex-mode and its `$' operator.
;; - move font-lock-syntactic-keywords in here.  Then again, maybe not.
;; - new functions `syntax-state', ... to replace uses of parse-partial-state
;;   with something higher-level (similar to syntax-ppss-context).
;; - interaction with mmm-mode.

;;; Code:

;; Note: PPSS stands for `parse-partial-sexp state'

(eval-when-compile (require 'cl))

(defvar font-lock-beginning-of-syntax-function)

(defsubst syntax-ppss-depth (ppss)
  (nth 0 ppss))

(defun syntax-ppss-toplevel-pos (ppss)
  "Get the latest syntactically outermost position found in a syntactic scan.
PPSS is a scan state, as returned by `partial-parse-sexp' or `syntax-ppss'.
An \"outermost position\" means one that it is outside of any syntactic entity:
outside of any parentheses, comments, or strings encountered in the scan.
If no such position is recorded in PPSS (because the end of the scan was
itself at the outermost level), return nil."
  ;; BEWARE! We rely on the undocumented 9th field.  The 9th field currently
  ;; contains the list of positions of the enclosing open-parens.
  ;; I.e. those positions are outside of any string/comment and the first of
  ;; those is outside of any paren (i.e. corresponds to a nil ppss).
  ;; If this list is empty but we are in a string or comment, then the 8th
  ;; field contains a similar "toplevel" position.
  (or (car (nth 9 ppss))
      (nth 8 ppss)))

(defsubst syntax-ppss-context (ppss)
  (cond
   ((nth 3 ppss) 'string)
   ((nth 4 ppss) 'comment)
   (t nil)))

(defvar syntax-ppss-max-span 20000
  "Threshold below which cache info is deemed unnecessary.
We try to make sure that cache entries are at least this far apart
from each other, to avoid keeping too much useless info.")

(defvar syntax-begin-function nil
  "Function to move back outside of any comment/string/paren.
This function should move the cursor back to some syntactically safe
point (where the PPSS is equivalent to nil).")

(defvar syntax-ppss-cache nil
  "List of (POS . PPSS) pairs, in decreasing POS order.")
(make-variable-buffer-local 'syntax-ppss-cache)
(defvar syntax-ppss-last nil
  "Cache of (LAST-POS . LAST-PPSS).")
(make-variable-buffer-local 'syntax-ppss-last)

(defalias 'syntax-ppss-after-change-function 'syntax-ppss-flush-cache)
(defun syntax-ppss-flush-cache (beg &rest ignored)
  "Flush the cache of `syntax-ppss' starting at position BEG."
  ;; Flush invalid cache entries.
  (while (and syntax-ppss-cache (> (caar syntax-ppss-cache) beg))
    (setq syntax-ppss-cache (cdr syntax-ppss-cache)))
  ;; Throw away `last' value if made invalid.
  (when (< beg (or (car syntax-ppss-last) 0))
    ;; If syntax-begin-function jumped to BEG, then the old state at BEG can
    ;; depend on the text after BEG (which is presumably changed).  So if
    ;; BEG=(car (nth 10 syntax-ppss-last)) don't reuse that data because the
    ;; assumed nil state at BEG may not be valid any more.
    (if (<= beg (or (syntax-ppss-toplevel-pos (cdr syntax-ppss-last))
                    (nth 3 syntax-ppss-last)
                    0))
	(setq syntax-ppss-last nil)
      (setcar syntax-ppss-last nil)))
  ;; Unregister if there's no cache left.  Sadly this doesn't work
  ;; because `before-change-functions' is temporarily bound to nil here.
  ;; (unless syntax-ppss-cache
  ;;   (remove-hook 'before-change-functions 'syntax-ppss-flush-cache t))
  )

(defvar syntax-ppss-stats
  [(0 . 0.0) (0 . 0.0) (0 . 0.0) (0 . 0.0) (0 . 0.0) (1 . 2500.0)])
(defun syntax-ppss-stats ()
  (mapcar (lambda (x)
	    (condition-case nil
		(cons (car x) (truncate (/ (cdr x) (car x))))
	      (error nil)))
	  syntax-ppss-stats))

(defun syntax-ppss (&optional pos)
  "Parse-Partial-Sexp State at POS.
The returned value is the same as `parse-partial-sexp' except that
the 2nd and 6th values of the returned state cannot be relied upon.
Point is at POS when this function returns."
  ;; Default values.
  (unless pos (setq pos (point)))
  ;;
  (let ((old-ppss (cdr syntax-ppss-last))
	(old-pos (car syntax-ppss-last))
	(ppss nil)
	(pt-min (point-min)))
    (if (and old-pos (> old-pos pos)) (setq old-pos nil))
    ;; Use the OLD-POS if usable and close.  Don't update the `last' cache.
    (condition-case nil
	(if (and old-pos (< (- pos old-pos)
			    ;; The time to use syntax-begin-function and
			    ;; find PPSS is assumed to be about 2 * distance.
			    (* 2 (/ (cdr (aref syntax-ppss-stats 5))
				    (1+ (car (aref syntax-ppss-stats 5)))))))
	    (progn
	      (incf (car (aref syntax-ppss-stats 0)))
	      (incf (cdr (aref syntax-ppss-stats 0)) (- pos old-pos))
	      (parse-partial-sexp old-pos pos nil nil old-ppss))

	  (cond
	   ;; Use OLD-PPSS if possible and close enough.
	   ((and (not old-pos) old-ppss
                 ;; If `pt-min' is too far from `pos', we could try to use
		 ;; other positions in (nth 9 old-ppss), but that doesn't
		 ;; seem to happen in practice and it would complicate this
		 ;; code (and the before-change-function code even more).
		 ;; But maybe it would be useful in "degenerate" cases such
		 ;; as when the whole file is wrapped in a set
		 ;; of parentheses.
		 (setq pt-min (or (syntax-ppss-toplevel-pos old-ppss)
				  (nth 2 old-ppss)))
		 (<= pt-min pos) (< (- pos pt-min) syntax-ppss-max-span))
	    (incf (car (aref syntax-ppss-stats 1)))
	    (incf (cdr (aref syntax-ppss-stats 1)) (- pos pt-min))
	    (setq ppss (parse-partial-sexp pt-min pos)))
	   ;; The OLD-* data can't be used.  Consult the cache.
	   (t
	    (let ((cache-pred nil)
		  (cache syntax-ppss-cache)
		  (pt-min (point-min))
		  ;; I differentiate between PT-MIN and PT-BEST because
		  ;; I feel like it might be important to ensure that the
		  ;; cache is only filled with 100% sure data (whereas
		  ;; syntax-begin-function might return incorrect data).
		  ;; Maybe that's just stupid.
		  (pt-best (point-min))
		  (ppss-best nil))
	      ;; look for a usable cache entry.
	      (while (and cache (< pos (caar cache)))
		(setq cache-pred cache)
		(setq cache (cdr cache)))
	      (if cache (setq pt-min (caar cache) ppss (cdar cache)))

	      ;; Setup the before-change function if necessary.
	      (unless (or syntax-ppss-cache syntax-ppss-last)
		(add-hook 'before-change-functions
			  'syntax-ppss-flush-cache t t))

	      ;; Use the best of OLD-POS and CACHE.
	      (if (or (not old-pos) (< old-pos pt-min))
		  (setq pt-best pt-min ppss-best ppss)
		(incf (car (aref syntax-ppss-stats 4)))
		(incf (cdr (aref syntax-ppss-stats 4)) (- pos old-pos))
		(setq pt-best old-pos ppss-best old-ppss))

	      ;; Use the `syntax-begin-function' if available.
	      ;; We could try using that function earlier, but:
	      ;; - The result might not be 100% reliable, so it's better to use
	      ;;   the cache if available.
	      ;; - The function might be slow.
	      ;; - If this function almost always finds a safe nearby spot,
	      ;;   the cache won't be populated, so consulting it is cheap.
	      (when (and (not syntax-begin-function)
			 (boundp 'font-lock-beginning-of-syntax-function)
			 font-lock-beginning-of-syntax-function)
		(set (make-local-variable 'syntax-begin-function)
		     font-lock-beginning-of-syntax-function))
	      (when (and syntax-begin-function
			 (progn (goto-char pos)
				(funcall syntax-begin-function)
				;; Make sure it's better.
				(> (point) pt-best))
			 ;; Simple sanity check.
			 (not (memq (get-text-property (point) 'face)
				    '(font-lock-string-face font-lock-doc-face
				      font-lock-comment-face))))
		(incf (car (aref syntax-ppss-stats 5)))
		(incf (cdr (aref syntax-ppss-stats 5)) (- pos (point)))
		(setq pt-best (point) ppss-best nil))

	      (cond
	       ;; Quick case when we found a nearby pos.
	       ((< (- pos pt-best) syntax-ppss-max-span)
		(incf (car (aref syntax-ppss-stats 2)))
		(incf (cdr (aref syntax-ppss-stats 2)) (- pos pt-best))
		(setq ppss (parse-partial-sexp pt-best pos nil nil ppss-best)))
	       ;; Slow case: compute the state from some known position and
	       ;; populate the cache so we won't need to do it again soon.
	       (t
		(incf (car (aref syntax-ppss-stats 3)))
		(incf (cdr (aref syntax-ppss-stats 3)) (- pos pt-min))

		;; If `pt-min' is too far, add a few intermediate entries.
		(while (> (- pos pt-min) (* 2 syntax-ppss-max-span))
		  (setq ppss (parse-partial-sexp
			      pt-min (setq pt-min (/ (+ pt-min pos) 2))
			      nil nil ppss))
		  (let ((pair (cons pt-min ppss)))
		    (if cache-pred
			(push pair (cdr cache-pred))
		      (push pair syntax-ppss-cache))))

		;; Compute the actual return value.
		(setq ppss (parse-partial-sexp pt-min pos nil nil ppss))

		;; Debugging check.
		;; (let ((real-ppss (parse-partial-sexp (point-min) pos)))
		;;   (setcar (last ppss 4) 0)
		;;   (setcar (last real-ppss 4) 0)
		;;   (setcar (last ppss 8) nil)
		;;   (setcar (last real-ppss 8) nil)
		;;   (unless (equal ppss real-ppss)
		;;     (message "!!Syntax: %s != %s" ppss real-ppss)
		;;     (setq ppss real-ppss)))

		;; Store it in the cache.
		(let ((pair (cons pos ppss)))
		  (if cache-pred
		      (if (> (- (caar cache-pred) pos) syntax-ppss-max-span)
			  (push pair (cdr cache-pred))
			(setcar cache-pred pair))
		    (if (or (null syntax-ppss-cache)
			    (> (- (caar syntax-ppss-cache) pos)
			       syntax-ppss-max-span))
			(push pair syntax-ppss-cache)
		      (setcar syntax-ppss-cache pair)))))))))

	  (setq syntax-ppss-last (cons pos ppss))
	  ppss)
      (args-out-of-range
       ;; If the buffer is more narrowed than when we built the cache,
       ;; we may end up calling parse-partial-sexp with a position before
       ;; point-min.  In that case, just parse from point-min assuming
       ;; a nil state.
       (parse-partial-sexp (point-min) pos)))))

;; Debugging functions

(defun syntax-ppss-debug ()
  (let ((pt nil)
	(min-diffs nil))
    (dolist (x (append syntax-ppss-cache (list (cons (point-min) nil))))
      (when pt (push (- pt (car x)) min-diffs))
      (setq pt (car x)))
    min-diffs))

;; XEmacs compatibility functions

;; (defun buffer-syntactic-context (&optional buffer)
;;   "Syntactic context at point in BUFFER.
;; Either of `string', `comment' or `nil'.
;; This is an XEmacs compatibility function."
;;   (with-current-buffer (or buffer (current-buffer))
;;     (syntax-ppss-context (syntax-ppss))))

;; (defun buffer-syntactic-context-depth (&optional buffer)
;;   "Syntactic parenthesis depth at point in BUFFER.
;; This is an XEmacs compatibility function."
;;   (with-current-buffer (or buffer (current-buffer))
;;     (syntax-ppss-depth (syntax-ppss))))

(provide 'syntax)

;; arch-tag: 302f1eeb-e77c-4680-a8c5-c543e01161a5
;;; syntax.el ends here
