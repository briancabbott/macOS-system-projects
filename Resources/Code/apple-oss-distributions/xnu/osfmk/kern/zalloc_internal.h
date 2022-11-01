/*
 * Copyright (c) 2000-2020 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/*
 * @OSF_COPYRIGHT@
 */
/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988,1987 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 */

#ifndef _KERN_ZALLOC_INTERNAL_H_
#define _KERN_ZALLOC_INTERNAL_H_

#include <kern/zalloc.h>
#include <kern/locks.h>
#include <kern/simple_lock.h>

#include <os/atomic_private.h>
#include <sys/queue.h>

#if KASAN
#include <san/kasan.h>
#include <kern/spl.h>
#endif /* !KASAN */

#if KASAN_ZALLOC
/*
 * Disable zalloc zero validation under kasan as it is
 * double-duty with what kasan already does.
 */
#define ZALLOC_ENABLE_ZERO_CHECK 0
#define ZONE_ENABLE_LOGGING 0
#elif DEBUG || DEVELOPMENT
#define ZALLOC_ENABLE_ZERO_CHECK 1
#define ZONE_ENABLE_LOGGING 1
#else
#define ZALLOC_ENABLE_ZERO_CHECK 1
#define ZONE_ENABLE_LOGGING 0
#endif

/*!
 * @file <kern/zalloc_internal.h>
 *
 * @abstract
 * Exposes some guts of zalloc to interact with the VM, debugging, copyio and
 * kalloc subsystems.
 */

__BEGIN_DECLS

#pragma GCC visibility push(hidden)

#if CONFIG_GZALLOC
typedef struct gzalloc_data {
	uint32_t        gzfc_index;
	vm_offset_t     *gzfc;
} gzalloc_data_t;
#endif

/*
 *	A zone is a collection of fixed size blocks for which there
 *	is fast allocation/deallocation access.  Kernel routines can
 *	use zones to manage data structures dynamically, creating a zone
 *	for each type of data structure to be managed.
 *
 */

/*!
 * @typedef zone_pva_t
 *
 * @brief
 * Type used to point to a page virtual address in the zone allocator.
 *
 * @description
 * - Valid pages have the top bit set.
 * - 0 represents the "NULL" page
 * - non 0 values with the top bit cleared represent queue heads,
 *   indexed from the beginning of the __DATA section of the kernel.
 *   (see zone_pageq_base).
 */
typedef struct zone_packed_virtual_address {
	uint32_t packed_address;
} zone_pva_t;

/*!
 * @struct zone_stats
 *
 * @abstract
 * Per-cpu structure used for basic zone stats.
 *
 * @discussion
 * The values aren't scaled for per-cpu zones.
 */
struct zone_stats {
	uint64_t            zs_mem_allocated;
	uint64_t            zs_mem_freed;
	uint32_t            zs_alloc_rr;     /* allocation rr bias */
};

STAILQ_HEAD(zone_depot, zone_magazine);

struct zone {
	/*
	 * Readonly / rarely written fields
	 */

	/*
	 * The first 4 fields match a zone_view.
	 *
	 * z_self points back to the zone when the zone is initialized,
	 * or is NULL else.
	 */
	struct zone        *z_self;
	zone_stats_t        z_stats;
	const char         *z_name;
	struct zone_view   *z_views;

	struct thread      *z_expander;
	struct zone_cache  *__zpercpu z_pcpu_cache;

	uint16_t            z_chunk_pages;  /* size used for more memory in pages  */
	uint16_t            z_chunk_elems;  /* count of allocations per chunk      */
	uint16_t            z_elems_rsv;    /* maintain a free reserve of elements */
	uint16_t            z_elem_size;    /* size of an element                  */
	uint16_t            z_pgz_oob_offs; /* element initial offset              */

	uint64_t /* 48 bits */
	/*
	 * Lifecycle state (Mutable after creation)
	 */
	    z_destroyed        :1,  /* zone is (being) destroyed */
	    z_async_refilling  :1,  /* asynchronous allocation pending? */
	    z_expanding_wait   :1,  /* is thread waiting for expansion? */
	    z_expander_vm_priv :1,  /* a vm privileged thread is expanding */

	/*
	 * Behavior configuration bits
	 */
	    z_percpu           :1,  /* the zone is percpu */
	    z_permanent        :1,  /* the zone allocations are permanent */
	    z_nocaching        :1,  /* disallow zone caching for this zone */
	    collectable        :1,  /* garbage collect empty pages */
	    exhaustible        :1,  /* merely return if empty? */
	    expandable         :1,  /* expand zone (with message)? */
	    no_callout         :1,
	    z_destructible     :1,  /* zone can be zdestroy()ed  */

	    _reserved          :19,

	/*
	 * Debugging features
	 */
	    alignment_required :1,  /* element alignment needs to be preserved */
	    z_pgz_tracked      :1,  /* this zone is tracked by pgzalloc */
	    z_pgz_use_guards   :1,  /* this zone uses guards with PGZ */
	    z_gzalloc_tracked  :1,  /* this zone is tracked by gzalloc */
	    z_nogzalloc        :1,  /* this zone doesn't participate with (p)gzalloc */
	    kasan_fakestacks   :1,
	    kasan_noquarantine :1,  /* whether to use the kasan quarantine */
	    z_tags_sizeclass   :6,  /* idx into zone_tags_sizeclasses to associate
	                             * sizeclass for a particualr kalloc tag */
	    z_uses_tags        :1,
	    z_tags_inline      :1,
	    z_log_on           :1,  /* zone logging was enabled by boot-arg */
	    z_tbi_tag          :1;  /* Zone supports tbi tagging */

	/*
	 * often mutated fields
	 */

	lck_spin_t          z_lock;
	struct zone_depot   z_recirc;

	/*
	 * Page accounting (wired / VA)
	 *
	 * Those numbers are unscaled for z_percpu zones
	 * (zone_scale_for_percpu() needs to be used to find the true value).
	 */
	uint32_t            z_wired_max;    /* how large can this zone grow        */
	uint32_t            z_wired_hwm;    /* z_wired_cur high watermark          */
	uint32_t            z_wired_cur;    /* number of pages used by this zone   */
	uint32_t            z_wired_empty;  /* pages collectable by GC             */
	uint32_t            z_va_cur;       /* amount of VA used by this zone      */

	/*
	 * list of metadata structs, which maintain per-page free element lists
	 */
	zone_pva_t          z_pageq_empty;  /* populated, completely empty pages   */
	zone_pva_t          z_pageq_partial;/* populated, partially filled pages   */
	zone_pva_t          z_pageq_full;   /* populated, completely full pages    */
	zone_pva_t          z_pageq_va;     /* non-populated VA pages              */

	/*
	 * Zone statistics
	 *
	 * z_contention_wma:
	 *   weighted moving average of the number of contentions per second,
	 *   in Z_CONTENTION_WMA_UNIT units (fixed point decimal).
	 *
	 * z_contention_cur:
	 *   count of recorded contentions that will be fused in z_contention_wma
	 *   at the next period.
	 *
	 * z_recirc_cur:
	 *   number of magazines in the recirculation depot.
	 *
	 * z_elems_free:
	 *   number of free elements in the zone.
	 *
	 * z_elems_{min,max}:
	 *   tracks the low/high watermark of z_elems_free for the current
	 *   weighted moving average period.
	 *
	 * z_elems_free_wss:
	 *   weighted moving average of the (z_elems_free_max - z_elems_free_min)
	 *   amplited which is used by the GC for trim operations.
	 *
	 * z_elems_avail:
	 *   number of elements in the zone (at all).
	 */
#define Z_CONTENTION_WMA_UNIT (1u << 8)
	uint32_t            z_contention_wma;
	uint32_t            z_contention_cur;
	uint32_t            z_recirc_cur;
	uint32_t            z_elems_free_max;
	uint32_t            z_elems_free_wss;
	uint32_t            z_elems_free_min;
	uint32_t            z_elems_free;   /* Number of free elements             */
	uint32_t            z_elems_avail;  /* Number of elements available        */

#if CONFIG_GZALLOC
	gzalloc_data_t      gz;
#endif
#if KASAN_ZALLOC
	uint32_t            z_kasan_redzone;
	spl_t               z_kasan_spl;
#endif
#if ZONE_ENABLE_LOGGING || CONFIG_ZLEAKS
	/*
	 * the allocation logs are used when:
	 *
	 * - zlog<n>= boot-args are used (and then z_log_on is set)
	 *
	 * - the leak detection was triggered for the zone.
	 *   In that case, the log can't ever be freed,
	 *   but it can be enabled/disabled dynamically.
	 */
	struct btlog       *z_btlog;
	struct btlog       *z_btlog_disabled;
#endif
#if DEBUG || DEVELOPMENT
	struct zone        *z_kt_next;
#endif
};

/*!
 * @typedef zone_security_flags_t
 *
 * @brief
 * Type used to store the immutable security properties of a zone.
 *
 * @description
 * These properties influence the security nature of a zone and can't be
 * modified after lockdown.
 */
typedef struct zone_security_flags {
	uint16_t
	/*
	 * Security sensitive configuration bits
	 */
	    z_submap_idx       :8,  /* a Z_SUBMAP_IDX_* value */
	    z_submap_from_end  :1,  /* allocate from the left or the right ? */
	    z_kheap_id         :3,  /* zone_kheap_id_t when part of a kalloc heap */
	    z_allows_foreign   :1,  /* allow non-zalloc space  */
	    z_noencrypt        :1,  /* do not encrypt pages when hibernating */
	    z_va_sequester     :1,  /* page sequester: no VA reuse with other zones */
	    z_kalloc_type      :1;  /* zones that does types based seggregation */
} zone_security_flags_t;


/*
 * Zsecurity config to enable sequestering VA of zones
 */
#if KASAN_ZALLOC || !defined(__LP64__)
#   define ZSECURITY_CONFIG_SEQUESTER                   OFF
#else
#   define ZSECURITY_CONFIG_SEQUESTER                   ON
#endif

/*
 * Zsecurity config to enable creating separate kalloc zones for
 * bags of bytes
 */
#if KASAN_ZALLOC || !defined(__LP64__)
#   define ZSECURITY_CONFIG_SUBMAP_USER_DATA            OFF
#else
#   define ZSECURITY_CONFIG_SUBMAP_USER_DATA            ON
#endif

/*
 * Leave kext heap on macOS for kalloc/kalloc_type callsites that aren't
 * in the BootKC.
 */
#if KASAN_ZALLOC || !defined(__LP64__)
#   define ZSECURITY_CONFIG_SEQUESTER_KEXT_KALLOC       OFF
#elif PLATFORM_MacOSX
#   define ZSECURITY_CONFIG_SEQUESTER_KEXT_KALLOC       OFF
#else
#   define ZSECURITY_CONFIG_SEQUESTER_KEXT_KALLOC       OFF
#endif

/*
 * Zsecurity config to enable strict free of iokit objects to zone
 * or heap they were allocated from.
 *
 * Turn ZSECURITY_OPTIONS_STRICT_IOKIT_FREE off on x86 so as not
 * not break third party kexts that haven't yet been recompiled
 * to use the new iokit macros.
 */
#if PLATFORM_MacOSX && __x86_64__
#   define ZSECURITY_CONFIG_STRICT_IOKIT_FREE           OFF
#else
#   define ZSECURITY_CONFIG_STRICT_IOKIT_FREE           ON
#endif

/*
 * Zsecurity config to enable the read-only allocator
 */
#if KASAN_ZALLOC || !defined(__LP64__)
#   define ZSECURITY_CONFIG_READ_ONLY                   OFF
#else
#   define ZSECURITY_CONFIG_READ_ONLY                   ON
#endif

/*
 * Zsecurity config to enable making heap feng-shui
 * less reliable.
 */
#if KASAN_ZALLOC || !defined(__LP64__)
#   define ZSECURITY_CONFIG_SAD_FENG_SHUI               OFF
#   define ZSECURITY_CONFIG_GENERAL_SUBMAPS             1
#else
#   define ZSECURITY_CONFIG_SAD_FENG_SHUI               ON
#   define ZSECURITY_CONFIG_GENERAL_SUBMAPS             4
#endif

/*
 * Zsecurity config to enable kalloc type segregation
 */
#if KASAN_ZALLOC || !defined(__LP64__)
#   define ZSECURITY_CONFIG_KALLOC_TYPE                 OFF
#   define ZSECURITY_CONFIG_KT_BUDGET                   0
#   define ZSECURITY_CONFIG_KT_VAR_BUDGET               0
#else
#   define ZSECURITY_CONFIG_KALLOC_TYPE                 ON
#if XNU_TARGET_OS_WATCH
#   define ZSECURITY_CONFIG_KT_BUDGET                   85
#else
#   define ZSECURITY_CONFIG_KT_BUDGET                   200
#endif
#   define ZSECURITY_CONFIG_KT_VAR_BUDGET               3
#endif


/*
 * Zsecurity options that can be toggled, as opposed to configs
 */
__options_decl(zone_security_options_t, uint64_t, {
	/*
	 * Zsecurity option to enable the kernel and kalloc data maps.
	 */
	ZSECURITY_OPTIONS_KERNEL_DATA_MAP       = 0x00000020,
});

#define ZSECURITY_NOT_A_COMPILE_TIME_CONFIG__OFF() 0
#define ZSECURITY_NOT_A_COMPILE_TIME_CONFIG__ON()  1
#define ZSECURITY_CONFIG2(v)     ZSECURITY_NOT_A_COMPILE_TIME_CONFIG__##v()
#define ZSECURITY_CONFIG1(v)     ZSECURITY_CONFIG2(v)
#define ZSECURITY_CONFIG(opt)    ZSECURITY_CONFIG1(ZSECURITY_CONFIG_##opt)
#define ZSECURITY_ENABLED(opt)   (zsecurity_options & ZSECURITY_OPTIONS_##opt)

__options_decl(kalloc_type_options_t, uint64_t, {
	/*
	 * kalloc type option to switch default accounting to private.
	 */
	KT_OPTIONS_ACCT                         = 0x00000001,
	/*
	 * kalloc type option to print additional stats regarding zone
	 * budget distribution and signatures.
	 */
	KT_OPTIONS_DEBUG                        = 0x00000002,
	/*
	 * kalloc type option to allow loose freeing between heaps
	 */
	KT_OPTIONS_LOOSE_FREE                   = 0x00000004,
});

__enum_decl(kt_var_heap_id_t, uint32_t, {
	/*
	 * Fake "data" heap used to link views of data-only allocation that
	 * have been redirected to KHEAP_DATA_BUFFERS
	 */
	KT_VAR_DATA_HEAP,
	/*
	 * Heap for pointer arrays
	 */
	KT_VAR_PTR_HEAP,
	/*
	 * Indicating first additional heap added
	 */
	KT_VAR__FIRST_FLEXIBLE_HEAP,
});

/*
 * Zone submap indices
 *
 * Z_SUBMAP_IDX_VM
 * this map has the special property that its allocations
 * can be done without ever locking the submap, and doesn't use
 * VM entries in the map (which limits certain VM map operations on it).
 *
 * On ILP32 a single zone lives here (the vm_map_entry_reserved_zone).
 *
 * On LP64 it is also used to restrict VM allocations on LP64 lower
 * in the kernel VA space, for pointer packing purposes.
 *
 * Z_SUBMAP_IDX_GENERAL_{0,1,2,3}
 * used for unrestricted allocations
 *
 * Z_SUBMAP_IDX_DATA
 * used to sequester bags of bytes from all other allocations and allow VA reuse
 * within the map
 *
 * Z_SUBMAP_IDX_READ_ONLY
 * used for the read-only allocator
 */
__enum_decl(zone_submap_idx_t, uint32_t, {
	Z_SUBMAP_IDX_VM,
	Z_SUBMAP_IDX_READ_ONLY,
	Z_SUBMAP_IDX_GENERAL_0,
#if ZSECURITY_CONFIG(SAD_FENG_SHUI)
	Z_SUBMAP_IDX_GENERAL_1,
	Z_SUBMAP_IDX_GENERAL_2,
	Z_SUBMAP_IDX_GENERAL_3,
#endif /* ZSECURITY_CONFIG(SAD_FENG_SHUI) */
	Z_SUBMAP_IDX_DATA,

	Z_SUBMAP_IDX_COUNT,
});

#define KALLOC_MINALIGN     (1 << KALLOC_LOG2_MINALIGN)
#define KALLOC_DLUT_SIZE    (2048 / KALLOC_MINALIGN)

struct kheap_zones {
	struct kalloc_zone_cfg         *cfg;
	struct kalloc_heap             *views;
	zone_kheap_id_t                 heap_id;
	uint16_t                        max_k_zone;
	uint8_t                         dlut[KALLOC_DLUT_SIZE];   /* table of indices into k_zone[] */
	uint8_t                         k_zindex_start;
	/* If there's no hit in the DLUT, then start searching from k_zindex_start. */
	zone_t                         *k_zone;
	vm_size_t                       kalloc_max;
};

/*
 * Variable kalloc_type heap config
 */
struct kt_heap_zones {
	zone_id_t                       kh_zstart;
	zone_kheap_id_t                 heap_id;
	struct kalloc_type_var_view    *views;
};

#define KT_VAR_MAX_HEAPS 8
#define MAX_ZONES       650
extern struct kt_heap_zones     kalloc_type_heap_array[KT_VAR_MAX_HEAPS];
extern zone_security_options_t  zsecurity_options;
extern zone_id_t _Atomic        num_zones;
extern uint32_t                 zone_view_count;
extern struct zone              zone_array[];
extern zone_security_flags_t    zone_security_array[];
extern uint16_t                 zone_ro_elem_size[];
extern const char * const       kalloc_heap_names[KHEAP_ID_COUNT];
extern mach_memory_info_t      *panic_kext_memory_info;
extern vm_size_t                panic_kext_memory_size;
extern vm_offset_t              panic_fault_address;

#define zone_index_foreach(i) \
	for (zone_id_t i = 1, num_zones_##i = os_atomic_load(&num_zones, acquire); \
	    i < num_zones_##i; i++)

#define zone_foreach(z) \
	for (zone_t z = &zone_array[1], \
	    last_zone_##z = &zone_array[os_atomic_load(&num_zones, acquire)]; \
	    z < last_zone_##z; z++)

struct zone_map_range {
	vm_offset_t min_address;
	vm_offset_t max_address;
} __attribute__((aligned(2 * sizeof(vm_offset_t))));

__abortlike
extern void zone_invalid_panic(zone_t zone);

__pure2
static inline zone_id_t
zone_index(zone_t z)
{
	zone_id_t zid = (zone_id_t)(z - zone_array);
	if (__improbable(zid >= MAX_ZONES)) {
		zone_invalid_panic(z);
	}
	return zid;
}

__pure2
static inline zone_t
zone_for_index(zone_id_t zid)
{
	return &zone_array[zid];
}

__pure2
static inline bool
zone_is_ro(zone_t zone)
{
	return zone >= &zone_array[ZONE_ID__FIRST_RO] &&
	       zone <= &zone_array[ZONE_ID__LAST_RO];
}

__pure2
static inline vm_offset_t
zone_elem_size_ro(zone_id_t zid)
{
	return zone_ro_elem_size[zid];
}

static inline bool
zone_addr_size_crosses_page(mach_vm_address_t addr, mach_vm_size_t size)
{
	return atop(addr ^ (addr + size - 1)) != 0;
}

__pure2
static inline uint16_t
zone_oob_offs(zone_t zone)
{
	uint16_t offs = 0;
#if CONFIG_PROB_GZALLOC
	offs = zone->z_pgz_oob_offs;
#else
	(void)zone;
#endif
	return offs;
}

__pure2
static inline vm_offset_t
zone_elem_size(zone_t zone)
{
	return zone->z_elem_size;
}

__pure2
static inline vm_offset_t
zone_elem_size_safe(zone_t zone)
{
	if (zone_is_ro(zone)) {
		zone_id_t zid = zone_index(zone);
		return zone_elem_size_ro(zid);
	}
	return zone_elem_size(zone);
}

__pure2
static inline zone_security_flags_t
zone_security_config(zone_t z)
{
	zone_id_t zid = zone_index(z);
	return zone_security_array[zid];
}

static inline uint32_t
zone_count_allocated(zone_t zone)
{
	return zone->z_elems_avail - zone->z_elems_free;
}

static inline vm_size_t
zone_scale_for_percpu(zone_t zone, vm_size_t size)
{
	if (zone->z_percpu) {
		size *= zpercpu_count();
	}
	return size;
}

static inline vm_size_t
zone_size_wired(zone_t zone)
{
	/*
	 * this either require the zone lock,
	 * or to be used for statistics purposes only.
	 */
	vm_size_t size = ptoa(os_atomic_load(&zone->z_wired_cur, relaxed));
	return zone_scale_for_percpu(zone, size);
}

static inline vm_size_t
zone_size_free(zone_t zone)
{
	return zone_scale_for_percpu(zone,
	           (vm_size_t)zone->z_elem_size * zone->z_elems_free);
}

/* Under KASAN builds, this also accounts for quarantined elements. */
static inline vm_size_t
zone_size_allocated(zone_t zone)
{
	return zone_scale_for_percpu(zone,
	           (vm_size_t)zone->z_elem_size * zone_count_allocated(zone));
}

static inline vm_size_t
zone_size_wasted(zone_t zone)
{
	return zone_size_wired(zone) - zone_scale_for_percpu(zone,
	           (vm_size_t)zone->z_elem_size * zone->z_elems_avail);
}

/*
 * For sysctl kern.zones_collectable_bytes used by memory_maintenance to check if a
 * userspace reboot is needed. The only other way to query for this information
 * is via mach_memory_info() which is unavailable on release kernels.
 */
extern uint64_t get_zones_collectable_bytes(void);

/*!
 * @enum zone_gc_level_t
 *
 * @const ZONE_GC_TRIM
 * Request a trimming GC: it will trim allocations in excess
 * of the working set size estimate only.
 *
 * @const ZONE_GC_DRAIN
 * Request a draining GC: this is an aggressive mode that will
 * cause all caches to be drained and all free pages returned to the system.
 *
 * @const ZONE_GC_JETSAM
 * Request to consider a jetsam, and then fallback to @c ZONE_GC_TRIM or
 * @c ZONE_GC_DRAIN depending on the state of the zone map.
 * To avoid deadlocks, only @c vm_pageout_garbage_collect() should ever
 * request a @c ZONE_GC_JETSAM level.
 */
__enum_closed_decl(zone_gc_level_t, uint32_t, {
	ZONE_GC_TRIM,
	ZONE_GC_DRAIN,
	ZONE_GC_JETSAM,
});

/*!
 * @function zone_gc
 *
 * @brief
 * Reduces memory used by zones by trimming caches and freelists.
 *
 * @discussion
 * @c zone_gc() is called:
 * - by the pageout daemon when the system needs more free pages.
 * - by the VM when contiguous page allocation requests get stuck
 *   (see vm_page_find_contiguous()).
 *
 * @param level         The zone GC level requested.
 */
extern void     zone_gc(zone_gc_level_t level);

extern void     zone_gc_trim(void);
extern void     zone_gc_drain(void);

#define ZONE_WSS_UPDATE_PERIOD  10
/*!
 * @function compute_zone_working_set_size
 *
 * @brief
 * Recomputes the working set size for every zone
 *
 * @discussion
 * This runs about every @c ZONE_WSS_UPDATE_PERIOD seconds (10),
 * computing an exponential moving average with a weight of 75%,
 * so that the history of the last minute is the dominating factor.
 */
extern void     compute_zone_working_set_size(void *);

/* Debug logging for zone-map-exhaustion jetsams. */
extern void     get_zone_map_size(uint64_t *current_size, uint64_t *capacity);
extern void     get_largest_zone_info(char *zone_name, size_t zone_name_len, uint64_t *zone_size);

/* Bootstrap zone module (create zone zone) */
extern void     zone_bootstrap(void);

/*!
 * @function zone_foreign_mem_init
 *
 * @brief
 * Steal memory from pmap (prior to initialization of zalloc)
 * for the special vm zones that allow foreign memory and store
 * the range so as to facilitate range checking in zfree.
 *
 * @param size              the size to steal (must be a page multiple)
 * @param allow_meta_steal  whether allocator metadata should be stolen too
 *                          due to a non natural config.
 */
__startup_func
extern vm_offset_t zone_foreign_mem_init(
	vm_size_t       size,
	bool            allow_meta_steal);

/*!
 * @function zone_get_foreign_alloc_size
 *
 * @brief
 * Compute the correct size (greater than @c ptoa(min_pages)) that is a multiple
 * of the allocation granule for the zone with the given creation flags and
 * element size.
 */
__startup_func
extern vm_size_t zone_get_foreign_alloc_size(
	const char          *name __unused,
	vm_size_t            elem_size,
	zone_create_flags_t  flags,
	uint16_t             min_pages);

/*!
 * @function zone_cram_foreign
 *
 * @brief
 * Cram memory allocated with @c zone_foreign_mem_init() into a zone.
 *
 * @param zone          The zone to cram memory into.
 * @param newmem        The base address for the memory to cram.
 * @param size          The size of the memory to cram into the zone.
 */
__startup_func
extern void     zone_cram_foreign(
	zone_t          zone,
	vm_offset_t     newmem,
	vm_size_t       size);

extern bool     zone_maps_owned(
	vm_address_t    addr,
	vm_size_t       size);

extern void     zone_map_sizes(
	vm_map_size_t  *psize,
	vm_map_size_t  *pfree,
	vm_map_size_t  *plargest_free);

extern bool
zone_map_nearing_exhaustion(void);

#if defined(__LP64__)
#define ZONE_POISON       0xdeadbeefdeadbeef
#else
#define ZONE_POISON       0xdeadbeef
#endif

static inline vm_tag_t
zalloc_flags_get_tag(zalloc_flags_t flags)
{
	return (vm_tag_t)((flags & Z_VM_TAG_MASK) >> Z_VM_TAG_SHIFT);
}

extern void    *zalloc_ext(
	zone_t          zone,
	zone_stats_t    zstats,
	zalloc_flags_t  flags,
	vm_size_t       elem_size);

extern void     zfree_ext(
	zone_t          zone,
	zone_stats_t    zstats,
	void           *addr,
	vm_size_t       elem_size);

extern zone_id_t zone_id_for_native_element(
	void           *addr,
	vm_size_t       esize);

#if CONFIG_PROB_GZALLOC
extern void *zone_element_pgz_oob_adjust(
	void           *addr,
	vm_size_t       esize,
	vm_size_t       req_size);
#endif /* CONFIG_PROB_GZALLOC */

extern vm_size_t zone_element_size(
	void           *addr,
	zone_t         *z,
	bool            clear_oob,
	vm_offset_t    *oob_offs);

__attribute__((overloadable))
extern bool      zone_range_contains(
	const struct zone_map_range *r,
	vm_offset_t     addr);

__attribute__((overloadable))
extern bool      zone_range_contains(
	const struct zone_map_range *r,
	vm_offset_t     addr,
	vm_offset_t     size);

extern vm_size_t zone_range_size(
	const struct zone_map_range *r);

/*!
 * @function zone_spans_ro_va
 *
 * @abstract
 * This function is used to check whether the specified address range
 * spans through the read-only zone range.
 *
 * @discussion
 * This only checks for the range specified within ZONE_ADDR_READONLY.
 * The parameters addr_start and addr_end are stripped off of PAC bits
 * before the check is made.
 */
extern bool zone_spans_ro_va(
	vm_offset_t     addr_start,
	vm_offset_t     addr_end);

/*!
 * @function __zalloc_ro_mut_atomic
 *
 * @abstract
 * This function is called from the pmap to perform the specified atomic
 * operation on memory from the read-only allocator.
 *
 * @discussion
 * This function is for internal use only and should not be called directly.
 */
static inline uint64_t
__zalloc_ro_mut_atomic(vm_offset_t dst, zro_atomic_op_t op, uint64_t value)
{
#define __ZALLOC_RO_MUT_OP(op, op2) \
	case ZRO_ATOMIC_##op##_8: \
	        return os_atomic_##op2((uint8_t *)dst, (uint8_t)value, seq_cst); \
	case ZRO_ATOMIC_##op##_16: \
	        return os_atomic_##op2((uint16_t *)dst, (uint16_t)value, seq_cst); \
	case ZRO_ATOMIC_##op##_32: \
	        return os_atomic_##op2((uint32_t *)dst, (uint32_t)value, seq_cst); \
	case ZRO_ATOMIC_##op##_64: \
	        return os_atomic_##op2((uint64_t *)dst, (uint64_t)value, seq_cst)

	switch (op) {
		__ZALLOC_RO_MUT_OP(OR, or_orig);
		__ZALLOC_RO_MUT_OP(XOR, xor_orig);
		__ZALLOC_RO_MUT_OP(AND, and_orig);
		__ZALLOC_RO_MUT_OP(ADD, add_orig);
		__ZALLOC_RO_MUT_OP(XCHG, xchg);
	default:
		panic("%s: Invalid atomic operation: %d", __func__, op);
	}

#undef __ZALLOC_RO_MUT_OP
}

/*!
 * @function zone_owns
 *
 * @abstract
 * This function is a soft version of zone_require that checks if a given
 * pointer belongs to the specified zone and should not be used outside
 * allocator code.
 *
 * @discussion
 * Note that zone_owns() can only work with:
 * - zones not allowing foreign memory
 * - zones in the general submap.
 *
 * @param zone          the zone the address needs to belong to.
 * @param addr          the element address to check.
 */
extern bool     zone_owns(
	zone_t          zone,
	void           *addr);

/**!
 * @function zone_submap
 *
 * @param zsflags       the security flags of a specified zone.
 * @returns             the zone (sub)map this zone allocates from.
 */
__pure2
extern vm_map_t zone_submap(
	zone_security_flags_t   zsflags);

/*
 *  Structure for keeping track of a backtrace, used for leak detection.
 *  This is in the .h file because it is used during panic, see kern/debug.c
 *  A non-zero size indicates that the trace is in use.
 */
struct ztrace {
	vm_size_t               zt_size;                        /* How much memory are all the allocations referring to this trace taking up? */
	uint32_t                zt_depth;                       /* depth of stack (0 to MAX_ZTRACE_DEPTH) */
	void*                   zt_stack[MAX_ZTRACE_DEPTH];     /* series of return addresses from OSBacktrace */
	uint32_t                zt_collisions;                  /* How many times did a different stack land here while it was occupied? */
	uint32_t                zt_hit_count;                   /* for determining effectiveness of hash function */
};

#ifndef VM_TAG_SIZECLASSES
#error MAX_TAG_ZONES
#endif
#if VM_TAG_SIZECLASSES

extern uint16_t zone_index_from_tag_index(
	uint32_t        tag_zone_index);

#endif /* VM_TAG_SIZECLASSES */

extern void kalloc_init_maps(
	vm_address_t min_address);

static inline void
zone_lock(zone_t zone)
{
#if KASAN_ZALLOC
	spl_t s = 0;
	if (zone->kasan_fakestacks) {
		s = splsched();
	}
#endif /* KASAN_ZALLOC */
	lck_spin_lock(&zone->z_lock);
#if KASAN_ZALLOC
	zone->z_kasan_spl = s;
#endif /* KASAN_ZALLOC */
}

static inline void
zone_unlock(zone_t zone)
{
#if KASAN_ZALLOC
	spl_t s = zone->z_kasan_spl;
	zone->z_kasan_spl = 0;
#endif /* KASAN_ZALLOC */
	lck_spin_unlock(&zone->z_lock);
#if KASAN_ZALLOC
	if (zone->kasan_fakestacks) {
		splx(s);
	}
#endif /* KASAN_ZALLOC */
}

#if CONFIG_GZALLOC
void gzalloc_init(vm_size_t);
void gzalloc_zone_init(zone_t);
void gzalloc_empty_free_cache(zone_t);
boolean_t gzalloc_enabled(void);

vm_offset_t gzalloc_alloc(zone_t, zone_stats_t zstats, zalloc_flags_t flags);
void gzalloc_free(zone_t, zone_stats_t zstats, void *);
boolean_t gzalloc_element_size(void *, zone_t *, vm_size_t *);
#endif /* CONFIG_GZALLOC */

#define MAX_ZONE_NAME   32      /* max length of a zone name we can take from the boot-args */

int track_this_zone(const char *zonename, const char *logname);
extern bool panic_include_kalloc_types;
extern zone_t kalloc_type_src_zone;
extern zone_t kalloc_type_dst_zone;

#if DEBUG || DEVELOPMENT
extern vm_size_t zone_element_info(void *addr, vm_tag_t * ptag);
extern bool zalloc_disable_copyio_check;
#else
#define zalloc_disable_copyio_check false
#endif /* DEBUG || DEVELOPMENT */

#pragma GCC visibility pop

__END_DECLS

#endif  /* _KERN_ZALLOC_INTERNAL_H_ */
