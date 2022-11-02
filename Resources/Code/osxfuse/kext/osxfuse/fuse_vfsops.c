/*
 * Copyright (c) 2006-2008 Amit Singh/Google Inc.
 * Copyright (c) 2010 Tuxera Inc.
 * Copyright (c) 2011 Anatol Pomozov
 * Copyright (c) 2012-2017 Benjamin Fleischer
 * All rights reserved.
 */

#include "fuse_vfsops.h"

#include "fuse_device.h"
#include "fuse_internal.h"
#include "fuse_ipc.h"
#include "fuse_locking.h"
#include "fuse_node.h"

#if M_OSXFUSE_ENABLE_INTERIM_FSNODE_LOCK
#  include "fuse_biglock_vnops.h"
#endif

#include <fuse_mount.h>

#include <libkern/version.h>

static const struct timespec kZeroTime = { 0, 0 };

vfstable_t fuse_vfs_table_ref = NULL;

errno_t (**fuse_vnode_operations)(void *);

static struct vnodeopv_desc fuse_vnode_operation_vector_desc = {
    &fuse_vnode_operations,              // opv_desc_vector_p
#if M_OSXFUSE_ENABLE_INTERIM_FSNODE_LOCK
    fuse_biglock_vnode_operation_entries // opv_desc_ops
#else
    fuse_vnode_operation_entries         // opv_desc_ops
#endif /* M_OSXFUSE_ENABLE_INTERIM_FSNODE_LOCK */
};

#if M_OSXFUSE_ENABLE_FIFOFS
errno_t (**fuse_fifo_operations)(void *);

static struct vnodeopv_desc fuse_fifo_operation_vector_desc = {
    &fuse_fifo_operations,      // opv_desc_vector_p
    fuse_fifo_operation_entries // opv_desc_ops
};
#endif /* M_OSXFUSE_ENABLE_FIFOFS */

#if M_OSXFUSE_ENABLE_SPECFS
errno_t (**fuse_spec_operations)(void *);

static struct vnodeopv_desc fuse_spec_operation_vector_desc = {
    &fuse_spec_operations,      // opv_desc_vector_p
    fuse_spec_operation_entries // opv_desc_ops
};
#endif /* M_OSXFUSE_ENABLE_SPECFS */

static struct vnodeopv_desc *fuse_vnode_operation_vector_desc_list[] =
{
    &fuse_vnode_operation_vector_desc,

#if M_OSXFUSE_ENABLE_FIFOFS
    &fuse_fifo_operation_vector_desc,
#endif

#if M_OSXFUSE_ENABLE_SPECFS
    &fuse_spec_operation_vector_desc,
#endif
};

#if M_OSXFUSE_ENABLE_INTERIM_FSNODE_LOCK

static errno_t
fuse_vfsop_biglock_mount(mount_t mp, vnode_t devvp, user_addr_t udata,
                 vfs_context_t context);

static errno_t
fuse_vfsop_biglock_unmount(mount_t mp, int mntflags, vfs_context_t context);

static errno_t
fuse_vfsop_biglock_root(mount_t mp, struct vnode **vpp, vfs_context_t context);

static errno_t
fuse_vfsop_biglock_getattr(mount_t mp, struct vfs_attr *attr, vfs_context_t context);

static errno_t
fuse_vfsop_biglock_sync(mount_t mp, int waitfor, vfs_context_t context);

static errno_t
fuse_vfsop_biglock_setattr(mount_t mp, struct vfs_attr *fsap, vfs_context_t context);

#endif /* M_OSXFUSE_ENABLE_INTERIM_FSNODE_LOCK */

static struct vfsops fuse_vfs_ops = {
#if M_OSXFUSE_ENABLE_INTERIM_FSNODE_LOCK
    fuse_vfsop_biglock_mount,   // vfs_mount
    NULL,                       // vfs_start
    fuse_vfsop_biglock_unmount, // vfs_unmount
    fuse_vfsop_biglock_root,    // vfs_root
    NULL,                       // vfs_quotactl
    fuse_vfsop_biglock_getattr, // vfs_getattr
    fuse_vfsop_biglock_sync,    // vfs_sync
    NULL,                       // vfs_vget
    NULL,                       // vfs_fhtovp
    NULL,                       // vfs_vptofh
    NULL,                       // vfs_init
    NULL,                       // vfs_sysctl
    fuse_vfsop_biglock_setattr, // vfs_setattr
#else
    fuse_vfsop_mount,   // vfs_mount
    NULL,               // vfs_start
    fuse_vfsop_unmount, // vfs_unmount
    fuse_vfsop_root,    // vfs_root
    NULL,               // vfs_quotactl
    fuse_vfsop_getattr, // vfs_getattr
    fuse_vfsop_sync,    // vfs_sync
    NULL,               // vfs_vget
    NULL,               // vfs_fhtovp
    NULL,               // vfs_vptofh
    NULL,               // vfs_init
    NULL,               // vfs_sysctl
    fuse_vfsop_setattr, // vfs_setattr
#endif
#if VERSION_MAJOR < 16
    {
#endif
    NULL,               // vfs_ioctl
    NULL,               // vfs_vget_snapdir
    NULL,               // vfs_reserved5
    NULL,               // vfs_reserved4
    NULL,               // vfs_reserved3
    NULL,               // vfs_reserved2
    NULL                // vfs_reserved1
#if VERSION_MAJOR < 16
    }
#endif
};

struct vfs_fsentry fuse_vfs_entry = {

    // VFS operations
    &fuse_vfs_ops,

    // Number of vnodeopv_desc being registered
    (int)(sizeof(fuse_vnode_operation_vector_desc_list) /\
          sizeof(*fuse_vnode_operation_vector_desc_list)),

    // The vnodeopv_desc's
    fuse_vnode_operation_vector_desc_list,

    // File system type number
    0,

    // File system type name
    OSXFUSE_NAME,

    // Flags specifying file system capabilities
#if M_OSXFUSE_ENABLE_INTERIM_FSNODE_LOCK
    VFS_TBLTHREADSAFE |
#endif
    VFS_TBL64BITREADY | VFS_TBLNOTYPENUM | VFS_TBLREADDIR_EXTENDED,

    // Reserved for future use
    { NULL, NULL }
};

static errno_t
fuse_vfsop_mount(mount_t mp, __unused vnode_t devvp, user_addr_t udata,
                 vfs_context_t context)
{
    int err      = 0;
    int mntopts  = 0;
    bool mounted = false;

    uint32_t drandom  = 0;
    uint32_t max_read = ~0;

    size_t len;

    fuse_device_t      fdev = NULL;
    struct fuse_data  *data = NULL;
    fuse_mount_args    fusefs_args;
    struct vfsstatfs  *vfsstatfsp = vfs_statfs(mp);

#if M_OSXFUSE_ENABLE_BIG_LOCK
    fuse_biglock_t    *biglock;
#endif

    fuse_trace_printf_vfsop();

    if (vfs_isupdate(mp)) {
        return ENOTSUP;
    }

    err = copyin(udata, &fusefs_args, sizeof(fusefs_args));
    if (err) {
        return EINVAL;
    }

    /*
     * Interesting flags that we can receive from mount or may want to
     * otherwise forcibly set include:
     *
     *     MNT_ASYNC
     *     MNT_AUTOMOUNTED
     *     MNT_DEFWRITE
     *     MNT_DONTBROWSE
     *     MNT_IGNORE_OWNERSHIP
     *     MNT_JOURNALED
     *     MNT_NODEV
     *     MNT_NOEXEC
     *     MNT_NOSUID
     *     MNT_NOUSERXATTR
     *     MNT_RDONLY
     *     MNT_SYNCHRONOUS
     *     MNT_UNION
     */

#if M_OSXFUSE_ENABLE_UNSUPPORTED
    vfs_setlocklocal(mp);
#endif /* M_OSXFUSE_ENABLE_UNSUPPORTED */

    /** Option Processing. **/

    if (fusefs_args.altflags & FUSE_MOPT_FSTYPENAME) {
        size_t typenamelen = strlen(fusefs_args.fstypename);
        if ((typenamelen <= 0) || (typenamelen > FUSE_TYPE_NAME_MAXLEN)) {
            return EINVAL;
        }
        snprintf(vfsstatfsp->f_fstypename, MFSTYPENAMELEN, "%s%s",
                 OSXFUSE_TYPE_NAME_PREFIX, fusefs_args.fstypename);
    }

    if ((fusefs_args.daemon_timeout > FUSE_MAX_DAEMON_TIMEOUT) ||
        (fusefs_args.daemon_timeout < FUSE_MIN_DAEMON_TIMEOUT)) {
        return EINVAL;
    }

    if (fusefs_args.altflags & FUSE_MOPT_SPARSE) {
        mntopts |= FSESS_SPARSE;
    }

    if (fusefs_args.altflags & FUSE_MOPT_SLOW_STATFS) {
        mntopts |= FSESS_SLOW_STATFS;
    }

    if (fusefs_args.altflags & FUSE_MOPT_AUTO_CACHE) {
        mntopts |= FSESS_AUTO_CACHE;
    }

    if (fusefs_args.altflags & FUSE_MOPT_AUTO_XATTR) {
        if (fusefs_args.altflags & FUSE_MOPT_NATIVE_XATTR) {
            return EINVAL;
        }
        mntopts |= FSESS_AUTO_XATTR;
    } else if (fusefs_args.altflags & FUSE_MOPT_NATIVE_XATTR) {
        mntopts |= FSESS_NATIVE_XATTR;
    }

    if (fusefs_args.altflags & FUSE_MOPT_NO_BROWSE) {
        vfs_setflags(mp, MNT_DONTBROWSE);
    }

    if (fusefs_args.altflags & FUSE_MOPT_JAIL_SYMLINKS) {
        mntopts |= FSESS_JAIL_SYMLINKS;
    }

    /*
     * Note that unlike Linux, which keeps allow_root in user-space and passes
     * allow_other in that case to the kernel, we let allow_root reach the
     * kernel. The 'if' ordering is important here.
     */
    if (fusefs_args.altflags & FUSE_MOPT_ALLOW_ROOT) {
        int is_member = 0;
        if ((kauth_cred_ismember_gid(kauth_cred_get(), fuse_admin_group,
                                     &is_member) == 0) && is_member) {
            mntopts |= FSESS_ALLOW_ROOT;
        } else {
            IOLog("osxfuse: caller not a member of osxfuse admin group (%d)\n",
                  fuse_admin_group);
            return EPERM;
        }
    } else if (fusefs_args.altflags & FUSE_MOPT_ALLOW_OTHER) {
        if (!fuse_allow_other && !fuse_vfs_context_issuser(context)) {
            int is_member = 0;
            if ((kauth_cred_ismember_gid(kauth_cred_get(), fuse_admin_group,
                                         &is_member) != 0) || !is_member) {
                return EPERM;
            }
        }
        mntopts |= FSESS_ALLOW_OTHER;
    }

    if (fusefs_args.altflags & FUSE_MOPT_NO_APPLEDOUBLE) {
        mntopts |= FSESS_NO_APPLEDOUBLE;
    }

    if (fusefs_args.altflags & FUSE_MOPT_NO_APPLEXATTR) {
        mntopts |= FSESS_NO_APPLEXATTR;
    }

    if ((fusefs_args.altflags & FUSE_MOPT_FSID) && (fusefs_args.fsid != 0)) {
        fsid_t   fsid;
        mount_t  other_mp;
        uint32_t target_dev;

        target_dev = FUSE_MAKEDEV(FUSE_CUSTOM_FSID_DEVICE_MAJOR,
                                  fusefs_args.fsid);

        fsid.val[0] = target_dev;
        fsid.val[1] = FUSE_CUSTOM_FSID_VAL1;

        other_mp = vfs_getvfs(&fsid);
        if (other_mp != NULL) {
            return EPERM;
        }

        vfsstatfsp->f_fsid.val[0] = target_dev;
        vfsstatfsp->f_fsid.val[1] = FUSE_CUSTOM_FSID_VAL1;

    } else {
        vfs_getnewfsid(mp);
    }

    if (fusefs_args.altflags & FUSE_MOPT_NO_LOCALCACHES) {
        mntopts |= FSESS_NO_ATTRCACHE;
        mntopts |= FSESS_NO_READAHEAD;
        mntopts |= FSESS_NO_UBC;
        mntopts |= FSESS_NO_VNCACHE;
    }

    if (fusefs_args.altflags & FUSE_MOPT_NO_ATTRCACHE) {
        mntopts |= FSESS_NO_ATTRCACHE;
    }

    if (fusefs_args.altflags & FUSE_MOPT_NO_READAHEAD) {
        mntopts |= FSESS_NO_READAHEAD;
    }

    if (fusefs_args.altflags & (FUSE_MOPT_NO_UBC | FUSE_MOPT_DIRECT_IO)) {
        mntopts |= FSESS_NO_UBC;
    }

    if (fusefs_args.altflags & FUSE_MOPT_NO_VNCACHE) {
        mntopts |= FSESS_NO_VNCACHE;
    }

    if (fusefs_args.altflags & FUSE_MOPT_NEGATIVE_VNCACHE) {
        if (mntopts & FSESS_NO_VNCACHE) {
            return EINVAL;
        }
        mntopts |= FSESS_NEGATIVE_VNCACHE;
    }

    if (fusefs_args.altflags & FUSE_MOPT_NO_SYNCWRITES) {

        /* Cannot mix 'nosyncwrites' with 'noubc' or 'noreadahead'. */
        if (mntopts & (FSESS_NO_READAHEAD | FSESS_NO_UBC)) {
            return EINVAL;
        }

        mntopts |= FSESS_NO_SYNCWRITES;
        vfs_clearflags(mp, MNT_SYNCHRONOUS);
        vfs_setflags(mp, MNT_ASYNC);

        /* We check for this only if we have nosyncwrites in the first place. */
        if (fusefs_args.altflags & FUSE_MOPT_NO_SYNCONCLOSE) {
            mntopts |= FSESS_NO_SYNCONCLOSE;
        }

    } else {
        vfs_clearflags(mp, MNT_ASYNC);
        vfs_setflags(mp, MNT_SYNCHRONOUS);
    }

    vfs_setauthopaque(mp);
    vfs_setauthopaqueaccess(mp);

    if ((fusefs_args.altflags & FUSE_MOPT_DEFAULT_PERMISSIONS) &&
        (fusefs_args.altflags & FUSE_MOPT_DEFER_PERMISSIONS)) {
        return EINVAL;
    }

    if (fusefs_args.altflags & FUSE_MOPT_DEFAULT_PERMISSIONS) {
        mntopts |= FSESS_DEFAULT_PERMISSIONS;
        vfs_clearauthopaque(mp);
    }

    if (fusefs_args.altflags & FUSE_MOPT_DEFER_PERMISSIONS) {
        mntopts |= FSESS_DEFER_PERMISSIONS;
    }

    if (fusefs_args.altflags & FUSE_MOPT_EXTENDED_SECURITY) {
        mntopts |= FSESS_EXTENDED_SECURITY;
        vfs_setextendedsecurity(mp);
    }

    if (fusefs_args.altflags & FUSE_MOPT_LOCALVOL) {
        mntopts |= FSESS_LOCALVOL;
        vfs_setflags(mp, MNT_LOCAL);
    }

    if (fusefs_args.altflags & FUSE_MOPT_EXCL_CREATE) {
        mntopts |= FSESS_EXCL_CREATE;
    }

    /* done checking incoming option bits */

    err = 0;

    vfs_setfsprivate(mp, NULL);

    fdev = fuse_device_get(fusefs_args.rdev);
    if (!fdev) {
        return EINVAL;
    }

    fuse_device_lock(fdev);

    drandom = fuse_device_get_random(fdev);
    if (fusefs_args.random != drandom) {
        fuse_device_unlock(fdev);
        IOLog("osxfuse: failing mount because of mismatched random\n");
        return EINVAL;
    }

    data = fuse_device_get_mpdata(fdev);

    if (!data) {
        fuse_device_unlock(fdev);
        return ENXIO;
    }

#if M_OSXFUSE_ENABLE_BIG_LOCK
    biglock = data->biglock;
    fuse_biglock_lock(biglock);
#endif

    if (data->mount_state != FM_NOTMOUNTED) {
#if M_OSXFUSE_ENABLE_BIG_LOCK
        fuse_biglock_unlock(biglock);
#endif
        fuse_device_unlock(fdev);
        return EALREADY;
    }

    if (!(data->dataflags & FSESS_OPENED)) {
        fuse_device_unlock(fdev);
        err = ENXIO;
        goto out;
    }

    data->mount_state = FM_MOUNTED;
    OSAddAtomic(1, (SInt32 *)&fuse_mount_count);
    mounted = true;

    if (fdata_dead_get(data)) {
        fuse_device_unlock(fdev);
        err = ENOTCONN;
        goto out;
    }

    if (!data->daemoncred) {
        panic("osxfuse: daemon found but identity unknown");
    }

    if (fuse_vfs_context_issuser(context) &&
        kauth_cred_getuid(vfs_context_ucred(context)) != kauth_cred_getuid(data->daemoncred)) {
        fuse_device_unlock(fdev);
        err = EPERM;
        goto out;
    }

    data->mp = mp;
    data->fdev = fdev;
    data->dataflags |= mntopts;

    data->daemon_timeout.tv_sec =  fusefs_args.daemon_timeout;
    data->daemon_timeout.tv_nsec = 0;
    if (data->daemon_timeout.tv_sec) {
        data->daemon_timeout_p = &(data->daemon_timeout);
    } else {
        data->daemon_timeout_p = NULL;
    }

    data->max_read = max_read;
    data->fssubtype = fusefs_args.fssubtype;
    data->mountaltflags = fusefs_args.altflags;
    data->noimplflags = (uint64_t)0;

    data->blocksize = fuse_round_size(fusefs_args.blocksize,
                                      FUSE_MIN_BLOCKSIZE, FUSE_MAX_BLOCKSIZE);

    data->iosize = fuse_round_iosize(fusefs_args.iosize);

    if (data->iosize < data->blocksize) {
        data->iosize = data->blocksize;
    }

    data->userkernel_bufsize = FUSE_DEFAULT_USERKERNEL_BUFSIZE;

    copystr(fusefs_args.fsname, vfsstatfsp->f_mntfromname,
            MNAMELEN - 1, &len);
    bzero(vfsstatfsp->f_mntfromname + len, MNAMELEN - len);

    copystr(fusefs_args.volname, data->volname, MAXPATHLEN - 1, &len);
    bzero(data->volname + len, MAXPATHLEN - len);

    /* previous location of vfs_setioattr() */

    vfs_setfsprivate(mp, data);

    fuse_device_unlock(fdev);

    /* Send a handshake message to the daemon. */
    fuse_internal_init(data, context);

out:
    if (err) {
        vfs_setfsprivate(mp, NULL);

        fuse_device_lock(fdev);
        data = fuse_device_get_mpdata(fdev); /* again */
        if (mounted) {
            OSAddAtomic(-1, (SInt32 *)&fuse_mount_count);
        }
        if (data) {
            data->mount_state = FM_NOTMOUNTED;
            if (!(data->dataflags & FSESS_OPENED)) {
#if M_OSXFUSE_ENABLE_BIG_LOCK
                assert(biglock == data->biglock);
                fuse_biglock_unlock(biglock);
#endif
                fuse_device_close_final(fdev);
                /* data is gone now */
            }
        }
        fuse_device_unlock(fdev);
    } else {
        vnode_t fuse_rootvp = NULLVP;
        err = fuse_vfsop_root(mp, &fuse_rootvp, context);
        if (err) {
            goto out; /* go back and follow error path */
        }
        err = vnode_ref(fuse_rootvp);
#if M_OSXFUSE_ENABLE_BIG_LOCK
        /*
         * Even though fuse_rootvp will not be reclaimed when calling vnode_put
         * because we incremented its usecount by calling vnode_ref release
         * biglock just to be safe.
         */
        fuse_biglock_unlock(biglock);
#endif /* M_OSXFUSE_ENABLE_BIG_LOCK */
        (void)vnode_put(fuse_rootvp);
#if M_OSXFUSE_ENABLE_BIG_LOCK
        fuse_biglock_lock(biglock);
#endif
        if (err) {
            goto out; /* go back and follow error path */
        } else {
            struct vfsioattr ioattr;

            vfs_ioattr(mp, &ioattr);
            ioattr.io_maxreadcnt = ioattr.io_maxwritecnt = data->iosize;
            ioattr.io_segreadcnt = ioattr.io_segwritecnt = data->iosize / PAGE_SIZE;
            ioattr.io_maxsegreadsize = ioattr.io_maxsegwritesize = data->iosize;
            ioattr.io_devblocksize = data->blocksize;
            vfs_setioattr(mp, &ioattr);
        }
    }

#if M_OSXFUSE_ENABLE_BIG_LOCK
    fuse_device_lock(fdev);
    data = fuse_device_get_mpdata(fdev); /* ...and again */
    if (data) {
        assert(data->biglock == biglock);
        fuse_biglock_unlock(biglock);
    }
    fuse_device_unlock(fdev);
#endif

    return err;
}

static errno_t
fuse_vfsop_unmount(mount_t mp, int mntflags, vfs_context_t context)
{
    int   err        = 0;
    int   flags      = 0;

    fuse_device_t          fdev;
    struct fuse_data      *data;
    struct fuse_dispatcher fdi;

    vnode_t fuse_rootvp = NULLVP;

    fuse_trace_printf_vfsop();

    if (mntflags & MNT_FORCE) {
        flags |= FORCECLOSE;
    }

    data = fuse_get_mpdata(mp);
    if (!data) {
        panic("osxfuse: no mount private data in vfs_unmount");
    }

#if M_OSXFUSE_ENABLE_BIG_LOCK
    fuse_biglock_lock(data->biglock);
#endif

    fdev = data->fdev;

    /*
     * Set mount state to FM_UNMOUNTING.
     *
     * Note: fdata_set_dead will signal VQ_DEAD if it is called for a volume,
     * that is still mounted.
     */
    fuse_device_lock(fdev);
    data->mount_state = FM_UNMOUNTING;
    fuse_device_unlock(fdev);

    if (fdata_dead_get(data)) {

        /*
         * If the file system daemon is dead, it's pointless to try to do
         * any unmount-time operations that go out to user space. Therefore,
         * we pretend that this is a force unmount. However, this isn't of much
         * use. That's because if any non-root vnode is in use, the vflush()
         * that the kernel does before calling our VFS_UNMOUNT will fail
         * if the original unmount wasn't forcible already. That earlier
         * vflush is called with SKIPROOT though, so it wouldn't bail out
         * on the root vnode being in use.
         *
         * If we want, we could set FORCECLOSE here so that a non-forced
         * unmount will be "upgraded" to a forced unmount if the root vnode
         * is busy (you are cd'd to the mount point, for example). It's not
         * quite pure to do that though.
         *
         *    flags |= FORCECLOSE;
         *    IOLog("osxfuse: forcing unmount on a dead file system\n");
         */

    } else if (!(data->dataflags & FSESS_INITED)) {
        flags |= FORCECLOSE;
        IOLog("osxfuse: forcing unmount on not-yet-alive file system\n");
        fdata_set_dead(data, false);
    }

    fuse_rootvp = data->rootvp;

#if M_OSXFUSE_ENABLE_BIG_LOCK
    fuse_biglock_unlock(data->biglock);
#endif
    err = vflush(mp, fuse_rootvp, flags);
#if M_OSXFUSE_ENABLE_BIG_LOCK
    fuse_biglock_lock(data->biglock);
#endif
    if (err) {
#if M_OSXFUSE_ENABLE_BIG_LOCK
        fuse_biglock_unlock(data->biglock);
#endif
        return err;
    }

    if (vnode_isinuse(fuse_rootvp, 1) && !(flags & FORCECLOSE)) {
#if M_OSXFUSE_ENABLE_BIG_LOCK
        fuse_biglock_unlock(data->biglock);
#endif
        return EBUSY;
    }

    data->rootvp = NULLVP;

#if M_OSXFUSE_ENABLE_BIG_LOCK
    fuse_biglock_unlock(data->biglock);
#endif
    vnode_rele(fuse_rootvp); /* We got this reference in fuse_vfsop_mount(). */
#if M_OSXFUSE_ENABLE_BIG_LOCK
    fuse_biglock_lock(data->biglock);
#endif

#if M_OSXFUSE_ENABLE_BIG_LOCK
    fuse_biglock_unlock(data->biglock);
#endif
    (void)vflush(mp, NULLVP, FORCECLOSE);
#if M_OSXFUSE_ENABLE_BIG_LOCK
    fuse_biglock_lock(data->biglock);
#endif

    if (!fdata_dead_get(data)) {
        fdisp_init(&fdi, 0 /* no data to send along */);
        fdisp_make(&fdi, FUSE_DESTROY, mp, FUSE_ROOT_ID, context);

        err = fdisp_wait_answ(&fdi);
        if (!err) {
            fuse_ticket_release(fdi.tick);
        }

        /* Note that dounmount() signals a VQ_UNMOUNT VFS event */

        fdata_set_dead(data, false);
    }

    fuse_device_lock(fdev);

    vfs_setfsprivate(mp, NULL);
    data->mount_state = FM_NOTMOUNTED;
    OSAddAtomic(-1, (SInt32 *)&fuse_mount_count);

#if M_OSXFUSE_ENABLE_BIG_LOCK
    fuse_biglock_unlock(data->biglock);
#endif

    if (!(data->dataflags & FSESS_OPENED)) {

        /* fdev->data was left for us to clean up */

        fuse_device_close_final(fdev);

        /* fdev->data is gone now */
    }

    fuse_device_unlock(fdev);

    return 0;
}

static errno_t
fuse_vfsop_root(mount_t mp, struct vnode **vpp, vfs_context_t context)
{
    int err = 0;
    vnode_t vp = NULLVP;
    struct fuse_entry_out  feo_root_data;
    struct fuse_abi_data   feo_root;
    struct fuse_abi_data   fa;
    struct fuse_data      *data = fuse_get_mpdata(mp);

    fuse_trace_printf_vfsop();

    if (data->rootvp != NULLVP) {
        *vpp = data->rootvp;
        return vnode_get(*vpp);
    }

    bzero(&feo_root_data, sizeof(feo_root_data));

    fuse_abi_data_init(&feo_root, FUSE_ABI_VERSION_MAX, &feo_root_data);
    fuse_abi_data_init(&fa, feo_root.fad_version, fuse_entry_out_get_attr(&feo_root));

    fuse_entry_out_set_nodeid(&feo_root, FUSE_ROOT_ID);
    fuse_entry_out_set_generation(&feo_root, 0);

    fuse_attr_set_ino(&fa, FUSE_ROOT_ID);
    fuse_attr_set_size(&fa, FUSE_ROOT_SIZE);
    fuse_attr_set_mode(&fa, VTTOIF(VDIR));

    err = FSNodeGetOrCreateFileVNodeByID(&vp, FN_IS_ROOT, &feo_root, mp,
                                         NULLVP /* dvp */, context,
                                         NULL /* oflags */);
    *vpp = vp;

    if (!err) {
        data->rootvp = *vpp;
    }

    return err;
}

static void
handle_capabilities_and_attributes(mount_t mp, struct vfs_attr *attr)
{

    struct fuse_data *data = fuse_get_mpdata(mp);
    if (!data) {
        return;
    }

    attr->f_capabilities.capabilities[VOL_CAPABILITIES_FORMAT] = 0
//      | VOL_CAP_FMT_PERSISTENTOBJECTIDS
        | VOL_CAP_FMT_SYMBOLICLINKS

        /*
         * Note that we don't really have hard links in a osxfuse file system
         * unless the user file system daemon provides persistent/consistent
         * inode numbers. Maybe instead of returning the "wrong" answer here
         * we should just deny knowledge of this capability in the valid bits
         * below.
         */
        | VOL_CAP_FMT_HARDLINKS
//      | VOL_CAP_FMT_JOURNAL
//      | VOL_CAP_FMT_JOURNAL_ACTIVE
        | VOL_CAP_FMT_NO_ROOT_TIMES
        | VOL_CAP_FMT_SPARSE_FILES
//      | VOL_CAP_FMT_ZERO_RUNS
        | VOL_CAP_FMT_CASE_SENSITIVE
        | VOL_CAP_FMT_CASE_PRESERVING
        | VOL_CAP_FMT_FAST_STATFS
        | VOL_CAP_FMT_2TB_FILESIZE
//      | VOL_CAP_FMT_OPENDENYMODES
        | VOL_CAP_FMT_HIDDEN_FILES
//      | VOL_CAP_FMT_PATH_FROM_ID
//      | VOL_CAP_FMT_NO_VOLUME_SIZES
//      | VOL_CAP_FMT_DECMPFS_COMPRESSION
//      | VOL_CAP_FMT_64BIT_OBJECT_IDS
#if VERSION_MAJOR >= 16
//      | VOL_CAP_FMT_DIR_HARDLINKS
//      | VOL_CAP_FMT_DOCUMENT_ID
//      | VOL_CAP_FMT_WRITE_GENERATION_COUNT
//      | VOL_CAP_FMT_NO_IMMUTABLE_FILES
//      | VOL_CAP_FMT_NO_PERMISSIONS
#endif
        ;

    if (data->dataflags & FSESS_CASE_INSENSITIVE) {
        attr->f_capabilities.capabilities[VOL_CAPABILITIES_FORMAT] &=
            ~VOL_CAP_FMT_CASE_SENSITIVE;
    }

    if (data->dataflags & FSESS_SLOW_STATFS) {
        attr->f_capabilities.capabilities[VOL_CAPABILITIES_FORMAT] &=
            ~VOL_CAP_FMT_FAST_STATFS;
    }

    attr->f_capabilities.valid[VOL_CAPABILITIES_FORMAT] = 0
        | VOL_CAP_FMT_PERSISTENTOBJECTIDS
        | VOL_CAP_FMT_SYMBOLICLINKS
        | VOL_CAP_FMT_HARDLINKS
        | VOL_CAP_FMT_JOURNAL
        | VOL_CAP_FMT_JOURNAL_ACTIVE
        | VOL_CAP_FMT_NO_ROOT_TIMES
        | VOL_CAP_FMT_SPARSE_FILES
        | VOL_CAP_FMT_ZERO_RUNS
        | VOL_CAP_FMT_CASE_SENSITIVE
        | VOL_CAP_FMT_CASE_PRESERVING
        | VOL_CAP_FMT_FAST_STATFS
        | VOL_CAP_FMT_2TB_FILESIZE
        | VOL_CAP_FMT_OPENDENYMODES
        | VOL_CAP_FMT_HIDDEN_FILES
        | VOL_CAP_FMT_PATH_FROM_ID
        | VOL_CAP_FMT_NO_VOLUME_SIZES
        | VOL_CAP_FMT_DECMPFS_COMPRESSION
        | VOL_CAP_FMT_64BIT_OBJECT_IDS
#if VERSION_MAJOR >= 16
        | VOL_CAP_FMT_DIR_HARDLINKS
        | VOL_CAP_FMT_DOCUMENT_ID
        | VOL_CAP_FMT_WRITE_GENERATION_COUNT
        | VOL_CAP_FMT_NO_IMMUTABLE_FILES
        | VOL_CAP_FMT_NO_PERMISSIONS
#endif
        ;

    attr->f_capabilities.capabilities[VOL_CAPABILITIES_INTERFACES] = 0
//      | VOL_CAP_INT_SEARCHFS
        | VOL_CAP_INT_ATTRLIST
//      | VOL_CAP_INT_NFSEXPORT
//      | VOL_CAP_INT_READDIRATTR
//      | VOL_CAP_INT_EXCHANGEDATA
//      | VOL_CAP_INT_COPYFILE
//      | VOL_CAP_INT_ALLOCATE
//      | VOL_CAP_INT_VOL_RENAME
        | VOL_CAP_INT_ADVLOCK
        | VOL_CAP_INT_FLOCK
        | VOL_CAP_INT_EXTENDED_SECURITY
//      | VOL_CAP_INT_USERACCESS
//      | VOL_CAP_INT_MANLOCK
//      | VOL_CAP_INT_NAMEDSTREAMS
//      | VOL_CAP_INT_EXTENDED_ATTR
#if VERSION_MAJOR >= 16
//      | VOL_CAP_INT_CLONE
//      | VOL_CAP_INT_RENAME_SWAP
//      | VOL_CAP_INT_RENAME_EXCL
#endif
        ;

    /*
     * Don't set the EXCHANGEDATA capability if it's known not to be
     * implemented in the FUSE daemon.
     */
    if (fuse_implemented(data, FSESS_NOIMPLBIT(EXCHANGE))) {
        attr->f_capabilities.capabilities[VOL_CAPABILITIES_INTERFACES] |=
            VOL_CAP_INT_EXCHANGEDATA;
    }

    /*
     * Don't set the ALLOCATE capability if it's known not to be implemented
     * in the FUSE daemon.
     */
    if (fuse_implemented(data, FSESS_NOIMPLBIT(FALLOCATE))) {
        attr->f_capabilities.capabilities[VOL_CAPABILITIES_INTERFACES] |=
            VOL_CAP_INT_ALLOCATE;
    }

    if (fuse_implemented(data, FSESS_NOIMPLBIT(SETVOLNAME))) {
        attr->f_capabilities.capabilities[VOL_CAPABILITIES_INTERFACES] |=
            VOL_CAP_INT_VOL_RENAME;
    }

    if (data->dataflags & FSESS_XTIMES) {
        attr->f_attributes.validattr.commonattr |=
            (ATTR_CMN_BKUPTIME | ATTR_CMN_CHGTIME | ATTR_CMN_CRTIME);
    }

    if (data->dataflags & FSESS_NATIVE_XATTR) {
        attr->f_capabilities.capabilities[VOL_CAPABILITIES_INTERFACES] |=
            VOL_CAP_INT_EXTENDED_ATTR;
    }

    attr->f_capabilities.valid[VOL_CAPABILITIES_INTERFACES] = 0
        | VOL_CAP_INT_SEARCHFS
        | VOL_CAP_INT_ATTRLIST
        | VOL_CAP_INT_NFSEXPORT
        | VOL_CAP_INT_READDIRATTR
        | VOL_CAP_INT_EXCHANGEDATA
        | VOL_CAP_INT_COPYFILE
        | VOL_CAP_INT_ALLOCATE
        | VOL_CAP_INT_VOL_RENAME
        | VOL_CAP_INT_ADVLOCK
        | VOL_CAP_INT_FLOCK
        | VOL_CAP_INT_EXTENDED_SECURITY
        | VOL_CAP_INT_USERACCESS
        | VOL_CAP_INT_MANLOCK
        | VOL_CAP_INT_NAMEDSTREAMS
        | VOL_CAP_INT_EXTENDED_ATTR
#if VERSION_MAJOR >= 16
        | VOL_CAP_INT_CLONE
        | VOL_CAP_INT_RENAME_SWAP
        | VOL_CAP_INT_RENAME_EXCL
#endif
        ;

    attr->f_capabilities.capabilities[VOL_CAPABILITIES_RESERVED1] = 0;
    attr->f_capabilities.valid[VOL_CAPABILITIES_RESERVED1] = 0;
    attr->f_capabilities.capabilities[VOL_CAPABILITIES_RESERVED2] = 0;
    attr->f_capabilities.valid[VOL_CAPABILITIES_RESERVED2] = 0;
    VFSATTR_SET_SUPPORTED(attr, f_capabilities);

    attr->f_attributes.validattr.commonattr = 0
        | ATTR_CMN_NAME
        | ATTR_CMN_DEVID
        | ATTR_CMN_FSID
        | ATTR_CMN_OBJTYPE
//      | ATTR_CMN_OBJTAG
        | ATTR_CMN_OBJID
//      | ATTR_CMN_OBJPERMANENTID
        | ATTR_CMN_PAROBJID
//      | ATTR_CMN_SCRIPT
//      | ATTR_CMN_CRTIME
        | ATTR_CMN_MODTIME
//      | ATTR_CMN_CHGTIME
//      | ATTR_CMN_ACCTIME
//      | ATTR_CMN_BKUPTIME
//      | ATTR_CMN_FNDRINFO
        | ATTR_CMN_OWNERID
        | ATTR_CMN_GRPID
        | ATTR_CMN_ACCESSMASK
        | ATTR_CMN_FLAGS
//      | ATTR_CMN_USERACCESS
        | ATTR_CMN_EXTENDED_SECURITY
//      | ATTR_CMN_UUID
//      | ATTR_CMN_GRPUUID
//      | ATTR_CMN_FILEID
//      | ATTR_CMN_PARENTID
//      | ATTR_CMN_FULLPATH
//      | ATTR_CMN_ADDEDTIME
#if VERSION_MAJOR >= 14
//      | ATTR_CMN_ERROR
//      | ATTR_CMN_DATA_PROTECT_FLAGS
#endif
        ;

    attr->f_attributes.validattr.volattr = 0
        | ATTR_VOL_FSTYPE
        | ATTR_VOL_SIGNATURE
        | ATTR_VOL_SIZE
        | ATTR_VOL_SPACEFREE
        | ATTR_VOL_SPACEAVAIL
//      | ATTR_VOL_MINALLOCATION
//      | ATTR_VOL_ALLOCATIONCLUMP
        | ATTR_VOL_IOBLOCKSIZE
//      | ATTR_VOL_OBJCOUNT
        | ATTR_VOL_FILECOUNT
//      | ATTR_VOL_DIRCOUNT
//      | ATTR_VOL_MAXOBJCOUNT
        | ATTR_VOL_MOUNTPOINT
        | ATTR_VOL_NAME
        | ATTR_VOL_MOUNTFLAGS
        | ATTR_VOL_MOUNTEDDEVICE
//      | ATTR_VOL_ENCODINGSUSED
        | ATTR_VOL_CAPABILITIES
        | ATTR_VOL_ATTRIBUTES
//      | ATTR_VOL_INFO
        ;
    attr->f_attributes.validattr.dirattr = 0
        | ATTR_DIR_LINKCOUNT
//      | ATTR_DIR_ENTRYCOUNT
//      | ATTR_DIR_MOUNTSTATUS
#if VERSION_MAJOR >= 16
//      | ATTR_DIR_ALLOCSIZE
//      | ATTR_DIR_IOBLOCKSIZE
//      | ATTR_DIR_DATALENGTH
#endif
        ;
    attr->f_attributes.validattr.fileattr = 0
        | ATTR_FILE_LINKCOUNT
        | ATTR_FILE_TOTALSIZE
        | ATTR_FILE_ALLOCSIZE
        | ATTR_FILE_IOBLOCKSIZE
        | ATTR_FILE_DEVTYPE
//      | ATTR_FILE_FORKCOUNT
//      | ATTR_FILE_FORKLIST
        | ATTR_FILE_DATALENGTH
        | ATTR_FILE_DATAALLOCSIZE
//      | ATTR_FILE_RSRCLENGTH
//      | ATTR_FILE_RSRCALLOCSIZE
        ;

    attr->f_attributes.validattr.forkattr = 0;
//      | ATTR_FORK_TOTALSIZE
//      | ATTR_FORK_ALLOCSIZE
        ;

    // All attributes that we do support, we support natively.

    attr->f_attributes.nativeattr.commonattr = \
        attr->f_attributes.validattr.commonattr;
    attr->f_attributes.nativeattr.volattr    = \
        attr->f_attributes.validattr.volattr;
    attr->f_attributes.nativeattr.dirattr    = \
        attr->f_attributes.validattr.dirattr;
    attr->f_attributes.nativeattr.fileattr   = \
        attr->f_attributes.validattr.fileattr;
    attr->f_attributes.nativeattr.forkattr   = \
        attr->f_attributes.validattr.forkattr;

    VFSATTR_SET_SUPPORTED(attr, f_attributes);
}

static errno_t
fuse_vfsop_getattr(mount_t mp, struct vfs_attr *attr, vfs_context_t context)
{
    int err = 0;
    bool deading = false;
    bool faking = false;

    struct fuse_dispatcher  fdi;
    struct fuse_data       *data;

    uint64_t blocks;
	uint64_t bfree;
	uint64_t bavail;
	uint64_t files;
	uint64_t ffree;
	uint32_t bsize;
	uint32_t frsize;

    fuse_trace_printf_vfsop();

    data = fuse_get_mpdata(mp);
    if (!data) {
        panic("osxfuse: no private data for mount point?");
    }

    if (!(data->dataflags & FSESS_INITED)) {
        /*
         * Note: coreservicesd requests ATTR_VOL_CAPABILITIES on the mount point
         * right before returning from mount(2). We need to fake the output
         * because the FUSE server might not be ready to respond yet.
         */
        faking = true;
        goto dostatfs;
    }

    fdisp_init(&fdi, 0);
    fdisp_make(&fdi, FUSE_STATFS, mp, FUSE_ROOT_ID, context);
    err = fdisp_wait_answ(&fdi);
    if (err) {
        /*
         * If we cannot communicate with the daemon (most likely because it's
         * dead), we still want to portray that we are a bonafide file system so
         * that we can be gracefully unmounted.
         */
        if (err == ENOTCONN) {
            deading = true;
            faking = true;
            goto dostatfs;
        }

        return err;
    }

dostatfs:
    if (faking) {
        blocks = 0;
        bfree = 0;
        bavail = 0;
        files = 0;
        ffree = 0;
        bsize = 0;
        frsize = 0;

    } else {
        struct fuse_abi_data fsfo;
        struct fuse_abi_data fksf;

        fuse_abi_data_init(&fsfo, DATOI(data), fdi.answ);
        fuse_abi_data_init(&fksf, DATOI(data), fuse_statfs_out_get_st(&fsfo));

        blocks = fuse_kstatfs_get_blocks(&fksf);
        bfree = fuse_kstatfs_get_bfree(&fksf);
        bavail = fuse_kstatfs_get_bavail(&fksf);
        files = fuse_kstatfs_get_files(&fksf);
        ffree = fuse_kstatfs_get_ffree(&fksf);
        bsize = fuse_kstatfs_get_bsize(&fksf);
        frsize = fuse_kstatfs_get_frsize(&fksf);
    }

    if (bsize == 0) {
        bsize = FUSE_DEFAULT_IOSIZE;
    }

    if (frsize == 0) {
        frsize = FUSE_DEFAULT_BLOCKSIZE;
    }

    /* optimal transfer block size; will go into f_iosize in the kernel */
    bsize = fuse_round_iosize(bsize);

    /* file system fragment size; will go into f_bsize in the kernel */
    frsize  = fuse_round_size(frsize, FUSE_MIN_BLOCKSIZE, FUSE_MAX_BLOCKSIZE);

    /* We must have: f_iosize >= f_bsize (fsfo.st.bsize >= fsfo.st_frsize) */
    if (bsize < frsize) {
        bsize = frsize;
    }

    /*
     * TBD: Possibility:
     *
     * For actual I/O to OSXFUSE's "virtual" storage device, we use
     * data->blocksize and data->iosize. These are really meant to be
     * constant across the lifetime of a single mount. If necessary, we
     * can experiment by updating the mount point's stat with the frsize
     * and bsize values we come across here.
     */

    /*
     * FUSE server will (might) give us this:
     *
     * __u64   blocks;  // total data blocks in the file system
     * __u64   bfree;   // free blocks in the file system
     * __u64   bavail;  // free blocks available to non-superuser
     * __u64   files;   // total file nodes in the file system
     * __u64   ffree;   // free file nodes in the file system
     * __u32   bsize;   // preferred/optimal file system block size
     * __u32   namelen; // maximum length of filenames
     * __u32   frsize;  // fundamental file system block size
     *
     * On macOS, we will map this data to struct vfs_attr as follows:
     *
     *  macOS                        FUSE
     *  -----                        ----
     *  uint64_t f_supported   <-    // handled here
     *  uint64_t f_active      <-    // handled here
     *  uint64_t f_objcount    <-    -
     *  uint64_t f_filecount   <-    files
     *  uint64_t f_dircount    <-    -
     *  uint32_t f_bsize       <-    frsize
     *  size_t   f_iosize      <-    bsize
     *  uint64_t f_blocks      <-    blocks
     *  uint64_t f_bfree       <-    bfree
     *  uint64_t f_bavail      <-    bavail
     *  uint64_t f_bused       <-    blocks - bfree
     *  uint64_t f_files       <-    files
     *  uint64_t f_ffree       <-    ffree
     *  fsid_t   f_fsid        <-    // handled elsewhere
     *  uid_t    f_owner       <-    // handled elsewhere
     *  ... capabilities       <-    // handled here
     *  ... attributes         <-    // handled here
     *  f_create_time          <-    -
     *  f_modify_time          <-    -
     *  f_access_time          <-    -
     *  f_backup_time          <-    -
     *  uint32_t f_fssubtype   <-    // daemon provides
     *  char *f_vol_name       <-    // handled here
     *  uint16_t f_signature   <-    // handled here
     *  uint16_t f_carbon_fsid <-    // handled here
     */

    VFSATTR_RETURN(attr, f_filecount, files);
    VFSATTR_RETURN(attr, f_bsize, frsize);
    VFSATTR_RETURN(attr, f_iosize, bsize);
    VFSATTR_RETURN(attr, f_blocks, blocks);
    VFSATTR_RETURN(attr, f_bfree, bfree);
    VFSATTR_RETURN(attr, f_bavail, bavail);
    VFSATTR_RETURN(attr, f_bused, (blocks - bfree));
    VFSATTR_RETURN(attr, f_files, files);
    VFSATTR_RETURN(attr, f_ffree, ffree);

    /* f_fsid and f_owner handled elsewhere. */

    /* Handle capabilities and attributes. */
    if (VFSATTR_IS_ACTIVE(attr, f_capabilities)
        || VFSATTR_IS_ACTIVE(attr, f_attributes)) {
        handle_capabilities_and_attributes(mp, attr);
    }

    VFSATTR_RETURN(attr, f_create_time, kZeroTime);
    VFSATTR_RETURN(attr, f_modify_time, kZeroTime);
    VFSATTR_RETURN(attr, f_access_time, kZeroTime);
    VFSATTR_RETURN(attr, f_backup_time, kZeroTime);

    if (deading) {
        VFSATTR_RETURN(attr, f_fssubtype, (uint32_t)FUSE_FSSUBTYPE_INVALID);
    } else {
        VFSATTR_RETURN(attr, f_fssubtype, data->fssubtype);
    }

    /* Daemon needs to pass this. */
    if (VFSATTR_IS_ACTIVE(attr, f_vol_name)) {
        if (data->volname[0] != 0) {
            strncpy(attr->f_vol_name, data->volname, MAXPATHLEN);
            attr->f_vol_name[MAXPATHLEN - 1] = '\0';
            VFSATTR_SET_SUPPORTED(attr, f_vol_name);
        }
    }

    VFSATTR_RETURN(attr, f_signature, OSSwapBigToHostInt16(FUSEFS_SIGNATURE));
    VFSATTR_RETURN(attr, f_carbon_fsid, 0);

    if (!faking) {
        fuse_ticket_release(fdi.tick);
    }

    return 0;
}

struct fuse_sync_cargs {
    vfs_context_t context;
    int waitfor;
    int error;
};

static int
fuse_sync_callback(vnode_t vp, void *cargs)
{
    int type;
    struct fuse_sync_cargs *args;
    struct fuse_vnode_data *fvdat;
    struct fuse_filehandle *fufh;
    struct fuse_data       *data;
    mount_t mp;

    if (!vnode_hasdirtyblks(vp)) {
        return VNODE_RETURNED;
    }

    mp = vnode_mount(vp);

    if (fuse_isdeadfs_mp(mp)) {
        return VNODE_RETURNED_DONE;
    }

    data = fuse_get_mpdata(mp);

    if (!fuse_implemented(data, (vnode_isdir(vp)) ?
        FSESS_NOIMPLBIT(FSYNCDIR) : FSESS_NOIMPLBIT(FSYNC))) {
        return VNODE_RETURNED;
    }

    args = (struct fuse_sync_cargs *)cargs;
    fvdat = VTOFUD(vp);

    cluster_push(vp, 0);

    for (type = 0; type < FUFH_MAXTYPE; type++) {
        fufh = &(fvdat->fufh[type]);
        if (FUFH_IS_VALID(fufh)) {
            (void)fuse_internal_fsync_fh(vp, args->context, fufh,
                                         FUSE_OP_FOREGROUNDED);
        }
    }

    /*
     * In general:
     *
     * - can use vnode_isinuse() if the need be
     * - vnode and UBC are in lock-step
     * - note that umount will call ubc_sync_range()
     */

    return VNODE_RETURNED;
}

static errno_t
fuse_vfsop_sync(mount_t mp, int waitfor, vfs_context_t context)
{
    struct fuse_sync_cargs args;
    int allerror = 0;

    fuse_trace_printf_vfsop();

    if (fuse_isdeadfs_mp(mp)) {
        return 0;
    }

    if (vfs_isupdate(mp)) {
        return 0;
    }

    if (vfs_isrdonly(mp)) {
        return EROFS; // should panic!?
    }

    /*
     * Write back each (modified) fuse node.
     */
    args.context = context;
    args.waitfor = waitfor;
    args.error = 0;

#if M_OSXFUSE_ENABLE_BIG_LOCK
    struct fuse_data *data = fuse_get_mpdata(mp);
    fuse_biglock_unlock(data->biglock);
#endif
    vnode_iterate(mp, 0, fuse_sync_callback, (void *)&args);
#if M_OSXFUSE_ENABLE_BIG_LOCK
    fuse_biglock_lock(data->biglock);
#endif

    if (args.error) {
        allerror = args.error;
    }

    /*
     * For other types of stale file system information, such as:
     *
     * - fs control info
     * - quota information
     * - modified superblock
     */

    return allerror;
}

static errno_t
fuse_vfsop_setattr(mount_t mp, struct vfs_attr *fsap, vfs_context_t context)
{
    int error = 0;

    fuse_trace_printf_vfsop();

    kauth_cred_t cred = vfs_context_ucred(context);

    if (!fuse_vfs_context_issuser(context) &&
        (kauth_cred_getuid(cred) != vfs_statfs(mp)->f_owner)) {
        return EACCES;
    }

    struct fuse_data *data = fuse_get_mpdata(mp);

    if (VFSATTR_IS_ACTIVE(fsap, f_vol_name)) {
        if (!fuse_implemented(data, FSESS_NOIMPLBIT(SETVOLNAME))) {
            error = ENOTSUP;
            goto out;
        }

        if (fsap->f_vol_name[0] == 0) {
            error = EINVAL;
            goto out;
        }

        size_t namelen = strlen(fsap->f_vol_name);
        if (namelen >= MAXPATHLEN) {
            error = ENAMETOOLONG;
            goto out;
        }

        vnode_t root_vp;

        error = fuse_vfsop_root(mp, &root_vp, context);
        if (error) {
            goto out;
        }

        struct fuse_dispatcher fdi;
        fdisp_init(&fdi, namelen + 1);
        fdisp_make_vp(&fdi, FUSE_SETVOLNAME, root_vp, context);
        memcpy((char *)fdi.indata, fsap->f_vol_name, namelen);
        ((char *)fdi.indata)[namelen] = '\0';

        error = fdisp_wait_answ(&fdi);
        if (!error) {
            fuse_ticket_release(fdi.tick);
        }

#if M_OSXFUSE_ENABLE_BIG_LOCK
        fuse_biglock_unlock(data->biglock);
#endif
        (void)vnode_put(root_vp);
#if M_OSXFUSE_ENABLE_BIG_LOCK
        fuse_biglock_lock(data->biglock);
#endif

        if (error) {
            if (error == ENOSYS) {
                error = ENOTSUP;
                fuse_clear_implemented(data, FSESS_NOIMPLBIT(SETVOLNAME));
            }
            goto out;
        }

        copystr(fsap->f_vol_name, data->volname, MAXPATHLEN - 1, &namelen);
        bzero(data->volname + namelen, MAXPATHLEN - namelen);

        VFSATTR_SET_SUPPORTED(fsap, f_vol_name);
    }

out:
    return error;
}

__private_extern__
int
fuse_setextendedsecurity(mount_t mp, int state)
{
    int err = EINVAL;
    struct fuse_data *data;

    data = fuse_get_mpdata(mp);

    if (!data) {
        return ENXIO;
    }

    if (state == 1) {
        /* Turning on extended security. */
        if ((data->dataflags & FSESS_NO_VNCACHE) ||
            (data->dataflags & FSESS_DEFER_PERMISSIONS)) {
            return EINVAL;
        }
        data->dataflags |= (FSESS_EXTENDED_SECURITY |
                            FSESS_DEFAULT_PERMISSIONS);;
        if (vfs_authopaque(mp)) {
            vfs_clearauthopaque(mp);
        }
        if (vfs_authopaqueaccess(mp)) {
            vfs_clearauthopaqueaccess(mp);
        }
        vfs_setextendedsecurity(mp);
        err = 0;
    } else if (state == 0) {
        /* Turning off extended security. */
        data->dataflags &= ~FSESS_EXTENDED_SECURITY;
        vfs_clearextendedsecurity(mp);
        err = 0;
    }

    return err;
}
#if M_OSXFUSE_ENABLE_INTERIM_FSNODE_LOCK

static errno_t
fuse_vfsop_biglock_mount(mount_t mp, vnode_t devvp, user_addr_t udata,
                 vfs_context_t context)
{
    errno_t res;

#if M_OSXFUSE_ENABLE_HUGE_LOCK
    fuse_hugelock_lock();
#endif /* M_OSXFUSE_ENABLE_HUGE_LOCK */

    res = fuse_vfsop_mount(mp, devvp, udata, context);

#if M_OSXFUSE_ENABLE_HUGE_LOCK
    fuse_hugelock_unlock();
#endif /* M_OSXFUSE_ENABLE_HUGE_LOCK */

    return res;
}

static errno_t
fuse_vfsop_biglock_unmount(mount_t mp, int mntflags, vfs_context_t context)
{
    errno_t res;

#if M_OSXFUSE_ENABLE_HUGE_LOCK
    fuse_hugelock_lock();
#endif /* M_OSXFUSE_ENABLE_HUGE_LOCK */

    res = fuse_vfsop_unmount(mp, mntflags, context);

#if M_OSXFUSE_ENABLE_HUGE_LOCK
    fuse_hugelock_unlock();
#endif /* M_OSXFUSE_ENABLE_HUGE_LOCK */

    return res;
}

static errno_t
fuse_vfsop_biglock_root(mount_t mp, struct vnode **vpp, vfs_context_t context)
{
    locked_vfsop(mp, fuse_vfsop_root, vpp, context);
}

static errno_t
fuse_vfsop_biglock_getattr(mount_t mp, struct vfs_attr *attr, vfs_context_t context)
{
    locked_vfsop(mp, fuse_vfsop_getattr, attr, context);
}

static errno_t
fuse_vfsop_biglock_sync(mount_t mp, int waitfor, vfs_context_t context)
{
    locked_vfsop(mp, fuse_vfsop_sync, waitfor, context);
}

static errno_t
fuse_vfsop_biglock_setattr(mount_t mp, struct vfs_attr *fsap, vfs_context_t context)
{
    locked_vfsop(mp, fuse_vfsop_setattr, fsap, context);
}

#endif /* M_OSXFUSE_ENABLE_INTERIM_FSNODE_LOCK */
