/*
   +----------------------------------------------------------------------+
   | PHP Version 4                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 2001 The PHP Group                                     |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Author: Tsukada Takuya <tsukada@fminn.nagano.nagano.jp>              |
   |         Rui Hirokawa <hirokawa@php.net>                              |
   +----------------------------------------------------------------------+
 */

/* $Id: mbstring.c,v 1.142.2.47.2.24 2008/06/13 14:50:03 hirokawa Exp $ */

/*
 * PHP4 Multibyte String module "mbstring"
 *
 * History:
 *   2000.5.19  Release php-4.0RC2_jstring-1.0
 *   2001.4.1   Release php4_jstring-1.0.91
 *   2001.4.30  Release php4_jstring-1.1 (contribute to The PHP Group)
 *   2001.5.1   Renamed from jstring to mbstring (hirokawa@php.net)
 */

/*
 * PHP3 Internationalization support program.
 *
 * Copyright (c) 1999,2000 by the PHP3 internationalization team.
 * All rights reserved.
 *
 * See README_PHP3-i18n-ja for more detail.
 *
 * Authors:
 *    Hironori Sato <satoh@jpnnet.com>
 *    Shigeru Kanemoto <sgk@happysize.co.jp>
 *    Tsukada Takuya <tsukada@fminn.nagano.nagano.jp>
 *    Rui Hirokawa <rui_hirokawa@ybb.ne.jp>
 */

/* {{{ includes */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "php_variables.h"
#include "mbstring.h"
#include "ext/standard/php_string.h"
#include "ext/standard/php_mail.h"
#include "ext/standard/exec.h"
#include "ext/standard/url.h"
#include "main/php_output.h"
#include "ext/standard/info.h"

#include "libmbfl/mbfl/mbfl_allocators.h"

#include "php_variables.h"
#include "php_globals.h"
#include "rfc1867.h"
#include "php_content_types.h"
#include "SAPI.h"
#include "php_unicode.h"
#include "TSRM.h"

#ifdef ZEND_MULTIBYTE
#include "zend_multibyte.h"
#endif /* ZEND_MULTIBYTE */

#if HAVE_MBSTRING

#if HAVE_MBREGEX
#include "mbregex.h"
#endif
/* }}} */

#ifdef ZTS
MUTEX_T mbregex_locale_mutex = NULL;
#endif

/* {{{ php_mb_encoding_handler_info_t */
typedef struct _php_mb_encoding_handler_info_t {
	int data_type;
	const char *separator;
	unsigned int report_errors: 1;
	enum mbfl_no_language to_language;
	enum mbfl_no_encoding to_encoding;
	enum mbfl_no_language from_language;
	int num_from_encodings;
	const enum mbfl_no_encoding *from_encodings;
} php_mb_encoding_handler_info_t;
/* }}} */

/* {{{ php_mb_default_identify_list */
typedef struct _php_mb_nls_ident_list {
	enum mbfl_no_language lang;
	enum mbfl_no_encoding* list;
	int list_size;
} php_mb_nls_ident_list;

static const enum mbfl_no_encoding php_mb_default_identify_list_ja[] = {
	mbfl_no_encoding_ascii,
	mbfl_no_encoding_jis,
	mbfl_no_encoding_utf8,
	mbfl_no_encoding_euc_jp,
	mbfl_no_encoding_sjis
};

static const enum mbfl_no_encoding php_mb_default_identify_list_cn[] = {
	mbfl_no_encoding_ascii,
	mbfl_no_encoding_utf8,
	mbfl_no_encoding_euc_cn,
	mbfl_no_encoding_cp936
};

static const enum mbfl_no_encoding php_mb_default_identify_list_tw_hk[] = {
	mbfl_no_encoding_ascii,
	mbfl_no_encoding_utf8,
	mbfl_no_encoding_euc_tw,
	mbfl_no_encoding_big5
};

static const enum mbfl_no_encoding php_mb_default_identify_list_kr[] = {
	mbfl_no_encoding_ascii,
	mbfl_no_encoding_utf8,
	mbfl_no_encoding_euc_kr,
	mbfl_no_encoding_uhc
};

static const enum mbfl_no_encoding php_mb_default_identify_list_ru[] = {
	mbfl_no_encoding_ascii,
	mbfl_no_encoding_utf8,
	mbfl_no_encoding_koi8r,
	mbfl_no_encoding_cp1251,
	mbfl_no_encoding_cp866
};

static const enum mbfl_no_encoding php_mb_default_identify_list_neut[] = {
	mbfl_no_encoding_ascii,
	mbfl_no_encoding_utf8
};


static const php_mb_nls_ident_list php_mb_default_identify_list[] = {
	{ mbfl_no_language_japanese, php_mb_default_identify_list_ja, sizeof(php_mb_default_identify_list_ja) / sizeof(php_mb_default_identify_list_ja[0]) },
	{ mbfl_no_language_korean, php_mb_default_identify_list_kr, sizeof(php_mb_default_identify_list_kr) / sizeof(php_mb_default_identify_list_kr[0]) },
	{ mbfl_no_language_traditional_chinese, php_mb_default_identify_list_tw_hk, sizeof(php_mb_default_identify_list_tw_hk) / sizeof(php_mb_default_identify_list_tw_hk[0]) },
	{ mbfl_no_language_simplified_chinese, php_mb_default_identify_list_cn, sizeof(php_mb_default_identify_list_cn) / sizeof(php_mb_default_identify_list_cn[0]) },
	{ mbfl_no_language_russian, php_mb_default_identify_list_ru, sizeof(php_mb_default_identify_list_ru) / sizeof(php_mb_default_identify_list_ru[0]) },
	{ mbfl_no_language_neutral, php_mb_default_identify_list_neut, sizeof(php_mb_default_identify_list_neut) / sizeof(php_mb_default_identify_list_neut[0]) }
};

/* }}} */

static const unsigned char third_and_rest_force_ref[] = { 3, BYREF_NONE, BYREF_NONE, BYREF_FORCE_REST };
static const unsigned char second_args_force_ref[]    = { 2, BYREF_NONE, BYREF_FORCE };
#if HAVE_MBREGEX
static const unsigned char third_argument_force_ref[] = { 3, BYREF_NONE, BYREF_NONE, BYREF_FORCE };
#endif

/* {{{ sapi_post_entry mbstr_post_entries[] */
static sapi_post_entry mbstr_post_entries[] = {
	{ DEFAULT_POST_CONTENT_TYPE, sizeof(DEFAULT_POST_CONTENT_TYPE)-1, sapi_read_standard_form_data, php_mbstr_post_handler },
	{ MULTIPART_CONTENT_TYPE,    sizeof(MULTIPART_CONTENT_TYPE)-1,    NULL,                         rfc1867_post_handler },
	{ NULL, 0, NULL, NULL }
};
/* }}} */

/* {{{ sapi_post_entry php_post_entries[] */
static sapi_post_entry php_post_entries[] = {
	{ DEFAULT_POST_CONTENT_TYPE, sizeof(DEFAULT_POST_CONTENT_TYPE)-1, sapi_read_standard_form_data,	php_std_post_handler },
	{ MULTIPART_CONTENT_TYPE,    sizeof(MULTIPART_CONTENT_TYPE)-1,    NULL,                         rfc1867_post_handler },
	{ NULL, 0, NULL, NULL }
};
/* }}} */

/* {{{ mb_overload_def mb_ovld[] */
static const struct mb_overload_def mb_ovld[] = {
	{MB_OVERLOAD_MAIL, "mail", "mb_send_mail", "mb_orig_mail"},
	{MB_OVERLOAD_STRING, "strlen", "mb_strlen", "mb_orig_strlen"},
	{MB_OVERLOAD_STRING, "strpos", "mb_strpos", "mb_orig_strpos"},
	{MB_OVERLOAD_STRING, "strrpos", "mb_strrpos", "mb_orig_strrpos"},
	{MB_OVERLOAD_STRING, "substr", "mb_substr", "mb_orig_substr"},
	{MB_OVERLOAD_STRING, "strtolower", "mb_strtolower", "mb_orig_strtolower"},
	{MB_OVERLOAD_STRING, "strtoupper", "mb_strtoupper", "mb_orig_strtoupper"},
	{MB_OVERLOAD_STRING, "substr_count", "mb_substr_count", "mb_orig_substr_count"},
#if HAVE_MBREGEX
	{MB_OVERLOAD_REGEX, "ereg", "mb_ereg", "mb_orig_ereg"},
	{MB_OVERLOAD_REGEX, "eregi", "mb_eregi", "mb_orig_eregi"},
	{MB_OVERLOAD_REGEX, "ereg_replace", "mb_ereg_replace", "mb_orig_ereg_replace"},
	{MB_OVERLOAD_REGEX, "eregi_replace", "mb_eregi_replace", "mb_orig_eregi_replace"},
	{MB_OVERLOAD_REGEX, "split", "mb_split", "mb_orig_split"},
#endif
	{0, NULL, NULL, NULL}
}; 
/* }}} */

#if HAVE_MBREGEX
struct def_mbctype_tbl {
	enum mbfl_no_encoding mbfl_encoding;
	int regex_encoding;
};

const struct def_mbctype_tbl mbctype_tbl[] = {
	{mbfl_no_encoding_ascii,MBCTYPE_ASCII},
	{mbfl_no_encoding_7bit,MBCTYPE_ASCII},
	{mbfl_no_encoding_8bit,MBCTYPE_ASCII},
	{mbfl_no_encoding_euc_jp,MBCTYPE_EUC},
	{mbfl_no_encoding_eucjp_win,MBCTYPE_EUC},
	{mbfl_no_encoding_sjis,MBCTYPE_SJIS},
	{mbfl_no_encoding_sjis_win,MBCTYPE_SJIS},
	{mbfl_no_encoding_utf8,MBCTYPE_UTF8},
	{mbfl_no_encoding_pass,-1}
};
#endif

/* {{{ function_entry mbstring_functions[] */
function_entry mbstring_functions[] = {
	PHP_FE(mb_convert_case,				NULL)
	PHP_FE(mb_strtoupper,				NULL)
	PHP_FE(mb_strtolower,				NULL)
	PHP_FE(mb_language,					NULL)
	PHP_FE(mb_internal_encoding,		NULL)
	PHP_FE(mb_http_input,				NULL)
	PHP_FE(mb_http_output,			NULL)
	PHP_FE(mb_detect_order,			NULL)
	PHP_FE(mb_substitute_character,	NULL)
	PHP_FE(mb_parse_str,			(unsigned char *)second_args_force_ref)
	PHP_FE(mb_output_handler,			NULL)
	PHP_FE(mb_preferred_mime_name,	NULL)
	PHP_FE(mb_strlen,					NULL)
	PHP_FE(mb_strpos,					NULL)
	PHP_FE(mb_strrpos,				NULL)
	PHP_FE(mb_substr_count,			NULL)
	PHP_FE(mb_substr,					NULL)
	PHP_FE(mb_strcut,					NULL)
	PHP_FE(mb_strwidth,				NULL)
	PHP_FE(mb_strimwidth,				NULL)
	PHP_FE(mb_convert_encoding,		NULL)
	PHP_FE(mb_detect_encoding,		NULL)
	PHP_FE(mb_convert_kana,			NULL)
	PHP_FE(mb_encode_mimeheader,		NULL)
	PHP_FE(mb_decode_mimeheader,		NULL)
	PHP_FE(mb_convert_variables,		(unsigned char *)third_and_rest_force_ref)
	PHP_FE(mb_encode_numericentity,		NULL)
	PHP_FE(mb_decode_numericentity,		NULL)
	PHP_FE(mb_send_mail,					NULL)
	PHP_FE(mb_get_info,					NULL)
	PHP_FE(mb_check_encoding,		NULL)
	PHP_FALIAS(mbstrlen,	mb_strlen,	NULL)
	PHP_FALIAS(mbstrpos,	mb_strpos,	NULL)
	PHP_FALIAS(mbstrrpos,	mb_strrpos,	NULL)
	PHP_FALIAS(mbsubstr,	mb_substr,	NULL)
	PHP_FALIAS(mbstrcut,	mb_strcut,	NULL)
	PHP_FALIAS(i18n_internal_encoding,	mb_internal_encoding,	NULL)
	PHP_FALIAS(i18n_http_input,			mb_http_input,		NULL)
	PHP_FALIAS(i18n_http_output,		mb_http_output,		NULL)
	PHP_FALIAS(i18n_convert,			mb_convert_encoding,	NULL)
	PHP_FALIAS(i18n_discover_encoding,	mb_detect_encoding,	NULL)
	PHP_FALIAS(i18n_mime_header_encode,	mb_encode_mimeheader,	NULL)
	PHP_FALIAS(i18n_mime_header_decode,	mb_decode_mimeheader,	NULL)
	PHP_FALIAS(i18n_ja_jp_hantozen,		mb_convert_kana,		NULL)
#if HAVE_MBREGEX
	PHP_FE(mb_regex_encoding,	NULL)
	PHP_FE(mb_regex_set_options,	NULL)
	PHP_FE(mb_ereg,			(unsigned char *)third_argument_force_ref)
	PHP_FE(mb_eregi,			(unsigned char *)third_argument_force_ref)
	PHP_FE(mb_ereg_replace,			NULL)
	PHP_FE(mb_eregi_replace,			NULL)
	PHP_FE(mb_split,					NULL)
	PHP_FE(mb_ereg_match,			NULL)
	PHP_FE(mb_ereg_search,			NULL)
	PHP_FE(mb_ereg_search_pos,		NULL)
	PHP_FE(mb_ereg_search_regs,		NULL)
	PHP_FE(mb_ereg_search_init,		NULL)
	PHP_FE(mb_ereg_search_getregs,	NULL)
	PHP_FE(mb_ereg_search_getpos,	NULL)
	PHP_FE(mb_ereg_search_setpos,	NULL)
	PHP_FALIAS(mbregex_encoding,	mb_regex_encoding,	NULL)
	PHP_FALIAS(mbereg,	mb_ereg,	NULL)
	PHP_FALIAS(mberegi,	mb_eregi,	NULL)
	PHP_FALIAS(mbereg_replace,	mb_ereg_replace,	NULL)
	PHP_FALIAS(mberegi_replace,	mb_eregi_replace,	NULL)
	PHP_FALIAS(mbsplit,	mb_split,	NULL)
	PHP_FALIAS(mbereg_match,	mb_ereg_match,	NULL)
	PHP_FALIAS(mbereg_search,	mb_ereg_search,	NULL)
	PHP_FALIAS(mbereg_search_pos,	mb_ereg_search_pos,	NULL)
	PHP_FALIAS(mbereg_search_regs,	mb_ereg_search_regs,	NULL)
	PHP_FALIAS(mbereg_search_init,	mb_ereg_search_init,	NULL)
	PHP_FALIAS(mbereg_search_getregs,	mb_ereg_search_getregs,	NULL)
	PHP_FALIAS(mbereg_search_getpos,	mb_ereg_search_getpos,	NULL)
	PHP_FALIAS(mbereg_search_setpos,	mb_ereg_search_setpos,	NULL)
#endif
	{ NULL, NULL, NULL }
};
/* }}} */

/* {{{ zend_module_entry mbstring_module_entry */
zend_module_entry mbstring_module_entry = {
    STANDARD_MODULE_HEADER,
	"mbstring",
	mbstring_functions,
	PHP_MINIT(mbstring),
	PHP_MSHUTDOWN(mbstring),
	PHP_RINIT(mbstring),
	PHP_RSHUTDOWN(mbstring),
	PHP_MINFO(mbstring),
    NO_VERSION_YET,
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

ZEND_DECLARE_MODULE_GLOBALS(mbstring)

#ifdef COMPILE_DL_MBSTRING
ZEND_GET_MODULE(mbstring)
#endif

/* {{{ allocators */
static void *_php_mb_allocators_malloc(unsigned int sz)
{
	return emalloc(sz);
}

static void *_php_mb_allocators_realloc(void *ptr, unsigned int sz)
{
	return erealloc(ptr, sz);
}

static void *_php_mb_allocators_calloc(unsigned int nelems, unsigned int szelem)
{
	return ecalloc(nelems, szelem);
}

static void _php_mb_allocators_free(void *ptr)
{
	efree(ptr);
} 

static void *_php_mb_allocators_pmalloc(unsigned int sz)
{
	return pemalloc(sz, 1);
}

static void *_php_mb_allocators_prealloc(void *ptr, unsigned int sz)
{
	return perealloc(ptr, sz, 1);
}

static void _php_mb_allocators_pfree(void *ptr)
{
	pefree(ptr, 1);
} 

static mbfl_allocators _php_mb_allocators = {
	_php_mb_allocators_malloc,
	_php_mb_allocators_realloc,
	_php_mb_allocators_calloc,
	_php_mb_allocators_free,
	_php_mb_allocators_pmalloc,
	_php_mb_allocators_prealloc,
	_php_mb_allocators_pfree
};
/* }}} */


/* {{{ static int php_mb_parse_encoding_list()
 *  Return 0 if input contains any illegal encoding, otherwise 1.
 *  Even if any illegal encoding is detected the result may contain a list 
 *  of parsed encodings.
 */
static int
php_mb_parse_encoding_list(const char *value, int value_length, enum mbfl_no_encoding **return_list, int *return_size, int persistent TSRMLS_DC)
{
	int n, l, size, bauto, ret = 1;
	char *p, *p1, *p2, *endp, *tmpstr;
	enum mbfl_no_encoding no_encoding;
	enum mbfl_no_encoding *src, *entry, *list;

	list = NULL;
	if (value == NULL || value_length <= 0) {
		if (return_list) {
			*return_list = NULL;
		}
		if (return_size) {
			*return_size = 0;
		}
		return 0;
	} else {
		enum mbfl_no_encoding *identify_list;
		int identify_list_size;

		identify_list = MBSTRG(default_detect_order_list);
		identify_list_size = MBSTRG(default_detect_order_list_size);

		/* copy the value string for work */
		if (value[0]=='"' && value[value_length-1]=='"' && value_length>2) {
			tmpstr = (char *)estrndup(value+1, value_length-2);
			value_length -= 2;
		}
		else
			tmpstr = (char *)estrndup(value, value_length);
		if (tmpstr == NULL) {
			return 0;
		}
		/* count the number of listed encoding names */
		endp = tmpstr + value_length;
		n = 1;
		p1 = tmpstr;
		while ((p2 = php_memnstr(p1, ",", 1, endp)) != NULL) {
			p1 = p2 + 1;
			n++;
		}
		size = n + identify_list_size;
		/* make list */
		list = (enum mbfl_no_encoding *)pecalloc(size, sizeof(int), persistent);
		if (list != NULL) {
			entry = list;
			n = 0;
			bauto = 0;
			p1 = tmpstr;
			do {
				p2 = p = php_memnstr(p1, ",", 1, endp);
				if (p == NULL) {
					p = endp;
				}
				*p = '\0';
				/* trim spaces */
				while (p1 < p && (*p1 == ' ' || *p1 == '\t')) {
					p1++;
				}
				p--;
				while (p > p1 && (*p == ' ' || *p == '\t')) {
					*p = '\0';
					p--;
				}
				/* convert to the encoding number and check encoding */
				if (strcasecmp(p1, "auto") == 0) {
					if (!bauto) {
						bauto = 1;
						l = identify_list_size;
						src = identify_list;
						while (l > 0) {
							*entry++ = *src++;
							l--;
							n++;
						}
					}
				} else {
					no_encoding = mbfl_name2no_encoding(p1);
					if (no_encoding != mbfl_no_encoding_invalid) {
						*entry++ = no_encoding;
						n++;
					} else {
						ret = 0;
					}
				}
				p1 = p2 + 1;
			} while (n < size && p2 != NULL);
			if (n > 0) {
				if (return_list) {
					*return_list = list;
				} else {
					pefree(list, persistent);
				}
			} else {
				pefree(list, persistent);
				if (return_list) {
					*return_list = NULL;
				}
				ret = 0;
			}
			if (return_size) {
				*return_size = n;
			}
		} else {
			if (return_list) {
				*return_list = NULL;
			}
			if (return_size) {
				*return_size = 0;
			}
			ret = 0;
		}
		efree(tmpstr);
	}

	return ret;
}
/* }}} */

/* {{{ MBSTRING_API php_mb_check_encoding_list */
MBSTRING_API int php_mb_check_encoding_list(const char *encoding_list TSRMLS_DC) {
	return php_mb_parse_encoding_list(encoding_list, strlen(encoding_list), NULL, NULL, 0 TSRMLS_CC);	
}
/* }}} */

/* {{{ static int php_mb_parse_encoding_array()
 *  Return 0 if input contains any illegal encoding, otherwise 1.
 *  Even if any illegal encoding is detected the result may contain a list 
 *  of parsed encodings.
 */
static int
php_mb_parse_encoding_array(zval *array, enum mbfl_no_encoding **return_list, int *return_size, int persistent TSRMLS_DC)
{
	zval **hash_entry;
	HashTable *target_hash;
	int i, n, l, size, bauto,ret = 1;
	enum mbfl_no_encoding no_encoding;
	enum mbfl_no_encoding *src, *list, *entry;

	list = NULL;
	if (Z_TYPE_P(array) == IS_ARRAY) {
		enum mbfl_no_encoding *identify_list;
		int identify_list_size;

		identify_list = MBSTRG(default_detect_order_list);
		identify_list_size = MBSTRG(default_detect_order_list_size);

		target_hash = Z_ARRVAL_P(array);
		zend_hash_internal_pointer_reset(target_hash);
		i = zend_hash_num_elements(target_hash);
		size = i + identify_list_size;
		list = (enum mbfl_no_encoding *)pecalloc(size, sizeof(int), persistent);
		if (list != NULL) {
			entry = list;
			bauto = 0;
			n = 0;
			while (i > 0) {
				if (zend_hash_get_current_data(target_hash, (void **) &hash_entry) == FAILURE) {
					break;
				}
				convert_to_string_ex(hash_entry);
				if (strcasecmp(Z_STRVAL_PP(hash_entry), "auto") == 0) {
					if (!bauto) {
						bauto = 1;
						l = identify_list_size; 
						src = identify_list;
						while (l > 0) {
							*entry++ = *src++;
							l--;
							n++;
						}
					}
				} else {
					no_encoding = mbfl_name2no_encoding(Z_STRVAL_PP(hash_entry));
					if (no_encoding != mbfl_no_encoding_invalid) {
						*entry++ = no_encoding;
						n++;
					} else {
						ret = 0;
					}
				}
				zend_hash_move_forward(target_hash);
				i--;
			}
			if (n > 0) {
				if (return_list) {
					*return_list = list;
				} else {
					pefree(list, persistent);
				}
			} else {
				pefree(list, persistent);
				if (return_list) {
					*return_list = NULL;
				}
				ret = 0;
			}
			if (return_size) {
				*return_size = n;
			}
		} else {
			if (return_list) {
				*return_list = NULL;
			}
			if (return_size) {
				*return_size = 0;
			}
			ret = 0;
		}
	}

	return ret;
}
/* }}} */

/* {{{ php_mb_nls_get_default_detect_order_list */
static int php_mb_nls_get_default_detect_order_list(enum mbfl_no_language lang, enum mbfl_no_encoding **plist, int* plist_size)
{
	size_t i;

	*plist = (enum mbfl_no_encoding *) php_mb_default_identify_list_neut;
	*plist_size = sizeof(php_mb_default_identify_list_neut) / sizeof(php_mb_default_identify_list_neut[0]);

	for (i = 0; i < sizeof(php_mb_default_identify_list) / sizeof(php_mb_default_identify_list[0]); i++) {
		if (php_mb_default_identify_list[i].lang == lang) {
			*plist = php_mb_default_identify_list[i].list;
			*plist_size = php_mb_default_identify_list[i].list_size;
			return 1;
		}
	}
	return 0;
}
/* }}} */

#if HAVE_MBREGEX
/* {{{ static void php_mbregex_free_cache() */
static void
php_mbregex_free_cache(mb_regex_t *pre) 
{
	mbre_free_pattern(pre);
}
/* }}} */
#endif

/* {{{ php.ini directive handler */
static PHP_INI_MH(OnUpdate_mbstring_language)
{
	enum mbfl_no_language no_language;

	no_language = mbfl_name2no_language(new_value);
	if (no_language == mbfl_no_language_invalid) {
		return FAILURE;
	}
	MBSTRG(language) = no_language;
	php_mb_nls_get_default_detect_order_list(no_language, &MBSTRG(default_detect_order_list), &MBSTRG(default_detect_order_list_size));
	return SUCCESS;
}
/* }}} */

/* {{{ static PHP_INI_MH(OnUpdate_mbstring_detect_order) */
static PHP_INI_MH(OnUpdate_mbstring_detect_order)
{
	enum mbfl_no_encoding *list;
	int size;

	if (php_mb_parse_encoding_list(new_value, new_value_length, &list, &size, 1 TSRMLS_CC)) {
		if (MBSTRG(detect_order_list) != NULL) {
			free(MBSTRG(detect_order_list));
		}
		MBSTRG(detect_order_list) = list;
		MBSTRG(detect_order_list_size) = size;
	} else {
		return FAILURE;
	}

	return SUCCESS;
}
/* }}} */

/* {{{ static PHP_INI_MH(OnUpdate_mbstring_http_input) */
static PHP_INI_MH(OnUpdate_mbstring_http_input)
{
	enum mbfl_no_encoding *list;
	int size;

	if (php_mb_parse_encoding_list(new_value, new_value_length, &list, &size, 1 TSRMLS_CC)) {
		if (MBSTRG(http_input_list) != NULL) {
			free(MBSTRG(http_input_list));
		}
		MBSTRG(http_input_list) = list;
		MBSTRG(http_input_list_size) = size;
	} else {
		return FAILURE;
	}

	return SUCCESS;
}
/* }}} */

/* {{{ static PHP_INI_MH(OnUpdate_mbstring_http_output) */
static PHP_INI_MH(OnUpdate_mbstring_http_output)
{
	enum mbfl_no_encoding no_encoding;

	no_encoding = mbfl_name2no_encoding(new_value);
	if (no_encoding != mbfl_no_encoding_invalid) {
		MBSTRG(http_output_encoding) = no_encoding;
		MBSTRG(current_http_output_encoding) = no_encoding;
	} else {
		if (new_value != NULL && new_value_length > 0) {
			return FAILURE;
		}
	}

	return SUCCESS;
}
/* }}} */

/* {{{ static PHP_INI_MH(OnUpdate_mbstring_internal_encoding) */
static PHP_INI_MH(OnUpdate_mbstring_internal_encoding)
{
	enum mbfl_no_encoding no_encoding;
#if HAVE_MBREGEX
	const struct def_mbctype_tbl *p = NULL;
#endif
	if (new_value == NULL) {
		return SUCCESS;
	}

	no_encoding = mbfl_name2no_encoding(new_value);
	if (no_encoding != mbfl_no_encoding_invalid) {
		MBSTRG(internal_encoding) = no_encoding;
		MBSTRG(current_internal_encoding) = no_encoding;
#if HAVE_MBREGEX
		p=&(mbctype_tbl[0]);
		while (p->regex_encoding >= 0){
			if (p->mbfl_encoding == MBSTRG(internal_encoding)){
				MBSTRG(default_mbctype) = p->regex_encoding;
				MBSTRG(current_mbctype) = p->regex_encoding;
				break;
			}
			p++;
		}
#endif
#ifdef ZEND_MULTIBYTE
		zend_multibyte_set_internal_encoding(new_value, new_value_length TSRMLS_CC);
#endif /* ZEND_MULTIBYTE */
	} else {
		if (new_value != NULL && new_value_length > 0) {
			return FAILURE;
		}
	}

	return SUCCESS;
}
/* }}} */

#ifdef ZEND_MULTIBYTE
/* {{{ static PHP_INI_MH(OnUpdate_mbstring_script_encoding) */
static PHP_INI_MH(OnUpdate_mbstring_script_encoding)
{
	int *list, size;

	if (php_mb_parse_encoding_list(new_value, new_value_length, &list, &size, 1 TSRMLS_CC)) {
		if (MBSTRG(script_encoding_list) != NULL) {
			free(MBSTRG(script_encoding_list));
		}
		MBSTRG(script_encoding_list) = list;
		MBSTRG(script_encoding_list_size) = size;
	} else {
		return FAILURE;
	}

	return SUCCESS;
}
/* }}} */
#endif /* ZEND_MULTIBYTE */

/* {{{ static PHP_INI_MH(OnUpdate_mbstring_substitute_character) */
static PHP_INI_MH(OnUpdate_mbstring_substitute_character)
{
	int c;
	char *endptr = NULL;

	if (new_value != NULL) {
		if (strcasecmp("none", new_value) == 0) {
			MBSTRG(filter_illegal_mode) = MBFL_OUTPUTFILTER_ILLEGAL_MODE_NONE;
		} else if (strcasecmp("long", new_value) == 0) {
			MBSTRG(filter_illegal_mode) = MBFL_OUTPUTFILTER_ILLEGAL_MODE_LONG;
		} else {
			MBSTRG(filter_illegal_mode) = MBFL_OUTPUTFILTER_ILLEGAL_MODE_CHAR;
			if (new_value_length >0) {
				c = strtol(new_value, &endptr, 0);
				if (*endptr == '\0') {
					MBSTRG(filter_illegal_substchar) = c;
				}
			}
		}
	}

	return SUCCESS;
}
/* }}} */

/* {{{ static PHP_INI_MH(OnUpdate_mbstring_encoding_translation) */
static PHP_INI_MH(OnUpdate_mbstring_encoding_translation)
{
	if (new_value == NULL) {
	   return FAILURE;
	}

	OnUpdateBool(entry, new_value, new_value_length, mh_arg1, mh_arg2, mh_arg3, stage TSRMLS_CC);

	if (MBSTRG(encoding_translation)){
		sapi_unregister_post_entry(php_post_entries);
		sapi_register_post_entries(mbstr_post_entries);
		sapi_register_treat_data(mbstr_treat_data);	   			   
	} else {
		sapi_unregister_post_entry(mbstr_post_entries);
		sapi_register_post_entries(php_post_entries);
		sapi_register_treat_data(php_default_treat_data);	   			   
	}

	return SUCCESS;
}
/* }}} */

/* {{{ php.ini directive registration */
PHP_INI_BEGIN()
	 PHP_INI_ENTRY("mbstring.language", "neutral", PHP_INI_SYSTEM | PHP_INI_PERDIR, OnUpdate_mbstring_language)
	 PHP_INI_ENTRY("mbstring.detect_order", NULL, PHP_INI_ALL, OnUpdate_mbstring_detect_order)
	 PHP_INI_ENTRY("mbstring.http_input", "pass", PHP_INI_ALL, OnUpdate_mbstring_http_input)
	 PHP_INI_ENTRY("mbstring.http_output", "pass", PHP_INI_ALL, OnUpdate_mbstring_http_output)
	 PHP_INI_ENTRY("mbstring.internal_encoding", NULL, PHP_INI_ALL, OnUpdate_mbstring_internal_encoding)
#ifdef ZEND_MULTIBYTE
	 PHP_INI_ENTRY("mbstring.script_encoding", NULL, PHP_INI_ALL, OnUpdate_mbstring_script_encoding)
#endif /* ZEND_MULTIBYTE */
	 PHP_INI_ENTRY("mbstring.substitute_character", NULL, PHP_INI_ALL, OnUpdate_mbstring_substitute_character)
	 STD_PHP_INI_ENTRY("mbstring.func_overload", "0", PHP_INI_SYSTEM |
	 PHP_INI_PERDIR, OnUpdateInt, func_overload, zend_mbstring_globals, mbstring_globals)
	 	 								  
	 STD_PHP_INI_BOOLEAN("mbstring.encoding_translation", "0",
	 PHP_INI_SYSTEM | PHP_INI_PERDIR, OnUpdate_mbstring_encoding_translation, 
	 encoding_translation, zend_mbstring_globals, mbstring_globals)					 
PHP_INI_END()
/* }}} */

/* {{{ module global initialize handler */
static void
php_mb_init_globals(zend_mbstring_globals *pglobals TSRMLS_DC)
{
	MBSTRG(language) = mbfl_no_language_uni;
	MBSTRG(current_language) = MBSTRG(language);
	MBSTRG(internal_encoding) = mbfl_no_encoding_invalid;
	MBSTRG(current_internal_encoding) = MBSTRG(internal_encoding);
#ifdef ZEND_MULTIBYTE
	MBSTRG(script_encoding_list) = NULL;
	MBSTRG(script_encoding_list_size) = 0;
#endif /* ZEND_MULTIBYTE */
	MBSTRG(http_output_encoding) = mbfl_no_encoding_pass;
	MBSTRG(current_http_output_encoding) = mbfl_no_encoding_pass;
	MBSTRG(http_input_identify) = mbfl_no_encoding_invalid;
	MBSTRG(http_input_identify_get) = mbfl_no_encoding_invalid;
	MBSTRG(http_input_identify_post) = mbfl_no_encoding_invalid;
	MBSTRG(http_input_identify_cookie) = mbfl_no_encoding_invalid;
	MBSTRG(http_input_identify_string) = mbfl_no_encoding_invalid;
	MBSTRG(http_input_list) = NULL;
	MBSTRG(http_input_list_size) = 0;
	MBSTRG(detect_order_list) = NULL;
	MBSTRG(detect_order_list_size) = 0;
	MBSTRG(current_detect_order_list) = NULL;
	MBSTRG(current_detect_order_list_size) = 0;
	MBSTRG(default_detect_order_list) = (enum mbfl_no_encoding *) php_mb_default_identify_list_neut;
	MBSTRG(default_detect_order_list_size) = sizeof(php_mb_default_identify_list_neut) / sizeof(php_mb_default_identify_list_neut[0]);
	MBSTRG(filter_illegal_mode) = MBFL_OUTPUTFILTER_ILLEGAL_MODE_CHAR;
	MBSTRG(filter_illegal_substchar) = 0x3f;	/* '?' */
	MBSTRG(current_filter_illegal_mode) = MBFL_OUTPUTFILTER_ILLEGAL_MODE_CHAR;
	MBSTRG(current_filter_illegal_substchar) = 0x3f;	/* '?' */
	MBSTRG(illegalchars) = 0;
	MBSTRG(func_overload) = 0;
	MBSTRG(encoding_translation) = 0;
	pglobals->outconv = NULL;
#if HAVE_MBREGEX
	MBSTRG(default_mbctype) = MBCTYPE_EUC;
	MBSTRG(current_mbctype) = MBCTYPE_EUC;
	zend_hash_init(&(MBSTRG(ht_rc)), 0, NULL, (void (*)(void *)) php_mbregex_free_cache, 1);
	MBSTRG(search_str) = (zval**)0;
	MBSTRG(search_str_val) = (zval*)0;
	MBSTRG(search_re) = (mb_regex_t*)0;
	MBSTRG(search_pos) = 0;
	MBSTRG(search_regs) = (struct mbre_registers*)0;
#endif
}
/* }}} */

/* {{{ static void mbstring_globals_dtor() */
static void
mbstring_globals_dtor(zend_mbstring_globals *pglobals TSRMLS_DC)
{
#if HAVE_MBREGEX
	zend_hash_destroy(&MBSTRG(ht_rc));
#endif
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION(mbstring) */
PHP_MINIT_FUNCTION(mbstring)
{
	__mbfl_allocators = &_php_mb_allocators;

#ifdef ZTS
	ts_allocate_id(&mbstring_globals_id, sizeof(zend_mbstring_globals),
		(ts_allocate_ctor) php_mb_init_globals,
		(ts_allocate_dtor) mbstring_globals_dtor);
#else
	php_mb_init_globals(&mbstring_globals TSRMLS_CC);
#endif

	REGISTER_INI_ENTRIES();


	if (MBSTRG(encoding_translation)) {
		sapi_unregister_post_entry(mbstr_post_entries);
		sapi_register_post_entries(mbstr_post_entries);
		sapi_register_treat_data(mbstr_treat_data);
	}

	REGISTER_LONG_CONSTANT("MB_OVERLOAD_MAIL", MB_OVERLOAD_MAIL, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MB_OVERLOAD_STRING", MB_OVERLOAD_STRING, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MB_OVERLOAD_REGEX", MB_OVERLOAD_REGEX, CONST_CS | CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("MB_CASE_UPPER", PHP_UNICODE_CASE_UPPER, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MB_CASE_LOWER", PHP_UNICODE_CASE_LOWER, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MB_CASE_TITLE", PHP_UNICODE_CASE_TITLE, CONST_CS | CONST_PERSISTENT);

#if HAVE_MBREGEX
# ifdef ZTS
	mbregex_locale_mutex = tsrm_mutex_alloc();
# endif
#endif
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION(mbstring) */
PHP_MSHUTDOWN_FUNCTION(mbstring)
{
	UNREGISTER_INI_ENTRIES();
	
	if (MBSTRG(http_input_list)) {
		free(MBSTRG(http_input_list));
	}
#ifdef ZEND_MULTIBYTE
	if (MBSTRG(script_encoding_list)) {
		free(MBSTRG(script_encoding_list));
	}
#endif /* ZEND_MULTIBYTE */
	if (MBSTRG(detect_order_list)) {
		free(MBSTRG(detect_order_list));
	}

	if (MBSTRG(encoding_translation)) {
		sapi_unregister_post_entry(mbstr_post_entries);
		sapi_register_post_entries(php_post_entries);
		sapi_register_treat_data(php_default_treat_data);
	}

#if HAVE_MBREGEX
# ifdef ZTS
	if (mbregex_locale_mutex != NULL) {
		tsrm_mutex_free(mbregex_locale_mutex);
	}
# endif
#endif

#ifdef ZTS
	ts_free_id(mbstring_globals_id);
#else
	mbstring_globals_dtor(&mbstring_globals TSRMLS_CC);
#endif

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RINIT_FUNCTION(mbstring) */
PHP_RINIT_FUNCTION(mbstring)
{
	int n;
	enum mbfl_no_encoding *list=NULL, *entry;
	zend_function *func, *orig;
	const struct mb_overload_def *p;

	MBSTRG(current_language) = MBSTRG(language);

	if (MBSTRG(internal_encoding) == mbfl_no_encoding_invalid) {
		char *default_enc = NULL;
		switch (MBSTRG(current_language)) {
			case mbfl_no_language_uni:
				default_enc = "UTF-8";
				break;
			case mbfl_no_language_japanese:
				default_enc = "EUC-JP";
				break;
			case mbfl_no_language_korean:
				default_enc = "EUC-KR";
				break;
			case mbfl_no_language_simplified_chinese:
				default_enc = "EUC-CN";
				break;
			case mbfl_no_language_traditional_chinese:
				default_enc = "EUC-TW";
				break;
			case mbfl_no_language_russian:
				default_enc = "KOI8-R";
				break;
			case mbfl_no_language_german:
				default_enc = "ISO-8859-15";
				break;
			case mbfl_no_language_english:
			default:
				default_enc = "ISO-8859-1";
				break;
		}
		if (default_enc) {
			zend_alter_ini_entry("mbstring.internal_encoding",
			                     sizeof("mbstring.internal_encoding"),
			                     default_enc, strlen(default_enc),
			                     PHP_INI_PERDIR, PHP_INI_STAGE_RUNTIME); 
		}
	}

	MBSTRG(current_internal_encoding) = MBSTRG(internal_encoding);
	MBSTRG(current_http_output_encoding) = MBSTRG(http_output_encoding);
	MBSTRG(current_filter_illegal_mode) = MBSTRG(filter_illegal_mode);
	MBSTRG(current_filter_illegal_substchar) = MBSTRG(filter_illegal_substchar);

	if (!MBSTRG(encoding_translation)) {
		MBSTRG(illegalchars) = 0;
	}

	n = 0;
	if (MBSTRG(detect_order_list)) {
		list = MBSTRG(detect_order_list);
		n = MBSTRG(detect_order_list_size);
	}
	if (n <= 0) {
		list = MBSTRG(default_detect_order_list);
		n = MBSTRG(default_detect_order_list_size);
	}
	entry = (enum mbfl_no_encoding *)safe_emalloc(n, sizeof(int), 0);
	MBSTRG(current_detect_order_list) = entry;
	MBSTRG(current_detect_order_list_size) = n;
	while (n > 0) {
		*entry++ = *list++;
		n--;
	}

 	/* override original function. */
	if (MBSTRG(func_overload)){
		p = &(mb_ovld[0]);
		
		while (p->type > 0) {
			if ((MBSTRG(func_overload) & p->type) == p->type && 
				zend_hash_find(EG(function_table), p->save_func,
					strlen(p->save_func)+1, (void **)&orig) != SUCCESS) {

				zend_hash_find(EG(function_table), p->ovld_func, strlen(p->ovld_func)+1 , (void **)&func);
				
				if (zend_hash_find(EG(function_table), p->orig_func, strlen(p->orig_func)+1, (void **)&orig) != SUCCESS) {
					php_error_docref("ref.mbstring" TSRMLS_CC, E_WARNING, "mbstring couldn't find function %s.", p->orig_func);
					return FAILURE;
				} else {
					zend_hash_add(EG(function_table), p->save_func, strlen(p->save_func)+1, orig, sizeof(zend_function), NULL);

					if (zend_hash_update(EG(function_table), p->orig_func, strlen(p->orig_func)+1, func, sizeof(zend_function), 
						NULL) == FAILURE) {
						php_error_docref("ref.mbstring" TSRMLS_CC, E_WARNING, "mbstring couldn't replace function %s.", p->orig_func);
						return FAILURE;
					}
				}
			}
			p++;
		}
	}
#if HAVE_MBREGEX
	MBSTRG(regex_default_options) = MBRE_OPTION_POSIXLINE;
#endif
#if defined(ZEND_MULTIBYTE) && defined(HAVE_MBSTRING)
	php_mb_set_zend_encoding(TSRMLS_C);
#endif /* ZEND_MULTIBYTE && HAVE_MBSTRING */
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION(mbstring) */
PHP_RSHUTDOWN_FUNCTION(mbstring)
{
	const struct mb_overload_def *p;
	zend_function *orig;

	if (MBSTRG(current_detect_order_list) != NULL) {
		efree(MBSTRG(current_detect_order_list));
		MBSTRG(current_detect_order_list) = NULL;
		MBSTRG(current_detect_order_list_size) = 0;
	}
	if (MBSTRG(outconv) != NULL) {
		MBSTRG(illegalchars) += mbfl_buffer_illegalchars(MBSTRG(outconv));
		mbfl_buffer_converter_delete(MBSTRG(outconv));
		MBSTRG(outconv) = NULL;
	}

	/* clear http input identification. */
	MBSTRG(http_input_identify) = mbfl_no_encoding_invalid;
	MBSTRG(http_input_identify_post) = mbfl_no_encoding_invalid;
	MBSTRG(http_input_identify_get) = mbfl_no_encoding_invalid;
	MBSTRG(http_input_identify_cookie) = mbfl_no_encoding_invalid;
	MBSTRG(http_input_identify_string) = mbfl_no_encoding_invalid;

 	/*  clear overloaded function. */
	if (MBSTRG(func_overload)){
		p = &(mb_ovld[0]);
		while (p->type > 0) {
			if ((MBSTRG(func_overload) & p->type) == p->type && 
				zend_hash_find(EG(function_table), p->save_func,
							   strlen(p->save_func)+1, (void **)&orig) == SUCCESS) {
				
				zend_hash_update(EG(function_table), p->orig_func, strlen(p->orig_func)+1, orig, sizeof(zend_function), NULL);
				zend_hash_del(EG(function_table), p->save_func, strlen(p->save_func)+1);
			}
			p++;
		}
	}

#if HAVE_MBREGEX
	MBSTRG(current_mbctype) = MBSTRG(default_mbctype);
	if (MBSTRG(search_str)) {
		if (ZVAL_REFCOUNT(*MBSTRG(search_str)) > 1) {
			ZVAL_DELREF(*MBSTRG(search_str));
		} else {
			zval_dtor(*MBSTRG(search_str));
			FREE_ZVAL(*MBSTRG(search_str));
		}
		MBSTRG(search_str) = (zval **)0;
		MBSTRG(search_str_val) = (zval *)0;
	}
	MBSTRG(search_pos) = 0;
	if (MBSTRG(search_re)) {
		efree(MBSTRG(search_re));
		MBSTRG(search_re) = (mb_regex_t *)0;
	}
	if (MBSTRG(search_regs)) {
		mbre_free_registers(MBSTRG(search_regs));
		efree(MBSTRG(search_regs));
		MBSTRG(search_regs) = (struct mbre_registers*)0;
	}
	zend_hash_clean(&MBSTRG(ht_rc));
#endif

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION(mbstring) */
PHP_MINFO_FUNCTION(mbstring)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "Multibyte Support", "enabled");
	php_info_print_table_row(2, "Japanese support", "enabled");	
	php_info_print_table_row(2, "Simplified chinese support", "enabled");	
	php_info_print_table_row(2, "Traditional chinese support", "enabled");	
	php_info_print_table_row(2, "Korean support", "enabled");	
	php_info_print_table_row(2, "Russian support", "enabled");	

	if (MBSTRG(encoding_translation)) {
		php_info_print_table_row(2, "HTTP input encoding translation", "enabled");	
	}
#if defined(HAVE_MBREGEX)
	php_info_print_table_row(2, "Multibyte (japanese) regex support", "enabled");	
#endif
	php_info_print_table_end();

	php_info_print_table_start();
	php_info_print_table_colspan_header(2, "mbstring extension makes use of \"streamable kanji code filter and converter\", which is distributed under the GNU Lesser General Public License version 2.1.");
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}
/* }}} */

/* {{{ proto string mb_language([string language])
   Sets the current language or Returns the current language as a string */
PHP_FUNCTION(mb_language)
{
	pval **arg1;
	char *name;
	enum mbfl_no_language no_language;

	if (ZEND_NUM_ARGS() == 0) {
		name = (char *)mbfl_no_language2name(MBSTRG(current_language));
		if (name != NULL) {
			RETURN_STRING(name, 1);
		} else {
			RETURN_FALSE;
		}
	} else if (ZEND_NUM_ARGS() == 1 && zend_get_parameters_ex(1, &arg1) != FAILURE) {
		convert_to_string_ex(arg1);
		no_language = mbfl_name2no_language(Z_STRVAL_PP(arg1));
		if (no_language == mbfl_no_language_invalid) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown language \"%s\"", Z_STRVAL_PP(arg1));
			RETURN_FALSE;
		} else {
			php_mb_nls_get_default_detect_order_list(no_language, &MBSTRG(default_detect_order_list), &MBSTRG(default_detect_order_list_size));
			MBSTRG(current_language) = no_language;
			RETURN_TRUE;
		}
	} else {
		WRONG_PARAM_COUNT;
	}
}
/* }}} */

/* {{{ proto string mb_internal_encoding([string encoding])
   Sets the current internal encoding or Returns the current internal encoding as a string */
PHP_FUNCTION(mb_internal_encoding)
{
	pval **arg1;
	char *name;
	enum mbfl_no_encoding no_encoding;

	if (ZEND_NUM_ARGS() == 0) {
		name = (char *)mbfl_no_encoding2name(MBSTRG(current_internal_encoding));
		if (name != NULL) {
			RETURN_STRING(name, 1);
		} else {
			RETURN_FALSE;
		}
	} else if (ZEND_NUM_ARGS() == 1 && zend_get_parameters_ex(1, &arg1) != FAILURE) {
		convert_to_string_ex(arg1);
		no_encoding = mbfl_name2no_encoding(Z_STRVAL_PP(arg1));
		if (no_encoding == mbfl_no_encoding_invalid) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown encoding \"%s\"", Z_STRVAL_PP(arg1));
			RETURN_FALSE;
		} else {
			MBSTRG(current_internal_encoding) = no_encoding;
#ifdef ZEND_MULTIBYTE
			zend_multibyte_set_internal_encoding(Z_STRVAL_PP(arg1), Z_STRLEN_PP(arg1) TSRMLS_CC);
#endif /* ZEND_MULTIBYTE */
			RETURN_TRUE;
		}
	} else {
		WRONG_PARAM_COUNT;
	}
}
/* }}} */

/* {{{ proto mixed mb_http_input([string type])
   Returns the input encoding */
PHP_FUNCTION(mb_http_input)
{
	pval **arg1;
	int result=0, retname, n;
	enum mbfl_no_encoding *entry;
	char *name, *list, *temp;

	retname = 1;
	if (ZEND_NUM_ARGS() == 0) {
		result = MBSTRG(http_input_identify);
	} else if (ARG_COUNT(ht) == 1 && zend_get_parameters_ex(1, &arg1) != FAILURE) {
		convert_to_string_ex(arg1);
		switch (*(Z_STRVAL_PP(arg1))) {
		case 'G':
		case 'g':
			result = MBSTRG(http_input_identify_get);
			break;
		case 'P':
		case 'p':
			result = MBSTRG(http_input_identify_post);
			break;
		case 'C':
		case 'c':
			result = MBSTRG(http_input_identify_cookie);
			break;
		case 'S':
		case 's':
			result = MBSTRG(http_input_identify_string);
			break;
		case 'I':
		case 'i':
			if (array_init(return_value) == FAILURE) {
				RETURN_FALSE;
			}
			entry = MBSTRG(http_input_list);
			n = MBSTRG(http_input_list_size);
			while (n > 0) {
				name = (char *)mbfl_no_encoding2name(*entry);
				if (name) {
					add_next_index_string(return_value, name, 1);
				}
				entry++;
				n--;
			}
			retname = 0;
			break;
		case 'L':
		case 'l':
			entry = MBSTRG(http_input_list);
			n = MBSTRG(http_input_list_size);
			list = NULL;
			while (n > 0) {
				name = (char *)mbfl_no_encoding2name(*entry);
				if (name) {
					if (list) {
						temp = list;
						spprintf(&list, 0, "%s,%s", temp, name);
						efree(temp);
						if (!list) { 
							break;
						}
					} else {
						list = estrdup(name);
					}
				}
				entry++;
				n--;
			}
			if (!list) {
				RETURN_FALSE;
			}
			RETVAL_STRING(list, 0);
			retname = 0;
			break;
		default:
			result = MBSTRG(http_input_identify);
			break;
		}
	} else {
		WRONG_PARAM_COUNT;
	}

	if (retname) {
		name = (char *)mbfl_no_encoding2name(result);
		if (name != NULL) {
			RETVAL_STRING(name, 1);
		} else {
			RETVAL_FALSE;
		}
	}
}
/* }}} */

/* {{{ proto string mb_http_output([string encoding])
   Sets the current output_encoding or returns the current output_encoding as a string */
PHP_FUNCTION(mb_http_output)
{
	pval **arg1;
	char *name;
	enum mbfl_no_encoding no_encoding;

	if (ZEND_NUM_ARGS() == 0) {
		name = (char *)mbfl_no_encoding2name(MBSTRG(current_http_output_encoding));
		if (name != NULL) {
			RETURN_STRING(name, 1);
		} else {
			RETURN_FALSE;
		}
	} else if (ZEND_NUM_ARGS() == 1 && zend_get_parameters_ex(1, &arg1) != FAILURE) {
		convert_to_string_ex(arg1);
		no_encoding = mbfl_name2no_encoding(Z_STRVAL_PP(arg1));
		if (no_encoding == mbfl_no_encoding_invalid) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown encoding \"%s\"", Z_STRVAL_PP(arg1));
			RETURN_FALSE;
		} else {
			MBSTRG(current_http_output_encoding) = no_encoding;
			RETURN_TRUE;
		}
	} else {
		WRONG_PARAM_COUNT;
	}
}
/* }}} */

/* {{{ proto bool|array mb_detect_order([mixed encoding-list])
   Sets the current detect_order or Return the current detect_order as a array */
PHP_FUNCTION(mb_detect_order)
{
	pval **arg1;
	int n, size;
	enum mbfl_no_encoding *list, *entry;
	char *name;

	if (ZEND_NUM_ARGS() == 0) {
		if (array_init(return_value) == FAILURE) {
			RETURN_FALSE;
		}
		entry = MBSTRG(current_detect_order_list);
		n = MBSTRG(current_detect_order_list_size);
		while (n > 0) {
			name = (char *)mbfl_no_encoding2name(*entry);
			if (name) {
				add_next_index_string(return_value, name, 1);
			}
			entry++;
			n--;
		}
	} else if (ZEND_NUM_ARGS() == 1 && zend_get_parameters_ex(1, &arg1) != FAILURE) {
		list = NULL;
		size = 0;
		switch (Z_TYPE_PP(arg1)) {
		case IS_ARRAY:
			if (!php_mb_parse_encoding_array(*arg1, &list, &size, 0 TSRMLS_CC)) {
				if (list) {
					efree(list);
				}
				RETURN_FALSE;
			}
			break;
		default:
			convert_to_string_ex(arg1);
			if (!php_mb_parse_encoding_list(Z_STRVAL_PP(arg1), Z_STRLEN_PP(arg1), &list, &size, 0 TSRMLS_CC)) {
				if (list) {
					efree(list);
				}
				RETURN_FALSE;
			}
			break;
		}
		if (list == NULL) {
			RETVAL_FALSE;
		} else {
			if (MBSTRG(current_detect_order_list)) {
				efree(MBSTRG(current_detect_order_list));
			}
			MBSTRG(current_detect_order_list) = list;
			MBSTRG(current_detect_order_list_size) = size;
			RETVAL_TRUE;
		}
	} else {
		WRONG_PARAM_COUNT;
	}
}
/* }}} */

/* {{{ proto mixed mb_substitute_character([mixed substchar])
   Sets the current substitute_character or returns the current substitute_character */
PHP_FUNCTION(mb_substitute_character)
{
	pval **arg1;

	if (ZEND_NUM_ARGS() == 0) {
		if (MBSTRG(current_filter_illegal_mode) == MBFL_OUTPUTFILTER_ILLEGAL_MODE_NONE) {
			RETVAL_STRING("none", 1);
		} else if (MBSTRG(current_filter_illegal_mode) == MBFL_OUTPUTFILTER_ILLEGAL_MODE_LONG) {
			RETVAL_STRING("long", 1);
		} else {
			RETVAL_LONG(MBSTRG(current_filter_illegal_substchar));
		}
	} else if (ZEND_NUM_ARGS() == 1 && zend_get_parameters_ex(1, &arg1) != FAILURE) {
		RETVAL_TRUE;
		switch (Z_TYPE_PP(arg1)) {
		case IS_STRING:
			if (strcasecmp("none", Z_STRVAL_PP(arg1)) == 0) {
				MBSTRG(current_filter_illegal_mode) = MBFL_OUTPUTFILTER_ILLEGAL_MODE_NONE;
			} else if (strcasecmp("long", Z_STRVAL_PP(arg1)) == 0) {
				MBSTRG(current_filter_illegal_mode) = MBFL_OUTPUTFILTER_ILLEGAL_MODE_LONG;
			} else {
				convert_to_long_ex(arg1);
				if (Z_LVAL_PP(arg1)< 0xffff && Z_LVAL_PP(arg1)> 0x0) {
					MBSTRG(current_filter_illegal_mode) = MBFL_OUTPUTFILTER_ILLEGAL_MODE_CHAR;
					MBSTRG(current_filter_illegal_substchar) = Z_LVAL_PP(arg1);
				} else {
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown character.");					  
					RETVAL_FALSE;					
				}
			}
			break;
		default:
			convert_to_long_ex(arg1);
			if (Z_LVAL_PP(arg1)< 0xffff && Z_LVAL_PP(arg1)> 0x0) {
				MBSTRG(current_filter_illegal_mode) = MBFL_OUTPUTFILTER_ILLEGAL_MODE_CHAR;
				MBSTRG(current_filter_illegal_substchar) = Z_LVAL_PP(arg1);
			} else {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown character.");
				RETVAL_FALSE;
			}
			break;
		}
	} else {
		WRONG_PARAM_COUNT;
	}
}
/* }}} */

/* {{{ proto string mb_preferred_mime_name(string encoding)
   Return the preferred MIME name (charset) as a string */
PHP_FUNCTION(mb_preferred_mime_name)
{
	pval **arg1;
	enum mbfl_no_encoding no_encoding;
	const char *name;

	if (ZEND_NUM_ARGS() == 1 && zend_get_parameters_ex(1, &arg1) != FAILURE) {
		convert_to_string_ex(arg1);
		no_encoding = mbfl_name2no_encoding(Z_STRVAL_PP(arg1));
		if (no_encoding == mbfl_no_encoding_invalid) {
			php_error_docref1(NULL TSRMLS_CC, Z_STRVAL_PP(arg1), E_WARNING, "Unknown encoding");
			RETVAL_FALSE;
		} else {
			name = mbfl_no2preferred_mime_name(no_encoding);
			if (name == NULL || *name == '\0') {
				php_error_docref1(NULL TSRMLS_CC, Z_STRVAL_PP(arg1), E_WARNING, "No MIME preferred name corresponding to that encoding");
				RETVAL_FALSE;
			} else {
				RETVAL_STRING((char *)name, 1);
			}
		}
	} else {
		WRONG_PARAM_COUNT;
	}
}
/* }}} */

/* {{{ static void php_mbstr_encoding_handler() */
static enum mbfl_no_encoding
php_mbstr_encoding_handler(const php_mb_encoding_handler_info_t *info, zval *arg, char *res TSRMLS_DC)
{
	char *var, *val;
	const char *s1, *s2;
	char *strtok_buf = NULL, **val_list;
	zval *array_ptr = (zval *) arg;
	int n, num, val_len, *len_list;
	enum mbfl_no_encoding from_encoding;
	mbfl_string string, resvar, resval;
	mbfl_encoding_detector *identd = NULL; 
	mbfl_buffer_converter *convd = NULL;

	mbfl_string_init_set(&string, info->to_language, info->to_encoding);
	mbfl_string_init_set(&resvar, info->to_language, info->to_encoding);
	mbfl_string_init_set(&resval, info->to_language, info->to_encoding);

	if (!res || *res == '\0') {
		return mbfl_no_encoding_invalid;
	}
	
	/* count the variables(separators) contained in the "res".
	 * separator may contain multiple separator chars.
	 * separaror chars are set in php.ini (arg_separator.input)
	 */
	num = 1;
	for (s1=res; *s1 != '\0'; s1++) {
		for (s2=info->separator; *s2 != '\0'; s2++) {
			if (*s1 == *s2) {
				num++;
			}	
		}
	}
	num *= 2; /* need space for variable name and value */
	
	val_list = (char **)ecalloc(num, sizeof(char *));
	len_list = (int *)ecalloc(num, sizeof(int));

	/* split and decode the query */
	n = 0;
	strtok_buf = NULL;
	var = php_strtok_r(res, info->separator, &strtok_buf);
	while (var)  {
		val = strchr(var, '=');
		if (val) { /* have a value */
			len_list[n] = php_url_decode(var, val-var);
			val_list[n] = var;
			n++;
			
			*val++ = '\0';
			val_list[n] = val;
			len_list[n] = php_url_decode(val, strlen(val));
		} else {
			len_list[n] = php_url_decode(var, strlen(var));
			val_list[n] = var;
			n++;
			
			val_list[n] = "";
			len_list[n] = 0;
		}
		n++;
		var = php_strtok_r(NULL, info->separator, &strtok_buf);
	} 
	num = n; /* make sure to process initilized vars only */
	
	/* initialize converter */
	if (info->num_from_encodings <= 0) {
		from_encoding = mbfl_no_encoding_pass;
	} else if (info->num_from_encodings == 1) {
		from_encoding = info->from_encodings[0];
	} else {
		/* auto detect */
		from_encoding = mbfl_no_encoding_invalid;
		identd = mbfl_encoding_detector_new(
				(enum mbfl_no_encoding *)info->from_encodings,
				info->num_from_encodings, 0);
		if (identd) {
			n = 0;
			while (n < num) {
				string.val = (unsigned char *)val_list[n];
				string.len = len_list[n];
				if (mbfl_encoding_detector_feed(identd, &string)) {
					break;
				}
				n++;
			}
			from_encoding = mbfl_encoding_detector_judge(identd);
			mbfl_encoding_detector_delete(identd);
		}
		if (from_encoding == mbfl_no_encoding_invalid) {
			from_encoding = mbfl_no_encoding_pass;
		}
	}
	convd = NULL;
	if (from_encoding != mbfl_no_encoding_pass) {
		convd = mbfl_buffer_converter_new(from_encoding, info->to_encoding, 0);
		if (convd != NULL) {
			mbfl_buffer_converter_illegal_mode(convd, MBSTRG(current_filter_illegal_mode));
			mbfl_buffer_converter_illegal_substchar(convd, MBSTRG(current_filter_illegal_substchar));
		} else {
			if (info->report_errors) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING,
						"Unable to create converter");
			}
		}
	}

	/* convert encoding */
	string.no_encoding = from_encoding;

	n = 0;
	while (n < num) {
		string.val = (unsigned char *)val_list[n];
		string.len = len_list[n];
		if (convd != NULL && mbfl_buffer_converter_feed_result(convd, &string, &resvar) != NULL) {
			var = (char *)resvar.val;
		} else {
			var = val_list[n];
		}
		n++;
		string.val = val_list[n];
		string.len = len_list[n];
		if (convd != NULL && mbfl_buffer_converter_feed_result(convd, &string, &resval) != NULL) {
			val = resval.val;
			val_len = resval.len;
		} else {
			val = val_list[n];
			val_len = len_list[n];
		}
		n++;
		/* add variable to symbol table */
		php_register_variable_safe(var, val, val_len, array_ptr TSRMLS_CC);
		if (convd != NULL){
			mbfl_string_clear(&resvar);
			mbfl_string_clear(&resval);
		}
	}

	if (convd != NULL) {
		MBSTRG(illegalchars) += mbfl_buffer_illegalchars(convd);
		mbfl_buffer_converter_delete(convd);
	}
	if (val_list != NULL) {
		efree((void *)val_list);
	}
	if (len_list != NULL) {
		efree((void *)len_list);
	}

	return from_encoding;
}
/* }}} */

/* {{{ SAPI_POST_HANDLER_FUNC(php_mbstr_post_handler) */
SAPI_POST_HANDLER_FUNC(php_mbstr_post_handler)
{
	enum mbfl_no_encoding detected;
	php_mb_encoding_handler_info_t info;

	MBSTRG(http_input_identify_post) = mbfl_no_encoding_invalid;

	info.data_type              = PARSE_POST;
	info.separator              = "&";
	info.report_errors          = 0;
	info.to_encoding            = MBSTRG(internal_encoding);
	info.to_language            = MBSTRG(language);
	info.from_encodings         = MBSTRG(http_input_list);
	info.num_from_encodings     = MBSTRG(http_input_list_size); 
	info.from_language          = MBSTRG(language);

	detected = php_mbstr_encoding_handler(&info, arg, SG(request_info).post_data TSRMLS_CC);

	MBSTRG(http_input_identify) = detected;
	if (detected != mbfl_no_encoding_invalid) {
		MBSTRG(http_input_identify_post) = detected;
	}
}
/* }}} */

#define IS_SJIS1(c) ((((c)>=0x81 && (c)<=0x9f) || ((c)>=0xe0 && (c)<=0xf5)) ? 1 : 0)
#define IS_SJIS2(c) ((((c)>=0x40 && (c)<=0x7e) || ((c)>=0x80 && (c)<=0xfc)) ? 1 : 0)

/* {{{ MBSTRING_API SAPI_TREAT_DATA_FUNC(mbstr_treat_data)
 * http input processing */
MBSTRING_API SAPI_TREAT_DATA_FUNC(mbstr_treat_data)
{
	char *res = NULL, *separator=NULL;
	const char *c_var;
	pval *array_ptr;
	int free_buffer=0;
	enum mbfl_no_encoding detected;
	php_mb_encoding_handler_info_t info;

	switch (arg) {
		case PARSE_POST:
		case PARSE_GET:
		case PARSE_COOKIE:
			ALLOC_ZVAL(array_ptr);
			array_init(array_ptr);
			INIT_PZVAL(array_ptr);
			switch (arg) {
				case PARSE_POST:
					PG(http_globals)[TRACK_VARS_POST] = array_ptr;
					break;
				case PARSE_GET:
					PG(http_globals)[TRACK_VARS_GET] = array_ptr;
					break;
				case PARSE_COOKIE:
					PG(http_globals)[TRACK_VARS_COOKIE] = array_ptr;
					break;
			}
			break;
		default:
			array_ptr=destArray;
			break;
	}

	if (arg==PARSE_POST) { 
		sapi_handle_post(array_ptr TSRMLS_CC);
		return;
	}

	if (arg == PARSE_GET) {		/* GET data */
		c_var = SG(request_info).query_string;
		if (c_var && *c_var) {
			res = (char *) estrdup(c_var);
			free_buffer = 1;
		} else {
			free_buffer = 0;
		}
	} else if (arg == PARSE_COOKIE) {		/* Cookie data */
		c_var = SG(request_info).cookie_data;
		if (c_var && *c_var) {
			res = (char *) estrdup(c_var);
			free_buffer = 1;
		} else {
			free_buffer = 0;
		}
	} else if (arg == PARSE_STRING) {		/* String data */
		res = str;
		free_buffer = 1;
	}

	if (!res) {
		return;
	}

	switch (arg) {
	case PARSE_POST:
	case PARSE_GET:
	case PARSE_STRING:
		separator = (char *) estrdup(PG(arg_separator).input);
		break;
	case PARSE_COOKIE:
		separator = ";\0";
		break;
	}
	
	switch(arg) {
	case PARSE_POST:
		MBSTRG(http_input_identify_post) = mbfl_no_encoding_invalid;
		break;
	case PARSE_GET:
		MBSTRG(http_input_identify_get) = mbfl_no_encoding_invalid;
		break;
	case PARSE_COOKIE:
		MBSTRG(http_input_identify_cookie) = mbfl_no_encoding_invalid;
		break;
	case PARSE_STRING:
		MBSTRG(http_input_identify_string) = mbfl_no_encoding_invalid;
		break;
	}

	info.data_type              = arg;
	info.separator              = separator; 
	info.report_errors          = 0;
	info.to_encoding            = MBSTRG(internal_encoding);
	info.to_language            = MBSTRG(language);
	info.from_encodings         = MBSTRG(http_input_list);
	info.num_from_encodings     = MBSTRG(http_input_list_size); 
	info.from_language          = MBSTRG(language);

	MBSTRG(illegalchars) = 0;

	detected = php_mbstr_encoding_handler(&info, array_ptr, res TSRMLS_CC);
	MBSTRG(http_input_identify) = detected;

	if (detected != mbfl_no_encoding_invalid) {
		switch(arg){
		case PARSE_POST:
			MBSTRG(http_input_identify_post) = detected;
			break;
		case PARSE_GET:
			MBSTRG(http_input_identify_get) = detected;
			break;
		case PARSE_COOKIE:
			MBSTRG(http_input_identify_cookie) = detected;
			break;
		case PARSE_STRING:
			MBSTRG(http_input_identify_string) = detected;
			break;
		}
	}

	if (arg != PARSE_COOKIE) {
		efree(separator);
	}

	if (free_buffer) {
		efree(res);
	}
}
/* }}} */

/* {{{ proto bool mb_parse_str(string encoded_string [, array result])
   Parses GET/POST/COOKIE data and sets global variables */
PHP_FUNCTION(mb_parse_str)
{
	pval **arg_str, **arg_array, *track_vars_array;
	char *var, *val, *encstr, *strtok_buf, **str_list, *separator;
	int n, num, val_len, *len_list, elistsz, old_rg, argc;
	enum mbfl_no_encoding *elist;
	enum mbfl_no_encoding from_encoding, to_encoding;
	mbfl_string string, resvar, resval;
	mbfl_encoding_detector *identd;
	mbfl_buffer_converter *convd;

	track_vars_array = NULL;
	argc = ZEND_NUM_ARGS();
	if (argc == 1) {
		if (zend_get_parameters_ex(1, &arg_str) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
	} else if (argc == 2) {
		if (zend_get_parameters_ex(2, &arg_str, &arg_array) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
		/* Clear out the array */
		zval_dtor(*arg_array);
		array_init(*arg_array);
		track_vars_array = *arg_array;
	} else {
		WRONG_PARAM_COUNT;
	}
	separator = (char *)estrdup(PG(arg_separator).input);
	if (separator == NULL) {
		RETURN_FALSE;
	}
	convert_to_string_ex(arg_str);
	encstr = estrndup(Z_STRVAL_PP(arg_str), Z_STRLEN_PP(arg_str));
	if (encstr == NULL) {
		efree((void *)separator);
		RETURN_FALSE;
	}
	mbfl_string_init_set(&string, MBSTRG(current_language), MBSTRG(current_internal_encoding));
	mbfl_string_init_set(&resvar, MBSTRG(current_language), MBSTRG(current_internal_encoding));
	mbfl_string_init_set(&resval, MBSTRG(current_language), MBSTRG(current_internal_encoding));

	/* count the variables contained in the query */
	num = 1;
	var = encstr;
	n = Z_STRLEN_PP(arg_str);
	while (n > 0) {
		char *p;
		for (p = separator; *p != '\0'; ++p) {
			if (*p == *var) {
				num++;
			}
		}
		var++;
		n--;
	}
	num *= 2;
	str_list = (char **)ecalloc(num, sizeof(char *));
	if (str_list == NULL) {
		efree((void *)separator);
		efree((void *)encstr);
		RETURN_FALSE;
	}
	len_list = (int *)ecalloc(num, sizeof(int));
	if (len_list == NULL) {
		efree((void *)separator);
		efree((void *)encstr);
		efree((void *)str_list);
		RETURN_FALSE;
	}

	/* split and decode the query */
	n = 0;
	strtok_buf = NULL;
	var = php_strtok_r(encstr, separator, &strtok_buf);
	while (var && n < num) {
		val = strchr(var, '=');
		if (val) { /* have a value */
			len_list[n] = php_url_decode(var, val-var);
			str_list[n] = var;
			n++;

			*val++ = '\0';
			str_list[n] = val;
			len_list[n] = php_url_decode(val, strlen(val));
		} else {
			len_list[n] = php_url_decode(var, strlen(var));
			str_list[n] = var;
			n++;

			str_list[n] = "";
			len_list[n] = 0;
		}
		n++;
		var = php_strtok_r(NULL, separator, &strtok_buf);
	}
	num = n;

	/* initialize converter */
	to_encoding = MBSTRG(current_internal_encoding);
	elist = MBSTRG(http_input_list);
	elistsz = MBSTRG(http_input_list_size);
	if (elistsz <= 0) {
		from_encoding = mbfl_no_encoding_pass;
	} else if (elistsz == 1) {
		from_encoding = *elist;
	} else {
		/* auto detect */
		from_encoding = mbfl_no_encoding_invalid;
		identd = mbfl_encoding_detector_new(elist, elistsz, 0);
		if (identd != NULL) {
			n = 0;
			while (n < num) {
				string.val = (unsigned char *)str_list[n];
				string.len = len_list[n];
				if (mbfl_encoding_detector_feed(identd, &string)) {
					break;
				}
				n++;
			}
			from_encoding = mbfl_encoding_detector_judge(identd);
			mbfl_encoding_detector_delete(identd);
		}
		if (from_encoding == mbfl_no_encoding_invalid) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to detect encoding");
			from_encoding = mbfl_no_encoding_pass;
		}
	}
	convd = NULL;
	if (from_encoding != mbfl_no_encoding_pass) {
		convd = mbfl_buffer_converter_new(from_encoding, to_encoding, 0);
		if (convd != NULL) {
			mbfl_buffer_converter_illegal_mode(convd, MBSTRG(current_filter_illegal_mode));
			mbfl_buffer_converter_illegal_substchar(convd, MBSTRG(current_filter_illegal_substchar));
		} else {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to create converter");
		}
	}

	/* convert encoding */
	string.no_encoding = from_encoding;
	old_rg = PG(register_globals);
	if (argc == 1) {
		zend_alter_ini_entry("register_globals", sizeof("register_globals"), "1", sizeof("1")-1, PHP_INI_PERDIR, PHP_INI_STAGE_RUNTIME);
	} else {
		zend_alter_ini_entry("register_globals", sizeof("register_globals"), "0", sizeof("0")-1, PHP_INI_PERDIR, PHP_INI_STAGE_RUNTIME);
	}
	n = 0;
	while (n < num) {
		/* convert variable name */
		string.val = str_list[n];
		string.len = len_list[n];
		if (convd != NULL && mbfl_buffer_converter_feed_result(convd, &string, &resvar) != NULL) {
			var = (char *)resvar.val;
		} else {
			var = str_list[n];
		}
		n++;
		/* convert value */
		string.val = str_list[n];
		string.len = len_list[n];
		if (convd != NULL && mbfl_buffer_converter_feed_result(convd, &string, &resval) != NULL) {
			val = resval.val;
			val_len = resval.len;
		} else {
			val = str_list[n];
			val_len = len_list[n];
		}
		n++;
		/* add variable to symbol table */
		php_register_variable_safe(var, val, val_len, track_vars_array TSRMLS_CC);
		mbfl_string_clear(&resvar);
		mbfl_string_clear(&resval);
	}
	if (old_rg) {
		zend_alter_ini_entry("register_globals", sizeof("register_globals"), "1", sizeof("1")-1, PHP_INI_PERDIR, PHP_INI_STAGE_RUNTIME);
	} else {
		zend_alter_ini_entry("register_globals", sizeof("register_globals"), "0", sizeof("0")-1, PHP_INI_PERDIR, PHP_INI_STAGE_RUNTIME);
	}

	if (convd != NULL) {
		MBSTRG(illegalchars) += mbfl_buffer_illegalchars(convd);
		mbfl_buffer_converter_delete(convd);
	}
	efree((void *)str_list);
	efree((void *)len_list);
	efree((void *)encstr);
	efree((void *)separator);
	MBSTRG(http_input_identify) = from_encoding;
	MBSTRG(http_input_identify_string) = from_encoding;
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string mb_output_handler(string contents, int status)
   Returns string in output buffer converted to the http_output encoding */
PHP_FUNCTION(mb_output_handler)
{
	char *arg_string;
	int arg_string_len;
	long arg_status;
	mbfl_string string, result;
	const char *charset;
	char *p;
	enum mbfl_no_encoding encoding;
	int last_feed, len;
	unsigned char send_text_mimetype = 0;
	char *s, *mimetype = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl", &arg_string, &arg_string_len, &arg_status) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	encoding = MBSTRG(current_http_output_encoding);

 	/* start phase only */
 	if ((arg_status & PHP_OUTPUT_HANDLER_START) != 0) {
 		/* delete the converter just in case. */
 		if (MBSTRG(outconv)) {
			MBSTRG(illegalchars) += mbfl_buffer_illegalchars(MBSTRG(outconv));
 			mbfl_buffer_converter_delete(MBSTRG(outconv));
 			MBSTRG(outconv) = NULL;
  		}
		if (encoding == mbfl_no_encoding_pass) {
			RETURN_STRINGL(arg_string, arg_string_len, 1);
		}

		/* analyze mime type */
		if (SG(sapi_headers).mimetype && 
			strncmp(SG(sapi_headers).mimetype, "text/", 5) == 0) {
			if ((s = strchr(SG(sapi_headers).mimetype,';')) == NULL){
				mimetype = estrdup(SG(sapi_headers).mimetype);
			} else {
				mimetype = estrndup(SG(sapi_headers).mimetype,s-SG(sapi_headers).mimetype);
			}
			send_text_mimetype = 1;
		} else if (SG(sapi_headers).send_default_content_type) {
			mimetype = SG(default_mimetype) ? SG(default_mimetype) : SAPI_DEFAULT_MIMETYPE;
		}

 		/* if content-type is not yet set, set it and activate the converter */
 		if (SG(sapi_headers).send_default_content_type || send_text_mimetype) {
			charset = mbfl_no2preferred_mime_name(encoding);
			if (charset) {
				len = spprintf( &p, 0, "Content-Type: %s; charset=%s",  mimetype, charset ); 
				if (sapi_add_header(p, len, 0) != FAILURE) {
					SG(sapi_headers).send_default_content_type = 0;
				}
			}
 			/* activate the converter */
 			MBSTRG(outconv) = mbfl_buffer_converter_new(MBSTRG(current_internal_encoding), encoding, 0);
			if (send_text_mimetype){
				efree(mimetype);
			}
 		}
  	}

 	/* just return if the converter is not activated. */
 	if (MBSTRG(outconv) == NULL) {
		RETURN_STRINGL(arg_string, arg_string_len, 1);
	}

 	/* flag */
 	last_feed = ((arg_status & PHP_OUTPUT_HANDLER_END) != 0);
 	/* mode */
 	mbfl_buffer_converter_illegal_mode(MBSTRG(outconv), MBSTRG(current_filter_illegal_mode));
 	mbfl_buffer_converter_illegal_substchar(MBSTRG(outconv), MBSTRG(current_filter_illegal_substchar));
 
 	/* feed the string */
 	mbfl_string_init(&string);
 	string.no_language = MBSTRG(current_language);
 	string.no_encoding = MBSTRG(current_internal_encoding);
 	string.val = (unsigned char *)arg_string;
 	string.len = arg_string_len;
 	mbfl_buffer_converter_feed(MBSTRG(outconv), &string);
 	if (last_feed) {
 		mbfl_buffer_converter_flush(MBSTRG(outconv));
	} 
 	/* get the converter output, and return it */
 	mbfl_buffer_converter_result(MBSTRG(outconv), &result);
 	RETVAL_STRINGL((char *)result.val, result.len, 0);		/* the string is already strdup()'ed */
 
 	/* delete the converter if it is the last feed. */
 	if (last_feed) {
		MBSTRG(illegalchars) += mbfl_buffer_illegalchars(MBSTRG(outconv));
		mbfl_buffer_converter_delete(MBSTRG(outconv));
		MBSTRG(outconv) = NULL;
	}
}
/* }}} */

/* {{{ proto int mb_strlen(string str [, string encoding])
   Get character numbers of a string */
PHP_FUNCTION(mb_strlen)
{
	pval **arg1, **arg2;
	int n;
	mbfl_string string;

	n = ZEND_NUM_ARGS();
	if ((n == 1 && zend_get_parameters_ex(1, &arg1) == FAILURE) ||
	   (n == 2 && zend_get_parameters_ex(2, &arg1, &arg2) == FAILURE) ||
	    n < 1 || n > 2) {
		WRONG_PARAM_COUNT;
	}
	if (Z_TYPE_PP(arg1) == IS_ARRAY ||
		Z_TYPE_PP(arg1) == IS_OBJECT) {
		php_error_docref(NULL TSRMLS_CC, E_NOTICE, "arg1 is invalid.");
		RETURN_FALSE;
	}
	if (( n ==2 && Z_TYPE_PP(arg2) == IS_ARRAY) ||
		( n ==2 && Z_TYPE_PP(arg2) == IS_OBJECT)) {
		php_error_docref(NULL TSRMLS_CC, E_NOTICE, "arg2 is invalid.");
		RETURN_FALSE;
	}

	convert_to_string_ex(arg1);
	mbfl_string_init(&string);
	string.no_language = MBSTRG(current_language);
	string.no_encoding = MBSTRG(current_internal_encoding);
	string.val = (unsigned char *)Z_STRVAL_PP(arg1);
	string.len = Z_STRLEN_PP(arg1);

	if (n == 2) {
		convert_to_string_ex(arg2);
		string.no_encoding = mbfl_name2no_encoding(Z_STRVAL_PP(arg2));
		if (string.no_encoding == mbfl_no_encoding_invalid) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown encoding \"%s\"", Z_STRVAL_PP(arg2));
			RETURN_FALSE;
		}
	}

	n = mbfl_strlen(&string);
	if (n >= 0) {
		RETVAL_LONG(n);
	} else {
		RETVAL_FALSE;
	}
}
/* }}} */

/* {{{ proto int mb_strpos(string haystack, string needle [, int offset [, string encoding]])
   Find position of first occurrence of a string within another */
PHP_FUNCTION(mb_strpos)
{
	pval **arg1, **arg2, **arg3, **arg4;
	int offset, n, reverse = 0;
	mbfl_string haystack, needle;

	mbfl_string_init(&haystack);
	mbfl_string_init(&needle);
	haystack.no_language = MBSTRG(current_language);
	haystack.no_encoding = MBSTRG(current_internal_encoding);
	needle.no_language = MBSTRG(current_language);
	needle.no_encoding = MBSTRG(current_internal_encoding);
	offset = 0;
	switch (ZEND_NUM_ARGS()) {
	case 2:
		if (zend_get_parameters_ex(2, &arg1, &arg2) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
		break;
	case 3:
		if (zend_get_parameters_ex(3, &arg1, &arg2, &arg3) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
		convert_to_long_ex(arg3);
		offset = Z_LVAL_PP(arg3);
		break;
	case 4:
		if (zend_get_parameters_ex(4, &arg1, &arg2, &arg3, &arg4) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
		convert_to_long_ex(arg3);
		offset = Z_LVAL_PP(arg3);
		convert_to_string_ex(arg4);
		haystack.no_encoding = needle.no_encoding = mbfl_name2no_encoding(Z_STRVAL_PP(arg4));
		if (haystack.no_encoding == mbfl_no_encoding_invalid) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown encoding \"%s\"", Z_STRVAL_PP(arg4));
			RETURN_FALSE;
		}
		break;
	default:
		WRONG_PARAM_COUNT;
	}

	convert_to_string_ex(arg1);
	convert_to_string_ex(arg2);

	if (offset < 0 || offset > Z_STRLEN_PP(arg1)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Offset not contained in string.");
		RETURN_FALSE;
	}
	if (Z_STRLEN_PP(arg2) == 0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Empty delimiter.");
		RETURN_FALSE;
	}
	haystack.val = (unsigned char *)Z_STRVAL_PP(arg1);
	haystack.len = Z_STRLEN_PP(arg1);
	needle.val = Z_STRVAL_PP(arg2);
	needle.len = Z_STRLEN_PP(arg2);

	n = mbfl_strpos(&haystack, &needle, offset, reverse);
	if (n >= 0) {
		RETVAL_LONG(n);
	} else {
		switch (-n) {
		case 1:
			break;
		case 2:
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Needle has not positive length.");
			break;
		case 4:
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown encoding or conversion error.");
			break;
		case 8:
			php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Argument is empty.");
			break;
		default:
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown error in mb_strpos.");
			break;			
		}
		RETVAL_FALSE;
	}
}
/* }}} */

/* {{{ proto int mb_strrpos(string haystack, string needle [, string encoding])
   Find the last occurrence of a character in a string within another */
PHP_FUNCTION(mb_strrpos)
{
	pval **arg1, **arg2, **arg3;
	int n;
	mbfl_string haystack, needle;

	mbfl_string_init(&haystack);
	mbfl_string_init(&needle);
	haystack.no_language = MBSTRG(current_language);
	haystack.no_encoding = MBSTRG(current_internal_encoding);
	needle.no_language = MBSTRG(current_language);
	needle.no_encoding = MBSTRG(current_internal_encoding);
	switch (ZEND_NUM_ARGS()) {
	case 2:
		if (zend_get_parameters_ex(2, &arg1, &arg2) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
		break;
	case 3:
		if (zend_get_parameters_ex(3, &arg1, &arg2, &arg3) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
		convert_to_string_ex(arg3);
		haystack.no_encoding = needle.no_encoding = mbfl_name2no_encoding(Z_STRVAL_PP(arg3));
		if (haystack.no_encoding == mbfl_no_encoding_invalid) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown encoding \"%s\"", Z_STRVAL_PP(arg3));
			RETURN_FALSE;
		}
		break;
	default:
		WRONG_PARAM_COUNT;
	}

	convert_to_string_ex(arg1);
	convert_to_string_ex(arg2);
	if (Z_STRLEN_PP(arg1) <= 0) {
		RETURN_FALSE;
	}
	if (Z_STRLEN_PP(arg2) <= 0) {
		RETURN_FALSE;
	}
	haystack.val = (unsigned char *)Z_STRVAL_PP(arg1);
	haystack.len = Z_STRLEN_PP(arg1);
	needle.val = (unsigned char *)Z_STRVAL_PP(arg2);
	needle.len = Z_STRLEN_PP(arg2);
	n = mbfl_strpos(&haystack, &needle, 0, 1);
	if (n >= 0) {
		RETVAL_LONG(n);
	} else {
		RETVAL_FALSE;
	}
}
/* }}} */

/* {{{ proto int mb_substr_count(string haystack, string needle [, string encoding])
   Count the number of substring occurrences */
PHP_FUNCTION(mb_substr_count)
{
	pval **arg1, **arg2, **arg3; 
	int n;
	mbfl_string haystack, needle;

	mbfl_string_init(&haystack);
	mbfl_string_init(&needle);
	haystack.no_language = MBSTRG(current_language);
	haystack.no_encoding = MBSTRG(current_internal_encoding);
	needle.no_language = MBSTRG(current_language);
	needle.no_encoding = MBSTRG(current_internal_encoding);
	switch (ZEND_NUM_ARGS()) {
	case 2:
		if (zend_get_parameters_ex(2, &arg1, &arg2) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
		break;
	case 3:
		if (zend_get_parameters_ex(3, &arg1, &arg2, &arg3) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
		convert_to_string_ex(arg3);
		haystack.no_encoding = needle.no_encoding = mbfl_name2no_encoding(Z_STRVAL_PP(arg3));
		if (haystack.no_encoding == mbfl_no_encoding_invalid) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown encoding \"%s\"", Z_STRVAL_PP(arg3));
			RETURN_FALSE;
		}
		break;
	default:
		WRONG_PARAM_COUNT;
	}

	convert_to_string_ex(arg1);
	convert_to_string_ex(arg2);

	if (Z_STRLEN_PP(arg2) <= 0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,"Empty substring.");
		RETURN_FALSE;
	}
	haystack.val = (unsigned char *)Z_STRVAL_PP(arg1);
	haystack.len = Z_STRLEN_PP(arg1);
	needle.val = (unsigned char *)Z_STRVAL_PP(arg2);
	needle.len = Z_STRLEN_PP(arg2);
	n = mbfl_substr_count(&haystack, &needle);
	if (n >= 0) {
		RETVAL_LONG(n);
	} else {
		RETVAL_FALSE;
	}
}
/* }}} */

/* {{{ proto string mb_substr(string str, int start [, int length [, string encoding]])
   Returns part of a string */
PHP_FUNCTION(mb_substr)
{
	pval **arg1, **arg2, **arg3, **arg4;
	int argc, from, len, mblen;
	mbfl_string string, result, *ret = NULL;

	mbfl_string_init(&string);
	string.no_language = MBSTRG(current_language);
	string.no_encoding = MBSTRG(current_internal_encoding);

	argc = ZEND_NUM_ARGS();
	switch (argc) {
	case 2:
		if (zend_get_parameters_ex(2, &arg1, &arg2) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
		break;
	case 3:
		if (zend_get_parameters_ex(3, &arg1, &arg2, &arg3) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
		break;
	case 4:
		if (zend_get_parameters_ex(4, &arg1, &arg2, &arg3, &arg4) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
		convert_to_string_ex(arg4);
		string.no_encoding = mbfl_name2no_encoding(Z_STRVAL_PP(arg4));
		if (string.no_encoding == mbfl_no_encoding_invalid) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown encoding \"%s\"", Z_STRVAL_PP(arg4));
			RETURN_FALSE;
		}
		break;
	default:
		WRONG_PARAM_COUNT;
	}

	convert_to_string_ex(arg1);
	string.val = (unsigned char *)Z_STRVAL_PP(arg1);
	string.len = Z_STRLEN_PP(arg1);

	convert_to_long_ex(arg2);
	from = Z_LVAL_PP(arg2);
	if (argc >= 3) {
		convert_to_long_ex(arg3);
		len = Z_LVAL_PP(arg3);
	} else {
		len = Z_STRLEN_PP(arg1);
	}

	/* measures length */
	mblen = 0;
	if (from < 0 || len < 0) {
		mblen = mbfl_strlen(&string);
	}

	/* if "from" position is negative, count start position from the end
	 * of the string
	 */
	if (from < 0) {
		from = mblen + from;
		if (from < 0) {
			from = 0;
		}
	}

	/* if "length" position is negative, set it to the length
	 * needed to stop that many chars from the end of the string
	 */
	if (len < 0) {
		len = (mblen - from) + len;
		if (len < 0) {
			len = 0;
		}
	}

	if (((MBSTRG(func_overload) & MB_OVERLOAD_STRING) == MB_OVERLOAD_STRING)
		&& (from >= mbfl_strlen(&string))) {
		RETURN_FALSE;
	}

	ret = mbfl_substr(&string, &result, from, len);
	if (ret != NULL) {
		RETVAL_STRINGL((char *)ret->val, ret->len, 0);		/* the string is already strdup()'ed */
	} else {
		RETVAL_FALSE;
	}
}
/* }}} */

/* {{{ proto string mb_strcut(string str, int start [, int length [, string encoding]])
   Returns part of a string */
PHP_FUNCTION(mb_strcut)
{
	pval **arg1, **arg2, **arg3, **arg4;
	int argc, from, len;
	mbfl_string string, result, *ret;

	mbfl_string_init(&string);
	string.no_language = MBSTRG(current_language);
	string.no_encoding = MBSTRG(current_internal_encoding);

	argc = ZEND_NUM_ARGS();
	switch (argc) {
	case 2:
		if (zend_get_parameters_ex(2, &arg1, &arg2) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
		break;
	case 3:
		if (zend_get_parameters_ex(3, &arg1, &arg2, &arg3) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
		break;
	case 4:
		if (zend_get_parameters_ex(4, &arg1, &arg2, &arg3, &arg4) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
		convert_to_string_ex(arg4);
		string.no_encoding = mbfl_name2no_encoding(Z_STRVAL_PP(arg4));
		if (string.no_encoding == mbfl_no_encoding_invalid) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown encoding \"%s\"", Z_STRVAL_PP(arg4));
			RETURN_FALSE;
		}
		break;
	default:
		WRONG_PARAM_COUNT;
	}

	convert_to_string_ex(arg1);
	string.val = Z_STRVAL_PP(arg1);
	string.len = Z_STRLEN_PP(arg1);

	convert_to_long_ex(arg2);
	from = Z_LVAL_PP(arg2);
	if (argc >= 3) {
		convert_to_long_ex(arg3);
		len = Z_LVAL_PP(arg3);
	} else {
		len = Z_STRLEN_PP(arg1);
	}

	/* if "from" position is negative, count start position from the end
	 * of the string
	 */
	if (from < 0) {
		from = Z_STRLEN_PP(arg1) + from;
		if (from < 0) {
			from = 0;
		}
	}

	/* if "length" position is negative, set it to the length
	 * needed to stop that many chars from the end of the string
	 */
	if (len < 0) {
		len = (Z_STRLEN_PP(arg1) - from) + len;
		if (len < 0) {
			len = 0;
		}
	}

	if (from > Z_STRLEN_PP(arg1)) {
		RETURN_FALSE;
	}
	if (((unsigned) from + (unsigned) len) > Z_STRLEN_PP(arg1)) {
		len = Z_STRLEN_PP(arg1) - from;
	}

	ret = mbfl_strcut(&string, &result, from, len);
	if (ret != NULL) {
		RETVAL_STRINGL(ret->val, ret->len, 0);		/* the string is already strdup()'ed */
	} else {
		RETVAL_FALSE;
	}
}
/* }}} */

/* {{{ proto int mb_strwidth(string str [, string encoding])
   Gets terminal width of a string */
PHP_FUNCTION(mb_strwidth)
{
	pval **arg1, **arg2;
	int n;
	mbfl_string string;

	n = ZEND_NUM_ARGS();
	if ((n == 1 && zend_get_parameters_ex(1, &arg1) == FAILURE) ||
	   (n == 2 && zend_get_parameters_ex(2, &arg1, &arg2) == FAILURE) ||
	    n < 1 || n > 2) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string_ex(arg1);
	mbfl_string_init(&string);
	string.no_language = MBSTRG(current_language);
	string.no_encoding = MBSTRG(current_internal_encoding);
	string.val = (unsigned char *)Z_STRVAL_PP(arg1);
	string.len = Z_STRLEN_PP(arg1);

	if (n == 2){
		convert_to_string_ex(arg2);
		string.no_encoding = mbfl_name2no_encoding(Z_STRVAL_PP(arg2));
		if (string.no_encoding == mbfl_no_encoding_invalid) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown encoding \"%s\"", Z_STRVAL_PP(arg2));
			RETURN_FALSE;
		}
	}

	n = mbfl_strwidth(&string);
	if (n >= 0) {
		RETVAL_LONG(n);
	} else {
		RETVAL_FALSE;
	}
}
/* }}} */

/* {{{ proto string mb_strimwidth(string str, int start, int width [, string trimmarker [, string encoding]])
   Trim the string in terminal width */
PHP_FUNCTION(mb_strimwidth)
{
	pval **arg1, **arg2, **arg3, **arg4, **arg5;
	int from, width;
	mbfl_string string, result, marker, *ret;

	mbfl_string_init(&string);
	mbfl_string_init(&marker);
	string.no_language = MBSTRG(current_language);
	string.no_encoding = MBSTRG(current_internal_encoding);
	marker.no_language = MBSTRG(current_language);
	marker.no_encoding = MBSTRG(current_internal_encoding);
	marker.val = NULL;
	marker.len = 0;

	switch (ZEND_NUM_ARGS()) {
	case 3:
		if (zend_get_parameters_ex(3, &arg1, &arg2, &arg3) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
		break;
	case 4:
		if (zend_get_parameters_ex(4, &arg1, &arg2, &arg3, &arg4) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
		break;
	case 5:
		if (zend_get_parameters_ex(5, &arg1, &arg2, &arg3, &arg4, &arg5) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
		convert_to_string_ex(arg5);
		string.no_encoding = marker.no_encoding = mbfl_name2no_encoding(Z_STRVAL_PP(arg5));
		if (string.no_encoding == mbfl_no_encoding_invalid) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown encoding \"%s\"", Z_STRVAL_PP(arg5));
			RETURN_FALSE;
		}
		break;
	default:
		WRONG_PARAM_COUNT;
	}

	convert_to_string_ex(arg1);
	string.val = (unsigned char *)Z_STRVAL_PP(arg1);
	string.len = Z_STRLEN_PP(arg1);

	convert_to_long_ex(arg2);
	from = Z_LVAL_PP(arg2);
	if (from < 0 || from > Z_STRLEN_PP(arg1)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Start position is out of reange");
		RETURN_FALSE;
	}

	convert_to_long_ex(arg3);
	width = Z_LVAL_PP(arg3);

	if (width < 0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Width is negative value");
		RETURN_FALSE;
	}

	if (ZEND_NUM_ARGS() >= 4) {
		convert_to_string_ex(arg4);
		marker.val = (unsigned char *)Z_STRVAL_PP(arg4);
		marker.len = Z_STRLEN_PP(arg4);
	}

	ret = mbfl_strimwidth(&string, &marker, &result, from, width);
	if (ret != NULL) {
		RETVAL_STRINGL((char *)ret->val, ret->len, 0);		/* the string is already strdup()'ed */
	} else {
		RETVAL_FALSE;
	}
}
/* }}} */

/* {{{ MBSTRING_API char *php_mb_convert_encoding() */
MBSTRING_API char * php_mb_convert_encoding(char *input, size_t length, char *_to_encoding, char *_from_encodings, size_t *output_len TSRMLS_DC)
{
	mbfl_string string, result, *ret = NULL;
	enum mbfl_no_encoding from_encoding, to_encoding;
	mbfl_buffer_converter *convd;
	int size;
	enum mbfl_no_encoding *list;
	char *output=NULL;

	if (output_len) {
		*output_len = 0;
	}
	if (!input) {
		return NULL;
	}
	/* new encoding */
	if (_to_encoding && strlen(_to_encoding)) {
		to_encoding = mbfl_name2no_encoding(_to_encoding);
		if (to_encoding == mbfl_no_encoding_invalid) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown encoding \"%s\"", _to_encoding);
			return NULL;
		}
	} else {
		to_encoding = MBSTRG(current_internal_encoding);
	}

	/* initialize string */
	mbfl_string_init(&string);
	mbfl_string_init(&result);
	from_encoding = MBSTRG(current_internal_encoding);
	string.no_encoding = from_encoding;
	string.no_language = MBSTRG(current_language);
	string.val = (unsigned char *)input;
	string.len = length;

	/* pre-conversion encoding */
	if (_from_encodings) {
		list = NULL;
		size = 0;
	    php_mb_parse_encoding_list(_from_encodings, strlen(_from_encodings), &list, &size, 0 TSRMLS_CC);
		if (size == 1) {
			from_encoding = *list;
			string.no_encoding = from_encoding;
		} else if (size > 1) {
			/* auto detect */
			from_encoding = mbfl_identify_encoding_no(&string, list, size, 0);
			if (from_encoding != mbfl_no_encoding_invalid) {
				string.no_encoding = from_encoding;
			} else {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to detect character encoding");
				from_encoding = mbfl_no_encoding_pass;
				to_encoding = from_encoding;
				string.no_encoding = from_encoding;
			}
		} else {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Illegal character encoding specified");
		}
		if (list != NULL) {
			efree((void *)list);
		}
	}

	/* initialize converter */
	convd = mbfl_buffer_converter_new(from_encoding, to_encoding, string.len);
	if (convd == NULL) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to create character encoding converter");
		return NULL;
	}
	mbfl_buffer_converter_illegal_mode(convd, MBSTRG(current_filter_illegal_mode));
	mbfl_buffer_converter_illegal_substchar(convd, MBSTRG(current_filter_illegal_substchar));

	/* do it */
	ret = mbfl_buffer_converter_feed_result(convd, &string, &result);
	if (ret) {
		if (output_len) {
			*output_len = ret->len;
		}
		output = (char *)ret->val;
	}

	MBSTRG(illegalchars) += mbfl_buffer_illegalchars(convd);
	mbfl_buffer_converter_delete(convd);
	return output;
}
/* }}} */

/* {{{ proto string mb_convert_encoding(string str, string to-encoding [, mixed from-encoding])
   Returns converted string in desired encoding */
PHP_FUNCTION(mb_convert_encoding)
{
	pval **arg_str, **arg_new, **arg_old;
	int i;
	size_t size, l, n;
	char *_from_encodings, *ret, *s_free = NULL;

	zval **hash_entry;
	HashTable *target_hash;

	_from_encodings = NULL;
	if (ZEND_NUM_ARGS() == 2) {
		if (zend_get_parameters_ex(2, &arg_str, &arg_new) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
	} else if (ZEND_NUM_ARGS() == 3) {
		if (zend_get_parameters_ex(3, &arg_str, &arg_new, &arg_old) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
		switch (Z_TYPE_PP(arg_old)) {
		case IS_ARRAY:

			target_hash = Z_ARRVAL_PP(arg_old);
			zend_hash_internal_pointer_reset(target_hash);
			i = zend_hash_num_elements(target_hash);
			_from_encodings = NULL;
			while (i > 0) {
				if (zend_hash_get_current_data(target_hash, (void **) &hash_entry) == FAILURE) {
					break;
				}
				convert_to_string_ex(hash_entry);					
				if ( _from_encodings) {
					l = strlen(_from_encodings);
					n = strlen(Z_STRVAL_PP(hash_entry));
					_from_encodings = erealloc(_from_encodings, l+n+2);
					strcpy(_from_encodings+l,",");
					strcpy(_from_encodings+l+1,Z_STRVAL_PP(hash_entry));
				} else {
					_from_encodings = estrdup(Z_STRVAL_PP(hash_entry));
				}
				zend_hash_move_forward(target_hash);
				i--;
			}
			if (_from_encodings != NULL && !strlen(_from_encodings)) {
				efree(_from_encodings);
				_from_encodings = NULL;
			}
			s_free = _from_encodings;
			break;
		default:
			convert_to_string_ex(arg_old);
			_from_encodings = Z_STRVAL_PP(arg_old);
			break;
		}
	} else {
		WRONG_PARAM_COUNT;
	}

	/* new encoding */
	convert_to_string_ex(arg_str);
	convert_to_string_ex(arg_new);
	ret = php_mb_convert_encoding( Z_STRVAL_PP(arg_str), Z_STRLEN_PP(arg_str), Z_STRVAL_PP(arg_new), _from_encodings, &size TSRMLS_CC);
	if (ret != NULL) {
		RETVAL_STRINGL(ret, size, 0);		/* the string is already strdup()'ed */
	} else {
		RETVAL_FALSE;
	}
	if ( s_free) {
		efree(s_free);
	}
}
/* }}} */

/* {{{ proto string mb_convert_case(string sourcestring, int mode [, string encoding])
   Returns a case-folded version of sourcestring */
PHP_FUNCTION(mb_convert_case)
{
	char *str, *from_encoding = (char*)mbfl_no2preferred_mime_name(MBSTRG(current_internal_encoding));
	int str_len, from_encoding_len;
	long case_mode = 0;
	char *newstr;
	size_t ret_len;

	RETVAL_FALSE;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl|s!", &str, &str_len,
				&case_mode, &from_encoding, &from_encoding_len) == FAILURE)
		RETURN_FALSE;

	newstr = php_unicode_convert_case(case_mode, str, (size_t) str_len, &ret_len, from_encoding TSRMLS_CC);

	if (newstr) {
		RETVAL_STRINGL(newstr, ret_len, 0);
	}	
}
/* }}} */

/* {{{ proto string mb_strtoupper(string sourcestring [, string encoding])
 *  Returns a uppercased version of sourcestring
 */
PHP_FUNCTION(mb_strtoupper)
{
	char *str, *from_encoding = (char*)mbfl_no2preferred_mime_name(MBSTRG(current_internal_encoding));
	int str_len, from_encoding_len;
	char *newstr;
	size_t ret_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|s!", &str, &str_len,
				&from_encoding, &from_encoding_len) == FAILURE) {
		RETURN_FALSE;
	}
	newstr = php_unicode_convert_case(PHP_UNICODE_CASE_UPPER, str, (size_t) str_len, &ret_len, from_encoding TSRMLS_CC);

	if (newstr) {
		RETURN_STRINGL(newstr, ret_len, 0);
	}
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto string mb_strtolower(string sourcestring [, string encoding])
 *  Returns a lowercased version of sourcestring
 */
PHP_FUNCTION(mb_strtolower)
{
	char *str, *from_encoding = (char*)mbfl_no2preferred_mime_name(MBSTRG(current_internal_encoding));
	int str_len, from_encoding_len;
	char *newstr;
	size_t ret_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|s!", &str, &str_len,
				&from_encoding, &from_encoding_len) == FAILURE) {
		RETURN_FALSE;
	}
	newstr = php_unicode_convert_case(PHP_UNICODE_CASE_LOWER, str, (size_t) str_len, &ret_len, from_encoding TSRMLS_CC);

	if (newstr) {
		RETURN_STRINGL(newstr, ret_len, 0);
	}
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto string mb_detect_encoding(string str [, mixed encoding_list [, bool strict]])
   Encodings of the given string is returned (as a string) */
PHP_FUNCTION(mb_detect_encoding)
{
	pval **arg_str, **arg_list, **arg_strict;
	mbfl_string string;
	const char *ret;
	enum mbfl_no_encoding *elist;
	int size, strict = 0;
	enum mbfl_no_encoding *list;

	if (ZEND_NUM_ARGS() == 1) {
		if (zend_get_parameters_ex(1, &arg_str) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
	} else if (ZEND_NUM_ARGS() == 2) {
		if (zend_get_parameters_ex(2, &arg_str, &arg_list) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
	} else if (ZEND_NUM_ARGS() == 3) {
		if (zend_get_parameters_ex(3, &arg_str, &arg_list, &arg_strict) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
	} else {
		WRONG_PARAM_COUNT;
	}

	/* make encoding list */
	list = NULL;
	size = 0;
	if (ZEND_NUM_ARGS() >= 2 &&  Z_STRVAL_PP(arg_list)) {
		switch (Z_TYPE_PP(arg_list)) {
		case IS_ARRAY:
			if (!php_mb_parse_encoding_array(*arg_list, &list, &size, 0 TSRMLS_CC)) {
				if (list) {
					efree(list);
					size = 0;
				}
			}
			break;
		default:
			convert_to_string_ex(arg_list);
			if (!php_mb_parse_encoding_list(Z_STRVAL_PP(arg_list), Z_STRLEN_PP(arg_list), &list, &size, 0 TSRMLS_CC)) {
				if (list) {
					efree(list);
					size = 0;
				}
			}
			break;
		}
		if (size <= 0) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Illegal argument");
		}
	}

	if (ZEND_NUM_ARGS() == 3) {
		convert_to_long_ex(arg_strict);
		strict = Z_LVAL_PP(arg_strict);
	}

	if (size > 0 && list != NULL) {
		elist = list;
	} else {
		elist = MBSTRG(current_detect_order_list);
		size = MBSTRG(current_detect_order_list_size);
	}

	convert_to_string_ex(arg_str);
	mbfl_string_init(&string);
	string.no_language = MBSTRG(current_language);
	string.val = (unsigned char *)Z_STRVAL_PP(arg_str);
	string.len = Z_STRLEN_PP(arg_str);
	ret = mbfl_identify_encoding_name(&string, elist, size, strict);
	if (list != NULL) {
		efree((void *)list);
	}
	if (ret != NULL) {
		RETVAL_STRING((char *)ret, 1);
	} else {
		RETVAL_FALSE;
	}
}
/* }}} */

/* {{{ proto string mb_encode_mimeheader(string str [, string charset [, string transfer-encoding [, string linefeed [, int indent]]]])
   Converts the string to MIME "encoded-word" in the format of =?charset?(B|Q)?encoded_string?= */
PHP_FUNCTION(mb_encode_mimeheader)
{
	pval **argv[5];
	enum mbfl_no_encoding charset, transenc;
	mbfl_string  string, result, *ret;
	char *p, *linefeed;
	int indent;

	if (ZEND_NUM_ARGS() < 1 || ZEND_NUM_ARGS() > 5 || zend_get_parameters_array_ex(ZEND_NUM_ARGS(), argv) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	charset = mbfl_no_encoding_pass;
	transenc = mbfl_no_encoding_base64;
	if (ZEND_NUM_ARGS() >= 2) {
		convert_to_string_ex(argv[1]);
		charset = mbfl_name2no_encoding(Z_STRVAL_PP(argv[1]));
		if (charset == mbfl_no_encoding_invalid) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown encoding \"%s\"", Z_STRVAL_PP(argv[1]));
			RETURN_FALSE;
		}
	} else {
		const mbfl_language *lang = mbfl_no2language(MBSTRG(current_language));
		if (lang != NULL) {
			charset = lang->mail_charset;
			transenc = lang->mail_header_encoding;
		}
	}

	if (ZEND_NUM_ARGS() >= 3) {
		convert_to_string_ex(argv[2]);
		p = Z_STRVAL_PP(argv[2]);
		if (*p == 'B' || *p == 'b') {
			transenc = mbfl_no_encoding_base64;
		} else if (*p == 'Q' || *p == 'q') {
			transenc = mbfl_no_encoding_qprint;
		}
	}

	linefeed = "\r\n";
	if (ZEND_NUM_ARGS() >= 4) {
		convert_to_string_ex(argv[3]);
		linefeed = Z_STRVAL_PP(argv[3]);
	}

	indent = 0;
	if (ZEND_NUM_ARGS() >= 5) {
		convert_to_long_ex(argv[4]);
		indent = Z_LVAL_PP(argv[4]);
	}

	convert_to_string_ex(argv[0]);
	mbfl_string_init(&string);
	mbfl_string_init(&result);
	string.no_language = MBSTRG(current_language);
	string.no_encoding = MBSTRG(current_internal_encoding);
	string.val = Z_STRVAL_PP(argv[0]);
	string.len = Z_STRLEN_PP(argv[0]);
	ret = mbfl_mime_header_encode(&string, &result, charset, transenc, linefeed, indent);
	if (ret != NULL) {
		RETVAL_STRINGL((char *)ret->val, ret->len, 0)	/* the string is already strdup()'ed */
	} else {
		RETVAL_FALSE;
	}
}
/* }}} */

/* {{{ proto string mb_decode_mimeheader(string string)
   Decodes the MIME "encoded-word" in the string */
PHP_FUNCTION(mb_decode_mimeheader)
{
	pval **arg_str;
	mbfl_string string, result, *ret;

	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(1, &arg_str) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string_ex(arg_str);
	mbfl_string_init(&string);
	mbfl_string_init(&result);
	string.no_language = MBSTRG(current_language);
	string.no_encoding = MBSTRG(current_internal_encoding);
	string.val = (unsigned char *)Z_STRVAL_PP(arg_str);
	string.len = Z_STRLEN_PP(arg_str);
	ret = mbfl_mime_header_decode(&string, &result, MBSTRG(current_internal_encoding));
	if (ret != NULL) {
		RETVAL_STRINGL((char *)ret->val, ret->len, 0)	/* the string is already strdup()'ed */
	} else {
		RETVAL_FALSE;
	}
}
/* }}} */

/* {{{ proto string mb_convert_kana(string str [, string option] [, string encoding])
   Conversion between full-width character and half-width character (Japanese) */
PHP_FUNCTION(mb_convert_kana)
{
	pval **arg1, **arg2, **arg3;
	int argc, opt, i, n;
	char *p;
	mbfl_string string, result, *ret;
	enum mbfl_no_encoding no_encoding;

	mbfl_string_init(&string);
	string.no_language = MBSTRG(current_language);
	string.no_encoding = MBSTRG(current_internal_encoding);

	argc = ZEND_NUM_ARGS();
	if ((argc == 1 && zend_get_parameters_ex(1, &arg1) == FAILURE) ||
	   (argc == 2 && zend_get_parameters_ex(2, &arg1, &arg2) == FAILURE) ||
	   (argc == 3 && zend_get_parameters_ex(3, &arg1, &arg2, &arg3) == FAILURE) ||
		argc < 1 || argc > 3) {
		WRONG_PARAM_COUNT;
	}

	convert_to_string_ex(arg1);
	string.val = (unsigned char *)Z_STRVAL_PP(arg1);
	string.len = Z_STRLEN_PP(arg1);

	/* option */
	if (argc >= 2){
		convert_to_string_ex(arg2);
		p = Z_STRVAL_PP(arg2);
		n = Z_STRLEN_PP(arg2);
		i = 0;
		opt = 0;
		while (i < n) {
			i++;
			switch (*p++) {
			case 'A':
				opt |= 0x1;
				break;
			case 'a':
				opt |= 0x10;
				break;
			case 'R':
				opt |= 0x2;
				break;
			case 'r':
				opt |= 0x20;
				break;
			case 'N':
				opt |= 0x4;
				break;
			case 'n':
				opt |= 0x40;
				break;
			case 'S':
				opt |= 0x8;
				break;
			case 's':
				opt |= 0x80;
				break;
			case 'K':
				opt |= 0x100;
				break;
			case 'k':
				opt |= 0x1000;
				break;
			case 'H':
				opt |= 0x200;
				break;
			case 'h':
				opt |= 0x2000;
				break;
			case 'V':
				opt |= 0x800;
				break;
			case 'C':
				opt |= 0x10000;
				break;
			case 'c':
				opt |= 0x20000;
				break;
			case 'M':
				opt |= 0x100000;
				break;
			case 'm':
				opt |= 0x200000;
				break;
			}
		}
	} else {
		opt = 0x900;
	}

	/* encoding */
	if (argc == 3) {
		convert_to_string_ex(arg3);
		no_encoding = mbfl_name2no_encoding(Z_STRVAL_PP(arg3));
		if (no_encoding == mbfl_no_encoding_invalid) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown encoding \"%s\"", Z_STRVAL_PP(arg3));
			RETURN_FALSE;
		} else {
			string.no_encoding = no_encoding;
		}
	}

	ret = mbfl_ja_jp_hantozen(&string, &result, opt);
	if (ret != NULL) {
		RETVAL_STRINGL((char *)ret->val, ret->len, 0);		/* the string is already strdup()'ed */
	} else {
		RETVAL_FALSE;
	}
}
/* }}} */

#define PHP_MBSTR_STACK_BLOCK_SIZE 32

/* {{{ proto string mb_convert_variables(string to-encoding, mixed from-encoding [, mixed ...])
   Converts the string resource in variables to desired encoding */
PHP_FUNCTION(mb_convert_variables)
{
	pval ***args, ***stack, **var, **hash_entry;
	HashTable *target_hash;
	mbfl_string string, result, *ret;
	enum mbfl_no_encoding from_encoding, to_encoding;
	mbfl_encoding_detector *identd;
	mbfl_buffer_converter *convd;
	int n, argc, stack_level, stack_max, elistsz;
	enum mbfl_no_encoding *elist;
	char *name;
	void *ptmp;

	argc = ZEND_NUM_ARGS();
	if (argc < 3) {
		WRONG_PARAM_COUNT;
	}
	args = (pval ***)ecalloc(argc, sizeof(pval **));
	if (args == NULL) {
		RETURN_FALSE;
	}
	if (zend_get_parameters_array_ex(argc, args) == FAILURE) {
		efree((void *)args);
		WRONG_PARAM_COUNT;
	}

	/* new encoding */
	convert_to_string_ex(args[0]);
	to_encoding = mbfl_name2no_encoding(Z_STRVAL_PP(args[0]));
	if (to_encoding == mbfl_no_encoding_invalid) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown encoding \"%s\"", Z_STRVAL_PP(args[0]));
		efree((void *)args);
		RETURN_FALSE;
	}

	/* initialize string */
	mbfl_string_init(&string);
	mbfl_string_init(&result);
	from_encoding = MBSTRG(current_internal_encoding);
	string.no_encoding = from_encoding;
	string.no_language = MBSTRG(current_language);

	/* pre-conversion encoding */
	elist = NULL;
	elistsz = 0;
	switch (Z_TYPE_PP(args[1])) {
	case IS_ARRAY:
		php_mb_parse_encoding_array(*args[1], &elist, &elistsz, 0 TSRMLS_CC);
		break;
	default:
		convert_to_string_ex(args[1]);
		php_mb_parse_encoding_list(Z_STRVAL_PP(args[1]), Z_STRLEN_PP(args[1]), &elist, &elistsz, 0 TSRMLS_CC);
		break;
	}
	if (elistsz <= 0) {
		from_encoding = mbfl_no_encoding_pass;
	} else if (elistsz == 1) {
		from_encoding = *elist;
	} else {
		/* auto detect */
		from_encoding = mbfl_no_encoding_invalid;
		stack_max = PHP_MBSTR_STACK_BLOCK_SIZE;
		stack = (pval ***)safe_emalloc(stack_max, sizeof(pval **), 0);
		if (stack != NULL) {
			stack_level = 0;
			identd = mbfl_encoding_detector_new(elist, elistsz, 0);
			if (identd != NULL) {
				n = 2;
				while (n < argc || stack_level > 0) {
					if (stack_level <= 0) {
						var = args[n++];
						if (Z_TYPE_PP(var) == IS_ARRAY || Z_TYPE_PP(var) == IS_OBJECT) {
							target_hash = HASH_OF(*var);
							if (target_hash != NULL) {
								zend_hash_internal_pointer_reset(target_hash);
							}
						}
					} else {
						stack_level--;
						var = stack[stack_level];
					}
					if (Z_TYPE_PP(var) == IS_ARRAY || Z_TYPE_PP(var) == IS_OBJECT) {
						target_hash = HASH_OF(*var);
						if (target_hash != NULL) {
							while (zend_hash_get_current_data(target_hash, (void **) &hash_entry) != FAILURE) {
								zend_hash_move_forward(target_hash);
								if (Z_TYPE_PP(hash_entry) == IS_ARRAY || Z_TYPE_PP(hash_entry) == IS_OBJECT) {
									if (stack_level >= stack_max) {
										stack_max += PHP_MBSTR_STACK_BLOCK_SIZE;
										ptmp = erealloc(stack, sizeof(pval **)*stack_max);
										if (ptmp == NULL) {
											php_error_docref(NULL TSRMLS_CC, E_WARNING, "stack err at %s:(%d)", __FILE__, __LINE__);
											continue;
										}
										stack = (pval ***)ptmp;
									}
									stack[stack_level] = var;
									stack_level++;
									var = hash_entry;
									target_hash = HASH_OF(*var);
									if (target_hash != NULL) {
										zend_hash_internal_pointer_reset(target_hash);
										continue;
									}
								} else if (Z_TYPE_PP(hash_entry) == IS_STRING) {
									string.val = (unsigned char *)Z_STRVAL_PP(hash_entry);
									string.len = Z_STRLEN_PP(hash_entry);
									if (mbfl_encoding_detector_feed(identd, &string)) {
										goto detect_end;		/* complete detecting */
									}
								}
							}
						}
					} else if (Z_TYPE_PP(var) == IS_STRING) {
						string.val = (unsigned char *)Z_STRVAL_PP(var);
						string.len = Z_STRLEN_PP(var);
						if (mbfl_encoding_detector_feed(identd, &string)) {
							goto detect_end;		/* complete detecting */
						}
					}
				}
detect_end:
				from_encoding = mbfl_encoding_detector_judge(identd);
				mbfl_encoding_detector_delete(identd);
			}
			efree(stack);
		}
		if (from_encoding == mbfl_no_encoding_invalid) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to detect encoding");
			from_encoding = mbfl_no_encoding_pass;
		}
	}
	if (elist != NULL) {
		efree((void *)elist);
	}
	/* create converter */
	convd = NULL;
	if (from_encoding != mbfl_no_encoding_pass) {
		convd = mbfl_buffer_converter_new(from_encoding, to_encoding, 0);
		if (convd == NULL) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to create converter");
			RETURN_FALSE;
		}
		mbfl_buffer_converter_illegal_mode(convd, MBSTRG(current_filter_illegal_mode));
		mbfl_buffer_converter_illegal_substchar(convd, MBSTRG(current_filter_illegal_substchar));
	}

	/* convert */
	if (convd != NULL) {
		stack_max = PHP_MBSTR_STACK_BLOCK_SIZE;
		stack = (pval ***)safe_emalloc(stack_max, sizeof(pval **), 0);
		if (stack != NULL) {
			stack_level = 0;
			n = 2;
			while (n < argc || stack_level > 0) {
				if (stack_level <= 0) {
					var = args[n++];
					if (Z_TYPE_PP(var) == IS_ARRAY || Z_TYPE_PP(var) == IS_OBJECT) {
						target_hash = HASH_OF(*var);
						if (target_hash != NULL) {
							zend_hash_internal_pointer_reset(target_hash);
						}
					}
				} else {
					stack_level--;
					var = stack[stack_level];
				}
				if (Z_TYPE_PP(var) == IS_ARRAY || Z_TYPE_PP(var) == IS_OBJECT) {
					target_hash = HASH_OF(*var);
					if (target_hash != NULL) {
						while (zend_hash_get_current_data(target_hash, (void **) &hash_entry) != FAILURE) {
							zend_hash_move_forward(target_hash);
							if (Z_TYPE_PP(hash_entry) == IS_ARRAY || Z_TYPE_PP(hash_entry) == IS_OBJECT) {
								if (stack_level >= stack_max) {
									stack_max += PHP_MBSTR_STACK_BLOCK_SIZE;
									ptmp = erealloc(stack, sizeof(pval **)*stack_max);
									if (ptmp == NULL) {
										php_error_docref(NULL TSRMLS_CC, E_WARNING, "stack err at %s:(%d)", __FILE__, __LINE__);
										continue;
									}
									stack = (pval ***)ptmp;
								}
								stack[stack_level] = var;
								stack_level++;
								var = hash_entry;
								SEPARATE_ZVAL(hash_entry);
								target_hash = HASH_OF(*var);
								if (target_hash != NULL) {
									zend_hash_internal_pointer_reset(target_hash);
									continue;
								}
							} else if (Z_TYPE_PP(hash_entry) == IS_STRING) {
								string.val = (unsigned char *)Z_STRVAL_PP(hash_entry);
								string.len = Z_STRLEN_PP(hash_entry);
								ret = mbfl_buffer_converter_feed_result(convd, &string, &result);
								if (ret != NULL) {
									if ((*hash_entry)->refcount > 1) {
										ZVAL_DELREF(*hash_entry);
										MAKE_STD_ZVAL(*hash_entry);
									} else {
										zval_dtor(*hash_entry);
									}
									ZVAL_STRINGL(*hash_entry, ret->val, ret->len, 0);
								}
							}
						}
					}
				} else if (Z_TYPE_PP(var) == IS_STRING) {
					string.val = (unsigned char *)Z_STRVAL_PP(var);
					string.len = Z_STRLEN_PP(var);
					ret = mbfl_buffer_converter_feed_result(convd, &string, &result);
					if (ret != NULL) {
						zval_dtor(*var);
						ZVAL_STRINGL(*var, ret->val, ret->len, 0);
					}
				}
			}
			efree(stack);
		}
		MBSTRG(illegalchars) += mbfl_buffer_illegalchars(convd);
		mbfl_buffer_converter_delete(convd);
	}

	efree((void *)args);

	name = (char *)mbfl_no_encoding2name(from_encoding);
	if (name != NULL) {
		RETURN_STRING(name, 1);
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ HTML numeric entity */
/* {{{ static void php_mbstr_numericentity_exec() */
static void
php_mbstr_numericentity_exec(INTERNAL_FUNCTION_PARAMETERS, int type)
{
	pval **arg1, **arg2, **arg3, **hash_entry;
	HashTable *target_hash;
	int argc, i, *convmap, *mapelm, mapsize=0;
	mbfl_string string, result, *ret;
	enum mbfl_no_encoding no_encoding;

	argc = ZEND_NUM_ARGS();
	if ((argc == 2 && zend_get_parameters_ex(2, &arg1, &arg2) == FAILURE) ||
	   (argc == 3 && zend_get_parameters_ex(3, &arg1, &arg2, &arg3) == FAILURE) ||
		argc < 2 || argc > 3) {
		WRONG_PARAM_COUNT;
	}

	convert_to_string_ex(arg1);
	mbfl_string_init(&string);
	string.no_language = MBSTRG(current_language);
	string.no_encoding = MBSTRG(current_internal_encoding);
	string.val = (unsigned char *)Z_STRVAL_PP(arg1);
	string.len = Z_STRLEN_PP(arg1);

	/* encoding */
	if (argc == 3) {
		convert_to_string_ex(arg3);
		no_encoding = mbfl_name2no_encoding(Z_STRVAL_PP(arg3));
		if (no_encoding == mbfl_no_encoding_invalid) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown encoding \"%s\"", Z_STRVAL_PP(arg3));
			RETURN_FALSE;
		} else {
			string.no_encoding = no_encoding;
		}
	}

	/* conversion map */
	convmap = NULL;
	if (Z_TYPE_PP(arg2) == IS_ARRAY){
		target_hash = Z_ARRVAL_PP(arg2);
		zend_hash_internal_pointer_reset(target_hash);
		i = zend_hash_num_elements(target_hash);
		if (i > 0) {
			convmap = (int *)safe_emalloc(i, sizeof(int), 0);
			if (convmap != NULL) {
				mapelm = convmap;
				mapsize = 0;
				while (i > 0) {
					if (zend_hash_get_current_data(target_hash, (void **) &hash_entry) == FAILURE) {
						break;
					}
					convert_to_long_ex(hash_entry);
					*mapelm++ = Z_LVAL_PP(hash_entry);
					mapsize++;
					i--;
					zend_hash_move_forward(target_hash);
				}
			}
		}
	}
	if (convmap == NULL) {
		RETURN_FALSE;
	}
	mapsize /= 4;

	ret = mbfl_html_numeric_entity(&string, &result, convmap, mapsize, type);
	if (ret != NULL) {
		RETVAL_STRINGL((char *)ret->val, ret->len, 0);
	} else {
		RETVAL_FALSE;
	}
	efree((void *)convmap);
}
/* }}} */

/* {{{ proto string mb_encode_numericentity(string string, array convmap [, string encoding])
   Converts specified characters to HTML numeric entities */
PHP_FUNCTION(mb_encode_numericentity)
{
	php_mbstr_numericentity_exec(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ proto string mb_decode_numericentity(string string, array convmap [, string encoding])
   Converts HTML numeric entities to character code */
PHP_FUNCTION(mb_decode_numericentity)
{
	php_mbstr_numericentity_exec(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */
/* }}} */

/* {{{ proto int mb_send_mail(string to, string subject, string message [, string additional_headers [, string additional_parameters]])
 *  Sends an email message with MIME scheme
 */
#if HAVE_SENDMAIL
#define SKIP_LONG_HEADER_SEP_MBSTRING(str, pos)						\
	if (str[pos] == '\r' && str[pos + 1] == '\n' && (str[pos + 2] == ' ' || str[pos + 2] == '\t')) {	\
		pos += 2;											\
		while (str[pos + 1] == ' ' || str[pos + 1] == '\t') {							\
			pos++;											\
		}                                               \
		continue;											\
	}													\

#define MAIL_ASCIIZ_CHECK_MBSTRING(str, len)			\
	pp = str;					\
	ee = pp + len;					\
	while ((pp = memchr(pp, '\0', (ee - pp)))) {	\
		*pp = ' ';				\
	}						\

PHP_FUNCTION(mb_send_mail)
{
	int argc, n;
	pval **argv[5];
	char *to=NULL, *message=NULL, *headers=NULL, *subject=NULL, *extra_cmd=NULL;
	char *message_buf=NULL, *subject_buf=NULL, *p;
	mbfl_string orig_str, conv_str;
	mbfl_string *pstr;	/* pointer to mbfl string for return value */
	enum mbfl_no_encoding
	    tran_cs,	/* transfar text charset */
	    head_enc,	/* header transfar encoding */
	    body_enc;	/* body transfar encoding */
	mbfl_memory_device device;	/* automatic allocateable buffer for additional header */
	const mbfl_language *lang;
	int err = 0;
	char *to_r = NULL;
	int to_len, extra_cmd_len, i;
	char *pp, *ee;

    if (PG(safe_mode) && (ZEND_NUM_ARGS() == 5)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "SAFE MODE Restriction in effect.  The fifth parameter is disabled in SAFE MODE.");
		RETURN_FALSE;
	}
    
	/* initialize */
	mbfl_memory_device_init(&device, 0, 0);
	mbfl_string_init(&orig_str);
	mbfl_string_init(&conv_str);

	/* character-set, transfer-encoding */
	tran_cs = mbfl_no_encoding_utf8;
	head_enc = mbfl_no_encoding_base64;
	body_enc = mbfl_no_encoding_base64;
	lang = mbfl_no2language(MBSTRG(current_language));
	if (lang != NULL) {
		tran_cs = lang->mail_charset;
		head_enc = lang->mail_header_encoding;
		body_enc = lang->mail_body_encoding;
	}

	argc = ZEND_NUM_ARGS();
	if (argc < 3 || argc > 5 || zend_get_parameters_array_ex(argc, argv) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	/* To: */
	convert_to_string_ex(argv[0]);
	if (Z_STRVAL_PP(argv[0])) {
		to = Z_STRVAL_PP(argv[0]);
		to_len = Z_STRLEN_PP(argv[0]);
		/* ASCIIZ check */
		MAIL_ASCIIZ_CHECK_MBSTRING(to, to_len);
		if (to_len > 0) {
			to_r = estrndup(to, to_len);
			for (; to_len; to_len--) {
				if (!isspace((unsigned char) to_r[to_len - 1])) {
					break;
				}
				to_r[to_len - 1] = '\0';
			}
			for (i = 0; to_r[i]; i++) {
				if (iscntrl((unsigned char) to_r[i])) {
						/* According to RFC 822, section 3.1.1 long headers may be
separated into
					 * parts using CRLF followed at least one linear-white-space
character ('\t' or ' ').
					 * To prevent these separators from being replaced with a space,
we use the
					 * SKIP_LONG_HEADER_SEP_MBSTRING to skip over them.
					 */
					SKIP_LONG_HEADER_SEP_MBSTRING(to_r, i);
					to_r[i] = ' ';
				}
			}
		} else {
			to_r = to;
		}
	} else {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Missing To: field");
		err = 1;
	}

	/* Subject: */
	convert_to_string_ex(argv[1]);
	if (Z_STRVAL_PP(argv[1])) {
		orig_str.no_language = MBSTRG(current_language);
		orig_str.val = (unsigned char *)Z_STRVAL_PP(argv[1]);
		orig_str.len = Z_STRLEN_PP(argv[1]);
		/* ASCIIZ check */
		MAIL_ASCIIZ_CHECK_MBSTRING(orig_str.val, orig_str.len);
		orig_str.no_encoding = MBSTRG(current_internal_encoding);
		if (orig_str.no_encoding == mbfl_no_encoding_invalid
		    || orig_str.no_encoding == mbfl_no_encoding_pass) {
			orig_str.no_encoding = mbfl_identify_encoding_no(&orig_str, MBSTRG(current_detect_order_list), MBSTRG(current_detect_order_list_size), 0);
		}
		pstr = mbfl_mime_header_encode(&orig_str, &conv_str, tran_cs, head_enc, "\n", sizeof("Subject: [PHP-jp nnnnnnnn]"));
		if (pstr != NULL) {
			subject_buf = subject = (char *)pstr->val;
		} else {
			subject = Z_STRVAL_PP(argv[1]);
		}
	} else {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Missing Subject: field");
		err = 1;
	}

	/* message body */
	convert_to_string_ex(argv[2]);
	if (Z_STRVAL_PP(argv[2])) {
		orig_str.no_language = MBSTRG(current_language);
		orig_str.val = Z_STRVAL_PP(argv[2]);
		orig_str.len = Z_STRLEN_PP(argv[2]);
		/* ASCIIZ check */
		MAIL_ASCIIZ_CHECK_MBSTRING(orig_str.val, orig_str.len);
		orig_str.no_encoding = MBSTRG(current_internal_encoding);

		if (orig_str.no_encoding == mbfl_no_encoding_invalid
		    || orig_str.no_encoding == mbfl_no_encoding_pass) {
			orig_str.no_encoding = mbfl_identify_encoding_no(&orig_str, MBSTRG(current_detect_order_list), MBSTRG(current_detect_order_list_size), 0);
		}

		pstr = NULL;
		{
			mbfl_string tmpstr;

			if (mbfl_convert_encoding(&orig_str, &tmpstr, tran_cs) != NULL) {
				tmpstr.no_encoding=mbfl_no_encoding_8bit;
				pstr = mbfl_convert_encoding(&tmpstr, &conv_str, body_enc);
				efree(tmpstr.val);
			}
		}
		if (pstr != NULL) {
			message_buf = message = (char *)pstr->val;
		} else {
			message = estrndup(Z_STRVAL_PP(argv[2]), Z_STRLEN_PP(argv[2]));
		}
	} else {
		/* this is not really an error, so it is allowed. */
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Empty message body");
		message = NULL;
	}

	/* other headers */
#define PHP_MBSTR_MAIL_MIME_HEADER1 "Mime-Version: 1.0\nContent-Type: text/plain"
#define PHP_MBSTR_MAIL_MIME_HEADER2 "; charset="
#define PHP_MBSTR_MAIL_MIME_HEADER3 "\nContent-Transfer-Encoding: "
	if (argc >= 4) {
		convert_to_string_ex(argv[3]);
		p = Z_STRVAL_PP(argv[3]);
		n = Z_STRLEN_PP(argv[3]);
		/* ASCIIZ check */
		MAIL_ASCIIZ_CHECK_MBSTRING(p, n);
		mbfl_memory_device_strncat(&device, p, n);
		if (p[n - 1] != '\n') {
			mbfl_memory_device_strncat(&device, "\n", 1);
		}
	}
	mbfl_memory_device_strncat(&device, PHP_MBSTR_MAIL_MIME_HEADER1, sizeof(PHP_MBSTR_MAIL_MIME_HEADER1) - 1);
	p = (char *)mbfl_no2preferred_mime_name(tran_cs);
	if (p != NULL) {
		mbfl_memory_device_strncat(&device, PHP_MBSTR_MAIL_MIME_HEADER2, sizeof(PHP_MBSTR_MAIL_MIME_HEADER2) - 1);
		mbfl_memory_device_strcat(&device, p);
	}
	mbfl_memory_device_strncat(&device, PHP_MBSTR_MAIL_MIME_HEADER3, sizeof(PHP_MBSTR_MAIL_MIME_HEADER3) - 1);
	p = (char *)mbfl_no2preferred_mime_name(body_enc);
	if (p == NULL) {
		p = "7bit";
	}
	mbfl_memory_device_strcat(&device, p);
	mbfl_memory_device_output('\0', &device);
	headers = (char *)device.buffer;

	if (argc == 5) {	/* extra options that get passed to the mailer */
		convert_to_string_ex(argv[4]);
		extra_cmd = Z_STRVAL_PP(argv[4]);
		extra_cmd_len = Z_STRLEN_PP(argv[4]);
		/* ASCIIZ check */
		MAIL_ASCIIZ_CHECK_MBSTRING(extra_cmd, extra_cmd_len);
	}

	if (extra_cmd) {
		extra_cmd = php_escape_shell_cmd(extra_cmd);
	} 

	if (!err && php_mail(to_r, subject, message, headers, extra_cmd TSRMLS_CC)) {
		RETVAL_TRUE;
	} else {
		RETVAL_FALSE;
	}

	if (to_r != to) {
		efree(to_r);
	}
	if (extra_cmd) {
		efree(extra_cmd);
	}
	if (subject_buf) {
		efree((void *)subject_buf);
	}
	if (message_buf) {
		efree((void *)message_buf);
	}
	mbfl_memory_device_clear(&device);
}

#else	/* HAVE_SENDMAIL */

PHP_FUNCTION(mb_send_mail)
{
	RETURN_FALSE;
}

#endif	/* HAVE_SENDMAIL */

/* }}} */

/* {{{ proto mixed mb_get_info([string type])
   Returns the current settings of mbstring */
PHP_FUNCTION(mb_get_info)
{
	char *typ = NULL;
	int typ_len;
	char *name;
	const struct mb_overload_def *over_func;
	zval *row;
	const mbfl_language *lang = mbfl_no2language(MBSTRG(current_language));

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &typ, &typ_len) == FAILURE) {
		RETURN_FALSE;
	}

	if (!typ || !strcasecmp("all", typ)) {
		array_init(return_value);
		if ((name = (char *)mbfl_no_encoding2name(MBSTRG(current_internal_encoding))) != NULL) {
			add_assoc_string(return_value, "internal_encoding", name, 1);
		}
		if ((name = (char *)mbfl_no_encoding2name(MBSTRG(http_input_identify))) != NULL) {
			add_assoc_string(return_value, "http_input", name, 1);
		}
		if ((name = (char *)mbfl_no_encoding2name(MBSTRG(current_http_output_encoding))) != NULL) {
			add_assoc_string(return_value, "http_output", name, 1);
		}
		if (MBSTRG(func_overload)){
			over_func = &(mb_ovld[0]);
			MAKE_STD_ZVAL(row);
			array_init(row);
			while (over_func->type > 0) {
				if ((MBSTRG(func_overload) & over_func->type) == over_func->type ) {
					add_assoc_string(row, over_func->orig_func, over_func->ovld_func, 1);
				}
				over_func++;
			}
			add_assoc_zval(return_value, "func_overload", row);
		} else {
			add_assoc_string(return_value, "func_overload", "no overload", 1);
 		}
		if (lang != NULL) {
			if ((name = (char *)mbfl_no_encoding2name(lang->mail_charset)) != NULL) {
				add_assoc_string(return_value, "mail_charset", name, 1);
			}
			if ((name = (char *)mbfl_no_encoding2name(lang->mail_header_encoding)) != NULL) {
				add_assoc_string(return_value, "mail_header_encoding", name, 1);
			}
			if ((name = (char *)mbfl_no_encoding2name(lang->mail_body_encoding)) != NULL) {
				add_assoc_string(return_value, "mail_body_encoding", name, 1);
			}
		}
	} else if (!strcasecmp("internal_encoding", typ)) {
		if ((name = (char *)mbfl_no_encoding2name(MBSTRG(current_internal_encoding))) != NULL) {
			RETVAL_STRING(name, 1);
		}		
	} else if (!strcasecmp("http_input", typ)) {
		if ((name = (char *)mbfl_no_encoding2name(MBSTRG(http_input_identify))) != NULL) {
			RETVAL_STRING(name, 1);
		}		
	} else if (!strcasecmp("http_output", typ)) {
		if ((name = (char *)mbfl_no_encoding2name(MBSTRG(current_http_output_encoding))) != NULL) {
			RETVAL_STRING(name, 1);
		}		
	} else if (!strcasecmp("func_overload", typ)) {
			if (MBSTRG(func_overload)){
				over_func = &(mb_ovld[0]);
				array_init(return_value);
				while (over_func->type > 0) {
					if ((MBSTRG(func_overload) & over_func->type) == over_func->type ) {
						add_assoc_string(return_value, over_func->orig_func, over_func->ovld_func, 1);
					}
					over_func++;
				}
			} else {
				RETVAL_STRING("no overload", 1);
			}
	} else if (!strcasecmp("mail_charset", typ)) {
		if (lang != NULL && (name = (char *)mbfl_no_encoding2name(lang->mail_charset)) != NULL) {
			RETVAL_STRING(name, 1);
		}
	} else if (!strcasecmp("mail_header_encoding", typ)) {
		if (lang != NULL && (name = (char *)mbfl_no_encoding2name(lang->mail_header_encoding)) != NULL) {
			RETVAL_STRING(name, 1);
		}
	} else if (!strcasecmp("mail_body_encoding", typ)) {
		if (lang != NULL && (name = (char *)mbfl_no_encoding2name(lang->mail_body_encoding)) != NULL) {
			RETVAL_STRING(name, 1);
		}
 	} else if (!strcasecmp("illegal_chars", typ)) {
 		RETVAL_LONG(MBSTRG(illegalchars));
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto bool mb_check_encoding([string var[, string encoding]])
   Check if the string is valid for the specified encoding */
PHP_FUNCTION(mb_check_encoding)
{
	char *var = NULL;
	int var_len;
	char *enc = NULL;
	int enc_len;
	mbfl_buffer_converter *convd;
	enum mbfl_no_encoding no_encoding = MBSTRG(current_internal_encoding);
	mbfl_string string, result, *ret = NULL;
	long illegalchars = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|ss", &var, &var_len, &enc, &enc_len) == FAILURE) {
		RETURN_FALSE;
	}

	if (var == NULL) {
		RETURN_BOOL(MBSTRG(illegalchars) == 0);
	}

	if (enc != NULL) {
		no_encoding = mbfl_name2no_encoding(enc);
		if (no_encoding == mbfl_no_encoding_invalid || no_encoding == mbfl_no_encoding_pass) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid encoding \"%s\"", enc);
			RETURN_FALSE;
		}
	}
	
	convd = mbfl_buffer_converter_new(no_encoding, no_encoding, 0);
	if (convd == NULL) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to create converter");
		RETURN_FALSE;
	}	
	mbfl_buffer_converter_illegal_mode(convd, MBSTRG(current_filter_illegal_mode));
	mbfl_buffer_converter_illegal_substchar(convd, MBSTRG(current_filter_illegal_substchar));	
	
	/* initialize string */
	mbfl_string_init(&string);
	mbfl_string_init(&result);
	string.no_encoding = no_encoding;
	string.no_language = MBSTRG(current_language);

	string.val = (unsigned char *)var;
	string.len = var_len;
	ret = mbfl_buffer_converter_feed_result(convd, &string, &result);
	illegalchars = mbfl_buffer_illegalchars(convd);
	mbfl_buffer_converter_delete(convd);

	if (ret != NULL) {
		MBSTRG(illegalchars) += illegalchars;
		if (illegalchars == 0 && strncmp(string.val, ret->val, string.len) == 0) {
			efree(ret->val);
			RETURN_TRUE;
		} else {
			efree(ret->val);
			RETURN_FALSE;
		}
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ MBSTRING_API int php_mb_encoding_translation() */
MBSTRING_API int php_mb_encoding_translation(TSRMLS_D) 
{
	return MBSTRG(encoding_translation);
}
/* }}} */

/* {{{ MBSTRING_API size_t php_mb_mbchar_bytes_ex() */
MBSTRING_API size_t php_mb_mbchar_bytes_ex(const char *s, const mbfl_encoding *enc)
{
	if (enc != NULL) {
		if (enc->flag & MBFL_ENCTYPE_MBCS) {
			if (enc->mblen_table != NULL) {
				if (s != NULL) return enc->mblen_table[*(unsigned char *)s];
			}
		} else if (enc->flag & (MBFL_ENCTYPE_WCS2BE | MBFL_ENCTYPE_WCS2LE)) {
			return 2;
		} else if (enc->flag & (MBFL_ENCTYPE_WCS4BE | MBFL_ENCTYPE_WCS4LE)) {
			return 4;
		}
	}
	return 1;
}
/* }}} */

/* {{{ MBSTRING_API size_t php_mb_mbchar_bytes() */
MBSTRING_API size_t php_mb_mbchar_bytes(const char *s TSRMLS_DC)
{
	return php_mb_mbchar_bytes_ex(s,
		mbfl_no2encoding(MBSTRG(internal_encoding)));
}
/* }}} */

/* {{{ MBSTRING_API char *php_mb_safe_strrchr_ex() */
MBSTRING_API char *php_mb_safe_strrchr_ex(const char *s, unsigned int c, size_t nbytes, const mbfl_encoding *enc)
{
	register const char *p = s;
	char *last=NULL;

	if (nbytes == (size_t)-1) {
		size_t nb = 0;

		while (*p != '\0') {
			if (nb == 0) {
				if ((unsigned char)*p == (unsigned char)c) {
					last = (char *)p;
				}
				nb = php_mb_mbchar_bytes_ex(p, enc);
				if (nb == 0) {
					return NULL; /* something is going wrong! */
				}
			}
			--nb;
			++p;
		}
	} else {
		register size_t bcnt = nbytes;
		register size_t nbytes_char;
		while (bcnt > 0) {
			if ((unsigned char)*p == (unsigned char)c) {
				last = (char *)p;
			}
			nbytes_char = php_mb_mbchar_bytes_ex(p, enc);
			if (bcnt < nbytes_char) {
				return NULL;
			}
			p += nbytes_char;
			bcnt -= nbytes_char;
		}
	}
	return last;
}
/* }}} */

/* {{{ MBSTRING_API char *php_mb_safe_strrchr() */
MBSTRING_API char *php_mb_safe_strrchr(const char *s, unsigned int c, size_t nbytes TSRMLS_DC)
{
	return php_mb_safe_strrchr_ex(s, c, nbytes,
		mbfl_no2encoding(MBSTRG(internal_encoding)));
}
/* }}} */

/* {{{ MBSTRING_API char *php_mb_strrchr() */
MBSTRING_API char *php_mb_strrchr(const char *s, char c TSRMLS_DC)
{
	return php_mb_safe_strrchr(s, c, -1 TSRMLS_CC);
}
/* }}} */

/* {{{ MBSTRING_API size_t php_mb_gpc_mbchar_bytes() */
MBSTRING_API size_t php_mb_gpc_mbchar_bytes(const char *s TSRMLS_DC)
{

	if (MBSTRG(http_input_identify) != mbfl_no_encoding_invalid){
		return php_mb_mbchar_bytes_ex(s,
    		mbfl_no2encoding(MBSTRG(http_input_identify)));
	} else {
		return php_mb_mbchar_bytes_ex(s,
	    	mbfl_no2encoding(MBSTRG(internal_encoding)));
	}
}
/* }}} */

/*	{{{ MBSTRING_API int php_mb_gpc_encoding_converter() */
MBSTRING_API int php_mb_gpc_encoding_converter(char **str, int *len, int num, const char *encoding_to, const char *encoding_from 
		TSRMLS_DC)
{
	int i;
	mbfl_string string, result, *ret = NULL;
	enum mbfl_no_encoding from_encoding, to_encoding;
	mbfl_buffer_converter *convd;

	if (encoding_to) {
		/* new encoding */
		to_encoding = mbfl_name2no_encoding(encoding_to);
		if (to_encoding == mbfl_no_encoding_invalid) {
			return -1;
		}
	} else {
		to_encoding = MBSTRG(current_internal_encoding);
	}	
	if (encoding_from) {
		/* old encoding */
		from_encoding = mbfl_name2no_encoding(encoding_from);
		if (from_encoding == mbfl_no_encoding_invalid) {
			return -1;
		}
	} else {
		from_encoding = MBSTRG(http_input_identify);
	}

	if (from_encoding == mbfl_no_encoding_pass) {
		return 0;
	}

	/* initialize string */
	mbfl_string_init(&string);
	mbfl_string_init(&result);
	string.no_encoding = from_encoding;
	string.no_language = MBSTRG(current_language);

	for (i=0; i<num; i++){
		string.val = (char*)str[i];
		string.len = len[i];

		/* initialize converter */
		convd = mbfl_buffer_converter_new(from_encoding, to_encoding, string.len);
		if (convd == NULL) {
			return -1;
		}
		mbfl_buffer_converter_illegal_mode(convd, MBSTRG(current_filter_illegal_mode));
		mbfl_buffer_converter_illegal_substchar(convd, MBSTRG(current_filter_illegal_substchar));
		
		/* do it */
		ret = mbfl_buffer_converter_feed_result(convd, &string, &result);
		if (ret != NULL) {
			efree(str[i]);
			str[i] = ret->val;
			len[i] = ret->len;
		}
		MBSTRG(illegalchars) += mbfl_buffer_illegalchars(convd);
		mbfl_buffer_converter_delete(convd);
	}
	return ret ? 0 : -1;
}

/* {{{ MBSTRING_API int php_mb_gpc_encoding_detector()
 */
MBSTRING_API int php_mb_gpc_encoding_detector(char **arg_string, int *arg_length, int num, char *arg_list TSRMLS_DC)
{
	mbfl_string string;
	enum mbfl_no_encoding *elist;
	enum mbfl_no_encoding encoding = mbfl_no_encoding_invalid;
	mbfl_encoding_detector *identd = NULL; 

	int size;
	enum mbfl_no_encoding *list;

	if (MBSTRG(http_input_list_size) == 1 && 
		MBSTRG(http_input_list)[0] == mbfl_no_encoding_pass) {
		MBSTRG(http_input_identify) = mbfl_no_encoding_pass;
		return SUCCESS;
	}

	if (MBSTRG(http_input_list_size) == 1 && 
		MBSTRG(http_input_list)[0] != mbfl_no_encoding_auto &&
		mbfl_no_encoding2name(MBSTRG(http_input_list)[0]) != NULL) {
		MBSTRG(http_input_identify) = MBSTRG(http_input_list)[0];
		return SUCCESS;
	}

	if (arg_list && strlen(arg_list)>0) {
		/* make encoding list */
		list = NULL;
		size = 0;
		php_mb_parse_encoding_list(arg_list, strlen(arg_list), &list, &size, 0 TSRMLS_CC);
		
		if (size > 0 && list != NULL) {
			elist = list;
		} else {
			elist = MBSTRG(current_detect_order_list);
			size = MBSTRG(current_detect_order_list_size);
			if (size <= 0){
				elist = MBSTRG(default_detect_order_list);
				size = MBSTRG(default_detect_order_list_size);
			}
		}
	} else {
		elist = MBSTRG(current_detect_order_list);
		size = MBSTRG(current_detect_order_list_size);
		if (size <= 0){
			elist = MBSTRG(default_detect_order_list);
			size = MBSTRG(default_detect_order_list_size);
		}
	}

	mbfl_string_init(&string);
	string.no_language = MBSTRG(current_language);

	identd = mbfl_encoding_detector_new(elist, size, 0);

	if (identd) {
		int n = 0;
		while(n < num){
			string.val = (unsigned char *)arg_string[n];
			string.len = arg_length[n];
			if (mbfl_encoding_detector_feed(identd, &string)) {
				break;
			}
			n++;
		}
		encoding = mbfl_encoding_detector_judge(identd);
		mbfl_encoding_detector_delete(identd);
	}

	if (encoding != mbfl_no_encoding_invalid) {
		MBSTRG(http_input_identify) = encoding;
		return SUCCESS;
	} else {
		return FAILURE;
	}
}
/* }}} */

#ifdef ZEND_MULTIBYTE
/* {{{ MBSTRING_API int php_mb_set_zend_encoding() */
MBSTRING_API int php_mb_set_zend_encoding(TSRMLS_D)
{
	/* 'd better use mbfl_memory_device? */
	char *name, *list = NULL;
	int n, *entry, list_size = 0;
	zend_encoding_detector encoding_detector;
	zend_encoding_converter encoding_converter;
	zend_multibyte_oddlen multibyte_oddlen;

	/* notify script encoding to Zend Engine */
	entry = MBSTRG(script_encoding_list);
	n = MBSTRG(script_encoding_list_size);
	while (n > 0) {
		name = (char *)mbfl_no_encoding2name(*entry);
		if (name) {
			list_size += strlen(name) + 1;
			if (!list) {
				list = (char*)emalloc(list_size);
				if (!list) {
					return -1;
				}
				*list = (char)NULL;
			} else {
				list = (char*)erealloc(list, list_size);
				if (!list) {
					return -1;
				}
				strcat(list, ",");
			}
			strcat(list, name);
		}
		entry++;
		n--;
	}
	zend_multibyte_set_script_encoding(list, (list ? strlen(list) : 0) TSRMLS_CC);
	if (list) {
		efree(list);
	}
	encoding_detector = php_mb_encoding_detector;
	encoding_converter = NULL;
	multibyte_oddlen = php_mb_oddlen;

	if (MBSTRG(encoding_translation)) {
		/* notify internal encoding to Zend Engine */
		name = (char*)mbfl_no_encoding2name(MBSTRG(current_internal_encoding));
		zend_multibyte_set_internal_encoding(name, strlen(name) TSRMLS_CC);

		encoding_converter = php_mb_encoding_converter;
	}

	zend_multibyte_set_functions(encoding_detector, encoding_converter,
			multibyte_oddlen TSRMLS_CC);

	return 0;
}
/* }}} */

/* {{{ char *php_mb_encoding_detector()
 * Interface for Zend Engine
 */
char* php_mb_encoding_detector(const char *arg_string, int arg_length, char *arg_list TSRMLS_DC)
{
	mbfl_string string;
	const char *ret;
	enum mbfl_no_encoding *elist;
	int size, *list;

	/* make encoding list */
	list = NULL;
	size = 0;
	php_mb_parse_encoding_list(arg_list, strlen(arg_list), &list, &size, 0 TSRMLS_CC);
	if (size <= 0) {
		return NULL;
	}
	if (size > 0 && list != NULL) {
		elist = list;
	} else {
		elist = MBSTRG(current_detect_order_list);
		size = MBSTRG(current_detect_order_list_size);
	}

	mbfl_string_init(&string);
	string.no_language = MBSTRG(current_language);
	string.val = (char*)arg_string;
	string.len = arg_length;
	ret = mbfl_identify_encoding_name(&string, elist, size, 0);
	if (list != NULL) {
		efree((void *)list);
	}
	if (ret != NULL) {
		return estrdup(ret);
	} else {
		return NULL;
	}
}
/* }}} */

/*	{{{ int php_mb_encoding_converter() */
int php_mb_encoding_converter(char **to, int *to_length, const char *from,
		int from_length, const char *encoding_to, const char *encoding_from 
		TSRMLS_DC)
{
	mbfl_string string, result, *ret;
	enum mbfl_no_encoding from_encoding, to_encoding;
	mbfl_buffer_converter *convd;

	/* new encoding */
	to_encoding = mbfl_name2no_encoding(encoding_to);
	if (to_encoding == mbfl_no_encoding_invalid) {
		return -1;
	}	
	/* old encoding */
	from_encoding = mbfl_name2no_encoding(encoding_from);
	if (from_encoding == mbfl_no_encoding_invalid) {
		return -1;
	}
	/* initialize string */
	mbfl_string_init(&string);
	mbfl_string_init(&result);
	string.no_encoding = from_encoding;
	string.no_language = MBSTRG(current_language);
	string.val = (char*)from;
	string.len = from_length;

	/* initialize converter */
	convd = mbfl_buffer_converter_new(from_encoding, to_encoding, string.len);
	if (convd == NULL) {
		return -1;
	}
	mbfl_buffer_converter_illegal_mode(convd, MBSTRG(current_filter_illegal_mode));
	mbfl_buffer_converter_illegal_substchar(convd, MBSTRG(current_filter_illegal_substchar));

	/* do it */
	ret = mbfl_buffer_converter_feed_result(convd, &string, &result);
	if (ret != NULL) {
		*to = ret->val;
		*to_length = ret->len;
	}
	MBSTRG(illegalchars) += mbfl_buffer_illegalchars(convd);
	mbfl_buffer_converter_delete(convd);

	return ret ? 0 : -1;
}
/* }}} */

/* {{{ int php_mb_oddlen()
 *	returns number of odd (e.g. appears only first byte of multibyte
 *	character) chars
 */
int php_mb_oddlen(const char *string, int length, const char *encoding TSRMLS_DC)
{
	mbfl_string mb_string;

	mbfl_string_init(&mb_string);
	mb_string.no_language = MBSTRG(current_language);
	mb_string.no_encoding = mbfl_name2no_encoding(encoding);
	mb_string.val = (char*)string;
	mb_string.len = length;

	if (mb_string.no_encoding == mbfl_no_encoding_invalid) {
		return 0;
	}
	return mbfl_oddlen(&mb_string);
}
/* }}} */
#endif /* ZEND_MULTIBYTE */

#endif	/* HAVE_MBSTRING */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: fdm=marker
 * vim: noet sw=4 ts=4
 */
