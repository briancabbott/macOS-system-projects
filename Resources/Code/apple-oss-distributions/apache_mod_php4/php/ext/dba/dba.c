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
   |          Marcus Boerger <helly@php.net>                              |
   +----------------------------------------------------------------------+
 */

/* $Id: dba.c,v 1.61.2.25.4.3 2007/12/31 07:22:46 sebastian Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"

#if HAVE_DBA

#include "php_ini.h"
#include <stdio.h> 
#include <fcntl.h>
#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
 
#include "php_dba.h"
#include "ext/standard/info.h"
#include "ext/standard/php_string.h"
#include "ext/standard/flock_compat.h" 

#include "php_gdbm.h"
#include "php_ndbm.h"
#include "php_dbm.h"
#include "php_cdb.h"
#include "php_db2.h"
#include "php_db3.h"
#include "php_db4.h"
#include "php_flatfile.h"
#include "php_inifile.h"

/* {{{ dba_functions[]
 */
function_entry dba_functions[] = {
	PHP_FE(dba_open, NULL)
	PHP_FE(dba_popen, NULL)
	PHP_FE(dba_close, NULL)
	PHP_FE(dba_delete, NULL)
	PHP_FE(dba_exists, NULL)
	PHP_FE(dba_fetch, NULL)
	PHP_FE(dba_insert, NULL)
	PHP_FE(dba_replace, NULL)
	PHP_FE(dba_firstkey, NULL)
	PHP_FE(dba_nextkey, NULL)
	PHP_FE(dba_optimize, NULL)
	PHP_FE(dba_sync, NULL)
	PHP_FE(dba_handlers, NULL)
	PHP_FE(dba_list, NULL)
	{NULL, NULL, NULL}
};
/* }}} */

PHP_MINIT_FUNCTION(dba);
PHP_MSHUTDOWN_FUNCTION(dba);
PHP_MINFO_FUNCTION(dba);

zend_module_entry dba_module_entry = {
	STANDARD_MODULE_HEADER,
	"dba",
	dba_functions, 
	PHP_MINIT(dba), 
	PHP_MSHUTDOWN(dba),
	NULL,
	NULL,
	PHP_MINFO(dba),
	NO_VERSION_YET,
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_DBA
ZEND_GET_MODULE(dba)
#endif

/* {{{ macromania */

#define DBA_ID_PARS 											\
	zval **id; 													\
	dba_info *info = NULL; 										\
	int ac = ZEND_NUM_ARGS()

/* these are used to get the standard arguments */

#define DBA_GET1 												\
	if(ac != 1 || zend_get_parameters_ex(ac, &id) != SUCCESS) { 		\
		WRONG_PARAM_COUNT; 										\
	}

/* {{{ php_dba_myke_key */
static size_t php_dba_make_key(zval **key, char **key_str, char **key_free TSRMLS_DC)
{
	if (Z_TYPE_PP(key) == IS_ARRAY) {
		zval **group, **name;
		HashPosition pos;
		size_t len;
	
		if (zend_hash_num_elements(Z_ARRVAL_PP(key)) != 2) {
			php_error_docref(NULL TSRMLS_CC, E_ERROR, "Key does not have exactly two elements: (key, name)");
			return -1;
		}
		zend_hash_internal_pointer_reset_ex(Z_ARRVAL_PP(key), &pos);
		zend_hash_get_current_data_ex(Z_ARRVAL_PP(key), (void **) &group, &pos);
		zend_hash_move_forward_ex(Z_ARRVAL_PP(key), &pos);
		zend_hash_get_current_data_ex(Z_ARRVAL_PP(key), (void **) &name, &pos);
		convert_to_string_ex(group);
		convert_to_string_ex(name);
		if (Z_STRLEN_PP(group) == 0) {
			*key_str = Z_STRVAL_PP(name);
			*key_free = NULL;
			return Z_STRLEN_PP(name);
		}
		len = spprintf(key_str, 0, "[%s]%s", Z_STRVAL_PP(group), Z_STRVAL_PP(name));
		*key_free = *key_str;
		return len;
	} else {
		convert_to_string_ex(key);
		*key_str = Z_STRVAL_PP(key);
		*key_free = NULL;
		return Z_STRLEN_PP(key);
	}
}
/* }}} */

#define DBA_GET2 												\
	zval **key; 												\
	char *key_str, *key_free;									\
	size_t key_len; 											\
	if(ac != 2 || zend_get_parameters_ex(ac, &key, &id) != SUCCESS) { 	\
		WRONG_PARAM_COUNT; 										\
	} 															\
	if ((key_len = php_dba_make_key(key, &key_str, &key_free TSRMLS_CC)) == 0) {\
		RETURN_FALSE;											\
	}

#define DBA_GET2_3												\
	zval **key; 												\
	char *key_str, *key_free;									\
	size_t key_len; 											\
	zval **tmp; 												\
	int skip = 0;  												\
	switch(ac) {												\
	case 2: 													\
		if (zend_get_parameters_ex(ac, &key, &id) != SUCCESS) { \
			WRONG_PARAM_COUNT; 									\
		} 														\
		break;  												\
	case 3: 													\
		if (zend_get_parameters_ex(ac, &key, &tmp, &id) != SUCCESS) { \
			WRONG_PARAM_COUNT; 									\
		} 														\
		convert_to_long_ex(tmp);								\
		skip = Z_LVAL_PP(tmp);  								\
		break;  												\
	default:													\
		WRONG_PARAM_COUNT; 										\
	} 															\
	if ((key_len = php_dba_make_key(key, &key_str, &key_free TSRMLS_CC)) == 0) {\
		RETURN_FALSE;											\
	}

#define DBA_GET3 												\
	zval **key, **val;											\
	char *key_str, *key_free;									\
	size_t key_len; 											\
	if(ac != 3 || zend_get_parameters_ex(ac, &key, &val, &id) != SUCCESS) { 	\
		WRONG_PARAM_COUNT; 										\
	} 															\
	convert_to_string_ex(val);									\
	if ((key_len = php_dba_make_key(key, &key_str, &key_free TSRMLS_CC)) == 0) {\
		RETURN_FALSE;											\
	}

#define DBA_ID_GET 												\
	ZEND_FETCH_RESOURCE2(info, dba_info *, id, -1, "DBA identifier", le_db, le_pdb);
	
#define DBA_ID_GET1   DBA_ID_PARS; DBA_GET1;   DBA_ID_GET
#define DBA_ID_GET2   DBA_ID_PARS; DBA_GET2;   DBA_ID_GET
#define DBA_ID_GET2_3 DBA_ID_PARS; DBA_GET2_3; DBA_ID_GET
#define DBA_ID_GET3   DBA_ID_PARS; DBA_GET3;   DBA_ID_GET

#define DBA_ID_DONE												\
	if (key_free) efree(key_free)
/* a DBA handler must have specific routines */

#define DBA_NAMED_HND(alias, name, flags) \
{\
	#alias, flags, dba_open_##name, dba_close_##name, dba_fetch_##name, dba_update_##name, \
	dba_exists_##name, dba_delete_##name, dba_firstkey_##name, dba_nextkey_##name, \
	dba_optimize_##name, dba_sync_##name, dba_info_##name \
},

#define DBA_HND(name, flags) DBA_NAMED_HND(name, name, flags)

/* check whether the user has write access */
#define DBA_WRITE_CHECK \
	if(info->mode != DBA_WRITER && info->mode != DBA_TRUNC && info->mode != DBA_CREAT) { \
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "You cannot perform a modification to a database without proper access"); \
		RETURN_FALSE; \
	}

/* }}} */

/* {{{ globals */

static dba_handler handler[] = {
#if DBA_GDBM
	DBA_HND(gdbm, DBA_LOCK_EXT) /* Locking done in library if set */
#endif
#if DBA_DBM
	DBA_HND(dbm, DBA_LOCK_ALL) /* No lock in lib */
#endif
#if DBA_NDBM
	DBA_HND(ndbm, DBA_LOCK_ALL) /* Could be done in library: filemode = 0644 + S_ENFMT */
#endif
#if DBA_CDB
	DBA_HND(cdb, DBA_STREAM_OPEN|DBA_LOCK_ALL) /* No lock in lib */
#endif
#if DBA_CDB_BUILTIN
    DBA_NAMED_HND(cdb_make, cdb, DBA_STREAM_OPEN|DBA_LOCK_ALL) /* No lock in lib */
#endif
#if DBA_DB2
	DBA_HND(db2, DBA_LOCK_ALL) /* No lock in lib */
#endif
#if DBA_DB3
	DBA_HND(db3, DBA_LOCK_ALL) /* No lock in lib */
#endif
#if DBA_DB4
	DBA_HND(db4, DBA_LOCK_ALL) /* No lock in lib */
#endif
#if DBA_INIFILE
	DBA_HND(inifile, DBA_STREAM_OPEN|DBA_LOCK_ALL) /* No lock in lib */
#endif
#if DBA_FLATFILE
	DBA_HND(flatfile, DBA_STREAM_OPEN|DBA_LOCK_ALL) /* No lock in lib */
#endif
	{ NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL }
};

#if DBA_FLATFILE
#define DBA_DEFAULT "flatfile"
#elif DBA_DB4
#define DBA_DEFAULT "db4"
#elif DBA_DB3
#define DBA_DEFAULT "db3"
#elif DBA_DB2
#define DBA_DEFAULT "db2"
#elif DBA_GDBM
#define DBA_DEFAULT "gdbm"
#elif DBA_NBBM
#define DBA_DEFAULT "ndbm"
#elif DBA_DBM
#define DBA_DEFAULT "dbm"
#else
#define DBA_DEFAULT ""
#endif
/* cdb/cdb_make and ini are no option here */

ZEND_BEGIN_MODULE_GLOBALS(dba)
	char *default_handler;
	dba_handler *default_hptr;
ZEND_END_MODULE_GLOBALS(dba) 

ZEND_DECLARE_MODULE_GLOBALS(dba)

#ifdef ZTS
#define DBA_G(v) TSRMG(dba_globals_id, zend_dba_globals *, v)
#else
#define DBA_G(v) (dba_globals.v)
#endif 

static int le_db;
static int le_pdb;

/* {{{ dba_fetch_resource
PHPAPI void dba_fetch_resource(dba_info **pinfo, zval **id TSRMLS_DC)
{
	dba_info *info;
	DBA_ID_FETCH
	*pinfo = info;
}
*/
/* }}} */

/* {{{ dba_get_handler
PHPAPI dba_handler *dba_get_handler(const char* handler_name)
{
	dba_handler *hptr;
	for (hptr = handler; hptr->name && strcasecmp(hptr->name, handler_name); hptr++);
	return hptr;
}
*/
/* }}} */

/* {{{ dba_close 
 */ 
static void dba_close(dba_info *info TSRMLS_DC)
{
	if (info->hnd) {
		info->hnd->close(info TSRMLS_CC);
	}
	if (info->path) {
		pefree(info->path, info->flags&DBA_PERSISTENT);
	}
	if (info->fp && info->fp!=info->lock.fp) {
		if(info->flags&DBA_PERSISTENT) {
			php_stream_pclose(info->fp);
		} else {
			php_stream_close(info->fp);
		}
	}
	if (info->lock.fd) {
		php_flock(info->lock.fd, LOCK_UN);
		/*close(info->lock.fd);*/
		info->lock.fd = 0;
	}
	if (info->lock.fp) {
		if(info->flags&DBA_PERSISTENT) {
			php_stream_pclose(info->lock.fp);
		} else {
			php_stream_close(info->lock.fp);
		}
	}
	if (info->lock.name) {
		pefree(info->lock.name, info->flags&DBA_PERSISTENT);
	}
	pefree(info, info->flags&DBA_PERSISTENT);
}
/* }}} */

/* {{{ dba_close_rsrc
 */
static void dba_close_rsrc(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
	dba_info *info = (dba_info *)rsrc->ptr; 

	dba_close(info TSRMLS_CC);
}
/* }}} */

/* {{{ dba_close_pe_rsrc_deleter */
int dba_close_pe_rsrc_deleter(list_entry *le, void *pDba TSRMLS_DC)
{
	return le->ptr == pDba;
}
/* }}} */

/* {{{ dba_close_pe_rsrc */
static void dba_close_pe_rsrc(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
	dba_info *info = (dba_info *)rsrc->ptr; 

	/* closes the resource by calling dba_close_rsrc() */
	zend_hash_apply_with_argument(&EG(persistent_list), (apply_func_arg_t) dba_close_pe_rsrc_deleter, info TSRMLS_CC);
}
/* }}} */

/* {{{ PHP_INI
 */
ZEND_INI_MH(OnUpdateDefaultHandler)
{
	dba_handler *hptr;

	if (!strlen(new_value)) {
		DBA_G(default_hptr) = NULL;
		return OnUpdateString(entry, new_value, new_value_length, mh_arg1, mh_arg2, mh_arg3, stage TSRMLS_CC);
	}

	for (hptr = handler; hptr->name && strcasecmp(hptr->name, new_value); hptr++);

	if (!hptr->name) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "No such handler: %s", new_value);
		return FAILURE;
	}
	DBA_G(default_hptr) = hptr;
	return OnUpdateString(entry, new_value, new_value_length, mh_arg1, mh_arg2, mh_arg3, stage TSRMLS_CC);
}

PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("dba.default_handler", DBA_DEFAULT, PHP_INI_ALL, OnUpdateDefaultHandler, default_handler,    zend_dba_globals, dba_globals)
PHP_INI_END()
/* }}} */
 
/* {{{ php_dba_init_globals
 */
static void php_dba_init_globals(zend_dba_globals *dba_globals)
{
	dba_globals->default_handler = "";
	dba_globals->default_hptr    = NULL;
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(dba)
{
	ZEND_INIT_MODULE_GLOBALS(dba, php_dba_init_globals, NULL);
	REGISTER_INI_ENTRIES();
	le_db = zend_register_list_destructors_ex(dba_close_rsrc, NULL, "dba", module_number);
	le_pdb = zend_register_list_destructors_ex(dba_close_pe_rsrc, dba_close_rsrc, "dba persistent", module_number);
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(dba)
{
	UNREGISTER_INI_ENTRIES();
	return SUCCESS;
}
/* }}} */

#include "ext/standard/php_smart_str.h"

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(dba)
{
	dba_handler *hptr;
	smart_str handlers = {0};

	for(hptr = handler; hptr->name; hptr++) {
		smart_str_appends(&handlers, hptr->name);
		smart_str_appendc(&handlers, ' ');
 	}

	php_info_print_table_start();
 	php_info_print_table_row(2, "DBA support", "enabled");
	if (handlers.c) {
		smart_str_0(&handlers);
		php_info_print_table_row(2, "Supported handlers", handlers.c);
		smart_str_free(&handlers);
	} else {
		php_info_print_table_row(2, "Supported handlers", "none");
	}
	php_info_print_table_end();
}
/* }}} */

/* {{{ php_dba_update
 */
static void php_dba_update(INTERNAL_FUNCTION_PARAMETERS, int mode)
{
	char *v;
	int len;
	DBA_ID_GET3;

	DBA_WRITE_CHECK;
	
	if (PG(magic_quotes_runtime)) {
		len = Z_STRLEN_PP(val);
		v = estrndup(Z_STRVAL_PP(val), len);
		php_stripslashes(v, &len TSRMLS_CC); 
		if(info->hnd->update(info, key_str, key_len, v, len, mode TSRMLS_CC) == SUCCESS) {
			efree(v);
			DBA_ID_DONE;
			RETURN_TRUE;
		}
		efree(v);
	} else {
		if(info->hnd->update(info, key_str, key_len, VALLEN(val), mode TSRMLS_CC) == SUCCESS)
		{
			DBA_ID_DONE;
			RETURN_TRUE;
		}
	}
	DBA_ID_DONE;
	RETURN_FALSE;
}
/* }}} */

#define FREENOW if(args) efree(args); if(key) efree(key)

/* {{{ php_find_dbm
 */
dba_info *php_dba_find(const char* path TSRMLS_DC)
{
	list_entry *le;
	dba_info *info;
	int numitems, i;

	numitems = zend_hash_next_free_element(&EG(regular_list));
	for (i=1; i<numitems; i++) {
		if (zend_hash_index_find(&EG(regular_list), i, (void **) &le)==FAILURE) {
			continue;
		}
		if (Z_TYPE_P(le) == le_db || Z_TYPE_P(le) == le_pdb) {
			info = (dba_info *)(le->ptr);
			if (!strcmp(info->path, path)) {
				return (dba_info *)(le->ptr);
			}
		}
	}

	return NULL;
}
/* }}} */

/* {{{ php_dba_open
 */
static void php_dba_open(INTERNAL_FUNCTION_PARAMETERS, int persistent)
{
	zval ***args = (zval ***) NULL;
	int ac = ZEND_NUM_ARGS();
	dba_mode_t modenr;
	dba_info *info, *other;
	dba_handler *hptr;
	char *key = NULL, *error = NULL;
	int keylen = 0;
	int i;
	int lock_mode, lock_flag, lock_dbf = 0;
	char *file_mode;
	char mode[4], *pmode, *lock_file_mode = NULL;
	int persistent_flag = persistent ? STREAM_OPEN_PERSISTENT : 0;
	
	if(ac < 2) {
		WRONG_PARAM_COUNT;
	}
	
	/* we pass additional args to the respective handler */
	args = safe_emalloc(ac, sizeof(zval *), 0);
	if (zend_get_parameters_array_ex(ac, args) != SUCCESS) {
		FREENOW;
		WRONG_PARAM_COUNT;
	}
		
	/* we only take string arguments */
	for (i = 0; i < ac; i++) {
		convert_to_string_ex(args[i]);
		keylen += Z_STRLEN_PP(args[i]);
	}

	if (persistent) {
		list_entry *le;
		
		/* calculate hash */
		key = safe_emalloc(keylen, 1, 1);
		key[keylen] = '\0';
		keylen = 0;
		
		for(i = 0; i < ac; i++) {
			memcpy(key+keylen, Z_STRVAL_PP(args[i]), Z_STRLEN_PP(args[i]));
			keylen += Z_STRLEN_PP(args[i]);
		}

		/* try to find if we already have this link in our persistent list */
		if (zend_hash_find(&EG(persistent_list), key, keylen+1, (void **) &le) == SUCCESS) {
			FREENOW;
			
			if (Z_TYPE_P(le) != le_pdb) {
				RETURN_FALSE;
			}
		
			info = (dba_info *)le->ptr;

			ZEND_REGISTER_RESOURCE(return_value, info, le_pdb);
			return;
		}
	}
	
	if (ac==2) {
		hptr = DBA_G(default_hptr);
		if (!hptr) {
			php_error_docref2(NULL TSRMLS_CC, Z_STRVAL_PP(args[0]), Z_STRVAL_PP(args[1]), E_WARNING, "No default handler selected");
			FREENOW;
			RETURN_FALSE;
		}
	} else {
		for (hptr = handler; hptr->name && strcasecmp(hptr->name, Z_STRVAL_PP(args[2])); hptr++);
	}

	if (!hptr->name) {
		php_error_docref2(NULL TSRMLS_CC, Z_STRVAL_PP(args[0]), Z_STRVAL_PP(args[1]), E_WARNING, "No such handler: %s", Z_STRVAL_PP(args[2]));
		FREENOW;
		RETURN_FALSE;
	}

	/* Check mode: [rwnc][fl]?t?
	 * r: Read
	 * w: Write
	 * n: Create/Truncate
	 * c: Create
	 *
	 * d: force lock on database file
	 * l: force lock on lck file
	 * -: ignore locking
	 *
	 * t: test open database, warning if locked
	 */
	strlcpy(mode, Z_STRVAL_PP(args[1]), sizeof(mode));
	pmode = &mode[0];
	if (pmode[0] && (pmode[1]=='d' || pmode[1]=='l' || pmode[1]=='-')) { /* force lock on db file or lck file or disable locking */
		switch (pmode[1]) {
		case 'd':
			lock_dbf = 1;
			if ((hptr->flags & DBA_LOCK_ALL) == 0) {
				lock_flag = (hptr->flags & DBA_LOCK_ALL);
				break;
			}
			/* no break */
		case 'l':
			lock_flag = DBA_LOCK_ALL;
			if ((hptr->flags & DBA_LOCK_ALL) == 0) {
				php_error_docref2(NULL TSRMLS_CC, Z_STRVAL_PP(args[0]), Z_STRVAL_PP(args[1]), E_NOTICE, "Handler %s does locking internally", hptr->name);
			}
			break;
		default:
		case '-':
			if ((hptr->flags & DBA_LOCK_ALL) == 0) {
				php_error_docref2(NULL TSRMLS_CC, Z_STRVAL_PP(args[0]), Z_STRVAL_PP(args[1]), E_WARNING, "Locking cannot be disabled for handler %s", hptr->name);
				FREENOW;
				RETURN_FALSE;
			}
			lock_flag = 0;
			break;
		}
	} else {
		lock_flag = (hptr->flags&DBA_LOCK_ALL);
		lock_dbf = 1;
	}
	switch (*pmode++) {
		case 'r': 
			modenr = DBA_READER; 
			lock_mode = (lock_flag & DBA_LOCK_READER) ? LOCK_SH : 0;
			file_mode = "r";
			break;
		case 'w': 
			modenr = DBA_WRITER; 
			lock_mode = (lock_flag & DBA_LOCK_WRITER) ? LOCK_EX : 0;
			file_mode = "r+b";
			break;
		case 'c': 
			modenr = DBA_CREAT; 
			lock_mode = (lock_flag & DBA_LOCK_CREAT) ? LOCK_EX : 0;
			if (lock_mode) {
				if (lock_dbf) {
					/* the create/append check will be done on the lock
					 * when the lib opens the file it is already created
					 */
					file_mode = "r+b";       /* read & write, seek 0 */
					lock_file_mode = "a+b";  /* append */
				} else {
					file_mode = "a+b";       /* append */
					lock_file_mode = "w+b";  /* create/truncate */
				}
			} else {
			file_mode = "a+b";
			}
			/* In case of the 'a+b' append mode, the handler is responsible 
			 * to handle any rewind problems (see flatfile handler).
			 */
			break;
		case 'n':
			modenr = DBA_TRUNC;
			lock_mode = (lock_flag & DBA_LOCK_TRUNC) ? LOCK_EX : 0;
			file_mode = "w+b";
			break;
		default:
			php_error_docref2(NULL TSRMLS_CC, Z_STRVAL_PP(args[0]), Z_STRVAL_PP(args[1]), E_WARNING, "Illegal DBA mode");
			FREENOW;
			RETURN_FALSE;
	}
	if (!lock_file_mode) {
		lock_file_mode = file_mode;
	}
	if (*pmode=='d' || *pmode=='l' || *pmode=='-') {
		pmode++; /* done already - skip here */
	}
	if (*pmode=='t') {
		pmode++;
		if (!lock_flag) {
			php_error_docref2(NULL TSRMLS_CC, Z_STRVAL_PP(args[0]), Z_STRVAL_PP(args[1]), E_WARNING, "You cannot combine modifiers - (no lock) and t (test lock)");
			FREENOW;
			RETURN_FALSE;
		}
		if (!lock_mode) {
			if ((hptr->flags & DBA_LOCK_ALL) == 0) {
				php_error_docref2(NULL TSRMLS_CC, Z_STRVAL_PP(args[0]), Z_STRVAL_PP(args[1]), E_WARNING, "Handler %s uses its own locking which doesn't support mode modifier t (test lock)", hptr->name);
				FREENOW;
				RETURN_FALSE;
			} else {
				php_error_docref2(NULL TSRMLS_CC, Z_STRVAL_PP(args[0]), Z_STRVAL_PP(args[1]), E_WARNING, "Handler %s doesn't uses locking for this mode which makes modifier t (test lock) obsolete", hptr->name);
				FREENOW;
				RETURN_FALSE;
			}
		} else {
			lock_mode |= LOCK_NB; /* test =: non blocking */
		}
	}
	if (*pmode) {
		php_error_docref2(NULL TSRMLS_CC, Z_STRVAL_PP(args[0]), Z_STRVAL_PP(args[1]), E_WARNING, "Illegal DBA mode");
		FREENOW;
		RETURN_FALSE;
	}
			
	info = pemalloc(sizeof(dba_info), persistent);
	memset(info, 0, sizeof(dba_info));
	info->path = pestrdup(Z_STRVAL_PP(args[0]), persistent);
	info->mode = modenr;
	info->argc = ac - 3;
	info->argv = args + 3;
	info->flags = (hptr->flags & ~DBA_LOCK_ALL) | (lock_flag & DBA_LOCK_ALL) | (persistent ? DBA_PERSISTENT : 0);
	info->lock.mode = lock_mode;

	/* if any open call is a locking call:
	 * check if we already habe a locking call open that should block this call
	 * the problem is some systems would allow read during write
	 */
	if (hptr->flags & DBA_LOCK_ALL) {
		if ((other = php_dba_find(info->path TSRMLS_CC)) != NULL) {
			if (   ( (lock_mode&LOCK_EX)        && (other->lock.mode&(LOCK_EX|LOCK_SH)) )
			    || ( (other->lock.mode&LOCK_EX) && (lock_mode&(LOCK_EX|LOCK_SH))        )
			   ) {
				error = "Unable to establish lock (database file already open)"; /* force failure exit */
			}
		}
	}

	if (!error && lock_mode) {
		if (lock_dbf) {
			info->lock.name = pestrdup(info->path, persistent);
		} else {
			spprintf(&info->lock.name, 0, "%s.lck", info->path);
			if (!strcmp(file_mode, "r")) {
				/* when in read only mode try to use existing .lck file first */
				/* do not log errors for .lck file while in read ony mode on .lck file */
				lock_file_mode = "rb";
				info->lock.fp = php_stream_open_wrapper(info->lock.name, lock_file_mode, STREAM_MUST_SEEK|IGNORE_PATH|ENFORCE_SAFE_MODE|persistent_flag, NULL);
			}
			if (!info->lock.fp) {
				/* when not in read mode or failed to open .lck file read only. now try again in create(write) mode and log errors */
				lock_file_mode = "a+b";
			}
		}
		if (!info->lock.fp) {
			info->lock.fp = php_stream_open_wrapper(info->lock.name, lock_file_mode, STREAM_MUST_SEEK|REPORT_ERRORS|IGNORE_PATH|ENFORCE_SAFE_MODE|persistent_flag, NULL);
		}
		if (!info->lock.fp) {
			dba_close(info TSRMLS_CC);
			/* stream operation already wrote an error message */
			FREENOW;
			RETURN_FALSE;
		}
		if (php_stream_cast(info->lock.fp, PHP_STREAM_AS_FD, (void*)&info->lock.fd, 1) == FAILURE) {
			dba_close(info TSRMLS_CC);
			/* stream operation already wrote an error message */
			FREENOW;
			RETURN_FALSE;
		}
		if (php_flock(info->lock.fd, lock_mode)) {		
			error = "Unable to establish lock"; /* force failure exit */
		}
	}

	/* centralised open stream for builtin */
	if (!error && (hptr->flags&DBA_STREAM_OPEN)==DBA_STREAM_OPEN) {
		if (info->lock.fp && lock_dbf) {
			info->fp = info->lock.fp; /* use the same stream for locking and database access */
		} else {
			info->fp = php_stream_open_wrapper(info->path, file_mode, STREAM_MUST_SEEK|REPORT_ERRORS|IGNORE_PATH|ENFORCE_SAFE_MODE|persistent_flag, NULL);
		}
		if (!info->fp) {
			dba_close(info TSRMLS_CC);
			/* stream operation already wrote an error message */
			FREENOW;
			RETURN_FALSE;
		}
	}

	if (error || hptr->open(info, &error TSRMLS_CC) != SUCCESS) {
		dba_close(info TSRMLS_CC);
		php_error_docref2(NULL TSRMLS_CC, Z_STRVAL_PP(args[0]), Z_STRVAL_PP(args[1]), E_WARNING, "Driver initialization failed for handler: %s%s%s", hptr->name, error?": ":"", error?error:"");
		FREENOW;
		RETURN_FALSE;
	}

	info->hnd = hptr;
	info->argc = 0;
	info->argv = NULL;

	if (persistent) {
		list_entry new_le;

		Z_TYPE(new_le) = le_pdb;
		new_le.ptr = info;
		if (zend_hash_update(&EG(persistent_list), key, keylen+1, &new_le, sizeof(list_entry), NULL) == FAILURE) {
			dba_close(info TSRMLS_CC);
			php_error_docref2(NULL TSRMLS_CC, Z_STRVAL_PP(args[0]), Z_STRVAL_PP(args[1]), E_WARNING, "Could not register persistent resource");
			FREENOW;
			RETURN_FALSE;
		}
	}

	ZEND_REGISTER_RESOURCE(return_value, info, (persistent ? le_pdb : le_db));
	FREENOW;
}
/* }}} */
#undef FREENOW

/* {{{ proto resource dba_popen(string path, string mode [, string handlername, string ...])
   Opens path using the specified handler in mode persistently */
PHP_FUNCTION(dba_popen)
{
	php_dba_open(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */

/* {{{ proto resource dba_open(string path, string mode [, string handlername, string ...])
   Opens path using the specified handler in mode*/
PHP_FUNCTION(dba_open)
{
	php_dba_open(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ proto void dba_close(resource handle)
   Closes database */
PHP_FUNCTION(dba_close)
{
	DBA_ID_GET1;
	
	zend_list_delete(Z_RESVAL_PP(id));
}
/* }}} */

/* {{{ proto bool dba_exists(string key, resource handle)
   Checks, if the specified key exists */
PHP_FUNCTION(dba_exists)
{
	DBA_ID_GET2;

	if(info->hnd->exists(info, key_str, key_len TSRMLS_CC) == SUCCESS) {
		DBA_ID_DONE;
		RETURN_TRUE;
	}
	DBA_ID_DONE;
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto string dba_fetch(string key, [int skip ,] resource handle)
   Fetches the data associated with key */
PHP_FUNCTION(dba_fetch)
{
	char *val;
	int len = 0;
	DBA_ID_GET2_3;

	if (ac==3) {
		if (!strcmp(info->hnd->name, "cdb")) {
			if (skip < 0) {
				php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Handler %s accepts only skip values greater than or equal to zero, using skip=0", info->hnd->name);
				skip = 0;
			}
		} else if (!strcmp(info->hnd->name, "inifile")) {
			/* "-1" is compareable to 0 but allows a non restrictive 
			 * access which is fater. For example 'inifile' uses this
			 * to allow faster access when the key was already found
			 * using firstkey/nextkey. However explicitly setting the
			 * value to 0 ensures the first value. 
			 */
			if (skip < -1) {
				php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Handler %s accepts only skip value -1 and greater, using skip=0", info->hnd->name);
				skip = 0;
			}
		} else {
			php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Handler %s does not support optional skip parameter, the value will be ignored", info->hnd->name);
			skip = 0;			
		}
	} else {
		skip = 0; 
	}
	if((val = info->hnd->fetch(info, key_str, key_len, skip, &len TSRMLS_CC)) != NULL) {
		if (val && PG(magic_quotes_runtime)) {
			val = php_addslashes(val, len, &len, 1 TSRMLS_CC);
		}
		DBA_ID_DONE;
		RETURN_STRINGL(val, len, 0);
	} 
	DBA_ID_DONE;
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto string dba_firstkey(resource handle)
   Resets the internal key pointer and returns the first key */
PHP_FUNCTION(dba_firstkey)
{
	char *fkey;
	int len;
	DBA_ID_GET1;

	fkey = info->hnd->firstkey(info, &len TSRMLS_CC);
	if(fkey)
		RETURN_STRINGL(fkey, len, 0);
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto string dba_nextkey(resource handle)
   Returns the next key */
PHP_FUNCTION(dba_nextkey)
{
	char *nkey;
	int len;
	DBA_ID_GET1;

	nkey = info->hnd->nextkey(info, &len TSRMLS_CC);
	if(nkey)
		RETURN_STRINGL(nkey, len, 0);
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto bool dba_delete(string key, resource handle)
   Deletes the entry associated with key
   If inifile: remove all other key lines */
PHP_FUNCTION(dba_delete)
{
	DBA_ID_GET2;
	
	DBA_WRITE_CHECK;
	
	if(info->hnd->delete(info, key_str, key_len TSRMLS_CC) == SUCCESS)
	{
		DBA_ID_DONE;
		RETURN_TRUE;
	}
	DBA_ID_DONE;
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto bool dba_insert(string key, string value, resource handle)
   If not inifile: Insert value as key, return false, if key exists already 
   If inifile: Add vakue as key (next instance of key) */
PHP_FUNCTION(dba_insert)
{
	php_dba_update(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */

/* {{{ proto bool dba_replace(string key, string value, resource handle)
   Inserts value as key, replaces key, if key exists already
   If inifile: remove all other key lines */
PHP_FUNCTION(dba_replace)
{
	php_dba_update(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ proto bool dba_optimize(resource handle)
   Optimizes (e.g. clean up, vacuum) database */
PHP_FUNCTION(dba_optimize)
{
	DBA_ID_GET1;
	
	DBA_WRITE_CHECK;
	if(info->hnd->optimize(info TSRMLS_CC) == SUCCESS) {
		RETURN_TRUE;
	}
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto bool dba_sync(resource handle)
   Synchronizes database */
PHP_FUNCTION(dba_sync)
{
	DBA_ID_GET1;
	
	if(info->hnd->sync(info TSRMLS_CC) == SUCCESS) {
		RETURN_TRUE;
	}
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto array dba_handlers([bool full_info])
   List configured database handlers */
PHP_FUNCTION(dba_handlers)
{
	dba_handler *hptr;
	zend_bool full_info = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &full_info) == FAILURE) {
		ZEND_WRONG_PARAM_COUNT();
		RETURN_FALSE;
	}

	array_init(return_value);

	for(hptr = handler; hptr->name; hptr++) {
		if (full_info) {
			add_assoc_string(return_value, hptr->name, hptr->info(hptr, NULL TSRMLS_CC), 0);
		} else {
			add_next_index_string(return_value, hptr->name, 1);
		}
 	}
}
/* }}} */

/* {{{ proto array dba_list()
   List opened databases */
PHP_FUNCTION(dba_list)
{
	ulong numitems, i;
	zend_rsrc_list_entry *le;
	dba_info *info;

	if (ZEND_NUM_ARGS()!=0) {
		ZEND_WRONG_PARAM_COUNT();
		RETURN_FALSE;
	}

	array_init(return_value);

	numitems = zend_hash_next_free_element(&EG(regular_list));
	for (i=1; i<numitems; i++) {
		if (zend_hash_index_find(&EG(regular_list), i, (void **) &le)==FAILURE) {
			continue;
		}
		if (Z_TYPE_P(le) == le_db || Z_TYPE_P(le) == le_pdb) {
			info = (dba_info *)(le->ptr);
			add_index_string(return_value, i, info->path, 1);
		}
	}
}
/* }}} */

#endif /* HAVE_DBA */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
