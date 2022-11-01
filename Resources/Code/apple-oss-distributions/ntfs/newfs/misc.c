/**
 * misc.c : miscellaneous :
 *
 * Copyright (c) 2008 Jean-Pierre Andre
 * Copyright (c) 2008-2012 Tuxera Inc.
 *
 * See LICENSE file for licensing information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "types.h"
#include "misc.h"
#include "logging.h"

/**
 * ntfs_calloc
 * 
 * Return a pointer to the allocated memory or NULL if the request fails.
 */
void *ntfs_calloc(size_t size)
{
	void *p;
	
	p = calloc(1, size);
	if (!p)
		ntfs_log_perror("Failed to calloc %lld bytes", (long long)size);
	return p;
}

void *ntfs_malloc(size_t size)
{
	void *p;
	
	p = malloc(size);
	if (!p)
		ntfs_log_perror("Failed to malloc %lld bytes", (long long)size);
	return p;
}
