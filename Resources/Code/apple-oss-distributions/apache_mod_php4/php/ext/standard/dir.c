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
   | Author: Thies C. Arntzen <thies@thieso.net>                          |
   +----------------------------------------------------------------------+
 */

/* $Id: dir.c,v 1.109.2.18.2.9 2007/12/31 07:22:52 sebastian Exp $ */

/* {{{ includes/startup/misc */

#include "php.h"
#include "fopen_wrappers.h"
#include "file.h"
#include "php_dir.h"
#include "php_string.h"

#ifdef HAVE_DIRENT_H
# include <dirent.h>
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <errno.h>

#ifdef PHP_WIN32
#include "win32/readdir.h"
#endif

#ifdef HAVE_GLOB
#ifndef PHP_WIN32
#include <glob.h>
#else
#include "win32/glob.h"
#endif
#endif

typedef struct {
	int default_dir;
} php_dir_globals;

#ifdef ZTS
#define DIRG(v) TSRMG(dir_globals_id, php_dir_globals *, v)
int dir_globals_id;
#else
#define DIRG(v) (dir_globals.v)
php_dir_globals dir_globals;
#endif

#if 0
typedef struct {
	int id;
	DIR *dir;
} php_dir;

static int le_dirp;
#endif

static zend_class_entry *dir_class_entry_ptr;

#define FETCH_DIRP() \
	if (ZEND_NUM_ARGS() == 0) { \
		myself = getThis(); \
		if (myself) { \
			if (zend_hash_find(Z_OBJPROP_P(myself), "handle", sizeof("handle"), (void **)&tmp) == FAILURE) { \
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to find my handle property"); \
				RETURN_FALSE; \
			} \
			ZEND_FETCH_RESOURCE(dirp, php_stream *, tmp, -1, "Directory", php_file_le_stream()); \
		} else { \
			ZEND_FETCH_RESOURCE(dirp, php_stream *, 0, DIRG(default_dir), "Directory", php_file_le_stream()); \
		} \
	} else if ((ZEND_NUM_ARGS() != 1) || zend_get_parameters_ex(1, &id) == FAILURE) { \
		WRONG_PARAM_COUNT; \
	} else { \
		dirp = (php_stream *) zend_fetch_resource(id TSRMLS_CC, -1, "Directory", NULL, 1, php_file_le_stream()); \
		if (!dirp) \
			RETURN_FALSE; \
	} 

static zend_function_entry php_dir_class_functions[] = {
	PHP_FALIAS(close,	closedir,	NULL)
	PHP_FALIAS(rewind,	rewinddir,	NULL)
	PHP_STATIC_FE("read", php_if_readdir, NULL)
	{NULL, NULL, NULL}
};


static void php_set_default_dir(int id TSRMLS_DC)
{
	if (DIRG(default_dir)!=-1) {
		zend_list_delete(DIRG(default_dir));
	}

	if (id != -1) {
		zend_list_addref(id);
	}
	
	DIRG(default_dir) = id;
}

PHP_RINIT_FUNCTION(dir)
{
	DIRG(default_dir) = -1;
	return SUCCESS;
}

PHP_MINIT_FUNCTION(dir)
{
	static char dirsep_str[2], pathsep_str[2];
	zend_class_entry dir_class_entry;

	INIT_CLASS_ENTRY(dir_class_entry, "Directory", php_dir_class_functions);
	dir_class_entry_ptr = zend_register_internal_class(&dir_class_entry TSRMLS_CC);

#ifdef ZTS
	ts_allocate_id(&dir_globals_id, sizeof(php_dir_globals), NULL, NULL);
#endif

	dirsep_str[0] = DEFAULT_SLASH;
	dirsep_str[1] = '\0';
	REGISTER_STRING_CONSTANT("DIRECTORY_SEPARATOR", dirsep_str, CONST_CS|CONST_PERSISTENT);

	pathsep_str[0] = ZEND_PATHS_SEPARATOR;
	pathsep_str[1] = '\0';
	REGISTER_STRING_CONSTANT("PATH_SEPARATOR", pathsep_str, CONST_CS|CONST_PERSISTENT);

#ifdef HAVE_GLOB
#ifdef GLOB_BRACE
	REGISTER_LONG_CONSTANT("GLOB_BRACE", GLOB_BRACE, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef GLOB_MARK
	REGISTER_LONG_CONSTANT("GLOB_MARK", GLOB_MARK, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef GLOB_NOSORT
	REGISTER_LONG_CONSTANT("GLOB_NOSORT", GLOB_NOSORT, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef GLOB_NOCHECK
	REGISTER_LONG_CONSTANT("GLOB_NOCHECK", GLOB_NOCHECK, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef GLOB_NOESCAPE
	REGISTER_LONG_CONSTANT("GLOB_NOESCAPE", GLOB_NOESCAPE, CONST_CS | CONST_PERSISTENT);
#endif

#ifndef GLOB_ONLYDIR
#define GLOB_ONLYDIR (1<<30)
#define GLOB_EMULATE_ONLYDIR
#define GLOB_FLAGMASK (~GLOB_ONLYDIR)
#else
#define GLOB_FLAGMASK (~0)
#endif

	REGISTER_LONG_CONSTANT("GLOB_ONLYDIR", GLOB_ONLYDIR, CONST_CS | CONST_PERSISTENT);

#endif /* HAVE_GLOB */

	return SUCCESS;
}
/* }}} */

/* {{{ internal functions */
static void _php_do_opendir(INTERNAL_FUNCTION_PARAMETERS, int createobject)
{
	pval **arg;
	php_stream *dirp;
	
	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(1, &arg) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string_ex(arg);
	
	dirp = php_stream_opendir(Z_STRVAL_PP(arg), ENFORCE_SAFE_MODE|REPORT_ERRORS, NULL);

	if (dirp == NULL) {
		RETURN_FALSE;
	}
		
	php_set_default_dir(dirp->rsrc_id TSRMLS_CC);

	if (createobject) {
		object_init_ex(return_value, dir_class_entry_ptr);
		add_property_stringl(return_value, "path", Z_STRVAL_PP(arg), Z_STRLEN_PP(arg), 1);
		add_property_resource(return_value, "handle", dirp->rsrc_id);
		php_stream_auto_cleanup(dirp); /* so we don't get warnings under debug */
	} else {
		php_stream_to_zval(dirp, return_value);
	}
}
/* }}} */

/* {{{ proto mixed opendir(string path)
   Open a directory and return a dir_handle */
PHP_FUNCTION(opendir)
{
	_php_do_opendir(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ proto object dir(string directory)
   Directory class with properties, handle and class and methods read, rewind and close */
PHP_FUNCTION(getdir)
{
	_php_do_opendir(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */

/* {{{ proto void closedir([resource dir_handle])
   Close directory connection identified by the dir_handle */
PHP_FUNCTION(closedir)
{
	pval **id, **tmp, *myself;
	php_stream *dirp;

	FETCH_DIRP();

	if (dirp->rsrc_id == DIRG(default_dir)) {
		php_set_default_dir(-1 TSRMLS_CC);
	}

	zend_list_delete(dirp->rsrc_id);
}
/* }}} */

#if defined(HAVE_CHROOT) && !defined(ZTS) && ENABLE_CHROOT_FUNC
/* {{{ proto bool chroot(string directory)
   Change root directory */
PHP_FUNCTION(chroot)
{
	char *str;
	int ret, str_len;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &str, &str_len) == FAILURE) {
		RETURN_FALSE;
	}
	
	ret = chroot(str);
	
	if (ret != 0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s (errno %d)", strerror(errno), errno);
		RETURN_FALSE;
	}

	ret = chdir("/");
	
	if (ret != 0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s (errno %d)", strerror(errno), errno);
		RETURN_FALSE;
	}

	RETURN_TRUE;
}
/* }}} */
#endif

/* {{{ proto bool chdir(string directory)
   Change the current directory */
PHP_FUNCTION(chdir)
{
	char *str;
	int ret, str_len;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &str, &str_len) == FAILURE) {
		RETURN_FALSE;
	}

	if ((PG(safe_mode) && !php_checkuid(str, NULL, CHECKUID_CHECK_FILE_AND_DIR)) || php_check_open_basedir(str TSRMLS_CC)) {
		RETURN_FALSE;
	}
	ret = VCWD_CHDIR(str);
	
	if (ret != 0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s (errno %d)", strerror(errno), errno);
		RETURN_FALSE;
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto mixed getcwd(void)
   Gets the current directory */
PHP_FUNCTION(getcwd)
{
	char path[MAXPATHLEN];
	char *ret=NULL;
	
	if (ZEND_NUM_ARGS() != 0) {
		WRONG_PARAM_COUNT;
	}

#if HAVE_GETCWD
	ret = VCWD_GETCWD(path, MAXPATHLEN);
#elif HAVE_GETWD
	ret = VCWD_GETWD(path);
#endif

	if (ret) {
		RETURN_STRING(path, 1);
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto void rewinddir([resource dir_handle])
   Rewind dir_handle back to the start */
PHP_FUNCTION(rewinddir)
{
	pval **id, **tmp, *myself;
	php_stream *dirp;
	
	FETCH_DIRP();

	php_stream_rewinddir(dirp);
}
/* }}} */

/* {{{ proto string readdir([resource dir_handle])
   Read directory entry from dir_handle */
PHP_NAMED_FUNCTION(php_if_readdir)
{
	pval **id, **tmp, *myself;
	php_stream *dirp;
	php_stream_dirent entry;

	FETCH_DIRP();

	if (php_stream_readdir(dirp, &entry)) {
		RETURN_STRINGL(entry.d_name, strlen(entry.d_name), 1);
	}
	RETURN_FALSE;
}
/* }}} */

#ifdef HAVE_GLOB
/* {{{ proto array glob(string pattern [, int flags])
   Find pathnames matching a pattern */
PHP_FUNCTION(glob)
{
	int cwd_skip = 0;
#ifdef ZTS
	char cwd[MAXPATHLEN];
	char work_pattern[MAXPATHLEN];
	char *result;
#endif
	char *pattern = NULL;
	int pattern_len;
	long flags = 0;
	glob_t globbuf;
	int n, ret;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l", &pattern, &pattern_len, &flags) == FAILURE) 
		return;

#ifdef ZTS 
	if (!IS_ABSOLUTE_PATH(pattern, pattern_len)) {
		result = VCWD_GETCWD(cwd, MAXPATHLEN);	
		if (!result) {
			cwd[0] = '\0';
		}
#ifdef PHP_WIN32
		if (IS_SLASH(*pattern)) {
			cwd[2] = '\0';
		}
#endif
		cwd_skip = strlen(cwd)+1;

		snprintf(work_pattern, MAXPATHLEN, "%s%c%s", cwd, DEFAULT_SLASH, pattern);
		pattern = work_pattern;
	} 
#endif

	if (PG(safe_mode) || (PG(open_basedir) && *PG(open_basedir))) {
		char *dirname = estrdup(pattern);
		php_dirname(dirname, strlen(dirname));

		if (PG(safe_mode) && (!php_checkuid(dirname, NULL, CHECKUID_CHECK_FILE_AND_DIR))) {
			efree(dirname);
			RETURN_FALSE;
		}
		if (php_check_open_basedir(dirname TSRMLS_CC)) {
			efree(dirname);
			RETURN_FALSE;
		}
		efree(dirname);
	}

	memset(&globbuf, 0, sizeof(glob_t));
	globbuf.gl_offs = 0;
	if (0 != (ret = glob(pattern, flags & GLOB_FLAGMASK, NULL, &globbuf))) {
#ifdef GLOB_NOMATCH
		if (GLOB_NOMATCH == ret) {
			/* Linux handles no matches as an error condition, but FreeBSD
			 * doesn't. This ensure that if no match is found, an empty array
			 * is always returned so it can be used without worrying in e.g.
			 * foreach() */
			array_init(return_value);
			return;
		}
#endif
		RETURN_FALSE;
	}

	/* now catch the FreeBSD style of "no matches" */
	if (!globbuf.gl_pathc || !globbuf.gl_pathv) {
		array_init(return_value);
		return;
	}

	array_init(return_value);
	for (n = 0; n < globbuf.gl_pathc; n++) {
		/* we need to this everytime since GLOB_ONLYDIR does not guarantee that
		 * all directories will be filtered. GNU libc documentation states the
		 * following: 
		 * If the information about the type of the file is easily available 
		 * non-directories will be rejected but no extra work will be done to 
		 * determine the information for each file. I.e., the caller must still be 
		 * able to filter directories out. 
		 */
		if (flags & GLOB_ONLYDIR) {
			struct stat s;

			if (0 != VCWD_STAT(globbuf.gl_pathv[n], &s)) {
				continue;
			}

			if (S_IFDIR != (s.st_mode & S_IFMT)) {
				continue;
			}
		}
		add_next_index_string(return_value, globbuf.gl_pathv[n]+cwd_skip, 1);
	}

	globfree(&globbuf);
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
