/*
 * Convert ASCII to PostScript.
 * Copyright (c) 1995-2002 Markku Rossi.
 *
 * Author: Markku Rossi <mtr@iki.fi>
 */

/*
 * This file is part of GNU enscript.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <limits.h>
#include "gsint.h"

/*
 * Types and definitions.
 */

/* Values for token flags. */

/* EPSF. */
#define F_EPSF_CENTER			0x01
#define F_EPSF_RIGHT			0x02
#define M_EPSF_JUSTIFICATION		0x03

#define F_EPSF_NO_CPOINT_UPDATE_X	0x04
#define F_EPSF_NO_CPOINT_UPDATE_Y	0x08

#define F_EPSF_ABSOLUTE_X		0x10
#define F_EPSF_ABSOLUTE_Y		0x20

#define F_EPSF_SCALE_X			0x40
#define F_EPSF_SCALE_Y			0x80


/* Predicate to check if we are at the correct slice. */
#define CORRECT_SLICE() (slicing == 0 || current_slice == slice)

/* Predicates for the current body font. */

/* Is character <ch> printable. */
#define ISPRINT(ch) (font_ctype[(unsigned char) (ch)] != ' ')

/* Does character <ch> exist in current body font? */
#define EXISTS(ch) (font_ctype[(unsigned char) (ch)] == '*')


#define RESOURCE_LINE_WIDTH 75

/* Token types. */
typedef enum
{
  tNONE,
  tEOF,
  tSTRING,
  tFORMFEED,
  tNEWLINE,
  tCARRIAGE_RETURN,
  tWRAPPED_NEWLINE,
  tEPSF,
  tSETFILENAME,
  tSETPAGENUMBER,
  tNEWPAGE,
  tFONT,
  tCOLOR,
  tBGCOLOR,
  tSAVEX,
  tLOADX,
  tPS
} TokenType;

/* Special escape tokens. */
typedef enum
{
  ESC_COMMENT,
  ESC_EPSF,
  ESC_FONT,
  ESC_COLOR,
  ESC_BGCOLOR,
  ESC_NEWPAGE,
  ESC_SETFILENAME,
  ESC_SETPAGENUMBER,
  ESC_SHADE,
  ESC_BGGRAY,
  ESC_ESCAPE,
  ESC_SAVEX,
  ESC_LOADX,
  ESC_PS
} SpecialEscape;

/* Token structure. */
struct gs_token_st
{
  TokenType type;
  unsigned int flags;
  double new_x;			/* Current point x after this token. */
  double new_y;			/* Current point y after this token. */
  int new_col;			/* Line column after this token. */

  union
    {
      int i;
      char *str;
      struct
	{
	  double x;		/* x-offset */
	  double y;		/* y-offset */
	  double w;		/* width */
	  double h;		/* height */
	  double xscale;
	  double yscale;
	  int llx, lly, urx, ury; /* Bounding box. */
	  char filename[PATH_MAX];
	  char *skipbuf;
	  unsigned int skipbuf_len;
	  unsigned int skipbuf_pos;
	  FILE *fp;		/* File from which eps image is read. */
	  int pipe;		/* Is <fp> opened to pipe?  */
	} epsf;
      Color color;
      Color bgcolor;
      struct
	{
	  char name[PATH_MAX];
	  FontPoint size;
	  InputEncoding encoding;
	} font;
      char filename[PATH_MAX];
    } u;
};

typedef struct gs_token_st Token;


/*
 * Prototypes for static functions.
 */

static void get_next_token ___P ((InputStream *is, double linestart,
				  double linepos, unsigned int col,
				  double linew, Token *token));

static void dump_ps_page_header ___P ((char *fname, int empty));

static void dump_ps_page_trailer ();

static void dump_empty_page ();

/*
 * Recognize a EPS file described by <token>.  Returns 1 if file was a
 * valid EPS file or 0 otherwise.  File is accepted if it starts with
 * the PostScript magic `%!' and it has a valid `%%BoundingBox' DSC
 * comment.
 */
static int recognize_eps_file ___P ((Token *token));

/*
 * Insert EPS file described by <token> to the output stream.
 */
static void paste_epsf ___P ((Token *token));

/*
 * Check if InputStream <is> contains a file which can be passed
 * through without any modifications.  Returns 1 if file was passed or
 * 0 otherwise.
 */
static int do_pass_through ___P ((char *fname, InputStream *is));

/*
 * Read one float dimension from InputStream <is>.  If <units> is
 * true, number can be followed by an optional unit specifier.  If
 * <horizontal> is true, dimension is horizontal, otherwise it is
 * vertical (this is used to find out how big `line' units are).
 */
static double read_float ___P ((InputStream *is, int units, int horizontal));

/*
 * Print linenumber <linenum> to the beginning of the current line.
 * Current line start is specified by point (x, y).
 */
static void print_line_number ___P ((double x, double y, double space,
				     double margin, unsigned int linenum));

/* Send PostScript to the output file. */
#define OUTPUT(body)	\
  do {			\
    if (cofp == NULL)	\
      cofp = ofp;	\
    if (do_print)	\
      fprintf body;	\
  } while (0)

/* Divert output to tmp file so the total page count can be counted. */
static void divert ();

/* Paste diverted data to the output and patch the total page counts. */
static void undivert ();

/*
 * Handle two-side printing related binding options.  This function is
 * called once for each even-numbered page.
 */
static void handle_two_side_options ();

/*
 * Global variables.
 */

unsigned int current_pagenum = 0; /* The number of the current page. */
unsigned int total_pages_in_file;
unsigned int input_filenum = 0;
unsigned int current_file_linenum;
int first_pagenum_for_file;
char *fname = NULL;		/* The name of the current input file. */


/*
 * Static variables
 */

/* Have we dumped PS header? */
static int ps_header_dumped = 0;

/* Divert file. */
static FILE *divertfp = NULL;

/* Current output() file. */
static FILE *cofp = NULL;

/* To print or not to print, that's a question. */
static int do_print = 1;

/* Is ^@font{}-defined font active? */
static int user_fontp = 0;

/* The user ^@font{}-defined font. */
static char user_font_name[PATH_MAX];
static FontPoint user_font_pt;
static InputEncoding user_font_encoding;

/* Is ^@color{}-defined color active? */
static int user_colorp = 0;

/* The user ^@color{}-defined color. */
static Color user_color;

/* Is ^@bgcolor{}-defined color active? */
static int user_bgcolorp = 0;

/* The user ^@bgcolor{}-defined color. */
static Color user_bgcolor;

/* The last linenumber printed by print_line_number(). */
static unsigned int print_line_number_last;

/* Registers to store X-coordinates with the ^@savex{} escape.
   Initially these are uninitialized. */
static double xstore[256];

/*
 * Global functions.
 */

void
dump_ps_header ()
{
  char *cp, *cp2;
  int i, j, got;

  /* Dump PS header only once. */
  if (ps_header_dumped)
    return;
  ps_header_dumped = 1;

  /*
   * Header.
   */

  OUTPUT ((cofp, "%s\n", output_first_line));
  OUTPUT ((cofp, "%%%%BoundingBox: %d %d %d %d\n", media->llx, media->lly,
	   media->urx, media->ury));
  OUTPUT ((cofp, "%%%%Title: %s\n", title));
  OUTPUT ((cofp, "%%%%For: %s\n", passwd->pw_gecos));
  OUTPUT ((cofp, "%%%%Creator: %s\n", version_string));
  OUTPUT ((cofp, "%%%%CreationDate: %s\n", date_string));
  OUTPUT ((cofp, "%%%%Orientation: %s\n",
	   ((nup > 1) && nup_landscape)
	   || ((nup == 1) && landscape) ? "Landscape" : "Portrait"));
  OUTPUT ((cofp, "%%%%Pages: (atend)\n"));
  OUTPUT ((cofp, "%%%%DocumentMedia: %s %d %d 0 () ()\n",
	   media->name, media->w, media->h));
  OUTPUT ((cofp, "%%%%DocumentNeededResources: (atend)\n"));

  if (count_key_value_set (pagedevice) > 0)
    OUTPUT ((cofp, "%%%%LanguageLevel: 2\n"));

  OUTPUT ((cofp, "%%%%EndComments\n"));


  /*
   * Procedure Definitions.
   */

  OUTPUT ((cofp, "%%%%BeginProlog\n"));

  /* Prolog. */
  OUTPUT ((cofp, "%%%%BeginResource: procset Enscript-Prolog %s\n",
	   ps_version_string));
  if (!paste_file ("enscript", ".pro"))
    FATAL ((stderr, _("couldn't find prolog \"%s\": %s\n"), "enscript.pro",
	    strerror (errno)));
  OUTPUT ((cofp, "%%%%EndResource\n"));

  /* Encoding vector. */
  OUTPUT ((cofp, "%%%%BeginResource: procset Enscript-Encoding-%s %s\n",
	   encoding_name, ps_version_string));
  if (!paste_file (encoding_name, ".enc"))
    FATAL ((stderr, _("couldn't find encoding file \"%s.enc\": %s\n"),
	    encoding_name, strerror (errno)));
  OUTPUT ((cofp, "%%%%EndResource\n"));

  OUTPUT ((cofp, "%%%%EndProlog\n"));


  /*
   * Document Setup.
   */

  OUTPUT ((cofp, "%%%%BeginSetup\n"));

  /* Download fonts. */
  for (got = strhash_get_first (download_fonts, &cp, &j, (void **) &cp2); got;
       got = strhash_get_next (download_fonts, &cp, &j, (void **) &cp2))
    download_font (cp);

  /* For each required font, emit %%IncludeResouce comment. */
  for (got = strhash_get_first (res_fonts, &cp, &j, (void **) &cp2); got;
       got = strhash_get_next (res_fonts, &cp, &j, (void **) &cp2))
    OUTPUT ((cofp, "%%%%IncludeResource: font %s\n", cp));

  OUTPUT ((cofp, "/HFpt_w %g def\n", HFpt.w));
  OUTPUT ((cofp, "/HFpt_h %g def\n", HFpt.h));


  /* Select our fonts. */

  /* Header font HF. */
  OUTPUT ((cofp, "/%s /HF-gs-font MF\n", HFname));
  OUTPUT ((cofp,
	   "/HF /HF-gs-font findfont [HFpt_w 0 0 HFpt_h 0 0] makefont def\n"));

  /* Our default typing font F. */
  OUTPUT ((cofp, "/%s /F-gs-font MF\n", Fname));
  OUTPUT ((cofp, "/F-gs-font %g %g SF\n", Fpt.w, Fpt.h));

  /* Underlay. */
  if (underlay != NULL)
    {
      OUTPUT ((cofp, "/ul_str (%s) def\n", underlay));
      OUTPUT ((cofp, "/ul_w_ptsize %g def\n", ul_ptsize.w));
      OUTPUT ((cofp, "/ul_h_ptsize %g def\n", ul_ptsize.h));
      OUTPUT ((cofp, "/ul_gray %g def\n", ul_gray));
      OUTPUT ((cofp, "/ul_x %g def\n", ul_x));
      OUTPUT ((cofp, "/ul_y %g def\n", ul_y));
      OUTPUT ((cofp, "/ul_angle %g def\n", ul_angle));
      OUTPUT ((cofp, "/ul_style %d def\n", ul_style));
      OUTPUT ((cofp, "/%s /F-ul-font MF\n", ul_font));
      OUTPUT ((cofp, "/ul_font /F-ul-font findfont \
[ul_w_ptsize 0 0 ul_h_ptsize 0 0] makefont def\n"));
    }

  /* Number of copies. */
  OUTPUT ((cofp, "/#copies %d def\n", num_copies));

  /* Page prefeed. */
  if (page_prefeed)
    OUTPUT ((cofp, "true page_prefeed\n"));

  /* Statusdict definitions. */
  if (count_key_value_set (statusdict) > 0)
    {
      OUTPUT ((cofp, "%% Statustdict definitions:\nstatusdict begin\n  "));
      i = 2;
      for (got = strhash_get_first (statusdict, &cp, &j, (void **) &cp2); got;
	   got = strhash_get_next (statusdict, &cp, &j, (void **) &cp2))
	{
	  j = strlen (cp) + 1 + strlen (cp2) + 1;
	  if (i + j > RESOURCE_LINE_WIDTH)
	    {
	      OUTPUT ((cofp, "\n  "));
	      i = 2;
	    }
	  OUTPUT ((cofp, "%s %s ", cp2, cp));
	  i += j;
	}
      OUTPUT ((cofp, "\nend\n"));
    }

  /* Page device definitions. */
  if (pslevel >= 2 &&
      (count_key_value_set (pagedevice) > 0 || generate_PageSize))
    {
      OUTPUT ((cofp, "%% Pagedevice definitions:\n"));
      OUTPUT ((cofp, "gs_languagelevel 1 gt {\n  <<\n    "));

      i = 4;
      for (got = strhash_get_first (pagedevice, &cp, &j, (void **) &cp2); got;
	   got = strhash_get_next (pagedevice, &cp, &j, (void **) &cp2))
	{
	  j = strlen (cp2) + 1 + strlen (cp) + 2;
	  if (i + j > RESOURCE_LINE_WIDTH)
	    {
	      OUTPUT ((cofp, "\n    "));
	      i = 4;
	    }
	  OUTPUT ((cofp, "/%s %s ", cp, cp2));
	  i += j;
	}

      if (generate_PageSize)
	{
	  if (i + 21 > RESOURCE_LINE_WIDTH)
	    {
	      OUTPUT ((cofp, "\n    "));
	      i = 4;
	    }
	  OUTPUT ((cofp, "/PageSize [%d %d] ", media->w, media->h));
	  i += 21;
	}

      OUTPUT ((cofp, "\n  >> setpagedevice\n} if\n"));
    }

  /*
   * Dump header procset.  Header must come after all font inclusions
   * and enscript's dynamic state definition.
   */
  if (header != HDR_NONE)
    {
      char *hdr;
      if (header == HDR_SIMPLE)
	hdr = "simple";
      else
	hdr = fancy_header_name;

      OUTPUT ((cofp, "%%%%BeginResource: procset Enscript-Header-%s %s\n",
	       hdr, ps_version_string));
      if (!paste_file (hdr, ".hdr"))
	FATAL ((stderr,
		_("couldn't find header definition file \"%s.hdr\": %s\n"),
		hdr, strerror (errno)));
      OUTPUT ((cofp, "%%%%EndResource\n"));
    }

  /*
   * Count output width and height here; we can't do it earlier because
   * header might have just allocated some extra space.
   */
  d_output_w = d_page_w;
  d_output_h = d_page_h - d_header_h - d_footer_h;

  /* Dump our current dynamic state. */
  OUTPUT ((cofp, "/d_page_w %d def\n", d_page_w));
  OUTPUT ((cofp, "/d_page_h %d def\n", d_page_h));

  OUTPUT ((cofp, "/d_header_x %d def\n", 0));
  OUTPUT ((cofp, "/d_header_y %d def\n", d_output_h + d_footer_h));
  OUTPUT ((cofp, "/d_header_w %d def\n", d_header_w));
  OUTPUT ((cofp, "/d_header_h %d def\n", d_header_h));

  OUTPUT ((cofp, "/d_footer_x %d def\n", 0));
  OUTPUT ((cofp, "/d_footer_y %d def\n", 0));
  OUTPUT ((cofp, "/d_footer_w %d def\n", d_header_w));
  OUTPUT ((cofp, "/d_footer_h %d def\n", d_footer_h));

  OUTPUT ((cofp, "/d_output_w %d def\n", d_output_w));
  OUTPUT ((cofp, "/d_output_h %d def\n", d_output_h));
  OUTPUT ((cofp, "/cols %d def\n", num_columns));

  OUTPUT ((cofp, "%%%%EndSetup\n"));
}


void
dump_ps_trailer ()
{
  int i, j, got;
  char *cp;
  void *value;
  unsigned int nup_subpage;

  if (!ps_header_dumped)
    /* No header, let's be consistent and forget trailer also. */
    return;

  /* The possible pending N-up showpage. */
  nup_subpage = (total_pages - 1) % nup;
  if (nup > 1 && nup_subpage + 1 != nup)
    /* N-up showpage missing. */
    OUTPUT ((cofp, "_R\nS\n"));

  /* Trailer. */

  OUTPUT ((cofp, "%%%%Trailer\n"));

  if (page_prefeed)
    OUTPUT ((cofp, "false page_prefeed\n"));

  OUTPUT ((cofp, "%%%%Pages: %d\n", total_pages));

  /* Document needed resources. */

  /* fonts. */
  OUTPUT ((cofp, "%%%%DocumentNeededResources: font "));
  i = 32;			/* length of the previous string. */
  for (got = strhash_get_first (res_fonts, &cp, &j, &value); got;
       got = strhash_get_next (res_fonts, &cp, &j, &value))
    {
      if (i + strlen (cp) + 1 > RESOURCE_LINE_WIDTH)
	{
	  OUTPUT ((cofp, "\n%%%%+ font "));
	  i = 9;		/* length of the previous string. */
	}
      OUTPUT ((cofp, "%s ", cp));
      i += strlen (cp) + 1;
    }
  OUTPUT ((cofp, "\n%%%%EOF\n"));
}


void
process_file (char *fname_arg, InputStream *is, int is_toc)
{
  int col;
  double x, y;
  double lx, ly;
  double linewidth;		/* Line width in points. */
  double lineend;
  int done = 0;
  int page_clear = 1;
  unsigned int line_column;
  unsigned int current_linenum;
  double linenumber_space = 0;
  double linenumber_margin = 0;
  Token token;
  int reuse_last_token = 0;
  unsigned int current_slice = 1;
  int last_wrapped_line = -1;
  int last_spaced_file_linenum = -1;
  int save_current_pagenum;
  int toc_pagenum = 0;

  /* Save filename. */
  xfree (fname);
  fname = xstrdup (fname_arg);

  /* Init page number and line counters. */
  if (!continuous_page_numbers)
    current_pagenum = 0;
  total_pages_in_file = 0;
  current_file_linenum = start_line_number;

  /*
   * Count possible line number spaces.  This should be enought for 99999
   * lines
   */
  linenumber_space = CHAR_WIDTH ('0') * 5 + 1.0;
  linenumber_margin = CHAR_WIDTH (':') + CHAR_WIDTH ('m');

  /* We got a new input file. */
  input_filenum++;

  /* We haven't printed any line numbers yet. */
  print_line_number_last = (unsigned int) -1;

  if (pass_through || output_language_pass_through)
    if (do_pass_through (fname, is))
      /* All done. */
      return;

  /* We have work to do, let's give header a chance to dump itself. */
  dump_ps_header ();

  /*
   * Align files to the file_align boundary, this is handy for two-side
   * printing.
   */
  while ((total_pages % file_align) != 0)
    {
      total_pages++;
      dump_empty_page ();
    }

  MESSAGE (1, (stderr, _("processing file \"%s\"...\n"), fname));

  linewidth = d_output_w / num_columns - 2 * d_output_x_margin
    - line_indent;

  /* Save the current running page number for possible toc usage. */
  first_pagenum_for_file = total_pages + 1;

  /*
   * Divert our output to a temp file.  We will re-process it
   * afterwards to patch, for example, the number of pages in the
   * document.
   */
  divert ();

  /* Process this input file. */
  while (!done)
    {
      /* Start a new page. */
      page_clear = 1;

      for (col = 0; !done && col < num_columns; col++)
	{
	  /* Move to the beginning of the column <col>. */
	  lx = x = col * d_output_w / (float) num_columns + d_output_x_margin
	    + line_indent;
	  lineend = lx + linewidth;

	  ly = y = d_footer_h + d_output_h - d_output_y_margin - LINESKIP;
	  current_linenum = 0;
	  line_column = 0;

	  while (1)
	    {
	      if (line_numbers && line_column == 0
		  && (current_file_linenum != last_spaced_file_linenum))
		{
		  /* Forward x by the amount needed by our line numbers. */
		  x += linenumber_space + linenumber_margin;
		  last_spaced_file_linenum = current_file_linenum;
		}

	      /* Get token. */
	      if (!reuse_last_token)
		get_next_token (is, lx, x, line_column, lineend, &token);
	      reuse_last_token = 0;

	      /*
	       * Page header printing is delayed to this point because
	       * we want to handle files ending with a newline character
	       * with care.  If the last newline would cause a pagebreak,
	       * otherwise we would print page header to the non-existent
	       * next page and that would be ugly ;)
	       */

	      if (token.type == tEOF)
		{
		  done = 1;
		  goto end_of_page;
		}

	      /*
	       * Now we know that we are going to make marks to this page
	       * => print page header.
	       */

	      if (page_clear)
		{
		  PageRange *pr;

		  current_pagenum++;
		  total_pages_in_file++;

		  /* Check page ranges. */
		  if (page_ranges == NULL)
		    do_print = 1;
		  else
		    {
		      do_print = 0;
		      for (pr = page_ranges; pr; pr = pr->next)
			{
			  if (pr->odd || pr->even)
			    {
			      if ((pr->odd && (current_pagenum % 2) == 1)
				  || (pr->even && (current_pagenum % 2) == 0))
				{
				  do_print = 1;
				  break;
				}
			    }
			  else
			    {
			      if (pr->start <= current_pagenum
				  && current_pagenum <= pr->end)
				{
				  do_print = 1;
				  break;
				}
			    }
			}
		    }

		  if (do_print)
		    total_pages++;

		  if (is_toc)
		    {
		      save_current_pagenum = current_pagenum;
		      toc_pagenum--;
		      current_pagenum = toc_pagenum;
		    }

		  dump_ps_page_header (fname, 0);
		  page_clear = 0;

		  if (is_toc)
		    current_pagenum = save_current_pagenum;
		}

	      /* Print line highlight. */
	      if (line_column == 0 && line_highlight_gray < 1.0)
		OUTPUT ((cofp, "%g %g %g %g %g line_highlight\n",
			 lx, (y - baselineskip
			      + (font_bbox_lly * Fpt.h / UNITS_PER_POINT)),
			 linewidth, Fpt.h + baselineskip,
			 line_highlight_gray));

	      /* Print line numbers if needed. */
	      if (line_numbers && line_column == 0 && token.type != tFORMFEED)
		print_line_number (lx, y, linenumber_space, linenumber_margin,
				   current_file_linenum);

	      /* Check rest of tokens. */
	      switch (token.type)
		{
		case tFORMFEED:
		  switch (formfeed_type)
		    {
		    case FORMFEED_COLUMN:
		      goto end_of_column;
		      break;

		    case FORMFEED_PAGE:
		      goto end_of_page;
		      break;

		    case FORMFEED_HCOLUMN:
		      /*
		       * Advance y-coordinate to the next even
		       * `horizontal_column_height' position.
		       */
		      {
			int current_row;

			current_row = (ly - y) / horizontal_column_height;
			y = ly - (current_row + 1) * horizontal_column_height;

			/* Check the end of the page. */
			if (y < d_footer_h + d_output_y_margin)
			  goto end_of_column;
		      }
		      break;
		    }
		  break;

		case tSTRING:
		  if (CORRECT_SLICE ())
		    {
		      if (bggray < 1.0)
			{
			  OUTPUT ((cofp, "%g %g %g %g %g (%s) bgs\n", x, y,
				   Fpt.h + baselineskip,
				   baselineskip
				   - (font_bbox_lly * Fpt.h / UNITS_PER_POINT),
				   bggray,
				   token.u.str));
			}
		      else if (user_bgcolorp)
			{
			  OUTPUT ((cofp, "%g %g %g %g %g %g %g (%s) bgcs\n",
				   x, y, Fpt.h + baselineskip,
				   baselineskip
				   - (font_bbox_lly * Fpt.h / UNITS_PER_POINT),
				   user_bgcolor.r,
				   user_bgcolor.g,
				   user_bgcolor.b,
				   token.u.str));
			}
		      else
			{
			  OUTPUT ((cofp, "%g %g M\n(%s) s\n", x, y,
				   token.u.str));
			}
		    }
		  x = token.new_x;
		  line_column = token.new_col;
		  break;

		case tCARRIAGE_RETURN:
		  /* Just reset the x-coordinate. */
		  x = col * d_output_w / (float) num_columns
		    + d_output_x_margin + line_indent;
		  line_column = 0;
		  break;

		case tNEWLINE:
		case tWRAPPED_NEWLINE:
		  if (token.type == tNEWLINE)
		    {
		      current_file_linenum++;
		      current_slice = 1;
		      y -= LINESKIP;
		    }
		  else
		    {
		      current_slice++;
		      if (!slicing)
			{
			  /* Mark wrapped line marks. */
			  switch (mark_wrapped_lines_style)
			    {
			    case MWLS_NONE:
			      /* nothing */
			      break;

			    case MWLS_PLUS:
			      OUTPUT ((cofp, "%g %g M (+) s\n", x, y));
			      break;

			    default:
			      /* Print some fancy graphics. */
			      OUTPUT ((cofp,
				       "%g %g %g %g %d wrapped_line_mark\n",
				       x, y, Fpt.w, Fpt.h,
				       mark_wrapped_lines_style));
			      break;
			    }

			  /*
			   * For wrapped newlines, decrement y only if
			   * we are not slicing the input.
			   */
			  y -= LINESKIP;
			}

		      /* Count the wrapped lines here. */
		      if (!slicing || current_slice > slice)
			if (current_file_linenum != last_wrapped_line)
			  {
			    if (do_print)
			      num_truncated_lines++;
			    last_wrapped_line = current_file_linenum;
			  }
		    }

		  current_linenum++;
		  if (current_linenum >= lines_per_page
		      || y < d_footer_h + d_output_y_margin)
		    goto end_of_column;

		  x = col * d_output_w / (float) num_columns
		    + d_output_x_margin + line_indent;
		  line_column = 0;
		  break;

		case tEPSF:
		  /* Count current point movement. */

		  if (token.flags & F_EPSF_ABSOLUTE_Y)
		    token.new_y = ly;
		  else
		    token.new_y = y;
		  token.new_y += token.u.epsf.y - token.u.epsf.h;

		  if (token.flags & F_EPSF_ABSOLUTE_X)
		    token.new_x = lx;
		  else
		    token.new_x = x;
		  token.new_x += token.u.epsf.x;

		  /* Check flags. */

		  /* Justification flags overwrite <x_ofs>. */
		  if (token.flags & F_EPSF_CENTER)
		    token.new_x = lx + (linewidth - token.u.epsf.w) / 2;
		  if (token.flags & F_EPSF_RIGHT)
		    token.new_x = lx + (linewidth - token.u.epsf.w);

		  /* Check if eps file does not fit to this column. */
		  if ((token.flags & F_EPSF_NO_CPOINT_UPDATE_Y) == 0
		      && token.new_y < d_footer_h + d_output_y_margin)
		    {
		      if (current_linenum == 0)
			{
			  /*
			   * At the beginning of the column, warn user
			   * and print image.
			   */
			  MESSAGE (0, (stderr, _("EPS file \"%s\" is too \
large for page\n"),
				       token.u.epsf.filename));
			}
		      else
			{
			  /* Must start a new column. */
			  reuse_last_token = 1;
			  goto end_of_column;
			}
		    }

		  /* Do paste. */
		  if (CORRECT_SLICE ())
		    paste_epsf (&token);

		  /* Update current point? */
		  if (!(token.flags & F_EPSF_NO_CPOINT_UPDATE_Y))
		    y = token.new_y;
		  if (!(token.flags & F_EPSF_NO_CPOINT_UPDATE_X))
		    x = token.new_x + token.u.epsf.w;

		  if (y < d_footer_h + d_output_y_margin)
		    goto end_of_column;
		  break;

		case tFONT:
		  /* Select a new current font. */
		  if (line_column == 0)
		    {
		      double newh;

		      /* Check for possible line skip change. */
		      if (token.u.font.name[0] == '\0')
			newh = default_Fpt.h;
		      else
			newh = token.u.font.size.h;

		      if (newh != Fpt.h)
			{
			  /* We need a different line skip value. */
			  y -= (newh - Fpt.h);
			}
		      /*
		       * We must check for page overflow after we have
		       * set the new font.
		       */
		    }

		  MESSAGE (2, (stderr, "^@font="));
		  if (token.u.font.name[0] == '\0')
		    {
		      /* Select the default font. */
		      Fpt.w = default_Fpt.w;
		      Fpt.h = default_Fpt.h;
		      Fname = default_Fname;
		      encoding = default_Fencoding;
		      OUTPUT ((cofp, "/F-gs-font %g %g SF\n", Fpt.w, Fpt.h));
		      user_fontp = 0;
		    }
		  else
		    {
		      strhash_put (res_fonts, token.u.font.name,
				   strlen (token.u.font.name) + 1,
				   NULL, NULL);
		      if (token.u.font.encoding == default_Fencoding)
			OUTPUT ((cofp, "/%s %g %g SUF\n", token.u.font.name,
				 token.u.font.size.w, token.u.font.size.h));
		      else if (token.u.font.encoding == ENC_PS)
			OUTPUT ((cofp, "/%s %g %g SUF_PS\n", token.u.font.name,
				 token.u.font.size.w, token.u.font.size.h));
		      else
			FATAL ((stderr,
				_("user font encoding can be only the system's default or `ps'")));

		      memset  (user_font_name, 0, sizeof(user_font_name));
		      strncpy (user_font_name, token.u.font.name, sizeof(user_font_name) - 1);
		      user_font_pt.w = token.u.font.size.w;
		      user_font_pt.h = token.u.font.size.h;
		      user_font_encoding = token.u.font.encoding;
		      user_fontp = 1;

		      Fpt.w = user_font_pt.w;
		      Fpt.h = user_font_pt.h;
		      Fname = user_font_name;
		      encoding = user_font_encoding;
		    }
		  MESSAGE (2, (stderr, "%s %g/%gpt\n", Fname, Fpt.w, Fpt.h));
		  read_font_info ();

		  /*
		   * Check for page overflow in that case that we were
		   * at the first column and font were changed to a bigger
		   * one.
		   */
		  if (y < d_footer_h + d_output_y_margin)
		    goto end_of_column;
		  break;

		case tCOLOR:
		  /* Select a new color. */
		  MESSAGE (2, (stderr, "^@color{%f %f %f}\n",
			       token.u.color.r,
			       token.u.color.g,
			       token.u.color.b));
		  if (token.u.color.r == token.u.color.g
		      && token.u.color.g == token.u.color.b
		      && token.u.color.b == 0.0)
		    {
		      /* Select the default color (black). */
		      OUTPUT ((cofp, "0 setgray\n"));
		      user_colorp = 0;
		    }
		  else
		    {
		      OUTPUT ((cofp, "%g %g %g setrgbcolor\n",
			       token.u.color.r,
			       token.u.color.g,
			       token.u.color.b));

		      user_color.r = token.u.color.r;
		      user_color.g = token.u.color.g;
		      user_color.b = token.u.color.b;
		      user_colorp = 1;
		    }
		  break;

		case tBGCOLOR:
		  /* Select a new background color. */
		  MESSAGE (2, (stderr, "^@bgcolor{%f %f %f}\n",
			       token.u.color.r,
			       token.u.color.g,
			       token.u.color.b));

		  if (token.u.color.r == token.u.color.g
		      && token.u.color.g == token.u.color.b
		      && token.u.color.b == 1.0)
		    {
		      /* Select the default bgcolor (white). */
		      user_bgcolorp = 0;
		    }
		  else
		    {
		      user_bgcolor.r = token.u.color.r;
		      user_bgcolor.g = token.u.color.g;
		      user_bgcolor.b = token.u.color.b;
		      user_bgcolorp = 1;
		    }
		  break;

		case tSETFILENAME:
		  xfree (fname);
		  fname = xstrdup (token.u.filename);
		  break;

		case tSETPAGENUMBER:
		  current_pagenum = token.u.i - 1;
		  break;

		case tNEWPAGE:
		  if (current_linenum >= token.u.i)
		    goto end_of_page;
		  break;

		case tSAVEX:
		  xstore[(unsigned char) token.u.i] = x;
		  break;

		case tLOADX:
		  x = xstore[(unsigned char) token.u.i];
		  break;

		case tPS:
		  OUTPUT ((cofp, "%g %g M\n%s\n", x, y, token.u.str));
		  xfree (token.u.str);
		  break;

		case tNONE:
		default:
		  FATAL ((stderr, "process_file(): got illegal token %d",
			  token.type));
		  break;
		}
	    }
	end_of_column:
	  ;			/* ULTRIX's cc needs this line. */
	}

    end_of_page:
      if (!page_clear)
	dump_ps_page_trailer ();
    }

  /*
   * Reset print flag to true so all the required document trailers
   * etc. get printed properly.
   */
  do_print = 1;

  /* Undivert our output from the temp file to our output stream. */
  undivert ();

  /* Table of contents? */
  if (toc)
    {
      char *cp;
      int save_total_pages = total_pages;

      /* use first pagenum in file for toc */
      total_pages = first_pagenum_for_file;

      cp = format_user_string ("TOC", toc_fmt_string);
      fprintf (toc_fp, "%s\n", cp);
      xfree (cp);

      total_pages = save_total_pages;
    }
}


/*
 * Static functions.
 */

/* Help macros. */

/* Check if character <ch> fits to current line. */
#define FITS_ON_LINE(ch) ((linepos + CHAR_WIDTH (ch) < linew) || col == 0)

/* Is line buffer empty? */
#define BUFFER_EMPTY() (bufpos == 0)

/* Unconditionally append character <ch> to the line buffer. */
#define APPEND_CHAR(ch) 				\
  do {							\
    if (bufpos >= buflen)				\
      {							\
	buflen += 4096;					\
	buffer = xrealloc (buffer, buflen);		\
      }							\
    buffer[bufpos++] = ch;				\
  } while (0)

/*
 * Copy character <ch> (it fits to this line) to output buffer and
 * update current point counters.
 */
#define EMIT(ch) 		\
  do {				\
    APPEND_CHAR (ch);		\
    linepos += CHAR_WIDTH (ch);	\
    col++;			\
  } while (0)

#define UNEMIT(ch)		\
  do {				\
    linepos -= CHAR_WIDTH (ch); \
    col--;			\
  } while (0)

#define ISSPACE(ch) ((ch) == ' ' || (ch) == '\t')
#define ISOCTAL(ch) ('0' <= (ch) && (ch) <= '7')

/* Read one special escape from input <fp>. */

static struct
{
  char *name;
  SpecialEscape escape;
} escapes[] =
  {
    {"comment",		ESC_COMMENT},
    {"epsf", 		ESC_EPSF},
    {"font", 		ESC_FONT},
    {"color",		ESC_COLOR},
    {"bgcolor",		ESC_BGCOLOR},
    {"newpage",		ESC_NEWPAGE},
    {"ps",		ESC_PS},
    {"setfilename",	ESC_SETFILENAME},
    {"setpagenumber",	ESC_SETPAGENUMBER},
    {"shade",		ESC_SHADE},
    {"bggray",		ESC_BGGRAY},
    {"escape",		ESC_ESCAPE},
    {"savex",		ESC_SAVEX},
    {"loadx",		ESC_LOADX},
    {NULL, 0},
  };


static void
read_special_escape (InputStream *is, Token *token)
{
  char escname[256];
  char buf[4096];
  int i, e;
  int ch;

  /* Get escape name. */
  for (i = 0; i < sizeof (escname) - 1 && (ch = is_getc (is)) != EOF; i++)
    {
      if (!isalnum (ch))
	{
	  is_ungetc (ch, is);
	  break;
	}
      else
	escname[i] = ch;
    }
  escname[i] = '\0';

  /* Lookup escape. */
  for (e = 0; escapes[e].name; e++)
    if (strcmp (escname, escapes[e].name) == 0)
      break;
  if (escapes[e].name == NULL)
    FATAL ((stderr, _("unknown special escape: %s"), escname));

  /*
   * The epsf escape takes optional arguments so it must be handled
   * differently.
   */
  if (escapes[e].escape == ESC_EPSF)
    {
      int i;
      int pw, ph;
      double scale;

      token->flags = 0;
      token->u.epsf.x = 0.0;
      token->u.epsf.y = 0.0;
      token->u.epsf.h = 0.0;
      token->u.epsf.pipe = 0;

      ch = is_getc (is);
      if (ch == '[')
	{
	  /* Read options. */
	  while ((ch = is_getc (is)) != EOF && ch != ']')
	    {
	      switch (ch)
		{
		case 'c':	/* center justification */
		  token->flags &= ~M_EPSF_JUSTIFICATION;
		  token->flags |= F_EPSF_CENTER;
		  break;

		case 'n':	/* no current point update */
		  /* Check the next character. */
		  ch = is_getc (is);
		  switch (ch)
		    {
		    case 'x':
		      token->flags |= F_EPSF_NO_CPOINT_UPDATE_X;
		      break;

		    case 'y':
		      token->flags |= F_EPSF_NO_CPOINT_UPDATE_Y;
		      break;

		    default:
		      is_ungetc (ch, is);
		      token->flags |= F_EPSF_NO_CPOINT_UPDATE_X;
		      token->flags |= F_EPSF_NO_CPOINT_UPDATE_Y;
		      break;
		    }
		  break;

		case 'r':	/* right justification */
		  token->flags &= ~M_EPSF_JUSTIFICATION;
		  token->flags |= F_EPSF_RIGHT;
		  break;


		case 's':	/* scale */
		  /* Check the next character. */
		  ch = is_getc (is);
		  switch (ch)
		    {
		    case 'x':
		      token->flags |= F_EPSF_SCALE_X;
		      token->u.epsf.xscale = read_float (is, 0, 1);
		      break;

		    case 'y':
		      token->flags |= F_EPSF_SCALE_Y;
		      token->u.epsf.yscale = read_float (is, 0, 0);
		      break;

		    default:
		      is_ungetc (ch, is);
		      token->flags |= F_EPSF_SCALE_X;
		      token->flags |= F_EPSF_SCALE_Y;
		      token->u.epsf.xscale = token->u.epsf.yscale
			= read_float (is, 0, 1);
		      break;
		    }
		  break;

		case 'x':	/* x-position */
		  token->u.epsf.x = read_float (is, 1, 1);

		  /* Check the next character. */
		  ch = is_getc (is);
		  switch (ch)
		    {
		    case 'a':
		      token->flags |= F_EPSF_ABSOLUTE_X;
		      break;

		    default:
		      is_ungetc (ch, is);
		      break;
		    }
		  break;

		case 'y':	/* y-position */
		  token->u.epsf.y = - read_float (is, 1, 0);

		  /* Check the next character. */
		  ch = is_getc (is);
		  switch (ch)
		    {
		    case 'a':
		      token->flags |= F_EPSF_ABSOLUTE_Y;
		      break;

		    default:
		      is_ungetc (ch, is);
		      break;
		    }
		  break;

		case 'h':	/* height */
		  token->u.epsf.h = read_float (is, 1, 0);
		  break;

		case ' ':
		case '\t':
		  break;

		default:
		  FATAL ((stderr, _("illegal option %c for ^@epsf escape"),
			  ch));
		}
	    }
	  if (ch != ']')
	    FATAL ((stderr,
		    _("malformed ^@epsf escape: no ']' after options")));

	  ch = is_getc (is);
	}
      if (ch == '{')
	{
	  /* Read filename. */
	  for (i = 0; (ch = is_getc (is)) != EOF && ch != '}'; i++)
	    {
	      token->u.epsf.filename[i] = ch;
	      if (i + 1 >= sizeof (token->u.epsf.filename))
		FATAL ((stderr,
			_("too long file name for ^@epsf escape:\n%.*s"),
			i, token->u.epsf.filename));
	    }
	  if (ch == EOF)
	    FATAL ((stderr, _("unexpected EOF while scanning ^@epsf escape")));

	  token->u.epsf.filename[i] = '\0';
	  token->type = tEPSF;
	}
      else
	FATAL ((stderr, _("malformed ^@epsf escape: no '{' found")));

      /*
       * Now we have a valid epsf-token in <token>.  Let's read BoundingBox
       * and do some calculations.
       */
      if (!recognize_eps_file (token))
	/* Recognize eps has already printed error message so we are done. */
	token->type = tNONE;
      else
	{
	  /* Some fixups for x and y dimensions. */
	  token->u.epsf.y += LINESKIP - 1;
	  if (token->u.epsf.h != 0.0)
	    token->u.epsf.h -= 1.0;

	  /* Count picture's width and height. */

	  pw = token->u.epsf.urx - token->u.epsf.llx;
	  ph = token->u.epsf.ury - token->u.epsf.lly;

	  /* The default scale. */
	  if (token->u.epsf.h == 0.0)
	    scale = 1.0;
	  else
	    scale = token->u.epsf.h / ph;

	  if ((token->flags & F_EPSF_SCALE_X) == 0)
	    token->u.epsf.xscale = scale;
	  if ((token->flags & F_EPSF_SCALE_Y) == 0)
	    token->u.epsf.yscale = scale;

	  pw *= token->u.epsf.xscale;
	  ph *= token->u.epsf.yscale;

	  token->u.epsf.w = pw;
	  token->u.epsf.h = ph;
	}
    }
  else if (escapes[e].escape == ESC_COMMENT)
    {
      /* Comment the rest of this line. */
      while ((ch = is_getc (is)) != EOF && ch != nl)
	;
      token->type = tNONE;
    }
  else
    {
      char *cp;
      int parenlevel;

      /*
       * Handle the rest of the escapes.
       */

      /* Read argument. */
      ch = is_getc (is);
      if (ch != '{')
	FATAL ((stderr, _("malformed %s escape: no '{' found"),
		escapes[e].name));

      parenlevel = 0;
      for (i = 0;
	   (ch = is_getc (is)) != EOF && (parenlevel > 0 || ch != '}'); i++)
	{
	  if (ch == '{')
	    parenlevel++;
	  else if (ch == '}')
	    parenlevel--;

	  buf[i] = ch;
	  if (i + 1 >= sizeof (buf))
	    FATAL ((stderr, _("too long argument for %s escape:\n%.*s"),
		    escapes[e].name, i, buf));
	}
      buf[i] = '\0';

      /* And now handle the escape. */
      switch (escapes[e].escape)
	{
	case ESC_FONT:
	  memset  (token->u.font.name, 0, sizeof(token->u.font.name));
	  strncpy (token->u.font.name, buf, sizeof(token->u.font.name) - 1);

	  /* Check for the default font. */
	  if (strcmp (token->u.font.name, "default") == 0)
	    token->u.font.name[0] = '\0';
	  else
	    {
	      if (!parse_font_spec (token->u.font.name, &cp,
				    &token->u.font.size,
				    &token->u.font.encoding))
		FATAL ((stderr, _("malformed font spec for ^@font escape: %s"),
			token->u.font.name));

	      memset  (token->u.font.name, 0, sizeof(token->u.font.name));
	      strncpy (token->u.font.name, cp, sizeof(token->u.font.name) - 1);
	      xfree (cp);
	    }
	  token->type = tFONT;
	  break;

	case ESC_COLOR:
	case ESC_BGCOLOR:
	  /* Check for the default color. */
	  if (strcmp (buf, "default") == 0)
	    {
	      double val = 0;

	      if (escapes[e].escape == ESC_BGCOLOR)
		val = 1;

	      token->u.color.r = val;
	      token->u.color.g = val;
	      token->u.color.b = val;
	    }
	  else
	    {
	      int got;

	      got = sscanf (buf, "%g %g %g",
			    &token->u.color.r,
			    &token->u.color.g,
			    &token->u.color.b);
	      switch (got)
		{
		case 0:
		case 2:
		  FATAL ((stderr,
			  _("malformed color spec for ^@%s escape: %s"),
			  escapes[e].escape == ESC_COLOR
			  ? "color" : "bgcolor",
			  buf));
		  break;

		case 1:
		  token->u.color.g = token->u.color.b = token->u.color.r;
		  break;

		default:
		  /* Got all three components. */
		  break;
		}
	    }
	  if (escapes[e].escape == ESC_COLOR)
	    token->type = tCOLOR;
	  else
	    token->type = tBGCOLOR;
	  break;

	case ESC_SHADE:
	  line_highlight_gray = atof (buf);
	  if (line_highlight_gray < 0.0 || line_highlight_gray > 1.0)
	    FATAL ((stderr, _("invalid value for ^@shade escape: %s"), buf));

	  token->type = tNONE;
	  break;

	case ESC_BGGRAY:
	  bggray = atof (buf);
	  if (bggray < 0.0 || bggray > 1.0)
	    FATAL ((stderr, _("invalid value for ^@bggray escape: %s"), buf));

	  token->type = tNONE;
	  break;

	case ESC_ESCAPE:
	  if (strcmp (buf, "default") == 0)
	    escape_char = default_escape_char;
	  else
	    escape_char = atoi (buf);
	  token->type = tNONE;
	  break;

	case ESC_SETFILENAME:
	  memset  (token->u.filename, 0, sizeof(token->u.font.name));
	  strncpy (token->u.filename, buf, sizeof(token->u.filename) - 1);
	  token->type = tSETFILENAME;
	  break;

	case ESC_SETPAGENUMBER:
	  token->u.i = atoi (buf);
	  token->type = tSETPAGENUMBER;
	  break;

	case ESC_NEWPAGE:
	  if (i == 0)
	    token->u.i = 1;	/* The default is the first line. */
	  else
	    token->u.i = atoi (buf);
	  token->type = tNEWPAGE;
	  break;

	case ESC_SAVEX:
	  token->type = tSAVEX;
	  token->u.i = atoi (buf);
	  break;

	case ESC_LOADX:
	  token->type = tLOADX;
	  token->u.i = atoi (buf);
	  break;

	case ESC_PS:
	  token->u.str = xstrdup (buf);
	  token->type = tPS;
	  break;

	default:
	  /* NOTREACHED */
	  abort ();
	  break;
	}
    }
}


/* Get next token from input file <fp>. */
static void
get_next_token (InputStream *is, double linestart, double linepos,
		unsigned int col, double linew, Token *token)
{
  static unsigned char *buffer = NULL; /* output buffer */
  static unsigned int buflen = 0; /* output buffer's length */
  unsigned int bufpos = 0;	/* current position in output buffer */
  int ch = 0;
  int done = 0;
  int i;
  static int pending_token = tNONE;
  unsigned int original_col = col;

  if (pending_token != tNONE)
    {
      token->type = pending_token;
      pending_token = tNONE;
      return;
    }

#define DONE_DONE 1
#define DONE_WRAP 2

  while (!done)
    {
      ch = is_getc (is);
      switch (ch)
	{
	case EOF:
	  if (BUFFER_EMPTY ())
	    {
	      token->type = tEOF;
	      return;
	    }

	  done = DONE_DONE;
	  break;

	case '\r':
	case '\n':
	  /*
	   * One of these is the newline character and the other one
	   * is carriage return.
	   */
	  if (ch == nl)
	    {
	      /* The newline character. */
	      if (BUFFER_EMPTY ())
		{
		  token->type = tNEWLINE;
		  return;
		}
	      else
		{
		  is_ungetc (ch, is);
		  done = DONE_DONE;
		}
	    }
	  else
	    {
	      /* The carriage return character. */
	      if (BUFFER_EMPTY ())
		{
		  token->type = tCARRIAGE_RETURN;
		  return;
		}
	      else
		{
		  is_ungetc (ch, is);
		  done = DONE_DONE;
		}
	    }
	  break;

	case '\t':
	  if (font_is_fixed)
	    {
	      i = tabsize - (col % tabsize);
	      for (; i > 0; i--)
		{
		  if (FITS_ON_LINE (' '))
		    EMIT (' ');
		  else
		    {
		      done = DONE_WRAP;
		      break;
		    }
		}
	    }
	  else
	    {
	      /* Proportional font. */

	      double grid = tabsize * CHAR_WIDTH (' ');
	      col++;

	      /* Move linepos to the next multiple of <grid>. */
	      linepos = (((int) ((linepos - linestart) / grid) + 1) * grid
			 + linestart);
	      if (linepos >= linew)
		done = DONE_WRAP;
	      else
		done = DONE_DONE;
	    }
	  break;

	case '\f':
	  if (BUFFER_EMPTY ())
	    {
	      if (interpret_formfeed)
		token->type = tFORMFEED;
	      else
		token->type = tNEWLINE;
	      return;
	    }
	  else
	    {
	      is_ungetc (ch, is);
	      done = DONE_DONE;
	    }
	  break;

	default:
	  /* Handle special escapes. */
	  if (special_escapes && ch == escape_char)
	    {
	      if (BUFFER_EMPTY ())
		{
		  /* Interpret special escapes. */
		  read_special_escape (is, token);
		  if (token->type != tNONE)
		    return;

		  /*
		   * Got tNONE special escape => read_special_escape()
		   * has already done what was needed.  Just read more.
		   */
		  break;
		}
	      else
		{
		  is_ungetc (ch, is);
		  done = DONE_DONE;
		  break;
		}
	    }

	  /* Handle backspace character. */
	  if (ch == bs)
	    {
	      if (BUFFER_EMPTY () || !EXISTS (buffer[bufpos - 1]))
		linepos -= CHAR_WIDTH ('m');
	      else
		linepos -= CHAR_WIDTH (buffer[bufpos - 1]);

	      done = DONE_DONE;
	      break;
	    }

	  /* Check normal characters. */
	  if (EXISTS (ch))
	    {
	      if (FITS_ON_LINE (ch))
		{
		  /*
		   * Print control characters (and optionally
		   * characters greater than 127) in the escaped form
		   * so PostScript interpreter will not hang on them.
		   */
		  if (ch < 040 || (clean_7bit && ch >= 0200))
		    {
		      char buf[10];

		      sprintf (buf, "\\%03o", ch);
		      for (i = 0; buf[i]; i++)
			APPEND_CHAR (buf[i]);

		      /* Update current point counters manually. */
		      linepos += CHAR_WIDTH (ch);
		      col++;
		    }
		  else if (ch == '(' || ch == ')' || ch == '\\')
		    {
		      /* These must be quoted in PostScript strings. */
		      APPEND_CHAR ('\\');
		      EMIT (ch);
		    }
		  else
		    EMIT (ch);
		}
	      else
		{
		  is_ungetc (ch, is);
		  done = DONE_WRAP;
		}
	    }
	  else if (ISPRINT (ch))
	    {
	      /* Printable, but do not exists in this font. */
	      if (FITS_ON_LINE ('?'))
		{
		  EMIT ('?');
		  if (missing_chars[ch]++ == 0)
		    num_missing_chars++;
		}
	      else
		{
		  is_ungetc (ch, is);
		  done = DONE_WRAP;
		}
	    }
	  else
	    {
	      char buf[20];
	      double len = 0.0;

	      /*
	       * Non-printable and does not exist in current font, print
	       * it in the format specified by non_printable_format.
	       */

	      if (non_printable_chars[ch]++ == 0)
		num_non_printable_chars++;

	      switch (non_printable_format)
		{
		case NPF_SPACE:
		  strcpy (buf, " ");
		  break;

		case NPF_QUESTIONMARK:
		  strcpy (buf, "?");
		  break;

		case NPF_CARET:
		  if (ch < 0x20)
		    {
		      buf[0] = '^';
		      buf[1] = '@' + ch;
		      buf[2] = '\0';
		      break;
		    }
		  /* FALLTHROUGH */

		case NPF_OCTAL:
		  sprintf (buf, "\\%03o", ch);
		  break;
		}

	      /* Count length. */
	      for (i = 0; buf[i]; i++)
		len += CHAR_WIDTH (buf[i]);

	      if (linepos + len < linew || col == 0)
		{
		  /* Print it. */
		  for (i = 0; buf[i]; i++)
		    {
		      if (buf[i] == '\\')
			APPEND_CHAR ('\\'); /* Escape '\\' characters. */
		      EMIT (buf[i]);
		    }
		}
	      else
		{
		  is_ungetc (ch, is);
		  done = DONE_WRAP;
		}
	    }
	  break;
	}
    }

  /* Got a string. */

  /* Check for wrapped line. */
  if (done == DONE_WRAP)
    {
      /* This line is too long. */
      ch = nl;
      if (line_end == LE_TRUNCATE)
	{
	  /* Truncate this line. */
	  while ((ch = is_getc (is)) != EOF && ch != nl)
	    ;
	}
      else if (!BUFFER_EMPTY () && line_end == LE_WORD_WRAP)
	{
	  int w;

	  if (ISSPACE (buffer[bufpos - 1]))
	    {
	      /* Skip all whitespace from the end of the wrapped line. */
	      while ((w = is_getc (is)) != EOF && ISSPACE (w))
		;
	      is_ungetc (w, is);
	    }
	  else
	    {
	      /* Find the previous word boundary for the wrap. */
	      for (w = bufpos - 1; w >= 0 && !ISSPACE (buffer[w]); w--)
		;
	      w++;
	      if (w > 0 || original_col > 0)
		{
		  /*
		   * Ok, we found a word boundary.  Now we must unemit
		   * characters from the buffer to the intput stream.
		   *
		   * Note:
		   *  - bufpos is unsigned integer variable
		   *  - some characters are escaped with '\\'
		   *  - some characters are printed in octal notation
		   */
		  do
		    {
		      bufpos--;

		      /* Check for '(', ')' and '\\'. */
		      if (bufpos > w
			  && (buffer[bufpos] == '('
			      || buffer[bufpos] ==  ')'
			      || buffer[bufpos] == '\\')
			  && buffer[bufpos - 1] == '\\')
			{
			  is_ungetc (buffer[bufpos], is);
			  UNEMIT (buffer[bufpos]);
			  bufpos--;
			}
		      /* Check the octal notations "\\%03o". */
		      else if (bufpos - 2 > w
			       && ISOCTAL (buffer[bufpos])
			       && ISOCTAL (buffer[bufpos - 1])
			       && ISOCTAL (buffer[bufpos - 2])
			       && buffer[bufpos - 3] == '\\')
			{
			  unsigned int ti;

			  /*
			   * It is a potential octal character.  Now we
			   * must process the buffer from the beginning
			   * and see if `bufpos - 3' really starts a character.
			   */
			  for (ti = w; ti < bufpos - 3; ti++)
			    {
			      if (buffer[ti] == '\\')
				{
				  if (ISOCTAL (buffer[ti + 1]))
				    {
				      unsigned int tti;

				      for (tti = 0;
					   tti < 3 && ISOCTAL (buffer[ti + 1]);
					   tti++, ti++)
					;
				    }
				  else
				    /* Simple escape. */
				    ti++;
				}
			    }

			  /*
			   * If <ti> is equal to <bufpos - 3>, we found
			   * an octal character, otherwise the leading
			   * backslash at <bufpos - 3> belongs to the
			   * previous character.
			   */
			  if (ti == bufpos - 3)
			    {
			      int tch;

			      tch = (((buffer[bufpos - 2] - '0') << 6)
				     + ((buffer[bufpos - 1] - '0') << 3)
				     + (buffer[bufpos] - '0'));
			      is_ungetc (tch, is);
			      UNEMIT (tch);
			      bufpos -= 3;
			    }
			  else
			    /* Normal character. */
			    goto unemit_normal;
			}
		      else
			{
			  /* Normal character, just unget it. */
			unemit_normal:
			  is_ungetc (buffer[bufpos], is);
			  UNEMIT (buffer[bufpos]);
			}
		    }
		  while (bufpos > w);
		}
	    }
	}

      if (ch == nl)
	{
	  if (line_end == LE_TRUNCATE)
	    {
	      if (do_print)
		num_truncated_lines++;
	      pending_token = tNEWLINE;
	    }
	  else
	    pending_token = tWRAPPED_NEWLINE;
	}
      else
	pending_token = tEOF;
    }

  APPEND_CHAR ('\0');
  token->type = tSTRING;
  token->u.str = (char *) buffer;
  token->new_x = linepos;
  token->new_col = col;
}


static void
dump_ps_page_header (char *fname, int empty)
{
  char buf[512];
  char *ftail;
  int got, i;
  char *cp, *cp2;
  char *cstr = "%%";
  unsigned int nup_subpage;

  /* The N-up printing sub-page. */
  nup_subpage = (total_pages - 1) % nup;

  /* Create fdir and ftail. */
  ftail = strrchr (fname, '/');

#if defined(WIN32)
  if (ftail == NULL)
    ftail = strrchr (fname, '\\');
#endif /* WIN32 */

  if (ftail == NULL)
    {
      buf[0] = '\0';
      ftail = fname;
    }
  else
    {
      ftail++;
      i = ftail - fname >= sizeof (buf)-1 ? sizeof (buf)-1 : ftail - fname;
      strncpy (buf, fname, i);
      buf[i] = '\0';
    }

  if (nup > 1)
    {
      /* N-up printing is active. */
      cstr = "%";

      if (nup_subpage == 0)
	{
	  /* This is a real page start. */

	  switch (page_label)
	    {
	    case LABEL_SHORT:
	      OUTPUT ((cofp, "%%%%Page: (%d-%d) %d\n", current_pagenum,
		       current_pagenum + nup - 1, total_pages / nup + 1));
	      break;

	    case LABEL_LONG:
	      OUTPUT ((cofp, "%%%%Page: (%s:%3d-%3d) %d\n", ftail,
		       current_pagenum, current_pagenum + nup - 1,
		       total_pages / nup + 1));
	      break;
	    }

	  /* Page setup. */
	  OUTPUT ((cofp, "%%%%BeginPageSetup\n_S\n"));

	  if ((total_pages / nup + 1) % 2 == 0)
	    /* Two-side binding options for the even pages. */
	    handle_two_side_options ();

#define PRINT_BOUNDING_BOXES 0

#if PRINT_BOUNDING_BOXES
	  OUTPUT ((cofp,
		   "%d %d moveto %d %d lineto %d %d lineto %d %d lineto closepath stroke\n",
		   media->llx, media->lly, media->llx, media->ury,
		   media->urx, media->ury, media->urx, media->lly));
#endif

	  if (landscape)
	    {
	      if (nup_landscape)
		OUTPUT ((cofp, "90 rotate\n%d %d translate\n",
			 media->lly, -media->urx));
	      else
		OUTPUT ((cofp, "%d %d translate\n", media->llx, media->lly));
	    }
	  else
	    {
	      if (nup_landscape)
		OUTPUT ((cofp, "90 rotate\n%d %d translate\n",
			 media->lly, -media->llx));
	      else
		OUTPUT ((cofp, "%d %d translate\n", media->llx, media->ury));
	    }
	}
    }

  /* Page start comment. */
  switch (page_label)
    {
    case LABEL_SHORT:
      OUTPUT ((cofp, "%sPage: (%d) %d\n", cstr, current_pagenum, total_pages));
      break;

    case LABEL_LONG:
      OUTPUT ((cofp, "%sPage: (%s:%3d) %d\n", cstr, ftail, current_pagenum,
	       total_pages));
      break;
    }

  /*
   * Page Setup.
   */

  OUTPUT ((cofp, "%sBeginPageSetup\n_S\n", cstr));

  if (nup > 1)
    {
      int xm, ym;

      OUTPUT ((cofp, "%% N-up sub-page %d/%d\n", nup_subpage + 1, nup));
      if (landscape)
	{
	  if (nup_columnwise)
	    {
	      xm = nup_subpage % nup_columns;
	      ym = nup_subpage / nup_columns;
	    }
	  else
	    {
	      xm = nup_subpage / nup_rows;
	      ym = nup_subpage % nup_rows;
	    }

	  OUTPUT ((cofp, "%d %d translate\n",
		   xm * (nup_width + nup_xpad),
		   ym * (nup_height + nup_ypad)));
	}
      else
	{
	  if (nup_columnwise)
	    {
	      xm = nup_subpage / nup_rows;
	      ym = nup_subpage % nup_rows;
	    }
	  else
	    {
	      xm = nup_subpage % nup_columns;
	      ym = nup_subpage / nup_columns;
	    }

	  OUTPUT ((cofp, "%d %d translate\n",
		   xm * (nup_width + nup_xpad),
		   -((int) (ym * (nup_height + nup_ypad) + nup_height))));
	}
      OUTPUT ((cofp, "%g dup scale\n", nup_scale));

      /* And finally, the real page setup. */
      if (landscape)
	OUTPUT ((cofp, "90 rotate\n%d %d translate\n", 0, -d_page_h));
    }
  else
    {
      /* No N-up printing. */

      if (total_pages % 2 == 0)
	/* Two-side binding options for the even pages. */
	handle_two_side_options ();

      if (landscape)
	OUTPUT ((cofp, "90 rotate\n%d %d translate\n",
		 media->lly, -media->urx));
      else
	OUTPUT ((cofp, "%d %d translate\n", media->llx, media->lly));
    }

  /* Some constants etc. */
  OUTPUT ((cofp, "/pagenum %d def\n", current_pagenum));

  cp = escape_string (fname);
  OUTPUT ((cofp, "/fname (%s) def\n", cp));
  xfree (cp);

  cp = escape_string (buf);
  OUTPUT ((cofp, "/fdir (%s) def\n", cp));
  xfree (cp);

  cp = escape_string (ftail);
  OUTPUT ((cofp, "/ftail (%s) def\n", cp));
  xfree (cp);

  /* Do we have a pending ^@font{} font? */
  if (user_fontp)
    {
      if (encoding == default_Fencoding)
	OUTPUT ((cofp, "/%s %g %g SUF\n", Fname, Fpt.w, Fpt.h));
      else
	/* This must be the case. */
	OUTPUT ((cofp, "/%s %g %g SUF_PS\n", Fname, Fpt.w, Fpt.h));
    }

  /* Dump user defined strings. */
  if (count_key_value_set (user_strings) > 0)
    {
      OUTPUT ((cofp, "%% User defined strings:\n"));
      for (got = strhash_get_first (user_strings, &cp, &i, (void **) &cp2);
	   got;
	   got = strhash_get_next (user_strings, &cp, &i, (void **) &cp2))
	{
	  cp2 = format_user_string ("%Format", cp2);
	  OUTPUT ((cofp, "/%s (%s) def\n", cp, cp2));
	  xfree (cp2);
	}
    }

  /* User supplied header? */
  if (page_header)
    {
      char *h_left;
      char *h_center;
      char *h_right = NULL;

      h_left = format_user_string ("page header", page_header);
      h_center = strchr (h_left, '|');
      if (h_center)
	{
	  *h_center = '\0';
	  h_center++;

	  h_right = strchr (h_center, '|');
	  if (h_right)
	    {
	      *h_right = '\0';
	      h_right++;
	    }
	}

      OUTPUT ((cofp, "/user_header_p true def\n"));
      OUTPUT ((cofp, "/user_header_left_str (%s) def\n", h_left));
      OUTPUT ((cofp, "/user_header_center_str (%s) def\n",
	       h_center ? h_center : ""));
      OUTPUT ((cofp, "/user_header_right_str (%s) def\n",
	       h_right ? h_right : ""));
      xfree (h_left);
    }
  else
    OUTPUT ((cofp, "/user_header_p false def\n"));

  /* User supplied footer? */
  if (page_footer)
    {
      char *f_left;
      char *f_center;
      char *f_right = NULL;

      f_left = format_user_string ("page footer", page_footer);
      f_center = strchr (f_left, '|');
      if (f_center)
	{
	  *f_center = '\0';
	  f_center++;

	  f_right = strchr (f_center, '|');
	  if (f_right)
	    {
	      *f_right = '\0';
	      f_right++;
	    }
	}

      OUTPUT ((cofp, "/user_footer_p true def\n"));
      OUTPUT ((cofp, "/user_footer_left_str (%s) def\n", f_left));
      OUTPUT ((cofp, "/user_footer_center_str (%s) def\n",
	       f_center ? f_center : ""));
      OUTPUT ((cofp, "/user_footer_right_str (%s) def\n",
	       f_right ? f_right : ""));
      xfree (f_left);
    }
  else
    OUTPUT ((cofp, "/user_footer_p false def\n"));

  OUTPUT ((cofp, "%%%%EndPageSetup\n"));

  /*
   * Mark standard page decorations.
   */

  if (!empty)
    {
      /* Highlight bars. */
      if (highlight_bars)
	OUTPUT ((cofp, "%d %f %d %f highlight_bars\n", highlight_bars,
		 LINESKIP, d_output_y_margin, highlight_bar_gray));

      /* Underlay. */
      if (underlay != NULL)
	{
	  if (ul_position_p || ul_angle_p)
	    OUTPUT ((cofp, "user_underlay\n"));
	  else
	    OUTPUT ((cofp, "underlay\n"));
	}

      /* Column lines. */
      if (num_columns > 1 && (header == HDR_FANCY || borders))
	OUTPUT ((cofp, "column_lines\n"));

      /* Borders around columns. */
      if (borders)
	OUTPUT ((cofp, "column_borders\n"));

      /* Header. */
      switch (header)
	{
	case HDR_NONE:
	  break;

	case HDR_SIMPLE:
	case HDR_FANCY:
	  OUTPUT ((cofp, "do_header\n"));
	  break;
	}
    }

  /* Do we have a pending ^@color{} color? */
  if (user_colorp)
    OUTPUT ((cofp, "%g %g %g setrgbcolor\n", user_color.r, user_color.g,
	     user_color.b));
}


static void
dump_ps_page_trailer ()
{
  unsigned int nup_subpage = (total_pages - 1) % nup;

  OUTPUT ((cofp, "_R\n"));

  if (nup > 1)
    {
      if (nup_subpage + 1 == nup)
	/* Real end of page. */
	OUTPUT ((cofp, "_R\nS\n"));
    }
  else
    OUTPUT ((cofp, "S\n"));
}


static void
dump_empty_page ()
{
  if (nup > 1)
    {
      unsigned int nup_subpage = (total_pages - 1) % nup;

      if (nup_subpage == 0)
	{
	  /* Real start of the page, must do it the harder way. */
	  dump_ps_page_header ("", 1);
	  OUTPUT ((cofp, "_R\n"));
	}
      else
	OUTPUT ((cofp, "%%Page: (-) %d\n", total_pages));

      if (nup_subpage + 1 == nup)
	/* This is the last page on this sheet, dump us. */
	OUTPUT ((cofp, "_R\nS\n"));
    }
  else
    OUTPUT ((cofp, "%%%%Page: (-) %d\nS\n", total_pages));
}


static int
recognize_eps_file (Token *token)
{
  int i;
  char buf[4096];
  int line;
  int valid_epsf;
  float llx, lly, urx, ury;

  MESSAGE (2, (stderr, "^@epsf=\"%s\"\n", token->u.epsf.filename));

  i = strlen (token->u.epsf.filename);
  /*
  if (i > 0 && token->u.epsf.filename[i - 1] == '|')
    {
      / * Read EPS data from pipe. * /
      token->u.epsf.pipe = 1;
      token->u.epsf.filename[i - 1] = '\0';
      token->u.epsf.fp = popen (token->u.epsf.filename, "r");
      if (token->u.epsf.fp == NULL)
	{
	  MESSAGE (0, (stderr,
		       _("epsf: couldn't open pipe to command \"%s\": %s\n"),
		       token->u.epsf.filename, strerror (errno)));
	  return 0;
	}
    }
  else
  */
    {
      char *filename;

      /* Read EPS data from file. */
      filename = tilde_subst (token->u.epsf.filename);

      token->u.epsf.fp = fopen (filename, "rb");
      xfree (filename);

      if (token->u.epsf.fp == NULL)
	{
	  if (token->u.epsf.filename[0] != '/')
	    {
	      /* Name is not absolute, let's lookup path. */
	      FileLookupCtx ctx;

	      ctx.name = token->u.epsf.filename;
	      ctx.suffix = "";
	      ctx.fullname = buffer_alloc ();

	      if (pathwalk (libpath, file_lookup, &ctx))
		token->u.epsf.fp = fopen (buffer_ptr (ctx.fullname), "rb");

	      buffer_free (ctx.fullname);
	    }
	  if (token->u.epsf.fp == NULL)
	    {
	      MESSAGE (0, (stderr, _("couldn't open EPS file \"%s\": %s\n"),
			   token->u.epsf.filename, strerror (errno)));
	      return 0;
	    }
	}
    }

  /* Find BoundingBox DSC comment. */

  line = 0;
  valid_epsf = 0;
  token->u.epsf.skipbuf = NULL;
  token->u.epsf.skipbuf_len = 0;
  token->u.epsf.skipbuf_pos = 0;

  while (fgets (buf, sizeof (buf), token->u.epsf.fp))
    {
      line++;

      /* Append data to the skip buffer. */
      i = strlen (buf);
      if (i + token->u.epsf.skipbuf_pos >= token->u.epsf.skipbuf_len)
	{
	  token->u.epsf.skipbuf_len += 8192;
	  token->u.epsf.skipbuf = xrealloc (token->u.epsf.skipbuf,
					    token->u.epsf.skipbuf_len);
	}
      memcpy (token->u.epsf.skipbuf + token->u.epsf.skipbuf_pos, buf, i);
      token->u.epsf.skipbuf_pos += i;

      /* Check the "%!" magic cookie. */
      if (line == 1)
	{
	  if (buf[0] != '%' || buf[1] != '!')
	    {
	      MESSAGE (0,
		       (stderr,
			_("EPS file \"%s\" does not start with \"%%!\" magic\n"),
			token->u.epsf.filename));
	      break;
	    }
	}

#define BB_DSC "%%BoundingBox:"

      if (strncmp (buf, BB_DSC, strlen (BB_DSC)) == 0)
	{
	  i = sscanf (buf + strlen (BB_DSC), "%f %f %f %f",
		      &llx, &lly, &urx, &ury);
	  if (i != 4)
	    {
	      /* (atend) ? */

	      /* Skip possible whitespace. */
	      for (i = strlen (BB_DSC);
		   buf[i] && (buf[i] == ' ' || buf[i] == '\t');
		   i++)
		;
#define BB_DSC_ATEND "(atend)"
	      if (strncmp (buf + i, BB_DSC_ATEND, strlen (BB_DSC_ATEND)) != 0)
		{
		  /* No, this BoundingBox comment is corrupted. */
		  MESSAGE (0, (stderr, _("EPS file \"%s\" contains malformed \
%%%%BoundingBox row:\n\"%.*s\"\n"),
			       token->u.epsf.filename, strlen (buf) - 1, buf));
		  break;
		}
	    }
	  else
	    {
	      /* It was a valid EPS file. */

	      /* We store bounding box in int format. */
	      token->u.epsf.llx = llx;
	      token->u.epsf.lly = lly;
	      token->u.epsf.urx = urx;
	      token->u.epsf.ury = ury;

	      valid_epsf = 1;
	      break;
	    }
	}
    }

  /* Check that we found the BoundingBox comment. */
  if (!valid_epsf)
    {
      MESSAGE (0, (stderr, _("EPS file \"%s\" is not a valid EPS file\n"),
		   token->u.epsf.filename));
      if (token->u.epsf.pipe)
	pclose (token->u.epsf.fp);
      else
	fclose (token->u.epsf.fp);
      xfree (token->u.epsf.skipbuf);
      return 0;
    }

  MESSAGE (2, (stderr, "BoundingBox: %d %d %d %d\n",
	       token->u.epsf.llx, token->u.epsf.lly,
	       token->u.epsf.urx, token->u.epsf.ury));

  return 1;
}


static void
paste_epsf (Token *token)
{
  char buf[4096];
  int i;

  /* EPSF import header. */
  OUTPUT ((cofp, "BeginEPSF\n"));
  OUTPUT ((cofp, "%g %g translate\n", token->new_x, token->new_y));
  OUTPUT ((cofp, "%g %g scale\n", token->u.epsf.xscale, token->u.epsf.yscale));
  OUTPUT ((cofp, "%d %d translate\n", -token->u.epsf.llx,
	   -token->u.epsf.lly));
  OUTPUT ((cofp, "%d %d %d %d Box clip newpath\n",
	   token->u.epsf.llx - 1,
	   token->u.epsf.lly - 1,
	   token->u.epsf.urx - token->u.epsf.llx + 2,
	   token->u.epsf.ury - token->u.epsf.lly + 2));
  OUTPUT ((cofp, "%%%%BeginDocument: %s%s\n", token->u.epsf.filename,
	   token->u.epsf.pipe ? "|" : ""));

  if (do_print)
    {
      /* Dump skip buffer. */
      fwrite (token->u.epsf.skipbuf, 1, token->u.epsf.skipbuf_pos, cofp);

      /* Dump file. */
      while ((i = fread (buf, 1, sizeof (buf), token->u.epsf.fp)) != 0)
	fwrite (buf, 1, i, cofp);
    }

  /* Add a newline to keep comments correct */
  OUTPUT ((cofp, "\n"));

  /* EPSF import trailer. */
  OUTPUT ((cofp, "%%%%EndDocument\nEndEPSF\n"));

  /* Cleanup. */
  if (token->u.epsf.pipe)
    pclose (token->u.epsf.fp);
  else
    fclose (token->u.epsf.fp);
  xfree (token->u.epsf.skipbuf);
}


static double
read_float (InputStream *is, int units, int horizontal)
{
  char buf[256];
  int i, ch;
  double val;

  for (i = 0; (i < sizeof (buf) - 1
	       && (ch = is_getc (is)) != EOF
	       && ISNUMBERDIGIT (ch));
       i++)
    buf[i] = ch;
  buf[i] = '\0';
  if (ch != EOF)
    is_ungetc (ch, is);

  val = atof (buf);

  if (units)
    {
      /* Get unit. */
      ch = is_getc (is);
      switch (ch)
	{
	case 'c':		/* centimeters */
	  val *= 72 / 2.54;
	  break;

	case 'p':		/* PostScript points */
	  break;

	case 'i':		/* inches */
	  val *= 72;
	  break;

	default:
	  is_ungetc (ch, is);
	  /* FALLTHROUGH */

	case 'l':		/* lines or characters */
	  if (horizontal)
	    val *= CHAR_WIDTH ('m');
	  else
	    val *= LINESKIP;
	  break;
	}
    }

  return val;
}


/* Magics used to recognize different pass-through files. */
static struct
{
  char *magic;
  unsigned int magiclen;
  char *name;
  int revert_delta;
} pass_through_magics[] =
  {
    {"%!", 	2, "PostScript", 	-2},
    {"\004%!",	3, "PostScript", 	-2},
    {"\033E",	2, "PCL",		-2},
    {"\033%",	2, "PCL",		-2},
    {NULL, 0, NULL, 0},
  };


static int
do_pass_through (char *fname, InputStream *is)
{
  int ch;
  unsigned long saved_pos = is->bufpos;
  int i, j;

  if (output_language_pass_through)
    MESSAGE (1,
	     (stderr,
	      _("passing through all input files for output language `%s'\n"),
	      output_language));
  else
    {
      /*
       * Try to recognize pass-through files.
       */

      for (i = 0; pass_through_magics[i].magic; i++)
	{
	  for (j = 0; j < pass_through_magics[i].magiclen; j++)
	    {
	      ch = is_getc (is);
	      if (ch == EOF
		  || ch != (unsigned char) pass_through_magics[i].magic[j])
		break;
	    }

	  if (j >= pass_through_magics[i].magiclen)
	    /* The <i>th one matched. */
	    break;

	  /*
	   * Try the next one, but first, seek the input stream to its
	   * start.
	   */
	  is->bufpos = saved_pos;
	}

      /* Did we find any? */
      if (pass_through_magics[i].magic == NULL)
	/* No we didn't. */
	return 0;

      /* Yes, it really is a pass-through file.  Now do the pass through. */

      is->bufpos += pass_through_magics[i].revert_delta;

      if (ps_header_dumped)
	{
	  /* A pass-through file between normal ASCII files, obey DSC. */

	  /*
	   * XXX I don't know how to handle PCL files... Let's hope none
	   * mixes them with the normal ASCII files.
	   */

	  OUTPUT ((cofp,
		   "%%%%Page: (%s) -1\n_S\n%%%%BeginDocument: %s\n",
		   fname, fname));
	}

      MESSAGE (1, (stderr, _("passing through %s file \"%s\"\n"),
		   pass_through_magics[i].name, fname));
    }

  /* And now, do the actual pass-through. */
  do
    {
      /* Note: this will be written directly to the <ofp>. */
      fwrite (is->buf + is->bufpos, 1, is->data_in_buf - is->bufpos, ofp);
      is->bufpos = is->data_in_buf;

      /* Read more data to the input buffer. */
      ch = is_getc (is);
      is->bufpos = 0;
    }
  while (ch != EOF);

  if (!output_language_pass_through)
    {
      if (ps_header_dumped)
	/*
	 * XXX How to end a PCL file mixed between ASCII files?
	 */
	OUTPUT ((cofp, "%%%%EndDocument\n_R\n"));
    }

  return 1;
}


static void
print_line_number (double x, double y, double space, double margin,
		   unsigned int linenum)
{
  double len = 0.0;
  char buf[20];
  int i;
  char *saved_Fname = "";
  FontPoint saved_Fpt;
  InputEncoding saved_Fencoding;

  saved_Fpt.w = 0.0;
  saved_Fpt.h = 0.0;

  /* Do not print linenumbers for wrapped lines. */
  if (linenum == print_line_number_last)
    return;
  print_line_number_last = linenum;

  if (user_fontp)
    {
      /* Re-select our default typing font. */
      saved_Fname = Fname;
      saved_Fpt.w = Fpt.w;
      saved_Fpt.h = Fpt.h;
      saved_Fencoding = encoding;

      Fname = default_Fname;
      Fpt.w = default_Fpt.w;
      Fpt.h = default_Fpt.h;
      encoding = default_Fencoding;

      OUTPUT ((cofp, "/F-gs-font %g %g SF\n", Fpt.w, Fpt.h));
      read_font_info ();
    }

  /* Count linenumber string length. */
  sprintf (buf, "%d", linenum);
  for (i = 0; buf[i]; i++)
    len += CHAR_WIDTH (buf[i]);

  /* Print line numbers. */
  OUTPUT ((cofp, "%g %g M (%s:) s\n", x + space - len, y, buf));

  if (user_fontp)
    {
      /* Switch back to the user font. */
      Fname = saved_Fname;
      Fpt.w = saved_Fpt.w;
      Fpt.h = saved_Fpt.h;
      encoding = saved_Fencoding;

      OUTPUT ((cofp, "/%s %g %g SUF\n", Fname, Fpt.w, Fpt.h));
      read_font_info ();
    }
}


/*
 * The name of the divert file, shared between divert() and undivert()
 * functions.
 */
static char divertfname[512];

static void
divert ()
{
  assert (divertfp == NULL);

  /* Open divert file. */

  divertfp = tmpfile ();
  if (divertfp == NULL)
    FATAL ((stderr, _("couldn't create temporary divert file: %s"),
	    strerror (errno)));

  cofp = divertfp;
}


static void
undivert ()
{
  char buf[1024];
  int doc_level = 0;
  char *cp;

  assert (divertfp != NULL);

  if (fseek (divertfp, 0, SEEK_SET) != 0)
    FATAL ((stderr, _("couldn't rewind divert file: %s"), strerror (errno)));

  while (fgets (buf, sizeof (buf), divertfp))
    {
      if (strncmp (buf, "%%BeginDocument", 15) == 0)
	doc_level++;
      else if (strncmp (buf, "%%EndDocument", 13) == 0)
	doc_level--;

      if (doc_level == 0)
	{
	  if (strncmp (buf, "% User defined strings", 22) == 0)
	    {
	      fputs (buf, ofp);
	      while (fgets (buf, sizeof (buf), divertfp))
		{
		  if (strncmp (buf, "%%EndPageSetup", 14) == 0)
		    break;

		  /* Patch total pages to the user defined strings. */
		  cp = strchr (buf, '\001');
		  if (cp)
		    {
		      *cp = '\0';
		      fputs (buf, ofp);
		      fprintf (ofp, "%d", total_pages_in_file);
		      fputs (cp + 1, ofp);
		    }
		  else
		    fputs (buf, ofp);
		}
	    }
	}

      fputs (buf, ofp);
    }

  fclose (divertfp);
  divertfp = NULL;

  cofp = ofp;
}


static void
handle_two_side_options ()
{
  if (rotate_even_pages)
    /* Rotate page 180 degrees. */
    OUTPUT ((cofp, "180 rotate\n%d %d translate\n",
	     -media->w, -media->h));

  if (swap_even_page_margins)
    OUTPUT ((cofp, "%d 0 translate\n",
	     -(media->llx - (media->w - media->urx))));
}
