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
   | Authors: Chris Vandomelen <chrisv@b0rked.dhs.org>                    |
   |          Sterling Hughes  <sterling@php.net>                         |
   |                                                                      |
   | WinSock: Daniel Beulshausen <daniel@php4win.de>                      |
   +----------------------------------------------------------------------+
 */

#ifndef PHP_SOCKETS_H
#define PHP_SOCKETS_H

/* $Id: php_sockets.h,v 1.26.2.5.2.3 2007/12/31 07:22:51 sebastian Exp $ */

#if HAVE_SOCKETS

extern zend_module_entry sockets_module_entry;
#define phpext_sockets_ptr &sockets_module_entry

#ifdef PHP_WIN32
#define PHP_SOCKETS_API __declspec(dllexport)
#include <winsock.h>
#else
#define PHP_SOCKETS_API
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#endif

PHP_MINIT_FUNCTION(sockets);
PHP_MINFO_FUNCTION(sockets);
PHP_RINIT_FUNCTION(sockets);
PHP_RSHUTDOWN_FUNCTION(sockets);

PHP_FUNCTION(socket_iovec_alloc);
PHP_FUNCTION(socket_iovec_free);
PHP_FUNCTION(socket_iovec_set);
PHP_FUNCTION(socket_iovec_fetch);
PHP_FUNCTION(socket_iovec_add);
PHP_FUNCTION(socket_iovec_delete);
PHP_FUNCTION(socket_select);
PHP_FUNCTION(socket_create_listen);
PHP_FUNCTION(socket_create_pair);
PHP_FUNCTION(socket_accept);
PHP_FUNCTION(socket_set_nonblock);
PHP_FUNCTION(socket_set_block);
PHP_FUNCTION(socket_listen);
PHP_FUNCTION(socket_close);
PHP_FUNCTION(socket_write);
PHP_FUNCTION(socket_read);
PHP_FUNCTION(socket_getsockname);
PHP_FUNCTION(socket_getpeername);
PHP_FUNCTION(socket_create);
PHP_FUNCTION(socket_connect);
PHP_FUNCTION(socket_strerror);
PHP_FUNCTION(socket_bind);
PHP_FUNCTION(socket_recv);
PHP_FUNCTION(socket_send);
PHP_FUNCTION(socket_recvfrom);
PHP_FUNCTION(socket_sendto);
#ifdef HAVE_CMSGHDR
PHP_FUNCTION(socket_recvmsg);
#endif
PHP_FUNCTION(socket_sendmsg);
PHP_FUNCTION(socket_readv);
PHP_FUNCTION(socket_writev);
PHP_FUNCTION(socket_get_option);
PHP_FUNCTION(socket_set_option);
PHP_FUNCTION(socket_shutdown);
PHP_FUNCTION(socket_last_error);
PHP_FUNCTION(socket_clear_error);

typedef struct php_iovec {
	struct iovec	*iov_array;
	unsigned int	count;
} php_iovec_t;

#ifndef PHP_WIN32
typedef int SOCKET;
#endif

typedef struct {
	SOCKET	bsd_socket;
	int		type;
	int		error;
} php_socket;

ZEND_BEGIN_MODULE_GLOBALS(sockets)
	int last_error;
	char *strerror_buf;
ZEND_END_MODULE_GLOBALS(sockets)

#ifdef ZTS
#define SOCKETS_G(v) TSRMG(sockets_globals_id, zend_sockets_globals *, v)
#else
#define SOCKETS_G(v) (sockets_globals.v)
#endif

#else
#define phpext_sockets_ptr NULL
#endif

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */

