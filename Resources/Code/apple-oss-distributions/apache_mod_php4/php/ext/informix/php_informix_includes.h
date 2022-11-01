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
   | Authors: Danny Heijl <Danny.Heijl@cevi.be>, initial cut (ODS 7)      |
   |          Christian Cartus <chc@idgruppe.de>, blobs, and IUS 9        |
   |          Jouni Ahto <jouni.ahto@exdec.fi>, configuration stuff       |
   |          Corne' Cornelius <cornec@reach.co.za>,  input descriptors   |
   | based on mysql code by: Zeev Suraski <zeev@php.net>                  |
   +----------------------------------------------------------------------+
*/

/* $Id: php_informix_includes.h,v 1.1.4.4.8.3 2007/12/31 07:22:48 sebastian Exp $ */

#ifndef PHP_INFORMIX_INCLUDES_H
#define PHP_INFORMIX_INCLUDES_H

#if HAVE_IFX                        /* with Informix */

#undef TYPEMAX
#undef CHAR

#include "locator.h"
#include "sqltypes.h"

#define MAX_RESID          64
#define BLOBINFILE 0      /* 0=in memory, 1=in file */

/* query result set data */
typedef struct ifx_res {
	char connecid[32];
	char cursorid[32];
	char descrpid[32];
	char i_descrpid[32];
	char statemid[32];
	int  isscroll;
	int  ishold;
	int  iscursory;
	int  paramquery;
	int  numcols;
	int  numicols;
	int  rowid;
	int  affected_rows;
	long sqlerrd[6];
	int res_id[MAX_RESID];
} IFX_RES;

typedef struct _IFX_IDRES {
	int type;
	union {
		struct {
			int mode;
			loc_t blob_data;
		} BLOBRES;

		struct {
			char *char_data;
			int len;
		} CHARRES;

#if HAVE_IFX_IUS
		struct {
			ifx_lo_t slob_data;
			ifx_lo_create_spec_t *createspec;
			int lofd;
		} SLOBRES;
#endif
	} DATARES;
} IFX_IDRES;

#define BLOB DATARES.BLOBRES
#define CHAR DATARES.CHARRES

#if HAVE_IFX_IUS
#define SLOB DATARES.SLOBRES
#endif

#endif /* HAVE_IFX */

#endif /* PHP_INFORMIX_INCLUDES_H */
