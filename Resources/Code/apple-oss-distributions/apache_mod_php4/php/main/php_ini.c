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
   | Author: Zeev Suraski <zeev@zend.com>                                 |
   +----------------------------------------------------------------------+
 */

/* $Id: php_ini.c,v 1.106.2.15.2.5 2007/12/31 07:22:54 sebastian Exp $ */

/* Check CWD for php.ini */
#define INI_CHECK_CWD

#include "php.h"
#include "ext/standard/info.h"
#include "zend_ini.h"
#include "php_ini.h"
#include "ext/standard/dl.h"
#include "zend_extensions.h"
#include "zend_highlight.h"
#include "SAPI.h"
#include "php_main.h"
#include "php_scandir.h"
#ifdef PHP_WIN32
#include "win32/php_registry.h"
#endif

#if HAVE_SCANDIR && HAVE_ALPHASORT && HAVE_DIRENT_H
#include <dirent.h>
#endif

#ifndef S_ISREG
#define S_ISREG(mode)   (((mode) & S_IFMT) == S_IFREG)
#endif

typedef struct _php_extension_lists {
	zend_llist engine;
	zend_llist functions;
} php_extension_lists;


/* True globals */
static HashTable configuration_hash;
PHPAPI char *php_ini_opened_path=NULL;
static php_extension_lists extension_lists;
PHPAPI char *php_ini_scanned_files=NULL;

/* {{{ php_ini_displayer_cb
 */
static void php_ini_displayer_cb(zend_ini_entry *ini_entry, int type)
{
	if (ini_entry->displayer) {
		ini_entry->displayer(ini_entry, type);
	} else {
		char *display_string;
		uint display_string_length, esc_html=0;
		TSRMLS_FETCH();

		if (type == ZEND_INI_DISPLAY_ORIG && ini_entry->modified) {
			if (ini_entry->orig_value && ini_entry->orig_value[0]) {
				display_string = ini_entry->orig_value;
				display_string_length = ini_entry->orig_value_length;
				esc_html = !sapi_module.phpinfo_as_text;
			} else {
				if (!sapi_module.phpinfo_as_text) {
					display_string = "<i>no value</i>";
					display_string_length = sizeof("<i>no value</i>") - 1;
				} else {
					display_string = "no value";
					display_string_length = sizeof("no value") - 1;
				}	
			}
		} else if (ini_entry->value && ini_entry->value[0]) {
			display_string = ini_entry->value;
			display_string_length = ini_entry->value_length;
			esc_html = !sapi_module.phpinfo_as_text;
		} else {
			if (!sapi_module.phpinfo_as_text) {
				display_string = "<i>no value</i>";
				display_string_length = sizeof("<i>no value</i>") - 1;
			} else {
				display_string = "no value";
				display_string_length = sizeof("no value") - 1;
			}	
		}

		if (esc_html) {
			php_html_puts(display_string, display_string_length TSRMLS_CC);
		} else {
			PHPWRITE(display_string, display_string_length);
		}
	}
}
/* }}} */

/* {{{ php_ini_displayer
 */
static int php_ini_displayer(zend_ini_entry *ini_entry, int module_number TSRMLS_DC)
{
	if (ini_entry->module_number != module_number) {
		return 0;
	}
	if (!sapi_module.phpinfo_as_text) {
		PUTS("<tr>");
		PUTS("<td class=\"e\">");
		PHPWRITE(ini_entry->name, ini_entry->name_length - 1);
		PUTS("</td><td class=\"v\">");
		php_ini_displayer_cb(ini_entry, ZEND_INI_DISPLAY_ACTIVE);
		PUTS("</td><td class=\"v\">");
		php_ini_displayer_cb(ini_entry, ZEND_INI_DISPLAY_ORIG);
		PUTS("</td></tr>\n");
	} else {
		PHPWRITE(ini_entry->name, ini_entry->name_length - 1);
		PUTS(" => ");
		php_ini_displayer_cb(ini_entry, ZEND_INI_DISPLAY_ACTIVE);
		PUTS(" => ");
		php_ini_displayer_cb(ini_entry, ZEND_INI_DISPLAY_ORIG);
		PUTS("\n");
	}	
	return 0;
}
/* }}} */

/* {{{ display_ini_entries
 */
PHPAPI void display_ini_entries(zend_module_entry *module)
{
	int module_number;
	TSRMLS_FETCH();

	if (module) {
		module_number = module->module_number;
	} else { 
		module_number = 0;
	}
	php_info_print_table_start();
	php_info_print_table_header(3, "Directive", "Local Value", "Master Value");
	zend_hash_apply_with_argument(EG(ini_directives), (apply_func_arg_t) php_ini_displayer, (void *) (long) module_number TSRMLS_CC);
	php_info_print_table_end();
}
/* }}} */

/* php.ini support */

#ifdef ZTS
# if (ZEND_DEBUG)
# define ZEND_EXTENSION_TOKEN	"zend_extension_debug_ts"
# else
# define ZEND_EXTENSION_TOKEN	"zend_extension_ts"
# endif
#else
# if (ZEND_DEBUG)
# define ZEND_EXTENSION_TOKEN	"zend_extension_debug"
# else
# define ZEND_EXTENSION_TOKEN	"zend_extension"
# endif
#endif

/* {{{ pvalue_config_destructor
 */
static void pvalue_config_destructor(zval *pvalue)
{   
	if (Z_TYPE_P(pvalue) == IS_STRING && Z_STRVAL_P(pvalue) != empty_string) {
		free(Z_STRVAL_P(pvalue));
	}
}
/* }}} */

/* {{{ php_config_ini_parser_cb
 */
static void php_config_ini_parser_cb(zval *arg1, zval *arg2, int callback_type, void *arg)
{
	switch (callback_type) {
		case ZEND_INI_PARSER_ENTRY: {
				zval *entry;

				if (!arg2) {
					break;
				}
				if (!strcasecmp(Z_STRVAL_P(arg1), "extension")) { /* load function module */
					zval copy;
					
					copy = *arg2;
					zval_copy_ctor(&copy);
					copy.refcount = 0;
					zend_llist_add_element(&extension_lists.functions, &copy); 
				} else if (!strcasecmp(Z_STRVAL_P(arg1), ZEND_EXTENSION_TOKEN)) { /* load Zend extension */
					char *extension_name = estrndup(Z_STRVAL_P(arg2), Z_STRLEN_P(arg2));
					
					zend_llist_add_element(&extension_lists.engine, &extension_name);
				} else {
					zend_hash_update(&configuration_hash, Z_STRVAL_P(arg1), Z_STRLEN_P(arg1) + 1, arg2, sizeof(zval), (void **) &entry);
					Z_STRVAL_P(entry) = zend_strndup(Z_STRVAL_P(entry), Z_STRLEN_P(entry));
				}
			}
			break;

		case ZEND_INI_PARSER_SECTION:
			break;
	}
}
/* }}} */

/* {{{ php_load_function_extension_cb
 */
static void php_load_function_extension_cb(void *arg TSRMLS_DC)
{
	zval *extension = (zval *) arg;
	zval zval;

	php_dl(extension, MODULE_PERSISTENT, &zval TSRMLS_CC);
}
/* }}} */

/* {{{ php_load_zend_extension_cb
 */
static void php_load_zend_extension_cb(void *arg TSRMLS_DC)
{
	zend_load_extension(*((char **) arg));
}
/* }}} */

#ifdef PHP_WIN32
#define NUM_INI_SEARCH_LOCATIONS	4
#else
#define NUM_INI_SEARCH_LOCATIONS	3
#endif

/* {{{ php_init_config
 */
int php_init_config()
{
	char *env_location, *php_ini_search_path;
#ifdef PHP_WIN32
	char *registry_location;
#endif
	char *binary_location;
	int safe_mode_state;
	char *open_basedir;
	int free_ini_search_path=0;
	zend_file_handle fh = {0};
	struct stat sb;
	char ini_file[MAXPATHLEN];
	char *p;
	zend_llist scanned_ini_list;
	int l, total_l=0;
	zend_llist_element *element;
	TSRMLS_FETCH();

	if (zend_hash_init(&configuration_hash, 0, NULL, (dtor_func_t) pvalue_config_destructor, 1) == FAILURE) {
		return FAILURE;
	}

	if (sapi_module.ini_defaults) {
		sapi_module.ini_defaults(&configuration_hash);
	}

	zend_llist_init(&extension_lists.engine, sizeof(char *), (llist_dtor_func_t) free_estring, 1);
	zend_llist_init(&extension_lists.functions, sizeof(zval), (llist_dtor_func_t)  ZVAL_DESTRUCTOR, 1);
	zend_llist_init(&scanned_ini_list, sizeof(char *), (llist_dtor_func_t) free_estring, 1);
	
	safe_mode_state = PG(safe_mode);
	open_basedir = PG(open_basedir);

	env_location = getenv("PHPRC");
	if (!env_location) {
		env_location = "";
	}
	if (sapi_module.php_ini_path_override) {
		php_ini_search_path = sapi_module.php_ini_path_override;
		free_ini_search_path = 0;
	} else {
		int search_path_size;
		char *default_location;
		static const char paths_separator[] = { ZEND_PATHS_SEPARATOR, 0 };

		search_path_size = MAXPATHLEN * NUM_INI_SEARCH_LOCATIONS + strlen(env_location) + NUM_INI_SEARCH_LOCATIONS + 1;
		php_ini_search_path = (char *) emalloc(search_path_size);
		free_ini_search_path = 1;
		php_ini_search_path[0] = 0;

		/*
		 * Prepare search path
		 */

		/* Add environment location */
		if (env_location[0]) {
			if (*php_ini_search_path) {
				strlcat(php_ini_search_path, paths_separator, search_path_size);
			}
			strlcat(php_ini_search_path, env_location, search_path_size);
		}

#ifdef PHP_WIN32
		registry_location = GetIniPathFromRegistry();
		if (registry_location) {
			if (*php_ini_search_path) {
				strlcat(php_ini_search_path, paths_separator, search_path_size);
			}
			strlcat(php_ini_search_path, registry_location, search_path_size);
			efree(registry_location);
		}
#endif

		/* Add cwd */
#ifdef INI_CHECK_CWD
		if (strcmp(sapi_module.name, "cli") != 0) {
			if (*php_ini_search_path) {
				strlcat(php_ini_search_path, paths_separator, search_path_size);
			}
			strlcat(php_ini_search_path, ".", search_path_size);
		}
#endif

		/* Add binary directory */
#ifdef PHP_WIN32
		binary_location = (char *) emalloc(MAXPATHLEN);
		if (GetModuleFileName(0, binary_location, MAXPATHLEN) == 0) {
			efree(binary_location);
			binary_location = NULL;
		}
#else
		if (sapi_module.executable_location) {
			binary_location = estrdup(sapi_module.executable_location);
		} else {
			binary_location = NULL;
		}
#endif
		if (binary_location) {
			char *separator_location = strrchr(binary_location, DEFAULT_SLASH);
			
			if (separator_location) {
				*(separator_location+1) = 0;
			}
			if (*php_ini_search_path) {
				strlcat(php_ini_search_path, paths_separator, search_path_size);
			}
			strlcat(php_ini_search_path, binary_location, search_path_size);
			efree(binary_location);
		}

		/* Add default location */
#ifdef PHP_WIN32
		default_location = (char *) emalloc(MAXPATHLEN + 1);
	
		if (0 < GetWindowsDirectory(default_location, MAXPATHLEN)) {
			if (*php_ini_search_path) {
				strlcat(php_ini_search_path, paths_separator, search_path_size);
			}
			strlcat(php_ini_search_path, default_location, search_path_size);
		}
		efree(default_location);
#else
		default_location = PHP_CONFIG_FILE_PATH;
		if (*php_ini_search_path) {
			strlcat(php_ini_search_path, paths_separator, search_path_size);
		}
		strlcat(php_ini_search_path, default_location, search_path_size);
#endif
	}

	PG(safe_mode) = 0;
	PG(open_basedir) = NULL;

	/* Check if php_ini_path_override is a file */
	if (!sapi_module.php_ini_ignore) {
		if (sapi_module.php_ini_path_override && sapi_module.php_ini_path_override[0]) {
			struct stat statbuf;
	
			if (!VCWD_STAT(sapi_module.php_ini_path_override, &statbuf)) {
				if (!((statbuf.st_mode & S_IFMT) == S_IFDIR)) {
					fh.handle.fp = VCWD_FOPEN(sapi_module.php_ini_path_override, "r");
					fh.filename = sapi_module.php_ini_path_override;
				}
			}
		}
		/* Search php-%sapi-module-name%.ini file in search path */
		if (!fh.handle.fp) {
			const char *fmt = "php-%s.ini";
			char *ini_fname = emalloc(strlen(fmt) + strlen(sapi_module.name));
			sprintf(ini_fname, fmt, sapi_module.name);
			fh.handle.fp = php_fopen_with_path(ini_fname, "r", php_ini_search_path, &php_ini_opened_path TSRMLS_CC);
			efree(ini_fname);
			if (fh.handle.fp) {
				fh.filename = php_ini_opened_path;
			}
		}
		/* Search php.ini file in search path */
		if (!fh.handle.fp) {
			fh.handle.fp = php_fopen_with_path("php.ini", "r", php_ini_search_path, &php_ini_opened_path TSRMLS_CC);
			if (fh.handle.fp) {
				fh.filename = php_ini_opened_path;
			}
		}
	}

	if (free_ini_search_path) {
		efree(php_ini_search_path);
	}

	PG(safe_mode) = safe_mode_state;
	PG(open_basedir) = open_basedir;

	if (fh.handle.fp) {
		fh.type = ZEND_HANDLE_FP;

		zend_parse_ini_file(&fh, 1, php_config_ini_parser_cb, &extension_lists);
	
		{
			zval tmp;
		
			Z_STRLEN(tmp) = strlen(fh.filename);
			Z_STRVAL(tmp) = zend_strndup(fh.filename, Z_STRLEN(tmp));
			Z_TYPE(tmp) = IS_STRING;
			zend_hash_update(&configuration_hash, "cfg_file_path", sizeof("cfg_file_path"), (void *) &tmp, sizeof(zval), NULL);
			if (php_ini_opened_path) {
				efree(php_ini_opened_path);
			}
			php_ini_opened_path = zend_strndup(Z_STRVAL(tmp), Z_STRLEN(tmp));
		}
	}

	/* If the config_file_scan_dir is set at compile-time, go and scan this directory and
	 * parse any .ini files found in this directory. */
	if (!sapi_module.php_ini_ignore && strlen(PHP_CONFIG_FILE_SCAN_DIR)) {
		struct dirent **namelist;
		int ndir, i;

		if ((ndir = php_scandir(PHP_CONFIG_FILE_SCAN_DIR, &namelist, 0, php_alphasort)) > 0) {
			for (i = 0; i < ndir; i++) {
				/* check for a .ini extension */
				if (!(p = strrchr(namelist[i]->d_name, '.')) || (p && strcmp(p, ".ini"))) {
					free(namelist[i]);
					continue;
				}
				snprintf(ini_file, MAXPATHLEN, "%s%c%s", PHP_CONFIG_FILE_SCAN_DIR, DEFAULT_SLASH, namelist[i]->d_name);
				if (VCWD_STAT(ini_file, &sb) == 0) {
					if (S_ISREG(sb.st_mode)) {
						if ((fh.handle.fp = VCWD_FOPEN(ini_file, "r"))) {
							fh.filename = ini_file;
							fh.type = ZEND_HANDLE_FP;
							zend_parse_ini_file(&fh, 1, php_config_ini_parser_cb, &extension_lists);
							/* Here, add it to the list of ini files read */
							l = strlen(ini_file);
							total_l += l + 2;
							p = estrndup(ini_file, l); 
							zend_llist_add_element(&scanned_ini_list, &p);
						}
					}
				}
				free(namelist[i]);
			}
			free(namelist);

			/* 
			 * Don't need an extra byte for the \0 in this malloc as the last
			 * element will not get a trailing , which gives us the byte for the \0
			 */
			if (total_l) {
				php_ini_scanned_files = (char *) malloc(total_l);
				*php_ini_scanned_files = '\0';
				for (element = scanned_ini_list.head; element; element = element->next) {
					strlcat(php_ini_scanned_files, *(char **)element->data, total_l);
					strlcat(php_ini_scanned_files, element->next ? ",\n" : "\n", total_l);
				}	
			}
			zend_llist_destroy(&scanned_ini_list);
		}
	}
	return SUCCESS;
}
/* }}} */

/* {{{ php_shutdown_config
 */
int php_shutdown_config(void)
{
	zend_hash_destroy(&configuration_hash);
	if (php_ini_opened_path) {
		free(php_ini_opened_path);
		php_ini_opened_path = NULL;
	}
	if (php_ini_scanned_files) {
		free(php_ini_scanned_files);
		php_ini_scanned_files = NULL;
	}
	return SUCCESS;
}
/* }}} */

/* {{{ php_ini_delayed_modules_startup
 */
void php_ini_delayed_modules_startup(TSRMLS_D)
{
	zend_llist_apply(&extension_lists.engine, php_load_zend_extension_cb TSRMLS_CC);
	zend_llist_apply(&extension_lists.functions, php_load_function_extension_cb TSRMLS_CC);

	zend_llist_destroy(&extension_lists.engine);
	zend_llist_destroy(&extension_lists.functions);
}
/* }}} */

/* {{{ cfg_get_entry
 */
zval *cfg_get_entry(char *name, uint name_length)
{
	zval *tmp;

	if (zend_hash_find(&configuration_hash, name, name_length, (void **) &tmp) == SUCCESS) {
		return tmp;
	} else {
		return NULL;
	}
}
/* }}} */

/* {{{ cfg_get_long
 */
PHPAPI int cfg_get_long(char *varname, long *result)
{
	zval *tmp, var;
	
	if (zend_hash_find(&configuration_hash, varname, strlen(varname) + 1, (void **) &tmp) == FAILURE) {
		*result = (long) NULL;
		return FAILURE;
	}
	var = *tmp;
	zval_copy_ctor(&var);
	convert_to_long(&var);
	*result = Z_LVAL(var);
	return SUCCESS;
}
/* }}} */

/* {{{ cfg_get_double
 */
PHPAPI int cfg_get_double(char *varname, double *result)
{
	zval *tmp, var;
	
	if (zend_hash_find(&configuration_hash, varname, strlen(varname) + 1, (void **) &tmp) == FAILURE) {
		*result = (double) 0;
		return FAILURE;
	}
	var = *tmp;
	zval_copy_ctor(&var);
	convert_to_double(&var);
	*result = Z_DVAL(var);
	return SUCCESS;
}
/* }}} */

/* {{{ cfg_get_string
 */
PHPAPI int cfg_get_string(char *varname, char **result)
{
	zval *tmp;

	if (zend_hash_find(&configuration_hash, varname, strlen(varname)+1, (void **) &tmp) == FAILURE) {
		*result = NULL;
		return FAILURE;
	}
	*result = Z_STRVAL_P(tmp);
	return SUCCESS;
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
