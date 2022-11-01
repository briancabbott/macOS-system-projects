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
   | Author: Marcus Boerger <helly@php.net>                               |
   +----------------------------------------------------------------------+
 */

/* $Id: dba_flatfile.c,v 1.8.2.6.4.3 2007/12/31 07:22:46 sebastian Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"

#if DBA_FLATFILE
#include "php_flatfile.h"

#include "libflatfile/flatfile.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define FLATFILE_DATA flatfile *dba = info->dbf
#define FLATFILE_GKEY datum gkey; gkey.dptr = (char *) key; gkey.dsize = keylen

DBA_OPEN_FUNC(flatfile)
{
	int fd;
#ifdef F_SETFL
	int flags;
#endif

	if (info->mode != DBA_READER) {
		if (SUCCESS != php_stream_cast(info->fp, PHP_STREAM_AS_FD, (void*)&fd, 1)) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not cast stream");
			return FAILURE;
		}
#ifdef F_SETFL
		/* Needed becasue some systems do not allow to write to the original 
		 * file contents with O_APPEND being set.
		 */
		flags = fcntl(fd, F_SETFL);
		fcntl(fd, F_SETFL, flags & ~O_APPEND);
#endif
	}

	info->dbf = pemalloc(sizeof(flatfile), info->flags&DBA_PERSISTENT);
	memset(info->dbf, 0, sizeof(flatfile));

	((flatfile*)info->dbf)->fp = info->fp;

	return SUCCESS;
}

DBA_CLOSE_FUNC(flatfile)
{
	FLATFILE_DATA;

	if (dba->nextkey.dptr) {
		efree(dba->nextkey.dptr);
	}
	pefree(dba, info->flags&DBA_PERSISTENT);
}

DBA_FETCH_FUNC(flatfile)
{
	datum gval;
	char *new = NULL;

	FLATFILE_DATA;
	FLATFILE_GKEY;

	gval = flatfile_fetch(dba, gkey TSRMLS_CC);
	if (gval.dptr) {
		if (newlen) {
			*newlen = gval.dsize;
		}
		new = estrndup(gval.dptr, gval.dsize);
		efree(gval.dptr);
	}
	return new;
}

DBA_UPDATE_FUNC(flatfile)
{
	datum gval;

	FLATFILE_DATA;
	FLATFILE_GKEY;
	gval.dptr = (char *) val;
	gval.dsize = vallen;
	
	switch(flatfile_store(dba, gkey, gval, mode==1 ? FLATFILE_INSERT : FLATFILE_REPLACE TSRMLS_CC)) {
	case -1:
		php_error_docref1(NULL TSRMLS_CC, key, E_WARNING, "Operation not possible");
		return FAILURE;
	default:
	case 0:
		return SUCCESS;
	case 1:
		php_error_docref1(NULL TSRMLS_CC, key, E_WARNING, "Key already exists");
		return SUCCESS;
	}
}

DBA_EXISTS_FUNC(flatfile)
{
	datum gval;
	FLATFILE_DATA;
	FLATFILE_GKEY;
	
	gval = flatfile_fetch(dba, gkey TSRMLS_CC);
	if (gval.dptr) {
		efree(gval.dptr);
		return SUCCESS;
	}
	return FAILURE;
}

DBA_DELETE_FUNC(flatfile)
{
	FLATFILE_DATA;
	FLATFILE_GKEY;
	return(flatfile_delete(dba, gkey TSRMLS_CC) == -1 ? FAILURE : SUCCESS);
}

DBA_FIRSTKEY_FUNC(flatfile)
{
	FLATFILE_DATA;

	if (dba->nextkey.dptr) {
		efree(dba->nextkey.dptr);
	}
	dba->nextkey = flatfile_firstkey(dba TSRMLS_CC);
	if (dba->nextkey.dptr) {
		if (newlen)  {
			*newlen = dba->nextkey.dsize;
		}
		return estrndup(dba->nextkey.dptr, dba->nextkey.dsize);
	}
	return NULL;
}

DBA_NEXTKEY_FUNC(flatfile)
{
	FLATFILE_DATA;
	
	if (!dba->nextkey.dptr) {
		return NULL;
	}
	
	if (dba->nextkey.dptr) {
		efree(dba->nextkey.dptr);
	}
	dba->nextkey = flatfile_nextkey(dba TSRMLS_CC);
	if (dba->nextkey.dptr) {
		if (newlen) {
			*newlen = dba->nextkey.dsize;
		}
		return estrndup(dba->nextkey.dptr, dba->nextkey.dsize);
	}
	return NULL;
}

DBA_OPTIMIZE_FUNC(flatfile)
{
	/* dummy */
	return SUCCESS;
}

DBA_SYNC_FUNC(flatfile)
{
	/* dummy */
	return SUCCESS;
}

DBA_INFO_FUNC(flatfile)
{
	return estrdup(flatfile_version());
}

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
