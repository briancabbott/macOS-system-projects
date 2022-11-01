/*
 * Copyright (c) 2005-2007 Rob Braun
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Rob Braun nor the names of his contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * 03-Apr-2005
 * DRI: Rob Braun <bbraun@synack.net>
 */
/*
 * Portions Copyright 2006, Apple Computer, Inc.
 * Christopher Ryan <ryanc@apple.com>
*/

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include "config.h"
#include <ctype.h>
#ifndef HAVE_ASPRINTF
#include "asprintf.h"
#endif
#include "xar.h"
#include "archive.h"
#include "filetree.h"

int xar_is_safe_filename(const char *in_filename, char** out_filename)
{
	int result = -1;
	
	// Bail for NULL
	if (in_filename == NULL) {
		goto exit;
	}

	// Set walker.
	size_t slash_count = 0;
	size_t in_filename_length = strlen(in_filename);

	// Find how many "/" we have.
	// This doesn't deal with \/ but it's close enough.
	for (size_t index = 0; index < in_filename_length; ++index)
	{
		if (in_filename[index] == '/') {
			if (slash_count <= PATH_MAX) {
				++slash_count;
			} else {
				fprintf(stderr, "slash_count exceeded PATH_MAX. Filename is invalid.\n");
				goto exit;
			}
		}
		
	}
	
	// If we didn't find any "/" just return 0.
	result = (slash_count == 0) ? 0 : -1;
	
	// If we don't have a slash or don't have a out, just bail.
	if (out_filename == NULL) {
		goto exit;
	}
	
	// Shortcut the rest with strdup in this case.
	if (slash_count == 0) {
		*out_filename = strdup(in_filename);
		goto exit;
	}
	
	
	// Now do the translation.
	*out_filename = calloc(in_filename_length+1, 1);
	
	for (size_t index = 0; index < in_filename_length; ++index)
	{
		if (in_filename[index] == '/') {
			// Add "_" at this location
			(*out_filename)[index] = ':';
		} else {
			(*out_filename)[index] = in_filename[index];
		}
	}
	
exit:
	
	return result;
}

// SC: This is function is a exact copy of dirname BUT instead of
// just using the same buffer over and over again, we allocate one.
// This means the result needs to be freed, but it also means the
// result will be correct.
char *xar_safe_dirname(const char *path)
{
	char *dname = malloc(PATH_MAX);
	const char *endp;
	size_t len;

	/* Empty or NULL string gets treated as "." */
	if (path == NULL || *path == '\0') {
		dname[0] = '.';
		dname[1] = '\0';
		return (dname);
	}

	/* Strip any trailing slashes */
	endp = path + strlen(path) - 1;
	while (endp > path && *endp == '/')
		endp--;

	/* Find the start of the dir */
	while (endp > path && *endp != '/')
		endp--;

	/* Either the dir is "/" or there are no slashes */
	if (endp == path) {
		dname[0] = *endp == '/' ? '/' : '.';
		dname[1] = '\0';
		return (dname);
	} else {
		/* Move forward past the separating slashes */
		do {
			endp--;
		} while (endp > path && *endp == '/');
	}

	len = endp - path + 1;
	if (len >= MAXPATHLEN) {
		errno = ENAMETOOLONG;
		free(dname);
		return (NULL);
	}
	memmove(dname, path, len);
	dname[len] = '\0';
	return (dname);
}


uint64_t xar_ntoh64(uint64_t num) {
	int t = 1234;
	union conv {
		uint64_t i64;
		uint32_t i32[2];
	} *in, out;

	if( ntohl(t) == t ) {
		out.i64 = num;
		return out.i64;
	}
	in = (union conv *)&num;
	out.i32[1] = ntohl(in->i32[0]);
	out.i32[0] = ntohl(in->i32[1]);
	return(out.i64);
}

uint32_t xar_swap32(uint32_t num) {
	uint8_t *one, *two;
	uint32_t ret;

	two = (uint8_t *)&ret;
	one = (uint8_t *)&num;
	two[3] = one[0];
	two[2] = one[1];
	two[1] = one[2];
	two[0] = one[3];

	return ret;
}

char *xar_get_safe_path(xar_file_t f)
{
	char *ret, *tmp;
	const char *unsafe_parent_name;
	xar_file_t i;

	xar_prop_get(f, "name", &unsafe_parent_name);
	if( unsafe_parent_name == NULL ) {
		return NULL;
	}
	
	// strdup the parent name into ret if safe.
	xar_is_safe_filename(unsafe_parent_name, &ret);
	
	for(i = XAR_FILE(f)->parent; i; i = XAR_FILE(i)->parent) {
		const char *unsafe_name;
		char* name = NULL;
		
		xar_prop_get(i, "name", &unsafe_name);
		xar_is_safe_filename(unsafe_name, &name);
		
		tmp = ret;
		asprintf(&ret, "%s/%s", name, tmp);
		free(tmp);
		free(name);
	}

	return ret;
}

/* xar_get_path
 * Summary: returns the archive path of the file f.
 * Caller needs to free the return value.
 */
char *xar_get_path(xar_file_t f) {
	char *ret, *tmp;
	const char *name;
	xar_file_t i;

	xar_prop_get(f, "name", &name);
	if( name == NULL ) {
		return NULL;
	}
	
	ret = strdup(name);
	for(i = XAR_FILE(f)->parent; i; i = XAR_FILE(i)->parent) {
		const char *name;
			xar_prop_get(i, "name", &name);
		tmp = ret;
		asprintf(&ret, "%s/%s", name, tmp);
		free(tmp);
	}

	return ret;
}


off_t xar_get_heap_offset(xar_t x) {
	return XAR(x)->toc_count + sizeof(xar_header_t);
}

/* xar_read_fd
 * Summary: Reads from a file descriptor a certain number of bytes to a specific
 * buffer.  This simple wrapper just handles certain retryable error situations.
 * Returns -1 when it fails fatally; the number of bytes read otherwise.
 */
ssize_t xar_read_fd( int fd, void * buffer, size_t nbyte ) {
	ssize_t rb;
	ssize_t total = 0;

	while ( total < nbyte ) {
		rb = read(fd, ((char *)buffer)+total, nbyte-total);
		if( rb == 0 ) {
			return total;
		} else if( rb < 0 ) {
			if( (errno == EINTR) || (errno == EAGAIN) )
				continue;
			return rb;
		}
		total += rb;
	}

	return total;
}

/* xar_pread_fd
 * Summary: Does the same as xar_read_fd, but uses pread(2) instead of read(2).
 */
ssize_t xar_pread_fd(int fd, void * buffer, size_t nbyte, off_t offset) {
	ssize_t rb;
	ssize_t total = 0;
	
	while ( total < nbyte ) {
		rb = pread(fd, ((char *)buffer)+total, nbyte-total, offset+total);
		if( rb == 0 ) {
			return total;
		} else if( rb < 0 ) {
			if( (errno == EINTR) || (errno == EAGAIN) )
				continue;
			return rb;
		}
		total += rb;
	}
	
	return total;
}

/* xar_write_fd
 * Summary: Writes from a buffer to a file descriptor.  Like xar_read_fd it
 * also just handles certain retryable error situations.
 * Returns -1 when it fails fatally; the number of bytes written otherwise.
 */
ssize_t xar_write_fd( int fd, void * buffer, size_t nbyte ) {
	ssize_t rb;
	ssize_t total = 0;

	while ( total < nbyte ) {
		rb = write(fd, ((char *)buffer)+total, nbyte-total);
		if( rb == 0 ) {
			return total;
		} else if( rb < 0 ) {
			if( (errno == EINTR) || (errno == EAGAIN) )
				continue;
			return rb;
		}
		total += rb;
	}

	return total;
}

/* xar_pwrite_fd
 * Summary: Does the same as xar_write_fd, but uses pwrite(2) instead of write(2).
 */
ssize_t xar_pwrite_fd( int fd, void * buffer, size_t nbyte, off_t offset ) {
	ssize_t rb;
	size_t total = 0;
	
	while( total < nbyte ) {
		rb = pwrite(fd, ((char *)buffer)+total, nbyte-total, offset+total);
		if( rb == 0 ) {
			return total;
		} else if( rb < 0 ) {
			if( (errno == EINTR) || (errno == EAGAIN) )
				continue;
			return rb;
		}
		total += rb;
	}
	
	return total;
}

dev_t xar_makedev(uint32_t major, uint32_t minor)
{
#ifdef makedev
	return makedev(major, minor);
#else
	return (major << 8) | minor;
#endif
}

void xar_devmake(dev_t dev, uint32_t *out_major, uint32_t *out_minor)
{
#ifdef major
	*out_major = major(dev);
#else
	*out_major = (dev >> 8) & 0xFF;
#endif
#ifdef minor
	*out_minor = minor(dev);
#else
	*out_minor = dev & 0xFF;
#endif
	return;
}

char* xar_path_nextcomponent(char** path_to_advance) {
	char* component_start = *path_to_advance;
	unsigned int component_length = 1;
	char* out_component = NULL;
	
	if (**path_to_advance == '\0') // If we're trying to look at the end of the path, punt
		return NULL;
		
	for (; **path_to_advance && (**path_to_advance != '/'); ++(*path_to_advance), ++component_length) {
		if (**path_to_advance == '\\') {	// Escape ignores next char
			++(*path_to_advance);
			++component_length;
			continue;
		}
	}
	
	if (**path_to_advance == '/') {
		++(*path_to_advance);
	}
	
	out_component = (char*)malloc(component_length);
	strncpy(out_component, component_start, component_length);
	
	out_component[component_length-1] = 0;
	
	return out_component;
}

char* xar_lowercase_string(const char* string)
{
	if (string == NULL)
		return NULL;
	
	size_t string_length = strlen(string)+1;
	char* result = calloc(string_length, 1);
	
	for (size_t index = 0; index < string_length; ++index) {
		result[index] = tolower(string[index]);
	}
	
	return result;
}
		

int xar_path_issane(char* path) {
	char* path_walker = path;
	char* component = NULL;
	int path_depth = 0;
	
	if (path == NULL) {
		return 0;
	}
	
	// Ban 0 length / absolute paths.
	if (strlen(path) == 0 || path[0] == '/')
		return 0;
	
	while ((component = xar_path_nextcomponent(&path_walker))) {
		
		if (strlen(component) == 0 || strcmp(component, ".") == 0) { // Since // is legal, and '.' is legal it's possible to have empty path elements. Ignore them
			free(component);
			continue;
		}
		
		if (strcmp(component, ".."))
			++path_depth;
		else
			--path_depth;
	
		free(component);

		if (path_depth < 0)	// We've escaped our root, this path is not sane.
			return 0;
	}
	
	return 1;
}


#ifndef HAVE_STRMODE
#include "strmode.h"
#endif

char *xar_get_type(xar_t x, xar_file_t f) {
	const char *type = NULL;
	xar_prop_get(f, "type", &type);
	if( type == NULL )
		type = "unknown";
	return strdup(type);
}

char *xar_get_size(xar_t x, xar_file_t f) {
	const char *size = NULL;
	const char *type = NULL;

	xar_prop_get(f, "type", &type);
	if( type != NULL ) {
		if( strcmp(type, "hardlink") == 0 ) {
			const char *link = NULL;
			link = xar_attr_get(f, "type", "link");
			if( link ) {
				if( strcmp(link, "original") != 0 ) {
					xar_iter_t i;
					i = xar_iter_new();
					if( i ) {
						xar_file_t tmpf;
						for(tmpf = xar_file_first(x, i); tmpf; tmpf = xar_file_next(i)) {
							const char *id;
							id = xar_attr_get(tmpf, NULL, "id");
							if( !id ) continue;
							if( strcmp(id, link) == 0 ) {
								f = tmpf;
								break;
							}
						}
					}
					xar_iter_free(i);
				}
			}
		}
	}
	xar_prop_get(f, "data/size", &size);
	if( size == NULL )
		size = "0";
	return strdup(size);
}

char *xar_get_mode(xar_t x, xar_file_t f) {
	const char *mode = NULL;
	const char *type = NULL;
	char *ret;
	mode_t m;
	xar_prop_get(f, "mode", &mode);
	if( mode == NULL )
		return  strdup("??????????");
	errno = 0;
	m = strtoll(mode, 0, 8);
	if( errno )
		return strdup("??????????");

	xar_prop_get(f, "type", &type);
	if( type == NULL )
		return strdup("??????????");
		
	if( strcmp(type, "file") == 0 )
		m |= S_IFREG;
	else if( strcmp(type, "hardlink") == 0 )
		m |= S_IFREG;
	else if( strcmp(type, "directory") == 0 )
		m |= S_IFDIR;
	else if( strcmp(type, "symlink") == 0 )
		m |= S_IFLNK;
	else if( strcmp(type, "fifo") == 0 )
		m |= S_IFIFO;
	else if( strcmp(type, "character special") == 0 )
		m |= S_IFCHR;
	else if( strcmp(type, "block special") == 0 )
		m |= S_IFBLK;
	else if( strcmp(type, "socket") == 0 )
		m |= S_IFSOCK;
#ifdef S_IFWHT
	else if( strcmp(type, "whiteout") == 0 )
		m |= S_IFWHT;
#endif

	ret = calloc(12,1);
	strmode(m, ret);

	return ret;
}

char *xar_get_owner(xar_t x, xar_file_t f) {
	const char *user = NULL;

	xar_prop_get(f, "user", &user);
	if( !user )
		return strdup("unknown");
	return strdup(user);
}

char *xar_get_group(xar_t x, xar_file_t f) {
	const char *group = NULL;

	xar_prop_get(f, "group", &group);
	if( !group )
		return strdup("unknown");
	return strdup(group);
}

char *xar_get_mtime(xar_t x, xar_file_t f) {
	const char *mtime = NULL;
	char *tmp;
	struct tm tm;

	xar_prop_get(f, "mtime", &mtime);
	if( !mtime )
		mtime = "1970-01-01T00:00:00Z";

	strptime(mtime, "%FT%T", &tm);
	tmp = calloc(128,1);
	strftime(tmp, 127, "%F %T", &tm);
	return tmp;
}

size_t xar_optimal_io_size_at_path(const char *path)
{
	// Start at 1MiB
	size_t optimal_rsize = 1024 * 1024;
	
	// Stat the destination of the archive to determine the optimal fs operation size
	struct statfs target_mount_stat_fs;
	if ( statfs(path, &target_mount_stat_fs) == 0 )
	{
		// iosize is the size that the filesystem likes to work in
		size_t fs_iosize = target_mount_stat_fs.f_iosize;
		if ( fs_iosize == -1 )
		{
			fs_iosize = optimal_rsize;
		}
		
		// If we're a remote filesystem, never let us go below the optimal size above of 1MiB
		// NFS is horrible and lies that the optimal size is 512 bytes.
		// Whereas SMB in my testing returns 7MiBs (far more practicle)
		// This ensures that we do something sane if reading over the network
		if ( ( target_mount_stat_fs.f_flags & MNT_LOCAL ) != MNT_LOCAL )
		{
			if ( fs_iosize > optimal_rsize )
			{
				optimal_rsize = fs_iosize;
			}
		}
		else
		{
			optimal_rsize = fs_iosize;
		}
	}
	
	return optimal_rsize;
}
