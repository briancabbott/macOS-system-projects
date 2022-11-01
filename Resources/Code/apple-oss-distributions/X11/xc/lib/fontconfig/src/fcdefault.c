/*
 * $XFree86: xc/lib/fontconfig/src/fcdefault.c,v 1.3 2002/09/26 00:17:28 keithp Exp $
 *
 * Copyright � 2001 Keith Packard, member of The XFree86 Project, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "fcint.h"
#include <locale.h>

static struct {
    char	*field;
    FcBool	value;
} FcBoolDefaults[] = {
    { FC_HINTING,	    FcTrue	},  /* !FT_LOAD_NO_HINTING */
    { FC_VERTICAL_LAYOUT,   FcFalse	},  /* FC_LOAD_VERTICAL_LAYOUT */
    { FC_AUTOHINT,	    FcFalse	},  /* FC_LOAD_FORCE_AUTOHINT */
    { FC_GLOBAL_ADVANCE,    FcTrue	},  /* !FC_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH */
};

#define NUM_FC_BOOL_DEFAULTS	(sizeof FcBoolDefaults / sizeof FcBoolDefaults[0])

void
FcDefaultSubstitute (FcPattern *pattern)
{
    FcValue v;
    int	    i;

    if (FcPatternGet (pattern, FC_STYLE, 0, &v) == FcResultNoMatch)
    {
	if (FcPatternGet (pattern, FC_WEIGHT, 0, &v) == FcResultNoMatch )
	{
	    FcPatternAddInteger (pattern, FC_WEIGHT, FC_WEIGHT_MEDIUM);
	}
	if (FcPatternGet (pattern, FC_SLANT, 0, &v) == FcResultNoMatch)
	{
	    FcPatternAddInteger (pattern, FC_SLANT, FC_SLANT_ROMAN);
	}
    }

    for (i = 0; i < NUM_FC_BOOL_DEFAULTS; i++)
	if (FcPatternGet (pattern, FcBoolDefaults[i].field, 0, &v) == FcResultNoMatch)
	    FcPatternAddBool (pattern, FcBoolDefaults[i].field, FcBoolDefaults[i].value);
    
    if (FcPatternGet (pattern, FC_PIXEL_SIZE, 0, &v) == FcResultNoMatch)
    {
	double	dpi, size, scale;

	if (FcPatternGetDouble (pattern, FC_SIZE, 0, &size) != FcResultMatch)
	{
	    size = 12.0;
	    (void) FcPatternDel (pattern, FC_SIZE);
	    FcPatternAddDouble (pattern, FC_SIZE, size);
	}
	if (FcPatternGetDouble (pattern, FC_SCALE, 0, &scale) != FcResultMatch)
	{
	    scale = 1.0;
	    (void) FcPatternDel (pattern, FC_SCALE);
	    FcPatternAddDouble (pattern, FC_SCALE, scale);
	}
	size *= scale;
	if (FcPatternGetDouble (pattern, FC_DPI, 0, &dpi) != FcResultMatch)
	{
	    dpi = 75.0;
	    (void) FcPatternDel (pattern, FC_DPI);
	    FcPatternAddDouble (pattern, FC_DPI, dpi);
	}
	size *= dpi / 72.0;
	FcPatternAddDouble (pattern, FC_PIXEL_SIZE, size);
    }

    if (FcPatternGet (pattern, FC_LANG, 0, &v) == FcResultNoMatch)
    {
	char	*lang;
	char	*territory;
	char	*after;
	int	lang_len, territory_len;
	char	lang_local[128];
	char	*ctype = setlocale (LC_CTYPE, NULL);

	/*
	 * Check if setlocale (LC_ALL, "") has been called
	 */
	if (!ctype || !strcmp (ctype, "C"))
	{
	    ctype = getenv ("LC_ALL");
	    if (!ctype)
	    {
		ctype = getenv ("LC_CTYPE");
		if (!ctype)
		    ctype = getenv ("LANG");
	    }
	}
	if (ctype)
	{
	    lang = ctype;
	    territory = strchr (ctype, '_');
	    if (territory)
	    {
		lang_len = territory - lang;
		territory = territory + 1;
		after = strchr (territory, '.');
		if (!after)
		{
		    after = strchr (territory, '@');
		    if (!after)
			after = territory + strlen (territory);
		}
		territory_len = after - territory;
		if (lang_len + 1 + territory_len + 1 <= sizeof (lang_local))
		{
		    strncpy (lang_local, lang, lang_len);
		    lang_local[lang_len] = '-';
		    strncpy (lang_local + lang_len + 1, territory, territory_len);
		    lang_local[lang_len + 1 + territory_len] = '\0';
		    FcPatternAddString (pattern, FC_LANG, (FcChar8 *) lang_local);
		}
	    }
	    else
		FcPatternAddString (pattern, FC_LANG, (FcChar8 *) lang);
	}
    }
    if (FcPatternGet (pattern, FC_FONTVERSION, 0, &v) == FcResultNoMatch)
    {
	FcPatternAddInteger (pattern, FC_FONTVERSION, 0x7fffffff);
    }
}
