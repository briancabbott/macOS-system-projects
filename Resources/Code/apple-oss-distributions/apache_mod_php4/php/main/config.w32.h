/*
	Build Configuration for Win32.
	This has only been tested with MS VisualC++ 6 (and later).

	$Id: config.w32.h,v 1.61.2.6 2004/03/29 19:03:59 helly Exp $
*/

/* Default PHP / PEAR directories */
#define CONFIGURATION_FILE_PATH "php.ini"
#define PEAR_INSTALLDIR "c:\\php4\\pear"
#define PHP_BINDIR "c:\\php4"
#define PHP_CONFIG_FILE_PATH (getenv("SystemRoot"))?getenv("SystemRoot"):""
#define PHP_CONFIG_FILE_SCAN_DIR ""
#define PHP_DATADIR "c:\\php4"
#define PHP_EXTENSION_DIR "c:\\php4"
#define PHP_INCLUDE_PATH	".;c:\\php4\\pear"
#define PHP_LIBDIR "c:\\php4"
#define PHP_LOCALSTATEDIR "c:\\php4"
#define PHP_PREFIX "c:\\php4"
#define PHP_SYSCONFDIR "c:\\php4"

/* Enable / Disable BCMATH extension (default: enabled) */
#define WITH_BCMATH 1

/* Enable / Disable crypt() function (default: enabled) */
#define HAVE_CRYPT 1
#define PHP_STD_DES_CRYPT 1
#define PHP_EXT_DES_CRYPT 0
#define PHP_MD5_CRYPT 1
#define PHP_BLOWFISH_CRYPT 0

/* Enable / Disable CALENDAR extension (default: enabled) */
#define HAVE_CALENDAR 1

/* Enable / Disable COM extension (default: enabled) */
#define HAVE_COM 1

/* Enable / Disable CTYPE extension (default: enabled) */
#define HAVE_CTYPE 1

/* Enable / Disable FTP extension (default: enabled) */
#define HAVE_FTP 1

/* Enable / Disable MBSTRING extension (default: disabled) */
/* #define HAVE_MBSTRING 0 */
/* #define HAVE_MBREGEX  0 */
/* #define HAVE_MBSTR_CN 0 */
/* #define HAVE_MBSTR_JA 0 */
/* #define HAVE_MBSTR_KR 0 */
/* #define HAVE_MBSTR_RU 0 */
/* #define HAVE_MBSTR_TW 0 */

/* Enable / Disable MySQL extension (default: enabled) */
#define HAVE_MYSQL 1

/* Enable / Disable ODBC extension (default: enabled) */
#define HAVE_UODBC 1

/* Enable / Disable OVERLOAD extension (default: enabled) */
#define HAVE_OVERLOAD 1

/* Enable / Disable PCRE extension (default: enabled) */
#define HAVE_BUNDLED_PCRE	1
#define HAVE_PCRE 1

/* Enable / Disable SESSION extension (default: enabled) */
#define HAVE_PHP_SESSION 1

/* Enable / Disable TOKENIZER extension (default: enabled) */
#define HAVE_TOKENIZER 1

/* Enable / Disable WDDX extension (default: enabled) */
#define HAVE_WDDX 1

/* Enable / Disable XML extension (default: enabled) */
#define HAVE_LIBEXPAT 1

/* Enable / Disable ZLIB extension (default: enabled) */
#define HAVE_ZLIB 1

/* PHP Runtime Configuration */
#define FORCE_CGI_REDIRECT 1
#define PHP_URL_FOPEN 1
#define PHP_SAFE_MODE 0
#define MAGIC_QUOTES 0
#define USE_CONFIG_FILE 1
#define DEFAULT_SHORT_OPEN_TAG "1"
#define ENABLE_PATHINFO_CHECK 1

/* Platform-Specific Configuration. Should not be changed. */
#define PHP_SIGCHILD 0
#define HAVE_LIBBIND 1
#define HAVE_GETSERVBYNAME 1
#define HAVE_GETSERVBYPORT 1
#define HAVE_GETPROTOBYNAME 1
#define HAVE_GETPROTOBYNUMBER 1
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#define HAVE_ERRMSG_H 0
#undef HAVE_ADABAS
#undef HAVE_SOLID
#undef HAVE_LINK
#undef HAVE_SYMLINK
#undef HAVE_USLEEP
#define HAVE_GETCWD 1
#define HAVE_POSIX_READDIR_R 1
#define NEED_ISBLANK 1
#define DISCARD_PATH 0
#undef HAVE_SETITIMER
#undef HAVE_IODBC
#define HAVE_LIBDL 1
#define HAVE_SENDMAIL 1
#define HAVE_GETTIMEOFDAY 1
#define HAVE_PUTENV 1
#define HAVE_LIMITS_H 1
#define HAVE_TZSET 1
#define HAVE_TZNAME 1
#undef HAVE_FLOCK
#define HAVE_ALLOCA 1
#undef HAVE_SYS_TIME_H
#define HAVE_SIGNAL_H 1
#undef HAVE_ST_BLKSIZE
#undef HAVE_ST_BLOCKS
#define HAVE_ST_RDEV 1
#define HAVE_UTIME_NULL 1
#define HAVE_VPRINTF 1
#define STDC_HEADERS 1
#define REGEX 1
#define HSREGEX 1
#define HAVE_GCVT 1
#define HAVE_GETLOGIN 1
#define HAVE_GETTIMEOFDAY 1
#define HAVE_MEMCPY 1
#define HAVE_MEMMOVE 1
#define HAVE_PUTENV 1
#define HAVE_REGCOMP 1
#define HAVE_SETLOCALE 1
#define HAVE_LOCALECONV 1
#define HAVE_LOCALE_H 1
#ifndef HAVE_LIBBIND
#define HAVE_SETVBUF 1
#endif
#define HAVE_SHUTDOWN 1
#define HAVE_SNPRINTF 1
#define HAVE_VSNPRINTF 1
#define HAVE_STRCASECMP 1
#define HAVE_STRDUP 1
#define HAVE_STRERROR 1
#define HAVE_STRSTR 1
#define HAVE_TEMPNAM 1
#define HAVE_UTIME 1
#undef HAVE_DIRENT_H
#define HAVE_ASSERT_H 1
#define HAVE_FCNTL_H 1
#define HAVE_GRP_H 0
#define HAVE_PWD_H 1
#define HAVE_STRING_H 1
#undef HAVE_SYS_FILE_H
#undef HAVE_SYS_SOCKET_H
#undef HAVE_SYS_WAIT_H
#define HAVE_SYSLOG_H 1
#undef HAVE_UNISTD_H
#define HAVE_LIBDL 1
#define HAVE_LIBM 1
#define HAVE_CUSERID 0
#undef HAVE_RINT
#define HAVE_STRFTIME 1
#define SIZEOF_INT 4
#define HAVE_GLOB
#define PHP_SHLIB_SUFFIX "dll"
#define HAVE_SQLDATASOURCES
#define POSIX_MALLOC_THRESHOLD 10

/* Win32 supports strcoll */
#define HAVE_STRCOLL 1

#undef HAVE_ATOF_ACCEPTS_NAN
#undef HAVE_ATOF_ACCEPTS_INF
#define HAVE_HUGE_VAL_NAN 1
