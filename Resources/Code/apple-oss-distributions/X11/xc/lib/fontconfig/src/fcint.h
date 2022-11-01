/*
 * $XFree86: xc/lib/fontconfig/src/fcint.h,v 1.28 2002/09/26 00:15:54 keithp Exp $
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

#ifndef _FCINT_H_
#define _FCINT_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <fontconfig/fontconfig.h>
#include <fontconfig/fcprivate.h>
#include <fontconfig/fcfreetype.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

typedef struct _FcSymbolic {
    const char	*name;
    int		value;
} FcSymbolic;

#ifndef FC_CONFIG_PATH
#define FC_CONFIG_PATH "fonts.conf"
#endif

#define FC_FONT_FILE_INVALID	((FcChar8 *) ".")
#define FC_FONT_FILE_DIR	((FcChar8 *) ".dir")

#define FC_DBG_MATCH	1
#define FC_DBG_MATCHV	2
#define FC_DBG_EDIT	4
#define FC_DBG_FONTSET	8
#define FC_DBG_CACHE	16
#define FC_DBG_CACHEV	32
#define FC_DBG_PARSE	64
#define FC_DBG_SCAN	128
#define FC_DBG_SCANV	256
#define FC_DBG_MEMORY	512

#define FC_MEM_CHARSET	    0
#define FC_MEM_CHARLEAF	    1
#define FC_MEM_FONTSET	    2
#define FC_MEM_FONTPTR	    3
#define FC_MEM_OBJECTSET    4
#define FC_MEM_OBJECTPTR    5
#define FC_MEM_MATRIX	    6
#define FC_MEM_PATTERN	    7
#define FC_MEM_PATELT	    8
#define FC_MEM_VALLIST	    9
#define FC_MEM_SUBSTATE	    10
#define FC_MEM_STRING	    11
#define FC_MEM_LISTBUCK	    12
#define FC_MEM_STRSET	    13
#define FC_MEM_STRLIST	    14
#define FC_MEM_CONFIG	    15
#define FC_MEM_LANGSET	    16
#define FC_MEM_ATOMIC	    17
#define FC_MEM_BLANKS	    18
#define FC_MEM_CACHE	    19
#define FC_MEM_STRBUF	    20
#define FC_MEM_SUBST	    21
#define FC_MEM_OBJECTTYPE   22
#define FC_MEM_CONSTANT	    23
#define FC_MEM_TEST	    24
#define FC_MEM_EXPR	    25
#define FC_MEM_VSTACK	    26
#define FC_MEM_ATTR	    27
#define FC_MEM_PSTACK	    28

#define FC_MEM_NUM	    29

typedef enum _FcValueBinding {
    FcValueBindingWeak, FcValueBindingStrong
} FcValueBinding;

typedef struct _FcValueList {
    struct _FcValueList    *next;
    FcValue		    value;
    FcValueBinding	    binding;
} FcValueList;

typedef struct _FcPatternElt {
    const char	    *object;
    FcValueList	    *values;
} FcPatternElt;


struct _FcPattern {
    int		    num;
    int		    size;
    FcPatternElt    *elts;
    int		    ref;
};

typedef enum _FcOp {
    FcOpInteger, FcOpDouble, FcOpString, FcOpMatrix, FcOpBool, FcOpCharSet, 
    FcOpNil,
    FcOpField, FcOpConst,
    FcOpAssign, FcOpAssignReplace, 
    FcOpPrependFirst, FcOpPrepend, FcOpAppend, FcOpAppendLast,
    FcOpQuest,
    FcOpOr, FcOpAnd, FcOpEqual, FcOpNotEqual, FcOpContains, FcOpNotContains,
    FcOpLess, FcOpLessEqual, FcOpMore, FcOpMoreEqual,
    FcOpPlus, FcOpMinus, FcOpTimes, FcOpDivide,
    FcOpNot, FcOpComma, FcOpInvalid
} FcOp;

typedef struct _FcExpr {
    FcOp   op;
    union {
	int	    ival;
	double	    dval;
	FcChar8	    *sval;
	FcMatrix    *mval;
	FcBool	    bval;
	FcCharSet   *cval;
	char	    *field;
	FcChar8	    *constant;
	struct {
	    struct _FcExpr *left, *right;
	} tree;
    } u;
} FcExpr;

typedef enum _FcQual {
    FcQualAny, FcQualAll, FcQualFirst, FcQualNotFirst
} FcQual;

#define FcMatchDefault	((FcMatchKind) -1)

typedef struct _FcTest {
    struct _FcTest	*next;
    FcMatchKind		kind;
    FcQual		qual;
    const char		*field;
    FcOp		op;
    FcExpr		*expr;
} FcTest;

typedef struct _FcEdit {
    struct _FcEdit *next;
    const char	    *field;
    FcOp	    op;
    FcExpr	    *expr;
    FcValueBinding  binding;
} FcEdit;

typedef struct _FcSubst {
    struct _FcSubst	*next;
    FcTest		*test;
    FcEdit		*edit;
} FcSubst;

typedef struct _FcCharLeaf {
    FcChar32	map[256/32];
} FcCharLeaf;

#define FC_REF_CONSTANT	    -1

struct _FcCharSet {
    int		    ref;	/* reference count */
    int		    num;	/* size of leaves and numbers arrays */
    FcCharLeaf	    **leaves;
    FcChar16	    *numbers;
};

struct _FcStrSet {
    int		    ref;	/* reference count */
    int		    num;
    int		    size;
    FcChar8	    **strs;
};

struct _FcStrList {
    FcStrSet	    *set;
    int		    n;
};

typedef struct _FcStrBuf {
    FcChar8 *buf;
    FcBool  allocated;
    FcBool  failed;
    int	    len;
    int	    size;
} FcStrBuf;

/*
 * The per-user ~/.fonts.cache-<version> file is loaded into
 * this data structure.  Each directory gets a substructure
 * which is validated by comparing the directory timestamp with
 * that saved in the cache.  When valid, the entire directory cache
 * can be immediately loaded without reading the directory.  Otherwise,
 * the files are checked individually; updated files are loaded into the
 * cache which is then rewritten to the users home directory
 */

#define FC_GLOBAL_CACHE_DIR_HASH_SIZE	    37
#define FC_GLOBAL_CACHE_FILE_HASH_SIZE	    67

typedef struct _FcGlobalCacheInfo {
    unsigned int		hash;
    FcChar8			*file;
    time_t			time;
    FcBool			referenced;
} FcGlobalCacheInfo;

typedef struct _FcGlobalCacheFile {
    struct _FcGlobalCacheFile	*next;
    FcGlobalCacheInfo		info;
    int				id;
    FcChar8			*name;
} FcGlobalCacheFile;

typedef struct _FcGlobalCacheDir FcGlobalCacheDir;

typedef struct _FcGlobalCacheSubdir {
    struct _FcGlobalCacheSubdir	*next;
    FcGlobalCacheDir		*ent;
} FcGlobalCacheSubdir;

struct _FcGlobalCacheDir {
    struct _FcGlobalCacheDir	*next;
    FcGlobalCacheInfo    	info;
    int				len;
    FcGlobalCacheFile		*ents[FC_GLOBAL_CACHE_FILE_HASH_SIZE];
    FcGlobalCacheSubdir		*subdirs;
};

typedef struct _FcGlobalCache {
    FcGlobalCacheDir		*ents[FC_GLOBAL_CACHE_DIR_HASH_SIZE];
    FcBool			updated;
    FcBool			broken;
    int				entries;
    int				referenced;
} FcGlobalCache;

struct _FcAtomic {
    FcChar8	*file;		/* original file name */
    FcChar8	*new;		/* temp file name -- write data here */
    FcChar8	*lck;		/* lockfile name (used for locking) */
    FcChar8	*tmp;		/* tmpfile name (used for locking) */
};

struct _FcBlanks {
    int		nblank;
    int		sblank;
    FcChar32	*blanks;
};

struct _FcConfig {
    /*
     * File names loaded from the configuration -- saved here as the
     * cache file must be consulted before the directories are scanned,
     * and those directives may occur in any order
     */
    FcStrSet	*configDirs;	    /* directories to scan for fonts */
    FcChar8	*cache;		    /* name of per-user cache file */
    /*
     * Set of allowed blank chars -- used to
     * trim fonts of bogus glyphs
     */
    FcBlanks	*blanks;
    /*
     * List of directories containing fonts,
     * built by recursively scanning the set 
     * of configured directories
     */
    FcStrSet	*fontDirs;
    /*
     * Names of all of the configuration files used
     * to create this configuration
     */
    FcStrSet	*configFiles;	    /* config files loaded */
    /*
     * Substitution instructions for patterns and fonts;
     * maxObjects is used to allocate appropriate intermediate storage
     * while performing a whole set of substitutions
     */
    FcSubst	*substPattern;	    /* substitutions for patterns */
    FcSubst	*substFont;	    /* substitutions for fonts */
    int		maxObjects;	    /* maximum number of tests in all substs */
    /*
     * The set of fonts loaded from the listed directories; the
     * order within the set does not determine the font selection,
     * except in the case of identical matches in which case earlier fonts
     * match preferrentially
     */
    FcFontSet	*fonts[FcSetApplication + 1];
    /*
     * Fontconfig can periodically rescan the system configuration
     * and font directories.  This rescanning occurs when font
     * listing requests are made, but no more often than rescanInterval
     * seconds apart.
     */
    time_t	rescanTime;	    /* last time information was scanned */
    int		rescanInterval;	    /* interval between scans */
};
 
extern FcConfig	*_fcConfig;

typedef struct _FcCharMap FcCharMap;

/* fcblanks.c */

/* fccache.c */

FcGlobalCache *
FcGlobalCacheCreate (void);

void
FcGlobalCacheDestroy (FcGlobalCache *cache);

FcBool
FcGlobalCacheCheckTime (FcGlobalCacheInfo *info);

void
FcGlobalCacheReferenced (FcGlobalCache	    *cache,
			 FcGlobalCacheInfo  *info);

FcGlobalCacheDir *
FcGlobalCacheDirGet (FcGlobalCache  *cache,
		     const FcChar8  *dir,
		     int	    len,
		     FcBool	    create_missing);

FcBool
FcGlobalCacheScanDir (FcFontSet		*set,
		      FcStrSet		*dirs,
		      FcGlobalCache	*cache,
		      const FcChar8	*dir);

FcGlobalCacheFile *
FcGlobalCacheFileGet (FcGlobalCache *cache,
		      const FcChar8 *file,
		      int	    id,
		      int	    *count);


void
FcGlobalCacheLoad (FcGlobalCache    *cache,
		   const FcChar8    *cache_file);

FcBool
FcGlobalCacheUpdate (FcGlobalCache  *cache,
		     const FcChar8  *file,
		     int	    id,
		     const FcChar8  *name);

FcBool
FcGlobalCacheSave (FcGlobalCache    *cache,
		   const FcChar8    *cache_file);

FcBool
FcDirCacheReadDir (FcFontSet *set, FcStrSet *dirs, const FcChar8 *dir);

FcBool
FcDirCacheWriteDir (FcFontSet *set, FcStrSet *dirs, const FcChar8 *dir);
    
/* fccfg.c */

FcBool
FcConfigAddConfigDir (FcConfig	    *config,
		      const FcChar8 *d);

FcBool
FcConfigAddFontDir (FcConfig	    *config,
		    const FcChar8   *d);

FcBool
FcConfigAddDir (FcConfig	*config,
		const FcChar8	*d);

FcBool
FcConfigAddConfigFile (FcConfig		*config,
		       const FcChar8	*f);

FcBool
FcConfigSetCache (FcConfig	*config,
		  const FcChar8	*c);

FcBool
FcConfigAddBlank (FcConfig	*config,
		  FcChar32    	blank);

FcBool
FcConfigAddEdit (FcConfig	*config,
		 FcTest		*test,
		 FcEdit		*edit,
		 FcMatchKind	kind);

void
FcConfigSetFonts (FcConfig	*config,
		  FcFontSet	*fonts,
		  FcSetName	set);

FcBool
FcConfigCompareValue (const FcValue m,
		      FcOp	    op,
		      const FcValue v);

/* fccharset.c */
FcCharSet *
FcCharSetFreeze (FcCharSet *cs);

FcBool
FcNameUnparseCharSet (FcStrBuf *buf, const FcCharSet *c);

FcCharSet *
FcNameParseCharSet (FcChar8 *string);

FcChar32
FcFreeTypeUcs4ToPrivate (FcChar32 ucs4, const FcCharMap *map);

FcChar32
FcFreeTypePrivateToUcs4 (FcChar32 private, const FcCharMap *map);

const FcCharMap *
FcFreeTypeGetPrivateMap (FT_Encoding encoding);
    
/* fcdbg.c */
void
FcValueListPrint (const FcValueList *l);

void
FcOpPrint (FcOp op);

void
FcTestPrint (const FcTest *test);

void
FcExprPrint (const FcExpr *expr);

void
FcEditPrint (const FcEdit *edit);

void
FcSubstPrint (const FcSubst *subst);

int
FcDebug (void);

/* fcdir.c */

/* fcfont.c */
int
FcFontDebug (void);
    
/* fcfreetype.c */
FcBool
FcFreeTypeIsExclusiveLang (const FcChar8  *lang);

FcBool
FcFreeTypeHasLang (FcPattern *pattern, const FcChar8 *lang);

/* fcfs.c */
/* fcgram.y */
int
FcConfigparse (void);

int
FcConfigwrap (void);
    
void
FcConfigerror (char *fmt, ...);
    
char *
FcConfigSaveField (const char *field);

FcTest *
FcTestCreate (FcMatchKind   kind,
	      FcQual	    qual,
	      const FcChar8 *field,
	      FcOp	    compare,
	      FcExpr	    *expr);

void
FcTestDestroy (FcTest *test);

FcExpr *
FcExprCreateInteger (int i);

FcExpr *
FcExprCreateDouble (double d);

FcExpr *
FcExprCreateString (const FcChar8 *s);

FcExpr *
FcExprCreateMatrix (const FcMatrix *m);

FcExpr *
FcExprCreateBool (FcBool b);

FcExpr *
FcExprCreateNil (void);

FcExpr *
FcExprCreateField (const char *field);

FcExpr *
FcExprCreateConst (const FcChar8 *constant);

FcExpr *
FcExprCreateOp (FcExpr *left, FcOp op, FcExpr *right);

void
FcExprDestroy (FcExpr *e);

FcEdit *
FcEditCreate (const char *field, FcOp op, FcExpr *expr, FcValueBinding binding);

void
FcEditDestroy (FcEdit *e);

/* fcinit.c */

void
FcMemReport (void);

void
FcMemAlloc (int kind, int size);

void
FcMemFree (int kind, int size);

/* fclang.c */
FcLangSet *
FcFreeTypeLangSet (const FcCharSet  *charset, 
		   const FcChar8    *exclusiveLang);

FcLangResult
FcLangCompare (const FcChar8 *s1, const FcChar8 *s2);
    
const FcCharSet *
FcCharSetForLang (const FcChar8 *lang);

FcLangSet *
FcLangSetPromote (const FcChar8 *lang);

FcLangSet *
FcNameParseLangSet (const FcChar8 *string);

FcBool
FcNameUnparseLangSet (FcStrBuf *buf, const FcLangSet *ls);

/* fclist.c */

/* fcmatch.c */

/* fcname.c */

FcBool
FcNameBool (FcChar8 *v, FcBool *result);

/* fcpat.c */
void
FcValueListDestroy (FcValueList *l);
    
FcPatternElt *
FcPatternFindElt (const FcPattern *p, const char *object);

FcPatternElt *
FcPatternInsertElt (FcPattern *p, const char *object);

FcBool
FcPatternAddWithBinding  (FcPattern	    *p,
			  const char	    *object,
			  FcValue	    value,
			  FcValueBinding    binding,
			  FcBool	    append);

FcPattern *
FcPatternFreeze (FcPattern *p);

/* fcrender.c */

/* fcmatrix.c */

extern const FcMatrix    FcIdentityMatrix;

void
FcMatrixFree (FcMatrix *mat);

/* fcstr.c */
FcChar8 *
FcStrPlus (const FcChar8 *s1, const FcChar8 *s2);
    
void
FcStrFree (FcChar8 *s);

void
FcStrBufInit (FcStrBuf *buf, FcChar8 *init, int size);

void
FcStrBufDestroy (FcStrBuf *buf);

FcChar8 *
FcStrBufDone (FcStrBuf *buf);

FcBool
FcStrBufChar (FcStrBuf *buf, FcChar8 c);

FcBool
FcStrBufString (FcStrBuf *buf, const FcChar8 *s);

FcBool
FcStrBufData (FcStrBuf *buf, const FcChar8 *s, int len);

int
FcStrCmpIgnoreBlanksAndCase (const FcChar8 *s1, const FcChar8 *s2);

#endif /* _FC_INT_H_ */
