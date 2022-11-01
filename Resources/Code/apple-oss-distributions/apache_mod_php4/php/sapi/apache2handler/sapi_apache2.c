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
   | Authors: Sascha Schumann <sascha@schumann.cx>                        |
   |          Parts based on Apache 1.3 SAPI module by                    |
   |          Rasmus Lerdorf and Zeev Suraski                             |
   +----------------------------------------------------------------------+
 */

/* $Id: sapi_apache2.c,v 1.1.2.40.2.13 2007/12/31 07:22:55 sebastian Exp $ */

#include <fcntl.h>

#include "php.h"
#include "php_main.h"
#include "php_ini.h"
#include "php_variables.h"
#include "SAPI.h"

#include "ext/standard/php_smart_str.h"
#ifndef NETWARE
#include "ext/standard/php_standard.h"
#else
#include "ext/standard/basic_functions.h"
#endif

#include "apr_strings.h"
#include "ap_config.h"
#include "util_filter.h"
#include "httpd.h"
#include "http_config.h"
#include "http_request.h"
#include "http_core.h"
#include "http_protocol.h"
#include "http_log.h"
#include "http_main.h"
#include "util_script.h"
#include "http_core.h"                         
#include "ap_mpm.h"

#include "php_apache.h"

/* UnixWare and Netware define shutdown to _shutdown, which causes problems later
 * on when using a structure member named shutdown. Since this source
 * file does not use the system call shutdown, it is safe to #undef it.K
 */
#undef shutdown
 
#define PHP_MAGIC_TYPE "application/x-httpd-php"
#define PHP_SOURCE_MAGIC_TYPE "application/x-httpd-php-source"
#define PHP_SCRIPT "php-script"

/* A way to specify the location of the php.ini dir in an apache directive */
char *apache2_php_ini_path_override = NULL;

static int
php_apache_sapi_ub_write(const char *str, uint str_length TSRMLS_DC)
{
	request_rec *r;
	php_struct *ctx;

	ctx = SG(server_context);
	r = ctx->r;
	
	if (ap_rwrite(str, str_length, r) < 0) {
		php_handle_aborted_connection();
	}
	
	return str_length; /* we always consume all the data passed to us. */
}

static int
php_apache_sapi_header_handler(sapi_header_struct *sapi_header,sapi_headers_struct *sapi_headers TSRMLS_DC)
{
	php_struct *ctx;
	char *val;

	ctx = SG(server_context);

	val = strchr(sapi_header->header, ':');

	if (!val) {
		sapi_free_header(sapi_header);
		return 0;
	}

	*val = '\0';
	
	do {
		val++;
	} while (*val == ' ');

	if (!strcasecmp(sapi_header->header, "content-type")) {
		val = apr_pstrdup(ctx->r->pool, val);
		ap_set_content_type(ctx->r, val);
	} else if (sapi_header->replace) {
		apr_table_set(ctx->r->headers_out, sapi_header->header, val);
	} else {
		apr_table_add(ctx->r->headers_out, sapi_header->header, val);
	}
	
	sapi_free_header(sapi_header);

	return 0;
}

static int
php_apache_sapi_send_headers(sapi_headers_struct *sapi_headers TSRMLS_DC)
{
	php_struct *ctx = SG(server_context);
	const char *sline = SG(sapi_headers).http_status_line;

	ctx->r->status = SG(sapi_headers).http_response_code;

	/* httpd requires that r->status_line is set to the first digit of
	 * the status-code: */
	if (sline && strlen(sline) > 12 && strncmp(sline, "HTTP/1.", 7) == 0 
		&& sline[8] == ' ') {
		ctx->r->status_line = apr_pstrdup(ctx->r->pool, sline + 9);
	}

	return SAPI_HEADER_SENT_SUCCESSFULLY;
}

static int
php_apache_sapi_read_post(char *buf, uint count_bytes TSRMLS_DC)
{
	apr_size_t len, tlen=0;
	php_struct *ctx = SG(server_context);
	request_rec *r;
	apr_bucket_brigade *brigade;

	r = ctx->r;
	brigade = ctx->brigade;
	len = count_bytes;

	/*
	 * This loop is needed because ap_get_brigade() can return us partial data
	 * which would cause premature termination of request read. Therefor we
	 * need to make sure that if data is available we fill the buffer completely.
	 */

	while (ap_get_brigade(r->input_filters, brigade, AP_MODE_READBYTES, APR_BLOCK_READ, len) == APR_SUCCESS) {
		apr_brigade_flatten(brigade, buf, &len);
		apr_brigade_cleanup(brigade);
		tlen += len;
		if (tlen == count_bytes || !len) {
			break;
		}
		buf += len;
		len = count_bytes - tlen;
	}
	
	return tlen;
}

static struct stat*
php_apache_sapi_get_stat(TSRMLS_D)
{
	php_struct *ctx = SG(server_context);

	ctx->finfo.st_uid = ctx->r->finfo.user;
	ctx->finfo.st_gid = ctx->r->finfo.group;
	ctx->finfo.st_dev = ctx->r->finfo.device;
	ctx->finfo.st_ino = ctx->r->finfo.inode;
#if defined(NETWARE) && defined(CLIB_STAT_PATCH)
	ctx->finfo.st_atime.tv_sec = ctx->r->finfo.atime/1000000;
	ctx->finfo.st_mtime.tv_sec = ctx->r->finfo.mtime/1000000;
	ctx->finfo.st_ctime.tv_sec = ctx->r->finfo.ctime/1000000;
#else
	ctx->finfo.st_atime = ctx->r->finfo.atime/1000000;
	ctx->finfo.st_mtime = ctx->r->finfo.mtime/1000000;
	ctx->finfo.st_ctime = ctx->r->finfo.ctime/1000000;
#endif

	ctx->finfo.st_size = ctx->r->finfo.size;
	ctx->finfo.st_nlink = ctx->r->finfo.nlink;

	return &ctx->finfo;
}

static char *
php_apache_sapi_read_cookies(TSRMLS_D)
{
	php_struct *ctx = SG(server_context);
	const char *http_cookie;

	http_cookie = apr_table_get(ctx->r->headers_in, "cookie");

	/* The SAPI interface should use 'const char *' */
	return (char *) http_cookie;
}

static char *
php_apache_sapi_getenv(char *name, size_t name_len TSRMLS_DC)
{
	php_struct *ctx = SG(server_context);
	const char *env_var;
	
	env_var = apr_table_get(ctx->r->subprocess_env, name);

	return (char *) env_var;
}

static void
php_apache_sapi_register_variables(zval *track_vars_array TSRMLS_DC)
{
	php_struct *ctx = SG(server_context);
	const apr_array_header_t *arr = apr_table_elts(ctx->r->subprocess_env);
	char *key, *val;
 	zval **path_translated_zv;

	APR_ARRAY_FOREACH_OPEN(arr, key, val)
		if (!val) val = empty_string;
		php_register_variable(key, val, track_vars_array TSRMLS_CC);
	APR_ARRAY_FOREACH_CLOSE()

	php_register_variable("PHP_SELF", ctx->r->uri, track_vars_array TSRMLS_CC);

	/* If PATH_TRANSLATED doesn't exist, copy it from SCRIPT_FILENAME */
 	if (!zend_hash_exists(Z_ARRVAL_P(track_vars_array), "PATH_TRANSLATED", sizeof("PATH_TRANSLATED"))
 		&& zend_hash_find(Z_ARRVAL_P(track_vars_array), "SCRIPT_FILENAME", sizeof("SCRIPT_FILENAME"), (void **) &path_translated_zv) == SUCCESS) {
 		php_register_variable("PATH_TRANSLATED", Z_STRVAL_PP(path_translated_zv), track_vars_array TSRMLS_CC);
 	}
}

static void
php_apache_sapi_flush(void *server_context)
{
	php_struct *ctx;
	request_rec *r;
	TSRMLS_FETCH();

	ctx = server_context;

	/* If we haven't registered a server_context yet,
	 * then don't bother flushing. */
	if (!server_context) {
		return;
	}

	r = ctx->r;

	sapi_send_headers(TSRMLS_C);

	r->status = SG(sapi_headers).http_response_code;
	SG(headers_sent) = 1;

	if (ap_rflush(r) < 0 || r->connection->aborted) {
		php_handle_aborted_connection();
	}
}

static void php_apache_sapi_log_message(char *msg)
{
	php_struct *ctx;
	TSRMLS_FETCH();

	ctx = SG(server_context);

	if (ctx == NULL) { /* we haven't initialized our ctx yet, oh well */
		ap_log_error(APLOG_MARK, APLOG_ERR | APLOG_STARTUP, 0, NULL, "%s", msg);
	} else {
		ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, ctx->r, "%s", msg);
	}
}

static void php_apache_sapi_log_message_ex(char *msg, request_rec *r)
{
	if (r) {
		ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, msg, r->filename);
	} else {
		php_apache_sapi_log_message(msg);
	}
}

extern zend_module_entry php_apache_module;

static int php_apache2_startup(sapi_module_struct *sapi_module)
{
	if (php_module_startup(sapi_module, &php_apache_module, 1)==FAILURE) {
		return FAILURE;
	}
	return SUCCESS;
}

static sapi_module_struct apache2_sapi_module = {
	"apache2handler",
	"Apache 2.0 Handler",

	php_apache2_startup,				/* startup */
	php_module_shutdown_wrapper,			/* shutdown */

	NULL,						/* activate */
	NULL,						/* deactivate */

	php_apache_sapi_ub_write,			/* unbuffered write */
	php_apache_sapi_flush,				/* flush */
	php_apache_sapi_get_stat,			/* get uid */
	php_apache_sapi_getenv,				/* getenv */

	php_error,					/* error handler */

	php_apache_sapi_header_handler,			/* header handler */
	php_apache_sapi_send_headers,			/* send headers handler */
	NULL,						/* send header handler */

	php_apache_sapi_read_post,			/* read POST data */
	php_apache_sapi_read_cookies,			/* read Cookies */

	php_apache_sapi_register_variables,
	php_apache_sapi_log_message,			/* Log message */

	STANDARD_SAPI_MODULE_PROPERTIES
};

static apr_status_t
php_apache_server_shutdown(void *tmp)
{
	apache2_sapi_module.shutdown(&apache2_sapi_module);
	sapi_shutdown();
#ifdef ZTS
	tsrm_shutdown();
#endif
	return APR_SUCCESS;
}

static void php_apache_add_version(apr_pool_t *p)
{
	TSRMLS_FETCH();
	if (PG(expose_php)) {
		ap_add_version_component(p, "PHP/" PHP_VERSION);
	}
}

static int php_pre_config(apr_pool_t *pconf, apr_pool_t *plog, apr_pool_t *ptemp)
{
#ifndef ZTS
	int threaded_mpm;

	ap_mpm_query(AP_MPMQ_IS_THREADED, &threaded_mpm);
	if(threaded_mpm) {
		ap_log_error(APLOG_MARK, APLOG_CRIT, 0, 0, "Apache is running a threaded MPM, but your PHP Module is not compiled to be threadsafe.  You need to recompile PHP.");
		return DONE;
	}
#endif
	/* When this is NULL, apache won't override the hard-coded default
	 * php.ini path setting. */
	apache2_php_ini_path_override = NULL;
	return OK;
}

static int
php_apache_server_startup(apr_pool_t *pconf, apr_pool_t *plog, apr_pool_t *ptemp, server_rec *s)
{
	void *data = NULL;
	const char *userdata_key = "apache2hook_post_config";

	/* Apache will load, unload and then reload a DSO module. This
	 * prevents us from starting PHP until the second load. */
	apr_pool_userdata_get(&data, userdata_key, s->process->pool);
	if (data == NULL) {
		/* We must use set() here and *not* setn(), otherwise the
		 * static string pointed to by userdata_key will be mapped
		 * to a different location when the DSO is reloaded and the
		 * pointers won't match, causing get() to return NULL when
		 * we expected it to return non-NULL. */
		apr_pool_userdata_set((const void *)1, userdata_key, apr_pool_cleanup_null, s->process->pool);
		return OK;
	}

	/* Set up our overridden path. */
	if (apache2_php_ini_path_override) {
		apache2_sapi_module.php_ini_path_override = apache2_php_ini_path_override;
	}
#ifdef ZTS
	tsrm_startup(1, 1, 0, NULL);
#endif
	sapi_startup(&apache2_sapi_module);
	apache2_sapi_module.startup(&apache2_sapi_module);
	apr_pool_cleanup_register(pconf, NULL, php_apache_server_shutdown, apr_pool_cleanup_null);
	php_apache_add_version(pconf);

	return OK;
}

static apr_status_t php_server_context_cleanup(void *data_)
{
	void **data = data_;
	*data = NULL;
	return APR_SUCCESS;
}

static int php_apache_request_ctor(request_rec *r, php_struct *ctx TSRMLS_DC)
{
	char *content_type;
	char *content_length;
	const char *auth;

	SG(sapi_headers).http_response_code = !r->status ? HTTP_OK : r->status;
	SG(request_info).content_type = apr_table_get(r->headers_in, "Content-Type");
	SG(request_info).query_string = apr_pstrdup(r->pool, r->args);
	SG(request_info).request_method = r->method;
	SG(request_info).request_uri = apr_pstrdup(r->pool, r->uri);
	SG(request_info).path_translated = apr_pstrdup(r->pool, r->filename);
	r->no_local_copy = 1;

	content_type = sapi_get_default_content_type(TSRMLS_C);
	ap_set_content_type(r, apr_pstrdup(r->pool, content_type));
	efree(content_type);

	content_length = (char *) apr_table_get(r->headers_in, "Content-Length");
	SG(request_info).content_length = (content_length ? atoi(content_length) : 0);

	apr_table_unset(r->headers_out, "Content-Length");
	apr_table_unset(r->headers_out, "Last-Modified");
	apr_table_unset(r->headers_out, "Expires");
	apr_table_unset(r->headers_out, "ETag");
	if (!PG(safe_mode) || (PG(safe_mode) && !ap_auth_type(r))) {
		auth = apr_table_get(r->headers_in, "Authorization");
		php_handle_auth_data(auth TSRMLS_CC);
		ctx->r->user = apr_pstrdup(ctx->r->pool, SG(request_info).auth_user);
	} else {
		SG(request_info).auth_user = NULL;
		SG(request_info).auth_password = NULL;
	}
	return php_request_startup(TSRMLS_C);
}

static void php_apache_request_dtor(request_rec *r TSRMLS_DC)
{
	php_request_shutdown(NULL);
}

static void php_apache_ini_dtor(request_rec *r, request_rec *p TSRMLS_DC)
{
	if (strcmp(r->protocol, "INCLUDED")) {
		zend_try { zend_ini_deactivate(TSRMLS_C); } zend_end_try();
	}
	if (p) {
		((php_struct *)SG(server_context))->r = p;
	} else {
		apr_pool_cleanup_run(r->pool, (void *)&SG(server_context), php_server_context_cleanup);
	}
}

static int php_handler(request_rec *r)
{
	php_struct *ctx;
	void *conf;
	apr_bucket_brigade *brigade;
	apr_bucket *bucket;
	apr_status_t rv;
	request_rec *parent_req = NULL;
	TSRMLS_FETCH();

#define PHPAP_INI_OFF php_apache_ini_dtor(r, parent_req TSRMLS_CC);

	conf = ap_get_module_config(r->per_dir_config, &php4_module);

	/* apply_config() needs r in some cases, so allocate server_context early */
	ctx = SG(server_context);
	if (ctx == NULL || (ctx && ctx->request_processed && !strcmp(r->protocol, "INCLUDED"))) {
		ctx = SG(server_context) = apr_pcalloc(r->pool, sizeof(*ctx));
		/* register a cleanup so we clear out the SG(server_context)
		 * after each request. Note: We pass in the pointer to the
		 * server_context in case this is handled by a different thread.
		 */
		apr_pool_cleanup_register(r->pool, (void *)&SG(server_context), php_server_context_cleanup, apr_pool_cleanup_null);
		ctx->r = r;
		ctx = NULL; /* May look weird to null it here, but it is to catch the right case in the first_try later on */
	} else {
		parent_req = ctx->r;
		ctx->r = r;
	}
	apply_config(conf);

	if (strcmp(r->handler, PHP_MAGIC_TYPE) && strcmp(r->handler, PHP_SOURCE_MAGIC_TYPE) && strcmp(r->handler, PHP_SCRIPT)) {
		/* Check for xbithack in this case. */
		if (!AP2(xbithack) || strcmp(r->handler, "text/html") || !(r->finfo.protection & APR_UEXECUTE)) {
			PHPAP_INI_OFF;
			return DECLINED;
		}
	}

	/* Give a 404 if PATH_INFO is used but is explicitly disabled in
	 * the configuration; default behaviour is to accept. */ 
	if (r->used_path_info == AP_REQ_REJECT_PATH_INFO
		&& r->path_info && r->path_info[0]) {
		PHPAP_INI_OFF;
		return HTTP_NOT_FOUND;
	}

	/* handle situations where user turns the engine off */
	if (!AP2(engine)) {
		PHPAP_INI_OFF;
		return DECLINED;
	}

	if (r->finfo.filetype == 0) {
		php_apache_sapi_log_message_ex("script '%s' not found or unable to stat", r);
		PHPAP_INI_OFF;
		return HTTP_NOT_FOUND;
	}
	if (r->finfo.filetype == APR_DIR) {
		php_apache_sapi_log_message_ex("attempt to invoke directory '%s' as script", r);
		PHPAP_INI_OFF;
		return HTTP_FORBIDDEN;
	}

	/* Setup the CGI variables if this is the main request */
	if (r->main == NULL || 
		/* .. or if the sub-request envinronment differs from the main-request. */ 
		r->subprocess_env != r->main->subprocess_env
	) {
		/* setup standard CGI variables */
		ap_add_common_vars(r);
		ap_add_cgi_vars(r);
	}

zend_first_try {

	if (ctx == NULL) {
normal:
		brigade = apr_brigade_create(r->pool, r->connection->bucket_alloc);
		ctx = SG(server_context);
		ctx->brigade = brigade;

		if (php_apache_request_ctor(r, ctx TSRMLS_CC)!=SUCCESS) {
			zend_bailout();
		}
	} else {
		if (!parent_req) {
			parent_req = ctx->r;
		}
		if (parent_req && parent_req->handler && strcmp(parent_req->handler, PHP_MAGIC_TYPE) && strcmp(parent_req->handler, PHP_SOURCE_MAGIC_TYPE) && strcmp(parent_req->handler, PHP_SCRIPT)) {
			if (php_apache_request_ctor(r, ctx TSRMLS_CC)!=SUCCESS) {
				zend_bailout();
			}
		}
		
		/* check if comming due to ErrorDocument */
		if (parent_req && parent_req->status != HTTP_OK && strcmp(r->protocol, "INCLUDED")) {
			parent_req = NULL;
			goto normal;
		}
		ctx->r = r;
		brigade = ctx->brigade;
	}

	if (AP2(last_modified)) {
		ap_update_mtime(r, r->finfo.mtime);
		ap_set_last_modified(r);
	}

	/* Determine if we need to parse the file or show the source */
	if (strncmp(r->handler, PHP_SOURCE_MAGIC_TYPE, sizeof(PHP_SOURCE_MAGIC_TYPE) - 1) == 0) {
		zend_syntax_highlighter_ini syntax_highlighter_ini;
		php_get_highlight_struct(&syntax_highlighter_ini);
		highlight_file((char *)r->filename, &syntax_highlighter_ini TSRMLS_CC);
	} else {
		zend_file_handle zfd = {0};

		zfd.type = ZEND_HANDLE_FILENAME;
		zfd.filename = (char *) r->filename;
		zfd.free_filename = 0;
		zfd.opened_path = NULL;

		if (!parent_req) {
			php_execute_script(&zfd TSRMLS_CC);
		} else {
			zend_execute_scripts(ZEND_INCLUDE TSRMLS_CC, NULL, 1, &zfd);
		}
#if MEMORY_LIMIT
		{
			char *mem_usage;
			
			mem_usage = apr_psprintf(ctx->r->pool, "%u", AG(allocated_memory_peak));
			apr_table_set(r->notes, "mod_php_memory_usage", mem_usage);
		}
#endif
	}

} zend_end_try();

	if (!parent_req) {
		php_apache_request_dtor(r TSRMLS_CC);
		ctx->request_processed = 1;
		bucket = apr_bucket_eos_create(r->connection->bucket_alloc);
		APR_BRIGADE_INSERT_TAIL(brigade, bucket);

		rv = ap_pass_brigade(r->output_filters, brigade);
		if (rv != APR_SUCCESS || r->connection->aborted) {
zend_first_try {
			php_handle_aborted_connection();
} zend_end_try();
		}
		apr_brigade_cleanup(brigade);
	} else {
		ctx->r = parent_req;
	}

	return OK;
}

static void php_register_hook(apr_pool_t *p)
{
	ap_hook_pre_config(php_pre_config, NULL, NULL, APR_HOOK_MIDDLE);
	ap_hook_post_config(php_apache_server_startup, NULL, NULL, APR_HOOK_MIDDLE);
	ap_hook_handler(php_handler, NULL, NULL, APR_HOOK_MIDDLE);
}

AP_MODULE_DECLARE_DATA module php4_module = {
	STANDARD20_MODULE_STUFF,
	create_php_config,		/* create per-directory config structure */
	merge_php_config,		/* merge per-directory config structures */
	NULL,				/* create per-server config structure */
	NULL,				/* merge per-server config structures */
	php_dir_cmds,			/* command apr_table_t */
	php_register_hook		/* register hooks */
};

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
