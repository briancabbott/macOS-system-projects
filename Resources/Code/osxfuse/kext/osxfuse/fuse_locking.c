/*
 * Copyright (c) 2006-2008 Amit Singh/Google Inc.
 * Copyright (c) 2010 Tuxera Inc.
 * Copyright (c) 2017 Benjamin Fleischer
 * All rights reserved.
 */

/*
 * Portions Copyright (c) 1999-2003 Apple Computer, Inc. All Rights Reserved.
 *
 * This file contains Original Code and/or Modifications of Original Code as
 * defined in and that are subject to the Apple Public Source License Version
 * 2.0 (the 'License'). You may not use this file except in compliance with
 * the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see
 * the License for the specific language governing rights and limitations
 * under the License.
 */

#include "fuse_locking.h"

#include "fuse_ipc.h"
#include "fuse_node.h"

#if M_OSXFUSE_ENABLE_TSLOCKING
#  include <sys/ubc.h>
#endif

#include <stdbool.h>

lck_attr_t     *fuse_lock_attr    = NULL;
lck_grp_attr_t *fuse_group_attr   = NULL;
lck_grp_t      *fuse_lock_group   = NULL;
lck_mtx_t      *fuse_device_mutex = NULL;

#if M_OSXFUSE_ENABLE_TSLOCKING

/*
 * Largely identical to HFS+ locking. Much of the code is from hfs_cnode.c.
 */

/*
 * Lock a fusenode.
 */
__private_extern__
int
fusefs_lock(fusenode_t cp, enum fusefslocktype locktype)
{
    int err = 0;
    void *thread = current_thread();

restart:
    if (locktype == FUSEFS_SHARED_LOCK) {
        lck_rw_lock_shared(cp->nodelock);
        cp->nodelockowner = FUSEFS_SHARED_OWNER;
    } else {
        lck_rw_lock_exclusive(cp->nodelock);
        cp->nodelockowner = thread;
    }

    /*
     * Skip nodes that no longer exist (were deleted).
     */
    if ((locktype != FUSEFS_FORCE_LOCK) && (cp->c_flag & C_NOEXISTS)) {
        fusefs_unlock(cp);
        err = ENOENT;
        goto out;
    }

    if (cp->flag & FN_GETATTR && cp->getattr_thread != thread) {
        fuse_lck_mtx_lock(cp->getattr_lock);
        fusefs_unlock(cp);

        while (true) {
            err = msleep(cp->getattr_thread, cp->getattr_lock, PDROP | PCATCH,
                         "fuse_getattr", NULL);
            if (err == 0) {
                goto restart;
            } else if (err == EINTR && fuse_kludge_thread_should_abort(thread)) {
                goto out;
            }
        }
    }

out:
    return err;
}

/*
 * Lock a pair of fusenodes.
 */
__private_extern__
int
fusefs_lockpair(fusenode_t cp1, fusenode_t cp2, enum fusefslocktype locktype)
{
    fusenode_t first, last;
    int error;

    /*
     * If cnodes match then just lock one.
     */
    if (cp1 == cp2) {
        return fusefs_lock(cp1, locktype);
    }

    /*
     * Lock in cnode parent-child order (if there is a relationship);
     * otherwise lock in cnode address order.
     */
    if ((cp1->vtype == VDIR) && (cp1->nodeid == cp2->parent_nodeid)) {
        first = cp1;
        last = cp2;
    } else if (cp1 < cp2) {
        first = cp1;
        last = cp2;
    } else {
        first = cp2;
        last = cp1;
    }

    if ( (error = fusefs_lock(first, locktype))) {
        return error;
    }

    if ( (error = fusefs_lock(last, locktype))) {
        fusefs_unlock(first);
        return error;
    }

    return 0;
}

/*
 * Check ordering of two fusenodes. Return true if they are are in-order.
 */
static int
fusefs_isordered(fusenode_t cp1, fusenode_t cp2)
{
    if (cp1 == cp2) {
        return 0;
    }

    if (cp1 == NULL || cp2 == (fusenode_t)0xffffffff) {
        return 1;
    }

    if (cp2 == NULL || cp1 == (fusenode_t)0xffffffff) {
        return 0;
    }

    if (cp1->nodeid == cp2->parent_nodeid) {
        return 1;  /* cp1 is the parent and should go first */
    }

    if (cp2->nodeid == cp1->parent_nodeid) {
        return 0;  /* cp1 is the child and should go last */
    }

    return (cp1 < cp2);  /* fall-back is to use address order */
}

/*
 * Acquire 4 fusenode locks.
 *   - locked in fusenode parent-child order (if there is a relationship)
 *     otherwise lock in fusenode address order (lesser address first).
 *   - all or none of the locks are taken
 *   - only one lock taken per fusenode (dup fusenodes are skipped)
 *   - some of the fusenode pointers may be null
 */
__private_extern__
int
fusefs_lockfour(fusenode_t cp1, fusenode_t cp2, fusenode_t cp3, fusenode_t cp4,
                enum fusefslocktype locktype)
{
    fusenode_t a[3];
    fusenode_t b[3];
    fusenode_t list[4];
    fusenode_t tmp;
    int i, j, k;
    int error;

    if (fusefs_isordered(cp1, cp2)) {
        a[0] = cp1; a[1] = cp2;
    } else {
        a[0] = cp2; a[1] = cp1;
    }

    if (fusefs_isordered(cp3, cp4)) {
        b[0] = cp3; b[1] = cp4;
    } else {
        b[0] = cp4; b[1] = cp3;
    }

    a[2] = (fusenode_t)0xffffffff;  /* sentinel value */
    b[2] = (fusenode_t)0xffffffff;  /* sentinel value */

    /*
     * Build the lock list, skipping over duplicates
     */
    for (i = 0, j = 0, k = 0; (i < 2 || j < 2); ) {
        tmp = fusefs_isordered(a[i], b[j]) ? a[i++] : b[j++];
        if (k == 0 || tmp != list[k-1])
            list[k++] = tmp;
    }

    /*
     * Now we can lock using list[0 - k].
     * Skip over NULL entries.
     */
    for (i = 0; i < k; ++i) {
        if (list[i])
            if ((error = fusefs_lock(list[i], locktype))) {
                /* Drop any locks we acquired. */
                while (--i >= 0) {
                    if (list[i])
                        fusefs_unlock(list[i]);
                }
                return error;
            }
    }

    return 0;
}

/*
 * Unlock a fusenode.
 */
__private_extern__
void
fusefs_unlock(fusenode_t cp)
{
    u_int32_t c_flag;
    vnode_t vp = NULLVP;
#if M_OSXFUSE_RSRC_FORK
    vnode_t rvp = NULLVP;
#endif

    c_flag = cp->c_flag;
    cp->c_flag &= ~(C_NEED_DVNODE_PUT | C_NEED_DATA_SETSIZE);
#if M_OSXFUSE_RSRC_FORK
    cp->c_flag &= ~(C_NEED_RVNODE_PUT | C_NEED_RSRC_SETSIZE);
#endif

    if (c_flag & (C_NEED_DVNODE_PUT | C_NEED_DATA_SETSIZE)) {
        vp = cp->vp;
    }

#if M_OSXFUSE_RSRC_FORK
    if (c_flag & (C_NEED_RVNODE_PUT | C_NEED_RSRC_SETSIZE)) {
        rvp = cp->c_rsrc_vp;
    }
#endif

    cp->nodelockowner = NULL;
    fusefs_lck_rw_done(cp->nodelock);

    /* Perform any vnode post processing after fusenode lock is dropped. */
    if (vp) {
        if (c_flag & C_NEED_DATA_SETSIZE) {
            ubc_setsize(vp, 0);
        }
        if (c_flag & C_NEED_DVNODE_PUT) {
            vnode_put(vp);
        }
    }

#if M_OSXFUSE_RSRC_FORK
    if (rvp) {
        if (c_flag & C_NEED_RSRC_SETSIZE) {
            ubc_setsize(rvp, 0);
        }
        if (c_flag & C_NEED_RVNODE_PUT) {
            vnode_put(rvp);
        }
    }
#endif
}

/*
 * Unlock a pair of fusenodes.
 */
__private_extern__
void
fusefs_unlockpair(fusenode_t cp1, fusenode_t cp2)
{
    fusefs_unlock(cp1);
    if (cp2 != cp1) {
        fusefs_unlock(cp2);
    }
}

/*
 * Unlock a group of fusenodes.
 */
__private_extern__
void
fusefs_unlockfour(fusenode_t cp1, fusenode_t cp2,
                  fusenode_t cp3, fusenode_t cp4)
{
    fusenode_t list[4];
    int i, k = 0;

    if (cp1) {
        fusefs_unlock(cp1);
        list[k++] = cp1;
    }

    if (cp2) {
        for (i = 0; i < k; ++i) {
            if (list[i] == cp2)
                goto skip1;
        }
        fusefs_unlock(cp2);
        list[k++] = cp2;
    }

skip1:
    if (cp3) {
        for (i = 0; i < k; ++i) {
            if (list[i] == cp3)
                goto skip2;
        }
        fusefs_unlock(cp3);
        list[k++] = cp3;
    }

skip2:
    if (cp4) {
        for (i = 0; i < k; ++i) {
            if (list[i] == cp4)
                return;
        }
        fusefs_unlock(cp4);
    }
}

/*
 * Protect a fusenode against truncation.
 *
 * Used mainly by read/write since they don't hold the fusenode lock across
 * calls to the cluster layer.
 *
 * The process doing a truncation must take the lock exclusive. The read/write
 * processes can take it non-exclusive.
 */
__private_extern__
void
fusefs_lock_truncate(fusenode_t cp, lck_rw_type_t lck_rw_type)
{
    if (cp->nodelockowner == current_thread()) {
        panic("osxfuse: fusefs_lock_truncate: cnode %p locked!", cp);
    }

    lck_rw_lock(cp->truncatelock, lck_rw_type);
}

__private_extern__
void
fusefs_unlock_truncate(fusenode_t cp)
{
    fusefs_lck_rw_done(cp->truncatelock);
}

#endif

#include <IOKit/IOLocks.h>

__private_extern__
void
fusefs_lck_rw_done(lck_rw_t *lock)
{
    IORWLockUnlock((IORWLock *)lock);
}

#if M_OSXFUSE_ENABLE_INTERIM_FSNODE_LOCK

/* Recursive lock used to lock the vfs functions awaiting more fine-grained
 * locking. Code was taken from IOLocks.cpp to imitate how an IORecursiveLock
 * behaves. */

struct _fusefs_recursive_lock {
    lck_mtx_t *mutex;
    lck_grp_t *group;
    thread_t thread;
    UInt32 count;
    UInt32 maxcount;
};

static fusefs_recursive_lock* fusefs_recursive_lock_alloc_internal(
        lck_grp_t *lock_group, UInt32 maxcount)
{
    struct _fusefs_recursive_lock *lock;

    if (lock_group == NULL)
        return 0;

    lock = (struct _fusefs_recursive_lock*) FUSE_OSMalloc(
        sizeof(struct _fusefs_recursive_lock), fuse_malloc_tag);
    if (!lock)
        return NULL;

    lock->mutex = lck_mtx_alloc_init(lock_group, LCK_ATTR_NULL);
    if (lock->mutex) {
        lock->group = lock_group;
        lock->thread = 0;
        lock->count = 0;
        lock->maxcount = maxcount;
    } else {
        FUSE_OSFree(lock, sizeof(struct _fusefs_recursive_lock),
            fuse_malloc_tag);
        lock = 0;
    }

    return (fusefs_recursive_lock*) lock;
}


fusefs_recursive_lock* fusefs_recursive_lock_alloc(void)
{
    return fusefs_recursive_lock_alloc_internal(fuse_lock_group, 0);
}

fusefs_recursive_lock* fusefs_recursive_lock_alloc_with_maxcount(UInt32 maxcount)
{
    return fusefs_recursive_lock_alloc_internal(fuse_lock_group, maxcount);
}

void fusefs_recursive_lock_free(fusefs_recursive_lock *lock)
{
    lck_mtx_free(lock->mutex, lock->group);
    FUSE_OSFree(lock, sizeof(fusefs_recursive_lock), fuse_malloc_tag);
}

/* Currently not exported in header as we don't use it anywhere. */
#if 0
lck_mtx_t* fusefs_recursive_lock_get_mach_lock(fusefs_recursive_lock *lock)
{
    return lock->mutex;
}
#endif

void fusefs_recursive_lock_lock(fusefs_recursive_lock *lock)
{
    if (lock->thread == current_thread()) {
        if (lock->maxcount > 0 && lock->count >= lock->maxcount) {
            panic("osxfuse: Attempted to lock max-locked recursive lock.");
        }
        lock->count++;
    } else {
        lck_mtx_lock(lock->mutex);
        assert(lock->thread == 0);
        assert(lock->count == 0);
        lock->thread = current_thread();
        lock->count = 1;
    }
}

/* Currently not exported in header as we don't use it anywhere. */
/* Can't find lck_mtx_try_lock in headers, so this function can't compile. */
#if 0
bool fusefs_recursive_lock_try_lock(fusefs_recursive_lock *lock)
{
    if (lock->thread == current_thread()) {
        if (lock->maxcount > 0 && lock->count >= lock->maxcount) {
            return false;
        }
        lock->count++;
        return true;
    } else {
        if (lck_mtx_try_lock(lock->mutex)) {
            assert(lock->thread == 0);
            assert(lock->count == 0);
            lock->thread = current_thread();
            lock->count = 1;
            return true;
	    }
    }
    return false;
}
#endif

void fusefs_recursive_lock_unlock(fusefs_recursive_lock *lock)
{
    assert(lock->thread == current_thread());

    if(lock->count == 0)
        panic("osxfuse: Attempted to unlock non-locked recursive lock.");

    if (0 == (--lock->count)) {
        lock->thread = 0;
        lck_mtx_unlock(lock->mutex);
    }
}

bool fusefs_recursive_lock_have_lock(fusefs_recursive_lock *lock)
{
    return lock->thread == current_thread();
}

/* Currently not exported in header as we don't use it anywhere. */
#if 0
int fusefs_recursive_lock_sleep(fusefs_recursive_lock *lock,
        void *event, UInt32 interType)
{
    UInt32 count = lock->count;
    int res;

    assert(lock->thread == current_thread());

    lock->count = 0;
    lock->thread = 0;
    res = lck_mtx_sleep(lock->mutex, LCK_SLEEP_DEFAULT, (event_t) event,
        (wait_interrupt_t) interType);

    // Must re-establish the recursive lock no matter why we woke up
    // otherwise we would potentially leave the return path corrupted.
    assert(lock->thread == 0);
    assert(lock->count == 0);
    lock->thread = current_thread();
    lock->count = count;
    return res;
}
#endif

/* Currently not exported in header as we don't use it anywhere. */
/* __OSAbsoluteTime is KERNEL_PRIVATE, so this function can't compile. */
#if 0
int fusefs_recursive_lock_sleep_deadline(fusefs_recursive_lock *lock,
        void *event, AbsoluteTime deadline, UInt32 interType)
{
    UInt32 count = lock->count;
    int res;

    assert(lock->thread == current_thread());

    lock->count = 0;
    lock->thread = 0;
    res = lck_mtx_sleep_deadline(lock->mutex, LCK_SLEEP_DEFAULT,
        (event_t) event, (wait_interrupt_t) interType,
        __OSAbsoluteTime(deadline));

    // Must re-establish the recursive lock no matter why we woke up
    // otherwise we would potentially leave the return path corrupted.
    assert(lock->thread == 0);
    assert(lock->count == 0);
    lock->thread = current_thread();
    lock->count = count;
    return res;
}
#endif

/* Currently not exported in header as we don't use it anywhere. */
#if 0
void fusefs_recursive_lock_wakeup(__unused fusefs_recursive_lock *lock,
        void *event, bool oneThread)
{
    thread_wakeup_prim((event_t) event, oneThread, THREAD_AWAKENED);
}
#endif

#if M_OSXFUSE_ENABLE_LOCK_LOGGING
lck_mtx_t *fuse_log_lock = NULL;
#endif /* M_OSXFUSE_ENABLE_LOCK_LOGGING */

#if M_OSXFUSE_ENABLE_HUGE_LOCK
fusefs_recursive_lock *fuse_huge_lock = NULL;
#endif /* M_OSXFUSE_ENABLE_HUGE_LOCK */

#endif /* M_OSXFUSE_ENABLE_INTERIM_FSNODE_LOCK */
