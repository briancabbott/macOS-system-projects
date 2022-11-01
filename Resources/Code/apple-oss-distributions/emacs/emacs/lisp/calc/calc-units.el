;;; calc-units.el --- unit conversion functions for Calc

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

;; This file is autoloaded from calc-ext.el.

(require 'calc-ext)
(require 'calc-macs)
(eval-when-compile
  (require 'calc-alg))

;;; Units operations.

;;; Units table last updated 9-Jan-91 by Ulrich Mueller (ulm@vsnhd1.cern.ch)
;;; with some additions by Przemek Klosowski (przemek@rrdstrad.nist.gov)
;;; Updated April 2002 by Jochen K�pper

;;; for CODATA 1998 see one of
;;; - Journal of Physical and Chemical Reference Data, 28(6), 1713-1852, 1999.
;;; - Reviews of Modern Physics, 72(2), 351-495, 2000.
;;; for CODATA 2005 see
;;; - http://physics.nist.gov/cuu/Constants/index.html

(defvar math-standard-units
  '( ;; Length
    ( m       nil                    "*Meter" )
    ( in      "2.54 cm"           "Inch" )
    ( ft      "12 in"                "Foot" )
    ( yd      "3 ft"                 "Yard" )
    ( mi      "5280 ft"              "Mile" )
    ( au      "149597870691 m"       "Astronomical Unit" ) ;; NASA JPL (http://neo.jpl.nasa.gov/glossary/au.html)
    ( lyr     "9460536207068016 m"   "Light Year" )
    ( pc      "206264.80625 au"      "Parsec" )
    ( nmi     "1852 m"               "Nautical Mile" )
    ( fath    "6 ft"                 "Fathom" )
    ( mu      "1 um"                 "Micron" )
    ( mil     "in/1000"              "Mil" )
    ( point   "in/72"                "Point (1/72 inch)" )
    ( Ang     "1e-10 m"              "Angstrom" )
    ( mfi     "mi+ft+in"             "Miles + feet + inches" )
    ;; TeX lengths
    ( texpt   "in/72.27"             "Point (TeX conventions)" )
    ( texpc   "12 texpt"             "Pica" )
    ( texbp   "point"                "Big point (TeX conventions)" )
    ( texdd   "1238/1157 texpt"      "Didot point" )
    ( texcc   "12 texdd"             "Cicero" )
    ( texsp   "1/66536 texpt"        "Scaled TeX point" )

    ;; Area
    ( hect    "10000 m^2"            "*Hectare" )
    ( a       "100 m^2"              "Are")
    ( acre    "mi^2 / 640"           "Acre" )
    ( b       "1e-28 m^2"            "Barn" )

    ;; Volume
    ( L       "1e-3 m^3"             "*Liter" )
    ( l       "L"                    "Liter" )
    ( gal     "4 qt"                 "US Gallon" )
    ( qt      "2 pt"                 "Quart" )
    ( pt      "2 cup"                "Pint" )
    ( cup     "8 ozfl"               "Cup" )
    ( ozfl    "2 tbsp"               "Fluid Ounce" )
    ( floz    "2 tbsp"               "Fluid Ounce" )
    ( tbsp    "3 tsp"                "Tablespoon" )
    ( tsp     "4.92892159375 ml"     "Teaspoon" )
    ( vol     "tsp+tbsp+ozfl+cup+pt+qt+gal" "Gallons + ... + teaspoons" )
    ( galC    "4.54609 L"            "Canadian Gallon" )
    ( galUK   "4.546092 L"           "UK Gallon" )

    ;; Time
    ( s       nil                    "*Second" )
    ( sec     "s"                    "Second" )
    ( min     "60 s"                 "Minute" )
    ( hr      "60 min"               "Hour" )
    ( day     "24 hr"                "Day" )
    ( wk      "7 day"                "Week" )
    ( hms     "wk+day+hr+min+s"      "Hours, minutes, seconds" )
    ( yr      "365.25 day"           "Year" )
    ( Hz      "1/s"                  "Hertz" )

    ;; Speed
    ( mph     "mi/hr"                "*Miles per hour" )
    ( kph     "km/hr"                "Kilometers per hour" )
    ( knot    "nmi/hr"               "Knot" )
    ( c       "299792458 m/s"        "Speed of light" ) ;;; CODATA 2005

    ;; Acceleration
    ( ga      "9.80665 m/s^2"        "*\"g\" acceleration" ) ;; CODATA 2005

    ;; Mass
    ( g       nil                    "*Gram" )
    ( lb      "16 oz"                "Pound (mass)" )
    ( oz      "28.349523125 g"       "Ounce (mass)" )
    ( ton     "2000 lb"              "Ton" )
    ( tpo     "ton+lb+oz"            "Tons + pounds + ounces (mass)" )
    ( t       "1000 kg"              "Metric ton" )
    ( tonUK   "1016.0469088 kg"      "UK ton" )
    ( lbt     "12 ozt"               "Troy pound" )
    ( ozt     "31.103475 g"          "Troy ounce" )
    ( ct      ".2 g"                 "Carat" )
    ( u       "1.66053886e-27 kg"    "Unified atomic mass" ) ;; CODATA 2005

    ;; Force
    ( N       "m kg/s^2"             "*Newton" )
    ( dyn     "1e-5 N"               "Dyne" )
    ( gf      "ga g"                 "Gram (force)" )
    ( lbf     "4.44822161526 N"      "Pound (force)" )
    ( kip     "1000 lbf"             "Kilopound (force)" )
    ( pdl     "0.138255 N"           "Poundal" )

    ;; Energy
    ( J       "N m"                  "*Joule" )
    ( erg     "1e-7 J"               "Erg" )
    ( cal     "4.1868 J"             "International Table Calorie" )
    ( Btu     "1055.05585262 J"      "International Table Btu" )
    ( eV      "ech V"                "Electron volt" )
    ( ev      "eV"                   "Electron volt" )
    ( therm   "105506000 J"          "EEC therm" )
    ( invcm   "h c/cm"               "Energy in inverse centimeters" )
    ( Kayser  "invcm"                "Kayser (inverse centimeter energy)" )
    ( men     "100/invcm"            "Inverse energy in meters" )
    ( Hzen    "h Hz"                 "Energy in Hertz")
    ( Ken     "k K"                  "Energy in Kelvins")
    ( Wh      "W hr"                 "Watt hour")
    ( Ws      "W s"                  "Watt second")

    ;; Power
    ( W       "J/s"                  "*Watt" )
    ( hp      "745.7 W"              "Horsepower" )

    ;; Temperature
    ( K       nil                    "*Degree Kelvin"     K )
    ( dK      "K"                    "Degree Kelvin"      K )
    ( degK    "K"                    "Degree Kelvin"      K )
    ( dC      "K"                    "Degree Celsius"     C )
    ( degC    "K"                    "Degree Celsius"     C )
    ( dF      "(5/9) K"              "Degree Fahrenheit"  F )
    ( degF    "(5/9) K"              "Degree Fahrenheit"  F )

    ;; Pressure
    ( Pa      "N/m^2"                "*Pascal" )
    ( bar     "1e5 Pa"               "Bar" )
    ( atm     "101325 Pa"            "Standard atmosphere" ) ;; CODATA 2005
    ( Torr    " 1.333224e2 Pa"       "Torr" ) ;; NIST (http://physics.nist.gov/Pubs/SP811/appenB9.html)
    ( mHg     "1000 Torr"            "Meter of mercury" )
    ( inHg    "25.4 mmHg"            "Inch of mercury" )
    ( inH2O   "2.490889e2 Pa"        "Inch of water" ) ;; NIST (http://physics.nist.gov/Pubs/SP811/appenB9.html)
    ( psi     "6894.75729317 Pa"     "Pound per square inch" )

    ;; Viscosity
    ( P       "0.1 Pa s"              "*Poise" )
    ( St      "1e-4 m^2/s"            "Stokes" )

    ;; Electromagnetism
    ( A       nil                     "*Ampere" )
    ( C       "A s"                   "Coulomb" )
    ( Fdy     "ech Nav"               "Faraday" )
    ( e       "1.60217653e-19 C"      "Elementary charge" ) ;; CODATA 2005
    ( ech     "1.60217653e-19 C"      "Elementary charge" ) ;; CODATA 2005
    ( V       "W/A"                   "Volt" )
    ( ohm     "V/A"                   "Ohm" )
    ( mho     "A/V"                   "Mho" )
    ( S       "A/V"                   "Siemens" )
    ( F       "C/V"                   "Farad" )
    ( H       "Wb/A"                  "Henry" )
    ( T       "Wb/m^2"                "Tesla" )
    ( Gs      "1e-4 T"                "Gauss" )
    ( Wb      "V s"                   "Weber" )

    ;; Luminous intensity
    ( cd      nil                     "*Candela" )
    ( sb      "1e4 cd/m^2"            "Stilb" )
    ( lm      "cd sr"                 "Lumen" )
    ( lx      "lm/m^2"                "Lux" )
    ( ph      "1e4 lx"                "Phot" )
    ( fc      "10.76391 lx"           "Footcandle" ) ;; NIST (http://physics.nist.gov/Pubs/SP811/appenB9.html)
    ( lam     "1e4 lm/m^2"            "Lambert" )
    ( flam    "3.426259 cd/m^2"       "Footlambert" ) ;; NIST (http://physics.nist.gov/Pubs/SP811/appenB9.html)

    ;; Radioactivity
    ( Bq      "1/s"                    "*Becquerel" )
    ( Ci      "3.7e10 Bq"              "Curie" )
    ( Gy      "J/kg"                   "Gray" )
    ( Sv      "Gy"                     "Sievert" )
    ( R       "2.58e-4 C/kg"           "Roentgen" )
    ( rd      ".01 Gy"                 "Rad" )
    ( rem     "rd"                     "Rem" )

    ;; Amount of substance
    ( mol     nil                      "*Mole" )

    ;; Plane angle
    ( rad     nil                      "*Radian" )
    ( circ    "2 pi rad"               "Full circle" )
    ( rev     "circ"                   "Full revolution" )
    ( deg     "circ/360"               "Degree" )
    ( arcmin  "deg/60"                 "Arc minute" )
    ( arcsec  "arcmin/60"              "Arc second" )
    ( grad    "circ/400"               "Grade" )
    ( rpm     "rev/min"                "Revolutions per minute" )

    ;; Solid angle
    ( sr      nil                      "*Steradian" )

    ;; Other physical quantities
    ( h       "6.6260693e-34 J s"     "*Planck's constant" ) ;; CODATA 2005
    ( hbar    "h / 2 pi"               "Planck's constant" )
    ( mu0     "4 pi 1e-7 H/m"          "Permeability of vacuum" )
    ( G       "6.6742e-11 m^3/kg^1/s^2" "Gravitational constant" ) ;; CODATA 2005
    ( Nav     "6.02214115e23 / mol"    "Avagadro's constant" ) ;; CODATA 2005
    ( me      "9.1093826e-31 kg"       "Electron rest mass" ) ;; CODATA 2005
    ( mp      "1.67262171e-27 kg"      "Proton rest mass" ) ;; CODATA 2005
    ( mn      "1.67492728e-27 kg"      "Neutron rest mass" ) ;; CODATA 2005
    ( mmu     "1.88353140e-28 kg"      "Muon rest mass" ) ;; CODATA 2005
    ( Ryd     "10973731.568525 /m"     "Rydberg's constant" ) ;; CODATA 2005
    ( k       "1.3806505e-23 J/K"      "Boltzmann's constant" ) ;; CODATA 2005
    ( alpha   "7.297352568e-3"         "Fine structure constant" ) ;; CODATA 2005
    ( muB     "927.400949e-26 J/T"     "Bohr magneton" ) ;; CODATA 2005
    ( muN     "5.05078343e-27 J/T"     "Nuclear magneton" ) ;; CODATA 2005
    ( mue     "-928.476412e-26 J/T"    "Electron magnetic moment" ) ;; CODATA 2005
    ( mup     "1.41060671e-26 J/T"     "Proton magnetic moment" ) ;; CODATA 2005
    ( R0      "8.314472 J/mol/K"       "Molar gas constant" ) ;; CODATA 2005
    ( V0      "22.710981e-3 m^3/mol"   "Standard volume of ideal gas" )))


(defvar math-additional-units nil
  "*Additional units table for user-defined units.
Must be formatted like math-standard-units.
If this is changed, be sure to set math-units-table to nil to ensure
that the combined units table will be rebuilt.")

(defvar math-unit-prefixes
  '( ( ?Y  (float 1 24)  "Yotta"  )
     ( ?Z  (float 1 21)  "Zetta"  )
     ( ?E  (float 1 18)  "Exa"    )
     ( ?P  (float 1 15)  "Peta"   )
     ( ?T  (float 1 12)  "Tera"	  )
     ( ?G  (float 1 9)   "Giga"	  )
     ( ?M  (float 1 6)   "Mega"	  )
     ( ?k  (float 1 3)   "Kilo"	  )
     ( ?K  (float 1 3)   "Kilo"	  )
     ( ?h  (float 1 2)   "Hecto"  )
     ( ?H  (float 1 2)   "Hecto"  )
     ( ?D  (float 1 1)   "Deka"	  )
     ( 0   (float 1 0)   nil      )
     ( ?d  (float 1 -1)  "Deci"	  )
     ( ?c  (float 1 -2)  "Centi"  )
     ( ?m  (float 1 -3)  "Milli"  )
     ( ?u  (float 1 -6)  "Micro"  )
     ( ?n  (float 1 -9)  "Nano"	  )
     ( ?p  (float 1 -12) "Pico"	  )
     ( ?f  (float 1 -15) "Femto"  )
     ( ?a  (float 1 -18) "Atto"   )
     ( ?z  (float 1 -21) "zepto"  )
     ( ?y  (float 1 -24) "yocto"  )))

(defvar math-standard-units-systems
  '( ( base  nil )
     ( si    ( ( g   '(* (var kg var-kg) (float 1 -3)) ) ) )
     ( mks   ( ( g   '(* (var kg var-kg) (float 1 -3)) ) ) )
     ( cgs   ( ( m   '(* (var cm var-cm) 100         ) ) ) )))

(defvar math-units-table nil
  "Internal units table derived from math-defined-units.
Entries are (SYMBOL EXPR DOC-STRING TEMP-TYPE BASE-UNITS).")

(defvar math-units-table-buffer-valid nil)

;;; Units commands.

(defun calc-base-units ()
  (interactive)
  (calc-slow-wrapper
   (let ((calc-autorange-units nil))
     (calc-enter-result 1 "bsun" (math-simplify-units
				  (math-to-standard-units (calc-top-n 1)
							  nil))))))

(defun calc-quick-units ()
  (interactive)
  (calc-slow-wrapper
   (let* ((num (- last-command-char ?0))
	  (pos (if (= num 0) 10 num))
	  (units (calc-var-value 'var-Units))
	  (expr (calc-top-n 1)))
     (unless (and (>= num 0) (<= num 9))
       (error "Bad unit number"))
     (unless (math-vectorp units)
       (error "No \"quick units\" are defined"))
     (unless (< pos (length units))
       (error "Unit number %d not defined" pos))
     (if (math-units-in-expr-p expr nil)
	 (calc-enter-result 1 (format "cun%d" num)
			    (math-convert-units expr (nth pos units)))
       (calc-enter-result 1 (format "*un%d" num)
			  (math-simplify-units
			   (math-mul expr (nth pos units))))))))

(defun calc-convert-units (&optional old-units new-units)
  (interactive)
  (calc-slow-wrapper
   (let ((expr (calc-top-n 1))
	 (uoldname nil)
	 unew
         units)
     (unless (math-units-in-expr-p expr t)
       (let ((uold (or old-units
		       (progn
			 (setq uoldname (read-string "Old units: "))
			 (if (equal uoldname "")
			     (progn
			       (setq uoldname "1")
			       1)
			   (if (string-match "\\` */" uoldname)
			       (setq uoldname (concat "1" uoldname)))
			   (math-read-expr uoldname))))))
	 (when (eq (car-safe uold) 'error)
	   (error "Bad format in units expression: %s" (nth 1 uold)))
	 (setq expr (math-mul expr uold))))
     (unless new-units
       (setq new-units (read-string (if uoldname
					(concat "Old units: "
						uoldname
						", new units: ")
				      "New units: "))))
     (when (string-match "\\` */" new-units)
       (setq new-units (concat "1" new-units)))
     (setq units (math-read-expr new-units))
     (when (eq (car-safe units) 'error)
       (error "Bad format in units expression: %s" (nth 2 units)))
     (let ((unew (math-units-in-expr-p units t))
	   (std (and (eq (car-safe units) 'var)
		     (assq (nth 1 units) math-standard-units-systems))))
       (if std
	   (calc-enter-result 1 "cvun" (math-simplify-units
					(math-to-standard-units expr
								(nth 1 std))))
	 (unless unew
	   (error "No units specified"))
	 (calc-enter-result 1 "cvun"
			    (math-convert-units
			     expr units
			     (and uoldname (not (equal uoldname "1"))))))))))

(defun calc-autorange-units (arg)
  (interactive "P")
  (calc-wrapper
   (calc-change-mode 'calc-autorange-units arg nil t)
   (message (if calc-autorange-units
		"Adjusting target unit prefix automatically"
	      "Using target units exactly"))))

(defun calc-convert-temperature (&optional old-units new-units)
  (interactive)
  (calc-slow-wrapper
   (let ((expr (calc-top-n 1))
	 (uold nil)
	 (uoldname nil)
	 unew)
     (setq uold (or old-units
		    (let ((units (math-single-units-in-expr-p expr)))
		      (if units
			  (if (consp units)
			      (list 'var (car units)
				    (intern (concat "var-"
						    (symbol-name
						     (car units)))))
			    (error "Not a pure temperature expression"))
			(math-read-expr
			 (setq uoldname (read-string
					 "Old temperature units: ")))))))
     (when (eq (car-safe uold) 'error)
       (error "Bad format in units expression: %s" (nth 2 uold)))
     (or (math-units-in-expr-p expr nil)
	 (setq expr (math-mul expr uold)))
     (setq unew (or new-units
		    (math-read-expr
		     (read-string (if uoldname
				      (concat "Old temperature units: "
					      uoldname
					      ", new units: ")
				    "New temperature units: ")))))
     (when (eq (car-safe unew) 'error)
       (error "Bad format in units expression: %s" (nth 2 unew)))
     (calc-enter-result 1 "cvtm" (math-simplify-units
				  (math-convert-temperature expr uold unew
							    uoldname))))))

(defun calc-remove-units ()
  (interactive)
  (calc-slow-wrapper
   (calc-enter-result 1 "rmun" (math-simplify-units
				(math-remove-units (calc-top-n 1))))))

(defun calc-extract-units ()
  (interactive)
  (calc-slow-wrapper
   (calc-enter-result 1 "rmun" (math-simplify-units
				(math-extract-units (calc-top-n 1))))))

;; The variables calc-num-units and calc-den-units are local to 
;; calc-explain-units, but are used by calc-explain-units-rec,
;; which is called by calc-explain-units.
(defvar calc-num-units)
(defvar calc-den-units)

(defun calc-explain-units ()
  (interactive)
  (calc-wrapper
   (let ((calc-num-units nil)
	 (calc-den-units nil))
     (calc-explain-units-rec (calc-top-n 1) 1)
     (and calc-den-units (string-match "^[^(].* .*[^)]$" calc-den-units)
	  (setq calc-den-units (concat "(" calc-den-units ")")))
     (if calc-num-units
	 (if calc-den-units
	     (message "%s per %s" calc-num-units calc-den-units)
	   (message "%s" calc-num-units))
       (if calc-den-units
	   (message "1 per %s" calc-den-units)
	 (message "No units in expression"))))))

(defun calc-explain-units-rec (expr pow)
  (let ((u (math-check-unit-name expr))
	pos)
    (if (and u (not (math-zerop pow)))
	(let ((name (or (nth 2 u) (symbol-name (car u)))))
	  (if (eq (aref name 0) ?\*)
	      (setq name (substring name 1)))
	  (if (string-match "[^a-zA-Z0-9']" name)
	      (if (string-match "^[a-zA-Z0-9' ()]*$" name)
		  (while (setq pos (string-match "[ ()]" name))
		    (setq name (concat (substring name 0 pos)
				       (if (eq (aref name pos) 32) "-" "")
				       (substring name (1+ pos)))))
		(setq name (concat "(" name ")"))))
	  (or (eq (nth 1 expr) (car u))
	      (setq name (concat (nth 2 (assq (aref (symbol-name
						     (nth 1 expr)) 0)
					      math-unit-prefixes))
				 (if (and (string-match "[^a-zA-Z0-9']" name)
					  (not (memq (car u) '(mHg gf))))
				     (concat "-" name)
				   (downcase name)))))
	  (cond ((or (math-equal-int pow 1)
		     (math-equal-int pow -1)))
		((or (math-equal-int pow 2)
		     (math-equal-int pow -2))
		 (if (equal (nth 4 u) '((m . 1)))
		     (setq name (concat "Square-" name))
		   (setq name (concat name "-squared"))))
		((or (math-equal-int pow 3)
		     (math-equal-int pow -3))
		 (if (equal (nth 4 u) '((m . 1)))
		     (setq name (concat "Cubic-" name))
		   (setq name (concat name "-cubed"))))
		(t
		 (setq name (concat name "^"
				    (math-format-number (math-abs pow))))))
	  (if (math-posp pow)
	      (setq calc-num-units (if calc-num-units
				  (concat calc-num-units " " name)
				name))
	    (setq calc-den-units (if calc-den-units
				(concat calc-den-units " " name)
			      name))))
      (cond ((eq (car-safe expr) '*)
	     (calc-explain-units-rec (nth 1 expr) pow)
	     (calc-explain-units-rec (nth 2 expr) pow))
	    ((eq (car-safe expr) '/)
	     (calc-explain-units-rec (nth 1 expr) pow)
	     (calc-explain-units-rec (nth 2 expr) (- pow)))
	    ((memq (car-safe expr) '(neg + -))
	     (calc-explain-units-rec (nth 1 expr) pow))
	    ((and (eq (car-safe expr) '^)
		  (math-realp (nth 2 expr)))
	     (calc-explain-units-rec (nth 1 expr)
				     (math-mul pow (nth 2 expr))))))))

(defun calc-simplify-units ()
  (interactive)
  (calc-slow-wrapper
   (calc-with-default-simplification
    (calc-enter-result 1 "smun" (math-simplify-units (calc-top-n 1))))))

(defun calc-view-units-table (n)
  (interactive "P")
  (and n (setq math-units-table-buffer-valid nil))
  (let ((win (get-buffer-window "*Units Table*")))
    (if (and win
	     math-units-table
	     math-units-table-buffer-valid)
	(progn
	  (bury-buffer (window-buffer win))
	  (let ((curwin (selected-window)))
	    (select-window win)
	    (switch-to-buffer nil)
	    (select-window curwin)))
      (math-build-units-table-buffer nil))))

(defun calc-enter-units-table (n)
  (interactive "P")
  (and n (setq math-units-table-buffer-valid nil))
  (math-build-units-table-buffer t)
  (message "%s" (substitute-command-keys "Type \\[calc] to return to the Calculator")))

(defun calc-define-unit (uname desc)
  (interactive "SDefine unit name: \nsDescription: ")
  (calc-wrapper
   (let ((form (calc-top-n 1))
	 (unit (assq uname math-additional-units)))
     (or unit
	 (setq math-additional-units
	       (cons (setq unit (list uname nil nil))
		     math-additional-units)
	       math-units-table nil))
     (setcar (cdr unit) (and (not (and (eq (car-safe form) 'var)
				       (eq (nth 1 form) uname)))
			     (not (math-equal-int form 1))
			     (math-format-flat-expr form 0)))
     (setcar (cdr (cdr unit)) (and (not (equal desc ""))
				   desc))))
  (calc-invalidate-units-table))

(defun calc-undefine-unit (uname)
  (interactive "SUndefine unit name: ")
  (calc-wrapper
   (let ((unit (assq uname math-additional-units)))
     (or unit
	 (if (assq uname math-standard-units)
	     (error "\"%s\" is a predefined unit name" uname)
	   (error "Unit name \"%s\" not found" uname)))
     (setq math-additional-units (delq unit math-additional-units)
	   math-units-table nil)))
  (calc-invalidate-units-table))

(defun calc-invalidate-units-table ()
  (setq math-units-table nil)
  (let ((buf (get-buffer "*Units Table*")))
    (and buf
	 (save-excursion
	   (set-buffer buf)
	   (save-excursion
	     (goto-char (point-min))
	     (if (looking-at "Calculator Units Table")
		 (let ((inhibit-read-only t))
		   (insert "(Obsolete) "))))))))

(defun calc-get-unit-definition (uname)
  (interactive "SGet definition for unit: ")
  (calc-wrapper
   (math-build-units-table)
   (let ((unit (assq uname math-units-table)))
     (or unit
	 (error "Unit name \"%s\" not found" uname))
     (let ((msg (nth 2 unit)))
       (if (stringp msg)
	   (if (string-match "^\\*" msg)
	       (setq msg (substring msg 1)))
	 (setq msg (symbol-name uname)))
       (if (nth 1 unit)
	   (progn
	     (calc-enter-result 0 "ugdf" (nth 1 unit))
	     (message "Derived unit: %s" msg))
	 (calc-enter-result 0 "ugdf" (list 'var uname
					   (intern
					    (concat "var-"
						    (symbol-name uname)))))
	 (message "Base unit: %s" msg))))))

(defun calc-permanent-units ()
  (interactive)
  (calc-wrapper
   (let (pos)
     (set-buffer (find-file-noselect (substitute-in-file-name
				      calc-settings-file)))
     (goto-char (point-min))
     (if (and (search-forward ";;; Custom units stored by Calc" nil t)
	      (progn
		(beginning-of-line)
		(setq pos (point))
		(search-forward "\n;;; End of custom units" nil t)))
	 (progn
	   (beginning-of-line)
	   (forward-line 1)
	   (delete-region pos (point)))
       (goto-char (point-max))
       (insert "\n\n")
       (forward-char -1))
     (insert ";;; Custom units stored by Calc on " (current-time-string) "\n")
     (if math-additional-units
	 (progn
	   (insert "(setq math-additional-units '(\n")
	   (let ((list math-additional-units))
	     (while list
	       (insert "  (" (symbol-name (car (car list))) " "
		       (if (nth 1 (car list))
			   (if (stringp (nth 1 (car list)))
			       (prin1-to-string (nth 1 (car list)))
			     (prin1-to-string (math-format-flat-expr
					       (nth 1 (car list)) 0)))
			 "nil")
		       " "
		       (prin1-to-string (nth 2 (car list)))
		       ")\n")
	       (setq list (cdr list))))
	   (insert "))\n"))
       (insert ";;; (no custom units defined)\n"))
     (insert ";;; End of custom units\n")
     (save-buffer))))


;; The variable math-cu-unit-list is local to math-build-units-table,
;; but is used by math-compare-unit-names, which is called (indirectly)
;; by math-build-units-table.
;; math-cu-unit-list is also local to math-convert-units, but is used
;; by math-convert-units-rec, which is called by math-convert-units.
(defvar math-cu-unit-list)

(defun math-build-units-table ()
  (or math-units-table
      (let* ((combined-units (append math-additional-units
				     math-standard-units))
	     (math-cu-unit-list (mapcar 'car combined-units))
	     tab)
	(message "Building units table...")
	(setq math-units-table-buffer-valid nil)
	(setq tab (mapcar (function
			   (lambda (x)
			     (list (car x)
				   (and (nth 1 x)
					(if (stringp (nth 1 x))
					    (let ((exp (math-read-plain-expr
							(nth 1 x))))
					      (if (eq (car-safe exp) 'error)
						  (error "Format error in definition of %s in units table: %s"
							 (car x) (nth 2 exp))
						exp))
					  (nth 1 x)))
				   (nth 2 x)
				   (nth 3 x)
				   (and (not (nth 1 x))
					(list (cons (car x) 1))))))
			  combined-units))
	(let ((math-units-table tab))
	  (mapcar 'math-find-base-units tab))
	(message "Building units table...done")
	(setq math-units-table tab))))

;; The variables math-fbu-base and math-fbu-entry are local to
;; math-find-base-units, but are used by math-find-base-units-rec,
;; which is called by math-find-base-units.
(defvar math-fbu-base)
(defvar math-fbu-entry)

(defun math-find-base-units (math-fbu-entry)
  (if (eq (nth 4 math-fbu-entry) 'boom)
      (error "Circular definition involving unit %s" (car math-fbu-entry)))
  (or (nth 4 math-fbu-entry)
      (let (math-fbu-base)
	(setcar (nthcdr 4 math-fbu-entry) 'boom)
	(math-find-base-units-rec (nth 1 math-fbu-entry) 1)
	'(or math-fbu-base
	    (error "Dimensionless definition for unit %s" (car math-fbu-entry)))
	(while (eq (cdr (car math-fbu-base)) 0)
	  (setq math-fbu-base (cdr math-fbu-base)))
	(let ((b math-fbu-base))
	  (while (cdr b)
	    (if (eq (cdr (car (cdr b))) 0)
		(setcdr b (cdr (cdr b)))
	      (setq b (cdr b)))))
	(setq math-fbu-base (sort math-fbu-base 'math-compare-unit-names))
	(setcar (nthcdr 4 math-fbu-entry) math-fbu-base)
	math-fbu-base)))

(defun math-compare-unit-names (a b)
  (memq (car b) (cdr (memq (car a) math-cu-unit-list))))

(defun math-find-base-units-rec (expr pow)
  (let ((u (math-check-unit-name expr)))
    (cond (u
	   (let ((ulist (math-find-base-units u)))
	     (while ulist
	       (let ((p (* (cdr (car ulist)) pow))
		     (old (assq (car (car ulist)) math-fbu-base)))
		 (if old
		     (setcdr old (+ (cdr old) p))
		   (setq math-fbu-base 
                         (cons (cons (car (car ulist)) p) math-fbu-base))))
	       (setq ulist (cdr ulist)))))
	  ((math-scalarp expr))
	  ((and (eq (car expr) '^)
		(integerp (nth 2 expr)))
	   (math-find-base-units-rec (nth 1 expr) (* pow (nth 2 expr))))
	  ((eq (car expr) '*)
	   (math-find-base-units-rec (nth 1 expr) pow)
	   (math-find-base-units-rec (nth 2 expr) pow))
	  ((eq (car expr) '/)
	   (math-find-base-units-rec (nth 1 expr) pow)
	   (math-find-base-units-rec (nth 2 expr) (- pow)))
	  ((eq (car expr) 'neg)
	   (math-find-base-units-rec (nth 1 expr) pow))
	  ((eq (car expr) '+)
	   (math-find-base-units-rec (nth 1 expr) pow))
	  ((eq (car expr) 'var)
	   (or (eq (nth 1 expr) 'pi)
	       (error "Unknown name %s in defining expression for unit %s"
		      (nth 1 expr) (car math-fbu-entry))))
	  (t (error "Malformed defining expression for unit %s" (car math-fbu-entry))))))


(defun math-units-in-expr-p (expr sub-exprs)
  (and (consp expr)
       (if (eq (car expr) 'var)
	   (math-check-unit-name expr)
	 (and (or sub-exprs
		  (memq (car expr) '(* / ^)))
	      (or (math-units-in-expr-p (nth 1 expr) sub-exprs)
		  (math-units-in-expr-p (nth 2 expr) sub-exprs))))))

(defun math-only-units-in-expr-p (expr)
  (and (consp expr)
       (if (eq (car expr) 'var)
	   (math-check-unit-name expr)
	 (if (memq (car expr) '(* /))
	     (and (math-only-units-in-expr-p (nth 1 expr))
		  (math-only-units-in-expr-p (nth 2 expr)))
	   (and (eq (car expr) '^)
		(and (math-only-units-in-expr-p (nth 1 expr))
		     (math-realp (nth 2 expr))))))))

(defun math-single-units-in-expr-p (expr)
  (cond ((math-scalarp expr) nil)
	((eq (car expr) 'var)
	 (math-check-unit-name expr))
	((eq (car expr) '*)
	 (let ((u1 (math-single-units-in-expr-p (nth 1 expr)))
	       (u2 (math-single-units-in-expr-p (nth 2 expr))))
	   (or (and u1 u2 'wrong)
	       u1
	       u2)))
	((eq (car expr) '/)
	 (if (math-units-in-expr-p (nth 2 expr) nil)
	     'wrong
	   (math-single-units-in-expr-p (nth 1 expr))))
	(t 'wrong)))

(defun math-check-unit-name (v)
  (and (eq (car-safe v) 'var)
       (or (assq (nth 1 v) (or math-units-table (math-build-units-table)))
	   (let ((name (symbol-name (nth 1 v))))
	     (and (> (length name) 1)
		  (assq (aref name 0) math-unit-prefixes)
		  (or (assq (intern (substring name 1)) math-units-table)
		      (and (eq (aref name 0) ?M)
			   (> (length name) 3)
			   (eq (aref name 1) ?e)
			   (eq (aref name 2) ?g)
			   (assq (intern (substring name 3))
				 math-units-table))))))))

;; The variable math-which-standard is local to math-to-standard-units,
;; but is used by math-to-standard-rec, which is called by
;; math-to-standard-units.
(defvar math-which-standard)

(defun math-to-standard-units (expr math-which-standard)
  (math-to-standard-rec expr))

(defun math-to-standard-rec (expr)
  (if (eq (car-safe expr) 'var)
      (let ((u (math-check-unit-name expr))
	    (base (nth 1 expr)))
	(if u
	    (progn
	      (if (nth 1 u)
		  (setq expr (math-to-standard-rec (nth 1 u)))
		(let ((st (assq (car u) math-which-standard)))
		  (if st
		      (setq expr (nth 1 st))
		    (setq expr (list 'var (car u)
				     (intern (concat "var-"
						     (symbol-name
						      (car u)))))))))
	      (or (null u)
		  (eq base (car u))
		  (setq expr (list '*
				   (nth 1 (assq (aref (symbol-name base) 0)
						math-unit-prefixes))
				   expr)))
	      expr)
	  (if (eq base 'pi)
	      (math-pi)
	    expr)))
    (if (Math-primp expr)
	expr
      (cons (car expr)
	    (mapcar 'math-to-standard-rec (cdr expr))))))

(defun math-apply-units (expr units ulist &optional pure)
  (setq expr (math-simplify-units expr))
  (if ulist
      (let ((new 0)
	    value)
	(or (math-numberp expr)
	    (error "Incompatible units"))
	(while (cdr ulist)
	  (setq value (math-div expr (nth 1 (car ulist)))
		value (math-floor (let ((calc-internal-prec
					 (1- calc-internal-prec)))
				    (math-normalize value)))
		new (math-add new (math-mul value (car (car ulist))))
		expr (math-sub expr (math-mul value (nth 1 (car ulist))))
		ulist (cdr ulist)))
	(math-add new (math-mul (math-div expr (nth 1 (car ulist)))
				(car (car ulist)))))
    (if pure
        expr
      (math-simplify-units (list '* expr units)))))

(defvar math-decompose-units-cache nil)
(defun math-decompose-units (units)
  (let ((u (math-check-unit-name units)))
    (and u (eq (car-safe (nth 1 u)) '+)
	 (setq units (nth 1 u))))
  (setq units (calcFunc-expand units))
  (and (eq (car-safe units) '+)
       (let ((entry (list units calc-internal-prec calc-prefer-frac)))
	 (or (equal entry (car math-decompose-units-cache))
	     (let ((ulist nil)
		   (utemp units)
		   qty unit)
	       (while (eq (car-safe utemp) '+)
		 (setq ulist (cons (math-decompose-unit-part (nth 2 utemp))
				   ulist)
		       utemp (nth 1 utemp)))
	       (setq ulist (cons (math-decompose-unit-part utemp) ulist)
		     utemp ulist)
	       (while (setq utemp (cdr utemp))
		 (unless (equal (nth 2 (car utemp)) (nth 2 (car ulist)))
		   (error "Inconsistent units in sum")))
	       (setq math-decompose-units-cache
		     (cons entry
			   (sort ulist
				 (function
				  (lambda (x y)
				    (not (Math-lessp (nth 1 x)
						     (nth 1 y))))))))))
	 (cdr math-decompose-units-cache))))

(defun math-decompose-unit-part (unit)
  (cons unit
	(math-is-multiple (math-simplify-units (math-to-standard-units
						unit nil))
			  t)))

;; The variable math-fcu-u is local to math-find-compatible-unit,
;; but is used by math-find-compatible-rec which is called by
;; math-find-compatible-unit.
(defvar math-fcu-u)

(defun math-find-compatible-unit (expr unit)
  (let ((math-fcu-u (math-check-unit-name unit)))
    (if math-fcu-u
	(math-find-compatible-unit-rec expr 1))))

(defun math-find-compatible-unit-rec (expr pow)
  (cond ((eq (car-safe expr) '*)
	 (or (math-find-compatible-unit-rec (nth 1 expr) pow)
	     (math-find-compatible-unit-rec (nth 2 expr) pow)))
	((eq (car-safe expr) '/)
	 (or (math-find-compatible-unit-rec (nth 1 expr) pow)
	     (math-find-compatible-unit-rec (nth 2 expr) (- pow))))
	((and (eq (car-safe expr) '^)
	      (integerp (nth 2 expr)))
	 (math-find-compatible-unit-rec (nth 1 expr) (* pow (nth 2 expr))))
	(t
	 (let ((u2 (math-check-unit-name expr)))
	   (if (equal (nth 4 math-fcu-u) (nth 4 u2))
	       (cons expr pow))))))

;; The variables math-cu-new-units and math-cu-pure are local to 
;; math-convert-units, but are used by math-convert-units-rec, 
;; which is called by math-convert-units.
(defvar math-cu-new-units)
(defvar math-cu-pure)

(defun math-convert-units (expr math-cu-new-units &optional math-cu-pure)
  (if (eq (car-safe math-cu-new-units) 'var)
      (let ((unew (assq (nth 1 math-cu-new-units)
                        (math-build-units-table))))
        (if (eq (car-safe (nth 1 unew)) '+)
            (setq math-cu-new-units (nth 1 unew)))))
  (math-with-extra-prec 2
    (let ((compat (and (not math-cu-pure) 
                       (math-find-compatible-unit expr math-cu-new-units)))
	  (math-cu-unit-list nil)
	  (math-combining-units nil))
      (if compat
	  (math-simplify-units
	   (math-mul (math-mul (math-simplify-units
				(math-div expr (math-pow (car compat)
							 (cdr compat))))
			       (math-pow math-cu-new-units (cdr compat)))
		     (math-simplify-units
		      (math-to-standard-units
		       (math-pow (math-div (car compat) math-cu-new-units)
				 (cdr compat))
		       nil))))
	(when (setq math-cu-unit-list (math-decompose-units math-cu-new-units))
	  (setq math-cu-new-units (nth 2 (car math-cu-unit-list))))
	(when (eq (car-safe expr) '+)
	  (setq expr (math-simplify-units expr)))
	(if (math-units-in-expr-p expr t)
	    (math-convert-units-rec expr)
	  (math-apply-units (math-to-standard-units
			     (list '/ expr math-cu-new-units) nil)
			    math-cu-new-units math-cu-unit-list math-cu-pure))))))

(defun math-convert-units-rec (expr)
  (if (math-units-in-expr-p expr nil)
      (math-apply-units (math-to-standard-units 
                         (list '/ expr math-cu-new-units) nil)
			math-cu-new-units math-cu-unit-list math-cu-pure)
    (if (Math-primp expr)
	expr
      (cons (car expr)
	    (mapcar 'math-convert-units-rec (cdr expr))))))

(defun math-convert-temperature (expr old new &optional pure)
  (let* ((units (math-single-units-in-expr-p expr))
	 (uold (if old
		   (if (or (null units)
			   (equal (nth 1 old) (car units)))
		       (math-check-unit-name old)
		     (error "Inconsistent temperature units"))
		 units))
	 (unew (math-check-unit-name new)))
    (unless (and (consp unew) (nth 3 unew))
      (error "Not a valid temperature unit"))
    (unless (and (consp uold) (nth 3 uold))
      (error "Not a pure temperature expression"))
    (let ((v (car uold)))
      (setq expr (list '/ expr (list 'var v
				     (intern (concat "var-"
						     (symbol-name v)))))))
    (or (eq (nth 3 uold) (nth 3 unew))
	(cond ((eq (nth 3 uold) 'K)
	       (setq expr (list '- expr '(float 27315 -2)))
	       (if (eq (nth 3 unew) 'F)
		   (setq expr (list '+ (list '* expr '(frac 9 5)) 32))))
	      ((eq (nth 3 uold) 'C)
	       (if (eq (nth 3 unew) 'F)
		   (setq expr (list '+ (list '* expr '(frac 9 5)) 32))
		 (setq expr (list '+ expr '(float 27315 -2)))))
	      (t
	       (setq expr (list '* (list '- expr 32) '(frac 5 9)))
	       (if (eq (nth 3 unew) 'K)
		   (setq expr (list '+ expr '(float 27315 -2)))))))
    (if pure
	expr
      (list '* expr new))))



(defun math-simplify-units (a)
  (let ((math-simplifying-units t)
	(calc-matrix-mode 'scalar))
    (math-simplify a)))
(defalias 'calcFunc-usimplify 'math-simplify-units)

;; The function created by math-defsimplify uses the variable
;; math-simplify-expr, and so is used by functions in math-defsimplify
(defvar math-simplify-expr)

(math-defsimplify (+ -)
  (and math-simplifying-units
       (math-units-in-expr-p (nth 1 math-simplify-expr) nil)
       (let* ((units (math-extract-units (nth 1 math-simplify-expr)))
	      (ratio (math-simplify (math-to-standard-units
				     (list '/ (nth 2 math-simplify-expr) units) nil))))
	 (if (math-units-in-expr-p ratio nil)
	     (progn
	       (calc-record-why "*Inconsistent units" math-simplify-expr)
	       math-simplify-expr)
	   (list '* (math-add (math-remove-units (nth 1 math-simplify-expr))
			      (if (eq (car math-simplify-expr) '-) 
                                  (math-neg ratio) ratio))
		 units)))))

(math-defsimplify *
  (math-simplify-units-prod))

(defun math-simplify-units-prod ()
  (and math-simplifying-units
       calc-autorange-units
       (Math-realp (nth 1 math-simplify-expr))
       (let* ((num (math-float (nth 1 math-simplify-expr)))
	      (xpon (calcFunc-xpon num))
	      (unitp (cdr (cdr math-simplify-expr)))
	      (unit (car unitp))
	      (pow (if (eq (car math-simplify-expr) '*) 1 -1))
	      u)
	 (and (eq (car-safe unit) '*)
	      (setq unitp (cdr unit)
		    unit (car unitp)))
	 (and (eq (car-safe unit) '^)
	      (integerp (nth 2 unit))
	      (setq pow (* pow (nth 2 unit))
		    unitp (cdr unit)
		    unit (car unitp)))
	 (and (setq u (math-check-unit-name unit))
	      (integerp xpon)
	      (or (< xpon 0)
		  (>= xpon (if (eq (car u) 'm) 1 3)))
	      (let* ((uxpon 0)
		     (pref (if (< pow 0)
			       (reverse math-unit-prefixes)
			     math-unit-prefixes))
		     (p pref)
		     pxpon pname)
		(or (eq (car u) (nth 1 unit))
		    (setq uxpon (* pow
				   (nth 2 (nth 1 (assq
						  (aref (symbol-name
							 (nth 1 unit)) 0)
						  math-unit-prefixes))))))
		(setq xpon (+ xpon uxpon))
		(while (and p
			    (or (memq (car (car p)) '(?d ?D ?h ?H))
				(and (eq (car (car p)) ?c)
				     (not (eq (car u) 'm)))
				(< xpon (setq pxpon (* (nth 2 (nth 1 (car p)))
						       pow)))
				(progn
				  (setq pname (math-build-var-name
					       (if (eq (car (car p)) 0)
						   (car u)
						 (concat (char-to-string
							  (car (car p)))
							 (symbol-name
							  (car u))))))
				  (and (/= (car (car p)) 0)
				       (assq (nth 1 pname)
					     math-units-table)))))
		  (setq p (cdr p)))
		(and p
		     (/= pxpon uxpon)
		     (or (not (eq p pref))
			 (< xpon (+ pxpon (* (math-abs pow) 3))))
		     (progn
		       (setcar (cdr math-simplify-expr)
			       (let ((calc-prefer-frac nil))
				 (calcFunc-scf (nth 1 math-simplify-expr)
					       (- uxpon pxpon))))
		       (setcar unitp pname)
		       math-simplify-expr)))))))

(defvar math-try-cancel-units)

(math-defsimplify /
  (and math-simplifying-units
       (let ((np (cdr math-simplify-expr))
	     (math-try-cancel-units 0)
	     n nn)
	 (setq n (if (eq (car-safe (nth 2 math-simplify-expr)) '*)
		     (cdr (nth 2 math-simplify-expr))
		   (nthcdr 2 math-simplify-expr)))
	 (if (math-realp (car n))
	     (progn
	       (setcar (cdr math-simplify-expr) (math-mul (nth 1 math-simplify-expr)
					    (let ((calc-prefer-frac nil))
					      (math-div 1 (car n)))))
	       (setcar n 1)))
	 (while (eq (car-safe (setq n (car np))) '*)
	   (math-simplify-units-divisor (cdr n) (cdr (cdr math-simplify-expr)))
	   (setq np (cdr (cdr n))))
	 (math-simplify-units-divisor np (cdr (cdr math-simplify-expr)))
	 (if (eq math-try-cancel-units 0)
	     (let* ((math-simplifying-units nil)
		    (base (math-simplify 
                           (math-to-standard-units math-simplify-expr nil))))
	       (if (Math-numberp base)
		   (setq math-simplify-expr base))))
	 (if (eq (car-safe math-simplify-expr) '/)
	     (math-simplify-units-prod))
	 math-simplify-expr)))

(defun math-simplify-units-divisor (np dp)
  (let ((n (car np))
	d dd temp)
    (while (eq (car-safe (setq d (car dp))) '*)
      (when (setq temp (math-simplify-units-quotient n (nth 1 d)))
	(setcar np (setq n temp))
	(setcar (cdr d) 1))
      (setq dp (cdr (cdr d))))
    (when (setq temp (math-simplify-units-quotient n d))
      (setcar np (setq n temp))
      (setcar dp 1))))

;; Simplify, e.g., "in / cm" to "2.54" in a units expression.
(defun math-simplify-units-quotient (n d)
  (let ((pow1 1)
	(pow2 1))
    (when (and (eq (car-safe n) '^)
	       (integerp (nth 2 n)))
      (setq pow1 (nth 2 n) n (nth 1 n)))
    (when (and (eq (car-safe d) '^)
	       (integerp (nth 2 d)))
      (setq pow2 (nth 2 d) d (nth 1 d)))
    (let ((un (math-check-unit-name n))
	  (ud (math-check-unit-name d)))
      (and un ud
	   (if (and (equal (nth 4 un) (nth 4 ud))
		    (eq pow1 pow2))
	       (math-to-standard-units (list '/ n d) nil)
	     (let (ud1)
	       (setq un (nth 4 un)
		     ud (nth 4 ud))
	       (while un
		 (setq ud1 ud)
		 (while ud1
		   (and (eq (car (car un)) (car (car ud1)))
			(setq math-try-cancel-units
			      (+ math-try-cancel-units
				 (- (* (cdr (car un)) pow1)
				    (* (cdr (car ud)) pow2)))))
		   (setq ud1 (cdr ud1)))
		 (setq un (cdr un)))
	       nil))))))

(math-defsimplify ^
  (and math-simplifying-units
       (math-realp (nth 2 math-simplify-expr))
       (if (memq (car-safe (nth 1 math-simplify-expr)) '(* /))
	   (list (car (nth 1 math-simplify-expr))
		 (list '^ (nth 1 (nth 1 math-simplify-expr)) 
                       (nth 2 math-simplify-expr))
		 (list '^ (nth 2 (nth 1 math-simplify-expr)) 
                       (nth 2 math-simplify-expr)))
	 (math-simplify-units-pow (nth 1 math-simplify-expr) 
                                  (nth 2 math-simplify-expr)))))

(math-defsimplify calcFunc-sqrt
  (and math-simplifying-units
       (if (memq (car-safe (nth 1 math-simplify-expr)) '(* /))
	   (list (car (nth 1 math-simplify-expr))
		 (list 'calcFunc-sqrt (nth 1 (nth 1 math-simplify-expr)))
		 (list 'calcFunc-sqrt (nth 2 (nth 1 math-simplify-expr))))
	 (math-simplify-units-pow (nth 1 math-simplify-expr) '(frac 1 2)))))

(math-defsimplify (calcFunc-floor
		   calcFunc-ceil
		   calcFunc-round
		   calcFunc-rounde
		   calcFunc-roundu
		   calcFunc-trunc
		   calcFunc-float
		   calcFunc-frac
		   calcFunc-abs
		   calcFunc-clean)
  (and math-simplifying-units
       (= (length math-simplify-expr) 2)
       (if (math-only-units-in-expr-p (nth 1 math-simplify-expr))
	   (nth 1 math-simplify-expr)
	 (if (and (memq (car-safe (nth 1 math-simplify-expr)) '(* /))
		  (or (math-only-units-in-expr-p
		       (nth 1 (nth 1 math-simplify-expr)))
		      (math-only-units-in-expr-p
		       (nth 2 (nth 1 math-simplify-expr)))))
	     (list (car (nth 1 math-simplify-expr))
		   (cons (car math-simplify-expr)
			 (cons (nth 1 (nth 1 math-simplify-expr))
			       (cdr (cdr math-simplify-expr))))
		   (cons (car math-simplify-expr)
			 (cons (nth 2 (nth 1 math-simplify-expr))
			       (cdr (cdr math-simplify-expr)))))))))

(defun math-simplify-units-pow (a pow)
  (if (and (eq (car-safe a) '^)
	   (math-check-unit-name (nth 1 a))
	   (math-realp (nth 2 a)))
      (list '^ (nth 1 a) (math-mul pow (nth 2 a)))
    (let* ((u (math-check-unit-name a))
	   (pf (math-to-simple-fraction pow))
	   (d (and (eq (car-safe pf) 'frac) (nth 2 pf))))
      (and u d
	   (math-units-are-multiple u d)
	   (list '^ (math-to-standard-units a nil) pow)))))


(defun math-units-are-multiple (u n)
  (setq u (nth 4 u))
  (while (and u (= (% (cdr (car u)) n) 0))
    (setq u (cdr u)))
  (null u))

(math-defsimplify calcFunc-sin
  (and math-simplifying-units
       (math-units-in-expr-p (nth 1 math-simplify-expr) nil)
       (let ((rad (math-simplify-units
		   (math-evaluate-expr
		    (math-to-standard-units (nth 1 math-simplify-expr) nil))))
	     (calc-angle-mode 'rad))
	 (and (eq (car-safe rad) '*)
	      (math-realp (nth 1 rad))
	      (eq (car-safe (nth 2 rad)) 'var)
	      (eq (nth 1 (nth 2 rad)) 'rad)
	      (list 'calcFunc-sin (nth 1 rad))))))

(math-defsimplify calcFunc-cos
  (and math-simplifying-units
       (math-units-in-expr-p (nth 1 math-simplify-expr) nil)
       (let ((rad (math-simplify-units
		   (math-evaluate-expr
		    (math-to-standard-units (nth 1 math-simplify-expr) nil))))
	     (calc-angle-mode 'rad))
	 (and (eq (car-safe rad) '*)
	      (math-realp (nth 1 rad))
	      (eq (car-safe (nth 2 rad)) 'var)
	      (eq (nth 1 (nth 2 rad)) 'rad)
	      (list 'calcFunc-cos (nth 1 rad))))))

(math-defsimplify calcFunc-tan
  (and math-simplifying-units
       (math-units-in-expr-p (nth 1 math-simplify-expr) nil)
       (let ((rad (math-simplify-units
		   (math-evaluate-expr
		    (math-to-standard-units (nth 1 math-simplify-expr) nil))))
	     (calc-angle-mode 'rad))
	 (and (eq (car-safe rad) '*)
	      (math-realp (nth 1 rad))
	      (eq (car-safe (nth 2 rad)) 'var)
	      (eq (nth 1 (nth 2 rad)) 'rad)
	      (list 'calcFunc-tan (nth 1 rad))))))

(math-defsimplify calcFunc-sec
  (and math-simplifying-units
       (math-units-in-expr-p (nth 1 math-simplify-expr) nil)
       (let ((rad (math-simplify-units
		   (math-evaluate-expr
		    (math-to-standard-units (nth 1 math-simplify-expr) nil))))
	     (calc-angle-mode 'rad))
	 (and (eq (car-safe rad) '*)
	      (math-realp (nth 1 rad))
	      (eq (car-safe (nth 2 rad)) 'var)
	      (eq (nth 1 (nth 2 rad)) 'rad)
	      (list 'calcFunc-sec (nth 1 rad))))))

(math-defsimplify calcFunc-csc
  (and math-simplifying-units
       (math-units-in-expr-p (nth 1 math-simplify-expr) nil)
       (let ((rad (math-simplify-units
		   (math-evaluate-expr
		    (math-to-standard-units (nth 1 math-simplify-expr) nil))))
	     (calc-angle-mode 'rad))
	 (and (eq (car-safe rad) '*)
	      (math-realp (nth 1 rad))
	      (eq (car-safe (nth 2 rad)) 'var)
	      (eq (nth 1 (nth 2 rad)) 'rad)
	      (list 'calcFunc-csc (nth 1 rad))))))

(math-defsimplify calcFunc-cot
  (and math-simplifying-units
       (math-units-in-expr-p (nth 1 math-simplify-expr) nil)
       (let ((rad (math-simplify-units
		   (math-evaluate-expr
		    (math-to-standard-units (nth 1 math-simplify-expr) nil))))
	     (calc-angle-mode 'rad))
	 (and (eq (car-safe rad) '*)
	      (math-realp (nth 1 rad))
	      (eq (car-safe (nth 2 rad)) 'var)
	      (eq (nth 1 (nth 2 rad)) 'rad)
	      (list 'calcFunc-cot (nth 1 rad))))))


(defun math-remove-units (expr)
  (if (math-check-unit-name expr)
      1
    (if (Math-primp expr)
	expr
      (cons (car expr)
	    (mapcar 'math-remove-units (cdr expr))))))

(defun math-extract-units (expr)
  (if (memq (car-safe expr) '(* /))
      (cons (car expr)
	    (mapcar 'math-extract-units (cdr expr)))
    (if (math-check-unit-name expr) expr 1)))

(defun math-build-units-table-buffer (enter-buffer)
  (if (not (and math-units-table math-units-table-buffer-valid
		(get-buffer "*Units Table*")))
      (let ((buf (get-buffer-create "*Units Table*"))
	    (uptr (math-build-units-table))
	    (calc-language (if (eq calc-language 'big) nil calc-language))
	    (calc-float-format '(float 0))
	    (calc-group-digits nil)
	    (calc-number-radix 10)
	    (calc-point-char ".")
	    (std nil)
	    u name shadowed)
	(save-excursion
	  (message "Formatting units table...")
	  (set-buffer buf)
          (let ((inhibit-read-only t))
            (erase-buffer)
            (insert "Calculator Units Table:\n\n")
            (insert "Unit    Type  Definition                  Description\n\n")
            (while uptr
              (setq u (car uptr)
                    name (nth 2 u))
              (when (eq (car u) 'm)
                (setq std t))
              (setq shadowed (and std (assq (car u) math-additional-units)))
              (when (and name
                         (> (length name) 1)
                         (eq (aref name 0) ?\*))
                (unless (eq uptr math-units-table)
                  (insert "\n"))
                (setq name (substring name 1)))
              (insert " ")
              (and shadowed (insert "("))
              (insert (symbol-name (car u)))
              (and shadowed (insert ")"))
              (if (nth 3 u)
                  (progn
                    (indent-to 10)
                    (insert (symbol-name (nth 3 u))))
                (or std
                    (progn
                      (indent-to 10)
                      (insert "U"))))
              (indent-to 14)
              (and shadowed (insert "("))
              (if (nth 1 u)
                  (insert (math-format-value (nth 1 u) 80))
                (insert (symbol-name (car u))))
              (and shadowed (insert ")"))
              (indent-to 41)
              (insert " ")
              (when name
                (insert name))
              (if shadowed
                  (insert " (redefined above)")
                (unless (nth 1 u)
                  (insert " (base unit)")))
              (insert "\n")
              (setq uptr (cdr uptr)))
            (insert "\n\nUnit Prefix Table:\n\n")
            (setq uptr math-unit-prefixes)
            (while uptr
              (setq u (car uptr))
              (insert " " (char-to-string (car u)))
              (if (equal (nth 1 u) (nth 1 (nth 1 uptr)))
                  (insert " " (char-to-string (car (car (setq uptr (cdr uptr)))))
                          "   ")
                (insert "     "))
              (insert "10^" (int-to-string (nth 2 (nth 1 u))))
              (indent-to 15)
              (insert "   " (nth 2 u) "\n")
              (while (eq (car (car (setq uptr (cdr uptr)))) 0)))
            (insert "\n"))
	  (view-mode)
	  (message "Formatting units table...done"))
	(setq math-units-table-buffer-valid t)
	(let ((oldbuf (current-buffer)))
	  (set-buffer buf)
	  (goto-char (point-min))
	  (set-buffer oldbuf))
	(if enter-buffer
	    (pop-to-buffer buf)
	  (display-buffer buf)))
    (if enter-buffer
	(pop-to-buffer (get-buffer "*Units Table*"))
      (display-buffer (get-buffer "*Units Table*")))))

(provide 'calc-units)

;; Local Variables:
;; coding: iso-latin-1
;; End:

;;; arch-tag: e993314f-3adc-4191-be61-4ef8874881c4
;;; calc-units.el ends here
