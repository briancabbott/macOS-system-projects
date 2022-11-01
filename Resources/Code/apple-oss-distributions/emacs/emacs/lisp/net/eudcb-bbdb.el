;;; eudcb-bbdb.el --- Emacs Unified Directory Client - BBDB Backend

;; Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004,
;;   2005, 2006, 2007 Free Software Foundation, Inc.

;; Author: Oscar Figueiredo <oscar@cpe.fr>
;; Maintainer: Pavel Jan�k <Pavel@Janik.cz>
;; Keywords: comm

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
;;    This library provides an interface to use BBDB as a backend of
;;    the Emacs Unified Directory Client.

;;; Code:

(require 'eudc)
(if (not (featurep 'bbdb))
    (load-library "bbdb"))
(if (not (featurep 'bbdb-com))
    (load-library "bbdb-com"))

;;{{{      Internal cooking

;; I don't like this but mapcar does not accept a parameter to the function and
;; I don't want to use mapcar*
(defvar eudc-bbdb-current-query nil)
(defvar eudc-bbdb-current-return-attributes nil)

(defvar eudc-bbdb-attributes-translation-alist
  '((name . lastname)
    (email . net)
    (phone . phones))
  "Alist mapping EUDC attribute names to BBDB names.")

(eudc-protocol-set 'eudc-query-function 'eudc-bbdb-query-internal 'bbdb)
(eudc-protocol-set 'eudc-list-attributes-function nil 'bbdb)
(eudc-protocol-set 'eudc-protocol-attributes-translation-alist
		   'eudc-bbdb-attributes-translation-alist 'bbdb)
(eudc-protocol-set 'eudc-bbdb-conversion-alist nil 'bbdb)
(eudc-protocol-set 'eudc-protocol-has-default-query-attributes nil 'bbdb)

(defun eudc-bbdb-format-query (query)
  "Format a EUDC query alist into a list suitable to `bbdb-search'."
  (let* ((firstname (cdr (assq 'firstname query)))
	 (lastname (cdr (assq 'lastname query)))
	 (name (or (and firstname lastname
			(concat firstname " " lastname))
		   firstname
		   lastname))
	(company (cdr (assq 'company query)))
	(net (cdr (assq 'net query)))
	(notes (cdr (assq 'notes query)))
	(phone (cdr (assq 'phone query))))
    (list name company net notes phone)))


(defun eudc-bbdb-filter-non-matching-record (record)
  "Return RECORD if it matches `eudc-bbdb-current-query', nil otherwise."
  (catch 'unmatch
    (progn
      (mapcar
       (function
	(lambda (condition)
	  (let ((attr (car condition))
		(val (cdr condition))
		(case-fold-search t)
		bbdb-val)
	    (or (and (memq attr '(firstname lastname aka company phones addresses net))
		     (progn
		       (setq bbdb-val
			     (eval (list (intern (concat "bbdb-record-"
							 (symbol-name attr)))
					 'record)))
		       (if (listp bbdb-val)
			   (if eudc-bbdb-enable-substring-matches
			       (eval `(or ,@(mapcar '(lambda (subval)
						       (string-match val
								     subval))
						  bbdb-val)))
			     (member (downcase val)
				     (mapcar 'downcase bbdb-val)))
			 (if eudc-bbdb-enable-substring-matches
			     (string-match val bbdb-val)
			   (string-equal (downcase val) (downcase bbdb-val))))))
		(throw 'unmatch nil)))))
       eudc-bbdb-current-query)
      record)))

(defun eudc-bbdb-extract-phones (record)
  (mapcar (function
	   (lambda (phone)
	     (if eudc-bbdb-use-locations-as-attribute-names
		 (cons (intern (bbdb-phone-location phone))
		       (bbdb-phone-string phone))
	       (cons 'phones (format "%s: %s"
				     (bbdb-phone-location phone)
				     (bbdb-phone-string phone))))))
	  (bbdb-record-phones record)))

(defun eudc-bbdb-extract-addresses (record)
  (let (s c val)
    (mapcar (function
	     (lambda (address)
	       (setq val (concat (unless (= 0 (length (setq s (bbdb-address-street1 address))))
				   (concat s "\n"))
				 (unless (= 0 (length (setq s (bbdb-address-street2 address))))
				   (concat s "\n"))
				 (unless (= 0 (length (setq s (bbdb-address-street3 address))))
				   (concat s "\n"))
				 (progn
				   (setq c (bbdb-address-city address))
				   (setq s (bbdb-address-state address))
				   (if (and (> (length c) 0) (> (length s) 0))
				       (concat c ", " s " ")
				     (concat c " ")))
				 (bbdb-address-zip-string address)))
	       (if eudc-bbdb-use-locations-as-attribute-names
		   (cons (intern (bbdb-address-location address)) val)
		 (cons 'addresses (concat (bbdb-address-location address) "\n" val)))))
	    (bbdb-record-addresses record))))

(defun eudc-bbdb-format-record-as-result (record)
  "Format the BBDB RECORD as a EUDC query result record.
The record is filtered according to `eudc-bbdb-current-return-attributes'"
  (let ((attrs (or eudc-bbdb-current-return-attributes
		   '(firstname lastname aka company phones addresses net notes)))
	attr
	eudc-rec
	val)
    (while (prog1
	       (setq attr (car attrs))
	     (setq attrs (cdr attrs)))
      (cond
       ((eq attr 'phones)
	(setq val (eudc-bbdb-extract-phones record)))
       ((eq attr 'addresses)
	(setq val (eudc-bbdb-extract-addresses record)))
       ((memq attr '(firstname lastname aka company net notes))
	(setq val (eval
		   (list (intern
			  (concat "bbdb-record-"
				  (symbol-name attr)))
			 'record))))
       (t
	(setq val "Unknown BBDB attribute")))
      (if val
	(cond
	 ((memq attr '(phones addresses))
	  (setq eudc-rec (append val eudc-rec)))
	 ((and (listp val)
	  (= 1 (length val)))
	  (setq eudc-rec (cons (cons attr (car val)) eudc-rec)))
	 ((> (length val) 0)
	  (setq eudc-rec (cons (cons attr val) eudc-rec)))
	 (t
	  (error "Unexpected attribute value")))))
    (nreverse eudc-rec)))



(defun eudc-bbdb-query-internal (query &optional return-attrs)
  "Query BBDB  with QUERY.
QUERY is a list of cons cells (ATTR . VALUE) where ATTRs should be valid
BBDB attribute names.
RETURN-ATTRS is a list of attributes to return, defaulting to
`eudc-default-return-attributes'."

  (let ((eudc-bbdb-current-query query)
	(eudc-bbdb-current-return-attributes return-attrs)
	(query-attrs (eudc-bbdb-format-query query))
	bbdb-attrs
	(records (bbdb-records))
	result
	filtered)
    ;; BBDB ORs its query attributes while EUDC ANDs them, hence we need to
    ;; call bbdb-search iteratively on the returned records for each of the
    ;; requested attributes
    (while (and records (> (length query-attrs) 0))
      (setq bbdb-attrs (append bbdb-attrs (list (car query-attrs))))
      (if (car query-attrs)
	  (setq records (eval `(bbdb-search ,(quote records) ,@bbdb-attrs))))
      (setq query-attrs (cdr query-attrs)))
    (mapcar (function
	     (lambda (record)
	       (setq filtered (eudc-filter-duplicate-attributes record))
	       ;; If there were duplicate attributes reverse the order of the
	       ;; record so the unique attributes appear first
	       (if (> (length filtered) 1)
		   (setq filtered (mapcar (function
					   (lambda (rec)
					     (reverse rec)))
					  filtered)))
	       (setq result (append result filtered))))
	    (delq nil
		  (mapcar 'eudc-bbdb-format-record-as-result
			  (delq nil
				(mapcar 'eudc-bbdb-filter-non-matching-record
					records)))))
    result))

;;}}}

;;{{{      High-level interfaces (interactive functions)

(defun eudc-bbdb-set-server (dummy)
  "Set the EUDC server to BBDB."
  (interactive)
  (eudc-set-server dummy 'bbdb)
  (message "BBDB server selected"))

;;}}}


(eudc-register-protocol 'bbdb)

(provide 'eudcb-bbdb)

;;; arch-tag: 38276208-75de-4dbc-ba6f-8db684c32e0a
;;; eudcb-bbdb.el ends here
