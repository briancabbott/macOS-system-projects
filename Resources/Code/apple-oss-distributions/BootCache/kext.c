/*
 * Boot-time performance cache.
 *
 * We implement a shim layer for a block device which can be advised ahead
 * of time of the likely read pattern for the block device.
 *
 * Armed with a sorted version of this read pattern (the "playlist"), we
 * cluster the reads into a private cache, taking advantage of the
 * sequential I/O pattern. Incoming read requests are then satisfied from
 * the cache (if possible), or by forwarding to the original strategy routine.
 *
 * We also record the pattern of incoming read requests (the "history
 * list"), in order to provide for adaptation to changes in behaviour by
 * the calling layer.
 *
 * The hit rate of the cache is also measured, and caching will be disabled
 * preemptively if the hit rate is too low.
 *
 * The cache is controlled by a sysctl interface.  Typical usage is as
 * follows:
 *
 * - Load a sorted read pattern.
 * - Wait for the operation(s) of interest to be performed.
 * - Fetch the new (unsorted) read pattern.
 *
 * Loading a read pattern initialises the cache; upon successful loading
 * the cache is active and readhead is being performed.  After fetching the
 * recorded read pattern, the cache remains around until jettisoned due
 * to memory pressure or the hit rate falls below a desirable level.
 *
 * Limitations:
 *
 * - only one copy of the cache may be active at a time
 */

/*
 * Emulate readahead, don't actually perform it.
 */
/*#define EMULATE_ONLY*/

/*
 * Ignore the batches on playlist entries.
 */
/*#define IGNORE_BATCH */

/*
 * Tunable parameters.
 */

/*
 * Minimum required physical memory before BootCache will
 * activate.  Insufficient physical memory leads to paging
 * activity during boot, which is not repeatable and can't
 * be distinguished from the repeatable read patterns.
 *
 * Size in megabytes.
 */
static int BC_minimum_mem_size = 256;

/*
 * Number of seconds to wait in BC_strategy for readahead to catch up
 * with our request.   Tuninig issues:
 *  - Waiting too long may hold up the system behind this I/O/.
 *  - Bypassing an I/O too early will reduce the hit rate and adversely
 *    affect performance on slow media.
 */
#define BC_READAHEAD_WAIT_DEFAULT	10
#define BC_READAHEAD_WAIT_CDROM		45
static int BC_wait_for_readahead = BC_READAHEAD_WAIT_DEFAULT;

/*
 * Cache hit ratio monitoring.
 *
 * The time overhead of a miss in the cache is approx 1/5000th of the
 * time it takes to satisfy an IO from a HDD. If our hit rate falls
 * below this, we're no longer providing a performance win and the
 * cache will be jettisoned.
 *
 * Make sure to keep the cache around for at least an hour
 * if the user hasn't logged in yet, though.
 */
static int BC_chit_max_num_IOs_since_last_hit = 10000;
static int BC_chit_prelogin_timeout_seconds = 3600;

/*
 * History recording timeout.
 *
 * If the history recording is not turned off, the history buffer
 * will grow to its maximum size. This timeout will kill the history
 * recording after the specified interval (in hz).
 */
static int BC_history_timeout = 600 * 100;

static int tag_batch_num = 0;

#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/kdebug.h>
#include <sys/kernel.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/ubc.h>
#include <sys/uio.h>

#include <uuid/uuid.h>

#include <mach/kmod.h>
#include <mach/mach_types.h>
#include <mach/vm_param.h>	/* for max_mem */

#include <vm/vm_kern.h>
#include <vm/vm_pageout.h>

#include <libkern/libkern.h>
#include <libkern/OSAtomic.h>

#include <kern/assert.h>
#include <kern/host.h>
#include <kern/thread_call.h>
#include <kern/locks.h>

#include <IOKit/storage/IOMediaBSDClient.h>
#include <IOKit/IOLib.h> // For IOMalloc

#include <pexpert/pexpert.h>

#include "BootCache_private.h"
#include "bc_utils.h"
#include "BootCache_debug.h"

/*
 * Trace macros
 */
#ifndef DBG_BOOTCACHE
#define DBG_BOOTCACHE	7
#endif

#define	DBG_BC_TAG					(1u << 0)
#define	DBG_BC_BATCH				(1u << 1)

#define DBG_BC_IO_HIT				(1u << 2)
#define DBG_BC_IO_HIT_STALLED		(1u << 3)
#define DBG_BC_IO_MISS				(1u << 4)
#define DBG_BC_IO_MISS_CUT_THROUGH	(1u << 5)
#define DBG_BC_PLAYBACK_IO			(1u << 6)

#define DBG_BC_RECORDING_START		(1u << 7)
#define DBG_BC_RECORDING_END		(1u << 8)

#ifdef BC_DEBUG
struct timeval debug_starttime;
struct timeval debug_currenttime;
lck_mtx_t debug_printlock;
char* debug_print_buffer;
size_t debug_print_buffer_length;
size_t debug_print_buffer_capacity;
bool debug_print_buffer_failed;
size_t debug_print_size;
char* debug_temp_buffer;
#endif

#define debug_bytes(cm_idx, ce, start_bytes, length_bytes, fmt, args...) \
({ \
	if (length_bytes > 0) { \
		if (ce) { \
			debug("%36s %#12llx: "RANGE_FMT" "fmt, (((struct BC_cache_extent *)ce)->ce_mount_idx < BC_cache->c_nmounts) ? uuid_string(BC_cache->c_mounts[((struct BC_cache_extent *)ce)->ce_mount_idx].cm_uuid) : "pre-setup mount", ((struct BC_cache_extent *)ce)->ce_diskoffset, RANGE_ARGS(start_bytes, length_bytes), ## args); \
		} else if (cm_idx >= 0 && cm_idx < BC_cache->c_nmounts) { \
			debug("%36s: "RANGE_FMT" "fmt, uuid_string(BC_cache->c_mounts[cm_idx].cm_uuid), RANGE_ARGS(start_bytes, length_bytes), ## args); \
		} else { \
			debug(""RANGE_FMT" "fmt, RANGE_ARGS(start_bytes, length_bytes), ## args); \
		} \
	} else { \
		if (ce) { \
			debug("%36s %#12llx: "fmt, (((struct BC_cache_extent *)ce)->ce_mount_idx < BC_cache->c_nmounts) ? uuid_string(BC_cache->c_mounts[((struct BC_cache_extent *)ce)->ce_mount_idx].cm_uuid) : "pre-setup mount", ((struct BC_cache_extent *)ce)->ce_diskoffset, ## args); \
		} else if (cm_idx >= 0 && cm_idx < BC_cache->c_nmounts) { \
			debug("%36s: "fmt, uuid_string(BC_cache->c_mounts[cm_idx].cm_uuid), ## args); \
		} else { \
			debug(""fmt, ## args); \
		} \
	} \
})

/*
 * Debug extents
 */
//#define BC_DEBUG_EXTENTS
//#define BC_DEBUG_LARGE_EXTENTS
#ifdef BC_DEBUG_EXTENTS
#warning BC_DEBUG_EXTENTS defined
#define debug_extents debug_bytes
#ifdef BC_DEBUG_LARGE_EXTENTS
#warning BC_DEBUG_LARGE_EXTENTS defined
#endif
#else
#define debug_extents(cm_idx, ce, start_bytes, length_bytes, fmt, args...)
#endif

/*
 * Debug bypasses
 */
// #define BC_DEBUG_BYPASSES
#ifdef BC_DEBUG_BYPASSES
#warning BC_DEBUG_BYPASSES defined
#endif

/*
 * Allocations
 */
#define _BC_MALLOC_SINGLE(_pointer) \
({ \
	(_pointer) = IOMallocType(typeof(*(_pointer))); \
})

#define _BC_MALLOC_ARRAY_WITHPOINTERS(_pointer, _count) \
({ \
	if ((_count) == 1) { \
		_BC_MALLOC_SINGLE(_pointer); \
	} else { \
		((_pointer) = (((ssize_t)(_count) <= 0) ? NULL : IONewZero(typeof(*(_pointer)), _count))); \
	} \
})

#define _BC_MALLOC_ARRAY_NOPOINTERS(_pointer, _count) \
({ \
	if ((_count) == 1) { \
		_BC_MALLOC_SINGLE(_pointer); \
	} else { \
		((_pointer) = (((ssize_t)(_count) <= 0) ? NULL : IONewZeroData(typeof(*(_pointer)), _count))); \
	} \
})

/*
 * Sanity macro, frees and zeroes a pointer if it is not already zeroed.
 */
#define _BC_FREE_SINGLE(_pointer) \
({ \
	if ((_pointer) != NULL) { \
		IOFreeType(_pointer, typeof(*(_pointer))); \
		(_pointer) = NULL; \
	} \
})

#define _BC_FREE_ARRAY_WITHPOINTERS(_pointer, _count) \
({ \
	if ((_count) == 1)  { \
		_BC_FREE_SINGLE(_pointer); \
	} else { \
		IOSafeDeleteNULL(_pointer, typeof(*(_pointer)), _count); \
	} \
})

#define _BC_FREE_ARRAY_NOPOINTERS(_pointer, _count) \
({ \
	if ((_count) == 1)  { \
		_BC_FREE_SINGLE(_pointer); \
	} else { \
		IODeleteData(_pointer, typeof(*(_pointer)), _count); \
	} \
})

/*
 * Debug allocations
 */
//#define BC_DEBUG_ALLOCATIONS
//#define BC_EXTRA_DEBUG_ALLOCATIONS

#ifdef BC_DEBUG_ALLOCATIONS
#ifdef BC_EXTRA_DEBUG_ALLOCATIONS
#define BC_MALLOC_SINGLE(_pointer) \
({ \
	debug("%s: allocating single of size %lu", #_pointer, sizeof(*_pointer)); \
	_BC_MALLOC_SINGLE(_pointer); \
	debug("%s: allocated single of size %lu: %p", #_pointer, sizeof(*_pointer), _pointer); \
})

#define BC_MALLOC_ARRAY_WITHPOINTERS(_pointer, _count) \
({ \
	debug("%s: allocating withpointer array item size %lu, count %lu", #_pointer, sizeof(*_pointer), (unsigned long)_count); \
	_BC_MALLOC_ARRAY_WITHPOINTERS(_pointer, _count); \
	debug("%s: allocated withpointer array item size %lu, count %lu: %p", #_pointer, sizeof(*_pointer), (unsigned long)_count, _pointer); \
})

#define BC_MALLOC_ARRAY_NOPOINTERS(_pointer, _count) \
({ \
	debug("%s: allocating nopointer array item size %lu, count %lu", #_pointer, sizeof(*_pointer), (unsigned long)_count); \
	_BC_MALLOC_ARRAY_NOPOINTERS(_pointer, _count); \
	debug("%s: allocated nopointer array item size %lu, count %lu: %p", #_pointer, sizeof(*_pointer), (unsigned long)_count, _pointer); \
})

#define BC_FREE_SINGLE(_pointer) \
({ \
	debug("%s: freeing single of size %lu: %p", #_pointer, sizeof(*_pointer), _pointer); \
	_BC_FREE_SINGLE(_pointer); \
	debug("%s: freed single of size %lu", #_pointer, sizeof(*_pointer)); \
})

#define BC_FREE_ARRAY_WITHPOINTERS(_pointer, _count) \
({ \
	debug("%s: freeing withpointer array item size %lu, count %lu: %p", #_pointer, sizeof(*_pointer), (unsigned long)_count, _pointer); \
	_BC_FREE_ARRAY_WITHPOINTERS(_pointer, _count); \
	debug("%s: freed withpointer array item size %lu, count %lu", #_pointer, sizeof(*_pointer), (unsigned long)_count); \
})

#define BC_FREE_ARRAY_NOPOINTERS(_pointer, _count) \
({ \
	debug("%s: freeing nopointer array item size %lu, count %lu: %p", #_pointer, sizeof(*_pointer), (unsigned long)_count, _pointer); \
	_BC_FREE_ARRAY_NOPOINTERS(_pointer, _count); \
	debug("%s: freed nopointer array item size %lu, count %lu", #_pointer, sizeof(*_pointer), (unsigned long)_count); \
})

#else // !BC_EXTRA_DEBUG_ALLOCATIONS

#define BC_MALLOC_SINGLE(_pointer) \
({ \
	_BC_MALLOC_SINGLE(_pointer); \
	debug("%s: allocated single of size %lu: %p", #_pointer, sizeof(*_pointer), _pointer); \
})

#define BC_MALLOC_ARRAY_WITHPOINTERS(_pointer, _count) \
({ \
	_BC_MALLOC_ARRAY_WITHPOINTERS(_pointer, _count); \
	debug("%s: allocated withpointer array item size %lu, count %lu: %p", #_pointer, sizeof(*_pointer), (unsigned long)_count, _pointer); \
})

#define BC_MALLOC_ARRAY_NOPOINTERS(_pointer, _count) \
({ \
	_BC_MALLOC_ARRAY_NOPOINTERS(_pointer, _count); \
	debug("%s: allocated nopointer array item size %lu, count %lu: %p", #_pointer, sizeof(*_pointer), (unsigned long)_count, _pointer); \
})

#define BC_FREE_SINGLE(_pointer) \
({ \
	debug("%s: freeing single of size %lu: %p", #_pointer, sizeof(*_pointer), _pointer); \
	_BC_FREE_SINGLE(_pointer); \
})

#define BC_FREE_ARRAY_WITHPOINTERS(_pointer, _count) \
({ \
	debug("%s: freeing withpointer array item size %lu, count %lu: %p", #_pointer, sizeof(*_pointer), (unsigned long)_count, _pointer); \
	_BC_FREE_ARRAY_WITHPOINTERS(_pointer, _count); \
})

#define BC_FREE_ARRAY_NOPOINTERS(_pointer, _count) \
({ \
	debug("%s: freeing nopointer array item size %lu, count %lu: %p", #_pointer, sizeof(*_pointer), (unsigned long)_count, _pointer); \
	_BC_FREE_ARRAY_NOPOINTERS(_pointer, _count); \
})
#endif

#else // !BC_DEBUG_ALLOCATIONS
#define BC_MALLOC_SINGLE             _BC_MALLOC_SINGLE
#define BC_MALLOC_ARRAY_WITHPOINTERS _BC_MALLOC_ARRAY_WITHPOINTERS
#define BC_MALLOC_ARRAY_NOPOINTERS   _BC_MALLOC_ARRAY_NOPOINTERS
#define BC_FREE_SINGLE               _BC_FREE_SINGLE
#define BC_FREE_ARRAY_WITHPOINTERS   _BC_FREE_ARRAY_WITHPOINTERS
#define BC_FREE_ARRAY_NOPOINTERS     _BC_FREE_ARRAY_NOPOINTERS
#endif

/* Copied from APFS headers to keep track of optimization statistics */
#ifndef FUSION_TIER2_DEVICE_BYTE_ADDR
#define FUSION_TIER2_DEVICE_BYTE_ADDR 0x4000000000000000ULL
#endif

/*
 * Default is to only cache for the volumes on the root disk, but can be overridden by "bc_cache_nonroot_disks=1" in boot-args
 */
static bool cache_nonroot_disks = false;

/*
 * Default is to only cache HDDs and Fusion, but can be overridden by "bc_cache_ssds=1" in boot-args
 */
static bool cache_ssds = false;

/*
 * rdar://69891587 (Disable BootCache on AS Macs)
 */
static bool cache_apple_silicon_macs = false;

static void BC_parse_boot_args(void) {
	uint32_t val;
	if (PE_parse_boot_argn("bc_cache_nonroot_disks", &val, sizeof(val))) {
		if (val == 1) {
			message("caching nonroot disks");
			cache_nonroot_disks = true;
		} else {
			message("not caching nonroot disks");
			cache_nonroot_disks = false;
		}
	}
	if (PE_parse_boot_argn("bc_cache_ssds", &val, sizeof(val))) {
		if (val == 1) {
			message("caching SSDs");
			cache_ssds = true;
		} else {
			message("not caching SSDs");
			cache_ssds = false;
		}
	}
	if (PE_parse_boot_argn("bc_cache_apple_silicon_macs", &val, sizeof(val))) {
		if (val == 1) {
			message("caching Apple Silicon Macs");
			cache_apple_silicon_macs = true;
		} else {
			message("not caching Apple Silicon Macs");
			cache_apple_silicon_macs = false;
		}
	}
}


/**************************************
 * Structures for the readahead cache *
 **************************************/

/*
 * A cache extent.
 *
 * Each extent represents a contiguous range of disk blocks which are
 * held in the cache.  All values in bytes.
 */
struct BC_cache_extent {
	lck_mtx_t   ce_lock;
	u_int64_t   ce_diskoffset;  /* physical offset on device */
	u_int64_t   ce_length;      /* data length */
	u_int64_t   ce_crypto_offset; /* crypto offset */
	off_t       ce_cacheoffset;	/* offset into cache backing store */
	union {
		uintptr_t *ce_blockmap;      /* track valid blocks for this extent when there are more than 64 of them */
		uintptr_t  ce_localblockmap; /* track valid blocks for this extent when there are less then 64 of them */
	};
	u_int32_t  ce_blockmap_arraycount; /* If 0, blockmap has not been determined yet. If 1, then blockmap is contained within ce_localblockmap. If >1, then ce_blockmap points to an allocated array of <ce_blockmap_arraycount> count */
	int         ce_mount_idx;   /* the index of the mount for this extent */
	u_int16_t   ce_batch;       /* batch number */
	u_int16_t   ce_flags;       /* flags */
#define CE_ABORTED  (1u << 0)    /* extent no longer has data */
#define CE_IODONE   (1u << 1)    /* extent has been read */
#define CE_LOWPRI   (1u << 2)    /* extent is low priority */
#define CE_SHARED   (1u << 3)    /* extent is in the shared cache */
};

/* A mount 
 *
 * The volume to which an extent refers
 */
struct BC_cache_mount {
	lck_rw_t                 cm_lock;      /* lock guards instance variables, extents have their own locks */
	uuid_t                   cm_uuid;      /* this mount's uuid */
	uuid_t                   cm_group_uuid;/* If non-0, then this mount shares disk space will all other mounts with the same cm_group_uuid (for apfs volumes and containers, this is the container's UUID) */
	int                      cm_nextents;  /* the number of extents that reference this mount */
	struct BC_cache_extent **cm_pextents;  /* a list of pointers to extents which reference
											this mount, sorted by block address */
	uint32_t                 cm_pextents_size; /* allocation array count of cm_pextents */
	uint32_t                 cm_state;     /* this mount's state */
#define CM_SETUP	(0)                    /* fields above are valid. Not mounted yet */
#define CM_READY	(1)                    /* every field is valid. Mounted and ready to go */
#define CM_ABORTED	(2)                    /* mount failed for some reason */

	uint32_t                 cm_fs_flags;  /* filesystem flags, see BootCache_private.h */
	
	/* fields below filled in when we detect that the volume has mounted */
	dev_t                    cm_dev;       /* the device for this mount */
	struct buf              *cm_bp;        /* struct buf allocated in the reader thread for this device */
	u_int64_t                cm_throttle_mask;/* the throttle mask to use in throttle APIs for this mount */
	u_int64_t                cm_blocksize; /* keep 64-bit for promotion */
	u_int64_t                cm_devsize;   /* range checking */
	u_int64_t                cm_maxread;   /* largest read the dev will support */
	struct BC_cache_disk*    cm_disk;      /* the mount's physical disk */
};

/* A physical disk
 *
 * The disk may contain several mounts
 * and will have one readahead thread
 */
struct BC_cache_disk {
	lck_mtx_t cd_lock;
	u_int64_t cd_disk_id;    /* The throttle mask, used as an identifier */
	int       cd_disk_num; /* used for stat batching */
	int       cd_flags;      /* flags */
#define CD_HAS_THREAD   (1u << 0)  /* there is a readahead thread in progress for this disk */
#define CD_ISSUE_LOWPRI (1u << 1)  /* the readahead thread is reading in low-priority extents */
#define CD_IS_SSD       (1u << 2)  /* this is a solid state disk */
	int       cd_nmounts;    /* The number of mounts that reference this disk */
	int       cd_batch;      /* What playback batch this disk is on */
};

/***************************************************************
 * Structures for tracking dev-container while cache is active *
 * <rdar://problem/43851924> When discarding data, discard data for that extent from all mounts in the same apfs container
 ***************************************************************/

/* This structure is copied by value, so must not contain any fields that cannot be copied by value */
struct BC_noncached_mount {
	dev_t                    nm_dev;       /* the device for this mount */
	uuid_t                   nm_group_uuid;/* If non-0, then this mount shares disk space will all other mounts with the same group_uuid (for apfs volumes and containers, this is the container's UUID) */
};

/************************************
 * Structures for history recording *
 ************************************/

/* see BC_history_entry and BC_history_mount in BootCache_private.h */

/*
 * Wrapper around BC_history_mount so we
 * can keep track of the device
 */
struct BC_history_mount_device {
	u_int64_t                       hmd_disk_id;
	struct BC_history_mount_device* hmd_next;
	uint32_t                        hmd_is_ssd;
	dev_t                           hmd_dev;
	int                             hmd_blocksize;
	struct BC_history_mount         hmd_mount;
};

/*
 * A history entry list cluster.
 */
struct BC_history_entry_cluster {
	struct BC_history_entry_cluster *hec_link;
	int                              hec_nentries;	/* number of entries in list */
	struct BC_history_entry          *hec_data; /* always allocated for BC_HISTORY_DATA_NUM_ALLOCED_ENTRIES entries */
};

// Allocate 128K buffers at a time for history entries
# define BC_HISTORY_DATA_DESIRED_ALLOC_SIZE  (128 * 1024)
# define BC_HISTORY_DATA_NUM_ALLOCED_ENTRIES (BC_HISTORY_DATA_DESIRED_ALLOC_SIZE / sizeof(struct BC_history_entry))
# define BC_HISTORY_MAXCLUSTERS              ((BC_MAXENTRIES / BC_HISTORY_DATA_NUM_ALLOCED_ENTRIES) + 1)


/*
 * The cache control structure.
 */
struct BC_cache_control {
	lck_grp_t  *c_lckgrp;
	
	/* the device we are covering */
	struct bdevsw	*c_devsw;
	
	/* saved switch table routines */
	strategy_fcn_t   *c_strategy;
	open_close_fcn_t *c_close;
	lck_mtx_t         c_handlers_lock;
	
	u_int64_t	c_cachesize;		/* total number of bytes in cache */
	
	u_int64_t   c_root_disk_id;     /* the throttle mask of the root disk, used as an id for the physical device */
	                                /* This needs to be updated to handle multiple physical volumes backing a mount */
	
	lck_rw_t                    c_noncached_mounts_lock;
	int                         c_noncached_nmounts; /* number of mounts in the array below */
	struct BC_noncached_mount  *c_noncached_mounts;  /* the array of mounts seen, but not in our cache */

	/*
	 * The mounts we're tracking
	 */
	int                     c_nmounts; /* number of mounts in the array below */
	struct BC_cache_mount  *c_mounts;  /* the array of mounts the cache extents refer to */

	/*
	 * Extents, in optimal read order.
	 *
	 * Each extent tracks the pages it owns in the buffer.
	 * It is assumed that PAGE_SIZE is a multiple of cm_blocksize for each mount.
	 *
	 * Each extent refers to one of the mounts, above, by index into c_mounts
	 */
	int                      c_nextentlists; /* number of extent lists we have */
	u_int64_t               *c_nextents;     /* number of extents in a given extent list */
	struct BC_cache_extent **c_extentlists;      /* an array of extent lists in the cache */
	
	int         c_batch_count;		/* number of extent batches there have ever been */
	int         c_batch_offset_for_new_playlists;		/* number of batches we've already done via previous playlists */
	vnode_t     c_vp;
	uint32_t    c_vid;
	/* rw lock protects the above fields through c_nmounts, but not their contents.
	 * Shared for general use of the fields,
	 * Exclusive for cache initialization or cleanup.
	 * Each mount/extent has its own lock for modifying its contents.
	 */
	lck_rw_t    c_cache_lock;
	
	
	/* history list, in reverse order */
	int                              c_history_num_clusters;
	struct BC_history_entry_cluster *c_history;
	lck_mtx_t   c_history_cluster_lock;
	struct BC_history_mount_device  *c_history_mounts;
	u_int64_t                         c_history_size;
	/* lock protects the above history fields and their contents */
	lck_rw_t   c_history_lock;
	
	/* fields below are accessed atomically */
	
	/* flags */
	u_int		c_flags; // See cache flags in private.h
	u_int		c_strategycalls;	/* count of busy strategy calls in the cache */
	
	uint32_t  c_readahead_throttles_cutthroughs;
	
	uint32_t  c_num_ios_since_last_hit; /* The number of cache misses since the last hit */
	
	uint32_t  c_num_reader_threads;     /* The number of reader threads active */
	lck_mtx_t c_reader_threads_lock;    /* protects c_num_reader_threads */
	
	/* statistics */
	int c_take_detailed_stats;
	struct BC_statistics c_stats;
	
	/* time-keeping */
	struct timeval c_loadtime;
	struct timeval c_cache_starttime;
	struct timeval c_history_starttime;
	
};

/*
 * The number of blocks per page; assumes that a page
 * will always be >= the size of a disk block.
 */
#define CB_PAGEBLOCKS(cm)			(PAGE_SIZE / (cm)->cm_blocksize)

/*
 * Convert block offset to page offset and vice versa.
 */
#define CB_BLOCK_TO_PAGE(cm, block)		((block) / CB_PAGEBLOCKS(cm))
#define CB_PAGE_TO_BLOCK(cm, page)		((page)  * CB_PAGEBLOCKS(cm))
#define CB_BLOCK_TO_BYTE(cm, block)		((block) * (cm)->cm_blocksize)
#define HB_BLOCK_TO_BYTE(hmd, block)	((block) * (hmd)->hmd_blocksize)
#define CB_BYTE_TO_BLOCK(cm, byte)		((byte)  / (cm)->cm_blocksize)

/*
 * The size of an addressable field in the block map.
 * This reflects the type for the blockmap.
 */
#define CB_MAPFIELDBITS			(sizeof(*((struct BC_cache_extent*)0)->ce_blockmap) * BYTE_SIZE)

OSCompileAssert(sizeof(*((struct BC_cache_extent*)0)->ce_blockmap) == sizeof(((struct BC_cache_extent*)0)->ce_localblockmap));

/*
 * The index into an array of addressable fields, and the bit within
 * the addressable field corresponding to an index in the map.
 */
#define CB_MAPIDX(x)			(((uint64_t)x) / CB_MAPFIELDBITS)
#define CB_MAPBIT(x)			(1ull << (((uint64_t)x) % CB_MAPFIELDBITS))

/*
 * Blockmap management. Must be called with extent lock held.
 */
#define CB_BLOCK_PRESENT(ce, block) \
((ce)->ce_blockmap_arraycount == 0 ? false : (((ce)->ce_blockmap_arraycount == 1 ? (ce)->ce_localblockmap : (ce)->ce_blockmap[CB_MAPIDX(block)]) & CB_MAPBIT(block)))
#define CB_MARK_BLOCK_PRESENT(ce, block) \
({ \
	if ((ce)->ce_blockmap_arraycount == 1) { \
		(ce)->ce_localblockmap |= CB_MAPBIT(block); \
	} else if ((ce)->ce_blockmap_arraycount > 1) { \
		(ce)->ce_blockmap[CB_MAPIDX(block)] |= CB_MAPBIT(block); \
	} \
})
#define CB_MARK_BLOCK_VACANT(ce, block) \
({ \
	if ((ce)->ce_blockmap_arraycount == 1) { \
		(ce)->ce_localblockmap &= ~CB_MAPBIT(block); \
	} else if ((ce)->ce_blockmap_arraycount > 1) { \
		(ce)->ce_blockmap[CB_MAPIDX(block)] &= ~CB_MAPBIT(block); \
	} \
})

/*
 * Determine whether a given page is vacant (all blocks are gone).
 * This takes advantage of the expectation that a page's blocks
 * will never cross the boundary of a field in the blockmap.
 */
#define CB_PAGE_VACANT(cm, ce, page)					\
((ce)->ce_blockmap_arraycount == 0 ? false : \
(!(((ce)->ce_blockmap_arraycount == 1 ? (ce)->ce_localblockmap : (ce)->ce_blockmap[CB_MAPIDX(CB_PAGE_TO_BLOCK(cm, page))]) &	\
(((1ull << CB_PAGEBLOCKS(cm)) - 1) << (CB_PAGE_TO_BLOCK(cm, page) %	\
CB_MAPFIELDBITS)))))

/* Maximum size of the boot cache */
#define BC_MAX_SIZE (max_mem)

/*
 * Synchronization macros
 */
#define LOCK_HISTORY_R()		lck_rw_lock_shared(&BC_cache->c_history_lock)
#define UNLOCK_HISTORY_R()		lck_rw_unlock_shared(&BC_cache->c_history_lock)
#define LOCK_HISTORY_W()		lck_rw_lock_exclusive(&BC_cache->c_history_lock)
#define UNLOCK_HISTORY_W()		lck_rw_unlock_exclusive(&BC_cache->c_history_lock)

#define LOCK_HISTORY_CLUSTERS()      lck_mtx_lock(&BC_cache->c_history_cluster_lock)
#define UNLOCK_HISTORY_CLUSTERS()    lck_mtx_unlock(&BC_cache->c_history_cluster_lock)

#define LOCK_NONCACHED_MOUNTS_R()		lck_rw_lock_shared(&BC_cache->c_noncached_mounts_lock)
#define UNLOCK_NONCACHED_MOUNTS_R()		lck_rw_unlock_shared(&BC_cache->c_noncached_mounts_lock)
#define LOCK_NONCACHED_MOUNTS_W()		lck_rw_lock_exclusive(&BC_cache->c_noncached_mounts_lock)
#define UNLOCK_NONCACHED_MOUNTS_W()		lck_rw_unlock_exclusive(&BC_cache->c_noncached_mounts_lock)

#define LOCK_EXTENT(e)			lck_mtx_lock(&(e)->ce_lock)
#define LOCK_EXTENT_TRY(e)		lck_mtx_try_lock(&(e)->ce_lock)
#define UNLOCK_EXTENT(e)		lck_mtx_unlock(&(e)->ce_lock)

/* mount locks should only be held while also holding the cache lock */
#define LOCK_MOUNT_R(m)			lck_rw_lock_shared(&(m)->cm_lock)
#define UNLOCK_MOUNT_R(m)		lck_rw_unlock_shared(&(m)->cm_lock)
#define LOCK_MOUNT_W(m)			lck_rw_lock_exclusive(&(m)->cm_lock)
#define UNLOCK_MOUNT_W(m)		lck_rw_unlock_exclusive(&(m)->cm_lock)
#define LOCK_MOUNT_W_TO_R(m)	lck_rw_lock_exclusive_to_shared(&(m)->cm_lock)
#define LOCK_MOUNT_R_TO_W(m)	lck_rw_lock_shared_to_exclusive(&(m)->cm_lock)

#define LOCK_DISK(d)			lck_mtx_lock(&(d)->cd_lock)
#define UNLOCK_DISK(d)			lck_mtx_unlock(&(d)->cd_lock)

#define LOCK_READERS()			lck_mtx_lock(&BC_cache->c_reader_threads_lock)
#define UNLOCK_READERS()		lck_mtx_unlock(&BC_cache->c_reader_threads_lock)

#define LOCK_HANDLERS()			lck_mtx_lock(&BC_cache->c_handlers_lock)
#define UNLOCK_HANDLERS()		lck_mtx_unlock(&BC_cache->c_handlers_lock)

#define LOCK_CACHE_R()			lck_rw_lock_shared(&BC_cache->c_cache_lock)
#define UNLOCK_CACHE_R()		lck_rw_unlock_shared(&BC_cache->c_cache_lock)
#define LOCK_CACHE_W()			lck_rw_lock_exclusive(&BC_cache->c_cache_lock)
#define UNLOCK_CACHE_W()		lck_rw_unlock_exclusive(&BC_cache->c_cache_lock)
#define LOCK_CACHE_TRY_W()		lck_rw_try_lock(&BC_cache->c_cache_lock, LCK_RW_TYPE_EXCLUSIVE)
#define LOCK_CACHE_TRY_R()		lck_rw_try_lock(&BC_cache->c_cache_lock, LCK_RW_TYPE_SHARED)
#define LOCK_CACHE_W_TO_R()		lck_rw_lock_exclusive_to_shared(&BC_cache->c_cache_lock)
#define LOCK_CACHE_R_TO_W()		lck_rw_lock_shared_to_exclusive(&BC_cache->c_cache_lock)

#define BC_ADD_NON_SHARED_CACHE_STAT(stat, inc)	OSAddAtomic64((inc), ((SInt64 *)&BC_cache->c_stats.ss_nonsharedcache.ss_##stat))
#define BC_ADD_SHARED_CACHE_STAT(stat, inc)	OSAddAtomic64((inc), ((SInt64 *)&BC_cache->c_stats.ss_sharedcache.ss_##stat))
#define BC_ADD_UNKNOWN_STAT(stat, inc)	OSAddAtomic64((inc), ((SInt64 *)&BC_cache->c_stats.ss_##stat))

#define BC_ADD_STAT(shared, stat, inc) \
do { \
if (shared) { \
BC_ADD_SHARED_CACHE_STAT(stat, inc); \
} else { \
BC_ADD_NON_SHARED_CACHE_STAT(stat, inc); \
} \
} while (0)

/*
 * Only one instance of the cache is supported.
 */
static struct BC_cache_control BC_cache_softc;
static struct BC_cache_control *BC_cache = &BC_cache_softc;

static int	BC_check_intersection(struct BC_cache_extent *ce, u_int64_t offset, u_int64_t length);
static int	BC_find_cache_mount(dev_t dev);
static struct BC_cache_extent**	BC_find_extent(struct BC_cache_mount *cm, u_int64_t offset, u_int64_t length, u_int64_t crypto_offset, int require_crypto_match, int* failed_due_to_crypto_mismatch, int contained, int* failed_with_intersection, int* pnum_extents);
static u_int64_t BC_discard_bytes(struct BC_cache_extent *ce, u_int64_t offset, u_int64_t length);
static u_int64_t BC_handle_discards(struct BC_cache_mount *cm, u_int64_t offset, u_int64_t length, int data_is_overwritten, u_int64_t* unread_discards, int is_shared);
static int	BC_blocks_present(struct BC_cache_extent *ce, u_int64_t base, u_int64_t nblk);
static void	BC_reader_thread(void *param0, wait_result_t param1);
static int	BC_strategy(struct buf *bp);
static int	BC_close(dev_t dev, int flags, int devtype, struct proc *p);
static int	BC_terminate_readahead(const char* reason);
static int	BC_terminate_cache(const char* reason);
static int	BC_terminate_history(const char* reason);
static void	BC_terminate_cache_async(const char* reason);
static void	BC_check_handlers(void);
static void	BC_next_valid_range(struct BC_cache_mount *cm, struct BC_cache_extent *ce, u_int64_t fromoffset,
								u_int64_t *nextpage, u_int64_t *nextoffsetinpage, u_int64_t *nextlength);
static int	BC_setup_disk(struct BC_cache_disk *cd, u_int64_t disk_id, int is_ssd);
static void	BC_teardown_disk(struct BC_cache_disk *cd);
static void	BC_mount_available(struct BC_cache_mount *cm);
static int	BC_setup_mount(struct BC_cache_mount *cm, struct BC_playlist_mount* pm);
static int	BC_fill_in_cache_mount(struct BC_cache_mount *cm, mount_t mount, dev_t dev, vnode_t devvp, uint32_t fs_flags);
static int	BC_fill_in_cache_mount_ex(struct BC_cache_mount *cm, dev_t dev, vnode_t devvp, u_int64_t throttle_mask, uint32_t fs_flags);
static void	BC_teardown_mount(struct BC_cache_mount *cm);
static int	BC_setup_extent(struct BC_cache_extent *ce, const struct BC_playlist_entry *pe, int batch_adjustment, u_int cache_mount_idx);
static void	BC_teardown_extent(struct BC_cache_extent *ce);
static int	BC_copyin_playlist(size_t mounts_size, user_addr_t mounts, size_t entries_size, user_addr_t entries);
static int	BC_init_cache(size_t mounts_size, user_addr_t mounts, size_t entries_size, user_addr_t entries, int record_history);
static int BC_update_mounts(void);
static void	BC_timeout_history(void *);
static struct BC_history_mount_device * BC_get_history_mount_device(dev_t dev, int* pindex);
static int	BC_add_history(vnode_t vp, daddr64_t blkno, u_int64_t length, int pid, int hit, int write, int tag, int shared, u_int64_t crypto_offset, dev_t dev);
static int	BC_size_history_mounts(void);
static int	BC_size_history_entries(void);
static int	BC_copyout_history_mounts(user_addr_t uptr);
static int	BC_copyout_history_entries(user_addr_t uptr);
static void	BC_discard_history(void);
static int	BC_setup(void);

static struct BC_noncached_mount BC_get_noncached_mount(dev_t dev);

static int	fill_in_bc_cache_mounts(mount_t mount, void* arg);
static int	check_for_new_mount_itr(mount_t mount, void* arg);
static int	extents_status(struct BC_cache_extent *ce_firstmatch, struct BC_cache_extent **pce_additional_extents, int nextents) ;
static void	wait_for_extent(struct BC_cache_extent *ce, struct timeval starttime) ;
	
/*
 * Sysctl interface.
 */
static int	BC_sysctl SYSCTL_HANDLER_ARGS;

SYSCTL_PROC(_kern, OID_AUTO, BootCache, CTLFLAG_RW | CTLFLAG_LOCKED,
			0, 0, BC_sysctl,
			"S,BC_command",
			"BootCache command interface");

/*
 * Netboot exports this hook so that we can determine
 * whether we were netbooted.
 */
extern int	netboot_root(void);

/*
 * bsd_init exports this hook so that we can register for notification
 * once the root filesystem is mounted.
 */
extern void (* mountroot_post_hook)(void);
extern void (* unmountroot_pre_hook)(void);

/*
 * Mach VM-specific functions.
 */
static int	BC_alloc_pagebuffer(void);
static void	BC_free_pagebuffer(void);
static void	BC_free_page(struct BC_cache_extent *ce, u_int64_t page);

/* Functions not prototyped elsewhere */
int     	ubc_range_op(vnode_t, off_t, off_t, int, int *); /* in sys/ubc_internal.h */

/*
 * Mach-o functions.
 */
#define MACH_KERNEL 1
#include <mach-o/loader.h>

/*
 * Set or clear a flag atomically and return the previous value of that bit.
 */
static inline UInt32
BC_set_flag(UInt32 flag)
{
	return flag & OSBitOrAtomic(flag, &BC_cache->c_flags);
}

static inline UInt32
BC_clear_flag(UInt32 flag)
{
	return flag & OSBitAndAtomic(~(flag), &BC_cache->c_flags);
}

/*
 * Return a user-readable string for a given uuid
 *
 * Returns a pointer to a static buffer, which is
 * racy, so this should only be used for debugging purposes
 */
static inline const char* uuid_string(uuid_t uuid)
{
	/* Racy, but used for debug output so who cares */
	static uuid_string_t uuidString[4];
	static int index = 0;

	int i = OSIncrementAtomic(&index) % 4;
	
	uuid_unparse(uuid, uuidString[i]);
	return (char*)uuidString[i];
}

#ifdef BC_DEBUG_EXTENTS
static void BC_print_extent_bitmap(struct BC_cache_extent * ce, uint64_t offset, uint64_t length) {
	assert(length == 0 || offset >= ce->ce_diskoffset);
	assert(length == 0 || offset + lenth <= ce->ce_diskoffset + ce->ce_length);

	const uint64_t num_blocks = ce->ce_length / BC_cache->c_mounts[ce->ce_mount_idx].cm_blocksize;
	
#ifndef BC_DEBUG_LARGE_EXTENTS
	if (num_blocks > 128) {
		debug_bytes(ce->ce_mount_idx, ce, offset, length, "bitmap: too large");
		return;
	}
#endif

	char* bitmap;
	const uint64_t bitmap_len = num_blocks + 1; // Enough to print entire extent, plus NUL terminator
	BC_MALLOC_ARRAY_NOPOINTERS(bitmap, bitmap_len);
	int bitmap_idx = 0;
	
	for (uint64_t blknum = 0; blknum < num_blocks; blknum++) {
		if (length > 0 &&
			blknum * BC_cache->c_mounts[ce->ce_mount_idx].cm_blocksize >= offset - ce->ce_diskoffset &&
			blknum * BC_cache->c_mounts[ce->ce_mount_idx].cm_blocksize < offset + length - ce->ce_diskoffset) {
			if (CB_BLOCK_PRESENT(ce, blknum)) {
				bitmap[bitmap_idx++] = '+';
			} else {
				bitmap[bitmap_idx++] = '-';
			}
		} else {
			if (CB_BLOCK_PRESENT(ce, blknum)) {
				bitmap[bitmap_idx++] = '1';
			} else {
				bitmap[bitmap_idx++] = '0';
			}
		}
	}
	bitmap[bitmap_idx++] ='\0';
	assert(bitmap_idx == bitmap_len);
	debug_bytes(ce->ce_mount_idx, ce, offset, length, "bitmap: %s", bitmap);
	BC_FREE_ARRAY_NOPOINTERS(bitmap, bitmap_len);
}
#else
#define BC_print_extent_bitmap(ce, offset, length)
#endif


/*
 * Test whether a given byte range on disk intersects with an extent's range.
 * This function assumes the ranges are within the same mount.
 */
static inline int
BC_check_intersection(struct BC_cache_extent *ce, u_int64_t offset, u_int64_t length)
{
	if ((offset < (ce->ce_diskoffset + ce->ce_length)) &&
		((offset + length) > ce->ce_diskoffset))
		return 1;
	return 0;
}

/*
 * Return the number of bytes of intersection for the given range in this extent.
 */
static inline u_int64_t
BC_intersection_size(struct BC_cache_extent *ce, u_int64_t offset, u_int64_t length)
{
	u_int64_t lower_bounds = ce->ce_diskoffset;
	if (offset > lower_bounds) {
		lower_bounds = offset;
	}
	
	u_int64_t upper_bounds = ce->ce_diskoffset + ce->ce_length;
	if (offset + length < upper_bounds) {
		upper_bounds = offset + length;
	}
	
	if (lower_bounds >= upper_bounds) {
		return 0;
	}
	
	return upper_bounds - lower_bounds;
}

/*
 * Find one of our cache mounts with the given device, if it has been seen this boot
 *
 * Called with the cache read lock held
 *
 * Returns the index of the mount, or -1 if it is not found
 */
static int
BC_find_cache_mount(dev_t dev)
{
	int i;
	
	if (dev == nulldev() || dev == NODEV)
		return(-1);
	
	if (!(BC_cache->c_flags & BC_FLAG_CACHEACTIVE) || (BC_cache->c_mounts == NULL))
		return(-1);
	
	
	for (i = 0; i < BC_cache->c_nmounts; i++) {
		if (BC_cache->c_mounts[i].cm_state == CM_READY) {
			if (BC_cache->c_mounts[i].cm_dev == dev) {
				return i;
			}
		}
	}
	
	return(-1);
}

/* The new throttling implementation needs to be able to check
 * whether an IO is in the boot cache before issuing the IO
 */
static int BC_cache_contains_block(dev_t device, u_int64_t blkno) {
	struct BC_cache_mount * cm = NULL;
	struct BC_cache_extent **pce, *ce;
	
	if (!(BC_cache->c_flags & BC_FLAG_CACHEACTIVE)) {
		return 0;
	}

	LOCK_CACHE_R();

	int cm_idx = BC_find_cache_mount(device);
	if (cm_idx == -1) {
		UNLOCK_CACHE_R();
		return 0;
	}
	
	cm = BC_cache->c_mounts + cm_idx;
	
	LOCK_MOUNT_R(cm);
	
	if (cm->cm_state != CM_READY) {
		UNLOCK_MOUNT_R(cm);
		UNLOCK_CACHE_R();
		return 0;
	}

	u_int64_t disk_offset = CB_BLOCK_TO_BYTE(cm, blkno);
	u_int64_t length = CB_BLOCK_TO_BYTE(cm, 1) /* one block */;
	
	pce = BC_find_extent(cm, disk_offset, length, 0, 0, NULL, 0, NULL, NULL);
	
	if (pce == NULL || *pce == NULL) {
		UNLOCK_MOUNT_R(cm);
		UNLOCK_CACHE_R();
		return 0;
	}
	
	ce = *pce;

	UNLOCK_MOUNT_R(cm);

	if (ce->ce_flags & CE_ABORTED) {
		UNLOCK_CACHE_R();
		return 0;
	}
	
	if (ce->ce_flags & CE_LOWPRI && 
		!(ce->ce_flags & CE_IODONE)) {
		UNLOCK_CACHE_R();
		return 0;
	}
	
	UNLOCK_CACHE_R();
	return 1;
}

/*
 * Find a cache extent containing or intersecting the offset/length.
 *
 * We need to be able to find both containing and intersecting
 * extents.  Rather than handle each seperately, we take advantage
 * of the fact that any containing extent will also be an intersecting
 * extent.
 *
 * Thus, a search for a containing extent should be a search for an
 * intersecting extent followed by a test for containment.
 *
 * Containment may include multiple extents. The returned extent will
 * be the first of these extents (lowest offset), but the next extents
 * in the array may also be required to contain the requested range.
 * Upon success, *pnum_extents contains the number of extents required
 * to contain the range.
 *
 * If require_crypto_match is true, the caller must already have verified
 * that the BC_cache_mount is encrypted
 *
 * Returns a pointer to a matching the entry in the mount's extent list
 * or NULL if no match is found.
 *
 * Called with the cache mount read lock held
 */
static struct BC_cache_extent **
BC_find_extent(struct BC_cache_mount *cm, u_int64_t offset, u_int64_t length, u_int64_t crypto_offset, int require_crypto_match, int* failed_due_to_crypto_mismatch, int contained, int* failed_with_intersection, int* pnum_extents)
{
	if (failed_due_to_crypto_mismatch) {
		*failed_due_to_crypto_mismatch = 0;
	}
	if (failed_with_intersection) {
		*failed_with_intersection = 0;
	}

	struct BC_cache_extent **p, **base, **last, **next, **prev; /* pointers into the mount's extent index list */
	size_t lim;
	
	/* invariants */
	assert(length > 0);
	
	base = cm->cm_pextents;
	last = base + cm->cm_nextents - 1;
	
	/*
	 * If the cache is inactive, exit.
	 */
	if (!(BC_cache->c_flags & BC_FLAG_CACHEACTIVE) || (base == NULL)) {
		return(NULL);
	}
	
	/*
	 * Range-check against the cache first.
	 *
	 * Note that we check for possible intersection, rather than
	 * containment.
	 */
	if (((offset + length) <= (*base)->ce_diskoffset) ||
	    (offset >= ((*last)->ce_diskoffset + (*last)->ce_length))) {
		return(NULL);
	}
	
	/*
	 * Perform a binary search for an extent intersecting
	 * the offset and length.
	 *
	 * This code is based on bsearch(3), and the description following
	 * is taken directly from it.
	 *
	 * The code below is a bit sneaky.  After a comparison fails, we
	 * divide the work in half by moving either left or right. If lim
	 * is odd, moving left simply involves halving lim: e.g., when lim
	 * is 5 we look at item 2, so we change lim to 2 so that we will
	 * look at items 0 & 1.  If lim is even, the same applies.  If lim
	 * is odd, moving right again involes halving lim, this time moving
	 * the base up one item past p: e.g., when lim is 5 we change base
	 * to item 3 and make lim 2 so that we will look at items 3 and 4.
	 * If lim is even, however, we have to shrink it by one before
	 * halving: e.g., when lim is 4, we still looked at item 2, so we
	 * have to make lim 3, then halve, obtaining 1, so that we will only
	 * look at item 3.
	 */
	for (lim = cm->cm_nextents; lim != 0; lim >>= 1) {
		p = base + (lim >> 1);
		
		if (BC_check_intersection((*p), offset, length)) {
			
			if ((require_crypto_match) &&
				((offset - (*p)->ce_diskoffset) != (crypto_offset - (*p)->ce_crypto_offset))) {
				if (BC_cache->c_take_detailed_stats) {
					xdebug("Read 0x%llx:%#llx on mount %s intersected, but has different crypto_offset: %#llx vs %#llx", offset, length, uuid_string(cm->cm_uuid), (offset - (*p)->ce_diskoffset), (crypto_offset - (*p)->ce_crypto_offset));
				}
				if (failed_due_to_crypto_mismatch) {
					*failed_due_to_crypto_mismatch = 1;
				}
				return NULL;
			}
			
			if (contained == 0)
				return(p);
			
			if (pnum_extents) {
				*pnum_extents = 1;
			}
			
			/*
			 * We are checking for containment, verify that here.
			 * Extents may be adjacent, so include neighboring
			 * extents in the containment check
			 */
			
			/* Check the high bound */
			if ((offset + length) > ((*p)->ce_diskoffset + (*p)->ce_length)) {
				
				/* check higher extents to see if they contain the high end of this range */
				prev = p;
				for (next = prev + 1;
					 next <= last;
					 next++) {
					
					/* extents within a mount should never overlap */
					assert(((*prev)->ce_diskoffset + (*prev)->ce_length) <= (*next)->ce_diskoffset);

					if ((require_crypto_match) &&
						((offset - (*next)->ce_diskoffset) != (crypto_offset - (*next)->ce_crypto_offset))) {
						if (BC_cache->c_take_detailed_stats) {
							xdebug("Read 0x%llx:%#llx on mount %s intersected, but has different crypto_offset for part of the range: %#llx vs %#llx", offset, length, uuid_string(cm->cm_uuid), (offset - (*next)->ce_diskoffset), (crypto_offset - (*next)->ce_crypto_offset));
						}
						if (failed_due_to_crypto_mismatch) {
							*failed_due_to_crypto_mismatch = 1;
						}
						if (failed_with_intersection) {
							*failed_with_intersection = 1;
						}
						return NULL;
					}
					
					if (((*prev)->ce_diskoffset + (*prev)->ce_length) < (*next)->ce_diskoffset) {
						/* extents are not adjacent */
						if (BC_cache->c_take_detailed_stats) {
							xdebug("Read 0x%llx:%#llx on mount %s intersected, but not contained by %d-extent cache range 0x%llx,%#llx (missed last %#llx bytes)", offset, length, uuid_string(cm->cm_uuid), *pnum_extents, (*p)->ce_diskoffset, ((*prev)->ce_diskoffset + (*prev)->ce_length), (offset + length) - ((*prev)->ce_diskoffset + (*prev)->ce_length));
						}
						if (failed_with_intersection) {
							*failed_with_intersection = 1;
						}
						return(NULL);
					}
					
					if (pnum_extents) {
						(*pnum_extents)++;
					}					
					
					if ((offset + length) <= ((*next)->ce_diskoffset + (*next)->ce_length)) {
						/* we contain the high end of the range, break so we check the low end below */
						break;
					}
					
					prev = next;
				}
				
				if (next > last) {
					/* ran off the end of the extent list */
					if (BC_cache->c_take_detailed_stats) {
						xdebug("Read 0x%llx:%#llx on mount %s intersected, but not contained by %d-extent cache range 0x%llx,%#llx (missed last %#llx bytes)", offset, length, uuid_string(cm->cm_uuid), *pnum_extents, (*p)->ce_diskoffset, ((*prev)->ce_diskoffset + (*prev)->ce_length), (offset + length) - ((*prev)->ce_diskoffset + (*prev)->ce_length));
					}
					if (failed_with_intersection) {
						*failed_with_intersection = 1;
					}
					return (NULL);
				}
			}
			
			/* Check the low bound */
			if (offset < (*p)->ce_diskoffset) {
				
				/* check lower extents to see if they contain the low end of this range */
				next = p;
				for (prev = next - 1;
					 prev >= base;
					 prev--) {
					
					/* extents within a mount should never overlap */
					assert(((*prev)->ce_diskoffset + (*prev)->ce_length) <= (*next)->ce_diskoffset);
					
					if ((require_crypto_match) &&
						((offset - (*prev)->ce_diskoffset) != (crypto_offset - (*prev)->ce_crypto_offset))) {
						if (BC_cache->c_take_detailed_stats) {
							xdebug("Read 0x%llx:%#llx on mount %s intersected, but has different crypto_offset for part of the range: %#llx vs %#llx", offset, length, uuid_string(cm->cm_uuid), (offset - (*prev)->ce_diskoffset), (crypto_offset - (*prev)->ce_crypto_offset));
						}
						if (failed_due_to_crypto_mismatch) {
							*failed_due_to_crypto_mismatch = 1;
						}
						if (failed_with_intersection) {
							*failed_with_intersection = 1;
						}
						return NULL;
					}

					if (((*prev)->ce_diskoffset + (*prev)->ce_length) < (*next)->ce_diskoffset) {
						/* extents are not adjacent */
						if (BC_cache->c_take_detailed_stats) {
							xdebug("Read 0x%llx:%#llx on mount %s intersected, but not contained by %d-extent cache range 0x%llx:%#llx (missed first %#llx bytes)", offset, length, uuid_string(cm->cm_uuid), *pnum_extents, (*next)->ce_diskoffset, ((*p)->ce_diskoffset + (*p)->ce_length), (*next)->ce_diskoffset - offset);
						}
						if (failed_with_intersection) {
							*failed_with_intersection = 1;
						}
						return(NULL);
					}
					
					if (pnum_extents) {
						(*pnum_extents)++;
					}										
					
					if (offset >= (*prev)->ce_diskoffset) {
						/* we contain the low end of the range (we checked high end above) */
						return(prev);
					}
					
					next = prev;
				}
				
				assert(prev < base); /* only break condition */
				
				if (prev < base) {
					/* ran off the end of the extent list */
					if (BC_cache->c_take_detailed_stats) {
						xdebug("Read 0x%llx:%#llx on mount %s intersected, but not contained by %d-extent cache range 0x%llx:%#llx (missed first %#llx bytes)", offset, length, uuid_string(cm->cm_uuid), *pnum_extents, (*next)->ce_diskoffset, ((*p)->ce_diskoffset + (*p)->ce_length), (*next)->ce_diskoffset - offset);
					}
					if (failed_with_intersection) {
						*failed_with_intersection = 1;
					}
					return (NULL);
				}
			}
			
			return(p);
		}
		
		if (offset > (*p)->ce_diskoffset) {	/* move right */
			base = p + 1;
			lim--;
		}				/* else move left */
	}
	/* found nothing */
	return(NULL);
}

/*
 * Discard data from the cache and free pages which are no longer required.
 *
 * This should be locked against anyone testing for !vacant pages.
 *
 * We return the number of bytes we marked unused. If the value is
 * zero, the offset/length did not overap this extent.
 *
 * Called with extent lock held.
 */
static u_int64_t
BC_discard_bytes(struct BC_cache_extent *ce, u_int64_t offset, u_int64_t length)
{
	struct BC_cache_mount *cm;
	u_int64_t estart, eend, dstart, dend, nblk, base, page, i, discarded, already_vacant;
	
	/* invariants */
	assert(length > 0);
#ifdef BC_DEBUG
	lck_mtx_assert(&ce->ce_lock, LCK_MTX_ASSERT_OWNED);
#endif
	
	/*
	 * Extent has been terminated: blockmap may no longer be valid.
	 */
	if ((ce->ce_flags & CE_ABORTED) ||
		ce->ce_length == 0)
		return 0;
	
	assert(ce->ce_blockmap_arraycount > 0);
	
	cm = BC_cache->c_mounts + ce->ce_mount_idx;
	
	/*
	 * Constrain offset and length to fit this extent, return immediately if
	 * there is no overlap.
	 */
	dstart = offset;			/* convert to start/end format */
	dend = offset + length;
	assert(dend > dstart);
	estart = ce->ce_diskoffset;
	eend = ce->ce_diskoffset + ce->ce_length;
	assert(eend > estart);
	
	if (dend <= estart)			/* check for overlap */
		return(0);
	if (eend <= dstart)
		return(0);
	
	if (dstart < estart)			/* clip to extent */
		dstart = estart;
	if (dend > eend)
		dend = eend;
	
	/*
	 * Convert length in bytes to a block count.
	 */
	nblk = CB_BYTE_TO_BLOCK(cm, dend - dstart);
	
	debug_extents(ce->ce_mount_idx, ce, dstart, dend - dstart, "marking vacant");
	BC_print_extent_bitmap(ce, dstart, dend - dstart);

	/*
	 * Mark blocks vacant.  For each vacated block, check whether the page
	 * holding it can be freed.
	 *
	 * Note that this could be optimised to reduce the number of
	 * page-freeable checks, but the logic would be more complex.
	 */
	base = CB_BYTE_TO_BLOCK(cm, dstart - ce->ce_diskoffset);
	assert(base >= 0);
	discarded = 0;
	already_vacant = 0;
	assert((base + nblk - 1) < howmany(ce->ce_length, cm->cm_blocksize));
	for (i = 0; i < nblk; i++) {
		
		/* this is a no-op if the block is already gone */
		if (CB_BLOCK_PRESENT(ce, base + i)) {
			
			// If you need to search for specific addresses: debug_extents(ce->ce_mount_idx, ce, ce->ce_diskoffset + (cm->cm_blocksize * (base + i)), cm->cm_blocksize, "marked vacant");

			/* mark the block as vacant */
			CB_MARK_BLOCK_VACANT(ce, base + i);
			discarded++;
			
			page = CB_BLOCK_TO_PAGE(cm, base + i);
			if (CB_PAGE_VACANT(cm, ce, page))
				BC_free_page(ce, page);
		} else {
			already_vacant++;
		}
	}
	
	if (discarded > 0 && already_vacant > 0) {
		debug_extents(ce->ce_mount_idx, ce, dstart, dend - dstart, "%#llx bytes marked vacant (%#llx already vacant)", discarded * cm->cm_blocksize, already_vacant * cm->cm_blocksize);
	}
	BC_print_extent_bitmap(ce, dstart, dend - dstart);
	
	return(discarded * cm->cm_blocksize);
}

/*
 * Blocks in an extent can be invalidated, e.g. by a write, before the
 * reader thread fills the extent. Search for the next range of viable blocks
 * in the extent, starting from offset 'fromoffset'.
 *
 * Values are returned by out parameters:
 * 	'nextpage' takes the page containing the start of the next run, or
 * 		-1 if no more runs are to be found.
 * 	'nextoffsetinpage' takes the offset into that page that the run begins.
 * 	'nextlength' takes the length in bytes of the range, capped at maxread.
 *
 * In other words, for this blockmap, if
 * 	    *fromoffset = 3 * blocksize,
 * 	1 1 0 0 0 1 1 1   1 1 1 1 1 1 0 0
 * 	*nextpage = 0
 * 	          *nextoffset = 5 * blocksize
 * 	                            *nextlength = 6 * blocksize
 *
 * Called with the extent lock held
 */
static void 
BC_next_valid_range(struct BC_cache_mount *cm, struct BC_cache_extent *ce, u_int64_t fromoffset,
					u_int64_t *nextpage, u_int64_t *nextoffsetinpage, u_int64_t *nextlength)
{
	u_int64_t maxblks, i, nextblk = 0;
	int found = 0;
	
	maxblks = howmany(ce->ce_length, cm->cm_blocksize);
	i = CB_BYTE_TO_BLOCK(cm, fromoffset);
	/* scan for the next range of valid blocks in the extent */
	for (; i < maxblks; i++) {
		if (CB_BLOCK_PRESENT(ce, i)) {
			if (found == 0) {
				nextblk = i;
			}
			found++;
		} else {
			if (found != 0)
				break;
		}
	}
	
	if (found) {
		/* found a valid range, so convert to (page, offset, length) */
		*nextpage = CB_BLOCK_TO_PAGE(cm, nextblk);
		*nextoffsetinpage = CB_BLOCK_TO_BYTE(cm, nextblk) % PAGE_SIZE;
		*nextlength = MIN(CB_BLOCK_TO_BYTE(cm, found), cm->cm_maxread);
	} else {
		*nextpage = -1;
	}
	
	return;
}

/*
 * Test for the presence of a range of blocks in the cache.
 *
 * Called with the extent lock held.
 */
static int
BC_blocks_present(struct BC_cache_extent *ce, u_int64_t base, u_int64_t nblk)
{
	int	blk;
	
	assert(base >= 0);
	assert((base + nblk) <= howmany(ce->ce_length, BC_cache->c_mounts[ce->ce_mount_idx].cm_blocksize));
	
	for (blk = 0; blk < nblk; blk++) {
		
		if (!CB_BLOCK_PRESENT(ce, base + blk)) {
			return(0);
		}
	}
	return(1);
}

/*
 * Readahead thread.
 */
static void
BC_reader_thread(void *param0, wait_result_t param1)
{
	thread_set_thread_name(current_thread(), "BootCache reader");

	struct BC_cache_disk *cd = NULL;
	struct BC_cache_mount *cm = NULL;
	struct BC_cache_extent *ce = NULL;
	u_int64_t count, num_mounts, ce_idx, cel_idx;
	upl_t	upl;
	kern_return_t kret;
	int error = 0;
	int issuing_lowpriority_extents = 0;
	
	/* We can access the BC_cache_disk passed in freely without locks
	 * since disks are only freed up in terminate cache after it has
	 * guanteed that no reader threads are running
	 */
	cd = (struct BC_cache_disk*) param0;
	
	num_mounts = cd->cd_nmounts;
	
	const char* termination_reason = NULL;
		
	if (BC_cache->c_flags & BC_FLAG_SHUTDOWN) {
		termination_reason = "shutdown before playback began";
		goto out;
	}
	
	for (;;) {
				
		if (issuing_lowpriority_extents) {
			debug("disk %d starting low priority batch", cd->cd_disk_num);
			KERNEL_DEBUG_CONSTANT(FSDBG_CODE(DBG_BOOTCACHE, DBG_BC_BATCH) |
								  DBG_FUNC_START,
								  ULONG_MAX, cd->cd_disk_num, 0, 0, 0);
		} else {
			debug("disk %d starting batch %d", cd->cd_disk_num, cd->cd_batch);
			KERNEL_DEBUG_CONSTANT(FSDBG_CODE(DBG_BOOTCACHE, DBG_BC_BATCH) | 
								  DBG_FUNC_START,
								  cd->cd_batch, cd->cd_disk_num, 0, 0, 0);
		}
		u_int64_t batchstart = mach_absolute_time();
		
		LOCK_CACHE_R();
		for (cel_idx = 0;
			 cel_idx < BC_cache->c_nextentlists;
			 cel_idx++) {
			
			/* iterate over extents to populate */
			for (ce_idx = 0;
				 ce_idx < BC_cache->c_nextents[cel_idx];
				 ce_idx++) {
								
				ce = BC_cache->c_extentlists[cel_idx] + ce_idx;
				
				cm = BC_cache->c_mounts + ce->ce_mount_idx;
				
				if (cm->cm_state != CM_READY) {
					continue;
				}
				
				if (cm->cm_disk != cd){
					continue;
				}
				
				/* Only read extents marked for this batch or earlier.
				 * All low-priority IO is read in one batch, however.
				 */
				if (ce->ce_batch > cd->cd_batch && !(cd->cd_flags & CD_ISSUE_LOWPRI)) {
					continue;
				}
								
				/* Check if already done or aborted */
				if (ce->ce_flags & (CE_ABORTED | CE_IODONE)) {
					continue;
				}
				
				/* Check if this extent is low priority and we're not yet reading in low-priority extents */
				if (!(cd->cd_flags & CD_ISSUE_LOWPRI) &&
					(ce->ce_flags & CE_LOWPRI)) {
					continue;
				}
				
				/* Shouldn't happen */
				if((cd->cd_flags & CD_ISSUE_LOWPRI) &&
				   !(ce->ce_flags & CE_LOWPRI)) {
					debug("Saw a regular extent while issuing throttled IO");
				}
				
				LOCK_EXTENT(ce);
				
				bool is_shared = (ce->ce_flags & CE_SHARED) != 0;
				
				/* Check if again with the lock */
				if (ce->ce_flags & (CE_ABORTED | CE_IODONE)) {
					UNLOCK_EXTENT(ce);
					continue;
				}
				
				/* loop reading to fill this extent */
				u_int64_t fromoffset = 0;
				
				LOCK_MOUNT_R(cm);
				
				if (cm->cm_state != CM_READY) {
					/* the mount was aborted. Whoever aborted it handles aborting the extents as well */
					UNLOCK_MOUNT_R(cm);
					UNLOCK_EXTENT(ce);
					continue;
				}
				
				u_int64_t io_start_time = mach_absolute_time();
				
				for (;;) {
					u_int64_t nextpage;
					u_int64_t nextoffsetinpage;
					u_int64_t nextlength;
					
					/* requested shutdown */
					if (BC_cache->c_flags & BC_FLAG_SHUTDOWN) {
						UNLOCK_MOUNT_R(cm);
						// termination reason is set by whoever set the shutdown flag
						goto out;
					}
					
					
					unsigned int pressure_level = kVMPressureNormal;
					kern_return_t ret = mach_vm_pressure_level_monitor(false, &pressure_level);
					if (ret == KERN_SUCCESS) {
						if (pressure_level != kVMPressureNormal) {
							UNLOCK_MOUNT_R(cm);
							message("Stopping playback due to memory pressure level %d", pressure_level);
							termination_reason = "memory pressure";
							goto out;
						}
					} else {
						static bool logged_pressure_err = false;
						if (!logged_pressure_err) {
							message("Got error checking memory pressure: %d", ret);
							logged_pressure_err = true;
						}
					}
					
					/*
					 * Find the next set of blocks that haven't been invalidated
					 * for this extent.
					 */
					BC_next_valid_range(cm, ce, fromoffset, &nextpage, &nextoffsetinpage, &nextlength);
					/* no more blocks to be read */
					if (nextpage == -1)
						break;
					
					/* set up fromoffset to read the next segment of the extent */
					fromoffset = (nextpage * PAGE_SIZE) + nextoffsetinpage + nextlength;
					
					kret = vnode_getwithvid(BC_cache->c_vp, BC_cache->c_vid);
					if (kret != KERN_SUCCESS) {
						UNLOCK_MOUNT_R(cm);
						message("reader thread: vnode_getwithvid failed - %d\n", kret);
						termination_reason = "vnode_getwithvid failed";
						goto out;
					}
					
					kret = ubc_create_upl(BC_cache->c_vp, 
										  ce->ce_cacheoffset + (nextpage * PAGE_SIZE), 
										  (int) roundup(nextoffsetinpage + nextlength, PAGE_SIZE),
										  &upl, 
										  NULL, 
										  UPL_SET_LITE|UPL_FILE_IO);
					if (kret != KERN_SUCCESS) {
						UNLOCK_MOUNT_R(cm);
						message("ubc_create_upl returned %d\n", kret);
						(void) vnode_put(BC_cache->c_vp);
						termination_reason = "upl creation failed";
						goto out;
					}
					
					/* cm_bp is only accessed from the reader thread,
					 * and there's only one reader thread for a given mount,
					 * so we don't need the write lock to modify it */
					
					/* set buf to fill the requested subset of the upl */
					buf_setblkno(cm->cm_bp, CB_BYTE_TO_BLOCK(cm, ce->ce_diskoffset + nextoffsetinpage) + CB_PAGE_TO_BLOCK(cm, nextpage));
					buf_setcount(cm->cm_bp, (unsigned int) nextlength);
					buf_setupl(cm->cm_bp, upl, (unsigned int) nextoffsetinpage);
					buf_setresid(cm->cm_bp, buf_count(cm->cm_bp));	/* ask for residual indication */
					buf_reset(cm->cm_bp, B_READ);

					if (cm->cm_fs_flags & BC_FS_APFS_ENCRYPTED) {
						/* set bufattr crypto_offset */
						bufattr_setcpoff(buf_attr(cm->cm_bp), ce->ce_crypto_offset + nextoffsetinpage + (nextpage * PAGE_SIZE));
					}

					/* If this is regular readahead, make sure any throttled IO are throttled by the readahead thread rdar://8592635
					 * If this is low-priority readahead, make sure this thread will be throttled appropriately later rdar://8734858
					 */
					throttle_info_handle_t throttle_info_handle;
					if ((error = throttle_info_ref_by_mask(cm->cm_throttle_mask, &throttle_info_handle)) == 0) {
						throttle_info_update_by_mask(throttle_info_handle, 0x0);
						throttle_info_rel_by_mask(throttle_info_handle);
					} else {
						(void)error;
						debug("Unable to update throttle info for mount %s: %d", uuid_string(cm->cm_uuid), error);
					}					
					
					BC_ADD_STAT(is_shared, initiated_reads, 1);
					if (cd->cd_disk_num < STAT_DISKMAX) {
						if (ce->ce_flags & CE_LOWPRI) {
							BC_ADD_STAT(is_shared, batch_initiated_reads_lowpri[cd->cd_disk_num], 1);
						} else {
							BC_ADD_STAT(is_shared, batch_initiated_reads[cd->cd_disk_num][MIN(cd->cd_batch, STAT_BATCHMAX)], 1);
						}
					}
					
					/* give the buf to the underlying strategy routine */
					KERNEL_DEBUG_CONSTANT(FSDBG_CODE(DBG_DKRW, DKIO_READ) | DBG_FUNC_NONE,
										  (uintptr_t) buf_kernel_addrperm_addr(cm->cm_bp), buf_device(cm->cm_bp), (long)buf_blkno(cm->cm_bp), buf_count(cm->cm_bp), 0);
					KERNEL_DEBUG_CONSTANT(FSDBG_CODE(DBG_BOOTCACHE, DBG_BC_PLAYBACK_IO) | DBG_FUNC_NONE, buf_kernel_addrperm_addr(cm->cm_bp), 0, 0, 0, 0);
					
					BC_cache->c_strategy(cm->cm_bp);
					
					/* wait for the bio to complete */
					buf_biowait(cm->cm_bp);
					
					kret = ubc_upl_commit(upl);
					(void) vnode_put(BC_cache->c_vp);
					
					/*
					 * If the read or commit returned an error, invalidate the bytes
					 * covered by the read (on a residual, we could avoid invalidating
					 * bytes that are claimed to be read as a minor optimisation, but we do
					 * not expect errors as a matter of course).
					 */
					if (kret != KERN_SUCCESS || buf_error(cm->cm_bp) || (buf_resid(cm->cm_bp) != 0)) {
						debug("%s read error: ret %d error %d resid %d "
							  "extent %lu %lu/%lu "
							  "(error buf %ld/%ld flags %08x)",
							  uuid_string(cm->cm_uuid),
							  kret, buf_error(cm->cm_bp), buf_resid(cm->cm_bp),
							  (unsigned long) ce_idx,
							  (unsigned long)ce->ce_diskoffset, (unsigned long)ce->ce_length,
							  (long)buf_blkno(cm->cm_bp), (long)buf_count(cm->cm_bp),
							  buf_flags(cm->cm_bp));
						
						debug_extents(ce->ce_mount_idx, ce, ce->ce_diskoffset + (nextpage * PAGE_SIZE) + nextoffsetinpage, nextlength, "read error");
						count = BC_discard_bytes(ce, ce->ce_diskoffset + (nextpage * PAGE_SIZE) + nextoffsetinpage, nextlength);
						debug("read error: discarded %llu bytes", count);
						BC_ADD_STAT(is_shared, read_errors, 1);
						BC_ADD_STAT(is_shared, read_errors_bytes, count);
					}

					/* I'd like to throttle the thread here, but I'm holding the extent and cache locks, so it's throttled below */
				}
				
				/* update stats */
				u_int64_t io_duration = mach_absolute_time() - io_start_time;
				if (ce->ce_flags & CE_LOWPRI) {
					BC_ADD_STAT(is_shared, batch_time_lowpri[cd->cd_disk_num], io_duration);
				} else {
					BC_ADD_STAT(is_shared, batch_time[cd->cd_disk_num][MIN(cd->cd_batch, STAT_BATCHMAX)], io_duration);
				}
				BC_ADD_STAT(is_shared, read_blocks, (u_int) CB_BYTE_TO_BLOCK(cm, ce->ce_length));
				BC_ADD_STAT(is_shared, read_bytes,  (u_int) ce->ce_length);
				if (ce->ce_flags & CE_LOWPRI) {
					BC_ADD_STAT(is_shared, read_bytes_lowpri,  (u_int) ce->ce_length);
					if (cd->cd_disk_num < STAT_DISKMAX) {
						BC_ADD_STAT(is_shared, batch_bytes_lowpri[cd->cd_disk_num],  (u_int) ce->ce_length);
					}
				} else {
					if (cd->cd_disk_num < STAT_DISKMAX) {
						BC_ADD_STAT(is_shared, batch_bytes[cd->cd_disk_num][MIN(cd->cd_batch, STAT_BATCHMAX)],  (u_int) ce->ce_length);
						if (ce->ce_batch < cd->cd_batch) {
							BC_ADD_STAT(is_shared, batch_late_bytes[cd->cd_disk_num][MIN(cd->cd_batch, STAT_BATCHMAX)], ce->ce_length);
						}
					}
				}


				debug_extents(ce->ce_mount_idx, ce, 0, 0, "done");
				UNLOCK_MOUNT_R(cm);
				
				/*
				 * Wake up anyone wanting this extent, as it is now ready for
				 * use.
				 */
				ce->ce_flags |= CE_IODONE;
				UNLOCK_EXTENT(ce);
				wakeup(ce);
				
				/* Let anyone waiting for the write lock to take hold */
				UNLOCK_CACHE_R();

				/* Take this opportunity of locklessness to throttle the reader thread, if necessary */
				if (issuing_lowpriority_extents) {
					throttle_lowpri_io((ce->ce_flags & CE_LOWPRI) ? 1 : 0);
				}
				
				LOCK_CACHE_R();
				if (issuing_lowpriority_extents && !(cd->cd_flags & CD_ISSUE_LOWPRI)) {
					/* New playlist arrived, go back to issuing regular priority extents */
					debug("New extents, interrupting low priority readahead");
					break;
				}

				if (cel_idx >= BC_cache->c_nextentlists ||
					ce_idx  >= BC_cache->c_nextents[cel_idx]) {
					/* cache shrunk while we weren't looking.
					 * Not a hard error, but we're done with this batch at least
					 */
					break;
				}
			}
			ce = NULL;
			if (issuing_lowpriority_extents && !(cd->cd_flags & CD_ISSUE_LOWPRI)) {
				/* New playlist arrived, go back to issuing regular priority extents */
				break;
			}
		}
		UNLOCK_CACHE_R();
		
		u_int64_t batchduration = mach_absolute_time() - batchstart;
		if (cd->cd_disk_num < STAT_DISKMAX) {
			if (issuing_lowpriority_extents) {
				BC_ADD_UNKNOWN_STAT(batch_time_lowpri[cd->cd_disk_num], batchduration);
			} else {
				/* Measure times for the first several batches separately */
				BC_ADD_UNKNOWN_STAT(batch_time[cd->cd_disk_num][MIN(cd->cd_batch, STAT_BATCHMAX)], batchduration);
			}
		}
		if (issuing_lowpriority_extents) {
			// debug("disk %d low priority batch done", cd->cd_disk_num);
			KERNEL_DEBUG_CONSTANT(FSDBG_CODE(DBG_BOOTCACHE, DBG_BC_BATCH) |
								  DBG_FUNC_END,
								  ULONG_MAX, cd->cd_disk_num, 0, 0, 0);
		} else {
			// debug("disk %d batch %d done", cd->cd_disk_num, cd->cd_batch);
			KERNEL_DEBUG_CONSTANT(FSDBG_CODE(DBG_BOOTCACHE, DBG_BC_BATCH) | 
								  DBG_FUNC_END,
								  cd->cd_batch, cd->cd_disk_num, 0, 0, 0);
		}
		

		LOCK_DISK(cd);
		if (issuing_lowpriority_extents && !(cd->cd_flags & CD_ISSUE_LOWPRI)) {
			/* New playlist arrived, go back to issuing regular extents */
			issuing_lowpriority_extents = 0;
			throttle_set_thread_io_policy(IOPOL_NORMAL);
		} else {
			if (!issuing_lowpriority_extents) {
				// Always increment batch number so we don't add new numbers to old batches when new mounts appear
				cd->cd_batch++;
			}
			
			if (cd->cd_batch >= BC_cache->c_batch_count || issuing_lowpriority_extents) {
				// We're done with the high priority work
				
				if (num_mounts == cd->cd_nmounts) {
					/* no new mounts appeared */
					if ( !(cd->cd_flags & CD_ISSUE_LOWPRI)) {
						/* go through the playlist again, issuing any low-priority extents we have */
						cd->cd_flags |= CD_ISSUE_LOWPRI;
						throttle_set_thread_io_policy(IOPOL_THROTTLE);
						issuing_lowpriority_extents = 1;
					} else {
						/* we're done */
						cd->cd_flags &= (~CD_HAS_THREAD);
						UNLOCK_DISK(cd);
						termination_reason = "playback completed successfully";
						goto out;
					}
				} else {
					/* New mounts appeared. Run through again */
					debug("disk %d more mounts detected (%d total)", cd->cd_disk_num, cd->cd_nmounts);
				}
			}
		}
		num_mounts = cd->cd_nmounts;
		UNLOCK_DISK(cd);
	}

out:
	/*
	 * If ce != NULL we have bailed out, but we cannot free memory beyond
	 * the bailout point as it may have been read in a previous batch and 
	 * there may be a sleeping strategy routine assuming that it will 
	 * remain valid.  Since we are typically killed immediately before a 
	 * complete cache termination, this does not represent a significant 
	 * problem.
	 *
	 * However, to prevent readers blocking waiting for readahead to
	 * complete for extents that will never be read, we mark all the
	 * extents we have given up on as aborted.
	 */
	if (ce != NULL) {
		UNLOCK_EXTENT(ce);
		
		/* the only errors that get us here are global, so mark the cache as shut down */
		LOCK_DISK(cd);
		BC_set_flag(BC_FLAG_SHUTDOWN);
		cd->cd_flags &= (~CD_HAS_THREAD);
		UNLOCK_DISK(cd);
		
		debug("reader thread failed, throwing out all remaining extents");

		int tmpcel_idx, tmpce_idx;
		int nonshared_discards = 0;
		int shared_discards = 0;
		for (tmpcel_idx = 0;
			 tmpcel_idx < BC_cache->c_nextentlists;
			 tmpcel_idx++) {
			for (tmpce_idx = 0;
				 tmpce_idx < BC_cache->c_nextents[tmpcel_idx];
				 tmpce_idx++) {
				ce = BC_cache->c_extentlists[tmpcel_idx] + tmpce_idx;
				LOCK_EXTENT(ce);
				/* abort any extent that hasn't been satisfied */
				if (!(ce->ce_flags & CE_IODONE)) {
					if (!(ce->ce_flags & CE_ABORTED)) {
						if (ce->ce_blockmap_arraycount > 0) {
							u_int64_t numdiscards = BC_discard_bytes(ce, ce->ce_diskoffset, ce->ce_length);
							if (ce->ce_flags & CE_SHARED) {
								shared_discards += numdiscards;
							} else {
								nonshared_discards += numdiscards;
							}
						} else {
							if (ce->ce_flags & CE_SHARED) {
								shared_discards += ce->ce_length;
							} else {
								nonshared_discards += ce->ce_length;
							}
						}
					}
					BC_teardown_extent(ce);
					/* wake up anyone asleep on this extent */
					UNLOCK_EXTENT(ce);
					wakeup(ce);
				} else {
					UNLOCK_EXTENT(ce);
				}
			}
		}
		BC_ADD_NON_SHARED_CACHE_STAT(readerror_unread, nonshared_discards);
		BC_ADD_SHARED_CACHE_STAT(readerror_unread, shared_discards);
		UNLOCK_CACHE_R();
	}
	
	if (termination_reason) {
		strncpy(BC_cache->c_stats.ss_playback_end_reason, termination_reason, sizeof(BC_cache->c_stats.ss_playback_end_reason));
	}

	debug("disk %d done", cd->cd_disk_num);
	
	/* wake up someone that might be waiting for this reader thread to exit */
	wakeup(&cd->cd_flags);
	
	LOCK_READERS();
	/* wake up someone that might be waiting for all the reader threads to exit */
	BC_cache->c_num_reader_threads--;
	if (BC_cache->c_num_reader_threads == 0) {
		wakeup(&BC_cache->c_num_reader_threads);
	}
	UNLOCK_READERS();
	
}

/*
 * Handle an incoming close request.
 */
static int
BC_close(dev_t dev, int flags, int devtype, struct proc *p)
{
	struct BC_cache_mount *cm;
	int i, cm_idx;

	/* Mark our cache_mount for this device as invalid */

	int nonshared_discards = 0;
	int shared_discards = 0;
	int nonshared_unread = 0;
	int shared_unread = 0;
	LOCK_CACHE_R();
	if (BC_cache->c_flags & BC_FLAG_CACHEACTIVE) {
		
		cm_idx = BC_find_cache_mount(dev);
		if (-1 != cm_idx) {
			cm = BC_cache->c_mounts + cm_idx;
			debug("Tearing down closing mount %s with dev 0x%x", uuid_string(cm->cm_uuid), dev);
			LOCK_MOUNT_R(cm);
			if (cm->cm_state != CM_ABORTED) {
				/*
				 * Mark all extents as aborted. This will notify any sleepers in the 
				 * strategy routine that the extent won't have data for them.
				 *
				 * Note that we shouldn't try to take an extent lock while holding
				 * the exclusive mount lock or we risk deadlock with the reader thread
				 */
				
				debug_extents(cm_idx, NULL, 0, 0, "closing, removing remaining extents");
				for (i = 0; i < cm->cm_nextents; i++) {
					LOCK_EXTENT(cm->cm_pextents[i]);
					if (! (cm->cm_pextents[i]->ce_flags & CE_ABORTED)) {
						u_int64_t num_bytes = 0;
						if (cm->cm_pextents[i]->ce_blockmap_arraycount > 0) {
							num_bytes = BC_discard_bytes(cm->cm_pextents[i], cm->cm_pextents[i]->ce_diskoffset, cm->cm_pextents[i]->ce_length);
						} else {
							num_bytes = cm->cm_pextents[i]->ce_length;
						}
						if (cm->cm_pextents[i]->ce_flags & CE_IODONE) {
							if (cm->cm_pextents[i]->ce_flags & CE_SHARED) {
								shared_discards += num_bytes;
							} else {
								nonshared_discards += num_bytes;
							}
						} else {
							if (cm->cm_pextents[i]->ce_flags & CE_SHARED) {
								shared_unread += num_bytes;
							} else {
								nonshared_unread += num_bytes;
							}
						}
					}
					BC_teardown_extent(cm->cm_pextents[i]);
					UNLOCK_EXTENT(cm->cm_pextents[i]);
					wakeup(cm->cm_pextents[i]);
				}
				if (! LOCK_MOUNT_R_TO_W(cm)) {
					/* If there is someone waiting for the exclusive lock on this mount,
					 * we want to grab it before they can so they don't waste time.
					 * If we fail to take the exclusive lock it means someone
					 * else is also trying to upgrate the mount lock. We don't keep any
					 * state, so we can simply take the write lock again.
					 */
					LOCK_MOUNT_W(cm);
					if (cm->cm_state == CM_ABORTED) {
						// Mount is aborted, no more work necessary
						UNLOCK_MOUNT_W(cm);
					} else {
						BC_teardown_mount(cm);
						UNLOCK_MOUNT_W(cm);
					}
				} else {
					BC_teardown_mount(cm);
					UNLOCK_MOUNT_W(cm);
				}
			} else {
				UNLOCK_MOUNT_R(cm);
			}
		}
		
		LOCK_NONCACHED_MOUNTS_W();
		for (int noncached_mount_idx = 0; noncached_mount_idx < BC_cache->c_noncached_nmounts; noncached_mount_idx++) {
			if (BC_cache->c_noncached_mounts[noncached_mount_idx].nm_dev == dev) {
				
				int newSize = BC_cache->c_noncached_nmounts - 1;
				if (newSize > 0) {
					struct BC_noncached_mount* shrunkArray;
					BC_MALLOC_ARRAY_NOPOINTERS(shrunkArray, newSize);
					
					if (shrunkArray) {
						
						// Copy all noncached mounts before noncached_mount_idx
						memmove(shrunkArray, BC_cache->c_noncached_mounts, sizeof(*shrunkArray) * noncached_mount_idx);
						
						// Copy all noncached mounts after noncached_mount_idx
						int numMountsAfterwards = BC_cache->c_noncached_nmounts - (noncached_mount_idx + 1);
						if (numMountsAfterwards > 0) {
							memmove(shrunkArray + noncached_mount_idx, BC_cache->c_noncached_mounts + (noncached_mount_idx + 1), sizeof(*shrunkArray) * numMountsAfterwards);
						}
						
						debug("Removed dev %#x -> group %s at index %d", BC_cache->c_noncached_mounts[noncached_mount_idx].nm_dev, uuid_string(BC_cache->c_noncached_mounts[noncached_mount_idx].nm_group_uuid), noncached_mount_idx);
						
						BC_FREE_ARRAY_NOPOINTERS(BC_cache->c_noncached_mounts, BC_cache->c_noncached_nmounts);
						BC_cache->c_noncached_mounts = shrunkArray;
						BC_cache->c_noncached_nmounts = newSize;
						
					} else {
						debug("Cleared dev %#x -> group %s at index %d", BC_cache->c_noncached_mounts[noncached_mount_idx].nm_dev, uuid_string(BC_cache->c_noncached_mounts[noncached_mount_idx].nm_group_uuid), noncached_mount_idx);
						// Unable to shrink array, just clear the entry
						BC_cache->c_noncached_mounts[noncached_mount_idx].nm_dev = 0;
					}
				} else {
					debug("Removed last dev %#x -> group %s at index %d", BC_cache->c_noncached_mounts[noncached_mount_idx].nm_dev, uuid_string(BC_cache->c_noncached_mounts[noncached_mount_idx].nm_group_uuid), noncached_mount_idx);
					BC_FREE_ARRAY_NOPOINTERS(BC_cache->c_noncached_mounts, BC_cache->c_noncached_nmounts);
					BC_cache->c_noncached_nmounts = 0;
				}
				
				break;
			}
		}
		UNLOCK_NONCACHED_MOUNTS_W();

	}
	
	BC_ADD_NON_SHARED_CACHE_STAT(close_discards, nonshared_discards);
	BC_ADD_SHARED_CACHE_STAT(close_discards, shared_discards);
	BC_ADD_NON_SHARED_CACHE_STAT(close_unread, nonshared_unread);
	BC_ADD_SHARED_CACHE_STAT(close_unread, shared_unread);
	
	UNLOCK_CACHE_R();
	
	return BC_cache->c_close(dev, flags, devtype, p);
}

/*
 * Returns CE_ABORTED if any of the extents in the array are aborted
 * Returns CE_LOWPRI  if any of the extents in the array are low priority and not done
 * Returns CE_IODONE  if all the extents in the array are done
 * Returns 0 otherwise
 */
static int extents_status(struct BC_cache_extent *ce_firstmatch, struct BC_cache_extent **pce_additional_extents, int nextents) {
	int ce_idx;
	int ret = CE_IODONE;
	struct BC_cache_extent *ce;
	
	
	for (ce_idx = 0;
		 ce_idx < nextents;
		 ce_idx++) {
		
		ce = (ce_idx == 0) ? ce_firstmatch : pce_additional_extents[ce_idx - 1];
		
		if (ce->ce_flags & CE_ABORTED) {
			return CE_ABORTED;
		}
		
		if (! (ce->ce_flags & CE_IODONE)) {
			if (ce->ce_flags & CE_LOWPRI) {
				ret = CE_LOWPRI;
			} else {
				if (ret == CE_IODONE) {
					ret = 0;
				}
			}
		}
	}
	return ret;
}

/*
 * Waits for the given extent to be filled or aborted
 *
 * Should only be called with the extent lock.
 * NO OTHER LOCKS SHOULD BE HELD INCLUDING THE CACHE LOCK
 *
 * We are guaranteed that the extent won't disappear when we release
 * the cache lock by the cleanup code waiting for BC_cache->c_strategycalls
 * to reach 0, and we're counted in there.
 */
static void wait_for_extent(struct BC_cache_extent *ce, struct timeval starttime) {
	struct timeval time;
	struct timespec timeout;
	microtime(&time);
	timersub(&time,
			 &starttime,
			 &time);
	if (time.tv_sec < BC_wait_for_readahead) {
		timeout.tv_sec = BC_wait_for_readahead - time.tv_sec;
		if (time.tv_usec == 0) {
			timeout.tv_nsec = 0;
		} else {
			timeout.tv_sec--;
			timeout.tv_nsec = NSEC_PER_USEC * (USEC_PER_SEC - time.tv_usec);
		}
		msleep(ce, &ce->ce_lock, PRIBIO, "BC_strategy", &timeout);
	}
}

/*
 * Called with the cache mount read lock held
 */
static bool bc_mount_should_throttle_cutthrough(int cm_idx, int is_swap) {
	if (BC_cache->c_readahead_throttles_cutthroughs &&
		!is_swap &&
		BC_cache->c_mounts[cm_idx].cm_disk &&
		(BC_cache->c_mounts[cm_idx].cm_disk->cd_flags & CD_HAS_THREAD) &&
		!(BC_cache->c_mounts[cm_idx].cm_disk->cd_flags & CD_ISSUE_LOWPRI) &&
		!(BC_cache->c_mounts[cm_idx].cm_disk->cd_flags & CD_IS_SSD)) {
		/* We're currently issuing readahead for this disk.
		 * Throttle this IO so we don't cut-through the readahead so much.
		 */
		return true;
	}
	return false;
}

/*
 * Handle an incoming IO request.
 */
static int
BC_strategy(struct buf *bp)
{
	struct BC_cache_extent *ce = NULL,  **pce = NULL, *ce_firstmatch = NULL, **pce_additional_extents = NULL;
	int num_extents = 0;
	uio_t uio = NULL;
	u_int64_t nbytes, discards = 0;
	struct timeval blocked_start_time, blocked_end_time;
	daddr64_t blkno;
	caddr_t buf_map_p, buf_extent_p;
	off_t disk_offset = 0;
	kern_return_t kret;
	int cm_idx = -1, ce_idx;
	dev_t dev;
	int32_t bufflags;
	int in_flight = 0, cache_active = 0, during_io = 0, take_detailed_stats = 0, cache_hit = 0;
	int cache_miss_due_to_crypto_mismatch = 0, cache_miss_with_intersection = 0;
	int is_rejected, did_block, status;
	int is_shared = 0, is_swap = 0;
	int should_throttle = 0;
	int is_ssd = 0;
	int is_stolen = 0;
	int unfilled = 0;
	vnode_t vp = NULL;
	bool vp_is_owned = false;
	int pid = 0;
	int dont_cache = 0;
	int throttle_tier = 0;
	bufattr_t bap = NULL;
	u_int64_t crypto_offset = 0;
	int is_encrypted = 0;
	int is_read = 0;
	char procname[128] = {0};

	assert(bp != NULL);
	
	nbytes = buf_count(bp);
	dev = buf_device(bp);

	/*
	 * If the buf doesn't have a device for some reason, pretend
	 * we never saw the request at all.
	 */
	if (dev == nulldev() || dev == NODEV) {
		BC_cache->c_strategy(bp);
		BC_ADD_STAT(is_shared, strategy_unknown, 1);
		BC_ADD_STAT(is_shared, strategy_unknown_bytes, nbytes);
		return 0;
	}
	
	if (BC_cache->c_flags & BC_FLAG_CACHEACTIVE) {
		cache_active = 1;
	}

	blkno = buf_blkno(bp);
	bufflags = buf_flags(bp);
	bap = buf_attr(bp);
	vp = buf_vnode(bp);
	
	if (bufflags & B_READ) {
		is_read = 1;
	}
	
	if (vp) {
		// We need to access vp after the buf_biodone / c_strategy call,
		// so take a reference out on it so it doesn't get recycled
		if (KERN_SUCCESS == vnode_get(vp)) {
			vp_is_owned = true;
		} else {
			debug("Unable to get vnode");
		}
	}
	
	if (vp && vnode_isdyldsharedcache(vp)) {
		is_shared = 1;
	}

	if (bap) {
		throttle_tier = bufattr_throttled(bap);
		crypto_offset = bufattr_cpoff(bap); // Unused if mount isn't encrypted
	}

	if (BC_cache->c_flags & BC_FLAG_HISTORYACTIVE) {
		pid = proc_selfpid();
		
	}
	
	if (vp && (vnode_israge(vp))) {
		dont_cache = 1;
	}
	
	if (vp && vnode_isswap(vp)) {
		is_swap = 1;
		
#ifdef BC_DEBUG
		if (procname[0] == '\0') {
			proc_selfname(procname, sizeof(procname));
		}
		const char* filename = vp ? vnode_getname(vp) : NULL;
		debug("%s swapfile from app %s for file %s (disk block 0x%llx)",
			  is_read ? "read from" : "write to",
			  procname,
			  filename?:"unknown",
			  blkno);
		if (filename) {
			vnode_putname(filename);
		}
#endif

		goto bypass;
	}

	/*
	 * If the cache is not active, bypass the request.  We may
	 * not be able to fill it, but we still want to record it.
	 */
	if (!cache_active) {
		goto bypass;
	}
		
	/*
	 * In order to prevent the cache cleanup code from racing with us
	 * when we sleep, we track the number of strategy calls in flight.
	 */
	OSAddAtomic(1, &BC_cache->c_strategycalls);
	LOCK_CACHE_R();
	in_flight = 1; /* we're included in the in_flight count */
	
	/* Get cache mount asap for use in case we bypass */
	cm_idx = BC_find_cache_mount(dev);
	
	if (cm_idx != -1) {
		disk_offset = CB_BLOCK_TO_BYTE(BC_cache->c_mounts + cm_idx, blkno);
		is_encrypted = (BC_cache->c_mounts[cm_idx].cm_fs_flags & BC_FS_APFS_ENCRYPTED);
		if (!is_encrypted) {
			// Ignore crypto offsets for non-encrypted volumes. Don't assert because the value may not be zero'ed out properly, but it is unused
			crypto_offset = 0;
		}
	}
	
#ifdef BC_DEBUG_BYPASSES
#define debug_bypass(ce, fmt, args...) \
({ \
	if (take_detailed_stats) { \
		if (procname[0] == '\0') { \
			proc_selfname(procname, sizeof(procname)); \
		} \
		const char* filename = vp ? vnode_getname(vp) : NULL; \
		debug_bytes(cm_idx, ce, disk_offset, nbytes, "bypassed: "fmt" (requested by %s for %s)", ## args, procname, filename?:"unknown"); \
		if (filename) { \
			vnode_putname(filename); \
		} \
	} \
})
#else
#define debug_bypass(ce, fmt, args...)
#endif

	if (BC_cache->c_take_detailed_stats) {
		take_detailed_stats = 1;
		BC_ADD_STAT(is_shared, strategy_calls, 1);
		if (throttle_tier) {
			BC_ADD_STAT(is_shared, strategy_throttled, 1);
		}
#ifdef BC_DEBUG
//		if (dont_cache) {
//			if (procname[0] == '\0') {
//				proc_selfname(procname, sizeof(procname));
//			}
//			const char* filename = vp ? vnode_getname(vp) : NULL;
//			debug("not recording %s%s from app %s for file %s (disk block 0x%llx) which is%s throttled", (vp && vnode_israge(vp)) ? "rapid age " : "", is_read ? "read" : "write", procname, filename?:"unknown", blkno, (bufflags & B_THROTTLED_IO) ? "" : " not");
//			if (filename) {
//				vnode_putname(filename);
//			}
//		}
#endif
	}
	
	/*
	 * if we haven't seen this mount before, pass it off
	 *
	 * If this is a mount we want to cache, we'll kick it off when
	 * adding to our history
	 */	
	if (cm_idx == -1) {
		/* don't have a mount for this IO */
		if (take_detailed_stats)
			BC_ADD_STAT(is_shared, strategy_noncached_mount, 1);
#ifdef BC_DEBUG_BYPASSES
		debug("No mount for dev %d for block %#llx (%#llx bytes)", dev, blkno, nbytes);
#endif
		goto bypass;
	}
	
	/* Get the info from the mount we need */
	LOCK_MOUNT_R(BC_cache->c_mounts + cm_idx);

	if (BC_cache->c_mounts[cm_idx].cm_disk &&
		(BC_cache->c_mounts[cm_idx].cm_disk->cd_flags & CD_HAS_THREAD) &&
		!(BC_cache->c_mounts[cm_idx].cm_disk->cd_flags & CD_ISSUE_LOWPRI)) {
		during_io = 1; /* for statistics */
		if (take_detailed_stats) {
			BC_ADD_STAT(is_shared, strategy_duringio, 1);
		}
	}
	
	if (bufflags & B_ENCRYPTED_IO) {
		UNLOCK_MOUNT_R(BC_cache->c_mounts + cm_idx);
		dont_cache = 1;
		debug_bypass(NULL, "encrypted IO");
		goto bypass;
	}
	
	if (BC_cache->c_mounts[cm_idx].cm_state != CM_READY) {
		/* the mount has been aborted, treat it like a missing mount */
		debug_bypass(NULL, "mount aborted");
		UNLOCK_MOUNT_R(BC_cache->c_mounts + cm_idx);
		cm_idx = -1;

		if (take_detailed_stats)
			BC_ADD_STAT(is_shared, strategy_unready_mount, 1);
		goto bypass;
	}
		
	/* if it's not a read, pass it off */
	if (!is_read) {
		UNLOCK_MOUNT_R(BC_cache->c_mounts + cm_idx);
		if (take_detailed_stats)
			BC_ADD_STAT(is_shared, strategy_nonread, 1);
		debug_bypass(NULL, "nonread");
		goto bypass;
	}	
	
	if ((nbytes % BC_cache->c_mounts[cm_idx].cm_blocksize) != 0) {
		debug_bypass(NULL, "IO with non-blocksize (%#llx) multiple", BC_cache->c_mounts[cm_idx].cm_blocksize);
		UNLOCK_MOUNT_R(BC_cache->c_mounts + cm_idx);
		if (take_detailed_stats)
			BC_ADD_STAT(is_shared, strategy_nonblocksize, 1);
		goto bypass;
	}
			
	if (take_detailed_stats) {
		BC_ADD_STAT(is_shared, extent_lookups, 1);
		BC_ADD_STAT(is_shared, requested_bytes, nbytes);
		if (cm_idx < STAT_MOUNTMAX) {
			BC_ADD_STAT(is_shared, requested_bytes_m[cm_idx], nbytes);
		}
	}
	
	/*
	 * Look for a cache extent containing this request.
	 */
	pce = BC_find_extent(BC_cache->c_mounts + cm_idx, disk_offset, nbytes, crypto_offset, is_encrypted, &cache_miss_due_to_crypto_mismatch, 1, &cache_miss_with_intersection, &num_extents);
	if (pce == NULL) {
		if (take_detailed_stats) {
			
			if (cache_miss_due_to_crypto_mismatch) {
				BC_ADD_STAT(is_shared, extent_crypto_mismatches, 1);
			}
			if (cache_miss_with_intersection) {
				BC_ADD_STAT(is_shared, extent_partial_hits, 1);
			}
			
			debug_bypass(NULL, "cache miss%s%s",
						 cache_miss_due_to_crypto_mismatch ? " due to crypto mismatch" : "",
						 cache_miss_with_intersection ? " (intersected, though)" : "");
		}
		UNLOCK_MOUNT_R(BC_cache->c_mounts + cm_idx);
		goto bypass;
	}
	
#ifdef BC_EXTRA_DEBUG
	if (!!is_shared != !!(pce[0]->ce_flags & CE_SHARED)) {
		if (procname[0] == '\0') {
			proc_selfname(procname, sizeof(procname));
		}
		const char* filename = vp ? vnode_getname(vp) : NULL;
		debug("%s %sshared cache from app %s for file %s (disk block 0x%llx), satisfied by %sshared cache",
			  is_read ? "read from" : "write to",
			  is_shared ? "" : "non-",
			  procname,
			  vp ? (filename?:"(unknown)") : "(no vp)",
			  blkno,
			  (pce[0]->ce_flags & CE_SHARED) ? "" : "non-");
		if (filename) {
			vnode_putname(filename);
		}
	}
#endif

	assert(num_extents > 0);
	
	cache_hit = 1;
	
	if (take_detailed_stats) {
		BC_ADD_STAT(is_shared, extent_hits, 1);
		if (during_io)
			BC_ADD_STAT(is_shared, hit_duringio, 1);
		if (num_extents > 1)
			BC_ADD_STAT(is_shared, hit_multiple, 1);
	}
	
	// pce is pointing into the mount's extent list, which may disappear while we're waiting for the extent to complete, so we need to keep a local copy of the extent pointers (the extents themsleves won't change even if the mount's extent list does)
	ce_firstmatch = *pce;
	
	// Avoid allocating memory in the (vastly) common case of a single extent match
	if (num_extents > 1) {
		BC_MALLOC_ARRAY_WITHPOINTERS(pce_additional_extents, num_extents - 1);
		if (pce_additional_extents == NULL) {
			UNLOCK_MOUNT_R(BC_cache->c_mounts + cm_idx);
			if (take_detailed_stats)
				BC_ADD_STAT(is_shared, hit_failure, 1);
			debug_bypass(ce_firstmatch, "unable to allocate memory for %d pointers", num_extents - 1);
			goto bypass;
		}
		memmove(pce_additional_extents, pce + 1, (num_extents - 1) * sizeof(*pce_additional_extents));
	}
	
	is_ssd = (BC_cache->c_mounts[cm_idx].cm_disk && 
			 (BC_cache->c_mounts[cm_idx].cm_disk->cd_flags & CD_IS_SSD));
	
	UNLOCK_MOUNT_R(BC_cache->c_mounts + cm_idx);
	
	/* if any extent is aborted, bypass immediately */
	status = extents_status(ce_firstmatch, pce_additional_extents, num_extents);
	if (status & CE_ABORTED) {
		if (take_detailed_stats)
			BC_ADD_STAT(is_shared, hit_aborted, 1);
		if (num_extents == 1) {
			debug_bypass(ce_firstmatch, "extent is aborted initially");
		} else {
			debug_bypass(ce_firstmatch, "one of the extents is aborted initially");
		}
		goto bypass;
	}
	
#ifndef EMULATE_ONLY
	
	did_block = 0;
	if (! (status & CE_IODONE)) {
						
		if (is_ssd) {
			/* Don't delay IOs on SSD since cut-throughs aren't harmful */
			unfilled = 1;
			debug_bypass(ce_firstmatch, "unfilled (on ssd)");
			goto bypass;
		}
		
		if (status & CE_LOWPRI) {
			/* Don't wait for low priority extents */
			if (take_detailed_stats)
				BC_ADD_STAT(is_shared, strategy_unfilled_lowpri, 1);
			unfilled = 1;
			cache_hit = 0;
			debug_bypass(ce_firstmatch, "unfilled (low priority)");
			goto bypass;
		}		
				
		/*
		 * wait for all the extents to finish
		 */
		microtime(&blocked_start_time);
		for (ce_idx = 0;
			 ce_idx < num_extents;
			 ce_idx++) {
			
			ce = (ce_idx == 0) ? ce_firstmatch : pce_additional_extents[ce_idx - 1];
			LOCK_EXTENT(ce);
			if (! (ce->ce_flags & (CE_ABORTED | CE_IODONE))) {
				did_block = 1;
				UNLOCK_CACHE_R();
				/*
				 * We need to release the cache lock since we will msleep in wait_for_extent.
				 * If we don't release the cache read lock and someone wants to add a new playlist,
				 * they go to grab the cache write lock, and everybody stalls for the msleep timeout.
				 *
				 * We are guaranteed that the cache won't disappear during this time since
				 * the cleanup code waits for all the strategy calls (this function) to complete
				 * by checking BC_cache->c_strategycalls, which we incremented above.
				 *
				 * The cache may have grown behind our backs, however. Mount pointers are not valid,
				 * but mount indexes still are. Extent pointers are still valid since the extent list
				 * is not modified (this is a requirement to be able to msleep with the extent's lock).
				 */
				wait_for_extent(ce, blocked_start_time);
				
				/* Cache lock must be grabbed without holding any locks to avoid deadlock rdar://8626772 */
				UNLOCK_EXTENT(ce);
				LOCK_CACHE_R();
				LOCK_EXTENT(ce);
				
				if (! (ce->ce_flags & (CE_ABORTED | CE_IODONE))) {
					/* strategy call timed out waiting for the extent */
					if (take_detailed_stats)
						BC_ADD_STAT(is_shared, strategy_timedout, 1);
#ifdef BC_DEBUG_BYPASSES
					microtime(&blocked_end_time);
					timersub(&blocked_end_time,
							 &blocked_start_time,
							 &blocked_end_time);
					debug_bypass(ce, "timed out waiting for extent after %lu.%03ds", blocked_end_time.tv_sec, blocked_end_time.tv_usec / 1000);
#endif
					goto bypass;
				}
				UNLOCK_EXTENT(ce);
				ce = NULL;
				
				/* Check every time we sleep so we can avoid more sleeping if unnecessary */
				status = extents_status(ce_firstmatch, pce_additional_extents, num_extents);
				if (status & CE_ABORTED) {
					if (take_detailed_stats)
						BC_ADD_STAT(is_shared, hit_aborted, 1);
					if (num_extents == 1) {
						debug_bypass(ce_firstmatch, "extent is aborted after waiting");
					} else {
						debug_bypass(ce_firstmatch, "one of the extents is aborted after waiting");
					}
					goto bypass;
				} else if (status & CE_IODONE) {
					break;
				}
			} else {
				UNLOCK_EXTENT(ce);
				ce = NULL;
			}
		}
		if (take_detailed_stats && did_block) {
			BC_ADD_STAT(is_shared, strategy_blocked, 1);
			microtime(&blocked_end_time);
			timersub(&blocked_end_time,
					 &blocked_start_time,
					 &blocked_end_time);
			u_int64_t ms_blocked = (blocked_end_time.tv_sec * 1000) + (blocked_end_time.tv_usec / (1000));
#ifdef BC_DEBUG
			if (procname[0] == '\0') {
				proc_selfname(procname, sizeof(procname));
			}
			const char* filename = vp ? vnode_getname(vp) : NULL;
			debug("Strategy routine blocked app %s waiting on file %s (disk offset 0x%llx) for %lu.%03d seconds", procname, filename?:"unknown", disk_offset, blocked_end_time.tv_sec, blocked_end_time.tv_usec / 1000);
			if (filename) {
				vnode_putname(filename);
			}
#endif
			BC_ADD_STAT(is_shared, strategy_time_blocked, ms_blocked);
			if (is_shared) {
				if (BC_cache->c_stats.ss_sharedcache.ss_strategy_time_longest_blocked < ms_blocked) {
					BC_cache->c_stats.ss_sharedcache.ss_strategy_time_longest_blocked = ms_blocked;
				}
			} else {
				if (BC_cache->c_stats.ss_nonsharedcache.ss_strategy_time_longest_blocked < ms_blocked) {
					BC_cache->c_stats.ss_nonsharedcache.ss_strategy_time_longest_blocked = ms_blocked;
				}
			}
		}
	}
	
	KERNEL_DEBUG_CONSTANT(FSDBG_CODE(DBG_BOOTCACHE, (did_block ? DBG_BC_IO_HIT_STALLED : DBG_BC_IO_HIT)) | DBG_FUNC_NONE, buf_kernel_addrperm_addr(bp), 0, 0, 0, 0);

	/*
	 * buf_map will do its best to provide access to this
	 * buffer via a kernel accessible address
	 * if it fails, we can still fall through.
	 */
	if (buf_map(bp, &buf_map_p)) {
		/* can't map, let someone else worry about it */
		if (take_detailed_stats)
			BC_ADD_STAT(is_shared, hit_failure, 1);
		debug_bypass(ce_firstmatch, "buf_map failed");
		goto bypass;
	}
	buf_extent_p = buf_map_p;
	
	kret = vnode_getwithvid(BC_cache->c_vp, BC_cache->c_vid);
	if (kret != KERN_SUCCESS) {
		debug_bypass(ce_firstmatch, "vnode_getwithvid failed - %d", kret);
		if (take_detailed_stats)
			BC_ADD_STAT(is_shared, hit_failure, 1);
		buf_unmap(bp);
		goto bypass;
	}
	
	/*
	 * fill in bp from the extents
	 */		
	for (ce_idx = 0;
		 ce_idx < num_extents;
		 ce_idx++) {
		u_int64_t nbytes_thisextent;
		u_int64_t diskoffset_thisextent;
		off_t     cacheoffset_thisextent;
		
		ce = (ce_idx == 0) ? ce_firstmatch : pce_additional_extents[ce_idx - 1];
		LOCK_EXTENT(ce);
				
		/*
		 * Check that the extent wasn't aborted while we were unlocked.
		 */
		if (ce->ce_flags & CE_ABORTED) {
			if (take_detailed_stats)
				BC_ADD_STAT(is_shared, hit_aborted, 1);
			vnode_put(BC_cache->c_vp);
			buf_unmap(bp);
			debug_bypass(ce, "extent is aborted at fulfillment");
			goto bypass;
		}
		
		assert(ce->ce_flags & CE_IODONE);
		
		diskoffset_thisextent  = MAX(ce->ce_diskoffset, disk_offset);
		nbytes_thisextent      = MIN(disk_offset + nbytes, ce->ce_diskoffset + ce->ce_length) - diskoffset_thisextent;
		cacheoffset_thisextent = ce->ce_cacheoffset + (diskoffset_thisextent - ce->ce_diskoffset);

		/* range check against extent */
		assert(diskoffset_thisextent  <  ce->ce_diskoffset + ce->ce_length);
		assert(diskoffset_thisextent  >= ce->ce_diskoffset);
		assert(nbytes_thisextent      <= (ce->ce_diskoffset + ce->ce_length) - diskoffset_thisextent);
		assert(cacheoffset_thisextent <  ce->ce_cacheoffset + ce->ce_length);
		assert(cacheoffset_thisextent >= ce->ce_cacheoffset);
		assert(nbytes_thisextent      <= (ce->ce_cacheoffset + ce->ce_length) - cacheoffset_thisextent);
		
		/* range check against buf_t */
		assert(diskoffset_thisextent    <  disk_offset + nbytes);
		assert(diskoffset_thisextent    >= disk_offset);
		assert(nbytes_thisextent        <= (disk_offset + nbytes) - diskoffset_thisextent);

		/* check our buf_map pointer */
		assert(buf_extent_p - buf_map_p == diskoffset_thisextent - disk_offset); /* didn't skip any bytes */
		assert(buf_map_p + nbytes       >= buf_extent_p + nbytes_thisextent); /* not overflowing the buffer */
		
		/* Make sure we're including the entire buffer requested */
		assert(ce_idx > 0 || disk_offset == diskoffset_thisextent); /* include the first byte */
		assert(ce_idx < (num_extents - 1) || disk_offset + nbytes == diskoffset_thisextent + nbytes_thisextent); /* include the last byte */
		
		/*
		 * Check that the requested blocks are in the buffer.
		 */
		if (!BC_blocks_present(ce,
							   CB_BYTE_TO_BLOCK(BC_cache->c_mounts + cm_idx, diskoffset_thisextent - ce->ce_diskoffset),
							   CB_BYTE_TO_BLOCK(BC_cache->c_mounts + cm_idx, nbytes_thisextent))) {
			if (take_detailed_stats)
				BC_ADD_STAT(is_shared, hit_blkmissing, 1);
			vnode_put(BC_cache->c_vp);
			buf_unmap(bp);
			
			debug_bypass(ce, "not all blocks are present");
#ifndef BC_DEBUG_BYPASSES
			debug_extents(cm_idx, ce, disk_offset, nbytes, "not all blocks are present");
#endif
			BC_print_extent_bitmap(ce, diskoffset_thisextent, nbytes_thisextent);
			goto bypass;
		}
		
		int resid = (int)nbytes_thisextent;
		uio = uio_create(1, cacheoffset_thisextent, UIO_SYSSPACE, UIO_READ);
		if (uio == NULL) {
			debug_bypass(ce, "couldn't allocate uio");
			if (take_detailed_stats)
				BC_ADD_STAT(is_shared, hit_failure, 1);
			vnode_put(BC_cache->c_vp);
			buf_unmap(bp);
			goto bypass;
		}
		
		kret = uio_addiov(uio, CAST_USER_ADDR_T(buf_extent_p), nbytes_thisextent);
		if (kret != KERN_SUCCESS) {
			debug_bypass(ce, "couldn't add iov to uio - %d", kret);
			if (take_detailed_stats)
				BC_ADD_STAT(is_shared, hit_failure, 1);
			vnode_put(BC_cache->c_vp);
			buf_unmap(bp);
			goto bypass;
		}
		
		kret = cluster_copy_ubc_data(BC_cache->c_vp, uio, &resid, 0);
		if (kret != KERN_SUCCESS) {
			debug_bypass(ce, "couldn't copy ubc data - %d", kret);
			if (take_detailed_stats)
				BC_ADD_STAT(is_shared, hit_failure, 1);
			vnode_put(BC_cache->c_vp);
			buf_unmap(bp);
			goto bypass;
		}
				
		if (resid != 0) {
			/* blocks have been stolen for pageout or contiguous memory */
			debug_bypass(ce, "blocks stolen");
			if (take_detailed_stats)
				BC_ADD_STAT(is_shared, hit_stolen, 1);
			vnode_put(BC_cache->c_vp);
			buf_unmap(bp);
			is_stolen = 1;
			goto bypass;
		}
		
		buf_extent_p += nbytes_thisextent;
		
		/* discard blocks we have touched */
		debug_extents(cm_idx, ce, disk_offset, nbytes, "hit");
		discards += BC_discard_bytes(ce, disk_offset, nbytes);

		UNLOCK_EXTENT(ce);
		ce = NULL;
		
		/* copy was successful */
		uio_free(uio);
		uio = NULL;
	}

	vnode_put(BC_cache->c_vp);

	buf_setresid(bp, 0);
	
	/* buf_unmap will take care of all cases */
	buf_unmap(bp);
	
	/* complete the request */
	buf_biodone(bp);
	
	/* can't dereference bp after a buf_biodone has been issued */

#else //ifndef EMULATE_ONLY
	(void) kret;
	(void) p;
	
	/* discard blocks we have touched */
	for (ce_idx = 0;
		 ce_idx < num_extents;
		 ce_idx++) {
		ce = (ce_idx == 0) ? ce_firstmatch : pce_additional_extents[ce_idx - 1];
		LOCK_EXTENT(ce);
		debug_extents(cm_idx, ce, disk_offset, nbytes, "hit");
		discards += BC_discard_bytes(ce, disk_offset, nbytes);
		UNLOCK_EXTENT(ce);
		ce = NULL;
	}
	
	/* send the IO to the disk, emulate hit in statistics */
	BC_cache->c_strategy(bp);
	
#endif //ifndef EMULATE_ONLY else
	
	if (take_detailed_stats) {
		BC_ADD_STAT(is_shared, hit_bytes, discards);
		if (cm_idx < STAT_MOUNTMAX) {
			BC_ADD_STAT(is_shared, hit_bytes_m[cm_idx], discards);
		}
		if (dont_cache) {
			BC_ADD_STAT(is_shared, strategy_hit_nocache, 1);
			BC_ADD_STAT(is_shared, hit_nocache_bytes, discards);
		}
	} else {
		BC_ADD_STAT(is_shared, hit_bytes_afterhistory, discards);
	}
			
	BC_cache->c_num_ios_since_last_hit = 0;
	
	/* we are not busy anymore */
	OSAddAtomic(-1, &BC_cache->c_strategycalls);
	UNLOCK_CACHE_R();
		
	if (num_extents > 1) {
		BC_FREE_ARRAY_WITHPOINTERS(pce_additional_extents, num_extents - 1);
	}

	if (! dont_cache) {
		/* record successful fulfilment (may block) */
		BC_add_history(vp_is_owned ? vp : NULL, blkno, nbytes, pid, 1, 0, 0, is_shared, crypto_offset, dev);
	}

	if (vp_is_owned) {
		vnode_put(vp);
	}

	/*
	 * spec_strategy wants to know if the read has been
	 * satisfied by the boot cache in order to avoid
	 * throttling the thread unnecessarily. spec_strategy
	 * will check for the special return value 0xcafefeed
	 * to indicate that the read was satisfied by the
	 * cache.
	 */
	
	const int IO_SATISFIED_BY_CACHE = ((int)0xcafefeed);
	return IO_SATISFIED_BY_CACHE;
	
bypass:
	if (ce != NULL)
		UNLOCK_EXTENT(ce);
	if (uio != NULL)
		uio_free(uio);
	if (pce_additional_extents != NULL) {
		if (num_extents > 1) {
			BC_FREE_ARRAY_WITHPOINTERS(pce_additional_extents, num_extents - 1);
		}
	}

	/* pass the request on */
	BC_cache->c_strategy(bp);
	
	/*
	 * can't dereference bp after c_strategy has been issued
	 * or else we race with buf_biodone
	 */
	void *bp_void = (void *)bp; // for use in ktrace
	bp = NULL;
	
	/* not really "bypassed" if the cache is not active */
	if (cache_active) {
		u_int64_t unread = 0;
		
		if (cm_idx != -1 && uuid_is_null(BC_cache->c_mounts[cm_idx].cm_group_uuid)) {
			// We have a cache_mount for this I/O's device, and it doesn't share disk space with any other cache_mount
			
			LOCK_MOUNT_R(BC_cache->c_mounts + cm_idx);
			if (BC_cache->c_mounts[cm_idx].cm_state == CM_READY) {
				discards += BC_handle_discards(BC_cache->c_mounts + cm_idx, disk_offset, nbytes, !is_read, &unread, is_shared);
			}
			
			/* Check if we should throttle this IO */
			if (bc_mount_should_throttle_cutthrough(cm_idx, is_swap)) {
				should_throttle = 1;
			}
			
			UNLOCK_MOUNT_R(BC_cache->c_mounts + cm_idx);
		} else {
			// We either don't have a cache_mount for this I/O's device, or the cache_mount potentially shares disk space with another cache_mount: need to check each cache_mount to see if it overlaps and discard the I/O's range from each one
			// <rdar://problem/43851924> When discarding data, discard data for that extent from all mounts in the same apfs container
			uuid_t container_uuid = {0};
			if (cm_idx != -1) {
				// We have a cache_mount for this device, and it has a group uuid, use it
				uuid_copy(container_uuid, BC_cache->c_mounts[cm_idx].cm_group_uuid);
			} else {
				// We don't have a cache_mount for this device (we're not caching this mount)
				// Still need to find the group UUID for this device in case we're caching other mounts in the same group, though
				struct BC_noncached_mount nm = BC_get_noncached_mount(dev);
				if (nm.nm_dev != dev) {
					message("Unable to determine grouping for I/O to dev %#x", dev);
				}
				uuid_copy(container_uuid, nm.nm_group_uuid);
			}
			
			if (!uuid_is_null(container_uuid)) {
				// We have a grouping for this device
				
				bool first_match = true;
				for (int mount_idx = 0; mount_idx < BC_cache->c_nmounts; mount_idx++) {
					if (BC_cache->c_mounts[mount_idx].cm_state == CM_READY &&
						0 == uuid_compare(BC_cache->c_mounts[mount_idx].cm_group_uuid, container_uuid)) {
						// This mount is in the same section of disk as this I/O's mount (i.e. same APFS container)
						// We make no guarantees that we don't have mounts with overlapping ranges in the same container, so we need to check all of them, not just stop at the first one that hits. Also, it could be a partial hit for multiple mounts even if the mounts don't overlap each other
						
						LOCK_MOUNT_R(BC_cache->c_mounts + mount_idx);
						
						// If we didn't have a matching mount, we don't know disk_offset yet. Get it from this mount from the same part of the disk (we assume any mounts sharing disk space will all have the same block size)
						off_t disk_offset_mount = CB_BLOCK_TO_BYTE(BC_cache->c_mounts + mount_idx, blkno);
						assert(disk_offset == 0 || disk_offset_mount == disk_offset);
						
						u_int64_t unread_mount = 0;
						u_int64_t discards_mount = 0;
						discards_mount = BC_handle_discards(BC_cache->c_mounts + mount_idx, disk_offset_mount, nbytes, !is_read, &unread_mount, is_shared);
						discards += discards_mount;
						unread += unread_mount;
						
						if (mount_idx != cm_idx && discards_mount > 0) {
							if (cm_idx != -1) {
								debug("Discarded %#llx bytes due to %s from mount %s matching container %s from I/O to mount %s", discards_mount, is_read ? "read" : "write", uuid_string(BC_cache->c_mounts[mount_idx].cm_uuid), uuid_string(BC_cache->c_mounts[mount_idx].cm_group_uuid), uuid_string(BC_cache->c_mounts[cm_idx].cm_group_uuid));
							} else {
								debug("Discarded %#llx bytes due to %s from mount %s matching container %s from I/O to non-cached mount with device %#x", discards_mount, is_read ? "read" : "write", uuid_string(BC_cache->c_mounts[mount_idx].cm_uuid), uuid_string(BC_cache->c_mounts[mount_idx].cm_group_uuid), dev);
							}
						}
						
						if (first_match) {
							// Only need to check the disk state once, since anything sharing the same space on disk will have the same cm_disk
							first_match = false;
							
							// If we don't have a cache mount for this I/O, we may still be during playback
							if (cm_idx == -1 &&
								BC_cache->c_mounts[mount_idx].cm_disk &&
								(BC_cache->c_mounts[mount_idx].cm_disk->cd_flags & CD_HAS_THREAD) &&
								!(BC_cache->c_mounts[mount_idx].cm_disk->cd_flags & CD_ISSUE_LOWPRI)) {
								during_io = 1; /* for statistics */
								if (take_detailed_stats) {
									BC_ADD_STAT(is_shared, strategy_duringio, 1);
								}
							}

							/* Check if we should throttle this IO */
							if (bc_mount_should_throttle_cutthrough(mount_idx, is_swap)) {
								should_throttle = 1;
							}
							
						}
						
						UNLOCK_MOUNT_R(BC_cache->c_mounts + mount_idx);
					}
				}
			} else {
				// No grouping for this noncached mount, nothing to discard
			}
		}
		
		if (discards > 0) {
			debug_extents(cm_idx, NULL, disk_offset, nbytes, "discarded %llu bytes due to bypass", discards);
		}

		if (take_detailed_stats) {
			BC_ADD_STAT(is_shared, strategy_bypassed, 1);
			if (during_io) {
				BC_ADD_STAT(is_shared, strategy_bypass_duringio, 1);
			}
			if (dont_cache) {
				BC_ADD_STAT(is_shared, strategy_bypass_nocache, 1);
				BC_ADD_STAT(is_shared, bypass_nocache_bytes, nbytes);
				if (during_io) {
					BC_ADD_STAT(is_shared, strategy_bypass_duringio_nocache, 1);
				}
			}
			if (cache_hit) {
				if (is_stolen) {
					BC_ADD_STAT(is_shared, stolen_discards, discards);
					BC_ADD_STAT(is_shared, stolen_unread, unread);
				} else {
					BC_ADD_STAT(is_shared, error_discards, discards);
					BC_ADD_STAT(is_shared, error_unread, unread);
				}
			} else if (unfilled) {
				BC_ADD_STAT(is_shared, lowpri_discards, discards);
				BC_ADD_STAT(is_shared, lowpri_unread, unread);
			} else if (dont_cache) {
				BC_ADD_STAT(is_shared, bypass_nocache_discards, discards);
				BC_ADD_STAT(is_shared, bypass_nocache_unread, unread);
			} else if (is_read) {
				BC_ADD_STAT(is_shared, read_discards, discards);
				BC_ADD_STAT(is_shared, read_unread, unread);
			} else {
				BC_ADD_STAT(is_shared, write_discards, discards);
				BC_ADD_STAT(is_shared, write_unread, unread);
			}
		} else {
			BC_ADD_STAT(is_shared, lost_bytes_afterhistory, discards);
		}
	}
	
	if (in_flight) {
		OSAddAtomic(-1, &BC_cache->c_strategycalls);
		UNLOCK_CACHE_R();
	}
	
	if (! is_swap) {
		if (! dont_cache) {
			is_rejected = BC_add_history(vp_is_owned ? vp : NULL, blkno, nbytes, pid, 0, (is_read ? 0 : 1), 0, is_shared, crypto_offset, dev);
			
			if (take_detailed_stats && during_io && !is_rejected) {
				if (cache_hit) {
					if (unfilled) {
						BC_ADD_STAT(is_shared, strategy_bypass_duringio_unfilled, 1);
					} else {					
						BC_ADD_STAT(is_shared, strategy_bypass_duringio_rootdisk_failure, 1);
					}
				} else if (is_read) {
					BC_ADD_STAT(is_shared, strategy_bypass_duringio_rootdisk_read, 1);
					if (cache_miss_due_to_crypto_mismatch) {
						BC_ADD_STAT(is_shared, strategy_bypass_duringio_rootdisk_read_crypto_mismatch, 1);
					}
					if (cache_miss_with_intersection) {
						BC_ADD_STAT(is_shared, strategy_bypass_duringio_rootdisk_read_partial_hits, 1);
					}
				} else {
					BC_ADD_STAT(is_shared, strategy_bypass_duringio_rootdisk_nonread, 1);
				}
			}
		}
	}
	
	/*
	 * Check to make sure that we're not missing the cache
	 * too often.  If we are, we're no longer providing a
	 * performance win and the best thing would be to give
	 * up.
	 *
	 * Don't count throttled IOs since those aren't time
	 * critical. (We don't want to jettison the cache just
	 * because spotlight is indexing)
	 */
	
	bool writeToSwap = false;
	if (is_swap && !is_read) {
		// rdar://78165238 (Writes to swapfile from DumpPanic jettisoning BootCache early)
		if (procname[0] == '\0') {
			proc_selfname(procname, sizeof(procname));
		}
		if ((0 != strncmp(procname, "DumpPanic", strlen("DumpPanic")))) {
			writeToSwap = true;
		}
	}
	
	/* if this is a read, and we do have an active cache, and the read isn't throttled */
	if (cache_active) {
		(void) is_stolen;
		if (writeToSwap /*|| is_stolen*/) {  //rdar://10651288&10658086 seeing stolen pages early during boot
			const char* reason = NULL;
			if (writeToSwap) {
				debug("detected write to swap file, jettisoning cache");
				reason = "write to swap file";
			} else {
				debug("detected stolen page, jettisoning cache");
				reason = "cache page stolen";
			}
			//rdar://9858070 Do this asynchronously to avoid deadlocks
			BC_terminate_cache_async(reason);
		} else if (is_read &&
				   !(throttle_tier)) {
			
			// <rdar://problem/39985442> Ignore cache misses from warmd for jettison tracking
			if (procname[0] == '\0') {
				proc_selfname(procname, sizeof(procname));
			}
			if ((0 != strncmp(procname, "warmd", strlen("warmd")))) {
				
				
				struct timeval current_time;
				if (BC_cache->c_stats.ss_history_num_recordings < 2) {
					microtime(&current_time);
					timersub(&current_time,
							 &BC_cache->c_loadtime,
							 &current_time);
				}
				/* Don't start counting misses until we've started login or hit our prelogin timeout */
				if (BC_cache->c_stats.ss_history_num_recordings >= 2 || current_time.tv_sec > BC_chit_prelogin_timeout_seconds) {
					
					/* increase number of IOs since last hit */
					OSAddAtomic(1, &BC_cache->c_num_ios_since_last_hit);
					
					if (BC_cache->c_num_ios_since_last_hit >= BC_chit_max_num_IOs_since_last_hit) {
						/*
						 * The hit rate is poor, shut the cache down.
						 */
						debug("hit rate below threshold (0 hits in the last %u lookups), jettisoning cache",
							  BC_cache->c_num_ios_since_last_hit);
						//rdar://9858070 Do this asynchronously to avoid deadlocks
						BC_terminate_cache_async("low hit rate");
					}
				}
			}
		}
	}
	
	if (writeToSwap && (! (BC_cache->c_flags & BC_FLAG_SHUTDOWN))) {
		/* if we're swapping, stop readahead */
		debug("Detected write to swap file. Terminating readahead");
		if (!(BC_cache->c_flags & BC_FLAG_SHUTDOWN) && BC_cache->c_num_reader_threads > 0) {
			strncpy(BC_cache->c_stats.ss_cache_end_reason, "write to swap file", sizeof(BC_cache->c_stats.ss_cache_end_reason));
		}
		BC_set_flag(BC_FLAG_SHUTDOWN);
	}
	
	if (cache_active) {
		KERNEL_DEBUG_CONSTANT(FSDBG_CODE(DBG_BOOTCACHE, (should_throttle ? DBG_BC_IO_MISS_CUT_THROUGH : DBG_BC_IO_MISS)) | DBG_FUNC_NONE, buf_kernel_addrperm_addr(bp_void), 0, 0, 0, 0);
	}
	
	if (vp_is_owned) {
		vnode_put(vp);
	}

	if (should_throttle && throttle_tier < IOPOL_THROTTLE) {
		
		if (procname[0] == '\0') {
			proc_selfname(procname, sizeof(procname));
		}
		if ((0 == strncmp(procname, "securityd", strlen("securityd"))) ||
			(0 == strncmp(procname, "kextd", strlen("kextd"))) ||
			(0 == strncmp(procname, "kernelmanagerd", strlen("kernelmanagerd")))) { // kernelmanagerd is the new kextd
			BC_ADD_STAT(is_shared, strategy_nonthrottled, 1);
			return 0;
		}
		
		/*
		 * We need to indicate to spec_strategy that we want to
		 * throttle this IO to avoid cutting through readahead
		 * too much. spec_strategy will check for the special
		 * return value 0xcafebeef to indicate that the IO
		 * should be throttled.
		 */
		
		BC_ADD_STAT(is_shared, strategy_forced_throttled, 1);
		
		const int IO_SHOULD_BE_THROTTLED = ((int)0xcafebeef);
		return IO_SHOULD_BE_THROTTLED;
	}
	
	return 0;
}

/*
 * Handle a block range that needs to be discarded.
 *
 * Called with the cache mount read lock held
 *
 * Returns the number of bytes discarded (that were read in already)
 * *unread_discards will be set to the number of bytes discarded that were unread
 */
static u_int64_t
BC_handle_discards(struct BC_cache_mount *cm, u_int64_t offset, u_int64_t length, int data_is_overwritten, u_int64_t* unread_out, int is_shared)
{
	struct BC_cache_extent **pce, **p;
	u_int64_t count;
	boolean_t locked;
	
	u_int64_t discards = 0;
	u_int64_t unread = 0;
	
	int try_lock_failed = 0; /* Only count this IO once even if we fail to grab the lock for multiple extents */
	
	/*
	 * Look for an extent that we overlap.
	 */
	if ((pce = BC_find_extent(cm, offset, length, 0, 0, NULL, 0, NULL, NULL)) == NULL)
		return 0;		/* doesn't affect us */
	
	/*
	 * Discard bytes in the matched extent.
	 * LOCK_EXTENT_TRY is used so we don't block this thread for this optimization rdar://19542373
	 */
	if (data_is_overwritten) {
		// If the data was overwritten, we must discard the now obsolete data we've cached
		locked = 1;
		LOCK_EXTENT(*pce);
	} else {
		locked = LOCK_EXTENT_TRY(*pce);
	}
	if (locked) {
		count = BC_discard_bytes((*pce), offset, length);
		if ((*pce)->ce_flags & CE_IODONE) {
			discards += count;
		} else {
			unread += count;
		}
		UNLOCK_EXTENT(*pce);
	} else {
		count = BC_intersection_size((*pce), offset, length);
		if (count > 0) {
			debug_extents((*pce)->ce_mount_idx, *pce, offset, length, "unable to lock to mark vacant");
		}
		BC_ADD_STAT(is_shared, unable_to_discard_bytes, count);
		if (!try_lock_failed) {
			BC_ADD_STAT(is_shared, unable_to_discard_count, 1);
			try_lock_failed = 1;
		}
	}
	
	/*
	 * Scan adjacent extents for possible overlap and discard there as well.
	 */
	p = pce - 1;
	while (p >= cm->cm_pextents && 
		   BC_check_intersection((*p), offset, length)) {
		if (data_is_overwritten) {
			// If the data was overwritten, we must discard the now obsolete data we've cached
			locked = 1;
			LOCK_EXTENT(*p);
		} else {
			locked = LOCK_EXTENT_TRY(*p);
		}
		if (locked) {
			count = BC_discard_bytes((*p), offset, length);
			if ((*p)->ce_flags & CE_IODONE) {
				discards += count;
			} else {
				unread += count;
			}
			UNLOCK_EXTENT(*p);
			/* rdar://28284713
			 * Do not break if count is 0
			 * The previous extent (p - 1) may still have blocks that need to be discarded.
			 * BC_check_intersection will handle loop termination
			 */
		} else {
			count = BC_intersection_size((*p), offset, length);
			if (count > 0) {
				debug_extents((*p)->ce_mount_idx, (*p), offset, length, "unable to lock to mark vacant");
			}
			BC_ADD_STAT(is_shared, unable_to_discard_bytes, count);
			if (!try_lock_failed) {
				BC_ADD_STAT(is_shared, unable_to_discard_count, 1);
				try_lock_failed = 1;
			}
		}
		p--;
	}
	p = pce + 1;
	while (p < (cm->cm_pextents + cm->cm_nextents) &&
		   BC_check_intersection((*p), offset, length)) {
		if (data_is_overwritten) {
			// If the data was overwritten, we must discard the now obsolete data we've cached
			locked = 1;
			LOCK_EXTENT(*p);
		} else {
			locked = LOCK_EXTENT_TRY(*p);
		}
		if (locked) {
			count = BC_discard_bytes((*p), offset, length);
			if ((*p)->ce_flags & CE_IODONE) {
				discards += count;
			} else {
				unread += count;
			}
			UNLOCK_EXTENT(*p);
			/* rdar://28284713
			 * Do not break if count is 0
			 * The next extent (p + 1) may still have blocks that need to be discarded.
			 * BC_check_intersection will handle loop termination
			 */
		} else {
			count = BC_intersection_size((*p), offset, length);
			if (count > 0) {
				debug_extents((*p)->ce_mount_idx, (*p), offset, length, "unable to lock to mark vacant");
			}
			BC_ADD_STAT(is_shared, unable_to_discard_bytes, count);
			if (!try_lock_failed) {
				BC_ADD_STAT(is_shared, unable_to_discard_count, 1);
				try_lock_failed = 1;
			}
		}
		p++;
	}
	
	if (unread_out) {
		*unread_out = unread;
	}
	
	return discards;
}

/*
 * Shut down readahead operations.
 */
static int
BC_terminate_readahead(const char* reason)
{
	int error;
	struct timespec timeout;
	timeout.tv_sec = 10;
	timeout.tv_nsec = 0;
	
	if (reason && !(BC_cache->c_flags & BC_FLAG_SHUTDOWN) && BC_cache->c_num_reader_threads > 0) {
		strncpy(BC_cache->c_stats.ss_cache_end_reason, reason, sizeof(BC_cache->c_stats.ss_cache_end_reason));
	}
	
	/*
	 * Signal the readahead thread to terminate, and wait for
	 * it to comply.  If this takes > 10 seconds, give up.
	 */
	BC_set_flag(BC_FLAG_SHUTDOWN);
	
	/*
	 * If readahead is still in progress, we have to shut it down
	 * cleanly.  This is an expensive process, but since readahead
	 * will normally complete long before the reads it tries to serve
	 * complete, it will typically only be invoked when the playlist
	 * is out of synch and the cache hitrate falls below the acceptable
	 * threshold.
	 *
	 * Note that if readahead is aborted, the reader thread will mark
	 * the aborted extents and wake up any strategy callers waiting
	 * on them, so we don't have to worry about them here.
	 */
	LOCK_READERS();
	while (BC_cache->c_num_reader_threads > 0) {
		debug("terminating active readahead");
		
		error = msleep(&BC_cache->c_num_reader_threads, &BC_cache->c_reader_threads_lock, PRIBIO, "BC_terminate_readahead", &timeout);
		if (error == EWOULDBLOCK) {
			UNLOCK_READERS();
			
			message("timed out waiting for I/O to stop");
			if (BC_cache->c_num_reader_threads == 0) {
				debug("but I/O has stopped!");
			} 
#ifdef BC_DEBUG
 			Debugger("I/O Kit wedged on us");
#endif
			/*
			 * It might be nice to free all the pages that
			 * aren't actually referenced by the outstanding
			 * region, since otherwise we may be camped on a
			 * very large amount of physical memory.
			 *
			 * Ideally though, this will never happen.
			 */
			return(EBUSY);	/* really EWEDGED */
		}
	}
	UNLOCK_READERS();
	
	return(0);
}

static void
BC_terminate_cache_thread(void *param0, wait_result_t param1)
{
	thread_set_thread_name(current_thread(), "BootCache termination");
	
	const char* reason = param0;
	BC_terminate_cache(reason);
}

/*
 * Start up an auxilliary thread to stop the cache so we avoid potential deadlocks
 */
static void
BC_terminate_cache_async(const char* reason)
{
	if (! (BC_cache->c_flags & BC_FLAG_CACHEACTIVE)) {
		return;
	}

	int error;
	thread_t rthread;

	debug("Kicking off thread to terminate cache");
	if ((error = kernel_thread_start(BC_terminate_cache_thread, (void*)reason, &rthread)) == KERN_SUCCESS) {
		thread_deallocate(rthread);
	} else {
		message("Unable to start thread to terminate cache: %d", error);
	}
}

/*
 * Terminate the cache.
 *
 * This prevents any further requests from being satisfied from the cache
 * and releases all the resources owned by it.
 *
 * Must be called with no locks held
 */
static int
BC_terminate_cache(const char* reason)
{
	int retry, cm_idx, j, ce_idx, cel_idx;

	BC_terminate_readahead(reason);
	
	/* can't shut down if readahead is still active */
	if (BC_cache->c_num_reader_threads > 0) {
		debug("cannot terminate cache while readahead is in progress");
		return(EBUSY);
	}
	
	LOCK_CACHE_R();
	
	LOCK_HANDLERS();
	if (!BC_clear_flag(BC_FLAG_CACHEACTIVE)) { 
		/* can't shut down if we're not active */
		debug("cannot terminate cache when not active");
		UNLOCK_HANDLERS();
		UNLOCK_CACHE_R();
		return(EALREADY);
	}
	
	bootcache_contains_block = NULL;
	
	debug("terminating cache...");
	
	/* if we're no longer recording history also, disconnect our strategy routine */
	BC_check_handlers();
	UNLOCK_HANDLERS();
	
	/*
	 * Mark all extents as FREED. This will notify any sleepers in the 
	 * strategy routine that the extent won't have data for them.
	 */
	int nonshared_discards = 0;
	int shared_discards = 0;
	int nonshared_unread = 0;
	int shared_unread = 0;
	for (cel_idx = 0;
		 cel_idx < BC_cache->c_nextentlists;
		 cel_idx++) {
		for (ce_idx = 0;
			 ce_idx < BC_cache->c_nextents[cel_idx];
			 ce_idx++) {
			struct BC_cache_extent *ce = BC_cache->c_extentlists[cel_idx] + ce_idx;
			LOCK_EXTENT(ce);		
			/* 
			 * Track unused bytes 
			 */
			if (ce->ce_blockmap_arraycount > 0 && BC_cache->c_mounts[ce->ce_mount_idx].cm_blocksize != 0) {
				for (j = 0; j < howmany(ce->ce_length, BC_cache->c_mounts[ce->ce_mount_idx].cm_blocksize); j++) {
					if (CB_BLOCK_PRESENT(ce, j)) {
						if (ce->ce_flags & CE_IODONE) {
							if (ce->ce_flags & CE_SHARED) {
								shared_discards += BC_cache->c_mounts[ce->ce_mount_idx].cm_blocksize;
							} else {
								nonshared_discards += BC_cache->c_mounts[ce->ce_mount_idx].cm_blocksize;
							}
						} else {
							if (ce->ce_flags & CE_SHARED) {
								shared_unread += BC_cache->c_mounts[ce->ce_mount_idx].cm_blocksize;
							} else {
								nonshared_unread += BC_cache->c_mounts[ce->ce_mount_idx].cm_blocksize;
							}
						}
					}
				}
			}
			BC_teardown_extent(ce);
			UNLOCK_EXTENT(ce);
			wakeup(ce);
		}
	}
	BC_ADD_NON_SHARED_CACHE_STAT(spurious_discards, nonshared_discards);
	BC_ADD_SHARED_CACHE_STAT(spurious_discards, shared_discards);
	BC_ADD_NON_SHARED_CACHE_STAT(spurious_unread, nonshared_unread);
	BC_ADD_SHARED_CACHE_STAT(spurious_unread, shared_unread);
	
	for (cm_idx = 0; cm_idx < BC_cache->c_nmounts; cm_idx++) {
		struct BC_cache_mount *cm = BC_cache->c_mounts + cm_idx;
		LOCK_MOUNT_W(cm);
		BC_teardown_mount(cm);
		UNLOCK_MOUNT_W(cm);
	}
	
	
	/*
	 * It is possible that one or more callers are asleep in the
	 * strategy routine (eg. if readahead was terminated early,
	 * or if we are called off the timeout).
	 * Check the count of callers in the strategy code, and sleep
	 * until there are none left (or we time out here).  Note that
	 * by clearing BC_FLAG_CACHEACTIVE above we prevent any new
	 * strategy callers from touching the cache, so the count
	 * must eventually drain to zero.
	 */
	retry = 0;
	while (BC_cache->c_strategycalls > 0) {
		tsleep(BC_cache, PRIBIO, "BC_terminate_cache", 10);
		if (retry++ > 50) {
			message("could not terminate cache, timed out with %u caller%s in BC_strategy",
					BC_cache->c_strategycalls,
					BC_cache->c_strategycalls == 1 ? "" : "s");
			UNLOCK_CACHE_R();
			return(EBUSY);	/* again really EWEDGED */
		}
	}
	
	if (! LOCK_CACHE_R_TO_W()) {
		/* We shouldn't get here. This is the only LOCK_CACHE_R_TO_W call,
		 * so this implies someone is terminating the cache in parallel with us,
		 * but we check for exclusivity by clearing BC_FLAG_CACHEACTIVE.
		 */
		message("Unable to upgrade cache lock to free resources");
		return(EBUSY);
	}
	
	for (cm_idx = 0; cm_idx < BC_cache->c_nmounts; cm_idx++) {
		struct BC_cache_mount *cm = BC_cache->c_mounts + cm_idx;
		if (cm->cm_disk != NULL) {
			for (j = cm_idx + 1; j < BC_cache->c_nmounts; j++) {
				if (BC_cache->c_mounts[j].cm_disk == cm->cm_disk) {
					BC_cache->c_mounts[j].cm_disk = NULL;
				}
			}
			BC_teardown_disk(cm->cm_disk);
			lck_mtx_destroy(&cm->cm_disk->cd_lock, BC_cache->c_lckgrp);
			BC_FREE_SINGLE(cm->cm_disk);
		}
	}	
	
	BC_free_pagebuffer();
	BC_cache->c_cachesize = 0;
	
	/* free memory held by extents and mounts */
	for (cel_idx = 0;
		 cel_idx < BC_cache->c_nextentlists;
		 cel_idx++) {
		for (ce_idx = 0;
			 ce_idx < BC_cache->c_nextents[cel_idx];
			 ce_idx++) {
			lck_mtx_destroy(&BC_cache->c_extentlists[cel_idx][ce_idx].ce_lock, BC_cache->c_lckgrp);
		}
		BC_FREE_ARRAY_WITHPOINTERS(BC_cache->c_extentlists[cel_idx], BC_cache->c_nextents[cel_idx]);
	}
	BC_FREE_ARRAY_WITHPOINTERS(BC_cache->c_extentlists, BC_cache->c_nextentlists);
	BC_FREE_ARRAY_NOPOINTERS(BC_cache->c_nextents, BC_cache->c_nextentlists);
	BC_cache->c_nextentlists = 0;

	for (cm_idx = 0; cm_idx < BC_cache->c_nmounts; cm_idx++) {
		lck_rw_destroy(&BC_cache->c_mounts[cm_idx].cm_lock, BC_cache->c_lckgrp);
	}
	BC_FREE_ARRAY_WITHPOINTERS(BC_cache->c_mounts, BC_cache->c_nmounts);
	BC_cache->c_nmounts = 0;
	
	BC_cache->c_batch_offset_for_new_playlists = BC_cache->c_batch_count;
	
	LOCK_NONCACHED_MOUNTS_W();
	BC_FREE_ARRAY_NOPOINTERS(BC_cache->c_noncached_mounts, BC_cache->c_noncached_nmounts);
	BC_cache->c_noncached_nmounts = 0;
	UNLOCK_NONCACHED_MOUNTS_W();
	
	
	UNLOCK_CACHE_W();
	
	/* record stop time */
	struct timeval endtime;
	microtime(&endtime);
	timersub(&endtime,
			 &BC_cache->c_cache_starttime,
			 &endtime);
	BC_cache->c_stats.ss_cache_time += (u_int) endtime.tv_sec * 1000 + (u_int) endtime.tv_usec / 1000;
	if (reason) {
		strncpy(BC_cache->c_stats.ss_cache_end_reason, reason, sizeof(BC_cache->c_stats.ss_cache_end_reason));
	}
	return(0);
}

/*
 * Reset the cache.
 *
 * This stops any playback and recording, clears any history, and returns the BootCache back to the state it was in after loading at boot
 */
static int
BC_reset_cache(void)
{
	int error = 0;
	
	/*
	 * If the cache is running, stop it.  If it's already stopped
	 * (it may have stopped itself), that's OK.
	 */
	
	error = BC_terminate_history(NULL);
	if (error != 0 && error != EALREADY) {
		message("Unable to terminate history: %d", error);
		return error;
	}
	BC_cache->c_stats.ss_history_num_recordings = 0;
	
	error = BC_terminate_cache(NULL);
	if (error != 0 && error != EALREADY) {
		message("Unable to terminate cache: %d", error);
		return error;
	}
	
	LOCK_HISTORY_W();
	BC_discard_history();
	UNLOCK_HISTORY_W();
	
	BC_cache->c_num_ios_since_last_hit = 0;
	BC_cache->c_stats.ss_bc_start_timestamp = 0;
	BC_cache->c_stats.ss_start_timestamp = 0;
	BC_cache->c_stats.ss_cache_time = 0;
	bzero(BC_cache->c_stats.ss_cache_end_reason, sizeof(BC_cache->c_stats.ss_cache_end_reason));
	bzero(BC_cache->c_stats.ss_history_end_reason, sizeof(BC_cache->c_stats.ss_history_end_reason));
	bzero(BC_cache->c_stats.ss_playback_end_reason, sizeof(BC_cache->c_stats.ss_playback_end_reason));
	BC_clear_flag(BC_FLAG_SHUTDOWN);
	
	return (0);
}

/*
 * Terminate history recording.
 *
 * This stops us recording any further history events.
 */
static int
BC_terminate_history(const char* reason)
{
	struct BC_history_mount_device  *hmd;
	LOCK_HANDLERS();
	if (!BC_clear_flag(BC_FLAG_HISTORYACTIVE)) { 
		/* can't shut down if we're not active */
		debug("cannot terminate history when not active");
		UNLOCK_HANDLERS();
		return(EALREADY);
	}
	
	debug("terminating history collection...");
	KERNEL_DEBUG_CONSTANT(FSDBG_CODE(DBG_BOOTCACHE, DBG_BC_RECORDING_END), 0, 0, 0, 0, 0);

	/* if the cache is no longer active also, disconnect our strategy routine */
	BC_check_handlers();
	UNLOCK_HANDLERS();
	
	/*
	 * Kill the timeout handler; we don't want it going off
	 * now.
	 */
	untimeout(BC_timeout_history, NULL);	
	
	if (BC_cache->c_take_detailed_stats) {
		BC_cache->c_stats.ss_history_mount_no_uuid = 0;
		BC_cache->c_stats.ss_history_mount_no_blocksize = 0;
		LOCK_HISTORY_R();
		for (hmd = BC_cache->c_history_mounts; hmd != NULL; hmd = hmd->hmd_next)
			if (uuid_is_null(hmd->hmd_mount.hm_uuid)) {
				debug("Unable to identify mount with dev 0x%x", hmd->hmd_dev);
				BC_ADD_UNKNOWN_STAT(history_mount_no_uuid, 1);
			} else if (hmd->hmd_blocksize == 0) {
				debug("Unable to get blocksize for mount with dev 0x%x", hmd->hmd_dev);
				BC_ADD_UNKNOWN_STAT(history_mount_no_blocksize, 1);
			}
		UNLOCK_HISTORY_R();
	}
	
	/* record stop time */
	if (BC_cache->c_take_detailed_stats) {
		struct timeval endtime;
		/* record stop time */
		microtime(&endtime);
		timersub(&endtime,
				 &BC_cache->c_history_starttime,
				 &endtime);
		BC_ADD_UNKNOWN_STAT(history_time, (u_int) endtime.tv_sec * 1000 + (u_int) endtime.tv_usec / 1000);
		
		if (reason) {
			strncpy(BC_cache->c_stats.ss_history_end_reason, reason, sizeof(BC_cache->c_stats.ss_history_end_reason));
		}
	}
	
	BC_cache->c_take_detailed_stats = 0;

	return(0);
}

/*
 * Check our strategy handlers and make sure they are set/cleared depending on our current state.
 *
 * Should be called after changing BC_FLAG_CACHEACTIVE or BC_FLAG_HISTORYACTIVE.
 * Called with the handlers lock held.
 */
static void
BC_check_handlers(void)
{	
	if (BC_cache->c_devsw == NULL ||
		BC_cache->c_strategy == NULL ||
		BC_cache->c_close == NULL) {
		debug("Cannot check device handlers: cache not setup properly");
		return;
	}
	
	/* if the cache or history recording is active, make sure we've captured the appropriate device handler routines */
	if ((BC_cache->c_flags & BC_FLAG_CACHEACTIVE) ||
		(BC_cache->c_flags & BC_FLAG_HISTORYACTIVE)) {
		
		if (BC_cache->c_devsw->d_strategy != (strategy_fcn_t*) BC_strategy ||
			BC_cache->c_devsw->d_close    != BC_close) {
			
			debug("connecting handlers...");
			
			/* connect the strategy and close routines */
			BC_cache->c_devsw->d_strategy = (strategy_fcn_t*) BC_strategy;
			BC_cache->c_devsw->d_close    = BC_close;
		}
	} else {
		if (BC_cache->c_devsw->d_strategy != BC_cache->c_strategy ||
			BC_cache->c_devsw->d_close    != BC_cache->c_close) {
			
			debug("disconnecting handlers...");
			
			/* disconnect the strategy and close routines */
			BC_cache->c_devsw->d_strategy = BC_cache->c_strategy;
			BC_cache->c_devsw->d_close    = BC_cache->c_close;
		}
	}
}

/*
 * Setup a cache extent with the parameters given by the playlist entry
 *
 * Returns 0 on success
 */
static int 
BC_setup_extent(struct BC_cache_extent *ce, const struct BC_playlist_entry *pe, int batch_adjustment, u_int cache_mount_idx)
{	
	lck_mtx_init(&ce->ce_lock, BC_cache->c_lckgrp,
				 LCK_ATTR_NULL);
	ce->ce_diskoffset = pe->pe_offset;
	ce->ce_mount_idx = cache_mount_idx;
	ce->ce_length = pe->pe_length;
#ifdef IGNORE_BATCH
	ce->ce_batch = 0;
#else
	ce->ce_batch = pe->pe_batch;
#endif
	ce->ce_batch += batch_adjustment;
	if (ce->ce_batch > BC_MAXBATCHES) {
		ce->ce_batch = BC_MAXBATCHES; /* cap batch number */
	}
	ce->ce_cacheoffset = 0;
	ce->ce_blockmap = NULL;
	ce->ce_blockmap_arraycount = 0;
	ce->ce_flags = 0;
	if (pe->pe_flags & BC_PE_LOWPRIORITY) {
		ce->ce_flags |= CE_LOWPRI;
	}
	if (pe->pe_flags & BC_PE_SHARED) {
		ce->ce_flags |= CE_SHARED;
	}
	ce->ce_crypto_offset = pe->pe_crypto_offset;

	/* track highest batch number for this playlist */
	if (ce->ce_batch >= BC_cache->c_batch_count) {
		BC_cache->c_batch_count = ce->ce_batch + 1;
		// debug("Largest batch is now %d from extent with disk offset %#llx", BC_cache->c_batch_count, ce->ce_diskoffset);
	}
	
	return 0;
}

/*
 * The blocksize is initialised from the first playlist read, the statistics
 * structure, or it can be pre-set by the caller.  Once set, only playlists with
 * matching sizes can be read/written.
 */
#define	BLOCK_ROUNDDOWN(cm, x)	(((x) / (cm)->cm_blocksize) * (cm)->cm_blocksize)
#define BLOCK_ROUNDUP(cm, x)	((((x) + ((cm)->cm_blocksize - 1)) / (cm)->cm_blocksize) * (cm)->cm_blocksize)
/*
 * optional for power-of-two blocksize roundup:
 * (((x) + ((cm)->cm_blocksize - 1)) & (~((cm)->cm_blocksize - 1)))
 */

/*
 * Fill in a cache extent now that its mount has been filled in
 * 
 * Called with the extent lock held
 *
 * Returns 0 on success
 */
static int 
BC_fill_in_extent(struct BC_cache_extent *ce)
{
	u_int64_t numblocks, roundsize, i;
	u_int64_t end;
	
	if ((ce->ce_flags & CE_ABORTED) ||
		ce->ce_length == 0) {
		return 1;
	}
	
	struct BC_cache_mount *cm = BC_cache->c_mounts + ce->ce_mount_idx;
	
	u_int64_t blocksize = cm->cm_blocksize;
	
	if (0 == blocksize) {
		return 1;
	}
	
	roundsize = roundup(ce->ce_length, PAGE_SIZE);
	
	/* make sure we're on block boundaries */
	end = ce->ce_diskoffset + roundsize;
	ce->ce_diskoffset = BLOCK_ROUNDDOWN(cm, ce->ce_diskoffset);
	ce->ce_length = BLOCK_ROUNDUP(cm, end) - ce->ce_diskoffset;
	
	/* make sure we didn't grow our pagebuffer size since is's already been allocated */
	if (roundup(ce->ce_length, PAGE_SIZE) > roundsize) {
		debug("Clipped extent %#llx by a page", ce->ce_diskoffset);
		BC_ADD_STAT(ce->ce_flags & CE_SHARED, extents_clipped, 1);
		ce->ce_length = roundsize;
	}
	
	numblocks = howmany(ce->ce_length, blocksize);
	
	ce->ce_blockmap_arraycount = (u_int32_t)howmany(numblocks, CB_MAPFIELDBITS);
	if (ce->ce_blockmap_arraycount > 1) {
		// Using the ce->ce_blockmap part of the union
		BC_MALLOC_ARRAY_NOPOINTERS(ce->ce_blockmap, ce->ce_blockmap_arraycount);
		if (!ce->ce_blockmap) {
			message("can't allocate extent blockmap for %llu blocks of %lu bytes", numblocks, ce->ce_blockmap_arraycount * sizeof(*ce->ce_blockmap));
			return 1;
		}
	} else {
		// Using the ce->ce_localblockmap part of the union
		ce->ce_localblockmap = 0x0;
	}
	
	for (i = 0; i < howmany(ce->ce_length, blocksize); i++) {
		CB_MARK_BLOCK_PRESENT((ce), i);
	}
	
	debug_extents(ce->ce_mount_idx, ce, ce->ce_diskoffset, ce->ce_length, "marked present");
	BC_print_extent_bitmap(ce, ce->ce_diskoffset, ce->ce_length);

	return 0;
}

static void
BC_teardown_extent(struct BC_cache_extent *ce)
{
	debug_extents(ce->ce_mount_idx, ce, 0, 0, "aborted");
	ce->ce_flags |= CE_ABORTED;
	if (ce->ce_blockmap_arraycount > 1) {
		BC_FREE_ARRAY_NOPOINTERS(ce->ce_blockmap, ce->ce_blockmap_arraycount);
	} else {
		// We didn't allocate an array and instead used ce->ce_localblockmap
		ce->ce_localblockmap = 0x0;
	}
	ce->ce_blockmap_arraycount = 0;
}

/*
 * Setup a cache disk.
 * Returns 0 on success
 */
static int
BC_setup_disk(struct BC_cache_disk *cd, u_int64_t disk_id, int is_ssd)
{
	static int next_disk_num;
	lck_mtx_init(&cd->cd_lock, BC_cache->c_lckgrp, LCK_ATTR_NULL);
	cd->cd_disk_id = disk_id;
	cd->cd_disk_num = next_disk_num++;
	cd->cd_flags = 0;
	cd->cd_nmounts = 0;
	cd->cd_batch = BC_cache->c_batch_offset_for_new_playlists;

	if (is_ssd) {
		cd->cd_flags |= CD_IS_SSD;
	}
	
	debug("Setup disk 0x%llx as disk num %d%s", disk_id, cd->cd_disk_num, (cd->cd_flags & CD_IS_SSD) ? " (ssd)" : "");
	
	return (0);
}

/*
 * Teardown a cache disk.
 */
static void
BC_teardown_disk(struct BC_cache_disk *cd)
{
	/* Nothing to do */
}


/*
 * Check if a mount has become available for readahead
 *
 * If so, make sure the reader thread picks up this mount's IOs.
 * If there is no reader thread for the mount's disk, kick one off.
 *
 * Called with the cache mount read lock held
 */
static void
BC_mount_available(struct BC_cache_mount *cm)
{
#ifdef EMULATE_ONLY
	int i;
	for (i = 0; i < cm->cm_nextents; i++)
			cm->cm_pextents[i]->ce_flags |= CE_IODONE;
#else
	int i, error;
	thread_t rthread;
	if (cm->cm_state != CM_READY) {
		/* not ready */
		return;
	}
	
	struct BC_cache_disk *cd = cm->cm_disk;
	LOCK_DISK(cd);
	cd->cd_nmounts++;
	LOCK_READERS();
	if (!(BC_cache->c_flags & BC_FLAG_SHUTDOWN)) {
		if (cd->cd_flags & CD_HAS_THREAD) {
			/* Make sure the thread is not issuing lowpriority IO */
			if (cd->cd_flags & CD_ISSUE_LOWPRI) {
				debug("Interrupting low-priority thread for new mount %s", uuid_string(cm->cm_uuid));
				cd->cd_flags &= (~CD_ISSUE_LOWPRI);
				/* TODO: Unthrottle the readahead thread here rather than waiting out its current throttle delay */
			}
			UNLOCK_READERS();
			UNLOCK_DISK(cd);
			return;
		}
		
		/* Make sure we issue regular IOs for the new playlist in case we were issuing low-priority previously */
		cd->cd_flags &= (~CD_ISSUE_LOWPRI);

		debug("Kicking off reader thread for disk %d for mount %s", cd->cd_disk_num, uuid_string(cm->cm_uuid));
		if ((error = kernel_thread_start(BC_reader_thread, cd, &rthread)) == KERN_SUCCESS) {
			thread_deallocate(rthread);
			BC_cache->c_num_reader_threads++;
			BC_ADD_UNKNOWN_STAT(readahead_threads, 1);
			cd->cd_flags |= CD_HAS_THREAD;
			UNLOCK_READERS();
			UNLOCK_DISK(cd);
			return;
		}
		
		message("Unable to start reader thread for disk %d: %d", cd->cd_disk_num, error);
	}
	UNLOCK_READERS();
	UNLOCK_DISK(cd);
	
	/*
	 * Getting here indicates some failure.
	 *
	 * Mark all extents as aborted. This will notify any sleepers in the 
	 * strategy routine that the extent won't have data for them.
	 */
	int nonshared_unread = 0;
	int shared_unread = 0;
	debug_extents(cm - BC_cache->c_mounts, NULL, 0, 0, "failed to become available, removing remaining extents");
	for (i = 0; i < cm->cm_nextents; i++) {
		LOCK_EXTENT(cm->cm_pextents[i]);
		if (! (cm->cm_pextents[i]->ce_flags & CE_ABORTED)) {
			if (cm->cm_pextents[i]->ce_blockmap_arraycount > 0) {
				u_int64_t count = BC_discard_bytes(cm->cm_pextents[i], cm->cm_pextents[i]->ce_diskoffset, cm->cm_pextents[i]->ce_length);
				if (cm->cm_pextents[i]->ce_flags & CE_SHARED) {
					shared_unread += count;
				} else {
					nonshared_unread += count;
				}
			} else {
				if (cm->cm_pextents[i]->ce_flags & CE_SHARED) {
					shared_unread += cm->cm_pextents[i]->ce_length;
				} else {
					nonshared_unread += cm->cm_pextents[i]->ce_length;
				}
			}
		}
		BC_teardown_extent(cm->cm_pextents[i]);
		UNLOCK_EXTENT(cm->cm_pextents[i]);
		wakeup(cm->cm_pextents[i]);
	}
	BC_ADD_NON_SHARED_CACHE_STAT(badreader_unread, nonshared_unread);
	BC_ADD_SHARED_CACHE_STAT(badreader_unread, shared_unread);
#endif
}

/*
 * Setup a cache mount from the playlist mount.
 *
 * Allocates cm_pextents large enough to hold pm->pm_nentries extents,
 * but leaves cm_nextents 0 since the array hasn't been initialized.
 *
 * Returns 0 on success
 */
static int
BC_setup_mount(struct BC_cache_mount *cm, struct BC_playlist_mount* pm)
{
	int error = 0;
	
	lck_rw_init(&cm->cm_lock, BC_cache->c_lckgrp, LCK_ATTR_NULL);
	uuid_copy(cm->cm_uuid, pm->pm_uuid);
	uuid_copy(cm->cm_group_uuid, pm->pm_group_uuid);
	cm->cm_fs_flags = pm->pm_fs_flags;
	
	/* These will be set once we've detected that this volume has been mounted */
	cm->cm_dev = nulldev();
	cm->cm_bp = NULL;
	cm->cm_devsize = 0;
	cm->cm_maxread = 0;
	cm->cm_disk = NULL;
	cm->cm_blocksize = 0;
	cm->cm_nextents = 0;
	
	/* Allocate the sorted extent array.
	 * We'll fill it in while we setup the extents.
	 */
	if (pm->pm_nentries == 0) {
		message("Playlist incuded mount with 0 entries");
		error = EINVAL;
		goto out;
	}
	
	cm->cm_pextents_size = pm->pm_nentries;
	BC_MALLOC_ARRAY_WITHPOINTERS(cm->cm_pextents, cm->cm_pextents_size);
	if (cm->cm_pextents == NULL) {
		message("can't allocate mount's extent array (%d entries)", pm->pm_nentries);
		error = ENOMEM;
		goto out;
	}
	
	cm->cm_state = CM_SETUP;
	
out:
	if (error) {
		cm->cm_state = CM_ABORTED;
		BC_FREE_ARRAY_WITHPOINTERS(cm->cm_pextents, cm->cm_pextents_size);
		cm->cm_pextents_size = 0;
		lck_rw_destroy(&cm->cm_lock, BC_cache->c_lckgrp);
	}
	return error;
}

/*
 * Returns a copy of the mount's structure (not a pointer, a copy).
 *
 * Returns a zero'ed out structure on failure.
 */
static struct BC_noncached_mount
BC_get_noncached_mount(dev_t dev)
{
	struct BC_noncached_mount nm;
	
	LOCK_NONCACHED_MOUNTS_R();
	for (int i = 0; i < BC_cache->c_noncached_nmounts; i++) {
		if (BC_cache->c_noncached_mounts[i].nm_dev == dev) {
			
			// Found matching noncached mount struct
			nm = BC_cache->c_noncached_mounts[i];
			UNLOCK_NONCACHED_MOUNTS_R();
			return nm;
		}
	}
	UNLOCK_NONCACHED_MOUNTS_R();
	
	// No matching noncached mount struct for this device, create one
	uuid_t group_uuid = {0};
	bc_get_group_uuid_for_dev(dev, group_uuid);
	
	debug("dev %#x has group UUID %s", dev, uuid_string(group_uuid));

	LOCK_NONCACHED_MOUNTS_W();

	// Make sure no one else filled in this dev while we weren't holding the lock
	for (int i = 0; i < BC_cache->c_noncached_nmounts; i++) {
		if (BC_cache->c_noncached_mounts[i].nm_dev == dev) {
			// Found matching noncached mount struct
			nm = BC_cache->c_noncached_mounts[i];

			if (0 != uuid_compare(nm.nm_group_uuid, group_uuid)) {
				message("dev %#x has mismatching group UUIDs! %s vs %s", dev, uuid_string(nm.nm_group_uuid), uuid_string(group_uuid));
			} else {
				debug("Lost race for dev %d, using group %s", dev, uuid_string(nm.nm_group_uuid));
			}
			
			UNLOCK_NONCACHED_MOUNTS_W();
			return nm;
		}
	}
	
	int newSize = BC_cache->c_noncached_nmounts + 1;
	struct BC_noncached_mount* extendedArray;
	BC_MALLOC_ARRAY_NOPOINTERS(extendedArray, newSize);
	
	if (extendedArray) {
		if (BC_cache->c_noncached_mounts) {
			memmove(extendedArray, BC_cache->c_noncached_mounts, sizeof(*extendedArray) * BC_cache->c_noncached_nmounts);
			BC_FREE_ARRAY_NOPOINTERS(BC_cache->c_noncached_mounts, BC_cache->c_noncached_nmounts);
		}
		
		extendedArray[newSize - 1].nm_dev = dev;
		uuid_copy(extendedArray[newSize - 1].nm_group_uuid, group_uuid);
		
		BC_cache->c_noncached_mounts = extendedArray;
		BC_cache->c_noncached_nmounts = newSize;
		
		nm = BC_cache->c_noncached_mounts[newSize - 1];
		
		debug("Added dev %#x -> group %s at index %d", dev, uuid_string(group_uuid), newSize - 1);
		
		UNLOCK_NONCACHED_MOUNTS_W();
		return nm;
	} else {
		message("Unable to allocate %d noncached mount entries for dev %#x -> %s", newSize, dev, uuid_string(group_uuid));
	}

	UNLOCK_NONCACHED_MOUNTS_W();

	nm.nm_dev = 0;
	uuid_clear(nm.nm_group_uuid);
	return nm;
}

// Handles APFS and HFS
//
// Upon success, *devvp_out must be vnode_put()
//
// Returns 0 on success
int BC_get_dev(mount_t mount, dev_t *dev_out, vnode_t *devvp_out) {
	if (!mount || !dev_out || !devvp_out) {
		return EINVAL;
	}
	
	*dev_out = nulldev();
	*devvp_out = NULLVP;
	
	vnode_t devvp = NULLVP;

#if DEBUG
	char name[MFSNAMELEN] = "";
	vfs_name(mount, name);
#endif

	devvp = vfs_devvp(mount);
	if (devvp == NULLVP) {
		debug("mount %s does not have a vnode", name);
		return ENODEV;
	}

	dev_t specrdev = vnode_specrdev(devvp);
	if (specrdev == nulldev() || specrdev == NODEV) {
		debug("mount %s does not have a device", name);
		vnode_put(devvp);
		return ENODEV;
	}
	
	*dev_out = specrdev;
	*devvp_out = devvp;
	return 0;
}

struct discards {
	int shared_discards;
	int nonshared_discards;
};

/*
 * Teardown all extents in this mount, then tear the mount down
 *
 * Called with the cache mount write lock held
 *
 * Returns the number of byte of cache entries that were discarded
 */
static struct discards BC_teardown_mount_and_extents(struct BC_cache_mount *cm) {
	assert(cm->cm_state != CM_READY);

	/*
	 * Mark all extents as aborted. This will notify any sleepers in the
	 * strategy routine that the extent won't have data for them.
	 *
	 * Note that its fine to take the extent lock here since no one will
	 * have extent lock and try to take the mount lock if the mount isn't CM_READY
	 */
	struct discards discards = {0};
	for (int i = 0; i < cm->cm_nextents; i++) {
		LOCK_EXTENT(cm->cm_pextents[i]);
		if (! (cm->cm_pextents[i]->ce_flags & CE_ABORTED)) {
			u_int64_t count;
			if (cm->cm_pextents[i]->ce_blockmap_arraycount > 0) {
					 count = BC_discard_bytes(cm->cm_pextents[i], cm->cm_pextents[i]->ce_diskoffset, cm->cm_pextents[i]->ce_length);
			} else {
				count = cm->cm_pextents[i]->ce_length;
			}
			if (cm->cm_pextents[i]->ce_flags & CE_SHARED) {
				discards.shared_discards += count;
			} else {
				discards.nonshared_discards += count;
			}

		}
		BC_teardown_extent(cm->cm_pextents[i]);
		UNLOCK_EXTENT(cm->cm_pextents[i]);
		wakeup(cm->cm_pextents[i]);
	}
	
	BC_teardown_mount(cm);
	
	return discards;
}


/*
 * Fill in the rest of the cache mount given the matching mount structure.
 *
 * Called with the cache mount write lock held
 *
 * Returns 0 if the mount was successfully filled in and cm_state will be CM_READY
 * Returns non-0 on failure
 *   If return value is EAGAIN, the mount will still be CM_SETUP and you can try again later
 *   If return value is non-0 and non-EAGAIN, the mount and all its extents will have been torn down and it will be CM_ABORTED
 */
static int
BC_fill_in_cache_mount(struct BC_cache_mount *cm, mount_t mount, dev_t dev, vnode_t devvp, uint32_t fs_flags)
{
	if (CM_SETUP != cm->cm_state) {
		return EINVAL;
	}

	u_int64_t throttle_mask = vfs_throttle_mask(mount);
	u_int64_t disk_id = throttle_mask;
	
	if (!cache_nonroot_disks) {
		// Use vfs_devvp to check against rootvp (rather than BC_get_dev)
		vnode_t rootdevvp = vfs_devvp(mount);
		if (rootdevvp == NULLVP) {
			message("mount %s does not have a vnode", uuid_string(cm->cm_uuid));
			struct discards discards = BC_teardown_mount_and_extents(cm);
			BC_ADD_SHARED_CACHE_STAT(mounterror_unread, discards.shared_discards);
			BC_ADD_NON_SHARED_CACHE_STAT(mounterror_unread, discards.nonshared_discards);
			return EINVAL;
		}
		// Unused, so make sure we dont forget to put it
		vnode_put(rootdevvp);
		
		if (rootdevvp == rootvp) {
			if (BC_cache->c_root_disk_id == 0) {
				BC_cache->c_root_disk_id = disk_id;
				if (BC_cache->c_root_disk_id == 0) {
					message("Root disk is 0");
				} else {
					debug("Root disk (via cache) is 0x%llx", BC_cache->c_root_disk_id);
				}
			} else if (BC_cache->c_root_disk_id != disk_id) {
				debug("Root disk 0x%llx doesn't match that found by cache 0x%llx", BC_cache->c_root_disk_id, disk_id);
			}
		} else if (0 == BC_cache->c_root_disk_id) {
			return EAGAIN; /* we haven't seen the root mount yet, try again later */
			
			//rdar://11653286 disk image volumes (FileVault 1) are messing with this check, so we're going back to != rather than !( & )
		} else if (BC_cache->c_root_disk_id != disk_id) {
			message("mount %s (disk 0x%llx) is not on the root disk (disk 0x%llx)", uuid_string(cm->cm_uuid), disk_id, BC_cache->c_root_disk_id);
			struct discards discards = BC_teardown_mount_and_extents(cm);
			BC_ADD_SHARED_CACHE_STAT(nonroot_unread, discards.shared_discards);
			BC_ADD_NON_SHARED_CACHE_STAT(nonroot_unread, discards.nonshared_discards);
			return ENODEV;
		}
	}

	if (fs_flags & BC_FS_APFS_ENCRYPTION_ROLLING) {
		// <rdar://problem/32395140> Bootcache needs to be disabled for APFS volumes rolling encryption (and their container)
		message("apfs mount %s has encryption rolling in-progress, skipping.", uuid_string(cm->cm_uuid));
		struct discards discards = BC_teardown_mount_and_extents(cm);
		BC_ADD_SHARED_CACHE_STAT(unsupported_unread, discards.shared_discards);
		BC_ADD_NON_SHARED_CACHE_STAT(unsupported_unread, discards.nonshared_discards);
		return ENOTSUP;
	}

	return BC_fill_in_cache_mount_ex(cm, dev, devvp, throttle_mask, fs_flags);
}

/*
 * Fill in the rest of the cache mount given the needed data.
 *
 * Called with the cache mount write lock held
 *
 * Returns 0 if the mount was successfully filled in and cm_state will be CM_READY
 * Returns non-0 on failure. The mount and all its extents will have been torn down and it will be CM_ABORTED
 */
static int
BC_fill_in_cache_mount_ex(struct BC_cache_mount *cm, dev_t dev, vnode_t devvp, u_int64_t throttle_mask, uint32_t fs_flags)
{
	u_int64_t blkcount, max_byte_count, max_segment_count, max_segment_byte_count, max_block_count;
	uint32_t blksize, is_ssd;
	int error = 0, mount_idx, i;
	u_int64_t disk_id;
	struct BC_cache_disk *cd;
	
	if (CM_SETUP != cm->cm_state) {
		error = EINVAL;
		goto out;
	}

	cm->cm_dev = dev;

	cm->cm_throttle_mask = throttle_mask;
	disk_id = cm->cm_throttle_mask; /* using the throttle mask as a way to identify the physical disk */
	/* debug("Got throttle mask %llx for mount %s", cm->cm_throttle_mask, uuid_string(cm->cm_uuid)); */

	
	// Check for mismatch in flags
	if ((fs_flags & BC_FS_APFS_ENCRYPTED) != (cm->cm_fs_flags & BC_FS_APFS_ENCRYPTED)) {
		if (fs_flags & BC_FS_APFS_ENCRYPTED) {
			message("mount %s is encrypted when it wasn't before", uuid_string(cm->cm_uuid));
		} else {
			message("mount %s is not encrypted when it was before", uuid_string(cm->cm_uuid));
		}
		error = ENODEV;
		goto out;
	}
	cm->cm_fs_flags = fs_flags; // Overwrite flags with the ones we found from the live mount, in case there's a difference that didn't cause a failure above

	/* See if we already have a cache_disk for this disk */
	for (mount_idx = 0; mount_idx < BC_cache->c_nmounts; mount_idx++) {
		if (BC_cache->c_mounts + mount_idx == cm) continue;
		
		cd = BC_cache->c_mounts[mount_idx].cm_disk;
		
		/*
		 * We're not handling mounts backed by multiple disks as gracefull as we should.
		 * Previously, this was cd->cd_disk_id == disk_id, so we had a thread per disk combination
		 * meaning reader threads may span multiple disks and disks may have multiple reader threads.
		 * We've only ever supported the root disk, however, so this wasn't a problem, it just missed
		 * cases where you'd have other volumes on one of the disks you're booting from.
		 *
		 * Now, since we are checking for cd->cd_disk_id & disk_id, we at least include all mounts that
		 * are on disks that the root mount uses. We still only have one reader thread, but we don't support
		 * playback on composite disks, so that's not a problem yet. See rdar://10081513
		 *
		 * This assumes that the root mount (or, at least the mount we care most about) will always appear first
		 *
		 */
		if (cd && (cd->cd_disk_id & disk_id)) {
			cm->cm_disk = cd;
			break;
		}
	}
	
	/* If we don't already have a cache_disk for this disk, allocate one */
	if (cm->cm_disk == NULL) {
		BC_MALLOC_SINGLE(cd);
		if (cd == NULL) {
			message("can't allocate memory for cache disk");
			error = ENOMEM;
			goto out;
		}
		
		if (VNOP_IOCTL(devvp,       /* vnode */
					   DKIOCISSOLIDSTATE,    /* cmd */
					   (caddr_t)&is_ssd, /* data */
					   0,
					   vfs_context_current()))           /* context */
		{
			message("can't determine if disk is a solid state disk for mount %s", uuid_string(cm->cm_uuid));
			is_ssd = 0;
		}
		
		if ((error = BC_setup_disk(cd, disk_id, is_ssd)) != 0) {
			BC_FREE_SINGLE(cd);
			message("cache disk setup failed: %d", error);
			goto out;
		}
		cm->cm_disk = cd;
	}
	
	
	if (!cache_ssds) {
		if ((cm->cm_disk->cd_flags & CD_IS_SSD) &&
			!(cm->cm_fs_flags & BC_FS_APFS_FUSION) &&
			!(cm->cm_fs_flags & BC_FS_CS_FUSION)) {
			debug("BootCache not caching mount %s (SSD)", uuid_string(cm->cm_uuid));
			error = EINVAL;
			goto out;
		}
	}
	
	
	if (VNOP_IOCTL(devvp,
				   DKIOCGETBLOCKCOUNT,
				   (caddr_t)&blkcount,
				   0,
				   vfs_context_current())
		|| blkcount == 0)
	{
		message("can't determine device size, not checking");
		blkcount = 0;
	}
	
	if (VNOP_IOCTL(devvp,
				   DKIOCGETBLOCKSIZE,
				   (caddr_t)&blksize,
				   0,
				   vfs_context_current())
		|| blksize == 0)
	{
		message("can't determine device block size for mount %s", uuid_string(cm->cm_uuid));
		// HFS is 512, APFS is 4096... so give up if we can't find the correct value
		error = EINVAL;
		goto out;
	}
	
	if (PAGE_SIZE < blksize || PAGE_SIZE % blksize != 0) {
		message("PAGE_SIZE (%d) is not a multiple of block size (%d) for mount %s", PAGE_SIZE, blksize, uuid_string(cm->cm_uuid));
		error = EINVAL;
		goto out;
	}
	
	cm->cm_blocksize = blksize;
	cm->cm_devsize = blksize * blkcount;
	
	/* max read size isn't larger than the max UPL size */
	cm->cm_maxread = ubc_upl_maxbufsize();

	/* maxread = min ( maxread, MAXBYTECOUNTREAD ) */
	if (0 == VNOP_IOCTL(devvp,
						DKIOCGETMAXBYTECOUNTREAD,
						(caddr_t)&max_byte_count,
						0,
						vfs_context_current())) {
		if (cm->cm_maxread > max_byte_count && max_byte_count > 0) {
			cm->cm_maxread = max_byte_count;
			debug("MAXBYTECOUNTREAD is %#llx", max_byte_count);
		}
	}
	
	/* maxread = min ( maxread, MAXBLOCKCOUNTREAD *  BLOCKSIZE ) */
	if (0 == VNOP_IOCTL(devvp,
						DKIOCGETMAXBLOCKCOUNTREAD,
						(caddr_t)&max_block_count,
						0,
						vfs_context_current())) {
		if (cm->cm_maxread > max_block_count * cm->cm_blocksize && max_block_count > 0) {
			cm->cm_maxread = max_block_count * cm->cm_blocksize;
			debug("MAXBLOCKCOUNTREAD is %#llx, BLOCKSIZE is %#llx, (multiplied %#llx)", max_block_count, cm->cm_blocksize, max_block_count * cm->cm_blocksize);
		}
	}

	/* maxread = min ( maxread, MAXSEGMENTCOUNTREAD * min (MAXSEGMENTBYTECOUNTREAD, PAGE_SIZE ) ) */
	if (0 == VNOP_IOCTL(devvp,
						DKIOCGETMAXSEGMENTCOUNTREAD,
						(caddr_t)&max_segment_count,
						0,
						vfs_context_current())) {
		
		if (max_segment_count > 0) {
			
			if (0 == VNOP_IOCTL(devvp,
								DKIOCGETMAXSEGMENTBYTECOUNTREAD,
								(caddr_t)&max_segment_byte_count,
								0,
								vfs_context_current())) {
				//rdar://13835534 Limit max_segment_byte_count to PAGE_SIZE because some drives don't handle the spec correctly
				if (max_segment_byte_count > PAGE_SIZE || max_segment_byte_count == 0) {
					debug("MAXSEGMENTBYTECOUNTREAD is %#llx, limiting to PAGE_SIZE %#x", max_segment_byte_count, PAGE_SIZE);
					max_segment_byte_count = PAGE_SIZE;
				}
			} else {
				debug("Unable to get MAXSEGMENTBYTECOUNTREAD, assuming PAGE_SIZE %#x", PAGE_SIZE);
				max_segment_byte_count = PAGE_SIZE;
			}
			
			if (cm->cm_maxread > max_segment_count * max_segment_byte_count) {
				cm->cm_maxread = max_segment_count * max_segment_byte_count;
				debug("MAXSEGMENTCOUNTREAD is %#llx, MAXSEGMENTBYTECOUNTREAD is %#llx, (multiplied %#llx)", max_segment_count, max_segment_byte_count, max_segment_count * max_segment_byte_count);
			}
		}
	}
	
	/* maxread = min ( maxread, MAX_UPL_TRANSFER * PAGE_SIZE ) */
	if (cm->cm_maxread > MAX_UPL_TRANSFER * PAGE_SIZE) {
		cm->cm_maxread = MAX_UPL_TRANSFER * PAGE_SIZE;
		debug("MAX_UPL_TRANSFER is %#x, PAGE_SIZE is %#x, (multiplied %#x)", MAX_UPL_TRANSFER, PAGE_SIZE, MAX_UPL_TRANSFER * PAGE_SIZE);
	}
	
	/* maxread = max ( maxread, MAXPHYS ) */
	if (cm->cm_maxread < MAXPHYS) {
		debug("can't determine device read size for mount %s; using default", uuid_string(cm->cm_uuid));
		cm->cm_maxread = MAXPHYS;
	}

	/* make sure maxread is a multiple of the block size */
	if (cm->cm_maxread % cm->cm_blocksize != 0) {
		debug("Mount max IO size (%#llx) not a multiple of block size (%#llx)", cm->cm_maxread, cm->cm_blocksize);
		cm->cm_maxread -= cm->cm_maxread % cm->cm_blocksize;
	}
	
	cm->cm_bp = buf_alloc(devvp);
	if (cm->cm_bp == NULL) {
		message("can't allocate buf");
		error = ENOMEM;
		goto out;
	}
	
	int nonshared_discards = 0;
	int shared_discards = 0;
	for (i = 0; i < cm->cm_nextents; i++) {
		LOCK_EXTENT(cm->cm_pextents[i]);
		if (0 != BC_fill_in_extent(cm->cm_pextents[i])) {
			if (cm->cm_pextents[i]->ce_flags & CE_SHARED) {
				shared_discards += cm->cm_pextents[i]->ce_length;
			} else {
				nonshared_discards += cm->cm_pextents[i]->ce_length;
			}
			BC_teardown_extent(cm->cm_pextents[i]);
			UNLOCK_EXTENT(cm->cm_pextents[i]);
			wakeup(cm->cm_pextents[i]);
		} else {
			UNLOCK_EXTENT(cm->cm_pextents[i]);
		}
	}
	BC_ADD_NON_SHARED_CACHE_STAT(extenterror_unread, nonshared_discards);
	BC_ADD_SHARED_CACHE_STAT(extenterror_unread, shared_discards);
	
	cm->cm_state = CM_READY;
	
	debug("Found new cache mount %s dev 0x%x, disk %d, %d extents, flags 0x%x", uuid_string(cm->cm_uuid), cm->cm_dev, cm->cm_disk->cd_disk_num, cm->cm_nextents, cm->cm_fs_flags);
	
	return 0;
	
out:
	{
		debug_extents(cm - BC_cache->c_mounts, NULL, 0, 0, "fill-in failed");
		struct discards discards = BC_teardown_mount_and_extents(cm);
		BC_ADD_SHARED_CACHE_STAT(mounterror_unread, discards.shared_discards);
		BC_ADD_NON_SHARED_CACHE_STAT(mounterror_unread, discards.nonshared_discards);

	}
	return error;
}

/*
 * Called with the cache mount write lock held
 */
static void
BC_teardown_mount(struct BC_cache_mount *cm)
{
	if (cm->cm_state == CM_SETUP) {
		debug("never found mount %s with %d entries", uuid_string(cm->cm_uuid), cm->cm_nextents);
	}
	
	if (cm->cm_bp) {
		buf_free(cm->cm_bp);
		cm->cm_bp = NULL;
	}
	cm->cm_nextents = 0;
	BC_FREE_ARRAY_WITHPOINTERS(cm->cm_pextents, cm->cm_pextents_size);
	cm->cm_pextents_size = 0;
	cm->cm_state = CM_ABORTED;
}

/*
 * Check to see which mounts have been mounted and finish setting them up.
 *
 * No extent/mount locks required since we have a write lock on the cache
 *
 * Called with the cache write lock held
 */
static int fill_in_bc_cache_mounts(mount_t mount, void* arg)
{
	int mount_idx, error, i;
	struct vfs_attr attr;
	int* go_again = (int*) arg;
	
	VFSATTR_INIT(&attr);
	VFSATTR_WANTED(&attr, f_uuid);
	error = vfs_getattr(mount, &attr, vfs_context_current());
	if ((0 != error) || (! VFSATTR_IS_SUPPORTED(&attr, f_uuid))) {
#ifdef BC_DEBUG
		char name[MFSNAMELEN];
		vfs_name(mount, name);
		if (strncmp("devfs", name, sizeof("devfs")) != 0) {
			debug("Found mount %s for IO without a UUID: error %d", name, error);
		}
#endif
		return VFS_RETURNED;
	} else {
		// debug("Found mount for IO with UUID %s", uuid_string(attr.f_uuid));
		
		for (mount_idx = 0; mount_idx < BC_cache->c_nmounts; mount_idx++) {
			struct BC_cache_mount *cm = BC_cache->c_mounts + mount_idx;
			int match = 0;
			if (CM_SETUP == cm->cm_state) {
				
				if (0 == uuid_compare(cm->cm_uuid, attr.f_uuid)) {
					match = 1;
				}
				
				/* a null uuid indicates we want to match the root volume, no matter what the uuid (8350414) */
				bool was_generalized = false;
				if ((!match) && uuid_is_null(cm->cm_uuid)) {
					vnode_t devvp = vfs_devvp(mount); // Use vfs_devvp for comparison to rootdev (rather than BC_get_dev)
					if (vnode_specrdev(devvp) == rootdev) {
						uuid_copy(cm->cm_uuid, attr.f_uuid);
						match = 1;
						was_generalized = true;
						debug("Found root mount %s", uuid_string(cm->cm_uuid));
					}
					vnode_put(devvp);
				}
				
				if (match) {
					/* Found a matching mount */
					
					// lookup dev and devvp from mount
					dev_t dev = nulldev();
					vnode_t devvp = NULLVP;
					error = BC_get_dev(mount, &dev, &devvp);
					if (error == 0) {
						
						uint32_t fs_flags = 0;
						dev_t container_dev = nulldev();
						uuid_t container_uuid = {0};
						bool container_has_encrypted_volumes = true;
						bool container_has_rolling_volumes = true;
						bc_get_volume_info(dev, &fs_flags, &container_dev, container_uuid, &container_has_encrypted_volumes, &container_has_rolling_volumes);
						
						if (was_generalized) {
							uuid_copy(cm->cm_group_uuid, container_uuid);
						}
						
						// All APFS volumes must have a non-null container UUID
						// <rdar://problem/43851924> When discarding data, discard data for that extent from all mounts in the same apfs container
						if (fs_flags & BC_FS_APFS) {
							if (uuid_is_null(container_uuid) || 0 != uuid_compare(cm->cm_group_uuid, container_uuid)) {
								
								// We don't expect the container UUID to ever change, so error out if it happens
								message("Not caching apfs volume %s with wrong container %s (really is %s)", uuid_string(cm->cm_uuid), uuid_string(cm->cm_group_uuid), uuid_string(container_uuid));
								
								struct discards discards = BC_teardown_mount_and_extents(cm);
								BC_ADD_SHARED_CACHE_STAT(unsupported_unread, discards.shared_discards);
								BC_ADD_NON_SHARED_CACHE_STAT(unsupported_unread, discards.nonshared_discards);
								return VFS_RETURNED_DONE;
							}
						}
						
						
						/* Locking here isn't necessary since we're only called while holding the cache write lock
						 * and no one holds the mount locks without also holding the cache lock
						 */
						if (BC_fill_in_cache_mount(cm, mount, dev, devvp, fs_flags) == EAGAIN) {
							*go_again = 1;
						}
						vnode_put(devvp);
						devvp = NULLVP;
						
						if (cm->cm_state == CM_READY &&
							(cm->cm_fs_flags & BC_FS_APFS)) {
							
							//---------------------------
							// Check for apfs container
							//---------------------------
							// <rdar://problem/31565082> BootCache should track I/Os on the APFS volume and play back
							//
							// We track apfs volumes, but there is still some I/O directly to the container, so we need to track the container as well as the volume
							
							if (container_dev == nulldev() || container_dev == NODEV || uuid_is_null(container_uuid)) {
								message("mount %s has no container", uuid_string(cm->cm_uuid));
							} else {
								debug("mount %s has container %s dev 0x%x (%sfusion)", uuid_string(cm->cm_uuid), uuid_string(container_uuid), container_dev, (fs_flags & BC_FS_APFS_FUSION) ? "" : "not ");
								
								for (mount_idx = 0; mount_idx < BC_cache->c_nmounts; mount_idx++) {
									struct BC_cache_mount *container_cm = BC_cache->c_mounts + mount_idx;
									if (CM_SETUP == container_cm->cm_state) {
										if (0 == uuid_compare(container_cm->cm_uuid, container_uuid)) {
											
											if (0 != uuid_compare(container_cm->cm_uuid, container_cm->cm_group_uuid)) {
												message("Not caching apfs container %s with mismatched group %s", uuid_string(container_cm->cm_uuid), uuid_string(container_cm->cm_group_uuid));
												struct discards discards = BC_teardown_mount_and_extents(container_cm);
												BC_ADD_SHARED_CACHE_STAT(unsupported_unread, discards.shared_discards);
												BC_ADD_NON_SHARED_CACHE_STAT(unsupported_unread, discards.nonshared_discards);
												break;
											}
											
											if (container_has_rolling_volumes) {
												// <rdar://problem/32395140> Bootcache needs to be disabled for APFS volumes rolling encryption (and their container)
												message("Not caching apfs container %s that contains rolling volumes", uuid_string(container_uuid));
												struct discards discards = BC_teardown_mount_and_extents(container_cm);
												BC_ADD_SHARED_CACHE_STAT(unsupported_unread, discards.shared_discards);
												BC_ADD_NON_SHARED_CACHE_STAT(unsupported_unread, discards.nonshared_discards);
												break;
											}
											
											// Get vnode_t for the container
											char apfsdevpath[64] = "";
											lookup_dev_name(container_dev, apfsdevpath, sizeof(apfsdevpath));
											if (apfsdevpath[0] == '\0') {
												message("unable to get apfs dev path for container %s", uuid_string(container_uuid));
												struct discards discards = BC_teardown_mount_and_extents(container_cm);
												BC_ADD_SHARED_CACHE_STAT(mounterror_unread, discards.shared_discards);
												BC_ADD_NON_SHARED_CACHE_STAT(mounterror_unread, discards.nonshared_discards);
												break;
											}
											debug("apfs container %s dev path is %s", uuid_string(container_uuid), apfsdevpath);
											
											vnode_t container_devvp = NULLVP;
											errno_t err = vnode_lookup(apfsdevpath, 0, &container_devvp, vfs_context_current());
											if (err != 0 || container_devvp == NULLVP) {
												message("unable to get open apfs container %s dev path %s: %d", uuid_string(container_uuid), apfsdevpath, err);
												struct discards discards = BC_teardown_mount_and_extents(container_cm);
												BC_ADD_SHARED_CACHE_STAT(mounterror_unread, discards.shared_discards);
												BC_ADD_NON_SHARED_CACHE_STAT(mounterror_unread, discards.nonshared_discards);
												break;
											}
											
											uint32_t flags = BC_FS_APFS | BC_FS_APFS_CONTAINER | (fs_flags & BC_FS_APFS_FUSION);
											
											BC_fill_in_cache_mount_ex(container_cm, container_dev, container_devvp, cm->cm_throttle_mask, flags);
											vnode_put(container_devvp);
											break;
										}
									}
								}
							}
						}
					}
					
					
					
					/* Check to see if we have any more mounts to fill in */
					for (i = 0; i < BC_cache->c_nmounts; i++) {
						if (CM_SETUP == BC_cache->c_mounts[i].cm_state) {
							/* have more to fill in, keep iterating */
							return VFS_RETURNED;
						}
					}
					
					return VFS_RETURNED_DONE;
				}
			}
		}
	} 

	/* Check to see if we have any containers to fill in (this case handles containers without any volumes in the playlist) */
	for (i = 0; i < BC_cache->c_nmounts; i++) {
		struct BC_cache_mount *container_cm = BC_cache->c_mounts + i;
		if (CM_SETUP == BC_cache->c_mounts[i].cm_state &&
			(BC_cache->c_mounts[i].cm_fs_flags & BC_FS_APFS_CONTAINER)) {
			
			// lookup dev and devvp from mount
			dev_t dev = nulldev();
			vnode_t devvp = NULLVP;
			error = BC_get_dev(mount, &dev, &devvp);
			if (error != 0) {
				continue;
			}
			vnode_put(devvp);
			devvp = NULLVP;

			uint32_t fs_flags = 0;
			dev_t container_dev = nulldev();
			uuid_t container_uuid = {0};
			bool container_has_encrypted_volumes = true;
			bool container_has_rolling_volumes = true;
			bc_get_volume_info(dev, &fs_flags, &container_dev, container_uuid, &container_has_encrypted_volumes, &container_has_rolling_volumes);
			
			if (!(fs_flags & BC_FS_APFS)) {
				debug("volume %s not apfs", uuid_string(attr.f_uuid));
				continue;
			}

			if (container_dev == nulldev() || container_dev == NODEV || uuid_is_null(container_uuid)) {
				message("apfs volume %s has no container", uuid_string(attr.f_uuid));
				continue;
			}
			debug("volume %s has container %s dev 0x%x (%sfusion)", uuid_string(attr.f_uuid), uuid_string(container_uuid), container_dev, (fs_flags & BC_FS_APFS_FUSION) ? "" : "not ");

			if (0 == uuid_compare(container_cm->cm_uuid, container_uuid)) {
				
				if (0 != uuid_compare(container_cm->cm_uuid, container_cm->cm_group_uuid)) {
					message("Not caching apfs container %s with mismatched group %s", uuid_string(container_cm->cm_uuid), uuid_string(container_cm->cm_group_uuid));
					struct discards discards = BC_teardown_mount_and_extents(container_cm);
					BC_ADD_SHARED_CACHE_STAT(unsupported_unread, discards.shared_discards);
					BC_ADD_NON_SHARED_CACHE_STAT(unsupported_unread, discards.nonshared_discards);
					break;
				}

				if (container_has_rolling_volumes) {
					// <rdar://problem/32395140> Bootcache needs to be disabled for APFS volumes rolling encryption (and their container)
					message("Not caching apfs container %s that contains rolling volumes", uuid_string(container_uuid));
					struct discards discards = BC_teardown_mount_and_extents(container_cm);
					BC_ADD_SHARED_CACHE_STAT(unsupported_unread, discards.shared_discards);
					BC_ADD_NON_SHARED_CACHE_STAT(unsupported_unread, discards.nonshared_discards);
					break;
				}
				
				// Get vnode_t for the container
				char apfsdevpath[64] = "";
				lookup_dev_name(container_dev, apfsdevpath, sizeof(apfsdevpath));
				if (apfsdevpath[0] == '\0') {
					message("unable to get apfs dev path for container %s", uuid_string(container_uuid));
					struct discards discards = BC_teardown_mount_and_extents(container_cm);
					BC_ADD_SHARED_CACHE_STAT(mounterror_unread, discards.shared_discards);
					BC_ADD_NON_SHARED_CACHE_STAT(mounterror_unread, discards.nonshared_discards);
					break;
				}
				debug("apfs container %s dev path is %s", uuid_string(container_uuid), apfsdevpath);
				
				vnode_t container_devvp = NULLVP;
				errno_t err = vnode_lookup(apfsdevpath, 0, &container_devvp, vfs_context_current());
				if (err != 0 || container_devvp == NULLVP) {
					message("unable to get open apfs container %s dev path %s: %d", uuid_string(container_uuid), apfsdevpath, err);
					struct discards discards = BC_teardown_mount_and_extents(container_cm);
					BC_ADD_SHARED_CACHE_STAT(mounterror_unread, discards.shared_discards);
					BC_ADD_NON_SHARED_CACHE_STAT(mounterror_unread, discards.nonshared_discards);
					break;
				}
				
				uint32_t flags = BC_FS_APFS | BC_FS_APFS_CONTAINER | (fs_flags & BC_FS_APFS_FUSION);
				
				u_int64_t throttle_mask = vfs_throttle_mask(mount);

				BC_fill_in_cache_mount_ex(container_cm, container_dev, container_devvp, throttle_mask, flags);
				vnode_put(container_devvp);
				break;
			}

		}
	}

	
	/* Check to see if we have any more mounts to fill in */
	for (i = 0; i < BC_cache->c_nmounts; i++) {
		if (CM_SETUP == BC_cache->c_mounts[i].cm_state) {
			/* have more to fill in, keep iterating */
			return VFS_RETURNED;
		}
	}
	
	debug("No mounts need to be filled");
	return VFS_RETURNED_DONE;
	
}

/*
 * Fetch the playlist from userspace.
 * Add it to the cache and readahead in batches after
 * the current cache, if any.
 *
 * Returns 0 on success. In failure cases, the cache is not modified.
 *
 * Called with the cache write lock held.
 */
static int
BC_copyin_playlist(const size_t mounts_size, const user_addr_t mounts_buf, const size_t entries_size, const user_addr_t entries_buf)
{
	struct BC_playlist_mount *playlist_mounts = NULL, *pm;
	struct BC_playlist_entry *playlist_entries = NULL, *pe;
	const u_int64_t nplaylist_entries = entries_size / sizeof(*playlist_entries);;
	const u_int nplaylist_mounts = (u_int) (mounts_size / sizeof(*playlist_mounts));
	struct BC_cache_mount  *cm, *old_cm;
	struct BC_cache_extent *ce;
	
	u_int ncache_mounts = 0, nnew_mounts = 0;
	u_int64_t ncache_extents = 0;
	struct BC_cache_mount *cache_mounts = NULL;
	struct BC_cache_extent *cache_extents = NULL;
	int *pmidx_to_cmidx = NULL;
	u_int64_t *next_old_extent_idx = NULL;
	
	u_int64_t *cache_nextents = NULL;
	struct BC_cache_extent **cache_extents_list = NULL;
	u_int num_cache_extents_lists = 0;
	
	u_int64_t nexents_shared = 0;
	u_int64_t nextents_nonshared = 0;
	
	int old_batch_count;
	u_int64_t old_cache_size;

	int error;
	u_int pm_idx, cm_idx, pm_idx2;
	u_int64_t pe_idx, ce_idx, max_entries;
	off_t p;
	u_int64_t size, num_shared_bytes = 0, num_nonshared_bytes = 0;
	u_int64_t actual, remaining_entries;
	
	int clipped_extents = 0, enveloped_extents = 0; 
	
	playlist_mounts = NULL;
	playlist_entries = NULL;
	
	if (BC_cache->c_nmounts > 0) {
		// BootCache doesn't support adding additional playlists when a cache is already in progress
		// TODO: Update cache construction code below to remove parts that handle additional playlists
		return EBUSY;
	}
	
#ifdef BC_DEBUG
	struct timeval start_time;
	microtime(&start_time);
#endif
	
	old_batch_count = BC_cache->c_batch_count;
	old_cache_size = BC_cache->c_cachesize;

	if (BC_cache->c_stats.ss_sharedcache.ss_total_extents + BC_cache->c_stats.ss_nonsharedcache.ss_total_extents > 0) {
		debug("adding %llu extents to existing cache of %llu extents", nplaylist_entries, BC_cache->c_stats.ss_sharedcache.ss_total_extents + BC_cache->c_stats.ss_nonsharedcache.ss_total_extents);
	} else {
		debug("setting up first cache of %llu extents", nplaylist_entries);
	}
	
	// We hold the write lock to the cache during this function, so BC_cache->c_nmounts won't be modified, but using a local variable - old_ncache_mounts - makes the static analyzer happy
	const int old_ncache_mounts = BC_cache->c_nmounts;
	for (cm_idx = 0; cm_idx < old_ncache_mounts; cm_idx++) {
		debug("Mount %s has %d extents", uuid_string(BC_cache->c_mounts[cm_idx].cm_uuid), BC_cache->c_mounts[cm_idx].cm_nextents);
	}

	/*
	 * Mount array
	 */
	BC_MALLOC_ARRAY_NOPOINTERS(playlist_mounts, nplaylist_mounts);
	if (playlist_mounts == NULL) {
		message("can't allocate unpack mount buffer of %ld bytes", mounts_size);
		error = ENOMEM;
		goto out;
	}
	if ((error = copyin(mounts_buf, playlist_mounts, mounts_size)) != 0) {
		message("copyin %ld bytes from 0x%llx to %p failed", mounts_size, mounts_buf, playlist_mounts);
		goto out;
	}
	
	/* map from the playlist mount index to the corresponding cache mount index */
	BC_MALLOC_ARRAY_NOPOINTERS(pmidx_to_cmidx, nplaylist_mounts);
	if (pmidx_to_cmidx == NULL) {
		message("can't allocate playlist index to cache index reference array for %u mounts", nplaylist_mounts);
		error = ENOMEM;
		goto out;
	}
	
	/* In order to merge the mount's current (sorted) extent list with the new (sorted)
	 * extent list from the new playlist we first grow the extent list allocation to fit
	 * both arrays, then merge the two arrays into the new allocation.
	 */
	
	if (old_ncache_mounts > 0) {
		/* next_old_extent_idx saves the index of the next unmerged extent in the original mount's extent list */
		BC_MALLOC_ARRAY_NOPOINTERS(next_old_extent_idx, old_ncache_mounts);
		if (next_old_extent_idx == NULL) {
			message("can't allocate index array for %d mounts", old_ncache_mounts);
			error = ENOMEM;
			goto out;
		}
		
		/* 0-filled since all the index start at 0 */
	}
	
	nnew_mounts = nplaylist_mounts;
	
	/* determine how many mounts are new and how many we already have */
	for (pm_idx = 0; pm_idx < nplaylist_mounts; pm_idx++) {
		pmidx_to_cmidx[pm_idx] = -1; /* invalid by default */
		
		for (cm_idx = 0; cm_idx < old_ncache_mounts; cm_idx++) {
			if (0 == uuid_compare(BC_cache->c_mounts[cm_idx].cm_uuid, playlist_mounts[pm_idx].pm_uuid)) {
				/* already have a record of this mount, won't need a new spot for it */
				pmidx_to_cmidx[pm_idx] = cm_idx;
				nnew_mounts--;
				break;
			}
		}
		
		/* check to make sure there aren't any duplicate mounts within the playlist */
		for (pm_idx2 = pm_idx + 1; pm_idx2 < nplaylist_mounts; pm_idx2++) {
			if (0 == uuid_compare(playlist_mounts[pm_idx2].pm_uuid, playlist_mounts[pm_idx].pm_uuid)) {
				message("Playlist had duplicate mount %s", uuid_string(playlist_mounts[pm_idx2].pm_uuid));
				error = EINVAL;
				goto out;
			}
		}
	}
	
	/* cache_mounts is the array of mounts that will replace the one currently in the cache */
	BC_MALLOC_ARRAY_WITHPOINTERS(cache_mounts, old_ncache_mounts + nnew_mounts);
	if (cache_mounts == NULL) {
		message("can't allocate memory for %u mounts", (old_ncache_mounts + nnew_mounts));
		error = ENOMEM;
		goto out;
	}
	memmove(cache_mounts, BC_cache->c_mounts, (old_ncache_mounts * sizeof(*BC_cache->c_mounts)));
	
	/* ncache_mounts is the current number of valid mounts in the mount array */
	ncache_mounts = old_ncache_mounts;
	
	for (pm_idx = 0; pm_idx < nplaylist_mounts; pm_idx++) {
		if (pmidx_to_cmidx[pm_idx] != -1) {
			/* We already have a record for this mount */
			
			if (playlist_mounts[pm_idx].pm_nentries == 0) {
				message("Playlist incuded mount with 0 entries");
				error = EINVAL;
				goto out;
			}

			cm_idx = pmidx_to_cmidx[pm_idx];
			
			assert(cm_idx < old_ncache_mounts);

			/* grow the mount's extent array to fit the new extents (we'll fill in the new extents below) */
			cache_mounts[cm_idx].cm_pextents_size = (BC_cache->c_mounts[cm_idx].cm_nextents + playlist_mounts[pm_idx].pm_nentries);
			BC_MALLOC_ARRAY_WITHPOINTERS(cache_mounts[cm_idx].cm_pextents, cache_mounts[cm_idx].cm_pextents_size);
			if (cache_mounts[cm_idx].cm_pextents == NULL) {
				message("can't allocate mount's extent array (%d entries)", (BC_cache->c_mounts[cm_idx].cm_nextents + playlist_mounts[pm_idx].pm_nentries));
				error = ENOMEM;
				goto out;
			}

			/* don't free the old extent list yet, the real BC_cache's mount still points to it and we may yet fail */

			/* cm_nextents is the number of valid extents in this mount's extent list. It starts out as 0 and we fill it in below */
			cache_mounts[cm_idx].cm_nextents = 0;
		} else {
			/* New mount for the cache */
			
			if ((error = BC_setup_mount(cache_mounts + ncache_mounts, playlist_mounts + pm_idx)) != 0) {
				goto out;
			}
			
			uuid_copy(BC_cache->c_stats.ss_mount_uuid[ncache_mounts], cache_mounts[ncache_mounts].cm_uuid);
			
			pmidx_to_cmidx[pm_idx] = ncache_mounts;
			ncache_mounts++;
		}
	}
	
	/* Shrink cache_mount array if we had some duplicate mounts.
	 * Needed so that the allocation remains ncache_mounts size for when we later free it
	 */
	if (ncache_mounts < old_ncache_mounts + nnew_mounts) {
		struct BC_cache_mount *temp_cache_mounts;
		BC_MALLOC_ARRAY_WITHPOINTERS(temp_cache_mounts, ncache_mounts);
		if (temp_cache_mounts == NULL) {
			message("can't allocate memory for %u mounts", ncache_mounts);
			error = ENOMEM;
			goto out;
		}
		memmove(temp_cache_mounts, cache_mounts, (ncache_mounts * sizeof(*cache_mounts)));
		BC_FREE_ARRAY_WITHPOINTERS(cache_mounts, (old_ncache_mounts + nnew_mounts));
		cache_mounts = temp_cache_mounts;
	}
	
	/* 
	 * Extent array
	 *
	 * The existing extent array cannot move since a strategy routine may be
	 * in the middle of an msleep with one of the extent locks. So, we allocate
	 * a new array for the new extents.
	 *
	 * Add one extent list to our arrays of extent lists
	 */
		
	num_cache_extents_lists = BC_cache->c_nextentlists + 1;
	/* cache_nextents is an array of ints, each indicating the number of extents in the list in cache_extents_list at the same index */
	BC_MALLOC_ARRAY_NOPOINTERS(cache_nextents, num_cache_extents_lists);
	if (cache_nextents == NULL) {
		message("can't allocate memory for %u extent list sizes", num_cache_extents_lists);
		error = ENOMEM;
		goto out;
	}
	memmove(cache_nextents, BC_cache->c_nextents, BC_cache->c_nextentlists * sizeof(*BC_cache->c_nextents));
	
	/* cache_extents_list is the array of extent lists. The extent list at the last index is for the new playlist's cache */
	BC_MALLOC_ARRAY_WITHPOINTERS(cache_extents_list, num_cache_extents_lists);
	if (cache_extents_list == NULL) {
		message("can't allocate memory for %u extent lists", num_cache_extents_lists);
		error = ENOMEM;
		goto out;
	}
	memmove(cache_extents_list,  BC_cache->c_extentlists,  BC_cache->c_nextentlists * sizeof(*BC_cache->c_extentlists));
	
	/* The extent list for this new playlist's cache */
	BC_MALLOC_ARRAY_WITHPOINTERS(cache_extents, nplaylist_entries);
	if (cache_extents == NULL) {
		message("can't allocate memory for %llu extents", nplaylist_entries);
		error = ENOMEM;
		goto out;
	}
	
	/* TODO: iterate over our history and remove any blocks we've already seen from the new cache */
		
	/* Fill in the new extents.
	 *
	 * We just tack the new playlist onto the end of any cache we currently have as a new batch.
	 *
	 * At this point, we assume that any additional caches should be in additional batches just as
	 * if there was a single recording with a tag separating the new extents. If we did decide to
	 * merge, it would still be hard since that would require reordering the extent list and
	 * BC_strategy assumes that the cache extents never move (see the msleep in wait_for_extent).
	 *
	 * We also don't coalesce the new extents into the old (and lower batch) extents.
	 * This would be a bit of work, we assume we want a new batch anyway, and it's rare that
	 * a new cache comes in while an old cache is still in readahead. So, we don't bother.
	 *
	 * We do clip any overlap with the new cache from the old cache, however.
	 *
	 * We don't clip any overlap with the history from the new extents.
	 * The history list isn't ordered, so we don't have a fast way to compare the ranges. It's also
	 * expected that the overlap is minimal. If we stopped recording history at some point, we don't
	 * have this info anyway. So, we don't bother.
	 */
	
	/* size is the size in bytes of the cache, used to allocate our page buffer */
	size = 0;
	/* num_shared_bytes and num_nonshared_bytes is the number of bytes in the cache (the difference between the sum of those two and size is overhead due to alignment) */
	num_shared_bytes = 0;
	num_nonshared_bytes = 0;
	
	/*
	 * Since the playlist control entry structure is not the same as the
	 * extent structure we use, we need to copy the control entries in
	 * and unpack them.
	 */
	BC_MALLOC_ARRAY_NOPOINTERS(playlist_entries, BC_PLC_CHUNK);
	if (playlist_entries == NULL) {
		message("can't allocate unpack buffer for %d entries", BC_PLC_CHUNK);
		error = ENOMEM;
		goto out;
	}
	
	remaining_entries = nplaylist_entries;
	user_addr_t next_entry_in_buf = entries_buf;
	while (remaining_entries > 0) {
		
		actual = MIN(remaining_entries, BC_PLC_CHUNK);
		if ((error = copyin(next_entry_in_buf, playlist_entries,
							actual * sizeof(struct BC_playlist_entry))) != 0) {
			message("copyin from 0x%llx to %p failed", next_entry_in_buf, playlist_entries);
			goto out;
		}
		
		/* unpack into our array */
		for (pe_idx = 0; pe_idx < actual; pe_idx++) {
			pe = playlist_entries + pe_idx;
			if (pe->pe_length == 0) {
				debug("Bad Playlist: entry %llu has 0 length", (nplaylist_entries - remaining_entries) + pe_idx);
				error = EINVAL;
				goto out;
			}
			pm_idx = pe->pe_mount_idx;
			
			if (pm_idx >= nplaylist_mounts) {
				message("Bad playlist: entry %llu referenced non-existent mount index %u", (nplaylist_entries - remaining_entries) + pe_idx, pm_idx);
				error = EINVAL;
				goto out;
			}
			
			pm = playlist_mounts + pm_idx;
			cm_idx = pmidx_to_cmidx[pm_idx];
			cm = cache_mounts + cm_idx;
			ce = cache_extents + ncache_extents;
			
			/* The size of the extent list is the number of playlist entries + the number of old cache extents */
			max_entries = pm->pm_nentries + ((cm_idx < old_ncache_mounts) ? BC_cache->c_mounts[cm_idx].cm_nextents : 0);
			
			if (cm->cm_nextents >= max_entries) {
				message("Bad playlist: more entries existed than the mount %s claimed (%d)", uuid_string(pm->pm_uuid), pm->pm_nentries);
				error = EINVAL;
				goto out;
			}			
			
			if (pe->pe_flags & BC_PE_SHARED) {
				nexents_shared++;
			} else {
				nextents_nonshared++;
			}
			if ((error = BC_setup_extent(ce, pe, old_batch_count, cm_idx)) != 0) {
				goto out;
			}
			
			/* Merge the new extent with the old extents for this mount. The new extent may get clipped. */
			if (cm_idx < old_ncache_mounts) {
				old_cm = BC_cache->c_mounts + cm_idx;
				/* Copy any lower or equal extents from the existing playlist down to the low end of the array */
				while (next_old_extent_idx[cm_idx] < old_cm->cm_nextents &&
					   old_cm->cm_pextents[next_old_extent_idx[cm_idx]]->ce_diskoffset <= ce->ce_diskoffset) {
					cm->cm_pextents[cm->cm_nextents] = old_cm->cm_pextents[next_old_extent_idx[cm_idx]];
					cm->cm_nextents++;
					next_old_extent_idx[cm_idx]++;
				}
				
				/* check for overlap with the next extent in the old list and clip the new extent */
				if (next_old_extent_idx[cm_idx] < old_cm->cm_nextents) {
					
					/* FIXME: rdar://9153031 If the new extent extends past the end of the old extent, we're losing caching! */
					if (ce->ce_diskoffset + ce->ce_length > (old_cm->cm_pextents[next_old_extent_idx[cm_idx]]->ce_diskoffset + old_cm->cm_pextents[next_old_extent_idx[cm_idx]]->ce_length)) {
						debug("!!! Entry %llu (0x%llx:%#llx) is getting clipped too much because of previous entry for mount %s (0x%llx:%#llx)", pe_idx, ce->ce_diskoffset, ce->ce_length, uuid_string(cm->cm_uuid), old_cm->cm_pextents[next_old_extent_idx[cm_idx]]->ce_diskoffset, old_cm->cm_pextents[next_old_extent_idx[cm_idx]]->ce_length);
					}
					
					u_int64_t max_offset = old_cm->cm_pextents[next_old_extent_idx[cm_idx]]->ce_diskoffset;
					if (max_offset < ce->ce_diskoffset + ce->ce_length) {
						if (max_offset <= ce->ce_diskoffset) {
							ce->ce_length = 0;
						} else {
							ce->ce_length = (max_offset - ce->ce_diskoffset);
						}
					}
				}
			}
			
			/* check for intersection with the next lower extent in the list and clip the new extent */
			if (cm->cm_nextents > 0) {
				u_int64_t min_offset = cm->cm_pextents[cm->cm_nextents - 1]->ce_diskoffset + cm->cm_pextents[cm->cm_nextents - 1]->ce_length;
				if (min_offset > ce->ce_diskoffset) {
					
					/* Check if this extent is overlapping with an extent from the same playlist */
					if (cm->cm_pextents[cm->cm_nextents - 1] >= cache_extents &&
						cm->cm_pextents[cm->cm_nextents - 1] < cache_extents + ncache_extents) {
						message("Bad playlist: entry %llu (0x%llx:%#llx) overlapped with previous entry for mount %s (0x%llx:%#llx)", pe_idx, ce->ce_diskoffset, ce->ce_length, uuid_string(cm->cm_uuid), cm->cm_pextents[cm->cm_nextents - 1]->ce_diskoffset, cm->cm_pextents[cm->cm_nextents - 1]->ce_length);
						error = EINVAL;
						goto out;
					}
					
					if (min_offset >= ce->ce_diskoffset + ce->ce_length) {
						ce->ce_length = 0;
					} else {
						ce->ce_length -= (min_offset - ce->ce_diskoffset);
						ce->ce_diskoffset = min_offset;
					}
				}
			}

			if (ce->ce_length != pe->pe_length) {
				clipped_extents++;
			}
			if (ce->ce_length == 0) {
				/* new extent is entirely captured in extents from the old cache, throw it out */
				enveloped_extents++;
				BC_teardown_extent(ce);
				lck_mtx_destroy(&ce->ce_lock, BC_cache->c_lckgrp);
				
				/* continue without incrementing ncache_extents so we reuse this extent index */
				continue;
			}
			
			// Stats for BOOTCACHE_ENTRIES_SORTED_BY_DISK_OFFSET
			size += roundup(ce->ce_length, PAGE_SIZE);
			if (ce->ce_flags & CE_SHARED) {
				num_shared_bytes += ce->ce_length;
			} else {
				num_nonshared_bytes += ce->ce_length;
			}
			
			cm->cm_pextents[cm->cm_nextents] = ce;
			cm->cm_nextents++;
			ncache_extents++;
		}
		remaining_entries -= actual;
		next_entry_in_buf += actual * sizeof(struct BC_playlist_entry);
	}
	
	/* Fill in any remaining extents from the original cache that are still sitting at the high end of the extent list */
	for (pm_idx = 0; pm_idx < nplaylist_mounts; pm_idx++) {
		cm_idx = pmidx_to_cmidx[pm_idx];
		cm = cache_mounts + cm_idx;
		pm = playlist_mounts + pm_idx;
		
		if (cm->cm_nextents == 0) {
			message("Bad playlist: No entries existed for mount %s (claimed %d)", uuid_string(pm->pm_uuid), pm->pm_nentries);
			error = EINVAL;
			goto out;
		}
		
		if (cm_idx < old_ncache_mounts) {
			if (next_old_extent_idx[cm_idx] < BC_cache->c_mounts[cm_idx].cm_nextents) {
				/* There are more extents in the old extent array, copy them over */
				memmove(cm->cm_pextents + cm->cm_nextents, BC_cache->c_mounts[cm_idx].cm_pextents + next_old_extent_idx[cm_idx], sizeof(*cm->cm_pextents) * (BC_cache->c_mounts[cm_idx].cm_nextents - next_old_extent_idx[cm_idx]));
				cm->cm_nextents += BC_cache->c_mounts[cm_idx].cm_nextents - next_old_extent_idx[cm_idx];
			}
		}
		
	}
		
	if (clipped_extents > 0) {
		debug("%d extents were clipped, %d of which were completely enveloped", clipped_extents, enveloped_extents);
	}
	
	if (ncache_extents == 0) {
		debug("No extents added, not updating cache");
		error = EALREADY;
		goto out;
	}

	BC_FREE_ARRAY_NOPOINTERS(playlist_entries, BC_PLC_CHUNK);
	BC_FREE_ARRAY_NOPOINTERS(playlist_mounts, nplaylist_mounts);
	
	if (ncache_extents < nplaylist_entries) {
		debug("%llu extents added out of %llu", ncache_extents, nplaylist_entries);
	}
	for (cm_idx = 0; cm_idx < ncache_mounts; cm_idx++) {
		debug("Mount %s now has %d extents", uuid_string(cache_mounts[cm_idx].cm_uuid), cache_mounts[cm_idx].cm_nextents);
	}
	
	
	/*
	 * In order to simplify allocation of the block and page maps, we round
	 * the size up to a multiple of CB_MAPFIELDBITS pages.  This means we
	 * may waste as many as 31 pages in the worst case.
	 */
	size = roundup(size, PAGE_SIZE * CB_MAPFIELDBITS);
	
	/*
	 * Verify that the cache is not larger than 50% of physical memory
	 * to avoid forcing pageout activity.
	 *
	 * max_mem and size are in bytes.  All calculations are done in page
	 * units to avoid overflow/sign issues.
	 *
	 * If the cache is too large, we discard it and go to record-only mode.
	 * This lets us survive the case where the playlist is corrupted or
	 * manually made too large, but the "real" bootstrap footprint
	 * would be OK.
	 */
	BC_cache->c_cachesize += size;
	if (BC_cache->c_cachesize > BC_MAX_SIZE) {
		message("cache size (%lu bytes) too large for physical memory (%#llx bytes), capping at %#llx bytes",
				(unsigned long)BC_cache->c_cachesize, max_mem, BC_MAX_SIZE);
		BC_cache->c_cachesize = BC_MAX_SIZE;
	}
		
	/*
	 * Allocate/grow the pagebuffer.
	 */
	if (BC_alloc_pagebuffer() != 0) {
		message("can't allocate %#llx bytes for cache buffer", BC_cache->c_cachesize);
		error = ENOMEM;
		goto out;
	}

	/*
	 * Fix up the extent cache offsets.
	 */
	p = 0;
	if (BC_cache->c_nextentlists > 0) {
		/* start at the offset of the end of the previous cache list */
		for (int64_t cel_idx = BC_cache->c_nextentlists - 1; cel_idx >= 0; cel_idx--) {
			if (BC_cache->c_nextents[cel_idx] > 0) {
				struct BC_cache_extent *last_ce = BC_cache->c_extentlists[cel_idx] + BC_cache->c_nextents[cel_idx] - 1;
				p = last_ce->ce_cacheoffset + roundup(last_ce->ce_length, PAGE_SIZE);
				break;
			}
		}
	}
	

	int nonshared_discards = 0;
	int shared_discards = 0;
	for (ce_idx = 0; ce_idx < ncache_extents; ce_idx++) {
		cache_extents[ce_idx].ce_cacheoffset = p;
		p += roundup(cache_extents[ce_idx].ce_length, PAGE_SIZE);
		
		/* Abort any extents that extend past our cache size */
		if (p >= BC_cache->c_cachesize || cache_extents[ce_idx].ce_length == 0) {
			// Extent hasn't been added to the pagebuffer, so don't BC_discard_extents since that will BC_page_free
			if (cache_extents[ce_idx].ce_flags & CE_SHARED) {
				shared_discards += cache_extents[ce_idx].ce_length;
			} else {
				nonshared_discards += cache_extents[ce_idx].ce_length;
			}
			BC_teardown_extent(cache_extents + ce_idx);
		}
	}
	BC_ADD_NON_SHARED_CACHE_STAT(cache_oversize, nonshared_discards);
	BC_ADD_SHARED_CACHE_STAT(cache_oversize, shared_discards);
	
	/* We've successfully integrated the new playlist with our cache, copy it into place */
	
	/* free up old cache's allocations that we're replacing */
	if (old_ncache_mounts > 0) {
		for (cm_idx = 0; cm_idx < old_ncache_mounts; cm_idx++) {
			/* If we have a new extent array for this mount, free the old one */
			if (BC_cache->c_mounts[cm_idx].cm_pextents != cache_mounts[cm_idx].cm_pextents) {
				BC_FREE_ARRAY_WITHPOINTERS(BC_cache->c_mounts[cm_idx].cm_pextents, BC_cache->c_mounts[cm_idx].cm_pextents_size);
				BC_cache->c_mounts[cm_idx].cm_pextents_size = 0;
			}
		}
		BC_FREE_ARRAY_WITHPOINTERS(BC_cache->c_mounts, BC_cache->c_nmounts);
	}
	BC_cache->c_mounts = cache_mounts;
	BC_cache->c_nmounts = ncache_mounts;
	
	if (BC_cache->c_nextentlists > 0) {
		BC_FREE_ARRAY_WITHPOINTERS(BC_cache->c_extentlists, BC_cache->c_nextentlists);
		BC_FREE_ARRAY_NOPOINTERS(BC_cache->c_nextents, BC_cache->c_nextentlists);
	}
	
	cache_nextents[BC_cache->c_nextentlists] = ncache_extents;
	cache_extents_list[BC_cache->c_nextentlists] = cache_extents;
	
	BC_cache->c_nextentlists++;
	
	BC_cache->c_nextents = cache_nextents;
	BC_cache->c_extentlists = cache_extents_list;
		
	BC_cache->c_stats.ss_total_mounts = ncache_mounts;
	BC_ADD_SHARED_CACHE_STAT(total_extents, nexents_shared);
	BC_ADD_NON_SHARED_CACHE_STAT(total_extents, nextents_nonshared);

	/* fill (or tear down) the extents for all the mounts we've already seen */
	int extent_error_shared_bytes = 0;
	int extent_error_nonshared_bytes = 0;
	int mount_error_shared_bytes = 0;
	int mount_error_nonshared_bytes = 0;
	for (ce_idx = 0; ce_idx < ncache_extents; ce_idx++) {
		ce = cache_extents + ce_idx;
		if (CM_SETUP != BC_cache->c_mounts[ce->ce_mount_idx].cm_state) {
			LOCK_EXTENT(ce);
			if (CM_READY == BC_cache->c_mounts[ce->ce_mount_idx].cm_state) {
				if (0 == BC_fill_in_extent(ce)) {
					UNLOCK_EXTENT(ce);
					continue;
				}
				if (ce->ce_flags & CE_SHARED) {
					extent_error_shared_bytes += ce->ce_length;
				} else {
					extent_error_nonshared_bytes += ce->ce_length;
				}
			} else {
				if (ce->ce_flags & CE_SHARED) {
					mount_error_shared_bytes += ce->ce_length;
				} else {
					mount_error_nonshared_bytes += ce->ce_length;
				}
			}
			/* either the mount is aborted or fill_in_extent failed */
			BC_teardown_extent(ce);
			UNLOCK_EXTENT(ce);
		}
	}
	BC_ADD_SHARED_CACHE_STAT(extenterror_unread, extent_error_shared_bytes);
	BC_ADD_NON_SHARED_CACHE_STAT(extenterror_unread, extent_error_nonshared_bytes);
	BC_ADD_SHARED_CACHE_STAT(mounterror_unread, mount_error_shared_bytes);
	BC_ADD_NON_SHARED_CACHE_STAT(mounterror_unread, mount_error_nonshared_bytes);
	
	/* Check to see if we have any more mounts to fill in */
	for (cm_idx = 0; cm_idx < BC_cache->c_nmounts; cm_idx++) {
		if (CM_SETUP == BC_cache->c_mounts[cm_idx].cm_state) {
			/* have at least one mount to fill in, iterate all of them */
			int go_again = 0;
			vfs_iterate(0, fill_in_bc_cache_mounts, &go_again);
			if (go_again) {
				debug("Missing root mount during last iteration, going again");
				vfs_iterate(0, fill_in_bc_cache_mounts, &go_again);
			}
			break;
		}
	}
		
	error = 0;

	/* all done */
out:
	BC_FREE_ARRAY_NOPOINTERS(playlist_entries, BC_PLC_CHUNK);
	BC_FREE_ARRAY_NOPOINTERS(playlist_mounts, nplaylist_mounts);
	BC_FREE_ARRAY_NOPOINTERS(pmidx_to_cmidx, nplaylist_mounts);
	BC_FREE_ARRAY_NOPOINTERS(next_old_extent_idx, old_ncache_mounts);
	if (error != 0) {
		if (error != EALREADY) {
			debug("cache setup failed, aborting");
		}

		/* If it was possible to fail after growing the page buffer, we'd need a way to shrink it back here */

		if (BC_cache->c_extentlists == NULL) {
			/* If this is our first cache, clear stats and our page buffer */
			BC_cache->c_stats.ss_sharedcache.ss_total_extents = 0;
			BC_cache->c_stats.ss_nonsharedcache.ss_total_extents = 0;
			BC_cache->c_stats.ss_total_mounts = 0;
			if (BC_cache->c_vp != NULL) {
				BC_free_pagebuffer();
			}
		}
		
		BC_cache->c_cachesize = old_cache_size;
		BC_cache->c_batch_count = old_batch_count;

		/* free up new cache's allocations that aren't part of the old cache */
		if (cache_extents) {
			for (ce_idx = 0; ce_idx < ncache_extents; ce_idx++) {
				BC_teardown_extent(cache_extents + ce_idx);
				lck_mtx_destroy(&cache_extents[ce_idx].ce_lock, BC_cache->c_lckgrp);
			}
			BC_FREE_ARRAY_WITHPOINTERS(cache_extents, nplaylist_entries);
		}
		BC_FREE_ARRAY_NOPOINTERS(cache_nextents, num_cache_extents_lists);
		BC_FREE_ARRAY_WITHPOINTERS(cache_extents_list, num_cache_extents_lists);

		if (cache_mounts) {
			for (cm_idx = 0; cm_idx < ncache_mounts; cm_idx++) {
				if (cm_idx >= BC_cache->c_nmounts) {
					/* newly allocated mounts, teardown completely */
					BC_teardown_mount(cache_mounts + cm_idx);
					lck_rw_destroy(&cache_mounts[cm_idx].cm_lock, BC_cache->c_lckgrp);
				} else if (BC_cache->c_mounts[cm_idx].cm_pextents != cache_mounts[cm_idx].cm_pextents) {
					/* old mounts with a new extent list */
					BC_FREE_ARRAY_WITHPOINTERS(cache_mounts[cm_idx].cm_pextents, cache_mounts[cm_idx].cm_pextents_size);
					cache_mounts[cm_idx].cm_pextents_size = 0;
				}
			}
			BC_FREE_ARRAY_WITHPOINTERS(cache_mounts, ncache_mounts);
		}
	} else {
		BC_ADD_SHARED_CACHE_STAT(cache_bytes, num_shared_bytes);
		BC_ADD_NON_SHARED_CACHE_STAT(cache_bytes, num_nonshared_bytes);
		BC_cache->c_stats.ss_cache_size = BC_cache->c_cachesize;
	}
	
#ifdef BC_DEBUG
	struct timeval end_time;
	microtime(&end_time);
	timersub(&end_time, &start_time, &end_time);
	debug("BC_copyin_playlist took %ldus", end_time.tv_sec * 1000000 + end_time.tv_usec);
#endif	
	
	return(error);
}

/*
 * Initialise the cache.  Fetch the playlist from userspace and
 * find and attach to our block device.
 */
static int
BC_init_cache(size_t mounts_size, user_addr_t mounts, size_t entries_size, user_addr_t entries, int record_history)
{
	int error = 0, copyin_error, mount_idx;
	struct timeval current_time;
	
	/* Everything should be set, or nothing should be set */
	if ((mounts_size == 0 || mounts == 0 || entries_size == 0 || entries == 0) &&
		(mounts_size != 0 || mounts != 0 || entries_size != 0 || entries != 0)) {
		debug("Invalid cache parameters: %ld, %#llx, %ld, %#llx", mounts_size, mounts, entries_size, entries);
		return(EINVAL);
	}
	
	
	if (! (BC_cache->c_flags & BC_FLAG_SETUP)) {
		debug("Cache not yet setup");
		return(ENXIO);
	}
		
	microtime(&current_time);
	u_int64_t current_machabs_time = mach_absolute_time();

	/*
	 * Start recording history.
	 *
	 * Only one history recording may be active at a time
	 */
	if (record_history) {
		LOCK_HANDLERS();
		if (BC_set_flag(BC_FLAG_HISTORYACTIVE)) {
			debug("history recording already started, only one playlist will be returned");
			BC_ADD_UNKNOWN_STAT(history_num_recordings, 1);
			UNLOCK_HANDLERS();
			error = EALREADY;
		} else {
			debug("starting history recording");
			KERNEL_DEBUG_CONSTANT(FSDBG_CODE(DBG_BOOTCACHE, DBG_BC_RECORDING_START), 0, 0, 0, 0, 0);
			tag_batch_num = 0;

			/* start the timeout handler */
			timeout(BC_timeout_history, NULL, BC_history_timeout);

			/* always take stats for the first two recordings (boot + login), even if we don't have a playlist */
			if (BC_cache->c_stats.ss_history_num_recordings < 2)
				BC_cache->c_take_detailed_stats = 1;

			BC_cache->c_history_starttime = current_time;
			BC_cache->c_history_size = 0;
			BC_cache->c_history_num_clusters = 0;
			BC_ADD_UNKNOWN_STAT(history_num_recordings, 1);
			BC_clear_flag(BC_FLAG_HTRUNCATED);
			
			/*
			 * Take over the strategy routine for our device; we are now
			 * recording read operations.
			 */
			BC_check_handlers();
			UNLOCK_HANDLERS();
			
			BC_update_mounts();
			
		}
	}
	
	/*
	 * If we have playlist data, fetch it.
	 */
	if (entries_size > 0) {
		LOCK_CACHE_W();

		if (BC_cache->c_flags & BC_FLAG_SHUTDOWN) {
			debug("cache already shutdown");
			error = ENXIO;
			UNLOCK_CACHE_W();
			goto out;
		}
		
		/* we're playing back a cache (or at least trying to),
		 * record detailed statistics until we stop recording history
		 */
		BC_cache->c_take_detailed_stats = 1;		
		
		copyin_error = BC_copyin_playlist(mounts_size, mounts, entries_size, entries);
		if (copyin_error != 0 && copyin_error != EALREADY) {
			message("can't load playlist: %d", copyin_error);
			UNLOCK_CACHE_W();
			error = copyin_error;
			goto out;
		}
				
		/* Always throttle cut-through IOs.
		 * I tried throttling only after login had started, but
		 * then users who log in quickly see a very long delay
		 * after entering their password before apps come up.
		 * I'd prefer delay the time until the login screen appears
		 * and provide a fast-as-possible login.
		 * rdar://8592401
		 */
		BC_cache->c_readahead_throttles_cutthroughs = 1;		
		
		LOCK_HANDLERS();
		if (BC_set_flag(BC_FLAG_CACHEACTIVE)) {
			/* cache is already running, we're adding to it */
			UNLOCK_HANDLERS();
		} else {
			/* first cache we've created this boot */
			
			BC_cache->c_cache_starttime = current_time;

			/*
			 * Take over the strategy routine for our device; we are now
			 * filling read operations from the cache.
			 *
			 * This must be done before we kick off reader threads to ensure
			 * that we see catch any writes that occur on blocks in our cache.
			 * This ensures that a write won't come in unnoticed on a block we've
			 * already read in so when it's subsequently requested, the cache gives
			 * stale data.
			 */
			BC_check_handlers();
			UNLOCK_HANDLERS();
			
			bootcache_contains_block = BC_cache_contains_block;
		}
		
		LOCK_CACHE_W_TO_R();
		if (copyin_error != EALREADY) {
			/*
			 * Start the reader threads.
			 */
			for (mount_idx = 0; mount_idx < BC_cache->c_nmounts; mount_idx++) {
				if (BC_cache->c_mounts[mount_idx].cm_state == CM_READY) {
					LOCK_MOUNT_R(BC_cache->c_mounts + mount_idx);
					BC_mount_available(BC_cache->c_mounts + mount_idx);
					UNLOCK_MOUNT_R(BC_cache->c_mounts + mount_idx);
				}
			}
		}
		
		UNLOCK_CACHE_R();
		
		/* cache is now active */
		
	}
	
	if (BC_cache->c_stats.ss_start_timestamp == 0) {
		BC_cache->c_stats.ss_bc_start_timestamp = current_machabs_time;
		BC_cache->c_stats.ss_start_timestamp = mach_absolute_time();
	}
	
out:
	return(error);
}

/*
 * Timeout on collection.
 *
 * We haven't heard from the userland component for too long;
 * stop recording history, but keep the current history
 * around in case they show up.
 */
static void
BC_timeout_history(void *junk)
{
	/* stop recording history */
	debug("history recording timed out");
	BC_terminate_history("timed out");
}

/*
 * Check for new mounts. Called with no locks held
 */
static int check_for_new_mount_itr(mount_t mount, void* arg) {
	struct vfs_attr attr;
	int error, mount_idx;
	int* go_again = (int*) arg;

	struct BC_history_mount_device *hmd = NULL;
	
	LOCK_HISTORY_R();
	if (! (BC_cache->c_flags & BC_FLAG_HISTORYACTIVE)) {
		/* don't do anything if we've stopped recording history */
		UNLOCK_HISTORY_R();
		return VFS_RETURNED_DONE;
	}	
	
	char name[MFSNAMELEN];
	vfs_name(mount, name);
	
	dev_t dev = nulldev();
	vnode_t devvp = NULLVP;

	// lookup dev and devvp from mount
	error = BC_get_dev(mount, &dev, &devvp);
	if (error != 0) {
		UNLOCK_HISTORY_R();
		return VFS_RETURNED;
	}
	
	uint32_t fs_flags = 0;
	dev_t container_dev = nulldev();
	uuid_t container_uuid = {0};
	bool container_has_encrypted_volumes = true;
	bool container_has_rolling_volumes = true;
	bc_get_volume_info(dev, &fs_flags, &container_dev, container_uuid, &container_has_encrypted_volumes, &container_has_rolling_volumes);
	
	if (fs_flags & BC_FS_APFS_ENCRYPTION_ROLLING) {
		// <rdar://problem/32395140> Bootcache needs to be disabled for APFS volumes rolling encryption (and their container)
		message("historic apfs mount %s has encryption rolling in-progress, skipping.", name);
		UNLOCK_HISTORY_R();
		vnode_put(devvp);
		return VFS_RETURNED;
	}
	
	if ((fs_flags & BC_FS_APFS) && uuid_is_null(container_uuid)) {
		// With <rdar://problem/43851924> (When discarding data, discard data for that extent from all mounts in the same apfs container), we must have a container UUID for all apfs volumes and containers
		message("historic apfs mount %s has no container UUID, skipping.", name);
		UNLOCK_HISTORY_R();
		vnode_put(devvp);
		return VFS_RETURNED;
	}
	
	/*
	 * A new mount appeared, but unfortunately we don't know which one.
	 * So, for every mount we have to check through our list of history_mounts
	 * to see if we already have info for it.
	 */	
	hmd = BC_get_history_mount_device(dev, NULL);
	if (hmd == NULL || (! uuid_is_null(hmd->hmd_mount.hm_uuid))) {
		/* Unable to allocate new space for a mount, or
		 * we already have info about this mount
		 */
		UNLOCK_HISTORY_R();
		vnode_put(devvp);
		return VFS_RETURNED;
	}
	
	/*
	 * We don't yet have info about this mount. It's new!
	 */
	
	hmd->hmd_blocksize = vfs_devblocksize(mount);
	
	VFSATTR_INIT(&attr);
	VFSATTR_WANTED(&attr, f_uuid);
	error = vfs_getattr(mount, &attr, vfs_context_current());
	
	if ((0 != error) || (! VFSATTR_IS_SUPPORTED(&attr, f_uuid))) {
#ifdef BC_DEBUG
		if (strncmp("devfs", name, sizeof("devfs")) != 0) {
			debug("Found mount %s without a UUID: error %d", name, error);
		}
#endif
		UNLOCK_HISTORY_R();
		vnode_put(devvp);
		return VFS_RETURNED;
	}

	u_int64_t throttle_mask = vfs_throttle_mask(mount);
	hmd->hmd_disk_id = throttle_mask;

	if (VNOP_IOCTL(devvp,              /* vnode */
				   DKIOCISSOLIDSTATE,    /* cmd */
				   (caddr_t)&hmd->hmd_is_ssd, /* data */
				   0,
				   vfs_context_current()))           /* context */
	{
		debug("can't determine if physical disk for history mount 0x%llx is an ssd", hmd->hmd_disk_id);
		hmd->hmd_is_ssd = 0;
	}
	
	if (!cache_nonroot_disks) {
		// Determine root disk
		if (BC_cache->c_root_disk_id == 0) {
			// Use vfs_devvp to compare against rootvp (rather than BC_get_dev)
			vnode_t rootdevvp = vfs_devvp(mount);
			if (rootdevvp != NULLVP) {
				// Unused, so make sure we dont forget to put it
				vnode_put(rootdevvp);
			}
			if (rootdevvp == rootvp) {
				BC_cache->c_root_disk_id = hmd->hmd_disk_id;
				if (BC_cache->c_root_disk_id == 0) {
					message("Root disk is 0");
				} else {
					debug("Root disk (via history) is 0x%llx", BC_cache->c_root_disk_id);
				}
			}
		}
	}

	
	/* Found info for our mount */
	hmd->hmd_mount.hm_nentries = 0;
	hmd->hmd_mount.hm_fs_flags = fs_flags;
	uuid_copy(hmd->hmd_mount.hm_uuid, attr.f_uuid);
	if (hmd->hmd_mount.hm_fs_flags & BC_FS_APFS) {
		// Make sure this mount is grouped with everything in this container when sorting the playlist later
		uuid_copy(hmd->hmd_mount.hm_group_uuid, container_uuid);
	}

	debug("Found new historic mount after %d IOs: %s, dev 0x%x, disk 0x%llx, blk %d%s, flags 0x%x %son root disk",
		  hmd->hmd_mount.hm_nentries,
		  uuid_string(hmd->hmd_mount.hm_uuid),
		  hmd->hmd_dev,
		  hmd->hmd_disk_id,
		  hmd->hmd_blocksize,
		  hmd->hmd_is_ssd ? ", ssd" : "",
		  hmd->hmd_mount.hm_fs_flags,
		  (BC_cache->c_root_disk_id == hmd->hmd_disk_id) ? "" :"not ");

	
	
	
	//---------------------------
	// Check for apfs container
	//---------------------------
	// <rdar://problem/31565082> BootCache should track I/Os on the APFS volume and play back
	//
	// We track apfs volumes, but there is still some I/O directly to the container, so we need to track the container as well as the volume

	struct BC_history_mount_device *container_hmd = NULL;
	
	if (! (hmd->hmd_mount.hm_fs_flags & BC_FS_APFS)) {
		UNLOCK_HISTORY_R();
		goto check_cache_mounts;
	}
	
	if (container_dev == nulldev() || container_dev == NODEV || uuid_is_null(container_uuid)) {
		message("mount %s has no container", uuid_string(attr.f_uuid));
		UNLOCK_HISTORY_R();
		goto check_cache_mounts;
	}
	debug("mount %s has container %s dev 0x%x (%sfusion)", uuid_string(hmd->hmd_mount.hm_uuid), uuid_string(container_uuid), container_dev, (fs_flags & BC_FS_APFS_FUSION) ? "" : "not ");
	
	// Make sure this mount is grouped with everything in this container when sorting the playlist later
	uuid_copy(hmd->hmd_mount.hm_group_uuid, container_uuid);

	if (container_has_rolling_volumes) {
		// <rdar://problem/32395140> Bootcache needs to be disabled for APFS volumes rolling encryption (and their container)
		message("Not recording apfs container %s that contains rolling volumes", uuid_string(container_uuid));
		UNLOCK_HISTORY_R();
		goto check_cache_mounts;
	}
	
	container_hmd = BC_get_history_mount_device(container_dev, NULL);
	if (container_hmd == NULL || (! uuid_is_null(container_hmd->hmd_mount.hm_uuid))) {
		/* Unable to allocate new space for a mount, or
		 * we already have info about this mount
		 */
		UNLOCK_HISTORY_R();
		goto check_cache_mounts;
	}
	
	// Container has the same info as all of its containing volumes, so just use the data from the volume we just populated above
	container_hmd->hmd_blocksize = hmd->hmd_blocksize;
	container_hmd->hmd_disk_id   = hmd->hmd_disk_id;
	container_hmd->hmd_is_ssd    = hmd->hmd_is_ssd;
	
	container_hmd->hmd_mount.hm_fs_flags = BC_FS_APFS | BC_FS_APFS_CONTAINER | (fs_flags & BC_FS_APFS_FUSION);
	uuid_copy(container_hmd->hmd_mount.hm_uuid, container_uuid);
	uuid_copy(container_hmd->hmd_mount.hm_group_uuid, container_uuid);

	debug("Found new historic container mount after %d IOs: %s, dev 0x%x, disk 0x%llx, blk %d%s, flags 0x%x %son root disk",
		  container_hmd->hmd_mount.hm_nentries,
		  uuid_string(container_hmd->hmd_mount.hm_uuid),
		  container_hmd->hmd_dev,
		  container_hmd->hmd_disk_id,
		  container_hmd->hmd_blocksize,
		  container_hmd->hmd_is_ssd ? ", ssd" : "",
		  container_hmd->hmd_mount.hm_fs_flags,
		  (BC_cache->c_root_disk_id == container_hmd->hmd_disk_id) ? "" :"not ");

	UNLOCK_HISTORY_R();

check_cache_mounts:

	if (BC_cache->c_flags & BC_FLAG_CACHEACTIVE) {
		LOCK_CACHE_R();
		/* Check if we have a playlist mount that matches this mount and set it up */
		for (mount_idx = 0; mount_idx < BC_cache->c_nmounts; mount_idx++) {
			struct BC_cache_mount *cm = BC_cache->c_mounts + mount_idx;
			if (CM_SETUP == cm->cm_state) {
				
				
				// Volume
				if (0 == uuid_compare(cm->cm_uuid, attr.f_uuid)) {
					/* Found a matching unfilled mount */
					
					LOCK_MOUNT_W(cm);
					
					// All APFS volumes must have a non-null container UUID
					// <rdar://problem/43851924> When discarding data, discard data for that extent from all mounts in the same apfs container
					if (fs_flags & BC_FS_APFS) {
						if (uuid_is_null(container_uuid) || 0 != uuid_compare(cm->cm_group_uuid, container_uuid)) {
							
							// We don't expect the container UUID to ever change, so error out if it happens
							message("Not caching apfs volume %s with wrong container %s (really is %s)", uuid_string(cm->cm_uuid), uuid_string(cm->cm_group_uuid), uuid_string(container_uuid));
							
							struct discards discards = BC_teardown_mount_and_extents(cm);
							BC_ADD_SHARED_CACHE_STAT(unsupported_unread, discards.shared_discards);
							BC_ADD_NON_SHARED_CACHE_STAT(unsupported_unread, discards.nonshared_discards);
							break;
						}
					}
					

					if ((error = BC_fill_in_cache_mount(cm, mount, dev, devvp, fs_flags)) == 0) {
						LOCK_MOUNT_W_TO_R(cm);
						/* Kick off another reader thread if this is a new physical disk */
						BC_mount_available(cm);
						UNLOCK_MOUNT_R(cm);
					} else {
						if (error == EAGAIN) {
							*go_again = 1;
						}
						UNLOCK_MOUNT_W(cm);
					}
					
					break;
					
				// Container
				} else if (!uuid_is_null(container_uuid) && 0 == uuid_compare(cm->cm_uuid, container_uuid)) {
					/* Found a matching unfilled mount for the container */
					
					if (0 != uuid_compare(cm->cm_uuid, cm->cm_group_uuid)) {
						message("Not caching apfs container %s with mismatched group %s", uuid_string(cm->cm_uuid), uuid_string(cm->cm_group_uuid));
						struct discards discards = BC_teardown_mount_and_extents(cm);
						BC_ADD_SHARED_CACHE_STAT(unsupported_unread, discards.shared_discards);
						BC_ADD_NON_SHARED_CACHE_STAT(unsupported_unread, discards.nonshared_discards);
						break;
					}

					if (container_has_rolling_volumes) {
						// <rdar://problem/32395140> Bootcache needs to be disabled for APFS volumes rolling encryption (and their container)
						message("Not caching apfs container %s that contains rolling volumes", uuid_string(container_uuid));
						struct discards discards = BC_teardown_mount_and_extents(cm);
						BC_ADD_SHARED_CACHE_STAT(unsupported_unread, discards.shared_discards);
						BC_ADD_NON_SHARED_CACHE_STAT(unsupported_unread, discards.nonshared_discards);
						break;
					}
					
					// Get vnode_t for the container
					char apfsdevpath[64] = "";
					lookup_dev_name(container_dev, apfsdevpath, sizeof(apfsdevpath));
					if (apfsdevpath[0] == '\0') {
						message("unable to get apfs dev path for container %s", uuid_string(container_uuid));
						struct discards discards = BC_teardown_mount_and_extents(cm);
						BC_ADD_SHARED_CACHE_STAT(mounterror_unread, discards.shared_discards);
						BC_ADD_NON_SHARED_CACHE_STAT(mounterror_unread, discards.nonshared_discards);
						break;
					}
					debug("apfs container %s dev path is %s", uuid_string(container_uuid), apfsdevpath);
					
					vnode_t container_devvp = NULLVP;
					errno_t err = vnode_lookup(apfsdevpath, 0, &container_devvp, vfs_context_current());
					if (err != 0 || container_devvp == NULLVP) {
						message("unable to get open apfs container %s dev path %s: %d", uuid_string(container_uuid), apfsdevpath, err);
						struct discards discards = BC_teardown_mount_and_extents(cm);
						BC_ADD_SHARED_CACHE_STAT(mounterror_unread, discards.shared_discards);
						BC_ADD_NON_SHARED_CACHE_STAT(mounterror_unread, discards.nonshared_discards);
						break;
					}
					
					uint32_t flags = BC_FS_APFS | BC_FS_APFS_CONTAINER | (fs_flags & BC_FS_APFS_FUSION);
					
					LOCK_MOUNT_W(cm);
					 // I/O direct to containers is never encrypted
					if (BC_fill_in_cache_mount_ex(cm, container_dev, container_devvp, throttle_mask, flags) == 0) {
						LOCK_MOUNT_W_TO_R(cm);
						/* Kick off another reader thread if this is a new physical disk */
						BC_mount_available(cm);
						UNLOCK_MOUNT_R(cm);
					} else {
						UNLOCK_MOUNT_W(cm);
					}
					
					vnode_put(container_devvp);
					
					break;
				}
			}
			
		}
		UNLOCK_CACHE_R();
	}
	
	vnode_put(devvp);
	return VFS_RETURNED;
}

/*
 * Check for new or disappearing mounts 
 */
static int BC_update_mounts(void) {
	
	/* don't do anything if we've stopped recording history */
	if (! (BC_cache->c_flags & BC_FLAG_HISTORYACTIVE)) {
		return ENXIO;
	}
	
	int go_again = 0;
	vfs_iterate(0, check_for_new_mount_itr, &go_again);
	if (go_again) {
		debug("Missing root mount during last iteration, going again");
		vfs_iterate(0, check_for_new_mount_itr, &go_again);
	}
	
	return 0;
}

/*
 * Find the mount that corresponds to this IO,
 * or the first empty slot.
 *
 * Called with the history read lock
 */
static struct BC_history_mount_device * BC_get_history_mount_device(dev_t dev, int* pindex) {
	struct BC_history_mount_device *tmphmd = NULL, **phmd, *ret = NULL;
	int mount_idx = 0;
	
	for (phmd = &BC_cache->c_history_mounts;
		 (*phmd) == NULL || (*phmd)->hmd_dev != dev;
		 phmd = &(*phmd)->hmd_next) {
		
		if (*phmd == NULL) {
			if (tmphmd == NULL) { /* we may have an allocation from previous iterations */
				BC_MALLOC_SINGLE(tmphmd);
				if (tmphmd == NULL) {
					message("could not allocate %lu bytes for history mount device",
							sizeof(struct BC_history_mount_device));
					BC_set_flag(BC_FLAG_HTRUNCATED);
					goto out;
				}
				
				/* claim the new entry */
				tmphmd->hmd_next = NULL;
				tmphmd->hmd_mount.hm_nentries = 0;
				tmphmd->hmd_mount.hm_fs_flags = 0x0;
				uuid_clear(tmphmd->hmd_mount.hm_uuid);
				uuid_clear(tmphmd->hmd_mount.hm_group_uuid);
				tmphmd->hmd_disk_id = 0;
				tmphmd->hmd_blocksize = 0;
				tmphmd->hmd_dev = dev;
			}
			
			if (OSCompareAndSwapPtr(NULL, tmphmd, phmd)) {
				tmphmd = NULL;
				if (BC_cache->c_take_detailed_stats) {
					BC_ADD_UNKNOWN_STAT(history_mounts, 1);
				}
				break;
			}
			
			/* someone else added a mount before we could, check if its the one we want */
			if ((*phmd)->hmd_dev == dev) break;
			
		}
		mount_idx++;
	}
	
	if (pindex)
		*pindex = mount_idx;
	ret = *phmd;
out:
	if (tmphmd)
		BC_FREE_SINGLE(tmphmd);
	
	return ret;
}

/*
 * Record an incoming IO in the history list.
 *
 * Returns non-0 if this IO was rejected (e.g. due to being non-root or pure SSD)
 *
 * Note that this function is not reentrant.
 */
static int
BC_add_history(vnode_t vp, daddr64_t blkno, u_int64_t length, int pid, int hit, int write, int tag, int shared, u_int64_t crypto_offset,  dev_t dev)
{	
	u_int64_t offset;
	struct BC_history_entry_cluster *hec = NULL;
	struct BC_history_mount_device *hmd = NULL;
	struct BC_history_entry *he;
	int mount_idx, entry_idx;
	int is_rejected = 0;
	
	/* don't do anything if we've stopped recording history */
	if (! (BC_cache->c_flags & BC_FLAG_HISTORYACTIVE))
		return 0;
	
	LOCK_HISTORY_R();
	
	/* check again, with lock */
	if (! (BC_cache->c_flags & BC_FLAG_HISTORYACTIVE))
		goto out;
	
	/* count IOs during boot (first recording) */
	if (BC_cache->c_take_detailed_stats) {
		if (! tag) {
			if (! write) {
				BC_ADD_STAT(shared, history_reads, 1);
				BC_ADD_STAT(shared, history_reads_bytes, length);
			} else {
				BC_ADD_STAT(shared, history_writes, 1);
				BC_ADD_STAT(shared, history_writes_bytes, length);
			}
		}
	}
	
	/* don't do anything if the history list has been truncated */
	if (!tag && (BC_cache->c_flags & BC_FLAG_HTRUNCATED)) {
		if (BC_cache->c_take_detailed_stats && !write && !tag) {
			BC_ADD_STAT(shared, history_reads_truncated, 1);
			BC_ADD_STAT(shared, history_reads_truncated_bytes, length);
		}
		goto out;
	}
	
	/*
	 * In order to avoid recording a playlist larger than we will
	 * allow to be played back, if the total byte count for recorded
	 * reads reaches 50% of the physical memory in the system, we
	 * start ignoring reads.
	 * This is preferable to dropping entries from the end of the
	 * playlist as we copy it in, since it will result in a clean
	 * cessation of caching at a single point, rather than
	 * introducing sporadic cache misses throughout the cache's run.
	 */
	if (!tag && (BC_cache->c_history_size + length) > BC_MAX_SIZE) {
		debug("History recording too large, capping at %#llxMB", BC_MAX_SIZE / (1024 * 1024));
		BC_set_flag(BC_FLAG_HTRUNCATED);
		if (BC_cache->c_take_detailed_stats && !write && !tag) {
			BC_ADD_STAT(shared, history_reads_truncated, 1);
			BC_ADD_STAT(shared, history_reads_truncated_bytes, length);
		}
		goto out;
	}
	
	mount_idx = 0;
	offset = 0;
	if (! tag) {
		
		hmd = BC_get_history_mount_device(dev, &mount_idx);
		if (hmd == NULL) {
			if (BC_cache->c_take_detailed_stats && !write && !tag) {
				BC_ADD_STAT(shared, history_reads_nomount, 1);
				BC_ADD_STAT(shared, history_reads_nomount_bytes, length);
			}
			goto out;
		}
		
		if (! (hmd->hmd_mount.hm_fs_flags & BC_FS_APFS_ENCRYPTED)) {
			if (crypto_offset != 0) {
				// Ignore crypto offsets for non-encrypted volumes. Don't assert because the value may not be zero'ed out properly, but it is unused
				crypto_offset = 0;
			}
		}

		if (uuid_is_null(hmd->hmd_mount.hm_uuid)) {
			/* Couldn't identify the mount
			 *
			 * Before the mount has appeared we don't want to
			 * include any of this device's IOs in the history
			 * since these IOs will be issued next boot before
			 * we're able to start this mount's part of the
			 * playlist.
			 */
			
			/* Keep track of the number of IOs we've seen until the mount appears */
			OSIncrementAtomic(&hmd->hmd_mount.hm_nentries);
			
			if (BC_cache->c_take_detailed_stats && !write && !tag) {
				BC_ADD_STAT(shared, history_reads_unknown, 1);
				BC_ADD_STAT(shared, history_reads_unknown_bytes, length);
			}
			goto out;
		}
		
		if (hmd->hmd_blocksize == 0) {
			// Couldn't get blocksize for the mount -> can't get accurate byte range
			
			if (BC_cache->c_take_detailed_stats && !write && !tag) {
				BC_ADD_STAT(shared, history_reads_no_blocksize, 1);
				BC_ADD_STAT(shared, history_reads_no_blocksize_bytes, length);
			}
			goto out;
		}
		
		offset = HB_BLOCK_TO_BYTE(hmd, blkno);
		
		/* Only track IOs on the root disk.
		 *
		 * We don't expect IOs on other physical disks
		 * to be part of the critical path to boot/login
		 * and we certainly don't expect the other disks
		 * to be contested enough to make a cache worthwhile
		 */
		
		if (!cache_nonroot_disks) {
			if (hmd->hmd_disk_id != BC_cache->c_root_disk_id) {
				is_rejected = 1;
				if (BC_cache->c_take_detailed_stats && !write && !tag) {
					BC_ADD_STAT(shared, history_reads_nonroot, 1);
					BC_ADD_STAT(shared, history_reads_nonroot_bytes, length);
				}
				goto out;
			}
		}
		
		if (!cache_ssds) {
			if (hmd->hmd_is_ssd &&
				!(hmd->hmd_mount.hm_fs_flags & BC_FS_APFS_FUSION) &&
				!(hmd->hmd_mount.hm_fs_flags & BC_FS_CS_FUSION)) {
				
				if (BC_cache->c_take_detailed_stats && !write && !tag) {
					BC_ADD_STAT(shared, history_reads_ssd, 1);
					BC_ADD_STAT(shared, history_reads_ssd_bytes, length);
				}
				
				is_rejected = 1;
				goto out;
			}
		}
		
	}
	
	
	/*
	 * Get the current entry cluster.
	 */
	while ((hec = BC_cache->c_history) == NULL ||
		   (entry_idx = OSAddAtomic(1, &hec->hec_nentries)) >= BC_HISTORY_DATA_NUM_ALLOCED_ENTRIES) {
		
		// We either have no history yet (hec == NULL), or the current history cluster is full
		
		if (hec) {
			// Decrement because we incremented above and ended up outside the array length
			OSAddAtomic(-1, &hec->hec_nentries);
		}
		
		/* limit the number of clusters we will allocate */
		if (BC_cache->c_history_num_clusters + 1 > BC_HISTORY_MAXCLUSTERS) {
			message("too many history clusters (%d, limit %ld)",
					BC_cache->c_history_num_clusters,
					(long)BC_HISTORY_MAXCLUSTERS);
			BC_set_flag(BC_FLAG_HTRUNCATED);
			if (BC_cache->c_take_detailed_stats && !write && !tag) {
				BC_ADD_STAT(shared, history_reads_truncated, 1);
				BC_ADD_STAT(shared, history_reads_truncated_bytes, length);
			}
			goto out;
		}
		
		LOCK_HISTORY_CLUSTERS();
		if (hec != BC_cache->c_history) {
			// Someone already created a new cluster
			UNLOCK_HISTORY_CLUSTERS();
			continue;
		}
		
		/* need a new cluster */
		
		BC_MALLOC_SINGLE(hec);
		if (hec) {
			BC_MALLOC_ARRAY_NOPOINTERS(hec->hec_data, BC_HISTORY_DATA_NUM_ALLOCED_ENTRIES);
		}
		
		if (hec == NULL || hec->hec_data == NULL) {
			if (hec) {
				message("could not allocate %lu bytes for history cluster", BC_HISTORY_DATA_NUM_ALLOCED_ENTRIES * sizeof(*hec->hec_data));
				BC_FREE_SINGLE(hec);
				hec = NULL;
			} else {
				message("could not allocate struct for history cluster");
			}
			
			BC_set_flag(BC_FLAG_HTRUNCATED);
			if (BC_cache->c_take_detailed_stats && !write && !tag) {
				BC_ADD_STAT(shared, history_reads_truncated, 1);
				BC_ADD_STAT(shared, history_reads_truncated_bytes, length);
			}
			UNLOCK_HISTORY_CLUSTERS();
			goto out;
		}
		
		/* claim the first entry of the new cluster*/
		hec->hec_nentries = 1;
		hec->hec_link = BC_cache->c_history;
		
		BC_cache->c_history = hec;
		entry_idx = 0;
		BC_cache->c_history_num_clusters++;
		UNLOCK_HISTORY_CLUSTERS();
		break;
	}
		
	/*
	 * We've claimed entry entry_idx of cluster hec, fill it in.
	 */
	he = &(hec->hec_data[entry_idx]);
	assert(he >= &hec->hec_data[0]);
	assert(he < &hec->hec_data[BC_HISTORY_DATA_NUM_ALLOCED_ENTRIES]);
	if (hmd && (hmd->hmd_mount.hm_fs_flags & BC_FS_APFS) && vp && VREG == vnode_vtype(vp)) {
		he->he_inode = apfs_get_inode(vp, blkno, length, vfs_context_current());
	} else {
		he->he_inode = 0;
	}
	he->he_offset    = offset;
	he->he_length    = length;
	he->he_pid       = pid;
	he->he_mount_idx = mount_idx;
	he->he_crypto_offset     = crypto_offset;
	he->he_flags     = 0x0;
	if (hit)    he->he_flags |= BC_HE_HIT;
	if (write)  he->he_flags |= BC_HE_WRITE;
	if (tag)    he->he_flags |= BC_HE_TAG;
	if (shared) he->he_flags |= BC_HE_SHARED;
	if (hmd)
		OSIncrementAtomic(&hmd->hmd_mount.hm_nentries);
	
	if (!write && !tag) {
		OSAddAtomic64(length, &BC_cache->c_history_size);
		if (BC_cache->c_take_detailed_stats) {
			BC_ADD_STAT(shared, history_entries, 1);
			BC_ADD_STAT(shared, history_entries_bytes, length);
			if (hmd) {
				if (hmd->hmd_mount.hm_fs_flags & BC_FS_APFS_FUSION) {
					if (!(he->he_offset & FUSION_TIER2_DEVICE_BYTE_ADDR)) {
						BC_ADD_STAT(shared, fusion_history_already_optimized_reads, 1);
						BC_ADD_STAT(shared, fusion_history_already_optimized_bytes, length);
					} else {
						BC_ADD_STAT(shared, fusion_history_not_already_optimized_reads, 1);
						BC_ADD_STAT(shared, fusion_history_not_already_optimized_bytes, length);
					}
				} else if (hmd->hmd_mount.hm_fs_flags & BC_FS_APFS) {
					BC_ADD_STAT(shared, hdd_history_reads, 1);
					BC_ADD_STAT(shared, hdd_history_bytes, length);
				}
			}
		}
	}
out:
	UNLOCK_HISTORY_R();
	return is_rejected;
}

/*
 * Return the size of the history buffer.
 */
static int 
BC_size_history_mounts(void)
{
	struct BC_history_mount_device *hmd;
	int nmounts;
	
	/* walk the list of mount clusters and count the number of mounts */
	nmounts = 0;
	for (hmd = BC_cache->c_history_mounts; hmd != NULL; hmd = hmd->hmd_next)
		nmounts ++;
	
	return(nmounts * sizeof(struct BC_history_mount));
}

/*
 * Return the size of the history buffer.
 */
static int 
BC_size_history_entries(void)
{
	struct BC_history_entry_cluster *hec;
	int nentries, cluster_entries;
	
	/* walk the list of clusters and count the number of entries */
	nentries = 0;
	for (hec = BC_cache->c_history; hec != NULL; hec = hec->hec_link) {
		cluster_entries = hec->hec_nentries;
		// Cap to BC_HISTORY_DATA_NUM_ALLOCED_ENTRIES because BC_add_history may be in the middle of trying to add more to this cluster, so hec_nentries may be artificially high
		if (cluster_entries > BC_HISTORY_DATA_NUM_ALLOCED_ENTRIES) {
			cluster_entries = BC_HISTORY_DATA_NUM_ALLOCED_ENTRIES;
		}
		
		nentries += cluster_entries;
	}
	
	return(nentries * sizeof(struct BC_history_entry));
}

/*
 * Copy out either the history buffer size, or the entire history buffer.
 *
 * Note that the cache must be shut down before calling this function in
 * order to avoid a race around the entry count/buffer size.
 */
static int
BC_copyout_history_mounts(user_addr_t uptr)
{
	struct BC_history_mount_device *hmd;
	int error;
	
	assert(uptr != 0);
	
	/*
	 * Copy out the mounts
	 */
	for (hmd = BC_cache->c_history_mounts;
		 hmd != NULL;
		 hmd = hmd->hmd_next) {
		
		/* copy the cluster out */
		if ((error = copyout(&(hmd->hmd_mount), uptr,
							 sizeof(struct BC_history_mount))) != 0)
			return(error);
		uptr += sizeof(struct BC_history_mount);
		
	}
	return(0);
}

/*
 * Copy out either the history buffer size, or the entire history buffer.
 *
 * Note that the cache must be shut down before calling this function in
 * order to avoid a race around the entry count/buffer size.
 */
static int
BC_copyout_history_entries(user_addr_t uptr)
{
	struct BC_history_entry_cluster *hec, *ohec;
	int error, cluster_entries;
	
	assert(uptr != 0);
	assert(BC_cache->c_history);
	
	/*
	 * Copying the history entries out is a little messy due to the fact that the
	 * clusters are on the list backwards, but we want the entries in order.
	 */
	ohec = NULL;
	if (BC_cache->c_history) {
		for (;;) {
			/* scan the list until we find the last one we haven't copied */
			hec = BC_cache->c_history;
			while (hec->hec_link != ohec)
				hec = hec->hec_link;
			ohec = hec;
			
			cluster_entries = hec->hec_nentries;
			// Cap to BC_HISTORY_DATA_NUM_ALLOCED_ENTRIES because BC_add_history may be in the middle of trying to add more to this cluster, so hec_nentries may be artificially high
			if (cluster_entries > BC_HISTORY_DATA_NUM_ALLOCED_ENTRIES) {
				cluster_entries = BC_HISTORY_DATA_NUM_ALLOCED_ENTRIES;
			}
			
			/* copy the cluster out */
			if ((error = copyout(hec->hec_data, uptr,
								 cluster_entries * sizeof(struct BC_history_entry))) != 0)
				return(error);
			uptr += cluster_entries * sizeof(struct BC_history_entry);
			
			/* if this was the last cluster, all done */
			if (hec == BC_cache->c_history)
				break;
		}
	}
	return(0);
}

/*
 * Discard the history list.
 */
static void
BC_discard_history(void)
{
	struct BC_history_mount_device  *hmd;
	struct BC_history_entry_cluster *hec;
	
	while (BC_cache->c_history_mounts != NULL) {
		hmd = BC_cache->c_history_mounts;
		BC_cache->c_history_mounts = hmd->hmd_next;
		BC_FREE_SINGLE(hmd);
	}
	
	while (BC_cache->c_history != NULL) {
		hec = BC_cache->c_history;
		BC_cache->c_history = hec->hec_link;
		BC_FREE_ARRAY_NOPOINTERS(hec->hec_data, BC_HISTORY_DATA_NUM_ALLOCED_ENTRIES);
		BC_FREE_SINGLE(hec);
	}
}

/*
 * Setup for the root mounted device
 */
static int
BC_setup(void)
{
	unsigned int boot_arg;
	
	/*
	 * If we have less than the minimum supported physical memory,
	 * bail immediately.  With too little memory, the cache will
	 * become polluted with nondeterministic pagein activity, leaving
	 * large amounts of wired memory around which further exacerbates
	 * the problem.
	 */
	if ((max_mem / (1024 * 1024)) < BC_minimum_mem_size) {
		debug("insufficient physical memory (%lluMB < %dMB required)",
			  (max_mem / (1024 * 1024)), BC_minimum_mem_size);
		return(ENOMEM);
	}
	
	/*
	 * Find our block device.
	 *
	 * Note that in the netbooted case, we are loaded but should not
	 * actually run, unless we're explicitly overridden.
	 *
	 * Note also that if the override is set, we'd better have a valid
	 * rootdev...
	 */
	if (netboot_root() && 
		(!PE_parse_boot_argn("BootCacheOverride", &boot_arg, sizeof(boot_arg)) ||
		 (boot_arg != 1))) {                          /* not interesting */
			
		return(ENXIO);
	}
	
	BC_parse_boot_args();
	
	if (!cache_ssds && !cache_nonroot_disks) {
		uint32_t root_disk_is_ssd = false;
		if (VNOP_IOCTL(rootvp,       /* vnode */
					   DKIOCISSOLIDSTATE,    /* cmd */
					   (caddr_t)&root_disk_is_ssd, /* data */
					   0,
					   vfs_context_current()))           /* context */
		{
			message("can't determine if root disk is an SSD");
			root_disk_is_ssd = false; // Assume false on failure
		}
		if (root_disk_is_ssd) {
			// Fusion drives are considered SSD by the DKIOCISSOLIDSTATE check above, need to differentiate
			uint32_t root_mount_flags = 0;
		bc_get_volume_info(rootdev, &root_mount_flags, NULL, NULL, NULL, NULL);
			if (!(root_mount_flags & BC_FS_APFS_FUSION) &&
				!(root_mount_flags & BC_FS_CS_FUSION)) {
				
				message("Root disk is an SSD, not caching");
				return(ENOTSUP);
			}
		}
	}

#if defined(__arm64__)
	if (!cache_apple_silicon_macs) {
		message("Not caching Apple Silicon Mac");
		return(ENOTSUP);
	}
#endif
	
	debug("Major of rootdev is %d", major(rootdev));
	
	BC_cache->c_devsw = &bdevsw[major(rootdev)];
	assert(BC_cache->c_devsw != NULL);
	
	/* Make sure we don't clobber the device strategy routine */
	if ((BC_cache->c_devsw->d_strategy == (strategy_fcn_t*)BC_strategy) ||
		(BC_cache->c_devsw->d_close    == BC_close)) {
		debug("cache was not terminated properly previously");		
		return(ENXIO);
	}
	
	BC_cache->c_strategy = BC_cache->c_devsw->d_strategy;
	BC_cache->c_close    = BC_cache->c_devsw->d_close;
	if (BC_cache->c_strategy == NULL ||
		BC_cache->c_close    == NULL) {	/* not configured */
		return(ENXIO);
	}
		
	BC_set_flag(BC_FLAG_SETUP);
	
	return(0);
}


/*
 * Called from the root-unmount hook.
 */
static void
BC_root_unmount_hook(void)
{
	debug("device unmounted");
	
	/*
	 * If the cache is running, stop it.  If it's already stopped
	 * (it may have stopped itself), that's OK.
	 */
	BC_terminate_cache("root unmounted");
	BC_terminate_history("root unmounted");
	
}


/*
 * Handle the command sysctl.
 */
static int
BC_sysctl SYSCTL_HANDLER_ARGS
{
	struct BC_command bc;
	int error;
	
#if BC_DEBUG
	char procname[128];
	proc_selfname(procname, sizeof(procname));
#endif
	
	/* get the commande structure and validate */
	if ((error = SYSCTL_IN(req, &bc, sizeof(bc))) != 0) {
		debug("couldn't get command by %s [%d], ppid %d", procname, proc_selfpid(), proc_selfppid());
		return(error);
	}
	if (bc.bc_magic != BC_MAGIC) {
		debug("bad command magic by %s [%d], ppid %d", procname, proc_selfpid(), proc_selfppid());
		return(EINVAL);
	}
	
	switch (bc.bc_opcode) {
		case BC_OP_START_NORECORDING:
		case BC_OP_START:
			debug("BC_OP_START(%lu mounts, %lu extents) by %s [%d]", (bc.bc_data1_size / sizeof(struct BC_playlist_mount)), (bc.bc_data2_size / sizeof(struct BC_playlist_entry)), procname, proc_selfppid());
			error = BC_init_cache(bc.bc_data1_size, (user_addr_t)bc.bc_data1, bc.bc_data2_size, (user_addr_t)bc.bc_data2, (bc.bc_opcode == BC_OP_START));
			if (error != 0 && error != EALREADY) {
				message("cache init failed");
			}
			break;
			
		case BC_OP_STOP:
			debug("BC_OP_STOP by %s [%d], ppid %d", procname, proc_selfpid(), proc_selfppid());
			
			/*
			 * If we're recording history, stop it.  If it's already stopped
			 * (it may have stopped itself), that's OK.
			 */
			BC_terminate_history("stop sysctl");
			
			/* return the size of the history buffer */
			LOCK_HISTORY_W();
			bc.bc_data1_size = BC_size_history_mounts();
			bc.bc_data2_size = BC_size_history_entries();
			if ((error = SYSCTL_OUT(req, &bc, sizeof(bc))) != 0)
				debug("could not return history size");
			UNLOCK_HISTORY_W();
			
			break;
			
		case BC_OP_MOUNT:
			/* debug("BC_OP_MOUNT"); */
			
			/*
			 * A notification that mounts have changed.
			 */
			error = BC_update_mounts();
			break;
			
		case BC_OP_HISTORY:
			debug("BC_OP_HISTORY by %s [%d], ppid %d", procname, proc_selfpid(), proc_selfppid());
			/* if there's a user buffer, verify the size and copy out */
			LOCK_HISTORY_W();
			if (bc.bc_data1 != 0 && bc.bc_data2 != 0) {
				if (bc.bc_data1_size < BC_size_history_mounts() ||
					bc.bc_data2_size < BC_size_history_entries()) {
					debug("supplied history buffer too small");
					error = EINVAL;
					UNLOCK_HISTORY_W();
					break;
				}
				if ((error = BC_copyout_history_mounts(bc.bc_data1)) != 0) {
					debug("could not copy out history mounts: %d", error);
				}
				if (!error && (error = BC_copyout_history_entries(bc.bc_data2)) != 0) {
					debug("could not copy out history entries: %d", error);
				}
			}
			/*
			 * Release the last of the memory we own.
			 */
			BC_discard_history();
			UNLOCK_HISTORY_W();
			break;
			
		case BC_OP_JETTISON:
			debug("BC_OP_JETTISON by %s [%d], ppid %d", procname, proc_selfpid(), proc_selfppid());
			
			/*
			 * Jettison the cache. If it's already jettisoned
			 * (it may have jettisoned itself), that's OK.
			 */
			BC_terminate_cache("jettison sysctl");
			break;
			
		case BC_OP_RESET:
			debug("BC_OP_RESET by %s [%d], ppid %d", procname, proc_selfpid(), proc_selfppid());
			
			/*
			 * Reset the cache to a clean state, ready for a playlist
			 */
			error = BC_reset_cache();
			break;
			
		case BC_OP_STATS:
			debug("BC_OP_STATS by %s [%d], ppid %d", procname, proc_selfpid(), proc_selfppid());
			/* check buffer size and copy out */
			if (bc.bc_data1_size != sizeof(BC_cache->c_stats)) {
				debug("stats structure wrong size");
				error = ENOMEM;
			} else {
				BC_cache->c_stats.ss_cache_flags = BC_cache->c_flags;
				if ((error = copyout(&BC_cache->c_stats, bc.bc_data1, bc.bc_data1_size)) != 0)
					debug("could not copy out statistics");
			}
			break;
			
		case BC_OP_DEBUG_BUFFER:
			debug("BC_OP_DEBUG_BUFFER by %s [%d], ppid %d", procname, proc_selfpid(), proc_selfppid());
#ifdef BC_DEBUG
			lck_mtx_lock(&debug_printlock);
			/* check buffer size and copy out */
			if (debug_print_buffer_length == 0) {
				error = ENOENT;
			} else {
				size_t copyout_size = debug_print_buffer_length + 1; // + 1 for terminating NUL character
				if (copyout_size > UINT_MAX) {
					error = E2BIG;
				} else {
					if (bc.bc_data1 != 0) {
						if (bc.bc_data1_size < copyout_size) {
							error = ENOMEM;
						} else {
							// Limit individual copyout sizes to avoid "transfer too large" panic
							const uint64_t MAXCOPYOUTSIZE = (1024 * 1024);
							for (size_t offset = 0; offset < copyout_size; offset += MAXCOPYOUTSIZE) {
								size_t copysize = MIN(MAXCOPYOUTSIZE, copyout_size - offset);
								error = copyout(debug_print_buffer + offset, bc.bc_data1 + offset, copysize);
								if (error != 0) {
									break;
								}
							}
						}
					}
					bc.bc_data1_size = (unsigned int)copyout_size;
				}
			}
			lck_mtx_unlock(&debug_printlock);
			if (!error) {
				if ((error = SYSCTL_OUT(req, &bc, sizeof(bc))) != 0) {
					debug("could not return debug buffer size");
				}
			}
#else
			error = ENODATA;
#endif
			break;
			
		case BC_OP_SET_USER_OVERSIZE:
			debug("BC_OP_SET_USER_OVERSIZE by %s [%d], ppid %d", procname, proc_selfpid(), proc_selfppid());
			/* check buffer size and copy in */
			if (bc.bc_data1_size != sizeof(BC_cache->c_stats.userspace_oversize)) {
				debug("userspace oversize structure wrong size");
				error = ENOMEM;
			} else {
				if ((error = copyin(bc.bc_data1, &BC_cache->c_stats.userspace_oversize, bc.bc_data1_size)) != 0)
					debug("could not copy in user statistics");
			}
			break;
			
		case BC_OP_SET_USER_TIMESTAMPS:
			debug("BC_OP_SET_USER_TIMESTAMPS by %s [%d], ppid %d", procname, proc_selfpid(), proc_selfppid());
			/* check buffer size and copy in */
			if (bc.bc_data1_size != sizeof(BC_cache->c_stats.userspace_timestamps)) {
				debug("userspace timestamps structure wrong size");
				error = ENOMEM;
			} else {
				if ((error = copyin(bc.bc_data1, &BC_cache->c_stats.userspace_timestamps, bc.bc_data1_size)) != 0)
					debug("could not copy in user timestamps");
			}
			break;
			
		case BC_OP_SET_FUSION_OPTIMIZATION_STATS:
			debug("BC_OP_SET_FUSION_OPTIMIZATION_STATS by %s [%d], ppid %d", procname, proc_selfpid(), proc_selfppid());
			/* check buffer size and copy in */
			if (bc.bc_data1_size != sizeof(BC_cache->c_stats.userspace_fusion_optimizations)) {
				debug("userspace fusion stats structure wrong size");
				error = ENOMEM;
			} else {
				struct BC_userspace_fusion_optimizations userspace_fusion_optimizations = {0};
				if ((error = copyin(bc.bc_data1, &userspace_fusion_optimizations, bc.bc_data1_size)) != 0) {
					debug("could not copy in user fusion statistics");
				} else {
					BC_cache->c_stats.userspace_fusion_optimizations = userspace_fusion_optimizations;
				}
			}
			
			break;
			
		case BC_OP_SET_HDD_OPTIMIZATION_STATS:
			debug("BC_OP_SET_HDD_OPTIMIZATION_STATS by %s [%d], ppid %d", procname, proc_selfpid(), proc_selfppid());
			/* check buffer size and copy in */
			if (bc.bc_data1_size != sizeof(BC_cache->c_stats.userspace_hdd_optimizations)) {
				debug("userspace hdd stats structure wrong size");
				error = ENOMEM;
			} else {
				struct BC_userspace_hdd_optimizations userspace_hdd_optimizations = {0};
				if ((error = copyin(bc.bc_data1, &userspace_hdd_optimizations, bc.bc_data1_size)) != 0) {
					debug("could not copy in user hdd statistics");
				} else {
					
					BC_cache->c_stats.userspace_hdd_optimizations = userspace_hdd_optimizations;
				}
			}
			break;

		case BC_OP_SET_HDD_OPTIMIZATION_STATE:
			debug("BC_OP_SET_HDD_OPTIMIZATION_STATE by %s [%d], ppid %d", procname, proc_selfpid(), proc_selfppid());
			/* check buffer size and copy in */
			if (bc.bc_data1_size != sizeof(BC_cache->c_stats.userspace_hdd_optimization_state)) {
				debug("userspace hdd state structure wrong size");
				error = ENOMEM;
			} else {
				struct BC_userspace_hdd_optimization_state userspace_hdd_optimization_state = {0};
				if ((error = copyin(bc.bc_data1, &userspace_hdd_optimization_state, bc.bc_data1_size)) != 0) {
					debug("could not copy in hdd state statistics");
				} else {
					
					BC_cache->c_stats.userspace_hdd_optimization_state = userspace_hdd_optimization_state;
				}
			}
			break;

		case BC_OP_TAG:
			debug("BC_OP_TAG by %s [%d], ppid %d", procname, proc_selfpid(), proc_selfppid());
			KERNEL_DEBUG_CONSTANT(FSDBG_CODE(DBG_BOOTCACHE, DBG_BC_TAG) |
								  DBG_FUNC_NONE, proc_selfppid(), tag_batch_num,
								  0, 0, 0);
			BC_add_history(0, 0, 0, 0, 0, 0, 1, 0, 0, 0);
			tag_batch_num++;
			
			if (bc.bc_data1_size == sizeof(tag_batch_num)) {
				if ((error = copyout(&tag_batch_num, bc.bc_data1, bc.bc_data1_size)) != 0)
					debug("could not copy out batch number");
			}

			break;
			
		case BC_OP_TEST:
			debug("BC_OP_TEST by %s [%d], ppid %d", procname, proc_selfpid(), proc_selfppid());
			
			if (! (BC_cache->c_flags & BC_FLAG_SETUP)) {
				error = ENODEV;
			}
			break;
			
		default:
			debug("Bad op: %d by %s [%d], ppid %d", bc.bc_opcode, procname, proc_selfpid(), proc_selfppid());
			error = EINVAL;
	}
	return(error);
}


/*
 * Initialise the block of pages we use to back our data.
 *
 */
static int
BC_alloc_pagebuffer()
{
	kern_return_t kret;
	int ret;
	
	if (BC_cache->c_vp == NULL) {
		
		kret = vnode_open(BC_BOOT_BACKINGFILE, (O_CREAT | O_NOFOLLOW | FREAD), 
						  0400, 0, &BC_cache->c_vp, vfs_context_current());
		if (kret != KERN_SUCCESS) {
			debug("vnode_open failed - %d", kret);
			return -1;
		}
		
		BC_cache->c_vid = vnode_vid(BC_cache->c_vp);

	} else {
		ret = vnode_getwithvid(BC_cache->c_vp, BC_cache->c_vid);
		if (ret != 0) {
			debug("vnode_getwithvid failed - %d", ret);
			return -1;
		}
	}
		
	assert(BC_cache->c_cachesize != 0);
	kret = ubc_setsize(BC_cache->c_vp, BC_cache->c_cachesize);
	if (kret != 1) {
		debug("ubc_setsize failed - %d", kret);
		(void) vnode_put(BC_cache->c_vp);
		return -1;
	}
	
	(void) vnode_put(BC_cache->c_vp);
	
	return(0);
}

/*
 * Release all resources associated with our pagebuffer.
 */
static void
BC_free_pagebuffer(void)
{
	kern_return_t kret;
	if (BC_cache->c_vp != NULL) {
		/* Can handle a vnode without ubc info */
		kret = vnode_getwithref(BC_cache->c_vp);
		if (kret != KERN_SUCCESS) {
			debug("vnode_getwithref failed - %d", kret);
			return;
		}
		
		kret = ubc_range_op(BC_cache->c_vp, 0, BC_cache->c_cachesize, 
							UPL_ROP_DUMP, NULL);
		if (kret != KERN_SUCCESS) {
			debug("ubc_range_op failed - %d", kret);
		}
		
#if 0
		/* need this interface to be exported... will eliminate need
		 * for the ubc_range_op above */
		kret = vnode_delete(BC_BOOT_BACKINGFILE, vfs_context_current());
		if (kret) {
			debug("vnode_delete failed - %d", error);
		}
#endif
		kret = vnode_close(BC_cache->c_vp, 0, vfs_context_current());
		if (kret != KERN_SUCCESS) {
			debug("vnode_close failed - %d", kret);
		}
		
		BC_cache->c_vp = NULL;
	}
}

/*
 * Release one page from the pagebuffer.
 */
static void
BC_free_page(struct BC_cache_extent *ce, u_int64_t page)
{
	off_t vpoffset;
	kern_return_t kret;
	
	vpoffset = (page * PAGE_SIZE) + ce->ce_cacheoffset;
	kret = vnode_getwithvid(BC_cache->c_vp, BC_cache->c_vid);
	if (kret != KERN_SUCCESS) {
		debug("vnode_getwithvid failed - %d", kret);
		return;
	}
	
	kret = ubc_range_op(BC_cache->c_vp, vpoffset, vpoffset + PAGE_SIZE,
						UPL_ROP_DUMP, NULL);
	if (kret != KERN_SUCCESS) {
		debug("ubc_range_op failed - %d", kret);
	}
	
	(void) vnode_put(BC_cache->c_vp);
}

void
BC_root_mount_hook(void)
{
	int error;
	
	if ((error = BC_setup()) != 0) {
		(void)error;
		debug("BootCache setup failed: %d", error);
	}
}

/*
 * Cache start/stop functions.
 */
void
BC_load(void)
{
#ifdef BC_DEBUG
	microtime(&debug_starttime);
#endif
	
	/* register our control interface */
	sysctl_register_oid(&sysctl__kern_BootCache);
	
	/* clear the control structure */
	bzero(BC_cache, sizeof(*BC_cache));
	
	BC_cache->c_lckgrp = lck_grp_alloc_init("BootCache", LCK_GRP_ATTR_NULL);
	lck_rw_init(&BC_cache->c_history_lock, BC_cache->c_lckgrp, LCK_ATTR_NULL);
	lck_rw_init(&BC_cache->c_noncached_mounts_lock, BC_cache->c_lckgrp, LCK_ATTR_NULL);
	lck_rw_init(&BC_cache->c_cache_lock, BC_cache->c_lckgrp, LCK_ATTR_NULL);
	lck_mtx_init(&BC_cache->c_history_cluster_lock, BC_cache->c_lckgrp, LCK_ATTR_NULL);
	lck_mtx_init(&BC_cache->c_handlers_lock, BC_cache->c_lckgrp, LCK_ATTR_NULL);
	lck_mtx_init(&BC_cache->c_reader_threads_lock, BC_cache->c_lckgrp, LCK_ATTR_NULL);
#ifdef BC_DEBUG
	lck_mtx_init(&debug_printlock, BC_cache->c_lckgrp, LCK_ATTR_NULL);
#endif
	
	unmountroot_pre_hook = BC_root_unmount_hook;
	mountroot_post_hook = BC_root_mount_hook;
	debug("waiting for playlist...");
	
	microtime(&BC_cache->c_loadtime);
	BC_cache->c_stats.ss_load_timestamp = mach_absolute_time();
}

int
BC_unload(void)
{
	int error;
	
	debug("preparing to unload...");
	unmountroot_pre_hook = NULL;
	
	/*
	 * If the cache is running, stop it.  If it's already stopped
	 * (it may have stopped itself), that's OK.
	 */
	BC_terminate_history("kext unload");
	if ((error = BC_terminate_cache("kext unload")) != 0) {
		if (error != EALREADY)
			goto out;
	}
	LOCK_HISTORY_W();
	BC_discard_history();
	UNLOCK_HISTORY_W();
	
	lck_rw_destroy(&BC_cache->c_history_lock, BC_cache->c_lckgrp);
	lck_rw_destroy(&BC_cache->c_noncached_mounts_lock, BC_cache->c_lckgrp);
	lck_rw_destroy(&BC_cache->c_cache_lock, BC_cache->c_lckgrp);
	lck_mtx_destroy(&BC_cache->c_handlers_lock, BC_cache->c_lckgrp);
	lck_mtx_destroy(&BC_cache->c_history_cluster_lock, BC_cache->c_lckgrp);
	lck_mtx_destroy(&BC_cache->c_reader_threads_lock, BC_cache->c_lckgrp);
	lck_grp_free(BC_cache->c_lckgrp);
	
	sysctl_unregister_oid(&sysctl__kern_BootCache);
	error = 0;
	
out:
	return(error);
}
