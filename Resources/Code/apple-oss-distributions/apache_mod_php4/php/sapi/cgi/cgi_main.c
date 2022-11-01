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
   | Authors: Rasmus Lerdorf <rasmus@lerdorf.on.ca>                       |
   |          Stig Bakken <ssb@fast.no>                                   |
   |          Zeev Suraski <zeev@zend.com>                                |
   | FastCGI: Ben Mansell <php@slimyhorror.com>                           |
   |          Shane Caraveo <shane@caraveo.com>                           |
   +----------------------------------------------------------------------+
*/

/* $Id: cgi_main.c,v 1.190.2.68.2.9 2007/12/31 07:22:55 sebastian Exp $ */

#include "php.h"
#include "php_globals.h"
#include "php_variables.h"
#include "zend_modules.h"

#include "SAPI.h"

#include <stdio.h>
#include "php.h"
#ifdef PHP_WIN32
#include "win32/time.h"
#include "win32/signal.h"
#include <process.h>
#endif
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_SIGNAL_H
#include <signal.h>
#endif
#if HAVE_SETLOCALE
#include <locale.h>
#endif
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#include "zend.h"
#include "zend_extensions.h"
#include "php_ini.h"
#include "php_globals.h"
#include "php_main.h"
#include "fopen_wrappers.h"
#include "ext/standard/php_standard.h"
#ifdef PHP_WIN32
#include <io.h>
#include <fcntl.h>
#include "win32/php_registry.h"
#endif

#ifdef __riscos__
#include <unixlib/local.h>
#endif

#include "zend_compile.h"
#include "zend_execute.h"
#include "zend_highlight.h"
#include "zend_indent.h"


#include "php_getopt.h"

#if PHP_FASTCGI
#include "fcgi_config.h"
#include "fcgiapp.h"
/* don't want to include fcgios.h, causes conflicts */
#ifdef PHP_WIN32
extern int OS_SetImpersonate(void);
#else
/* XXX this will need to change later when threaded fastcgi is 
   implemented.  shane */
struct sigaction act, old_term, old_quit, old_int;
#endif

static void (*php_php_import_environment_variables)(zval *array_ptr TSRMLS_DC);

#ifndef PHP_WIN32
/* these globals used for forking children on unix systems */
/**
 * Number of child processes that will get created to service requests
 */
static int children = 0;

/**
 * Set to non-zero if we are the parent process
 */
static int parent = 1;

/**
 * Process group
 */
static pid_t pgroup;
#endif

#endif

#define PHP_MODE_STANDARD	1
#define PHP_MODE_HIGHLIGHT	2
#define PHP_MODE_INDENT		3
#define PHP_MODE_LINT		4
#define PHP_MODE_STRIP		5

static char *php_optarg = NULL;
static int php_optind = 1;

static const opt_struct OPTIONS[] = {
	{'a', 0, "interactive"},
#ifndef PHP_WIN32
	{'b', 1, "bindpath"},
#endif
	{'C', 0, "no-chdir"},
	{'c', 1, "php-ini"},
	{'d', 1, "define"},
	{'e', 0, "profile-info"},
	{'f', 1, "file"},
	{'g', 1, "global"},
	{'h', 0, "help"},
	{'i', 0, "info"},
	{'l', 0, "syntax-check"},
	{'m', 0, "modules"},
	{'n', 0, "no-php-ini"},
	{'q', 0, "no-header"},
	{'s', 0, "syntax-highlight"},
	{'s', 0, "syntax-highlighting"},
	{'w', 0, "strip"},
	{'?', 0, "usage"},/* help alias (both '?' and 'usage') */
	{'v', 0, "version"},
	{'z', 1, "zend-extension"},
	{'-', 0, NULL} /* end of args */
};

#if ENABLE_PATHINFO_CHECK
/* true global.  this is retreived once only, even for fastcgi */
long fix_pathinfo=1;
#endif

#ifdef PHP_WIN32
#define TRANSLATE_SLASHES(path) \
	{ \
		char *tmp = path; \
		while (*tmp) { \
			if (*tmp == '\\') *tmp = '/'; \
			tmp++; \
		} \
	}
#else
#define TRANSLATE_SLASHES(path)
#endif

static int print_module_info(zend_module_entry *module, void *arg TSRMLS_DC)
{
	php_printf("%s\n", module->name);
	return 0;
}

static int module_name_cmp(const void *a, const void *b TSRMLS_DC)
{
	Bucket *f = *((Bucket **) a);
	Bucket *s = *((Bucket **) b);

	return strcasecmp(((zend_module_entry *)f->pData)->name,
				  ((zend_module_entry *)s->pData)->name);
}

static void print_modules(TSRMLS_D)
{
	HashTable sorted_registry;
	zend_module_entry tmp;

	zend_hash_init(&sorted_registry, 50, NULL, NULL, 1);
	zend_hash_copy(&sorted_registry, &module_registry, NULL, &tmp, sizeof(zend_module_entry));
	zend_hash_sort(&sorted_registry, zend_qsort, module_name_cmp, 0 TSRMLS_CC);
	zend_hash_apply_with_argument(&sorted_registry, (apply_func_arg_t) print_module_info, NULL TSRMLS_CC);
	zend_hash_destroy(&sorted_registry);
}

static int print_extension_info(zend_extension *ext, void *arg TSRMLS_DC)
{
	php_printf("%s\n", ext->name);
	return 0;
}

static int extension_name_cmp(const zend_llist_element **f,
							  const zend_llist_element **s TSRMLS_DC)
{
	return strcmp(((zend_extension *)(*f)->data)->name,
				  ((zend_extension *)(*s)->data)->name);
}

static void print_extensions(TSRMLS_D)
{
	zend_llist sorted_exts;

	zend_llist_copy(&sorted_exts, &zend_extensions);
	zend_llist_sort(&sorted_exts, extension_name_cmp TSRMLS_CC);
	zend_llist_apply_with_argument(&sorted_exts, (llist_apply_with_arg_func_t) print_extension_info, NULL TSRMLS_CC);
	zend_llist_destroy(&sorted_exts);
}

#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif

static size_t sapi_cgibin_single_write(const char *str, uint str_length TSRMLS_DC)
{

#if PHP_FASTCGI
	if (!FCGX_IsCGI()) {
		FCGX_Request *request = (FCGX_Request *)SG(server_context);
		long ret = FCGX_PutStr( str, str_length, request->out );
		if (ret <= 0) {
			return 0;
		}
		return ret;
	}
#endif

#ifdef PHP_WRITE_STDOUT
	{
		long ret;

		ret = write(STDOUT_FILENO, str, str_length);
		if (ret <= 0) return 0;
		return ret;
	}
#else
	{
		size_t ret;

		ret = fwrite(str, 1, MIN(str_length, 16384), stdout);
		return ret;
	}
#endif
}

static int sapi_cgibin_ub_write(const char *str, uint str_length TSRMLS_DC)
{
	const char *ptr = str;
	uint remaining = str_length;
	size_t ret;

	while (remaining > 0) {
		ret = sapi_cgibin_single_write(ptr, remaining TSRMLS_CC);
		if (!ret) {
			php_handle_aborted_connection();
			return str_length - remaining;
		}
		ptr += ret;
		remaining -= ret;
	}

	return str_length;
}


static void sapi_cgibin_flush(void *server_context)
{
#if PHP_FASTCGI
	if (!FCGX_IsCGI()) {
		FCGX_Request *request = (FCGX_Request *)server_context;
		if (
#ifndef PHP_WIN32
		!parent && 
#endif
		request && FCGX_FFlush(request->out) == -1) {
			php_handle_aborted_connection();
		}
		return;
	}
#endif
	if (fflush(stdout)==EOF) {
		php_handle_aborted_connection();
	}
}

#define SAPI_CGI_MAX_HEADER_LENGTH 1024

static int sapi_cgi_send_headers(sapi_headers_struct *sapi_headers TSRMLS_DC)
{
	char buf[SAPI_CGI_MAX_HEADER_LENGTH];
	sapi_header_struct *h;
	zend_llist_position pos;
	long rfc2616_headers = 0, nph = 0;

	if(SG(request_info).no_headers == 1) {
		return  SAPI_HEADER_SENT_SUCCESSFULLY;
	}
	/* Check whether to send RFC 2616 style headers compatible with
	 * PHP versions 4.2.3 and earlier compatible with web servers
	 * such as IIS. Default is informal CGI RFC header compatible 
	 * with Apache.
	 */
	if (cfg_get_long("cgi.rfc2616_headers", &rfc2616_headers) == FAILURE) {
		rfc2616_headers = 0;
	}

	if (cfg_get_long("cgi.nph", &nph) == FAILURE) {
		nph = 0;
	}

	if (nph || SG(sapi_headers).http_response_code != 200) {
		int len;
		
		if (rfc2616_headers && SG(sapi_headers).http_status_line) {
			len = snprintf(buf, SAPI_CGI_MAX_HEADER_LENGTH, 
						   "%s\r\n", SG(sapi_headers).http_status_line);

			if (len > SAPI_CGI_MAX_HEADER_LENGTH) {
				len = SAPI_CGI_MAX_HEADER_LENGTH;
			}

		} else {
			len = sprintf(buf, "Status: %d\r\n", SG(sapi_headers).http_response_code);
		}

		PHPWRITE_H(buf, len);
	}

	h = zend_llist_get_first_ex(&sapi_headers->headers, &pos);
	while (h) {
		if (h->header_len) {
			PHPWRITE_H(h->header, h->header_len);
			PHPWRITE_H("\r\n", 2);
		}
		h = zend_llist_get_next_ex(&sapi_headers->headers, &pos);
	}
	PHPWRITE_H("\r\n", 2);

	return SAPI_HEADER_SENT_SUCCESSFULLY;
}


static int sapi_cgi_read_post(char *buffer, uint count_bytes TSRMLS_DC)
{
	int read_bytes=0, tmp_read_bytes;

	count_bytes = MIN(count_bytes, (uint)SG(request_info).content_length-SG(read_post_bytes));
	while (read_bytes < count_bytes) {
#if PHP_FASTCGI
		if (!FCGX_IsCGI()) {
			FCGX_Request *request = (FCGX_Request *)SG(server_context);
			tmp_read_bytes = FCGX_GetStr(buffer+read_bytes, count_bytes-read_bytes, request->in );
		} else {
			tmp_read_bytes = read(0, buffer+read_bytes, count_bytes-read_bytes);
		}
#else
		tmp_read_bytes = read(0, buffer+read_bytes, count_bytes-read_bytes);
#endif

		if (tmp_read_bytes<=0) {
			break;
		}
		read_bytes += tmp_read_bytes;
	}
	return read_bytes;
}

static char *sapi_cgibin_getenv(char *name, size_t name_len TSRMLS_DC)
{
#if PHP_FASTCGI
	/* when php is started by mod_fastcgi, no regular environment
	   is provided to PHP.  It is always sent to PHP at the start
	   of a request.  So we have to do our own lookup to get env
	   vars.  This could probably be faster somehow.  */
	if (!FCGX_IsCGI()) {
		FCGX_Request *request = (FCGX_Request *)SG(server_context);
		return FCGX_GetParam(name,request->envp);
	}
#endif
	/*  if cgi, or fastcgi and not found in fcgi env
		check the regular environment */
	return getenv(name);
}

static char *_sapi_cgibin_putenv(char *name, char *value TSRMLS_DC)
{
	int len=0;
	char *buf = NULL;
	if (!name) {
		return NULL;
	}
	len = strlen(name) + (value?strlen(value):0) + sizeof("=") + 2;
	buf = (char *)malloc(len);
	if (buf == NULL) {
		return getenv(name);
	}
	if (value) {
		snprintf(buf,len-1,"%s=%s", name, value);
	} else {
		snprintf(buf,len-1,"%s=", name);
	}
#if PHP_FASTCGI
	/* when php is started by mod_fastcgi, no regular environment
	   is provided to PHP.  It is always sent to PHP at the start
	   of a request.  So we have to do our own lookup to get env
	   vars.  This could probably be faster somehow.  */
	if (!FCGX_IsCGI()) {
		FCGX_Request *request = (FCGX_Request *)SG(server_context);
		FCGX_PutEnv(request,buf);
		free(buf);
		return sapi_cgibin_getenv(name,0 TSRMLS_CC);
	}
#endif
	/*  if cgi, or fastcgi and not found in fcgi env
		check the regular environment 
		this leaks, but it's only cgi anyway, we'll fix
		it for 5.0
	*/
	putenv(buf);
	return getenv(name);
}

static char *sapi_cgi_read_cookies(TSRMLS_D)
{
	return sapi_cgibin_getenv((char *)"HTTP_COOKIE",0 TSRMLS_CC);
}

#if PHP_FASTCGI
void cgi_php_import_environment_variables(zval *array_ptr TSRMLS_DC)
{
	if (!FCGX_IsCGI()) {
		FCGX_Request *request = (FCGX_Request *)SG(server_context);
		char **env, *p, *t;

		for (env = request->envp; env != NULL && *env != NULL; env++) {
			p = strchr(*env, '=');
			if (!p) {				/* malformed entry? */
				continue;
			}
			t = estrndup(*env, p - *env);
			php_register_variable(t, p+1, array_ptr TSRMLS_CC);
			efree(t);
		}
	}
	/* call php's original import as a catch-all */
	php_php_import_environment_variables(array_ptr TSRMLS_CC);
}
#endif

static void sapi_cgi_register_variables(zval *track_vars_array TSRMLS_DC)
{
	/* In CGI mode, we consider the environment to be a part of the server
	 * variables
	 */
	php_import_environment_variables(track_vars_array TSRMLS_CC);
	/* Build the special-case PHP_SELF variable for the CGI version */
	php_register_variable("PHP_SELF", (SG(request_info).request_uri ? SG(request_info).request_uri:""), track_vars_array TSRMLS_CC);
}


static void sapi_cgi_log_message(char *message)
{
#if PHP_FASTCGI
	long logging = 1;
	TSRMLS_FETCH();

	if (cfg_get_long("fastcgi.logging", &logging) == FAILURE) {
		logging = 1;
	}
                                                                                                        
	if (!FCGX_IsCGI() && logging) {
		FCGX_Request *request = (FCGX_Request *)SG(server_context);
		if (request) {
			FCGX_FPrintF( request->err, "%s\n", message );
		}
		/* ignore return code */
	} else
#endif /* PHP_FASTCGI */
	fprintf(stderr, "%s\n", message);
}

static int sapi_cgi_deactivate(TSRMLS_D)
{
	fflush(stdout);
	if(SG(request_info).argv0) {
		free(SG(request_info).argv0);
		SG(request_info).argv0 = NULL;
	}
	return SUCCESS;
}


static int php_cgi_startup(sapi_module_struct *sapi_module)
{
	if (php_module_startup(sapi_module, NULL, 0) == FAILURE) {
		return FAILURE;
	}
	return SUCCESS;
}

static int sapi_cgi_get_fd(int *fd TSRMLS_DC)
{
#if PHP_FASTCGI
	FCGX_Request *request = (FCGX_Request *)SG(server_context);
	
	*fd = request->ipcFd;
	if (*fd >= 0) return SUCCESS;
	return FAILURE;
#else
	*fd = STDOUT_FILENO;
	return SUCCESS;
#endif
}

static int sapi_cgi_force_http_10(TSRMLS_D)
{
	return SUCCESS;
}

/* {{{ sapi_module_struct cgi_sapi_module
 */
static sapi_module_struct cgi_sapi_module = {
#if PHP_FASTCGI
	"cgi-fcgi",						/* name */
	"CGI/FastCGI",					/* pretty name */
#else
	"cgi",							/* name */
	"CGI",							/* pretty name */
#endif
	
	php_cgi_startup,				/* startup */
	php_module_shutdown_wrapper,	/* shutdown */

	NULL,							/* activate */
	sapi_cgi_deactivate,			/* deactivate */

	sapi_cgibin_ub_write,			/* unbuffered write */
	sapi_cgibin_flush,				/* flush */
	NULL,							/* get uid */
	sapi_cgibin_getenv,				/* getenv */

	php_error,						/* error handler */

	NULL,							/* header handler */
	sapi_cgi_send_headers,			/* send headers handler */
	NULL,							/* send header handler */

	sapi_cgi_read_post,				/* read POST data */
	sapi_cgi_read_cookies,			/* read Cookies */

	sapi_cgi_register_variables,	/* register server variables */
	sapi_cgi_log_message,			/* Log message */

	NULL,							/* php.ini path override */
	NULL,							/* block interruptions */
	NULL,							/* unblock interruptions */
	NULL,							/* default post reader */
	NULL,							/* treat data */
	NULL,							/* executable location */
	
	0,								/* php.ini ignore */

	sapi_cgi_get_fd,				/* get fd */
	sapi_cgi_force_http_10,			/* force HTTP/1.0 */
};
/* }}} */

/* {{{ php_cgi_usage
 */
static void php_cgi_usage(char *argv0)
{
	char *prog;

	prog = strrchr(argv0, '/');
	if (prog) {
		prog++;
	} else {
		prog = "php";
	}

	php_printf("Usage: %s [-q] [-h] [-s] [-v] [-i] [-f <file>] \n"
			   "       %s <file> [args...]\n"
			   "  -a               Run interactively\n"
#if PHP_FASTCGI && !defined(PHP_WIN32)
			   "  -b <address:port>|<port> Bind Path for external FASTCGI Server mode\n"
#endif
			   "  -C               Do not chdir to the script's directory\n"
			   "  -c <path>|<file> Look for php.ini file in this directory\n"
			   "  -n               No php.ini file will be used\n"
			   "  -d foo[=bar]     Define INI entry foo with value 'bar'\n"
			   "  -e               Generate extended information for debugger/profiler\n"
			   "  -f <file>        Parse <file>.  Implies `-q'\n"
			   "  -h               This help\n"
			   "  -i               PHP information\n"
			   "  -l               Syntax check only (lint)\n"
			   "  -m               Show compiled in modules\n"
			   "  -q               Quiet-mode.  Suppress HTTP Header output.\n"
			   "  -s               Display colour syntax highlighted source.\n"
			   "  -v               Version number\n"
			   "  -w               Display source with stripped comments and whitespace.\n"
			   "  -z <file>        Load Zend extension <file>.\n",
			   prog, prog);
}
/* }}} */

/* {{{ init_request_info

  initializes request_info structure

  specificly in this section we handle proper translations
  for:

  PATH_INFO
	derived from the portion of the URI path following 
	the script name but preceding any query data
	may be empty

  PATH_TRANSLATED
    derived by taking any path-info component of the 
	request URI and performing any virtual-to-physical 
	translation appropriate to map it onto the server's 
	document repository structure

	empty if PATH_INFO is empty

	The env var PATH_TRANSLATED **IS DIFFERENT** than the
	request_info.path_translated variable, the latter should
	match SCRIPT_FILENAME instead.

  SCRIPT_NAME
    set to a URL path that could identify the CGI script
	rather than the interpreter.  PHP_SELF is set to this.

  REQUEST_URI
    uri section following the domain:port part of a URI

  SCRIPT_FILENAME
    The virtual-to-physical translation of SCRIPT_NAME (as per 
	PATH_TRANSLATED)

  These settings are documented at
  http://cgi-spec.golux.com/


  Based on the following URL request:
  
  http://localhost/info.php/test?a=b 
 
  should produce, which btw is the same as if
  we were running under mod_cgi on apache (ie. not
  using ScriptAlias directives):
 
  PATH_INFO=/test
  PATH_TRANSLATED=/docroot/test
  SCRIPT_NAME=/info.php
  REQUEST_URI=/info.php/test?a=b
  SCRIPT_FILENAME=/docroot/info.php
  QUERY_STRING=a=b
 
  but what we get is (cgi/mod_fastcgi under apache):
  
  PATH_INFO=/info.php/test
  PATH_TRANSLATED=/docroot/info.php/test
  SCRIPT_NAME=/php/php-cgi  (from the Action setting I suppose)
  REQUEST_URI=/info.php/test?a=b
  SCRIPT_FILENAME=/path/to/php/bin/php-cgi  (Action setting translated)
  QUERY_STRING=a=b
 
  Comments in the code below refer to using the above URL in a request

 */
static void init_request_info(TSRMLS_D)
{
	char *env_script_filename = sapi_cgibin_getenv("SCRIPT_FILENAME",0 TSRMLS_CC);
	char *env_path_translated = sapi_cgibin_getenv("PATH_TRANSLATED",0 TSRMLS_CC);
	char *script_path_translated = env_script_filename;

#if !DISCARD_PATH
	/* some broken servers do not have script_filename or argv0
	   an example, IIS configured in some ways.  then they do more
	   broken stuff and set path_translated to the cgi script location */
	if (!script_path_translated && env_path_translated) {
		script_path_translated = env_path_translated;
	}
#endif

	/* initialize the defaults */
	SG(request_info).path_translated = NULL;
	SG(request_info).request_method = NULL;
	SG(request_info).query_string = NULL;
	SG(request_info).request_uri = NULL;
	SG(request_info).content_type = NULL;
	SG(request_info).content_length = 0;
	SG(sapi_headers).http_response_code = 200;

	/* script_path_translated being set is a good indication that
	   we are running in a cgi environment, since it is always
	   null otherwise.  otherwise, the filename
	   of the script will be retreived later via argc/argv */
	if (script_path_translated) {
		const char *auth;
		char *content_length = sapi_cgibin_getenv("CONTENT_LENGTH",0 TSRMLS_CC);
		char *content_type = sapi_cgibin_getenv("CONTENT_TYPE",0 TSRMLS_CC);
		char *env_path_info = sapi_cgibin_getenv("PATH_INFO",0 TSRMLS_CC);
		char *env_script_name = sapi_cgibin_getenv("SCRIPT_NAME",0 TSRMLS_CC);
#if ENABLE_PATHINFO_CHECK
		struct stat st;
		char *env_redirect_url = sapi_cgibin_getenv("REDIRECT_URL",0 TSRMLS_CC);
		char *env_document_root = sapi_cgibin_getenv("DOCUMENT_ROOT",0 TSRMLS_CC);

		if (fix_pathinfo) {

			/* save the originals first for anything we change later */
			if (env_path_translated) {
				_sapi_cgibin_putenv("ORIG_PATH_TRANSLATED",env_path_translated TSRMLS_CC);
			}
			if (env_path_info) {
				_sapi_cgibin_putenv("ORIG_PATH_INFO",env_path_info TSRMLS_CC);
			}
			if (env_script_name) {
				_sapi_cgibin_putenv("ORIG_SCRIPT_NAME",env_script_name TSRMLS_CC);
			}
			if (env_script_filename) {
				_sapi_cgibin_putenv("ORIG_SCRIPT_FILENAME",env_script_filename TSRMLS_CC);
			}
			if (!env_document_root) {
				/* ini version of document root */
				if (!env_document_root) {
					env_document_root = PG(doc_root);
				}
				/* set the document root, this makes a more
				   consistent env for php scripts */
				if (env_document_root) {
					env_document_root = _sapi_cgibin_putenv("DOCUMENT_ROOT",env_document_root TSRMLS_CC);
					/* fix docroot */
					TRANSLATE_SLASHES(env_document_root);
				}
			}

			if (env_path_translated != NULL && env_redirect_url != NULL) {
				/* 
				   pretty much apache specific.  If we have a redirect_url
				   then our script_filename and script_name point to the
				   php executable
				*/
				script_path_translated = env_path_translated;
				/* we correct SCRIPT_NAME now in case we don't have PATH_INFO */
				env_script_name = _sapi_cgibin_putenv("SCRIPT_NAME",env_redirect_url TSRMLS_CC);
			}

#ifdef __riscos__
			/* Convert path to unix format*/
			__riscosify_control|=__RISCOSIFY_DONT_CHECK_DIR;
			script_path_translated=__unixify(script_path_translated,0,NULL,1,0);
#endif
			
			/*
			 * if the file doesn't exist, try to extract PATH_INFO out
			 * of it by stat'ing back through the '/'
			 * this fixes url's like /info.php/test
			 */
			if (script_path_translated && stat( script_path_translated, &st ) == -1 ) {
				char *pt = estrdup(script_path_translated);
				int len = strlen(pt);
				char *ptr;

				while( (ptr = strrchr(pt,'/')) || (ptr = strrchr(pt,'\\')) ) {
					*ptr = 0;
					if ( stat(pt, &st) == 0 && S_ISREG(st.st_mode) ) {
						/*
						 * okay, we found the base script!
						 * work out how many chars we had to strip off;
						 * then we can modify PATH_INFO
						 * accordingly
						 *
						 * we now have the makings of
						 * PATH_INFO=/test
						 * SCRIPT_FILENAME=/docroot/info.php
						 *
						 * we now need to figure out what docroot is.
						 * if DOCUMENT_ROOT is set, this is easy, otherwise,
						 * we have to play the game of hide and seek to figure
						 * out what SCRIPT_NAME should be
						 */
						int slen = len - strlen(pt);
						int pilen = strlen( env_path_info );
						char *path_info = env_path_info + pilen - slen;

						env_path_info = _sapi_cgibin_putenv("PATH_INFO",path_info TSRMLS_CC);
						script_path_translated = _sapi_cgibin_putenv("SCRIPT_FILENAME",pt TSRMLS_CC);
						TRANSLATE_SLASHES(pt);

						/* figure out docroot
						   SCRIPT_FILENAME minus SCRIPT_NAME
						   */

						if (env_document_root) {
							int l = strlen(env_document_root);
							int path_translated_len = 0;
							char *path_translated = NULL;
							if (l && env_document_root[l-1]=='/') {
								--l;
							}

							/* we have docroot, so we should have:
							 * DOCUMENT_ROOT=/docroot
							 * SCRIPT_FILENAME=/docroot/info.php
							 *
							 * SCRIPT_NAME is the portion of the path beyond docroot
							 */
							env_script_name = _sapi_cgibin_putenv("SCRIPT_NAME",pt+l TSRMLS_CC);

							/*
							 * PATH_TRANSATED = DOCUMENT_ROOT + PATH_INFO
							 */
							path_translated_len = l + strlen(env_path_info) + 2;
							path_translated = (char *)emalloc(path_translated_len);
							*path_translated = 0;
							strncat(path_translated,env_document_root,l);
							strcat(path_translated,env_path_info);
							env_path_translated = _sapi_cgibin_putenv("PATH_TRANSLATED",path_translated TSRMLS_CC);
							efree(path_translated);
						} else if (env_script_name &&
							strstr(pt,env_script_name)) {
							/*
							 * PATH_TRANSATED = PATH_TRANSATED - SCRIPT_NAME + PATH_INFO
							 */
							int ptlen = strlen(pt)-strlen(env_script_name);
							int path_translated_len = ptlen + strlen(env_path_info) + 2;
							char *path_translated = NULL;
							path_translated = (char *)emalloc(path_translated_len);
							*path_translated = 0;
							strncat(path_translated,pt,ptlen);
							strcat(path_translated,env_path_info);
							env_path_translated = _sapi_cgibin_putenv("PATH_TRANSLATED",path_translated TSRMLS_CC);
							efree(path_translated);
						}
						break;
					}
				}
				if (!ptr) {
					/*
					 * if we stripped out all the '/' and still didn't find
					 * a valid path... we will fail, badly. of course we would
					 * have failed anyway... we output 'no input file' now.
					 */
					script_path_translated = _sapi_cgibin_putenv("SCRIPT_FILENAME",NULL TSRMLS_CC);
					SG(sapi_headers).http_response_code = 404;
				}
				if (pt) {
					efree(pt);
				}
			} else {
				/* make sure path_info/translated are empty */
				script_path_translated = _sapi_cgibin_putenv("SCRIPT_FILENAME",script_path_translated TSRMLS_CC);
				_sapi_cgibin_putenv("PATH_INFO", NULL TSRMLS_CC);
				_sapi_cgibin_putenv("PATH_TRANSLATED", NULL TSRMLS_CC);
			}
			SG(request_info).request_uri = sapi_cgibin_getenv("SCRIPT_NAME",0 TSRMLS_CC);
		} else {
#endif
			/* pre 4.3 behaviour, shouldn't be used but provides BC */
			if (env_path_info) {
				SG(request_info).request_uri = env_path_info;
			} else {
				SG(request_info).request_uri = env_script_name;
			}
#if !DISCARD_PATH
			if (env_path_translated)
				script_path_translated = env_path_translated;
#endif
#if ENABLE_PATHINFO_CHECK
		}
#endif
		SG(request_info).request_method = sapi_cgibin_getenv("REQUEST_METHOD",0 TSRMLS_CC);
		SG(request_info).query_string = sapi_cgibin_getenv("QUERY_STRING",0 TSRMLS_CC);
		/* some server configurations allow '..' to slip through in the
		   translated path.   We'll just refuse to handle such a path. */
		if (script_path_translated && !strstr(script_path_translated, "..")) {
			SG(request_info).path_translated = estrdup(script_path_translated);
		}
		SG(request_info).content_type = (content_type ? content_type : "" );
		SG(request_info).content_length = (content_length?atoi(content_length):0);
		
		/* The CGI RFC allows servers to pass on unvalidated Authorization data */
		auth = sapi_cgibin_getenv("HTTP_AUTHORIZATION",0 TSRMLS_CC);
		php_handle_auth_data(auth TSRMLS_CC);
	}
}
/* }}} */

static void define_command_line_ini_entry(char *arg)
{
	char *name, *value;

	name = arg;
	value = strchr(arg, '=');
	if (value) {
		*value = 0;
		value++;
	} else {
		value = "1";
	}
	zend_alter_ini_entry(name, strlen(name)+1, value, strlen(value), PHP_INI_SYSTEM, PHP_INI_STAGE_ACTIVATE);
}


static void php_register_command_line_global_vars(char **arg TSRMLS_DC)
{
	char *var, *val;

	var = *arg;
	val = strchr(var, '=');
	if (!val) {
		printf("No value specified for variable '%s'\n", var);
	} else {
		*val++ = '\0';
		php_register_variable(var, val, NULL TSRMLS_CC);
	}
	efree(*arg);
}

#if PHP_FASTCGI
/**
 * Clean up child processes upon exit
 */
void fastcgi_cleanup(int signal)
{

#ifdef DEBUG_FASTCGI
	fprintf( stderr, "FastCGI shutdown, pid %d\n", getpid() );
#endif

#ifndef PHP_WIN32
	sigaction( SIGTERM, &old_term, 0 );

	/* Kill all the processes in our process group */
	kill( -pgroup, SIGTERM );
#endif

	/* We should exit at this point, but MacOSX doesn't seem to */
	exit( 0 );
}
#endif

/* {{{ main
 */
int main(int argc, char *argv[])
{
	int exit_status = SUCCESS;
	int cgi = 0, c, i, len;
	zend_file_handle file_handle = {0};
	int retval = FAILURE;
	char *s;
/* temporary locals */
	int behavior=PHP_MODE_STANDARD;
	int no_headers=0;
	int orig_optind=php_optind;
	char *orig_optarg=php_optarg;
	char *script_file=NULL;
	zend_llist global_vars;
	int interactive=0;
#if FORCE_CGI_REDIRECT
	long force_redirect = 1;
	char *redirect_status_env = NULL;
#endif

/* end of temporary locals */
#ifdef ZTS
	zend_compiler_globals *compiler_globals;
	zend_executor_globals *executor_globals;
	php_core_globals *core_globals;
	sapi_globals_struct *sapi_globals;
	void ***tsrm_ls;
#endif

#if PHP_FASTCGI
	int max_requests = 500;
	int requests = 0;
	int fastcgi = !FCGX_IsCGI();
#ifndef PHP_WIN32
	char *bindpath = NULL;
#endif
	int fcgi_fd = 0;
	FCGX_Request request;
#ifdef PHP_WIN32
	long impersonate = 0;
#else
    int status = 0;
#endif
#endif /* PHP_FASTCGI */

#ifdef HAVE_SIGNAL_H
#if defined(SIGPIPE) && defined(SIG_IGN)
	signal(SIGPIPE, SIG_IGN); /* ignore SIGPIPE in standalone mode so
								that sockets created via fsockopen()
								don't kill PHP if the remote site
								closes it.  in apache|apxs mode apache
								does that for us!  thies@thieso.net
								20000419 */
#endif
#endif

#ifdef ZTS
	tsrm_startup(1, 1, 0, NULL);
#endif

	sapi_startup(&cgi_sapi_module);

#ifdef PHP_WIN32
	_fmode = _O_BINARY;			/*sets default for file streams to binary */
	setmode(_fileno(stdin), O_BINARY);		/* make the stdio mode be binary */
	setmode(_fileno(stdout), O_BINARY);		/* make the stdio mode be binary */
	setmode(_fileno(stderr), O_BINARY);		/* make the stdio mode be binary */
#endif

#if PHP_FASTCGI
	if (!fastcgi) {
#endif
	/* Make sure we detect we are a cgi - a bit redundancy here,
	   but the default case is that we have to check only the first one. */
	if (getenv("SERVER_SOFTWARE")
		|| getenv("SERVER_NAME")
		|| getenv("GATEWAY_INTERFACE")
		|| getenv("REQUEST_METHOD")) {
		cgi = 1;
	}
#if PHP_FASTCGI
	}
#endif

	if (!cgi
#if PHP_FASTCGI
		/* allow ini override for fastcgi */
#endif
		) {
		while ((c=php_getopt(argc, argv, OPTIONS, &php_optarg, &php_optind, 0))!=-1) {
			switch (c) {
				case 'c':
					cgi_sapi_module.php_ini_path_override = strdup(php_optarg);
					break;
				case 'n':
					cgi_sapi_module.php_ini_ignore = 1;
					break;
#if PHP_FASTCGI
#ifndef PHP_WIN32
				/* if we're started on command line, check to see if
				   we are being started as an 'external' fastcgi
				   server by accepting a bindpath parameter. */
				case 'b':
					if (!fastcgi) {
						bindpath = strdup(php_optarg);
					}
					break;
#endif
#endif
			}

		}
		php_optind = orig_optind;
		php_optarg = orig_optarg;
	}

#ifdef ZTS
	compiler_globals = ts_resource(compiler_globals_id);
	executor_globals = ts_resource(executor_globals_id);
	core_globals = ts_resource(core_globals_id);
	sapi_globals = ts_resource(sapi_globals_id);
	tsrm_ls = ts_resource(0);
	SG(request_info).path_translated = NULL;
#endif

	cgi_sapi_module.executable_location = argv[0];

	/* startup after we get the above ini override se we get things right */
	if (php_module_startup(&cgi_sapi_module, NULL, 0) == FAILURE) {
#ifdef ZTS
		tsrm_shutdown();
#endif
		return FAILURE;
	}

#if FORCE_CGI_REDIRECT
	/* check force_cgi after startup, so we have proper output */
	if (cfg_get_long("cgi.force_redirect", &force_redirect) == FAILURE) {
        force_redirect = 1;
	}
	if (cgi && force_redirect) {
        if (cfg_get_string("cgi.redirect_status_env", &redirect_status_env) == FAILURE) {
            redirect_status_env = NULL;
        }
		/* Apache will generate REDIRECT_STATUS,
		 * Netscape and redirect.so will generate HTTP_REDIRECT_STATUS.
		 * redirect.so and installation instructions available from
		 * http://www.koehntopp.de/php.
		 *   -- kk@netuse.de
		 */
		if (!getenv("REDIRECT_STATUS") 
			&& !getenv ("HTTP_REDIRECT_STATUS")
			/* this is to allow a different env var to be configured
			    in case some server does something different than above */
			&& (!redirect_status_env || !getenv(redirect_status_env))
			) {
			SG(sapi_headers).http_response_code = 400;
			PUTS("<b>Security Alert!</b> The PHP CGI cannot be accessed directly.\n\n\
<p>This PHP CGI binary was compiled with force-cgi-redirect enabled.  This\n\
means that a page will only be served up if the REDIRECT_STATUS CGI variable is\n\
set, e.g. via an Apache Action directive.</p>\n\
<p>For more information as to <i>why</i> this behaviour exists, see the <a href=\"http://php.net/security.cgi-bin\">\
manual page for CGI security</a>.</p>\n\
<p>For more information about changing this behaviour or re-enabling this webserver,\n\
consult the installation file that came with this distribution, or visit \n\
<a href=\"http://php.net/install.windows\">the manual page</a>.</p>\n");

#ifdef ZTS
	        tsrm_shutdown();
#endif

			return FAILURE;
		}
	}
#endif							/* FORCE_CGI_REDIRECT */

#if ENABLE_PATHINFO_CHECK
	if (cfg_get_long("cgi.fix_pathinfo", &fix_pathinfo) == FAILURE) {
		fix_pathinfo = 0;
	}
#endif

#if PHP_FASTCGI
#ifndef PHP_WIN32
	/* for windows, socket listening is broken in the fastcgi library itself
	   so dissabling this feature on windows till time is available to fix it */
	if (bindpath) {
		/* this must be done to make FCGX_OpenSocket work correctly 
		   bug 23664 */
		close(0);
		/* Pass on the arg to the FastCGI library, with one exception.
		 * If just a port is specified, then we prepend a ':' onto the
		 * path (it's what the fastcgi library expects)
		 */
		
		if (strchr(bindpath, ':') == NULL) {
			char *tmp;

			tmp = malloc(strlen(bindpath) + 2);
			tmp[0] = ':';
			memcpy(tmp + 1, bindpath, strlen(bindpath) + 1);

			fcgi_fd = FCGX_OpenSocket(tmp, 128);
			free(tmp);
		} else {
			fcgi_fd = FCGX_OpenSocket(bindpath, 128);
		}
		if( fcgi_fd < 0 ) {
			fprintf(stderr, "Couldn't create FastCGI listen socket on port %s\n", bindpath);
#ifdef ZTS
	        tsrm_shutdown();
#endif
			return FAILURE;
		}
		fastcgi = !FCGX_IsCGI();
	}
#endif
	if (fastcgi) {
		/* How many times to run PHP scripts before dying */
		if( getenv( "PHP_FCGI_MAX_REQUESTS" )) {
			max_requests = atoi( getenv( "PHP_FCGI_MAX_REQUESTS" ));
			if( !max_requests ) {
				fprintf( stderr,
					 "PHP_FCGI_MAX_REQUESTS is not valid\n" );
				return FAILURE;
			}
		}

		/* make php call us to get _ENV vars */
		php_php_import_environment_variables = php_import_environment_variables;
		php_import_environment_variables = cgi_php_import_environment_variables;

		/* library is already initialized, now init our request */
		FCGX_Init();
		FCGX_InitRequest( &request, fcgi_fd, 0 );

#ifndef PHP_WIN32
	/* Pre-fork, if required */
	if( getenv( "PHP_FCGI_CHILDREN" )) {
		children = atoi( getenv( "PHP_FCGI_CHILDREN" ));
		if( !children ) {
			fprintf( stderr,
				 "PHP_FCGI_CHILDREN is not valid\n" );
			return FAILURE;
		}
	}

	if( children ) {
		int running = 0;
		pid_t pid;

		/* Create a process group for ourself & children */
		setsid();
		pgroup = getpgrp();
#ifdef DEBUG_FASTCGI
		fprintf( stderr, "Process group %d\n", pgroup );
#endif

		/* Set up handler to kill children upon exit */
		act.sa_flags = 0;
		act.sa_handler = fastcgi_cleanup;
		if( sigaction( SIGTERM, &act, &old_term ) ||
		    sigaction( SIGINT, &act, &old_int ) ||
		    sigaction( SIGQUIT, &act, &old_quit )) {
			perror( "Can't set signals" );
			exit( 1 );
		}

		while( parent ) {
			do {
#ifdef DEBUG_FASTCGI
				fprintf( stderr, "Forking, %d running\n",
					 running );
#endif
				pid = fork();
				switch( pid ) {
				case 0:
					/* One of the children.
					 * Make sure we don't go round the
					 * fork loop any more
					 */
					parent = 0;

					/* don't catch our signals */
					sigaction( SIGTERM, &old_term, 0 );
					sigaction( SIGQUIT, &old_quit, 0 );
					sigaction( SIGINT, &old_int, 0 );
					break;
				case -1:
					perror( "php (pre-forking)" );
					exit( 1 );
					break;
				default:
					/* Fine */
					running++;
					break;
				}
			} while( parent && ( running < children ));

			if( parent ) {
#ifdef DEBUG_FASTCGI
				fprintf( stderr, "Wait for kids, pid %d\n",
					 getpid() );
#endif
				while (wait( &status ) < 0) {
				}
				running--;
			}
		}
	}

#endif /* WIN32 */
	}
#endif /* FASTCGI */

	zend_first_try {
		if (!cgi
#if PHP_FASTCGI
			&& !fastcgi
#endif
			) {
			while ((c=php_getopt(argc, argv, OPTIONS, &php_optarg, &php_optind, 1))!=-1) {
				switch (c) {
					case 'h':
					case '?':
						no_headers = 1;
						php_output_startup();
						php_output_activate(TSRMLS_C);
						SG(headers_sent) = 1;
						php_cgi_usage(argv[0]);
						php_end_ob_buffers(1 TSRMLS_CC);
						exit(1);
						break;
				}
			}
			php_optind = orig_optind;
			php_optarg = orig_optarg;
		}

#if PHP_FASTCGI
		/* start of FAST CGI loop */
		/* Initialise FastCGI request structure */

#ifdef PHP_WIN32
		/* attempt to set security impersonation for fastcgi
		   will only happen on NT based OS, others will ignore it. */
		if (fastcgi) {
			if (cfg_get_long("fastcgi.impersonate", &impersonate) == FAILURE) {
				impersonate = 0;
			}
			if (impersonate) OS_SetImpersonate();
		}
#endif

		while (!fastcgi
			|| FCGX_Accept_r( &request ) >= 0) {
#endif

#if PHP_FASTCGI
		SG(server_context) = (void *) &request;
#else
		SG(server_context) = (void *) 1; /* avoid server_context==NULL checks */
#endif

		init_request_info(TSRMLS_C);

		zend_llist_init(&global_vars, sizeof(char *), NULL, 0);

		CG(interactive) = 0;

		if (!cgi
#if PHP_FASTCGI
			&& !fastcgi
#endif
			) { 

			if (cgi_sapi_module.php_ini_path_override && cgi_sapi_module.php_ini_ignore) {
				no_headers = 1;  
				php_output_startup();
				php_output_activate(TSRMLS_C);
				SG(headers_sent) = 1;
				php_printf("You cannot use both -n and -c switch. Use -h for help.\n");
				php_end_ob_buffers(1 TSRMLS_CC);
				exit(1);
			}
		
			while ((c = php_getopt(argc, argv, OPTIONS, &php_optarg, &php_optind, 0)) != -1) {
				switch (c) {
					
  				case 'a':	/* interactive mode */
						printf("Interactive mode enabled\n\n");
						interactive=1;
						break;
					
				case 'C': /* don't chdir to the script directory */
						SG(options) |= SAPI_OPTION_NO_CHDIR;
						break;
				case 'd': /* define ini entries on command line */
						define_command_line_ini_entry(php_optarg);
						break;
						
  				case 'e': /* enable extended info output */
						CG(extended_info) = 1;
						break;

  				case 'f': /* parse file */
						script_file = estrdup(php_optarg);
						no_headers = 1;
						/* arguments after the file are considered script args */
						SG(request_info).argc = argc - (php_optind-1);
						SG(request_info).argv = &argv[php_optind-1];
						break;

  				case 'g': /* define global variables on command line */
						{
							char *arg = estrdup(php_optarg);

							zend_llist_add_element(&global_vars, &arg);
						}
						break;

				case 'i': /* php info & quit */
						if (php_request_startup(TSRMLS_C) == FAILURE) {
							php_module_shutdown(TSRMLS_C);
							return FAILURE;
						}
						if (no_headers) {
							SG(headers_sent) = 1;
							SG(request_info).no_headers = 1;
						}
						php_print_info(0xFFFFFFFF TSRMLS_CC);
						php_end_ob_buffers(1 TSRMLS_CC);
						exit(0);
						break;

  				case 'l': /* syntax check mode */
						no_headers = 1;
						behavior=PHP_MODE_LINT;
						break;

				case 'm': /* list compiled in modules */
					php_output_startup();
					php_output_activate(TSRMLS_C);
					SG(headers_sent) = 1;
					php_printf("[PHP Modules]\n");
					print_modules(TSRMLS_C);
					php_printf("\n[Zend Modules]\n");
					print_extensions(TSRMLS_C);
					php_printf("\n");
					php_end_ob_buffers(1 TSRMLS_CC);
					exit(0);
					break;

#if 0 /* not yet operational, see also below ... */
  				case '': /* generate indented source mode*/ 
						behavior=PHP_MODE_INDENT;
						break;
#endif

  				case 'q': /* do not generate HTTP headers */
						no_headers = 1;
						break;

  				case 's': /* generate highlighted HTML from source */
						behavior=PHP_MODE_HIGHLIGHT;
						break;

				case 'v': /* show php version & quit */
						no_headers = 1;
						if (php_request_startup(TSRMLS_C) == FAILURE) {
							php_module_shutdown(TSRMLS_C);
							return FAILURE;
						}
						if (no_headers) {
							SG(headers_sent) = 1;
							SG(request_info).no_headers = 1;
						}
#if ZEND_DEBUG
						php_printf("PHP %s (%s) (built: %s %s) (DEBUG)\nCopyright (c) 1997-2008 The PHP Group\n%s", PHP_VERSION, sapi_module.name, __DATE__, __TIME__, get_zend_version());
#else
						php_printf("PHP %s (%s) (built: %s %s)\nCopyright (c) 1997-2008 The PHP Group\n%s", PHP_VERSION, sapi_module.name, __DATE__, __TIME__, get_zend_version());
#endif
						php_end_ob_buffers(1 TSRMLS_CC);
						exit(0);
						break;

  				case 'w': 
						behavior=PHP_MODE_STRIP;
						break;

				case 'z': /* load extension file */
						zend_load_extension(php_optarg);
						break;

					default:
						break;
				}
			}

			if (script_file) {
				/* override path_translated if -f on command line */
				if (SG(request_info).path_translated) {
					STR_FREE(SG(request_info).path_translated);
				}
				SG(request_info).path_translated = script_file;
			}

			if (no_headers) {
				SG(headers_sent) = 1;
				SG(request_info).no_headers = 1;
			}

			if (!SG(request_info).path_translated && argc > php_optind) {
				/* arguments after the file are considered script args */
				SG(request_info).argc = argc - php_optind;
				SG(request_info).argv = &argv[php_optind];
				/* file is on command line, but not in -f opt */
				SG(request_info).path_translated = estrdup(argv[php_optind++]);
			}

			/* all remaining arguments are part of the query string
			   this section of code concatenates all remaining arguments
			   into a single string, seperating args with a &
			   this allows command lines like:

			   test.php v1=test v2=hello+world!
			   test.php "v1=test&v2=hello world!"
			   test.php v1=test "v2=hello world!"
			*/
			if (!SG(request_info).query_string && argc > php_optind) {
				len = 0;
				for (i = php_optind; i < argc; i++) {
					len += strlen(argv[i]) + 1;
				}

				s = malloc(len + 1);	/* leak - but only for command line version, so ok */
				*s = '\0';			/* we are pretending it came from the environment  */
				for (i = php_optind, len = 0; i < argc; i++) {
					strcat(s, argv[i]);
					if (i < (argc - 1)) {
						strcat(s, PG(arg_separator).input);
					}
				}
				SG(request_info).query_string = s;
			}
		} /* end !cgi && !fastcgi */

		/* 
			we never take stdin if we're (f)cgi, always
			rely on the web server giving us the info
			we need in the environment. 
		*/
		if (SG(request_info).path_translated || cgi 
#if PHP_FASTCGI
			|| fastcgi
#endif
		)
		{
			file_handle.type = ZEND_HANDLE_FILENAME;
			file_handle.filename = SG(request_info).path_translated;
			file_handle.handle.fp = NULL;
		} else {
			file_handle.filename = "-";
			file_handle.type = ZEND_HANDLE_FP;
			file_handle.handle.fp = stdin;
		}

		file_handle.opened_path = NULL;
		file_handle.free_filename = 0;

		/* request startup only after we've done all we can to
		   get path_translated */
        if (php_request_startup(TSRMLS_C)==FAILURE) {
#if PHP_FASTCGI
			if (fastcgi) {
				FCGX_Finish_r(&request);
			}
#endif
            php_module_shutdown(TSRMLS_C);
            return FAILURE;
        }
		if (no_headers) {
			SG(headers_sent) = 1;
			SG(request_info).no_headers = 1;
		}

        /* This actually destructs the elements of the list - ugly hack */
        zend_llist_apply(&global_vars, (llist_apply_func_t) php_register_command_line_global_vars TSRMLS_CC);
        zend_llist_destroy(&global_vars);
		
		/* 
			at this point path_translated will be set if:
			1. we are running from shell and got filename was there
			2. we are running as cgi or fastcgi
		*/
		if (cgi || SG(request_info).path_translated) {
			retval = php_fopen_primary_script(&file_handle TSRMLS_CC);
		}
		/* 
			if we are unable to open path_translated and we are not
			running from shell (so fp == NULL), then fail.
		*/
		if (retval == FAILURE && file_handle.handle.fp == NULL) {
			SG(sapi_headers).http_response_code = 404;
			PUTS("No input file specified.\n");
#if PHP_FASTCGI
			/* we want to serve more requests if this is fastcgi
			   so cleanup and continue, request shutdown is
			   handled later */
			if (fastcgi) {
				goto fastcgi_request_done;
			}
#endif
			php_request_shutdown((void *) 0);
			php_module_shutdown(TSRMLS_C);
			return FAILURE;
		}

		if (file_handle.handle.fp && (file_handle.handle.fp != stdin)) {
			/* #!php support */
			c = fgetc(file_handle.handle.fp);
			if (c == '#') {
				while (c != 10 && c != 13) {
					c = fgetc(file_handle.handle.fp);	/* skip to end of line */
				}
				/* handle situations where line is terminated by \r\n */
				if (c == 13) {
					if (fgetc(file_handle.handle.fp) != 10) {
						long pos = ftell(file_handle.handle.fp);
						fseek(file_handle.handle.fp, pos - 1, SEEK_SET);
					}
				}
				CG(zend_lineno) = -2;
			} else {
				rewind(file_handle.handle.fp);
			}
		}

		switch (behavior) {
			case PHP_MODE_STANDARD:
				php_execute_script(&file_handle TSRMLS_CC);
				break;
			case PHP_MODE_LINT:
				PG(during_request_startup) = 0;
				exit_status = php_lint_script(&file_handle TSRMLS_CC);
				if (exit_status == SUCCESS) {
					zend_printf("No syntax errors detected in %s\n", file_handle.filename);
				} else {
					zend_printf("Errors parsing %s\n", file_handle.filename);
				}
				break;
			case PHP_MODE_STRIP:
				if (open_file_for_scanning(&file_handle TSRMLS_CC) == SUCCESS) {
					zend_strip(TSRMLS_C);
					fclose(file_handle.handle.fp);
					php_end_ob_buffers(1 TSRMLS_CC);
				}
				return SUCCESS;
				break;
			case PHP_MODE_HIGHLIGHT:
				{
					zend_syntax_highlighter_ini syntax_highlighter_ini;

					if (open_file_for_scanning(&file_handle TSRMLS_CC) == SUCCESS) {
						php_get_highlight_struct(&syntax_highlighter_ini);
						zend_highlight(&syntax_highlighter_ini TSRMLS_CC);
						fclose(file_handle.handle.fp);
						php_end_ob_buffers(1 TSRMLS_CC);
					}
					return SUCCESS;
				}
				break;
#if 0
			/* Zeev might want to do something with this one day */
			case PHP_MODE_INDENT:
				open_file_for_scanning(&file_handle TSRMLS_CC);
				zend_indent();
				fclose(file_handle.handle.fp);
				return SUCCESS;
				break;
#endif
		}

#if PHP_FASTCGI
fastcgi_request_done:
#endif
		{
			char *path_translated;
		
			/*	Go through this trouble so that the memory manager doesn't warn
			 *	about SG(request_info).path_translated leaking
			 */
			if (SG(request_info).path_translated) {
				path_translated = strdup(SG(request_info).path_translated);
				STR_FREE(SG(request_info).path_translated);
				SG(request_info).path_translated = path_translated;
			}
			
			php_request_shutdown((void *) 0);
			if (exit_status == 0) {
				exit_status = EG(exit_status);
			}

			if (SG(request_info).path_translated) {
				free(SG(request_info).path_translated);
				SG(request_info).path_translated = NULL;
			}
		}

#if PHP_FASTCGI
			if (!fastcgi) break;
			/* only fastcgi will get here */
			requests++;
			if(max_requests && (requests == max_requests)) {
				FCGX_Finish_r(&request);
#ifndef PHP_WIN32
				if (bindpath) {
					free(bindpath);
				}
#endif
				break;
			}
			/* end of fastcgi loop */
		}
#endif

		if (cgi_sapi_module.php_ini_path_override) {
			free(cgi_sapi_module.php_ini_path_override);
		}
	} zend_catch {
		exit_status = 255;
	} zend_end_try();

	SG(server_context) = NULL;
	php_module_shutdown(TSRMLS_C);

#ifdef ZTS
	tsrm_shutdown();
#endif

#if PHP_WIN32 && ZEND_DEBUG && 0
	_CrtDumpMemoryLeaks( );
#endif

	return exit_status;
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
