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
   | Authors: Rasmus Lerdorf <rasmus@php.net>                             |
   |          Jim Winstead <jimw@php.net>                                 |
   |          Hartmut Holzgraefe <hholzgra@php.net>                       |
   |          Wez Furlong <wez@thebrainroom.com>                          |
   +----------------------------------------------------------------------+
 */
/* $Id: http_fopen_wrapper.c,v 1.53.2.20.2.11 2007/12/31 07:22:52 sebastian Exp $ */ 

#include "php.h"
#include "php_globals.h"
#include "php_streams.h"
#include "php_network.h"
#include "php_ini.h"
#include "ext/standard/basic_functions.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef PHP_WIN32
#include <windows.h>
#include <winsock.h>
#define O_RDONLY _O_RDONLY
#include "win32/param.h"
#else
#include <sys/param.h>
#endif

#include "php_standard.h"

#include <sys/types.h>
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef PHP_WIN32
#include <winsock.h>
#elif defined(NETWARE) && defined(USE_WINSOCK)
#include <novsock2.h>
#else
#include <netinet/in.h>
#include <netdb.h>
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#endif

#if defined(PHP_WIN32) || defined(__riscos__) || defined(NETWARE)
#undef AF_UNIX
#endif

#if defined(AF_UNIX)
#include <sys/un.h>
#endif

#include "php_fopen_wrappers.h"

#define HTTP_HEADER_BLOCK_SIZE		1024
#define PHP_URL_REDIRECT_MAX		20
#define HTTP_HEADER_USER_AGENT		1
#define HTTP_HEADER_HOST		2
#define HTTP_HEADER_AUTH		4
#define HTTP_HEADER_FROM		8

php_stream *php_stream_url_wrap_http_ex(php_stream_wrapper *wrapper, char *path, char *mode, int options, char **opened_path, php_stream_context *context, int redirect_max, int header_init STREAMS_DC TSRMLS_DC)
{
	php_stream *stream = NULL;
	php_url *resource = NULL;
	int use_ssl;
	char *scratch = NULL;
	char *tmp = NULL;
	char *ua_str = NULL;
	zval **ua_zval = NULL, **tmpzval = NULL;
	int scratch_len = 0;
	int body = 0;
	char location[HTTP_HEADER_BLOCK_SIZE];
	zval **response_header = NULL;
	int reqok = 0;
	char *http_header_line = NULL;
	char tmp_line[128];
	size_t chunk_size = 0, file_size = 0;
	int eol_detect, have_header = 0;

	tmp_line[0] = '\0';

 	if (redirect_max < 1) {
 		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Circular redirect, aborting.");
 		return NULL;
 	}

	if (strpbrk(mode, "aw+")) {
		php_stream_wrapper_log_error(wrapper, options TSRMLS_CC, "HTTP wrapper does not support writeable connections.");
		return NULL;
	}

	resource = php_url_parse(path);
	if (resource == NULL) {
		return NULL;
	}

 	if (strncasecmp(resource->scheme, "http", sizeof("http")) && strncasecmp(resource->scheme, "https", sizeof("https"))) {
 		php_url_free(resource);
 		return php_stream_open_wrapper_ex(path, mode, ENFORCE_SAFE_MODE | REPORT_ERRORS, NULL, context);
 	}
	
	use_ssl = resource->scheme && (strlen(resource->scheme) > 4) && resource->scheme[4] == 's';

	/* choose default ports */
	if (use_ssl && resource->port == 0)
		resource->port = 443;
	else if (resource->port == 0)
		resource->port = 80;

	stream = php_stream_sock_open_host(resource->host, resource->port, SOCK_STREAM, NULL, 0);
	if (stream == NULL) {
		eol_detect = 0;
		goto out;
	}

	/* avoid problems with auto-detecting when reading the headers -> the headers
	 * are always in canonical \r\n format */
   	eol_detect = stream->flags & (PHP_STREAM_FLAG_DETECT_EOL | PHP_STREAM_FLAG_EOL_MAC);
	stream->flags &= ~(PHP_STREAM_FLAG_DETECT_EOL | PHP_STREAM_FLAG_EOL_MAC);
	
	php_stream_context_set(stream, context);

	php_stream_notify_info(context, PHP_STREAM_NOTIFY_CONNECT, NULL, 0);
	
#ifdef HAVE_OPENSSL_EXT
	if (use_ssl)	{

		if (context) {
			/* set the CN we expect to be on the remote cert.
			 * You still need to have enabled verification (verify_peer) in the context for
			 * this to have an effect */
			zval *cn;

			ALLOC_INIT_ZVAL(cn);
			ZVAL_STRING(cn, resource->host, 1);
			php_stream_context_set_option(context, "ssl", "CN_match", cn);
		}
		
		if (php_stream_sock_ssl_activate(stream, 1) == FAILURE)	{
			php_stream_wrapper_log_error(wrapper, options TSRMLS_CC, "Unable to activate SSL mode");
			php_stream_close(stream);
			stream = NULL;
			goto out;
		}
	}
#endif

	if (context && php_stream_context_get_option(context, "http", "method", &tmpzval) == SUCCESS) {
		if (Z_TYPE_PP(tmpzval) == IS_STRING && Z_STRLEN_PP(tmpzval) > 0) {
	        	scratch_len = strlen(path) + 29 + Z_STRLEN_PP(tmpzval);
	                scratch = emalloc(scratch_len);
	                strlcpy(scratch, Z_STRVAL_PP(tmpzval), Z_STRLEN_PP(tmpzval) + 1);
	                strcat(scratch, " ");
		}
	}

	if (!scratch) {
		scratch_len = strlen(path) + 32;
	        scratch = emalloc(scratch_len);
	        strcpy(scratch, "GET ");
	}

	/* file */
	if (resource->path && *resource->path)
		strlcat(scratch, resource->path, scratch_len);
	else
		strlcat(scratch, "/", scratch_len);
	
	/* query string */
	if (resource->query)	{
		strlcat(scratch, "?", scratch_len);
		strlcat(scratch, resource->query, scratch_len);
	}

	/* protocol version we are speaking */
	strlcat(scratch, " HTTP/1.0\r\n", scratch_len);

	/* send it */
	php_stream_write(stream, scratch, strlen(scratch));
	
	if (context && php_stream_context_get_option(context, "http", "header", &tmpzval) == SUCCESS && 
		Z_TYPE_PP(tmpzval) == IS_STRING && Z_STRLEN_PP(tmpzval)) {
		/* Remove newlines and spaces from start and end, php_trim will estrndup() */
		tmp = php_trim(Z_STRVAL_PP(tmpzval), Z_STRLEN_PP(tmpzval), NULL, 0, NULL, 3 TSRMLS_CC);
		if (strlen(tmp) > 0) {
			/* Output trimmed headers with \r\n at the end */
			php_stream_write(stream, tmp, strlen(tmp));
			php_stream_write(stream, "\r\n", sizeof("\r\n") - 1);

			/* Make lowercase for easy comparison against 'standard' headers */
			php_strtolower(tmp, strlen(tmp));
			if (strstr(tmp, "user-agent:")) {
				 have_header |= HTTP_HEADER_USER_AGENT;
			}
			if (strstr(tmp, "host:")) {
				 have_header |= HTTP_HEADER_HOST;
			}
			if (strstr(tmp, "from:")) {
				 have_header |= HTTP_HEADER_FROM;
				}
			if (strstr(tmp, "authorization:")) {
				 have_header |= HTTP_HEADER_AUTH;
			}
		}
		efree(tmp);
	}

	/* auth header if it was specified */
	if (((have_header & HTTP_HEADER_AUTH) == 0) && resource->user && resource->pass) {
		/* decode the strings first */
		php_url_decode(resource->user, strlen(resource->user));
		php_url_decode(resource->pass, strlen(resource->pass));

		/* scratch is large enough, since it was made large enough for the whole URL */
		strcpy(scratch, resource->user);
		strcat(scratch, ":");
		strcat(scratch, resource->pass);

		tmp = php_base64_encode((unsigned char*)scratch, strlen(scratch), NULL);

		if (snprintf(scratch, scratch_len, "Authorization: Basic %s\r\n", tmp) > 0) {
			php_stream_write(stream, scratch, strlen(scratch));
			php_stream_notify_info(context, PHP_STREAM_NOTIFY_AUTH_REQUIRED, NULL, 0);
		}

		efree(tmp);
		tmp = NULL;
	}

	/* if the user has configured who they are, send a From: line */
	if (((have_header & HTTP_HEADER_FROM) == 0) && cfg_get_string("from", &tmp) == SUCCESS)	{
		if (snprintf(scratch, scratch_len, "From: %s\r\n", tmp) > 0) {
			php_stream_write(stream, scratch, strlen(scratch));
		}
	}

	/* Send Host: header so name-based virtual hosts work */
	if ((have_header & HTTP_HEADER_HOST) == 0) {
		if ((use_ssl && resource->port != 443) || (!use_ssl && resource->port != 80))	{
			if (snprintf(scratch, scratch_len, "Host: %s:%i\r\n", resource->host, resource->port) > 0) {
				php_stream_write(stream, scratch, strlen(scratch));
			}
		} else {
			if (snprintf(scratch, scratch_len, "Host: %s\r\n", resource->host) > 0) {
				php_stream_write(stream, scratch, strlen(scratch));
			}
		}
	}

	if (context && 
	    php_stream_context_get_option(context, "http", "user_agent", &ua_zval) == SUCCESS &&
		Z_TYPE_PP(ua_zval) == IS_STRING) {
		ua_str = Z_STRVAL_PP(ua_zval);
	} else if (FG(user_agent)) {
		ua_str = FG(user_agent);
	}

	if (ua_str) {
#define _UA_HEADER "User-Agent: %s\r\n"
		char *ua;
		size_t ua_len;
		
		ua_len = sizeof(_UA_HEADER) + strlen(ua_str);
		
		/* ensure the header is only sent if user_agent is not blank */
		if (ua_len > sizeof(_UA_HEADER)) {
			ua = emalloc(ua_len + 1);
			if ((ua_len = snprintf(ua, ua_len, _UA_HEADER, ua_str)) > 0) {
				ua[ua_len] = 0;
				php_stream_write(stream, ua, ua_len);
			} else {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Cannot construct User-agent header");
			}

			if (ua) {
				efree(ua);
			}
		}	
	}

	php_stream_write(stream, "\r\n", sizeof("\r\n")-1);

	/* Request content, such as for POST requests */
	if (context && php_stream_context_get_option(context, "http", "content", &tmpzval) == SUCCESS &&
		Z_TYPE_PP(tmpzval) == IS_STRING && Z_STRLEN_PP(tmpzval) > 0) {
		php_stream_write(stream, Z_STRVAL_PP(tmpzval), Z_STRLEN_PP(tmpzval));
		php_stream_write(stream, "\r\n\r\n", sizeof("\r\n\r\n")-1);
	}

	location[0] = '\0';

	/* 
	 * We need to read the HTTP response header one-by-one, because
	 * the original author did not know about MSG_PEEK.
	 * The chunk_size will be reset later, once we have read the
	 * header completely.
	 */
	if (options & STREAM_WILL_CAST)
		chunk_size = php_stream_set_chunk_size(stream, 1);

	if (!header_init && FAILURE == zend_hash_find(EG(active_symbol_table),
				"http_response_header", sizeof("http_response_header"), (void **) &response_header)) {
		header_init = 1;
	}

	if (header_init) {
		zval *tmp;
		MAKE_STD_ZVAL(tmp);
		array_init(tmp);
		ZEND_SET_SYMBOL(EG(active_symbol_table), "http_response_header", tmp);
	
		zend_hash_find(EG(active_symbol_table),
				"http_response_header", sizeof("http_response_header"), (void **) &response_header);
	}


	if (!php_stream_eof(stream)) {
		size_t tmp_line_len;
		/* get response header */

		if (php_stream_get_line(stream, tmp_line, sizeof(tmp_line) - 1, &tmp_line_len) != NULL) {
			zval *http_response;
			int response_code;

			MAKE_STD_ZVAL(http_response);
			ZVAL_NULL(http_response);

			if (tmp_line_len > 9) {
				response_code = atoi(tmp_line + 9);
			} else {
				response_code = 0;
			}
			switch(response_code) {
				case 200:
				case 206: /* partial content */
				case 302:
				case 301:
					reqok = 1;
					break;
				case 403:
					php_stream_notify_error(context, PHP_STREAM_NOTIFY_AUTH_RESULT,
							tmp_line, response_code);
					break;
				default:
					/* safety net in the event tmp_line == NULL */
					if (!tmp_line_len) {
						tmp_line[0] = '\0';
					}
					php_stream_notify_error(context, PHP_STREAM_NOTIFY_FAILURE,
							tmp_line, response_code);
			}
			
			Z_STRLEN_P(http_response) = tmp_line_len;
			Z_STRVAL_P(http_response) = estrndup(tmp_line, Z_STRLEN_P(http_response));
			if (Z_STRVAL_P(http_response)[Z_STRLEN_P(http_response)-1]=='\n') {
				Z_STRVAL_P(http_response)[Z_STRLEN_P(http_response)-1]=0;
				Z_STRLEN_P(http_response)--;
				if (Z_STRVAL_P(http_response)[Z_STRLEN_P(http_response)-1]=='\r') {
					Z_STRVAL_P(http_response)[Z_STRLEN_P(http_response)-1]=0;
					Z_STRLEN_P(http_response)--;
				}
			}
			Z_TYPE_P(http_response) = IS_STRING;
			zend_hash_next_index_insert(Z_ARRVAL_PP(response_header), &http_response, sizeof(zval *), NULL);
		}
	} else {
		php_stream_wrapper_log_error(wrapper, options TSRMLS_CC, "HTTP request failed, unexpected end of socket!");
		goto out;
	}
	
	/* read past HTTP headers */
	
	http_header_line = emalloc(HTTP_HEADER_BLOCK_SIZE);

	while (!body && !php_stream_eof(stream)) {
		size_t http_header_line_length;
		if (php_stream_get_line(stream, http_header_line, HTTP_HEADER_BLOCK_SIZE, &http_header_line_length) && *http_header_line != '\n' && *http_header_line != '\r') {
			char *e = http_header_line + http_header_line_length - 1;
			while (*e == '\n' || *e == '\r') {
				e--;
			}
			http_header_line_length = e - http_header_line + 1;
			http_header_line[http_header_line_length] = '\0';

			if (!strncasecmp(http_header_line, "Location: ", 10)) {
				strlcpy(location, http_header_line + 10, sizeof(location));
			} else if (!strncasecmp(http_header_line, "Content-Type: ", 14)) {
				php_stream_notify_info(context, PHP_STREAM_NOTIFY_MIME_TYPE_IS, http_header_line + 14, 0);
			} else if (!strncasecmp(http_header_line, "Content-Length: ", 16)) {
				file_size = atoi(http_header_line + 16);
				php_stream_notify_file_size(context, file_size, http_header_line, 0);
			}

			if (http_header_line[0] == '\0') {
				body = 1;
			} else {
				zval *http_header;

				MAKE_STD_ZVAL(http_header);

				ZVAL_STRINGL(http_header, http_header_line, http_header_line_length, 1);
				
				zend_hash_next_index_insert(Z_ARRVAL_PP(response_header), &http_header, sizeof(zval *), NULL);
			}
		} else {
			break;
		}
	}
	
	if (!reqok || location[0] != '\0')	{		
		if (location[0] != '\0')
			php_stream_notify_info(context, PHP_STREAM_NOTIFY_REDIRECTED, location, 0);

		php_stream_close(stream);
		stream = NULL;

		if (location[0] != '\0')	{

			zval *entry, **entryp;
			char new_path[HTTP_HEADER_BLOCK_SIZE];
			char loc_path[HTTP_HEADER_BLOCK_SIZE];

			*new_path='\0';
			if (strlen(location)<8 || (strncasecmp(location, "http://", sizeof("http://")-1) && 
							strncasecmp(location, "https://", sizeof("https://")-1) && 
							strncasecmp(location, "ftp://", sizeof("ftp://")-1) && 
							strncasecmp(location, "ftps://", sizeof("ftps://")-1))) 
			{
				if (*location != '/') {
					if (*(location+1) != '\0' && resource->path) {		
						char *s = strrchr(resource->path, '/');
						if (!s) {
							s = resource->path;
							if (!s[0]) {
								efree(s);
								s = resource->path = estrdup("/");
							} else {
								*s = '/';
							}
						}
						s[1] = '\0'; 
						if (resource->path && *(resource->path) == '/' && *(resource->path + 1) == '\0') {
							snprintf(loc_path, sizeof(loc_path) - 1, "%s%s", resource->path, location);
						} else {
							snprintf(loc_path, sizeof(loc_path) - 1, "%s/%s", resource->path, location);
						}
					} else {
						snprintf(loc_path, sizeof(loc_path) - 1, "/%s", location);
					}
				} else {
					strlcpy(loc_path, location, sizeof(loc_path));
				}
				if ((use_ssl && resource->port != 443) || (!use_ssl && resource->port != 80)) {
					snprintf(new_path, sizeof(new_path) - 1, "%s://%s:%d%s", resource->scheme, resource->host, resource->port, loc_path);
				} else {
					snprintf(new_path, sizeof(new_path) - 1, "%s://%s%s", resource->scheme, resource->host, loc_path);
				}
			} else {
				strlcpy(new_path, location, sizeof(new_path));
			}

			php_url_free(resource);
			/* check for invalid redirection URLs */
			if ((resource = php_url_parse(new_path)) == NULL) {
				php_stream_wrapper_log_error(wrapper, options TSRMLS_CC, "Invalid redirect url! %s", new_path);
				goto out;
			}

#define CHECK_FOR_CNTRL_CHARS(val) {	\
	if (val) {	\
		unsigned char *s, *e;	\
		int l;	\
		l = php_url_decode(val, strlen(val));	\
		s = val; e = s + l;	\
		while (s < e) {	\
			if (iscntrl(*s)) {	\
				php_stream_wrapper_log_error(wrapper, options TSRMLS_CC, "Invalid redirect url! %s", new_path);	\
				goto out;	\
			}	\
			s++;	\
		}	\
	}	\
}	\
			/* check for control characters in login, password & path */
			if (strncasecmp(new_path, "http://", sizeof("http://") - 1) || strncasecmp(new_path, "https://", sizeof("https://") - 1)) {
				CHECK_FOR_CNTRL_CHARS(resource->user)
				CHECK_FOR_CNTRL_CHARS(resource->pass)
				CHECK_FOR_CNTRL_CHARS(resource->path)
			}

			stream = php_stream_url_wrap_http_ex(NULL, new_path, mode, options, opened_path, context, --redirect_max, 0 STREAMS_CC TSRMLS_CC);
			if (stream && stream->wrapperdata)	{
				entryp = &entry;
				MAKE_STD_ZVAL(entry);
				ZVAL_EMPTY_STRING(entry);
				zend_hash_next_index_insert(Z_ARRVAL_PP(response_header), entryp, sizeof(zval *), NULL);
				zend_hash_internal_pointer_reset(Z_ARRVAL_P(stream->wrapperdata));
				while (zend_hash_get_current_data(Z_ARRVAL_P(stream->wrapperdata), (void **)&entryp) == SUCCESS) {
					zval_add_ref(entryp);
					zend_hash_next_index_insert(Z_ARRVAL_PP(response_header), entryp, sizeof(zval *), NULL);
					zend_hash_move_forward(Z_ARRVAL_P(stream->wrapperdata));
				}
				zval_dtor(stream->wrapperdata);
				FREE_ZVAL(stream->wrapperdata);
			}
		} else {
			php_stream_wrapper_log_error(wrapper, options TSRMLS_CC, "HTTP request failed! %s", tmp_line);
		}
	}
out:
	if (http_header_line)
		efree(http_header_line);
	if (scratch)
		efree(scratch);
	if (resource) {
		php_url_free(resource);
	}

	if (stream) {
		if (header_init) {
			stream->wrapperdata = *response_header;
			zval_add_ref(response_header);
		}
		php_stream_notify_progress_init(context, 0, file_size);
		/* Restore original chunk size now that we're done with headers (if applicable) */
		if (options & STREAM_WILL_CAST)
			php_stream_set_chunk_size(stream, chunk_size);

		/* restore the users auto-detect-line-endings setting */
		stream->flags |= eol_detect;

		/* as far as streams are concerned, we are now at the start of
		 * the stream */
		stream->position = 0;
	}

	return stream;
}

php_stream *php_stream_url_wrap_http(php_stream_wrapper *wrapper, char *path, char *mode, int options, char **opened_path, php_stream_context *context STREAMS_DC TSRMLS_DC)
{
	return php_stream_url_wrap_http_ex(wrapper, path, mode, options, opened_path, context, PHP_URL_REDIRECT_MAX, 1 STREAMS_CC TSRMLS_CC);
}

static int php_stream_http_stream_stat(php_stream_wrapper *wrapper,
		php_stream *stream,
		php_stream_statbuf *ssb
		TSRMLS_DC)
{
	/* one day, we could fill in the details based on Date: and Content-Length:
	 * headers.  For now, we return with a failure code to prevent the underlying
	 * file's details from being used instead. */
	return -1;
}

static php_stream_wrapper_ops http_stream_wops = {
	php_stream_url_wrap_http,
	NULL, /* stream_close */
	php_stream_http_stream_stat,
	NULL, /* stat_url */
	NULL, /* opendir */
	"HTTP"
};

php_stream_wrapper php_stream_http_wrapper =	{
	&http_stream_wops,
	NULL,
	1 /* is_url */
};

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
