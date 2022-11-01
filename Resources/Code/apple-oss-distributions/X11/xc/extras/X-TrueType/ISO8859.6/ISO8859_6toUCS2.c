/* ===EmacsMode: -*- Mode: C; tab-width:4; c-basic-offset: 4; -*- === */
/* ===FileName: ===

   Copyright (c) 1998 Takuya SHIOZAKI, All Rights reserved.
   Copyright (c) 1998,1999 Pablo Saratxaga <srtxg@chanae.alphanet.ch>

===Notice
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
   SUCH DAMAGE.

   Major Release ID: X-TrueType Server Version 1.3 [Aoi MATSUBARA Release 3]

Notice===

   This table data derived from Unicode, Inc.
   (ftp://ftp.unicode.org/Public/MAPPINGS/ISO8859/8859-6.TXT)

   added support for iso-8859-6-8, iso-8859-6-16 and asmo 449+
   encoded fonts -- srtxg

   Font encoding data from:
	http://www.langbox.com/fontara8.html (iso-8859-8)
	http://www.langbox.com/fontara16.html (iso-8859-16)
	http://www.langbox.com/asmo449.html (asmo 449+)

	NOTE: in iso-8859-6-16 the codes 0x91 (damatan medial)
	and 0x92 (kasratan medial) are not listed on unicode; however
	from the unicode charts it seems logical that those two chars
	are going to 0xfe73 and 0xfe75; and as those unicode values
	are not attributed I'll use them; if they get attributed it is
	likely that they will be to those damatan medial and
	kasratan medial glyphs.
	0x81 (fathatan on shadda isolated), 0x98 (fathatan on
	shadda medial), 0x99 (damatan on shadda medial) and 0x9a (kasratan
	on shadda medial) have no unicode equivalents; the same is true for:

       0xF1 U+ ARABIC LAM FOR LIGATURE LAM ALEF INITIAL FORM (TWO CELL GLYPH)
       0xF2 U+ ARABIC LAM FOR LIGATURE LAM ALEF MEDIAL FORM (TWO CELL GLYPH)
       0xF3 U+ ARABIC ALEF FOR LIGATURE LAM ALEF (TWO CELL GLYPH)
       0xF4 U+ ARABIC MADDA ON ALEF FOR LIGATURE MADDA ON LAM ALEF (TWO CELL GLYPH)
       0xF5 U+ ARABIC HAMZA ON ALEF FOR LIGATURE HAMZA ON LAM ALEF (TWO CELL GLYPH)
       0xF6 U+ ARABIC HAMZA UNDER ALEF FOR LIGATURE HAMZA UNDER LAM ALEF (TWO CELL GLYPH)
       0xF7 U+ ARABIC COMBO (TAIL EXTENSION FOR SEEN, SHIN, SAAD, DAAD IN TWO CELL GLYPH)

	for ISO-8859-6-8 the following have no unicode values either:

       0xA1 U+ ARABIC LIGATURE ALEF OF LAM ALEF (TWO CELL GLYPH)
       0xA2 U+ ARABIC LIGATURE MADDA ON ALEF OF LAM ALEF (TWO CELL GLYPH)
       0xA3 U+ ARABIC LIGATURE HAMZA ON ALEF OF LAM ALEF (TWO CELL GLYPH)
       0xA4 U+ ARABIC LIGATURE HAMZA UNDER ALEF OF LAM ALEF (TWO CELL GLYPH)
       0xA5 U+ ARABIC LIGATURE LAM OF LAM ALEF INITIAL FORM (TWO CELL GLYPH)
       0xA6 U+ ARABIC LIGATURE LAM OF LAM ALEF MEDIAL FORM (TWO CELL GLYPH)

	I put 0xf0?? for them so if the True Type font has also
	a microsoft-symbol encoding table the glyphs will match.  -- srtxg

 */
/* $XFree86: xc/extras/X-TrueType/ISO8859.6/ISO8859_6toUCS2.c,v 1.2 2000/06/27 21:26:32 tsi Exp $ */

#include "xttversion.h"

static char const * const releaseID =
    _XTT_RELEASE_NAME;

#include "xttcommon.h"
#include "xttcap.h"
#include "xttcconv.h"
#include "xttcconvP.h"

#define ALTCHR 0x0020

static ucs2_t tblIso8859_6ToUcs2[] = {
/* 0x00A0 - 0x00FF */
    0x00A0, ALTCHR, ALTCHR, ALTCHR, 0x00A4, ALTCHR, ALTCHR, ALTCHR, 
    ALTCHR, ALTCHR, ALTCHR, ALTCHR, 0x060C, 0x00AD, ALTCHR, ALTCHR, 
    ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, 
    ALTCHR, ALTCHR, ALTCHR, 0x061B, ALTCHR, ALTCHR, ALTCHR, 0x061F, 
    ALTCHR, 0x0621, 0x0622, 0x0623, 0x0624, 0x0625, 0x0626, 0x0627, 
    0x0628, 0x0629, 0x062A, 0x062B, 0x062C, 0x062D, 0x062E, 0x062F, 
    0x0630, 0x0631, 0x0632, 0x0633, 0x0634, 0x0635, 0x0636, 0x0637, 
    0x0638, 0x0639, 0x063A, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, 
    0x0640, 0x0641, 0x0642, 0x0643, 0x0644, 0x0645, 0x0646, 0x0647, 
    0x0648, 0x0649, 0x064A, 0x064B, 0x064C, 0x064D, 0x064E, 0x064F, 
    0x0650, 0x0651, 0x0652, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, 
    ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, 
};

static ucs2_t tblIso8859_6_8ToUcs2[] = {
/* 0x00A0 - 0x00FF */
    0x00A0, 0xf0a1, 0xf0a2, 0xf0a3, 0xf0a4, 0xf0a5, 0xf0a6, ALTCHR,
    0x064b, 0x064c, 0x064d, 0x064e, 0x064f, 0x0650, 0x0651, 0x0652,
    0x0660, 0x0661, 0x0662, 0x0663, 0x0664, 0x0665, 0x0666, 0x0667,
    0x0668, 0x0669, 0x060c, 0x061B, 0xfe71, 0xfe73, 0xfe75, 0x061F,
    0xfe8b, 0xfe80, 0xfe82, 0xfe84, 0xfe86, 0xfe88, 0xfe8a, 0xfe8e,
    0xfe90, 0xfe94, 0xfe96, 0xfe9a, 0xfe9e, 0xfea2, 0xfea6, 0xfeab,
    0xfeac, 0xfeae, 0xfeb0, 0xfeb2, 0xfeb6, 0xfeba, 0xfebe, 0xfec4,
    0xfec8, 0xfeca, 0xfece, 0xfe77, 0xfe79, 0xfe7b, 0xfe7f, 0xfe7d,
    0x0640, 0xfed2, 0xfed6, 0xfeda, 0xfede, 0xfee2, 0xfee6, 0xfeea,
    0xfeee, 0xfef0, 0xfef2, 0xfe92, 0xfe98, 0xfe9c, 0xfea0, 0xfea4,
    0xfea8, 0xfeb4, 0xfeb8, 0xfebc, 0xfec0, 0xfecc, 0xfed0, 0xfed4,
    0xfed8, 0xfedc, 0xfee0, 0xfee4, 0xfee8, 0xfeec, 0xfef4, ALTCHR,
};

static ucs2_t tblIso8859_6_16ToUcs2[] = {
/* 0x0000 - 0x007F */
    ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR,
    ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR,
    ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR,
    ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR,
    0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x066A, 0x0026, 0x0027,
    0x0029, 0x0028, 0x002a, 0x002b, 0x060C, 0x00AD, 0x06d4, 0x002f,
    0x0660, 0x0661, 0x0662, 0x0663, 0x0664, 0x0665, 0x0666, 0x0667,
    0x0668, 0x0669, 0x003a, 0x061B, 0x203a, 0x003d, 0x2039, 0x061F,
    0x0040, 0xfe80, 0xfe81, 0xfe83, 0xfe85, 0xfe87, 0xfe89, 0xfe8d,
    0xfe8f, 0xfe93, 0xfe95, 0xfe99, 0xfe9d, 0xfea1, 0xfea5, 0xfea9,
    0xfeab, 0xfead, 0xfeaf, 0xfeb1, 0xfeb5, 0xfeb9, 0xfebd, 0xfec1,
    0xfec5, 0xfec9, 0xfecd, 0x005d, 0x005c, 0x005b, 0x005e, 0x005f,
    0x0640, 0xfed1, 0xfed5, 0xfed9, 0xfedd, 0xfee1, 0xfee5, 0xfee9,
    0xfeed, 0xfeef, 0xfef1, 0x064b, 0x064c, 0x064d, 0x064e, 0x064f,
    0x0650, 0x0651, 0x0652, ALTCHR, ALTCHR, ALTCHR, 0xfef5, 0xfef7,
    0xfef9, 0xfefb, ALTCHR, 0x007d, 0x007c, 0x007b, 0x007e, ALTCHR,
/* 0x0080 - 0x009F */
    ALTCHR, 0xf081, 0xfc5e, 0xfc5f, 0xfc60, 0xfc61, 0xfc62, ALTCHR,
    ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR,
    0xfe71, 0xfe73, 0xfe75, 0xfe77, 0xfe79, 0xfe7b, 0xfe7f, 0xfe7d,
    0xf098, 0xf099, 0xf09a, ALTCHR, 0xfcfd, 0xfcf3, 0xfcf4, ALTCHR,
/* 0x00A0 - 0x00FF */
    0xfe8c, 0xfe82, 0xfe84, 0xfe86, 0xfe88, 0xfe8e, 0xfeaa, 0xfeac,
    0xfeae, 0xfeb0, 0xfeee, 0xfef0, 0xfe91, 0xfe92, 0xfe90, 0xfe97,
    0xfe98, 0xfe94, 0xfe97, 0xfe98, 0xfe96, 0xfe9b, 0xfe9c, 0xfe9a,
    0xfe9f, 0xfea0, 0xfe9e, 0xfea3, 0xfea4, 0xfea2, 0xfea7, 0xfea8,
    0xfea6, 0xfeb3, 0xfeb4, 0xfeb2, 0xfeb7, 0xfeb8, 0xfeb6, 0xfebb,
    0xfebc, 0xfeba, 0xfebf, 0xfec0, 0xfebe, 0xfec3, 0xfec4, 0xfec2,
    0xfec7, 0xfec8, 0xfec6, 0xfecb, 0xfecc, 0xfeca, 0xfeef, 0xfed0,
    0xfeee, 0xfed3, 0xfed4, 0xfed2, 0xfed7, 0xfed8, 0xfed6, 0xfedb,
    0xfedc, 0xfeda, 0xfedf, 0xfee0, 0xfede, 0xfee3, 0xfee4, 0xfee2,
    0xfee7, 0xfee8, 0xfee6, 0xfeeb, 0xfeec, 0xfeea, 0xfef3, 0xfef4,
    0xfef2, 0xf0f1, 0xf0f2, 0xf0f3, 0xf0f4, 0xf0f5, 0xf0f6, 0xf0f7,
    0xfe8b, 0xfe8a, 0xfef6, 0xfefa, 0xfef8, 0xfefc, ALTCHR, ALTCHR
};

static ucs2_t tblAsmo449ToUcs2[] = {
/* 0x00A0 - 0x00FF */
    0x00A0, 0x0021, 0x0022, 0x0023, 0x0024, 0x066A, 0x0026, 0x0027,
    0x0029, 0x0028, 0x002a, 0x002b, 0x060C, 0x00AD, 0x06d4, 0x002f,
    0x0660, 0x0661, 0x0662, 0x0663, 0x0664, 0x0665, 0x0666, 0x0667,
    0x0668, 0x0669, 0x003a, 0x061B, 0x203a, 0x003d, 0x2039, 0x061F,
    0x0040, 0x0621, 0x0622, 0x0623, 0x0624, 0x0625, 0x0626, 0x0627,
    0x0628, 0x0629, 0x062A, 0x062B, 0x062C, 0x062D, 0x062E, 0x062F,
    0x0630, 0x0631, 0x0632, 0x0633, 0x0634, 0x0635, 0x0636, 0x0637,
    0x0638, 0x0639, 0x063A, 0x005d, 0x005c, 0x005b, 0x005e, 0x005f,
    0x0640, 0x0641, 0x0642, 0x0643, 0x0644, 0x0645, 0x0646, 0x0647,
    0x0648, 0x0649, 0x064A, 0x064B, 0x064C, 0x064D, 0x064E, 0x064F,
    0x0650, 0x0651, 0x0652, ALTCHR, ALTCHR, ALTCHR, ALTCHR, ALTCHR,
    ALTCHR, ALTCHR, ALTCHR, 0x007d, 0x007c, 0x007b, 0x007e, ALTCHR,
};

CODE_CONV_ISO8859_TO_UCS2(cc_iso8859_6_to_ucs2, /* function name */
                          tblIso8859_6ToUcs2, /* table name */
                          ALTCHR /* alt char code (on UCS2) */
                          )
CODE_CONV_ISO8859_TO_UCS2(cc_iso8859_6_8_to_ucs2, /* function name */
			  tblIso8859_6_8ToUcs2, /* table name */
			  ALTCHR /* alt char code (on UCS2) */
			  )
CODE_CONV_ONE_OCTET_TO_UCS2_ALL(cc_iso8859_6_16_to_ucs2, /* function name */
                                tblIso8859_6_16ToUcs2) /* table name */
CODE_CONV_ISO8859_TO_UCS2(cc_asmo449_to_ucs2, /* function name */
                          tblAsmo449ToUcs2, /* table name */
                          ALTCHR /* alt char code (on UCS2) */
                          )


/* end of file */
