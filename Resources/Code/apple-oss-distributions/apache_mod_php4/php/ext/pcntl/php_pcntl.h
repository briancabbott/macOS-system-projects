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
   | Author: Jason Greene <jason@inetgurus.net>                           |
   +----------------------------------------------------------------------+
 */

/* $Id: php_pcntl.h,v 1.11.4.3.2.3 2007/12/31 07:22:50 sebastian Exp $ */

#ifndef PHP_PCNTL_H
#define PHP_PCNTL_H

#include <sys/wait.h>
#include "php_signal.h"
#include "php_ticks.h"
extern zend_module_entry pcntl_module_entry;
#define phpext_pcntl_ptr &pcntl_module_entry

#ifdef PHP_WIN32
#define PHP_PCNTL_API __declspec(dllexport)
#else
#define PHP_PCNTL_API
#endif

PHP_MINIT_FUNCTION(pcntl);
PHP_MSHUTDOWN_FUNCTION(pcntl);
PHP_RINIT_FUNCTION(pcntl);
PHP_RSHUTDOWN_FUNCTION(pcntl);
PHP_MINFO_FUNCTION(pcntl);

PHP_FUNCTION(pcntl_alarm);
PHP_FUNCTION(pcntl_fork);
PHP_FUNCTION(pcntl_waitpid);
PHP_FUNCTION(pcntl_wifexited);
PHP_FUNCTION(pcntl_wifstopped);
PHP_FUNCTION(pcntl_wifsignaled);
PHP_FUNCTION(pcntl_wexitstatus);
PHP_FUNCTION(pcntl_wtermsig);
PHP_FUNCTION(pcntl_wstopsig);
PHP_FUNCTION(pcntl_signal);
PHP_FUNCTION(pcntl_exec);

struct php_pcntl_pending_signal {
	struct php_pcntl_pending_signal *next;
	long signo;
};

ZEND_BEGIN_MODULE_GLOBALS(pcntl)
	HashTable php_signal_table;
	int processing_signal_queue;
	struct php_pcntl_pending_signal *head, *tail, *spares;
ZEND_END_MODULE_GLOBALS(pcntl)

#ifdef ZTS
#define PCNTL_G(v) TSRMG(pcntl_globals_id, zend_pcntl_globals *, v)
#else
#define PCNTL_G(v)	(pcntl_globals.v)
#endif

#endif	/* PHP_PCNTL_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 */
