/*
   +----------------------------------------------------------------------+
   | Zend Engine                                                          |
   +----------------------------------------------------------------------+
   | Copyright (c) 2003 Sascha Schumann                                   |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.00 of the Zend license,     |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.zend.com/license/2_00.txt.                                |
   | If you did not receive a copy of the Zend license and are unable to  |
   | obtain it through the world-wide-web, please send a note to          |
   | license@zend.com so we can mail you a copy immediately.              |
   +----------------------------------------------------------------------+
   | Authors: Sascha Schumann <sascha@schumann.cx>                        |
   |          Ard Biesheuvel <ard@ard.nu>                                 |
   +----------------------------------------------------------------------+
*/

/* for zend_bool and limits.h */
#include "zend.h"

#if defined(__i386__) && defined(__GNUC__)

#define ZEND_SIGNED_MULTIPLY_LONG(a, b, lval, dval, usedval) do {	\
	long __tmpvar; 													\
	__asm__ ("imul %3,%0\n"											\
		"adc $0,%1" 												\
			: "=r"(__tmpvar),"=r"(usedval) 							\
			: "0"(a), "r"(b), "1"(0));								\
	if (usedval) (dval) = (double) (a) * (double) (b);				\
	else (lval) = __tmpvar;											\
} while (0)

#else

#define ZEND_SIGNED_MULTIPLY_LONG(a, b, lval, dval, usedval) do {		\
	long   __lres  = (a) * (b);											\
	double __dres  = (double)(a) * (double)(b);							\
	double __delta = (double) __lres - __dres;							\
	if ( ((usedval) = (( __dres + __delta ) != __dres))) {				\
		(dval) = __dres;												\
	} else {															\
		(lval) = __lres;												\
	}																	\
} while (0)

#endif
