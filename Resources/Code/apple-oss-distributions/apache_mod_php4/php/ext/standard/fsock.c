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
   | Authors: Paul Panotzki - Bunyip Information Systems                  |
   |          Jim Winstead <jimw@php.net>                                 |
   |          Sascha Schumann <sascha@schumann.cx>                        |
   +----------------------------------------------------------------------+
*/

/* $Id: fsock.c,v 1.106.2.13.4.4 2007/12/31 07:22:52 sebastian Exp $ */

/* converted to PHP Streams and moved much code to main/network.c [wez] */

/* Synced with php 3.0 revision 1.121 1999-06-18 [ssb] */
/* Synced with php 3.0 revision 1.133 1999-07-21 [sas] */

#include "php.h"
#include "php_globals.h"
#include <stdlib.h>
#include <stddef.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef PHP_WIN32
#include <winsock.h>
#elif defined(NETWARE)
#ifdef USE_WINSOCK
#include <novsock2.h>
#else
#include <netinet/in.h>
#include <netdb.h>
#include <sys/select.h>
#endif
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
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifndef PF_INET
#define PF_INET AF_INET
#endif

#ifndef PF_UNIX
#define PF_UNIX AF_UNIX
#endif

#include <string.h>
#include <errno.h>

#include "base64.h"
#include "file.h"
#include "url.h"
#include "fsock.h"

#include "php_network.h"

#ifdef PHP_WIN32
#define EWOULDBLOCK WSAEWOULDBLOCK
#elif defined(NETWARE)
#ifdef USE_WINSOCK
#define EWOULDBLOCK WSAEWOULDBLOCK
#endif
#endif

/* {{{ php_lookup_hostname */

/*
 * Converts a host name to an IP address.
 * TODO: This looks like unused code suitable for nuking.
 */
PHPAPI int php_lookup_hostname(const char *addr, struct in_addr *in)
{
	struct hostent *host_info;

	if (!inet_aton(addr, in)) {
		/* XXX NOT THREAD SAFE */
		host_info = gethostbyname(addr);
		if (host_info == 0) {
			/* Error: unknown host */
			return -1;
		}
		*in = *((struct in_addr *) host_info->h_addr);
	}
	return 0;
}
/* }}} */

/* {{{ php_fsockopen() */

static void php_fsockopen_stream(INTERNAL_FUNCTION_PARAMETERS, int persistent)
{
	char *host;
	int host_len;
	long port = -1;
	zval *zerrno = NULL, *zerrstr = NULL, *zcontext = NULL;
	double timeout = FG(default_socket_timeout);
	unsigned long conv;
	struct timeval tv;
	char *hashkey = NULL;
	php_stream *stream = NULL;
	php_stream_context *context = NULL;
	int err;

	RETVAL_FALSE;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|lzzdr", &host, &host_len, &port, &zerrno, &zerrstr, &timeout, &zcontext) == FAILURE) {
		RETURN_FALSE;
	}
	if (zcontext) {
		ZEND_FETCH_RESOURCE(context, php_stream_context*, &zcontext, -1, "stream-context", php_le_stream_context());
	}

	if (persistent) {
		spprintf(&hashkey, 0, "pfsockopen__%s:%ld", host, port);

		switch(php_stream_from_persistent_id(hashkey, &stream TSRMLS_CC)) {
			case PHP_STREAM_PERSISTENT_SUCCESS:
				if (_php_network_is_stream_alive(stream TSRMLS_CC)) {
					php_stream_to_zval(stream, return_value);
				} else {
					/* it died; we need to replace it */
					php_stream_pclose(stream);
					break;
				}
				
				/* fall through */
			case PHP_STREAM_PERSISTENT_FAILURE:
				efree(hashkey);
				return;
		}
	}

	/* prepare the timeout value for use */
	conv = (unsigned long) (timeout * 1000000.0);
	tv.tv_sec = conv / 1000000;
	tv.tv_usec = conv % 1000000;

	if (zerrno)	{
		zval_dtor(zerrno);
		ZVAL_LONG(zerrno, 0);
	}
	if (zerrstr) {
		zval_dtor(zerrstr);
		ZVAL_STRING(zerrstr, "", 1);
	}

	if (port > 0)	{ /* connect to a host */
		enum php_sslflags_t { php_ssl_none, php_ssl_v23, php_ssl_tls };
		enum php_sslflags_t ssl_flags = php_ssl_none;
		struct {
			char *proto;
			int protolen;
			int socktype;
			enum php_sslflags_t ssl_flags;
			/* more flags to be added here */
		} sockmodes[] = {
			{ "udp://", 6, SOCK_DGRAM,	php_ssl_none },
			{ "tcp://", 6, SOCK_STREAM,	php_ssl_none },
			{ "ssl://", 6, SOCK_STREAM, php_ssl_v23 },
			{ "tls://", 6, SOCK_STREAM, php_ssl_tls },
			/* more modes to be added here */
			{ NULL, 0, 0 }
		};
		int socktype = SOCK_STREAM;
		int i;

		for (i = 0; sockmodes[i].proto != NULL; i++) {
			if (strncmp(host, sockmodes[i].proto, sockmodes[i].protolen) == 0) {
				ssl_flags = sockmodes[i].ssl_flags;		
				socktype = sockmodes[i].socktype;
				host += sockmodes[i].protolen;
				break;
			}
		}
#ifndef HAVE_OPENSSL_EXT
		if (ssl_flags != php_ssl_none) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "no SSL support in this build");
		}
		else
#endif
		stream = php_stream_sock_open_host(host, (unsigned short)port, socktype, &tv, hashkey);

		/* Preserve error */
		err = php_socket_errno();

		if (stream == NULL) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "unable to connect to %s:%ld", host, port);
		} else if (context) {
			php_stream_context_set(stream, context);
		}
		
#ifdef HAVE_OPENSSL_EXT
		if (stream && ssl_flags != php_ssl_none) {
			int ssl_ret = FAILURE;
			switch(ssl_flags)	{
				case php_ssl_v23:
					ssl_ret = php_stream_sock_ssl_activate_with_method(stream, 1, SSLv23_client_method(), NULL TSRMLS_CC);
					break;
				case php_ssl_tls:
					ssl_ret = php_stream_sock_ssl_activate_with_method(stream, 1, TLSv1_client_method(), NULL TSRMLS_CC);
					break;
				default:
					/* unknown ?? */
					break;
			}
			if (ssl_ret == FAILURE) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "failed to activate SSL mode %d", ssl_flags);
				php_stream_close(stream);
				stream = NULL;
			}
		}
#endif
		
	} else {
		/* FIXME: Win32 - this probably does not return sensible errno and errstr */
		stream = php_stream_sock_open_unix(host, host_len, hashkey, &tv);
		err = php_socket_errno();
	}

	if (hashkey)
		efree(hashkey);
	
	if (stream == NULL)	{
		if (zerrno) {
			zval_dtor(zerrno);
			ZVAL_LONG(zerrno, err);
		}
		if (zerrstr) {
			char *buf = php_socket_strerror(err, NULL, 0);

			/* no need to dup; we would only need to efree buf anyway */
			zval_dtor(zerrstr);
			ZVAL_STRING(zerrstr, buf, 0);
		}
		RETURN_FALSE;
	}
	
	if (zcontext) {
		zend_list_addref(Z_RESVAL_P(zcontext));
	}
	php_stream_to_zval(stream, return_value);
}

/* }}} */

/* {{{ proto int fsockopen(string hostname, int port [, int errno [, string errstr [, float timeout [, resource context]]]])
   Open Internet or Unix domain socket connection */
PHP_FUNCTION(fsockopen)
{
	php_fsockopen_stream(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */
/* {{{ proto int pfsockopen(string hostname, int port [, int errno [, string errstr [, float timeout [, resource context]]]])
   Open persistent Internet or Unix domain socket connection */
PHP_FUNCTION(pfsockopen)
{
	php_fsockopen_stream(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */

/* {{{ RSHUTDOWN_FUNCTION(fsock) */
PHP_RSHUTDOWN_FUNCTION(fsock)
{
	return SUCCESS;
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
