/*
 * ntfs_hash.c - NTFS kernel inode hash operations.
 *
 * Copyright (c) 2006-2011 Anton Altaparmakov.  All Rights Reserved.
 * Portions Copyright (c) 2006-2011 Apple Inc.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution. 
 * 3. Neither the name of Apple Inc. ("Apple") nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ALTERNATIVELY, provided that this notice and licensing terms are retained in
 * full, this file may be redistributed and/or modified under the terms of the
 * GNU General Public License (GPL) Version 2, in which case the provisions of
 * that version of the GPL will apply to you instead of the license terms
 * above.  You can obtain a copy of the GPL Version 2 at
 * http://developer.apple.com/opensource/licenses/gpl-2.txt.
 */

#include <sys/cdefs.h>

#include <sys/errno.h>
#include <sys/kernel_types.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/queue.h>
#ifdef KERNEL
#include <sys/systm.h>
#else
#include <search.h>
#endif
#include <sys/ucred.h>
#include <sys/vnode.h>

#include <libkern/OSAtomic.h>
#ifdef KERNEL
#include <libkern/OSMalloc.h>

#include <kern/locks.h>
#endif

#include "ntfs.h"
#include "ntfs_debug.h"
#include "ntfs_hash.h"
#include "ntfs_inode.h"
#include "ntfs_layout.h"
#include "ntfs_types.h"
#include "ntfs_inode.h"
#include "ntfs_volume.h"

/* Structures associated with ntfs inode caching. */
static ntfs_inode_list_head *ntfs_inode_hash_table;
static unsigned long ntfs_inode_hash_mask;
#ifndef KERNEL
/* Random choice of initial table size: 128 entries */
#define desiredvnodes 128

/* Simple redirects for kernel MALLOC/FREE to userspace malloc/free. */
#define MALLOC(space, cast, size, type, flags) ((space) = calloc(1, (size)))
#define FREE(ptr, type) free(ptr)

/* <stolen from bsd/sys/malloc.h> */
#define	M_TEMP		80	/* misc temporary data buffers */
/* </stolen from bsd/sys/malloc.h> */

/* <stolen from bsd/kern/kern_subr.c> */

/*
 * General routine to allocate a hash table.
 */
void *
hashinit(int elements, int type, u_long *hashmask)
{
	long hashsize;
	LIST_HEAD(generic, generic) *hashtbl;
	int i;

	if (elements <= 0)
		panic("hashinit: bad cnt");
	for (hashsize = 1; hashsize <= elements; hashsize <<= 1)
		continue;
	hashsize >>= 1;
	MALLOC(hashtbl, struct generic *,
		hashsize * sizeof(*hashtbl), type, M_WAITOK|M_ZERO);
	if (hashtbl != NULL) {
		for (i = 0; i < hashsize; i++)
			LIST_INIT(&hashtbl[i]);
		*hashmask = hashsize - 1;
	}
	return (hashtbl);
}

/* </stolen from bsd/kern/kern_subr.c> */
#endif

/* A sleeping lock to protect concurrent accesses to the ntfs inode hash. */
lck_mtx_t ntfs_inode_hash_lock;

/**
 * ntfs_inode_hash_init - initialize the ntfs inode hash
 *
 * Initialize the ntfs inode hash.
 */
errno_t ntfs_inode_hash_init(void)
{
	/* Create the ntfs inode hash. */
	ntfs_inode_hash_table = hashinit(desiredvnodes, M_TEMP,
			&ntfs_inode_hash_mask);
	if (!ntfs_inode_hash_table) {
		ntfs_error(NULL, "Failed to allocate ntfs inode hash table.");
		return ENOMEM;
	}
	ntfs_debug("ntfs_inode_hash_mask 0x%lx.", ntfs_inode_hash_mask);
	/* Initialize the ntfs inode hash lock. */
	lck_mtx_init(&ntfs_inode_hash_lock, ntfs_lock_grp, ntfs_lock_attr);
	return 0;
}

/**
 * ntfs_inode_hash_deinit - deinitialize the ntfs inode hash
 *
 * Deinitialize the ntfs inode hash.
 */
void ntfs_inode_hash_deinit(void)
{
	/* Deinitialize the ntfs inode hash lock. */
	lck_mtx_destroy(&ntfs_inode_hash_lock, ntfs_lock_grp);
	/*
	 * Free the ntfs inode hash.
	 *
	 * FIXME: There is no hashdeinit() function so we do it ourselves but
	 * this means that if the implementation of hashinit() changes, this
	 * code will need to be adapted.
	 */
	FREE(ntfs_inode_hash_table, M_TEMP);
}

/**
 * ntfs_inode_hash - calculate the hash for a given ntfs inode
 * @vol:	ntfs volume to which the inode belongs
 * @mft_no:	inode number/mft record number of the ntfs inode
 *
 * Return the hash for the ntfs inode with mft record number @mft_no on the
 * volume @vol.
 */
static inline unsigned long ntfs_inode_hash(const ntfs_volume *vol,
		const ino64_t mft_no)
{
	return (vol->dev + mft_no) & ntfs_inode_hash_mask;
}

/**
 * ntfs_inode_hash_list - get the hash bucket list for a given ntfs inode
 * @vol:	ntfs volume to which the inode belongs
 * @mft_no:	inode number/mft record number of the ntfs inode
 *
 * Return the hash bucket list for the ntfs inode with mft record number
 * @mft_no on the volume @vol.
 */
static inline ntfs_inode_list_head *ntfs_inode_hash_list(const ntfs_volume *vol,
		const ino64_t mft_no)
{
	return ntfs_inode_hash_table + ntfs_inode_hash(vol, mft_no);
}

/**
 * ntfs_inode_hash_list_find_nolock - find and return a loaded ntfs inode
 *
 * Search the ntfs inode hash bucket @list for the ntfs inode matching @na and
 * if present return it.  If not present return NULL.
 *
 * Locking: Caller must hold the @ntfs_inode_hash_lock.  Note the lock may be
 *	    dropped if an inode is found to be under reclaim or in the process
 *	    of being loaded, in which cases we drop the lock and wait for the
 *	    inode to be reclaimed/loaded and then we retry the search again.
 */
static inline ntfs_inode *ntfs_inode_hash_list_find_nolock(
		const ntfs_volume *vol, const ntfs_inode_list_head *list,
		const ntfs_attr *na)
{
	ntfs_inode *ni;

	/*
	 * Iterate over all the entries in the hash bucket matching @mp and
	 * @mft_no.  If the ntfs_inode is not in cache, the loop is exited with
	 * @ni set to NULL.
	 */
retry:
	LIST_FOREACH(ni, list, hash) {
		if (ni->vol != vol)
			continue;
		if (!ntfs_inode_test(ni, na))
			continue;
		/*
		 * Make sure that the inode cannot disappear under us at this
		 * point by going to sleep and retrying if it is in the process
		 * of being discarded or allocated.
		 */
		if (NInoReclaim(ni) || NInoAlloc(ni)) {
#ifdef DEBUG
			const char *op;

			if (NInoReclaim(ni))
				op = "reclaim";
			else /* if (NInoAlloc(ni)) */
				op = "allocat";
			ntfs_debug("Inode is being %sed, waiting and "
					"retrying.", op);
#endif
			/* Drops the hash lock. */
			ntfs_inode_wait(ni, &ntfs_inode_hash_lock);
			lck_mtx_lock(&ntfs_inode_hash_lock);
			goto retry;
		}
		/* Found the inode. */
		break;
	}
	return ni;
}

/**
 * ntfs_inode_hash_list_find - find and return a loaded ntfs inode
 *
 * Search the ntfs inode hash bucket @list for the ntfs inode matching @na and
 * if present return it.  If not present return NULL.
 *
 * If the found ntfs inode has a vnode attached, then get an iocount reference
 * on the vnode.
 */
static inline ntfs_inode *ntfs_inode_hash_list_find(const ntfs_volume *vol,
		const ntfs_inode_list_head *list, const ntfs_attr *na)
{
	ntfs_inode *ni;

retry:
	lck_mtx_lock(&ntfs_inode_hash_lock);
	ni = ntfs_inode_hash_list_find_nolock(vol, list, na);
	if (ni) {
		vnode_t vn;
		u32 vn_id = 0;

		// FIXME: If this is an extent inode (i.e. it has no vnode), do
		// we want to take an iocount reference on its base vnode?  If
		// so we would need to make sure to release it when finished
		// with the extent inode. -> Need to do that but only when we
		// start looking up extent inodes from the $MFT pageout code
		// path so that the base inode cannot disappear under us which
		// would also cause the extent ntfs inode to disappear under
		// us.
		vn = ni->vn;
		if (vn)
			vn_id = vnode_vid(vn);
		lck_mtx_unlock(&ntfs_inode_hash_lock);
		if (vn && vnode_getwithvid(vn, vn_id))
			goto retry;
		return ni;
	}
	lck_mtx_unlock(&ntfs_inode_hash_lock);
	return ni;
}

/**
 * ntfs_inode_hash_lookup - find and return a loaded ntfs inode
 *
 * Search the ntfs inode hash for the ntfs inode matching @na and if present
 * return it.  If not present return NULL.
 *
 * If the found ntfs inode has a vnode attached, then get an iocount reference
 * on the vnode.
 */
ntfs_inode *ntfs_inode_hash_lookup(ntfs_volume *vol, const ntfs_attr *na)
{
	ntfs_inode_list_head *list;
	ntfs_inode *ni;

	ntfs_debug("Entering for mft_no 0x%llx, type 0x%x, name_len 0x%x.",
			(unsigned long long)na->mft_no,
			(unsigned)le32_to_cpu(na->type), na->name_len);
	list = ntfs_inode_hash_list(vol, na->mft_no);
	ni = ntfs_inode_hash_list_find(vol, list, na);
	ntfs_debug("Done (ntfs_inode %sfound in cache).", ni ? "" : "not ");
	return ni;
}

/**
 * ntfs_inode_hash_get - find or allocate, and return a loaded ntfs inode
 *
 * Search the ntfs inode hash for the ntfs inode matching @na and if present
 * return it.
 *
 * If the found ntfs inode has a vnode attached, then get an iocount reference
 * on the vnode.
 *
 * If not present, allocate the ntfs inode, add it to the hash, and initialize
 * it before returning it.  The inode will be marked NInoAlloc() and no vnode
 * will be attached yet.
 */
ntfs_inode *ntfs_inode_hash_get(ntfs_volume *vol, const ntfs_attr *na)
{
	ntfs_inode_list_head *list;
	ntfs_inode *ni, *nni;

	ntfs_debug("Entering for mft_no 0x%llx, type 0x%x, name_len 0x%x.",
			(unsigned long long)na->mft_no,
			(unsigned)le32_to_cpu(na->type), na->name_len);
	list = ntfs_inode_hash_list(vol, na->mft_no);
	ni = ntfs_inode_hash_list_find(vol, list, na);
	if (ni) {
		ntfs_debug("Done (ntfs_inode found in cache).");
		return ni;
	}
	/* Not found, allocate a new ntfs_inode and initialize it. */
	nni = OSMalloc(sizeof(ntfs_inode), ntfs_malloc_tag);
	if (!nni) {
		ntfs_error(vol->mp, "Failed to allocate new ntfs_inode.");
		return nni;
	}
	if (ntfs_inode_init(vol, nni, na)) {
		OSFree(nni, sizeof(ntfs_inode), ntfs_malloc_tag);
		ntfs_error(vol->mp, "Failed to initialize new ntfs_inode.");
		return NULL;
	}
	/*
	 * Take the hash lock and ensure a racing process did not already
	 * allocate the inode by searching for it again in the cache.
	 */
retry:
	lck_mtx_lock(&ntfs_inode_hash_lock);
	ni = ntfs_inode_hash_list_find_nolock(vol, list, na);
	if (ni) {
		/*
		 * Someone else already added the ntfs inode so return that and
		 * throw away ours.
		 */
		vnode_t vn;
		u32 vn_id = 0;

		vn = ni->vn;
		if (vn)
			vn_id = vnode_vid(vn);
		/* Drops the hash lock. */
		ntfs_inode_wait_locked(ni, &ntfs_inode_hash_lock);
		if (vn && vnode_getwithvid(vn, vn_id))
			goto retry;
		OSFree(nni, sizeof(ntfs_inode), ntfs_malloc_tag);
		ntfs_debug("Done (ntfs_inode found in cache - lost race)).");
		return ni;
	}
	/*
	 * We have allocated a new ntfs inode, it is NInoLocked() and
	 * NInoAlloc() and we hold the hash lock so we can now add our inode to
	 * the hash list bucket and drop the hash lock.
	 */
	LIST_INSERT_HEAD(list, nni, hash);
	lck_mtx_unlock(&ntfs_inode_hash_lock);
	/* Add the inode to the list of inodes in the volume. */
	lck_mtx_lock(&vol->inodes_lock);
	LIST_INSERT_HEAD(&vol->inodes, nni, inodes);
	lck_mtx_unlock(&vol->inodes_lock);
	ntfs_debug("Done (new ntfs_inode added to cache).");
	return nni;
}

/**
 * ntfs_inode_hash_rm - remove an ntfs inode from the ntfs inode hash
 * @ni:		ntfs inode to remove from the hash
 *
 * Remove the ntfs inode @ni from the ntfs inode hash.
 */
void ntfs_inode_hash_rm(ntfs_inode *ni)
{
	lck_mtx_lock(&ntfs_inode_hash_lock);
	ntfs_inode_hash_rm_nolock(ni);
	lck_mtx_unlock(&ntfs_inode_hash_lock);
}
