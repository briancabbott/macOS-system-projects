/*
 * $XFree86: xc/lib/fontconfig/src/fclang.c,v 1.8 2002/12/14 02:03:59 dawes Exp $
 *
 * Copyright � 2002 Keith Packard, member of The XFree86 Project, Inc.
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

typedef struct {
    FcChar8	*lang;
    FcCharSet	charset;
} FcLangCharSet;

#include "../fc-lang/fclang.h"

struct _FcLangSet {
    FcChar32	map[NUM_LANG_SET_MAP];
    FcStrSet	*extra;
};

#define FcLangSetBitSet(ls, id)	((ls)->map[(id)>>5] |= ((FcChar32) 1 << ((id) & 0x1f)))
#define FcLangSetBitGet(ls, id) (((ls)->map[(id)>>5] >> ((id) & 0x1f)) & 1)

FcLangSet *
FcFreeTypeLangSet (const FcCharSet  *charset, 
		   const FcChar8    *exclusiveLang)
{
    int		    i;
    FcChar32	    missing;
    const FcCharSet *exclusiveCharset = 0;
    FcLangSet	    *ls;
    

    if (exclusiveLang)
	exclusiveCharset = FcCharSetForLang (exclusiveLang);
    ls = FcLangSetCreate ();
    if (!ls)
	return 0;
    for (i = 0; i < NUM_LANG_CHAR_SET; i++)
    {
	/*
	 * Check for Han charsets to make fonts
	 * which advertise support for a single language
	 * not support other Han languages
	 */
	if (exclusiveCharset &&
	    FcFreeTypeIsExclusiveLang (fcLangCharSets[i].lang) &&
	    fcLangCharSets[i].charset.leaves != exclusiveCharset->leaves)
	{
	    continue;
	}
	missing = FcCharSetSubtractCount (&fcLangCharSets[i].charset, charset);
        if (FcDebug() & FC_DBG_SCANV)
	{
	    if (missing && missing < 10)
	    {
		FcCharSet   *missed = FcCharSetSubtract (&fcLangCharSets[i].charset, 
							 charset);
		FcChar32    ucs4;
		FcChar32    map[FC_CHARSET_MAP_SIZE];
		FcChar32    next;

		printf ("\n%s(%d) ", fcLangCharSets[i].lang, missing);
		printf ("{");
		for (ucs4 = FcCharSetFirstPage (missed, map, &next);
		     ucs4 != FC_CHARSET_DONE;
		     ucs4 = FcCharSetNextPage (missed, map, &next))
		{
		    int	    i, j;
		    for (i = 0; i < FC_CHARSET_MAP_SIZE; i++)
			if (map[i])
			{
			    for (j = 0; j < 32; j++)
				if (map[i] & (1 << j))
				    printf (" %04x", ucs4 + i * 32 + j);
			}
		}
		printf (" }\n\t");
		FcCharSetDestroy (missed);
	    }
	    else
		printf ("%s(%d) ", fcLangCharSets[i].lang, missing);
	}
	if (!missing)
	    FcLangSetBitSet (ls, i);
    }

    if (FcDebug() & FC_DBG_SCANV)
	printf ("\n");
    
    
    return ls;
}

#define FcLangEnd(c)	((c) == '-' || (c) == '\0')

FcLangResult
FcLangCompare (const FcChar8 *s1, const FcChar8 *s2)
{
    FcChar8	    c1, c2;
    FcLangResult    result = FcLangDifferentLang;

    for (;;)
    {
	c1 = *s1++;
	c2 = *s2++;
	
	c1 = FcToLower (c1);
	c2 = FcToLower (c2);
	if (c1 != c2)
	{
	    if (FcLangEnd (c1) && FcLangEnd (c2))
		result = FcLangDifferentCountry;
	    return result;
	}
	else if (!c1)
	    return FcLangEqual;
	else if (c1 == '-')
	    result = FcLangDifferentCountry;
    }
}

const FcCharSet *
FcCharSetForLang (const FcChar8 *lang)
{
    int		i;
    int		country = -1;
    for (i = 0; i < NUM_LANG_CHAR_SET; i++)
    {
	switch (FcLangCompare (lang, fcLangCharSets[i].lang)) {
	case FcLangEqual:
	    return &fcLangCharSets[i].charset;
	case FcLangDifferentCountry:
	    if (country == -1)
		country = i;
	default:
	    break;
	}
    }
    if (country == -1)
	return 0;
    return &fcLangCharSets[i].charset;
}

FcLangSet *
FcLangSetCreate (void)
{
    FcLangSet	*ls;

    ls = malloc (sizeof (FcLangSet));
    if (!ls)
	return 0;
    FcMemAlloc (FC_MEM_LANGSET, sizeof (FcLangSet));
    memset (ls->map, '\0', sizeof (ls->map));
    ls->extra = 0;
    return ls;
}

void
FcLangSetDestroy (FcLangSet *ls)
{
    if (ls->extra)
	FcStrSetDestroy (ls->extra);
    FcMemFree (FC_MEM_LANGSET, sizeof (FcLangSet));
    free (ls);
}

FcLangSet *
FcLangSetCopy (const FcLangSet *ls)
{
    FcLangSet	*new;

    new = FcLangSetCreate ();
    if (!new)
	goto bail0;
    memcpy (new->map, ls->map, sizeof (new->map));
    if (ls->extra)
    {
	FcStrList	*list;
	FcChar8		*extra;
	
	new->extra = FcStrSetCreate ();
	if (!new->extra)
	    goto bail1;

	list = FcStrListCreate (ls->extra);	
	if (!list)
	    goto bail1;
	
	while ((extra = FcStrListNext (list)))
	    if (!FcStrSetAdd (new->extra, extra))
	    {
		FcStrListDone (list);
		goto bail1;
	    }
	FcStrListDone (list);
    }
    return new;
bail1:
    FcLangSetDestroy (new);
bail0:
    return 0;
}

static int
FcLangSetIndex (const FcChar8 *lang)
{
    int	    low, high, mid;
    int	    cmp;

    low = 0;
    high = NUM_LANG_CHAR_SET - 1;
    while (low <= high)
    {
	mid = (high + low) >> 1;
	cmp = FcStrCmpIgnoreCase (fcLangCharSets[mid].lang, lang);
	if (cmp == 0) 
	    return mid;
	if (cmp < 0)
	    low = mid + 1;
	else
	    high = mid - 1;
    }
    if (cmp < 0)
	mid++;
    return -(mid + 1);
}

FcBool
FcLangSetAdd (FcLangSet *ls, const FcChar8 *lang)
{
    int	    id;

    id = FcLangSetIndex (lang);
    if (id >= 0)
    {
	FcLangSetBitSet (ls, id);
	return FcTrue;
    }
    if (!ls->extra)
    {
	ls->extra = FcStrSetCreate ();
	if (!ls->extra)
	    return FcFalse;
    }
    return FcStrSetAdd (ls->extra, lang);
}

FcLangResult
FcLangSetHasLang (const FcLangSet *ls, const FcChar8 *lang)
{
    int		    id;
    FcLangResult    best, r;
    int		    i;

    id = FcLangSetIndex (lang);
    if (id < 0)
	id = -id - 1;
    else if (FcLangSetBitGet (ls, id))
	return FcLangEqual;
    best = FcLangDifferentLang;
    for (i = id - 1; i >= 0; i--)
    {
	r = FcLangCompare (lang, fcLangCharSets[i].lang);
	if (r == FcLangDifferentLang)
	    break;
	if (FcLangSetBitGet (ls, i) && r < best)
	    best = r;
    }
    for (i = id; i < NUM_LANG_CHAR_SET; i++)
    {
	r = FcLangCompare (lang, fcLangCharSets[i].lang);
	if (r == FcLangDifferentLang)
	    break;
	if (FcLangSetBitGet (ls, i) && r < best)
	    best = r;
    }
    if (ls->extra)
    {
	FcStrList	*list = FcStrListCreate (ls->extra);
	FcChar8		*extra;
	FcLangResult	r;
	
	if (list)
	{
	    while (best > FcLangEqual && (extra = FcStrListNext (list)))
	    {
		r = FcLangCompare (lang, extra);
		if (r < best)
		    best = r;
	    }
	    FcStrListDone (list);
	}
    }
    return best;
}

static FcLangResult
FcLangSetCompareStrSet (const FcLangSet *ls, FcStrSet *set)
{
    FcStrList	    *list = FcStrListCreate (set);
    FcLangResult    r, best = FcLangDifferentLang;
    FcChar8	    *extra;

    if (list)
    {
	while (best > FcLangEqual && (extra = FcStrListNext (list)))
	{
	    r = FcLangSetHasLang (ls, extra);
	    if (r < best)
		best = r;
	}
	FcStrListDone (list);
    }
    return best;
}

FcLangResult
FcLangSetCompare (const FcLangSet *lsa, const FcLangSet *lsb)
{
    int		    i, j;
    FcLangResult    best, r;

    for (i = 0; i < NUM_LANG_SET_MAP; i++)
	if (lsa->map[i] & lsb->map[i])
	    return FcLangEqual;
    best = FcLangDifferentLang;
    for (j = 0; j < NUM_COUNTRY_SET; j++)
	for (i = 0; i < NUM_LANG_SET_MAP; i++)
	    if ((lsa->map[i] & fcLangCountrySets[j][i]) &&
		(lsb->map[i] & fcLangCountrySets[j][i]))
	    {
		best = FcLangDifferentCountry;
		break;
	    }
    if (lsa->extra)
    {
	r = FcLangSetCompareStrSet (lsb, lsa->extra);
	if (r < best)
	    best = r;
    }
    if (best > FcLangEqual && lsb->extra)
    {
	r = FcLangSetCompareStrSet (lsa, lsb->extra);
	if (r < best)
	    best = r;
    }
    return best;
}

/*
 * Used in computing values -- mustn't allocate any storage
 */
FcLangSet *
FcLangSetPromote (const FcChar8 *lang)
{
    static FcLangSet	ls;
    static FcStrSet	strs;
    static FcChar8	*str;
    int			id;

    memset (ls.map, '\0', sizeof (ls.map));
    ls.extra = 0;
    id = FcLangSetIndex (lang);
    if (id > 0)
    {
	FcLangSetBitSet (&ls, id);
    }
    else
    {
	ls.extra = &strs;
	strs.num = 1;
	strs.size = 1;
	strs.strs = &str;
	strs.ref = 1;
	str = (FcChar8 *) lang;
    }
    return &ls;
}

FcChar32
FcLangSetHash (const FcLangSet *ls)
{
    FcChar32	h = 0;
    int		i;

    for (i = 0; i < NUM_LANG_SET_MAP; i++)
	h ^= ls->map[i];
    if (ls->extra)
	h ^= ls->extra->num;
    return h;
}

FcLangSet *
FcNameParseLangSet (const FcChar8 *string)
{
    FcChar8	    lang[32];
    const FcChar8   *end, *next;
    FcLangSet	    *ls;

    ls = FcLangSetCreate ();
    if (!ls)
	goto bail0;

    while (string && *string) 
    {
	end = (FcChar8 *) strchr ((char *) string, '|');
	if (!end)
	{
	    end = string + strlen ((char *) string);
	    next = end;
	}
	else
	    next = end + 1;
	if (end - string < sizeof (lang) - 1)
	{
	    strncpy ((char *) lang, (char *) string, end - string);
	    lang[end-string] = '\0';
	    if (!FcLangSetAdd (ls, lang))
		goto bail1;
	}
	string = next;
    }
    return ls;
bail1:
    FcLangSetDestroy (ls);
bail0:
    return 0;
}

FcBool
FcNameUnparseLangSet (FcStrBuf *buf, const FcLangSet *ls)
{
    int		i, bit;
    FcChar32	bits;
    FcBool	first = FcTrue;

    for (i = 0; i < NUM_LANG_SET_MAP; i++)
    {
	if ((bits = ls->map[i]))
	{
	    for (bit = 0; bit <= 31; bit++)
		if (bits & (1 << bit))
		{
		    int id = (i << 5) | bit;
		    if (!first)
			if (!FcStrBufChar (buf, '|'))
			    return FcFalse;
		    if (!FcStrBufString (buf, fcLangCharSets[id].lang))
			return FcFalse;
		    first = FcFalse;
		}
	}
    }
    if (ls->extra)
    {
	FcStrList   *list = FcStrListCreate (ls->extra);
	FcChar8	    *extra;

	if (!list)
	    return FcFalse;
	while ((extra = FcStrListNext (list)))
	{
	    if (!first)
		if (!FcStrBufChar (buf, '|'))
		    return FcFalse;
	    if (!FcStrBufString (buf, extra));
		return FcFalse;
	    first = FcFalse;
	}
    }
    return FcTrue;
}

FcBool
FcLangSetEqual (const FcLangSet *lsa, const FcLangSet *lsb)
{
    int	    i;

    for (i = 0; i < NUM_LANG_SET_MAP; i++)
    {
	if (lsa->map[i] != lsb->map[i])
	    return FcFalse;
    }
    if (!lsa->extra && !lsb->extra)
	return FcTrue;
    if (lsa->extra && lsb->extra)
	return FcStrSetEqual (lsa->extra, lsb->extra);
    return FcFalse;
}
