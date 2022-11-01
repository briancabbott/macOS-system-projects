/*
 * $XFree86: xc/lib/fontconfig/src/fccfg.c,v 1.24 2002/12/21 02:31:53 dawes Exp $
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

#include "fcint.h"

FcConfig    *_fcConfig;

FcConfig *
FcConfigCreate (void)
{
    FcSetName	set;
    FcConfig	*config;

    config = malloc (sizeof (FcConfig));
    if (!config)
	goto bail0;
    FcMemAlloc (FC_MEM_CONFIG, sizeof (FcConfig));
    
    config->configDirs = FcStrSetCreate ();
    if (!config->configDirs)
	goto bail1;
    
    config->configFiles = FcStrSetCreate ();
    if (!config->configFiles)
	goto bail2;
    
    config->fontDirs = FcStrSetCreate ();
    if (!config->fontDirs)
	goto bail3;
    
    config->cache = 0;
    if (!FcConfigSetCache (config, (FcChar8 *) ("~/" FC_USER_CACHE_FILE)))
	goto bail4;

    config->blanks = 0;

    config->substPattern = 0;
    config->substFont = 0;
    config->maxObjects = 0;
    for (set = FcSetSystem; set <= FcSetApplication; set++)
	config->fonts[set] = 0;

    config->rescanTime = time(0);
    config->rescanInterval = 30;    
    
    return config;

bail4:
    FcStrSetDestroy (config->fontDirs);
bail3:
    FcStrSetDestroy (config->configFiles);
bail2:
    FcStrSetDestroy (config->configDirs);
bail1:
    free (config);
    FcMemFree (FC_MEM_CONFIG, sizeof (FcConfig));
bail0:
    return 0;
}

typedef struct _FcFileTime {
    time_t  time;
    FcBool  set;
} FcFileTime;

static FcFileTime
FcConfigNewestFile (FcStrSet *files)
{
    FcStrList	    *list = FcStrListCreate (files);
    FcFileTime	    newest = { 0, FcFalse };
    FcChar8	    *file;
    struct  stat    statb;

    if (list)
    {
	while ((file = FcStrListNext (list)))
	    if (stat ((char *) file, &statb) == 0)
		if (!newest.set || statb.st_mtime - newest.time > 0)
		    newest.time = statb.st_mtime;
	FcStrListDone (list);
    }
    return newest;
}

FcBool
FcConfigUptoDate (FcConfig *config)
{
    FcFileTime	config_time, font_time;
    time_t	now = time(0);
    if (!config)
    {
	config = FcConfigGetCurrent ();
	if (!config)
	    return FcFalse;
    }
    config_time = FcConfigNewestFile (config->configFiles);
    font_time = FcConfigNewestFile (config->configDirs);
    if ((config_time.set && config_time.time - config->rescanTime > 0) ||
	(font_time.set && font_time.time - config->rescanTime) > 0)
    {
	return FcFalse;
    }
    config->rescanTime = now;
    return FcTrue;
}

static void
FcSubstDestroy (FcSubst *s)
{
    FcSubst *n;
    
    while (s)
    {
	n = s->next;
	FcTestDestroy (s->test);
	FcEditDestroy (s->edit);
	s = n;
    }
}

void
FcConfigDestroy (FcConfig *config)
{
    FcSetName	set;

    if (config == _fcConfig)
	_fcConfig = 0;

    FcStrSetDestroy (config->configDirs);
    FcStrSetDestroy (config->fontDirs);
    FcStrSetDestroy (config->configFiles);

    FcStrFree (config->cache);

    FcSubstDestroy (config->substPattern);
    FcSubstDestroy (config->substFont);
    for (set = FcSetSystem; set <= FcSetApplication; set++)
	if (config->fonts[set])
	    FcFontSetDestroy (config->fonts[set]);
    free (config);
    FcMemFree (FC_MEM_CONFIG, sizeof (FcConfig));
}

/*
 * Scan the current list of directories in the configuration
 * and build the set of available fonts. Update the
 * per-user cache file to reflect the new configuration
 */

FcBool
FcConfigBuildFonts (FcConfig *config)
{
    FcFontSet	    *fonts;
    FcGlobalCache   *cache;
    FcStrList	    *list;
    FcChar8	    *dir;

    fonts = FcFontSetCreate ();
    if (!fonts)
	goto bail0;
    
    cache = FcGlobalCacheCreate ();
    if (!cache)
	goto bail1;

    FcGlobalCacheLoad (cache, config->cache);

    list = FcConfigGetFontDirs (config);
    if (!list)
	goto bail1;

    while ((dir = FcStrListNext (list)))
    {
	if (FcDebug () & FC_DBG_FONTSET)
	    printf ("scan dir %s\n", dir);
	FcDirScan (fonts, config->fontDirs, cache, config->blanks, dir, FcFalse);
    }
    
    FcStrListDone (list);
    
    if (FcDebug () & FC_DBG_FONTSET)
	FcFontSetPrint (fonts);

    FcGlobalCacheSave (cache, config->cache);
    FcGlobalCacheDestroy (cache);

    FcConfigSetFonts (config, fonts, FcSetSystem);
    
    return FcTrue;
bail1:
    FcFontSetDestroy (fonts);
bail0:
    return FcFalse;
}

FcBool
FcConfigSetCurrent (FcConfig *config)
{
    if (!config->fonts)
	if (!FcConfigBuildFonts (config))
	    return FcFalse;

    if (_fcConfig)
	FcConfigDestroy (_fcConfig);
    _fcConfig = config;
    return FcTrue;
}

FcConfig *
FcConfigGetCurrent (void)
{
    if (!_fcConfig)
	if (!FcInit ())
	    return 0;
    return _fcConfig;
}

FcBool
FcConfigAddConfigDir (FcConfig	    *config,
		      const FcChar8 *d)
{
    return FcStrSetAddFilename (config->configDirs, d);
}

FcStrList *
FcConfigGetConfigDirs (FcConfig   *config)
{
    if (!config)
    {
	config = FcConfigGetCurrent ();
	if (!config)
	    return 0;
    }
    return FcStrListCreate (config->configDirs);
}

FcBool
FcConfigAddFontDir (FcConfig	    *config,
		    const FcChar8   *d)
{
    return FcStrSetAddFilename (config->fontDirs, d);
}

FcBool
FcConfigAddDir (FcConfig	    *config,
		const FcChar8	    *d)
{
    return (FcConfigAddConfigDir (config, d) && 
	    FcConfigAddFontDir (config, d));
}

FcStrList *
FcConfigGetFontDirs (FcConfig	*config)
{
    if (!config)
    {
	config = FcConfigGetCurrent ();
	if (!config)
	    return 0;
    }
    return FcStrListCreate (config->fontDirs);
}

FcBool
FcConfigAddConfigFile (FcConfig	    *config,
		       const FcChar8   *f)
{
    FcBool	ret;
    FcChar8	*file = FcConfigFilename (f);
    
    if (!file)
	return FcFalse;
    
    ret = FcStrSetAdd (config->configFiles, file);
    FcStrFree (file);
    return ret;
}

FcStrList *
FcConfigGetConfigFiles (FcConfig    *config)
{
    if (!config)
    {
	config = FcConfigGetCurrent ();
	if (!config)
	    return 0;
    }
    return FcStrListCreate (config->configFiles);
}

FcBool
FcConfigSetCache (FcConfig	*config,
		  const FcChar8	*c)
{
    FcChar8    *new = FcStrCopyFilename (c);
    
    if (!new)
	return FcFalse;
    if (config->cache)
	FcStrFree (config->cache);
    config->cache = new;
    return FcTrue;
}

FcChar8 *
FcConfigGetCache (FcConfig  *config)
{
    if (!config)
    {
	config = FcConfigGetCurrent ();
	if (!config)
	    return 0;
    }
    return config->cache;
}

FcFontSet *
FcConfigGetFonts (FcConfig	*config,
		  FcSetName	set)
{
    if (!config)
    {
	config = FcConfigGetCurrent ();
	if (!config)
	    return 0;
    }
    return config->fonts[set];
}

void
FcConfigSetFonts (FcConfig	*config,
		  FcFontSet	*fonts,
		  FcSetName	set)
{
    if (config->fonts[set])
	FcFontSetDestroy (config->fonts[set]);
    config->fonts[set] = fonts;
}



FcBlanks *
FcConfigGetBlanks (FcConfig	*config)
{
    if (!config)
    {
	config = FcConfigGetCurrent ();
	if (!config)
	    return 0;
    }
    return config->blanks;
}

FcBool
FcConfigAddBlank (FcConfig	*config,
		  FcChar32    	blank)
{
    FcBlanks	*b;
    
    b = config->blanks;
    if (!b)
    {
	b = FcBlanksCreate ();
	if (!b)
	    return FcFalse;
    }
    if (!FcBlanksAdd (b, blank))
	return FcFalse;
    config->blanks = b;
    return FcTrue;
}

int
FcConfigGetRescanInverval (FcConfig *config)
{
    if (!config)
    {
	config = FcConfigGetCurrent ();
	if (!config)
	    return 0;
    }
    return config->rescanInterval;
}

FcBool
FcConfigSetRescanInverval (FcConfig *config, int rescanInterval)
{
    if (!config)
    {
	config = FcConfigGetCurrent ();
	if (!config)
	    return FcFalse;
    }
    config->rescanInterval = rescanInterval;
    return FcTrue;
}

FcBool
FcConfigAddEdit (FcConfig	*config,
		 FcTest		*test,
		 FcEdit		*edit,
		 FcMatchKind	kind)
{
    FcSubst	*subst, **prev;
    FcTest	*t;
    int		num;

    subst = (FcSubst *) malloc (sizeof (FcSubst));
    if (!subst)
	return FcFalse;
    FcMemAlloc (FC_MEM_SUBST, sizeof (FcSubst));
    if (kind == FcMatchPattern)
	prev = &config->substPattern;
    else
	prev = &config->substFont;
    for (; *prev; prev = &(*prev)->next);
    *prev = subst;
    subst->next = 0;
    subst->test = test;
    subst->edit = edit;
    num = 0;
    for (t = test; t; t = t->next)
    {
	if (t->kind == FcMatchDefault)
	    t->kind = kind;
	num++;
    }
    if (config->maxObjects < num)
	config->maxObjects = num;
    if (FcDebug () & FC_DBG_EDIT)
    {
	printf ("Add Subst ");
	FcSubstPrint (subst);
    }
    return FcTrue;
}

typedef struct _FcSubState {
    FcPatternElt   *elt;
    FcValueList    *value;
} FcSubState;

static FcValue
FcConfigPromote (FcValue v, FcValue u)
{
    if (v.type == FcTypeInteger)
    {
	v.type = FcTypeDouble;
	v.u.d = (double) v.u.i;
    }
    else if (v.type == FcTypeVoid && u.type == FcTypeMatrix)
    {
	v.u.m = &FcIdentityMatrix;
	v.type = FcTypeMatrix;
    }
    else if (v.type == FcTypeString && u.type == FcTypeLangSet)
    {
	v.u.l = FcLangSetPromote (v.u.s);
	v.type = FcTypeLangSet;
    }
    return v;
}

FcBool
FcConfigCompareValue (FcValue	m,
		      FcOp	op,
		      FcValue	v)
{
    FcBool    ret = FcFalse;
    
    m = FcConfigPromote (m, v);
    v = FcConfigPromote (v, m);
    if (m.type == v.type) 
    {
	switch (m.type) {
	case FcTypeInteger:
	    break;	/* FcConfigPromote prevents this from happening */
	case FcTypeDouble:
	    switch (op) {
	    case FcOpEqual:
	    case FcOpContains:
		ret = m.u.d == v.u.d;
		break;
	    case FcOpNotEqual:
	    case FcOpNotContains:
		ret = m.u.d != v.u.d;
		break;
	    case FcOpLess:    
		ret = m.u.d < v.u.d;
		break;
	    case FcOpLessEqual:    
		ret = m.u.d <= v.u.d;
		break;
	    case FcOpMore:    
		ret = m.u.d > v.u.d;
		break;
	    case FcOpMoreEqual:    
		ret = m.u.d >= v.u.d;
		break;
	    default:
		break;
	    }
	    break;
	case FcTypeBool:
	    switch (op) {
	    case FcOpEqual:    
	    case FcOpContains:
		ret = m.u.b == v.u.b;
		break;
	    case FcOpNotEqual:
	    case FcOpNotContains:
		ret = m.u.b != v.u.b;
		break;
	    default:
		break;
	    }
	    break;
	case FcTypeString:
	    switch (op) {
	    case FcOpEqual:    
	    case FcOpContains:
		ret = FcStrCmpIgnoreCase (m.u.s, v.u.s) == 0;
		break;
	    case FcOpNotEqual:
	    case FcOpNotContains:
		ret = FcStrCmpIgnoreCase (m.u.s, v.u.s) != 0;
		break;
	    default:
		break;
	    }
	    break;
	case FcTypeMatrix:
	    switch (op) {
	    case FcOpEqual:
	    case FcOpContains:
		ret = FcMatrixEqual (m.u.m, v.u.m);
		break;
	    case FcOpNotEqual:
	    case FcOpNotContains:
		ret = !FcMatrixEqual (m.u.m, v.u.m);
		break;
	    default:
		break;
	    }
	    break;
	case FcTypeCharSet:
	    switch (op) {
	    case FcOpContains:
		/* m contains v if v is a subset of m */
		ret = FcCharSetIsSubset (v.u.c, m.u.c);
		break;
	    case FcOpNotContains:
		/* m contains v if v is a subset of m */
		ret = !FcCharSetIsSubset (v.u.c, m.u.c);
		break;
	    case FcOpEqual:
		ret = FcCharSetEqual (m.u.c, v.u.c);
		break;
	    case FcOpNotEqual:
		ret = !FcCharSetEqual (m.u.c, v.u.c);
		break;
	    default:
		break;
	    }
	    break;
	case FcTypeLangSet:
	    switch (op) {
	    case FcOpContains:
		ret = FcLangSetCompare (v.u.l, m.u.l) != FcLangDifferentLang;
		break;
	    case FcOpNotContains:
		ret = FcLangSetCompare (v.u.l, m.u.l) == FcLangDifferentLang;
		break;
	    case FcOpEqual:
		ret = FcLangSetEqual (v.u.l, m.u.l);
		break;
	    case FcOpNotEqual:
		ret = !FcLangSetEqual (v.u.l, m.u.l);
		break;
	    default:
		break;
	    }
	    break;
	case FcTypeVoid:
	    switch (op) {
	    case FcOpEqual:
	    case FcOpContains:
		ret = FcTrue;
		break;
	    default:
		break;
	    }
	    break;
	case FcTypeFTFace:
	    switch (op) {
	    case FcOpEqual:
	    case FcOpContains:
		ret = m.u.f == v.u.f;
		break;
	    case FcOpNotEqual:
	    case FcOpNotContains:
		ret = m.u.f != v.u.f;
		break;
	    default:
		break;
	    }
	    break;
	}
    }
    else
    {
	if (op == FcOpNotEqual || op == FcOpNotContains)
	    ret = FcTrue;
    }
    return ret;
}


static FcValue
FcConfigEvaluate (FcPattern *p, FcExpr *e)
{
    FcValue	v, vl, vr;
    FcResult	r;
    FcMatrix	*m;
    
    switch (e->op) {
    case FcOpInteger:
	v.type = FcTypeInteger;
	v.u.i = e->u.ival;
	break;
    case FcOpDouble:
	v.type = FcTypeDouble;
	v.u.d = e->u.dval;
	break;
    case FcOpString:
	v.type = FcTypeString;
	v.u.s = e->u.sval;
	v = FcValueSave (v);
	break;
    case FcOpMatrix:
	v.type = FcTypeMatrix;
	v.u.m = e->u.mval;
	v = FcValueSave (v);
	break;
    case FcOpCharSet:
	v.type = FcTypeCharSet;
	v.u.c = e->u.cval;
	v = FcValueSave (v);
	break;
    case FcOpBool:
	v.type = FcTypeBool;
	v.u.b = e->u.bval;
	break;
    case FcOpField:
	r = FcPatternGet (p, e->u.field, 0, &v);
	if (r != FcResultMatch)
	    v.type = FcTypeVoid;
	break;
    case FcOpConst:
	if (FcNameConstant (e->u.constant, &v.u.i))
	    v.type = FcTypeInteger;
	else
	    v.type = FcTypeVoid;
	break;
    case FcOpQuest:
	vl = FcConfigEvaluate (p, e->u.tree.left);
	if (vl.type == FcTypeBool)
	{
	    if (vl.u.b)
		v = FcConfigEvaluate (p, e->u.tree.right->u.tree.left);
	    else
		v = FcConfigEvaluate (p, e->u.tree.right->u.tree.right);
	}
	else
	    v.type = FcTypeVoid;
	FcValueDestroy (vl);
	break;
    case FcOpEqual:
    case FcOpNotEqual:
    case FcOpLess:
    case FcOpLessEqual:
    case FcOpMore:
    case FcOpMoreEqual:
    case FcOpContains:
    case FcOpNotContains:
	vl = FcConfigEvaluate (p, e->u.tree.left);
	vr = FcConfigEvaluate (p, e->u.tree.right);
	v.type = FcTypeBool;
	v.u.b = FcConfigCompareValue (vl, e->op, vr);
	FcValueDestroy (vl);
	FcValueDestroy (vr);
	break;	
    case FcOpOr:
    case FcOpAnd:
    case FcOpPlus:
    case FcOpMinus:
    case FcOpTimes:
    case FcOpDivide:
	vl = FcConfigEvaluate (p, e->u.tree.left);
	vr = FcConfigEvaluate (p, e->u.tree.right);
	vl = FcConfigPromote (vl, vr);
	vr = FcConfigPromote (vr, vl);
	if (vl.type == vr.type)
	{
	    switch (vl.type) {
	    case FcTypeDouble:
		switch (e->op) {
		case FcOpPlus:	   
		    v.type = FcTypeDouble;
		    v.u.d = vl.u.d + vr.u.d; 
		    break;
		case FcOpMinus:
		    v.type = FcTypeDouble;
		    v.u.d = vl.u.d - vr.u.d; 
		    break;
		case FcOpTimes:
		    v.type = FcTypeDouble;
		    v.u.d = vl.u.d * vr.u.d; 
		    break;
		case FcOpDivide:
		    v.type = FcTypeDouble;
		    v.u.d = vl.u.d / vr.u.d; 
		    break;
		default:
		    v.type = FcTypeVoid; 
		    break;
		}
		if (v.type == FcTypeDouble &&
		    v.u.d == (double) (int) v.u.d)
		{
		    v.type = FcTypeInteger;
		    v.u.i = (int) v.u.d;
		}
		break;
	    case FcTypeBool:
		switch (e->op) {
		case FcOpOr:
		    v.type = FcTypeBool;
		    v.u.b = vl.u.b || vr.u.b;
		    break;
		case FcOpAnd:
		    v.type = FcTypeBool;
		    v.u.b = vl.u.b && vr.u.b;
		    break;
		default:
		    v.type = FcTypeVoid; 
		    break;
		}
		break;
	    case FcTypeString:
		switch (e->op) {
		case FcOpPlus:
		    v.type = FcTypeString;
		    v.u.s = FcStrPlus (vl.u.s, vr.u.s);
		    if (!v.u.s)
			v.type = FcTypeVoid;
		    break;
		default:
		    v.type = FcTypeVoid;
		    break;
		}
		break;
	    case FcTypeMatrix:
		switch (e->op) {
		case FcOpTimes:
		    v.type = FcTypeMatrix;
		    m = malloc (sizeof (FcMatrix));
		    if (m)
		    {
			FcMemAlloc (FC_MEM_MATRIX, sizeof (FcMatrix));
			FcMatrixMultiply (m, vl.u.m, vr.u.m);
			v.u.m = m;
		    }
		    else
		    {
			v.type = FcTypeVoid;
		    }
		    break;
		default:
		    v.type = FcTypeVoid;
		    break;
		}
		break;
	    default:
		v.type = FcTypeVoid;
		break;
	    }
	}
	else
	    v.type = FcTypeVoid;
	FcValueDestroy (vl);
	FcValueDestroy (vr);
	break;
    case FcOpNot:
	vl = FcConfigEvaluate (p, e->u.tree.left);
	switch (vl.type) {
	case FcTypeBool:
	    v.type = FcTypeBool;
	    v.u.b = !vl.u.b;
	    break;
	default:
	    v.type = FcTypeVoid;
	    break;
	}
	FcValueDestroy (vl);
	break;
    default:
	v.type = FcTypeVoid;
	break;
    }
    return v;
}

static FcValueList *
FcConfigMatchValueList (FcPattern	*p,
			FcTest		*t,
			FcValueList	*values)
{
    FcValueList	    *ret = 0;
    FcExpr	    *e = t->expr;
    FcValue	    value;
    FcValueList	    *v;
    
    while (e)
    {
	if (e->op == FcOpComma)
	{
	    value = FcConfigEvaluate (p, e->u.tree.left);
	    e = e->u.tree.right;
	}
	else
	{
	    value = FcConfigEvaluate (p, e);
	    e = 0;
	}

	for (v = values; v; v = v->next)
	{
	    if (FcConfigCompareValue (v->value, t->op, value))
	    {
		if (!ret)
		    ret = v;
	    }
	    else
	    {
		if (t->qual == FcQualAll)
		{
		    ret = 0;
		    break;
		}
	    }
	}
	FcValueDestroy (value);
    }
    return ret;
}

static FcValueList *
FcConfigValues (FcPattern *p, FcExpr *e, FcValueBinding binding)
{
    FcValueList	*l;
    
    if (!e)
	return 0;
    l = (FcValueList *) malloc (sizeof (FcValueList));
    if (!l)
	return 0;
    FcMemAlloc (FC_MEM_VALLIST, sizeof (FcValueList));
    if (e->op == FcOpComma)
    {
	l->value = FcConfigEvaluate (p, e->u.tree.left);
	l->next  = FcConfigValues (p, e->u.tree.right, binding);
    }
    else
    {
	l->value = FcConfigEvaluate (p, e);
	l->next  = 0;
    }
    l->binding = binding;
    while (l && l->value.type == FcTypeVoid)
    {
	FcValueList	*next = l->next;
	
	FcMemFree (FC_MEM_VALLIST, sizeof (FcValueList));
	free (l);
	l = next;
    }
    return l;
}

static FcBool
FcConfigAdd (FcValueList    **head,
	     FcValueList    *position,
	     FcBool	    append,
	     FcValueList    *new)
{
    FcValueList    **prev, *last;
    
    if (append)
    {
	if (position)
	    prev = &position->next;
	else
	    for (prev = head; *prev; prev = &(*prev)->next)
		;
    }
    else
    {
	if (position)
	{
	    for (prev = head; *prev; prev = &(*prev)->next)
	    {
		if (*prev == position)
		    break;
	    }
	}
	else
	    prev = head;

	if (FcDebug () & FC_DBG_EDIT)
	{
	    if (!*prev)
		printf ("position not on list\n");
	}
    }

    if (FcDebug () & FC_DBG_EDIT)
    {
	printf ("%s list before ", append ? "Append" : "Prepend");
	FcValueListPrint (*head);
	printf ("\n");
    }
    
    if (new)
    {
	last = new;
	while (last->next)
	    last = last->next;
    
	last->next = *prev;
	*prev = new;
    }
    
    if (FcDebug () & FC_DBG_EDIT)
    {
	printf ("%s list after ", append ? "Append" : "Prepend");
	FcValueListPrint (*head);
	printf ("\n");
    }
    
    return FcTrue;
}

static void
FcConfigDel (FcValueList    **head,
	     FcValueList    *position)
{
    FcValueList    **prev;

    for (prev = head; *prev; prev = &(*prev)->next)
    {
	if (*prev == position)
	{
	    *prev = position->next;
	    position->next = 0;
	    FcValueListDestroy (position);
	    break;
	}
    }
}

static void
FcConfigPatternAdd (FcPattern	*p,
		    const char	*object,
		    FcValueList	*list,
		    FcBool	append)
{
    if (list)
    {
	FcPatternElt    *e = FcPatternInsertElt (p, object);
    
	if (!e)
	    return;
	FcConfigAdd (&e->values, 0, append, list);
    }
}

/*
 * Delete all values associated with a field
 */
static void
FcConfigPatternDel (FcPattern	*p,
		    const char	*object)
{
    FcPatternElt    *e = FcPatternFindElt (p, object);
    if (!e)
	return;
    while (e->values)
	FcConfigDel (&e->values, e->values);
}

static void
FcConfigPatternCanon (FcPattern	    *p,
		      const char    *object)
{
    FcPatternElt    *e = FcPatternFindElt (p, object);
    if (!e)
	return;
    if (!e->values)
	FcPatternDel (p, object);
}

FcBool
FcConfigSubstituteWithPat (FcConfig    *config,
			   FcPattern   *p,
			   FcPattern   *p_pat,
			   FcMatchKind kind)
{
    FcSubst	    *s;
    FcSubState	    *st;
    int		    i;
    FcTest	    *t;
    FcEdit	    *e;
    FcValueList	    *l;
    FcPattern	    *m;

    if (!config)
    {
	config = FcConfigGetCurrent ();
	if (!config)
	    return FcFalse;
    }

    st = (FcSubState *) malloc (config->maxObjects * sizeof (FcSubState));
    if (!st && config->maxObjects)
	return FcFalse;
    FcMemAlloc (FC_MEM_SUBSTATE, config->maxObjects * sizeof (FcSubState));

    if (FcDebug () & FC_DBG_EDIT)
    {
	printf ("FcConfigSubstitute ");
	FcPatternPrint (p);
    }
    if (kind == FcMatchPattern)
	s = config->substPattern;
    else
	s = config->substFont;
    for (; s; s = s->next)
    {
	/*
	 * Check the tests to see if
	 * they all match the pattern
	 */
	for (t = s->test, i = 0; t; t = t->next, i++)
	{
	    if (FcDebug () & FC_DBG_EDIT)
	    {
		printf ("FcConfigSubstitute test ");
		FcTestPrint (t);
	    }
	    st[i].elt = 0;
	    if (kind == FcMatchFont && t->kind == FcMatchPattern)
		m = p_pat;
	    else
		m = p;
	    if (m)
		st[i].elt = FcPatternFindElt (m, t->field);
	    else
		st[i].elt = 0;
	    /*
	     * If there's no such field in the font,
	     * then FcQualAll matches while FcQualAny does not
	     */
	    if (!st[i].elt)
	    {
		if (t->qual == FcQualAll)
		{
		    st[i].value = 0;
		    continue;
		}
		else
		    break;
	    }
	    /*
	     * Check to see if there is a match, mark the location
	     * to apply match-relative edits
	     */
	    st[i].value = FcConfigMatchValueList (m, t, st[i].elt->values);
	    if (!st[i].value)
		break;
	    if (t->qual == FcQualFirst && st[i].value != st[i].elt->values)
		break;
	    if (t->qual == FcQualNotFirst && st[i].value == st[i].elt->values)
		break;
	}
	if (t)
	{
	    if (FcDebug () & FC_DBG_EDIT)
		printf ("No match\n");
	    continue;
	}
	if (FcDebug () & FC_DBG_EDIT)
	{
	    printf ("Substitute ");
	    FcSubstPrint (s);
	}
	for (e = s->edit; e; e = e->next)
	{
	    /*
	     * Evaluate the list of expressions
	     */
	    l = FcConfigValues (p, e->expr, e->binding);
	    /*
	     * Locate any test associated with this field, skipping
	     * tests associated with the pattern when substituting in
	     * the font
	     */
	    for (t = s->test, i = 0; t; t = t->next, i++)
	    {
		if ((t->kind == FcMatchFont || kind == FcMatchPattern) &&
		    !FcStrCmpIgnoreCase ((FcChar8 *) t->field, 
					 (FcChar8 *) e->field))
		{
		    if (!st[i].elt)
			t = 0;
		    break;
		}
	    }
	    switch (e->op) {
	    case FcOpAssign:
		/*
		 * If there was a test, then replace the matched
		 * value with the new list of values
		 */
		if (t)
		{
		    FcValueList	*thisValue = st[i].value;
		    FcValueList	*nextValue = thisValue ? thisValue->next : 0;
		    
		    /*
		     * Append the new list of values after the current value
		     */
		    FcConfigAdd (&st[i].elt->values, thisValue, FcTrue, l);
		    /*
		     * Delete the marked value
		     */
		    FcConfigDel (&st[i].elt->values, thisValue);
		    /*
		     * Adjust any pointers into the value list to ensure
		     * future edits occur at the same place
		     */
		    for (t = s->test, i = 0; t; t = t->next, i++)
		    {
			if (st[i].value == thisValue)
			    st[i].value = nextValue;
		    }
		    break;
		}
		/* fall through ... */
	    case FcOpAssignReplace:
		/*
		 * Delete all of the values and insert
		 * the new set
		 */
		FcConfigPatternDel (p, e->field);
		FcConfigPatternAdd (p, e->field, l, FcTrue);
		/*
		 * Adjust any pointers into the value list as they no
		 * longer point to anything valid
		 */
		if (t)
		{
		    FcPatternElt    *thisElt = st[i].elt;
		    for (t = s->test, i = 0; t; t = t->next, i++)
		    {
			if (st[i].elt == thisElt)
			    st[i].value = 0;
		    }
		}
		break;
	    case FcOpPrepend:
		if (t)
		{
		    FcConfigAdd (&st[i].elt->values, st[i].value, FcFalse, l);
		    break;
		}
		/* fall through ... */
	    case FcOpPrependFirst:
		FcConfigPatternAdd (p, e->field, l, FcFalse);
		break;
	    case FcOpAppend:
		if (t)
		{
		    FcConfigAdd (&st[i].elt->values, st[i].value, FcTrue, l);
		    break;
		}
		/* fall through ... */
	    case FcOpAppendLast:
		FcConfigPatternAdd (p, e->field, l, FcTrue);
		break;
	    default:
		break;
	    }
	}
	/*
	 * Now go through the pattern and eliminate
	 * any properties without data
	 */
	for (e = s->edit; e; e = e->next)
	    FcConfigPatternCanon (p, e->field);

	if (FcDebug () & FC_DBG_EDIT)
	{
	    printf ("FcConfigSubstitute edit");
	    FcPatternPrint (p);
	}
    }
    FcMemFree (FC_MEM_SUBSTATE, config->maxObjects * sizeof (FcSubState));
    free (st);
    if (FcDebug () & FC_DBG_EDIT)
    {
	printf ("FcConfigSubstitute done");
	FcPatternPrint (p);
    }
    return FcTrue;
}

FcBool
FcConfigSubstitute (FcConfig	*config,
		    FcPattern	*p,
		    FcMatchKind	kind)
{
    return FcConfigSubstituteWithPat (config, p, 0, kind);
}

#ifndef FONTCONFIG_PATH
#define FONTCONFIG_PATH	"/etc/fonts"
#endif

#ifndef FONTCONFIG_FILE
#define FONTCONFIG_FILE	"fonts.conf"
#endif

static FcChar8 *
FcConfigFileExists (const FcChar8 *dir, const FcChar8 *file)
{
    FcChar8    *path;

    if (!dir)
	dir = (FcChar8 *) "";
    path = malloc (strlen ((char *) dir) + 1 + strlen ((char *) file) + 1);
    if (!path)
	return 0;

    strcpy ((char *) path, (const char *) dir);
    /* make sure there's a single separating / */
    if ((!path[0] || path[strlen((char *) path)-1] != '/') && file[0] != '/')
	strcat ((char *) path, "/");
    strcat ((char *) path, (char *) file);

    FcMemAlloc (FC_MEM_STRING, strlen ((char *) path) + 1);
    if (access ((char *) path, R_OK) == 0)
	return path;
    
    FcStrFree (path);
    return 0;
}

static FcChar8 **
FcConfigGetPath (void)
{
    FcChar8    **path;
    FcChar8    *env, *e, *colon;
    FcChar8    *dir;
    int	    npath;
    int	    i;

    npath = 2;	/* default dir + null */
    env = (FcChar8 *) getenv ("FONTCONFIG_PATH");
    if (env)
    {
	e = env;
	npath++;
	while (*e)
	    if (*e++ == ':')
		npath++;
    }
    path = calloc (npath, sizeof (FcChar8 *));
    if (!path)
	goto bail0;
    i = 0;

    if (env)
    {
	e = env;
	while (*e) 
	{
	    colon = (FcChar8 *) strchr ((char *) e, ':');
	    if (!colon)
		colon = e + strlen ((char *) e);
	    path[i] = malloc (colon - e + 1);
	    if (!path[i])
		goto bail1;
	    strncpy ((char *) path[i], (const char *) e, colon - e);
	    path[i][colon - e] = '\0';
	    if (*colon)
		e = colon + 1;
	    else
		e = colon;
	    i++;
	}
    }
    
    dir = (FcChar8 *) FONTCONFIG_PATH;
    path[i] = malloc (strlen ((char *) dir) + 1);
    if (!path[i])
	goto bail1;
    strcpy ((char *) path[i], (const char *) dir);
    return path;

bail1:
    for (i = 0; path[i]; i++)
	free (path[i]);
    free (path);
bail0:
    return 0;
}

static void
FcConfigFreePath (FcChar8 **path)
{
    FcChar8    **p;

    for (p = path; *p; p++)
	free (*p);
    free (path);
}

FcChar8 *
FcConfigFilename (const FcChar8 *url)
{
    FcChar8    *file, *dir, **path, **p;
    
    if (!url || !*url)
    {
	url = (FcChar8 *) getenv ("FONTCONFIG_FILE");
	if (!url)
	    url = (FcChar8 *) FONTCONFIG_FILE;
    }
    file = 0;
    switch (*url) {
    case '~':
	dir = (FcChar8 *) getenv ("HOME");
	if (dir)
	    file = FcConfigFileExists (dir, url + 1);
	else
	    file = 0;
	break;
    case '/':
	file = FcConfigFileExists (0, url);
	break;
    default:
	path = FcConfigGetPath ();
	if (!path)
	    return 0;
	for (p = path; *p; p++)
	{
	    file = FcConfigFileExists (*p, url);
	    if (file)
		break;
	}
	FcConfigFreePath (path);
	break;
    }
    return file;
}

/*
 * Manage the application-specific fonts
 */

FcBool
FcConfigAppFontAddFile (FcConfig    *config,
			const FcChar8  *file)
{
    FcFontSet	*set;
    FcStrSet	*subdirs;
    FcStrList	*sublist;
    FcChar8	*subdir;

    if (!config)
    {
	config = FcConfigGetCurrent ();
	if (!config)
	    return FcFalse;
    }

    subdirs = FcStrSetCreate ();
    if (!subdirs)
	return FcFalse;
    
    set = FcConfigGetFonts (config, FcSetApplication);
    if (!set)
    {
	set = FcFontSetCreate ();
	if (!set)
	{
	    FcStrSetDestroy (subdirs);
	    return FcFalse;
	}
	FcConfigSetFonts (config, set, FcSetApplication);
    }
	
    if (!FcFileScan (set, subdirs, 0, config->blanks, file, FcFalse))
    {
	FcStrSetDestroy (subdirs);
	return FcFalse;
    }
    if ((sublist = FcStrListCreate (subdirs)))
    {
	while ((subdir = FcStrListNext (sublist)))
	{
	    FcConfigAppFontAddDir (config, subdir);
	}
	FcStrListDone (sublist);
    }
    return FcTrue;
}

FcBool
FcConfigAppFontAddDir (FcConfig	    *config,
		       const FcChar8   *dir)
{
    FcFontSet	*set;
    FcStrSet	*subdirs;
    FcStrList	*sublist;
    FcChar8	*subdir;
    
    if (!config)
    {
	config = FcConfigGetCurrent ();
	if (!config)
	    return FcFalse;
    }
    subdirs = FcStrSetCreate ();
    if (!subdirs)
	return FcFalse;
    
    set = FcConfigGetFonts (config, FcSetApplication);
    if (!set)
    {
	set = FcFontSetCreate ();
	if (!set)
	{
	    FcStrSetDestroy (subdirs);
	    return FcFalse;
	}
	FcConfigSetFonts (config, set, FcSetApplication);
    }
    
    if (!FcDirScan (set, subdirs, 0, config->blanks, dir, FcFalse))
    {
	FcStrSetDestroy (subdirs);
	return FcFalse;
    }
    if ((sublist = FcStrListCreate (subdirs)))
    {
	while ((subdir = FcStrListNext (sublist)))
	{
	    FcConfigAppFontAddDir (config, subdir);
	}
	FcStrListDone (sublist);
    }
    return FcTrue;
}

void
FcConfigAppFontClear (FcConfig	    *config)
{
    FcConfigSetFonts (config, 0, FcSetApplication);
}
