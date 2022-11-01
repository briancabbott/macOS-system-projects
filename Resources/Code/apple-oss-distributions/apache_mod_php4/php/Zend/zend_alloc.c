/*
   +----------------------------------------------------------------------+
   | Zend Engine                                                          |
   +----------------------------------------------------------------------+
   | Copyright (c) 1998-2003 Zend Technologies Ltd. (http://www.zend.com) |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.00 of the Zend license,     |
   | that is bundled with this package in the file LICENSE, and is        | 
   | available through the world-wide-web at the following url:           |
   | http://www.zend.com/license/2_00.txt.                                |
   | If you did not receive a copy of the Zend license and are unable to  |
   | obtain it through the world-wide-web, please send a note to          |
   | license@zend.com so we can mail you a copy immediately.              |
   +----------------------------------------------------------------------+
   | Authors: Andi Gutmans <andi@zend.com>                                |
   |          Zeev Suraski <zeev@zend.com>                                |
   +----------------------------------------------------------------------+
*/


#include <stdlib.h>

#include "zend.h"
#include "zend_alloc.h"
#include "zend_globals.h"
#include "zend_fast_cache.h"
#ifdef HAVE_SIGNAL_H
# include <signal.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifndef ZTS
ZEND_API zend_alloc_globals alloc_globals;
#endif


#define ZEND_DISABLE_MEMORY_CACHE 0


#ifdef ZEND_WIN32
#define ZEND_DO_MALLOC(size)		(AG(memory_heap) ? HeapAlloc(AG(memory_heap), HEAP_NO_SERIALIZE, size) : malloc(size))
#define ZEND_DO_FREE(ptr)			(AG(memory_heap) ? HeapFree(AG(memory_heap), HEAP_NO_SERIALIZE, ptr) : free(ptr))
#define ZEND_DO_REALLOC(ptr, size)	(AG(memory_heap) ? HeapReAlloc(AG(memory_heap), HEAP_NO_SERIALIZE, ptr, size) : realloc(ptr, size))
#else
#define ZEND_DO_MALLOC(size)		malloc(size)
#define ZEND_DO_FREE(ptr)			free(ptr)
#define ZEND_DO_REALLOC(ptr, size)	realloc(ptr, size)
#endif

#if ZEND_DEBUG
# define END_MAGIC_SIZE sizeof(long)
static long mem_block_end_magic = MEM_BLOCK_END_MAGIC;
#else
# define END_MAGIC_SIZE 0
#endif


# if MEMORY_LIMIT
#  if ZEND_DEBUG
#define CHECK_MEMORY_LIMIT(s, rs) _CHECK_MEMORY_LIMIT(s, rs, __zend_filename, __zend_lineno)
#  else
#define CHECK_MEMORY_LIMIT(s, rs)	_CHECK_MEMORY_LIMIT(s, rs, NULL, 0)
#  endif

#define _CHECK_MEMORY_LIMIT(s, rs, file, lineno) { if ((ssize_t)(rs) > (ssize_t)(INT_MAX - AG(allocated_memory))) { \
									if (file) { \
										fprintf(stderr, "Integer overflow in memory_limit check detected at %s:%d\n", file, lineno); \
									} else { \
										fprintf(stderr, "Integer overflow in memory_limit check detected\n"); \
									} \
									exit(1); \
								} \
								AG(allocated_memory) += rs;\
								if (AG(memory_limit)<AG(allocated_memory)) {\
									int php_mem_limit = AG(memory_limit); \
									AG(allocated_memory) -= rs; \
									if (EG(in_execution) && AG(memory_limit)+1048576 > AG(allocated_memory)) { \
										AG(memory_limit) = AG(allocated_memory) + 1048576; \
										if (file) { \
											zend_error(E_ERROR,"Allowed memory size of %d bytes exhausted at %s:%d (tried to allocate %d bytes)", php_mem_limit, file, lineno, s); \
										} else { \
											zend_error(E_ERROR,"Allowed memory size of %d bytes exhausted (tried to allocate %d bytes)", php_mem_limit, s); \
										} \
									} else { \
										if (file) { \
											fprintf(stderr, "Allowed memory size of %d bytes exhausted at %s:%d (tried to allocate %d bytes)\n", php_mem_limit, file, lineno, s); \
										} else { \
											fprintf(stderr, "Allowed memory size of %d bytes exhausted (tried to allocate %d bytes)\n", php_mem_limit, s); \
										} \
										exit(1); \
									} \
								} \
							}
# endif

#ifndef CHECK_MEMORY_LIMIT
#define CHECK_MEMORY_LIMIT(s, rs)
#endif


#define REMOVE_POINTER_FROM_LIST(p)				\
	if (p==AG(head)) {							\
		AG(head) = p->pNext;					\
	} else {									\
		p->pLast->pNext = p->pNext;				\
	}											\
	if (p->pNext) {								\
		p->pNext->pLast = p->pLast;				\
	}

#define ADD_POINTER_TO_LIST(p)		\
	p->pNext = AG(head);			\
	if (AG(head)) {					\
		AG(head)->pLast = p;		\
	}								\
	AG(head) = p;					\
	p->pLast = (zend_mem_header *) NULL;

#define DECLARE_CACHE_VARS()	\
	size_t real_size;		\
	unsigned int cache_index

#define REAL_SIZE(size) ((size+7) & ~0x7)

#define CALCULATE_REAL_SIZE_AND_CACHE_INDEX(size)	\
	real_size = REAL_SIZE(size);				\
	cache_index = real_size >> 3;

#define SIZE real_size

#define CACHE_INDEX cache_index

ZEND_API void *_emalloc(size_t size ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC)
{
	zend_mem_header *p = NULL;
	DECLARE_CACHE_VARS();
	TSRMLS_FETCH();

	CALCULATE_REAL_SIZE_AND_CACHE_INDEX(size);

	if (size > INT_MAX || SIZE < size) {
		goto emalloc_error;
	}

	if (!ZEND_DISABLE_MEMORY_CACHE && (CACHE_INDEX < MAX_CACHED_MEMORY) && (AG(cache_count)[CACHE_INDEX] > 0)) {
		p = AG(cache)[CACHE_INDEX][--AG(cache_count)[CACHE_INDEX]];
#if ZEND_DEBUG
		p->filename = __zend_filename;
		p->lineno = __zend_lineno;
		p->orig_filename = __zend_orig_filename;
		p->orig_lineno = __zend_orig_lineno;
		p->magic = MEM_BLOCK_START_MAGIC;
		p->reported = 0;
		/* Setting the thread id should not be necessary, because we fetched this block
		 * from this thread's cache
		 */
		AG(cache_stats)[CACHE_INDEX][1]++;
		memcpy((((char *) p) + sizeof(zend_mem_header) + MEM_HEADER_PADDING + size), &mem_block_end_magic, sizeof(long));
#endif
		p->cached = 0;
		p->size = size;
		return (void *)((char *)p + sizeof(zend_mem_header) + MEM_HEADER_PADDING);
	} else {
#if ZEND_DEBUG
		if (CACHE_INDEX<MAX_CACHED_MEMORY) {
			AG(cache_stats)[CACHE_INDEX][0]++;
		}
#endif
#if MEMORY_LIMIT
		CHECK_MEMORY_LIMIT(size, SIZE);
		if (AG(allocated_memory) > AG(allocated_memory_peak)) {
			AG(allocated_memory_peak) = AG(allocated_memory);
		}
#endif
		p  = (zend_mem_header *) ZEND_DO_MALLOC(sizeof(zend_mem_header) + MEM_HEADER_PADDING + SIZE + END_MAGIC_SIZE);
	}

emalloc_error:

	HANDLE_BLOCK_INTERRUPTIONS();

	if (!p) {
		fprintf(stderr,"FATAL:  emalloc():  Unable to allocate %ld bytes\n", (long) size);
#if ZEND_DEBUG && defined(HAVE_KILL) && defined(HAVE_GETPID)
		kill(getpid(), SIGSEGV);
#else
		exit(1);
#endif
		HANDLE_UNBLOCK_INTERRUPTIONS();
		return (void *)p;
	}
	p->cached = 0;
	ADD_POINTER_TO_LIST(p);
	p->size = size; /* Save real size for correct cache output */
#if ZEND_DEBUG
	p->filename = __zend_filename;
	p->lineno = __zend_lineno;
	p->orig_filename = __zend_orig_filename;
	p->orig_lineno = __zend_orig_lineno;
	p->magic = MEM_BLOCK_START_MAGIC;
	p->reported = 0;
# ifdef ZTS
	p->thread_id = tsrm_thread_id();
# endif
	memcpy((((char *) p) + sizeof(zend_mem_header) + MEM_HEADER_PADDING + size), &mem_block_end_magic, sizeof(long));
#endif

	HANDLE_UNBLOCK_INTERRUPTIONS();
	return (void *)((char *)p + sizeof(zend_mem_header) + MEM_HEADER_PADDING);
}

#include "zend_multiply.h"

ZEND_API void *_safe_emalloc(size_t nmemb, size_t size, size_t offset ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC)
{

	if (nmemb < LONG_MAX 
			&& size < LONG_MAX 
			&& offset < LONG_MAX
			&& nmemb >= 0 
			&& size >= 0 
			&& offset >= 0) {
		long lval;
		double dval;
		int use_dval;

		ZEND_SIGNED_MULTIPLY_LONG(nmemb, size, lval, dval, use_dval);

		if (!use_dval
			&& lval < LONG_MAX - offset) {
			return emalloc_rel(lval + offset);
		}
	}

	zend_error(E_ERROR, "Possible integer overflow in memory allocation (%ld * %ld + %ld)", nmemb, size, offset);
	return 0;
}

ZEND_API void _efree(void *ptr ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC)
{
	zend_mem_header *p = (zend_mem_header *) ((char *)ptr - sizeof(zend_mem_header) - MEM_HEADER_PADDING);
	DECLARE_CACHE_VARS();
	TSRMLS_FETCH();

#if defined(ZTS) && TSRM_DEBUG
	if (p->thread_id != tsrm_thread_id()) {
		tsrm_error(TSRM_ERROR_LEVEL_ERROR, "Memory block allocated at %s:(%d) on thread %x freed at %s:(%d) on thread %x, ignoring",
			p->filename, p->lineno, p->thread_id,
			__zend_filename, __zend_lineno, tsrm_thread_id());
		return;
	}
#endif

	CALCULATE_REAL_SIZE_AND_CACHE_INDEX(p->size);
#if ZEND_DEBUG
	if (!_mem_block_check(ptr, 1 ZEND_FILE_LINE_RELAY_CC ZEND_FILE_LINE_ORIG_RELAY_CC)) {
		return;
	}
	memset(ptr, 0x5a, p->size);
#endif

	if (!ZEND_DISABLE_MEMORY_CACHE 
		&& (CACHE_INDEX < MAX_CACHED_MEMORY) && (AG(cache_count)[CACHE_INDEX] < MAX_CACHED_ENTRIES)) {
		AG(cache)[CACHE_INDEX][AG(cache_count)[CACHE_INDEX]++] = p;
		p->cached = 1;
#if ZEND_DEBUG
		p->magic = MEM_BLOCK_CACHED_MAGIC;
#endif
		return;
	}
	HANDLE_BLOCK_INTERRUPTIONS();
	REMOVE_POINTER_FROM_LIST(p);

#if MEMORY_LIMIT
	AG(allocated_memory) -= SIZE;
#endif
	
	ZEND_DO_FREE(p);
	HANDLE_UNBLOCK_INTERRUPTIONS();
}


ZEND_API void *_ecalloc(size_t nmemb, size_t size ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC)
{
	char *p;
	size_t _size = nmemb * size;
    
	if (nmemb && (_size/nmemb!=size)) {
		fprintf(stderr,"FATAL:  ecalloc():  Unable to allocate %ld * %ld bytes\n", (long) nmemb, (long) size);
#if ZEND_DEBUG && HAVE_KILL && HAVE_GETPID
		kill(getpid(), SIGSEGV);
#else
		exit(1);
#endif
	}
	
	p = (char *) _emalloc(_size ZEND_FILE_LINE_RELAY_CC ZEND_FILE_LINE_ORIG_RELAY_CC);
	if (p) {
		memset(p, 0, _size);
	}
	
	return ((void *)p);
}


ZEND_API void *_erealloc(void *ptr, size_t size, int allow_failure ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC)
{
	zend_mem_header *p;
	zend_mem_header *orig;
	DECLARE_CACHE_VARS();
	TSRMLS_FETCH();

	if (!ptr) {
		return _emalloc(size ZEND_FILE_LINE_RELAY_CC ZEND_FILE_LINE_ORIG_RELAY_CC);
	}

	p = orig = (zend_mem_header *) ((char *)ptr-sizeof(zend_mem_header)-MEM_HEADER_PADDING);

#if defined(ZTS) && TSRM_DEBUG
	if (p->thread_id != tsrm_thread_id()) {
		void *new_p;

		tsrm_error(TSRM_ERROR_LEVEL_ERROR, "Memory block allocated at %s:(%d) on thread %x reallocated at %s:(%d) on thread %x, duplicating",
			p->filename, p->lineno, p->thread_id,
			__zend_filename, __zend_lineno, tsrm_thread_id());
		new_p = _emalloc(size ZEND_FILE_LINE_RELAY_CC ZEND_FILE_LINE_ORIG_RELAY_CC);
		memcpy(new_p, ptr, p->size);
		return new_p;
	}
#endif

	CALCULATE_REAL_SIZE_AND_CACHE_INDEX(size);

	HANDLE_BLOCK_INTERRUPTIONS();

	if (size > INT_MAX || SIZE < size) {
		REMOVE_POINTER_FROM_LIST(p);
		p = NULL;
		goto erealloc_error;
	}

#if MEMORY_LIMIT
	CHECK_MEMORY_LIMIT(size - p->size, SIZE - REAL_SIZE(p->size));
	if (AG(allocated_memory) > AG(allocated_memory_peak)) {
		AG(allocated_memory_peak) = AG(allocated_memory);
	}
#endif
	REMOVE_POINTER_FROM_LIST(p);
	p = (zend_mem_header *) ZEND_DO_REALLOC(p, sizeof(zend_mem_header)+MEM_HEADER_PADDING+SIZE+END_MAGIC_SIZE);
erealloc_error:
	if (!p) {
		if (!allow_failure) {
			fprintf(stderr,"FATAL:  erealloc():  Unable to allocate %ld bytes\n", (long) size);
#if ZEND_DEBUG && HAVE_KILL && HAVE_GETPID
			kill(getpid(), SIGSEGV);
#else
			exit(1);
#endif
		}
		ADD_POINTER_TO_LIST(orig);
		HANDLE_UNBLOCK_INTERRUPTIONS();
		return (void *)NULL;
	}
	ADD_POINTER_TO_LIST(p);
#if ZEND_DEBUG
	p->filename = __zend_filename;
	p->lineno = __zend_lineno;
	p->magic = MEM_BLOCK_START_MAGIC;
	memcpy((((char *) p) + sizeof(zend_mem_header) + MEM_HEADER_PADDING + size), &mem_block_end_magic, sizeof(long));
#endif	

	p->size = size;

	HANDLE_UNBLOCK_INTERRUPTIONS();
	return (void *)((char *)p+sizeof(zend_mem_header)+MEM_HEADER_PADDING);
}


ZEND_API char *_estrdup(const char *s ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC)
{
	int length;
	char *p;

	length = strlen(s)+1;
	HANDLE_BLOCK_INTERRUPTIONS();
	p = (char *) _emalloc(length ZEND_FILE_LINE_RELAY_CC ZEND_FILE_LINE_ORIG_RELAY_CC);
	if (!p) {
		HANDLE_UNBLOCK_INTERRUPTIONS();
		return (char *)NULL;
	}
	HANDLE_UNBLOCK_INTERRUPTIONS();
	memcpy(p, s, length);
	return p;
}

ZEND_API char *_estrndup(const char *s, uint length ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC)
{
	char *p;

	HANDLE_BLOCK_INTERRUPTIONS();
	p = (char *) _emalloc(length+1 ZEND_FILE_LINE_RELAY_CC ZEND_FILE_LINE_ORIG_RELAY_CC);
	if (!p) {
		HANDLE_UNBLOCK_INTERRUPTIONS();
		return (char *)NULL;
	}
	HANDLE_UNBLOCK_INTERRUPTIONS();
	memcpy(p, s, length);
	p[length] = 0;
	return p;
}


ZEND_API char *zend_strndup(const char *s, uint length)
{
	char *p;

	p = (char *) malloc(length+1);
	if (!p) {
		return (char *)NULL;
	}
	if (length) {
		memcpy(p, s, length);
	}
	p[length] = 0;
	return p;
}


ZEND_API int zend_set_memory_limit(unsigned int memory_limit)
{
#if MEMORY_LIMIT
	TSRMLS_FETCH();

	AG(memory_limit) = memory_limit;
	return SUCCESS;
#else
	return FAILURE;
#endif
}


ZEND_API void start_memory_manager(TSRMLS_D)
{
	AG(head) = NULL;
	
#if MEMORY_LIMIT
	AG(memory_limit) = 1<<30;		/* ridiculous limit, effectively no limit */
	AG(allocated_memory) = 0;
	AG(memory_exhausted) = 0;
	AG(allocated_memory_peak) = 0;
#endif
#if ZEND_ENABLE_FAST_CACHE
	memset(AG(fast_cache_list_head), 0, sizeof(AG(fast_cache_list_head)));
#endif
#if !ZEND_DISABLE_MEMORY_CACHE
	memset(AG(cache_count), 0, sizeof(AG(cache_count)));
#endif

#ifdef ZEND_WIN32
	AG(memory_heap) = HeapCreate(HEAP_NO_SERIALIZE, 256*1024, 0);
#endif

#if ZEND_DEBUG
	memset(AG(cache_stats), 0, sizeof(AG(cache_stats)));
	memset(AG(fast_cache_stats), 0, sizeof(AG(fast_cache_stats)));
#endif
}


ZEND_API void shutdown_memory_manager(int silent, int clean_cache TSRMLS_DC)
{
	zend_mem_header *p, *t;

#if ZEND_DEBUG
	int had_leaks = 0;
#endif

#if defined(ZEND_WIN32) && !ZEND_DEBUG
	if (clean_cache && AG(memory_heap)) {
		HeapDestroy(AG(memory_heap));
		return;
	}
#endif

#if ZEND_ENABLE_FAST_CACHE
	zend_fast_cache_list_entry *fast_cache_list_entry, *next_fast_cache_list_entry;
	unsigned int fci;

	for (fci=0; fci<MAX_FAST_CACHE_TYPES; fci++) {
		fast_cache_list_entry = AG(fast_cache_list_head)[fci];
		while (fast_cache_list_entry) {
			next_fast_cache_list_entry = fast_cache_list_entry->next;
			efree(fast_cache_list_entry);
			fast_cache_list_entry = next_fast_cache_list_entry;
		}
		AG(fast_cache_list_head)[fci] = NULL;
	}
#endif /* ZEND_ENABLE_FAST_CACHE */

#if !ZEND_DISABLE_MEMORY_CACHE
	if (1 || clean_cache) {
		unsigned int i, j;
		zend_mem_header *ptr;

		for (i=0; i<MAX_CACHED_MEMORY; i++) {
			for (j=0; j<AG(cache_count)[i]; j++) {
				ptr = (zend_mem_header *) AG(cache)[i][j];
#if MEMORY_LIMIT
				AG(allocated_memory) -= REAL_SIZE(ptr->size);
#endif
				REMOVE_POINTER_FROM_LIST(ptr);
				ZEND_DO_FREE(ptr);
			}
			AG(cache_count)[i] = 0;
		}
	}
#endif /* !ZEND_DISABLE_MEMORY_CACHE */

	p = AG(head);
	t = AG(head);
	while (t) {
		if (!t->cached) {
#if ZEND_DEBUG
			if (!t->cached && !t->reported) {
				zend_mem_header *iterator;
				int total_leak=0, total_leak_count=0;

				had_leaks = 1;
				if (!silent) {
					zend_message_dispatcher(ZMSG_MEMORY_LEAK_DETECTED, t);
				}
				t->reported = 1;
				for (iterator=t->pNext; iterator; iterator=iterator->pNext) {
					if (!iterator->cached
						&& iterator->filename==t->filename
						&& iterator->lineno==t->lineno) {
						total_leak += iterator->size;
						total_leak_count++;
						iterator->reported = 1;
					}
				}
				if (!silent && total_leak_count>0) {
					zend_message_dispatcher(ZMSG_MEMORY_LEAK_REPEATED, (void *) (long) (total_leak_count));
				}
			}
#endif
#if MEMORY_LIMIT
			AG(allocated_memory) -= REAL_SIZE(t->size);
#endif
			p = t->pNext;
			REMOVE_POINTER_FROM_LIST(t);
			ZEND_DO_FREE(t);
			t = p;
		} else {
			t = t->pNext;
		}
	}

#if MEMORY_LIMIT
	AG(memory_exhausted)=0;
	AG(allocated_memory_peak) = 0;
#endif


#if (ZEND_DEBUG)
	do {
		zval display_memory_cache_stats;
		int i, j;

		if (clean_cache) {
			/* we're shutting down completely, don't even touch the INI subsystem */
			break;
		}
		if (zend_get_configuration_directive("display_memory_cache_stats", sizeof("display_memory_cache_stats"), &display_memory_cache_stats)==FAILURE) {
			break;
		}
		if (!atoi(display_memory_cache_stats.value.str.val)) {
			break;
		}
		fprintf(stderr, "Memory cache statistics\n"
						"-----------------------\n\n"
						"[zval, %2ld]\t\t%d / %d (%.2f%%)\n"
						"[hash, %2ld]\t\t%d / %d (%.2f%%)\n",
						(long) sizeof(zval),
						AG(fast_cache_stats)[ZVAL_CACHE_LIST][1], AG(fast_cache_stats)[ZVAL_CACHE_LIST][0]+AG(fast_cache_stats)[ZVAL_CACHE_LIST][1],
						((double) AG(fast_cache_stats)[ZVAL_CACHE_LIST][1] / (AG(fast_cache_stats)[ZVAL_CACHE_LIST][0]+AG(fast_cache_stats)[ZVAL_CACHE_LIST][1]))*100,
						(long) sizeof(HashTable),
						AG(fast_cache_stats)[HASHTABLE_CACHE_LIST][1], AG(fast_cache_stats)[HASHTABLE_CACHE_LIST][0]+AG(fast_cache_stats)[HASHTABLE_CACHE_LIST][1],
						((double) AG(fast_cache_stats)[HASHTABLE_CACHE_LIST][1] / (AG(fast_cache_stats)[HASHTABLE_CACHE_LIST][0]+AG(fast_cache_stats)[HASHTABLE_CACHE_LIST][1]))*100);


		for (i=0; i<MAX_CACHED_MEMORY; i+=2) {
			fprintf(stderr, "[%2d, %2d]\t\t", i, i+1);
			for (j=0; j<2; j++) {
				fprintf(stderr, "%d / %d (%.2f%%)\t\t",
						AG(cache_stats)[i+j][1], AG(cache_stats)[i+j][0]+AG(cache_stats)[i+j][1],
						((double) AG(cache_stats)[i+j][1] / (AG(cache_stats)[i+j][0]+AG(cache_stats)[i+j][1]))*100);
			}
			fprintf(stderr, "\n");
		}
					
	} while (0);
#endif

#if defined(ZEND_WIN32) && ZEND_DEBUG
	if (clean_cache && AG(memory_heap)) {
		HeapDestroy(AG(memory_heap));
		return;
	}
#endif
}


#if ZEND_DEBUG
void zend_debug_alloc_output(char *format, ...)
{
	char output_buf[256];
	va_list args;

	va_start(args, format);
	vsprintf(output_buf, format, args);
	va_end(args);

#ifdef ZEND_WIN32
	OutputDebugString(output_buf);
#else
	fprintf(stderr, "%s", output_buf);
#endif
}


ZEND_API int _mem_block_check(void *ptr, int silent ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC)
{
	zend_mem_header *p = (zend_mem_header *) ((char *)ptr - sizeof(zend_mem_header) - MEM_HEADER_PADDING);
	int no_cache_notice=0;
	int valid_beginning=1;
	int had_problems=0;
	long end_magic;

	if (silent==2) {
		silent = 1;
		no_cache_notice = 1;
	}
	if (silent==3) {
		silent = 0;
		no_cache_notice = 1;
	}
	if (!silent) {
		zend_message_dispatcher(ZMSG_LOG_SCRIPT_NAME, NULL);
		zend_debug_alloc_output("---------------------------------------\n");
		zend_debug_alloc_output("%s(%d) : Block 0x%0.8lX status:\n" ZEND_FILE_LINE_RELAY_CC, (long) p);
		if (__zend_orig_filename) {
			zend_debug_alloc_output("%s(%d) : Actual location (location was relayed)\n" ZEND_FILE_LINE_ORIG_RELAY_CC);
		}
		zend_debug_alloc_output("%10s\t","Beginning:  ");
	}

	switch (p->magic) {
		case MEM_BLOCK_START_MAGIC:
			if (!silent) {
				zend_debug_alloc_output("OK (allocated on %s:%d, %d bytes)\n", p->filename, p->lineno, p->size);
			}
			break; /* ok */
		case MEM_BLOCK_FREED_MAGIC:
			if (!silent) {
				zend_debug_alloc_output("Freed\n");
				had_problems = 1;
			} else {
				return _mem_block_check(ptr, 0 ZEND_FILE_LINE_RELAY_CC ZEND_FILE_LINE_ORIG_RELAY_CC);
			}
			break;
		case MEM_BLOCK_CACHED_MAGIC:
			if (!silent) {
				if (!no_cache_notice) {
					zend_debug_alloc_output("Cached (allocated on %s:%d, %d bytes)\n", p->filename, p->lineno, p->size);
					had_problems = 1;
				}
			} else {
				if (!no_cache_notice) {
					return _mem_block_check(ptr, 0 ZEND_FILE_LINE_RELAY_CC ZEND_FILE_LINE_ORIG_RELAY_CC);
				}
			}
			break;
		default:
			if (!silent) {
				zend_debug_alloc_output("Overrun (magic=0x%0.8lX, expected=0x%0.8lX)\n", p->magic, MEM_BLOCK_START_MAGIC);
			} else {
				return _mem_block_check(ptr, 0 ZEND_FILE_LINE_RELAY_CC ZEND_FILE_LINE_ORIG_RELAY_CC);
			}
			had_problems = 1;
			valid_beginning = 0;
			break;
	}


	memcpy(&end_magic, (((char *) p)+sizeof(zend_mem_header)+MEM_HEADER_PADDING+p->size), sizeof(long));

	if (valid_beginning && (end_magic != MEM_BLOCK_END_MAGIC)) {
		char *overflow_ptr, *magic_ptr=(char *) &mem_block_end_magic;
		int overflows=0;
		int i;

		if (silent) {
			return _mem_block_check(ptr, 0 ZEND_FILE_LINE_RELAY_CC ZEND_FILE_LINE_ORIG_RELAY_CC);
		}
		had_problems = 1;
		overflow_ptr = (char *) &end_magic;

		for (i=0; i<sizeof(long); i++) {
			if (overflow_ptr[i]!=magic_ptr[i]) {
				overflows++;
			}
		}

		zend_debug_alloc_output("%10s\t", "End:");
		zend_debug_alloc_output("Overflown (magic=0x%0.8lX instead of 0x%0.8lX)\n", end_magic, MEM_BLOCK_END_MAGIC);
		zend_debug_alloc_output("%10s\t","");
		if (overflows>=sizeof(long)) {
			zend_debug_alloc_output("At least %d bytes overflown\n", sizeof(long));
		} else {
			zend_debug_alloc_output("%d byte(s) overflown\n", overflows);
		}
	} else if (!silent) {
		zend_debug_alloc_output("%10s\t", "End:");
		if (valid_beginning) {
			zend_debug_alloc_output("OK\n");
		} else {
			zend_debug_alloc_output("Unknown\n");
		}
	}

	if (had_problems) {
		int foo = 5;

		foo += 1;
	}
		
	if (!silent) {
		zend_debug_alloc_output("---------------------------------------\n");
	}
	return ((!had_problems) ? 1 : 0);
}


ZEND_API void _full_mem_check(int silent ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC)
{
	zend_mem_header *p;
	int errors=0;
	TSRMLS_FETCH();

	p = AG(head);
	

	zend_debug_alloc_output("------------------------------------------------\n");
	zend_debug_alloc_output("Full Memory Check at %s:%d\n" ZEND_FILE_LINE_RELAY_CC);

	while (p) {
		if (!_mem_block_check((void *)((char *)p + sizeof(zend_mem_header) + MEM_HEADER_PADDING), (silent?2:3) ZEND_FILE_LINE_RELAY_CC ZEND_FILE_LINE_ORIG_RELAY_CC)) {
			errors++;
		}
		p = p->pNext;
	}
	zend_debug_alloc_output("End of full memory check %s:%d (%d errors)\n" ZEND_FILE_LINE_RELAY_CC, errors);
	zend_debug_alloc_output("------------------------------------------------\n");
}
#endif


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
