/*
 * $XFree86: xc/lib/fontconfig/fc-lang/fc-lang.c,v 1.4 2002/12/14 02:03:58 dawes Exp $
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

/*
 * fc-lang
 *
 * Read a set of language orthographies and build C declarations for
 * charsets which can then be used to identify which languages are
 * supported by a given font.  Note that it would be nice if
 * this could be done while compiling the library, but this
 * code uses a number of routines from the library.  It's
 * expediant to just ship the pre-built version along with the
 * source orthographies.
 */

static void 
fatal (char *file, int lineno, char *msg)
{
    fprintf (stderr, "%s:%d: %s\n", file, lineno, msg);
    exit (1);
}

static char *
get_line (FILE *f, char *line, int *lineno)
{
    char    *hash;
    if (!fgets (line, 1024, f))
	return 0;
    ++(*lineno);
    hash = strchr (line, '#');
    if (hash)
	*hash = '\0';
    if (line[0] == '\0' || line[0] == '\n' || line[0] == '\032' || line[0] == '\r')
	return get_line (f, line, lineno);
    return line;
}

/*
 * build a single charset from a source file
 *
 * The file format is quite simple, either
 * a single hex value or a pair separated with a dash
 *
 * Comments begin with '#'
 */

static FcCharSet *
scan (FILE *f, char *file)
{
    FcCharSet	*c = 0;
    FcCharSet	*n;
    int		start, end, ucs4;
    char	line[1024];
    int		lineno = 0;

    while (get_line (f, line, &lineno))
    {
	if (!strncmp (line, "include", 7))
	{
	    file = strchr (line, ' ');
	    while (*file == ' ')
		file++;
	    end = strlen (file);
	    if (file[end-1] == '\n')
		file[end-1] = '\0';
	    f = fopen (file, "r");
	    if (!f)
		fatal (file, 0, "can't open");
	    c = scan (f, file);
	    fclose (f);
	    return c;
	}
	if (strchr (line, '-'))
	{
	    if (sscanf (line, "%x-%x", &start, &end) != 2)
		fatal (file, lineno, "parse error");
	}
	else
	{
	    if (sscanf (line, "%x", &start) != 1)
		fatal (file, lineno, "parse error");
	    end = start;
	}
	if (!c)
	    c = FcCharSetCreate ();
	for (ucs4 = start; ucs4 <= end; ucs4++)
	{
	    if (!FcCharSetAddChar (c, ucs4))
		fatal (file, lineno, "out of memory");
	}
    }
    n = FcCharSetFreeze (c);
    FcCharSetDestroy (c);
    return n;
}

/*
 * Convert a file name into a name suitable for C declarations
 */
static char *
get_name (char *file)
{
    char    *name;
    char    *dot;

    dot = strchr (file, '.');
    if (!dot)
	dot = file + strlen(file);
    name = malloc (dot - file + 1);
    strncpy (name, file, dot - file);
    name[dot-file] = '\0';
    return name;
}

/*
 * Convert a C name into a language name
 */
static char *
get_lang (char *name)
{
    char    *lang = malloc (strlen (name) + 1);
    char    *l = lang;
    char    c;

    while ((c = *name++))
    {
	if (isupper (c))
	    c = tolower (c);
	if (c == '_')
	    c = '-';
	if (c == ' ')
	    continue;
	*l++ = c;
    }
    *l++ = '\0';
    return lang;
}

static int compare (const void *a, const void *b)
{
    const FcChar8    *const *as = a, *const *bs = b;
    return FcStrCmpIgnoreCase (*as, *bs);
}

#define MAX_LANG	    1024
#define MAX_LANG_SET_MAP    ((MAX_LANG + 31) / 32)

#define BitSet(map, id)   ((map)[(id)>>5] |= ((FcChar32) 1 << ((id) & 0x1f)))
#define BitGet(map, id)   ((map)[(id)>>5] >> ((id) & 0x1f)) & 1)

int
main (int argc, char **argv)
{
    char	*files[MAX_LANG];
    FcCharSet	*sets[MAX_LANG];
    int		duplicate[MAX_LANG];
    int		country[MAX_LANG];
    char	*names[MAX_LANG];
    char	*langs[MAX_LANG];
    FILE	*f;
    int		ncountry = 0;
    int		i = 0;
    FcCharLeaf	**leaves, **sleaves;
    int		total_leaves = 0;
    int		l, sl, tl;
    int		c;
    char	line[1024];
    FcChar32	map[MAX_LANG_SET_MAP];
    int		num_lang_set_map;
    
    while (*++argv)
    {
	if (i == MAX_LANG)
	    fatal (*argv, 0, "Too many languages");
	files[i++] = *argv;
    }
    files[i] = 0;
    qsort (files, i, sizeof (char *), compare);
    i = 0;
    while (files[i])
    {
	f = fopen (files[i], "r");
	if (!f)
	    fatal (files[i], 0, strerror (errno));
	sets[i] = scan (f, files[i]);
	names[i] = get_name (files[i]);
	langs[i] = get_lang(names[i]);
	if (strchr (langs[i], '-'))
	    country[ncountry++] = i;

	total_leaves += sets[i]->num;
	i++;
	fclose (f);
    }
    sets[i] = 0;
    leaves = malloc (total_leaves * sizeof (FcCharLeaf *));
    tl = 0;
    /*
     * Find unique leaves
     */
    for (i = 0; sets[i]; i++)
    {
	sleaves = sets[i]->leaves;
	for (sl = 0; sl < sets[i]->num; sl++)
	{
	    for (l = 0; l < tl; l++)
		if (leaves[l] == sleaves[sl])
		    break;
	    if (l == tl)
		leaves[tl++] = sleaves[sl];
	}
    }

    /*
     * Scan the input until the marker is found
     */
    
    while (fgets (line, sizeof (line), stdin))
    {
	if (!strncmp (line, "@@@", 3))
	    break;
	fputs (line, stdout);
    }
    
    printf ("/* total size: %d unique leaves: %d */\n\n",
	    total_leaves, tl);
    /*
     * Dump leaves
     */
    printf ("static const FcCharLeaf	leaves[%d] = {\n", tl);
    for (l = 0; l < tl; l++)
    {
	printf ("    { { /* %d */", l);
	for (i = 0; i < 256/32; i++)
	{
	    if (i % 4 == 0)
		printf ("\n   ");
	    printf (" 0x%08x,", leaves[l]->map[i]);
	}
	printf ("\n    } },\n");
    }
    printf ("};\n\n");
    printf ("#define L(n) ((FcCharLeaf *) &leaves[n])\n\n");

    /*
     * Find duplicate charsets
     */
    duplicate[0] = -1;
    for (i = 1; sets[i]; i++)
    {
	int j;

	duplicate[i] = -1;
	for (j = 0; j < i; j++)
	    if (sets[j] == sets[i])
	    {
		duplicate[i] = j;
		break;
	    }
    }

    /*
     * Dump arrays
     */
    for (i = 0; sets[i]; i++)
    {
	int n;
	
	if (duplicate[i] >= 0)
	    continue;
	printf ("static const FcCharLeaf *leaves_%s[%d] = {\n",
		names[i], sets[i]->num);
	for (n = 0; n < sets[i]->num; n++)
	{
	    if (n % 8 == 0)
		printf ("   ");
	    for (l = 0; l < tl; l++)
		if (leaves[l] == sets[i]->leaves[n])
		    break;
	    if (l == tl)
		fatal (names[i], 0, "can't find leaf");
	    printf (" L(%3d),", l);
	    if (n % 8 == 7)
		printf ("\n");
	}
	if (n % 8 != 0)
	    printf ("\n");
	printf ("};\n\n");
	

	printf ("static const FcChar16 numbers_%s[%d] = {\n",
		names[i], sets[i]->num);
	for (n = 0; n < sets[i]->num; n++)
	{
	    if (n % 8 == 0)
		printf ("   ");
	    printf (" 0x%04x,", sets[i]->numbers[n]);
	    if (n % 8 == 7)
		printf ("\n");
	}
	if (n % 8 != 0)
	    printf ("\n");
	printf ("};\n\n");
    }
    printf ("#undef L\n\n");
    /*
     * Dump sets
     */
    printf ("static const FcLangCharSet  fcLangCharSets[] = {\n");
    for (i = 0; sets[i]; i++)
    {
	int	j = duplicate[i];
	if (j < 0)
	    j = i;
	printf ("    { (FcChar8 *) \"%s\",\n"
		"      { FC_REF_CONSTANT, %d, "
		"(FcCharLeaf **) leaves_%s, "
		"(FcChar16 *) numbers_%s } },\n",
		langs[i],
		sets[j]->num, names[j], names[j]);
    }
    printf ("};\n\n");
    printf ("#define NUM_LANG_CHAR_SET	%d\n", i);
    num_lang_set_map = (i + 31) / 32;
    printf ("#define NUM_LANG_SET_MAP	%d\n", num_lang_set_map);
    /*
     * Dump indices with country codes
     */
    if (ncountry)
    {
	int	ncountry_ent = 0;
	printf ("\n");
	printf ("static const FcChar32 fcLangCountrySets[][NUM_LANG_SET_MAP] = {\n");
	for (c = 0; c < ncountry; c++)
	{
	    i = country[c];
	    if (i >= 0)
	    {
		int l = strchr (langs[i], '-') - langs[i];
		int d, k;

		for (k = 0; k < num_lang_set_map; k++)
		    map[k] = 0;

		BitSet (map, i);
		for (d = c + 1; d < ncountry; d++)
		{
		    int j = country[d];
		    if (j >= 0 && !strncmp (langs[j], langs[i], l))
		    {
			BitSet(map, j);
			country[d] = -1;
		    }
		}
		printf ("    {");
		for (k = 0; k < num_lang_set_map; k++)
		    printf (" 0x%08x,", map[k]);
		printf (" }, /* %*.*s */\n",
			l, l, langs[i]);
		++ncountry_ent;
	    }
	}
	printf ("};\n\n");
	printf ("#define NUM_COUNTRY_SET %d\n", ncountry_ent);
    }
    
    while (fgets (line, sizeof (line), stdin))
	fputs (line, stdout);
    
    fflush (stdout);
    exit (ferror (stdout));
}
