/*
 * $XFree86: xc/lib/fontconfig/src/fcmatch.c,v 1.21 2002/09/26 00:17:28 keithp Exp $
 *
 * Copyright � 2000 Keith Packard, member of The XFree86 Project, Inc.
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

#include <string.h>
#include <ctype.h>
#include "fcint.h"
#include <stdio.h>

static double
FcCompareInteger (char *object, FcValue value1, FcValue value2)
{
    int	v;
    
    if (value2.type != FcTypeInteger || value1.type != FcTypeInteger)
	return -1.0;
    v = value2.u.i - value1.u.i;
    if (v < 0)
	v = -v;
    return (double) v;
}

static double
FcCompareString (char *object, FcValue value1, FcValue value2)
{
    if (value2.type != FcTypeString || value1.type != FcTypeString)
	return -1.0;
    return (double) FcStrCmpIgnoreCase (value1.u.s, value2.u.s) != 0;
}

static double
FcCompareFamily (char *object, FcValue value1, FcValue value2)
{
    if (value2.type != FcTypeString || value1.type != FcTypeString)
	return -1.0;
    return (double) FcStrCmpIgnoreBlanksAndCase (value1.u.s, value2.u.s) != 0;
}

static double
FcCompareLang (char *object, FcValue value1, FcValue value2)
{
    FcLangResult    result;
    
    switch (value1.type) {
    case FcTypeLangSet:
	switch (value2.type) {
	case FcTypeLangSet:
	    result = FcLangSetCompare (value1.u.l, value2.u.l);
	    break;
	case FcTypeString:
	    result = FcLangSetHasLang (value1.u.l, value2.u.s);
	    break;
	default:
	    return -1.0;
	}
	break;
    case FcTypeString:
	switch (value2.type) {
	case FcTypeLangSet:
	    result = FcLangSetHasLang (value2.u.l, value1.u.s);
	    break;
	case FcTypeString:
	    result = FcLangCompare (value1.u.s, value2.u.s);
	    break;
	default:
	    return -1.0;
	}
	break;
    default:
	return -1.0;
    }
    switch (result) {
    case FcLangEqual:
	return 0;
    case FcLangDifferentCountry:
	return 1;
    case FcLangDifferentLang:
    default:
	return 2;
    }
}

static double
FcCompareBool (char *object, FcValue value1, FcValue value2)
{
    if (value2.type != FcTypeBool || value1.type != FcTypeBool)
	return -1.0;
    return (double) value2.u.b != value1.u.b;
}

static double
FcCompareCharSet (char *object, FcValue value1, FcValue value2)
{
    if (value2.type != FcTypeCharSet || value1.type != FcTypeCharSet)
	return -1.0;
    return (double) FcCharSetSubtractCount (value1.u.c, value2.u.c);
}

static double
FcCompareSize (char *object, FcValue value1, FcValue value2)
{
    double  v1, v2, v;

    switch (value1.type) {
    case FcTypeInteger:
	v1 = value1.u.i;
	break;
    case FcTypeDouble:
	v1 = value1.u.d;
	break;
    default:
	return -1;
    }
    switch (value2.type) {
    case FcTypeInteger:
	v2 = value2.u.i;
	break;
    case FcTypeDouble:
	v2 = value2.u.d;
	break;
    default:
	return -1;
    }
    if (v2 == 0)
	return 0;
    v = v2 - v1;
    if (v < 0)
	v = -v;
    return v;
}

typedef struct _FcMatcher {
    char	    *object;
    double	    (*compare) (char *object, FcValue value1, FcValue value2);
    int		    strong, weak;
} FcMatcher;

/*
 * Order is significant, it defines the precedence of
 * each value, earlier values are more significant than
 * later values
 */
static FcMatcher _FcMatchers [] = {
    { FC_FOUNDRY,	FcCompareString,	0, 0 },
#define MATCH_FOUNDRY	    0
    
    { FC_CHARSET,	FcCompareCharSet,	1, 1 },
#define MATCH_CHARSET	    1
    
    { FC_FAMILY,    	FcCompareFamily,	2, 4 },
#define MATCH_FAMILY	    2
    
    { FC_LANG,		FcCompareLang,		3, 3 },
#define MATCH_LANG	    3
    
    { FC_SPACING,	FcCompareInteger,	5, 5 },
#define MATCH_SPACING	    4
    
    { FC_PIXEL_SIZE,	FcCompareSize,		6, 6 },
#define MATCH_PIXEL_SIZE    5
    
    { FC_STYLE,		FcCompareString,	7, 7 },
#define MATCH_STYLE	    6
    
    { FC_SLANT,		FcCompareInteger,	8, 8 },
#define MATCH_SLANT	    7
    
    { FC_WEIGHT,	FcCompareInteger,	9, 9 },
#define MATCH_WEIGHT	    8
    
    { FC_ANTIALIAS,	FcCompareBool,		10, 10 },
#define MATCH_ANTIALIAS	    9
    
    { FC_RASTERIZER,	FcCompareString,	11, 11 },
#define MATCH_RASTERIZER    10
    
    { FC_OUTLINE,	FcCompareBool,		12, 12 },
#define MATCH_OUTLINE	    11

    { FC_FONTVERSION,	FcCompareInteger,	13, 13 },
#define MATCH_FONTVERSION   12
};

#define NUM_MATCH_VALUES    14

static FcBool
FcCompareValueList (const char  *object,
		    FcValueList	*v1orig,	/* pattern */
		    FcValueList *v2orig,	/* target */
		    FcValue	*bestValue,
		    double	*value,
		    FcResult	*result)
{
    FcValueList    *v1, *v2;
    double    	    v, best, bestStrong, bestWeak;
    int		    i;
    int		    j;
    
    /*
     * Locate the possible matching entry by examining the
     * first few characters in object
     */
    i = -1;
    switch (FcToLower (object[0])) {
    case 'f':
	switch (FcToLower (object[1])) {
	case 'o':
	    switch (FcToLower (object[2])) {
	    case 'u':
		i = MATCH_FOUNDRY; break;
	    case 'n':
		i = MATCH_FONTVERSION; break;
	    }
	    break;
	case 'a':
	    i = MATCH_FAMILY; break;
	}
	break;
    case 'c':
	i = MATCH_CHARSET; break;
    case 'a':
	i = MATCH_ANTIALIAS; break;
    case 'l':
	i = MATCH_LANG; break;
    case 's':
	switch (FcToLower (object[1])) {
	case 'p':
	    i = MATCH_SPACING; break;
	case 't':
	    i = MATCH_STYLE; break;
	case 'l':
	    i = MATCH_SLANT; break;
	}
	break;
    case 'p':
	i = MATCH_PIXEL_SIZE; break;
    case 'w':
	i = MATCH_WEIGHT; break;
    case 'r':
	i = MATCH_RASTERIZER; break;
    case 'o':
	i = MATCH_OUTLINE; break;
    }
    if (i == -1 || 
	FcStrCmpIgnoreCase ((FcChar8 *) _FcMatchers[i].object,
			    (FcChar8 *) object) != 0)
    {
	if (bestValue)
	    *bestValue = v2orig->value;
	return FcTrue;
    }
#if 0
    for (i = 0; i < NUM_MATCHER; i++)
    {
	if (!FcStrCmpIgnoreCase ((FcChar8 *) _FcMatchers[i].object,
				 (FcChar8 *) object))
	    break;
    }
    if (i == NUM_MATCHER)
    {
	if (bestValue)
	    *bestValue = v2orig->value;
	return FcTrue;
    }
#endif
    best = 1e99;
    bestStrong = 1e99;
    bestWeak = 1e99;
    j = 0;
    for (v1 = v1orig; v1; v1 = v1->next)
    {
	for (v2 = v2orig; v2; v2 = v2->next)
	{
	    v = (*_FcMatchers[i].compare) (_FcMatchers[i].object,
					    v1->value,
					    v2->value);
	    if (v < 0)
	    {
		*result = FcResultTypeMismatch;
		return FcFalse;
	    }
	    if (FcDebug () & FC_DBG_MATCHV)
		printf (" v %g j %d ", v, j);
	    v = v * 100 + j;
	    if (v < best)
	    {
		if (bestValue)
		    *bestValue = v2->value;
		best = v;
	    }
	    if (v1->binding == FcValueBindingStrong)
	    {
		if (v < bestStrong)
		    bestStrong = v;
	    }
	    else
	    {
		if (v < bestWeak)
		    bestWeak = v;
	    }
	}
	j++;
    }
    if (FcDebug () & FC_DBG_MATCHV)
    {
	printf (" %s: %g ", object, best);
	FcValueListPrint (v1orig);
	printf (", ");
	FcValueListPrint (v2orig);
	printf ("\n");
    }
    if (value)
    {
	int weak    = _FcMatchers[i].weak;
	int strong  = _FcMatchers[i].strong;
	if (weak == strong)
	    value[strong] += best;
	else
	{
	    value[weak] += bestWeak;
	    value[strong] += bestStrong;
	}
    }
    return FcTrue;
}

/*
 * Return a value indicating the distance between the two lists of
 * values
 */

static FcBool
FcCompare (FcPattern	*pat,
	   FcPattern	*fnt,
	   double	*value,
	   FcResult	*result)
{
    int		    i, i1, i2;
    
    for (i = 0; i < NUM_MATCH_VALUES; i++)
	value[i] = 0.0;
    
    i1 = 0;
    i2 = 0;
    while (i1 < pat->num && i2 < fnt->num)
    {
	i = strcmp (pat->elts[i1].object, fnt->elts[i2].object);
	if (i > 0)
	    i2++;
	else if (i < 0)
	    i1++;
	else
	{
	    if (!FcCompareValueList (pat->elts[i1].object,
				     pat->elts[i1].values,
				     fnt->elts[i2].values,
				     0,
				     value,
				     result))
		return FcFalse;
	    i1++;
	    i2++;
	}
    }
    return FcTrue;
#if 0
    for (i1 = 0; i1 < pat->num; i1++)
    {
	for (i2 = 0; i2 < fnt->num; i2++)
	{
	    if (!strcmp (pat->elts[i1].object, fnt->elts[i2].object))
	    {
		break;
	    }
	}
    }
    return FcTrue;
#endif
}

FcPattern *
FcFontRenderPrepare (FcConfig	    *config,
		     FcPattern	    *pat,
		     FcPattern	    *font)
{
    FcPattern	    *new;
    int		    i;
    FcPatternElt    *fe, *pe;
    FcValue	    v;
    FcResult	    result;
    
    new = FcPatternCreate ();
    if (!new)
	return 0;
    for (i = 0; i < font->num; i++)
    {
	fe = &font->elts[i];
	pe = FcPatternFindElt (pat, fe->object);
	if (pe)
	{
	    if (!FcCompareValueList (pe->object, pe->values, 
				     fe->values, &v, 0, &result))
	    {
		FcPatternDestroy (new);
		return 0;
	    }
	}
	else
	    v = fe->values->value;
	FcPatternAdd (new, fe->object, v, FcFalse);
    }
    for (i = 0; i < pat->num; i++)
    {
	pe = &pat->elts[i];
	fe = FcPatternFindElt (font, pe->object);
	if (!fe)
	    FcPatternAdd (new, pe->object, pe->values->value, FcTrue);
    }
    FcConfigSubstituteWithPat (config, new, pat, FcMatchFont);
    return new;
}

FcPattern *
FcFontSetMatch (FcConfig    *config,
		FcFontSet   **sets,
		int	    nsets,
		FcPattern   *p,
		FcResult    *result)
{
    double    	    score[NUM_MATCH_VALUES], bestscore[NUM_MATCH_VALUES];
    int		    f;
    FcFontSet	    *s;
    FcPattern	    *best;
    int		    i;
    int		    set;

    for (i = 0; i < NUM_MATCH_VALUES; i++)
	bestscore[i] = 0;
    best = 0;
    if (FcDebug () & FC_DBG_MATCH)
    {
	printf ("Match ");
	FcPatternPrint (p);
    }
    if (!config)
    {
	config = FcConfigGetCurrent ();
	if (!config)
	    return 0;
    }
    for (set = 0; set < nsets; set++)
    {
	s = sets[set];
	if (!s)
	    continue;
	for (f = 0; f < s->nfont; f++)
	{
	    if (FcDebug () & FC_DBG_MATCHV)
	    {
		printf ("Font %d ", f);
		FcPatternPrint (s->fonts[f]);
	    }
	    if (!FcCompare (p, s->fonts[f], score, result))
		return 0;
	    if (FcDebug () & FC_DBG_MATCHV)
	    {
		printf ("Score");
		for (i = 0; i < NUM_MATCH_VALUES; i++)
		{
		    printf (" %g", score[i]);
		}
		printf ("\n");
	    }
	    for (i = 0; i < NUM_MATCH_VALUES; i++)
	    {
		if (best && bestscore[i] < score[i])
		    break;
		if (!best || score[i] < bestscore[i])
		{
		    for (i = 0; i < NUM_MATCH_VALUES; i++)
			bestscore[i] = score[i];
		    best = s->fonts[f];
		    break;
		}
	    }
	}
    }
    if (FcDebug () & FC_DBG_MATCH)
    {
	printf ("Best score");
	for (i = 0; i < NUM_MATCH_VALUES; i++)
	    printf (" %g", bestscore[i]);
	FcPatternPrint (best);
    }
    if (!best)
    {
	*result = FcResultNoMatch;
	return 0;
    }
    return FcFontRenderPrepare (config, p, best);
}

FcPattern *
FcFontMatch (FcConfig	*config,
	     FcPattern	*p, 
	     FcResult	*result)
{
    FcFontSet	*sets[2];
    int		nsets;

    if (!config)
    {
	config = FcConfigGetCurrent ();
	if (!config)
	    return 0;
    }
    nsets = 0;
    if (config->fonts[FcSetSystem])
	sets[nsets++] = config->fonts[FcSetSystem];
    if (config->fonts[FcSetApplication])
	sets[nsets++] = config->fonts[FcSetApplication];
    return FcFontSetMatch (config, sets, nsets, p, result);
}

typedef struct _FcSortNode {
    FcPattern	*pattern;
    double	score[NUM_MATCH_VALUES];
} FcSortNode;

static int
FcSortCompare (const void *aa, const void *ab)
{
    FcSortNode  *a = *(FcSortNode **) aa;
    FcSortNode  *b = *(FcSortNode **) ab;
    double	*as = &a->score[0];
    double	*bs = &b->score[0];
    double	ad = 0, bd = 0;
    int         i;

    i = NUM_MATCH_VALUES;
    while (i-- && (ad = *as++) == (bd = *bs++))
	;
    return ad < bd ? -1 : ad > bd ? 1 : 0;
}

static FcBool
FcSortWalk (FcSortNode **n, int nnode, FcFontSet *fs, FcCharSet **cs, FcBool trim)
{
    FcCharSet	*ncs;
    FcSortNode	*node;

    while (nnode--)
    {
	node = *n++;
	if (FcPatternGetCharSet (node->pattern, FC_CHARSET, 0, &ncs) == 
	    FcResultMatch)
	{
	    /*
	     * If this font isn't a subset of the previous fonts,
	     * add it to the list
	     */
	    if (!trim || !*cs || !FcCharSetIsSubset (ncs, *cs))
	    {
		if (*cs)
		{
		    ncs = FcCharSetUnion (ncs, *cs);
		    if (!ncs)
			return FcFalse;
		    FcCharSetDestroy (*cs);
		}
		else
		    ncs = FcCharSetCopy (ncs);
		*cs = ncs;
		FcPatternReference (node->pattern);
		if (FcDebug () & FC_DBG_MATCH)
		{
		    printf ("Add ");
		    FcPatternPrint (node->pattern);
		}
		if (!FcFontSetAdd (fs, node->pattern))
		{
		    FcPatternDestroy (node->pattern);
		    return FcFalse;
		}
	    }
	}
    }
    return FcTrue;
}

void
FcFontSetSortDestroy (FcFontSet *fs)
{
    FcFontSetDestroy (fs);
}

FcFontSet *
FcFontSetSort (FcConfig	    *config,
	       FcFontSet    **sets,
	       int	    nsets,
	       FcPattern    *p,
	       FcBool	    trim,
	       FcCharSet    **csp,
	       FcResult	    *result)
{
    FcFontSet	    *ret;
    FcFontSet	    *s;
    FcSortNode	    *nodes;
    FcSortNode	    **nodeps, **nodep;
    int		    nnodes;
    FcSortNode	    *new;
    FcCharSet	    *cs;
    int		    set;
    int		    f;
    int		    i;

    if (FcDebug () & FC_DBG_MATCH)
    {
	printf ("Sort ");
	FcPatternPrint (p);
    }
    nnodes = 0;
    for (set = 0; set < nsets; set++)
    {
	s = sets[set];
	if (!s)
	    continue;
	nnodes += s->nfont;
    }
    if (!nnodes)
	goto bail0;
    /* freed below */
    nodes = malloc (nnodes * sizeof (FcSortNode) + nnodes * sizeof (FcSortNode *));
    if (!nodes)
	goto bail0;
    nodeps = (FcSortNode **) (nodes + nnodes);
    
    new = nodes;
    nodep = nodeps;
    for (set = 0; set < nsets; set++)
    {
	s = sets[set];
	if (!s)
	    continue;
	for (f = 0; f < s->nfont; f++)
	{
	    if (FcDebug () & FC_DBG_MATCHV)
	    {
		printf ("Font %d ", f);
		FcPatternPrint (s->fonts[f]);
	    }
	    new->pattern = s->fonts[f];
	    if (!FcCompare (p, new->pattern, new->score, result))
		goto bail1;
	    if (FcDebug () & FC_DBG_MATCHV)
	    {
		printf ("Score");
		for (i = 0; i < NUM_MATCH_VALUES; i++)
		{
		    printf (" %g", new->score[i]);
		}
		printf ("\n");
	    }
	    *nodep = new;
	    new++;
	    nodep++;
	}
    }

    nnodes = new - nodes;
    
    qsort (nodeps, nnodes, sizeof (FcSortNode *),
	   FcSortCompare);

    ret = FcFontSetCreate ();
    if (!ret)
	goto bail1;

    cs = 0;

    if (!FcSortWalk (nodeps, nnodes, ret, &cs, trim))
	goto bail2;

    if (csp)
	*csp = cs;
    else
	FcCharSetDestroy (cs);

    free (nodes);

    return ret;

bail2:
    if (cs)
	FcCharSetDestroy (cs);
    FcFontSetDestroy (ret);
bail1:
    free (nodes);
bail0:
    return 0;
}

FcFontSet *
FcFontSort (FcConfig	*config,
	    FcPattern	*p, 
	    FcBool	trim,
	    FcCharSet	**csp,
	    FcResult	*result)
{
    FcFontSet	*sets[2];
    int		nsets;

    if (!config)
    {
	config = FcConfigGetCurrent ();
	if (!config)
	    return 0;
    }
    nsets = 0;
    if (config->fonts[FcSetSystem])
	sets[nsets++] = config->fonts[FcSetSystem];
    if (config->fonts[FcSetApplication])
	sets[nsets++] = config->fonts[FcSetApplication];
    return FcFontSetSort (config, sets, nsets, p, trim, csp, result);
}
