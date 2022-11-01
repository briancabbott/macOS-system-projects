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
   | Authors: David Croft <david@infotrek.co.uk>                          |
   |          John Donagher <john@webmeta.com>                            |
   +----------------------------------------------------------------------+
*/

/* $Id: php_pfpro.h,v 1.12.2.2.8.3 2007/12/31 07:22:50 sebastian Exp $ */

#ifndef PHP_PFPRO_H
#define PHP_PFPRO_H

#if HAVE_PFPRO

extern zend_module_entry pfpro_module_entry;
#define phpext_pfpro_ptr &pfpro_module_entry

#ifdef PHP_WIN32
#define PHP_PFPRO_API __declspec(dllexport)
#else
#define PHP_PFPRO_API
#endif

#if PFPRO_VERSION < 3
#define pfproVersion() PNVersion()
#define pfproInit() PNInit()
#define pfproCleanup() PNCleanup()
#endif

PHP_MINIT_FUNCTION(pfpro);
PHP_MSHUTDOWN_FUNCTION(pfpro);
PHP_RINIT_FUNCTION(pfpro);
PHP_RSHUTDOWN_FUNCTION(pfpro);
PHP_MINFO_FUNCTION(pfpro);

PHP_FUNCTION(pfpro_version);	        /* Return library version     */
PHP_FUNCTION(pfpro_init);               /* Initialise pfpro gateway   */
PHP_FUNCTION(pfpro_cleanup);            /* Shut down cleanly          */
PHP_FUNCTION(pfpro_process_raw);        /* Raw transaction processing */
PHP_FUNCTION(pfpro_process);            /* Transaction processing     */

ZEND_BEGIN_MODULE_GLOBALS(pfpro)
	int initialized;
	char *defaulthost;
	long defaultport;
	long defaulttimeout;
	char *proxyaddress;
	long proxyport;
	char *proxylogon;
	char *proxypassword;
ZEND_END_MODULE_GLOBALS(pfpro)

#ifdef ZTS
#define PFPROG(v) TSRMG(pfpro_globals_id, zend_pfpro_globals *, v)
#else
#define PFPROG(v) (pfpro_globals.v)
#endif

#else

#define phpext_pfpro_ptr NULL

#endif

#endif	/* PHP_PFPRO_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
