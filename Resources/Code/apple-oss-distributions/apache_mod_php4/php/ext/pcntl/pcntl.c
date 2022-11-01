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

/* $Id: pcntl.c,v 1.28.4.6.2.3 2007/12/31 07:22:50 sebastian Exp $ */

#define PCNTL_DEBUG 0

#if PCNTL_DEBUG
#define DEBUG_OUT printf("DEBUG: ");printf
#define IF_DEBUG(z) z
#else
#define IF_DEBUG(z)
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_pcntl.h"

ZEND_DECLARE_MODULE_GLOBALS(pcntl)

function_entry pcntl_functions[] = {
	PHP_FE(pcntl_fork,			NULL)
	PHP_FE(pcntl_waitpid,		second_arg_force_ref)
	PHP_FE(pcntl_signal,		NULL)
	PHP_FE(pcntl_wifexited,		NULL)
	PHP_FE(pcntl_wifstopped,	NULL)
	PHP_FE(pcntl_wifsignaled,	NULL)
	PHP_FE(pcntl_wexitstatus,	NULL)
	PHP_FE(pcntl_wtermsig,		NULL)
	PHP_FE(pcntl_wstopsig,		NULL)
	PHP_FE(pcntl_exec,			NULL)
	PHP_FE(pcntl_alarm,			NULL)
	{NULL, NULL, NULL}	
};

zend_module_entry pcntl_module_entry = {
	STANDARD_MODULE_HEADER,
	"pcntl",
	pcntl_functions,
	PHP_MINIT(pcntl),
	PHP_MSHUTDOWN(pcntl),
	PHP_RINIT(pcntl),
	PHP_RSHUTDOWN(pcntl),
	PHP_MINFO(pcntl),
	NO_VERSION_YET,
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_PCNTL
ZEND_GET_MODULE(pcntl)
#endif

static void pcntl_signal_handler(int);
static void pcntl_tick_handler();
  
void php_register_signal_constants(INIT_FUNC_ARGS)
{

	/* Wait Constants */
#ifdef WNOHANG
	REGISTER_LONG_CONSTANT("WNOHANG",  (long) WNOHANG, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef WUNTRACED
	REGISTER_LONG_CONSTANT("WUNTRACED",  (long) WUNTRACED, CONST_CS | CONST_PERSISTENT);
#endif

	/* Signal Constants */
	REGISTER_LONG_CONSTANT("SIG_IGN",  (long) SIG_IGN, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIG_DFL",  (long) SIG_DFL, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIG_ERR",  (long) SIG_ERR, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGHUP",   (long) SIGHUP,  CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGINT",   (long) SIGINT,  CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGQUIT",  (long) SIGQUIT, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGILL",   (long) SIGILL,  CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGTRAP",  (long) SIGTRAP, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGABRT",  (long) SIGABRT, CONST_CS | CONST_PERSISTENT);
#ifdef SIGIOT
	REGISTER_LONG_CONSTANT("SIGIOT",   (long) SIGIOT,  CONST_CS | CONST_PERSISTENT);
#endif
	REGISTER_LONG_CONSTANT("SIGBUS",   (long) SIGBUS,  CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGFPE",   (long) SIGFPE,  CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGKILL",  (long) SIGKILL, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGUSR1",  (long) SIGUSR1, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGSEGV",  (long) SIGSEGV, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGUSR2",  (long) SIGUSR2, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGPIPE",  (long) SIGPIPE, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGALRM",  (long) SIGALRM, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGTERM",  (long) SIGTERM, CONST_CS | CONST_PERSISTENT);
#ifdef SIGSTKFLT
	REGISTER_LONG_CONSTANT("SIGSTKFLT",(long) SIGSTKFLT, CONST_CS | CONST_PERSISTENT);
#endif 
#ifdef SIGCLD
	REGISTER_LONG_CONSTANT("SIGCLD",   (long) SIGCLD, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef SIGCHLD
	REGISTER_LONG_CONSTANT("SIGCHLD",  (long) SIGCHLD, CONST_CS | CONST_PERSISTENT);
#endif
	REGISTER_LONG_CONSTANT("SIGCONT",  (long) SIGCONT, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGSTOP",  (long) SIGSTOP, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGTSTP",  (long) SIGTSTP, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGTTIN",  (long) SIGTTIN, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGTTOU",  (long) SIGTTOU, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGURG",   (long) SIGURG , CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGXCPU",  (long) SIGXCPU, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGXFSZ",  (long) SIGXFSZ, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGVTALRM",(long) SIGVTALRM, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGPROF",  (long) SIGPROF, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGWINCH", (long) SIGWINCH, CONST_CS | CONST_PERSISTENT);
#ifdef SIGPOLL
	REGISTER_LONG_CONSTANT("SIGPOLL",  (long) SIGPOLL, CONST_CS | CONST_PERSISTENT);
#endif
	REGISTER_LONG_CONSTANT("SIGIO",    (long) SIGIO, CONST_CS | CONST_PERSISTENT);
#ifdef SIGPWR
	REGISTER_LONG_CONSTANT("SIGPWR",   (long) SIGPWR, CONST_CS | CONST_PERSISTENT);
#endif
	REGISTER_LONG_CONSTANT("SIGSYS",   (long) SIGSYS, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGBABY",  (long) SIGSYS, CONST_CS | CONST_PERSISTENT);
}

static void php_pcntl_init_globals(zend_pcntl_globals *pcntl_globals)
{ 
	memset(pcntl_globals, 0, sizeof(*pcntl_globals));
}

PHP_RINIT_FUNCTION(pcntl)
{
	zend_hash_init(&PCNTL_G(php_signal_table), 16, NULL, ZVAL_PTR_DTOR, 0);
	PCNTL_G(head) = PCNTL_G(tail) = PCNTL_G(spares) = NULL;
	return SUCCESS;
}

PHP_MINIT_FUNCTION(pcntl)
{
	php_register_signal_constants(INIT_FUNC_ARGS_PASSTHRU);
	ZEND_INIT_MODULE_GLOBALS(pcntl, php_pcntl_init_globals, NULL);
	php_add_tick_function(pcntl_tick_handler);

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(pcntl)
{
	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(pcntl)
{
	struct php_pcntl_pending_signal *sig;

	/* FIXME: if a signal is delivered after this point, things will go pear shaped;
	 * need to remove signal handlers */
	zend_hash_destroy(&PCNTL_G(php_signal_table));
	while (PCNTL_G(head)) {
		sig = PCNTL_G(head);
		PCNTL_G(head) = sig->next;
		efree(sig);
	}
	while (PCNTL_G(spares)) {
		sig = PCNTL_G(spares);
		PCNTL_G(spares) = sig->next;
		efree(sig);
	}
	return SUCCESS;
}

PHP_MINFO_FUNCTION(pcntl)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "pcntl support", "enabled");
	php_info_print_table_end();
}

/* {{{ proto int pcntl_fork(void)
   Forks the currently running process following the same behavior as the UNIX fork() system call*/
PHP_FUNCTION(pcntl_fork)
{
	pid_t id;

	id = fork();
	if (id == -1) {
		php_error(E_ERROR, "Error %d in %s", errno, get_active_function_name(TSRMLS_C));
	}
	
	RETURN_LONG((long) id);
}
/* }}} */

/* {{{ proto int pcntl_alarm(int seconds)
   Set an alarm clock for delivery of a signal*/
PHP_FUNCTION(pcntl_alarm)
{
	long seconds;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &seconds) == FAILURE)
		return;
	
	RETURN_LONG ((long) alarm(seconds));
}
/* }}} */

/* {{{ proto int pcntl_waitpid(long pid, long status, long options)
   Waits on or returns the status of a forked child as defined by the waitpid() system call */
PHP_FUNCTION(pcntl_waitpid)
{
	long pid, options = 0;
	zval *z_status = NULL;
	int status;
	pid_t child_id;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "lz|l", &pid, &z_status, &options) == FAILURE)
		return;
	
	convert_to_long_ex(&z_status);

	status = Z_LVAL_P(z_status);

	child_id = waitpid((pid_t) pid, &status, options);

	Z_LVAL_P(z_status) = status;

	RETURN_LONG((long) child_id);
}
/* }}} */

/* {{{ proto bool pcntl_wifexited(long status) 
   Returns true if the child status code represents a successful exit */
PHP_FUNCTION(pcntl_wifexited)
{
#ifdef WIFEXITED
	zval **status;
	int status_word;
	
	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(ZEND_NUM_ARGS(), &status) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	
	status_word = (int) Z_LVAL_PP(status);
	
	if (WIFEXITED(status_word)) RETURN_TRUE;
#endif
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto bool pcntl_wifstopped(long status) 
   Returns true if the child status code represents a stopped process (WUNTRACED must have been used with waitpid) */
PHP_FUNCTION(pcntl_wifstopped)
{
#ifdef WIFSTOPPED
	zval **status;
	int status_word;
	
	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(ZEND_NUM_ARGS(), &status) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	
	status_word = (int) Z_LVAL_PP(status);
	
	if (WIFSTOPPED(status_word)) RETURN_TRUE;
#endif
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto bool pcntl_wifsignaled(long status) 
   Returns true if the child status code represents a process that was terminated due to a signal */
PHP_FUNCTION(pcntl_wifsignaled)
{
#ifdef WIFSIGNALED
	zval **status;
	int status_word;
	
	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(ZEND_NUM_ARGS(), &status) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	
	status_word = (int) Z_LVAL_PP(status);
	
	if (WIFSIGNALED(status_word)) RETURN_TRUE;
#endif
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto int pcntl_wexitstatus(long status) 
   Returns the status code of a child's exit */
PHP_FUNCTION(pcntl_wexitstatus)
{
#ifdef WEXITSTATUS
	zval **status;
	int status_word;
	
	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(ZEND_NUM_ARGS(), &status) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	
	status_word = (int) Z_LVAL_PP(status);

	/* WEXITSTATUS only returns 8 bits so we *MUST* cast this to signed char
	   if you want to have valid negative exit codes */
	RETURN_LONG((signed char) WEXITSTATUS(status_word));
#else
	RETURN_FALSE;
#endif
}
/* }}} */

/* {{{ proto int pcntl_wtermsig(long status) 
   Returns the number of the signal that terminated the process who's status code is passed  */
PHP_FUNCTION(pcntl_wtermsig)
{
#ifdef WTERMSIG
	zval **status;
	int status_word;
	
	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(ZEND_NUM_ARGS(), &status) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	
	status_word = (int) Z_LVAL_PP(status);
	
	RETURN_LONG(WTERMSIG(status_word));
#else
	RETURN_FALSE;
#endif
}
/* }}} */

/* {{{ proto int pcntl_wstopsig(long status) 
   Returns the number of the signal that caused the process to stop who's status code is passed */
PHP_FUNCTION(pcntl_wstopsig)
{
#ifdef WSTOPSIG
	zval **status;
	int status_word;
   
	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(ZEND_NUM_ARGS(), &status) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
   
	status_word = (int) Z_LVAL_PP(status);

 	RETURN_LONG(WSTOPSIG(status_word));
#else
	RETURN_FALSE;
#endif
}
/* }}} */

/* {{{ proto bool pcntl_exec(string path [, array args [, array envs]])
   Executes specified program in current process space as defined by exec(2) */
PHP_FUNCTION(pcntl_exec)
{
	zval *args, *envs;
	zval **element;
	HashTable *args_hash, *envs_hash;
	int argc = 0, argi = 0;
	int envc = 0, envi = 0;
	int return_val = 0;
	char **argv = NULL, **envp = NULL;
	char **current_arg, **pair;
	int pair_length;
	char *key;
	int key_length;
	char *path;
	int path_len;
	long key_num;
		
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|aa", &path, &path_len, &args, &envs) == FAILURE) {
		return;
	}
	
	if (ZEND_NUM_ARGS() > 1) {
		/* Build argumnent list */
		args_hash = HASH_OF(args);
		argc = zend_hash_num_elements(args_hash);
		
		argv = safe_emalloc((argc + 2), sizeof(char *), 0);
		*argv = path;
		for ( zend_hash_internal_pointer_reset(args_hash), current_arg = argv+1; 
			(argi < argc && (zend_hash_get_current_data(args_hash, (void **) &element) == SUCCESS));
			(argi++, current_arg++, zend_hash_move_forward(args_hash)) ) {

			convert_to_string_ex(element);
			*current_arg = Z_STRVAL_PP(element);
		}
		*(current_arg) = NULL;
	} else {
		argv = emalloc(2 * sizeof(char *));
		*argv = path;
		*(argv+1) = NULL;
	}

	if ( ZEND_NUM_ARGS() == 3 ) {
		/* Build environment pair list */
		envs_hash = HASH_OF(envs);
		envc = zend_hash_num_elements(envs_hash);
		
		envp = safe_emalloc((envc + 1), sizeof(char *), 0);
		for ( zend_hash_internal_pointer_reset(envs_hash), pair = envp; 
			(envi < envc && (zend_hash_get_current_data(envs_hash, (void **) &element) == SUCCESS));
			(envi++, pair++, zend_hash_move_forward(envs_hash)) ) {
			switch (return_val = zend_hash_get_current_key_ex(envs_hash, &key, &key_length, &key_num, 0, NULL)) {
				case HASH_KEY_IS_LONG:
					key = emalloc(101);
					snprintf(key, 100, "%ld", key_num);
					key_length = strlen(key);
					break;
				case HASH_KEY_NON_EXISTANT:
					pair--;
					continue;
			}

			convert_to_string_ex(element);

			/* Length of element + equal sign + length of key + null */ 
			pair_length = Z_STRLEN_PP(element) + key_length + 2;
			*pair = emalloc(pair_length);
			strlcpy(*pair, key, key_length); 
			strlcat(*pair, "=", pair_length);
			strlcat(*pair, Z_STRVAL_PP(element), pair_length);
			
			/* Cleanup */
			if (return_val == HASH_KEY_IS_LONG) efree(key);
		}
		*(pair) = NULL;
	}
	
	if (execve(path, argv, envp) == -1) {
		php_error(E_WARNING, "Error has occured in %s: (errno %d) %s",
				  get_active_function_name(TSRMLS_C), errno, strerror(errno));
	}
	
	/* Cleanup */
	if (envp != NULL) {
		for (pair = envp; *pair != NULL; pair++) efree(*pair);
		efree(envp);
	}

	efree(argv);
	
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto bool pcntl_signal(long signo, mixed handle, [bool restart_syscalls])
   Assigns a system signal handler to a PHP function */
PHP_FUNCTION(pcntl_signal)
{
	zval *handle, **dest_handle = NULL;
	char *func_name;
	long signo;
	zend_bool restart_syscalls = 1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "lz|b", &signo, &handle, &restart_syscalls) == FAILURE) {
		return;
	}

	if (!PCNTL_G(spares)) {
		/* since calling malloc() from within a signal handler is not portable,
		 * pre-allocate a few records for recording signals */
		int i;
		for (i = 0; i < 32; i++) {
			struct php_pcntl_pending_signal *psig;

			psig = emalloc(sizeof(*psig));
			psig->next = PCNTL_G(spares);
			PCNTL_G(spares) = psig;
		}
	}

	/* Special long value case for SIG_DFL and SIG_IGN */
	if (Z_TYPE_P(handle)==IS_LONG) {
		if (Z_LVAL_P(handle)!= (long) SIG_DFL && Z_LVAL_P(handle) != (long) SIG_IGN) {
			php_error(E_WARNING, "Invalid value for handle argument specifEied in %s", get_active_function_name(TSRMLS_C));
		}
		if (php_signal(signo, (Sigfunc *) Z_LVAL_P(handle), (int) restart_syscalls) == SIG_ERR) {
			php_error(E_WARNING, "Error assigning signal in %s", get_active_function_name(TSRMLS_C));
			RETURN_FALSE;
		}
		RETURN_TRUE;
	}
	
	if (!zend_is_callable(handle, 0, &func_name)) {
		php_error(E_WARNING, "%s: %s is not a callable function name error", get_active_function_name(TSRMLS_C), func_name);
		efree(func_name);
		RETURN_FALSE;
	}
	efree(func_name);
	
	/* Add the function name to our signal table */
	zend_hash_index_update(&PCNTL_G(php_signal_table), signo, (void **) &handle, sizeof(zval *), (void **) &dest_handle);
	if (dest_handle) zval_add_ref(dest_handle);
	
	if (php_signal(signo, pcntl_signal_handler, (int) restart_syscalls) == SIG_ERR) {
		php_error(E_WARNING, "Error assigning signal in %s", get_active_function_name(TSRMLS_C));
		RETURN_FALSE;
	}
	RETURN_TRUE;
}
/* }}} */

/* Our custom signal handler that calls the appropriate php_function */
static void pcntl_signal_handler(int signo)
{
	struct php_pcntl_pending_signal *psig;
	TSRMLS_FETCH();
	
	psig = PCNTL_G(spares);
	if (!psig) {
		/* oops, too many signals for us to track, so we'll forget about this one */
		return;
	}
	PCNTL_G(spares) = psig->next;

	psig->signo = signo;
	psig->next = NULL;

	/* the head check is important, as the tick handler cannot atomically clear both
	 * the head and tail */
	if (PCNTL_G(head) && PCNTL_G(tail)) {
		PCNTL_G(tail)->next = psig;
	} else {
		PCNTL_G(head) = psig;
	}
	PCNTL_G(tail) = psig;
}

void pcntl_tick_handler()
{
	zval *param, **handle, *retval;
	struct php_pcntl_pending_signal *queue, *next;
	TSRMLS_FETCH();

	/* Bail if the queue is empty or if we are already playing the queue*/
	if (! PCNTL_G(head) || PCNTL_G(processing_signal_queue))
		return;

	/* Prevent reentrant handler calls */
	PCNTL_G(processing_signal_queue) = 1;

	queue = PCNTL_G(head);
	PCNTL_G(head) = NULL; /* simple stores are atomic */
	
	/* Allocate */
	MAKE_STD_ZVAL(param);
	MAKE_STD_ZVAL(retval);

	while (queue) {
		if (zend_hash_index_find(&PCNTL_G(php_signal_table), queue->signo, (void **) &handle)==SUCCESS) {
			ZVAL_LONG(param, queue->signo);

			/* Call php signal handler - Note that we do not report errors, and we ignore the return value */
			/* FIXME: this is probably broken when multiple signals are handled in this while loop (retval) */
			call_user_function(EG(function_table), NULL, *handle, retval, 1, &param TSRMLS_CC);
		}

		next = queue->next;
		queue->next = PCNTL_G(spares);
		PCNTL_G(spares) = queue;
		queue = next;
	}

	/* Re-enable queue */
	PCNTL_G(processing_signal_queue) = 0;

	/* Clean up */
	efree(param);
	efree(retval);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 */
