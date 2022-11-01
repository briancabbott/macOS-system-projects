/*
 * Darwin ACL VFS module
 *
 * Copyright (c) 2006-2007 Apple Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* This header has to be here due to preprocessor conflicts with Samba
 * headers.
 */
#include <sys/types.h>

#include <stdint.h>

typedef enum BRLMForkType {kBRLMDataFork = 1, kBRLMResFork= 2} BRLMForkType;
typedef enum BRLMLockType {kBRLMFree = 0x0000, kBRLMRLock = 0x0001, kBRLMWLock=0x0002} BRLMLockType;
typedef enum BRLMStatus {
    BRLMNoErr = 0,
	BRLMInitErr = 30003,
    /*Errors in the database*/
	BRLMDBOpenErr = 30013,
    BRLMDBInvalidErr = 30023,
	BRLMDBStaleErr =30043,
	BRLMDBFullErr = 30053,
	BRLMDBBoundsError = 30063,   /*attempting to delete a record not cantained in a region*/
    NLMHashTableBoundaryErr = 30083,
    BRLMDBInvalidRefErr = 31013,   /*any time an operation witha NLMRef fails*/
	BRLMFileRecNotFound = 31023,
    BRLMProcRecNotFound = 31033,
	BRLMBRLRecNotFound = 31043,
    BRLMFileRecAlreadyExists = 31053,
	BRLMOpenDenied = 32013,
    BRLMLockConflict = 32023,
    /*Sytem Errors*/
    BRLMParamErr = 35000,
    BRLMMemErr = 35013,
	BRLMSysErr = 35023,
    BRLMMiscErr = 39993
} BRLMStatus;

typedef struct NLMData NLMData;
typedef NLMData* NLMDataPtr;
typedef NLMDataPtr BRLMRef;

/*these map 1-1 with afp access permissions*/
enum {
	kBRLMRead =		0x01,
	kBRLMWrite =		0x02,
	kBRLMDenyRead = 	0x10,
	kBRLMDenyWrite = 0x20,
	kBRLMAccessMask =	0x33
};

/* these constants are for the options flag in BRLMPosixOpen	*/
enum {
	kBRLMOpenTruncate = 0x0001
 };

BRLMStatus BRLMInit(void);
BRLMStatus BRLMClose(void);

BRLMStatus BRLMPosixOpen(const u_int8_t* path,
                        BRLMForkType forkType,
						int8_t openPermissions,
                        int32_t creatPermissions,
                        mode_t mode,
                        BRLMRef* ref,
                        uint32_t sessionID,
						uint32_t options);
BRLMStatus BRLMCloseRef(BRLMRef ref);
BRLMStatus BRLMByteRangeLock(const BRLMRef ref, BRLMLockType type, u_int64_t start, u_int64_t count);
BRLMStatus BRLMByteRangeUnlock(const BRLMRef ref, BRLMLockType type, u_int64_t start, u_int64_t count);
BRLMStatus BRLMCanRead(BRLMRef ref, u_int64_t offset, u_int64_t count);
BRLMStatus BRLMCanWrite(BRLMRef ref, u_int64_t offset, u_int64_t count);

int32_t BRLMGetDescriptor(BRLMRef ref);
int16_t BRLMGetForkRefNum(BRLMRef ref);

BRLMStatus NLMDebug(uint32_t i);
BRLMStatus BRLMExchange(BRLMRef ref1, BRLMRef ref2);

#undef DEBUGLEVEL
#define BOOL_DEFINED

#include "includes.h"
#include "talloc.h"
#include "md5.h"
#include "MacExtensions.h"

#include <sys/xattr.h>
#include <sys/attr.h>
#include <sys/vnode.h>

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_VFS

#define MODULE_NAME "darwin_streams"

#define AFPINFO_STREAM_CANON 		":AFP_AfpInfo:$DATA"
#define AFPRESOURCE_STREAM_CANON	":AFP_Resource:$DATA"

static int module_trace_level = 100;
static int module_trace_brlm_level = 100;
static int module_trace_sys_entry_level= 100;

#define CANONICAL_CASE CASE_UPPER

static char afp_info_name[sizeof(AFPINFO_STREAM)];
static char afp_resource_name[sizeof(AFPRESOURCE_STREAM)];

static int darwin_open_brlm(vfs_handle_struct *handle,
		const char * fname,
		struct files_struct * fsp,
		BRLMForkType ftype,
		int flags,
		mode_t mode);

#define MODULE_TRACE		module_trace_level
#define MODULE_TRACE_BRLM	module_trace_brlm_level
#define MODULE_TRACE_SYS_ENTRY	module_trace_sys_entry_level

#define TRACE_SYS_ENTRY() DEBUG(MODULE_TRACE_SYS_ENTRY, \
	("%s: entered %s\n", MODULE_NAME, __FUNCTION__))

#define BRLM_CALL(expr, status) do { \
		int sav; \
		errno = 0; \
		status = (expr); \
		sav = errno; \
		DEBUG(MODULE_TRACE_BRLM, \
			("%s: %s gave result result=%d errno=%d\n", \
			 MODULE_NAME, #expr, (int)status, sav)); \
		errno = sav; \
	} while (0);

struct darwin_stream_io;

typedef ssize_t (*darwin_stream_pread)(struct darwin_stream_io *sio,
			int fd, void *data, size_t count, SMB_OFF_T offset);

typedef ssize_t (*darwin_stream_pwrite)(struct darwin_stream_io *sio,
			int fd, const void *data, size_t count, SMB_OFF_T offset);

typedef int (*darwin_stream_ftrunc)(struct darwin_stream_io *sio,
			int fd, SMB_OFF_T len);

typedef int (*darwin_stream_fstat)(struct darwin_stream_io *sio,
			int fd, SMB_STRUCT_STAT *sbuf);

typedef BOOL (*darwin_stream_lock)(struct darwin_stream_io *sio,
			int fd, int op, SMB_OFF_T offset, SMB_OFF_T count,
			int type);

typedef BOOL (*darwin_stream_getlock)(struct darwin_stream_io *sio,
			int fd, SMB_OFF_T *poffset, SMB_OFF_T *pcount,
			int *ptype, pid_t *ppid);

typedef int (*darwin_stream_close)(struct darwin_stream_io *sio, int fd);

struct darwin_stream_io
{

	union {
		/* Extra info for streams built on xattrs. */
		struct {
			char * sname;  /* stream (or xattr) name. */
		} xattr;

		/* Extra info for the primary data stream. */
		struct {
			BRLMRef * bref;
		} dfork;

	} xtra;

	darwin_stream_pread	pread;
	darwin_stream_pwrite	pwrite;
	darwin_stream_ftrunc	ftruncate;
	darwin_stream_fstat	fstat;
	darwin_stream_lock	lock;
	darwin_stream_getlock	getlock;
	darwin_stream_close	close;
};

typedef BOOL (*name_filter)(const char * name);

static ssize_t enosys_ssize_t(void * unused)
{
	errno = ENOSYS;
	return -1;
}

static void debug_attribute_names(int level, const char * list, size_t total)
{
	const char * name;
	const char * end;

	if (DEBUGLVL(level)) {
		end = list + total;

		for (name = list; name < end; ) {
			DEBUGADD(level, ("        %s\n", name));
			name += (strlen(name) + 1);
		}
	}
}

/* Return TRUE if we are only opening for attribute access. */
static BOOL is_attribute_open(const struct files_struct * fsp)
{
    BOOL rwopen;

    rwopen = fsp->access_mask & (FILE_READ_DATA | FILE_WRITE_DATA |
	    GENERIC_READ_ACCESS | GENERIC_WRITE_ACCESS);

    return rwopen ? False : True;
}

/* Map from a stream name to one of the special xattr names reserved by Apple.
 * This assumes the stream name has been normalised by the caller.
 */
static const char * map_special_xattr_names(const char * sname)
{
	if (strcmp(sname, afp_resource_name) == 0) {
		return XATTR_RESOURCEFORK_NAME;
	} else if (strcmp(sname, afp_info_name) == 0) {
		return XATTR_FINDERINFO_NAME;
	} else {
		return sname;
	}
}

/* Take a buffer containing a sequence on NULL-terminated strings and remove
 * any that are not allowed according to the filter function. The return is the
 * amount of valid data remaining. The passed-in buffer is modified, but not
 * grown or shrunk.
 */
static ssize_t filter_name_list(char * list, ssize_t total, name_filter allowed)
{
	char * name;
	ssize_t newtotal = 0;
	size_t xlen;
	char * end = list + total;

	if (!list) {
		return 0;
	}

	for (name = list; name < end; ) {
	    xlen = strlen(name) + 1;

	    if (!allowed(name)) {
		    size_t remain;
		    /* Move the remainder of the buffer up to the last
		     * unfiltered entry.
		     */
		    remain = end - (name + xlen);
		    memmove(name, name + xlen, remain);
		    /* Pull the end marker in by the amount we removed from the
		     * middle.
		     */
		    end -= xlen;
	    } else {
		    newtotal += xlen;
		    name += xlen;
	    }

	}

	return newtotal;
}

static ssize_t get_xattr_size(const char * path, int fd,
				const char * name, int flags)
{
	ssize_t ret;

	if (path) {
		ret = getxattr(path, name, NULL, 0, 0, flags);
	} else {
		ret =  fgetxattr(fd, name, NULL, 0, 0, flags);
	}

	if (ret == -1 && errno == ENOATTR) {
		/* XNU equates emptiness with nonexistence for these
		 * attributes. Make sure we don't get confused.
		 */
		if (strcmp(name, XATTR_FINDERINFO_NAME) == 0 ||
		    strcmp(name, XATTR_RESOURCEFORK_NAME) == 0) {
			errno = 0;
			return 0;
		}
	}

	return ret;
}

/* ========================================================================
   getxattr(2), setxattr(2) and listxattr(2) emulation.

   These map directly to the Darwin extended attributes API, but
   carefully avoid presenting any streams that were stored in xattrs.
   ======================================================================== */

static BOOL exclude_stream_names(const char * const name)
{
	/* A user stream masquerading as an xattr. */
	if (*name == ':') {
		return False;
	}

	/* Clients are supposed to use the stream name, not the xattr name. */
	if (strcmp(name, XATTR_FINDERINFO_NAME) == 0) {
		return False;
	}

	/* Clients are supposed to use the stream name, not the xattr name. */
	if (strcmp(name, XATTR_RESOURCEFORK_NAME) == 0) {
		return False;
	}

	return True;
}

static ssize_t do_list_xattr(const char * path, int fd,
		    void * list, size_t max, int options)
{
	ssize_t total; /* total length of returned name */
	ssize_t	valid; /* length of valid xattr names */

	if (path) {
		total = listxattr(path, list, max, options);
	} else {
		total = flistxattr(fd, list, max, options);
	}

	/* Bail on error or if the caller is testing for the required
	 * buffer size.
	 */
	if (total <= 0 || list == NULL) {
		return total;
	}

	/* Filter out xattrs that are masquerading as streams. */
	valid = filter_name_list(list, total, exclude_stream_names);

	DEBUG(MODULE_TRACE,
		("%u bytes of xattr names filtered down to %u bytes\n",
		 (unsigned)total, (unsigned)valid));
	return valid;
}

static ssize_t do_get_xattr(const char * path, int fd,
		    const char * name, void * value, size_t size, int options)
{
	ssize_t len;

	/* Don't let xattr clients look at any streams that we have
	 * stored in xattrs.
	 */
	if (!exclude_stream_names(name)) {
		errno = ENOATTR;
		return -1;
	}

	if (path) {
		len = getxattr(path, name, value, size, 0, options);
	} else {
		len = fgetxattr(fd, name, value, size, 0, options);
	}

	return len;
}

static ssize_t do_setxattr(const char * path, int fd,
		    const char * name, void * value, size_t size, int options)
{
	ssize_t len;

	/* Don't let xattr clients look at any streams that we have
	 * stored in xattrs.
	 */
	if (!exclude_stream_names(name)) {
		errno = EINVAL;
		return -1;
	}

	if (path) {
		len = setxattr(path, name, value, size, 0, options);
	} else {
		len = fsetxattr(fd, name, value, size, 0, options);
	}

	return len;
}

static ssize_t list_existing_xattr(const char * path, int fd, char ** names)
{
	ssize_t sz;
	size_t	buflen = 0;

	*names = NULL;

	if (path) {
		sz = listxattr(path, NULL, 0, 0);
	} else {
		sz = flistxattr(fd, NULL, 0, 0);
	}

	if (sz <= 0) {
		return sz;
	}

	buflen = sz;
	*names = SMB_MALLOC(buflen);
	if (*names == NULL) {
		return -1;
	}

	for (;;)
	{
		if (path) {
			sz = listxattr(path, *names, buflen, 0);
		} else {
			sz = flistxattr(fd, *names, buflen, 0);
		}

		switch (sz) {
		case 0:
			SAFE_FREE(*names);
			return 0;

		case -1:
			/* Oops, need to grow the name buffer ... */
			if (errno == ERANGE) {
				buflen *= 2;
				*names = SMB_REALLOC(*names, buflen);
				if (*names == NULL) {
					return -1;
				}

				continue;
			}

			SAFE_FREE(*names);
			return -1;

		default:
			return sz;
		}

	}

	return -1;
}

static ssize_t get_existing_xattr(const char * xname, int fd,
		void ** buf, size_t *bufsz)
{
	ssize_t sz = -1;

	*buf = NULL;
	*bufsz = 0;

	do
	{
		*bufsz += 1024;
		*buf = SMB_REALLOC(*buf, *bufsz);
		if (*buf == NULL) {
			/* SMB_REALLOC frees on failure. */
			*bufsz = 0;
			errno = ENOMEM;
			return -1;
		}

		sz = fgetxattr(fd, xname, *buf, *bufsz, 0, 0);
	} while (sz < 0 && errno == ERANGE);

	if (sz < 0) {
		int sav = errno;
		SAFE_FREE(*buf);
		*bufsz = 0;
		errno = sav;
		return -1;
	}

	DEBUG(MODULE_TRACE,
		("%s: xattr '%s' is %ld bytes in a %lu byte buffer\n",
		MODULE_NAME, xname, (long)sz, (unsigned long)*bufsz));

	return sz;
}

static int do_removexattr(const char * path, int fd, const char * name,
			int options)
{
	/* Don't let xattr clients look at any streams that we have
	 * stored in xattrs.
	 */
	if (!exclude_stream_names(name)) {
		errno = EINVAL;
		return -1;
	}

	if (path) {
		return removexattr(path, name, options);
	} else {
		return fremovexattr(fd, name, options);
	}
}

static ssize_t darwin_sys_listxattr(vfs_handle_struct *handle,
		    const char *path, char *list, size_t size)
{
	return do_list_xattr(path, -1, list, size, 0);
}

static ssize_t darwin_sys_llistxattr(vfs_handle_struct *handle,
		    const char *path, char *list, size_t size)
{
	return do_list_xattr(path, -1, list, size, XATTR_NOFOLLOW);
}

static ssize_t darwin_sys_flistxattr(vfs_handle_struct *handle,
		    struct files_struct *fsp, int fd, char *list,
		    size_t size)
{
	return do_list_xattr(NULL, fd, list, size, XATTR_NOFOLLOW);
}

static ssize_t darwin_sys_getxattr(vfs_handle_struct *handle,
		    const char *path, char *name, void* value, size_t size)
{
	return do_get_xattr(path, -1, name, value, size, 0);
}

static ssize_t darwin_sys_lgetxattr(vfs_handle_struct *handle,
		    const char *path, char *name, void* value, size_t size)
{
	return do_get_xattr(path, -1, name, value, size, XATTR_NOFOLLOW);
}

static ssize_t darwin_sys_fgetxattr(vfs_handle_struct *handle,
		    struct files_struct *fsp, int fd,
		    char *name, void *value, size_t size)
{
	return do_get_xattr(NULL, fd, name, value, size, 0);
}

static ssize_t darwin_sys_setxattr(vfs_handle_struct *handle,
		    const char *path, char *name, void* value,
		    size_t size, int flags)
{
	return do_setxattr(path, -1, name, value, size, flags);
}

static ssize_t darwin_sys_lsetxattr(vfs_handle_struct *handle,
		    const char *path, char *name, void* value, size_t size,
		    int flags)
{
	return do_setxattr(path, -1, name, value, size, flags | XATTR_NOFOLLOW);
}

static ssize_t darwin_sys_fsetxattr(vfs_handle_struct *handle,
		    struct files_struct *fsp, int fd,
		    char *name, void *value, size_t size, int flags)
{
	return do_setxattr(NULL, fd, name, value, size, flags);
}

static ssize_t darwin_sys_removexattr(vfs_handle_struct *handle,
		    const char *path, char *name)
{
	return do_removexattr(path, -1, name, 0);
}

static ssize_t darwin_sys_lremovexattr(vfs_handle_struct *handle,
		    const char *path, char *name)
{
	return do_removexattr(path, -1, name, XATTR_NOFOLLOW);
}

static ssize_t darwin_sys_fremovexattr(vfs_handle_struct *handle,
		    struct files_struct *fsp, int fd, char *name)
{
	return do_removexattr(NULL, fd, name, 0);
}

/* ========================================================================
   locking wrappers for xattrs, finder info and the resource fork.
   ======================================================================== */

/* FIXME: this should store POSIX-style locks in a TDB since we can't store
 * then lock info in  the kernel.
 */

static BOOL darwin_getlock_ignore(struct darwin_stream_io *sio,
		    int fd, SMB_OFF_T *poffset, SMB_OFF_T *pcount,
		    int *ptype, pid_t *ppid)
{
	errno = ENOTSUP;
	return False;
}

static BOOL darwin_lock_ignore(struct darwin_stream_io *sio,
		    int fd, int op, SMB_OFF_T offset, SMB_OFF_T count,
		    int type)
{
	errno = ENOTSUP;
	return False;
}

static BOOL darwin_getlock_brlm(struct darwin_stream_io *sio,
		    int fd, SMB_OFF_T *poffset, SMB_OFF_T *pcount,
		    int *ptype, pid_t *ppid)
{
	BRLMRef bref = *sio->xtra.dfork.bref;
	BRLMStatus bstatus;

	if (bref == 0) {
		errno = EINVAL;
		return False;
	}

	BRLM_CALL(BRLMCanRead(bref, *poffset, *pcount), bstatus);
	if (bstatus == BRLMLockConflict) {
		/* If we can't read a range, someone must have a write
		 * lock on it.
		 */
		*ptype = F_WRLCK;
		return True;
	}

	BRLM_CALL(BRLMCanWrite(bref, *poffset, *pcount), bstatus);
	if (bstatus == BRLMLockConflict) {
		/* If we can read a range, but can't write it, someone must
		 * have a read lock in it.
		 */
		*ptype = F_RDLCK;
		return True;
	}

	if (bstatus == BRLMNoErr) {
		*ptype = F_UNLCK;
		return True;
	}

	/* BRLM sets errno for us. */
	return False;
}

static BOOL darwin_lock_brlm(struct darwin_stream_io *sio,
		    int fd, int op,
		    SMB_OFF_T offset, SMB_OFF_T count,
		    int type)
{
	BRLMRef bref = *sio->xtra.dfork.bref;
	BRLMStatus bstatus;

	if (bref == 0) {
		errno = EINVAL;
		return False;
	}

	if (type != F_RDLCK && type != F_WRLCK && type != F_UNLCK) {
		DEBUG(0, ("%s: invalid lock type %d\n",
			    MODULE_NAME, type));
		errno = EINVAL;
		return False;
	}

	SMB_ASSERT(op != SMB_F_GETLK);

	if (op != SMB_F_SETLK && op != SMB_F_SETLKW) {
		DEBUG(0, ("%s: invalid lock operation %d\n",
			    MODULE_NAME, op));
		errno = EINVAL;
		return False;
	}

	switch (type) {
	case F_RDLCK:
		BRLM_CALL(BRLMByteRangeLock(bref, kBRLMRLock,
					offset, count), bstatus);
		break;
	case F_WRLCK:
		BRLM_CALL(BRLMByteRangeLock(bref, kBRLMWLock,
					offset, count), bstatus);
		break;
	case F_UNLCK:
	       BRLM_CALL(BRLMByteRangeUnlock(bref, kBRLMFree,
					offset, count), bstatus);
	       break;

	default:
		errno = EINVAL;
		return False;
	}

	return (bstatus == BRLMNoErr) ? True : False;
}

/* ========================================================================
   pwrite(2) wrappers for xattrs, finder info and the resource fork.
   ======================================================================== */

/* Write to an extended attribute as if it were a file stream. Unfortunately
 * the position argument to the xattr APIs is ignored (except for the resouce
 * fork). This means we have to read, update and copy.
 */
static ssize_t darwin_pwrite_xattr(struct darwin_stream_io *sio,
			int fd, const void *data,
			size_t count, SMB_OFF_T offset)
{
	ssize_t sz;
	size_t	bufsz;
	void *	buf;

	sz = get_existing_xattr(sio->xtra.xattr.sname, fd, &buf, &bufsz);
	if (sz < 0) {
		return -1;
	}

	if ((offset + count) > bufsz) {
		buf = SMB_REALLOC(buf, offset + count);
		if (buf == NULL) {
			errno = ENOMEM;
			return -1;
		}
	}

	memcpy((uint8_t *)buf + offset, data, count);
	sz = fsetxattr(fd, sio->xtra.xattr.sname, buf, offset + count, 0, 0);
	if (sz < 0) {
		int errsav = errno;
		SAFE_FREE(buf);
		errno = errsav;
		return -1;
	}

	SAFE_FREE(buf);
	return count;
}

static ssize_t darwin_pwrite_finfo(struct darwin_stream_io *sio,
			int fd, const void *data,
			size_t count, SMB_OFF_T offset)
{
	AfpInfo afpi;
	int ret;

	/* Additional restrictions on writing the finder info:
	 *	1. you have to write from the beginning
	 *	2. you have to write all of it
	 */
	if (offset != 0) {
		errno = ENOTSUP;
		return -1;
	}

	if (count < AFP_INFO_SIZE) {
		errno = ENOTSUP;
		return -1;
	}

	/*
	 uint32       	afpi_Signature;
	 uint32       	afpi_Version;
	 uint32       	afpi_Reserved1;
	 uint32       	afpi_BackupTime;
	 unsigned char 	afpi_FinderInfo[AFP_FinderSize];
	 unsigned char 	afpi_ProDosInfo[6];
	 unsigned char 	afpi_Reserved2[6];
	 */
	afpi.afpi_Signature = RIVAL(data, offset);
	offset += 4; /* uint32  afpi_Signature */
	afpi.afpi_Version = RIVAL(data, offset);
	offset += 4; /* uint32  afpi_Version */
	afpi.afpi_Reserved1 = RIVAL(data, offset);
	offset += 4; /* uint32  afpi_Reserved1 */
	afpi.afpi_BackupTime = RIVAL(data, offset);
	offset += 4; /* uint32  afpi_BackupTime */
	memcpy(afpi.afpi_FinderInfo, (uint8_t *)data + offset, AFP_FinderSize);
	offset += AFP_FinderSize; /* 32 bytes */
	memcpy(afpi.afpi_ProDosInfo, (uint8_t *)data + offset, 6);
	offset += 6;
	memcpy(afpi.afpi_Reserved2, (uint8_t *)data + offset, 6);
	offset += 6;
	/*
	 * On success, 0 is returned.  On failure, -1 is returned and the global
     * variable errno is set as follows.
	 */
	ret = fsetxattr(fd, XATTR_FINDERINFO_NAME,
		afpi.afpi_FinderInfo, sizeof(afpi.afpi_FinderInfo), 0, 0);
	if (ret < 0) {
		return -1;
	}

	/*
	 * Pretend we wrote everything they request. We are only writing
	 * the finder info anyways. VISTA will write 512 and then set the
	 * eof to AFP_INFO_SIZE (60 bytes).
	 */
	return count;
}

/* ========================================================================
   pread(2) wrappers for xattrs, finder info and the resource fork.
   ======================================================================== */

static ssize_t darwin_pread_xattr(struct darwin_stream_io *sio,
			int fd, void *data,
			size_t count, SMB_OFF_T offset)
{
	ssize_t sz;
	size_t	bufsz;
	size_t	overlap;
	void *	buf;

	sz = get_existing_xattr(sio->xtra.xattr.sname, fd, &buf, &bufsz);
	if (sz < 0) {
		return -1;
	}

	/* Attempt to read past EOF. */
	if (sz <= offset) {
		return 0;
	}

	overlap = (offset + count) > sz ? (sz - offset) : count;
	memcpy(data, (uint8 *)buf + offset, overlap);

	SAFE_FREE(buf);
	return overlap;
}

static ssize_t darwin_pread_finfo(struct darwin_stream_io *sio,
			int fd, void *data,
			size_t count, SMB_OFF_T offset)
{
	AfpInfo	afpi;
	size_t	bufsz;
	void *	buf;
	int	ret;

	/* Additional restrictions on reading the finder info:
	 * 	1. We only allow complete reads of the data.
	 * 	2. You can read pass the eof, but you cannot do a partial read of the data.
	 * 	3. If you are reading from the begining then you must read all the data.
	 *
	 * VISTA will attempt a 512 read. I traced this between Vista and Windows 2000. We
	 * now do the samething as the Windows 2000 server. We treat it like any other attempt
	 * to read pass the eof.
	 */
	if (offset != 0) {
	    	if (offset >= AFP_INFO_SIZE)
		    return 0;
		/* Trying to do a partial read not allowed */
		errno = ENOTSUP;
		return -1;
	}

	if (count <  AFP_INFO_SIZE) {
		errno = ENOTSUP;
		return -1;
	}

	ret = get_existing_xattr(XATTR_FINDERINFO_NAME, fd, &buf, &bufsz);
	if (ret < 0) {
		ret = 0;
	}

	if (ret != 0 && ret != AFP_FinderSize) {
		DEBUG(0, ("%s: expected %s to be %d bytes, "
			    "but found %d bytes\n",
			MODULE_NAME, XATTR_FINDERINFO_NAME,
			(int)AFP_FinderSize, (int)ret));
		SAFE_FREE(buf);
		errno = EINVAL;
		return -1;
	}

	/* FinderInfo is not the same structure as AFPInfo. Need to
	 * translate it. If the fgetxattr failed or there was no Finder info,
	 * we return an empty Finder info. According to comments in XNU, this
	 * is conventionally equivalent to not Finder info.
	 */
	ZERO_STRUCT(afpi);
	/*
	 uint32       	afpi_Signature;
	 uint32       	afpi_Version;
	 uint32       	afpi_Reserved1;
	 uint32       	afpi_BackupTime;
	 unsigned char 	afpi_FinderInfo[AFP_FinderSize];
	 unsigned char 	afpi_ProDosInfo[6];
	 unsigned char 	afpi_Reserved2[6];
	 */
	afpi.afpi_Signature = AFP_Signature; /* "AFP\0" */
	afpi.afpi_Version = AFP_Version;

	if (ret == AFP_FinderSize) {
		memcpy(afpi.afpi_FinderInfo, buf,
			sizeof(afpi.afpi_FinderInfo));
	}

	/*
	 * The above code makes sure that offset starts at zero. I know its
	 * crazy, but the afpinfo header is in big endian alway.
	 */
	RSIVAL(data, offset, afpi.afpi_Signature);
	offset += 4; /* uint32  afpi_Signature */
	RSIVAL(data, offset, afpi.afpi_Version);
	offset += 4; /* uint32  afpi_Version */
	RSIVAL(data, offset, afpi.afpi_Reserved1);
	offset += 4; /* uint32  afpi_Reserved1 */
	RSIVAL(data, offset, afpi.afpi_BackupTime);
	offset += 4; /* uint32  afpi_BackupTime */
	memcpy((uint8_t *)data + offset, afpi.afpi_FinderInfo, AFP_FinderSize);
	offset += AFP_FinderSize; /* 32 bytes */
	memcpy((uint8_t *)data + offset, afpi.afpi_ProDosInfo, 6);
	offset += 6;
	memcpy((uint8_t *)data + offset, afpi.afpi_Reserved2, 6);
	offset += 6;

	SAFE_FREE(buf);
	return AFP_INFO_SIZE;
}

/* ========================================================================
   ftruncate(2) wrappers for xattrs, finder info and the resource fork.
   ======================================================================== */

static int darwin_ftruncate_xattr(struct darwin_stream_io *sio,
				int fd, SMB_OFF_T len)
{
	char null = '\0';

	/* This is not exactly truncating, but it's as close as we can get
	 * without being able to pass a zero length to setxattr.
	 */
	return fsetxattr(fd, sio->xtra.xattr.sname, &null, 1, 0, 0);
}

static int darwin_ftruncate_finfo(struct darwin_stream_io *sio,
				int fd, SMB_OFF_T len)
{
	char null[AFP_FinderSize] = {0};


	/* This is not exactly truncating, but it's as close as we can get
	 * without being able to pass a zero length to setxattr.
	 */
	return fsetxattr(fd, sio->xtra.xattr.sname, &null, sizeof(null), 0, 0);
}

/* ========================================================================
   fstat(2) wrappers for xattrs, finder info and the resource fork.
   ======================================================================== */

static ino_t stream_inode(const SMB_STRUCT_STAT * sbuf, const char * sname)
{
	struct MD5Context ctx;
	unsigned char hash[16];

	MD5Init(&ctx);
	MD5Update(&ctx, (unsigned char *)&(sbuf->st_dev), sizeof(sbuf->st_dev));
	MD5Update(&ctx, (unsigned char *)&(sbuf->st_ino), sizeof(sbuf->st_ino));
	MD5Update(&ctx, (unsigned char *)sname, strlen(sname));
	MD5Final(hash, &ctx);

	DEBUG(MODULE_TRACE,
		("mapped st_ino=%u, st_dev=%u, sname='%s' to inode %u\n",
		(unsigned)sbuf->st_ino, (unsigned)sbuf->st_dev,
		sname, (unsigned)(*(ino_t *)hash) ));

	/* Hopefully all the variation is in the lower 4 (or 8) bytes! */
	return *(ino_t *)hash;
}

static int darwin_fstat_rsrc(struct darwin_stream_io *sio,
			int fd, SMB_STRUCT_STAT *sbuf)
{
	if (sys_fstat(fd, sbuf) == -1) {
		return -1;
	}

	/* The resource fork shares an inode number with the data fork. We
	 * need to fake up a distinct inode so that the locking layer can
	 * lock the both forks separately.
	 */
	sbuf->st_ino = stream_inode(sbuf, "/..namedfork/rsrc");

	DEBUG(MODULE_TRACE,
		("rsrc st_size=%u, st_blocks=%u, st_ino=%u\n",
		(unsigned)sbuf->st_size, (unsigned)sbuf->st_blocks,
		(unsigned)sbuf->st_ino));
	return 0;
}

static int darwin_fstat_xattr(struct darwin_stream_io *sio,
			int fd, SMB_STRUCT_STAT *sbuf)
{
	if (sys_fstat(fd, sbuf) == -1) {
		return -1;
	}

	DEBUG(MODULE_TRACE,
		("stream '%s' st_size=%u, st_blocks=%u, st_ino=%u\n",
		sio->xtra.xattr.sname, (unsigned)sbuf->st_size,
		(unsigned)sbuf->st_blocks, (unsigned)sbuf->st_ino));

	sbuf->st_size = get_xattr_size(NULL, fd, sio->xtra.xattr.sname, 0);
	if (sbuf->st_size == -1) {
		return -1;
	}

	/* We are pretending that this xattr contains the AFP_AfpInfo stream,
	 * but it really only contains the finder info field. Touch up the size
	 * to preserve the illusion.
	 */
	if (strcmp(sio->xtra.xattr.sname, XATTR_FINDERINFO_NAME) == 0 &&
	    sbuf->st_size == AFP_FinderSize) {
		sbuf->st_size = AFP_INFO_SIZE;
	}

	/* Touch up st_blocks based on st_size. This overcounts when st_size is
	 * an even number of blocks. It doesn't matter.
	 */
	sbuf->st_blocks = sbuf->st_size % STAT_ST_BLOCKSIZE + 1;

	/* Try to generate a unique inode number for each stream. Relies on
	 * stream name normalisation and a bit of luck.
	 */
	sbuf->st_ino = stream_inode(sbuf, sio->xtra.xattr.sname);

	/* Make sure that the stream appears to be a regular file,
	 * irrespective of its underlying type.
	 */
	sbuf->st_mode &= ~S_IFMT;
	sbuf->st_mode |= S_IFREG;

	DEBUG(MODULE_TRACE,
		("stream '%s' st_size=%u, st_blocks=%u, st_ino=%u\n",
		sio->xtra.xattr.sname, (unsigned)sbuf->st_size,
		(unsigned)sbuf->st_blocks, (unsigned)sbuf->st_ino));
	return 0;
}

/* ========================================================================
   close(2) wrappers.
   ======================================================================== */

static int darwin_close_brlm(struct darwin_stream_io *sio, int fd)
{
	BRLMStatus bstatus;
	BRLMRef * bref = sio->xtra.dfork.bref;

	/* Note that the fd that is passed here is not guaranteed to be the
	 * one associated with this file descriptor. It is guaranteed to
	 * be for the same file (which means that it was opened by the BRLM
	 * framework at some point).

	 */

	if (*bref != 0) {
		BRLM_CALL(BRLMCloseRef(*bref), bstatus);
		*bref = 0;
	}

	return 0;
}

static int brlm_handle_destructor(void * mem_ctx)
{
	BRLMStatus bstatus;
	BRLMRef * bref = (BRLMRef *)mem_ctx;

	if (*bref != 0) {
		BRLM_CALL(BRLMCloseRef(*bref), bstatus);
		*bref = 0;
	}

	return 0;
}

/* ========================================================================
   open(2) wrappers for BRLM, xattrs, finder info and the resource fork.
   ======================================================================== */

static int darwin_open_xattr(vfs_handle_struct *handle,
		const char * fname,
		const char * sname,
		struct files_struct * fsp,
		int flags,
		mode_t mode)
{
	ssize_t ret;
	int	baseflags;
	struct darwin_stream_io *sio = NULL;
	BOOL	xattr_is_present = True;
	int	hostfd;

	DEBUG(MODULE_TRACE, ("xattr method: fname='%s' sname='%s'\n",
		    fname, sname ? sname : ""));

	/* We use baseflags to turn off nasty side-effects when opening the
	 * underlying file.
	 */
	baseflags = flags;
	baseflags &= ~O_TRUNC;
	baseflags &= ~O_EXCL;
	/* Leave O_CREAT on so the underlying file can be created. */

	hostfd = sys_open(fname, baseflags, mode);

	/* It is legit to open a stream on a directory, but the base
	 * fd has to be read-only.
	 */
	if ((hostfd == -1) && (errno == EISDIR)) {
		baseflags &= ~O_ACCMODE;
		baseflags |= O_RDONLY;
		hostfd = sys_open(fname, baseflags, mode);
	}

	if (hostfd == -1) {
		return -1;
	}

	ret = get_xattr_size(NULL, hostfd, sname, 0);
	if (ret == -1) {
		if (errno != ENOATTR) {
			goto fail;
		}

		xattr_is_present = False;
	}

	if (!xattr_is_present) {
		/* Lookout! This is racey since someone else might have set a
		 * real value since we determined that the xattr wasn't
		 * present. There's not much we can do about this with the
		 * current API.
		 */
		if (flags & O_CREAT) {
			char null = '\0';
			ret = fsetxattr(hostfd, sname, &null, 1, 0,
				flags & O_EXCL ? XATTR_CREATE : 0);
			/* This is a "best effort" create. HFS can fail with
			 * ERANGE for com.apple.FinderInfo, and we know that
			 * if the client writes data, it will create the
			 * attribute as a side-effect.
			 */
		} else {
			errno = ENOENT;
			goto fail;
		}
	}

	/* This might be redundant if we just created the xattr above, but
	 * might not be if we are racing with another writer.
	 */
	if (flags & O_TRUNC) {
		char null = '\0';
		fsetxattr(hostfd, sname, &null, 1, 0, XATTR_REPLACE);
	}

	sio = VFS_ADD_FSP_EXTENSION(handle, fsp, struct darwin_stream_io);
	if (!sio) {
		errno = ENOMEM;
		goto fail;
	}

	sio->xtra.xattr.sname =
	    talloc_strdup(VFS_MEMCTX_FSP_EXTENSION(handle, fsp), sname);

	/* We don't need a special close operation. xtra.xattr.sname will be
	 * release when the extension talloc context is destroyed.
	 */

	sio->pread = darwin_pread_xattr;
	sio->pwrite = darwin_pwrite_xattr;
	sio->ftruncate = darwin_ftruncate_xattr;
	sio->fstat = darwin_fstat_xattr;
	sio->getlock = darwin_getlock_ignore;
	sio->lock = darwin_lock_ignore;

	return hostfd;

fail:
	{
		int sav = errno;
		if (sio) {
			VFS_REMOVE_FSP_EXTENSION(handle, fsp);
		}
		close(hostfd);
		errno = sav;
		return -1;
	}
}

static int darwin_open_finfo(vfs_handle_struct *handle,
		const char * fname,
		struct files_struct * fsp,
		int flags,
		mode_t mode)
{
	int ret;
	struct darwin_stream_io *sio;

	DEBUG(MODULE_TRACE, ("finfo method: fname='%s'\n", fname));

	/* Map the CIFS finder info stream to the MacOSX finder info
	 * extended attribute.
	 */
	ret = darwin_open_xattr(handle, fname, XATTR_FINDERINFO_NAME,
				fsp, flags|O_CREAT, mode);
	if (ret < 0) {
		return ret;
	}

	sio = VFS_FETCH_FSP_EXTENSION(handle, fsp);
	SMB_ASSERT(sio != NULL);

	/* Override the extended attribute ops, because we will need to to a
	 * bit of extra checking.
	 */
	sio->pread = darwin_pread_finfo;
	sio->pwrite = darwin_pwrite_finfo;
	sio->ftruncate = darwin_ftruncate_finfo;

	return ret;
}

static int darwin_open_rsrc(vfs_handle_struct *handle,
		const char * fname,
		struct files_struct * fsp,
		int flags,
		mode_t mode)
{
	int fd;
	int basefd;
	struct darwin_stream_io *sio;
	int errsav;
	char *fullname = NULL;

	DEBUG(MODULE_TRACE, ("rsrc method: fname='%s'\n", fname));

	fullname = talloc_asprintf(NULL, "%s%s", fname, "/..namedfork/rsrc");
	if (!fullname) {
		errno = ENOMEM;
		return -1;
	}

	sio = VFS_ADD_FSP_EXTENSION(handle, fsp, struct darwin_stream_io);
	if (!sio) {
		talloc_free(fullname);
		errno = ENOMEM;
		return -1;
	}

	sio->fstat = darwin_fstat_rsrc;

	/* If the base file does not exist, opening the resource fork will
	 * fail, even if we get O_CREAT. This makes sure that the base file
	 * is always around, but doesn't prevent an unlink racing with the
	 * second open.
	 */
	basefd = sys_open(fname, flags, mode);
	errsav = errno;
	if (basefd == -1) {
		talloc_free(fullname);
		VFS_REMOVE_FSP_EXTENSION(handle, fsp);
		errno = errsav;
		return -1;
	}

	if (lp_parm_bool(SNUM(handle->conn), MODULE_NAME, "brlm", False) &&
	    !is_attribute_open(fsp)) {
		fd = darwin_open_brlm(handle, fullname, fsp,
			    kBRLMResFork, flags, mode);
	} else {
		fd = sys_open(fullname, flags, mode);
	}

	errsav = errno;
	if (fd == -1) {
		VFS_REMOVE_FSP_EXTENSION(handle, fsp);
	}

	close(basefd);
	talloc_free(fullname);

	errno = errsav;
	return fd;
}

static int brlm_map_mode(const struct files_struct * fsp, int flags)
{
	uint32 dmode;
	int bmode;

	dmode = map_share_mode_to_deny_mode(fsp->share_access, 0);
	switch (dmode) {
		case DENY_ALL:
			bmode = kBRLMDenyRead | kBRLMDenyWrite;
			break;
		case DENY_WRITE:
			bmode = kBRLMDenyWrite;
			break;
		case DENY_READ:
			bmode = kBRLMDenyRead;
			break;
		default:
			bmode = 0;
	}

	switch (flags & O_ACCMODE) {
		case O_RDONLY:
			bmode |= kBRLMRead;
			break;
		case O_WRONLY:
			bmode |= kBRLMWrite;
			break;
		case O_RDWR:
			bmode |= (kBRLMRead | kBRLMWrite);
			break;
	}

	DEBUG(MODULE_TRACE,
	    ("mapped share_access=%#x to deny mode=%#x to BRLM mode=%#x\n",
		    fsp->share_access, dmode, bmode));
	return bmode;
}

static int darwin_open_brlm(vfs_handle_struct *handle,
		const char * fname,
		struct files_struct * fsp,
		BRLMForkType ftype,
		int flags,
		mode_t mode)
{
	BRLMRef bref;
	BRLMStatus bstatus;
	int bmode;
	struct darwin_stream_io *sio;

	DEBUG(MODULE_TRACE, ("brlm method: fname='%s'\n", fname));

	sio = VFS_ADD_FSP_EXTENSION(handle, fsp, struct darwin_stream_io);
	if (!sio) {
		errno = ENOMEM;
		return -1;
	}

	bmode = brlm_map_mode(fsp, flags);

	BRLM_CALL(BRLMPosixOpen((unsigned char *)fname, ftype,
			bmode, flags & (~O_ACCMODE), mode, &bref, 0, 0),
		  bstatus);

	switch (bstatus) {
		case BRLMNoErr:
		    sio->xtra.dfork.bref =
			    talloc_zero(VFS_MEMCTX_FSP_EXTENSION(handle, fsp),
				    BRLMRef);

		    /* Set a destructor to close this BRLM reference whenthe
		     * the fsp is destroyed.
		     */
		    talloc_set_destructor((void *)(sio->xtra.dfork.bref),
					    brlm_handle_destructor);
		    *sio->xtra.dfork.bref = bref;

		    SMB_ASSERT(sio->lock == NULL);
		    SMB_ASSERT(sio->getlock == NULL);
		    SMB_ASSERT(sio->close == NULL);

		    sio->lock = darwin_lock_brlm;
		    sio->getlock = darwin_getlock_brlm;
		    sio->close = darwin_close_brlm;

		    /* FIXME: BRLMPosixOpen always sets O_NONBLOCK. We should
		     * turn this off if it's not in the original mode.
		     */
		    DEBUG(MODULE_TRACE, ("%s: opening %s gave fd=%d\n",
				MODULE_NAME, fname, BRLMGetDescriptor(bref)));

		    return BRLMGetDescriptor(bref);

		case BRLMOpenDenied:
		case BRLMLockConflict:
			errno = EDEADLK;
			/* FALLTHRU */

		default:
			VFS_REMOVE_FSP_EXTENSION(handle, fsp);
			return(-1);
	}
}

/* ========================================================================
   Stream emulation entry points.
   ======================================================================== */

static int darwin_sys_connect(vfs_handle_struct *handle,
		const char *service, const char *user)
{
    /* Set up message levels depending on the config, eg.
     *		darwin_streams:msgtrace = 4
     *		darwin_streams:msgbrlm = 0
     *		darwin_streams:msgentry = 100
     */

    MODULE_TRACE = lp_parm_int(SNUM(handle->conn), MODULE_NAME,
	    "msgtrace", MODULE_TRACE);
    MODULE_TRACE_BRLM = lp_parm_int(SNUM(handle->conn), MODULE_NAME,
	    "msgbrlm", MODULE_TRACE_BRLM);
    MODULE_TRACE_SYS_ENTRY = lp_parm_int(SNUM(handle->conn), MODULE_NAME,
	    "msgentry", MODULE_TRACE_SYS_ENTRY);

    return SMB_VFS_NEXT_CONNECT(handle, service, user);
}

static int darwin_sys_open(vfs_handle_struct * handle,
		const char * path,
		struct files_struct * fsp,
		int flags,
		mode_t mode)
{
	pstring fname;
	pstring sname;

	TRACE_SYS_ENTRY();
	pstrcpy(fname, path);
	if (!NT_STATUS_IS_OK(split_ntfs_stream_name(fname, sname))) {
		/* Not necessarily the right error code. */
		errno = ENOENT;
		return -1;
	}

	DEBUG(MODULE_TRACE, ("split fname='%s' sname='%s'\n", fname, sname));

	/* If it's not a stream, punt it. */
	if (!(*sname)) {
		int ret;
		if (lp_parm_bool(SNUM(handle->conn), MODULE_NAME, "brlm", False) &&
		    !is_attribute_open(fsp)) {
			ret = darwin_open_brlm(handle, fname, fsp,
					    kBRLMDataFork, flags, mode);
		} else {
			ret = sys_open(path, flags, mode);
		}

		if (ret != -1) {
			fsp->is_sendfile_capable = lp_use_sendfile(SNUM(handle->conn));
		}

		return ret;
	}

	/* Only allow streams named :foo, etc. Higher layers abide by this
	 * rule, and it guarantees protection of the com.apple xattr namespace
	 * from callers' sticky fingers.
	 */
	if (*sname != ':') {
		DEBUG(MODULE_TRACE,
			("fname='%s' sname='%s'\n", fname, sname));
		DEBUGADD(MODULE_TRACE,
			("\tstream name has no leading ':'\n"));
		errno = EINVAL;
		return -1;
	}

	/* Normalise to emulate case-insensitive stream name lookups. */
	strnorm(sname, CANONICAL_CASE);

	if (strcmp(sname, afp_resource_name) == 0) {
		return darwin_open_rsrc(handle, fname, fsp, flags, mode);
	} else if (strcmp(sname, afp_info_name) == 0) {
		return darwin_open_finfo(handle, fname, fsp, flags, mode);
	} else {
		return darwin_open_xattr(handle, fname, sname, fsp, flags, mode);
	}
}

static int darwin_sys_unlink(vfs_handle_struct *handle, const char *path)
{
	pstring fname;
	pstring sname;
	const char * mapped;
	int ret;

	TRACE_SYS_ENTRY();

	pstrcpy(fname, path);
	if (!NT_STATUS_IS_OK(split_ntfs_stream_name(fname, sname))) {
		/* Not necessarily the right error code. */
		errno = ENOENT;
		return -1;
	}

	if (!(*sname)) {
		return unlink(path);
	}

	if (*sname != ':') {
		errno = ENOENT;
		return -1;
	}

	DEBUG(MODULE_TRACE, ("fname='%s' sname='%s'\n", fname, sname));

	/* Normalise to upper case to emulate case-insensitive stream name
	 * lookups.
	 */
	strnorm(sname, CANONICAL_CASE);

	mapped = map_special_xattr_names(sname);
	ret = removexattr(fname, mapped, XATTR_NOFOLLOW);

	/* Special xattrs may go AWOL as a side-effect of being zeroed. Since
	 * we can't tell when this happens, we have to just swallow the error.
	 */
	if (mapped != sname) {
		errno = (errno == ENOATTR) ? 0 : errno;
	} else {
		errno = (errno == ENOATTR) ? ENOENT : errno;
	}

	return ret;
}

static int darwin_sys_close(vfs_handle_struct *handle,
		    struct files_struct * fsp, int fd)
{
	struct darwin_stream_io *sio = NULL;
	int ret;

	TRACE_SYS_ENTRY();

	/* If this is a resource fork or the primary data fork, the fd here is
	 * that taken directly from open. For the xattr-based streams, the fd
	 * is that of the file hosting the xattr. In any case, the right thing
	 * to do is to just close it.
	 */

	sio = VFS_FETCH_FSP_EXTENSION(handle, fsp);
	if (sio && sio->close) {
		ret = sio->close(sio, fd);
	} else {
		ret = close(fd);
	}

	return ret;
}

static int darwin_sys_pread(vfs_handle_struct *handle,
			files_struct *fsp,
			int fd, void *data,
			size_t count, SMB_OFF_T offset)
{
	struct darwin_stream_io * sio;

	TRACE_SYS_ENTRY();

	sio = VFS_FETCH_FSP_EXTENSION(handle, fsp);
	if (!sio || !sio->pread) {
		return sys_pread(fd, data, count, offset);
	}

	return sio->pread(sio, fd, data, count, offset);

}

static ssize_t darwin_sys_pwrite(vfs_handle_struct *handle,
			files_struct *fsp,
			int fd, const void *data,
			size_t count, SMB_OFF_T offset)
{
	struct darwin_stream_io * sio;

	TRACE_SYS_ENTRY();

	sio = VFS_FETCH_FSP_EXTENSION(handle, fsp);
	if (!sio || !sio->pwrite) {
		return sys_pwrite(fd, data, count, offset);
	}
	return sio->pwrite(sio, fd, data, count, offset);
}

static int darwin_sys_set_create_time(vfs_handle_struct *handle,
			const char *path,
			time_t createtime)
{
	struct attrlist alist = {0};
	struct timespec ts = {0};
	pstring fname;
	pstring sname;

	pstrcpy(fname, path);
	if (!NT_STATUS_IS_OK(split_ntfs_stream_name(fname, sname))) {
		/* Not necessarily the right error code. */
		errno = ENOENT;
		return -1;
	}

	/*
	 * If we decided to add setting all times here we need to
	 * allocate a buffer big enough to handle all the times.
	 * Since we are only doing create time just use the timespec
	 * structure.
	 */
	ts.tv_sec = createtime;
        alist.bitmapcount = ATTR_BIT_MAP_COUNT;
	alist.commonattr = ATTR_CMN_CRTIME;
	return  setattrlist (fname, &alist, (void*)&ts, sizeof(struct timespec), 0);
}

/*
 * Give a path return the correct case of the end component.
 */
static BOOL darwin_sys_get_preserved_name(vfs_handle_struct *handle,
			const char *path, pstring name)
{
	struct attrlist attrlist;
	char attrbuf[sizeof(struct attrreference) + sizeof(uint32_t) + NAME_MAX + 1];
	struct attrreference * data = (struct attrreference *)attrbuf;
	uint32_t *nmlen;
	char *preserved_name = NULL;
	int len, maxlen;

	ZERO_STRUCT(attrlist);
	ZERO_STRUCT(attrbuf);
	attrlist.bitmapcount = ATTR_BIT_MAP_COUNT;
	attrlist.commonattr = ATTR_CMN_NAME;
	/* Call getattrlist to get the real volume name */
	if (getattrlist(path, &attrlist, attrbuf, sizeof(attrbuf), FSOPT_NOFOLLOW) != 0) {
		DEBUG(5, ("getattrlist for %s failed: %s\n", path, strerror(errno)));
		return False;
	}
	/* Make sure we didn't get something bad */
	maxlen = data->attr_dataoffset - (sizeof(struct attrreference) + sizeof(uint32_t));
	nmlen = (uint32_t *)(attrbuf+sizeof(struct attrreference));
	/* Should never happen, but just to be safe */
	if (*nmlen > maxlen) {
		DEBUG(5, ("name length to large for buffer nmlen = %d  maxlen = %u\n",
			    *nmlen, maxlen));
		return False;
	}
	len = *nmlen++;
	preserved_name = (char *)nmlen;
	preserved_name[len] = 0;
	pstrcpy(name, preserved_name);
	return True;
}

static int darwin_sys_ftruncate(vfs_handle_struct *handle,
			files_struct *fsp,
			int fd, SMB_OFF_T len)
{
	struct darwin_stream_io * sio;

	TRACE_SYS_ENTRY();

	sio = VFS_FETCH_FSP_EXTENSION(handle, fsp);
	if (!sio || !sio->ftruncate) {
		int ret = sys_ftruncate(fd, len);
		DEBUG(MODULE_TRACE,
			("%s: ftruncate fd=%d len=%u gave ret=%d errno=%d\n",
			MODULE_NAME, fd, (unsigned)len, ret, errno));
		return ret;
	}

	return sio->ftruncate(sio, fd, len);

}

static int darwin_sys_fstat(vfs_handle_struct *handle,
			files_struct *fsp,
			int fd, SMB_STRUCT_STAT *sbuf)
{
	struct darwin_stream_io * sio;

	TRACE_SYS_ENTRY();

	sio = VFS_FETCH_FSP_EXTENSION(handle, fsp);
	if (!sio || !sio->fstat) {
		return sys_fstat(fd, sbuf);
	}

	return sio->fstat(sio, fd, sbuf);
}

static int darwin_sys_stat_common(vfs_handle_struct *handle,
			const char * path,
			SMB_STRUCT_STAT * sbuf,
			int flags)
{
	int ret, sav;
	const char *mapped;
	pstring fname;
	pstring sname;

	int (*stat_func)(const char *, SMB_STRUCT_STAT *);

	pstrcpy(fname, path);
	if (!NT_STATUS_IS_OK(split_ntfs_stream_name(fname, sname))) {
		/* Not necessarily the right error code. */
		errno = ENOENT;
		return -1;
	}

	stat_func = (flags == XATTR_NOFOLLOW) ? sys_lstat : sys_stat;

	if (!(*sname)) {
		/* No streams involved ... */
		return stat_func(fname, sbuf);
	}

	strnorm(sname, CANONICAL_CASE);

	/* Take a different stat path for the resource fork to avoid
	 * cases where the xattr layer confuses an empty xattr for a
	 * missing xattr.
	 */
	if (strcmp(sname, afp_resource_name) == 0) {
		char * rsrc;

		rsrc = talloc_asprintf(NULL, "%s%s",
			    fname, "/..namedfork/rsrc");
		if (!rsrc) {
			errno = ENOMEM;
			return -1;
		}

		ret = stat_func(rsrc, sbuf);
		if (ret == -1) {
			sav = errno;
			talloc_free(rsrc);
			errno = sav;
			return -1;
		}

		sbuf->st_ino = stream_inode(sbuf, "/..namedfork/rsrc");
		talloc_free(rsrc);
		return 0;
	}

	/* For other streams-on-xattrs, we take the attributes of the
	 * underlying file and touch them up as necessary.
	 */
	if (stat_func(fname, sbuf) == -1) {
		return -1;
	}

	mapped = map_special_xattr_names(sname);
	sbuf->st_size = get_xattr_size(fname, -1, mapped, flags);
	sav = errno;

	if (sbuf->st_size == -1) {
		return -1;
	}

	/* We are pretending that this xattr contains the AFP_AfpInfo stream,
	 * but it really only contains the finder info field. Touch up the size
	 * to preserve the illusion.
	 */
	if (strcmp(sname, afp_info_name) == 0 &&
	    sbuf->st_size == AFP_FinderSize) {
		sbuf->st_size = AFP_INFO_SIZE;
	}

	sbuf->st_ino = stream_inode(sbuf, sname);
	sbuf->st_mode &= ~S_IFMT;
	sbuf->st_mode |= S_IFREG;
	sbuf->st_blocks = sbuf->st_size % STAT_ST_BLOCKSIZE + 1;

	errno = sav;
	return 0;
}

static int darwin_sys_stat(vfs_handle_struct *handle,
			const char * path,
			SMB_STRUCT_STAT * sbuf)
{
	TRACE_SYS_ENTRY();
	return darwin_sys_stat_common(handle, path, sbuf, 0);
}

static int darwin_sys_lstat(vfs_handle_struct *handle,
			const char * path,
			SMB_STRUCT_STAT * sbuf)
{
	TRACE_SYS_ENTRY();
	return darwin_sys_stat_common(handle, path, sbuf, XATTR_NOFOLLOW);
}

static SMB_STRUCT_DIR * darwin_sys_opendir(vfs_handle_struct *handle,
			const char * path,
			const char * mask,
			uint32 attr)
{
	TRACE_SYS_ENTRY();

	if (is_ntfs_stream_name(path)) {
		errno = ENOTDIR;
		return NULL;
	}

	return sys_opendir(path);
}

static ssize_t darwin_sys_lock(vfs_handle_struct *handle,
			files_struct *fsp, int fd, int op,
			SMB_OFF_T offset, SMB_OFF_T count, int type)
{
	struct darwin_stream_io * sio;

	TRACE_SYS_ENTRY();

	sio = VFS_FETCH_FSP_EXTENSION(handle, fsp);
	if (!sio || !sio->lock) {
		return fcntl_lock(fd, op, offset, count, type);
	}
	return sio->lock(sio, fd, op, offset, count, type);
}

static BOOL darwin_sys_getlock(vfs_handle_struct *handle,
			files_struct *fsp, int fd,
			SMB_OFF_T *poffset, SMB_OFF_T *pcount,
			int *ptype, pid_t *ppid)
{
	struct darwin_stream_io * sio;

	TRACE_SYS_ENTRY();

	sio = VFS_FETCH_FSP_EXTENSION(handle, fsp);
	if (!sio || !sio->getlock) {
		return fcntl_getlock(fd, poffset, pcount, ptype, ppid);
	}

	return sio->getlock(sio, fd, poffset, pcount, ptype, ppid);
}

static int darwin_sys_ntimes(struct vfs_handle_struct *handle,
		const char *path, const struct timespec ts[2])
{
	pstring fname;
	pstring sname;

	TRACE_SYS_ENTRY();
	pstrcpy(fname, path);
	if (!NT_STATUS_IS_OK(split_ntfs_stream_name(fname, sname))) {
		/* Not necessarily the right error code. */
		errno = ENOENT;
		return -1;
	}

	/* Updating the timestamps on a stream is the same as updating the
	 * timestamps on the host file.
	 */

	return SMB_VFS_NEXT_NTIMES(handle, fname, ts);
}

static int darwin_sys_chflags(struct vfs_handle_struct *handle,
		const char *path, int flags)
{
	pstring fname;
	pstring sname;

	TRACE_SYS_ENTRY();
	pstrcpy(fname, path);
	if (!NT_STATUS_IS_OK(split_ntfs_stream_name(fname, sname))) {
		/* Not necessarily the right error code. */
		errno = ENOENT;
		return -1;
	}

	/* Updating the flags on a stream is the same as updating the
	 * timestamps on the host file.
	 */

	return chflags(fname, flags);
}

/* ========================================================================
   Streaminfo implementation.
   ======================================================================== */

#define DEFAULT_STREAM_COUNT 2
#define DEFAULT_STREAM_LEN \
	(sizeof(afp_info_name) + sizeof(afp_resource_name))

static BOOL include_stream_names(const char * name)
{
	if (*name == ':') {
		return True;
	}

	/* This catches all "normal" xattrs as well as the resource fork and
	 * finder info xatter mappings.
	 */
	return False;
}

static int darwin_sys_streaminfo(vfs_handle_struct *handle,
	struct files_struct *fsp,
	const char *fname,
	char **names, size_t **sizes)
{
	int smax = DEFAULT_STREAM_COUNT; /* max stream names we allocated */
	int scount = 0;	/* current number of stream names */
	char * end;
	char * name;
	ssize_t total;
	ssize_t valid;
	BOOL isDir = False;

	TRACE_SYS_ENTRY();

	/* Make sure that the caller is asking for stream info for an
	 * ordinary file, not some sort of stream.
	 */
	if (fsp && fsp->fh->fd != -1) {
		if (is_ntfs_stream_name(fsp->fsp_name)) {
			errno = EINVAL;
			return -1;
		}
	} else if (is_ntfs_stream_name(fname)) {
		errno = EINVAL;
		return -1;
	}

	/* Fetch all the xattr names. */
	if (fsp && fsp->fh->fd != -1) {
	    	isDir = fsp->is_directory;
		total = list_existing_xattr(NULL, fsp->fh->fd, names);
	} else {
		SMB_STRUCT_STAT sbuf;
	   	if (sys_stat(fname, &sbuf) != -1 )
	       		isDir = S_ISDIR(sbuf.st_mode);
		total = list_existing_xattr(fname, -1, names);
	}

	if (total < 0) {
		goto fail;
	}

	/* Filter out the xattr names that are not actually named streams. */
	valid = filter_name_list(*names, total, include_stream_names);

	if ((total - valid) < DEFAULT_STREAM_LEN) {
		/* We have don't have enough room stashed for the
		 * default streams.
		 */
		*names = SMB_REALLOC(*names, total + DEFAULT_STREAM_LEN);
		if (*names == NULL) {
			goto fail;
		}

		total += DEFAULT_STREAM_LEN;
	}

	/* Preallocate any default entries. */
	if (smax) {
		*sizes = SMB_CALLOC_ARRAY(size_t, smax);
		if (*sizes == NULL) {
			goto fail;
		}
	}

	/* Walk the sequence of valid names and figure out what the length of
	 * the corresponding xattr is.
	 */
	end = (*names) + valid;
	for (name = *names; name < end; name += (strlen(name) + 1)) {
		if (scount >= (smax - DEFAULT_STREAM_COUNT)) {
			smax += 20;
			*sizes = SMB_REALLOC(*sizes, sizeof(size_t) * smax);
			if (*sizes == NULL) {
				goto fail;
			}
		}

		if (fsp && fsp->fh->fd != -1) {
			(*sizes)[scount] = get_xattr_size(NULL, fsp->fh->fd, name, 0);
		} else {
			(*sizes)[scount] = get_xattr_size(fname, -1, name, 0);
		}

		DEBUG(MODULE_TRACE, ("stream '%s' is %u bytes\n",
			    name, (unsigned)(*sizes)[scount]));

		++scount;
	}

	/* NOTE: We are careful to always append :$DATA to all stream names.
	 * This matches the canonicalisation that Windows does, except that we
	 * have to clobber the case to normal form.
	 */
	memcpy((*names) + valid, afp_info_name, sizeof(afp_info_name));
	valid += sizeof(afp_info_name);
	if (fsp && fsp->fh->fd != -1) {
		smax = get_xattr_size(NULL, fsp->fh->fd, XATTR_FINDERINFO_NAME, 0);
	} else {
		smax = get_xattr_size(fname, -1, XATTR_FINDERINFO_NAME, 0);
	}
	(*sizes)[scount++] = smax > 0 ? AFP_INFO_SIZE : 0;

	if (! isDir ) {
		memcpy((*names) + valid, afp_resource_name, sizeof(afp_resource_name));
		valid += sizeof(afp_resource_name);

		if (fsp && fsp->fh->fd != -1) {
			smax = get_xattr_size(NULL, fsp->fh->fd, XATTR_RESOURCEFORK_NAME, 0);
		} else {
			smax = get_xattr_size(fname, -1, XATTR_RESOURCEFORK_NAME, 0);
		}
		(*sizes)[scount++] = smax > 0 ? smax : 0;
	}

	if (fsp && fsp->fh->fd != -1) {
		DEBUG(MODULE_TRACE, ("%s: %d streams for file fname=%s fd=%d:\n",
			MODULE_NAME, scount, fsp->fsp_name, fsp->fh->fd));
	} else {
		DEBUG(MODULE_TRACE, ("%s: %d streams for file fname=%s:\n",
			    MODULE_NAME, scount, fname));
	}
	debug_attribute_names(MODULE_TRACE, *names, total);

	return scount;

fail:
	{
		int sav = errno;
		sav = errno;
		SAFE_FREE(*names);
		SAFE_FREE(*sizes);
		errno = sav;
		return -1;
	}
}

#undef DEFAULT_STREAM_LEN
#undef DEFAULT_STREAM_COUNT

/* ========================================================================
   VFS operations structure
   ======================================================================== */

static vfs_op_tuple darwin_streams_ops[] = {

	{SMB_VFS_OP(darwin_sys_connect), SMB_VFS_OP_CONNECT,
	    SMB_VFS_LAYER_TRANSPARENT},

	{SMB_VFS_OP(darwin_sys_open), SMB_VFS_OP_OPEN,
	    SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(darwin_sys_close), SMB_VFS_OP_CLOSE,
	    SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(darwin_sys_unlink), SMB_VFS_OP_UNLINK,
	    SMB_VFS_LAYER_OPAQUE},

	/* Don't support read and write calls. Samba will never call
	 * these if pread and pwrite are available.
	 */
	{SMB_VFS_OP(enosys_ssize_t), SMB_VFS_OP_READ,
	    SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(enosys_ssize_t), SMB_VFS_OP_WRITE,
	    SMB_VFS_LAYER_OPAQUE},

	{SMB_VFS_OP(darwin_sys_pread), SMB_VFS_OP_PREAD,
	    SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(darwin_sys_pwrite), SMB_VFS_OP_PWRITE,
	    SMB_VFS_LAYER_OPAQUE},

	{SMB_VFS_OP(darwin_sys_stat), SMB_VFS_OP_STAT,
	    SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(darwin_sys_lstat), SMB_VFS_OP_LSTAT,
	    SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(darwin_sys_fstat), SMB_VFS_OP_FSTAT,
	    SMB_VFS_LAYER_OPAQUE},

	{SMB_VFS_OP(darwin_sys_lock), SMB_VFS_OP_LOCK,
	    SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(darwin_sys_getlock), SMB_VFS_OP_GETLOCK,
	    SMB_VFS_LAYER_OPAQUE},

	{SMB_VFS_OP(darwin_sys_set_create_time), SMB_VFS_OP_SET_CREATE_TIME,
	    SMB_VFS_LAYER_OPAQUE},

	{SMB_VFS_OP(darwin_sys_get_preserved_name), SMB_VFS_OP_GET_PRESERVED_NAME,
	    SMB_VFS_LAYER_OPAQUE},

	{SMB_VFS_OP(darwin_sys_ftruncate), SMB_VFS_OP_FTRUNCATE,
	    SMB_VFS_LAYER_OPAQUE},

	{SMB_VFS_OP(darwin_sys_opendir), SMB_VFS_OP_OPENDIR,
	    SMB_VFS_LAYER_OPAQUE},

	{SMB_VFS_OP(darwin_sys_streaminfo), SMB_VFS_OP_STREAMINFO,
	    SMB_VFS_LAYER_OPAQUE},

	{SMB_VFS_OP(darwin_sys_chflags), SMB_VFS_OP_CHFLAGS,
	    SMB_VFS_LAYER_OPAQUE},

	{SMB_VFS_OP(darwin_sys_getxattr), SMB_VFS_OP_GETXATTR,
	    SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(darwin_sys_setxattr), SMB_VFS_OP_SETXATTR,
	    SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(darwin_sys_listxattr), SMB_VFS_OP_LISTXATTR,
	    SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(darwin_sys_removexattr), SMB_VFS_OP_REMOVEXATTR,
	    SMB_VFS_LAYER_OPAQUE},

	{SMB_VFS_OP(darwin_sys_fgetxattr), SMB_VFS_OP_FGETXATTR,
	    SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(darwin_sys_fsetxattr), SMB_VFS_OP_FSETXATTR,
	    SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(darwin_sys_flistxattr), SMB_VFS_OP_FLISTXATTR,
	    SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(darwin_sys_fremovexattr), SMB_VFS_OP_FREMOVEXATTR,
	    SMB_VFS_LAYER_OPAQUE},

	{SMB_VFS_OP(darwin_sys_lgetxattr), SMB_VFS_OP_LGETXATTR,
	    SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(darwin_sys_lsetxattr), SMB_VFS_OP_LSETXATTR,
	    SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(darwin_sys_llistxattr), SMB_VFS_OP_LLISTXATTR,
	    SMB_VFS_LAYER_OPAQUE},
	{SMB_VFS_OP(darwin_sys_lremovexattr), SMB_VFS_OP_LREMOVEXATTR,
	    SMB_VFS_LAYER_OPAQUE},

	{SMB_VFS_OP(darwin_sys_ntimes),	SMB_VFS_OP_NTIMES,
	    SMB_VFS_LAYER_TRANSPARENT},

	{SMB_VFS_OP(NULL), SMB_VFS_OP_NOOP, SMB_VFS_LAYER_NOOP}
};

NTSTATUS vfs_darwin_streams_init(void)
{
	BRLMStatus bstatus;

	/* Speculatively init BRLM, even if we aren't going to use it. */
	BRLM_CALL(BRLMInit(), bstatus);
	setenv("MallocBadFreeAbort", "1", 1 /* overwrite */);

	memcpy(afp_info_name, AFPINFO_STREAM,
		sizeof(AFPINFO_STREAM));
	memcpy(afp_resource_name, AFPRESOURCE_STREAM,
		sizeof(AFPRESOURCE_STREAM));

	strnorm(afp_info_name, CANONICAL_CASE);
	strnorm(afp_resource_name, CANONICAL_CASE);

	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION,
			MODULE_NAME, darwin_streams_ops);
}
