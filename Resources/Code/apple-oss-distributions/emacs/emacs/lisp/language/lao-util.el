;;; lao-util.el --- utilities for Lao -*- coding: iso-2022-7bit; -*-

;; Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006, 2007
;;   Free Software Foundation, Inc.
;; Copyright (C) 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007
;;   National Institute of Advanced Industrial Science and Technology (AIST)
;;   Registration Number H14PRO021

;; Keywords: multilingual, Lao

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

;; Setting information of Thai characters.

(defconst lao-category-table (make-category-table))
(define-category ?c "Lao consonant" lao-category-table)
(define-category ?s "Lao semi-vowel" lao-category-table)
(define-category ?v "Lao upper/lower vowel" lao-category-table)
(define-category ?t "Lao tone" lao-category-table)

(let ((l '((?(1!(B consonant "LETTER KOR  KAI'" "CHICKEN")
	   (?(1"(B consonant "LETTER KHOR KHAI'" "EGG")
	   (?(1#(B invalid nil)
	   (?(1$(B consonant "LETTER QHOR QHWARGN" "BUFFALO")
	   (?(1%(B invalid nil)
	   (?  invalid nil)
	   (?(1'(B consonant "LETTER NGOR NGUU" "SNAKE")
	   (?(1((B consonant "LETTER JOR JUA" "BUDDHIST NOVICE")
	   (?(1)(B invalid nil)
	   (?(1*(B consonant "LETTER XOR X\"ARNG" "ELEPHANT")
	   (?(1+(B invalid nil)
	   (?(1,(B invalid nil)
	   (?(1-(B consonant "LETTER YOR YUNG" "MOSQUITO")
	   (?(1.(B invalid nil)
	   (?(1.(B invalid nil)
	   (?(1.(B invalid nil)
	   (?(1.(B invalid nil)
	   (?(1.(B invalid nil)
	   (?(1.(B invalid nil)
	   (?(14(B consonant "LETTER DOR DANG" "NOSE")
	   (?(15(B consonant "LETTER TOR TAR" "EYE")
	   (?(16(B consonant "LETTER THOR THUNG" "TO ASK,QUESTION")
	   (?(17(B consonant "LETTER DHOR DHARM" "FLAG")
	   (?(18(B invalid nil)
	   (?(19(B consonant "LETTER NOR NOK" "BIRD")
	   (?(1:(B consonant "LETTER BOR BED" "FISHHOOK")
	   (?(1;(B consonant "LETTER POR PAR" "FISH")
	   (?(1<(B consonant "LETTER HPOR HPER\"" "BEE")
	   (?(1=(B consonant "LETTER FHOR FHAR" "WALL")
	   (?(1>(B consonant "LETTER PHOR PHUU" "MOUNTAIN")
	   (?(1?(B consonant "LETTER FOR FAI" "FIRE")
	   (?(1@(B invalid nil)
	   (?(1A(B consonant "LETTER MOR MAR\"" "HORSE")
	   (?(1B(B consonant "LETTER GNOR GNAR" "MEDICINE")
	   (?(1C(B consonant "LETTER ROR ROD" "CAR")
	   (?(1D(B invalid nil)
	   (?(1E(B consonant "LETTER LOR LIING" "MONKEY")
	   (?(1F(B invalid nil)
	   (?(1G(B consonant "LETTER WOR WII" "HAND FAN")
	   (?(1H(B invalid nil)
	   (?(1I(B invalid nil)
	   (?(1J(B consonant "LETTER SOR SEA" "TIGER")
	   (?(1K(B consonant "LETTER HHOR HHAI" "JAR")
	   (?(1L(B invalid nil)
	   (?(1M(B consonant "LETTER OR OOW" "TAKE")
	   (?(1N(B consonant "LETTER HOR HEA" "BOAT")
	   (?(1O(B special "ELLIPSIS")
	   (?(1P(B vowel-base "VOWEL SIGN SARA A")
	   (?(1Q(B vowel-upper "VOWEL SIGN MAI KAN")
	   (?(1R(B vowel-base "VOWEL SIGN SARA AR")
	   (?(1S(B vowel-base "VOWEL SIGN SARA AM")
	   (?(1T(B vowel-upper "VOWEL SIGN SARA I")
	   (?(1U(B vowel-upper "VOWEL SIGN SARA II")
	   (?(1V(B vowel-upper "VOWEL SIGN SARA EU")
	   (?(1W(B vowel-upper "VOWEL SIGN SARA UR")
	   (?(1X(B vowel-lower "VOWEL SIGN SARA U")
	   (?(1Y(B vowel-lower "VOWEL SIGN SARA UU")
	   (?(1Z(B invalid nil)
	   (?(1[(B vowel-upper "VOWEL SIGN MAI KONG")
	   (?(1\(B semivowel-lower "SEMIVOWEL SIGN LO")
	   (?(1](B vowel-base "SEMIVOWEL SIGN SARA IA")
	   (?(1^(B invalid nil)
	   (?(1_(B invalid nil)
	   (?(1`(B vowel-base "VOWEL SIGN SARA EE")
	   (?(1a(B vowel-base "VOWEL SIGN SARA AA")
	   (?(1b(B vowel-base "VOWEL SIGN SARA OO")
	   (?(1c(B vowel-base "VOWEL SIGN SARA EI MAI MUAN\"")
	   (?(1d(B vowel-base "VOWEL SIGN SARA AI MAI MAY")
	   (?(1e(B invalid nil)
	   (?(1f(B special "KO LA (REPETITION)")
	   (?(1g(B invalid nil)
	   (?(1h(B tone "TONE MAI EK")
	   (?(1i(B tone "TONE MAI THO")
	   (?(1j(B tone "TONE MAI TI")
	   (?(1k(B tone "TONE MAI JADTAWAR")
	   (?(1l(B tone "CANCELLATION MARK")
	   (?(1m(B vowel-upper "VOWEL SIGN SARA OR")
	   (?(1n(B invalid nil)
	   (?(1o(B invalid nil)
	   (?(1p(B special "DIGIT ZERO")
	   (?(1q(B special "DIGIT ONE")
	   (?(1r(B special "DIGIT TWO")
	   (?(1s(B special "DIGIT THREE")
	   (?(1t(B special "DIGIT FOUR")
	   (?(1u(B special "DIGIT FIVE")
	   (?(1v(B special "DIGIT SIX")
	   (?(1w(B special "DIGIT SEVEN")
	   (?(1x(B special "DIGIT EIGHT")
	   (?(1y(B special "DIGIT NINE")
	   (?(1z(B invalid nil)
	   (?(1{(B invalid nil)
	   (?(1|(B consonant "LETTER NHOR NHUU" "MOUSE")
	   (?(1}(B consonant "LETTER MHOR MHAR" "DOG")
	   (?(1~(B invalid nil)
	   ;; Unicode equivalents
	   (?$,1D!(B consonant "LETTER KOR  KAI'" "CHICKEN")
	   (?$,1D"(B consonant "LETTER KHOR KHAI'" "EGG")
	   (?$,1D$(B consonant "LETTER QHOR QHWARGN" "BUFFALO")
	   (?$,1D'(B consonant "LETTER NGOR NGUU" "SNAKE")
	   (?$,1D((B consonant "LETTER JOR JUA" "BUDDHIST NOVICE")
	   (?$,1D*(B consonant "LETTER XOR X\"ARNG" "ELEPHANT")
	   (?$,1D-(B consonant "LETTER YOR YUNG" "MOSQUITO")
	   (?$,1D4(B consonant "LETTER DOR DANG" "NOSE")
	   (?$,1D5(B consonant "LETTER TOR TAR" "EYE")
	   (?$,1D6(B consonant "LETTER THOR THUNG" "TO ASK,QUESTION")
	   (?$,1D7(B consonant "LETTER DHOR DHARM" "FLAG")
	   (?$,1D9(B consonant "LETTER NOR NOK" "BIRD")
	   (?$,1D:(B consonant "LETTER BOR BED" "FISHHOOK")
	   (?$,1D;(B consonant "LETTER POR PAR" "FISH")
	   (?$,1D<(B consonant "LETTER HPOR HPER\"" "BEE")
	   (?$,1D=(B consonant "LETTER FHOR FHAR" "WALL")
	   (?$,1D>(B consonant "LETTER PHOR PHUU" "MOUNTAIN")
	   (?$,1D?(B consonant "LETTER FOR FAI" "FIRE")
	   (?$,1DA(B consonant "LETTER MOR MAR\"" "HORSE")
	   (?$,1DB(B consonant "LETTER GNOR GNAR" "MEDICINE")
	   (?$,1DC(B consonant "LETTER ROR ROD" "CAR")
	   (?$,1DE(B consonant "LETTER LOR LIING" "MONKEY")
	   (?$,1DG(B consonant "LETTER WOR WII" "HAND FAN")
	   (?$,1DJ(B consonant "LETTER SOR SEA" "TIGER")
	   (?$,1DK(B consonant "LETTER HHOR HHAI" "JAR")
	   (?$,1DM(B consonant "LETTER OR OOW" "TAKE")
	   (?$,1DN(B consonant "LETTER HOR HEA" "BOAT")
	   (?$,1DO(B special "ELLIPSIS")
	   (?$,1DP(B vowel-base "VOWEL SIGN SARA A")
	   (?$,1DQ(B vowel-upper "VOWEL SIGN MAI KAN")
	   (?$,1DR(B vowel-base "VOWEL SIGN SARA AR")
	   (?$,1DS(B vowel-base "VOWEL SIGN SARA AM")
	   (?$,1DT(B vowel-upper "VOWEL SIGN SARA I")
	   (?$,1DU(B vowel-upper "VOWEL SIGN SARA II")
	   (?$,1DV(B vowel-upper "VOWEL SIGN SARA EU")
	   (?$,1DW(B vowel-upper "VOWEL SIGN SARA UR")
	   (?$,1DX(B vowel-lower "VOWEL SIGN SARA U")
	   (?$,1DY(B vowel-lower "VOWEL SIGN SARA UU")
	   (?$,1D[(B vowel-upper "VOWEL SIGN MAI KONG")
	   (?$,1D\(B semivowel-lower "SEMIVOWEL SIGN LO")
	   (?$,1D](B vowel-base "SEMIVOWEL SIGN SARA IA")
	   (?$,1D`(B vowel-base "VOWEL SIGN SARA EE")
	   (?$,1Da(B vowel-base "VOWEL SIGN SARA AA")
	   (?$,1Db(B vowel-base "VOWEL SIGN SARA OO")
	   (?$,1Dc(B vowel-base "VOWEL SIGN SARA EI MAI MUAN\"")
	   (?$,1Dd(B vowel-base "VOWEL SIGN SARA AI MAI MAY")
	   (?$,1Df(B special "KO LA (REPETITION)")
	   (?$,1Dh(B tone "TONE MAI EK")
	   (?$,1Di(B tone "TONE MAI THO")
	   (?$,1Dj(B tone "TONE MAI TI")
	   (?$,1Dk(B tone "TONE MAI JADTAWAR")
	   (?$,1Dl(B tone "CANCELLATION MARK")
	   (?$,1Dm(B vowel-upper "VOWEL SIGN SARA OR")
	   (?$,1Dp(B special "DIGIT ZERO")
	   (?$,1Dq(B special "DIGIT ONE")
	   (?$,1Dr(B special "DIGIT TWO")
	   (?$,1Ds(B special "DIGIT THREE")
	   (?$,1Dt(B special "DIGIT FOUR")
	   (?$,1Du(B special "DIGIT FIVE")
	   (?$,1Dv(B special "DIGIT SIX")
	   (?$,1Dw(B special "DIGIT SEVEN")
	   (?$,1Dx(B special "DIGIT EIGHT")
	   (?$,1Dy(B special "DIGIT NINE")
	   (?$,1D|(B consonant "LETTER NHOR NHUU" "MOUSE")
	   (?$,1D}(B consonant "LETTER MHOR MHAR" "DOG")))
      elm)
  (while l
    (setq elm (car l) l (cdr l))
    (let ((char (car elm))
	  (ptype (nth 1 elm)))
      (cond ((eq ptype 'consonant)
	     (modify-category-entry char ?c lao-category-table))
	    ((memq ptype '(vowel-upper vowel-lower))
	     (modify-category-entry char ?v lao-category-table))
	    ((eq ptype 'semivowel-lower)
	     (modify-category-entry char ?s lao-category-table))
	    ((eq ptype 'tone)
	     (modify-category-entry char ?t lao-category-table)))
      (put-char-code-property char 'phonetic-type ptype)
      (put-char-code-property char 'name (nth 2 elm))
      (put-char-code-property char 'meaning (nth 3 elm)))))

;; The general composing rules are as follows:
;;
;;                          T
;;       V        T         V                  T
;; CV -> C, CT -> C, CVT -> C, Cv -> C, CvT -> C
;;                                   v         v
;;                             T
;;        V         T          V                   T
;; CsV -> C, CsT -> C, CsVT -> C, Csv -> C, CvT -> C
;;        s         s          s         s         s
;;                                       v         v


;; where C: consonant, V: vowel upper, v: vowel lower,
;;       T: tone mark, s: semivowel lower

(defvar lao-composition-pattern
  "\\cc\\(\\ct\\|\\cv\\ct?\\|\\cs\\(\\ct\\|\\cv\\ct?\\)?\\)"
  "Regular expression matching a Lao composite sequence.")

;;;###autoload
(defun lao-compose-string (str)
  (with-category-table lao-category-table
   (let ((idx 0))
     (while (setq idx (string-match lao-composition-pattern str idx))
       (compose-string str idx (match-end 0))
       (setq idx (match-end 0))))
   str))

;;; LRT: Lao <-> Roman Transcription

;; Upper vowels and tone-marks are put on the letter.
;; Semi-vowel-sign-lo and lower vowels are put under the letter.

(defconst lao-transcription-consonant-alist
  (sort '(;; single consonants
	  ("k" . "(1!(B")
	  ("kh" . "(1"(B")
	  ("qh" . "(1$(B")
	  ("ng" . "(1'(B")
	  ("j" . "(1((B")
	  ("s" . "(1J(B")
	  ("x" . "(1*(B")
	  ("y" . "(1-(B")
	  ("d" . "(14(B")
	  ("t" . "(15(B")
	  ("th" . "(16(B")
	  ("dh" . "(17(B")
	  ("n" . "(19(B")
	  ("b" . "(1:(B")
	  ("p" . "(1;(B")
	  ("hp" . "(1<(B")
	  ("fh" . "(1=(B")
	  ("ph" . "(1>(B")
	  ("f" . "(1?(B")
	  ("m" . "(1A(B")
	  ("gn" . "(1B(B")
	  ("l" . "(1E(B")
	  ("r" . "(1C(B")
	  ("v" . "(1G(B")
	  ("w" . "(1G(B")
	  ("hh" . "(1K(B")
	  ("O" . "(1M(B")
	  ("h" . "(1N(B")
	  ("nh" . "(1|(B")
	  ("mh" . "(1}(B")
	  ("lh" . ["(1K\(B"])
	  ;; double consonants
	  ("ngh" . ["(1K'(B"])
	  ("yh" . ["(1K](B"])
	  ("wh" . ["(1KG(B"])
	  ("hl" . ["(1KE(B"])
	  ("hy" . ["(1K-(B"])
	  ("hn" . ["(1K9(B"])
	  ("hm" . ["(1KA(B"])
	  )
	(function (lambda (x y) (> (length (car x)) (length (car y)))))))

(defconst lao-transcription-semi-vowel-alist
  '(("r" . "(1\(B")))

(defconst lao-transcription-vowel-alist
  (sort '(("a" . "(1P(B")
	  ("ar" . "(1R(B")
	  ("i" . "(1T(B")
	  ("ii" . "(1U(B")
	  ("eu" . "(1V(B")
	  ("ur" . "(1W(B")
	  ("u" . "(1X(B")
	  ("uu" . "(1Y(B")
	  ("e" . ["(1`P(B"])
	  ("ee" . "(1`(B")
	  ("ae" . ["(1aP(B"])
	  ("aa" . "(1a(B")
	  ("o" . ["(1bP(B"])
	  ("oo" . "(1b(B")
	  ("oe" . ["(1`RP(B"])
	  ("or" . "(1m(B")
	  ("er" . ["(1`T(B"])
	  ("ir" . ["(1`U(B"])
	  ("ua" . ["(1[GP(B"])
	  ("uaa" . ["(1[G(B"])
	  ("ie" . ["(1`Q]P(B"])
	  ("ia" . ["(1`Q](B"])
	  ("ea" . ["(1`VM(B"])
	  ("eaa" . ["(1`WM(B"])
	  ("ai" . "(1d(B")
	  ("ei" . "(1c(B")
	  ("ao" . ["(1`[R(B"])
	  ("aM" . "(1S(B"))
	(function (lambda (x y) (> (length (car x)) (length (car y)))))))

;; Maa-sakod is put at the tail.
(defconst lao-transcription-maa-sakod-alist
  '(("k" . "(1!(B")
    ("g" . "(1'(B")
    ("y" . "(1-(B")
    ("d" . "(14(B")
    ("n" . "(19(B")
    ("b" . "(1:(B")
    ("m" . "(1A(B")
    ("v" . "(1G(B")
    ("w" . "(1G(B")
    ))

(defconst lao-transcription-tone-alist
  '(("'" . "(1h(B")
    ("\"" . "(1i(B")
    ("^" . "(1j(B")
    ("+" . "(1k(B")
    ("~" . "(1l(B")))

(defconst lao-transcription-punctuation-alist
  '(("\\0" . "(1p(B")
    ("\\1" . "(1q(B")
    ("\\2" . "(1r(B")
    ("\\3" . "(1s(B")
    ("\\4" . "(1t(B")
    ("\\5" . "(1u(B")
    ("\\6" . "(1v(B")
    ("\\7" . "(1w(B")
    ("\\8" . "(1x(B")
    ("\\9" . "(1y(B")
    ("\\\\" . "(1f(B")
    ("\\$" . "(1O(B")))

(defconst lao-transcription-pattern
  (concat
   "\\("
   (mapconcat 'car lao-transcription-consonant-alist "\\|")
   "\\)\\("
   (mapconcat 'car lao-transcription-semi-vowel-alist "\\|")
   "\\)?\\(\\("
   (mapconcat 'car lao-transcription-vowel-alist "\\|")
   "\\)\\("
   (mapconcat 'car lao-transcription-maa-sakod-alist "\\|")
   "\\)?\\("
   (mapconcat (lambda (x) (regexp-quote (car x)))
	      lao-transcription-tone-alist "\\|")
   "\\)?\\)?\\|"
   (mapconcat (lambda (x) (regexp-quote (car x)))
	      lao-transcription-punctuation-alist "\\|")
   )
  "Regexp of Roman transcription pattern for one Lao syllable.")

(defconst lao-transcription-pattern
  (concat
   "\\("
   (regexp-opt (mapcar 'car lao-transcription-consonant-alist))
   "\\)\\("
   (regexp-opt (mapcar 'car lao-transcription-semi-vowel-alist))
   "\\)?\\(\\("
   (regexp-opt (mapcar 'car lao-transcription-vowel-alist))
   "\\)\\("
   (regexp-opt (mapcar 'car lao-transcription-maa-sakod-alist))
   "\\)?\\("
   (regexp-opt (mapcar 'car lao-transcription-tone-alist))
   "\\)?\\)?\\|"
   (regexp-opt (mapcar 'car lao-transcription-punctuation-alist))
   )
  "Regexp of Roman transcription pattern for one Lao syllable.")

(defconst lao-vowel-reordering-rule
  '(("(1P(B" (0 ?(1P(B) (0 ?(1Q(B))
    ("(1R(B" (0 ?(1R(B))
    ("(1T(B" (0 ?(1U(B))
    ("(1U(B" (0 ?(1U(B))
    ("(1V(B" (0 ?(1V(B))
    ("(1W(B" (0 ?(1W(B))
    ("(1X(B" (0 ?(1X(B))
    ("(1Y(B" (0 ?(1Y(B))
    ("(1`P(B" (?(1`(B 0 ?(1P(B) (?(1`(B 0 ?(1Q(B))
    ("(1`(B" (?(1`(B 0))
    ("(1aP(B" (?(1a(B 0 ?(1P(B) (?(1a(B 0 ?(1Q(B))
    ("(1a(B" (?(1a(B 0))
    ("(1bP(B" (?(1b(B 0 ?(1P(B) (0 ?(1[(B) (?(1-(B ?(1b(B 0 ?(1Q(B) (?(1G(B ?(1b(B 0 ?(1Q(B))
    ("(1b(B" (?(1b(B 0))
    ("(1`RP(B" (?(1`(B 0 ?(1R(B ?(1P(B) (0 ?(1Q(B ?(1M(B))
    ("(1m(B" (0 ?(1m(B) (0 ?(1M(B))
    ("(1`T(B" (?(1`(B 0 ?(1T(B))
    ("(1`U(B" (?(1`(B 0 ?(1U(B))
    ("(1[GP(B" (0 ?(1[(B ?(1G(B ?(1P(B) (0 ?(1Q(B ?(1G(B))
    ("(1[G(B" (0 ?(1[(B ?(1G(B) (0 ?(1G(B))
    ("(1`Q]P(B" (?(1`(B 0 ?(1Q(B ?(1](B ?(1P(B) (0 ?(1Q(B ?(1](B))
    ("(1`Q](B" (?(1`(B 0 ?(1Q(B ?(1](B) (0 ?(1](B))
    ("(1`VM(B" (?(1`(B 0 ?(1V(B ?(1M(B))
    ("(1`WM(B" (?(1`(B 0 ?(1W(B ?(1M(B))
    ("(1d(B" (?(1d(B 0))
    ("(1c(B" (?(1c(B 0))
    ("(1`[R(B" (?(1`(B 0 ?(1[(B ?(1R(B))
    ("(1S(B" (0 ?(1S(B))

    ;; Unicode equivalents
    ("$,1DP(B" (0 ?$,1DP(B) (0 ?$,1DQ(B))
    ("$,1DR(B" (0 ?$,1DR(B))
    ("$,1DT(B" (0 ?$,1DU(B))
    ("$,1DU(B" (0 ?$,1DU(B))
    ("$,1DV(B" (0 ?$,1DV(B))
    ("$,1DW(B" (0 ?$,1DW(B))
    ("$,1DX(B" (0 ?$,1DX(B))
    ("$,1DY(B" (0 ?$,1DY(B))
    ("$,1D`DP(B" (?$,1D`(B 0 ?$,1DP(B) (?$,1D`(B 0 ?$,1DQ(B))
    ("$,1D`(B" (?$,1D`(B 0))
    ("$,1DaDP(B" (?$,1Da(B 0 ?$,1DP(B) (?$,1Da(B 0 ?$,1DQ(B))
    ("$,1Da(B" (?$,1Da(B 0))
    ("$,1DbDP(B" (?$,1Db(B 0 ?$,1DP(B) (0 ?$,1D[(B) (?$,1D-(B ?$,1Db(B 0 ?$,1DQ(B) (?$,1DG(B ?$,1Db(B 0 ?$,1DQ(B))
    ("$,1Db(B" (?$,1Db(B 0))
    ("$,1D`DRDP(B" (?$,1D`(B 0 ?$,1DR(B ?$,1DP(B) (0 ?$,1DQ(B ?$,1DM(B))
    ("$,1Dm(B" (0 ?$,1Dm(B) (0 ?$,1DM(B))
    ("$,1D`DT(B" (?$,1D`(B 0 ?$,1DT(B))
    ("$,1D`DU(B" (?$,1D`(B 0 ?$,1DU(B))
    ("$,1D[DGDP(B" (0 ?$,1D[(B ?$,1DG(B ?$,1DP(B) (0 ?$,1DQ(B ?$,1DG(B))
    ("$,1D[DG(B" (0 ?$,1D[(B ?$,1DG(B) (0 ?$,1DG(B))
    ("$,1D`DQD]DP(B" (?$,1D`(B 0 ?$,1DQ(B ?$,1D](B ?$,1DP(B) (0 ?$,1DQ(B ?$,1D](B))
    ("$,1D`DQD](B" (?$,1D`(B 0 ?$,1DQ(B ?$,1D](B) (0 ?$,1D](B))
    ("$,1D`DVDM(B" (?$,1D`(B 0 ?$,1DV(B ?$,1DM(B))
    ("$,1D`DWDM(B" (?$,1D`(B 0 ?$,1DW(B ?$,1DM(B))
    ("$,1Dd(B" (?$,1Dd(B 0))
    ("$,1Dc(B" (?$,1Dc(B 0))
    ("$,1D`D[DR(B" (?$,1D`(B 0 ?$,1D[(B ?$,1DR(B))
    ("$,1DS(B" (0 ?$,1DS(B)))
  "Alist of Lao vowel string vs the corresponding re-ordering rule.
Each element has this form:
	(VOWEL NO-MAA-SAKOD-RULE WITH-MAA-SAKOD-RULE (MAA-SAKOD-0 RULE-0) ...)

VOWEL is a vowel string (e.g. \"(1`Q]P(B\").

NO-MAA-SAKOD-RULE is a rule to re-order and modify VOWEL following a
consonant.  It is a list vowel characters or 0.  The element 0
indicate the place to embed a consonant.

Optional WITH-MAA-SAKOD-RULE is a rule to re-order and modify VOWEL
follwoing a consonant and preceding a maa-sakod character.  If it is
nil, NO-MAA-SAKOD-RULE is used.  The maa-sakod character is alwasy
appended at the tail.

For instance, rule `(\"(1`WM(B\" (?(1`(B t ?(1W(B ?(1M(B))' tells that this vowel
string following a consonant `(1!(B' should be re-ordered as \"(1`!WM(B\".

Optional (MAA-SAKOD-n RULE-n) are rules specially applied to maa-sakod
character MAA-SAKOD-n.")

;;;###autoload
(defun lao-transcribe-single-roman-syllable-to-lao (from to &optional str)
  "Transcribe a Romanized Lao syllable in the region FROM and TO to Lao string.
Only the first syllable is transcribed.
The value has the form: (START END LAO-STRING), where
START and END are the beggining and end positions of the Roman Lao syllable,
LAO-STRING is the Lao character transcription of it.

Optional 3rd arg STR, if non-nil, is a string to search for Roman Lao
syllable.  In that case, FROM and TO are indexes to STR."
  (if str
      (if (setq from (string-match lao-transcription-pattern str from))
	  (progn
	    (if (>= from to)
		(setq from nil)
	      (setq to (match-end 0)))))
    (save-excursion
      (goto-char from)
      (if (setq to (re-search-forward lao-transcription-pattern to t))
	  (setq from (match-beginning 0))
	(setq from nil))))
  (if from
      (let* ((consonant (match-string 1 str))
	     (semivowel (match-string 3 str))
	     (vowel (match-string 5 str))
	     (maa-sakod (match-string 8 str))
	     (tone (match-string 9 str))
	     lao-consonant lao-semivowel lao-vowel lao-maa-sakod lao-tone
	     clen cidx)
	(setq to (match-end 0))
	(if (not consonant)
	    (setq str (cdr (assoc (match-string 0 str)
				  lao-transcription-punctuation-alist)))
	  (setq lao-consonant
		(cdr (assoc consonant lao-transcription-consonant-alist)))
	  (if (vectorp lao-consonant)
	      (setq lao-consonant (aref lao-consonant 0)))
	  (setq clen (length lao-consonant))
	  (if semivowel
	      ;; Include semivowel in STR.
	      (setq lao-semivowel
		    (cdr (assoc semivowel lao-transcription-semi-vowel-alist))
		    str (if (= clen 1)
			    (concat lao-consonant lao-semivowel)
			  (concat (substring lao-consonant 0 1) lao-semivowel
				  (substring lao-consonant 1))))
	    (setq str lao-consonant))
	  (if vowel
	      (let (rule)
		(setq lao-vowel
		      (cdr (assoc vowel lao-transcription-vowel-alist)))
		(if (vectorp lao-vowel)
		    (setq lao-vowel (aref lao-vowel 0)))
		(setq rule (assoc lao-vowel lao-vowel-reordering-rule))
		(if (null maa-sakod)
		    (setq rule (nth 1 rule))
		  (setq lao-maa-sakod
			(cdr (assoc maa-sakod lao-transcription-maa-sakod-alist))
			rule
			(or (cdr (assq (aref lao-maa-sakod 0) (nthcdr 2 rule)))
			    (nth 2 rule)
			    (nth 1 rule))))
		(or rule
		    (error "Lao vowel %S has no re-ordering rule" lao-vowel))
		(setq lao-consonant str str "")
		(while rule
		  (if (= (car rule) 0)
		      (setq str (concat str lao-consonant)
			    cidx (length str))
		    (setq str (concat str (list (car rule)))))
		  (setq rule (cdr rule)))
		(or cidx
		    (error "Lao vowel %S has malformed re-ordering rule" vowel))
		;; Set CIDX to after upper or lower vowel if any.
		(let ((len (length str)))
		  (while (and (< cidx len)
			      (memq (get-char-code-property (aref str cidx)
							    'phonetic-type)
				    '(vowel-lower vowel-upper)))
		    (setq cidx (1+ cidx))))
		(if lao-maa-sakod
		    (setq str (concat str lao-maa-sakod)))
		(if tone
		    (setq lao-tone
			  (cdr (assoc tone lao-transcription-tone-alist))
			  str (concat (substring str 0 cidx) lao-tone
				      (substring str cidx)))))))
	(list from to (lao-compose-string str)))))

;;;###autoload
(defun lao-transcribe-roman-to-lao-string (str)
  "Transcribe Romanized Lao string STR to Lao character string."
  (let ((from 0)
	(to (length str))
	(lao-str "")
	val)
    (while (setq val (lao-transcribe-single-roman-syllable-to-lao from to str))
      (let ((start (car val))
	    (end (nth 1 val))
	    (lao (nth 2 val)))
	(if (> start from)
	    (setq lao-str (concat lao-str (substring str from start) lao))
	  (setq lao-str (concat lao-str lao)))
	(setq from end)))
    (if (< from to)
	(concat lao-str (substring str from to))
      lao-str)))

;;;###autoload
(defun lao-post-read-conversion (len)
  (lao-compose-region (point) (+ (point) len))
  len)

;;;###autoload
(defun lao-composition-function (from to pattern &optional string)
  "Compose Lao text in the region FROM and TO.
The text matches the regular expression PATTERN.
Optional 4th argument STRING, if non-nil, is a string containing text
to compose.

The return value is number of composed characters."
  (if (< (1+ from) to)
      (progn
	(if string
	    (compose-string string from to)
	  (compose-region from to))
	(- to from))))

;;;###autoload
(defun lao-compose-region (from to)
  (interactive "r")
  (save-restriction
    (narrow-to-region from to)
    (goto-char (point-min))
    (with-category-table lao-category-table
      (while (re-search-forward lao-composition-pattern nil t)
	(compose-region (match-beginning 0) (point))))))

;;
(provide 'lao-util)

;;; arch-tag: 1f828781-3cb8-4695-88af-8f33222338ce
;;; lao-util.el ends here
