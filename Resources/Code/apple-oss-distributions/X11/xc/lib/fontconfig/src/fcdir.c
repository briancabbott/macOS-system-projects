/*
 * $XFree86: xc/lib/fontconfig/src/fcdir.c,v 1.10 2002/09/26 00:15:53 keithp Exp $
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
#include <dirent.h>

static FcBool
FcFileIsDir (const FcChar8 *file)
{
    struct stat	    statb;

    if (stat ((const char *) file, &statb) != 0)
	return FcFalse;
    return S_ISDIR(statb.st_mode);
}

FcBool
FcFileScan (FcFontSet	    *set,
	    FcStrSet	    *dirs,
	    FcGlobalCache   *cache,
	    FcBlanks	    *blanks,
	    const FcChar8   *file,
	    FcBool	    force)
{
    int			id;
    FcChar8		*name;
    FcPattern		*font;
    FcBool		ret = FcTrue;
    FcBool		isDir;
    int			count = 0;
    FcGlobalCacheFile	*cache_file;
    FcGlobalCacheDir	*cache_dir;
    FcBool		need_scan;
    
    if (force)
	cache = 0;
    id = 0;
    do
    {
	need_scan = FcTrue;
	font = 0;
	/*
	 * Check the cache
	 */
	if (cache)
	{
	    if ((cache_file = FcGlobalCacheFileGet (cache, file, id, &count)))
	    {
		/*
		 * Found a cache entry for the file
		 */
		if (FcGlobalCacheCheckTime (&cache_file->info))
		{
		    name = cache_file->name;
		    need_scan = FcFalse;
		    FcGlobalCacheReferenced (cache, &cache_file->info);
		    /* "." means the file doesn't contain a font */
		    if (FcStrCmp (name, FC_FONT_FILE_INVALID) != 0)
		    {
			font = FcNameParse (name);
			if (font)
			    if (!FcPatternAddString (font, FC_FILE, file))
				ret = FcFalse;
		    }
		}
	    }
	    else if ((cache_dir = FcGlobalCacheDirGet (cache, file,
						       strlen ((const char *) file),
						       FcFalse)))
	    {
		if (FcGlobalCacheCheckTime (&cache_dir->info))
		{
		    font = 0;
		    need_scan = FcFalse;
		    FcGlobalCacheReferenced (cache, &cache_dir->info);
		    if (!FcStrSetAdd (dirs, file))
			ret = FcFalse;
		}
	    }
	}
	/*
	 * Nothing in the cache, scan the file
	 */
	if (need_scan)
	{
	    if (FcDebug () & FC_DBG_SCAN)
	    {
		printf ("\tScanning file %s...", file);
		fflush (stdout);
	    }
	    font = FcFreeTypeQuery (file, id, blanks, &count);
	    if (FcDebug () & FC_DBG_SCAN)
		printf ("done\n");
	    isDir = FcFalse;
	    if (!font && FcFileIsDir (file))
	    {
		isDir = FcTrue;
		ret = FcStrSetAdd (dirs, file);
		if (cache && ret)
		    FcGlobalCacheUpdate (cache, file, 0, FC_FONT_FILE_DIR);
	    }
	    /*
	     * Update the cache
	     */
	    if (cache && font)
	    {
		FcChar8	*unparse;

		unparse = FcNameUnparse (font);
		if (unparse)
		{
		    (void) FcGlobalCacheUpdate (cache, file, id, unparse);
		    FcStrFree (unparse);
		}
	    }
	}
	/*
	 * Add the font
	 */
	if (font)
	{
	    if (!FcFontSetAdd (set, font))
	    {
		FcPatternDestroy (font);
		font = 0;
		ret = FcFalse;
	    }
	}
	id++;
    } while (font && ret && id < count);
    return ret;
}

#define FC_MAX_FILE_LEN	    4096

FcBool
FcDirScan (FcFontSet	    *set,
	   FcStrSet	    *dirs,
	   FcGlobalCache    *cache,
	   FcBlanks	    *blanks,
	   const FcChar8    *dir,
	   FcBool	    force)
{
    DIR			*d;
    struct dirent	*e;
    FcChar8		*file;
    FcChar8		*base;
    FcBool		ret = FcTrue;

    if (!force)
    {
	/*
	 * Check fonts.cache-<version> file
	 */
	if (FcDirCacheReadDir (set, dirs, dir))
	    return FcTrue;
    
	/*
	 * Check ~/.fonts.cache-<version> file
	 */
	if (cache && FcGlobalCacheScanDir (set, dirs, cache, dir))
	    return FcTrue;
    }
    
    /* freed below */
    file = (FcChar8 *) malloc (strlen ((char *) dir) + 1 + FC_MAX_FILE_LEN + 1);
    if (!file)
	return FcFalse;

    strcpy ((char *) file, (char *) dir);
    strcat ((char *) file, "/");
    base = file + strlen ((char *) file);
    
    d = opendir ((char *) dir);
    
    if (!d)
    {
	free (file);
	/* Don't complain about missing directories */
	if (errno == ENOENT)
	    return FcTrue;
	return FcFalse;
    }
    while (ret && (e = readdir (d)))
    {
	if (e->d_name[0] != '.' && strlen (e->d_name) < FC_MAX_FILE_LEN)
	{
	    strcpy ((char *) base, (char *) e->d_name);
	    ret = FcFileScan (set, dirs, cache, blanks, file, force);
	}
    }
    free (file);
    closedir (d);
    if (ret && cache)
	FcGlobalCacheUpdate (cache, dir, 0, 0);
	
    return ret;
}

FcBool
FcDirSave (FcFontSet *set, FcStrSet *dirs, const FcChar8 *dir)
{
    return FcDirCacheWriteDir (set, dirs, dir);
}
