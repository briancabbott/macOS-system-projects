;;; calc-macs.el --- important macros for Calc

;; Copyright (C) 1990, 1991, 1992, 1993, 2001, 2002, 2003, 2004,
;;   2005, 2006, 2007 Free Software Foundation, Inc.

;; Author: David Gillespie <daveg@synaptics.com>
;; Maintainer: Jay Belanger <jay.p.belanger@gmail.com>

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

(defmacro calc-wrapper (&rest body)
  `(calc-do (function (lambda ()
			,@body))))

(defmacro calc-slow-wrapper (&rest body)
  `(calc-do
    (function (lambda () ,@body)) (point)))

(defmacro math-showing-full-precision (form)
  `(let ((calc-float-format calc-full-float-format))
     ,form))

(defmacro math-with-extra-prec (delta &rest body)
  `(math-normalize
    (let ((calc-internal-prec (+ calc-internal-prec ,delta)))
      ,@body)))

(defmacro math-working (msg arg)	; [Public]
  `(if (eq calc-display-working-message 'lots)
       (math-do-working ,msg ,arg)))

(defmacro calc-with-default-simplification (&rest body)
  `(let ((calc-simplify-mode (and (not (memq calc-simplify-mode '(none num)))
				  calc-simplify-mode)))
     ,@body))

(defmacro calc-with-trail-buffer (&rest body)
  `(let ((save-buf (current-buffer))
	 (calc-command-flags nil))
     (with-current-buffer (calc-trail-display t)
       (progn
	 (goto-char calc-trail-pointer)
	 ,@body))))

;;; Faster in-line version zerop, normalized values only.
(defsubst Math-zerop (a)		; [P N]
  (if (consp a)
      (and (not (memq (car a) '(bigpos bigneg)))
	   (if (eq (car a) 'float)
	       (eq (nth 1 a) 0)
	     (math-zerop a)))
    (eq a 0)))

(defsubst Math-integer-negp (a)
  (if (consp a)
      (eq (car a) 'bigneg)
    (< a 0)))

(defsubst Math-integer-posp (a)
  (if (consp a)
      (eq (car a) 'bigpos)
    (> a 0)))

(defsubst Math-negp (a)
  (if (consp a)
      (or (eq (car a) 'bigneg)
	  (and (not (eq (car a) 'bigpos))
	       (if (memq (car a) '(frac float))
		   (Math-integer-negp (nth 1 a))
		 (math-negp a))))
    (< a 0)))

(defsubst Math-looks-negp (a)		; [P x] [Public]
  (or (Math-negp a)
      (and (consp a) (or (eq (car a) 'neg)
			 (and (memq (car a) '(* /))
			      (or (math-looks-negp (nth 1 a))
				  (math-looks-negp (nth 2 a))))))))

(defsubst Math-posp (a)
  (if (consp a)
      (or (eq (car a) 'bigpos)
	  (and (not (eq (car a) 'bigneg))
	       (if (memq (car a) '(frac float))
		   (Math-integer-posp (nth 1 a))
		 (math-posp a))))
    (> a 0)))

(defsubst Math-integerp (a)
  (or (not (consp a))
      (memq (car a) '(bigpos bigneg))))

(defsubst Math-natnump (a)
  (if (consp a)
      (eq (car a) 'bigpos)
    (>= a 0)))

(defsubst Math-ratp (a)
  (or (not (consp a))
      (memq (car a) '(bigpos bigneg frac))))

(defsubst Math-realp (a)
  (or (not (consp a))
      (memq (car a) '(bigpos bigneg frac float))))

(defsubst Math-anglep (a)
  (or (not (consp a))
      (memq (car a) '(bigpos bigneg frac float hms))))

(defsubst Math-numberp (a)
  (or (not (consp a))
      (memq (car a) '(bigpos bigneg frac float cplx polar))))

(defsubst Math-scalarp (a)
  (or (not (consp a))
      (memq (car a) '(bigpos bigneg frac float cplx polar hms))))

(defsubst Math-vectorp (a)
  (and (consp a) (eq (car a) 'vec)))

(defsubst Math-messy-integerp (a)
  (and (consp a)
       (eq (car a) 'float)
       (>= (nth 2 a) 0)))

(defsubst Math-objectp (a)		;  [Public]
  (or (not (consp a))
      (memq (car a)
	    '(bigpos bigneg frac float cplx polar hms date sdev intv mod))))

(defsubst Math-objvecp (a)		;  [Public]
  (or (not (consp a))
      (memq (car a)
	    '(bigpos bigneg frac float cplx polar hms date
		     sdev intv mod vec))))

;;; Compute the negative of A.  [O O; o o] [Public]
(defsubst Math-integer-neg (a)
  (if (consp a)
      (if (eq (car a) 'bigpos)
	  (cons 'bigneg (cdr a))
	(cons 'bigpos (cdr a)))
    (- a)))

(defsubst Math-equal (a b)
  (= (math-compare a b) 0))

(defsubst Math-lessp (a b)
  (= (math-compare a b) -1))

(defsubst Math-primp (a)
  (or (not (consp a))
      (memq (car a) '(bigpos bigneg frac float cplx polar
			     hms date mod var))))

(defsubst Math-num-integerp (a)
  (or (not (consp a))
      (memq (car a) '(bigpos bigneg))
      (and (eq (car a) 'float)
	   (>= (nth 2 a) 0))))

(defsubst Math-bignum-test (a)		; [B N; B s; b b]
  (if (consp a)
      a
    (math-bignum a)))

(defsubst Math-equal-int (a b)
  (or (eq a b)
      (and (consp a)
	   (eq (car a) 'float)
	   (eq (nth 1 a) b)
	   (= (nth 2 a) 0))))

(defsubst Math-natnum-lessp (a b)
  (if (consp a)
      (and (consp b)
	   (= (math-compare-bignum (cdr a) (cdr b)) -1))
    (or (consp b)
	(< a b))))

(provide 'calc-macs)

;;; arch-tag: 08ba8ec2-fcff-4b80-a079-ec661bdb057e
;;; calc-macs.el ends here
