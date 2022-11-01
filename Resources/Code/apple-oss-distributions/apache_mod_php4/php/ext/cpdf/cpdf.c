/*
   +----------------------------------------------------------------------+
   | PHP Version 4                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2008 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Author: Uwe Steinmann <Uwe.Steinmann@fernuni-hagen.de>               |
   +----------------------------------------------------------------------+
 */

/* $Id: cpdf.c,v 1.43.4.7.2.3 2007/12/31 07:22:45 sebastian Exp $ */
/* cpdflib.h -- C language API definitions for ClibPDF library
 * Copyright (C) 1998 FastIO Systems, All Rights Reserved.
*/

/* Note that there is no code from the cpdflib package in this file */

#if defined(THREAD_SAFE)
#undef THREAD_SAFE
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_globals.h"
#include "zend_list.h"
#include "ext/standard/php_standard.h"
#include "ext/standard/head.h"
#include <math.h>
#if HAVE_LIBGD13
#include <gd.h>
#endif

#include <cpdflib.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef PHP_WIN32
# include <io.h>
# include <fcntl.h>
#endif

#if HAVE_CPDFLIB
#include "php_cpdf.h"
#include "ext/standard/info.h"

#ifdef THREAD_SAFE
DWORD CPDFlibTls;
static int numthreads=0;

typedef struct cpdflib_global_struct{
	int le_cpdf;
	int le_outline;
#if HAVE_LIBGD13
    int le_gd;
#endif
} cpdflib_global_struct;

# define CPDF_GLOBAL(v) TSRMG(CPDFlibTls, cpdflib_global_struct *, v)

#else
# define CPDF_GLOBAL(a) a
static int le_cpdf;
static int le_outline;
#if HAVE_LIBGD13
static int le_gd;
#endif
#endif

function_entry cpdf_functions[] = {
  PHP_FE(cpdf_global_set_document_limits, NULL)
	PHP_FE(cpdf_set_creator, NULL)
	PHP_FE(cpdf_set_title, NULL)
	PHP_FE(cpdf_set_subject, NULL)
	PHP_FE(cpdf_set_keywords, NULL)
	PHP_FE(cpdf_open, NULL)
	PHP_FE(cpdf_close, NULL)
	PHP_FE(cpdf_set_viewer_preferences, NULL)
	PHP_FE(cpdf_page_init, NULL)
	PHP_FE(cpdf_finalize_page, NULL)
	PHP_FE(cpdf_set_current_page, NULL)
	PHP_FE(cpdf_begin_text, NULL)
	PHP_FE(cpdf_end_text, NULL)
	PHP_FE(cpdf_show, NULL)
	PHP_FE(cpdf_show_xy, NULL)
	PHP_FE(cpdf_text, NULL)
	PHP_FE(cpdf_continue_text, NULL)
	PHP_FE(cpdf_set_font, NULL)
	PHP_FE(cpdf_set_font_directories, NULL)
	PHP_FE(cpdf_set_font_map_file, NULL)
	PHP_FE(cpdf_set_leading, NULL)
	PHP_FE(cpdf_set_text_rendering, NULL)
	PHP_FE(cpdf_set_horiz_scaling, NULL)
	PHP_FE(cpdf_set_text_rise, NULL)
	PHP_FE(cpdf_set_text_matrix, NULL)
	PHP_FE(cpdf_set_text_pos, NULL)
	PHP_FE(cpdf_rotate_text, NULL)
	PHP_FE(cpdf_set_char_spacing, NULL)
	PHP_FE(cpdf_set_word_spacing, NULL)
	PHP_FE(cpdf_stringwidth, NULL)
	PHP_FE(cpdf_save, NULL)
	PHP_FE(cpdf_restore, NULL)
	PHP_FE(cpdf_translate, NULL)
	PHP_FE(cpdf_scale, NULL)
	PHP_FE(cpdf_rotate, NULL)
	PHP_FE(cpdf_setflat, NULL)
	PHP_FE(cpdf_setlinejoin, NULL)
	PHP_FE(cpdf_setlinecap, NULL)
	PHP_FE(cpdf_setmiterlimit, NULL)
	PHP_FE(cpdf_setlinewidth, NULL)
	PHP_FE(cpdf_setdash, NULL)
	PHP_FE(cpdf_moveto, NULL)
	PHP_FE(cpdf_rmoveto, NULL)
	PHP_FE(cpdf_lineto, NULL)
	PHP_FE(cpdf_rlineto, NULL)
	PHP_FE(cpdf_curveto, NULL)
	PHP_FE(cpdf_circle, NULL)
	PHP_FE(cpdf_arc, NULL)
	PHP_FE(cpdf_rect, NULL)
	PHP_FE(cpdf_newpath, NULL)
	PHP_FE(cpdf_closepath, NULL)
	PHP_FE(cpdf_stroke, NULL)
	PHP_FE(cpdf_closepath_stroke, NULL)
	PHP_FE(cpdf_fill, NULL)
	PHP_FE(cpdf_fill_stroke, NULL)
	PHP_FE(cpdf_closepath_fill_stroke, NULL)
	PHP_FE(cpdf_clip, NULL)
	PHP_FE(cpdf_setgray_fill, NULL)
	PHP_FE(cpdf_setgray_stroke, NULL)
	PHP_FE(cpdf_setgray, NULL)
	PHP_FE(cpdf_setrgbcolor_fill, NULL)
	PHP_FE(cpdf_setrgbcolor_stroke, NULL)
	PHP_FE(cpdf_setrgbcolor, NULL)
	PHP_FE(cpdf_set_page_animation, NULL)
	PHP_FE(cpdf_finalize, NULL)
	PHP_FE(cpdf_output_buffer, NULL)
	PHP_FE(cpdf_save_to_file, NULL)
	PHP_FE(cpdf_import_jpeg, NULL)
#if HAVE_LIBGD13
	PHP_FE(cpdf_place_inline_image, NULL)
#endif
	PHP_FE(cpdf_add_annotation, NULL)
	PHP_FE(cpdf_add_outline, NULL)
	PHP_FE(cpdf_set_action_url, NULL)
	{NULL, NULL, NULL}
};

zend_module_entry cpdf_module_entry = {
    STANDARD_MODULE_HEADER,
	"cpdf",
    cpdf_functions,
    PHP_MINIT(cpdf),
    PHP_MSHUTDOWN(cpdf),
    PHP_RINIT(cpdf),
    NULL,
    PHP_MINFO(cpdf),
    NO_VERSION_YET,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_CPDF
ZEND_GET_MODULE(cpdf)
#endif

static void _free_outline(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
}

static void _free_doc(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
	CPDFdoc *pdf = (CPDFdoc *)rsrc->ptr;

	cpdf_close(pdf);
}

PHP_MINIT_FUNCTION(cpdf)
{
	CPDF_GLOBAL(le_outline) = zend_register_list_destructors_ex(_free_outline, NULL, "cpdf outline", module_number);
	CPDF_GLOBAL(le_cpdf) = zend_register_list_destructors_ex(_free_doc, NULL, "cpdf", module_number);

	REGISTER_LONG_CONSTANT("CPDF_PM_NONE", PM_NONE, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("CPDF_PM_OUTLINES", PM_OUTLINES, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("CPDF_PM_THUMBS", PM_THUMBS, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("CPDF_PM_FULLSCREEN", PM_FULLSCREEN, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("CPDF_PL_SINGLE", PL_SINGLE, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("CPDF_PL_1COLUMN", PL_1COLUMN, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("CPDF_PL_2LCOLUMN", PL_2LCOLUMN, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("CPDF_PL_2RCOLUMN", PL_2RCOLUMN, CONST_CS | CONST_PERSISTENT);


	return SUCCESS;
}

PHP_RINIT_FUNCTION(cpdf)
{
/*	CPDF_GLOBAL(le_outline) = NULL; */
	return SUCCESS;
}

PHP_MINFO_FUNCTION(cpdf) {
	/* need to use a PHPAPI function here because it is external module in windows */
	/* (don't knwo if that still applies (cmv) */
	php_info_print_table_start();
	php_info_print_table_row(2, "CPDF Support", "enabled");
	php_info_print_table_row(2, "Version", cpdf_version() );
	php_info_print_table_end();
}

PHP_MSHUTDOWN_FUNCTION(cpdf)
{
	return SUCCESS;
}

/* {{{ proto bool cpdf_global_set_document_limits(int maxPages, int maxFonts, int maxImages, int maxAnnots, int maxObjects)
   Sets document settings for all documents */
PHP_FUNCTION(cpdf_global_set_document_limits)
{
	pval *argv[5];
	int argc;

	argc = ZEND_NUM_ARGS();
	if(argc != 5)
		WRONG_PARAM_COUNT;
	if (getParametersArray(ht, argc, argv) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long(argv[0]);
	convert_to_long(argv[1]);
	convert_to_long(argv[2]);
	convert_to_long(argv[3]);
	convert_to_long(argv[4]);

	cpdf_setGlobalDocumentLimits(Z_LVAL_P(argv[0]), Z_LVAL_P(argv[1]), Z_LVAL_P(argv[2]), Z_LVAL_P(argv[3]), Z_LVAL_P(argv[4]));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_set_creator(int pdfdoc, string creator)
   Sets the creator field */
PHP_FUNCTION(cpdf_set_creator)
{
	pval *arg1, *arg2;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 2 || getParameters(ht, 2, &arg1, &arg2) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_string(arg2);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if (!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d (type=%d)", get_active_function_name(TSRMLS_C), id, type);
		RETURN_FALSE;
	}

	cpdf_setCreator(pdf, Z_STRVAL_P(arg2));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_set_title(int pdfptr, string title)
   Fills the title field of the info structure */
PHP_FUNCTION(cpdf_set_title)
{
	pval *arg1, *arg2;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 2 || getParameters(ht, 2, &arg1, &arg2) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_string(arg2);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if (!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d (type=%d)", get_active_function_name(TSRMLS_C), id, type);
		RETURN_FALSE;
	}

	cpdf_setTitle(pdf, Z_STRVAL_P(arg2));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_set_subject(int pdfptr, string subject)
   Fills the subject field of the info structure */
PHP_FUNCTION(cpdf_set_subject)
{
	pval *arg1, *arg2;
	int id, type;
	CPDFdoc *pdf;


	if (ZEND_NUM_ARGS() != 2 || getParameters(ht, 2, &arg1, &arg2) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_string(arg2);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if (!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d (type=%d)", get_active_function_name(TSRMLS_C), id, type);
		RETURN_FALSE;
	}

	cpdf_setSubject(pdf, Z_STRVAL_P(arg2));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_set_keywords(int pdfptr, string keywords)
   Fills the keywords field of the info structure */
PHP_FUNCTION(cpdf_set_keywords)
{
	pval *arg1, *arg2;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 2 || getParameters(ht, 2, &arg1, &arg2) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_string(arg2);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if (!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d (type=%d)", get_active_function_name(TSRMLS_C), id, type);
		RETURN_FALSE;
	}

	cpdf_setKeywords(pdf, Z_STRVAL_P(arg2));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_set_viewer_preferences(int pdfdoc, array preferences)
   How to show the document in the viewer */
PHP_FUNCTION(cpdf_set_viewer_preferences)
{
	zval *arg1, *arg2;
	zval **zvalue;
	int id, type;
	CPDFdoc *pdf;
	CPDFviewerPrefs vP = { 0, 0, 0, 0, 0, 0, 0, 0 };

	if(ZEND_NUM_ARGS() != 2)
		WRONG_PARAM_COUNT;

	if (getParameters(ht, 2, &arg1, &arg2) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long(arg1);
	convert_to_array(arg2);

	id = Z_LVAL_P (arg1);

	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	if (zend_hash_find (Z_ARRVAL_P(arg2), "pagemode", sizeof ("pagemode"), (void **) &zvalue) == SUCCESS)
	{
		convert_to_long_ex (zvalue);
		vP.pageMode = Z_LVAL_PP (zvalue);
	}
	if (zend_hash_find (Z_ARRVAL_P(arg2), "hidetoolbar", sizeof ("hidetoolbar"), (void **) &zvalue) == SUCCESS)
	{
		convert_to_long_ex (zvalue);
		vP.hideToolbar = Z_LVAL_PP (zvalue);
	}
	if (zend_hash_find (Z_ARRVAL_P(arg2), "hidemenubar", sizeof ("hidemenubar"), (void **) &zvalue) == SUCCESS)
	{
		convert_to_long_ex (zvalue);
		vP.hideMenubar = Z_LVAL_PP (zvalue);
	}
	if (zend_hash_find (Z_ARRVAL_P(arg2), "hidewindowui", sizeof ("hidewindowui"), (void **) &zvalue) == SUCCESS)
	{
		convert_to_long_ex (zvalue);
		vP.hideWindowUI = Z_LVAL_PP (zvalue);
	}
	if (zend_hash_find (Z_ARRVAL_P(arg2), "fitwindow", sizeof ("fitwindow"), (void **) &zvalue) == SUCCESS)
	{
		convert_to_long_ex (zvalue);
		vP.fitWindow = Z_LVAL_PP (zvalue);
	}
	if (zend_hash_find (Z_ARRVAL_P(arg2), "centerwindow", sizeof ("centerwindow"), (void **) &zvalue) == SUCCESS)
	{
		convert_to_long_ex (zvalue);
		vP.centerWindow = Z_LVAL_PP (zvalue);
	}
	if (zend_hash_find (Z_ARRVAL_P(arg2), "pagelayout", sizeof ("pagelayout"), (void **) &zvalue) == SUCCESS)
	{
		convert_to_long_ex (zvalue);
		vP.pageLayout = Z_LVAL_PP (zvalue);
	}
	if (zend_hash_find (Z_ARRVAL_P(arg2), "nonfspagemode", sizeof ("nonfspagemode"), (void **) &zvalue) == SUCCESS)
	{
		convert_to_long_ex (zvalue);
		vP.nonFSPageMode = Z_LVAL_PP (zvalue);
	}

	cpdf_setViewerPreferences(pdf, &vP);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto int cpdf_open(int compression [, string filename [, array doc_limits]])
   Opens a new pdf document */
PHP_FUNCTION(cpdf_open)
{
	pval *arg1, *arg2, *arg3;
	int id, argc;
	CPDFdoc *cpdf;

	argc = ZEND_NUM_ARGS();
	switch(argc) {
		case 1:
			if (getParameters(ht, 1, &arg1) == FAILURE) {
				WRONG_PARAM_COUNT;
			}
			break;
		case 2:
			if (getParameters(ht, 2, &arg1, &arg2) == FAILURE) {
				WRONG_PARAM_COUNT;
			}
			break;
		case 3:
			if (getParameters(ht, 3, &arg1, &arg2, &arg3) == FAILURE) {
				WRONG_PARAM_COUNT;
			}
			break;
		default:
			WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);

	cpdf = cpdf_open(0, NULL);
	if(!cpdf)
		RETURN_FALSE;
	if(Z_LVAL_P(arg1) == 1)
		cpdf_enableCompression(cpdf, YES);
	else
		cpdf_enableCompression(cpdf, NO);

	if(argc > 1) {
		convert_to_string(arg2);
#if APACHE
		if(strcmp(Z_STRVAL_P(arg2), "-") == 0)
			php_error(E_WARNING, "%s(): Writing to stdout as described in the ClibPDF manual is not possible if php is used as an Apache module. Write to a memory stream and use cpdf_output_buffer() instead.", get_active_function_name(TSRMLS_C));
#endif
		if (php_check_open_basedir(Z_STRVAL_P(arg2) TSRMLS_CC) || (PG(safe_mode) && !php_checkuid(Z_STRVAL_P(arg2), "rb+", CHECKUID_CHECK_MODE_PARAM))) {
			cpdf_close(cpdf);
			RETURN_FALSE;
		}

		cpdf_setOutputFilename(cpdf, Z_STRVAL_P(arg2));
	}
	cpdf_init(cpdf);

	id = zend_list_insert(cpdf, CPDF_GLOBAL(le_cpdf));
	RETURN_LONG(id);
}
/* }}} */

/* {{{ proto void cpdf_close(int pdfdoc)
   Closes the pdf document */
PHP_FUNCTION(cpdf_close)
{
	pval *arg1;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 1 || getParameters(ht, 1, &arg1) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}
	zend_list_delete(id);

	RETURN_TRUE;
}
/* }}} */

#define BUFFERLEN 40
/* {{{ proto bool cpdf_page_init(int pdfdoc, int pagenr, int orientation, int height, int width [, float unit])
   Starts page */
PHP_FUNCTION(cpdf_page_init)
{
	pval *argv[6];
	int id, type, pagenr, orientation;
	int height, width, argc;
	char buffer[BUFFERLEN];
	CPDFdoc *pdf;

	argc = ZEND_NUM_ARGS();
	if(argc < 5 || argc > 6)
		WRONG_PARAM_COUNT;
	if (getParametersArray(ht, argc, argv) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long(argv[0]);
	convert_to_long(argv[1]);
	convert_to_long(argv[2]);
	convert_to_long(argv[3]);
	convert_to_long(argv[4]);
	id=Z_LVAL_P(argv[0]);
	pagenr=Z_LVAL_P(argv[1]);
	orientation=Z_LVAL_P(argv[2]);
	height = Z_LVAL_P(argv[3]);
	width = Z_LVAL_P(argv[4]);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	if(argc > 5) {
		convert_to_double(argv[5]);
		if(Z_DVAL_P(argv[5]) > 0.0)
			cpdf_setDefaultDomainUnit(pdf, Z_DVAL_P(argv[5]));
	}
	snprintf(buffer, BUFFERLEN, "0 0 %d %d", width, height);
	cpdf_pageInit(pdf, pagenr, orientation, buffer, buffer);

	RETURN_TRUE;
}
/* }}} */
#undef BUFFERLEN

/* {{{ proto bool cpdf_finalize_page(int pdfdoc, int pagenr)
   Ends the page to save memory */
PHP_FUNCTION(cpdf_finalize_page)
{
	pval *arg1, *arg2;
	int id, type, pagenr;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 2 || getParameters(ht, 2, &arg1, &arg2) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_long(arg2);
	id=Z_LVAL_P(arg1);
	pagenr=Z_LVAL_P(arg2);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	cpdf_finalizePage(pdf, pagenr);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_set_current_page(int pdfdoc, int pagenr)
   Sets page for output */
PHP_FUNCTION(cpdf_set_current_page)
{
	pval *arg1, *arg2;
	int id, type, pagenr;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 2 || getParameters(ht, 2, &arg1, &arg2) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_long(arg2);
	id=Z_LVAL_P(arg1);
	pagenr=Z_LVAL_P(arg2);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	cpdf_setCurrentPage(pdf, pagenr);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_begin_text(int pdfdoc)
   Starts text section */
PHP_FUNCTION(cpdf_begin_text)
{
	pval *arg1;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 1 || getParameters(ht, 1, &arg1) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	cpdf_beginText(pdf, 0);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_end_text(int pdfdoc)
   Ends text section */
PHP_FUNCTION(cpdf_end_text)
{
	pval *arg1;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 1 || getParameters(ht, 1, &arg1) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	cpdf_endText(pdf);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_show(int pdfdoc, string text)
   Output text at current position */
PHP_FUNCTION(cpdf_show)
{
	pval *arg1, *arg2;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 2 || getParameters(ht, 2, &arg1, &arg2) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_string(arg2);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	cpdf_textShow(pdf, Z_STRVAL_P(arg2));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_show_xy(int pdfdoc, string text, float x-koor, float y-koor [, int mode])
   Output text at position */
PHP_FUNCTION(cpdf_show_xy)
{
	pval *argv[5];
	int id, type, argc, mode=0;
	CPDFdoc *pdf;

	argc = ZEND_NUM_ARGS();
	if((argc < 4) || (argc > 5))
		WRONG_PARAM_COUNT;
	if (getParametersArray(ht, argc, argv) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long(argv[0]);
	convert_to_string(argv[1]);
	convert_to_double(argv[2]);
	convert_to_double(argv[3]);
	id=Z_LVAL_P(argv[0]);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	if(argc == 5) {
		convert_to_long(argv[4]);
		mode = Z_LVAL_P(argv[4]);
	}
	if(mode == 1)
		cpdf_rawText(pdf, (float) Z_DVAL_P(argv[2]), (float) Z_DVAL_P(argv[3]), 0.0, Z_STRVAL_P(argv[1]));
	else
		cpdf_text(pdf, (float) Z_DVAL_P(argv[2]), (float) Z_DVAL_P(argv[3]), 0.0, Z_STRVAL_P(argv[1]));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_continue_text(int pdfdoc, string text)
   Outputs text in next line */
PHP_FUNCTION(cpdf_continue_text)
{
	pval *arg1, *arg2;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 2 || getParameters(ht, 2, &arg1, &arg2) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_string(arg2);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	cpdf_textCRLFshow(pdf, Z_STRVAL_P(arg2));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_text(int pdfdoc, string text [, float x-koor, float y-koor [, int mode [, float orientation [, int alignmode]]]])
   Outputs text */
PHP_FUNCTION(cpdf_text)
{
	pval *argv[7];
	int id, type, argc, mode=0;
	CPDFdoc *pdf;

	argc = ZEND_NUM_ARGS();
	if((argc < 2) || (argc == 3) || (argc > 7) || getParametersArray(ht, argc, argv) == FAILURE)
			WRONG_PARAM_COUNT;

	convert_to_long(argv[0]);
	convert_to_string(argv[1]);
	id=Z_LVAL_P(argv[0]);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	if(argc > 4) {
		convert_to_long(argv[4]);
		mode = Z_LVAL_P(argv[2]);
	}
	switch(argc) {
		case 2:
			cpdf_textShow(pdf, Z_STRVAL_P(argv[1]));
			break;
		case 4:
			convert_to_double(argv[2]);
			convert_to_double(argv[3]);
			cpdf_text(pdf, (float) Z_DVAL_P(argv[2]),
			          (float) Z_DVAL_P(argv[3]),
			           0.0,
			           Z_STRVAL_P(argv[1]));
			break;
		case 5:
			convert_to_double(argv[2]);
			convert_to_double(argv[3]);
			if(mode == 1)
				cpdf_rawText(pdf, (float) Z_DVAL_P(argv[2]),
			             	(float) Z_DVAL_P(argv[3]),
			             	0.0,
			              	Z_STRVAL_P(argv[1]));
			else
				cpdf_text(pdf, (float) Z_DVAL_P(argv[2]),
				          (float) Z_DVAL_P(argv[3]),
				          0.0,
				          Z_STRVAL_P(argv[1]));
			break;
		case 6:
			convert_to_double(argv[2]);
			convert_to_double(argv[3]);
			convert_to_double(argv[5]);
			if(mode == 1)
				cpdf_rawText(pdf, (float) Z_DVAL_P(argv[2]),
			             	(float) Z_DVAL_P(argv[3]),
			             	(float) Z_DVAL_P(argv[5]),
			              	Z_STRVAL_P(argv[1]));
			else
				cpdf_text(pdf, (float) Z_DVAL_P(argv[2]),
			             	(float) Z_DVAL_P(argv[3]),
			             	(float) Z_DVAL_P(argv[5]),
			              	Z_STRVAL_P(argv[1]));
			break;
		case 7:
			convert_to_double(argv[2]);
			convert_to_double(argv[3]);
			convert_to_double(argv[5]);
			convert_to_long(argv[6]);
			if(mode == 1)
				cpdf_rawTextAligned(pdf, (float) Z_DVAL_P(argv[2]),
			             	(float) Z_DVAL_P(argv[3]),
			             	(float) Z_DVAL_P(argv[5]),
			             	Z_LVAL_P(argv[6]),
			              	Z_STRVAL_P(argv[1]));
			else
				cpdf_textAligned(pdf, (float) Z_DVAL_P(argv[2]),
			             	(float) Z_DVAL_P(argv[3]),
			             	(float) Z_DVAL_P(argv[5]),
			             	Z_LVAL_P(argv[6]),
			              	Z_STRVAL_P(argv[1]));
			break;
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_set_font(int pdfdoc, string font, float size, string encoding)
   Selects the current font face, size and encoding */
PHP_FUNCTION(cpdf_set_font)
{
	pval *arg1, *arg2, *arg3, *arg4;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 4 || getParameters(ht, 4, &arg1, &arg2, &arg3, &arg4) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_string(arg2);
	convert_to_double(arg3);
	convert_to_string(arg4);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}
	
/*	if(Z_LVAL_P(arg4) > 6) {
		php_error(E_WARNING, "%s(): Font encoding set to 5", get_active_function_name(TSRMLS_C));
		Z_LVAL_P(arg4) = 5;
	}
*/
	cpdf_setFont(pdf, Z_STRVAL_P(arg2), Z_STRVAL_P(arg4), (float) Z_DVAL_P(arg3));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_set_font_directories(int pdfdoc, string pfmdir, string pfbdir)
   Sets directories to search when using external fonts */
PHP_FUNCTION(cpdf_set_font_directories)
{
	pval *arg1, *arg2, *arg3;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 3 || getParameters(ht, 3, &arg1, &arg2, &arg3) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_string(arg2);
	convert_to_string(arg3);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	cpdf_setFontDirectories(pdf, Z_STRVAL_P(arg2), Z_STRVAL_P(arg3));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_set_font_map_file(int pdfdoc, string filename)
   Sets fontname to filename translation map when using external fonts */
PHP_FUNCTION(cpdf_set_font_map_file)
{
	pval *arg1, *arg2;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 2 || getParameters(ht, 2, &arg1, &arg2) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_string(arg2);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	if (php_check_open_basedir(Z_STRVAL_P(arg2) TSRMLS_CC) || (PG(safe_mode) && !php_checkuid(Z_STRVAL_P(arg2), "rb+", CHECKUID_CHECK_MODE_PARAM))) {
		RETURN_FALSE;
	}

	cpdf_setFontMapFile(pdf, Z_STRVAL_P(arg2));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_set_leading(int pdfdoc, float distance)
   Sets distance between text lines */
PHP_FUNCTION(cpdf_set_leading)
{
	pval *arg1, *arg2;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 2 || getParameters(ht, 2, &arg1, &arg2) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_double(arg2);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}
	
	cpdf_setTextLeading(pdf, (float) Z_DVAL_P(arg2));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_set_text_rendering(int pdfdoc, int rendermode)
   Determines how text is rendered */
PHP_FUNCTION(cpdf_set_text_rendering)
{
	pval *arg1, *arg2;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 2 || getParameters(ht, 2, &arg1, &arg2) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_long(arg2);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}
	
	cpdf_setTextRenderingMode(pdf, Z_LVAL_P(arg2));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_set_horiz_scaling(int pdfdoc, float scale)
   Sets horizontal scaling of text */
PHP_FUNCTION(cpdf_set_horiz_scaling)
{
	pval *arg1, *arg2;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 2 || getParameters(ht, 2, &arg1, &arg2) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_double(arg2);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}
	
	cpdf_setHorizontalScaling(pdf, (float) Z_DVAL_P(arg2) * 100.0);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_set_text_rise(int pdfdoc, float value)
   Sets the text rise */
PHP_FUNCTION(cpdf_set_text_rise)
{
	pval *arg1, *arg2;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 2 || getParameters(ht, 2, &arg1, &arg2) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_double(arg2);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}
	
	cpdf_setTextRise(pdf, (float) Z_DVAL_P(arg2));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_set_text_matrix(int pdfdoc, arry matrix)
   Sets the text matrix */
PHP_FUNCTION(cpdf_set_text_matrix)
{
	pval *arg1, *arg2, *data;
	int id, type, i;
	HashTable *matrix;
	CPDFdoc *pdf;
	float *pdfmatrixptr;
	float pdfmatrix[6];

	if (ZEND_NUM_ARGS() != 2 || getParameters(ht, 2, &arg1, &arg2) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_array(arg2);
	id=Z_LVAL_P(arg1);
	matrix=Z_ARRVAL_P(arg2);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}
	
	if(zend_hash_num_elements(matrix) != 6) {
		php_error(E_WARNING, "%s(): Text matrix must have 6 elements", get_active_function_name(TSRMLS_C));
		RETURN_FALSE;
	}

	pdfmatrixptr = pdfmatrix;
	zend_hash_internal_pointer_reset(matrix);
	for(i=0; i<zend_hash_num_elements(matrix); i++) {
		zend_hash_get_current_data(matrix, (void *) &data);
		switch(Z_TYPE_P(data)) {
			case IS_DOUBLE:
				*pdfmatrixptr++ = (float) Z_DVAL_P(data);
				break;
			default:
				*pdfmatrixptr++ = 0.0;
				break;
		}
		zend_hash_move_forward(matrix);
	}

	cpdf_setTextMatrix(pdf, pdfmatrix[0], pdfmatrix[1],
                       pdfmatrix[2], pdfmatrix[3],
                       pdfmatrix[4], pdfmatrix[5]);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_set_text_pos(int pdfdoc, float x, float y [, int mode])
   Sets the position of text for the next cpdf_show call */
PHP_FUNCTION(cpdf_set_text_pos)
{
	pval *argv[4];
	int id, type, argc, mode=0;
	CPDFdoc *pdf;

	argc = ZEND_NUM_ARGS();
	if((argc < 3) || (argc > 4))
		WRONG_PARAM_COUNT;
	if (getParametersArray(ht, argc, argv) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long(argv[0]);
	convert_to_double(argv[1]);
	convert_to_double(argv[2]);
	id=Z_LVAL_P(argv[0]);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}
	
	if(argc > 3) {
		convert_to_long(argv[3]);
		mode = Z_LVAL_P(argv[3]);
	}
	if(mode == 1)
		cpdf_rawSetTextPosition(pdf, (float) Z_DVAL_P(argv[1]), (float) Z_DVAL_P(argv[2]));
	else
		cpdf_setTextPosition(pdf, (float) Z_DVAL_P(argv[1]), (float) Z_DVAL_P(argv[2]));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_rotate_text(int pdfdoc, float angle)
   Sets text rotation angle */
PHP_FUNCTION(cpdf_rotate_text)
{
	pval *arg1, *arg2;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 2 || getParameters(ht, 2, &arg1, &arg2) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_double(arg2);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	cpdf_rotateText(pdf, (float) Z_DVAL_P(arg2));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_set_char_spacing(int pdfdoc, float space)
   Sets character spacing */
PHP_FUNCTION(cpdf_set_char_spacing)
{
	pval *arg1, *arg2;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 2 || getParameters(ht, 2, &arg1, &arg2) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_double(arg2);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	cpdf_setCharacterSpacing(pdf, (float) Z_DVAL_P(arg2));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_set_word_spacing(int pdfdoc, float space)
   Sets spacing between words */
PHP_FUNCTION(cpdf_set_word_spacing)
{
	pval *arg1, *arg2;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 2 || getParameters(ht, 2, &arg1, &arg2) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_double(arg2);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	cpdf_setWordSpacing(pdf, (float) Z_DVAL_P(arg2));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto float cpdf_stringwidth(int pdfdoc, string text)
   Returns width of text in current font */
PHP_FUNCTION(cpdf_stringwidth)
{
	pval *arg1, *arg2;
	int id, type;
	double width;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 2 || getParameters(ht, 2, &arg1, &arg2) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_string(arg2);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	width = (double) cpdf_stringWidth(pdf, Z_STRVAL_P(arg2));

	RETURN_DOUBLE((double)width);
}
/* }}} */

/* {{{ proto bool cpdf_save(int pdfdoc)
   Saves current enviroment */
PHP_FUNCTION(cpdf_save)
{
	pval *arg1;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 1 || getParameters(ht, 1, &arg1) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	cpdf_gsave(pdf);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_restore(int pdfdoc)
   Restores formerly saved enviroment */
PHP_FUNCTION(cpdf_restore)
{
	pval *arg1;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 1 || getParameters(ht, 1, &arg1) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	cpdf_grestore(pdf);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_translate(int pdfdoc, float x, float y)
   Sets origin of coordinate system */
PHP_FUNCTION(cpdf_translate)
{
	pval *arg1, *arg2, *arg3;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 3 || getParameters(ht, 3, &arg1, &arg2, &arg3) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_double(arg2);
	convert_to_double(arg3);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	cpdf_rawTranslate(pdf, (float) Z_DVAL_P(arg2), (float) Z_DVAL_P(arg3));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_scale(int pdfdoc, float x_scale, float y_scale)
   Sets scaling */
PHP_FUNCTION(cpdf_scale)
{
	pval *arg1, *arg2, *arg3;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 3 || getParameters(ht, 3, &arg1, &arg2, &arg3) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_double(arg2);
	convert_to_double(arg3);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	cpdf_scale(pdf, (float) Z_DVAL_P(arg2), (float) Z_DVAL_P(arg3));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_rotate(int pdfdoc, float angle)
   Sets rotation */
PHP_FUNCTION(cpdf_rotate)
{
	pval *arg1, *arg2;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 2 || getParameters(ht, 2, &arg1, &arg2) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_double(arg2);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	cpdf_rotate(pdf, (float) Z_DVAL_P(arg2));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_setflat(int pdfdoc, float value)
   Sets flatness */
PHP_FUNCTION(cpdf_setflat)
{
	pval *arg1, *arg2;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 2 || getParameters(ht, 2, &arg1, &arg2) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_double(arg2);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	if((Z_LVAL_P(arg2) > 100) && (Z_LVAL_P(arg2) < 0)) {
		php_error(E_WARNING, "%s(): Parameter has to between 0 and 100", get_active_function_name(TSRMLS_C));
		RETURN_FALSE;
	}

	cpdf_setflat(pdf, (int) Z_DVAL_P(arg2));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_setlinejoin(int pdfdoc, int value)
   Sets linejoin parameter */
PHP_FUNCTION(cpdf_setlinejoin)
{
	pval *arg1, *arg2;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 2 || getParameters(ht, 2, &arg1, &arg2) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_long(arg2);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	if((Z_LVAL_P(arg2) > 2) && (Z_LVAL_P(arg2) < 0)) {
		php_error(E_WARNING, "%s(): Parameter has to between 0 and 2", get_active_function_name(TSRMLS_C));
		RETURN_FALSE;
	}

	cpdf_setlinejoin(pdf, Z_LVAL_P(arg2));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_setlinecap(int pdfdoc, int value)
   Sets linecap parameter */
PHP_FUNCTION(cpdf_setlinecap)
{
	pval *arg1, *arg2;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 2 || getParameters(ht, 2, &arg1, &arg2) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_long(arg2);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	if((Z_LVAL_P(arg2) > 2) && (Z_LVAL_P(arg2) < 0)) {
		php_error(E_WARNING, "%s(): Parameter has to be > 0 and =< 2", get_active_function_name(TSRMLS_C));
		RETURN_FALSE;
	}

	cpdf_setlinecap(pdf, Z_LVAL_P(arg2));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_setmiterlimit(int pdfdoc, float value)
   Sets miter limit */
PHP_FUNCTION(cpdf_setmiterlimit)
{
	pval *arg1, *arg2;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 2 || getParameters(ht, 2, &arg1, &arg2) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_double(arg2);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	if(Z_DVAL_P(arg2) < 1) {
		php_error(E_WARNING, "%s(): Parameter has to be >= 1", get_active_function_name(TSRMLS_C));
		RETURN_FALSE;
	}

	cpdf_setmiterlimit(pdf, (float) Z_DVAL_P(arg2));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_setlinewidth(int pdfdoc, float width)
   Sets line width */
PHP_FUNCTION(cpdf_setlinewidth)
{
	pval *arg1, *arg2;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 2 || getParameters(ht, 2, &arg1, &arg2) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_double(arg2);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	cpdf_setlinewidth(pdf, (float) Z_DVAL_P(arg2));

	RETURN_TRUE;
}
/* }}} */

#define BUFFERLEN 20
/* {{{ proto bool cpdf_setdash(int pdfdoc, long white, long black)
   Sets dash pattern */
PHP_FUNCTION(cpdf_setdash)
{
	pval *arg1, *arg2, *arg3;
	int id, type;
	char buffer[BUFFERLEN];
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 3 || getParameters(ht, 3, &arg1, &arg2, &arg3) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_long(arg2);
	convert_to_long(arg3);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}
	if (!pdf->currentMemStream) {
		RETURN_FALSE;
	}
	snprintf(buffer, BUFFERLEN, "[%d %d] 0", (int) Z_LVAL_P(arg2), (int) Z_LVAL_P(arg3));
	cpdf_setdash(pdf, buffer);

	RETURN_TRUE;
}
/* }}} */
#undef BUFFERLEN

/* {{{ proto bool cpdf_moveto(int pdfdoc, float x, float y [, int mode])
   Sets current point */
PHP_FUNCTION(cpdf_moveto)
{
	pval *argv[4];
	int id, type, argc, mode=0;
	CPDFdoc *pdf;

	argc = ZEND_NUM_ARGS();
	if((argc < 3) || (argc > 4))
		WRONG_PARAM_COUNT;
	if (getParametersArray(ht, argc, argv) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long(argv[0]);
	convert_to_double(argv[1]);
	convert_to_double(argv[2]);
	id=Z_LVAL_P(argv[0]);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	if(argc > 3) {
		convert_to_long(argv[3]);
		mode = Z_LVAL_P(argv[3]);
	}
	if(mode == 1)
		cpdf_rawMoveto(pdf, (float) Z_DVAL_P(argv[1]), (float) Z_DVAL_P(argv[2]));
	else
		cpdf_moveto(pdf, (float) Z_DVAL_P(argv[1]), (float) Z_DVAL_P(argv[2]));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_rmoveto(int pdfdoc, float x, float y [, int mode])
   Sets current point */
PHP_FUNCTION(cpdf_rmoveto)
{
	pval *argv[4];
	int id, type, argc, mode=0;
	CPDFdoc *pdf;

	argc = ZEND_NUM_ARGS();
	if((argc < 3) || (argc > 4))
		WRONG_PARAM_COUNT;
	if (getParametersArray(ht, argc, argv) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long(argv[0]);
	convert_to_double(argv[1]);
	convert_to_double(argv[2]);
	id=Z_LVAL_P(argv[0]);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	if(argc > 3) {
		convert_to_long(argv[3]);
		mode = Z_LVAL_P(argv[3]);
	}
	if(mode == 1)
		cpdf_rawRmoveto(pdf, (float) Z_DVAL_P(argv[1]), (float) Z_DVAL_P(argv[2]));
	else
		cpdf_rmoveto(pdf, (float) Z_DVAL_P(argv[1]), (float) Z_DVAL_P(argv[2]));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_curveto(int pdfdoc, float x1, float y1, float x2, float y2, float x3, float y3 [, int mode])
   Draws a curve */
PHP_FUNCTION(cpdf_curveto)
{
	pval *argv[8];
	int id, type, argc, mode=0;
	CPDFdoc *pdf;

	argc = ZEND_NUM_ARGS();
	if((argc < 7) || (argc > 8))
		WRONG_PARAM_COUNT;
	if (getParametersArray(ht, argc, argv) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long(argv[0]);
	convert_to_double(argv[1]);
	convert_to_double(argv[2]);
	convert_to_double(argv[3]);
	convert_to_double(argv[4]);
	convert_to_double(argv[5]);
	convert_to_double(argv[6]);
	id=Z_LVAL_P(argv[0]);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	if(argc > 7) {
		convert_to_long(argv[7]);
		mode = Z_LVAL_P(argv[7]);
	}
	if(mode == 1)
		cpdf_rawCurveto(pdf, (float) Z_DVAL_P(argv[1]),
	                	(float) Z_DVAL_P(argv[2]),
	                	(float) Z_DVAL_P(argv[3]),
	                	(float) Z_DVAL_P(argv[4]),
	                	(float) Z_DVAL_P(argv[5]),
	                	(float) Z_DVAL_P(argv[6]));
	else
		cpdf_curveto(pdf, (float) Z_DVAL_P(argv[1]),
	                	(float) Z_DVAL_P(argv[2]),
	                	(float) Z_DVAL_P(argv[3]),
	                	(float) Z_DVAL_P(argv[4]),
	                	(float) Z_DVAL_P(argv[5]),
	                	(float) Z_DVAL_P(argv[6]));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_lineto(int pdfdoc, float x, float y [, int mode])
   Draws a line */
PHP_FUNCTION(cpdf_lineto)
{
	pval *argv[4];
	int id, type, argc, mode=0;
	CPDFdoc *pdf;

	argc = ZEND_NUM_ARGS();
	if((argc < 3) || (argc > 4))
		WRONG_PARAM_COUNT;
	if (getParametersArray(ht, argc, argv) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long(argv[0]);
	convert_to_double(argv[1]);
	convert_to_double(argv[2]);
	id=Z_LVAL_P(argv[0]);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	if(argc > 3) {
		convert_to_long(argv[3]);
		mode = Z_LVAL_P(argv[3]);
	}
	if(mode == 1)
		cpdf_rawLineto(pdf, (float) Z_DVAL_P(argv[1]), (float) Z_DVAL_P(argv[2]));
	else
		cpdf_lineto(pdf, (float) Z_DVAL_P(argv[1]), (float) Z_DVAL_P(argv[2]));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_rlineto(int pdfdoc, float x, float y [, int mode])
   Draws a line relative to current point */
PHP_FUNCTION(cpdf_rlineto)
{
	pval *argv[4];
	int id, type, argc, mode=0;
	CPDFdoc *pdf;

	argc = ZEND_NUM_ARGS();
	if((argc < 3) || (argc > 4))
		WRONG_PARAM_COUNT;
	if (getParametersArray(ht, argc, argv) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long(argv[0]);
	convert_to_double(argv[1]);
	convert_to_double(argv[2]);
	id=Z_LVAL_P(argv[0]);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	if(argc > 3) {
		convert_to_long(argv[3]);
		mode = Z_LVAL_P(argv[3]);
	}
	if(mode == 1)
		cpdf_rawRlineto(pdf, (float) Z_DVAL_P(argv[1]), (float) Z_DVAL_P(argv[2]));
	else
		cpdf_rlineto(pdf, (float) Z_DVAL_P(argv[1]), (float) Z_DVAL_P(argv[2]));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_circle(int pdfdoc, float x, float y, float radius [, int mode])
   Draws a circle */
PHP_FUNCTION(cpdf_circle)
{
	pval *argv[5];
	int id, type, argc, mode=0;
	CPDFdoc *pdf;

	argc = ZEND_NUM_ARGS();
	if((argc < 4) || (argc > 5))
		WRONG_PARAM_COUNT;
	if (getParametersArray(ht, argc, argv) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long(argv[0]);
	convert_to_double(argv[1]);
	convert_to_double(argv[2]);
	convert_to_double(argv[3]);
	id=Z_LVAL_P(argv[0]);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	if(argc > 4) {
		convert_to_long(argv[4]);
		mode = Z_LVAL_P(argv[4]);
	}
	if(mode == 1)
		cpdf_rawCircle(pdf, (float) Z_DVAL_P(argv[1]), (float) Z_DVAL_P(argv[2]), (float) Z_DVAL_P(argv[3]));
	else
		cpdf_circle(pdf, (float) Z_DVAL_P(argv[1]), (float) Z_DVAL_P(argv[2]), (float) Z_DVAL_P(argv[3]));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_arc(int pdfdoc, float x, float y, float radius, float start, float end [, int mode])
   Draws an arc */
PHP_FUNCTION(cpdf_arc)
{
	pval *argv[7];
	int id, type, argc, mode=0;
	CPDFdoc *pdf;

	argc = ZEND_NUM_ARGS();
	if((argc < 6) || (argc > 7))
		WRONG_PARAM_COUNT;
	if (getParametersArray(ht, argc, argv) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long(argv[0]);
	convert_to_double(argv[1]);
	convert_to_double(argv[2]);
	convert_to_double(argv[3]);
	convert_to_double(argv[4]);
	convert_to_double(argv[5]);
	id=Z_LVAL_P(argv[0]);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	if(argc > 6) {
		convert_to_long(argv[6]);
		mode = Z_LVAL_P(argv[6]);
	}
	if(mode == 1)
		cpdf_rawArc(pdf, (float) Z_DVAL_P(argv[1]), (float) Z_DVAL_P(argv[2]), (float) Z_DVAL_P(argv[3]), (float) Z_DVAL_P(argv[4]), (float) Z_DVAL_P(argv[5]), 1);
	else
		cpdf_arc(pdf, (float) Z_DVAL_P(argv[1]), (float) Z_DVAL_P(argv[2]), (float) Z_DVAL_P(argv[3]), (float) Z_DVAL_P(argv[4]), (float) Z_DVAL_P(argv[5]), 1);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_rect(int pdfdoc, float x, float y, float width, float height [, int mode])
   Draws a rectangle */
PHP_FUNCTION(cpdf_rect)
{
	pval *argv[6];
	int id, type, argc, mode=0;
	CPDFdoc *pdf;

	argc = ZEND_NUM_ARGS();
	if((argc < 5) || (argc > 6))
		WRONG_PARAM_COUNT;
	if (getParametersArray(ht, argc, argv) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long(argv[0]);
	convert_to_double(argv[1]);
	convert_to_double(argv[2]);
	convert_to_double(argv[3]);
	convert_to_double(argv[4]);
	id=Z_LVAL_P(argv[0]);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	if(argc > 5) {
		convert_to_long(argv[5]);
		mode = Z_LVAL_P(argv[5]);
	}
	if(mode == 1)
		cpdf_rawRect(pdf, (float) Z_DVAL_P(argv[1]),
	                	(float) Z_DVAL_P(argv[2]),
	                	(float) Z_DVAL_P(argv[3]),
	                	(float) Z_DVAL_P(argv[4]));
	else
		cpdf_rect(pdf, (float) Z_DVAL_P(argv[1]),
	                	(float) Z_DVAL_P(argv[2]),
	                	(float) Z_DVAL_P(argv[3]),
	                	(float) Z_DVAL_P(argv[4]));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_newpath(int pdfdoc)
   Starts new path */
PHP_FUNCTION(cpdf_newpath)
{
	pval *arg1;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 1 || getParameters(ht, 1, &arg1) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	cpdf_newpath(pdf);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_closepath(int pdfdoc)
   Close path */
PHP_FUNCTION(cpdf_closepath)
{
	pval *arg1;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 1 || getParameters(ht, 1, &arg1) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	cpdf_closepath(pdf);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_closepath_stroke(int pdfdoc)
   Close path and draw line along path */
PHP_FUNCTION(cpdf_closepath_stroke)
{
	pval *arg1;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 1 || getParameters(ht, 1, &arg1) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	cpdf_closepath(pdf);
	cpdf_stroke(pdf);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_stroke(int pdfdoc)
   Draws line along path path */
PHP_FUNCTION(cpdf_stroke)
{
	pval *arg1;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 1 || getParameters(ht, 1, &arg1) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	cpdf_stroke(pdf);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_fill(int pdfdoc)
   Fills current path */
PHP_FUNCTION(cpdf_fill)
{
	pval *arg1;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 1 || getParameters(ht, 1, &arg1) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	cpdf_fill(pdf);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_fill_stroke(int pdfdoc)
   Fills and stroke current path */
PHP_FUNCTION(cpdf_fill_stroke)
{
	pval *arg1;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 1 || getParameters(ht, 1, &arg1) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	cpdf_fill(pdf);
	cpdf_stroke(pdf);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_closepath_fill_stroke(int pdfdoc)
   Close, fill and stroke current path */
PHP_FUNCTION(cpdf_closepath_fill_stroke)
{
	pval *arg1;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 1 || getParameters(ht, 1, &arg1) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	cpdf_closepath(pdf);
	cpdf_fill(pdf);
	cpdf_stroke(pdf);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_clip(int pdfdoc)
   Clips to current path */
PHP_FUNCTION(cpdf_clip)
{
	pval *arg1;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 1 || getParameters(ht, 1, &arg1) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	cpdf_clip(pdf);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_setgray_fill(int pdfdoc, float value)
   Sets filling color to gray value */
PHP_FUNCTION(cpdf_setgray_fill)
{
	pval *arg1, *arg2;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 2 || getParameters(ht, 2, &arg1, &arg2) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_double(arg2);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	cpdf_setgrayFill(pdf, (float) Z_DVAL_P(arg2));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_setgray_stroke(int pdfdoc, float value)
   Sets drawing color to gray value */
PHP_FUNCTION(cpdf_setgray_stroke)
{
	pval *arg1, *arg2;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 2 || getParameters(ht, 2, &arg1, &arg2) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_double(arg2);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	cpdf_setgrayStroke(pdf, (float) Z_DVAL_P(arg2));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_setgray(int pdfdoc, float value)
   Sets drawing and filling color to gray value */
PHP_FUNCTION(cpdf_setgray)
{
	pval *arg1, *arg2;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 2 || getParameters(ht, 2, &arg1, &arg2) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_double(arg2);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	cpdf_setgray(pdf, (float) Z_DVAL_P(arg2));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_setrgbcolor_fill(int pdfdoc, float red, float green, float blue)
   Sets filling color to rgb color value */
PHP_FUNCTION(cpdf_setrgbcolor_fill)
{
	pval *arg1, *arg2, *arg3, *arg4;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 4 || getParameters(ht, 4, &arg1, &arg2, &arg3, &arg4) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_double(arg2);
	convert_to_double(arg3);
	convert_to_double(arg4);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	cpdf_setrgbcolorFill(pdf, (float) Z_DVAL_P(arg2), (float) Z_DVAL_P(arg3), (float) Z_DVAL_P(arg4));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_setrgbcolor_stroke(int pdfdoc, float red, float green, float blue)
   Sets drawing color to RGB color value */
PHP_FUNCTION(cpdf_setrgbcolor_stroke)
{
	pval *arg1, *arg2, *arg3, *arg4;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 4 || getParameters(ht, 4, &arg1, &arg2, &arg3, &arg4) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_double(arg2);
	convert_to_double(arg3);
	convert_to_double(arg4);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	cpdf_setrgbcolorStroke(pdf, (float) Z_DVAL_P(arg2), (float) Z_DVAL_P(arg3), (float) Z_DVAL_P(arg4));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_setrgbcolor(int pdfdoc, float red, float green, float blue)
   Sets drawing and filling color to RGB color value */
PHP_FUNCTION(cpdf_setrgbcolor)
{
	pval *arg1, *arg2, *arg3, *arg4;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 4 || getParameters(ht, 4, &arg1, &arg2, &arg3, &arg4) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_double(arg2);
	convert_to_double(arg3);
	convert_to_double(arg4);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	cpdf_setrgbcolor(pdf, (float) Z_DVAL_P(arg2), (float) Z_DVAL_P(arg3), (float) Z_DVAL_P(arg4));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_set_page_animation(int pdfdoc, int transition, float duration, float direction, int orientation, int inout)
   Sets transition between pages */
PHP_FUNCTION(cpdf_set_page_animation)
{
	pval *arg1, *arg2, *arg3, *arg4, *arg5, *arg6;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 6 || getParameters(ht, 6, &arg1, &arg2, &arg3, &arg4, &arg5, &arg6) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_long(arg2);
	convert_to_double(arg3);
	convert_to_double(arg4);
	convert_to_long(arg5);
	convert_to_long(arg6);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	cpdf_setPageTransition(pdf, Z_LVAL_P(arg2), Z_DVAL_P(arg3), Z_DVAL_P(arg4),
	                       Z_LVAL_P(arg5), Z_LVAL_P(arg6));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_finalize(int pdfdoc)
   Creates PDF doc in memory */
PHP_FUNCTION(cpdf_finalize)
{
	pval *arg1;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 1 || getParameters(ht, 1, &arg1) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	cpdf_finalizeAll(pdf);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_output_buffer(int pdfdoc)
   Returns the internal memory stream as string */
PHP_FUNCTION(cpdf_output_buffer)
{
	pval *arg1;
	int id, type, lenght;
	CPDFdoc *pdf;
	char *buffer;

	if (ZEND_NUM_ARGS() != 1 || getParameters(ht, 1, &arg1) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	buffer = cpdf_getBufferForPDF(pdf, &lenght);

	php_write(buffer, lenght TSRMLS_CC);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_save_to_file(int pdfdoc, string filename)
   Saves the internal memory stream to a file */
PHP_FUNCTION(cpdf_save_to_file)
{
	pval *arg1, *arg2;
	int id, type;
	CPDFdoc *pdf;

	if (ZEND_NUM_ARGS() != 2 || getParameters(ht, 2, &arg1, &arg2) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(arg1);
	convert_to_string(arg2);
	id=Z_LVAL_P(arg1);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

#if APACHE
	if(strcmp(Z_STRVAL_P(arg2), "-") == 0)
		php_error(E_WARNING, "%s(): Writing to stdout as described in the ClibPDF manual is not possible if php is used as an Apache module. Use cpdf_output_buffer() instead.", get_active_function_name(TSRMLS_C));
#endif

	if (php_check_open_basedir(Z_STRVAL_P(arg2) TSRMLS_CC) || (PG(safe_mode) && !php_checkuid(Z_STRVAL_P(arg2), "wb+", CHECKUID_CHECK_MODE_PARAM))) {
		RETURN_FALSE;
	}

	cpdf_savePDFmemoryStreamToFile(pdf, Z_STRVAL_P(arg2));

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool cpdf_import_jpeg(int pdfdoc, string filename, float x, float y, float angle, float width, float height, float x_scale, float y_scale, int gsave [, int mode])
   Includes JPEG image */
PHP_FUNCTION(cpdf_import_jpeg)
{
	pval *argv[11];
	int id, type, argc, mode=0;
	float width, height, xscale, yscale;
	CPDFdoc *pdf;

	argc = ZEND_NUM_ARGS();
	if((argc < 10) || (argc > 11))
		WRONG_PARAM_COUNT;
	if (getParametersArray(ht, argc, argv) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long(argv[0]);
	convert_to_string(argv[1]);

	if (php_check_open_basedir(Z_STRVAL_P(argv[1]) TSRMLS_CC) || (PG(safe_mode) && !php_checkuid(Z_STRVAL_P(argv[1]), "rb+", CHECKUID_CHECK_MODE_PARAM))) {
		RETURN_FALSE;
	}

	convert_to_double(argv[2]);
	convert_to_double(argv[3]);
	convert_to_double(argv[4]);
	convert_to_double(argv[5]);
	width = (float) Z_DVAL_P(argv[5]);
	convert_to_double(argv[6]);
	height = (float) Z_DVAL_P(argv[6]);
	convert_to_double(argv[7]);
	xscale = (float) Z_DVAL_P(argv[7]);
	convert_to_double(argv[8]);
	yscale = (float) Z_DVAL_P(argv[8]);
	convert_to_long(argv[9]);
	id=Z_LVAL_P(argv[0]);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	if(argc > 10) {
		convert_to_long(argv[10]);
		mode = Z_LVAL_P(argv[10]);
	}
	if(mode == 1)
		cpdf_rawImportImage(pdf, Z_STRVAL_P(argv[1]),
	                	    JPEG_IMG,
	                	    (float) Z_DVAL_P(argv[2]),
	                	    (float) Z_DVAL_P(argv[3]),
	                	    (float) Z_DVAL_P(argv[4]),
	                	    &width,
	                	    &height,
	                	    &xscale,
	                	    &yscale,
	                	    Z_LVAL_P(argv[9]));
	else
		cpdf_rawImportImage(pdf, Z_STRVAL_P(argv[1]),
	                	    JPEG_IMG,
	                	    (float) Z_DVAL_P(argv[2]),
	                	    (float) Z_DVAL_P(argv[3]),
	                	    (float) Z_DVAL_P(argv[4]),
	                	    &width,
	                	    &height,
	                	    &xscale,
	                	    &yscale,
	                	    Z_LVAL_P(argv[9]));

	RETURN_TRUE;
}
/* }}} */

#if HAVE_LIBGD13
/* {{{ proto bool cpdf_place_inline_image(int pdfdoc, int gdimage, float x, float y, float angle, fload width, float height, int gsave [, int mode])
   Includes image */
PHP_FUNCTION(cpdf_place_inline_image)
{
	pval *argv[11];
	int id, gid, type, argc, mode=0;
	int count, i, j, color;
	CPDFdoc *pdf;
	unsigned char *buffer, *ptr;
	gdImagePtr im;

	argc = ZEND_NUM_ARGS();
	if((argc < 8) || (argc > 9))
		WRONG_PARAM_COUNT;
	if (getParametersArray(ht, argc, argv) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long(argv[0]);
	convert_to_long(argv[1]);
	convert_to_double(argv[2]);
	convert_to_double(argv[3]);
	convert_to_double(argv[4]);
	convert_to_double(argv[5]);
	convert_to_double(argv[6]);
	convert_to_long(argv[7]);
	id=Z_LVAL_P(argv[0]);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	gid=Z_LVAL_P(argv[1]);
	im = zend_list_find(gid, &type);
	
	ZEND_GET_RESOURCE_TYPE_ID(CPDF_GLOBAL(le_gd), "gd");
	if(!CPDF_GLOBAL(le_gd))
	{
		php_error(E_ERROR, "%s(): Unable to find handle for GD image stream. Please check the GD extension is loaded.", get_active_function_name(TSRMLS_C));
	}

	if (!im || type != CPDF_GLOBAL(le_gd)) {
		php_error(E_WARNING, "%s(): Unable to find image pointer", get_active_function_name(TSRMLS_C));
		RETURN_FALSE;
	}

	if(argc > 8) {
		convert_to_long(argv[8]);
		mode = Z_LVAL_P(argv[8]);
	}

	count = 3 * im->sx * im->sy;
	buffer = (unsigned char *) safe_emalloc(3 * im->sx, im->sy, 0);

	ptr = buffer;
	for(i=0; i<im->sy; i++) {
		for(j=0; j<im->sx; j++) {
#if HAVE_LIBGD20
			if(gdImageTrueColor(im)) {
				if (im->tpixels && gdImageBoundsSafe(im, j, i)) {
					color = gdImageTrueColorPixel(im, j, i);
					*ptr++ = (color >> 16) & 0xFF;
					*ptr++ = (color >> 8) & 0xFF;
					*ptr++ = color & 0xFF;
				}
			} else {
#endif
				if (im->pixels && gdImageBoundsSafe(im, j, i)) {
					color = im->pixels[i][j];
					*ptr++ = im->red[color];
					*ptr++ = im->green[color];
					*ptr++ = im->blue[color];
				}
#if HAVE_LIBGD20
			}
#endif
		}
	}

	if(mode == 1)
		cpdf_placeInLineImage(pdf, buffer, count,
	                	    (float) Z_DVAL_P(argv[2]),
	                	    (float) Z_DVAL_P(argv[3]),
	                	    (float) Z_DVAL_P(argv[4]),
	                	    (float) Z_DVAL_P(argv[5]),
	                	    (float) Z_DVAL_P(argv[6]),
	                	    im->sx,
	                	    im->sy,
	                	    8, 2, Z_LVAL_P(argv[7]));
	else
		cpdf_rawPlaceInLineImage(pdf, buffer, count,
	                	    (float) Z_DVAL_P(argv[2]),
	                	    (float) Z_DVAL_P(argv[3]),
	                	    (float) Z_DVAL_P(argv[4]),
	                	    (float) Z_DVAL_P(argv[5]),
	                	    (float) Z_DVAL_P(argv[6]),
	                	    im->sx,
	                	    im->sy,
	                	    8, 2, Z_LVAL_P(argv[7]));

	efree(buffer);
	RETURN_TRUE;
}
/* }}} */
#endif

/* {{{ proto bool cpdf_add_annotation(int pdfdoc, float xll, float yll, float xur, float xur, string title, string text [, int mode])
   Sets annotation */
PHP_FUNCTION(cpdf_add_annotation)
{
	pval *argv[11];
	int id, type, argc, mode=0;
	CPDFdoc *pdf;
	CPDFannotAttrib attrib;

	argc = ZEND_NUM_ARGS();
	if((argc < 7) || (argc > 8))
		WRONG_PARAM_COUNT;
	if (getParametersArray(ht, argc, argv) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long(argv[0]);
	convert_to_double(argv[1]);
	convert_to_double(argv[2]);
	convert_to_double(argv[3]);
	convert_to_double(argv[4]);
	convert_to_string(argv[5]);
	convert_to_string(argv[6]);
	id=Z_LVAL_P(argv[0]);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	attrib.flags = AF_NOZOOM | AF_NOROTATE | AF_READONLY;
	attrib.border_array = "[0 0 1 [4 2]]";
	attrib.BS = NULL;
	attrib.r = 0.00;
	attrib.g = 1.00;
	attrib.b = 1.00;
	if(argc > 7) {
		convert_to_long(argv[7]);
		mode = Z_LVAL_P(argv[7]);
	}
	if(mode == 1)
		cpdf_rawSetAnnotation(pdf, (float) Z_DVAL_P(argv[1]),
	                	    (float) Z_DVAL_P(argv[2]),
	                	    (float) Z_DVAL_P(argv[3]),
	                	    (float) Z_DVAL_P(argv[4]),
	                	    Z_STRVAL_P(argv[5]),
	                	    Z_STRVAL_P(argv[6]),
		                    &attrib);
	else
		cpdf_setAnnotation(pdf, (float) Z_DVAL_P(argv[1]),
	                	    (float) Z_DVAL_P(argv[2]),
	                	    (float) Z_DVAL_P(argv[3]),
	                	    (float) Z_DVAL_P(argv[4]),
	                	    Z_STRVAL_P(argv[5]),
	                	    Z_STRVAL_P(argv[6]),
		                    &attrib);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto void cpdf_set_action_url(int pdfdoc, float xll, float yll, float xur, float xur, string url [, int mode])
   Sets hyperlink */
PHP_FUNCTION(cpdf_set_action_url)
{
	pval *argv[11];
	int id, type, argc, mode=0;
	CPDFdoc *pdf;
	CPDFannotAttrib attrib;

	argc = ZEND_NUM_ARGS();
	if((argc < 6) || (argc > 7))
		WRONG_PARAM_COUNT;
	if (getParametersArray(ht, argc, argv) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long(argv[0]);
	convert_to_double(argv[1]);
	convert_to_double(argv[2]);
	convert_to_double(argv[3]);
	convert_to_double(argv[4]);
	convert_to_string(argv[5]);
	id=Z_LVAL_P(argv[0]);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	attrib.flags = AF_NOZOOM | AF_NOROTATE | AF_READONLY;
	attrib.border_array = "[0 0 1 [4 0]]";
	attrib.BS = NULL;
	attrib.r = 0.00;
	attrib.g = 0.00;
	attrib.b = 1.00;
	if(argc > 6) {
		convert_to_long(argv[6]);
		mode = Z_LVAL_P(argv[6]);
	}
	if(mode == 1)
		cpdf_rawSetActionURL(pdf, (float) Z_DVAL_P(argv[1]),
	                	    (float) Z_DVAL_P(argv[2]),
	                	    (float) Z_DVAL_P(argv[3]),
	                	    (float) Z_DVAL_P(argv[4]),
	                	    Z_STRVAL_P(argv[5]),
		                    &attrib);
	else
		cpdf_setActionURL(pdf, (float) Z_DVAL_P(argv[1]),
	                	    (float) Z_DVAL_P(argv[2]),
	                	    (float) Z_DVAL_P(argv[3]),
	                	    (float) Z_DVAL_P(argv[4]),
	                	    Z_STRVAL_P(argv[5]),
		                    &attrib);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto int cpdf_add_outline(int pdfdoc, int lastoutline, int sublevel, int open, int pagenr, string title)
   Adds outline */
PHP_FUNCTION(cpdf_add_outline)
{
	pval *argv[11];
	int id, oid, type, argc;
	CPDFdoc *pdf;
	CPDFoutlineEntry *lastoutline;

	argc = ZEND_NUM_ARGS();
	if(argc != 6)
		WRONG_PARAM_COUNT;
	if (getParametersArray(ht, argc, argv) == FAILURE)
		WRONG_PARAM_COUNT;

	convert_to_long(argv[0]);
	convert_to_long(argv[1]);
	convert_to_long(argv[2]);
	convert_to_long(argv[3]);
	convert_to_long(argv[4]);
	convert_to_string(argv[5]);
	id=Z_LVAL_P(argv[0]);
	pdf = zend_list_find(id, &type);
	if(!pdf || type!=CPDF_GLOBAL(le_cpdf)) {
		php_error(E_WARNING, "%s(): Unable to find identifier %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE;
	}

	oid=Z_LVAL_P(argv[1]);
	lastoutline = zend_list_find(oid, &type);
	if(!lastoutline || type!=CPDF_GLOBAL(le_outline)) {
		lastoutline = NULL;
/*		php_error(E_WARNING, "%s(): Unable to find last outline entry %d", get_active_function_name(TSRMLS_C), id);
		RETURN_FALSE; */
	}

	lastoutline = cpdf_addOutlineEntry(pdf, lastoutline,
	              	     Z_LVAL_P(argv[2]),
	              	     Z_LVAL_P(argv[3]),
	               	     Z_LVAL_P(argv[4]),
	               	     Z_STRVAL_P(argv[5]),
		             1, 0.0, 0.0, 0.0, 0.0);

	id = zend_list_insert(lastoutline, CPDF_GLOBAL(le_outline));
	RETURN_LONG(id);
}
/* }}} */

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
