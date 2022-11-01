;;; faces.el --- Lisp faces

;; Copyright (C) 1992, 1993, 1994, 1995, 1996, 1998, 1999, 2000, 2001,
;;   2002, 2003, 2004, 2005, 2006, 2007 Free Software Foundation, Inc.

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

;;; Code:

(eval-when-compile
  (require 'cl)
  ;; Warning suppression -- can't require x-win in batch:
  (autoload 'xw-defined-colors "x-win"))

(defvar help-xref-stack-item)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Font selection.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(defgroup font-selection nil
  "Influencing face font selection."
  :group 'faces)


(defcustom face-font-selection-order
  '(:width :height :weight :slant)
  "*A list specifying how face font selection chooses fonts.
Each of the four symbols `:width', `:height', `:weight', and `:slant'
must appear once in the list, and the list must not contain any other
elements.  Font selection tries to find a best matching font for
those face attributes first that appear first in the list.  For
example, if `:slant' appears before `:height', font selection first
tries to find a font with a suitable slant, even if this results in
a font height that isn't optimal."
  :tag "Font selection order"
  :type '(list symbol symbol symbol symbol)
  :group 'font-selection
  :set #'(lambda (symbol value)
	   (set-default symbol value)
	   (internal-set-font-selection-order value)))


;; This is defined originally in xfaces.c.
(defcustom face-font-family-alternatives
  '(("courier" "fixed")
    ("helv" "helvetica" "arial" "fixed"))
  "*Alist of alternative font family names.
Each element has the form (FAMILY ALTERNATIVE1 ALTERNATIVE2 ...).
If fonts of family FAMILY can't be loaded, try ALTERNATIVE1, then
ALTERNATIVE2 etc."
  :tag "Alternative font families to try"
  :type '(repeat (repeat string))
  :group 'font-selection
  :set #'(lambda (symbol value)
	   (set-default symbol value)
	   (internal-set-alternative-font-family-alist value)))


;; This is defined originally in xfaces.c.
(defcustom face-font-registry-alternatives
  (if (eq system-type 'windows-nt)
      '(("iso8859-1" "ms-oemlatin")
	("gb2312.1980" "gb2312")
	("jisx0208.1990" "jisx0208.1983" "jisx0208.1978")
	("ksc5601.1989" "ksx1001.1992" "ksc5601.1987")
	("muletibetan-2" "muletibetan-0"))
    '(("gb2312.1980" "gb2312.80&gb8565.88" "gbk*")
      ("jisx0208.1990" "jisx0208.1983" "jisx0208.1978")
      ("ksc5601.1989" "ksx1001.1992" "ksc5601.1987")
      ("muletibetan-2" "muletibetan-0")))
  "*Alist of alternative font registry names.
Each element has the form (REGISTRY ALTERNATIVE1 ALTERNATIVE2 ...).
If fonts of registry REGISTRY can be loaded, font selection
tries to find a best matching font among all fonts of registry
REGISTRY, ALTERNATIVE1, ALTERNATIVE2, and etc."
  :tag "Alternative font registries to try"
  :type '(repeat (repeat string))
  :version "21.1"
  :group 'font-selection
  :set #'(lambda (symbol value)
	   (set-default symbol value)
	   (internal-set-alternative-font-registry-alist value)))



;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Creation, copying.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


(defun face-list ()
  "Return a list of all defined face names."
  (mapcar #'car face-new-frame-defaults))


;;; ### If not frame-local initialize by what X resources?

(defun make-face (face &optional no-init-from-resources)
  "Define a new face with name FACE, a symbol.
NO-INIT-FROM-RESOURCES non-nil means don't initialize frame-local
variants of FACE from X resources.  (X resources recognized are found
in the global variable `face-x-resources'.)  If FACE is already known
as a face, leave it unmodified.  Value is FACE."
  (interactive "SMake face: ")
  (unless (facep face)
    ;; Make frame-local faces (this also makes the global one).
    (dolist (frame (frame-list))
      (internal-make-lisp-face face frame))
    ;; Add the face to the face menu.
    (when (fboundp 'facemenu-add-new-face)
      (facemenu-add-new-face face))
    ;; Define frame-local faces for all frames from X resources.
    (unless no-init-from-resources
      (make-face-x-resource-internal face)))
  face)


(defun make-empty-face (face)
  "Define a new, empty face with name FACE.
If the face already exists, it is left unmodified.  Value is FACE."
  (interactive "SMake empty face: ")
  (make-face face 'no-init-from-resources))


(defun copy-face (old-face new-face &optional frame new-frame)
  "Define a face just like OLD-FACE, with name NEW-FACE.

If NEW-FACE already exists as a face, it is modified to be like
OLD-FACE.  If it doesn't already exist, it is created.

If the optional argument FRAME is given as a frame, NEW-FACE is
changed on FRAME only.
If FRAME is t, the frame-independent default specification for OLD-FACE
is copied to NEW-FACE.
If FRAME is nil, copying is done for the frame-independent defaults
and for each existing frame.

If the optional fourth argument NEW-FRAME is given,
copy the information from face OLD-FACE on frame FRAME
to NEW-FACE on frame NEW-FRAME."
  (let ((inhibit-quit t))
    (if (null frame)
	(progn
	  (dolist (frame (frame-list))
	    (copy-face old-face new-face frame))
	  (copy-face old-face new-face t))
      (internal-copy-lisp-face old-face new-face frame new-frame))
    new-face))



;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Obsolete functions
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; The functions in this section are defined because Lisp packages use
;; them, despite the prefix `internal-' suggesting that they are
;; private to the face implementation.

(defun internal-find-face (name &optional frame)
  "Retrieve the face named NAME.
Return nil if there is no such face.
If NAME is already a face, it is simply returned.
The optional argument FRAME is ignored."
  (facep name))
(make-obsolete 'internal-find-face 'facep "21.1")


(defun internal-get-face (name &optional frame)
  "Retrieve the face named NAME; error if there is none.
If NAME is already a face, it is simply returned.
The optional argument FRAME is ignored."
  (or (facep name)
      (check-face name)))
(make-obsolete 'internal-get-face "see `facep' and `check-face'." "21.1")


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Predicates, type checks.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(defun facep (face)
  "Return non-nil if FACE is a face name or internal face object.
Return nil otherwise.  A face name can be a string or a symbol.
An internal face object is a vector of the kind used internally
to record face data."
  (internal-lisp-face-p face))


(defun check-face (face)
  "Signal an error if FACE doesn't name a face.
Value is FACE."
  (unless (facep face)
    (error "Not a face: %s" face))
  face)


;; The ID returned is not to be confused with the internally used IDs
;; of realized faces.  The ID assigned to Lisp faces is used to
;; support faces in display table entries.

(defun face-id (face &optional frame)
  "Return the internal ID of face with name FACE.
The optional argument FRAME is ignored, since the internal face ID
of a face name is the same for all frames."
  (check-face face)
  (get face 'face))


(defun face-equal (face1 face2 &optional frame)
  "Non-nil if faces FACE1 and FACE2 are equal.
Faces are considered equal if all their attributes are equal.
If the optional argument FRAME is given, report on FACE1 and FACE2 in that frame.
If FRAME is t, report on the defaults for FACE1 and FACE2 (for new frames).
If FRAME is omitted or nil, use the selected frame."
  (internal-lisp-face-equal-p face1 face2 frame))


(defun face-differs-from-default-p (face &optional frame)
  "Return non-nil if FACE displays differently from the default face.
If the optional argument FRAME is given, report on face FACE in that frame.
If FRAME is t, report on the defaults for face FACE (for new frames).
If FRAME is omitted or nil, use the selected frame."
  (let ((attrs
	 '(:family :width :height :weight :slant :foreground
	   :background :underline :overline :strike-through
	   :box :inverse-video))
	(differs nil))
    (while (and attrs (not differs))
      (let* ((attr (pop attrs))
	     (attr-val (face-attribute face attr frame t)))
	(when (and
	       (not (eq attr-val 'unspecified))
	       (display-supports-face-attributes-p (list attr attr-val)
						   frame))
	  (setq differs attr))))
    differs))


(defun face-nontrivial-p (face &optional frame)
  "True if face FACE has some non-nil attribute.
If the optional argument FRAME is given, report on face FACE in that frame.
If FRAME is t, report on the defaults for face FACE (for new frames).
If FRAME is omitted or nil, use the selected frame."
  (not (internal-lisp-face-empty-p face frame)))



;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Setting face attributes from X resources.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(defcustom face-x-resources
  '((:family (".attributeFamily" . "Face.AttributeFamily"))
    (:width (".attributeWidth" . "Face.AttributeWidth"))
    (:height (".attributeHeight" . "Face.AttributeHeight"))
    (:weight (".attributeWeight" . "Face.AttributeWeight"))
    (:slant (".attributeSlant" . "Face.AttributeSlant"))
    (:foreground (".attributeForeground" . "Face.AttributeForeground"))
    (:background (".attributeBackground" . "Face.AttributeBackground"))
    (:overline (".attributeOverline" . "Face.AttributeOverline"))
    (:strike-through (".attributeStrikeThrough" . "Face.AttributeStrikeThrough"))
    (:box (".attributeBox" . "Face.AttributeBox"))
    (:underline (".attributeUnderline" . "Face.AttributeUnderline"))
    (:inverse-video (".attributeInverse" . "Face.AttributeInverse"))
    (:stipple
     (".attributeStipple" . "Face.AttributeStipple")
     (".attributeBackgroundPixmap" . "Face.AttributeBackgroundPixmap"))
    (:bold (".attributeBold" . "Face.AttributeBold"))
    (:italic (".attributeItalic" . "Face.AttributeItalic"))
    (:font (".attributeFont" . "Face.AttributeFont"))
    (:inherit (".attributeInherit" . "Face.AttributeInherit")))
  "*List of X resources and classes for face attributes.
Each element has the form (ATTRIBUTE ENTRY1 ENTRY2...) where ATTRIBUTE is
the name of a face attribute, and each ENTRY is a cons of the form
\(RESOURCE . CLASS) with RESOURCE being the resource and CLASS being the
X resource class for the attribute."
  :type '(repeat (cons symbol (repeat (cons string string))))
  :group 'faces)


(defun set-face-attribute-from-resource (face attribute resource class frame)
  "Set FACE's ATTRIBUTE from X resource RESOURCE, class CLASS on FRAME.
Value is the attribute value specified by the resource, or nil
if not present.  This function displays a message if the resource
specifies an invalid attribute."
  (let* ((face-name (face-name face))
	 (value (internal-face-x-get-resource (concat face-name resource)
					      class frame)))
    (when value
      (condition-case ()
	  (internal-set-lisp-face-attribute-from-resource
	   face attribute (downcase value) frame)
	(error
	 (message "Face %s, frame %s: invalid attribute %s %s from X resource"
		  face-name frame attribute value))))
    value))


(defun set-face-attributes-from-resources (face frame)
  "Set attributes of FACE from X resources for FRAME."
  (when (memq (framep frame) '(x w32 mac))
    (dolist (definition face-x-resources)
      (let ((attribute (car definition)))
	(dolist (entry (cdr definition))
	  (set-face-attribute-from-resource face attribute (car entry)
					    (cdr entry) frame))))))


(defun make-face-x-resource-internal (face &optional frame)
  "Fill frame-local FACE on FRAME from X resources.
FRAME nil or not specified means do it for all frames."
  (if (null frame)
      (dolist (frame (frame-list))
	(set-face-attributes-from-resources face frame))
    (set-face-attributes-from-resources face frame)))



;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Retrieving face attributes.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(defun face-name (face)
  "Return the name of face FACE."
  (symbol-name (check-face face)))


(defun face-attribute (face attribute &optional frame inherit)
  "Return the value of FACE's ATTRIBUTE on FRAME.
If the optional argument FRAME is given, report on face FACE in that frame.
If FRAME is t, report on the defaults for face FACE (for new frames).
If FRAME is omitted or nil, use the selected frame.

If INHERIT is nil, only attributes directly defined by FACE are considered,
  so the return value may be `unspecified', or a relative value.
If INHERIT is non-nil, FACE's definition of ATTRIBUTE is merged with the
  faces specified by its `:inherit' attribute; however the return value
  may still be `unspecified' or relative.
If INHERIT is a face or a list of faces, then the result is further merged
  with that face (or faces), until it becomes specified and absolute.

To ensure that the return value is always specified and absolute, use a
value of `default' for INHERIT; this will resolve any unspecified or
relative values by merging with the `default' face (which is always
completely specified)."
  (let ((value (internal-get-lisp-face-attribute face attribute frame)))
    (when (and inherit (face-attribute-relative-p attribute value))
      ;; VALUE is relative, so merge with inherited faces
      (let ((inh-from (face-attribute face :inherit frame)))
	(unless (or (null inh-from) (eq inh-from 'unspecified))
          (condition-case nil
              (setq value
                    (face-attribute-merged-with attribute value inh-from frame))
            ;; The `inherit' attribute may point to non existent faces.
            (error nil)))))
    (when (and inherit
	       (not (eq inherit t))
	       (face-attribute-relative-p attribute value))
      ;; We should merge with INHERIT as well
      (setq value (face-attribute-merged-with attribute value inherit frame)))
    value))

(defun face-attribute-merged-with (attribute value faces &optional frame)
  "Merges ATTRIBUTE, initially VALUE, with faces from FACES until absolute.
FACES may be either a single face or a list of faces.
\[This is an internal function.]"
  (cond ((not (face-attribute-relative-p attribute value))
	 value)
	((null faces)
	 value)
	((consp faces)
	 (face-attribute-merged-with
	  attribute
	  (face-attribute-merged-with attribute value (car faces) frame)
	  (cdr faces)
	  frame))
	(t
	 (merge-face-attribute attribute
			       value
			       (face-attribute faces attribute frame t)))))


(defmacro face-attribute-specified-or (value &rest body)
  "Return VALUE, unless it's `unspecified', in which case evaluate BODY and return the result."
  (let ((temp (make-symbol "value")))
    `(let ((,temp ,value))
       (if (not (eq ,temp 'unspecified))
	   ,temp
	 ,@body))))

(defun face-foreground (face &optional frame inherit)
  "Return the foreground color name of FACE, or nil if unspecified.
If the optional argument FRAME is given, report on face FACE in that frame.
If FRAME is t, report on the defaults for face FACE (for new frames).
If FRAME is omitted or nil, use the selected frame.

If INHERIT is nil, only a foreground color directly defined by FACE is
  considered, so the return value may be nil.
If INHERIT is t, and FACE doesn't define a foreground color, then any
  foreground color that FACE inherits through its `:inherit' attribute
  is considered as well; however the return value may still be nil.
If INHERIT is a face or a list of faces, then it is used to try to
  resolve an unspecified foreground color.

To ensure that a valid color is always returned, use a value of
`default' for INHERIT; this will resolve any unspecified values by
merging with the `default' face (which is always completely specified)."
  (face-attribute-specified-or (face-attribute face :foreground frame inherit)
			       nil))

(defun face-background (face &optional frame inherit)
  "Return the background color name of FACE, or nil if unspecified.
If the optional argument FRAME is given, report on face FACE in that frame.
If FRAME is t, report on the defaults for face FACE (for new frames).
If FRAME is omitted or nil, use the selected frame.

If INHERIT is nil, only a background color directly defined by FACE is
  considered, so the return value may be nil.
If INHERIT is t, and FACE doesn't define a background color, then any
  background color that FACE inherits through its `:inherit' attribute
  is considered as well; however the return value may still be nil.
If INHERIT is a face or a list of faces, then it is used to try to
  resolve an unspecified background color.

To ensure that a valid color is always returned, use a value of
`default' for INHERIT; this will resolve any unspecified values by
merging with the `default' face (which is always completely specified)."
  (face-attribute-specified-or (face-attribute face :background frame inherit)
			       nil))

(defun face-stipple (face &optional frame inherit)
 "Return the stipple pixmap name of FACE, or nil if unspecified.
If the optional argument FRAME is given, report on face FACE in that frame.
If FRAME is t, report on the defaults for face FACE (for new frames).
If FRAME is omitted or nil, use the selected frame.

If INHERIT is nil, only a stipple directly defined by FACE is
  considered, so the return value may be nil.
If INHERIT is t, and FACE doesn't define a stipple, then any stipple
  that FACE inherits through its `:inherit' attribute is considered as
  well; however the return value may still be nil.
If INHERIT is a face or a list of faces, then it is used to try to
  resolve an unspecified stipple.

To ensure that a valid stipple or nil is always returned, use a value of
`default' for INHERIT; this will resolve any unspecified values by merging
with the `default' face (which is always completely specified)."
  (face-attribute-specified-or (face-attribute face :stipple frame inherit)
			       nil))


(defalias 'face-background-pixmap 'face-stipple)


(defun face-underline-p (face &optional frame)
 "Return non-nil if FACE is underlined.
If the optional argument FRAME is given, report on face FACE in that frame.
If FRAME is t, report on the defaults for face FACE (for new frames).
If FRAME is omitted or nil, use the selected frame."
 (eq (face-attribute face :underline frame) t))


(defun face-inverse-video-p (face &optional frame)
 "Return non-nil if FACE is in inverse video on FRAME.
If the optional argument FRAME is given, report on face FACE in that frame.
If FRAME is t, report on the defaults for face FACE (for new frames).
If FRAME is omitted or nil, use the selected frame."
 (eq (face-attribute face :inverse-video frame) t))


(defun face-bold-p (face &optional frame)
  "Return non-nil if the font of FACE is bold on FRAME.
If the optional argument FRAME is given, report on face FACE in that frame.
If FRAME is t, report on the defaults for face FACE (for new frames).
If FRAME is omitted or nil, use the selected frame.
Use `face-attribute' for finer control."
  (let ((bold (face-attribute face :weight frame)))
    (memq bold '(semi-bold bold extra-bold ultra-bold))))


(defun face-italic-p (face &optional frame)
  "Return non-nil if the font of FACE is italic on FRAME.
If the optional argument FRAME is given, report on face FACE in that frame.
If FRAME is t, report on the defaults for face FACE (for new frames).
If FRAME is omitted or nil, use the selected frame.
Use `face-attribute' for finer control."
  (let ((italic (face-attribute face :slant frame)))
    (memq italic '(italic oblique))))



;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Face documentation.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(defun face-documentation (face)
  "Get the documentation string for FACE.
If FACE is a face-alias, get the documentation for the target face."
  (let ((alias (get face 'face-alias))
        doc)
    (if alias
        (progn
          (setq doc (get alias 'face-documentation))
          (format "%s is an alias for the face `%s'.%s" face alias
                  (if doc (format "\n%s" doc)
                    "")))
      (get face 'face-documentation))))


(defun set-face-documentation (face string)
  "Set the documentation string for FACE to STRING."
  ;; Perhaps the text should go in DOC.
  (put face 'face-documentation (purecopy string)))


(defalias 'face-doc-string 'face-documentation)
(defalias 'set-face-doc-string 'set-face-documentation)



;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Setting face attributes.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


(defvar inhibit-face-set-after-frame-default nil
  "If non-nil, that tells `face-set-after-frame-default' to do nothing.")

(defun set-face-attribute (face frame &rest args)
  "Set attributes of FACE on FRAME from ARGS.

FRAME nil means change attributes on all frames.  FRAME t means change
the default for new frames (this is done automatically each time an
attribute is changed on all frames).

ARGS must come in pairs ATTRIBUTE VALUE.  ATTRIBUTE must be a valid
face attribute name. All attributes can be set to `unspecified';
this fact is not further mentioned below.

The following attributes are recognized:

`:family'

VALUE must be a string specifying the font family, e.g. ``courier'',
or a fontset alias name.  If a font family is specified, wild-cards `*'
and `?' are allowed.

`:width'

VALUE specifies the relative proportionate width of the font to use.
It must be one of the symbols `ultra-condensed', `extra-condensed',
`condensed', `semi-condensed', `normal', `semi-expanded', `expanded',
`extra-expanded', or `ultra-expanded'.

`:height'

VALUE must be either an integer specifying the height of the font to use
in 1/10 pt, a floating point number specifying the amount by which to
scale any underlying face, or a function, which is called with the old
height (from the underlying face), and should return the new height.

`:weight'

VALUE specifies the weight of the font to use.  It must be one of the
symbols `ultra-bold', `extra-bold', `bold', `semi-bold', `normal',
`semi-light', `light', `extra-light', `ultra-light'.

`:slant'

VALUE specifies the slant of the font to use.  It must be one of the
symbols `italic', `oblique', `normal', `reverse-italic', or
`reverse-oblique'.

`:foreground', `:background'

VALUE must be a color name, a string.

`:underline'

VALUE specifies whether characters in FACE should be underlined.  If
VALUE is t, underline with foreground color of the face.  If VALUE is
a string, underline with that color.  If VALUE is nil, explicitly
don't underline.

`:overline'

VALUE specifies whether characters in FACE should be overlined.  If
VALUE is t, overline with foreground color of the face.  If VALUE is a
string, overline with that color.  If VALUE is nil, explicitly don't
overline.

`:strike-through'

VALUE specifies whether characters in FACE should be drawn with a line
striking through them.  If VALUE is t, use the foreground color of the
face.  If VALUE is a string, strike-through with that color.  If VALUE
is nil, explicitly don't strike through.

`:box'

VALUE specifies whether characters in FACE should have a box drawn
around them.  If VALUE is nil, explicitly don't draw boxes.  If
VALUE is t, draw a box with lines of width 1 in the foreground color
of the face.  If VALUE is a string, the string must be a color name,
and the box is drawn in that color with a line width of 1.  Otherwise,
VALUE must be a property list of the form `(:line-width WIDTH
:color COLOR :style STYLE)'.  If a keyword/value pair is missing from
the property list, a default value will be used for the value, as
specified below.  WIDTH specifies the width of the lines to draw; it
defaults to 1.  If WIDTH is negative, the absolute value is the width
of the lines, and draw top/bottom lines inside the characters area,
not around it.  COLOR is the name of the color to draw in, default is
the foreground color of the face for simple boxes, and the background
color of the face for 3D boxes.  STYLE specifies whether a 3D box
should be draw.  If STYLE is `released-button', draw a box looking
like a released 3D button.  If STYLE is `pressed-button' draw a box
that appears like a pressed button.  If STYLE is nil, the default if
the property list doesn't contain a style specification, draw a 2D
box.

`:inverse-video'

VALUE specifies whether characters in FACE should be displayed in
inverse video.  VALUE must be one of t or nil.

`:stipple'

If VALUE is a string, it must be the name of a file of pixmap data.
The directories listed in the `x-bitmap-file-path' variable are
searched.  Alternatively, VALUE may be a list of the form (WIDTH
HEIGHT DATA) where WIDTH and HEIGHT are the size in pixels, and DATA
is a string containing the raw bits of the bitmap.  VALUE nil means
explicitly don't use a stipple pattern.

For convenience, attributes `:family', `:width', `:height', `:weight',
and `:slant' may also be set in one step from an X font name:

`:font'

Set font-related face attributes from VALUE.  VALUE must be a valid
XLFD font name.  If it is a font name pattern, the first matching font
will be used.

For compatibility with Emacs 20, keywords `:bold' and `:italic' can
be used to specify that a bold or italic font should be used.  VALUE
must be t or nil in that case.  A value of `unspecified' is not allowed.

`:inherit'

VALUE is the name of a face from which to inherit attributes, or a list
of face names.  Attributes from inherited faces are merged into the face
like an underlying face would be, with higher priority than underlying faces."
  (let ((where (if (null frame) 0 frame)))
    (setq args (purecopy args))
    ;; If we set the new-frame defaults, this face is modified outside Custom.
    (if (memq where '(0 t))
	(put (or (get face 'face-alias) face) 'face-modified t))
    (while args
      ;; Don't recursively set the attributes from the frame's font param
      ;; when we update the frame's font param fro the attributes.
      (let ((inhibit-face-set-after-frame-default t))
	(internal-set-lisp-face-attribute face (car args)
					  (purecopy (cadr args))
					  where))
      (setq args (cdr (cdr args))))))


(defun make-face-bold (face &optional frame noerror)
  "Make the font of FACE be bold, if possible.
FRAME nil or not specified means change face on all frames.
Argument NOERROR is ignored and retained for compatibility.
Use `set-face-attribute' for finer control of the font weight."
  (interactive (list (read-face-name "Make which face bold")))
  (set-face-attribute face frame :weight 'bold))


(defun make-face-unbold (face &optional frame noerror)
  "Make the font of FACE be non-bold, if possible.
FRAME nil or not specified means change face on all frames.
Argument NOERROR is ignored and retained for compatibility."
  (interactive (list (read-face-name "Make which face non-bold")))
  (set-face-attribute face frame :weight 'normal))


(defun make-face-italic (face &optional frame noerror)
  "Make the font of FACE be italic, if possible.
FRAME nil or not specified means change face on all frames.
Argument NOERROR is ignored and retained for compatibility.
Use `set-face-attribute' for finer control of the font slant."
  (interactive (list (read-face-name "Make which face italic")))
  (set-face-attribute face frame :slant 'italic))


(defun make-face-unitalic (face &optional frame noerror)
  "Make the font of FACE be non-italic, if possible.
FRAME nil or not specified means change face on all frames.
Argument NOERROR is ignored and retained for compatibility."
  (interactive (list (read-face-name "Make which face non-italic")))
  (set-face-attribute face frame :slant 'normal))


(defun make-face-bold-italic (face &optional frame noerror)
  "Make the font of FACE be bold and italic, if possible.
FRAME nil or not specified means change face on all frames.
Argument NOERROR is ignored and retained for compatibility.
Use `set-face-attribute' for finer control of font weight and slant."
  (interactive (list (read-face-name "Make which face bold-italic")))
  (set-face-attribute face frame :weight 'bold :slant 'italic))


(defun set-face-font (face font &optional frame)
  "Change font-related attributes of FACE to those of FONT (a string).
FRAME nil or not specified means change face on all frames.
This sets the attributes `:family', `:width', `:height', `:weight',
and `:slant'.  When called interactively, prompt for the face and font."
  (interactive (read-face-and-attribute :font))
  (set-face-attribute face frame :font font))


;; Implementation note: Emulating gray background colors with a
;; stipple pattern is now part of the face realization process, and is
;; done in C depending on the frame on which the face is realized.

(defun set-face-background (face color &optional frame)
  "Change the background color of face FACE to COLOR (a string).
FRAME nil or not specified means change face on all frames.
COLOR can be a system-defined color name (see `list-colors-display')
or a hex spec of the form #RRGGBB.
When called interactively, prompts for the face and color."
  (interactive (read-face-and-attribute :background))
  (set-face-attribute face frame :background (or color 'unspecified)))


(defun set-face-foreground (face color &optional frame)
  "Change the foreground color of face FACE to COLOR (a string).
FRAME nil or not specified means change face on all frames.
COLOR can be a system-defined color name (see `list-colors-display')
or a hex spec of the form #RRGGBB.
When called interactively, prompts for the face and color."
  (interactive (read-face-and-attribute :foreground))
  (set-face-attribute face frame :foreground (or color 'unspecified)))


(defun set-face-stipple (face stipple &optional frame)
  "Change the stipple pixmap of face FACE to STIPPLE.
FRAME nil or not specified means change face on all frames.
STIPPLE should be a string, the name of a file of pixmap data.
The directories listed in the `x-bitmap-file-path' variable are searched.

Alternatively, STIPPLE may be a list of the form (WIDTH HEIGHT DATA)
where WIDTH and HEIGHT are the size in pixels,
and DATA is a string, containing the raw bits of the bitmap."
  (interactive (read-face-and-attribute :stipple))
  (set-face-attribute face frame :stipple (or stipple 'unspecified)))


(defun set-face-underline-p (face underline &optional frame)
  "Specify whether face FACE is underlined.
UNDERLINE nil means FACE explicitly doesn't underline.
UNDERLINE non-nil means FACE explicitly does underlining
with the same of the foreground color.
If UNDERLINE is a string, underline with the color named UNDERLINE.
FRAME nil or not specified means change face on all frames.
Use `set-face-attribute' to ``unspecify'' underlining."
  (interactive
   (let ((list (read-face-and-attribute :underline)))
     (list (car list) (eq (car (cdr list)) t))))
  (set-face-attribute face frame :underline underline))

(define-obsolete-function-alias 'set-face-underline
                                'set-face-underline-p "22.1")


(defun set-face-inverse-video-p (face inverse-video-p &optional frame)
  "Specify whether face FACE is in inverse video.
INVERSE-VIDEO-P non-nil means FACE displays explicitly in inverse video.
INVERSE-VIDEO-P nil means FACE explicitly is not in inverse video.
FRAME nil or not specified means change face on all frames.
Use `set-face-attribute' to ``unspecify'' the inverse video attribute."
  (interactive
   (let ((list (read-face-and-attribute :inverse-video)))
     (list (car list) (eq (car (cdr list)) t))))
  (set-face-attribute face frame :inverse-video inverse-video-p))


(defun set-face-bold-p (face bold-p &optional frame)
  "Specify whether face FACE is bold.
BOLD-P non-nil means FACE should explicitly display bold.
BOLD-P nil means FACE should explicitly display non-bold.
FRAME nil or not specified means change face on all frames.
Use `set-face-attribute' or `modify-face' for finer control."
  (if (null bold-p)
      (make-face-unbold face frame)
    (make-face-bold face frame)))


(defun set-face-italic-p (face italic-p &optional frame)
  "Specify whether face FACE is italic.
ITALIC-P non-nil means FACE should explicitly display italic.
ITALIC-P nil means FACE should explicitly display non-italic.
FRAME nil or not specified means change face on all frames.
Use `set-face-attribute' or `modify-face' for finer control."
  (if (null italic-p)
      (make-face-unitalic face frame)
    (make-face-italic face frame)))


(defalias 'set-face-background-pixmap 'set-face-stipple)


(defun invert-face (face &optional frame)
  "Swap the foreground and background colors of FACE.
If FRAME is omitted or nil, it means change face on all frames.
If FACE specifies neither foreground nor background color,
set its foreground and background to the background and foreground
of the default face.  Value is FACE."
  (interactive (list (read-face-name "Invert face")))
  (let ((fg (face-attribute face :foreground frame))
	(bg (face-attribute face :background frame)))
    (if (not (and (eq fg 'unspecified) (eq bg 'unspecified)))
	(set-face-attribute face frame :foreground bg :background fg)
      (set-face-attribute face frame
			  :foreground
			  (face-attribute 'default :background frame)
			  :background
			  (face-attribute 'default :foreground frame))))
  face)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Interactively modifying faces.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(defun read-face-name (prompt &optional string-describing-default multiple)
  "Read a face, defaulting to the face or faces on the char after point.
If it has the property `read-face-name', that overrides the `face' property.
PROMPT should be a string that describes what the caller will do with the face;
it should not end in a space.
STRING-DESCRIBING-DEFAULT should describe what default the caller will use if
the user just types RET; you can omit it.
If MULTIPLE is non-nil, return a list of faces (possibly only one).
Otherwise, return a single face."
  (let ((faceprop (or (get-char-property (point) 'read-face-name)
		      (get-char-property (point) 'face)))
        (aliasfaces nil)
        (nonaliasfaces nil)
	faces)
    ;; Try to get a face name from the buffer.
    (if (memq (intern-soft (thing-at-point 'symbol)) (face-list))
	(setq faces (list (intern-soft (thing-at-point 'symbol)))))
    ;; Add the named faces that the `face' property uses.
    (if (and (listp faceprop)
	     ;; Don't treat an attribute spec as a list of faces.
	     (not (keywordp (car faceprop)))
	     (not (memq (car faceprop) '(foreground-color background-color))))
	(dolist (f faceprop)
	  (if (symbolp f)
	      (push f faces)))
      (if (symbolp faceprop)
	  (push faceprop faces)))
    (delete-dups faces)

    ;; Build up the completion tables.
    (mapatoms (lambda (s)
                (if (custom-facep s)
                    (if (get s 'face-alias)
                        (push (symbol-name s) aliasfaces)
                      (push (symbol-name s) nonaliasfaces)))))

    ;; If we only want one, and the default is more than one,
    ;; discard the unwanted ones now.
    (unless multiple
      (if faces
	  (setq faces (list (car faces)))))
    (require 'crm)
    (let* ((input
	    ;; Read the input.
	    (completing-read-multiple
	     (if (or faces string-describing-default)
		 (format "%s (default %s): " prompt
			 (if faces (mapconcat 'symbol-name faces ",")
			   string-describing-default))
	       (format "%s: " prompt))
	     (complete-in-turn nonaliasfaces aliasfaces)
	     nil t nil nil
	     (if faces (mapconcat 'symbol-name faces ","))))
	   ;; Canonicalize the output.
	   (output
	    (cond ((or (equal input "") (equal input '("")))
		   faces)
		  ((stringp input)
		   (mapcar 'intern (split-string input ", *" t)))
		  ((listp input)
		   (mapcar 'intern input))
		  (input))))
      ;; Return either a list of faces or just one face.
      (if multiple
	  output
	(car output)))))


(defun face-valid-attribute-values (attribute &optional frame)
  "Return valid values for face attribute ATTRIBUTE.
The optional argument FRAME is used to determine available fonts
and colors.  If it is nil or not specified, the selected frame is
used.  Value is an alist of (NAME . VALUE) if ATTRIBUTE expects a value
out of a set of discrete values.  Value is `integerp' if ATTRIBUTE expects
an integer value."
  (let ((valid
         (case attribute
           (:family
            (if window-system
                (mapcar #'(lambda (x) (cons (car x) (car x)))
                        (x-font-family-list))
	      ;; Only one font on TTYs.
	      (list (cons "default" "default"))))
           ((:width :weight :slant :inverse-video)
            (mapcar #'(lambda (x) (cons (symbol-name x) x))
                    (internal-lisp-face-attribute-values attribute)))
           ((:underline :overline :strike-through :box)
            (if window-system
                (nconc (mapcar #'(lambda (x) (cons (symbol-name x) x))
                               (internal-lisp-face-attribute-values attribute))
                       (mapcar #'(lambda (c) (cons c c))
                               (defined-colors frame)))
	      (mapcar #'(lambda (x) (cons (symbol-name x) x))
		      (internal-lisp-face-attribute-values attribute))))
           ((:foreground :background)
            (mapcar #'(lambda (c) (cons c c))
                    (defined-colors frame)))
           ((:height)
            'integerp)
           (:stipple
            (and (memq window-system '(x w32 mac))
                 (mapcar #'list
                         (apply #'nconc
                                (mapcar (lambda (dir)
                                          (and (file-readable-p dir)
                                               (file-directory-p dir)
                                               (directory-files dir)))
                                        x-bitmap-file-path)))))
           (:inherit
            (cons '("none" . nil)
                  (mapcar #'(lambda (c) (cons (symbol-name c) c))
                          (face-list))))
           (t
            (error "Internal error")))))
    (if (and (listp valid) (not (memq attribute '(:inherit))))
	(nconc (list (cons "unspecified" 'unspecified)) valid)
      valid)))


(defvar face-attribute-name-alist
  '((:family . "font family")
    (:width . "character set width")
    (:height . "height in 1/10 pt")
    (:weight . "weight")
    (:slant . "slant")
    (:underline . "underline")
    (:overline . "overline")
    (:strike-through . "strike-through")
    (:box . "box")
    (:inverse-video . "inverse-video display")
    (:foreground . "foreground color")
    (:background . "background color")
    (:stipple . "background stipple")
    (:inherit . "inheritance"))
  "An alist of descriptive names for face attributes.
Each element has the form (ATTRIBUTE-NAME . DESCRIPTION) where
ATTRIBUTE-NAME is a face attribute name (a keyword symbol), and
DESCRIPTION is a descriptive name for ATTRIBUTE-NAME.")


(defun face-descriptive-attribute-name (attribute)
  "Return a descriptive name for ATTRIBUTE."
  (cdr (assq attribute face-attribute-name-alist)))


(defun face-read-string (face default name &optional completion-alist)
  "Interactively read a face attribute string value.
FACE is the face whose attribute is read.  If non-nil, DEFAULT is the
default string to return if no new value is entered.  NAME is a
descriptive name of the attribute for prompting.  COMPLETION-ALIST is an
alist of valid values, if non-nil.

Entering nothing accepts the default string DEFAULT.
Value is the new attribute value."
  ;; Capitalize NAME (we don't use `capitalize' because that capitalizes
  ;; each word in a string separately).
  (setq name (concat (upcase (substring name 0 1)) (substring name 1)))
  (let* ((completion-ignore-case t)
	 (value (completing-read
		 (if default
		     (format "%s for face `%s' (default %s): "
			     name face default)
		   (format "%s for face `%s': " name face))
		 completion-alist nil nil nil nil default)))
    (if (equal value "") default value)))


(defun face-read-integer (face default name)
  "Interactively read an integer face attribute value.
FACE is the face whose attribute is read.  DEFAULT is the default
value to return if no new value is entered.  NAME is a descriptive
name of the attribute for prompting.  Value is the new attribute value."
  (let ((new-value
	 (face-read-string face
			   (format "%s" default)
			   name
			   (list (cons "unspecified" 'unspecified)))))
    (cond ((equal new-value "unspecified")
	   'unspecified)
	  ((member new-value '("unspecified-fg" "unspecified-bg"))
	   new-value)
	  (t
	   (string-to-number new-value)))))


(defun read-face-attribute (face attribute &optional frame)
  "Interactively read a new value for FACE's ATTRIBUTE.
Optional argument FRAME nil or unspecified means read an attribute value
of a global face.  Value is the new attribute value."
  (let* ((old-value (face-attribute face attribute frame))
	 (attribute-name (face-descriptive-attribute-name attribute))
	 (valid (face-valid-attribute-values attribute frame))
	 new-value)
    ;; Represent complex attribute values as strings by printing them
    ;; out.  Stipple can be a vector; (WIDTH HEIGHT DATA).  Box can be
    ;; a list `(:width WIDTH :color COLOR)' or `(:width WIDTH :shadow
    ;; SHADOW)'.
    (when (and (or (eq attribute :stipple)
		   (eq attribute :box))
	       (or (consp old-value)
		   (vectorp old-value)))
      (setq old-value (prin1-to-string old-value)))
    (cond ((listp valid)
	   (let ((default
		   (or (car (rassoc old-value valid))
		       (format "%s" old-value))))
	     (setq new-value
		   (face-read-string face default attribute-name valid))
	     (if (equal new-value default)
		 ;; Nothing changed, so don't bother with all the stuff
		 ;; below.  In particular, this avoids a non-tty color
		 ;; from being canonicalized for a tty when the user
		 ;; just uses the default.
		 (setq new-value old-value)
	       ;; Terminal frames can support colors that don't appear
	       ;; explicitly in VALID, using color approximation code
	       ;; in tty-colors.el.
	       (when (and (memq attribute '(:foreground :background))
			  (not (memq window-system '(x w32 mac)))
			  (not (member new-value
				       '("unspecified"
					 "unspecified-fg" "unspecified-bg"))))
		 (setq new-value (car (tty-color-desc new-value frame))))
	       (when (assoc new-value valid)
		 (setq new-value (cdr (assoc new-value valid)))))))
	  ((eq valid 'integerp)
	   (setq new-value (face-read-integer face old-value attribute-name)))
	  (t (error "Internal error")))
    ;; Convert stipple and box value text we read back to a list or
    ;; vector if it looks like one.  This makes the assumption that a
    ;; pixmap file name won't start with an open-paren.
    (when (and (or (eq attribute :stipple)
		   (eq attribute :box))
	       (stringp new-value)
	       (string-match "^[[(]" new-value))
      (setq new-value (read new-value)))
    new-value))


(defun read-face-font (face &optional frame)
  "Read the name of a font for FACE on FRAME.
If optional argument FRAME is nil or omitted, use the selected frame."
  (let ((completion-ignore-case t))
    (completing-read (format "Set font attributes of face `%s' from font: " face)
		     (x-list-fonts "*" nil frame))))


(defun read-all-face-attributes (face &optional frame)
  "Interactively read all attributes for FACE.
If optional argument FRAME is nil or omitted, use the selected frame.
Value is a property list of attribute names and new values."
  (let (result)
    (dolist (attribute face-attribute-name-alist result)
      (setq result (cons (car attribute)
			 (cons (read-face-attribute face (car attribute) frame)
			       result))))))

(defun modify-face (&optional face foreground background stipple
			      bold-p italic-p underline inverse-p frame)
  "Modify attributes of faces interactively.
If optional argument FRAME is nil or omitted, modify the face used
for newly created frame, i.e. the global face.
For non-interactive use, `set-face-attribute' is preferred.
When called from Lisp, if FACE is nil, all arguments but FRAME are ignored
and the face and its settings are obtained by querying the user."
  (interactive)
  (if face
      (set-face-attribute face frame
			  :foreground (or foreground 'unspecified)
			  :background (or background 'unspecified)
			  :stipple stipple
			  :bold bold-p
			  :italic italic-p
			  :underline underline
			  :inverse-video inverse-p)
    (setq face (read-face-name "Modify face"))
    (apply #'set-face-attribute face frame
	   (read-all-face-attributes face frame))))

(defun read-face-and-attribute (attribute &optional frame)
  "Read face name and face attribute value.
ATTRIBUTE is the attribute whose new value is read.
FRAME nil or unspecified means read attribute value of global face.
Value is a list (FACE NEW-VALUE) where FACE is the face read
\(a symbol), and NEW-VALUE is value read."
  (cond ((eq attribute :font)
	 (let* ((prompt "Set font-related attributes of face")
		(face (read-face-name prompt))
		(font (read-face-font face frame)))
	   (list face font)))
	(t
	 (let* ((attribute-name (face-descriptive-attribute-name attribute))
		(prompt (format "Set %s of face" attribute-name))
		(face (read-face-name prompt))
		(new-value (read-face-attribute face attribute frame)))
	   (list face new-value)))))



;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Listing faces.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(defvar list-faces-sample-text
  "abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ"
  "*Text string to display as the sample text for `list-faces-display'.")


;; The name list-faces would be more consistent, but let's avoid a
;; conflict with Lucid, which uses that name differently.

(defvar help-xref-stack)
(defun list-faces-display (&optional regexp)
  "List all faces, using the same sample text in each.
The sample text is a string that comes from the variable
`list-faces-sample-text'.

If REGEXP is non-nil, list only those faces with names matching
this regular expression.  When called interactively with a prefix
arg, prompt for a regular expression."
  (interactive (list (and current-prefix-arg
                          (read-string "List faces matching regexp: "))))
  (let ((all-faces (zerop (length regexp)))
	(frame (selected-frame))
	(max-length 0)
	faces line-format
	disp-frame window face-name)
    ;; We filter and take the max length in one pass
    (setq faces
	  (delq nil
		(mapcar (lambda (f)
			  (let ((s (symbol-name f)))
			    (when (or all-faces (string-match regexp s))
			      (setq max-length (max (length s) max-length))
			      f)))
			(sort (face-list) #'string-lessp))))
    (unless faces
      (error "No faces matching \"%s\"" regexp))
    (setq max-length (1+ max-length)
	  line-format (format "%%-%ds" max-length))
    (with-output-to-temp-buffer "*Faces*"
      (save-excursion
	(set-buffer standard-output)
	(setq truncate-lines t)
	(insert
	 (substitute-command-keys
	  (concat
	   "Use "
	   (if (display-mouse-p) "\\[help-follow-mouse] or ")
	   "\\[help-follow] on a face name to customize it\n"
	   "or on its sample text for a description of the face.\n\n")))
	(setq help-xref-stack nil)
	(dolist (face faces)
	  (setq face-name (symbol-name face))
	  (insert (format line-format face-name))
	  ;; Hyperlink to a customization buffer for the face.  Using
	  ;; the help xref mechanism may not be the best way.
	  (save-excursion
	    (save-match-data
	      (search-backward face-name)
	      (setq help-xref-stack-item `(list-faces-display ,regexp))
	      (help-xref-button 0 'help-customize-face face)))
	  (let ((beg (point))
		(line-beg (line-beginning-position)))
	    (insert list-faces-sample-text)
	    ;; Hyperlink to a help buffer for the face.
	    (save-excursion
	      (save-match-data
		(search-backward list-faces-sample-text)
		(help-xref-button 0 'help-face face)))
	    (insert "\n")
	    (put-text-property beg (1- (point)) 'face face)
	    ;; Make all face commands default to the proper face
	    ;; anywhere in the line.
	    (put-text-property line-beg (1- (point)) 'read-face-name face)
	    ;; If the sample text has multiple lines, line up all of them.
	    (goto-char beg)
	    (forward-line 1)
	    (while (not (eobp))
	      (insert-char ?\s max-length)
	      (forward-line 1))))
	(goto-char (point-min)))
      (print-help-return-message))
    ;; If the *Faces* buffer appears in a different frame,
    ;; copy all the face definitions from FRAME,
    ;; so that the display will reflect the frame that was selected.
    (setq window (get-buffer-window (get-buffer "*Faces*") t))
    (setq disp-frame (if window (window-frame window)
		       (car (frame-list))))
    (or (eq frame disp-frame)
	(let ((faces (face-list)))
	  (while faces
	    (copy-face (car faces) (car faces) frame disp-frame)
	    (setq faces (cdr faces)))))))


(defun describe-face (face &optional frame)
  "Display the properties of face FACE on FRAME.
Interactively, FACE defaults to the faces of the character after point
and FRAME defaults to the selected frame.

If the optional argument FRAME is given, report on face FACE in that frame.
If FRAME is t, report on the defaults for face FACE (for new frames).
If FRAME is omitted or nil, use the selected frame."
  (interactive (list (read-face-name "Describe face" "= `default' face" t)))
  (let* ((attrs '((:family . "Family")
		  (:width . "Width")
		  (:height . "Height")
		  (:weight . "Weight")
		  (:slant . "Slant")
		  (:foreground . "Foreground")
		  (:background . "Background")
		  (:underline . "Underline")
		  (:overline . "Overline")
		  (:strike-through . "Strike-through")
		  (:box . "Box")
		  (:inverse-video . "Inverse")
		  (:stipple . "Stipple")
		  (:font . "Font or fontset")
		  (:inherit . "Inherit")))
	(max-width (apply #'max (mapcar #'(lambda (x) (length (cdr x)))
					attrs))))
    (help-setup-xref (list #'describe-face face) (interactive-p))
    (unless face
      (setq face 'default))
    (if (not (listp face))
	(setq face (list face)))
    (with-output-to-temp-buffer (help-buffer)
      (save-excursion
	(set-buffer standard-output)
	(dolist (f face)
	  (insert "Face: " (symbol-name f))
	  (if (not (facep f))
	      (insert "   undefined face.\n")
	    (let ((customize-label "customize this face")
		  file-name)
	      (insert (concat " (" (propertize "sample" 'font-lock-face f) ")"))
	      (princ (concat " (" customize-label ")\n"))
	      (insert "Documentation: "
		      (or (face-documentation f)
			  "Not documented as a face.")
		      "\n")
	      (with-current-buffer standard-output
		(save-excursion
		  (re-search-backward
		   (concat "\\(" customize-label "\\)") nil t)
		  (help-xref-button 1 'help-customize-face f)))
	      ;; The next 4 sexps are copied from describe-function-1
	      ;; and simplified.
	      (setq file-name (symbol-file f 'defface))
	      (setq file-name (describe-simplify-lib-file-name file-name))
	      (when file-name
		(princ "Defined in `")
		(princ file-name)
		(princ "'")
		;; Make a hyperlink to the library.
		(save-excursion
		  (re-search-backward "`\\([^`']+\\)'" nil t)
		  (help-xref-button 1 'help-face-def f file-name))
		(princ ".")
		(terpri)
		(terpri))
	      (dolist (a attrs)
		(let ((attr (face-attribute f (car a) frame)))
		  (insert (make-string (- max-width (length (cdr a))) ?\s)
			  (cdr a) ": " (format "%s" attr))
		  (if (and (eq (car a) :inherit)
			   (not (eq attr 'unspecified)))
		      ;; Make a hyperlink to the parent face.
		      (save-excursion
			(re-search-backward ": \\([^:]+\\)" nil t)
			(help-xref-button 1 'help-face attr)))
		  (insert "\n")))))
	  (terpri)))
      (print-help-return-message))))


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Face specifications (defface).
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; Parameter FRAME Is kept for call compatibility to with previous
;; face implementation.

(defun face-attr-construct (face &optional frame)
  "Return a `defface'-style attribute list for FACE on FRAME.
Value is a property list of pairs ATTRIBUTE VALUE for all specified
face attributes of FACE where ATTRIBUTE is the attribute name and
VALUE is the specified value of that attribute."
  (let (result)
    (dolist (entry face-attribute-name-alist result)
      (let* ((attribute (car entry))
	     (value (face-attribute face attribute)))
	(unless (eq value 'unspecified)
	  (setq result (nconc (list attribute value) result)))))))


(defun face-spec-set-match-display (display frame)
  "Non-nil if DISPLAY matches FRAME.
DISPLAY is part of a spec such as can be used in `defface'.
If FRAME is nil, the current FRAME is used."
  (let* ((conjuncts display)
	 conjunct req options
	 ;; t means we have succeeded against all the conjuncts in
	 ;; DISPLAY that have been tested so far.
	 (match t))
    (if (eq conjuncts t)
	(setq conjuncts nil))
    (while (and conjuncts match)
      (setq conjunct (car conjuncts)
	    conjuncts (cdr conjuncts)
	    req (car conjunct)
	    options (cdr conjunct)
	    match (cond ((eq req 'type)
			 (or (memq window-system options)
			     ;; FIXME: This should be revisited to use
			     ;; display-graphic-p, provided that the
			     ;; color selection depends on the number
			     ;; of supported colors, and all defface's
			     ;; are changed to look at number of colors
			     ;; instead of (type graphic) etc.
			     (and (null window-system)
				  (memq 'tty options))
			     (and (memq 'motif options)
				  (featurep 'motif))
			     (and (memq 'gtk options)
				  (featurep 'gtk))
			     (and (memq 'lucid options)
				  (featurep 'x-toolkit)
				  (not (featurep 'motif))
				  (not (featurep 'gtk)))
			     (and (memq 'x-toolkit options)
				  (featurep 'x-toolkit))))
			((eq req 'min-colors)
			 (>= (display-color-cells frame) (car options)))
			((eq req 'class)
			 (memq (frame-parameter frame 'display-type) options))
			((eq req 'background)
			 (memq (frame-parameter frame 'background-mode)
			       options))
			((eq req 'supports)
			 (display-supports-face-attributes-p options frame))
			(t (error "Unknown req `%S' with options `%S'"
				  req options)))))
    match))


(defun face-spec-choose (spec &optional frame)
  "Choose the proper attributes for FRAME, out of SPEC.
If SPEC is nil, return nil."
  (unless frame
    (setq frame (selected-frame)))
  (let ((tail spec)
	result defaults)
    (while tail
      (let* ((entry (pop tail))
	     (display (car entry))
	     (attrs (cdr entry))
	     thisval)
	;; Get the attributes as actually specified by this alternative.
	(setq thisval
	      (if (null (cdr attrs)) ;; was (listp (car attrs))
		  ;; Old-style entry, the attribute list is the
		  ;; first element.
		  (car attrs)
		attrs))

	;; If the condition is `default', that sets the default
	;; for following conditions.
	(if (eq display 'default)
	    (setq defaults thisval)
	  ;; Otherwise, if it matches, use it.
	  (when (face-spec-set-match-display display frame)
	    (setq result thisval)
	    (setq tail nil)))))
    (if defaults (append result defaults) result)))


(defun face-spec-reset-face (face &optional frame)
  "Reset all attributes of FACE on FRAME to unspecified."
  (let ((attrs face-attribute-name-alist))
    (while attrs
      (let ((attr-and-name (car attrs)))
	(set-face-attribute face frame (car attr-and-name) 'unspecified))
      (setq attrs (cdr attrs)))))


(defun face-spec-set (face spec &optional frame)
  "Set FACE's attributes according to the first matching entry in SPEC.
FRAME is the frame whose frame-local face is set.  FRAME nil means
do it on all frames.  See `defface' for information about SPEC.
If SPEC is nil, do nothing."
  (let ((attrs (face-spec-choose spec frame)))
    (when spec
      (face-spec-reset-face face frame))
    (while attrs
      (let ((attribute (car attrs))
	    (value (car (cdr attrs))))
	;; Support some old-style attribute names and values.
	(case attribute
	  (:bold (setq attribute :weight value (if value 'bold 'normal)))
	  (:italic (setq attribute :slant value (if value 'italic 'normal)))
	  ((:foreground :background)
	   ;; Compatibility with 20.x.  Some bogus face specs seem to
	   ;; exist containing things like `:foreground nil'.
	   (if (null value) (setq value 'unspecified)))
	  (t (unless (assq attribute face-x-resources)
	       (setq attribute nil))))
	(when attribute
	  (set-face-attribute face frame attribute value)))
      (setq attrs (cdr (cdr attrs)))))
  ;; When we reset the face based on its spec, then it is unmodified
  ;; as far as Custom is concerned.
  (if (null frame)
      (put (or (get face 'face-alias) face) 'face-modified nil)))


(defun face-attr-match-p (face attrs &optional frame)
  "Return t if attributes of FACE match values in plist ATTRS.
Optional parameter FRAME is the frame whose definition of FACE
is used.  If nil or omitted, use the selected frame."
  (unless frame
    (setq frame (selected-frame)))
  (let ((list face-attribute-name-alist)
	(match t))
    (while (and match (not (null list)))
      (let* ((attr (car (car list)))
	     (specified-value
	      (if (plist-member attrs attr)
		  (plist-get attrs attr)
		'unspecified))
	     (value-now (face-attribute face attr frame)))
	(setq match (equal specified-value value-now))
	(setq list (cdr list))))
    match))

(defun face-spec-match-p (face spec &optional frame)
  "Return t if FACE, on FRAME, matches what SPEC says it should look like."
  (face-attr-match-p face (face-spec-choose spec frame) frame))

(defsubst face-default-spec (face)
  "Return the default face-spec for FACE, ignoring any user customization.
If there is no default for FACE, return nil."
  (get face 'face-defface-spec))

(defsubst face-user-default-spec (face)
  "Return the user's customized face-spec for FACE, or the default if none.
If there is neither a user setting nor a default for FACE, return nil."
  (or (get face 'customized-face)
      (get face 'saved-face)
      (face-default-spec face)))


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Frame-type independent color support.
;;; We keep the old x-* names as aliases for back-compatibility.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(defun defined-colors (&optional frame)
  "Return a list of colors supported for a particular frame.
The argument FRAME specifies which frame to try.
The value may be different for frames on different display types.
If FRAME doesn't support colors, the value is nil.
If FRAME is nil, that stands for the selected frame."
  (if (memq (framep (or frame (selected-frame))) '(x w32 mac))
      (xw-defined-colors frame)
    (mapcar 'car (tty-color-alist frame))))
(defalias 'x-defined-colors 'defined-colors)

(defun color-defined-p (color &optional frame)
  "Return non-nil if color COLOR is supported on frame FRAME.
If FRAME is omitted or nil, use the selected frame.
If COLOR is the symbol `unspecified' or one of the strings
\"unspecified-fg\" or \"unspecified-bg\", the value is nil."
  (if (member color '(unspecified "unspecified-bg" "unspecified-fg"))
      nil
    (if (member (framep (or frame (selected-frame))) '(x w32 mac))
	(xw-color-defined-p color frame)
      (numberp (tty-color-translate color frame)))))
(defalias 'x-color-defined-p 'color-defined-p)

(defun color-values (color &optional frame)
  "Return a description of the color named COLOR on frame FRAME.
The value is a list of integer RGB values--(RED GREEN BLUE).
These values appear to range from 0 to 65280 or 65535, depending
on the system; white is \(65280 65280 65280\) or \(65535 65535 65535\).
If FRAME is omitted or nil, use the selected frame.
If FRAME cannot display COLOR, the value is nil.
If COLOR is the symbol `unspecified' or one of the strings
\"unspecified-fg\" or \"unspecified-bg\", the value is nil."
  (if (member color '(unspecified "unspecified-fg" "unspecified-bg"))
      nil
    (if (memq (framep (or frame (selected-frame))) '(x w32 mac))
	(xw-color-values color frame)
      (tty-color-values color frame))))
(defalias 'x-color-values 'color-values)

(defun display-color-p (&optional display)
  "Return t if DISPLAY supports color.
The optional argument DISPLAY specifies which display to ask about.
DISPLAY should be either a frame or a display name (a string).
If omitted or nil, that stands for the selected frame's display."
  (if (memq (framep-on-display display) '(x w32 mac))
      (xw-display-color-p display)
    (tty-display-color-p display)))
(defalias 'x-display-color-p 'display-color-p)

(defun display-grayscale-p (&optional display)
  "Return non-nil if frames on DISPLAY can display shades of gray."
  (let ((frame-type (framep-on-display display)))
    (cond
     ((memq frame-type '(x w32 mac))
      (x-display-grayscale-p display))
     (t
      (> (tty-color-gray-shades display) 2)))))


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Background mode.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(defcustom frame-background-mode nil
  "*The brightness of the background.
Set this to the symbol `dark' if your background color is dark,
`light' if your background is light, or nil (automatic by default)
if you want Emacs to examine the brightness for you.  Don't set this
variable with `setq'; this won't have the expected effect."
  :group 'faces
  :set #'(lambda (var value)
	   (set-default var value)
	   (mapc 'frame-set-background-mode (frame-list)))
  :initialize 'custom-initialize-changed
  :type '(choice (const dark)
		 (const light)
		 (const :tag "automatic" nil)))

(defvar default-frame-background-mode nil
  "Internal variable for the default brightness of the background.
Emacs sets it automatically depending on the terminal type.
The value `nil' means `dark'.  If Emacs runs in non-windowed
mode from `xterm' or a similar terminal emulator, the value is
`light'.  On rxvt terminals, the value depends on the environment
variable COLORFGBG.")

(defun frame-set-background-mode (frame)
  "Set up display-dependent faces on FRAME.
Display-dependent faces are those which have different definitions
according to the `background-mode' and `display-type' frame parameters."
  (let* ((bg-resource
	  (and window-system
	       (x-get-resource "backgroundMode" "BackgroundMode")))
	 (bg-color (frame-parameter frame 'background-color))
	 (bg-mode
	  (cond (frame-background-mode)
		(bg-resource
		 (intern (downcase bg-resource)))
		((and (null window-system) (null bg-color))
		 ;; No way to determine this automatically (?).
		 (or default-frame-background-mode 'dark))
		;; Unspecified frame background color can only happen
		;; on tty's.
		((member bg-color '(unspecified "unspecified-bg"))
		 (or default-frame-background-mode 'dark))
		((equal bg-color "unspecified-fg") ; inverted colors
		 (if (eq default-frame-background-mode 'light) 'dark 'light))
		((>= (apply '+ (color-values bg-color frame))
		    ;; Just looking at the screen, colors whose
		    ;; values add up to .6 of the white total
		    ;; still look dark to me.
		    (* (apply '+ (color-values "white" frame)) .6))
		 'light)
		(t 'dark)))
	 (display-type
	  (cond ((null window-system)
		 (if (tty-display-color-p frame) 'color 'mono))
		((display-color-p frame)
		 'color)
		((x-display-grayscale-p frame)
		 'grayscale)
		(t 'mono)))
	 (old-bg-mode
	  (frame-parameter frame 'background-mode))
	 (old-display-type
	  (frame-parameter frame 'display-type)))

    (unless (and (eq bg-mode old-bg-mode) (eq display-type old-display-type))
      (let ((locally-modified-faces nil))
	;; Before modifying the frame parameters, we collect a list of
	;; faces that don't match what their face-spec says they should
	;; look like; we then avoid changing these faces below.  A
	;; negative list is used on the assumption that most faces will
	;; be unmodified, so we can avoid consing in the common case.
	(dolist (face (face-list))
	  (when (not (face-spec-match-p face
					(face-user-default-spec face)
					(selected-frame)))
	    (push face locally-modified-faces)))
	;; Now change to the new frame parameters
	(modify-frame-parameters frame
				 (list (cons 'background-mode bg-mode)
				       (cons 'display-type display-type)))
	;; For all named faces, choose face specs matching the new frame
	;; parameters, unless they have been locally modified.
	(dolist (face (face-list))
	  (unless (memq face locally-modified-faces)
	    (face-spec-set face (face-user-default-spec face) frame)))))))


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Frame creation.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(defun x-handle-named-frame-geometry (parameters)
  "Add geometry parameters for a named frame to parameter list PARAMETERS.
Value is the new parameter list."
  (let* ((name (or (cdr (assq 'name parameters))
		   (cdr (assq 'name default-frame-alist))))
	 (x-resource-name name)
	 (res-geometry (if name (x-get-resource "geometry" "Geometry"))))
    (when res-geometry
      (let ((parsed (x-parse-geometry res-geometry)))
	;; If the resource specifies a position, call the position
	;; and size "user-specified".
	(when (or (assq 'top parsed)
		  (assq 'left parsed))
	  (setq parsed (append '((user-position . t) (user-size . t)) parsed)))
	;; Put the geometry parameters at the end.  Copy
	;; default-frame-alist so that they go after it.
	(setq parameters (append parameters default-frame-alist parsed))))
    parameters))


(defun x-handle-reverse-video (frame parameters)
  "Handle the reverse-video frame parameter and X resource.
`x-create-frame' does not handle this one."
  (when (cdr (or (assq 'reverse parameters)
		 (assq 'reverse default-frame-alist)
		 (let ((resource (x-get-resource "reverseVideo"
						 "ReverseVideo")))
		   (if resource
		       (cons nil (member (downcase resource)
					 '("on" "true")))))))
      (let* ((params (frame-parameters frame))
	     (bg (cdr (assq 'foreground-color params)))
	     (fg (cdr (assq 'background-color params))))
	(modify-frame-parameters frame
				 (list (cons 'foreground-color fg)
				       (cons 'background-color bg)))
	(if (equal bg (cdr (assq 'border-color params)))
	    (modify-frame-parameters frame
				     (list (cons 'border-color fg))))
	(if (equal bg (cdr (assq 'mouse-color params)))
	    (modify-frame-parameters frame
				     (list (cons 'mouse-color fg))))
	(if (equal bg (cdr (assq 'cursor-color params)))
	    (modify-frame-parameters frame
				     (list (cons 'cursor-color fg)))))))


(defun x-create-frame-with-faces (&optional parameters)
  "Create a frame from optional frame parameters PARAMETERS.
Parameters not specified by PARAMETERS are taken from
`default-frame-alist'.  If PARAMETERS specify a frame name,
handle X geometry resources for that name.  If either PARAMETERS
or `default-frame-alist' contains a `reverse' parameter, or
the X resource ``reverseVideo'' is present, handle that.
Value is the new frame created."
  (setq parameters (x-handle-named-frame-geometry parameters))
  (let ((visibility-spec (assq 'visibility parameters))
	(frame-list (frame-list))
	(frame (x-create-frame (cons '(visibility . nil) parameters)))
	success)
    (unwind-protect
	(progn
	  (x-handle-reverse-video frame parameters)
	  (frame-set-background-mode frame)
	  (face-set-after-frame-default frame)
	  (if (or (null frame-list) (null visibility-spec))
	      (make-frame-visible frame)
	    (modify-frame-parameters frame (list visibility-spec)))
	  (setq success t))
      (unless success
	(delete-frame frame)))
    frame))

(defun face-set-after-frame-default (frame)
  "Set frame-local faces of FRAME from face specs and resources.
Initialize colors of certain faces from frame parameters."
  (unless inhibit-face-set-after-frame-default
    (if (face-attribute 'default :font t)
	(set-face-attribute 'default frame :font
			    (face-attribute 'default :font t))
      (set-face-attribute 'default frame :family
			  (face-attribute 'default :family t))
      (set-face-attribute 'default frame :height
			  (face-attribute 'default :height t))
      (set-face-attribute 'default frame :slant
			  (face-attribute 'default :slant t))
      (set-face-attribute 'default frame :weight
			  (face-attribute 'default :weight t))
      (set-face-attribute 'default frame :width
			  (face-attribute 'default :width t))))
  ;; Find attributes that should be initialized from frame parameters.
  (let ((face-params '((foreground-color default :foreground)
		       (background-color default :background)
		       (border-color border :background)
		       (cursor-color cursor :background)
		       (scroll-bar-foreground scroll-bar :foreground)
		       (scroll-bar-background scroll-bar :background)
		       (mouse-color mouse :background)))
	apply-params)
    (dolist (param face-params)
      (let* ((value (frame-parameter frame (nth 0 param)))
	     (face (nth 1 param))
	     (attr (nth 2 param))
	     (default-value (face-attribute face attr t)))
	;; Compile a list of face attributes to set, but don't set
	;; them yet.  The call to make-face-x-resource-internal,
	;; below, can change frame parameters, and the final set of
	;; frame parameters should be the ones acquired at this step.
	(if (eq default-value 'unspecified)
	    ;; The face spec does not specify a new-frame value for
	    ;; this attribute.  Check if the existing frame parameter
	    ;; specifies it.
	    (if value
		(push (list face frame attr value) apply-params))
	  ;; The face spec specifies a value for this attribute, to be
	  ;; applied to the face on all new frames.
	  (push (list face frame attr default-value) apply-params))))
    ;; Initialize faces from face specs and X resources.  The
    ;; condition-case prevents invalid specs from causing frame
    ;; creation to fail.
    (dolist (face (delq 'default (face-list)))
      (condition-case ()
	  (progn
	    (face-spec-set face (face-user-default-spec face) frame)
	    (if (memq window-system '(x w32 mac))
		(make-face-x-resource-internal face frame))
	    (internal-merge-in-global-face face frame))
	(error nil)))
    ;; Apply the attributes specified by frame parameters.  This
    ;; rewrites parameters changed by make-face-x-resource-internal
    (dolist (param apply-params)
      (apply 'set-face-attribute param))))

(defun tty-handle-reverse-video (frame parameters)
  "Handle the reverse-video frame parameter for terminal frames."
  (when (cdr (or (assq 'reverse parameters)
		 (assq 'reverse default-frame-alist)))
    (let* ((params (frame-parameters frame))
	   (bg (cdr (assq 'foreground-color params)))
	   (fg (cdr (assq 'background-color params))))
      (modify-frame-parameters frame
			       (list (cons 'foreground-color fg)
				     (cons 'background-color bg)))
      (if (equal bg (cdr (assq 'mouse-color params)))
	  (modify-frame-parameters frame
				   (list (cons 'mouse-color fg))))
      (if (equal bg (cdr (assq 'cursor-color params)))
	  (modify-frame-parameters frame
				   (list (cons 'cursor-color fg)))))))


(defun tty-create-frame-with-faces (&optional parameters)
  "Create a frame from optional frame parameters PARAMETERS.
Parameters not specified by PARAMETERS are taken from
`default-frame-alist'.  If either PARAMETERS or `default-frame-alist'
contains a `reverse' parameter, handle that.  Value is the new frame
created."
  (let ((frame (make-terminal-frame parameters))
	success)
    (unwind-protect
	(progn
	  (tty-handle-reverse-video frame (frame-parameters frame))
	  (frame-set-background-mode frame)
	  (face-set-after-frame-default frame)
	  (setq success t))
      (unless success
	(delete-frame frame)))
    frame))


;; Called from C function init_display to initialize faces of the
;; dumped terminal frame on startup.

(defun tty-set-up-initial-frame-faces ()
  (let ((frame (selected-frame)))
    (frame-set-background-mode frame)
    (face-set-after-frame-default frame)))




;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Compatiblity with 20.2
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; Update a frame's faces when we change its default font.

(defalias 'frame-update-faces 'ignore "")
(make-obsolete 'frame-update-faces "no longer necessary." "21.1")

;; Update the colors of FACE, after FRAME's own colors have been
;; changed.

(define-obsolete-function-alias 'frame-update-face-colors
    'frame-set-background-mode "21.1")


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Standard faces.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(defgroup basic-faces nil
  "The standard faces of Emacs."
  :group 'faces)

(defface default
  '((t nil))
  "Basic default face."
  :group 'basic-faces)

(defface bold
  '((t :weight bold))
  "Basic bold face."
  :group 'basic-faces)

(defface italic
  '((((supports :slant italic))
     :slant italic)
    (((supports :underline t))
     :underline t)
    (t
     ;; default to italic, even it doesn't appear to be supported,
     ;; because in some cases the display engine will do it's own
     ;; workaround (to `dim' on ttys)
     :slant italic))
  "Basic italic face."
  :group 'basic-faces)

(defface bold-italic
  '((t :weight bold :slant italic))
  "Basic bold-italic face."
  :group 'basic-faces)

(defface underline
  '((((supports :underline t))
     :underline t)
    (((supports :weight bold))
     :weight bold)
    (t :underline t))
  "Basic underlined face."
  :group 'basic-faces)

(defface fixed-pitch
  '((t :family "courier"))
  "The basic fixed-pitch face."
  :group 'basic-faces)

(defface variable-pitch
  '((t :family "helv"))
  "The basic variable-pitch face."
  :group 'basic-faces)

(defface shadow
  '((((class color grayscale) (min-colors 88) (background light))
     :foreground "grey50")
    (((class color grayscale) (min-colors 88) (background dark))
     :foreground "grey70")
    (((class color) (min-colors 8) (background light))
     :foreground "green")
    (((class color) (min-colors 8) (background dark))
     :foreground "yellow"))
  "Basic face for shadowed text."
  :group 'basic-faces
  :version "22.1")

(defface link
  '((((class color) (min-colors 88) (background light))
     :foreground "blue1" :underline t)
    (((class color) (background light))
     :foreground "blue" :underline t)
    (((class color) (min-colors 88) (background dark))
     :foreground "cyan1" :underline t)
    (((class color) (background dark))
     :foreground "cyan" :underline t)
    (t :inherit underline))
  "Basic face for unvisited links."
  :group 'basic-faces
  :version "22.1")

(defface link-visited
  '((default :inherit link)
    (((class color) (background light)) :foreground "magenta4")
    (((class color) (background dark)) :foreground "violet"))
  "Basic face for visited links."
  :group 'basic-faces
  :version "22.1")

(defface highlight
  '((((class color) (min-colors 88) (background light))
     :background "darkseagreen2")
    (((class color) (min-colors 88) (background dark))
     :background "darkolivegreen")
    (((class color) (min-colors 16) (background light))
     :background "darkseagreen2")
    (((class color) (min-colors 16) (background dark))
     :background "darkolivegreen")
    (((class color) (min-colors 8))
     :background "green" :foreground "black")
    (t :inverse-video t))
  "Basic face for highlighting."
  :group 'basic-faces)

(defface region
  '((((class color) (min-colors 88) (background dark))
     :background "blue3")
    (((class color) (min-colors 88) (background light))
     :background "lightgoldenrod2")
    (((class color) (min-colors 16) (background dark))
     :background "blue3")
    (((class color) (min-colors 16) (background light))
     :background "lightgoldenrod2")
    (((class color) (min-colors 8))
     :background "blue" :foreground "white")
    (((type tty) (class mono))
     :inverse-video t)
    (t :background "gray"))
  "Basic face for highlighting the region."
  :version "21.1"
  :group 'basic-faces)

(defface secondary-selection
  '((((class color) (min-colors 88) (background light))
     :background "yellow1")
    (((class color) (min-colors 88) (background dark))
     :background "SkyBlue4")
    (((class color) (min-colors 16) (background light))
     :background "yellow")
    (((class color) (min-colors 16) (background dark))
     :background "SkyBlue4")
    (((class color) (min-colors 8))
     :background "cyan" :foreground "black")
    (t :inverse-video t))
  "Basic face for displaying the secondary selection."
  :group 'basic-faces)

(defface trailing-whitespace
  '((((class color) (background light))
     :background "red1")
    (((class color) (background dark))
     :background "red1")
    (t :inverse-video t))
  "Basic face for highlighting trailing whitespace."
  :version "21.1"
  :group 'whitespace-faces	; like `show-trailing-whitespace'
  :group 'basic-faces)

(defface escape-glyph
  '((((background dark)) :foreground "cyan")
    ;; See the comment in minibuffer-prompt for
    ;; the reason not to use blue on MS-DOS.
    (((type pc)) :foreground "magenta")
    ;; red4 is too dark, but some say blue is too loud.
    ;; brown seems to work ok. -- rms.
    (t :foreground "brown"))
  "Face for characters displayed as sequences using `^' or `\\'."
  :group 'basic-faces
  :version "22.1")

(defface nobreak-space
  '((((class color) (min-colors 88)) :inherit escape-glyph :underline t)
    (((class color) (min-colors 8)) :background "magenta")
    (t :inverse-video t))
  "Face for displaying nobreak space."
  :group 'basic-faces
  :version "22.1")

(defgroup mode-line-faces nil
  "Faces used in the mode line."
  :group 'mode-line
  :group 'faces
  :version "22.1")

(defface mode-line
  '((((class color) (min-colors 88))
     :box (:line-width -1 :style released-button)
     :background "grey75" :foreground "black")
    (t
     :inverse-video t))
  "Basic mode line face for selected window."
  :version "21.1"
  :group 'mode-line-faces
  :group 'basic-faces)

(defface mode-line-inactive
  '((default
     :inherit mode-line)
    (((class color) (min-colors 88) (background light))
     :weight light
     :box (:line-width -1 :color "grey75" :style nil)
     :foreground "grey20" :background "grey90")
    (((class color) (min-colors 88) (background dark) )
     :weight light
     :box (:line-width -1 :color "grey40" :style nil)
     :foreground "grey80" :background "grey30"))
  "Basic mode line face for non-selected windows."
  :version "22.1"
  :group 'mode-line-faces
  :group 'basic-faces)

(defface mode-line-highlight
  '((((class color) (min-colors 88))
     :box (:line-width 2 :color "grey40" :style released-button))
    (t
     :inherit highlight))
  "Basic mode line face for highlighting."
  :version "22.1"
  :group 'mode-line-faces
  :group 'basic-faces)

(defface mode-line-buffer-id
  '((t (:weight bold)))
  "Face used for buffer identification parts of the mode line."
  :version "22.1"
  :group 'mode-line-faces
  :group 'basic-faces)

;; Make `modeline' an alias for `mode-line', for compatibility.
(put 'modeline 'face-alias 'mode-line)
(put 'modeline-inactive 'face-alias 'mode-line-inactive)
(put 'modeline-highlight 'face-alias 'mode-line-highlight)
(put 'modeline-buffer-id 'face-alias 'mode-line-buffer-id)

(defface header-line
  '((default
     :inherit mode-line)
    (((type tty))
     ;; This used to be `:inverse-video t', but that doesn't look very
     ;; good when combined with inverse-video mode-lines and multiple
     ;; windows.  Underlining looks better, and is more consistent with
     ;; the window-system face variants, which deemphasize the
     ;; header-line in relation to the mode-line face.  If a terminal
     ;; can't underline, then the header-line will end up without any
     ;; highlighting; this may be too confusing in general, although it
     ;; happens to look good with the only current use of header-lines,
     ;; the info browser. XXX
     :inverse-video nil	       ;Override the value inherited from mode-line.
     :underline t)
    (((class color grayscale) (background light))
     :background "grey90" :foreground "grey20"
     :box nil)
    (((class color grayscale) (background dark))
     :background "grey20" :foreground "grey90"
     :box nil)
    (((class mono) (background light))
     :background "white" :foreground "black"
     :inverse-video nil
     :box nil
     :underline t)
    (((class mono) (background dark))
     :background "black" :foreground "white"
     :inverse-video nil
     :box nil
     :underline t))
  "Basic header-line face."
  :version "21.1"
  :group 'basic-faces)

(defface vertical-border
  '((((type tty)) :inherit mode-line-inactive))
  "Face used for vertical window dividers on ttys."
  :version "22.1"
  :group 'basic-faces)

(defface minibuffer-prompt
  '((((background dark)) :foreground "cyan")
    ;; Don't use blue because many users of the MS-DOS port customize
    ;; their foreground color to be blue.
    (((type pc)) :foreground "magenta")
    (t :foreground "medium blue"))
  "Face for minibuffer prompts.
By default, Emacs automatically adds this face to the value of
`minibuffer-prompt-properties', which is a list of text properties
used to display the prompt text."
  :version "22.1"
  :group 'basic-faces)

(setq minibuffer-prompt-properties
      (append minibuffer-prompt-properties (list 'face 'minibuffer-prompt)))

(defface fringe
  '((((class color) (background light))
     :background "grey95")
    (((class color) (background dark))
     :background "grey10")
    (t
     :background "gray"))
  "Basic face for the fringes to the left and right of windows under X."
  :version "21.1"
  :group 'frames
  :group 'basic-faces)

(defface scroll-bar '((t nil))
  "Basic face for the scroll bar colors under X."
  :version "21.1"
  :group 'frames
  :group 'basic-faces)

(defface border '((t nil))
  "Basic face for the frame border under X."
  :version "21.1"
  :group 'frames
  :group 'basic-faces)

(defface cursor '((t nil))
  "Basic face for the cursor color under X.
Note: Other faces cannot inherit from the cursor face."
  :version "21.1"
  :group 'cursor
  :group 'basic-faces)

(put 'cursor 'face-no-inherit t)

(defface mouse '((t nil))
  "Basic face for the mouse color under X."
  :version "21.1"
  :group 'mouse
  :group 'basic-faces)

(defface tool-bar
  '((default
     :box (:line-width 1 :style released-button)
     :foreground "black")
    (((type x w32 mac) (class color))
     :background "grey75")
    (((type x) (class mono))
     :background "grey"))
  "Basic tool-bar face."
  :version "21.1"
  :group 'basic-faces)

(defface menu
  '((((type tty))
     :inverse-video t)
    (((type x-toolkit))
     )
    (t
     :inverse-video t))
  "Basic face for the font and colors of the menu bar and popup menus."
  :version "21.1"
  :group 'menu
  :group 'basic-faces)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Manipulating font names.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; This is here for compatibilty with Emacs 20.2.  For example,
;; international/fontset.el uses x-resolve-font-name.  The following
;; functions are not used in the face implementation itself.

(defvar x-font-regexp nil)
(defvar x-font-regexp-head nil)
(defvar x-font-regexp-weight nil)
(defvar x-font-regexp-slant nil)

(defconst x-font-regexp-weight-subnum 1)
(defconst x-font-regexp-slant-subnum 2)
(defconst x-font-regexp-swidth-subnum 3)
(defconst x-font-regexp-adstyle-subnum 4)

;;; Regexps matching font names in "Host Portable Character Representation."
;;;
(let ((- 		"[-?]")
      (foundry		"[^-]+")
      (family 		"[^-]+")
      (weight		"\\(bold\\|demibold\\|medium\\)")		; 1
;     (weight\?		"\\(\\*\\|bold\\|demibold\\|medium\\|\\)")	; 1
      (weight\?		"\\([^-]*\\)")					; 1
      (slant		"\\([ior]\\)")					; 2
;     (slant\?		"\\([ior?*]?\\)")				; 2
      (slant\?		"\\([^-]?\\)")					; 2
;     (swidth		"\\(\\*\\|normal\\|semicondensed\\|\\)")	; 3
      (swidth		"\\([^-]*\\)")					; 3
;     (adstyle		"\\(\\*\\|sans\\|\\)")				; 4
      (adstyle		"\\([^-]*\\)")					; 4
      (pixelsize	"[0-9]+")
      (pointsize	"[0-9][0-9]+")
      (resx		"[0-9][0-9]+")
      (resy		"[0-9][0-9]+")
      (spacing		"[cmp?*]")
      (avgwidth		"[0-9]+")
      (registry		"[^-]+")
      (encoding		"[^-]+")
      )
  (setq x-font-regexp
	(concat "\\`\\*?[-?*]"
		foundry - family - weight\? - slant\? - swidth - adstyle -
		pixelsize - pointsize - resx - resy - spacing - avgwidth -
		registry - encoding "\\*?\\'"
		))
  (setq x-font-regexp-head
	(concat "\\`[-?*]" foundry - family - weight\? - slant\?
		"\\([-*?]\\|\\'\\)"))
  (setq x-font-regexp-slant (concat - slant -))
  (setq x-font-regexp-weight (concat - weight -))
  nil)


(defun x-resolve-font-name (pattern &optional face frame)
  "Return a font name matching PATTERN.
All wildcards in PATTERN are instantiated.
If PATTERN is nil, return the name of the frame's base font, which never
contains wildcards.
Given optional arguments FACE and FRAME, return a font which is
also the same size as FACE on FRAME, or fail."
  (or (symbolp face)
      (setq face (face-name face)))
  (and (eq frame t)
       (setq frame nil))
  (if pattern
      ;; Note that x-list-fonts has code to handle a face with nil as its font.
      (let ((fonts (x-list-fonts pattern face frame 1)))
	(or fonts
	    (if face
		(if (string-match "\\*" pattern)
		    (if (null (face-font face))
			(error "No matching fonts are the same height as the frame default font")
		      (error "No matching fonts are the same height as face `%s'" face))
		  (if (null (face-font face))
		      (error "Height of font `%s' doesn't match the frame default font"
			     pattern)
		    (error "Height of font `%s' doesn't match face `%s'"
			   pattern face)))
	      (error "No fonts match `%s'" pattern)))
	(car fonts))
    (cdr (assq 'font (frame-parameters (selected-frame))))))


(defun x-frob-font-weight (font which)
  (let ((case-fold-search t))
    (cond ((string-match x-font-regexp font)
	   (concat (substring font 0
			      (match-beginning x-font-regexp-weight-subnum))
		   which
		   (substring font (match-end x-font-regexp-weight-subnum)
			      (match-beginning x-font-regexp-adstyle-subnum))
		   ;; Replace the ADD_STYLE_NAME field with *
		   ;; because the info in it may not be the same
		   ;; for related fonts.
		   "*"
		   (substring font (match-end x-font-regexp-adstyle-subnum))))
	  ((string-match x-font-regexp-head font)
	   (concat (substring font 0 (match-beginning 1)) which
		   (substring font (match-end 1))))
	  ((string-match x-font-regexp-weight font)
	   (concat (substring font 0 (match-beginning 1)) which
		   (substring font (match-end 1)))))))
(make-obsolete 'x-frob-font-weight 'make-face-... "21.1")

(defun x-frob-font-slant (font which)
  (let ((case-fold-search t))
    (cond ((string-match x-font-regexp font)
	   (concat (substring font 0
			      (match-beginning x-font-regexp-slant-subnum))
		   which
		   (substring font (match-end x-font-regexp-slant-subnum)
			      (match-beginning x-font-regexp-adstyle-subnum))
		   ;; Replace the ADD_STYLE_NAME field with *
		   ;; because the info in it may not be the same
		   ;; for related fonts.
		   "*"
		   (substring font (match-end x-font-regexp-adstyle-subnum))))
	  ((string-match x-font-regexp-head font)
	   (concat (substring font 0 (match-beginning 2)) which
		   (substring font (match-end 2))))
	  ((string-match x-font-regexp-slant font)
	   (concat (substring font 0 (match-beginning 1)) which
		   (substring font (match-end 1)))))))
(make-obsolete 'x-frob-font-slant 'make-face-... "21.1")

;; These aliases are here so that we don't get warnings about obsolete
;; functions from the byte compiler.
(defalias 'internal-frob-font-weight 'x-frob-font-weight)
(defalias 'internal-frob-font-slant 'x-frob-font-slant)

(defun x-make-font-bold (font)
  "Given an X font specification, make a bold version of it.
If that can't be done, return nil."
  (internal-frob-font-weight font "bold"))
(make-obsolete 'x-make-font-bold 'make-face-bold "21.1")

(defun x-make-font-demibold (font)
  "Given an X font specification, make a demibold version of it.
If that can't be done, return nil."
  (internal-frob-font-weight font "demibold"))
(make-obsolete 'x-make-font-demibold 'make-face-bold "21.1")

(defun x-make-font-unbold (font)
  "Given an X font specification, make a non-bold version of it.
If that can't be done, return nil."
  (internal-frob-font-weight font "medium"))
(make-obsolete 'x-make-font-unbold 'make-face-unbold "21.1")

(defun x-make-font-italic (font)
  "Given an X font specification, make an italic version of it.
If that can't be done, return nil."
  (internal-frob-font-slant font "i"))
(make-obsolete 'x-make-font-italic 'make-face-italic "21.1")

(defun x-make-font-oblique (font) ; you say tomayto...
  "Given an X font specification, make an oblique version of it.
If that can't be done, return nil."
  (internal-frob-font-slant font "o"))
(make-obsolete 'x-make-font-oblique 'make-face-italic "21.1")

(defun x-make-font-unitalic (font)
  "Given an X font specification, make a non-italic version of it.
If that can't be done, return nil."
  (internal-frob-font-slant font "r"))
(make-obsolete 'x-make-font-unitalic 'make-face-unitalic "21.1")

(defun x-make-font-bold-italic (font)
  "Given an X font specification, make a bold and italic version of it.
If that can't be done, return nil."
  (and (setq font (internal-frob-font-weight font "bold"))
       (internal-frob-font-slant font "i")))
(make-obsolete 'x-make-font-bold-italic 'make-face-bold-italic "21.1")

(provide 'faces)

;; arch-tag: 19a4759f-2963-445f-b004-425b9aadd7d6
;;; faces.el ends here
