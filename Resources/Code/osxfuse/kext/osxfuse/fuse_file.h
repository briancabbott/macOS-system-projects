/*
 * Copyright (c) 2006-2008 Amit Singh/Google Inc.
 * All rights reserved.
 */

#ifndef _FUSE_FILE_H_
#define _FUSE_FILE_H_

#include "fuse.h"

#include <fuse_param.h>

#include <sys/kauth.h>
#include <sys/mman.h>

typedef enum fufh_type {
    FUFH_INVALID = -1,
    FUFH_RDONLY  = 0,
    FUFH_WRONLY  = 1,
    FUFH_RDWR    = 2,
    FUFH_MAXTYPE = 3,
} fufh_type_t;

struct fuse_filehandle {
    uint64_t fh_id;
    int32_t  open_count;
    int32_t  open_flags;
    int32_t  fuse_open_flags;
    int32_t  aux_count;
};
typedef struct fuse_filehandle * fuse_filehandle_t;

#define FUFH_IS_VALID(f)  ((f)->open_count > 0)
#define FUFH_USE_INC(f)   ((f)->open_count++)
#define FUFH_USE_DEC(f)   ((f)->open_count--)
#define FUFH_USE_RESET(f) ((f)->open_count = 0)
#define FUFH_AUX_INC(f)   ((f)->aux_count++)

FUSE_INLINE
fufh_type_t
fuse_filehandle_xlate_from_mmap(int fflags)
{
    if (fflags & PROT_WRITE) {
        if (fflags & (PROT_READ | PROT_EXEC)) {
            return FUFH_RDWR;
        } else {
            return FUFH_WRONLY;
        }
    } else if (fflags & (PROT_READ | PROT_EXEC)) {
        return FUFH_RDONLY;
    } else {
        IOLog("osxfuse: mmap being attempted with no region accessibility\n");
        return FUFH_INVALID;
    }
}

FUSE_INLINE
fufh_type_t
fuse_filehandle_xlate_from_fflags(int fflags)
{
    if ((fflags & FREAD) && (fflags & FWRITE)) {
        return FUFH_RDWR;
    } else if (fflags & (FWRITE)) {
        return FUFH_WRONLY;
    } else if (fflags & (FREAD)) {
        return FUFH_RDONLY;
    } else {
        /*
         * Looks like there might be a code path in Apple's
         * IOHDIXController/AppleDiskImagesFileBackingStore
         * that calls vnode_open() with a 0 fmode argument.
         * Translate 0 to FREAD, which is most likely what
         * that kext intends to do anyway. Lets hope the
         * calls to VNOP_OPEN and VNOP_CLOSE do match up
         * even with this fudging.
         */
        if (fflags == 0) {
            return FUFH_RDONLY;
        } else {
            panic("osxfuse: What kind of a flag is this (%x)?", fflags);
        }
    }

    return FUFH_INVALID;
}

FUSE_INLINE
int
fuse_filehandle_xlate_to_oflags(fufh_type_t type)
{
    int oflags = -1;

    switch (type) {

    case FUFH_RDONLY:
        oflags = O_RDONLY;
        break;

    case FUFH_WRONLY:
        oflags = O_WRONLY;
        break;

    case FUFH_RDWR:
        oflags = O_RDWR;
        break;

    default:
        break;
    }

    return oflags;
}

/*
 * 0 return => can proceed
 */
FUSE_INLINE
int
fuse_filehandle_preflight_status(vnode_t vp, vnode_t dvp, vfs_context_t context,
                                 fufh_type_t fufh_type)
{
    vfs_context_t icontext = context;
    kauth_action_t action  = 0;
    mount_t mp = vnode_mount(vp);
    int err = 0;

    if (vfs_authopaque(mp) || !vfs_issynchronous(mp) || !vnode_isreg(vp)) {
        goto out;
    }

#if M_OSXFUSE_ENABLE_UNSUPPORTED
    if (!icontext) {
        icontext = vfs_context_current();
    }
#endif /* M_OSXFUSE_ENABLE_UNSUPPORTED */

    if (!icontext) {
        goto out;
    }

    switch (fufh_type) {
    case FUFH_RDONLY:
        action |= KAUTH_VNODE_READ_DATA;
        break;

    case FUFH_WRONLY:
        action |= KAUTH_VNODE_WRITE_DATA;
        break;

    case FUFH_RDWR:
        action |= (KAUTH_VNODE_READ_DATA | KAUTH_VNODE_WRITE_DATA);
        break;

    default:
        err = EINVAL;
        break;
    }

    if (!err) {
        err = vnode_authorize(vp, dvp, action, icontext);
    }

out:
    return err;
}

int fuse_filehandle_get(vnode_t vp, vfs_context_t context,
                        fufh_type_t fufh_type, int mode);

int fuse_filehandle_put(vnode_t vp, vfs_context_t context,
                        fufh_type_t fufh_type, fuse_op_waitfor_t waitfor);

#endif /* _FUSE_FILE_H_ */
