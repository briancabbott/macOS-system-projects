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
/*
 *	File:	kern/zalloc.c
 *	Author:	Avadis Tevanian, Jr.
 *
 *	Zone-based memory allocator.  A zone is a collection of fixed size
 *	data blocks for which quick allocation/deallocation is possible.
 */

#define ZALLOC_ALLOW_DEPRECATED 1
#if !ZALLOC_TEST
#include <mach/mach_types.h>
#include <mach/vm_param.h>
#include <mach/kern_return.h>
#include <mach/mach_host_server.h>
#include <mach/task_server.h>
#include <mach/machine/vm_types.h>
#include <machine/machine_routines.h>
#include <mach/vm_map.h>
#include <mach/sdt.h>
#if __x86_64__
#include <i386/cpuid.h>
#endif

#include <kern/bits.h>
#include <kern/btlog.h>
#include <kern/startup.h>
#include <kern/kern_types.h>
#include <kern/assert.h>
#include <kern/backtrace.h>
#include <kern/host.h>
#include <kern/macro_help.h>
#include <kern/sched.h>
#include <kern/locks.h>
#include <kern/sched_prim.h>
#include <kern/misc_protos.h>
#include <kern/thread_call.h>
#include <kern/zalloc_internal.h>
#include <kern/kalloc.h>
#include <kern/debug.h>

#include <prng/random.h>

#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_compressor.h> /* C_SLOT_PACKED_PTR* */

#include <pexpert/pexpert.h>

#include <machine/machparam.h>
#include <machine/machine_routines.h>  /* ml_cpu_get_info */

#include <os/atomic.h>

#include <libkern/OSDebug.h>
#include <libkern/OSAtomic.h>
#include <libkern/section_keywords.h>
#include <sys/kdebug.h>

#include <san/kasan.h>
#include <libsa/stdlib.h>
#include <sys/errno.h>

#include <IOKit/IOBSD.h>

#if DEBUG
#define z_debug_assert(expr)  assert(expr)
#else
#define z_debug_assert(expr)  (void)(expr)
#endif

/* Returns pid of the task with the largest number of VM map entries.  */
extern pid_t find_largest_process_vm_map_entries(void);

/*
 * Callout to jetsam. If pid is -1, we wake up the memorystatus thread to do asynchronous kills.
 * For any other pid we try to kill that process synchronously.
 */
extern boolean_t memorystatus_kill_on_zone_map_exhaustion(pid_t pid);

extern zone_t vm_map_entry_zone;
extern zone_t vm_object_zone;
extern zone_t ipc_service_port_label_zone;

ZONE_DEFINE_TYPE(percpu_u64_zone, "percpu.64", uint64_t,
    ZC_PERCPU | ZC_ALIGNMENT_REQUIRED | ZC_KASAN_NOREDZONE);

#if CONFIG_KERNEL_TBI && KASAN_TBI
#define ZONE_MIN_ELEM_SIZE      (sizeof(uint64_t) * 2)
#define ZONE_ALIGN_SIZE         ZONE_MIN_ELEM_SIZE
#else /* CONFIG_KERNEL_TBI && KASAN_TBI */
#define ZONE_MIN_ELEM_SIZE      sizeof(uint64_t)
#define ZONE_ALIGN_SIZE         ZONE_MIN_ELEM_SIZE
#endif /* CONFIG_KERNEL_TBI && KASAN_TBI */

#define ZONE_MAX_ALLOC_SIZE     (32 * 1024)
#if ZSECURITY_CONFIG(SAD_FENG_SHUI)
#define ZONE_CHUNK_ALLOC_SIZE   (256 * 1024)
#endif /* ZSECURITY_CONFIG(SAD_FENG_SHUI) */

__enum_closed_decl(zm_len_t, uint16_t, {
	ZM_CHUNK_FREE           = 0x0,
	/* 1 through 8 are valid lengths */
	ZM_CHUNK_LEN_MAX        = 0x8,

	/* PGZ magical values */
	ZM_PGZ_FREE             = 0x0,
	ZM_PGZ_ALLOCATED        = 0xa, /* [a]llocated   */
	ZM_PGZ_GUARD            = 0xb, /* oo[b]         */
	ZM_PGZ_DOUBLE_FREE      = 0xd, /* [d]ouble_free */

	/* secondary page markers */
	ZM_SECONDARY_PAGE       = 0xe,
	ZM_SECONDARY_PCPU_PAGE  = 0xf,
});

struct zone_page_metadata {
	union {
		struct {
			/* The index of the zone this metadata page belongs to */
			zone_id_t       zm_index : 11;

			/*
			 * Whether `zm_bitmap` is an inline bitmap
			 * or a packed bitmap reference
			 */
			uint16_t        zm_inline_bitmap : 1;

			/*
			 * Zones allocate in "chunks" of zone_t::z_chunk_pages
			 * consecutive pages, or zpercpu_count() pages if the
			 * zone is percpu.
			 *
			 * The first page of it has its metadata set with:
			 * - 0 if none of the pages are currently wired
			 * - the number of wired pages in the chunk
			 *   (not scaled for percpu).
			 *
			 * Other pages in the chunk have their zm_chunk_len set
			 * to ZM_SECONDARY_PAGE or ZM_SECONDARY_PCPU_PAGE
			 * depending on whether the zone is percpu or not.
			 * For those, zm_page_index holds the index of that page
			 * in the run.
			 *
			 * Metadata used for PGZ pages can have 3 values:
			 * - ZM_PGZ_FREE:         slot is free
			 * - ZM_PGZ_ALLOCATED:    slot holds an allocated element
			 *                        at offset (zm_pgz_orig_addr & PAGE_MASK)
			 * - ZM_PGZ_DOUBLE_FREE:  slot detected a double free
			 *                        (will panic).
			 */
			zm_len_t        zm_chunk_len : 4;
		};
		uint16_t zm_bits;
	};

	union {
#define ZM_ALLOC_SIZE_LOCK      1u
		uint16_t zm_alloc_size; /* first page only */
		uint16_t zm_page_index; /* secondary pages only */
		uint16_t zm_oob_offs;   /* in guard pages  */
	};
	union {
		uint32_t zm_bitmap;     /* most zones      */
		uint32_t zm_bump;       /* permanent zones */
	};

	union {
		struct {
			zone_pva_t      zm_page_next;
			zone_pva_t      zm_page_prev;
		};
		vm_offset_t zm_pgz_orig_addr;
		struct zone_page_metadata *zm_pgz_slot_next;
	};
};
static_assert(sizeof(struct zone_page_metadata) == 16, "validate packing");

__enum_closed_decl(zone_addr_kind_t, uint32_t, {
	ZONE_ADDR_FOREIGN,
	ZONE_ADDR_NATIVE,
	ZONE_ADDR_READONLY
});
#define ZONE_ADDR_KIND_COUNT 3

static const char * const zone_map_range_names[] = {
	[ZONE_ADDR_FOREIGN]      = "Foreign",
	[ZONE_ADDR_NATIVE]       = "Native",
	[ZONE_ADDR_READONLY]     = "Readonly",
};

/*!
 * @typedef zone_element_t
 *
 * @brief
 * Type that represents a "resolved" zone element.
 *
 * @description
 * This type encodes an element pointer as a pair of:
 * { chunk base, element index }.
 *
 * The chunk base is extracted with @c trunc_page()
 * as it is always page aligned, and occupies the bits above @c PAGE_SHIFT.
 *
 * The other bits encode the element index in the chunk rather than its address.
 */
typedef struct zone_element {
	vm_offset_t                 ze_value;
} zone_element_t;

/*!
 * @typedef zone_magazine_t
 *
 * @brief
 * Magazine of cached allocations.
 *
 * @field zm_cur        how many elements this magazine holds (unused while loaded).
 * @field zm_link       linkage used by magazine depots.
 * @field zm_elems      an array of @c zc_mag_size() elements.
 */
typedef struct zone_magazine {
	uint16_t                    zm_cur;
	STAILQ_ENTRY(zone_magazine) zm_link;
	zone_element_t              zm_elems[0];
} *zone_magazine_t;

/*!
 * @typedef zone_cache_t
 *
 * @brief
 * Magazine of cached allocations.
 *
 * @discussion
 * Below is a diagram of the caching system. This design is inspired by the
 * paper "Magazines and Vmem: Extending the Slab Allocator to Many CPUs and
 * Arbitrary Resources" by Jeff Bonwick and Jonathan Adams and the FreeBSD UMA
 * zone allocator (itself derived from this seminal work).
 *
 * It is divided into 3 layers:
 * - the per-cpu layer,
 * - the recirculation depot layer,
 * - the Zone Allocator.
 *
 * The per-cpu and recirculation depot layer use magazines (@c zone_magazine_t),
 * which are stacks of up to @c zc_mag_size() elements.
 *
 * <h2>CPU layer</h2>
 *
 * The CPU layer (@c zone_cache_t) looks like this:
 *
 *      ╭─ a ─ f ─┬───────── zm_depot ──────────╮
 *      │ ╭─╮ ╭─╮ │ ╭─╮ ╭─╮ ╭─╮ ╭─╮ ╭─╮         │
 *      │ │#│ │#│ │ │#│ │#│ │#│ │#│ │#│         │
 *      │ │#│ │ │ │ │#│ │#│ │#│ │#│ │#│         │
 *      │ │ │ │ │ │ │#│ │#│ │#│ │#│ │#│         │
 *      │ ╰─╯ ╰─╯ │ ╰─╯ ╰─╯ ╰─╯ ╰─╯ ╰─╯         │
 *      ╰─────────┴─────────────────────────────╯
 *
 * It has two pre-loaded magazines (a)lloc and (f)ree which we allocate from,
 * or free to. Serialization is achieved through disabling preemption, and only
 * the current CPU can acces those allocations. This is represented on the left
 * hand side of the diagram above.
 *
 * The right hand side is the per-cpu depot. It consists of @c zm_depot_count
 * full magazines, and is protected by the @c zm_depot_lock for access.
 * The lock is expected to absolutely never be contended, as only the local CPU
 * tends to access the local per-cpu depot in regular operation mode.
 *
 * However unlike UMA, our implementation allows for the zone GC to reclaim
 * per-CPU magazines aggresively, which is serialized with the @c zm_depot_lock.
 *
 *
 * <h2>Recirculation Depot</h2>
 *
 * The recirculation depot layer is a list similar to the per-cpu depot,
 * however it is different in two fundamental ways:
 *
 * - it is protected by the regular zone lock,
 * - elements referenced by the magazines in that layer appear free
 *   to the zone layer.
 *
 *
 * <h2>Magazine circulation and sizing</h2>
 *
 * The caching system sizes itself dynamically. Operations that allocate/free
 * a single element call @c zone_lock_nopreempt_check_contention() which records
 * contention on the lock by doing a trylock and recording its success.
 *
 * This information is stored in the @c z_contention_cur field of the zone,
 * and a windoed moving average is maintained in @c z_contention_wma.
 * Each time a CPU registers any contention, it will also allow its own per-cpu
 * cache to grow, incrementing @c zc_depot_max, which is how the per-cpu layer
 * might grow into using its local depot.
 *
 * Note that @c zc_depot_max assume that the (a) and (f) pre-loaded magazines
 * on average contain @c zc_mag_size() elements.
 *
 * When a per-cpu layer cannot hold more full magazines in its depot,
 * then it will overflow about 1/3 of its depot into the recirculation depot
 * (see @c zfree_cached_slow().  Conversely, when a depot is empty, then it will
 * refill its per-cpu depot to about 1/3 of its size from the recirculation
 * depot (see @c zalloc_cached_slow()).
 *
 * Lastly, the zone layer keeps track of the high and low watermark of how many
 * elements have been free per period of time (including being part of the
 * recirculation depot) in the @c z_elems_free_min and @c z_elems_free_max
 * fields. A weighted moving average of the amplitude of this is maintained in
 * the @c z_elems_free_wss which informs the zone GC on how to gently trim
 * zones without hurting performance.
 *
 *
 * <h2>Security considerations</h2>
 *
 * The zone caching layer has been designed to avoid returning elements in
 * a strict LIFO behavior: @c zalloc() will allocate from the (a) magazine,
 * and @c zfree() free to the (f) magazine, and only swap them when the
 * requested operation cannot be fulfilled.
 *
 * The per-cpu overflow depot or the recirculation depots are similarly used
 * in FIFO order.
 *
 * More importantly, when magazines flow through the recirculation depot,
 * the elements they contain are marked as "free" in the zone layer bitmaps.
 * Because allocations out of per-cpu caches verify the bitmaps at allocation
 * time, this acts as a poor man's double-free quarantine. The magazines
 * allow to avoid the cost of the bit-scanning involved in the zone-level
 * @c zalloc_item() codepath.
 *
 *
 * @field zc_alloc_cur      denormalized number of elements in the (a) magazine
 * @field zc_free_cur       denormalized number of elements in the (f) magazine
 * @field zc_alloc_elems    a pointer to the array of elements in (a)
 * @field zc_free_elems     a pointer to the array of elements in (f)
 *
 * @field zc_depot_lock     a lock to access @c zc_depot, @c zc_depot_cur.
 * @field zc_depot          a list of @c zc_depot_cur full magazines
 * @field zc_depot_cur      number of magazines in @c zc_depot
 * @field zc_depot_max      the maximum number of elements in @c zc_depot,
 *                          protected by the zone lock.
 */
typedef struct zone_cache {
	uint16_t                   zc_alloc_cur;
	uint16_t                   zc_free_cur;
	uint16_t                   zc_depot_cur;
	uint16_t                   __zc_padding;
	zone_element_t            *zc_alloc_elems;
	zone_element_t            *zc_free_elems;
	hw_lock_bit_t              zc_depot_lock;
	uint32_t                   zc_depot_max;
	struct zone_depot          zc_depot;
} *zone_cache_t;

#if !__x86_64__
static
#endif
__security_const_late struct {
	struct zone_map_range      zi_map_range[ZONE_ADDR_KIND_COUNT];
	struct zone_map_range      zi_meta_range; /* debugging only */
	struct zone_map_range      zi_bits_range; /* bits buddy allocator */
	struct zone_map_range      zi_pgz_range;
	struct zone_page_metadata *zi_pgz_meta;

	/*
	 * The metadata lives within the zi_meta_range address range.
	 *
	 * The correct formula to find a metadata index is:
	 *     absolute_page_index - page_index(MIN(zi_map_range[*].min_address))
	 *
	 * And then this index is used to dereference zi_meta_range.min_address
	 * as a `struct zone_page_metadata` array.
	 *
	 * To avoid doing that substraction all the time in the various fast-paths,
	 * zi_meta_base are pre-offset with that minimum page index to avoid redoing
	 * that math all the time.
	 *
	 * Do note that the array might have a hole punched in the middle,
	 * see zone_metadata_init().
	 */
	struct zone_page_metadata *zi_meta_base;
} zone_info;

/*
 * Initial array of metadata for stolen memory.
 *
 * The numbers here have to be kept in sync with vm_map_steal_memory()
 * so that we have reserved enough metadata.
 *
 * After zone_init() has run (which happens while the kernel is still single
 * threaded), the metadata is moved to its final dynamic location, and
 * this array is unmapped with the rest of __startup_data at lockdown.
 */
#define ZONE_FOREIGN_META_INLINE_COUNT    64
__startup_data
static struct zone_page_metadata
    zone_foreign_meta_array_startup[ZONE_FOREIGN_META_INLINE_COUNT];
__startup_data
static struct zone_map_range zone_early_steal;

/*
 *	The zone_locks_grp allows for collecting lock statistics.
 *	All locks are associated to this group in zinit.
 *	Look at tools/lockstat for debugging lock contention.
 */
static LCK_GRP_DECLARE(zone_locks_grp, "zone_locks");
static LCK_MTX_EARLY_DECLARE(zone_metadata_region_lck, &zone_locks_grp);

/*
 *	The zone metadata lock protects:
 *	- metadata faulting,
 *	- VM submap VA allocations,
 *	- early gap page queue list
 */
#define zone_meta_lock()   lck_mtx_lock(&zone_metadata_region_lck);
#define zone_meta_unlock() lck_mtx_unlock(&zone_metadata_region_lck);

/*
 *	Exclude more than one concurrent garbage collection
 */
static LCK_GRP_DECLARE(zone_gc_lck_grp, "zone_gc");
static LCK_MTX_EARLY_DECLARE(zone_gc_lock, &zone_gc_lck_grp);
static LCK_SPIN_DECLARE(zone_exhausted_lock, &zone_gc_lck_grp);

/*
 * Panic logging metadata
 */
bool panic_include_zprint = false;
bool panic_include_kalloc_types = false;
zone_t kalloc_type_src_zone = ZONE_NULL;
zone_t kalloc_type_dst_zone = ZONE_NULL;
mach_memory_info_t *panic_kext_memory_info = NULL;
vm_size_t panic_kext_memory_size = 0;
vm_offset_t panic_fault_address = 0;

/*
 *      Protects zone_array, num_zones, num_zones_in_use, and
 *      zone_destroyed_bitmap
 */
static SIMPLE_LOCK_DECLARE(all_zones_lock, 0);
static zone_id_t        num_zones_in_use;
zone_id_t _Atomic       num_zones;
SECURITY_READ_ONLY_LATE(unsigned int) zone_view_count;

/*
 * Initial globals for zone stats until we can allocate the real ones.
 * Those get migrated inside the per-CPU ones during zone_init() and
 * this array is unmapped with the rest of __startup_data at lockdown.
 */

/* zone to allocate zone_magazine structs from */
static SECURITY_READ_ONLY_LATE(zone_t) zc_magazine_zone;
/*
 * Until pid1 is made, zone caching is off,
 * until compute_zone_working_set_size() runs for the firt time.
 *
 * -1 represents the "never enabled yet" value.
 */
static int8_t zone_caching_disabled = -1;

__startup_data
static struct zone_cache zone_cache_startup[MAX_ZONES];
__startup_data
static struct zone_stats zone_stats_startup[MAX_ZONES];
struct zone              zone_array[MAX_ZONES];
SECURITY_READ_ONLY_LATE(zone_security_flags_t) zone_security_array[MAX_ZONES] = {
	[0 ... MAX_ZONES - 1] = {
		.z_allows_foreign = false,
		.z_kheap_id       = KHEAP_ID_NONE,
		.z_noencrypt      = false,
		.z_submap_idx     = Z_SUBMAP_IDX_GENERAL_0,
		.z_kalloc_type    = false,
		.z_va_sequester   = ZSECURITY_CONFIG(SEQUESTER),
	},
};
SECURITY_READ_ONLY_LATE(uint16_t) zone_ro_elem_size[MAX_ZONES];

/* Initialized in zone_bootstrap(), how many "copies" the per-cpu system does */
static SECURITY_READ_ONLY_LATE(unsigned) zpercpu_early_count;

/* Used to keep track of destroyed slots in the zone_array */
static bitmap_t zone_destroyed_bitmap[BITMAP_LEN(MAX_ZONES)];

/* number of zone mapped pages used by all zones */
static size_t _Atomic zone_pages_wired;
static size_t _Atomic zone_pages_jetsam_threshold = ~0;

#if CONFIG_PROB_GZALLOC
static int32_t _Atomic zone_guard_pages;
#endif /* CONFIG_PROB_GZALLOC */

#define ZSECURITY_DEFAULT ( \
	ZSECURITY_OPTIONS_KERNEL_DATA_MAP | \
	0)
TUNABLE(zone_security_options_t, zsecurity_options, "zs", ZSECURITY_DEFAULT);

/* Time in (ms) after which we panic for zone exhaustions */
TUNABLE(int, zone_exhausted_timeout, "zet", 5000);

#if VM_TAG_SIZECLASSES
/* enable tags for zones that ask for it */
static TUNABLE(bool, zone_tagging_on, "-zt", false);
#endif /* VM_TAG_SIZECLASSES */

#if DEBUG || DEVELOPMENT
static int zalloc_simulate_vm_pressure;
TUNABLE(bool, zalloc_disable_copyio_check, "-no-copyio-zalloc-check", false);
#endif /* DEBUG || DEVELOPMENT */

/*
 * Zone caching tunables
 *
 * zc_mag_size():
 *   size of magazines, larger to reduce contention at the expense of memory
 *
 * zc_auto_enable_threshold
 *   number of contentions per second after which zone caching engages
 *   automatically.
 *
 *   0 to disable.
 *
 * zc_grow_threshold
 *   numer of contentions per second after which the per-cpu depot layer
 *   grows at each newly observed contention without restriction.
 *
 *   0 to disable.
 *
 * zc_recirc_denom
 *   denominator of the fraction of per-cpu depot to migrate to/from
 *   the recirculation depot layer at a time. Default 3 (1/3).
 *
 * zc_defrag_ratio
 *   percentage of the working set to recirc size below which
 *   the zone is defragmented. Default is 66%.
 *
 * zc_defrag_threshold
 *   how much memory needs to be free before the auto-defrag is even considered.
 *   Default is 512k.
 *
 * zc_autogc_ratio
 *   percentage of the working set to min-free size below which
 *   the zone is auto-GCed to the working set size. Default is 20%.
 *
 * zc_autogc_threshold
 *   how much memory needs to be free before the auto-gc is even considered.
 *   Default is 4M.
 *
 * zc_free_batch_size
 *   The size of batches of frees/reclaim that can be done keeping
 *   the zone lock held (and preemption disabled).
 */
static TUNABLE(uint16_t, zc_magazine_size, "zc_mag_size", 8);
static TUNABLE(uint32_t, zc_auto_threshold, "zc_auto_enable_threshold", 20);
static TUNABLE(uint32_t, zc_grow_threshold, "zc_grow_threshold", 8);
static TUNABLE(uint32_t, zc_recirc_denom, "zc_recirc_denom", 3);
static TUNABLE(uint32_t, zc_defrag_ratio, "zc_defrag_ratio", 66);
static TUNABLE(uint32_t, zc_defrag_threshold, "zc_defrag_threshold", 512u << 10);
static TUNABLE(uint32_t, zc_autogc_ratio, "zc_autogc_ratio", 20);
static TUNABLE(uint32_t, zc_autogc_threshold, "zc_autogc_threshold", 4u << 20);
static TUNABLE(uint32_t, zc_free_batch_size, "zc_free_batch_size", 256);

static SECURITY_READ_ONLY_LATE(size_t)    zone_pages_wired_max;
static SECURITY_READ_ONLY_LATE(vm_map_t)  zone_submaps[Z_SUBMAP_IDX_COUNT];
static SECURITY_READ_ONLY_LATE(vm_map_t)  zone_meta_submaps[2];
static char const * const zone_submaps_names[Z_SUBMAP_IDX_COUNT] = {
	[Z_SUBMAP_IDX_VM]               = "VM",
	[Z_SUBMAP_IDX_READ_ONLY]        = "RO",
#if ZSECURITY_CONFIG(SAD_FENG_SHUI)
	[Z_SUBMAP_IDX_GENERAL_0]        = "GEN0",
	[Z_SUBMAP_IDX_GENERAL_1]        = "GEN1",
	[Z_SUBMAP_IDX_GENERAL_2]        = "GEN2",
	[Z_SUBMAP_IDX_GENERAL_3]        = "GEN3",
#else
	[Z_SUBMAP_IDX_GENERAL_0]        = "GEN",
#endif /* ZSECURITY_CONFIG(SAD_FENG_SHUI) */
	[Z_SUBMAP_IDX_DATA]             = "DATA",
};

#if __x86_64__
#define ZONE_ENTROPY_CNT 8
#else
#define ZONE_ENTROPY_CNT 2
#endif
static struct zone_bool_gen {
	struct bool_gen zbg_bg;
	uint32_t zbg_entropy[ZONE_ENTROPY_CNT];
} zone_bool_gen[MAX_CPUS];

#if CONFIG_PROB_GZALLOC
/*
 * Probabilistic gzalloc
 * =====================
 *
 *
 * Probabilistic guard zalloc samples allocations and will protect them by
 * double-mapping the page holding them and returning the secondary virtual
 * address to its callers.
 *
 * Its data structures are lazily allocated if the `pgz` or `pgz1` boot-args
 * are set.
 *
 *
 * Unlike GZalloc, PGZ uses a fixed amount of memory, and is compatible with
 * most zalloc/kalloc features:
 * - zone_require is functional
 * - zone caching or zone tagging is compatible
 * - non-blocking allocation work (they will always return NULL with gzalloc).
 *
 * PGZ limitations:
 * - VA sequestering isn't respected, as the slots (which are in limited
 *   quantity) will be reused for any type, however the PGZ quarantine
 *   somewhat mitigates the impact.
 * - zones with elements larger than a page cannot be protected.
 *
 *
 * Tunables:
 * --------
 *
 * pgz=1:
 *   Turn on probabilistic guard malloc for all zones
 *
 *   (default on for DEVELOPMENT, off for RELEASE, or if pgz1... are specified)
 *
 * pgz_sample_rate=0 to 2^31
 *   average sample rate between two guarded allocations.
 *   0 means every allocation.
 *
 *   The default is a random number between 1000 and 10,000
 *
 * pgz_slots
 *   how many allocations to protect.
 *
 *   Each costs:
 *   - a PTE in the pmap (when allocated)
 *   - 2 zone page meta's (every other page is a "guard" one, 32B total)
 *   - 64 bytes per backtraces.
 *   On LP64 this is <16K per 100 slots.
 *
 *   The default is ~200 slots per G of physical ram (32k / G)
 *
 *   TODO:
 *   - try harder to allocate elements at the "end" to catch OOB more reliably.
 *
 * pgz_quarantine
 *   how many slots should be free at any given time.
 *
 *   PGZ will round robin through free slots to be reused, but free slots are
 *   important to detect use-after-free by acting as a quarantine.
 *
 *   By default, PGZ will keep 33% of the slots around at all time.
 *
 * pgz1=<name>, pgz2=<name>, ..., pgzn=<name>...
 *   Specific zones for which to enable probabilistic guard malloc.
 *   There must be no numbering gap (names after the gap will be ignored).
 */
#if DEBUG || DEVELOPMENT
static TUNABLE(bool, pgz_all, "pgz", true);
#else
static TUNABLE(bool, pgz_all, "pgz", false);
#endif
static TUNABLE(uint32_t, pgz_sample_rate, "pgz_sample_rate", 0);
static TUNABLE(uint32_t, pgz_slots, "pgz_slots", UINT32_MAX);
static TUNABLE(uint32_t, pgz_quarantine, "pgz_quarantine", 0);
#endif /* CONFIG_PROB_GZALLOC */

static zone_t zone_find_largest(uint64_t *zone_size);

#endif /* !ZALLOC_TEST */
#pragma mark Zone metadata
#if !ZALLOC_TEST

static inline bool
zone_has_index(zone_t z, zone_id_t zid)
{
	return zone_array + zid == z;
}

static zone_element_t
zone_element_encode(vm_offset_t base, vm_offset_t eidx)
{
	return (zone_element_t){ .ze_value = base | eidx };
}

static vm_offset_t
zone_element_base(zone_element_t ze)
{
	return trunc_page(ze.ze_value);
}

static vm_offset_t
zone_element_idx(zone_element_t ze)
{
	return ze.ze_value & PAGE_MASK;
}

static vm_offset_t
zone_element_addr(zone_t z, zone_element_t ze, vm_offset_t esize)
{
	vm_offset_t offs = zone_oob_offs(z);

	return offs + zone_element_base(ze) + esize * zone_element_idx(ze);
}

__abortlike
void
zone_invalid_panic(zone_t zone)
{
	panic("zone %p isn't in the zone_array", zone);
}

__abortlike
static void
zone_metadata_corruption(zone_t zone, struct zone_page_metadata *meta,
    const char *kind)
{
	panic("zone metadata corruption: %s (meta %p, zone %s%s)",
	    kind, meta, zone_heap_name(zone), zone->z_name);
}

__abortlike
static void
zone_invalid_element_addr_panic(zone_t zone, vm_offset_t addr)
{
	panic("zone element pointer validation failed (addr: %p, zone %s%s)",
	    (void *)addr, zone_heap_name(zone), zone->z_name);
}

__abortlike
static void
zone_page_metadata_index_confusion_panic(zone_t zone, vm_offset_t addr,
    struct zone_page_metadata *meta)
{
	zone_security_flags_t zsflags = zone_security_config(zone), src_zsflags;
	zone_id_t zidx;
	zone_t src_zone;

	if (zsflags.z_kalloc_type) {
		panic_include_kalloc_types = true;
		kalloc_type_dst_zone = zone;
	}

	zidx = meta->zm_index;
	if (zidx >= os_atomic_load(&num_zones, relaxed)) {
		panic("%p expected in zone %s%s[%d], but metadata has invalid zidx: %d",
		    (void *)addr, zone_heap_name(zone), zone->z_name, zone_index(zone),
		    zidx);
	}

	src_zone = &zone_array[zidx];
	src_zsflags = zone_security_array[zidx];
	if (src_zsflags.z_kalloc_type) {
		panic_include_kalloc_types = true;
		kalloc_type_src_zone = src_zone;
	}

	panic("%p not in the expected zone %s%s[%d], but found in %s%s[%d]",
	    (void *)addr, zone_heap_name(zone), zone->z_name, zone_index(zone),
	    zone_heap_name(src_zone), src_zone->z_name, zidx);
}

__abortlike
static void
zone_page_metadata_native_queue_corruption(zone_t zone, zone_pva_t *queue)
{
	panic("foreign metadata index %d enqueued in native head %p from zone %s%s",
	    queue->packed_address, queue, zone_heap_name(zone),
	    zone->z_name);
}

__abortlike
static void
zone_page_metadata_list_corruption(zone_t zone, struct zone_page_metadata *meta)
{
	panic("metadata list corruption through element %p detected in zone %s%s",
	    meta, zone_heap_name(zone), zone->z_name);
}

__abortlike __unused
static void
zone_invalid_foreign_addr_panic(zone_t zone, vm_offset_t addr)
{
	panic("addr %p being freed to foreign zone %s%s not from foreign range",
	    (void *)addr, zone_heap_name(zone), zone->z_name);
}

__abortlike
static void
zone_page_meta_accounting_panic(zone_t zone, struct zone_page_metadata *meta,
    const char *kind)
{
	panic("accounting mismatch (%s) for zone %s%s, meta %p", kind,
	    zone_heap_name(zone), zone->z_name, meta);
}

__abortlike
static void
zone_meta_double_free_panic(zone_t zone, zone_element_t ze, const char *caller)
{
	panic("%s: double free of %p to zone %s%s", caller,
	    (void *)zone_element_addr(zone, ze, zone_elem_size(zone)),
	    zone_heap_name(zone), zone->z_name);
}

__abortlike
static void
zone_accounting_panic(zone_t zone, const char *kind)
{
	panic("accounting mismatch (%s) for zone %s%s", kind,
	    zone_heap_name(zone), zone->z_name);
}

#define zone_counter_sub(z, stat, value)  ({ \
	if (os_sub_overflow((z)->stat, value, &(z)->stat)) { \
	    zone_accounting_panic(z, #stat " wrap-around"); \
	} \
	(z)->stat; \
})

static inline void
zone_elems_free_add(zone_t z, uint32_t count)
{
	uint32_t n = (z->z_elems_free += count);
	if (z->z_elems_free_max < n) {
		z->z_elems_free_max = n;
	}
}

static inline void
zone_elems_free_sub(zone_t z, uint32_t count)
{
	uint32_t n = zone_counter_sub(z, z_elems_free, count);

	if (z->z_elems_free_min > n) {
		z->z_elems_free_min = n;
	}
}

static inline uint16_t
zone_meta_alloc_size_add(zone_t z, struct zone_page_metadata *m,
    vm_offset_t esize)
{
	if (os_add_overflow(m->zm_alloc_size, (uint16_t)esize, &m->zm_alloc_size)) {
		zone_page_meta_accounting_panic(z, m, "alloc_size wrap-around");
	}
	return m->zm_alloc_size;
}

static inline uint16_t
zone_meta_alloc_size_sub(zone_t z, struct zone_page_metadata *m,
    vm_offset_t esize)
{
	if (os_sub_overflow(m->zm_alloc_size, esize, &m->zm_alloc_size)) {
		zone_page_meta_accounting_panic(z, m, "alloc_size wrap-around");
	}
	return m->zm_alloc_size;
}

__abortlike
static void
zone_nofail_panic(zone_t zone)
{
	panic("zalloc(Z_NOFAIL) can't be satisfied for zone %s%s (potential leak)",
	    zone_heap_name(zone), zone->z_name);
}

#if __arm64__
// <rdar://problem/48304934> arm64 doesn't use ldp when I'd expect it to
#define zone_range_load(r, rmin, rmax) \
	asm("ldp %[rmin], %[rmax], [%[range]]" \
	    : [rmin] "=r"(rmin), [rmax] "=r"(rmax) \
	    : [range] "r"(r), "m"((r)->min_address), "m"((r)->max_address))
#else
#define zone_range_load(r, rmin, rmax) \
	({ rmin = (r)->min_address; rmax = (r)->max_address; })
#endif

__attribute__((overloadable))
__header_always_inline bool
zone_range_contains(const struct zone_map_range *r, vm_offset_t addr)
{
	vm_offset_t rmin, rmax;

#if CONFIG_KERNEL_TBI
	addr = VM_KERNEL_TBI_FILL(addr);
#endif /* CONFIG_KERNEL_TBI */

	/*
	 * The `&` is not a typo: we really expect the check to pass,
	 * so encourage the compiler to eagerly load and test without branches
	 */
	zone_range_load(r, rmin, rmax);
	return (addr >= rmin) & (addr < rmax);
}

__attribute__((overloadable))
__header_always_inline bool
zone_range_contains(const struct zone_map_range *r, vm_offset_t addr, vm_offset_t size)
{
	vm_offset_t rmin, rmax;

#if CONFIG_KERNEL_TBI
	addr = VM_KERNEL_TBI_FILL(addr);
#endif /* CONFIG_KERNEL_TBI */

	/*
	 * The `&` is not a typo: we really expect the check to pass,
	 * so encourage the compiler to eagerly load and test without branches
	 */
	zone_range_load(r, rmin, rmax);
	return (addr >= rmin) & (addr + size >= rmin) & (addr + size <= rmax);
}

__header_always_inline bool
zone_spans_ro_va(vm_offset_t addr_start, vm_offset_t addr_end)
{
	vm_offset_t rmin, rmax;

#if CONFIG_KERNEL_TBI
	addr_start = VM_KERNEL_STRIP_UPTR(addr_start);
	addr_end = VM_KERNEL_STRIP_UPTR(addr_end);
#endif /* CONFIG_KERNEL_TBI */

	zone_range_load(&zone_info.zi_map_range[ZONE_ADDR_READONLY], rmin, rmax);

	/*
	 * Either the start and the end are leftward of the read-only range, or they
	 * are both completely rightward. If neither, then they span over the range.
	 */

	if ((addr_start < rmin) && (addr_end < rmin)) {
		/* Leftward */
		return false;
	} else if ((addr_start > rmax) && (addr_end > rmax)) {
		/* Rightward */
		return false;
	}

	return true;
}

__header_always_inline vm_size_t
zone_range_size(const struct zone_map_range *r)
{
	vm_offset_t rmin, rmax;

	zone_range_load(r, rmin, rmax);
	return rmax - rmin;
}

#define from_zone_map(addr, size, kind) \
	__builtin_choose_expr(__builtin_constant_p(size) ? (size) == 1 : 0, \
	zone_range_contains(&zone_info.zi_map_range[kind], (vm_offset_t)(addr)), \
	zone_range_contains(&zone_info.zi_map_range[kind], \
	    (vm_offset_t)(addr), size))

#define zone_native_size() \
	zone_range_size(&zone_info.zi_map_range[ZONE_ADDR_NATIVE])

#define zone_foreign_size() \
	zone_range_size(&zone_info.zi_map_range[ZONE_ADDR_FOREIGN])

#define zone_readonly_size() \
	zone_range_size(&zone_info.zi_map_range[ZONE_ADDR_READONLY])

__header_always_inline bool
zone_pva_is_null(zone_pva_t page)
{
	return page.packed_address == 0;
}

__header_always_inline bool
zone_pva_is_queue(zone_pva_t page)
{
	// actual kernel pages have the top bit set
	return (int32_t)page.packed_address > 0;
}

__header_always_inline bool
zone_pva_is_equal(zone_pva_t pva1, zone_pva_t pva2)
{
	return pva1.packed_address == pva2.packed_address;
}

__header_always_inline zone_pva_t *
zone_pageq_base(void)
{
	extern zone_pva_t data_seg_start[] __SEGMENT_START_SYM("__DATA");

	/*
	 * `-1` so that if the first __DATA variable is a page queue,
	 * it gets a non 0 index
	 */
	return data_seg_start - 1;
}

__header_always_inline void
zone_queue_set_head(zone_t z, zone_pva_t queue, zone_pva_t oldv,
    struct zone_page_metadata *meta)
{
	zone_pva_t *queue_head = &zone_pageq_base()[queue.packed_address];

	if (!zone_pva_is_equal(*queue_head, oldv)) {
		zone_page_metadata_list_corruption(z, meta);
	}
	*queue_head = meta->zm_page_next;
}

__header_always_inline zone_pva_t
zone_queue_encode(zone_pva_t *headp)
{
	return (zone_pva_t){ (uint32_t)(headp - zone_pageq_base()) };
}

__header_always_inline zone_pva_t
zone_pva_from_addr(vm_address_t addr)
{
	// cannot use atop() because we want to maintain the sign bit
	return (zone_pva_t){ (uint32_t)((intptr_t)addr >> PAGE_SHIFT) };
}

__header_always_inline zone_pva_t
zone_pva_from_element(zone_element_t ze)
{
	return zone_pva_from_addr(ze.ze_value);
}

__header_always_inline vm_address_t
zone_pva_to_addr(zone_pva_t page)
{
	// cause sign extension so that we end up with the right address
	return (vm_offset_t)(int32_t)page.packed_address << PAGE_SHIFT;
}

__header_always_inline struct zone_page_metadata *
zone_pva_to_meta(zone_pva_t page)
{
	return &zone_info.zi_meta_base[page.packed_address];
}

__header_always_inline zone_pva_t
zone_pva_from_meta(struct zone_page_metadata *meta)
{
	return (zone_pva_t){ (uint32_t)(meta - zone_info.zi_meta_base) };
}

__header_always_inline struct zone_page_metadata *
zone_meta_from_addr(vm_offset_t addr)
{
	return zone_pva_to_meta(zone_pva_from_addr(addr));
}

__header_always_inline struct zone_page_metadata *
zone_meta_from_element(zone_element_t ze)
{
	return zone_pva_to_meta(zone_pva_from_element(ze));
}

__header_always_inline zone_id_t
zone_index_from_ptr(const void *ptr)
{
	return zone_pva_to_meta(zone_pva_from_addr((vm_offset_t)ptr))->zm_index;
}

__header_always_inline vm_offset_t
zone_meta_to_addr(struct zone_page_metadata *meta)
{
	return ptoa((int32_t)(meta - zone_info.zi_meta_base));
}

__header_always_inline void
zone_meta_queue_push(zone_t z, zone_pva_t *headp,
    struct zone_page_metadata *meta)
{
	zone_pva_t head = *headp;
	zone_pva_t queue_pva = zone_queue_encode(headp);
	struct zone_page_metadata *tmp;

	meta->zm_page_next = head;
	if (!zone_pva_is_null(head)) {
		tmp = zone_pva_to_meta(head);
		if (!zone_pva_is_equal(tmp->zm_page_prev, queue_pva)) {
			zone_page_metadata_list_corruption(z, meta);
		}
		tmp->zm_page_prev = zone_pva_from_meta(meta);
	}
	meta->zm_page_prev = queue_pva;
	*headp = zone_pva_from_meta(meta);
}

__header_always_inline struct zone_page_metadata *
zone_meta_queue_pop_native(zone_t z, zone_pva_t *headp, vm_offset_t *page_addrp)
{
	zone_pva_t head = *headp;
	struct zone_page_metadata *meta = zone_pva_to_meta(head);
	vm_offset_t page_addr = zone_pva_to_addr(head);
	struct zone_page_metadata *tmp;

	if (!from_zone_map(page_addr, 1, ZONE_ADDR_NATIVE)) {
		zone_page_metadata_native_queue_corruption(z, headp);
	}

	if (!zone_pva_is_null(meta->zm_page_next)) {
		tmp = zone_pva_to_meta(meta->zm_page_next);
		if (!zone_pva_is_equal(tmp->zm_page_prev, head)) {
			zone_page_metadata_list_corruption(z, meta);
		}
		tmp->zm_page_prev = meta->zm_page_prev;
	}
	*headp = meta->zm_page_next;

	meta->zm_page_next = meta->zm_page_prev = (zone_pva_t){ 0 };
	*page_addrp = page_addr;

	if (!zone_has_index(z, meta->zm_index)) {
		zone_page_metadata_index_confusion_panic(z,
		    zone_meta_to_addr(meta), meta);
	}
	return meta;
}

__header_always_inline void
zone_meta_remqueue(zone_t z, struct zone_page_metadata *meta)
{
	zone_pva_t meta_pva = zone_pva_from_meta(meta);
	struct zone_page_metadata *tmp;

	if (!zone_pva_is_null(meta->zm_page_next)) {
		tmp = zone_pva_to_meta(meta->zm_page_next);
		if (!zone_pva_is_equal(tmp->zm_page_prev, meta_pva)) {
			zone_page_metadata_list_corruption(z, meta);
		}
		tmp->zm_page_prev = meta->zm_page_prev;
	}
	if (zone_pva_is_queue(meta->zm_page_prev)) {
		zone_queue_set_head(z, meta->zm_page_prev, meta_pva, meta);
	} else {
		tmp = zone_pva_to_meta(meta->zm_page_prev);
		if (!zone_pva_is_equal(tmp->zm_page_next, meta_pva)) {
			zone_page_metadata_list_corruption(z, meta);
		}
		tmp->zm_page_next = meta->zm_page_next;
	}

	meta->zm_page_next = meta->zm_page_prev = (zone_pva_t){ 0 };
}

__header_always_inline void
zone_meta_requeue(zone_t z, zone_pva_t *headp,
    struct zone_page_metadata *meta)
{
	zone_meta_remqueue(z, meta);
	zone_meta_queue_push(z, headp, meta);
}

/* prevents a given metadata from ever reaching the z_pageq_empty queue */
static inline void
zone_meta_lock_in_partial(zone_t z, struct zone_page_metadata *m, uint32_t len)
{
	uint16_t new_size = zone_meta_alloc_size_add(z, m, ZM_ALLOC_SIZE_LOCK);

	assert(new_size % sizeof(vm_offset_t) == ZM_ALLOC_SIZE_LOCK);
	if (new_size == ZM_ALLOC_SIZE_LOCK) {
		zone_meta_requeue(z, &z->z_pageq_partial, m);
		zone_counter_sub(z, z_wired_empty, len);
	}
}

/* allows a given metadata to reach the z_pageq_empty queue again */
static inline void
zone_meta_unlock_from_partial(zone_t z, struct zone_page_metadata *m, uint32_t len)
{
	uint16_t new_size = zone_meta_alloc_size_sub(z, m, ZM_ALLOC_SIZE_LOCK);

	assert(new_size % sizeof(vm_offset_t) == 0);
	if (new_size == 0) {
		zone_meta_requeue(z, &z->z_pageq_empty, m);
		z->z_wired_empty += len;
	}
}

/*
 * Routine to populate a page backing metadata in the zone_metadata_region.
 * Must be called without the zone lock held as it might potentially block.
 */
static void
zone_meta_populate(vm_offset_t base, vm_size_t size)
{
	struct zone_page_metadata *from = zone_meta_from_addr(base);
	struct zone_page_metadata *to   = from + atop(size);
	vm_offset_t page_addr = trunc_page(from);
	vm_map_t map;

	if (page_addr < zone_meta_submaps[0]->max_offset) {
		map = zone_meta_submaps[0];
	} else {
		map = zone_meta_submaps[1];
	}

	for (; page_addr < (vm_offset_t)to; page_addr += PAGE_SIZE) {
#if !KASAN_ZALLOC
		/*
		 * This can race with another thread doing a populate on the same metadata
		 * page, where we see an updated pmap but unmapped KASan shadow, causing a
		 * fault in the shadow when we first access the metadata page. Avoid this
		 * by always synchronizing on the zone_metadata_region lock with KASan.
		 */
		if (pmap_find_phys(kernel_pmap, page_addr)) {
			continue;
		}
#endif

		for (;;) {
			kern_return_t ret = KERN_SUCCESS;

			/*
			 * All updates to the zone_metadata_region are done
			 * under the zone_metadata_region_lck
			 */
			zone_meta_lock();
			if (0 == pmap_find_phys(kernel_pmap, page_addr)) {
				ret = kernel_memory_populate(map, page_addr,
				    PAGE_SIZE, KMA_NOPAGEWAIT | KMA_KOBJECT | KMA_ZERO,
				    VM_KERN_MEMORY_OSFMK);
			}
			zone_meta_unlock();

			if (ret == KERN_SUCCESS) {
				break;
			}

			/*
			 * We can't pass KMA_NOPAGEWAIT under a global lock as it leads
			 * to bad system deadlocks, so if the allocation failed,
			 * we need to do the VM_PAGE_WAIT() outside of the lock.
			 */
			VM_PAGE_WAIT();
		}
	}
}

__abortlike
static void
zone_invalid_element_panic(zone_t zone, vm_offset_t addr, bool cache)
{
	struct zone_page_metadata *meta;
	vm_offset_t page, esize = zone_elem_size(zone);
	const char *from_cache = "";

	if (cache) {
		zone_element_t ze = { .ze_value = addr };
		addr = zone_element_addr(zone, ze, esize);
		from_cache = " (from cache)";

		if (zone_element_idx(ze) >= zone->z_chunk_elems) {
			panic("eidx %d for addr %p being freed to zone %s%s, is larger "
			    "than number fo element in chunk (%d)", (int)zone_element_idx(ze),
			    (void *)addr, zone_heap_name(zone), zone->z_name,
			    zone->z_chunk_elems);
		}
	}

	if (!from_zone_map(addr, esize, ZONE_ADDR_NATIVE) &&
	    !from_zone_map(addr, esize, ZONE_ADDR_FOREIGN)) {
		panic("addr %p being freed to zone %s%s%s, isn't from zone map",
		    (void *)addr, zone_heap_name(zone), zone->z_name, from_cache);
	}
	page = trunc_page(addr);
	meta = zone_meta_from_addr(addr);

	if (meta->zm_chunk_len == ZM_SECONDARY_PCPU_PAGE) {
		panic("metadata %p corresponding to addr %p being freed to "
		    "zone %s%s%s, is marked as secondary per cpu page",
		    meta, (void *)addr, zone_heap_name(zone), zone->z_name,
		    from_cache);
	}
	if (meta->zm_chunk_len > ZM_CHUNK_LEN_MAX) {
		panic("metadata %p corresponding to addr %p being freed to "
		    "zone %s%s%s, has chunk len greater than max",
		    meta, (void *)addr, zone_heap_name(zone), zone->z_name,
		    from_cache);
	}

	if (meta->zm_chunk_len == ZM_SECONDARY_PAGE) {
		page -= ptoa(meta->zm_page_index);
	}

	if ((addr - page - zone_oob_offs(zone)) % esize) {
		panic("addr %p being freed to zone %s%s%s, isn't aligned to "
		    "zone element size", (void *)addr, zone_heap_name(zone),
		    zone->z_name, from_cache);
	}

	zone_invalid_element_addr_panic(zone, addr);
}

__header_always_inline
struct zone_page_metadata *
zone_element_validate(zone_t zone, zone_element_t ze)
{
	struct zone_page_metadata *meta;
	vm_offset_t page = zone_element_base(ze);
	vm_offset_t addr;

	if (!from_zone_map(page, 1, ZONE_ADDR_NATIVE) &&
	    !from_zone_map(page, 1, ZONE_ADDR_FOREIGN)) {
		zone_invalid_element_panic(zone, ze.ze_value, true);
	}
	meta = zone_meta_from_addr(page);

	if (meta->zm_chunk_len > ZM_CHUNK_LEN_MAX) {
		zone_invalid_element_panic(zone, ze.ze_value, true);
	}
	if (zone_element_idx(ze) >= zone->z_chunk_elems) {
		zone_invalid_element_panic(zone, ze.ze_value, true);
	}

	if (!zone_has_index(zone, meta->zm_index)) {
		addr = zone_element_addr(zone, ze, zone_elem_size(zone));
		zone_page_metadata_index_confusion_panic(zone, addr, meta);
	}

	return meta;
}

__attribute__((always_inline))
static struct zone_page_metadata *
zone_element_resolve(zone_t zone, vm_offset_t addr, vm_offset_t esize,
    zone_element_t *ze)
{
	struct zone_page_metadata *meta;
	vm_offset_t offs = zone_oob_offs(zone);
	vm_offset_t page, eidx;

	if (!from_zone_map(addr, esize, ZONE_ADDR_NATIVE) &&
	    !from_zone_map(addr, esize, ZONE_ADDR_FOREIGN)) {
		zone_invalid_element_panic(zone, addr, false);
	}
	page = trunc_page(addr);
	meta = zone_meta_from_addr(addr);

	if (meta->zm_chunk_len == ZM_SECONDARY_PCPU_PAGE) {
		zone_invalid_element_panic(zone, addr, false);
	}
	if (meta->zm_chunk_len == ZM_SECONDARY_PAGE) {
		page -= ptoa(meta->zm_page_index);
		meta -= meta->zm_page_index;
	}

	eidx = (addr - page - offs) / esize;
	if ((addr - page - offs) % esize) {
		zone_invalid_element_panic(zone, addr, false);
	}

	if (!zone_has_index(zone, meta->zm_index)) {
		zone_page_metadata_index_confusion_panic(zone, addr, meta);
	}

	*ze = zone_element_encode(page, eidx);
	return meta;
}

#if CONFIG_PROB_GZALLOC
void *
zone_element_pgz_oob_adjust(void *elem, vm_size_t esize, vm_size_t req_size)
{
	struct zone_page_metadata *meta;
	vm_offset_t addr = (vm_offset_t)elem;
	vm_offset_t offs_max = PAGE_SIZE - (1u << KALLOC_LOG2_MINALIGN);
	vm_offset_t offs;

	if (esize - req_size >= offs_max) {
		/*
		 * The code in zone_element_size() is trying finding
		 * the guard page for the chunk using <addr + element_size>
		 * and this needs to hit the guard page which holds
		 * the metadata for the offset we applied.
		 *
		 * For this to work, when "esize" is larger than PAGE_SIZE,
		 * it is important that the offset is sub-page, else we will
		 * jump over the guard page and not find an offset.
		 *
		 * TOOD: find a scheme to make this actually work for arbitrary
		 *       offsets without costing us having to fully resolve
		 *       the item.
		 */
		offs = offs_max;
	} else if (req_size <= (1u << KALLOC_LOG2_MINALIGN)) {
		offs = esize - (1u << KALLOC_LOG2_MINALIGN);
	} else {
		offs = esize - roundup(req_size, 1u << KALLOC_LOG2_MINALIGN);
	}

	meta = zone_meta_from_addr(addr + esize);
	meta->zm_oob_offs = (uint16_t)offs;
	return (void *)(addr + offs);
}
#endif /* CONFIG_PROB_GZALLOC */

/*
 * Routine to get the size of a zone allocated address.
 * If the address doesnt belong to the zone maps, returns 0.
 */
vm_size_t
zone_element_size(void *elem, zone_t *z, bool clear_oob, vm_offset_t *oob_offs)
{
	vm_address_t addr = (vm_address_t)elem;
	struct zone_page_metadata *meta, *next;
	vm_size_t esize, offs;
	zone_t zone;

	if (from_zone_map(addr, sizeof(void *), ZONE_ADDR_NATIVE) ||
	    from_zone_map(addr, sizeof(void *), ZONE_ADDR_FOREIGN)) {
		meta  = zone_meta_from_addr(addr);
		zone  = &zone_array[meta->zm_index];
		esize = zone_elem_size_safe(zone);
		next  = zone_meta_from_addr(addr + esize + 1);
		offs  = 0;

#if CONFIG_PROB_GZALLOC
		/*
		 * If the element is crossing a page and the metadata
		 * for the next page is a guard, it very likely means
		 * that it was allocated by kalloc_ext() and
		 * zone_element_pgz_oob_adjust() was used.
		 *
		 * Adjust the element back with the info stashed
		 * in that guard page metadata.
		 *
		 * Note: if clear_oob isn't set, then `elem` might
		 * be a pointer inside the element, which makes
		 * dereferencing next->zm_chunk_len unsafe
		 * for large elements. This makes copyio checks less potent.
		 */
		if (oob_offs && zone->z_pgz_use_guards &&
		    (clear_oob || esize <= PAGE_SIZE) &&
		    next->zm_chunk_len == ZM_PGZ_GUARD) {
			offs = next->zm_oob_offs;
			if (clear_oob) {
				next->zm_oob_offs = 0;
			}
		}
#else
		(void)clear_oob;
#endif /* CONFIG_PROB_GZALLOC */
		if (oob_offs) {
			*oob_offs = offs;
		}
		if (z) {
			*z = zone;
		}
		return esize;
	}

	if (oob_offs) {
		*oob_offs = 0;
	}
#if CONFIG_GZALLOC
	if (__improbable(gzalloc_enabled())) {
		vm_size_t gzsize;
		if (gzalloc_element_size(elem, z, &gzsize)) {
			return gzsize;
		}
	}
#endif /* CONFIG_GZALLOC */

	return 0;
}

zone_id_t
zone_id_for_native_element(void *addr, vm_size_t esize)
{
	zone_id_t zid = ZONE_ID_INVALID;
	if (from_zone_map(addr, esize, ZONE_ADDR_NATIVE)) {
		zid = zone_index_from_ptr(addr);
		__builtin_assume(zid != ZONE_ID_INVALID);
	}
	return zid;
}

/* This function just formats the reason for the panics by redoing the checks */
__abortlike
static void
zone_require_panic(zone_t zone, void *addr)
{
	uint32_t zindex;
	zone_t other;

	if (!from_zone_map(addr, zone_elem_size(zone), ZONE_ADDR_NATIVE)) {
		panic("zone_require failed: address not in a zone (addr: %p)", addr);
	}

	zindex = zone_index_from_ptr(addr);
	other = &zone_array[zindex];
	if (zindex >= os_atomic_load(&num_zones, relaxed) || !other->z_self) {
		panic("zone_require failed: invalid zone index %d "
		    "(addr: %p, expected: %s%s)", zindex,
		    addr, zone_heap_name(zone), zone->z_name);
	} else {
		panic("zone_require failed: address in unexpected zone id %d (%s%s) "
		    "(addr: %p, expected: %s%s)",
		    zindex, zone_heap_name(other), other->z_name,
		    addr, zone_heap_name(zone), zone->z_name);
	}
}

__abortlike
static void
zone_id_require_panic(zone_id_t zid, void *addr)
{
	zone_require_panic(&zone_array[zid], addr);
}

/*
 * Routines to panic if a pointer is not mapped to an expected zone.
 * This can be used as a means of pinning an object to the zone it is expected
 * to be a part of.  Causes a panic if the address does not belong to any
 * specified zone, does not belong to any zone, has been freed and therefore
 * unmapped from the zone, or the pointer contains an uninitialized value that
 * does not belong to any zone.
 *
 * Note that this can only work with collectable zones without foreign pages.
 */
void
zone_require(zone_t zone, void *addr)
{
	vm_size_t esize = zone_elem_size(zone);

	if (__probable(from_zone_map(addr, esize, ZONE_ADDR_NATIVE))) {
		if (zone_has_index(zone, zone_index_from_ptr(addr))) {
			return;
		}
#if CONFIG_GZALLOC
	} else if (__probable(zone->z_gzalloc_tracked)) {
		return;
#endif
	}
	zone_require_panic(zone, addr);
}

void
zone_id_require(zone_id_t zid, vm_size_t esize, void *addr)
{
	if (__probable(from_zone_map(addr, esize, ZONE_ADDR_NATIVE))) {
		if (zid == zone_index_from_ptr(addr)) {
			return;
		}
#if CONFIG_GZALLOC
	} else if (__probable(zone_array[zid].z_gzalloc_tracked)) {
		return;
#endif
	}
	zone_id_require_panic(zid, addr);
}

void
zone_id_require_allow_foreign(zone_id_t zid, vm_size_t esize, void *addr)
{
	if (__probable(from_zone_map(addr, esize, ZONE_ADDR_NATIVE) ||
	    from_zone_map(addr, esize, ZONE_ADDR_FOREIGN))) {
		if (zid == zone_index_from_ptr(addr)) {
			return;
		}
#if CONFIG_GZALLOC
	} else if (__probable(zone_array[zid].z_gzalloc_tracked)) {
		return;
#endif
	}
	zone_id_require_panic(zid, addr);
}

bool
zone_owns(zone_t zone, void *addr)
{
	vm_size_t esize = zone_elem_size_safe(zone);

	if (__probable(from_zone_map(addr, esize, ZONE_ADDR_NATIVE))) {
		return zone_has_index(zone, zone_index_from_ptr(addr));
#if CONFIG_GZALLOC
	} else if (__probable(zone->z_gzalloc_tracked)) {
		return true;
#endif
	}
	return false;
}

__startup_func
static struct zone_map_range
zone_kmem_suballoc(
	vm_offset_t     addr,
	vm_size_t       size,
	int             flags,
	vm_tag_t        tag,
	vm_map_t        *new_map)
{
	vm_map_kernel_flags_t vmk_flags = VM_MAP_KERNEL_FLAGS_NONE;
	struct zone_map_range r;
	kern_return_t kr;

	vmk_flags.vmkf_permanent = TRUE;
	kr = kmem_suballoc(kernel_map, &addr, size,
	    VM_MAP_CREATE_NEVER_FAULTS | VM_MAP_CREATE_DISABLE_HOLELIST,
	    flags, vmk_flags, tag, new_map);
	if (kr != KERN_SUCCESS) {
		panic("kmem_suballoc(%p:%p) failed: %d",
		    (void *)addr, (void *)(addr + size), kr);
	}

	r.min_address = addr;
	r.max_address = addr + size;
	return r;
}

#endif /* !ZALLOC_TEST */
#pragma mark Zone bits allocator

/*!
 * @defgroup Zone Bitmap allocator
 * @{
 *
 * @brief
 * Functions implementing the zone bitmap allocator
 *
 * @discussion
 * The zone allocator maintains which elements are allocated or free in bitmaps.
 *
 * When the number of elements per page is smaller than 32, it is stored inline
 * on the @c zone_page_metadata structure (@c zm_inline_bitmap is set,
 * and @c zm_bitmap used for storage).
 *
 * When the number of elements is larger, then a bitmap is allocated from
 * a buddy allocator (impelemented under the @c zba_* namespace). Pointers
 * to bitmaps are implemented as a packed 32 bit bitmap reference, stored in
 * @c zm_bitmap. The low 3 bits encode the scale (order) of the allocation in
 * @c ZBA_GRANULE units, and hence actual allocations encoded with that scheme
 * cannot be larger than 1024 bytes (8192 bits).
 *
 * This buddy allocator can actually accomodate allocations as large
 * as 8k on 16k systems and 2k on 4k systems.
 *
 * Note: @c zba_* functions are implementation details not meant to be used
 * outside of the allocation of the allocator itself. Interfaces to the rest of
 * the zone allocator are documented and not @c zba_* prefixed.
 */

#define ZBA_CHUNK_SIZE          PAGE_MAX_SIZE
#define ZBA_GRANULE             sizeof(uint64_t)
#define ZBA_GRANULE_BITS        (8 * sizeof(uint64_t))
#define ZBA_MAX_ORDER           (PAGE_MAX_SHIFT - 4)
#define ZBA_MAX_ALLOC_ORDER     7
#define ZBA_SLOTS               (ZBA_CHUNK_SIZE / ZBA_GRANULE)
static_assert(2ul * ZBA_GRANULE << ZBA_MAX_ORDER == ZBA_CHUNK_SIZE, "chunk sizes");
static_assert(ZBA_MAX_ALLOC_ORDER <= ZBA_MAX_ORDER, "ZBA_MAX_ORDER is enough");

struct zone_bits_chain {
	uint32_t zbc_next;
	uint32_t zbc_prev;
} __attribute__((aligned(ZBA_GRANULE)));

struct zone_bits_head {
	uint32_t zbh_next;
	uint32_t zbh_unused;
} __attribute__((aligned(ZBA_GRANULE)));

static_assert(sizeof(struct zone_bits_chain) == ZBA_GRANULE, "zbc size");
static_assert(sizeof(struct zone_bits_head) == ZBA_GRANULE, "zbh size");

struct zone_bits_allocator_meta {
	uint32_t zbam_chunks;
	uint32_t __zbam_padding;
	struct zone_bits_head zbam_lists[ZBA_MAX_ORDER + 1];
};

struct zone_bits_allocator_header {
	uint64_t zbah_bits[ZBA_SLOTS / (8 * sizeof(uint64_t))];
};

#if ZALLOC_TEST
static struct zalloc_bits_allocator_test_setup {
	vm_offset_t zbats_base;
	void      (*zbats_populate)(vm_address_t addr, vm_size_t size);
} zba_test_info;

static struct zone_bits_allocator_header *
zba_base_header(void)
{
	return (struct zone_bits_allocator_header *)zba_test_info.zbats_base;
}

static void
zba_populate(uint32_t n)
{
	vm_address_t base = zba_test_info.zbats_base;
	zba_test_info.zbats_populate(base + n * ZBA_CHUNK_SIZE, ZBA_CHUNK_SIZE);
}
#else
__startup_data
static uint8_t zba_chunk_startup[ZBA_CHUNK_SIZE]
__attribute__((aligned(ZBA_CHUNK_SIZE)));
static LCK_MTX_EARLY_DECLARE(zba_mtx, &zone_locks_grp);

static struct zone_bits_allocator_header *
zba_base_header(void)
{
	return (struct zone_bits_allocator_header *)zone_info.zi_bits_range.min_address;
}

static void
zba_lock(void)
{
	lck_mtx_lock(&zba_mtx);
}

static void
zba_unlock(void)
{
	lck_mtx_unlock(&zba_mtx);
}

static void
zba_populate(uint32_t n)
{
	vm_size_t size = ZBA_CHUNK_SIZE;
	vm_address_t addr;

	addr = zone_info.zi_bits_range.min_address + n * size;
	if (addr >= zone_info.zi_bits_range.max_address) {
		uint64_t zsize = 0;
		zone_t z = zone_find_largest(&zsize);
		panic("zba_populate: out of bitmap space, "
		    "likely due to memory leak in zone [%s%s] "
		    "(%luM, %d elements allocated)",
		    zone_heap_name(z), zone_name(z),
		    (unsigned long)zsize >> 20,
		    zone_count_allocated(z));
	}

	for (;;) {
		kern_return_t kr = KERN_SUCCESS;

		if (0 == pmap_find_phys(kernel_pmap, addr)) {
			kr = kernel_memory_populate(zone_meta_submaps[0], addr, size,
			    KMA_NOPAGEWAIT | KMA_KOBJECT | KMA_ZERO,
			    VM_KERN_MEMORY_OSFMK);
		}

		if (kr == KERN_SUCCESS) {
			return;
		}

		zba_unlock();
		VM_PAGE_WAIT();
		zba_lock();
	}
}
#endif

__pure2
static struct zone_bits_allocator_meta *
zba_meta(void)
{
	return (struct zone_bits_allocator_meta *)&zba_base_header()[1];
}

__pure2
static uint64_t *
zba_slot_base(void)
{
	return (uint64_t *)zba_base_header();
}

__pure2
static vm_address_t
zba_page_addr(uint32_t n)
{
	return (vm_address_t)zba_base_header() + n * ZBA_CHUNK_SIZE;
}

__pure2
static struct zone_bits_head *
zba_head(uint32_t order)
{
	return &zba_meta()->zbam_lists[order];
}

__pure2
static uint32_t
zba_head_index(uint32_t order)
{
	uint32_t hdr_size = sizeof(struct zone_bits_allocator_header) +
	    offsetof(struct zone_bits_allocator_meta, zbam_lists);
	return (hdr_size / ZBA_GRANULE) + order;
}

__pure2
static struct zone_bits_chain *
zba_chain_for_index(uint32_t index)
{
	return (struct zone_bits_chain *)(zba_slot_base() + index);
}

__pure2
static uint32_t
zba_chain_to_index(const struct zone_bits_chain *zbc)
{
	return (uint32_t)((const uint64_t *)zbc - zba_slot_base());
}

__abortlike
static void
zba_head_corruption_panic(uint32_t order)
{
	panic("zone bits allocator head[%d:%p] is corrupt", order,
	    zba_head(order));
}

__abortlike
static void
zba_chain_corruption_panic(struct zone_bits_chain *a, struct zone_bits_chain *b)
{
	panic("zone bits allocator freelist is corrupt (%p <-> %p)", a, b);
}

static void
zba_push_block(struct zone_bits_chain *zbc, uint32_t order)
{
	struct zone_bits_head *hd = zba_head(order);
	uint32_t hd_index = zba_head_index(order);
	uint32_t index = zba_chain_to_index(zbc);
	struct zone_bits_chain *next;

	if (hd->zbh_next) {
		next = zba_chain_for_index(hd->zbh_next);
		if (next->zbc_prev != hd_index) {
			zba_head_corruption_panic(order);
		}
		next->zbc_prev = index;
	}
	zbc->zbc_next = hd->zbh_next;
	zbc->zbc_prev = hd_index;
	hd->zbh_next = index;
}

static void
zba_remove_block(struct zone_bits_chain *zbc)
{
	struct zone_bits_chain *prev = zba_chain_for_index(zbc->zbc_prev);
	uint32_t index = zba_chain_to_index(zbc);

	if (prev->zbc_next != index) {
		zba_chain_corruption_panic(prev, zbc);
	}
	if ((prev->zbc_next = zbc->zbc_next)) {
		struct zone_bits_chain *next = zba_chain_for_index(zbc->zbc_next);
		if (next->zbc_prev != index) {
			zba_chain_corruption_panic(zbc, next);
		}
		next->zbc_prev = zbc->zbc_prev;
	}
}

static vm_address_t
zba_try_pop_block(uint32_t order)
{
	struct zone_bits_head *hd = zba_head(order);
	struct zone_bits_chain *zbc;

	if (hd->zbh_next == 0) {
		return 0;
	}

	zbc = zba_chain_for_index(hd->zbh_next);
	zba_remove_block(zbc);
	return (vm_address_t)zbc;
}

static struct zone_bits_allocator_header *
zba_header(vm_offset_t addr)
{
	addr &= -(vm_offset_t)ZBA_CHUNK_SIZE;
	return (struct zone_bits_allocator_header *)addr;
}

static size_t
zba_node_parent(size_t node)
{
	return (node - 1) / 2;
}

static size_t
zba_node_left_child(size_t node)
{
	return node * 2 + 1;
}

static size_t
zba_node_buddy(size_t node)
{
	return ((node - 1) ^ 1) + 1;
}

static size_t
zba_node(vm_offset_t addr, uint32_t order)
{
	vm_offset_t offs = (addr % ZBA_CHUNK_SIZE) / ZBA_GRANULE;
	return (offs >> order) + (1 << (ZBA_MAX_ORDER - order + 1)) - 1;
}

static struct zone_bits_chain *
zba_chain_for_node(struct zone_bits_allocator_header *zbah, size_t node, uint32_t order)
{
	vm_offset_t offs = (node - (1 << (ZBA_MAX_ORDER - order + 1)) + 1) << order;
	return (struct zone_bits_chain *)((vm_offset_t)zbah + offs * ZBA_GRANULE);
}

static void
zba_node_flip_split(struct zone_bits_allocator_header *zbah, size_t node)
{
	zbah->zbah_bits[node / 64] ^= 1ull << (node % 64);
}

static bool
zba_node_is_split(struct zone_bits_allocator_header *zbah, size_t node)
{
	return zbah->zbah_bits[node / 64] & (1ull << (node % 64));
}

static void
zba_free(vm_offset_t addr, uint32_t order)
{
	struct zone_bits_allocator_header *zbah = zba_header(addr);
	struct zone_bits_chain *zbc;
	size_t node = zba_node(addr, order);

	while (node) {
		size_t parent = zba_node_parent(node);

		zba_node_flip_split(zbah, parent);
		if (zba_node_is_split(zbah, parent)) {
			break;
		}

		zbc = zba_chain_for_node(zbah, zba_node_buddy(node), order);
		zba_remove_block(zbc);
		order++;
		node = parent;
	}

	zba_push_block(zba_chain_for_node(zbah, node, order), order);
}

static vm_size_t
zba_chunk_header_size(uint32_t n)
{
	vm_size_t hdr_size = sizeof(struct zone_bits_allocator_header);
	if (n == 0) {
		hdr_size += sizeof(struct zone_bits_allocator_meta);
	}
	return hdr_size;
}

static void
zba_init_chunk(uint32_t n)
{
	vm_size_t hdr_size = zba_chunk_header_size(n);
	vm_offset_t page = zba_page_addr(n);
	struct zone_bits_allocator_header *zbah = zba_header(page);
	vm_size_t size = ZBA_CHUNK_SIZE;
	size_t node;

	for (uint32_t o = ZBA_MAX_ORDER + 1; o-- > 0;) {
		if (size < hdr_size + (ZBA_GRANULE << o)) {
			continue;
		}
		size -= ZBA_GRANULE << o;
		node = zba_node(page + size, o);
		zba_node_flip_split(zbah, zba_node_parent(node));
		zba_push_block(zba_chain_for_node(zbah, node, o), o);
	}

	zba_meta()->zbam_chunks = n + 1;
}

__attribute__((noinline))
static void
zba_grow(void)
{
	uint32_t chunk = zba_meta()->zbam_chunks;

	zba_populate(chunk);
	if (zba_meta()->zbam_chunks == chunk) {
		zba_init_chunk(chunk);
	}
}

static vm_offset_t
zba_alloc(uint32_t order)
{
	struct zone_bits_allocator_header *zbah;
	uint32_t cur = order;
	vm_address_t addr;
	size_t node;

	while ((addr = zba_try_pop_block(cur)) == 0) {
		if (cur++ >= ZBA_MAX_ORDER) {
			zba_grow();
			cur = order;
		}
	}

	zbah = zba_header(addr);
	node = zba_node(addr, cur);
	zba_node_flip_split(zbah, zba_node_parent(node));
	while (cur > order) {
		cur--;
		zba_node_flip_split(zbah, node);
		node = zba_node_left_child(node);
		zba_push_block(zba_chain_for_node(zbah, node + 1, cur), cur);
	}

	return addr;
}

#define zba_map_index(type, n)    (n / (8 * sizeof(type)))
#define zba_map_bit(type, n)      ((type)1 << (n % (8 * sizeof(type))))
#define zba_map_mask_lt(type, n)  (zba_map_bit(type, n) - 1)
#define zba_map_mask_ge(type, n)  ((type)-zba_map_bit(type, n))

#if !ZALLOC_TEST
static uint32_t
zba_bits_ref_order(uint32_t bref)
{
	return bref & 0x7;
}

static bitmap_t *
zba_bits_ref_ptr(uint32_t bref)
{
	return zba_slot_base() + (bref >> 3);
}

static vm_offset_t
zba_scan_bitmap_inline(zone_t zone, struct zone_page_metadata *meta,
    zalloc_flags_t flags, vm_offset_t eidx)
{
	size_t i = eidx / 32;
	uint32_t map;

	if (eidx % 32) {
		map = meta[i].zm_bitmap & zba_map_mask_ge(uint32_t, eidx);
		if (map) {
			eidx = __builtin_ctz(map);
			meta[i].zm_bitmap ^= 1u << eidx;
			return i * 32 + eidx;
		}
		i++;
	}

	uint32_t chunk_len = meta->zm_chunk_len;
	if (flags & Z_PCPU) {
		chunk_len = zpercpu_count();
	}
	for (int j = 0; j < chunk_len; j++, i++) {
		if (i >= chunk_len) {
			i = 0;
		}
		if (__probable(map = meta[i].zm_bitmap)) {
			meta[i].zm_bitmap &= map - 1;
			return i * 32 + __builtin_ctz(map);
		}
	}

	zone_page_meta_accounting_panic(zone, meta, "zm_bitmap");
}

static vm_offset_t
zba_scan_bitmap_ref(zone_t zone, struct zone_page_metadata *meta,
    vm_offset_t eidx)
{
	uint32_t bits_size = 1 << zba_bits_ref_order(meta->zm_bitmap);
	bitmap_t *bits = zba_bits_ref_ptr(meta->zm_bitmap);
	size_t i = eidx / 64;
	uint64_t map;

	if (eidx % 64) {
		map = bits[i] & zba_map_mask_ge(uint64_t, eidx);
		if (map) {
			eidx = __builtin_ctzll(map);
			bits[i] ^= 1ull << eidx;
			return i * 64 + eidx;
		}
		i++;
	}

	for (int j = 0; j < bits_size; i++, j++) {
		if (i >= bits_size) {
			i = 0;
		}
		if (__probable(map = bits[i])) {
			bits[i] &= map - 1;
			return i * 64 + __builtin_ctzll(map);
		}
	}

	zone_page_meta_accounting_panic(zone, meta, "zm_bitmap");
}

/*!
 * @function zone_meta_find_and_clear_bit
 *
 * @brief
 * The core of the bitmap allocator: find a bit set in the bitmaps.
 *
 * @discussion
 * This method will round robin through available allocations,
 * with a per-core memory of the last allocated element index allocated.
 *
 * This is done in order to avoid a fully LIFO behavior which makes exploiting
 * double-free bugs way too practical.
 *
 * @param zone          The zone we're allocating from.
 * @param meta          The main metadata for the chunk being allocated from.
 * @param flags         the alloc flags (for @c Z_PCPU).
 */
static vm_offset_t
zone_meta_find_and_clear_bit(zone_t zone, struct zone_page_metadata *meta,
    zalloc_flags_t flags)
{
	zone_stats_t zs = zpercpu_get(zone->z_stats);
	vm_offset_t eidx = zs->zs_alloc_rr + 1;

	if (meta->zm_inline_bitmap) {
		eidx = zba_scan_bitmap_inline(zone, meta, flags, eidx);
	} else {
		eidx = zba_scan_bitmap_ref(zone, meta, eidx);
	}
	zs->zs_alloc_rr = (uint16_t)eidx;
	return eidx;
}

/*!
 * @function zone_meta_bits_init
 *
 * @brief
 * Initializes the zm_bitmap field(s) for a newly assigned chunk.
 *
 * @param meta          The main metadata for the initialized chunk.
 * @param count         The number of elements the chunk can hold
 *                      (which might be partial for partially populated chunks).
 * @param nbits         The maximum nuber of bits that will be used.
 */
static void
zone_meta_bits_init(struct zone_page_metadata *meta,
    uint32_t count, uint32_t nbits)
{
	static_assert(ZONE_MAX_ALLOC_SIZE / ZONE_MIN_ELEM_SIZE <=
	    ZBA_GRANULE_BITS << ZBA_MAX_ORDER, "bitmaps will be large enough");

	if (meta->zm_inline_bitmap) {
		/*
		 * We're called with the metadata zm_bitmap fields already
		 * zeroed out.
		 */
		for (size_t i = 0; 32 * i < count; i++) {
			if (32 * i + 32 <= count) {
				meta[i].zm_bitmap = ~0u;
			} else {
				meta[i].zm_bitmap = zba_map_mask_lt(uint32_t, count);
			}
		}
	} else {
		uint32_t order = flsll((nbits - 1) / ZBA_GRANULE_BITS);
		uint64_t *bits;

		assert(order <= ZBA_MAX_ALLOC_ORDER);
		assert(count <= ZBA_GRANULE_BITS << order);

		zba_lock();
		bits = (uint64_t *)zba_alloc(order);
		zba_unlock();

		for (size_t i = 0; i < 1u << order; i++) {
			if (64 * i + 64 <= count) {
				bits[i] = ~0ull;
			} else if (64 * i < count) {
				bits[i] = zba_map_mask_lt(uint64_t, count);
			} else {
				bits[i] = 0ull;
			}
		}

		meta->zm_bitmap = (uint32_t)((vm_offset_t)bits -
		    (vm_offset_t)zba_slot_base()) + order;
	}
}

/*!
 * @function zone_meta_bits_merge
 *
 * @brief
 * Adds elements <code>[start, end)</code> to a chunk being extended.
 *
 * @param meta          The main metadata for the extended chunk.
 * @param start         The index of the first element to add to the chunk.
 * @param end           The index of the last (exclusive) element to add.
 */
static void
zone_meta_bits_merge(struct zone_page_metadata *meta,
    uint32_t start, uint32_t end)
{
	if (meta->zm_inline_bitmap) {
		while (start < end) {
			size_t s_i = start / 32;
			size_t s_e = end / 32;

			if (s_i == s_e) {
				meta[s_i].zm_bitmap |= zba_map_mask_lt(uint32_t, end) &
				    zba_map_mask_ge(uint32_t, start);
				break;
			}

			meta[s_i].zm_bitmap |= zba_map_mask_ge(uint32_t, start);
			start += 32 - (start % 32);
		}
	} else {
		uint64_t *bits = zba_bits_ref_ptr(meta->zm_bitmap);

		while (start < end) {
			size_t s_i = start / 64;
			size_t s_e = end / 64;

			if (s_i == s_e) {
				bits[s_i] |= zba_map_mask_lt(uint64_t, end) &
				    zba_map_mask_ge(uint64_t, start);
				break;
			}
			bits[s_i] |= zba_map_mask_ge(uint64_t, start);
			start += 64 - (start % 64);
		}
	}
}

/*!
 * @function zone_bits_free
 *
 * @brief
 * Frees a bitmap to the zone bitmap allocator.
 *
 * @param bref
 * A bitmap reference set by @c zone_meta_bits_init() in a @c zm_bitmap field.
 */
static void
zone_bits_free(uint32_t bref)
{
	zba_lock();
	zba_free((vm_offset_t)zba_bits_ref_ptr(bref), zba_bits_ref_order(bref));
	zba_unlock();
}

/*!
 * @function zone_meta_is_free
 *
 * @brief
 * Returns whether a given element appears free.
 */
static bool
zone_meta_is_free(struct zone_page_metadata *meta, zone_element_t ze)
{
	vm_offset_t eidx = zone_element_idx(ze);
	if (meta->zm_inline_bitmap) {
		uint32_t bit = zba_map_bit(uint32_t, eidx);
		return meta[zba_map_index(uint32_t, eidx)].zm_bitmap & bit;
	} else {
		bitmap_t *bits = zba_bits_ref_ptr(meta->zm_bitmap);
		uint64_t bit = zba_map_bit(uint64_t, eidx);
		return bits[zba_map_index(uint64_t, eidx)] & bit;
	}
}

/*!
 * @function zone_meta_mark_free
 *
 * @brief
 * Marks an element as free and returns whether it was marked as used.
 */
static bool
zone_meta_mark_free(struct zone_page_metadata *meta, zone_element_t ze)
{
	vm_offset_t eidx = zone_element_idx(ze);

	if (meta->zm_inline_bitmap) {
		uint32_t bit = zba_map_bit(uint32_t, eidx);
		if (meta[zba_map_index(uint32_t, eidx)].zm_bitmap & bit) {
			return false;
		}
		meta[zba_map_index(uint32_t, eidx)].zm_bitmap ^= bit;
	} else {
		bitmap_t *bits = zba_bits_ref_ptr(meta->zm_bitmap);
		uint64_t bit = zba_map_bit(uint64_t, eidx);
		if (bits[zba_map_index(uint64_t, eidx)] & bit) {
			return false;
		}
		bits[zba_map_index(uint64_t, eidx)] ^= bit;
	}
	return true;
}

/*!
 * @function zone_meta_mark_used
 *
 * @brief
 * Marks an element as used and returns whether it was marked as free
 */
static bool
zone_meta_mark_used(struct zone_page_metadata *meta, zone_element_t ze)
{
	vm_offset_t eidx = zone_element_idx(ze);

	if (meta->zm_inline_bitmap) {
		uint32_t bit = zba_map_bit(uint32_t, eidx);
		if (meta[zba_map_index(uint32_t, eidx)].zm_bitmap & bit) {
			meta[zba_map_index(uint32_t, eidx)].zm_bitmap ^= bit;
			return true;
		}
	} else {
		bitmap_t *bits = zba_bits_ref_ptr(meta->zm_bitmap);
		uint64_t bit = zba_map_bit(uint64_t, eidx);
		if (bits[zba_map_index(uint64_t, eidx)] & bit) {
			bits[zba_map_index(uint64_t, eidx)] ^= bit;
			return true;
		}
	}
	return false;
}

#endif /* !ZALLOC_TEST */
/*! @} */
#pragma mark ZTAGS
#if !ZALLOC_TEST
#if VM_TAG_SIZECLASSES
/*
 * Zone tagging allows for per "tag" accounting of allocations for the kalloc
 * zones only.
 *
 * There are 3 kinds of tags that can be used:
 * - pre-registered VM_KERN_MEMORY_*
 * - dynamic tags allocated per call sites in core-kernel (using vm_tag_alloc())
 * - per-kext tags computed by IOKit (using the magic Z_VM_TAG_BT_BIT marker).
 *
 * The VM tracks the statistics in lazily allocated structures.
 * See vm_tag_will_update_zone(), vm_tag_update_zone_size().
 *
 * If for some reason the requested tag cannot be accounted for,
 * the tag is forced to VM_KERN_MEMORY_KALLOC which is pre-allocated.
 *
 * Each allocated element also remembers the tag it was assigned,
 * in its ztSlot() which lets zalloc/zfree update statistics correctly.
 */

// for zones with tagging enabled:

// calculate a pointer to the tag base entry,
// holding either a uint32_t the first tag offset for a page in the zone map,
// or two uint16_t tags if the page can only hold one or two elements

#define ZTAGBASE(zone, element) \
	(&((uint32_t *)zone_tagbase_min)[atop((element) - \
	    zone_info.zi_map_range[ZONE_ADDR_NATIVE].min_address)])

static vm_offset_t  zone_tagbase_min;
static vm_offset_t  zone_tagbase_max;
static vm_offset_t  zone_tagbase_map_size;
static vm_map_t     zone_tagbase_map;

static vm_offset_t  zone_tags_min;
static vm_offset_t  zone_tags_max;
static vm_offset_t  zone_tags_map_size;
static vm_map_t     zone_tags_map;

// simple heap allocator for allocating the tags for new memory

static LCK_MTX_EARLY_DECLARE(ztLock, &zone_locks_grp); /* heap lock */

/*
 * Array of all sizeclasses used by kalloc variants so that we can
 * have accounting per size class for each kalloc callsite
 */
uint16_t zone_tags_sizeclasses[VM_TAG_SIZECLASSES];

enum{
	ztFreeIndexCount = 8,
	ztFreeIndexMax   = (ztFreeIndexCount - 1),
	ztTagsPerBlock   = 4
};

struct ztBlock {
#if __LITTLE_ENDIAN__
	uint64_t free:1,
	    next:21,
	    prev:21,
	    size:21;
#else
// ztBlock needs free bit least significant
#error !__LITTLE_ENDIAN__
#endif
};
typedef struct ztBlock ztBlock;

static ztBlock * ztBlocks;
static uint32_t  ztBlocksCount;
static uint32_t  ztBlocksFree;

zalloc_flags_t
__zone_flags_mix_tag(zone_t z, zalloc_flags_t flags, vm_allocation_site_t *site)
{
	if (__improbable(z->z_uses_tags)) {
		vm_tag_t tag = zalloc_flags_get_tag(flags);
		if (flags & Z_VM_TAG_BT_BIT) {
			tag = vm_tag_bt() ?: tag;
		}
		if (tag == VM_KERN_MEMORY_NONE && site) {
			tag = vm_tag_alloc(site);
		}
		if (tag != VM_KERN_MEMORY_NONE) {
			tag = vm_tag_will_update_zone(tag, z->z_tags_sizeclass,
			    flags & (Z_WAITOK | Z_NOWAIT | Z_NOPAGEWAIT));
		}
		flags = Z_VM_TAG(flags & ~Z_VM_TAG_MASK, tag);
	}

	return flags;
}

static uint32_t
ztLog2up(uint32_t size)
{
	if (1 == size) {
		size = 0;
	} else {
		size = 32 - __builtin_clz(size - 1);
	}
	return size;
}

// pointer to the tag for an element
static vm_tag_t *
ztSlot(zone_t zone, vm_offset_t element)
{
	vm_tag_t *result;
	if (zone->z_tags_inline) {
		result = (vm_tag_t *)ZTAGBASE(zone, element);
		if ((PAGE_MASK & element) >= zone_elem_size(zone)) {
			result++;
		}
	} else {
		result = &((vm_tag_t *)zone_tags_min)[ZTAGBASE(zone, element)[0] +
		    (element & PAGE_MASK) / zone_elem_size(zone)];
	}
	return result;
}

static uint32_t
ztLog2down(uint32_t size)
{
	size = 31 - __builtin_clz(size);
	return size;
}

static void
ztFault(vm_map_t map, const void * address, size_t size, uint32_t flags)
{
	vm_map_offset_t addr = (vm_map_offset_t) address;
	vm_map_offset_t page, end;

	page = trunc_page(addr);
	end  = round_page(addr + size);

	for (; page < end; page += page_size) {
		if (!pmap_find_phys(kernel_pmap, page)) {
			kern_return_t __unused
			ret = kernel_memory_populate(map, page, PAGE_SIZE,
			    KMA_KOBJECT | flags, VM_KERN_MEMORY_DIAG);
			assert(ret == KERN_SUCCESS);
		}
	}
}

static boolean_t
ztPresent(const void * address, size_t size)
{
	vm_map_offset_t addr = (vm_map_offset_t) address;
	vm_map_offset_t page, end;
	boolean_t       result;

	page = trunc_page(addr);
	end  = round_page(addr + size);
	for (result = TRUE; (page < end); page += page_size) {
		result = pmap_find_phys(kernel_pmap, page);
		if (!result) {
			break;
		}
	}
	return result;
}


void __unused
ztDump(boolean_t sanity);
void __unused
ztDump(boolean_t sanity)
{
	uint32_t q, cq, p;

	for (q = 0; q <= ztFreeIndexMax; q++) {
		p = q;
		do{
			if (sanity) {
				cq = ztLog2down(ztBlocks[p].size);
				if (cq > ztFreeIndexMax) {
					cq = ztFreeIndexMax;
				}
				if (!ztBlocks[p].free
				    || ((p != q) && (q != cq))
				    || (ztBlocks[ztBlocks[p].next].prev != p)
				    || (ztBlocks[ztBlocks[p].prev].next != p)) {
					kprintf("zterror at %d", p);
					ztDump(FALSE);
					kprintf("zterror at %d", p);
					assert(FALSE);
				}
				continue;
			}
			kprintf("zt[%03d]%c %d, %d, %d\n",
			    p, ztBlocks[p].free ? 'F' : 'A',
			    ztBlocks[p].next, ztBlocks[p].prev,
			    ztBlocks[p].size);
			p = ztBlocks[p].next;
			if (p == q) {
				break;
			}
		}while (p != q);
		if (!sanity) {
			printf("\n");
		}
	}
	if (!sanity) {
		printf("-----------------------\n");
	}
}



#define ZTBDEQ(idx)                                                 \
    ztBlocks[ztBlocks[(idx)].prev].next = ztBlocks[(idx)].next;     \
    ztBlocks[ztBlocks[(idx)].next].prev = ztBlocks[(idx)].prev;

static void
ztFree(zone_t zone __unused, uint32_t index, uint32_t count)
{
	uint32_t q, w, p, size, merge;

	assert(count);
	ztBlocksFree += count;

	// merge with preceding
	merge = (index + count);
	if ((merge < ztBlocksCount)
	    && ztPresent(&ztBlocks[merge], sizeof(ztBlocks[merge]))
	    && ztBlocks[merge].free) {
		ZTBDEQ(merge);
		count += ztBlocks[merge].size;
	}

	// merge with following
	merge = (index - 1);
	if ((merge > ztFreeIndexMax)
	    && ztPresent(&ztBlocks[merge], sizeof(ztBlocks[merge]))
	    && ztBlocks[merge].free) {
		size = ztBlocks[merge].size;
		count += size;
		index -= size;
		ZTBDEQ(index);
	}

	q = ztLog2down(count);
	if (q > ztFreeIndexMax) {
		q = ztFreeIndexMax;
	}
	w = q;
	// queue in order of size
	while (TRUE) {
		p = ztBlocks[w].next;
		if (p == q) {
			break;
		}
		if (ztBlocks[p].size >= count) {
			break;
		}
		w = p;
	}
	ztBlocks[p].prev = index;
	ztBlocks[w].next = index;

	// fault in first
	ztFault(zone_tags_map, &ztBlocks[index], sizeof(ztBlocks[index]), 0);

	// mark first & last with free flag and size
	ztBlocks[index].free = TRUE;
	ztBlocks[index].size = count;
	ztBlocks[index].prev = w;
	ztBlocks[index].next = p;
	if (count > 1) {
		index += (count - 1);
		// fault in last
		ztFault(zone_tags_map, &ztBlocks[index], sizeof(ztBlocks[index]), 0);
		ztBlocks[index].free = TRUE;
		ztBlocks[index].size = count;
	}
}

static uint32_t
ztAlloc(zone_t zone, uint32_t count)
{
	uint32_t q, w, p, leftover;

	assert(count);

	q = ztLog2up(count);
	if (q > ztFreeIndexMax) {
		q = ztFreeIndexMax;
	}
	do{
		w = q;
		while (TRUE) {
			p = ztBlocks[w].next;
			if (p == q) {
				break;
			}
			if (ztBlocks[p].size >= count) {
				// dequeue, mark both ends allocated
				ztBlocks[w].next = ztBlocks[p].next;
				ztBlocks[ztBlocks[p].next].prev = w;
				ztBlocks[p].free = FALSE;
				ztBlocksFree -= ztBlocks[p].size;
				if (ztBlocks[p].size > 1) {
					ztBlocks[p + ztBlocks[p].size - 1].free = FALSE;
				}

				// fault all the allocation
				ztFault(zone_tags_map, &ztBlocks[p], count * sizeof(ztBlocks[p]), 0);
				// mark last as allocated
				if (count > 1) {
					ztBlocks[p + count - 1].free = FALSE;
				}
				// free remainder
				leftover = ztBlocks[p].size - count;
				if (leftover) {
					ztFree(zone, p + ztBlocks[p].size - leftover, leftover);
				}

				return p;
			}
			w = p;
		}
		q++;
	}while (q <= ztFreeIndexMax);

	return -1U;
}

__startup_func
static void
zone_tagging_init(vm_size_t max_zonemap_size)
{
	struct zone_map_range r;

	// allocate submaps VM_KERN_MEMORY_DIAG

	zone_tagbase_map_size = atop(max_zonemap_size) * sizeof(uint32_t);
	r = zone_kmem_suballoc(0, round_page(zone_tagbase_map_size),
	    VM_FLAGS_ANYWHERE, VM_KERN_MEMORY_DIAG, &zone_tagbase_map);
	zone_tagbase_min = r.min_address;
	zone_tagbase_max = r.max_address;

	zone_tags_map_size = 2048 * 1024 * sizeof(vm_tag_t);
	r = zone_kmem_suballoc(0, round_page(zone_tags_map_size),
	    VM_FLAGS_ANYWHERE, VM_KERN_MEMORY_DIAG, &zone_tags_map);
	zone_tags_min = r.min_address;
	zone_tags_max = r.max_address;

	ztBlocks = (ztBlock *) zone_tags_min;
	ztBlocksCount = (uint32_t)(zone_tags_map_size / sizeof(ztBlock));

	// initialize the qheads
	lck_mtx_lock(&ztLock);

	ztFault(zone_tags_map, &ztBlocks[0], sizeof(ztBlocks[0]), 0);
	for (uint32_t idx = 0; idx < ztFreeIndexCount; idx++) {
		ztBlocks[idx].free = TRUE;
		ztBlocks[idx].next = idx;
		ztBlocks[idx].prev = idx;
		ztBlocks[idx].size = 0;
	}
	// free remaining space
	ztFree(NULL, ztFreeIndexCount, ztBlocksCount - ztFreeIndexCount);

	lck_mtx_unlock(&ztLock);
}

static void
ztMemoryAdd(zone_t zone, vm_offset_t mem, vm_size_t size)
{
	uint32_t * tagbase;
	uint32_t   count, block, blocks, idx;
	size_t     pages;

	pages = atop(size);
	tagbase = ZTAGBASE(zone, mem);

	lck_mtx_lock(&ztLock);

	// fault tagbase
	ztFault(zone_tagbase_map, tagbase, pages * sizeof(uint32_t), 0);

	if (!zone->z_tags_inline) {
		// allocate tags
		count = (uint32_t)(size / zone_elem_size(zone));
		blocks = ((count + ztTagsPerBlock - 1) / ztTagsPerBlock);
		block = ztAlloc(zone, blocks);
		if (-1U == block) {
			ztDump(false);
		}
		assert(-1U != block);
	}

	lck_mtx_unlock(&ztLock);

	if (!zone->z_tags_inline) {
		// set tag base for each page
		block *= ztTagsPerBlock;
		for (idx = 0; idx < pages; idx++) {
			vm_offset_t esize = zone_elem_size(zone);
			tagbase[idx] = block + (uint32_t)((ptoa(idx) + esize - 1) / esize);
		}
	}
}

static void
ztMemoryRemove(zone_t zone, vm_offset_t mem, vm_size_t size)
{
	uint32_t * tagbase;
	uint32_t   count, block, blocks, idx;
	size_t     pages;

	// set tag base for each page
	pages = atop(size);
	tagbase = ZTAGBASE(zone, mem);
	block = tagbase[0];
	for (idx = 0; idx < pages; idx++) {
		tagbase[idx] = 0xFFFFFFFF;
	}

	lck_mtx_lock(&ztLock);
	if (!zone->z_tags_inline) {
		count = (uint32_t)(size / zone_elem_size(zone));
		blocks = ((count + ztTagsPerBlock - 1) / ztTagsPerBlock);
		assert(block != 0xFFFFFFFF);
		block /= ztTagsPerBlock;
		ztFree(NULL /* zone is unlocked */, block, blocks);
	}

	lck_mtx_unlock(&ztLock);
}

uint16_t
zone_index_from_tag_index(uint32_t sizeclass_idx)
{
	return zone_tags_sizeclasses[sizeclass_idx];
}

#endif /* VM_TAG_SIZECLASSES */
#endif /* !ZALLOC_TEST */
#pragma mark zalloc helpers
#if !ZALLOC_TEST

__pure2
static inline uint16_t
zc_mag_size(void)
{
	return zc_magazine_size;
}

__attribute__((noinline, cold))
static void
zone_lock_was_contended(zone_t zone, zone_cache_t zc)
{
	lck_spin_lock_nopreempt(&zone->z_lock);

	/*
	 * If zone caching has been disabled due to memory pressure,
	 * then recording contention is not useful, give the system
	 * time to recover.
	 */
	if (__improbable(zone_caching_disabled)) {
		return;
	}

	zone->z_contention_cur++;

	if (zc == NULL || zc->zc_depot_max >= INT16_MAX * zc_mag_size()) {
		return;
	}

	/*
	 * Let the depot grow based on how bad the contention is,
	 * and how populated the zone is.
	 */
	if (zone->z_contention_wma < 2 * Z_CONTENTION_WMA_UNIT) {
		if (zc->zc_depot_max * zpercpu_count() * 20u >=
		    zone->z_elems_avail) {
			return;
		}
	}
	if (zone->z_contention_wma < 4 * Z_CONTENTION_WMA_UNIT) {
		if (zc->zc_depot_max * zpercpu_count() * 10u >=
		    zone->z_elems_avail) {
			return;
		}
	}
	if (!zc_grow_threshold || zone->z_contention_wma <
	    zc_grow_threshold * Z_CONTENTION_WMA_UNIT) {
		return;
	}

	zc->zc_depot_max++;
}

static inline void
zone_lock_nopreempt_check_contention(zone_t zone, zone_cache_t zc)
{
	if (lck_spin_try_lock_nopreempt(&zone->z_lock)) {
		return;
	}

	zone_lock_was_contended(zone, zc);
}

static inline void
zone_lock_check_contention(zone_t zone, zone_cache_t zc)
{
	disable_preemption();
	zone_lock_nopreempt_check_contention(zone, zc);
}

static inline void
zone_unlock_nopreempt(zone_t zone)
{
	lck_spin_unlock_nopreempt(&zone->z_lock);
}

static inline void
zone_depot_lock_nopreempt(zone_cache_t zc)
{
	hw_lock_bit_nopreempt(&zc->zc_depot_lock, 0, &zone_locks_grp);
}

static inline void
zone_depot_unlock_nopreempt(zone_cache_t zc)
{
	hw_unlock_bit_nopreempt(&zc->zc_depot_lock, 0);
}

static inline void
zone_depot_lock(zone_cache_t zc)
{
	hw_lock_bit(&zc->zc_depot_lock, 0, &zone_locks_grp);
}

static inline void
zone_depot_unlock(zone_cache_t zc)
{
	hw_unlock_bit(&zc->zc_depot_lock, 0);
}

const char *
zone_name(zone_t z)
{
	return z->z_name;
}

const char *
zone_heap_name(zone_t z)
{
	zone_security_flags_t zsflags = zone_security_config(z);
	if (__probable(zsflags.z_kheap_id < KHEAP_ID_COUNT)) {
		return kalloc_heap_names[zsflags.z_kheap_id];
	}
	return "invalid";
}

static uint32_t
zone_alloc_pages_for_nelems(zone_t z, vm_size_t max_elems)
{
	vm_size_t elem_count, chunks;

	elem_count = ptoa(z->z_percpu ? 1 : z->z_chunk_pages) /
	    zone_elem_size_safe(z);
	chunks = (max_elems + elem_count - 1) / elem_count;

	return (uint32_t)MIN(UINT32_MAX, chunks * z->z_chunk_pages);
}

static inline vm_size_t
zone_submaps_approx_size(void)
{
	vm_size_t size = 0;

	for (unsigned idx = 0; idx < Z_SUBMAP_IDX_COUNT; idx++) {
		if (zone_submaps[idx] != VM_MAP_NULL) {
			size += zone_submaps[idx]->size;
		}
	}

	return size;
}

static void
zone_cache_swap_magazines(zone_cache_t cache)
{
	uint16_t count_a = cache->zc_alloc_cur;
	uint16_t count_f = cache->zc_free_cur;
	zone_element_t *elems_a = cache->zc_alloc_elems;
	zone_element_t *elems_f = cache->zc_free_elems;

	z_debug_assert(count_a <= zc_mag_size());
	z_debug_assert(count_f <= zc_mag_size());

	cache->zc_alloc_cur = count_f;
	cache->zc_free_cur = count_a;
	cache->zc_alloc_elems = elems_f;
	cache->zc_free_elems = elems_a;
}

/*!
 * @function zone_magazine_load
 *
 * @brief
 * Cache the value of @c zm_cur on the cache to avoid a dependent load
 * on the allocation fastpath.
 */
static void
zone_magazine_load(uint16_t *count, zone_element_t **elems, zone_magazine_t mag)
{
	z_debug_assert(mag->zm_cur <= zc_mag_size());
	*count = mag->zm_cur;
	*elems = mag->zm_elems;
}

/*!
 * @function zone_magazine_replace
 *
 * @brief
 * Unlod a magazine and load a new one instead.
 */
static zone_magazine_t
zone_magazine_replace(uint16_t *count, zone_element_t **elems,
    zone_magazine_t mag)
{
	zone_magazine_t old;

	old = (zone_magazine_t)((uintptr_t)*elems -
	    offsetof(struct zone_magazine, zm_elems));
	old->zm_cur = *count;
	z_debug_assert(old->zm_cur <= zc_mag_size());
	zone_magazine_load(count, elems, mag);

	return old;
}

static zone_magazine_t
zone_magazine_alloc(zalloc_flags_t flags)
{
	return zalloc_flags(zc_magazine_zone, flags | Z_ZERO);
}

static void
zone_magazine_free(zone_magazine_t mag)
{
	(zfree)(zc_magazine_zone, mag);
}

static void
zone_magazine_free_list(struct zone_depot *mags)
{
	zone_magazine_t mag, tmp;

	STAILQ_FOREACH_SAFE(mag, mags, zm_link, tmp) {
		zone_magazine_free(mag);
	}

	STAILQ_INIT(mags);
}

static void
zone_enable_caching(zone_t zone)
{
	zone_cache_t caches;

	caches = zalloc_percpu_permanent_type(struct zone_cache);
	zpercpu_foreach(zc, caches) {
		zone_magazine_load(&zc->zc_alloc_cur, &zc->zc_alloc_elems,
		    zone_magazine_alloc(Z_WAITOK | Z_NOFAIL));
		zone_magazine_load(&zc->zc_free_cur, &zc->zc_free_elems,
		    zone_magazine_alloc(Z_WAITOK | Z_NOFAIL));
		STAILQ_INIT(&zc->zc_depot);
	}

	if (os_atomic_xchg(&zone->z_pcpu_cache, caches, release)) {
		panic("allocating caches for zone %s twice", zone->z_name);
	}
}

bool
zone_maps_owned(vm_address_t addr, vm_size_t size)
{
	return from_zone_map(addr, size, ZONE_ADDR_NATIVE);
}

void
zone_map_sizes(
	vm_map_size_t    *psize,
	vm_map_size_t    *pfree,
	vm_map_size_t    *plargest_free)
{
	vm_map_size_t size, free, largest;

	vm_map_sizes(zone_submaps[0], psize, pfree, plargest_free);

	for (uint32_t i = 1; i < Z_SUBMAP_IDX_COUNT; i++) {
		vm_map_sizes(zone_submaps[i], &size, &free, &largest);
		*psize += size;
		*pfree += free;
		*plargest_free = MAX(*plargest_free, largest);
	}
}

__attribute__((always_inline))
vm_map_t
zone_submap(zone_security_flags_t zsflags)
{
	return zone_submaps[zsflags.z_submap_idx];
}

unsigned
zpercpu_count(void)
{
	return zpercpu_early_count;
}

/*
 * Returns a random number between [bound_min, bound_max)
 *
 * DO NOT COPY THIS CODE OUTSIDE OF ZALLOC
 *
 * This uses Intel's rdrand because random() uses FP registers
 * which causes FP faults and allocations which isn't something
 * we can do from zalloc itself due to reentrancy problems.
 *
 * For pre-rdrand machines (which we no longer support),
 * we use a bad biased random generator that doesn't use FP.
 * Such HW is no longer supported, but VM of newer OSes on older
 * bare metal is made to limp along (with reduced security) this way.
 */
#if ZSECURITY_CONFIG(SAD_FENG_SHUI) || CONFIG_PROB_GZALLOC
static uint32_t
zalloc_random_uniform(uint32_t bound_min, uint32_t bound_max)
{
	uint32_t bits = 32 - __builtin_clz(bound_max - bound_min);
	uint32_t mask = ~0u >> (32 - bits);
	uint32_t v;

	do {
#if __x86_64__
		if (__probable(cpuid_features() & CPUID_FEATURE_RDRAND)) {
			asm volatile ("1: rdrand %0; jnc 1b\n"
                            : "=r" (v) :: "cc");
		} else {
			disable_preemption();
			int cpu = cpu_number();
			v = random_bool_gen_bits(&zone_bool_gen[cpu].zbg_bg,
			    zone_bool_gen[cpu].zbg_entropy,
			    ZONE_ENTROPY_CNT, bits);
			enable_preemption();
		}
#else
		v = (uint32_t)early_random();
#endif
		v = bound_min + (v & mask);
	} while (v >= bound_max);

	return v;
}
#endif /* ZSECURITY_CONFIG(SAD_FENG_SHUI) || CONFIG_PROB_GZALLOC */

#if ZONE_ENABLE_LOGGING || CONFIG_PROB_GZALLOC
/*
 * Track all kalloc zones of specified size for zlog name
 * kalloc.type.<size> or kalloc.type.var.<size> or kalloc.<size>
 */
static bool
track_kalloc_zones(zone_t z, const char *logname)
{
	const char *prefix;
	size_t len;
	zone_security_flags_t zsflags = zone_security_config(z);

	prefix = "kalloc.type.var.";
	len    = strlen(prefix);
	if (zsflags.z_kalloc_type && zsflags.z_kheap_id == KHEAP_ID_KT_VAR &&
	    strncmp(logname, prefix, len) == 0) {
		vm_size_t sizeclass = strtoul(logname + len, NULL, 0);

		return zone_elem_size(z) == sizeclass;
	}

	prefix = "kalloc.type.";
	len    = strlen(prefix);
	if (zsflags.z_kalloc_type && zsflags.z_kheap_id != KHEAP_ID_KT_VAR &&
	    strncmp(logname, prefix, len) == 0) {
		vm_size_t sizeclass = strtoul(logname + len, NULL, 0);

		return zone_elem_size(z) == sizeclass;
	}

	prefix = "kalloc.";
	len    = strlen(prefix);
	if ((zsflags.z_kheap_id || zsflags.z_kalloc_type) &&
	    strncmp(logname, prefix, len) == 0) {
		vm_size_t sizeclass = strtoul(logname + len, NULL, 0);

		return zone_elem_size(z) == sizeclass;
	}

	return false;
}
#endif

int
track_this_zone(const char *zonename, const char *logname)
{
	unsigned int len;
	const char *zc = zonename;
	const char *lc = logname;

	/*
	 * Compare the strings.  We bound the compare by MAX_ZONE_NAME.
	 */

	for (len = 1; len <= MAX_ZONE_NAME; zc++, lc++, len++) {
		/*
		 * If the current characters don't match, check for a space in
		 * in the zone name and a corresponding period in the log name.
		 * If that's not there, then the strings don't match.
		 */

		if (*zc != *lc && !(*zc == ' ' && *lc == '.')) {
			break;
		}

		/*
		 * The strings are equal so far.  If we're at the end, then it's a match.
		 */

		if (*zc == '\0') {
			return TRUE;
		}
	}

	return FALSE;
}

#if DEBUG || DEVELOPMENT

vm_size_t
zone_element_info(void *addr, vm_tag_t * ptag)
{
	vm_size_t     size = 0;
	vm_tag_t      tag = VM_KERN_MEMORY_NONE;
	struct zone *src_zone;

	if (from_zone_map(addr, sizeof(void *), ZONE_ADDR_NATIVE) ||
	    from_zone_map(addr, sizeof(void *), ZONE_ADDR_FOREIGN)) {
		src_zone = &zone_array[zone_index_from_ptr(addr)];
#if VM_TAG_SIZECLASSES
		if (__improbable(src_zone->z_uses_tags)) {
			tag = *ztSlot(src_zone, (vm_offset_t)addr) >> 1;
		}
#endif /* VM_TAG_SIZECLASSES */
		size = zone_elem_size_safe(src_zone);
	} else {
#if CONFIG_GZALLOC
		gzalloc_element_size(addr, NULL, &size);
#endif /* CONFIG_GZALLOC */
	}
	*ptag = tag;
	return size;
}

#endif /* DEBUG || DEVELOPMENT */
#endif /* !ZALLOC_TEST */

#pragma mark Zone zeroing and early random
#if !ZALLOC_TEST

/*
 * Zone zeroing
 *
 * All allocations from zones are zeroed on free and are additionally
 * check that they are still zero on alloc. The check is
 * always on, on embedded devices. Perf regression was detected
 * on intel as we cant use the vectorized implementation of
 * memcmp_zero_ptr_aligned due to cyclic dependenices between
 * initization and allocation. Therefore we perform the check
 * on 20% of the allocations.
 */
#if ZALLOC_ENABLE_ZERO_CHECK
#if defined(__x86_64__) || defined(__arm__)
/*
 * Peform zero validation on every 5th allocation
 */
static TUNABLE(uint32_t, zzc_rate, "zzc_rate", 5);
static uint32_t PERCPU_DATA(zzc_decrementer);
#endif /* defined(__x86_64__) || defined(__arm__) */

/*
 * Determine if zero validation for allocation should be skipped
 */
static bool
zalloc_skip_zero_check(void)
{
#if defined(__x86_64__) || defined(__arm__)
	uint32_t *counterp, cnt;

	counterp = PERCPU_GET(zzc_decrementer);
	cnt = *counterp;
	if (__probable(cnt > 0)) {
		*counterp  = cnt - 1;
		return true;
	}
	*counterp = zzc_rate - 1;
#endif /* !(defined(__x86_64__) || defined(__arm__)) */
	return false;
}

__abortlike
static void
zalloc_uaf_panic(zone_t z, uintptr_t elem, size_t size)
{
	uint32_t esize = (uint32_t)zone_elem_size(z);
	uint32_t first_offs = ~0u;
	uintptr_t first_bits = 0, v;
	char buf[1024];
	int pos = 0;

#if __LP64__
#define ZPF  "0x%016lx"
#else
#define ZPF  "0x%08lx"
#endif

	buf[0] = '\0';

	for (uint32_t o = 0; o < size; o += sizeof(v)) {
		if ((v = *(uintptr_t *)(elem + o)) == 0) {
			continue;
		}
		pos += scnprintf(buf + pos, sizeof(buf) - pos, "\n"
		    "%5d: "ZPF, o, v);
		if (first_offs > o) {
			first_offs = o;
			first_bits = v;
		}
	}

	(panic)("[%s%s]: element modified after free "
	"(off:%d, val:"ZPF", sz:%d, ptr:%p)%s",
	zone_heap_name(z), zone_name(z),
	first_offs, first_bits, esize, (void *)elem, buf);

#undef ZPF
}

static void
zalloc_validate_element(zone_t zone, vm_offset_t elem, vm_size_t size,
    zalloc_flags_t flags)
{
#if CONFIG_GZALLOC
	if (zone->z_gzalloc_tracked) {
		return;
	}
#endif /* CONFIG_GZALLOC */

	if (flags & Z_NOZZC) {
		return;
	}
	if (memcmp_zero_ptr_aligned((void *)elem, size)) {
		zalloc_uaf_panic(zone, elem, size);
	}
	if (flags & Z_PCPU) {
		for (size_t i = zpercpu_count(); --i > 0;) {
			elem += PAGE_SIZE;
			if (memcmp_zero_ptr_aligned((void *)elem, size)) {
				zalloc_uaf_panic(zone, elem, size);
			}
		}
	}
}

#endif /* ZALLOC_ENABLE_ZERO_CHECK */

static void
zone_early_scramble_rr(zone_t zone, zone_stats_t zstats)
{
	int cpu = cpu_number();
	zone_stats_t zs = zpercpu_get_cpu(zstats, cpu);
	uint32_t bits;

	bits = random_bool_gen_bits(&zone_bool_gen[cpu].zbg_bg,
	    zone_bool_gen[cpu].zbg_entropy, ZONE_ENTROPY_CNT, 8);

	zs->zs_alloc_rr += bits;
	zs->zs_alloc_rr %= zone->z_chunk_elems;
}

#endif /* !ZALLOC_TEST */
#pragma mark Zone Leak Detection
#if !ZALLOC_TEST
#if ZONE_ENABLE_LOGGING || CONFIG_ZLEAKS

/*
 * Zone leak debugging code
 *
 * When enabled, this code keeps a log to track allocations to a particular
 * zone that have not yet been freed.
 *
 * Examining this log will reveal the source of a zone leak.
 *
 * The log is allocated only when logging is enabled (it is off by default),
 * so there is no effect on the system when it's turned off.
 *
 * Zone logging is enabled with the `zlog<n>=<zone>` boot-arg for each
 * zone name to log, with n starting at 1.
 *
 * Leaks debugging utilizes 2 tunables:
 * - zlsize (in kB) which describes how much "size" the record covers
 *   (zones with smaller elements get more records, default is 4M).
 *
 * - zlfreq (in kB) which describes a sample rate in cumulative allocation
 *   size at which automatic leak detection will sample allocations.
 *   (default is 16k)
 *
 *
 * Zone corruption logging
 *
 * Logging can also be used to help identify the source of a zone corruption.
 *
 * First, identify the zone that is being corrupted,
 * then add "-zc zlog<n>=<zone name>" to the boot-args.
 *
 * When -zc is used in conjunction with zlog,
 * it changes the logging style to track both allocations and frees to the zone.
 *
 * When the corruption is detected, examining the log will show you the stack
 * traces of the callers who last allocated and freed any particular element in
 * the zone.
 *
 * Corruption debugging logs will have zrecs records
 * (tuned by the zrecs= boot-arg, 16k elements per G of RAM by default).
 */

#define ZRECORDS_MAX            (256u << 10)
#define ZRECORDS_DEFAULT        (16u  << 10)
static TUNABLE(uint32_t, zrecs, "zrecs", 0);
static TUNABLE(uint32_t, zlsize, "zlsize", 4 * 1024);
static TUNABLE(uint32_t, zlfreq, "zlfreq", 16);

__startup_func
static void
zone_leaks_init_zrecs(void)
{
	/*
	 * Don't allow more than ZRECORDS_MAX records,
	 * even if the user asked for more.
	 *
	 * This prevents accidentally hogging too much kernel memory
	 * and making the system unusable.
	 */
	if (zrecs == 0) {
		zrecs = ZRECORDS_DEFAULT *
		    (uint32_t)((max_mem + (1ul << 30)) >> 30);
	}
	if (zrecs > ZRECORDS_MAX) {
		zrecs = ZRECORDS_MAX;
	}
}
STARTUP(TUNABLES, STARTUP_RANK_MIDDLE, zone_leaks_init_zrecs);

static uint32_t
zone_leaks_record_count(zone_t z)
{
	uint32_t recs = (zlsize << 10) / zone_elem_size(z);

	return MIN(MAX(recs, ZRECORDS_DEFAULT), ZRECORDS_MAX);
}

static uint32_t
zone_leaks_sample_rate(zone_t z)
{
	return (zlfreq << 10) / zone_elem_size(z);
}

#if ZONE_ENABLE_LOGGING
/* Log allocations and frees to help debug a zone element corruption */
static TUNABLE(bool, corruption_debug_flag, "-zc", false);

/*
 * A maximum of 10 zlog<n> boot args can be provided (zlog1 -> zlog10)
 */
#define MAX_ZONES_LOG_REQUESTS  10
/*
 * As all kalloc type zones of a specificified size are logged, by providing
 * a single zlog boot-arg, the maximum number of zones that can be logged
 * is higher than MAX_ZONES_LOG_REQUESTS
 */
#define MAX_ZONES_LOGGED        20

static int num_zones_logged = 0;

/**
 * @function zone_setup_logging
 *
 * @abstract
 * Optionally sets up a zone for logging.
 *
 * @discussion
 * We recognized two boot-args:
 *
 *	zlog=<zone_to_log>
 *	zrecs=<num_records_in_log>
 *	zlsize=<memory to cover for leaks>
 *
 * The zlog arg is used to specify the zone name that should be logged,
 * and zrecs/zlsize is used to control the size of the log.
 */
static void
zone_setup_logging(zone_t z)
{
	char zone_name[MAX_ZONE_NAME]; /* Temp. buffer for the zone name */
	char zlog_name[MAX_ZONE_NAME]; /* Temp. buffer to create the strings zlog1, zlog2 etc... */
	char zlog_val[MAX_ZONE_NAME];  /* the zone name we're logging, if any */
	bool logging_on = false;

	if (num_zones_logged >= MAX_ZONES_LOGGED) {
		return;
	}

	/*
	 * Append kalloc heap name to zone name (if zone is used by kalloc)
	 */
	snprintf(zone_name, MAX_ZONE_NAME, "%s%s", zone_heap_name(z), z->z_name);

	/* zlog0 isn't allowed. */
	for (int i = 1; i <= MAX_ZONES_LOG_REQUESTS; i++) {
		snprintf(zlog_name, MAX_ZONE_NAME, "zlog%d", i);

		if (PE_parse_boot_argn(zlog_name, zlog_val, sizeof(zlog_val))) {
			if (track_this_zone(zone_name, zlog_val) ||
			    track_kalloc_zones(z, zlog_val)) {
				logging_on = true;
				break;
			}
		}
	}

	/*
	 * Backwards compat. with the old boot-arg used to specify single zone
	 * logging i.e. zlog Needs to happen after the newer zlogn checks
	 * because the prefix will match all the zlogn
	 * boot-args.
	 */
	if (!logging_on &&
	    PE_parse_boot_argn("zlog", zlog_val, sizeof(zlog_val))) {
		if (track_this_zone(zone_name, zlog_val) ||
		    track_kalloc_zones(z, zlog_val)) {
			logging_on = true;
		}
	}

	/*
	 * If we want to log a zone, see if we need to allocate buffer space for
	 * the log.
	 *
	 * Some vm related zones are zinit'ed before we can do a kmem_alloc, so
	 * we have to defer allocation in that case.
	 *
	 * zone_init() will finish the job.
	 *
	 * If we want to log one of the VM related zones that's set up early on,
	 * we will skip allocation of the log until zinit is called again later
	 * on some other zone.
	 */
	if (logging_on) {
		if (os_atomic_inc(&num_zones_logged, relaxed) >
		    MAX_ZONES_LOGGED) {
			os_atomic_dec(&num_zones_logged, relaxed);
			return;
		}

		if (corruption_debug_flag) {
			z->z_btlog = btlog_create(BTLOG_LOG, zrecs, 0);
		} else {
			z->z_btlog = btlog_create(BTLOG_HASH,
			    zone_leaks_record_count(z), 0);
		}
		if (z->z_btlog) {
			z->z_log_on = true;
			printf("zone[%s%s]: logging enabled\n",
			    zone_heap_name(z), z->z_name);
		} else {
			printf("zone[%s%s]: failed to enable logging\n",
			    zone_heap_name(z), z->z_name);
		}
	}
}

#endif /* ZONE_ENABLE_LOGGING */
#if CONFIG_ZLEAKS

static thread_call_data_t zone_leaks_callout;

/*
 * The zone leak detector, abbreviated 'zleak', keeps track
 * of a subset of the currently outstanding allocations
 * made by the zone allocator.
 *
 * It will engage itself automatically if the zone map usage
 * goes above zleak_pages_global_wired_threshold pages.
 *
 * When that threshold is reached, zones who use more than
 * zleak_pages_per_zone_wired_threshold pages will get
 * a BTLOG_HASH btlog with sampling to minimize perf impact,
 * yet receive statistical data about the backtrace that is
 * the most likely to cause the leak.
 *
 * If the zone goes under the threshold enough, then the log
 * is disabled and backtraces freed. Data can be collected
 * from userspace with the zlog(1) command.
 */

/* whether the zleaks subsystem thinks the map is under pressure */
uint32_t                zleak_active;
SECURITY_READ_ONLY_LATE(vm_size_t) zleak_max_zonemap_size;

/* Size of zone map at which to start collecting data */
static size_t           zleak_pages_global_wired_threshold = ~0;
vm_size_t               zleak_global_tracking_threshold = ~0;

/* Size a zone will have before we will collect data on it */
static size_t           zleak_pages_per_zone_wired_threshold = ~0;
vm_size_t               zleak_per_zone_tracking_threshold = ~0;

static inline bool
zleak_should_enable_for_zone(zone_t z)
{
	if (z->z_log_on) {
		return false;
	}
	if (z->z_btlog) {
		return false;
	}
	if (!zleak_active) {
		return false;
	}
	return z->z_wired_cur >= zleak_pages_per_zone_wired_threshold;
}

static inline bool
zleak_should_disable_for_zone(zone_t z)
{
	if (z->z_log_on) {
		return false;
	}
	if (!z->z_btlog) {
		return false;
	}
	if (!zleak_active) {
		return true;
	}
	return z->z_wired_cur < zleak_pages_per_zone_wired_threshold / 2;
}

static inline bool
zleak_should_activate(size_t pages)
{
	return !zleak_active && pages >= zleak_pages_global_wired_threshold;
}

static inline bool
zleak_should_deactivate(size_t pages)
{
	return zleak_active && pages < zleak_pages_global_wired_threshold / 2;
}

static void
zleaks_enable_async(__unused thread_call_param_t p0, __unused thread_call_param_t p1)
{
	size_t pages = os_atomic_load(&zone_pages_wired, relaxed);
	btlog_t log;

	if (zleak_should_activate(pages)) {
		zleak_active = 1;
	} else if (zleak_should_deactivate(pages)) {
		zleak_active = 0;
	}

	zone_foreach(z) {
		if (zleak_should_disable_for_zone(z)) {
			log = z->z_btlog;
			z->z_btlog = NULL;
			assert(z->z_btlog_disabled == NULL);
			btlog_disable(log);
			z->z_btlog_disabled = log;
		}

		if (zleak_should_enable_for_zone(z)) {
			log = z->z_btlog_disabled;
			if (log == NULL) {
				log = btlog_create(BTLOG_HASH,
				    zone_leaks_record_count(z),
				    zone_leaks_sample_rate(z));
			} else {
				z->z_btlog_disabled = NULL;
				btlog_enable(log);
			}
			os_atomic_store(&z->z_btlog, log, release);
		}
	}
}

__startup_func
static void
zleak_init(void)
{
	zleak_max_zonemap_size = ptoa(zone_pages_wired_max);

	zleak_update_threshold(&zleak_global_tracking_threshold,
	    zleak_max_zonemap_size / 2);
	zleak_update_threshold(&zleak_per_zone_tracking_threshold,
	    zleak_global_tracking_threshold / 8);

	thread_call_setup_with_options(&zone_leaks_callout,
	    zleaks_enable_async, NULL, THREAD_CALL_PRIORITY_USER,
	    THREAD_CALL_OPTIONS_ONCE);
}
STARTUP(ZALLOC, STARTUP_RANK_SECOND, zleak_init);

kern_return_t
zleak_update_threshold(vm_size_t *arg, uint64_t value)
{
	if (value >= zleak_max_zonemap_size) {
		return KERN_INVALID_VALUE;
	}

	if (arg == &zleak_global_tracking_threshold) {
		zleak_global_tracking_threshold = (vm_size_t)value;
		zleak_pages_global_wired_threshold = atop(value);
		if (startup_phase >= STARTUP_SUB_THREAD_CALL) {
			thread_call_enter(&zone_leaks_callout);
		}
		return KERN_SUCCESS;
	}

	if (arg == &zleak_per_zone_tracking_threshold) {
		zleak_per_zone_tracking_threshold = (vm_size_t)value;
		zleak_pages_per_zone_wired_threshold = atop(value);
		if (startup_phase >= STARTUP_SUB_THREAD_CALL) {
			thread_call_enter(&zone_leaks_callout);
		}
		return KERN_SUCCESS;
	}

	return KERN_INVALID_ARGUMENT;
}

static void
panic_display_zleaks(bool has_syms)
{
	bool did_header = false;
	vm_address_t bt[BTLOG_MAX_DEPTH];
	uint32_t len, count;

	zone_foreach(z) {
		btlog_t log = z->z_btlog;

		if (log == NULL || btlog_get_type(log) != BTLOG_HASH) {
			continue;
		}

		count = btlog_guess_top(log, bt, &len);
		if (count == 0) {
			continue;
		}

		if (!did_header) {
			paniclog_append_noflush("Zone (suspected) leak report:\n");
			did_header = true;
		}

		paniclog_append_noflush("  Zone:    %s%s\n",
		    zone_heap_name(z), zone_name(z));
		paniclog_append_noflush("  Count:   %d (%ld bytes)\n", count,
		    (long)count * zone_scale_for_percpu(z, zone_elem_size(z)));
		paniclog_append_noflush("  Size:    %ld\n",
		    (long)zone_size_wired(z));
		paniclog_append_noflush("  Top backtrace:\n");
		for (uint32_t i = 0; i < len; i++) {
			if (has_syms) {
				paniclog_append_noflush("    %p ", (void *)bt[i]);
				panic_print_symbol_name(bt[i]);
				paniclog_append_noflush("\n");
			} else {
				paniclog_append_noflush("    %p\n", (void *)bt[i]);
			}
		}

		kmod_panic_dump(bt, len);
		paniclog_append_noflush("\n");
	}
}
#endif /* CONFIG_ZLEAKS */

static void
zalloc_log(btlog_t log, vm_offset_t addr, zalloc_flags_t flags, void *fp)
{
	btref_t ref;

	if (btlog_sample(log)) {
		ref = btref_get(fp, (flags & Z_NOWAIT) ? BTREF_GET_NOWAIT : 0);
		btlog_record(log, (void *)addr, ZOP_ALLOC, ref);
	}
}

static void
zfree_log(btlog_t log, vm_offset_t addr, void *fp)
{
	/*
	 * See if we're doing logging on this zone.
	 *
	 * There are two styles of logging used depending on
	 * whether we're trying to catch a leak or corruption.
	 */
	if (btlog_get_type(log) == BTLOG_LOG) {
		/*
		 * We're logging to catch a corruption.
		 *
		 * Add a record of this zfree operation to log.
		 */
		btlog_record(log, (void *)addr, ZOP_FREE,
		    btref_get(fp, BTREF_GET_NOWAIT));
	} else {
		/*
		 * We're logging to catch a leak.
		 *
		 * Remove any record we might have for this element
		 * since it's being freed.  Note that we may not find it
		 * if the buffer overflowed and that's OK.
		 *
		 * Since the log is of a limited size, old records get
		 * overwritten if there are more zallocs than zfrees.
		 */
		btlog_erase(log, (void *)addr);
	}
}

#endif /* ZONE_ENABLE_LOGGING || CONFIG_ZLEAKS */
#endif /* !ZALLOC_TEST */
#pragma mark zone (re)fill
#if !ZALLOC_TEST

/*!
 * @defgroup Zone Refill
 * @{
 *
 * @brief
 * Functions handling The zone refill machinery.
 *
 * @discussion
 * Zones are refilled based on 2 mechanisms: direct expansion, async expansion.
 *
 * @c zalloc_ext() is the codepath that kicks the zone refill when the zone is
 * dropping below half of its @c z_elems_rsv (0 for most zones) and will:
 *
 * - call @c zone_expand_locked() directly if the caller is allowed to block,
 *
 * - wakeup the asynchroous expansion thread call if the caller is not allowed
 *   to block, or if the reserve becomes depleted.
 *
 *
 * <h2>Synchronous expansion</h2>
 *
 * This mechanism is actually the only one that may refill a zone, and all the
 * other ones funnel through this one eventually.
 *
 * @c zone_expand_locked() implements the core of the expansion mechanism,
 * and will do so while a caller specified predicate is true.
 *
 * Zone expansion allows for up to 2 threads to concurrently refill the zone:
 * - one VM privileged thread,
 * - one regular thread.
 *
 * Regular threads that refill will put down their identity in @c z_expander,
 * so that priority inversion avoidance can be implemented.
 *
 * However, VM privileged threads are allowed to use VM page reserves,
 * which allows for the system to recover from extreme memory pressure
 * situations, allowing for the few allocations that @c zone_gc() or
 * killing processes require.
 *
 * When a VM privileged thread is also expanding, the @c z_expander_vm_priv bit
 * is set. @c z_expander is not necessarily the identity of this VM privileged
 * thread (it is if the VM privileged thread came in first, but wouldn't be, and
 * could even be @c THREAD_NULL otherwise).
 *
 * Note that the pageout-scan daemon might be BG and is VM privileged. To avoid
 * spending a whole pointer on priority inheritance for VM privileged threads
 * (and other issues related to having two owners), we use the rwlock boost as
 * a stop gap to avoid priority inversions.
 *
 *
 * <h2>Chunk wiring policies</h2>
 *
 * Zones allocate memory in chunks of @c zone_t::z_chunk_pages pages at a time
 * to try to minimize fragmentation relative to element sizes not aligning with
 * a chunk size well.  However, this can grow large and be hard to fulfill on
 * a system under a lot of memory pressure (chunks can be as long as 8 pages on
 * 4k page systems).
 *
 * This is why, when under memory pressure the system allows chunks to be
 * partially populated. The metadata of the first page in the chunk maintains
 * the count of actually populated pages.
 *
 * The metadata for addresses assigned to a zone are found of 4 queues:
 * - @c z_pageq_empty has chunk heads with populated pages and no allocated
 *   elements (those can be targeted by @c zone_gc()),
 * - @c z_pageq_partial has chunk heads with populated pages that are partially
 *   used,
 * - @c z_pageq_full has chunk heads with populated pages with no free elements
 *   left,
 * - @c z_pageq_va has either chunk heads for sequestered VA space assigned to
 *   the zone forever (if @c z_va_sequester is enabled), or the first secondary
 *   metadata for a chunk whose corresponding page is not populated in the
 *   chunk.
 *
 * When new pages need to be wired/populated, chunks from the @c z_pageq_va
 * queues are preferred.
 *
 *
 * <h2>Asynchronous expansion</h2>
 *
 * This mechanism allows for refilling zones used mostly with non blocking
 * callers. It relies on a thread call (@c zone_expand_callout) which will
 * iterate all zones and refill the ones marked with @c z_async_refilling.
 *
 * NOTE: If the calling thread for zalloc_noblock is lower priority than
 *       the thread_call, then zalloc_noblock to an empty zone may succeed.
 *
 *
 * <h2>Dealing with zone allocations from the mach VM code</h2>
 *
 * The implementation of the mach VM itself uses the zone allocator
 * for things like the vm_map_entry data structure. In order to prevent
 * a recursion problem when adding more pages to a zone, the VM zones
 * use the Z_SUBMAP_IDX_VM submap which doesn't use kernel_memory_allocate()
 * or any VM map functions to allocate.
 *
 * Instead, a really simple coalescing first-fit allocator is used
 * for this submap, and no one else than zalloc can allocate from it.
 *
 * Memory is directly populated which doesn't require allocation of
 * VM map entries, and avoids recursion. The cost of this scheme however,
 * is that `vm_map_lookup_entry` will not function on those addresses
 * (nor any API relying on it).
 */

static thread_call_data_t zone_expand_callout;

static inline kma_flags_t
zone_kma_flags(zone_t z, zone_security_flags_t zsflags, zalloc_flags_t flags)
{
	kma_flags_t kmaflags = KMA_KOBJECT | KMA_ZERO;

	if (zsflags.z_noencrypt) {
		kmaflags |= KMA_NOENCRYPT;
	}
	if (flags & Z_NOPAGEWAIT) {
		kmaflags |= KMA_NOPAGEWAIT;
	}
	if (z->z_permanent || (!z->z_destructible && zsflags.z_va_sequester)) {
		kmaflags |= KMA_PERMANENT;
	}
	if (zsflags.z_submap_from_end) {
		kmaflags |= KMA_LAST_FREE;
	}

	return kmaflags;
}

static inline void
zone_add_wired_pages(uint32_t pages)
{
	size_t count = os_atomic_add(&zone_pages_wired, pages, relaxed);

#if CONFIG_ZLEAKS
	if (__improbable(zleak_should_activate(count) &&
	    startup_phase >= STARTUP_SUB_THREAD_CALL)) {
		thread_call_enter(&zone_leaks_callout);
	}
#else
	(void)count;
#endif
}

static inline void
zone_remove_wired_pages(uint32_t pages)
{
	size_t count = os_atomic_sub(&zone_pages_wired, pages, relaxed);

#if CONFIG_ZLEAKS
	if (__improbable(zleak_should_deactivate(count) &&
	    startup_phase >= STARTUP_SUB_THREAD_CALL)) {
		thread_call_enter(&zone_leaks_callout);
	}
#else
	(void)count;
#endif
}

/*!
 * @function zcram_and_lock()
 *
 * @brief
 * Prepare some memory for being usable for allocation purposes.
 *
 * @discussion
 * Prepare memory in <code>[addr + ptoa(pg_start), addr + ptoa(pg_end))</code>
 * to be usable in the zone.
 *
 * This function assumes the metadata is already populated for the range.
 *
 * Calling this function with @c pg_start being 0 means that the memory
 * is either a partial chunk, or a full chunk, that isn't published anywhere
 * and the initialization can happen without locks held.
 *
 * Calling this function with a non zero @c pg_start means that we are extending
 * an existing chunk: the memory in <code>[addr, addr + ptoa(pg_start))</code>,
 * is already usable and published in the zone, so extending it requires holding
 * the zone lock.
 *
 * @param zone          The zone to cram new populated pages into
 * @param addr          The base address for the chunk(s)
 * @param pg_va_new     The number of virtual pages newly assigned to the zone
 * @param pg_start      The first newly populated page relative to @a addr.
 * @param pg_end        The after-last newly populated page relative to @a addr.
 * @param kind          The kind of memory assigned to the zone.
 */
static void
zcram_and_lock(zone_t zone, vm_offset_t addr, uint32_t pg_va_new,
    uint32_t pg_start, uint32_t pg_end, zone_addr_kind_t kind)
{
	zone_id_t zindex = zone_index(zone);
	vm_offset_t elem_size = zone_elem_size_safe(zone);
	uint32_t free_start = 0, free_end = 0;
	uint32_t oob_offs = zone_oob_offs(zone);

	struct zone_page_metadata *meta = zone_meta_from_addr(addr);
	uint32_t chunk_pages = zone->z_chunk_pages;

	assert(pg_start < pg_end && pg_end <= chunk_pages);

	if (pg_start == 0) {
		uint16_t chunk_len = (uint16_t)pg_end;
		uint16_t secondary_len = ZM_SECONDARY_PAGE;
		bool inline_bitmap = false;

		if (zone->z_percpu) {
			chunk_len = 1;
			secondary_len = ZM_SECONDARY_PCPU_PAGE;
			assert(pg_end == zpercpu_count());
		}
		if (!zone->z_permanent) {
			inline_bitmap = zone->z_chunk_elems <= 32 * chunk_pages;
		}

		meta[0] = (struct zone_page_metadata){
			.zm_index         = zindex,
			.zm_inline_bitmap = inline_bitmap,
			.zm_chunk_len     = chunk_len,
		};
		if (kind == ZONE_ADDR_FOREIGN) {
			/* Never hit z_pageq_empty */
			meta[0].zm_alloc_size = ZM_ALLOC_SIZE_LOCK;
		}

		for (uint16_t i = 1; i < chunk_pages; i++) {
			meta[i] = (struct zone_page_metadata){
				.zm_index          = zindex,
				.zm_inline_bitmap  = inline_bitmap,
				.zm_chunk_len      = secondary_len,
				.zm_page_index     = i,
			};
		}

		free_end = (uint32_t)(ptoa(chunk_len) - oob_offs) / elem_size;
		if (!zone->z_permanent) {
			zone_meta_bits_init(meta, free_end, zone->z_chunk_elems);
		}
	} else {
		assert(!zone->z_percpu && !zone->z_permanent);

		free_end = (uint32_t)(ptoa(pg_end) - oob_offs) / elem_size;
		free_start = (uint32_t)(ptoa(pg_start) - oob_offs) / elem_size;
	}

#if VM_TAG_SIZECLASSES
	if (__improbable(zone->z_uses_tags)) {
		assert(kind == ZONE_ADDR_NATIVE && !zone->z_percpu);
		ztMemoryAdd(zone, addr + ptoa(pg_start),
		    ptoa(pg_end - pg_start));
	}
#endif /* VM_TAG_SIZECLASSES */

	/*
	 * Insert the initialized pages / metadatas into the right lists.
	 */

	zone_lock(zone);
	assert(zone->z_self == zone);

	if (pg_start != 0) {
		assert(meta->zm_chunk_len == pg_start);

		zone_meta_bits_merge(meta, free_start, free_end);
		meta->zm_chunk_len = (uint16_t)pg_end;

		/*
		 * consume the zone_meta_lock_in_partial()
		 * done in zone_expand_locked()
		 */
		zone_meta_alloc_size_sub(zone, meta, ZM_ALLOC_SIZE_LOCK);
		zone_meta_remqueue(zone, meta);
	}

	if (zone->z_permanent || meta->zm_alloc_size) {
		zone_meta_queue_push(zone, &zone->z_pageq_partial, meta);
	} else {
		zone_meta_queue_push(zone, &zone->z_pageq_empty, meta);
		zone->z_wired_empty += zone->z_percpu ? 1 : pg_end;
	}
	if (pg_end < chunk_pages) {
		/* push any non populated residual VA on z_pageq_va */
		zone_meta_queue_push(zone, &zone->z_pageq_va, meta + pg_end);
	}

	zone_elems_free_add(zone, free_end - free_start);
	zone->z_elems_avail += free_end - free_start;
	zone->z_wired_cur   += zone->z_percpu ? 1 : pg_end - pg_start;
	if (pg_va_new) {
		zone->z_va_cur += zone->z_percpu ? 1 : pg_va_new;
	}
	if (zone->z_wired_hwm < zone->z_wired_cur) {
		zone->z_wired_hwm = zone->z_wired_cur;
	}

#if CONFIG_ZLEAKS
	if (__improbable(zleak_should_enable_for_zone(zone) &&
	    startup_phase >= STARTUP_SUB_THREAD_CALL)) {
		thread_call_enter(&zone_leaks_callout);
	}
#endif /* CONFIG_ZLEAKS */

	zone_add_wired_pages(pg_end - pg_start);
}

static void
zcram(zone_t zone, vm_offset_t addr, uint32_t pages, zone_addr_kind_t kind)
{
	uint32_t chunk_pages = zone->z_chunk_pages;

	assert(pages % chunk_pages == 0);
	for (; pages > 0; pages -= chunk_pages, addr += ptoa(chunk_pages)) {
		zcram_and_lock(zone, addr, chunk_pages, 0, chunk_pages, kind);
		zone_unlock(zone);
	}
}

void
zone_cram_foreign(zone_t zone, vm_offset_t newmem, vm_size_t size)
{
	uint32_t pages = (uint32_t)atop(size);
	zone_security_flags_t zsflags = zone_security_config(zone);

	if (!from_zone_map(newmem, size, ZONE_ADDR_FOREIGN)) {
		panic("zone_cram_foreign: foreign memory [%p] being crammed is "
		    "outside of expected range", (void *)newmem);
	}
	if (!zsflags.z_allows_foreign) {
		panic("zone_cram_foreign: foreign memory [%p] being crammed in "
		    "zone '%s%s' not expecting it", (void *)newmem,
		    zone_heap_name(zone), zone_name(zone));
	}
	if (size % ptoa(zone->z_chunk_pages)) {
		panic("zone_cram_foreign: foreign memory [%p] being crammed has "
		    "invalid size %zx", (void *)newmem, (size_t)size);
	}
	if (startup_phase >= STARTUP_SUB_ZALLOC) {
		panic("zone_cram_foreign: foreign memory [%p] being crammed "
		    "after zalloc is initialized", (void *)newmem);
	}

	bzero((void *)newmem, size);
	zcram(zone, newmem, pages, ZONE_ADDR_FOREIGN);
}

__attribute__((overloadable))
static inline bool
zone_submap_is_sequestered(zone_submap_idx_t idx)
{
	switch (idx) {
	case Z_SUBMAP_IDX_READ_ONLY:
	case Z_SUBMAP_IDX_VM:
		return true;
	case Z_SUBMAP_IDX_DATA:
		return false;
	default:
		return ZSECURITY_CONFIG(SEQUESTER);
	}
}

__attribute__((overloadable))
static inline bool
zone_submap_is_sequestered(zone_security_flags_t zsflags)
{
	return zone_submap_is_sequestered(zsflags.z_submap_idx);
}

/*!
 * @function zone_submap_alloc_sequestered_va
 *
 * @brief
 * Allocates VA without using vm_find_space().
 *
 * @discussion
 * Allocate VA quickly without using the slower vm_find_space() for cases
 * when the submaps are fully sequestered.
 *
 * The VM submap is used to implement the VM itself so it is always sequestered,
 * as it can't kernel_memory_allocate which needs to always allocate vm entries.
 * However, it can use vm_map_enter() which tries to coalesce entries, which
 * always works, so the VM map only ever needs 2 entries (one for each end).
 *
 * The RO submap is similarly always sequestered if it exists (as a non
 * sequestered RO submap makes very little sense).
 *
 * The allocator is a very simple bump-allocator
 * that allocates from either end.
 */
static kern_return_t
zone_submap_alloc_sequestered_va(zone_security_flags_t zsflags, uint32_t pages,
    vm_offset_t *addrp)
{
	vm_size_t size = ptoa(pages);
	vm_map_t map = zone_submap(zsflags);
	vm_map_entry_t first, last;
	vm_map_offset_t addr;

	vm_map_lock(map);

	first = vm_map_first_entry(map);
	last = vm_map_last_entry(map);

	if (first->vme_end + size > last->vme_start) {
		vm_map_unlock(map);
		return KERN_NO_SPACE;
	}

	if (zsflags.z_submap_from_end) {
		last->vme_start -= size;
		addr = last->vme_start;
		VME_OFFSET_SET(last, addr);
	} else {
		addr = first->vme_end;
		first->vme_end += size;
	}
	map->size += size;

	vm_map_unlock(map);

	*addrp = addr;
	return KERN_SUCCESS;
}

void
zone_fill_initially(zone_t zone, vm_size_t nelems)
{
	kma_flags_t kmaflags;
	kern_return_t kr;
	vm_offset_t addr;
	uint32_t pages;
	zone_security_flags_t zsflags = zone_security_config(zone);

	assert(!zone->z_permanent && !zone->collectable && !zone->z_destructible);
	assert(zone->z_elems_avail == 0);

	kmaflags = zone_kma_flags(zone, zsflags, Z_WAITOK) | KMA_PERMANENT;
	pages = zone_alloc_pages_for_nelems(zone, nelems);
	if (zone_submap_is_sequestered(zsflags)) {
		kr = zone_submap_alloc_sequestered_va(zsflags, pages, &addr);
		if (kr == KERN_SUCCESS) {
			kr = kernel_memory_populate(zone_submap(zsflags), addr,
			    ptoa(pages), kmaflags, VM_KERN_MEMORY_ZONE);
		}
	} else {
		assert(zsflags.z_submap_idx != Z_SUBMAP_IDX_READ_ONLY);
		kr = kernel_memory_allocate(zone_submap(zsflags), &addr,
		    ptoa(pages), 0, kmaflags, VM_KERN_MEMORY_ZONE);
	}

	if (kr != KERN_SUCCESS) {
		panic("kernel_memory_allocate() of %u pages failed", pages);
	}

	zone_meta_populate(addr, ptoa(pages));
	zcram(zone, addr, pages, ZONE_ADDR_NATIVE);
}

#if ZSECURITY_CONFIG(SAD_FENG_SHUI)
__attribute__((noinline))
static void
zone_scramble_va_and_unlock(
	zone_t                      z,
	struct zone_page_metadata  *meta,
	uint32_t                    runs,
	uint32_t                    pages,
	uint32_t                    chunk_pages)
{
	struct zone_page_metadata *arr[ZONE_CHUNK_ALLOC_SIZE / 4096];

	/*
	 * Fisher–Yates shuffle, for an array with indices [0, n)
	 *
	 * for i from n−1 downto 1 do
	 *     j ← random integer such that 0 ≤ j ≤ i
	 *     exchange a[j] and a[i]
	 *
	 * The point here is that early allocations aren't at a fixed
	 * distance from each other.
	 */
	for (uint32_t i = 0, j = 0; i < runs; i++, j += chunk_pages) {
		arr[i] = meta + j;
	}

	for (uint32_t i = runs - 1; i > 0; i--) {
		uint32_t j = zalloc_random_uniform(0, i + 1);

		meta   = arr[j];
		arr[j] = arr[i];
		arr[i] = meta;
	}

	zone_lock(z);

	for (uint32_t i = 0; i < runs; i++) {
		zone_meta_queue_push(z, &z->z_pageq_va, arr[i]);
	}
	z->z_va_cur += z->z_percpu ? runs : pages;
}
#endif

static void
zone_allocate_va_locked(zone_t z, zalloc_flags_t flags)
{
	zone_security_flags_t zsflags = zone_security_config(z);
	struct zone_page_metadata *meta;
	kma_flags_t kmaflags = zone_kma_flags(z, zsflags, flags) | KMA_VAONLY;
	uint32_t chunk_pages = z->z_chunk_pages;
	uint32_t runs, pages, guards;
	kern_return_t kr;
	vm_offset_t addr;

	zone_unlock(z);

	pages  = chunk_pages;
	guards = 0;
	runs   = 1;
#if ZSECURITY_CONFIG(SAD_FENG_SHUI)
	if (!z->z_percpu && zone_submap_is_sequestered(zsflags)) {
		pages = roundup(atop(ZONE_CHUNK_ALLOC_SIZE), chunk_pages);
		runs  = pages / chunk_pages;
	}
#endif /* !ZSECURITY_CONFIG(SAD_FENG_SHUI) */
#if CONFIG_PROB_GZALLOC
	/*
	 * For configuration with probabilistic guard zalloc,
	 * in zones that opt-in (kalloc, ZC_PGZ_USE_GUARDS),
	 * add a guard page after each chunk.
	 *
	 * Those guard pages are marked with the ZM_PGZ_GUARD
	 * magical chunk len, and their zm_oob_offs field
	 * is used to remember optional shift applied
	 * to returned elements, in order to right-pad-them
	 * as much as possible.
	 *
	 * (see kalloc_ext, zone_element_size, ...).
	 */
	if (z->z_pgz_use_guards) {
		guards = runs;
		chunk_pages++;
	}
#endif /* CONFIG_PROB_GZALLOC */

	if (zone_submap_is_sequestered(zsflags)) {
		kr = zone_submap_alloc_sequestered_va(zsflags,
		    pages + guards, &addr);
	} else {
		assert(zsflags.z_submap_idx != Z_SUBMAP_IDX_READ_ONLY);
		kr = kernel_memory_allocate(zone_submap(zsflags), &addr,
		    ptoa(pages + guards), 0, kmaflags, VM_KERN_MEMORY_ZONE);
	}

	if (kr != KERN_SUCCESS) {
		uint64_t zone_size = 0;
		zone_t zone_largest = zone_find_largest(&zone_size);
		panic("zalloc[%d]: zone map exhausted while allocating from zone [%s%s], "
		    "likely due to memory leak in zone [%s%s] "
		    "(%luM, %d elements allocated)",
		    kr, zone_heap_name(z), zone_name(z),
		    zone_heap_name(zone_largest), zone_name(zone_largest),
		    (unsigned long)zone_size >> 20,
		    zone_count_allocated(zone_largest));
	}

	meta = zone_meta_from_addr(addr);
	zone_meta_populate(addr, ptoa(pages + guards));

	for (uint32_t i = 0; i < pages + guards; i++) {
		meta[i].zm_index = zone_index(z);
	}
#if CONFIG_PROB_GZALLOC
	if (guards) {
		for (uint32_t i = 0; i < pages + guards; i += chunk_pages) {
			meta[i + chunk_pages - 1].zm_chunk_len = ZM_PGZ_GUARD;
		}
		os_atomic_add(&zone_guard_pages, guards, relaxed);
	}
#endif /* CONFIG_PROB_GZALLOC */

#if ZSECURITY_CONFIG(SAD_FENG_SHUI)
	if (__improbable(zone_caching_disabled < 0)) {
		return zone_scramble_va_and_unlock(z, meta, runs, pages, chunk_pages);
	}
#endif /* ZSECURITY_CONFIG(SAD_FENG_SHUI) */

	zone_lock(z);

	for (uint32_t i = 0; i < pages + guards; i += chunk_pages) {
		zone_meta_queue_push(z, &z->z_pageq_va, meta + i);
	}
	z->z_va_cur += z->z_percpu ? runs : pages;
}

static bool
zone_expand_pred_nope(__unused zone_t z)
{
	return false;
}

static inline void
ZONE_TRACE_VM_KERN_REQUEST_START(vm_size_t size)
{
#if DEBUG || DEVELOPMENT
	VM_DEBUG_CONSTANT_EVENT(vm_kern_request, VM_KERN_REQUEST, DBG_FUNC_START,
	    size, 0, 0, 0);
#else
	(void)size;
#endif
}

static inline void
ZONE_TRACE_VM_KERN_REQUEST_END(uint32_t pages)
{
#if DEBUG || DEVELOPMENT
	task_t task = current_task_early();
	if (pages && task) {
		ledger_credit(task->ledger, task_ledgers.pages_grabbed_kern, pages);
	}
	VM_DEBUG_CONSTANT_EVENT(vm_kern_request, VM_KERN_REQUEST, DBG_FUNC_END,
	    pages, 0, 0, 0);
#else
	(void)pages;
#endif
}

__attribute__((noinline))
static void
__ZONE_MAP_EXHAUSTED_AND_WAITING_FOR_GC__(zone_t z, uint32_t pgs)
{
	uint64_t wait_start = 0;
	long mapped;
	zone_security_flags_t zsflags = zone_security_config(z);

	thread_wakeup(VM_PAGEOUT_GC_EVENT);

	if (zsflags.z_allows_foreign || current_thread()->options & TH_OPT_VMPRIV) {
		/*
		 * "allow foreign" zones are allowed to overcommit
		 * because they're used to reclaim memory (VM support).
		 */
		return;
	}

	mapped = os_atomic_load(&zone_pages_wired, relaxed);

	/*
	 * If the zone map is really exhausted, wait on the GC thread,
	 * donating our priority (which is important because the GC
	 * thread is at a rather low priority).
	 */
	for (uint32_t n = 1; mapped >= zone_pages_wired_max - pgs; n++) {
		uint32_t wait_ms = n * (n + 1) / 2;
		uint64_t interval;

		if (n == 1) {
			wait_start = mach_absolute_time();
		} else {
			thread_wakeup(VM_PAGEOUT_GC_EVENT);
		}
		if (zone_exhausted_timeout > 0 &&
		    wait_ms > zone_exhausted_timeout) {
			panic("zone map exhaustion: waited for %dms "
			    "(pages: %ld, max: %ld, wanted: %d)",
			    wait_ms, mapped, zone_pages_wired_max, pgs);
		}

		clock_interval_to_absolutetime_interval(wait_ms, NSEC_PER_MSEC,
		    &interval);

		lck_spin_lock(&zone_exhausted_lock);
		lck_spin_sleep_with_inheritor(&zone_exhausted_lock,
		    LCK_SLEEP_UNLOCK, &zone_pages_wired,
		    vm_pageout_gc_thread, THREAD_UNINT, wait_start + interval);

		mapped = os_atomic_load(&zone_pages_wired, relaxed);
	}
}

static bool
zone_expand_wait_for_pages(bool waited)
{
	if (waited) {
		return false;
	}
#if DEBUG || DEVELOPMENT
	if (zalloc_simulate_vm_pressure) {
		return false;
	}
#endif /* DEBUG || DEVELOPMENT */
	return !vm_pool_low();
}

static void
zone_expand_locked(zone_t z, zalloc_flags_t flags, bool (*pred)(zone_t))
{
	thread_t self = current_thread();
	bool vm_priv = (self->options & TH_OPT_VMPRIV);
	bool clear_vm_priv;
	thread_pri_floor_t token;
	zone_security_flags_t zsflags = zone_security_config(z);

	for (;;) {
		if (!pred) {
			/* NULL pred means "try just once" */
			pred = zone_expand_pred_nope;
		} else if (!pred(z)) {
			return;
		}

		if (vm_priv && !z->z_expander_vm_priv) {
			/*
			 * Claim the vm priv overcommit slot
			 *
			 * We do not track exact ownership for VM privileged
			 * threads, so use the rwlock boost as a stop-gap
			 * just in case.
			 */
			token = thread_priority_floor_start();
			z->z_expander_vm_priv = true;
			clear_vm_priv = true;
		} else {
			clear_vm_priv = false;
		}

		if (z->z_expander == NULL) {
			z->z_expander = self;
			break;
		}
		if (clear_vm_priv) {
			break;
		}

		if (flags & Z_NOPAGEWAIT) {
			return;
		}

		z->z_expanding_wait = true;
		lck_spin_sleep_with_inheritor(&z->z_lock, LCK_SLEEP_DEFAULT,
		    &z->z_expander, z->z_expander,
		    TH_UNINT, TIMEOUT_WAIT_FOREVER);
	}

	do {
		struct zone_page_metadata *meta = NULL;
		uint32_t new_va = 0, cur_pages = 0, min_pages = 0, pages = 0;
		vm_page_t page_list = NULL;
		vm_offset_t addr = 0;
		int waited = 0;

		/*
		 * While we hold the zone lock, look if there's VA we can:
		 * - complete from partial pages,
		 * - reuse from the sequester list.
		 *
		 * When the page is being populated we pretend we allocated
		 * an extra element so that zone_gc() can't attempt to free
		 * the chunk (as it could become empty while we wait for pages).
		 */
		if (zone_pva_is_null(z->z_pageq_va)) {
			zone_allocate_va_locked(z, flags);
		}

		meta = zone_meta_queue_pop_native(z, &z->z_pageq_va, &addr);
		if (meta->zm_chunk_len == ZM_SECONDARY_PAGE) {
			cur_pages = meta->zm_page_index;
			meta -= cur_pages;
			addr -= ptoa(cur_pages);
			zone_meta_lock_in_partial(z, meta, cur_pages);
		}
		zone_unlock(z);

		/*
		 * And now allocate pages to populate our VA.
		 */
		if (z->z_percpu) {
			min_pages = z->z_chunk_pages;
		} else {
			min_pages = (uint32_t)atop(round_page(zone_oob_offs(z) +
			    zone_elem_size(z)));
		}

		/*
		 * Trigger jetsams via VM_PAGEOUT_GC_EVENT
		 * if we're running out of zone memory
		 */
		if (__improbable(zone_map_nearing_exhaustion())) {
			__ZONE_MAP_EXHAUSTED_AND_WAITING_FOR_GC__(z, min_pages);
		}

		ZONE_TRACE_VM_KERN_REQUEST_START(ptoa(z->z_chunk_pages - cur_pages));

		while (pages < z->z_chunk_pages - cur_pages) {
			vm_page_t m = vm_page_grab();

			if (m) {
				pages++;
				m->vmp_snext = page_list;
				page_list = m;
				vm_page_zero_fill(m);
				continue;
			}

			if (pages >= min_pages &&
			    !zone_expand_wait_for_pages(waited)) {
				break;
			}

			if ((flags & Z_NOPAGEWAIT) == 0) {
				waited++;
				VM_PAGE_WAIT();
				continue;
			}

			/*
			 * Undo everything and bail out:
			 *
			 * - free pages
			 * - undo the fake allocation if any
			 * - put the VA back on the VA page queue.
			 */
			vm_page_free_list(page_list, FALSE);
			ZONE_TRACE_VM_KERN_REQUEST_END(pages);

			zone_lock(z);

			if (cur_pages) {
				zone_meta_unlock_from_partial(z, meta, cur_pages);
			}
			if (meta) {
				zone_meta_queue_push(z, &z->z_pageq_va,
				    meta + cur_pages);
			}
			goto page_shortage;
		}

		kernel_memory_populate_with_pages(zone_submap(zsflags),
		    addr + ptoa(cur_pages), ptoa(pages), page_list,
		    zone_kma_flags(z, zsflags, flags), VM_KERN_MEMORY_ZONE,
		    (zsflags.z_submap_idx == Z_SUBMAP_IDX_READ_ONLY) ? VM_PROT_READ : VM_PROT_READ | VM_PROT_WRITE);

		ZONE_TRACE_VM_KERN_REQUEST_END(pages);

		zcram_and_lock(z, addr, new_va, cur_pages, cur_pages + pages,
		    ZONE_ADDR_NATIVE);
	} while (pred(z));

page_shortage:
	if (clear_vm_priv) {
		z->z_expander_vm_priv = false;
		thread_priority_floor_end(&token);
	}
	if (z->z_expander == self) {
		z->z_expander = THREAD_NULL;
	}
	if (z->z_expanding_wait) {
		z->z_expanding_wait = false;
		wakeup_all_with_inheritor(&z->z_expander, THREAD_AWAKENED);
	}
}

static bool
zalloc_needs_refill(zone_t zone)
{
	if (zone->z_elems_free > zone->z_elems_rsv) {
		return false;
	}
	if (zone->z_wired_cur < zone->z_wired_max) {
		return true;
	}
	if (zone->exhaustible) {
		return false;
	}
	if (zone->expandable) {
		/*
		 * If we're expandable, just don't go through this again.
		 */
		zone->z_wired_max = ~0u;
		return true;
	}
	zone_unlock(zone);

	panic("zone '%s%s' exhausted", zone_heap_name(zone), zone_name(zone));
}

static void
zone_expand_async(__unused thread_call_param_t p0, __unused thread_call_param_t p1)
{
	zone_foreach(z) {
		if (z->no_callout) {
			/* z_async_refilling will never be set */
			continue;
		}

		zone_lock(z);
		if (z->z_self && z->z_async_refilling) {
			z->z_async_refilling = false;
			zone_expand_locked(z, Z_WAITOK, zalloc_needs_refill);
		}
		zone_unlock(z);
	}
}

static inline void
zone_expand_async_schedule_if_needed(zone_t zone)
{
	if (__improbable(startup_phase < STARTUP_SUB_THREAD_CALL)) {
		return;
	}

	if (zone->z_elems_free > zone->z_elems_rsv || zone->z_async_refilling ||
	    zone->no_callout) {
		return;
	}

	if (!zone->expandable && zone->z_wired_cur >= zone->z_wired_max) {
		return;
	}

	if (startup_phase < STARTUP_SUB_EARLY_BOOT) {
		return;
	}

	if (zone->z_elems_free == 0 || !vm_pool_low()) {
		zone->z_async_refilling = true;
		thread_call_enter(&zone_expand_callout);
	}
}

#endif /* !ZALLOC_TEST */
#pragma mark zone jetsam integration
#if !ZALLOC_TEST

/*
 * We're being very conservative here and picking a value of 95%. We might need to lower this if
 * we find that we're not catching the problem and are still hitting zone map exhaustion panics.
 */
#define ZONE_MAP_JETSAM_LIMIT_DEFAULT 95

/*
 * Threshold above which largest zones should be included in the panic log
 */
#define ZONE_MAP_EXHAUSTION_PRINT_PANIC 80

/*
 * Trigger zone-map-exhaustion jetsams if the zone map is X% full,
 * where X=zone_map_jetsam_limit.
 *
 * Can be set via boot-arg "zone_map_jetsam_limit". Set to 95% by default.
 */
TUNABLE_WRITEABLE(unsigned int, zone_map_jetsam_limit, "zone_map_jetsam_limit",
    ZONE_MAP_JETSAM_LIMIT_DEFAULT);

kern_return_t
zone_map_jetsam_set_limit(uint32_t value)
{
	if (value <= 0 || value > 100) {
		return KERN_INVALID_VALUE;
	}

	zone_map_jetsam_limit = value;
	os_atomic_store(&zone_pages_jetsam_threshold,
	    zone_pages_wired_max * value / 100, relaxed);
	return KERN_SUCCESS;
}

void
get_zone_map_size(uint64_t *current_size, uint64_t *capacity)
{
	vm_offset_t phys_pages = os_atomic_load(&zone_pages_wired, relaxed);
	*current_size = ptoa_64(phys_pages);
	*capacity = ptoa_64(zone_pages_wired_max);
}

void
get_largest_zone_info(char *zone_name, size_t zone_name_len, uint64_t *zone_size)
{
	zone_t largest_zone = zone_find_largest(zone_size);

	/*
	 * Append kalloc heap name to zone name (if zone is used by kalloc)
	 */
	snprintf(zone_name, zone_name_len, "%s%s",
	    zone_heap_name(largest_zone), largest_zone->z_name);
}

static bool
zone_map_nearing_threshold(unsigned int threshold)
{
	uint64_t phys_pages = os_atomic_load(&zone_pages_wired, relaxed);
	return phys_pages * 100 > zone_pages_wired_max * threshold;
}

bool
zone_map_nearing_exhaustion(void)
{
	vm_size_t pages = os_atomic_load(&zone_pages_wired, relaxed);

	return pages >= os_atomic_load(&zone_pages_jetsam_threshold, relaxed);
}


#define VMENTRY_TO_VMOBJECT_COMPARISON_RATIO 98

/*
 * Tries to kill a single process if it can attribute one to the largest zone. If not, wakes up the memorystatus thread
 * to walk through the jetsam priority bands and kill processes.
 */
static zone_t
kill_process_in_largest_zone(void)
{
	pid_t pid = -1;
	uint64_t zone_size = 0;
	zone_t largest_zone = zone_find_largest(&zone_size);

	printf("zone_map_exhaustion: Zone mapped %lld of %lld, used %lld, capacity %lld [jetsam limit %d%%]\n",
	    ptoa_64(os_atomic_load(&zone_pages_wired, relaxed)),
	    ptoa_64(zone_pages_wired_max),
	    (uint64_t)zone_submaps_approx_size(),
	    (uint64_t)(zone_foreign_size() + zone_native_size()),
	    zone_map_jetsam_limit);
	printf("zone_map_exhaustion: Largest zone %s%s, size %lu\n", zone_heap_name(largest_zone),
	    largest_zone->z_name, (uintptr_t)zone_size);

	/*
	 * We want to make sure we don't call this function from userspace.
	 * Or we could end up trying to synchronously kill the process
	 * whose context we're in, causing the system to hang.
	 */
	assert(current_task() == kernel_task);

	/*
	 * If vm_object_zone is the largest, check to see if the number of
	 * elements in vm_map_entry_zone is comparable.
	 *
	 * If so, consider vm_map_entry_zone as the largest. This lets us target
	 * a specific process to jetsam to quickly recover from the zone map
	 * bloat.
	 */
	if (largest_zone == vm_object_zone) {
		unsigned int vm_object_zone_count = zone_count_allocated(vm_object_zone);
		unsigned int vm_map_entry_zone_count = zone_count_allocated(vm_map_entry_zone);
		/* Is the VM map entries zone count >= 98% of the VM objects zone count? */
		if (vm_map_entry_zone_count >= ((vm_object_zone_count * VMENTRY_TO_VMOBJECT_COMPARISON_RATIO) / 100)) {
			largest_zone = vm_map_entry_zone;
			printf("zone_map_exhaustion: Picking VM map entries as the zone to target, size %lu\n",
			    (uintptr_t)zone_size_wired(largest_zone));
		}
	}

	/* TODO: Extend this to check for the largest process in other zones as well. */
	if (largest_zone == vm_map_entry_zone) {
		pid = find_largest_process_vm_map_entries();
	} else {
		printf("zone_map_exhaustion: Nothing to do for the largest zone [%s%s]. "
		    "Waking up memorystatus thread.\n", zone_heap_name(largest_zone),
		    largest_zone->z_name);
	}
	if (!memorystatus_kill_on_zone_map_exhaustion(pid)) {
		printf("zone_map_exhaustion: Call to memorystatus failed, victim pid: %d\n", pid);
	}

	return largest_zone;
}

#endif /* !ZALLOC_TEST */
#pragma mark probabilistic gzalloc
#if !ZALLOC_TEST
#if CONFIG_PROB_GZALLOC

extern uint32_t random(void);
struct pgz_backtrace {
	uint32_t  pgz_depth;
	int32_t   pgz_bt[MAX_ZTRACE_DEPTH];
};

static int32_t  PERCPU_DATA(pgz_sample_counter);
static SECURITY_READ_ONLY_LATE(struct pgz_backtrace *) pgz_backtraces;
static uint32_t pgz_uses;       /* number of zones using PGZ */
static int32_t  pgz_slot_avail;
#if OS_ATOMIC_HAS_LLSC
struct zone_page_metadata *pgz_slot_head;
#else
static struct pgz_slot_head {
	uint32_t psh_count;
	uint32_t psh_slot;
} pgz_slot_head;
#endif
struct zone_page_metadata *pgz_slot_tail;
static SECURITY_READ_ONLY_LATE(vm_map_t) pgz_submap;

static struct zone_page_metadata *
pgz_meta(uint32_t index)
{
	return &zone_info.zi_pgz_meta[2 * index + 1];
}

static struct pgz_backtrace *
pgz_bt(uint32_t slot, bool free)
{
	return &pgz_backtraces[2 * slot + free];
}

static void
pgz_backtrace(struct pgz_backtrace *bt, void *fp)
{
	struct backtrace_control ctl = {
		.btc_frame_addr = (uintptr_t)fp,
	};

	bt->pgz_depth = (uint32_t)backtrace_packed(BTP_KERN_OFFSET_32,
	    (uint8_t *)bt->pgz_bt, sizeof(bt->pgz_bt), &ctl, NULL) / 4;
}

static uint32_t
pgz_slot(vm_offset_t addr)
{
	return (uint32_t)((addr - zone_info.zi_pgz_range.min_address) >> (PAGE_SHIFT + 1));
}

static vm_offset_t
pgz_addr(uint32_t slot)
{
	return zone_info.zi_pgz_range.min_address + ptoa(2 * slot + 1);
}

static bool
pgz_sample(zalloc_flags_t flags)
{
	int32_t *counterp, cnt;

	counterp = PERCPU_GET(pgz_sample_counter);
	cnt = *counterp;
	if (__probable(cnt > 0)) {
		*counterp  = cnt - 1;
		return false;
	}

	if (pgz_slot_avail <= 0) {
		return false;
	}

	/*
	 * zalloc_random_uniform() might block, so when the sampled allocation
	 * requested Z_NOWAIT, set the counter to `-1` which will cause
	 * the next allocation that can block to generate a new random value.
	 * No allocation on this CPU will sample until then.
	 */
	if (flags & Z_NOWAIT) {
		*counterp = -1;
	} else {
		enable_preemption();
		*counterp = zalloc_random_uniform(0, 2 * pgz_sample_rate);
		disable_preemption();
	}

	return cnt == 0;
}

static inline bool
pgz_slot_alloc(uint32_t *slot)
{
	struct zone_page_metadata *m;
	uint32_t tries = 100;

	disable_preemption();

#if OS_ATOMIC_USE_LLSC
	int32_t ov, nv;
	os_atomic_rmw_loop(&pgz_slot_avail, ov, nv, relaxed, {
		if (__improbable(ov <= 0)) {
		        os_atomic_rmw_loop_give_up({
				enable_preemption();
				return false;
			});
		}
		nv = ov - 1;
	});
#else
	if (__improbable(os_atomic_dec_orig(&pgz_slot_avail, relaxed) <= 0)) {
		os_atomic_inc(&pgz_slot_avail, relaxed);
		enable_preemption();
		return false;
	}
#endif

again:
	if (__improbable(tries-- == 0)) {
		/*
		 * Too much contention,
		 * extremely unlikely but do not stay stuck.
		 */
		os_atomic_inc(&pgz_slot_avail, relaxed);
		enable_preemption();
		return false;
	}

#if OS_ATOMIC_HAS_LLSC
	do {
		m = os_atomic_load_exclusive(&pgz_slot_head, dependency);
		if (__improbable(m->zm_pgz_slot_next == NULL)) {
			/*
			 * Either we are waiting for an enqueuer (unlikely)
			 * or we are competing with another core and
			 * are looking at a popped element.
			 */
			os_atomic_clear_exclusive();
			goto again;
		}
	} while (!os_atomic_store_exclusive(&pgz_slot_head,
	    m->zm_pgz_slot_next, relaxed));
#else
	struct zone_page_metadata *base = zone_info.zi_pgz_meta;
	struct pgz_slot_head ov, nv;
	os_atomic_rmw_loop(&pgz_slot_head, ov, nv, dependency, {
		m = &base[ov.psh_slot * 2];
		if (__improbable(m->zm_pgz_slot_next == NULL)) {
		        /*
		         * Either we are waiting for an enqueuer (unlikely)
		         * or we are competing with another core and
		         * are looking at a popped element.
		         */
		        os_atomic_rmw_loop_give_up(goto again);
		}
		nv.psh_count = ov.psh_count + 1;
		nv.psh_slot  = (uint32_t)((m->zm_pgz_slot_next - base) / 2);
	});
#endif

	enable_preemption();

	m->zm_pgz_slot_next = NULL;
	*slot = (uint32_t)((m - zone_info.zi_pgz_meta) / 2);
	return true;
}

static inline bool
pgz_slot_free(uint32_t slot)
{
	struct zone_page_metadata *m = &zone_info.zi_pgz_meta[2 * slot];
	struct zone_page_metadata *t;

	disable_preemption();
	t = os_atomic_xchg(&pgz_slot_tail, m, relaxed);
	os_atomic_store(&t->zm_pgz_slot_next, m, release);
	os_atomic_inc(&pgz_slot_avail, relaxed);
	enable_preemption();

	return true;
}

/*!
 * @function pgz_protect()
 *
 * @brief
 * Try to protect an allocation with PGZ.
 *
 * @param zone          The zone the allocation was made against.
 * @param addr          An allocated element address to protect.
 * @param flags         The @c zalloc_flags_t passed to @c zalloc.
 * @param fp            The caller frame pointer (for the backtrace).
 * @returns             The new address for the element, or @c addr.
 */
__attribute__((noinline))
static vm_offset_t
pgz_protect(zone_t zone, vm_offset_t addr, zalloc_flags_t flags, void *fp)
{
	kern_return_t kr;
	uint32_t slot;

	if (!pgz_slot_alloc(&slot)) {
		return addr;
	}

	/*
	 * Try to double-map the page (may fail if Z_NOWAIT).
	 * we will always find a PA because pgz_init() pre-expanded the pmap.
	 */
	vm_offset_t  new_addr = pgz_addr(slot);
	pmap_paddr_t pa = kvtophys(trunc_page(addr));

	kr = pmap_enter_options_addr(kernel_pmap, new_addr, pa,
	    VM_PROT_READ | VM_PROT_WRITE, VM_PROT_NONE, 0, TRUE,
	    (flags & Z_NOWAIT) ? PMAP_OPTIONS_NOWAIT : 0, NULL);

	if (__improbable(kr != KERN_SUCCESS)) {
		pgz_slot_free(slot);
		return addr;
	}

	struct zone_page_metadata tmp = {
		.zm_chunk_len = ZM_PGZ_ALLOCATED,
		.zm_index     = zone_index(zone),
	};
	struct zone_page_metadata *meta = pgz_meta(slot);

	os_atomic_store(&meta->zm_bits, tmp.zm_bits, relaxed);
	os_atomic_store(&meta->zm_pgz_orig_addr, addr, relaxed);
	pgz_backtrace(pgz_bt(slot, false), fp);

	return new_addr + (addr & PAGE_MASK);
}

/*!
 * @function pgz_unprotect()
 *
 * @brief
 * Release a PGZ slot and returns the original address of a freed element.
 *
 * @param addr          A PGZ protected element address.
 * @param fp            The caller frame pointer (for the backtrace).
 * @returns             The non protected address for the element
 *                      that was passed to @c pgz_protect().
 */
__attribute__((noinline))
static vm_offset_t
pgz_unprotect(vm_offset_t addr, void *fp)
{
	struct zone_page_metadata *meta;
	struct zone_page_metadata tmp;
	uint32_t slot;

	slot = pgz_slot(addr);
	meta = zone_meta_from_addr(addr);
	tmp  = *meta;
	if (tmp.zm_chunk_len != ZM_PGZ_ALLOCATED) {
		goto double_free;
	}

	pmap_remove(kernel_pmap, trunc_page(addr), trunc_page(addr) + PAGE_SIZE);

	pgz_backtrace(pgz_bt(slot, true), fp);

	tmp.zm_chunk_len = ZM_PGZ_FREE;
	tmp.zm_bits = os_atomic_xchg(&meta->zm_bits, tmp.zm_bits, relaxed);
	if (tmp.zm_chunk_len != ZM_PGZ_ALLOCATED) {
		goto double_free;
	}

	pgz_slot_free(slot);
	return tmp.zm_pgz_orig_addr;

double_free:
	panic_fault_address = addr;
	meta->zm_chunk_len = ZM_PGZ_DOUBLE_FREE;
	panic("probabilistic gzalloc double free: %p", (void *)addr);
}

bool
pgz_owned(mach_vm_address_t addr)
{
	vm_offset_t rmin, rmax;

#if CONFIG_KERNEL_TBI
	addr = VM_KERNEL_TBI_FILL(addr);
#endif /* CONFIG_KERNEL_TBI */

	zone_range_load(&zone_info.zi_pgz_range, rmin, rmax);

	return (addr >= rmin) & (addr < rmax);
}


__attribute__((always_inline))
vm_offset_t
__pgz_decode(mach_vm_address_t addr, mach_vm_size_t size)
{
	struct zone_page_metadata *meta;

	if (__probable(!pgz_owned(addr))) {
		return (vm_offset_t)addr;
	}

	if (zone_addr_size_crosses_page(addr, size)) {
		panic("invalid size for PGZ protected address %p:%p",
		    (void *)addr, (void *)(addr + size));
	}

	meta = zone_meta_from_addr((vm_offset_t)addr);
	if (meta->zm_chunk_len != ZM_PGZ_ALLOCATED) {
		panic_fault_address = (vm_offset_t)addr;
		panic("probabilistic gzalloc use-after-free: %p", (void *)addr);
	}

	return trunc_page(meta->zm_pgz_orig_addr) + (addr & PAGE_MASK);
}

__attribute__((always_inline))
vm_offset_t
__pgz_decode_allow_invalid(vm_offset_t addr, zone_id_t zid)
{
	struct zone_page_metadata *meta;

	if (__probable(!pgz_owned(addr))) {
		return addr;
	}

	meta = zone_meta_from_addr(addr);
	addr = trunc_page(meta->zm_pgz_orig_addr) + (addr & PAGE_MASK);

	if (zid != ZONE_ID_ANY && zone_index_from_ptr((void *)addr) != zid) {
		return 0;
	}

	return addr;
}

static void
pgz_zone_init(zone_t z)
{
	char zn[MAX_ZONE_NAME];
	char zv[MAX_ZONE_NAME];
	char key[30];

	if (zone_elem_size(z) > PAGE_SIZE) {
		return;
	}

	if (zone_index(z) == ZONE_ID_SELECT_SET) {
		return;
	}

	if (pgz_all) {
		os_atomic_inc(&pgz_uses, relaxed);
		z->z_pgz_tracked = true;
		return;
	}

	snprintf(zn, sizeof(zn), "%s%s", zone_heap_name(z), zone_name(z));

	for (int i = 1;; i++) {
		snprintf(key, sizeof(key), "pgz%d", i);
		if (!PE_parse_boot_argn(key, zv, sizeof(zv))) {
			break;
		}
		if (track_this_zone(zn, zv) || track_kalloc_zones(z, zv)) {
			os_atomic_inc(&pgz_uses, relaxed);
			z->z_pgz_tracked = true;
			break;
		}
	}
}

__startup_func
static vm_size_t
pgz_get_size(void)
{
	if (pgz_slots == UINT32_MAX) {
		/*
		 * Scale with RAM size: ~200 slots a G
		 */
		pgz_slots = (uint32_t)(sane_size >> 22);
	}

	/*
	 * Make sure that the slot allocation scheme works.
	 * see pgz_slot_alloc() / pgz_slot_free();
	 */
	if (pgz_slots < zpercpu_count() * 4) {
		pgz_slots = zpercpu_count() * 4;
	}
	if (pgz_slots >= UINT16_MAX) {
		pgz_slots = UINT16_MAX - 1;
	}

	/*
	 * Quarantine is 33% of slots by default, no more than 90%.
	 */
	if (pgz_quarantine == 0) {
		pgz_quarantine = pgz_slots / 3;
	}
	if (pgz_quarantine > pgz_slots * 9 / 10) {
		pgz_quarantine = pgz_slots * 9 / 10;
	}
	pgz_slot_avail = pgz_slots - pgz_quarantine;

	return ptoa(2 * pgz_slots + 1);
}

__startup_func
static void
pgz_init(void)
{
	if (!pgz_uses) {
		return;
	}

	if (pgz_sample_rate == 0) {
		/*
		 * If no rate was provided, pick a random one that scales
		 * with the number of protected zones.
		 *
		 * Use a binomal distribution to avoid having too many
		 * really fast sample rates.
		 */
		uint32_t factor = MIN(pgz_uses, 10);
		uint32_t max_rate = 1000 * factor;
		uint32_t min_rate =  100 * factor;

		pgz_sample_rate = (zalloc_random_uniform(min_rate, max_rate) +
		    zalloc_random_uniform(min_rate, max_rate)) / 2;
	}

	struct zone_map_range *r = &zone_info.zi_pgz_range;
	zone_info.zi_pgz_meta = zone_meta_from_addr(r->min_address);
	zone_meta_populate(r->min_address, zone_range_size(r));

	for (size_t i = 0; i < 2 * pgz_slots + 1; i += 2) {
		zone_info.zi_pgz_meta[i].zm_chunk_len = ZM_PGZ_GUARD;
	}

	for (size_t i = 1; i < pgz_slots; i++) {
		zone_info.zi_pgz_meta[2 * i - 1].zm_pgz_slot_next =
		    &zone_info.zi_pgz_meta[2 * i + 1];
	}
#if OS_ATOMIC_HAS_LLSC
	pgz_slot_head = &zone_info.zi_pgz_meta[1];
#endif
	pgz_slot_tail = &zone_info.zi_pgz_meta[2 * pgz_slots - 1];

	pgz_backtraces = zalloc_permanent(sizeof(struct pgz_backtrace) *
	    2 * pgz_slots, ZALIGN_PTR);

	/*
	 * expand the pmap so that pmap_enter_options_addr()
	 * in pgz_protect() never need to call pmap_expand().
	 */
	for (uint32_t slot = 0; slot < pgz_slots; slot++) {
		(void)pmap_enter_options_addr(kernel_pmap, pgz_addr(slot), 0,
		    VM_PROT_NONE, VM_PROT_NONE, 0, FALSE,
		    PMAP_OPTIONS_NOENTER, NULL);
	}

	/* do this last as this will enable pgz */
	percpu_foreach(counter, pgz_sample_counter) {
		*counter = zalloc_random_uniform(0, 2 * pgz_sample_rate);
	}
}
STARTUP(EARLY_BOOT, STARTUP_RANK_MIDDLE, pgz_init);

static void
panic_display_pgz_bt(bool has_syms, uint32_t slot, bool free)
{
	struct pgz_backtrace *bt = pgz_bt(slot, free);
	const char *what = free ? "Free" : "Allocation";
	uintptr_t buf[MAX_ZTRACE_DEPTH];

	if (!ml_validate_nofault((vm_offset_t)bt, sizeof(*bt))) {
		paniclog_append_noflush("  Can't decode %s Backtrace\n", what);
		return;
	}

	backtrace_unpack(BTP_KERN_OFFSET_32, buf, MAX_ZTRACE_DEPTH,
	    (uint8_t *)bt->pgz_bt, 4 * bt->pgz_depth);

	paniclog_append_noflush("  %s Backtrace:\n", what);
	for (uint32_t i = 0; i < bt->pgz_depth && i < MAX_ZTRACE_DEPTH; i++) {
		if (has_syms) {
			paniclog_append_noflush("    %p ", (void *)buf[i]);
			panic_print_symbol_name(buf[i]);
			paniclog_append_noflush("\n");
		} else {
			paniclog_append_noflush("    %p\n", (void *)buf[i]);
		}
	}
	kmod_panic_dump((vm_offset_t *)buf, bt->pgz_depth);
}

static void
panic_display_pgz_uaf_info(bool has_syms, vm_offset_t addr)
{
	struct zone_page_metadata *meta;
	vm_offset_t elem, esize;
	const char *type;
	const char *prob;
	uint32_t slot;
	zone_t z;

	slot = pgz_slot(addr);
	meta = pgz_meta(slot);
	elem = pgz_addr(slot) + (meta->zm_pgz_orig_addr & PAGE_MASK);

	paniclog_append_noflush("Probabilistic GZAlloc Report:\n");

	if (ml_validate_nofault((vm_offset_t)meta, sizeof(*meta)) &&
	    meta->zm_index &&
	    meta->zm_index < os_atomic_load(&num_zones, relaxed)) {
		z = &zone_array[meta->zm_index];
	} else {
		paniclog_append_noflush("  Zone    : <unknown>\n");
		paniclog_append_noflush("  Address : %p\n", (void *)addr);
		paniclog_append_noflush("\n");
		return;
	}

	esize = zone_elem_size(z);
	paniclog_append_noflush("  Zone    : %s%s\n",
	    zone_heap_name(z), zone_name(z));
	paniclog_append_noflush("  Address : %p\n", (void *)addr);
	paniclog_append_noflush("  Element : [%p, %p) of size %d\n",
	    (void *)elem, (void *)(elem + esize), (uint32_t)esize);

	if (addr < elem) {
		type = "out-of-bounds(underflow) + use-after-free";
		prob = "low";
	} else if (meta->zm_chunk_len == ZM_PGZ_DOUBLE_FREE) {
		type = "double-free";
		prob = "high";
	} else if (addr < elem + esize) {
		type = "use-after-free";
		prob = "high";
	} else if (meta->zm_chunk_len != ZM_PGZ_ALLOCATED) {
		type = "out-of-bounds + use-after-free";
		prob = "low";
	} else {
		type = "out-of-bounds";
		prob = "high";
	}
	paniclog_append_noflush("  Kind    : %s (%s confidence)\n",
	    type, prob);
	if (addr < elem) {
		paniclog_append_noflush("  Access  : %d byte(s) before\n",
		    (uint32_t)(elem - addr) + 1);
	} else if (addr < elem + esize) {
		paniclog_append_noflush("  Access  : %d byte(s) inside\n",
		    (uint32_t)(addr - elem) + 1);
	} else {
		paniclog_append_noflush("  Access  : %d byte(s) past\n",
		    (uint32_t)(addr - (elem + esize)) + 1);
	}

	panic_display_pgz_bt(has_syms, slot, false);
	if (meta->zm_chunk_len != ZM_PGZ_ALLOCATED) {
		panic_display_pgz_bt(has_syms, slot, true);
	}

	paniclog_append_noflush("\n");
}

#endif /* CONFIG_PROB_GZALLOC */
#endif /* !ZALLOC_TEST */
#pragma mark zfree
#if !ZALLOC_TEST

/*!
 * @defgroup zfree
 * @{
 *
 * @brief
 * The codepath for zone frees.
 *
 * @discussion
 * There are 4 major ways to allocate memory that end up in the zone allocator:
 * - @c zfree()
 * - @c zfree_percpu()
 * - @c kfree*()
 * - @c zfree_permanent()
 *
 * While permanent zones have their own allocation scheme, all other codepaths
 * will eventually go through the @c zfree_ext() choking point.
 *
 * Ignoring the @c gzalloc_free() codepath, the decision tree looks like this:
 * <code>
 * zfree_ext()
 *      ├───> zfree_cached() ────────────────╮
 *      │       │                            │
 *      │       │                            │
 *      │       ├───> zfree_cached_slow() ───┤
 *      │       │            │               │
 *      │       │            v               │
 *      ╰───────┴───> zfree_item() ──────────┴───>
 * </code>
 *
 * @c zfree_ext() takes care of all the generic work to perform on an element
 * before it is freed (zeroing, logging, tagging, ...) then will hand it off to:
 * - @c zfree_item() if zone caching is off
 * - @c zfree_cached() if zone caching is on.
 *
 * @c zfree_cached can take a number of decisions:
 * - a fast path if the (f) or (a) magazines have space (preemption disabled),
 * - using the cpu local or recirculation depot calling @c zfree_cached_slow(),
 * - falling back to @c zfree_item() when CPU caching has been disabled.
 */

#if KASAN_ZALLOC
/*
 * Called from zfree() to add the element being freed to the KASan quarantine.
 *
 * Returns true if the newly-freed element made it into the quarantine without
 * displacing another, false otherwise. In the latter case, addrp points to the
 * address of the displaced element, which will be freed by the zone.
 */
static bool
kasan_quarantine_freed_element(
	zone_t          *zonep,         /* the zone the element is being freed to */
	void            **addrp)        /* address of the element being freed */
{
	zone_t zone = *zonep;
	void *addr = *addrp;

	/*
	 * Resize back to the real allocation size and hand off to the KASan
	 * quarantine. `addr` may then point to a different allocation, if the
	 * current element replaced another in the quarantine. The zone then
	 * takes ownership of the swapped out free element.
	 */
	vm_size_t usersz = zone_elem_size(zone) - 2 * zone->z_kasan_redzone;
	vm_size_t sz = usersz;

	if (addr && zone->z_kasan_redzone) {
		kasan_check_free((vm_address_t)addr, usersz, KASAN_HEAP_ZALLOC);
		addr = (void *)kasan_dealloc((vm_address_t)addr, &sz);
		assert(sz == zone_elem_size(zone));
	}
	if (addr && !zone->kasan_noquarantine) {
		kasan_free(&addr, &sz, KASAN_HEAP_ZALLOC, zonep, usersz, true);
		if (!addr) {
			return TRUE;
		}
	}
	if (addr && zone->kasan_noquarantine) {
		kasan_unpoison(addr, zone_elem_size(zone));
	}
	*addrp = addr;
	return FALSE;
}
#endif /* KASAN_ZALLOC */

__header_always_inline void
zfree_drop(zone_t zone, struct zone_page_metadata *meta, zone_element_t ze,
    bool recirc)
{
	vm_offset_t esize = zone_elem_size(zone);

	if (zone_meta_mark_free(meta, ze) == recirc) {
		zone_meta_double_free_panic(zone, ze, __func__);
	}

	vm_offset_t old_size = meta->zm_alloc_size;
	vm_offset_t max_size = ptoa(meta->zm_chunk_len) + ZM_ALLOC_SIZE_LOCK;
	vm_offset_t new_size = zone_meta_alloc_size_sub(zone, meta, esize);

	if (new_size == 0) {
		/* whether the page was on the intermediate or all_used, queue, move it to free */
		zone_meta_requeue(zone, &zone->z_pageq_empty, meta);
		zone->z_wired_empty += meta->zm_chunk_len;
	} else if (old_size + esize > max_size) {
		/* first free element on page, move from all_used */
		zone_meta_requeue(zone, &zone->z_pageq_partial, meta);
	}
}

static void
zfree_item(zone_t zone, struct zone_page_metadata *meta, zone_element_t ze)
{
	/* transfer preemption count to lock */
	zone_lock_nopreempt_check_contention(zone, NULL);

	zfree_drop(zone, meta, ze, false);
	zone_elems_free_add(zone, 1);

	zone_unlock(zone);
}

__attribute__((noinline))
static void
zfree_cached_slow(zone_t zone, struct zone_page_metadata *meta,
    zone_element_t ze, zone_cache_t cache)
{
	struct zone_depot mags;
	zone_magazine_t mag = NULL;
	uint32_t depot_max;
	uint16_t n_mags = 0;

	if (zone_meta_is_free(meta, ze)) {
		zone_meta_double_free_panic(zone, ze, __func__);
	}

	if (zone == zc_magazine_zone) {
		mag = (zone_magazine_t)zone_element_addr(zone, ze,
		    zone_elem_size(zone));
#if KASAN_ZALLOC
		kasan_poison_range((vm_offset_t)mag, zone_elem_size(zone),
		    ASAN_VALID);
#endif
	} else {
		mag = zone_magazine_alloc(Z_NOWAIT);
		if (__improbable(mag == NULL)) {
			return zfree_item(zone, meta, ze);
		}
		mag->zm_cur = 1;
		mag->zm_elems[0] = ze;
	}

	mag = zone_magazine_replace(&cache->zc_free_cur,
	    &cache->zc_free_elems, mag);

	z_debug_assert(cache->zc_free_cur <= 1);
	z_debug_assert(mag->zm_cur == zc_mag_size());

	/*
	 * Depot growth policy:
	 *
	 * The zc_alloc and zc_free are on average half empty/full,
	 * hence count for "1" unit of zc_mag_size().
	 *
	 * We use the local depot for each `zc_depot_max` extra `zc_mag_size()`
	 * worth of element we're allowed.
	 *
	 * If pushing the bucket puts us in excess of `zc_depot_max`,
	 * then we trim (1/zc_recirc_denom) buckets out, in order
	 * to amortize taking the zone lock.
	 *
	 * Note that `zc_depot_max` can be mutated by the GC concurrently,
	 * so take a copy that we use throughout.
	 */
	depot_max = os_atomic_load(&cache->zc_depot_max, relaxed);
	if (2 * zc_mag_size() <= depot_max) {
		zone_depot_lock_nopreempt(cache);

		STAILQ_INSERT_TAIL(&cache->zc_depot, mag, zm_link);
		cache->zc_depot_cur++;

		if (__probable((cache->zc_depot_cur + 1) * zc_mag_size() <=
		    depot_max)) {
			return zone_depot_unlock(cache);
		}

		n_mags = 1;
		STAILQ_FIRST(&mags) = mag = STAILQ_FIRST(&cache->zc_depot);

		/* always leave at least once magazine behind */
		while (n_mags + 1 < cache->zc_depot_cur &&
		    n_mags * zc_mag_size() * zc_recirc_denom < depot_max) {
			mag = STAILQ_NEXT(mag, zm_link);
			n_mags++;
		}

		cache->zc_depot_cur -= n_mags;
		STAILQ_FIRST(&cache->zc_depot) = STAILQ_NEXT(mag, zm_link);
		STAILQ_NEXT(mag, zm_link) = NULL;

		zone_depot_unlock(cache);

		mags.stqh_last = &STAILQ_NEXT(mag, zm_link);
	} else {
		enable_preemption();

		n_mags = 1;
		STAILQ_FIRST(&mags) = mag;
		mags.stqh_last = &STAILQ_NEXT(mag, zm_link);
		STAILQ_NEXT(mag, zm_link) = NULL;
	}

	/*
	 * Preflight validity of all the elements before we touch the zone
	 * metadata, and then insert them into the recirculation depot.
	 */
	STAILQ_FOREACH(mag, &mags, zm_link) {
		for (uint16_t i = 0; i < zc_mag_size(); i++) {
			zone_element_validate(zone, mag->zm_elems[i]);
		}
	}

	zone_lock_check_contention(zone, cache);

	STAILQ_FOREACH(mag, &mags, zm_link) {
		for (uint16_t i = 0; i < zc_mag_size(); i++) {
			zone_element_t e = mag->zm_elems[i];

			if (!zone_meta_mark_free(zone_meta_from_element(e), e)) {
				zone_meta_double_free_panic(zone, e, __func__);
			}
		}
	}
	STAILQ_CONCAT(&zone->z_recirc, &mags);
	zone->z_recirc_cur += n_mags;

	zone_elems_free_add(zone, n_mags * zc_mag_size());

	zone_unlock(zone);
}

static void
zfree_cached(zone_t zone, struct zone_page_metadata *meta, zone_element_t ze)
{
	zone_cache_t cache = zpercpu_get(zone->z_pcpu_cache);

	if (cache->zc_free_cur >= zc_mag_size()) {
		if (cache->zc_alloc_cur >= zc_mag_size()) {
			return zfree_cached_slow(zone, meta, ze, cache);
		}
		zone_cache_swap_magazines(cache);
	}

	if (__improbable(cache->zc_alloc_elems == NULL)) {
		return zfree_item(zone, meta, ze);
	}

	if (zone_meta_is_free(meta, ze)) {
		zone_meta_double_free_panic(zone, ze, __func__);
	}

	uint16_t idx = cache->zc_free_cur++;
	if (idx >= zc_mag_size()) {
		zone_accounting_panic(zone, "zc_free_cur overflow");
	}
	cache->zc_free_elems[idx] = ze;

	enable_preemption();
}

/*
 *     The function is noinline when zlog can be used so that the backtracing can
 *     reliably skip the zfree_ext() and zfree_log()
 *     boring frames.
 */
#if ZONE_ENABLE_LOGGING
__attribute__((noinline))
#endif /* ZONE_ENABLE_LOGGING */
void
zfree_ext(zone_t zone, zone_stats_t zstats, void *addr, vm_size_t elem_size)
{
	struct zone_page_metadata *page_meta;
	vm_offset_t     elem = (vm_offset_t)addr;
	zone_element_t  ze;

	DTRACE_VM2(zfree, zone_t, zone, void*, addr);

#if CONFIG_KERNEL_TBI && KASAN_TBI
	if (zone->z_tbi_tag) {
		elem = kasan_tbi_tag_zfree(elem, elem_size, zone->z_percpu);
		/* addr is still consumed in the function: gzalloc_free */
		addr = (void *)elem;
	}
#endif /* CONFIG_KERNEL_TBI && KASAN_TBI */
#if CONFIG_PROB_GZALLOC
	if (__improbable(pgz_owned(elem))) {
		elem = pgz_unprotect(elem, __builtin_frame_address(0));
		addr = (void *)elem;
	}
#endif /* CONFIG_PROB_GZALLOC */
#if VM_TAG_SIZECLASSES
	if (__improbable(zone->z_uses_tags)) {
		vm_tag_t tag = *ztSlot(zone, elem) >> 1;
		// set the tag with b0 clear so the block remains inuse
		*ztSlot(zone, elem) = 0xFFFE;
		vm_tag_update_zone_size(tag, zone->z_tags_sizeclass,
		    -(long)elem_size);
	}
#endif /* VM_TAG_SIZECLASSES */

#if KASAN_ZALLOC
	/*
	 * Call zone_element_resolve() and throw away the results in
	 * order to validate the element and its zone membership.
	 * Any validation panics need to happen now, while we're
	 * still close to the caller.
	 *
	 * Note that elem has not been adjusted, so we have to remove the
	 * redzone first.
	 */
	zone_element_t            ze_discard;
	vm_offset_t               elem_actual = elem - zone->z_kasan_redzone;
	(void)zone_element_resolve(zone, elem_actual, elem_size, &ze_discard);

	if (kasan_quarantine_freed_element(&zone, &addr)) {
		return;
	}
	/*
	 * kasan_quarantine_freed_element() might return a different
	 * {zone, addr} than the one being freed for kalloc heaps.
	 *
	 * Make sure we reload everything.
	 */
	elem = (vm_offset_t)addr;
	elem_size = zone_elem_size(zone);
#endif
#if ZONE_ENABLE_LOGGING || CONFIG_ZLEAKS
	if (__improbable(zone->z_btlog)) {
		zfree_log(zone->z_btlog, elem, __builtin_frame_address(0));
	}
#endif /* ZONE_ENABLE_LOGGING */
#if CONFIG_GZALLOC
	if (__improbable(zone->z_gzalloc_tracked)) {
		return gzalloc_free(zone, zstats, addr);
	}
#endif /* CONFIG_GZALLOC */

	page_meta = zone_element_resolve(zone, elem, elem_size, &ze);
#if KASAN_ZALLOC
	if (zone->z_percpu) {
		zpercpu_foreach_cpu(i) {
			kasan_poison_range(elem + ptoa(i), elem_size,
			    ASAN_HEAP_FREED);
		}
	} else {
		kasan_poison_range(elem, elem_size, ASAN_HEAP_FREED);
	}
#endif

	disable_preemption();
	zpercpu_get(zstats)->zs_mem_freed += elem_size;

	if (zone->z_pcpu_cache) {
		return zfree_cached(zone, page_meta, ze);
	}

	return zfree_item(zone, page_meta, ze);
}

void
(zfree)(union zone_or_view zov, void *addr)
{
	zone_t zone = zov.zov_view->zv_zone;
	zone_stats_t zstats = zov.zov_view->zv_stats;
	vm_offset_t esize = zone_elem_size(zone);

	assert(zone > &zone_array[ZONE_ID__LAST_RO]);
	assert(!zone->z_percpu);
#if !KASAN_KALLOC
	bzero(addr, esize);
#endif /* !KASAN_KALLOC */
	zfree_ext(zone, zstats, addr, esize);
}

__attribute__((noinline))
void
zfree_percpu(union zone_or_view zov, void *addr)
{
	zone_t zone = zov.zov_view->zv_zone;
	zone_stats_t zstats = zov.zov_view->zv_stats;
	vm_offset_t esize = zone_elem_size(zone);

	assert(zone > &zone_array[ZONE_ID__LAST_RO]);
	assert(zone->z_percpu);
	addr = (void *)__zpcpu_demangle(addr);
#if !KASAN_KALLOC
	zpercpu_foreach_cpu(i) {
		bzero((char *)addr + ptoa(i), esize);
	}
#endif /* !KASAN_KALLOC */
	zfree_ext(zone, zstats, addr, esize);
}

void
(zfree_id)(zone_id_t zid, void *addr)
{
	(zfree)(&zone_array[zid], addr);
}

void
(zfree_ro)(zone_id_t zid, void *addr)
{
	assert(zid >= ZONE_ID__FIRST_RO && zid <= ZONE_ID__LAST_RO);
	zone_t zone = &zone_array[zid];
	zone_stats_t zstats = zone->z_stats;
	vm_offset_t esize = zone_ro_elem_size[zid];

#if ZSECURITY_CONFIG(READ_ONLY)
	assert(zone_security_array[zid].z_submap_idx == Z_SUBMAP_IDX_READ_ONLY);
	pmap_ro_zone_bzero(zid, (vm_offset_t)addr, 0, esize);
#elif !KASAN_KALLOC
	(void)zid;
	bzero(addr, esize);
#endif /* !KASAN_KALLOC */
	zfree_ext(zone, zstats, addr, esize);
}

/*! @} */
#endif /* !ZALLOC_TEST */
#pragma mark zalloc
#if !ZALLOC_TEST

/*!
 * @defgroup zalloc
 * @{
 *
 * @brief
 * The codepath for zone allocations.
 *
 * @discussion
 * There are 4 major ways to allocate memory that end up in the zone allocator:
 * - @c zalloc(), @c zalloc_flags(), ...
 * - @c zalloc_percpu()
 * - @c kalloc*()
 * - @c zalloc_permanent()
 *
 * While permanent zones have their own allocation scheme, all other codepaths
 * will eventually go through the @c zalloc_ext() choking point.
 *
 * Ignoring the @c zalloc_gz() codepath, the decision tree looks like this:
 * <code>
 * zalloc_ext()
 *      │
 *      ├───> zalloc_cached() ──────> zalloc_cached_fast() ───╮
 *      │         │                             ^             │
 *      │         │                             │             │
 *      │         ╰───> zalloc_cached_slow() ───╯             │
 *      │                         │                           │
 *      │<─────────────────╮      ├─────────────╮             │
 *      │                  │      │             │             │
 *      │                  │      v             │             │
 *      │<───────╮  ╭──> zalloc_item_slow() ────┤             │
 *      │        │  │                           │             │
 *      │        │  │                           v             │
 *      ╰───> zalloc_item() ──────────> zalloc_item_fast() ───┤
 *                                                            │
 *                                                            v
 *                                                     zalloc_return()
 * </code>
 *
 *
 * The @c zalloc_item() track is used when zone caching is off:
 * - @c zalloc_item_fast() is used when there are enough elements available,
 * - @c zalloc_item_slow() is used when a refill is needed, which can cause
 *   the zone to grow. This is the only codepath that refills.
 *
 * This track uses the zone lock for serialization:
 * - taken in @c zalloc_item(),
 * - maintained during @c zalloc_item_slow() (possibly dropped and re-taken),
 * - dropped in @c zalloc_item_fast().
 *
 *
 * The @c zalloc_cached() track is used when zone caching is on:
 * - @c zalloc_cached_fast() is taken when the cache has elements,
 * - @c zalloc_cached_slow() is taken if a cache refill is needed.
 *   It can chose many strategies:
 *    ~ @c zalloc_cached_from_depot() to try to reuse cpu stashed magazines,
 *    ~ @c zalloc_cached_from_recirc() using the global recirculation depot
 *      @c z_recirc,
 *    ~ using zalloc_import() if the zone has enough elements,
 *    ~ falling back to the @c zalloc_item() track if zone caching is disabled
 *      due to VM pressure or the zone has no available elements.
 *
 * This track disables preemption for serialization:
 * - preemption is disabled in @c zalloc_ext(),
 * - kept disabled during @c zalloc_cached_slow(), converted into a zone lock
 *   if switching to @c zalloc_item_slow(),
 * - preemption is reenabled in @c zalloc_cached_fast().
 *
 * @c zalloc_cached_from_depot() also takes depot locks (taken by the caller,
 * released by @c zalloc_cached_from_depot().
 *
 * In general the @c zalloc_*_slow() codepaths deal with refilling and will
 * tail call into the @c zalloc_*_fast() code to perform the actual allocation.
 *
 * @c zalloc_return() is the final function everyone tail calls into,
 * which prepares the element for consumption by the caller and deals with
 * common treatment (zone logging, tags, kasan, validation, ...).
 */

/*!
 * @function zalloc_import
 *
 * @brief
 * Import @c n elements in the specified array, opposite of @c zfree_drop().
 *
 * @param zone          The zone to import elements from
 * @param elems         The array to import into
 * @param n             The number of elements to import. Must be non zero,
 *                      and smaller than @c zone->z_elems_free.
 */
__header_always_inline void
zalloc_import(zone_t zone, zone_element_t *elems, zalloc_flags_t flags,
    vm_size_t esize, uint32_t n)
{
	uint32_t i = 0;

	assertf(STAILQ_EMPTY(&zone->z_recirc),
	    "Trying to import from zone %p [%s%s] with non empty recirc",
	    zone, zone_heap_name(zone), zone_name(zone));

	do {
		vm_offset_t page, eidx, size = 0;
		struct zone_page_metadata *meta;

		if (!zone_pva_is_null(zone->z_pageq_partial)) {
			meta = zone_pva_to_meta(zone->z_pageq_partial);
			page = zone_pva_to_addr(zone->z_pageq_partial);
		} else if (!zone_pva_is_null(zone->z_pageq_empty)) {
			meta = zone_pva_to_meta(zone->z_pageq_empty);
			page = zone_pva_to_addr(zone->z_pageq_empty);
			zone_counter_sub(zone, z_wired_empty, meta->zm_chunk_len);
		} else {
			zone_accounting_panic(zone, "z_elems_free corruption");
		}

		if (!zone_has_index(zone, meta->zm_index)) {
			zone_page_metadata_index_confusion_panic(zone, page, meta);
		}

		vm_offset_t old_size = meta->zm_alloc_size;
		vm_offset_t max_size = ptoa(meta->zm_chunk_len) + ZM_ALLOC_SIZE_LOCK;

		do {
			eidx = zone_meta_find_and_clear_bit(zone, meta, flags);
			elems[i++] = zone_element_encode(page, eidx);
			size += esize;
		} while (i < n && old_size + size + esize <= max_size);

		vm_offset_t new_size = zone_meta_alloc_size_add(zone, meta, size);

		if (new_size + esize > max_size) {
			zone_meta_requeue(zone, &zone->z_pageq_full, meta);
		} else if (old_size == 0) {
			/* remove from free, move to intermediate */
			zone_meta_requeue(zone, &zone->z_pageq_partial, meta);
		}
	} while (i < n);
}

/*!
 * @function zalloc_return
 *
 * @brief
 * Performs the tail-end of the work required on allocations before the caller
 * uses them.
 *
 * @discussion
 * This function is called without any zone lock held,
 * and preemption back to the state it had when @c zalloc_ext() was called.
 *
 * @param zone          The zone we're allocating from.
 * @param ze            The encoded element we just allocated.
 * @param flags         The flags passed to @c zalloc_ext() (for Z_ZERO).
 * @param elem_size     The element size for this zone.
 */
__attribute__((noinline))
static void *
zalloc_return(zone_t zone, zone_element_t ze, zalloc_flags_t flags __unused,
    vm_offset_t elem_size)
{
	vm_offset_t addr = zone_element_addr(zone, ze, elem_size);

#if CONFIG_KERNEL_TBI && KASAN_TBI
	addr = kasan_tbi_fix_address_tag(addr);
#endif /* CONFIG_KERNEL_TBI && KASAN_TBI */
#if ZALLOC_ENABLE_ZERO_CHECK
	zalloc_validate_element(zone, addr, elem_size, flags);
#endif /* ZALLOC_ENABLE_ZERO_CHECK */
#if ZONE_ENABLE_LOGGING || CONFIG_ZLEAKS
	if (__improbable(zone->z_btlog)) {
		zalloc_log(zone->z_btlog, addr, flags,
		    __builtin_frame_address(0));
	}
#endif /* ZONE_ENABLE_LOGGING || CONFIG_ZLEAKS */
#if KASAN_ZALLOC
	if (zone->z_kasan_redzone) {
		addr = kasan_alloc(addr, elem_size,
		    elem_size - 2 * zone->z_kasan_redzone,
		    zone->z_kasan_redzone);
		elem_size -= 2 * zone->z_kasan_redzone;
	}
	if (flags & Z_PCPU) {
		zpercpu_foreach_cpu(i) {
			kasan_poison_range(addr + ptoa(i), elem_size, ASAN_VALID);
			__nosan_bzero((char *)addr + ptoa(i), elem_size);
		}
	} else {
		kasan_poison_range(addr, elem_size, ASAN_VALID);
		__nosan_bzero((char *)addr, elem_size);
	}
#endif /* KASAN_ZALLOC */

#if VM_TAG_SIZECLASSES
	if (__improbable(zone->z_uses_tags)) {
		vm_tag_t tag = zalloc_flags_get_tag(flags);
		if (tag == VM_KERN_MEMORY_NONE) {
			zone_security_flags_t zsflags = zone_security_config(zone);
			if (zsflags.z_kheap_id == KHEAP_ID_DATA_BUFFERS) {
				tag = VM_KERN_MEMORY_KALLOC_DATA;
			} else {
				tag = VM_KERN_MEMORY_KALLOC;
			}
		}
		// set the tag with b0 clear so the block remains inuse
		*ztSlot(zone, addr) = (vm_tag_t)(tag << 1);
		vm_tag_update_zone_size(tag, zone->z_tags_sizeclass,
		    (long)elem_size);
	}
#endif /* VM_TAG_SIZECLASSES */
#if CONFIG_PROB_GZALLOC
	if ((flags & Z_PGZ) && !zone_addr_size_crosses_page(addr, elem_size)) {
		addr = pgz_protect(zone, addr, flags,
		    __builtin_frame_address(0));
	}
#endif

#if CONFIG_KERNEL_TBI && KASAN_TBI
	if (__probable(zone->z_tbi_tag)) {
		addr = kasan_tbi_tag_zalloc(addr, elem_size, (flags & Z_PCPU));
	} else {
		addr = kasan_tbi_tag_zalloc_default(addr, elem_size, (flags & Z_PCPU));
	}
#endif /* CONFIG_KERNEL_TBI && KASAN_TBI */

	DTRACE_VM2(zalloc, zone_t, zone, void*, addr);
	return (void *)addr;
}

#if CONFIG_GZALLOC
/*!
 * @function zalloc_gz
 *
 * @brief
 * Performs allocations for zones using gzalloc.
 *
 * @discussion
 * This function is noinline so that it doesn't affect the codegen
 * of the fastpath.
 */
__attribute__((noinline))
static void *
zalloc_gz(zone_t zone, zone_stats_t zstats, zalloc_flags_t flags, vm_size_t esize)
{
	vm_offset_t addr = gzalloc_alloc(zone, zstats, flags);
	return zalloc_return(zone, zone_element_encode(addr, 0),
	           flags, esize);
}
#endif /* CONFIG_GZALLOC */

__attribute__((noinline))
static void *
zalloc_item_fast(zone_t zone, zone_stats_t zstats, zalloc_flags_t flags,
    vm_size_t esize)
{
	zone_element_t ze;

	zalloc_import(zone, &ze, flags, esize, 1);
	zone_elems_free_sub(zone, 1);
	zpercpu_get(zstats)->zs_mem_allocated += esize;
	zone_unlock(zone);

	return zalloc_return(zone, ze, flags, esize);
}

static inline bool
zalloc_item_slow_should_schedule_async(zone_t zone, zalloc_flags_t flags)
{
	/*
	 * If we can't wait, then async it is.
	 */
	if (flags & (Z_NOWAIT | Z_NOPAGEWAIT)) {
		return true;
	}

	if (zone->z_elems_free == 0) {
		return false;
	}

	/*
	 * Early boot gets to tap in foreign reserves
	 */
	if (startup_phase < STARTUP_SUB_EARLY_BOOT) {
		return true;
	}

	/*
	 * Allow threads to tap up to 3/4 of the reserve only doing asyncs.
	 * Note that reserve-less zones will always say "true" here.
	 */
	if (zone->z_elems_free >= zone->z_elems_rsv / 4) {
		return true;
	}

	/*
	 * After this, only VM and GC threads get to tap in the reserve.
	 */
	return current_thread()->options & (TH_OPT_ZONE_PRIV | TH_OPT_VMPRIV);
}

/*!
 * @function zalloc_item_slow
 *
 * @brief
 * Performs allocations when the zone is out of elements.
 *
 * @discussion
 * This function might drop the lock and reenable preemption,
 * which means the per-CPU caching layer or recirculation depot
 * might have received elements.
 */
__attribute__((noinline))
static void *
zalloc_item_slow(zone_t zone, zone_stats_t zstats, zalloc_flags_t flags,
    vm_size_t esize)
{
	if (zalloc_item_slow_should_schedule_async(zone, flags)) {
		zone_expand_async_schedule_if_needed(zone);
	} else {
		zone_expand_locked(zone, flags, zalloc_needs_refill);
	}
	if (__improbable(zone->z_elems_free == 0)) {
		zone_unlock(zone);
		if (__improbable(flags & Z_NOFAIL)) {
			zone_nofail_panic(zone);
		}
		DTRACE_VM2(zalloc, zone_t, zone, void*, NULL);
		return NULL;
	}

	/*
	 * We might have changed core or got preempted/blocked while expanding
	 * the zone. Allocating from the zone when the recirculation depot
	 * is not empty is not allowed.
	 *
	 * It will be rare but possible for the depot to refill while we were
	 * waiting for pages. If that happens we need to start over.
	 */
	if (!STAILQ_EMPTY(&zone->z_recirc)) {
		zone_unlock(zone);
		return zalloc_ext(zone, zstats, flags, esize);
	}

	return zalloc_item_fast(zone, zstats, flags, esize);
}

/*!
 * @function zalloc_item
 *
 * @brief
 * Performs allocations when zone caching is off.
 *
 * @discussion
 * This function calls @c zalloc_item_slow() when refilling the zone
 * is needed, or @c zalloc_item_fast() if the zone has enough free elements.
 */
static void *
zalloc_item(zone_t zone, zone_stats_t zstats, zalloc_flags_t flags,
    vm_size_t esize)
{
	zone_lock_nopreempt_check_contention(zone, NULL);

	/*
	 * When we commited to the zalloc_item() path,
	 * zone caching might have been flipped/enabled.
	 *
	 * If we got preempted for long enough, the recirculation layer
	 * can have been populated, and allocating from the zone would be
	 * incorrect.
	 *
	 * So double check for this extremely rare race here.
	 */
	if (__improbable(!STAILQ_EMPTY(&zone->z_recirc))) {
		zone_unlock(zone);
		return zalloc_ext(zone, zstats, flags, esize);
	}

	if (__improbable(zone->z_elems_free <= zone->z_elems_rsv)) {
		return zalloc_item_slow(zone, zstats, flags, esize);
	}

	return zalloc_item_fast(zone, zstats, flags, esize);
}

__attribute__((always_inline))
static void *
zalloc_cached_fast(zone_t zone, zone_stats_t zstats, zalloc_flags_t flags,
    vm_size_t esize, zone_cache_t cache, zone_magazine_t freemag)
{
	zone_element_t ze;
	uint32_t index;

	index = --cache->zc_alloc_cur;
	if (index >= zc_mag_size()) {
		zone_accounting_panic(zone, "zc_alloc_cur wrap around");
	}
	ze = cache->zc_alloc_elems[index];
	cache->zc_alloc_elems[index].ze_value = 0;

	zpercpu_get(zstats)->zs_mem_allocated += esize;
	enable_preemption();

	if (zone_meta_is_free(zone_meta_from_element(ze), ze)) {
		zone_meta_double_free_panic(zone, ze, __func__);
	}

	if (freemag) {
		zone_magazine_free(freemag);
	}
	return zalloc_return(zone, ze, flags, esize);
}

__attribute__((noinline))
static void *
zalloc_cached_from_depot(
	zone_t                  zone,
	zone_stats_t            zstats,
	zalloc_flags_t          flags,
	vm_size_t               esize,
	zone_cache_t            cache)
{
	zone_magazine_t mag = STAILQ_FIRST(&cache->zc_depot);

	STAILQ_REMOVE_HEAD(&cache->zc_depot, zm_link);
	STAILQ_NEXT(mag, zm_link) = NULL;

	if (cache->zc_depot_cur-- == 0) {
		zone_accounting_panic(zone, "zc_depot_cur wrap-around");
	}
	zone_depot_unlock_nopreempt(cache);

	mag = zone_magazine_replace(&cache->zc_alloc_cur,
	    &cache->zc_alloc_elems, mag);

	z_debug_assert(cache->zc_alloc_cur == zc_mag_size());
	z_debug_assert(mag->zm_cur == 0);

	if (zone == zc_magazine_zone) {
		enable_preemption();
		bzero(mag, esize);
		return mag;
	}

	return zalloc_cached_fast(zone, zstats, flags, esize, cache, mag);
}

__attribute__((noinline))
static void *
zalloc_cached_import(
	zone_t                  zone,
	zone_stats_t            zstats,
	zalloc_flags_t          flags,
	vm_size_t               esize,
	zone_cache_t            cache)
{
	uint16_t n_elems = zc_mag_size();

	if (zone->z_elems_free < n_elems + zone->z_elems_rsv / 2 &&
	    os_sub_overflow(zone->z_elems_free,
	    zone->z_elems_rsv / 2, &n_elems)) {
		n_elems = 0;
	}

	z_debug_assert(n_elems <= zc_mag_size());

	if (__improbable(n_elems == 0)) {
		/*
		 * If importing elements would deplete the zone,
		 * call zalloc_item_slow()
		 */
		return zalloc_item_slow(zone, zstats, flags, esize);
	}

	if (__improbable(zone_caching_disabled)) {
		if (__improbable(zone_caching_disabled < 0)) {
			/*
			 * In the first 10s after boot, mess with
			 * the scan position in order to make early
			 * allocations patterns less predictible.
			 */
			zone_early_scramble_rr(zone, zstats);
		}
		return zalloc_item_fast(zone, zstats, flags, esize);
	}

	zalloc_import(zone, cache->zc_alloc_elems, flags, esize, n_elems);

	cache->zc_alloc_cur = n_elems;
	zone_elems_free_sub(zone, n_elems);

	zone_unlock_nopreempt(zone);

	return zalloc_cached_fast(zone, zstats, flags, esize, cache, NULL);
}

static void *
zalloc_cached_from_recirc(
	zone_t                  zone,
	zone_stats_t            zstats,
	zalloc_flags_t          flags,
	vm_size_t               esize,
	zone_cache_t            cache)
{
	struct zone_depot mags;
	zone_magazine_t mag;
	uint16_t n_mags = 1;

	STAILQ_FIRST(&mags) = mag = STAILQ_FIRST(&zone->z_recirc);

	for (;;) {
		for (uint16_t i = 0; i < zc_mag_size(); i++) {
			zone_element_t e = mag->zm_elems[i];

			if (!zone_meta_mark_used(zone_meta_from_element(e), e)) {
				zone_meta_double_free_panic(zone, e, __func__);
			}
		}

		if (n_mags >= zone->z_recirc_cur) {
			STAILQ_INIT(&zone->z_recirc);
			assert(STAILQ_NEXT(mag, zm_link) == NULL);
			break;
		}

		if (n_mags * zc_mag_size() * zc_recirc_denom >=
		    cache->zc_depot_max) {
			STAILQ_FIRST(&zone->z_recirc) = STAILQ_NEXT(mag, zm_link);
			STAILQ_NEXT(mag, zm_link) = NULL;
			break;
		}

		n_mags++;
		mag = STAILQ_NEXT(mag, zm_link);
	}

	zone_elems_free_sub(zone, n_mags * zc_mag_size());
	zone_counter_sub(zone, z_recirc_cur, n_mags);

	zone_unlock_nopreempt(zone);

	mags.stqh_last = &STAILQ_NEXT(mag, zm_link);

	/*
	 * And then incorporate everything into our per-cpu layer.
	 */

	mag = STAILQ_FIRST(&mags);

	if (n_mags > 1) {
		STAILQ_FIRST(&mags) = STAILQ_NEXT(mag, zm_link);
		STAILQ_NEXT(mag, zm_link) = NULL;

		zone_depot_lock_nopreempt(cache);

		cache->zc_depot_cur += n_mags - 1;
		STAILQ_CONCAT(&cache->zc_depot, &mags);

		zone_depot_unlock_nopreempt(cache);
	}

	mag = zone_magazine_replace(&cache->zc_alloc_cur,
	    &cache->zc_alloc_elems, mag);
	z_debug_assert(cache->zc_alloc_cur == zc_mag_size());
	z_debug_assert(mag->zm_cur == 0);

	return zalloc_cached_fast(zone, zstats, flags, esize, cache, mag);
}

__attribute__((noinline))
static void *
zalloc_cached_slow(
	zone_t                  zone,
	zone_stats_t            zstats,
	zalloc_flags_t          flags,
	vm_size_t               esize,
	zone_cache_t            cache)
{
	/*
	 * Try to allocate from our local depot, if there's one.
	 */
	if (STAILQ_FIRST(&cache->zc_depot)) {
		zone_depot_lock_nopreempt(cache);

		if (STAILQ_FIRST(&cache->zc_depot)) {
			return zalloc_cached_from_depot(zone, zstats, flags,
			           esize, cache);
		}

		zone_depot_unlock_nopreempt(cache);
	}

	zone_lock_nopreempt_check_contention(zone, cache);

	/*
	 * If the recirculation depot is empty, we'll need to import.
	 * The system is tuned for this to be extremely rare.
	 */
	if (__improbable(STAILQ_EMPTY(&zone->z_recirc))) {
		return zalloc_cached_import(zone, zstats, flags, esize, cache);
	}

	/*
	 * If the recirculation depot has elements, then try to fill
	 * the local per-cpu depot to (1 / zc_recirc_denom)
	 */
	return zalloc_cached_from_recirc(zone, zstats, flags, esize, cache);
}

/*!
 * @function zalloc_cached
 *
 * @brief
 * Performs allocations when zone caching is on.
 *
 * @discussion
 * This function calls @c zalloc_cached_fast() when the caches have elements
 * ready.
 *
 * Else it will call @c zalloc_cached_slow() so that the cache is refilled,
 * which might switch to the @c zalloc_item_slow() track when the backing zone
 * needs to be refilled.
 */
static void *
zalloc_cached(zone_t zone, zone_stats_t zstats, zalloc_flags_t flags,
    vm_size_t esize)
{
	zone_cache_t cache;

	cache = zpercpu_get(zone->z_pcpu_cache);

	if (cache->zc_alloc_cur == 0) {
		if (__improbable(cache->zc_free_cur == 0)) {
			return zalloc_cached_slow(zone, zstats, flags, esize, cache);
		}
		zone_cache_swap_magazines(cache);
	}

	return zalloc_cached_fast(zone, zstats, flags, esize, cache, NULL);
}

/*!
 * @function zalloc_ext
 *
 * @brief
 * The core implementation of @c zalloc(), @c zalloc_flags(), @c zalloc_percpu().
 */
void *
zalloc_ext(zone_t zone, zone_stats_t zstats, zalloc_flags_t flags, vm_size_t esize)
{
	/*
	 * KASan uses zalloc() for fakestack, which can be called anywhere.
	 * However, we make sure these calls can never block.
	 */
	assertf(startup_phase < STARTUP_SUB_EARLY_BOOT ||
#if KASAN_ZALLOC
	    zone->kasan_fakestacks ||
#endif /* KASAN_ZALLOC */
	    ml_get_interrupts_enabled() ||
	    ml_is_quiescing() ||
	    debug_mode_active(),
	    "Calling {k,z}alloc from interrupt disabled context isn't allowed");

	/*
	 * Make sure Z_NOFAIL was not obviously misused
	 */
	if (flags & Z_NOFAIL) {
		assert(!zone->exhaustible &&
		    (flags & (Z_NOWAIT | Z_NOPAGEWAIT)) == 0);
	}

#if CONFIG_GZALLOC
	if (__improbable(zone->z_gzalloc_tracked)) {
		return zalloc_gz(zone, zstats, flags, esize);
	}
#endif /* CONFIG_GZALLOC */

	disable_preemption();

#if ZALLOC_ENABLE_ZERO_CHECK
	if (zalloc_skip_zero_check()) {
		flags |= Z_NOZZC;
	}
#endif
#if CONFIG_PROB_GZALLOC
	if (zone->z_pgz_tracked && pgz_sample(flags)) {
		flags |= Z_PGZ;
	}
#endif /* CONFIG_PROB_GZALLOC */

	if (zone->z_pcpu_cache) {
		return zalloc_cached(zone, zstats, flags, esize);
	}

	return zalloc_item(zone, zstats, flags, esize);
}

__attribute__((always_inline))
void *
zalloc(union zone_or_view zov)
{
	return zalloc_flags(zov, Z_WAITOK);
}

__attribute__((always_inline))
void *
zalloc_noblock(union zone_or_view zov)
{
	return zalloc_flags(zov, Z_NOWAIT);
}

void *
zalloc_flags(union zone_or_view zov, zalloc_flags_t flags)
{
	zone_t zone = zov.zov_view->zv_zone;
	zone_stats_t zstats = zov.zov_view->zv_stats;
	vm_size_t esize = zone_elem_size(zone);

	assert(zone > &zone_array[ZONE_ID__LAST_RO]);
	assert(!zone->z_percpu);
	return zalloc_ext(zone, zstats, flags, esize);
}

__attribute__((always_inline))
void *
(zalloc_id)(zone_id_t zid, zalloc_flags_t flags)
{
	return zalloc_flags(&zone_array[zid], flags);
}

void *
(zalloc_ro)(zone_id_t zid, zalloc_flags_t flags)
{
	assert(zid >= ZONE_ID__FIRST_RO && zid <= ZONE_ID__LAST_RO);
	zone_t zone = &zone_array[zid];
	zone_stats_t zstats = zone->z_stats;
	vm_size_t esize = zone_ro_elem_size[zid];
	void *elem;

	assert(!zone->z_percpu);
	elem = zalloc_ext(zone, zstats, flags, esize);
#if ZSECURITY_CONFIG(READ_ONLY)
	assert(zone_security_array[zid].z_submap_idx == Z_SUBMAP_IDX_READ_ONLY);
	if (elem) {
		zone_require_ro(zid, esize, elem);
	}
#endif
	return elem;
}

#if ZSECURITY_CONFIG(READ_ONLY)

__attribute__((always_inline))
static bool
from_current_stack(const vm_offset_t addr, vm_size_t size)
{
	vm_offset_t start = (vm_offset_t)__builtin_frame_address(0);
	vm_offset_t end = (start + kernel_stack_size - 1) & -kernel_stack_size;
	return (addr >= start) && (addr + size < end);
}

#if XNU_MONITOR
/*
 * Check if an address is from const memory i.e TEXT or DATA CONST segements
 * or the SECURITY_READ_ONLY_LATE section.
 */
__attribute__((always_inline))
static bool
from_const_memory(const vm_offset_t addr, vm_size_t size)
{
	extern uint8_t text_start[] __SEGMENT_START_SYM("__TEXT");
	extern uint8_t text_end[] __SEGMENT_END_SYM("__TEXT");

	extern uint8_t data_const_start[] __SEGMENT_START_SYM("__DATA_CONST");
	extern uint8_t data_const_end[] __SEGMENT_END_SYM("__DATA_CONST");

	extern uint8_t security_start[] __SECTION_START_SYM(SECURITY_SEGMENT_NAME,
	    SECURITY_SECTION_NAME);
	extern uint8_t security_end[] __SECTION_END_SYM(SECURITY_SEGMENT_NAME,
	    SECURITY_SECTION_NAME);

	const uint8_t *_addr = (const uint8_t *) addr;

	return (_addr >= text_start && _addr + size <= text_end) ||
	       (_addr >= data_const_start && _addr + size <= data_const_end) ||
	       (_addr >= security_start && _addr + size <= security_end);
}
#else
__attribute__((always_inline))
static bool
from_const_memory(const vm_offset_t addr, vm_size_t size)
{
	(void) addr;
	(void) size;
	return true;
}
#endif /* XNU_MONITOR */

__abortlike
static void
zalloc_ro_mut_validation_panic(zone_id_t zid, void *elem,
    const vm_offset_t src, vm_size_t src_size)
{
	if (from_zone_map(src, src_size, ZONE_ADDR_READONLY)) {
		zone_t src_zone = &zone_array[zone_index_from_ptr((void *)src)];
		zone_t dst_zone = &zone_array[zid];
		panic("zalloc_ro_mut failed: source (%p) not from same zone as dst (%p)"
		    " (expected: %s, actual: %s", (void *)src, elem, src_zone->z_name,
		    dst_zone->z_name);
	}
	vm_offset_t start = (vm_offset_t)__builtin_frame_address(0);
	vm_offset_t end = (start + kernel_stack_size - 1) & -kernel_stack_size;
	panic("zalloc_ro_mut failed: source (%p) neither from RO zone map nor from"
	    " current stack (%p - %p)\n", (void *)src, (void *)start, (void *)end);
}

__attribute__((always_inline))
static void
zalloc_ro_mut_validate_src(zone_id_t zid, void *elem,
    const vm_offset_t src, vm_size_t src_size)
{
	if (from_current_stack(src, src_size) ||
	    (from_zone_map(src, src_size, ZONE_ADDR_READONLY) &&
	    zid == zone_index_from_ptr((void *)src)) ||
	    from_const_memory(src, src_size)) {
		return;
	}
	zalloc_ro_mut_validation_panic(zid, elem, src, src_size);
}

#endif /* ZSECURITY_CONFIG(READ_ONLY) */

__attribute__((noinline))
void
zalloc_ro_mut(zone_id_t zid, void *elem, vm_offset_t offset,
    const void *new_data, vm_size_t new_data_size)
{
	assert(zid >= ZONE_ID__FIRST_RO && zid <= ZONE_ID__LAST_RO);

#if ZSECURITY_CONFIG(READ_ONLY)
	zalloc_ro_mut_validate_src(zid, elem, (vm_offset_t)new_data,
	    new_data_size);
	pmap_ro_zone_memcpy(zid, (vm_offset_t) elem, offset,
	    (vm_offset_t) new_data, new_data_size);
#else
	(void)zid;
	memcpy((void *)((uintptr_t)elem + offset), new_data, new_data_size);
#endif
}

__attribute__((noinline))
uint64_t
zalloc_ro_mut_atomic(zone_id_t zid, void *elem, vm_offset_t offset,
    zro_atomic_op_t op, uint64_t value)
{
	assert(zid >= ZONE_ID__FIRST_RO && zid <= ZONE_ID__LAST_RO);

#if ZSECURITY_CONFIG(READ_ONLY)
	value = pmap_ro_zone_atomic_op(zid, (vm_offset_t)elem, offset, op, value);
#else
	(void)zid;
	value = __zalloc_ro_mut_atomic((vm_offset_t)elem + offset, op, value);
#endif
	return value;
}

void
zalloc_ro_clear(zone_id_t zid, void *elem, vm_offset_t offset, vm_size_t size)
{
	assert(zid >= ZONE_ID__FIRST_RO && zid <= ZONE_ID__LAST_RO);
#if ZSECURITY_CONFIG(READ_ONLY)
	pmap_ro_zone_bzero(zid, (vm_offset_t)elem, offset, size);
#else
	(void)zid;
	bzero((void *)((uintptr_t)elem + offset), size);
#endif
}

/*
 * This function will run in the PPL and needs to be robust
 * against an attacker with arbitrary kernel write.
 */

#if ZSECURITY_CONFIG(READ_ONLY)

__abortlike
static void
zone_id_require_ro_panic(zone_id_t zid, vm_size_t esize, void *addr)
{
	vm_offset_t va = (vm_offset_t)addr;
	uint32_t zindex;
	zone_t other;
	zone_t zone = &zone_array[zid];

	if (!from_zone_map(addr, 1, ZONE_ADDR_READONLY)) {
		panic("zone_require_ro failed: address not in a ro zone (addr: %p)", addr);
	}

	if (zone_addr_size_crosses_page(va, esize)) {
		panic("zone_require_ro failed: address crosses a page (addr: %p)", addr);
	}

	zindex = zone_index_from_ptr(addr);
	other = &zone_array[zindex];
	if (zindex >= os_atomic_load(&num_zones, relaxed) || !other->z_self) {
		panic("zone_require_ro failed: invalid zone index %d "
		    "(addr: %p, expected: %s%s)", zindex,
		    addr, zone_heap_name(zone), zone->z_name);
	} else {
		panic("zone_require_ro failed: address in unexpected zone id %d (%s%s) "
		    "(addr: %p, expected: %s%s)",
		    zindex, zone_heap_name(other), other->z_name,
		    addr, zone_heap_name(zone), zone->z_name);
	}
}

#endif /* ZSECURITY_CONFIG(READ_ONLY) */

__attribute__((always_inline))
void
zone_require_ro(zone_id_t zid, vm_size_t esize, void *addr)
{
#if ZSECURITY_CONFIG(READ_ONLY)
	vm_offset_t va = (vm_offset_t)addr;
	struct zone_page_metadata *meta = zone_meta_from_addr(va);

	/*
	 * Check that:
	 * - the first byte of the element is in the map
	 * - the element doesn't cross a page (implies it is wholy in the map)
	 * - the zone ID matches
	 *
	 * The code is weirdly written to minimize instruction count.
	 */
	if (!from_zone_map(addr, 1, ZONE_ADDR_READONLY) ||
	    zone_addr_size_crosses_page(va, esize) ||
	    zid != meta->zm_index) {
		zone_id_require_ro_panic(zid, esize, addr);
	}
#else
#pragma unused(zid, esize, addr)
#endif
}

void
zone_require_ro_range_contains(zone_id_t zid, void *addr)
{
	vm_size_t esize = zone_ro_elem_size[zid];
	vm_offset_t va = (vm_offset_t)addr;

	/* this is called by the pmap and we know for those the RO submap is on */
	assert(zone_security_array[zid].z_submap_idx == Z_SUBMAP_IDX_READ_ONLY);

	if (!from_zone_map(addr, esize, ZONE_ADDR_READONLY)) {
		zone_t zone = &zone_array[zid];
		zone_invalid_element_addr_panic(zone, va);
	}
}

void *
zalloc_percpu(union zone_or_view zov, zalloc_flags_t flags)
{
	zone_t zone = zov.zov_view->zv_zone;
	zone_stats_t zstats = zov.zov_view->zv_stats;
	vm_size_t esize = zone_elem_size(zone);

	assert(zone > &zone_array[ZONE_ID__LAST_RO]);
	assert(zone->z_percpu);
	flags |= Z_PCPU;
	return (void *)__zpcpu_mangle(zalloc_ext(zone, zstats, flags, esize));
}

static void *
_zalloc_permanent(zone_t zone, vm_size_t size, vm_offset_t mask)
{
	struct zone_page_metadata *page_meta;
	vm_offset_t offs, addr;
	zone_pva_t pva;

	assert(ml_get_interrupts_enabled() ||
	    ml_is_quiescing() ||
	    debug_mode_active() ||
	    startup_phase < STARTUP_SUB_EARLY_BOOT);

	size = (size + mask) & ~mask;
	assert(size <= PAGE_SIZE);

	zone_lock(zone);
	assert(zone->z_self == zone);

	for (;;) {
		pva = zone->z_pageq_partial;
		while (!zone_pva_is_null(pva)) {
			page_meta = zone_pva_to_meta(pva);
			if (page_meta->zm_bump + size <= PAGE_SIZE) {
				goto found;
			}
			pva = page_meta->zm_page_next;
		}

		zone_expand_locked(zone, Z_WAITOK, NULL);
	}

found:
	offs = (uint16_t)((page_meta->zm_bump + mask) & ~mask);
	page_meta->zm_bump = (uint16_t)(offs + size);
	page_meta->zm_alloc_size += size;
	zone->z_elems_free -= size;
	zpercpu_get(zone->z_stats)->zs_mem_allocated += size;

	if (page_meta->zm_alloc_size >= PAGE_SIZE - sizeof(vm_offset_t)) {
		zone_meta_requeue(zone, &zone->z_pageq_full, page_meta);
	}

	zone_unlock(zone);

	addr = offs + zone_pva_to_addr(pva);

	DTRACE_VM2(zalloc, zone_t, zone, void*, addr);
	return (void *)addr;
}

static void *
_zalloc_permanent_large(size_t size, vm_offset_t mask, vm_tag_t tag)
{
	kern_return_t kr;
	vm_offset_t addr;

	kr = kernel_memory_allocate(kernel_map, &addr, size, mask,
	    KMA_KOBJECT | KMA_PERMANENT | KMA_ZERO, tag);
	if (kr != 0) {
		panic("%s: unable to allocate %zd bytes (%d)",
		    __func__, (size_t)size, kr);
	}
	return (void *)addr;
}

void *
zalloc_permanent_tag(vm_size_t size, vm_offset_t mask, vm_tag_t tag)
{
	if (size <= PAGE_SIZE) {
		zone_t zone = &zone_array[ZONE_ID_PERMANENT];
		return _zalloc_permanent(zone, size, mask);
	}
	return _zalloc_permanent_large(size, mask, tag);
}

void *
zalloc_percpu_permanent(vm_size_t size, vm_offset_t mask)
{
	zone_t zone = &zone_array[ZONE_ID_PERCPU_PERMANENT];
	return (void *)__zpcpu_mangle(_zalloc_permanent(zone, size, mask));
}

/*! @} */
#endif /* !ZALLOC_TEST */
#pragma mark zone GC / trimming
#if !ZALLOC_TEST

static thread_call_data_t zone_defrag_callout;

static void
zone_reclaim_chunk(zone_t z, struct zone_page_metadata *meta,
    uint32_t free_count, struct zone_depot *mags)
{
	vm_address_t page_addr;
	vm_size_t    size_to_free;
	uint32_t     bitmap_ref;
	uint32_t     page_count;
	zone_security_flags_t zsflags = zone_security_config(z);
	bool         sequester = zsflags.z_va_sequester && !z->z_destroyed;
	uint16_t     oob_guards = 0;

#if CONFIG_PROB_GZALLOC
	/*
	 * See zone_allocate_va_locked: we added a guard page
	 * at the end of chunks, so if we are going to kmem_free,
	 * we want to return that one dude too.
	 */
	oob_guards = z->z_pgz_use_guards;
#endif /* CONFIG_PROB_GZALLOC */

	if (zone_submap_is_sequestered(zsflags)) {
		/*
		 * If the entire map is sequestered, we can't return the VA.
		 * It stays pinned to the zone forever.
		 */
		sequester = true;
	}

	zone_meta_queue_pop_native(z, &z->z_pageq_empty, &page_addr);

	page_count = meta->zm_chunk_len;

	if (meta->zm_alloc_size) {
		zone_metadata_corruption(z, meta, "alloc_size");
	}
	if (z->z_percpu) {
		if (page_count != 1) {
			zone_metadata_corruption(z, meta, "page_count");
		}
		size_to_free = ptoa(z->z_chunk_pages);
		zone_remove_wired_pages(z->z_chunk_pages);
	} else {
		if (page_count > z->z_chunk_pages) {
			zone_metadata_corruption(z, meta, "page_count");
		}
		if (page_count < z->z_chunk_pages) {
			/* Dequeue non populated VA from z_pageq_va */
			zone_meta_remqueue(z, meta + page_count);
		}
		size_to_free = ptoa(page_count);
		zone_remove_wired_pages(page_count);
	}

	zone_counter_sub(z, z_elems_free, free_count);
	zone_counter_sub(z, z_elems_avail, free_count);
	zone_counter_sub(z, z_wired_empty, page_count);
	zone_counter_sub(z, z_wired_cur, page_count);
	if (z->z_elems_free_min < free_count) {
		z->z_elems_free_min = 0;
	} else {
		z->z_elems_free_min -= free_count;
	}
	if (z->z_elems_free_max < free_count) {
		z->z_elems_free_max = 0;
	} else {
		z->z_elems_free_max -= free_count;
	}

	bitmap_ref = 0;
	if (sequester) {
		if (meta->zm_inline_bitmap) {
			for (int i = 0; i < meta->zm_chunk_len; i++) {
				meta[i].zm_bitmap = 0;
			}
		} else {
			bitmap_ref = meta->zm_bitmap;
			meta->zm_bitmap = 0;
		}
		meta->zm_chunk_len = 0;
	} else {
		if (!meta->zm_inline_bitmap) {
			bitmap_ref = meta->zm_bitmap;
		}
		zone_counter_sub(z, z_va_cur, z->z_percpu ? 1 : z->z_chunk_pages);
		bzero(meta, sizeof(*meta) * (z->z_chunk_pages + oob_guards));
	}

#if CONFIG_ZLEAKS
	if (__improbable(zleak_should_disable_for_zone(z) &&
	    startup_phase >= STARTUP_SUB_THREAD_CALL)) {
		thread_call_enter(&zone_leaks_callout);
	}
#endif /* CONFIG_ZLEAKS */

	zone_unlock(z);

	if (bitmap_ref) {
		zone_bits_free(bitmap_ref);
	}

	/* Free the pages for metadata and account for them */
#if KASAN_ZALLOC
	kasan_poison_range(page_addr, size_to_free, ASAN_VALID);
#endif
#if VM_TAG_SIZECLASSES
	if (z->z_uses_tags) {
		ztMemoryRemove(z, page_addr, size_to_free);
	}
#endif /* VM_TAG_SIZECLASSES */

	if (sequester) {
		kernel_memory_depopulate(zone_submap(zsflags), page_addr,
		    size_to_free, KMA_KOBJECT, VM_KERN_MEMORY_ZONE);
	} else {
		assert(zsflags.z_submap_idx != Z_SUBMAP_IDX_VM);
		kmem_free(zone_submap(zsflags), page_addr,
		    ptoa(z->z_chunk_pages + oob_guards));
#if CONFIG_PROB_GZALLOC
		if (z->z_pgz_use_guards) {
			os_atomic_dec(&zone_guard_pages, relaxed);
		}
#endif /* CONFIG_PROB_GZALLOC */
	}

	zone_magazine_free_list(mags);
	thread_yield_to_preemption();

	zone_lock(z);

	if (sequester) {
		zone_meta_queue_push(z, &z->z_pageq_va, meta);
	}
}

static uint16_t
zone_reclaim_elements(zone_t z, uint16_t *count, zone_element_t *elems)
{
	uint16_t n = *count;

	z_debug_assert(n <= zc_mag_size());

	for (uint16_t i = 0; i < n; i++) {
		zone_element_t ze = elems[i];
		elems[i].ze_value = 0;
		zfree_drop(z, zone_element_validate(z, ze), ze, false);
	}

	*count = 0;
	return n;
}

static uint16_t
zone_reclaim_recirc_magazine(zone_t z, struct zone_depot *mags)
{
	zone_magazine_t mag = STAILQ_FIRST(&z->z_recirc);

	STAILQ_REMOVE_HEAD(&z->z_recirc, zm_link);
	STAILQ_INSERT_TAIL(mags, mag, zm_link);
	zone_counter_sub(z, z_recirc_cur, 1);

	z_debug_assert(mag->zm_cur == zc_mag_size());

	for (uint16_t i = 0; i < zc_mag_size(); i++) {
		zone_element_t ze = mag->zm_elems[i];
		mag->zm_elems[i].ze_value = 0;
		zfree_drop(z, zone_element_validate(z, ze), ze, true);
	}

	mag->zm_cur = 0;

	return zc_mag_size();
}

static void
zone_depot_trim(zone_cache_t zc, struct zone_depot *head)
{
	zone_magazine_t mag;

	if (zc->zc_depot_cur == 0 ||
	    2 * (zc->zc_depot_cur + 1) * zc_mag_size() <= zc->zc_depot_max) {
		return;
	}

	zone_depot_lock(zc);

	while (zc->zc_depot_cur &&
	    2 * (zc->zc_depot_cur + 1) * zc_mag_size() > zc->zc_depot_max) {
		mag = STAILQ_FIRST(&zc->zc_depot);
		STAILQ_REMOVE_HEAD(&zc->zc_depot, zm_link);
		STAILQ_INSERT_TAIL(head, mag, zm_link);
		zc->zc_depot_cur--;
	}

	zone_depot_unlock(zc);
}

__enum_decl(zone_reclaim_mode_t, uint32_t, {
	ZONE_RECLAIM_TRIM,
	ZONE_RECLAIM_DRAIN,
	ZONE_RECLAIM_DESTROY,
});

/*!
 * @function zone_reclaim
 *
 * @brief
 * Drains or trim the zone.
 *
 * @discussion
 * Draining the zone will free it from all its elements.
 *
 * Trimming the zone tries to respect the working set size, and avoids draining
 * the depot when it's not necessary.
 *
 * @param z             The zone to reclaim from
 * @param mode          The purpose of this reclaim.
 */
static void
zone_reclaim(zone_t z, zone_reclaim_mode_t mode)
{
	struct zone_depot mags = STAILQ_HEAD_INITIALIZER(mags);
	zone_magazine_t mag;
	zone_security_flags_t zsflags = zone_security_config(z);

	zone_lock(z);

	if (mode == ZONE_RECLAIM_DESTROY) {
		if (!z->z_destructible || z->z_elems_rsv ||
		    zsflags.z_allows_foreign) {
			panic("zdestroy: Zone %s%s isn't destructible",
			    zone_heap_name(z), z->z_name);
		}

		if (!z->z_self || z->z_expander || z->z_expander_vm_priv ||
		    z->z_async_refilling || z->z_expanding_wait) {
			panic("zdestroy: Zone %s%s in an invalid state for destruction",
			    zone_heap_name(z), z->z_name);
		}

#if !KASAN_ZALLOC
		/*
		 * Unset the valid bit. We'll hit an assert failure on further
		 * operations on this zone, until zinit() is called again.
		 *
		 * Leave the zone valid for KASan as we will see zfree's on
		 * quarantined free elements even after the zone is destroyed.
		 */
		z->z_self = NULL;
#endif
		z->z_destroyed = true;
	} else if (z->z_destroyed) {
		return zone_unlock(z);
	} else if (z->z_elems_free <= z->z_elems_rsv) {
		/* If the zone is under its reserve level, leave it alone. */
		return zone_unlock(z);
	}

	if (z->z_pcpu_cache) {
		if (mode != ZONE_RECLAIM_TRIM) {
			zpercpu_foreach(zc, z->z_pcpu_cache) {
				zc->zc_depot_max /= 2;
			}
		} else {
			zpercpu_foreach(zc, z->z_pcpu_cache) {
				if (zc->zc_depot_max > 0) {
					zc->zc_depot_max--;
				}
			}
		}

		zone_unlock(z);

		if (mode == ZONE_RECLAIM_TRIM) {
			zpercpu_foreach(zc, z->z_pcpu_cache) {
				zone_depot_trim(zc, &mags);
			}
		} else {
			zpercpu_foreach(zc, z->z_pcpu_cache) {
				zone_depot_lock(zc);
				STAILQ_CONCAT(&mags, &zc->zc_depot);
				zc->zc_depot_cur = 0;
				zone_depot_unlock(zc);
			}
		}

		zone_lock(z);

		uint32_t freed = 0;

		STAILQ_FOREACH(mag, &mags, zm_link) {
			freed += zone_reclaim_elements(z,
			    &mag->zm_cur, mag->zm_elems);

			if (freed >= zc_free_batch_size) {
				z->z_elems_free_min += freed;
				z->z_elems_free_max += freed;
				z->z_elems_free += freed;
				zone_unlock(z);
				thread_yield_to_preemption();
				zone_lock(z);
				freed = 0;
			}
		}

		if (mode == ZONE_RECLAIM_DESTROY) {
			zpercpu_foreach(zc, z->z_pcpu_cache) {
				freed += zone_reclaim_elements(z,
				    &zc->zc_alloc_cur, zc->zc_alloc_elems);
				freed += zone_reclaim_elements(z,
				    &zc->zc_free_cur, zc->zc_free_elems);
			}

			z->z_elems_free_wss = 0;
			z->z_elems_free_min = 0;
			z->z_elems_free_max = 0;
			z->z_contention_cur = 0;
			z->z_contention_wma = 0;
		} else {
			z->z_elems_free_min += freed;
			z->z_elems_free_max += freed;
		}
		z->z_elems_free += freed;
	}

	for (;;) {
		struct zone_page_metadata *meta;
		uint32_t count, goal, freed = 0;

		goal = z->z_elems_rsv;
		if (mode == ZONE_RECLAIM_TRIM) {
			/*
			 * When trimming, only free elements in excess
			 * of the working set estimate.
			 *
			 * However if we are in a situation where the working
			 * set estimate is clearly growing, ignore the estimate
			 * as the next working set update will grow it and
			 * we want to avoid churn.
			 */
			goal = MAX(goal, MAX(z->z_elems_free_wss,
			    z->z_elems_free - z->z_elems_free_min));

			/*
			 * Add some slop to account for "the last partial chunk in flight"
			 * so that we do not deplete the recirculation depot too harshly.
			 */
			goal += z->z_chunk_elems / 2;
		}

		if (z->z_elems_free <= goal) {
			break;
		}

		/*
		 * If we're above target, but we have no free page, then drain
		 * the recirculation depot until we get a free chunk or exhaust
		 * the depot.
		 *
		 * This is rather abrupt but also somehow will reduce
		 * fragmentation anyway, and the zone code will import
		 * over time anyway.
		 */
		while (z->z_recirc_cur && zone_pva_is_null(z->z_pageq_empty)) {
			if (freed >= zc_free_batch_size) {
				zone_unlock(z);
				zone_magazine_free_list(&mags);
				thread_yield_to_preemption();
				zone_lock(z);
				freed = 0;
				/* we dropped the lock, needs to reassess */
				continue;
			}
			freed += zone_reclaim_recirc_magazine(z, &mags);
		}

		if (zone_pva_is_null(z->z_pageq_empty)) {
			break;
		}

		meta  = zone_pva_to_meta(z->z_pageq_empty);
		count = (uint32_t)ptoa(meta->zm_chunk_len) / zone_elem_size(z);

		if (z->z_elems_free - count < goal) {
			break;
		}

		zone_reclaim_chunk(z, meta, count, &mags);
	}

	zone_unlock(z);

	zone_magazine_free_list(&mags);
}

static void
zone_reclaim_all(zone_reclaim_mode_t mode)
{
	/*
	 * Start with zones with VA sequester since depopulating
	 * pages will not need to allocate vm map entries for holes,
	 * which will give memory back to the system faster.
	 */
	zone_index_foreach(zid) {
		zone_t z = &zone_array[zid];
		if (z == zc_magazine_zone) {
			continue;
		}
		if (zone_security_array[zid].z_va_sequester && z->collectable) {
			zone_reclaim(z, mode);
		}
	}

	zone_index_foreach(zid) {
		zone_t z = &zone_array[zid];
		if (z == zc_magazine_zone) {
			continue;
		}
		if (!zone_security_array[zid].z_va_sequester && z->collectable) {
			zone_reclaim(z, mode);
		}
	}

	zone_reclaim(zc_magazine_zone, mode);
}

void
zone_userspace_reboot_checks(void)
{
	vm_size_t label_zone_size = zone_size_allocated(ipc_service_port_label_zone);
	if (label_zone_size != 0) {
		panic("Zone %s should be empty upon userspace reboot. Actual size: %lu.",
		    ipc_service_port_label_zone->z_name, (unsigned long)label_zone_size);
	}
}

void
zone_gc(zone_gc_level_t level)
{
	zone_reclaim_mode_t mode;
	zone_t largest_zone = NULL;

	switch (level) {
	case ZONE_GC_TRIM:
		mode = ZONE_RECLAIM_TRIM;
		break;
	case ZONE_GC_DRAIN:
		mode = ZONE_RECLAIM_DRAIN;
		break;
	case ZONE_GC_JETSAM:
		largest_zone = kill_process_in_largest_zone();
		mode = ZONE_RECLAIM_TRIM;
		break;
	}

	current_thread()->options |= TH_OPT_ZONE_PRIV;
	lck_mtx_lock(&zone_gc_lock);

	zone_reclaim_all(mode);

	if (level == ZONE_GC_JETSAM && zone_map_nearing_exhaustion()) {
		/*
		 * If we possibly killed a process, but we're still critical,
		 * we need to drain harder.
		 */
		zone_reclaim(largest_zone, ZONE_RECLAIM_DRAIN);
		zone_reclaim_all(ZONE_RECLAIM_DRAIN);
	}

	lck_mtx_unlock(&zone_gc_lock);
	current_thread()->options &= ~TH_OPT_ZONE_PRIV;
}

void
zone_gc_trim(void)
{
	zone_gc(ZONE_GC_TRIM);
}

void
zone_gc_drain(void)
{
	zone_gc(ZONE_GC_DRAIN);
}

static bool
zone_defrag_needed(zone_t z)
{
	uint32_t recirc_size = z->z_recirc_cur * zc_mag_size();

	if (recirc_size <= z->z_chunk_elems / 2) {
		return false;
	}
	if (recirc_size * z->z_elem_size <= zc_defrag_threshold) {
		return false;
	}
	return recirc_size * zc_defrag_ratio > z->z_elems_free_wss * 100;
}

/*!
 * @function zone_defrag
 *
 * @brief
 * Resize the recirculation depot to match the working set size.
 *
 * @discussion
 * When zones grow very large due to a spike in usage, and then some of those
 * elements get freed, the elements in magazines in the recirculation depot
 * are in no particular order.
 *
 * In order to control fragmentation, we need to detect "empty" pages so that
 * they get onto the @c z_pageq_empty freelist, so that allocations re-pack
 * naturally.
 *
 * This is done very gently, never in excess of the working set and some slop.
 */
static bool
zone_autogc_needed(zone_t z)
{
	uint32_t free_min = z->z_elems_free_min;

	if (free_min * z->z_elem_size <= zc_autogc_threshold) {
		return false;
	}

	return free_min * zc_autogc_ratio > z->z_elems_free_wss * 100;
}

static void
zone_defrag(zone_t z)
{
	struct zone_depot mags = STAILQ_HEAD_INITIALIZER(mags);
	zone_magazine_t mag, tmp;
	uint32_t freed = 0, goal = 0;

	zone_lock(z);

	goal = z->z_elems_free_wss + z->z_chunk_elems / 2 +
	    zc_mag_size() - 1;

	while (z->z_recirc_cur * zc_mag_size() > goal) {
		if (freed >= zc_free_batch_size) {
			zone_unlock(z);
			thread_yield_to_preemption();
			zone_lock(z);
			freed = 0;
			/* we dropped the lock, needs to reassess */
			continue;
		}
		freed += zone_reclaim_recirc_magazine(z, &mags);
	}

	zone_unlock(z);

	STAILQ_FOREACH_SAFE(mag, &mags, zm_link, tmp) {
		zone_magazine_free(mag);
	}
}

static void
zone_defrag_async(__unused thread_call_param_t p0, __unused thread_call_param_t p1)
{
	zone_foreach(z) {
		if (!z->collectable || z == zc_magazine_zone) {
			continue;
		}

		if (zone_autogc_needed(z)) {
			current_thread()->options |= TH_OPT_ZONE_PRIV;
			lck_mtx_lock(&zone_gc_lock);
			zone_reclaim(z, ZONE_RECLAIM_TRIM);
			lck_mtx_unlock(&zone_gc_lock);
			current_thread()->options &= ~TH_OPT_ZONE_PRIV;
		} else if (zone_defrag_needed(z)) {
			zone_defrag(z);
		}
	}

	if (zone_autogc_needed(zc_magazine_zone)) {
		current_thread()->options |= TH_OPT_ZONE_PRIV;
		lck_mtx_lock(&zone_gc_lock);
		zone_reclaim(zc_magazine_zone, ZONE_RECLAIM_TRIM);
		lck_mtx_unlock(&zone_gc_lock);
		current_thread()->options &= ~TH_OPT_ZONE_PRIV;
	} else if (zone_defrag_needed(zc_magazine_zone)) {
		zone_defrag(zc_magazine_zone);
	}
}

void
compute_zone_working_set_size(__unused void *param)
{
	uint32_t zc_auto = zc_auto_threshold;
	bool kick_defrag = false;

	/*
	 * Keep zone caching disabled until the first proc is made.
	 */
	if (__improbable(zone_caching_disabled < 0)) {
		return;
	}

	zone_caching_disabled = vm_pool_low();

	if (os_mul_overflow(zc_auto, Z_CONTENTION_WMA_UNIT, &zc_auto)) {
		zc_auto = 0;
	}

	zone_foreach(z) {
		uint32_t wma;
		bool needs_caching = false;

		if (z->z_self != z) {
			continue;
		}

		zone_lock(z);

		wma = z->z_elems_free_max - z->z_elems_free_min;
		wma = (3 * wma + z->z_elems_free_wss) / 4;
		z->z_elems_free_max = z->z_elems_free_min = z->z_elems_free;
		z->z_elems_free_wss = wma;

		if (!kick_defrag &&
		    (zone_defrag_needed(z) || zone_autogc_needed(z))) {
			kick_defrag = true;
		}

		/* fixed point decimal of contentions per second */
		wma = z->z_contention_cur * Z_CONTENTION_WMA_UNIT /
		    ZONE_WSS_UPDATE_PERIOD;
		z->z_contention_cur = 0;
		z->z_contention_wma = (3 * wma + z->z_contention_wma) / 4;

		/*
		 * If the zone seems to be very quiet,
		 * gently lower its cpu-local depot size.
		 */
		if (z->z_pcpu_cache && wma < Z_CONTENTION_WMA_UNIT / 2 &&
		    z->z_contention_wma < Z_CONTENTION_WMA_UNIT / 2) {
			zpercpu_foreach(zc, z->z_pcpu_cache) {
				if (zc->zc_depot_max > zc_mag_size()) {
					zc->zc_depot_max--;
				}
			}
		}

		/*
		 * If the zone has been contending like crazy for two periods,
		 * and is eligible, maybe it's time to enable caching.
		 */
		if (!z->z_nocaching && !z->z_pcpu_cache && !z->exhaustible &&
		    zc_auto && z->z_contention_wma >= zc_auto && wma >= zc_auto) {
			needs_caching = true;
		}

		zone_unlock(z);

		if (needs_caching) {
			zone_enable_caching(z);
		}
	}

	if (kick_defrag) {
		thread_call_enter(&zone_defrag_callout);
	}
}

#endif /* !ZALLOC_TEST */
#pragma mark vm integration, MIG routines
#if !ZALLOC_TEST

extern unsigned int stack_total;
#if defined (__x86_64__)
extern unsigned int inuse_ptepages_count;
#endif

static void
panic_print_types_in_zone(zone_t z, const char* debug_str)
{
	kalloc_type_view_t kt_cur = NULL;
	const char *prev_type = "";
	size_t skip_over_site = sizeof("site.") - 1;
	paniclog_append_noflush("kalloc types in zone, %s (%s):\n",
	    debug_str, z->z_name);
	kt_cur = (kalloc_type_view_t) z->z_views;
	while (kt_cur) {
		struct zone_view kt_zv = kt_cur->kt_zv;
		const char *typename = kt_zv.zv_name + skip_over_site;
		if (strcmp(typename, prev_type) != 0) {
			paniclog_append_noflush("\t%-50s\n", typename);
			prev_type = typename;
		}
		kt_cur = (kalloc_type_view_t) kt_zv.zv_next;
	}
	paniclog_append_noflush("\n");
}

static void
panic_display_kalloc_types(void)
{
	if (kalloc_type_src_zone) {
		panic_print_types_in_zone(kalloc_type_src_zone, "addr belongs to");
	}
	if (kalloc_type_dst_zone) {
		panic_print_types_in_zone(kalloc_type_dst_zone,
		    "addr is being freed to");
	}
}

static void
zone_find_n_largest(const uint32_t n, zone_t *largest_zones,
    uint64_t *zone_size)
{
	zone_index_foreach(zid) {
		zone_t z = &zone_array[zid];
		vm_offset_t size = zone_size_wired(z);

		if (zid == ZONE_ID_VM_PAGES) {
			continue;
		}
		for (uint32_t i = 0; i < n; i++) {
			if (size > zone_size[i]) {
				largest_zones[i] = z;
				zone_size[i] = size;
				break;
			}
		}
	}
}

#define NUM_LARGEST_ZONES 5
static void
panic_display_largest_zones(void)
{
	zone_t largest_zones[NUM_LARGEST_ZONES]  = { NULL };
	uint64_t largest_size[NUM_LARGEST_ZONES] = { 0 };

	zone_find_n_largest(NUM_LARGEST_ZONES, (zone_t *) &largest_zones,
	    (uint64_t *) &largest_size);

	paniclog_append_noflush("Largest zones:\n%-28s %10s %10s\n",
	    "Zone Name", "Cur Size", "Free Size");
	for (uint32_t i = 0; i < NUM_LARGEST_ZONES; i++) {
		zone_t z = largest_zones[i];
		paniclog_append_noflush("%-8s%-20s %9luM %9luK\n",
		    zone_heap_name(z), z->z_name,
		    (uintptr_t)largest_size[i] >> 20,
		    (uintptr_t)zone_size_free(z) >> 10);
	}
}

static void
panic_display_zprint(void)
{
	panic_display_largest_zones();
	paniclog_append_noflush("%-20s %10lu\n", "Kernel Stacks",
	    (uintptr_t)(kernel_stack_size * stack_total));
#if defined (__x86_64__)
	paniclog_append_noflush("%-20s %10lu\n", "PageTables",
	    (uintptr_t)ptoa(inuse_ptepages_count));
#endif
	paniclog_append_noflush("%-20s %10lu\n", "Kalloc.Large",
	    (uintptr_t)kalloc_large_total);

	if (panic_kext_memory_info) {
		mach_memory_info_t *mem_info = panic_kext_memory_info;

		paniclog_append_noflush("\n%-5s %10s\n", "Kmod", "Size");
		for (uint32_t i = 0; i < panic_kext_memory_size / sizeof(mem_info[0]); i++) {
			if ((mem_info[i].flags & VM_KERN_SITE_TYPE) != VM_KERN_SITE_KMOD) {
				continue;
			}
			if (mem_info[i].size > (1024 * 1024)) {
				paniclog_append_noflush("%-5lld %10lld\n",
				    mem_info[i].site, mem_info[i].size);
			}
		}
	}
}

static void
panic_display_zone_info(void)
{
	paniclog_append_noflush("Zone info:\n");
	for (uint32_t i = 0; i < ZONE_ADDR_KIND_COUNT; i++) {
		paniclog_append_noflush("  %-8s: %p - %p\n",
		    zone_map_range_names[i],
		    (void *) zone_info.zi_map_range[i].min_address,
		    (void *) zone_info.zi_map_range[i].max_address);
	}
	paniclog_append_noflush("  Metadata: %p - %p\n"
	    "  Bitmaps : %p - %p\n"
	    "\n",
	    (void *) zone_info.zi_meta_range.min_address,
	    (void *) zone_info.zi_meta_range.max_address,
	    (void *) zone_info.zi_bits_range.min_address,
	    (void *) zone_info.zi_bits_range.max_address);
}

static void
panic_display_zone_fault(vm_offset_t addr)
{
	struct zone_page_metadata meta = { };
	vm_map_t map = VM_MAP_NULL;
	vm_offset_t oob_offs = 0, size = 0;
	int map_idx = -1;
	zone_t z = NULL;
	const char *kind = "whild deref";
	bool oob = false;

	/*
	 * First: look if we bumped into guard pages between submaps
	 */
	for (int i = 0; i < Z_SUBMAP_IDX_COUNT; i++) {
		map = zone_submaps[i];
		if (map == VM_MAP_NULL) {
			continue;
		}

		if (addr >= map->min_offset && addr < map->max_offset) {
			map_idx = i;
			break;
		}
	}

	if (map_idx == -1) {
		/* this really shouldn't happen, submaps are back to back */
		return;
	}

	paniclog_append_noflush("Probabilistic GZAlloc Report:\n");

	/*
	 * Second: look if there's just no metadata at all
	 */
	if (ml_nofault_copy((vm_offset_t)zone_meta_from_addr(addr),
	    (vm_offset_t)&meta, sizeof(meta)) != sizeof(meta) ||
	    meta.zm_index == 0 || meta.zm_index >= MAX_ZONES ||
	    zone_array[meta.zm_index].z_self == NULL) {
		paniclog_append_noflush("  Zone    : <unknown>\n");
		kind = "wild deref, missing or invalid metadata";
	} else {
		z = &zone_array[meta.zm_index];
		paniclog_append_noflush("  Zone    : %s%s\n",
		    zone_heap_name(z), zone_name(z));
		if (meta.zm_chunk_len == ZM_PGZ_GUARD) {
			kind = "out-of-bounds (high confidence)";
			oob = true;
			size = zone_element_size((void *)addr,
			    &z, false, &oob_offs);
		} else {
			kind = "use-after-free (medium confidence)";
		}
	}

	paniclog_append_noflush("  Address : %p\n", (void *)addr);
	if (oob) {
		paniclog_append_noflush("  Element : [%p, %p) of size %d\n",
		    (void *)(trunc_page(addr) - (size - oob_offs)),
		    (void *)trunc_page(addr), (uint32_t)(size - oob_offs));
	}
	paniclog_append_noflush("  Submap  : %s [%p; %p)\n",
	    zone_submaps_names[map_idx],
	    (void *)map->min_offset, (void *)map->max_offset);
	paniclog_append_noflush("  Kind    : %s\n", kind);
	if (oob) {
		paniclog_append_noflush("  Access  : %d byte(s) past\n",
		    (uint32_t)(addr & PAGE_MASK) + 1);
	}
	paniclog_append_noflush("  Metadata: zid:%d inl:%d cl:0x%x "
	    "0x%04x 0x%08x 0x%08x 0x%08x\n",
	    meta.zm_index, meta.zm_inline_bitmap, meta.zm_chunk_len,
	    meta.zm_alloc_size, meta.zm_bitmap,
	    meta.zm_page_next.packed_address,
	    meta.zm_page_prev.packed_address);
	paniclog_append_noflush("\n");
}

void
panic_display_zalloc(void)
{
	bool keepsyms = false;

	PE_parse_boot_argn("keepsyms", &keepsyms, sizeof(keepsyms));

	panic_display_zone_info();

	if (panic_fault_address) {
#if CONFIG_PROB_GZALLOC
		if (pgz_owned(panic_fault_address)) {
			panic_display_pgz_uaf_info(keepsyms, panic_fault_address);
		} else
#endif /* CONFIG_PROB_GZALLOC */
		if (zone_maps_owned(panic_fault_address, 1)) {
			panic_display_zone_fault(panic_fault_address);
		}
	}

	if (panic_include_zprint) {
		panic_display_zprint();
	} else if (zone_map_nearing_threshold(ZONE_MAP_EXHAUSTION_PRINT_PANIC)) {
		panic_display_largest_zones();
	}
#if CONFIG_ZLEAKS
	if (zleak_active) {
		panic_display_zleaks(keepsyms);
	}
#endif
	if (panic_include_kalloc_types) {
		panic_display_kalloc_types();
	}
}

/*
 * Creates a vm_map_copy_t to return to the caller of mach_* MIG calls
 * requesting zone information.
 * Frees unused pages towards the end of the region, and zero'es out unused
 * space on the last page.
 */
static vm_map_copy_t
create_vm_map_copy(
	vm_offset_t             start_addr,
	vm_size_t               total_size,
	vm_size_t               used_size)
{
	kern_return_t   kr;
	vm_offset_t             end_addr;
	vm_size_t               free_size;
	vm_map_copy_t   copy;

	if (used_size != total_size) {
		end_addr = start_addr + used_size;
		free_size = total_size - (round_page(end_addr) - start_addr);

		if (free_size >= PAGE_SIZE) {
			kmem_free(ipc_kernel_map,
			    round_page(end_addr), free_size);
		}
		bzero((char *) end_addr, round_page(end_addr) - end_addr);
	}

	kr = vm_map_copyin(ipc_kernel_map, (vm_map_address_t)start_addr,
	    (vm_map_size_t)used_size, TRUE, &copy);
	assert(kr == KERN_SUCCESS);

	return copy;
}

static boolean_t
get_zone_info(
	zone_t                   z,
	mach_zone_name_t        *zn,
	mach_zone_info_t        *zi)
{
	struct zone zcopy;
	vm_size_t cached = 0;

	assert(z != ZONE_NULL);
	zone_lock(z);
	if (!z->z_self) {
		zone_unlock(z);
		return FALSE;
	}
	zcopy = *z;
	if (z->z_pcpu_cache) {
		zpercpu_foreach(zc, z->z_pcpu_cache) {
			cached += zc->zc_alloc_cur + zc->zc_free_cur;
			cached += zc->zc_depot_cur * zc_mag_size();
		}
	}
	zone_unlock(z);

	if (zn != NULL) {
		/*
		 * Append kalloc heap name to zone name (if zone is used by kalloc)
		 */
		char temp_zone_name[MAX_ZONE_NAME] = "";
		snprintf(temp_zone_name, MAX_ZONE_NAME, "%s%s",
		    zone_heap_name(z), z->z_name);

		/* assuming here the name data is static */
		(void) __nosan_strlcpy(zn->mzn_name, temp_zone_name,
		    strlen(temp_zone_name) + 1);
	}

	if (zi != NULL) {
		*zi = (mach_zone_info_t) {
			.mzi_count = zone_count_allocated(&zcopy) - cached,
			.mzi_cur_size = ptoa_64(zone_scale_for_percpu(&zcopy, zcopy.z_wired_cur)),
			// max_size for zprint is now high-watermark of pages used
			.mzi_max_size = ptoa_64(zone_scale_for_percpu(&zcopy, zcopy.z_wired_hwm)),
			.mzi_elem_size = zone_scale_for_percpu(&zcopy, zcopy.z_elem_size),
			.mzi_alloc_size = ptoa_64(zcopy.z_chunk_pages),
			.mzi_exhaustible = (uint64_t)zcopy.exhaustible,
		};
		zpercpu_foreach(zs, zcopy.z_stats) {
			zi->mzi_sum_size += zs->zs_mem_allocated;
		}
		if (zcopy.collectable) {
			SET_MZI_COLLECTABLE_BYTES(zi->mzi_collectable,
			    ptoa_64(zone_scale_for_percpu(&zcopy, zcopy.z_wired_empty)));
			SET_MZI_COLLECTABLE_FLAG(zi->mzi_collectable, TRUE);
		}
	}

	return TRUE;
}

kern_return_t
task_zone_info(
	__unused task_t                                 task,
	__unused mach_zone_name_array_t *namesp,
	__unused mach_msg_type_number_t *namesCntp,
	__unused task_zone_info_array_t *infop,
	__unused mach_msg_type_number_t *infoCntp)
{
	return KERN_FAILURE;
}


/* mach_memory_info entitlement */
#define MEMORYINFO_ENTITLEMENT "com.apple.private.memoryinfo"

/* macro needed to rate-limit mach_memory_info */
#define NSEC_DAY (NSEC_PER_SEC * 60 * 60 * 24)

/* declarations necessary to call kauth_cred_issuser() */
struct ucred;
extern int kauth_cred_issuser(struct ucred *);
extern struct ucred *kauth_cred_get(void);

static kern_return_t
mach_memory_info_security_check(void)
{
	/* If not root or does not have the memoryinfo entitlement, fail */
	if (!kauth_cred_issuser(kauth_cred_get())) {
		return KERN_NO_ACCESS;
	}

#if CONFIG_DEBUGGER_FOR_ZONE_INFO
	if (!IOTaskHasEntitlement(current_task(), MEMORYINFO_ENTITLEMENT)) {
		return KERN_DENIED;
	}

	/*
	 * On release non-mac arm devices, allow mach_memory_info
	 * to be called twice per day per boot. memorymaintenanced
	 * calls it once per day, which leaves room for a sysdiagnose.
	 */
	static uint64_t first_call, second_call = 0;
	uint64_t now = 0;
	absolutetime_to_nanoseconds(ml_get_timebase(), &now);

	if (!first_call) {
		first_call = now;
	} else if (!second_call) {
		second_call = now;
	} else if (first_call + NSEC_DAY > now) {
		return KERN_DENIED;
	} else if (first_call + NSEC_DAY < now) {
		first_call = now;
		second_call = 0;
	}
#endif

	return KERN_SUCCESS;
}

kern_return_t
mach_zone_info(
	host_priv_t             host,
	mach_zone_name_array_t  *namesp,
	mach_msg_type_number_t  *namesCntp,
	mach_zone_info_array_t  *infop,
	mach_msg_type_number_t  *infoCntp)
{
	return mach_memory_info(host, namesp, namesCntp, infop, infoCntp, NULL, NULL);
}


kern_return_t
mach_memory_info(
	host_priv_t             host,
	mach_zone_name_array_t  *namesp,
	mach_msg_type_number_t  *namesCntp,
	mach_zone_info_array_t  *infop,
	mach_msg_type_number_t  *infoCntp,
	mach_memory_info_array_t *memoryInfop,
	mach_msg_type_number_t   *memoryInfoCntp)
{
	mach_zone_name_t        *names;
	vm_offset_t             names_addr;
	vm_size_t               names_size;

	mach_zone_info_t        *info;
	vm_offset_t             info_addr;
	vm_size_t               info_size;

	mach_memory_info_t      *memory_info;
	vm_offset_t             memory_info_addr;
	vm_size_t               memory_info_size;
	vm_size_t               memory_info_vmsize;
	unsigned int            num_info;

	unsigned int            max_zones, used_zones, i;
	mach_zone_name_t        *zn;
	mach_zone_info_t        *zi;
	kern_return_t           kr;

	uint64_t                zones_collectable_bytes = 0;

	if (host == HOST_NULL) {
		return KERN_INVALID_HOST;
	}

	kr = mach_memory_info_security_check();
	if (kr != KERN_SUCCESS) {
		return kr;
	}

	/*
	 *	We assume that zones aren't freed once allocated.
	 *	We won't pick up any zones that are allocated later.
	 */

	max_zones = os_atomic_load(&num_zones, relaxed);

	names_size = round_page(max_zones * sizeof *names);
	kr = kmem_alloc_pageable(ipc_kernel_map,
	    &names_addr, names_size, VM_KERN_MEMORY_IPC);
	if (kr != KERN_SUCCESS) {
		return kr;
	}
	names = (mach_zone_name_t *) names_addr;

	info_size = round_page(max_zones * sizeof *info);
	kr = kmem_alloc_pageable(ipc_kernel_map,
	    &info_addr, info_size, VM_KERN_MEMORY_IPC);
	if (kr != KERN_SUCCESS) {
		kmem_free(ipc_kernel_map,
		    names_addr, names_size);
		return kr;
	}
	info = (mach_zone_info_t *) info_addr;

	zn = &names[0];
	zi = &info[0];

	used_zones = max_zones;
	for (i = 0; i < max_zones; i++) {
		if (!get_zone_info(&(zone_array[i]), zn, zi)) {
			used_zones--;
			continue;
		}
		zones_collectable_bytes += GET_MZI_COLLECTABLE_BYTES(zi->mzi_collectable);
		zn++;
		zi++;
	}

	*namesp = (mach_zone_name_t *) create_vm_map_copy(names_addr, names_size, used_zones * sizeof *names);
	*namesCntp = used_zones;

	*infop = (mach_zone_info_t *) create_vm_map_copy(info_addr, info_size, used_zones * sizeof *info);
	*infoCntp = used_zones;

	num_info = 0;
	memory_info_addr = 0;

	if (memoryInfop && memoryInfoCntp) {
		vm_map_copy_t           copy;
		num_info = vm_page_diagnose_estimate();
		memory_info_size = num_info * sizeof(*memory_info);
		memory_info_vmsize = round_page(memory_info_size);
		kr = kmem_alloc_pageable(ipc_kernel_map,
		    &memory_info_addr, memory_info_vmsize, VM_KERN_MEMORY_IPC);
		if (kr != KERN_SUCCESS) {
			return kr;
		}

		kr = vm_map_wire_kernel(ipc_kernel_map, memory_info_addr, memory_info_addr + memory_info_vmsize,
		    VM_PROT_READ | VM_PROT_WRITE, VM_KERN_MEMORY_IPC, FALSE);
		assert(kr == KERN_SUCCESS);

		memory_info = (mach_memory_info_t *) memory_info_addr;
		vm_page_diagnose(memory_info, num_info, zones_collectable_bytes);

		kr = vm_map_unwire(ipc_kernel_map, memory_info_addr, memory_info_addr + memory_info_vmsize, FALSE);
		assert(kr == KERN_SUCCESS);

		kr = vm_map_copyin(ipc_kernel_map, (vm_map_address_t)memory_info_addr,
		    (vm_map_size_t)memory_info_size, TRUE, &copy);
		assert(kr == KERN_SUCCESS);

		*memoryInfop = (mach_memory_info_t *) copy;
		*memoryInfoCntp = num_info;
	}

	return KERN_SUCCESS;
}

kern_return_t
mach_zone_info_for_zone(
	host_priv_t                     host,
	mach_zone_name_t        name,
	mach_zone_info_t        *infop)
{
	zone_t zone_ptr;

	if (host == HOST_NULL) {
		return KERN_INVALID_HOST;
	}

#if CONFIG_DEBUGGER_FOR_ZONE_INFO
	if (!PE_i_can_has_debugger(NULL)) {
		return KERN_INVALID_HOST;
	}
#endif

	if (infop == NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	zone_ptr = ZONE_NULL;
	zone_foreach(z) {
		/*
		 * Append kalloc heap name to zone name (if zone is used by kalloc)
		 */
		char temp_zone_name[MAX_ZONE_NAME] = "";
		snprintf(temp_zone_name, MAX_ZONE_NAME, "%s%s",
		    zone_heap_name(z), z->z_name);

		/* Find the requested zone by name */
		if (track_this_zone(temp_zone_name, name.mzn_name)) {
			zone_ptr = z;
			break;
		}
	}

	/* No zones found with the requested zone name */
	if (zone_ptr == ZONE_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	if (get_zone_info(zone_ptr, NULL, infop)) {
		return KERN_SUCCESS;
	}
	return KERN_FAILURE;
}

kern_return_t
mach_zone_info_for_largest_zone(
	host_priv_t                     host,
	mach_zone_name_t        *namep,
	mach_zone_info_t        *infop)
{
	if (host == HOST_NULL) {
		return KERN_INVALID_HOST;
	}

#if CONFIG_DEBUGGER_FOR_ZONE_INFO
	if (!PE_i_can_has_debugger(NULL)) {
		return KERN_INVALID_HOST;
	}
#endif

	if (namep == NULL || infop == NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	if (get_zone_info(zone_find_largest(NULL), namep, infop)) {
		return KERN_SUCCESS;
	}
	return KERN_FAILURE;
}

uint64_t
get_zones_collectable_bytes(void)
{
	uint64_t zones_collectable_bytes = 0;
	mach_zone_info_t zi;

	zone_foreach(z) {
		if (get_zone_info(z, NULL, &zi)) {
			zones_collectable_bytes +=
			    GET_MZI_COLLECTABLE_BYTES(zi.mzi_collectable);
		}
	}

	return zones_collectable_bytes;
}

kern_return_t
mach_zone_get_zlog_zones(
	host_priv_t                             host,
	mach_zone_name_array_t  *namesp,
	mach_msg_type_number_t  *namesCntp)
{
#if ZONE_ENABLE_LOGGING
	unsigned int max_zones, logged_zones, i;
	kern_return_t kr;
	zone_t zone_ptr;
	mach_zone_name_t *names;
	vm_offset_t names_addr;
	vm_size_t names_size;

	if (host == HOST_NULL) {
		return KERN_INVALID_HOST;
	}

	if (namesp == NULL || namesCntp == NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	max_zones = os_atomic_load(&num_zones, relaxed);

	names_size = round_page(max_zones * sizeof *names);
	kr = kmem_alloc_pageable(ipc_kernel_map,
	    &names_addr, names_size, VM_KERN_MEMORY_IPC);
	if (kr != KERN_SUCCESS) {
		return kr;
	}
	names = (mach_zone_name_t *) names_addr;

	zone_ptr = ZONE_NULL;
	logged_zones = 0;
	for (i = 0; i < max_zones; i++) {
		zone_t z = &(zone_array[i]);
		assert(z != ZONE_NULL);

		/* Copy out the zone name if zone logging is enabled */
		if (z->z_btlog) {
			get_zone_info(z, &names[logged_zones], NULL);
			logged_zones++;
		}
	}

	*namesp = (mach_zone_name_t *) create_vm_map_copy(names_addr, names_size, logged_zones * sizeof *names);
	*namesCntp = logged_zones;

	return KERN_SUCCESS;

#else /* ZONE_ENABLE_LOGGING */
#pragma unused(host, namesp, namesCntp)
	return KERN_FAILURE;
#endif /* ZONE_ENABLE_LOGGING */
}

kern_return_t
mach_zone_get_btlog_records(
	host_priv_t             host,
	mach_zone_name_t        name,
	zone_btrecord_array_t  *recsp,
	mach_msg_type_number_t *numrecs)
{
#if ZONE_ENABLE_LOGGING
	zone_btrecord_t *recs;
	kern_return_t    kr;
	vm_address_t     addr;
	vm_size_t        size;
	zone_t           zone_ptr;
	vm_map_copy_t    copy;

	if (host == HOST_NULL) {
		return KERN_INVALID_HOST;
	}

	if (recsp == NULL || numrecs == NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	zone_ptr = ZONE_NULL;
	zone_foreach(z) {
		/*
		 * Append kalloc heap name to zone name (if zone is used by kalloc)
		 */
		char temp_zone_name[MAX_ZONE_NAME] = "";
		snprintf(temp_zone_name, MAX_ZONE_NAME, "%s%s",
		    zone_heap_name(z), z->z_name);

		/* Find the requested zone by name */
		if (track_this_zone(temp_zone_name, name.mzn_name)) {
			zone_ptr = z;
			break;
		}
	}

	/* No zones found with the requested zone name */
	if (zone_ptr == ZONE_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	/* Logging not turned on for the requested zone */
	if (!zone_ptr->z_btlog) {
		return KERN_FAILURE;
	}

	kr = btlog_get_records(zone_ptr->z_btlog, &recs, numrecs);
	if (kr != KERN_SUCCESS) {
		return kr;
	}

	addr = (vm_address_t)recs;
	size = sizeof(zone_btrecord_t) * *numrecs;

	kr = vm_map_copyin(ipc_kernel_map, addr, size, TRUE, &copy);
	assert(kr == KERN_SUCCESS);

	*recsp = (zone_btrecord_t *)copy;
	return KERN_SUCCESS;

#else /* !ZONE_ENABLE_LOGGING */
#pragma unused(host, name, recsp, numrecs)
	return KERN_FAILURE;
#endif /* !ZONE_ENABLE_LOGGING */
}


#if DEBUG || DEVELOPMENT

kern_return_t
mach_memory_info_check(void)
{
	mach_memory_info_t * memory_info;
	mach_memory_info_t * info;
	unsigned int         num_info;
	vm_offset_t          memory_info_addr;
	kern_return_t        kr;
	size_t               memory_info_size, memory_info_vmsize;
	uint64_t             top_wired, zonestotal, total;

	num_info = vm_page_diagnose_estimate();
	memory_info_size = num_info * sizeof(*memory_info);
	memory_info_vmsize = round_page(memory_info_size);
	kr = kmem_alloc(kernel_map, &memory_info_addr, memory_info_vmsize, VM_KERN_MEMORY_DIAG);
	assert(kr == KERN_SUCCESS);

	memory_info = (mach_memory_info_t *) memory_info_addr;
	vm_page_diagnose(memory_info, num_info, 0);

	top_wired = total = zonestotal = 0;
	zone_foreach(z) {
		zonestotal += zone_size_wired(z);
	}

	for (uint32_t idx = 0; idx < num_info; idx++) {
		info = &memory_info[idx];
		if (!info->size) {
			continue;
		}
		if (VM_KERN_COUNT_WIRED == info->site) {
			top_wired = info->size;
		}
		if (VM_KERN_SITE_HIDE & info->flags) {
			continue;
		}
		if (!(VM_KERN_SITE_WIRED & info->flags)) {
			continue;
		}
		total += info->size;
	}
	total += zonestotal;

	printf("vm_page_diagnose_check %qd of %qd, zones %qd, short 0x%qx\n",
	    total, top_wired, zonestotal, top_wired - total);

	kmem_free(kernel_map, memory_info_addr, memory_info_vmsize);

	return kr;
}

extern boolean_t(*volatile consider_buffer_cache_collect)(int);

#endif /* DEBUG || DEVELOPMENT */

kern_return_t
mach_zone_force_gc(
	host_t host)
{
	if (host == HOST_NULL) {
		return KERN_INVALID_HOST;
	}

#if DEBUG || DEVELOPMENT
	/* Callout to buffer cache GC to drop elements in the apfs zones */
	if (consider_buffer_cache_collect != NULL) {
		(void)(*consider_buffer_cache_collect)(0);
	}
	zone_gc(ZONE_GC_DRAIN);
#endif /* DEBUG || DEVELOPMENT */
	return KERN_SUCCESS;
}

zone_t
zone_find_largest(uint64_t *zone_size)
{
	zone_t    largest_zone  = 0;
	uint64_t  largest_zone_size = 0;
	zone_find_n_largest(1, &largest_zone, &largest_zone_size);
	if (zone_size) {
		*zone_size = largest_zone_size;
	}
	return largest_zone;
}

#endif /* !ZALLOC_TEST */
#pragma mark zone creation, configuration, destruction
#if !ZALLOC_TEST

static zone_t
zone_init_defaults(zone_id_t zid)
{
	zone_t z = &zone_array[zid];

	z->z_wired_max = ~0u;
	z->collectable = true;
	z->expandable = true;

	lck_spin_init(&z->z_lock, &zone_locks_grp, LCK_ATTR_NULL);
	STAILQ_INIT(&z->z_recirc);
	return z;
}

static bool
zone_is_initializing(zone_t z)
{
	return !z->z_self && !z->z_destroyed;
}

void
zone_set_noexpand(zone_t zone, vm_size_t nelems)
{
	if (!zone_is_initializing(zone)) {
		panic("%s: called after zone_create()", __func__);
	}
	zone->expandable = false;
	zone->z_wired_max = zone_alloc_pages_for_nelems(zone, nelems);
}

void
zone_set_exhaustible(zone_t zone, vm_size_t nelems)
{
	if (!zone_is_initializing(zone)) {
		panic("%s: called after zone_create()", __func__);
	}
	zone->expandable = false;
	zone->exhaustible = true;
	zone->z_wired_max = zone_alloc_pages_for_nelems(zone, nelems);
}

/**
 * @function zone_create_find
 *
 * @abstract
 * Finds an unused zone for the given name and element size.
 *
 * @param name          the zone name
 * @param size          the element size (including redzones, ...)
 * @param flags         the flags passed to @c zone_create*
 * @param zid_inout     the desired zone ID or ZONE_ID_ANY
 *
 * @returns             a zone to initialize further.
 */
static zone_t
zone_create_find(
	const char             *name,
	vm_size_t               size,
	zone_create_flags_t     flags,
	zone_id_t              *zid_inout)
{
	zone_id_t nzones, zid = *zid_inout;
	zone_t z;

	simple_lock(&all_zones_lock, &zone_locks_grp);

	nzones = (zone_id_t)os_atomic_load(&num_zones, relaxed);
	assert(num_zones_in_use <= nzones && nzones < MAX_ZONES);

	if (__improbable(nzones < ZONE_ID__FIRST_DYNAMIC)) {
		/*
		 * The first time around, make sure the reserved zone IDs
		 * have an initialized lock as zone_index_foreach() will
		 * enumerate them.
		 */
		while (nzones < ZONE_ID__FIRST_DYNAMIC) {
			zone_init_defaults(nzones++);
		}

		os_atomic_store(&num_zones, nzones, release);
	}

	if (zid != ZONE_ID_ANY) {
		if (zid >= ZONE_ID__FIRST_DYNAMIC) {
			panic("zone_create: invalid desired zone ID %d for %s",
			    zid, name);
		}
		if (flags & ZC_DESTRUCTIBLE) {
			panic("zone_create: ID %d (%s) must be permanent", zid, name);
		}
		if (zone_array[zid].z_self) {
			panic("zone_create: creating zone ID %d (%s) twice", zid, name);
		}
		z = &zone_array[zid];
	} else {
		if (flags & ZC_DESTRUCTIBLE) {
			/*
			 * If possible, find a previously zdestroy'ed zone in the
			 * zone_array that we can reuse.
			 */
			for (int i = bitmap_first(zone_destroyed_bitmap, MAX_ZONES);
			    i >= 0; i = bitmap_next(zone_destroyed_bitmap, i)) {
				z = &zone_array[i];

				/*
				 * If the zone name and the element size are the
				 * same, we can just reuse the old zone struct.
				 */
				if (strcmp(z->z_name, name) || zone_elem_size(z) != size) {
					continue;
				}
				bitmap_clear(zone_destroyed_bitmap, i);
				z->z_destroyed = false;
				z->z_self = z;
				zid = (zone_id_t)i;
				goto out;
			}
		}

		zid = nzones++;
		z = zone_init_defaults(zid);

		/*
		 * The release barrier pairs with the acquire in
		 * zone_index_foreach() and makes sure that enumeration loops
		 * always see an initialized zone lock.
		 */
		os_atomic_store(&num_zones, nzones, release);
	}

out:
	num_zones_in_use++;
	simple_unlock(&all_zones_lock);

	*zid_inout = zid;
	return z;
}

__abortlike
static void
zone_create_panic(const char *name, const char *f1, const char *f2)
{
	panic("zone_create: creating zone %s: flag %s and %s are incompatible",
	    name, f1, f2);
}
#define zone_create_assert_not_both(name, flags, current_flag, forbidden_flag) \
	if ((flags) & forbidden_flag) { \
	        zone_create_panic(name, #current_flag, #forbidden_flag); \
	}

/*
 * Adjusts the size of the element based on minimum size, alignment
 * and kasan redzones
 */
static vm_size_t
zone_elem_adjust_size(
	const char             *name __unused,
	vm_size_t               elem_size,
	zone_create_flags_t     flags __unused,
	uint32_t               *redzone __unused)
{
	vm_size_t size;
	/*
	 * Adjust element size for minimum size and pointer alignment
	 */
	size = (elem_size + ZONE_ALIGN_SIZE - 1) & -ZONE_ALIGN_SIZE;
	if (size < ZONE_MIN_ELEM_SIZE) {
		size = ZONE_MIN_ELEM_SIZE;
	}

#if KASAN_ZALLOC
	/*
	 * Expand the zone allocation size to include the redzones.
	 *
	 * For page-multiple zones add a full guard page because they
	 * likely require alignment.
	 */
	uint32_t redzone_tmp;
	if (flags & (ZC_KASAN_NOREDZONE | ZC_PERCPU)) {
		redzone_tmp = 0;
	} else if ((size & PAGE_MASK) == 0) {
		if (size != PAGE_SIZE && (flags & ZC_ALIGNMENT_REQUIRED)) {
			panic("zone_create: zone %s can't provide more than PAGE_SIZE"
			    "alignment", name);
		}
		redzone_tmp = PAGE_SIZE;
	} else if (flags & ZC_ALIGNMENT_REQUIRED) {
		redzone_tmp = 0;
	} else {
		redzone_tmp = KASAN_GUARD_SIZE;
	}
	size += redzone_tmp * 2;
	if (redzone) {
		*redzone = redzone_tmp;
	}
#endif
	return size;
}

/*
 * Returns the allocation chunk size that has least framentation
 */
static vm_size_t
zone_get_min_alloc_granule(
	vm_size_t               elem_size,
	zone_create_flags_t     flags)
{
	vm_size_t alloc_granule = PAGE_SIZE;
	if (flags & ZC_PERCPU) {
		alloc_granule = PAGE_SIZE * zpercpu_count();
		if (PAGE_SIZE % elem_size > 256) {
			panic("zone_create: per-cpu zone has too much fragmentation");
		}
	} else if (flags & ZC_READONLY) {
		alloc_granule = PAGE_SIZE;
	} else if ((elem_size & PAGE_MASK) == 0) {
		/* zero fragmentation by definition */
		alloc_granule = elem_size;
	} else if (alloc_granule % elem_size == 0) {
		/* zero fragmentation by definition */
	} else {
		vm_size_t frag = (alloc_granule % elem_size) * 100 / alloc_granule;
		vm_size_t alloc_tmp = PAGE_SIZE;
		vm_size_t max_chunk_size = ZONE_MAX_ALLOC_SIZE;
		/*
		 * Increase chunk size to 48K for sizes larger than 4K on 16k
		 * machines, so as to reduce internal fragementation for kalloc
		 * zones with sizes 12K and 24K.
		 */
#if __ARM_16K_PG__
		if (elem_size > 4 * 1024) {
			max_chunk_size = 48 * 1024;
		}
#endif
		while ((alloc_tmp += PAGE_SIZE) <= max_chunk_size) {
			vm_size_t frag_tmp = (alloc_tmp % elem_size) * 100 / alloc_tmp;
			if (frag_tmp < frag) {
				frag = frag_tmp;
				alloc_granule = alloc_tmp;
			}
		}
	}
	return alloc_granule;
}

vm_size_t
zone_get_foreign_alloc_size(
	const char             *name __unused,
	vm_size_t               elem_size,
	zone_create_flags_t     flags,
	uint16_t                min_pages)
{
	vm_size_t adjusted_size = zone_elem_adjust_size(name, elem_size, flags,
	    NULL);
	vm_size_t alloc_granule = zone_get_min_alloc_granule(adjusted_size,
	    flags);
	vm_size_t min_size = min_pages * PAGE_SIZE;
	/*
	 * Round up min_size to a multiple of alloc_granule
	 */
	return ((min_size + alloc_granule - 1) / alloc_granule)
	       * alloc_granule;
}

zone_t
zone_create_ext(
	const char             *name,
	vm_size_t               size,
	zone_create_flags_t     flags,
	zone_id_t               zid,
	void                  (^extra_setup)(zone_t))
{
	vm_size_t alloc;
	uint32_t redzone;
	zone_t z;
	zone_security_flags_t *zsflags;

	if (size > ZONE_MAX_ALLOC_SIZE) {
		panic("zone_create: element size too large: %zd", (size_t)size);
	}

	if (size < 2 * sizeof(vm_size_t)) {
		/* Elements are too small for kasan. */
		flags |= ZC_KASAN_NOQUARANTINE | ZC_KASAN_NOREDZONE;
	}

	size = zone_elem_adjust_size(name, size, flags, &redzone);
	/*
	 * Allocate the zone slot, return early if we found an older match.
	 */
	z = zone_create_find(name, size, flags, &zid);
	if (__improbable(z->z_self)) {
		/* We found a zone to reuse */
		return z;
	}

	/*
	 * Initialize the zone properly.
	 */

	/*
	 * If the kernel is post lockdown, copy the zone name passed in.
	 * Else simply maintain a pointer to the name string as it can only
	 * be a core XNU zone (no unloadable kext exists before lockdown).
	 */
	if (startup_phase >= STARTUP_SUB_LOCKDOWN) {
		size_t nsz = MIN(strlen(name) + 1, MACH_ZONE_NAME_MAX_LEN);
		char *buf = zalloc_permanent(nsz, ZALIGN_NONE);
		strlcpy(buf, name, nsz);
		z->z_name = buf;
	} else {
		z->z_name = name;
	}
	if (__probable(zone_array[ZONE_ID_PERCPU_PERMANENT].z_self)) {
		z->z_stats = zalloc_percpu_permanent_type(struct zone_stats);
	} else {
		/*
		 * zone_init() hasn't run yet, use the storage provided by
		 * zone_stats_startup(), and zone_init() will replace it
		 * with the final value once the PERCPU zone exists.
		 */
		z->z_stats = __zpcpu_mangle_for_boot(&zone_stats_startup[zone_index(z)]);
	}

	alloc = zone_get_min_alloc_granule(size, flags);

	z->z_elem_size = (uint16_t)size;
	z->z_chunk_pages = (uint16_t)atop(alloc);
	if (flags & ZC_PERCPU) {
		z->z_chunk_elems = (uint16_t)(PAGE_SIZE / z->z_elem_size);
	} else {
		z->z_chunk_elems = (uint16_t)(alloc / z->z_elem_size);
	}
	if (zone_element_idx(zone_element_encode(0,
	    z->z_chunk_elems - 1)) != z->z_chunk_elems - 1) {
		panic("zone_element_encode doesn't work for zone [%s]", name);
	}

#if KASAN_ZALLOC
	z->z_kasan_redzone = redzone;
	if (strncmp(name, "fakestack.", sizeof("fakestack.") - 1) == 0) {
		z->kasan_fakestacks = true;
	}
#endif

	/*
	 * Handle KPI flags
	 */
	zsflags = &zone_security_array[zid];
	/*
	 * Some zones like ipc ports and procs rely on sequestering for
	 * correctness, so explicitly turn on sequestering despite the
	 * configuration in zsecurity_options.
	 */
	if (flags & ZC_SEQUESTER) {
		zsflags->z_va_sequester = true;
	}

	/* ZC_CACHING applied after all configuration is done */
	if (flags & ZC_NOCACHING) {
		z->z_nocaching = true;
	}

	if (flags & ZC_READONLY) {
		zone_create_assert_not_both(name, flags, ZC_READONLY, ZC_VM);
#if ZSECURITY_CONFIG(READ_ONLY)
		zsflags->z_submap_idx = Z_SUBMAP_IDX_READ_ONLY;
		zsflags->z_va_sequester = true;
#endif
		zone_ro_elem_size[zid] = (uint16_t)size;
		assert(size <= PAGE_SIZE);
		if ((PAGE_SIZE % size) * 10 >= PAGE_SIZE) {
			panic("Fragmentation greater than 10%% with elem size %d zone %s%s",
			    (uint32_t)size, zone_heap_name(z), z->z_name);
		}
	}

	if (flags & ZC_PERCPU) {
		zone_create_assert_not_both(name, flags, ZC_PERCPU, ZC_ALLOW_FOREIGN);
		zone_create_assert_not_both(name, flags, ZC_PERCPU, ZC_READONLY);
		zone_create_assert_not_both(name, flags, ZC_PERCPU, ZC_PGZ_USE_GUARDS);
		z->z_percpu = true;
		z->z_nogzalloc = true;
	}
	if (flags & ZC_NOGC) {
		z->collectable = false;
	}
	/*
	 * Handle ZC_NOENCRYPT from xnu only
	 */
	if (startup_phase < STARTUP_SUB_LOCKDOWN && flags & ZC_NOENCRYPT) {
		zsflags->z_noencrypt = true;
	}
	if (flags & ZC_ALIGNMENT_REQUIRED) {
		z->alignment_required = true;
	}
	if (flags & (ZC_NOGZALLOC | ZC_READONLY)) {
		z->z_nogzalloc = true;
	}
	if (flags & ZC_NOCALLOUT) {
		z->no_callout = true;
	}
	if (flags & ZC_DESTRUCTIBLE) {
		zone_create_assert_not_both(name, flags, ZC_DESTRUCTIBLE, ZC_ALLOW_FOREIGN);
		zone_create_assert_not_both(name, flags, ZC_DESTRUCTIBLE, ZC_READONLY);
		z->z_destructible = true;
	}
	/*
	 * Handle Internal flags
	 */
#if CONFIG_PROB_GZALLOC
	if (flags & (ZC_PGZ_USE_GUARDS | ZC_KALLOC_HEAP | ZC_KALLOC_TYPE)) {
		/*
		 * Try to turn on guard pages only for zones
		 * with a chance of OOB.
		 */
		z->z_pgz_use_guards = true;
		z->z_pgz_oob_offs = (uint16_t)(alloc -
		    z->z_chunk_elems * z->z_elem_size);
	}
#endif
	if (!(flags & ZC_NOTBITAG)) {
		z->z_tbi_tag = true;
	}
	if (flags & ZC_KALLOC_TYPE) {
		zsflags->z_kalloc_type = true;
	}
	if (flags & ZC_ALLOW_FOREIGN) {
		zone_create_assert_not_both(name, flags,
		    ZC_ALLOW_FOREIGN, ZC_PGZ_USE_GUARDS);
		zsflags->z_allows_foreign = true;
	}
	if (flags & ZC_VM) {
		zsflags->z_submap_idx = Z_SUBMAP_IDX_VM;
		zsflags->z_va_sequester = true;
	}
	if (flags & ZC_KASAN_NOQUARANTINE) {
		z->kasan_noquarantine = true;
	}
	/* ZC_KASAN_NOREDZONE already handled */

	/*
	 * Then if there's extra tuning, do it
	 */
	if (extra_setup) {
		extra_setup(z);
	}

	/*
	 * Configure debugging features
	 */
#if CONFIG_GZALLOC
	if (!z->z_nogzalloc && (flags & ZC_VM) == 0) {
		gzalloc_zone_init(z); /* might set z->z_gzalloc_tracked */
		if (z->z_gzalloc_tracked) {
			z->z_nocaching = true;
		}
	}
#endif
#if CONFIG_PROB_GZALLOC
	if (!z->z_gzalloc_tracked && (flags & (ZC_READONLY | ZC_PERCPU)) == 0) {
		pgz_zone_init(z);
	}
#endif
#if ZONE_ENABLE_LOGGING
	if (!z->z_gzalloc_tracked && startup_phase >= STARTUP_SUB_ZALLOC) {
		/*
		 * Check for and set up zone leak detection
		 * if requested via boot-args.
		 */
		zone_setup_logging(z);
	}
#endif /* ZONE_ENABLE_LOGGING */

#if VM_TAG_SIZECLASSES
	if (!z->z_gzalloc_tracked && (zsflags->z_kheap_id || zsflags->z_kalloc_type)
	    && zone_tagging_on) {
		assert(startup_phase < STARTUP_SUB_LOCKDOWN);
		static uint16_t sizeclass_idx;
		z->z_uses_tags = true;
		z->z_tags_inline = (((page_size + size - 1) / size) <=
		    (sizeof(uint32_t) / sizeof(uint16_t)));
		if (zsflags->z_kheap_id == KHEAP_ID_DEFAULT) {
			zone_tags_sizeclasses[sizeclass_idx] = (uint16_t)size;
			z->z_tags_sizeclass = sizeclass_idx++;
		} else {
			uint16_t i = 0;
			for (; i < sizeclass_idx; i++) {
				if (size == zone_tags_sizeclasses[i]) {
					z->z_tags_sizeclass = i;
					break;
				}
			}
			/*
			 * Size class wasn't found, add it to zone_tags_sizeclasses
			 */
			if (i == sizeclass_idx) {
				assert(i < VM_TAG_SIZECLASSES);
				zone_tags_sizeclasses[i] = (uint16_t)size;
				z->z_tags_sizeclass = sizeclass_idx++;
			}
		}
		assert(z->z_tags_sizeclass < VM_TAG_SIZECLASSES);
	}
#endif

	/*
	 * Finally, fixup properties based on security policies, boot-args, ...
	 */
#if ZSECURITY_CONFIG(SUBMAP_USER_DATA)
	if (zsflags->z_kheap_id == KHEAP_ID_DATA_BUFFERS) {
		zsflags->z_submap_idx = Z_SUBMAP_IDX_DATA;
		zsflags->z_va_sequester = false;
	}
#endif

	if ((flags & ZC_CACHING) && !z->z_nocaching) {
		/*
		 * If zcache hasn't been initialized yet, remember our decision,
		 *
		 * zone_enable_caching() will be called again by
		 * zcache_bootstrap(), while the system is still single
		 * threaded, to build the missing caches.
		 */
		if (__probable(zc_magazine_zone)) {
			zone_enable_caching(z);
		} else {
			z->z_pcpu_cache =
			    __zpcpu_mangle_for_boot(&zone_cache_startup[zid]);
		}
	}

	zone_lock(z);
	z->z_self = z;
	zone_unlock(z);

	return z;
}

__startup_func
void
zone_create_startup(struct zone_create_startup_spec *spec)
{
	zone_t z;

	z = zone_create_ext(spec->z_name, spec->z_size,
	    spec->z_flags, spec->z_zid, spec->z_setup);
	if (spec->z_var) {
		*spec->z_var = z;
	}
}

/*
 * The 4 first field of a zone_view and a zone alias, so that the zone_or_view_t
 * union works. trust but verify.
 */
#define zalloc_check_zov_alias(f1, f2) \
    static_assert(offsetof(struct zone, f1) == offsetof(struct zone_view, f2))
zalloc_check_zov_alias(z_self, zv_zone);
zalloc_check_zov_alias(z_stats, zv_stats);
zalloc_check_zov_alias(z_name, zv_name);
zalloc_check_zov_alias(z_views, zv_next);
#undef zalloc_check_zov_alias

__startup_func
void
zone_view_startup_init(struct zone_view_startup_spec *spec)
{
	struct kalloc_heap *heap = NULL;
	zone_view_t zv = spec->zv_view;
	zone_t z;
	zone_security_flags_t zsflags;

	switch (spec->zv_heapid) {
	case KHEAP_ID_DEFAULT:
		panic("%s: Use KALLOC_TYPE_DEFINE for zone view %s instead"
		    "of ZONE_VIEW_DEFINE as it is from default kalloc heap",
		    __func__, zv->zv_name);
		__builtin_unreachable();
	case KHEAP_ID_DATA_BUFFERS:
		heap = KHEAP_DATA_BUFFERS;
		break;
	case KHEAP_ID_KEXT:
		heap = KHEAP_KEXT;
		break;
	default:
		heap = NULL;
	}

	if (heap) {
		z = kalloc_heap_zone_for_size(heap, spec->zv_size);
	} else {
		z = *spec->zv_zone;
		assert(spec->zv_size <= zone_elem_size(z));
	}

	assert(z);

	zv->zv_zone  = z;
	zv->zv_stats = zalloc_percpu_permanent_type(struct zone_stats);
	zv->zv_next  = z->z_views;
	zsflags = zone_security_config(z);
	if (z->z_views == NULL && zsflags.z_kheap_id == KHEAP_ID_NONE) {
		/*
		 * count the raw view for zones not in a heap,
		 * kalloc_heap_init() already counts it for its members.
		 */
		zone_view_count += 2;
	} else {
		zone_view_count += 1;
	}
	z->z_views = zv;
}

zone_t
zone_create(
	const char             *name,
	vm_size_t               size,
	zone_create_flags_t     flags)
{
	return zone_create_ext(name, size, flags, ZONE_ID_ANY, NULL);
}

static_assert(ZONE_ID__LAST_RO_EXT - ZONE_ID__FIRST_RO_EXT == ZC_RO_ID__LAST);

zone_id_t
zone_create_ro(
	const char             *name,
	vm_size_t               size,
	zone_create_flags_t     flags,
	zone_create_ro_id_t     zc_ro_id)
{
	assert(zc_ro_id <= ZC_RO_ID__LAST);
	zone_id_t reserved_zid = ZONE_ID__FIRST_RO_EXT + zc_ro_id;
	(void)zone_create_ext(name, size, ZC_READONLY | flags, reserved_zid, NULL);
	return reserved_zid;
}

zone_t
zinit(
	vm_size_t       size,           /* the size of an element */
	vm_size_t       max,            /* maximum memory to use */
	vm_size_t       alloc __unused, /* allocation size */
	const char      *name)          /* a name for the zone */
{
	zone_t z = zone_create(name, size, ZC_DESTRUCTIBLE);
	z->z_wired_max = zone_alloc_pages_for_nelems(z, max / size);
	return z;
}

void
zdestroy(zone_t z)
{
	unsigned int zindex = zone_index(z);
	zone_security_flags_t zsflags = zone_security_array[zindex];

	current_thread()->options |= TH_OPT_ZONE_PRIV;
	lck_mtx_lock(&zone_gc_lock);

	zone_reclaim(z, ZONE_RECLAIM_DESTROY);

	lck_mtx_unlock(&zone_gc_lock);
	current_thread()->options &= ~TH_OPT_ZONE_PRIV;

#if CONFIG_GZALLOC
	if (__improbable(z->z_gzalloc_tracked)) {
		/* If the zone is gzalloc managed dump all the elements in the free cache */
		gzalloc_empty_free_cache(z);
	}
#endif

	zone_lock(z);

	if (!zone_submap_is_sequestered(zsflags)) {
		while (!zone_pva_is_null(z->z_pageq_va)) {
			struct zone_page_metadata *meta;
			vm_offset_t free_addr;

			zone_counter_sub(z, z_va_cur, z->z_percpu ? 1 : z->z_chunk_pages);
			meta = zone_meta_queue_pop_native(z, &z->z_pageq_va, &free_addr);
			assert(meta->zm_chunk_len <= ZM_CHUNK_LEN_MAX);
			bzero(meta, sizeof(*meta) * z->z_chunk_pages);
			zone_unlock(z);
			kmem_free(zone_submap(zsflags), free_addr, ptoa(z->z_chunk_pages));
			zone_lock(z);
		}
	}

#if !KASAN_ZALLOC
	/* Assert that all counts are zero */
	if (z->z_elems_avail || z->z_elems_free || zone_size_wired(z) ||
	    (z->z_va_cur && !zone_submap_is_sequestered(zsflags))) {
		panic("zdestroy: Zone %s%s isn't empty at zdestroy() time",
		    zone_heap_name(z), z->z_name);
	}

	/* consistency check: make sure everything is indeed empty */
	assert(zone_pva_is_null(z->z_pageq_empty));
	assert(zone_pva_is_null(z->z_pageq_partial));
	assert(zone_pva_is_null(z->z_pageq_full));
	if (!zone_submap_is_sequestered(zsflags)) {
		assert(zone_pva_is_null(z->z_pageq_va));
	}
#endif

	zone_unlock(z);

	simple_lock(&all_zones_lock, &zone_locks_grp);

	assert(!bitmap_test(zone_destroyed_bitmap, zindex));
	/* Mark the zone as empty in the bitmap */
	bitmap_set(zone_destroyed_bitmap, zindex);
	num_zones_in_use--;
	assert(num_zones_in_use > 0);

	simple_unlock(&all_zones_lock);
}

#endif /* !ZALLOC_TEST */
#pragma mark zalloc module init
#if !ZALLOC_TEST

/*
 *	Initialize the "zone of zones" which uses fixed memory allocated
 *	earlier in memory initialization.  zone_bootstrap is called
 *	before zone_init.
 */
__startup_func
void
zone_bootstrap(void)
{
#if DEBUG || DEVELOPMENT
#if __x86_64__
	if (PE_parse_boot_argn("kernPOST", NULL, 0)) {
		/*
		 * rdar://79781535 Disable early gaps while running kernPOST on Intel
		 * the fp faulting code gets triggered and deadlocks.
		 */
		zone_caching_disabled = 1;
	}
#endif /* __x86_64__ */
#endif /* DEBUG || DEVELOPMENT */

	/* Validate struct zone_packed_virtual_address expectations */
	static_assert((intptr_t)VM_MIN_KERNEL_ADDRESS < 0, "the top bit must be 1");
	if (VM_KERNEL_POINTER_SIGNIFICANT_BITS - PAGE_SHIFT > 31) {
		panic("zone_pva_t can't pack a kernel page address in 31 bits");
	}

	zpercpu_early_count = ml_early_cpu_max_number() + 1;

	/*
	 * Initialize random used to scramble early allocations
	 */
	zpercpu_foreach_cpu(cpu) {
		random_bool_init(&zone_bool_gen[cpu].zbg_bg);
	}

#if CONFIG_PROB_GZALLOC
	/*
	 * Set pgz_sample_counter on the boot CPU so that we do not sample
	 * any allocation until PGZ has been properly setup (in pgz_init()).
	 */
	*PERCPU_GET_MASTER(pgz_sample_counter) = INT32_MAX;
#endif /* CONFIG_PROB_GZALLOC */

	/*
	 * the KASAN quarantine for kalloc doesn't understand heaps
	 * and trips the heap confusion panics. At the end of the day,
	 * all these security measures are double duty with KASAN.
	 *
	 * On 32bit kernels, these protections are just too expensive.
	 */
#if !defined(__LP64__) || KASAN_ZALLOC
	zsecurity_options &= ~ZSECURITY_OPTIONS_KERNEL_DATA_MAP;
#endif

#if ZSECURITY_CONFIG(SAD_FENG_SHUI)
	/*
	 * Randomly assign zones to one of the 4 general submaps,
	 * and pick whether they allocate from the begining
	 * or the end of it.
	 *
	 * A lot of OOB exploitation relies on precise interleaving
	 * of specific types in the heap.
	 *
	 * Woops, you can't guarantee that anymore.
	 */
	for (zone_id_t i = 1; i < MAX_ZONES; i++) {
		uint32_t r = zalloc_random_uniform(0,
		    ZSECURITY_CONFIG_GENERAL_SUBMAPS * 2);

		zone_security_array[i].z_submap_from_end = (r & 1);
		zone_security_array[i].z_submap_idx += (r >> 1);
	}
#endif /* ZSECURITY_CONFIG(SAD_FENG_SHUI) */

	thread_call_setup_with_options(&zone_expand_callout,
	    zone_expand_async, NULL, THREAD_CALL_PRIORITY_HIGH,
	    THREAD_CALL_OPTIONS_ONCE);

	thread_call_setup_with_options(&zone_defrag_callout,
	    zone_defrag_async, NULL, THREAD_CALL_PRIORITY_USER,
	    THREAD_CALL_OPTIONS_ONCE);
}

#define ZONE_GUARD_SIZE                 (64UL << 10)

#if __LP64__
static inline vm_offset_t
zone_restricted_va_max(void)
{
	vm_offset_t compressor_max = VM_PACKING_MAX_PACKABLE(C_SLOT_PACKED_PTR);
	vm_offset_t vm_page_max    = VM_PACKING_MAX_PACKABLE(VM_PAGE_PACKED_PTR);

	return trunc_page(MIN(compressor_max, vm_page_max));
}
#endif

__startup_func
static void
zone_tunables_fixup(void)
{
	int wdt = 0;

#if CONFIG_PROB_GZALLOC && (DEVELOPMENT || DEBUG)
	if (!PE_parse_boot_argn("pgz", NULL, 0) &&
	    PE_parse_boot_argn("pgz1", NULL, 0)) {
		/*
		 * if pgz1= was used, but pgz= was not,
		 * then the more specific pgz1 takes precedence.
		 */
		pgz_all = false;
	}
#endif

	if (zone_map_jetsam_limit == 0 || zone_map_jetsam_limit > 100) {
		zone_map_jetsam_limit = ZONE_MAP_JETSAM_LIMIT_DEFAULT;
	}
	if (zc_magazine_size > PAGE_SIZE / ZONE_MIN_ELEM_SIZE) {
		zc_magazine_size = (uint16_t)(PAGE_SIZE / ZONE_MIN_ELEM_SIZE);
	}
	if (PE_parse_boot_argn("wdt", &wdt, sizeof(wdt)) && wdt == -1 &&
	    !PE_parse_boot_argn("zet", NULL, 0)) {
		zone_exhausted_timeout = -1;
	}
}
STARTUP(TUNABLES, STARTUP_RANK_MIDDLE, zone_tunables_fixup);

__startup_func
static vm_size_t
zone_phys_size_max(void)
{
	vm_size_t zsize;
	vm_size_t zsizearg;

	if (PE_parse_boot_argn("zsize", &zsizearg, sizeof(zsizearg))) {
		zsize = zsizearg * (1024ULL * 1024);
	} else {
		/* Set target zone size as 1/4 of physical memory */
		zsize = (vm_size_t)(sane_size >> 2);
#if defined(__LP64__)
		zsize += zsize >> 1;
#endif /* __LP64__ */
	}

	if (zsize < CONFIG_ZONE_MAP_MIN) {
		zsize = CONFIG_ZONE_MAP_MIN;   /* Clamp to min */
	}
	if (zsize > sane_size >> 1) {
		zsize = (vm_size_t)(sane_size >> 1); /* Clamp to half of RAM max */
	}
	if (zsizearg == 0 && zsize > ZONE_MAP_MAX) {
		/* if zsize boot-arg not present and zsize exceeds platform maximum, clip zsize */
		printf("NOTE: zonemap size reduced from 0x%lx to 0x%lx\n",
		    (uintptr_t)zsize, (uintptr_t)ZONE_MAP_MAX);
		zsize = ZONE_MAP_MAX;
	}

	return (vm_size_t)trunc_page(zsize);
}

__startup_func
static struct zone_map_range
zone_init_allocate_va(vm_map_address_t addr, vm_size_t size, bool random)
{
	int vm_alloc_flags = VM_FLAGS_ANYWHERE;
	struct zone_map_range r;
	kern_return_t kr;
	vm_map_entry_t entry;

	if (random) {
		vm_alloc_flags |= VM_FLAGS_RANDOM_ADDR;
	}

	vm_object_reference(kernel_object);

	kr = vm_map_enter(kernel_map, &addr, size, 0,
	    vm_alloc_flags, VM_MAP_KERNEL_FLAGS_NONE, VM_KERN_MEMORY_ZONE,
	    kernel_object, addr, FALSE, VM_PROT_NONE, VM_PROT_NONE,
	    VM_INHERIT_NONE);

	if (KERN_SUCCESS != kr) {
		panic("vm_map_enter(0x%zx) failed: %d", (size_t)size, kr);
	}

	vm_map_lookup_entry(kernel_map, addr, &entry);
	VME_OFFSET_SET(entry, addr);

	r.min_address = (vm_offset_t)addr;
	r.max_address = (vm_offset_t)addr + size;
	return r;
}

__startup_func
static void
zone_submap_init(
	vm_offset_t            *submap_min,
	zone_submap_idx_t       idx,
	uint64_t                zone_sub_map_numer,
	uint64_t               *remaining_denom,
	vm_offset_t            *remaining_size)
{
	vm_map_address_t addr;
	vm_offset_t submap_start, submap_end;
	vm_size_t submap_size;
	vm_map_t  submap;
	vm_prot_t prot = VM_PROT_DEFAULT;
	kern_return_t kr;

	submap_size = trunc_page(zone_sub_map_numer * *remaining_size /
	    *remaining_denom);
	submap_start = *submap_min;
	submap_end = submap_start + submap_size;

#if defined(__LP64__)
	if (idx == Z_SUBMAP_IDX_VM) {
		vm_offset_t restricted_va_max = zone_restricted_va_max();
		if (submap_end > restricted_va_max) {
#if DEBUG || DEVELOPMENT
			printf("zone_init: submap[%d] clipped to %zdM of %zdM\n", idx,
			    (size_t)(restricted_va_max - submap_start) >> 20,
			    (size_t)submap_size >> 20);
#endif /* DEBUG || DEVELOPMENT */
			*remaining_size -= submap_end - restricted_va_max;
			submap_end  = restricted_va_max;
			submap_size = restricted_va_max - submap_start;
		}

		vm_packing_verify_range("vm_compressor",
		    submap_start, submap_end, VM_PACKING_PARAMS(C_SLOT_PACKED_PTR));
		vm_packing_verify_range("vm_page",
		    submap_start, submap_end, VM_PACKING_PARAMS(VM_PAGE_PACKED_PTR));
	}
#endif /* defined(__LP64__) */

	zone_kmem_suballoc(*submap_min, submap_size,
	    VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE,
	    VM_KERN_MEMORY_ZONE, &submap);

	if (idx == Z_SUBMAP_IDX_READ_ONLY) {
		zone_info.zi_map_range[ZONE_ADDR_READONLY].min_address = submap_start;
		zone_info.zi_map_range[ZONE_ADDR_READONLY].max_address = submap_end;
		prot = VM_PROT_NONE;
	}

	addr = submap_start;
	kr = vm_map_enter(submap, &addr, ZONE_GUARD_SIZE / 2, 0,
	    VM_FLAGS_FIXED | VM_FLAGS_PERMANENT,
	    VM_MAP_KERNEL_FLAGS_NONE, VM_KERN_MEMORY_ZONE,
	    kernel_object, addr, FALSE, prot, prot, VM_INHERIT_NONE);
	if (kr != KERN_SUCCESS) {
		panic("ksubmap[%d]: failed to make first entry (%d)", idx, kr);
	}

	addr = submap_end - ZONE_GUARD_SIZE / 2;
	kr = vm_map_enter(submap, &addr, ZONE_GUARD_SIZE / 2, 0,
	    VM_FLAGS_FIXED | VM_FLAGS_PERMANENT,
	    VM_MAP_KERNEL_FLAGS_NONE, VM_KERN_MEMORY_ZONE,
	    kernel_object, addr, FALSE, prot, prot, VM_INHERIT_NONE);
	if (kr != KERN_SUCCESS) {
		panic("ksubmap[%d]: failed to make last entry (%d)", idx, kr);
	}

#if DEBUG || DEVELOPMENT
	printf("zone_init: submap[%d] %p:%p (%zuM)\n",
	    idx, (void *)submap_start, (void *)submap_end,
	    (size_t)submap_size >> 20);
#endif /* DEBUG || DEVELOPMENT */

	zone_submaps[idx] = submap;
	*submap_min       = submap_end;
	*remaining_size  -= submap_size;
	*remaining_denom -= zone_sub_map_numer;
}

/*
 * Allocate metadata array and migrate foreign initial metadata.
 *
 * So that foreign pages and native pages have the same scheme,
 * we allocate VA space that covers both foreign and native pages.
 */
__startup_func
static void
zone_metadata_init(void)
{
	struct zone_map_range r0 = zone_info.zi_map_range[0];
	struct zone_map_range r1 = zone_info.zi_map_range[1];
	struct zone_map_range mr, br;
	vm_size_t meta_size, bits_size, foreign_base;
	vm_offset_t hstart, hend;

	if (r0.min_address > r1.min_address) {
		r0 = zone_info.zi_map_range[1];
		r1 = zone_info.zi_map_range[0];
	}

	meta_size = round_page(atop(r1.max_address - r0.min_address) *
	    sizeof(struct zone_page_metadata)) + ZONE_GUARD_SIZE * 2;

	/*
	 * Allocations can't be smaller than 8 bytes, which is 128b / 16B per 1k
	 * of physical memory (16M per 1G).
	 *
	 * Let's preallocate for the worst to avoid weird panics.
	 */
	bits_size = round_page(16 * (ptoa(zone_pages_wired_max) >> 10));

	/*
	 * Compute the size of the "hole" in the middle of the range.
	 *
	 * If it is smaller than 256k, just leave it be, with this layout:
	 *
	 *   [G][ r0 meta ][ hole ][ r1 meta ][ bits ][G]
	 *
	 * else punch a hole with guard pages around the hole, and place the
	 * bits in the hole if it fits, or after r1 otherwise, yielding either
	 * of the following layouts:
	 *
	 *      |__________________hend____________|
	 *      |__hstart_|                        |
	 *   [G][ r0 meta ][ bits ][G]..........[G][ r1 meta ][G]
	 *   [G][ r0 meta ][G]..................[G][ r1 meta ][ bits ][G]
	 */
	hstart = round_page(atop(r0.max_address - r0.min_address) *
	    sizeof(struct zone_page_metadata));
	hend = trunc_page(atop(r1.min_address - r0.min_address) *
	    sizeof(struct zone_page_metadata));

	if (hstart >= hend || hend - hstart < (256ul << 10)) {
		mr = zone_kmem_suballoc(0, meta_size + bits_size,
		    VM_FLAGS_ANYWHERE | VM_FLAGS_RANDOM_ADDR,
		    VM_KERN_MEMORY_ZONE, &zone_meta_submaps[0]);
		mr.min_address += ZONE_GUARD_SIZE;
		mr.max_address -= ZONE_GUARD_SIZE;
		br.max_address  = mr.max_address;
		mr.max_address -= bits_size;
		br.min_address  = mr.max_address;

#if DEBUG || DEVELOPMENT
		printf("zone_init: metadata  %p:%p (%zuK)\n",
		    (void *)mr.min_address, (void *)mr.max_address,
		    (size_t)zone_range_size(&mr) >> 10);
		printf("zone_init: metabits  %p:%p (%zuK)\n",
		    (void *)br.min_address, (void *)br.max_address,
		    (size_t)zone_range_size(&br) >> 10);
#endif /* DEBUG || DEVELOPMENT */
	} else {
		vm_size_t size, alloc_size = meta_size;
		vm_offset_t base;
		bool bits_in_middle = true;

		if (hend - hstart - 2 * ZONE_GUARD_SIZE < bits_size) {
			alloc_size += bits_size;
			bits_in_middle = false;
		}

		mr = zone_init_allocate_va(0, alloc_size, true);

		base = mr.min_address;
		size = ZONE_GUARD_SIZE + hstart + ZONE_GUARD_SIZE;
		if (bits_in_middle) {
			size += bits_size;
			br.min_address = base + ZONE_GUARD_SIZE + hstart;
			br.max_address = br.min_address + bits_size;
		}
		zone_kmem_suballoc(base, size,
		    VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE,
		    VM_KERN_MEMORY_ZONE, &zone_meta_submaps[0]);

		base += size;
		size = mr.min_address + hend - base;
		kmem_free(kernel_map, base, size);

		base = mr.min_address + hend;
		size = mr.max_address - base;
		zone_kmem_suballoc(base, size,
		    VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE | VM_FLAGS_PERMANENT,
		    VM_KERN_MEMORY_ZONE, &zone_meta_submaps[1]);

		mr.min_address += ZONE_GUARD_SIZE;
		mr.max_address -= ZONE_GUARD_SIZE;
		if (!bits_in_middle) {
			br.max_address  = mr.max_address;
			mr.max_address -= bits_size;
			br.min_address  = mr.max_address;
		}

#if DEBUG || DEVELOPMENT
		printf("zone_init: metadata0 %p:%p (%zuK)\n",
		    (void *)mr.min_address, (void *)(mr.min_address + hstart),
		    (size_t)hstart >> 10);
		printf("zone_init: metadata1 %p:%p (%zuK)\n",
		    (void *)(mr.min_address + hend), (void *)mr.max_address,
		    (size_t)(zone_range_size(&mr) - hend) >> 10);
		printf("zone_init: metabits  %p:%p (%zuK)\n",
		    (void *)br.min_address, (void *)br.max_address,
		    (size_t)zone_range_size(&br) >> 10);
#endif /* DEBUG || DEVELOPMENT */
	}

	br.min_address = (br.min_address + ZBA_CHUNK_SIZE - 1) & -ZBA_CHUNK_SIZE;
	br.max_address = br.max_address & -ZBA_CHUNK_SIZE;

	zone_info.zi_meta_range = mr;
	zone_info.zi_bits_range = br;

	/*
	 * Migrate the original static metadata into its new location.
	 */
	struct zone_page_metadata *early_meta = zone_foreign_meta_array_startup;

	if (zone_early_steal.min_address) {
		early_meta = (void *)zone_early_steal.min_address;
	}

	zone_info.zi_meta_base = (struct zone_page_metadata *)mr.min_address -
	    zone_pva_from_addr(r0.min_address).packed_address;
	foreign_base = zone_info.zi_map_range[ZONE_ADDR_FOREIGN].min_address;
	zone_meta_populate(foreign_base, zone_foreign_size());
	memcpy(zone_meta_from_addr(foreign_base), early_meta,
	    atop(zone_foreign_size()) * sizeof(struct zone_page_metadata));

	if (zone_early_steal.min_address) {
		pmap_remove(kernel_pmap, zone_early_steal.min_address,
		    zone_early_steal.max_address);
	}

	zba_populate(0);
	memcpy(zba_base_header(), zba_chunk_startup,
	    sizeof(zba_chunk_startup));
}

/*
 * Global initialization of Zone Allocator.
 * Runs after zone_bootstrap.
 */
__startup_func
static void
zone_init(void)
{
	vm_size_t       zone_map_size;
	vm_size_t       remaining_size;
	vm_offset_t     submap_min = 0;
	uint64_t        denom = 0;
	uint32_t        submap_count = 0;
	uint16_t        submap_ratios[Z_SUBMAP_IDX_COUNT] = {
#if ZSECURITY_CONFIG(READ_ONLY)
		[Z_SUBMAP_IDX_VM]               = 15,
		[Z_SUBMAP_IDX_READ_ONLY]        =  5,
#else
		[Z_SUBMAP_IDX_VM]               = 20,
#endif /* !ZSECURITY_CONFIG(READ_ONLY) */
#if ZSECURITY_CONFIG(SUBMAP_USER_DATA) && ZSECURITY_CONFIG(SAD_FENG_SHUI)
		[Z_SUBMAP_IDX_GENERAL_0]        = 15,
		[Z_SUBMAP_IDX_GENERAL_1]        = 15,
		[Z_SUBMAP_IDX_GENERAL_2]        = 15,
		[Z_SUBMAP_IDX_GENERAL_3]        = 15,
		[Z_SUBMAP_IDX_DATA]             = 20,
#elif ZSECURITY_CONFIG(SUBMAP_USER_DATA)
		[Z_SUBMAP_IDX_GENERAL_0]        = 40,
		[Z_SUBMAP_IDX_DATA]             = 40,
#elif ZSECURITY_CONFIG(SAD_FENG_SHUI)
#error invalid configuration: SAD_FENG_SHUI requires SUBMAP_USER_DATA
#else
		[Z_SUBMAP_IDX_GENERAL_0]        = 80,
#endif /* ZSECURITY_CONFIG(SUBMAP_USER_DATA) && ZSECURITY_CONFIG(SAD_FENG_SHUI) */
	};

	zone_pages_wired_max = (uint32_t)atop(zone_phys_size_max());

	for (unsigned idx = 0; idx < Z_SUBMAP_IDX_COUNT; idx++) {
		denom += submap_ratios[idx];
		if (submap_ratios[idx] != 0) {
			submap_count++;
		}
	}

#if __LP64__
	zone_map_size = ZONE_MAP_VA_SIZE_LP64;
#else
	zone_map_size = ptoa(zone_pages_wired_max *
	    (denom + submap_ratios[Z_SUBMAP_IDX_VM]) / denom);
#endif

	/*
	 * And now allocate the various pieces of VA and submaps.
	 *
	 * Make a first allocation of contiguous VA, that we'll deallocate,
	 * and we'll carve-out memory in that range again linearly.
	 * The kernel is stil single threaded at this stage.
	 */

	struct zone_map_range *map_range =
	    &zone_info.zi_map_range[ZONE_ADDR_NATIVE];

	*map_range = zone_init_allocate_va(0, zone_map_size, false);
	submap_min = map_range->min_address;
	remaining_size = zone_map_size;

#if CONFIG_PROB_GZALLOC
	vm_size_t pgz_size = pgz_get_size();

	zone_info.zi_pgz_range = zone_kmem_suballoc(submap_min, pgz_size,
	    VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE,
	    VM_KERN_MEMORY_ZONE, &pgz_submap);

	submap_min     += pgz_size;
	remaining_size -= pgz_size;
#if DEBUG || DEVELOPMENT
	printf("zone_init: pgzalloc  %p:%p (%zuM) [%d slots]\n",
	    (void *)zone_info.zi_pgz_range.min_address,
	    (void *)zone_info.zi_pgz_range.max_address,
	    (size_t)pgz_size >> 20, pgz_slots);
#endif /* DEBUG || DEVELOPMENT */
#endif /* CONFIG_PROB_GZALLOC */

	/*
	 * Allocate the submaps
	 */
	for (zone_submap_idx_t idx = 0; idx < Z_SUBMAP_IDX_COUNT; idx++) {
		if (submap_ratios[idx] == 0) {
			zone_submaps[idx] = VM_MAP_NULL;
		} else {
			zone_submap_init(&submap_min, idx, submap_ratios[idx],
			    &denom, &remaining_size);
		}
	}

	assert(submap_min == map_range->max_address);

	/*
	 * needs to be done before zone_metadata_init() which occupies
	 * random space in the kernel, and the maps need a HUGE range.
	 */
	kalloc_init_maps(map_range->max_address);

	zone_metadata_init();

#if VM_TAG_SIZECLASSES
	if (zone_tagging_on) {
		zone_tagging_init(zone_map_size);
	}
#endif
#if CONFIG_GZALLOC
	gzalloc_init(zone_map_size);
#endif

	zone_create_flags_t kma_flags = ZC_NOCACHING |
	    ZC_NOGC | ZC_NOGZALLOC | ZC_NOCALLOUT |
	    ZC_KASAN_NOQUARANTINE | ZC_KASAN_NOREDZONE | ZC_VM_LP64;

	(void)zone_create_ext("vm.permanent", 1, kma_flags,
	    ZONE_ID_PERMANENT, ^(zone_t z) {
		z->z_permanent = true;
		z->z_elem_size = 1;
	});
	(void)zone_create_ext("vm.permanent.percpu", 1,
	    kma_flags | ZC_PERCPU, ZONE_ID_PERCPU_PERMANENT, ^(zone_t z) {
		z->z_permanent = true;
		z->z_elem_size = 1;
	});

	/*
	 * Now migrate the startup statistics into their final storage.
	 */
	int cpu = cpu_number();
	zone_index_foreach(idx) {
		zone_t tz = &zone_array[idx];

		if (tz->z_stats == __zpcpu_mangle_for_boot(&zone_stats_startup[idx])) {
			zone_stats_t zs = zalloc_percpu_permanent_type(struct zone_stats);

			*zpercpu_get_cpu(zs, cpu) = *zpercpu_get_cpu(tz->z_stats, cpu);
			tz->z_stats = zs;
		}
	}

#if VM_TAG_SIZECLASSES
	if (zone_tagging_on) {
		vm_allocation_zones_init();
	}
#endif
}
STARTUP(ZALLOC, STARTUP_RANK_FIRST, zone_init);

__startup_func
static void
zone_cache_bootstrap(void)
{
	zone_t magzone;

	magzone = zone_create("zcc_magazine_zone", sizeof(struct zone_magazine) +
	    zc_mag_size() * sizeof(zone_element_t),
	    ZC_NOGZALLOC | ZC_KASAN_NOREDZONE | ZC_KASAN_NOQUARANTINE |
	    ZC_SEQUESTER | ZC_CACHING | ZC_ZFREE_CLEARMEM | ZC_PGZ_USE_GUARDS);
	magzone->z_elems_rsv = (uint16_t)(2 * zpercpu_count());

	os_atomic_store(&zc_magazine_zone, magzone, compiler_acq_rel);

	/*
	 * Now that we are initialized, we can enable zone caching for zones that
	 * were made before zcache_bootstrap() was called.
	 *
	 * The system is still single threaded so we don't need to take the lock.
	 */
	zone_index_foreach(i) {
		zone_t z = &zone_array[i];
		if (z->z_pcpu_cache) {
			z->z_pcpu_cache = NULL;
			zone_enable_caching(z);
		}
#if ZONE_ENABLE_LOGGING
		if (!z->z_gzalloc_tracked && z->z_self == z) {
			zone_setup_logging(z);
		}
#endif /* ZONE_ENABLE_LOGGING */
	}
}
STARTUP(ZALLOC, STARTUP_RANK_MIDDLE, zone_cache_bootstrap);

void
zalloc_first_proc_made(void)
{
	zone_caching_disabled = 0;
}

__startup_func
vm_offset_t
zone_foreign_mem_init(vm_size_t size, bool allow_meta_steal)
{
	struct zone_page_metadata *base;
	vm_offset_t mem;

	if (atop(size) <= ZONE_FOREIGN_META_INLINE_COUNT) {
		base = zone_foreign_meta_array_startup;
	} else if (allow_meta_steal) {
		vm_size_t steal_size;

		printf("zinit: not enough early foreigh metadata "
		    "(%d > %d) stealing from the pmap\n",
		    (int)atop(size), ZONE_FOREIGN_META_INLINE_COUNT);
		steal_size = round_page(atop(size) * sizeof(*base));
		base = pmap_steal_memory(steal_size);
		zone_early_steal.min_address = (vm_offset_t)base;
		zone_early_steal.max_address = (vm_offset_t)base + steal_size;
	} else {
		panic("ZONE_FOREIGN_META_INLINE_COUNT has become too small: "
		    "%d > %d", (int)atop(size), ZONE_FOREIGN_META_INLINE_COUNT);
	}

	mem = (vm_offset_t)pmap_steal_memory(size);

	zone_info.zi_meta_base = base - zone_pva_from_addr(mem).packed_address;
	zone_info.zi_map_range[ZONE_ADDR_FOREIGN].min_address = mem;
	zone_info.zi_map_range[ZONE_ADDR_FOREIGN].max_address = mem + size;

	zone_info.zi_bits_range = (struct zone_map_range){
		.min_address = (vm_offset_t)zba_chunk_startup,
		.max_address = (vm_offset_t)zba_chunk_startup +
	    sizeof(zba_chunk_startup),
	};
	zba_init_chunk(0);

	return mem;
}

#endif /* !ZALLOC_TEST */
#pragma mark - tests
#if DEBUG || DEVELOPMENT

/*
 * Used for sysctl zone tests that aren't thread-safe. Ensure only one
 * thread goes through at a time.
 *
 * Or we can end up with multiple test zones (if a second zinit() comes through
 * before zdestroy()), which could lead us to run out of zones.
 */
static bool any_zone_test_running = FALSE;

static uintptr_t *
zone_copy_allocations(zone_t z, uintptr_t *elems, zone_pva_t page_index)
{
	vm_offset_t elem_size = zone_elem_size(z);
	vm_offset_t base;
	struct zone_page_metadata *meta;

	while (!zone_pva_is_null(page_index)) {
		base  = zone_pva_to_addr(page_index) + zone_oob_offs(z);
		meta  = zone_pva_to_meta(page_index);

		if (meta->zm_inline_bitmap) {
			for (size_t i = 0; i < meta->zm_chunk_len; i++) {
				uint32_t map = meta[i].zm_bitmap;

				for (; map; map &= map - 1) {
					*elems++ = INSTANCE_PUT(base +
					    elem_size * __builtin_clz(map));
				}
				base += elem_size * 32;
			}
		} else {
			uint32_t order = zba_bits_ref_order(meta->zm_bitmap);
			bitmap_t *bits = zba_bits_ref_ptr(meta->zm_bitmap);
			for (size_t i = 0; i < (1u << order); i++) {
				uint64_t map = bits[i];

				for (; map; map &= map - 1) {
					*elems++ = INSTANCE_PUT(base +
					    elem_size * __builtin_clzll(map));
				}
				base += elem_size * 64;
			}
		}

		page_index = meta->zm_page_next;
	}
	return elems;
}

kern_return_t
zone_leaks(const char * zoneName, uint32_t nameLen, leak_site_proc proc)
{
	zone_t        zone = NULL;
	uintptr_t *   array;
	uintptr_t *   next;
	uintptr_t     element;
	uint32_t      idx, count, found;
	uint32_t      nobtcount;
	uint32_t      elemSize;
	size_t        maxElems;
	kern_return_t kr;

	zone_foreach(z) {
		if (!strncmp(zoneName, z->z_name, nameLen)) {
			zone = z;
			break;
		}
	}
	if (zone == NULL) {
		return KERN_INVALID_NAME;
	}

	elemSize = (uint32_t)zone_elem_size(zone);
	maxElems = (zone->z_elems_avail + 1) & ~1ul;

	kr = kmem_alloc_kobject(kernel_map, (vm_offset_t *) &array,
	    maxElems * sizeof(uintptr_t), VM_KERN_MEMORY_DIAG);
	if (KERN_SUCCESS != kr) {
		return kr;
	}

	zone_lock(zone);

	next = array;
	next = zone_copy_allocations(zone, next, zone->z_pageq_partial);
	next = zone_copy_allocations(zone, next, zone->z_pageq_full);
	count = (uint32_t)(next - array);

	zone_unlock(zone);

	zone_leaks_scan(array, count, (uint32_t)zone_elem_size(zone), &found);
	assert(found <= count);

	for (idx = 0; idx < count; idx++) {
		element = array[idx];
		if (kInstanceFlagReferenced & element) {
			continue;
		}
		element = INSTANCE_PUT(element) & ~kInstanceFlags;
	}

#if ZONE_ENABLE_LOGGING
	if (zone->z_btlog && !corruption_debug_flag) {
		// btlog_copy_backtraces_for_elements will set kInstanceFlagReferenced on elements it found
		static_assert(sizeof(vm_address_t) == sizeof(uintptr_t));
		btlog_copy_backtraces_for_elements(zone->z_btlog,
		    (vm_address_t *)array, &count, elemSize, proc);
	}
#endif /* ZONE_ENABLE_LOGGING */

	for (nobtcount = idx = 0; idx < count; idx++) {
		element = array[idx];
		if (!element) {
			continue;
		}
		if (kInstanceFlagReferenced & element) {
			continue;
		}
		nobtcount++;
	}
	if (nobtcount) {
		proc(nobtcount, elemSize, BTREF_NULL);
	}

	kmem_free(kernel_map, (vm_offset_t) array, maxElems * sizeof(uintptr_t));

	return KERN_SUCCESS;
}

static int
zone_ro_basic_test_run(__unused int64_t in, int64_t *out)
{
	zone_security_flags_t zsflags;
	uint32_t x = 4;
	uint32_t *test_ptr;

	if (os_atomic_xchg(&any_zone_test_running, true, relaxed)) {
		printf("zone_ro_basic_test: Test already running.\n");
		return EALREADY;
	}

	zsflags = zone_security_array[ZONE_ID__FIRST_RO];

	for (int i = 0; i < 3; i++) {
#if ZSECURITY_CONFIG(READ_ONLY)
		/* Basic Test: Create int zone, zalloc int, modify value, free int */
		printf("zone_ro_basic_test: Basic Test iteration %d\n", i);
		printf("zone_ro_basic_test: create a sub-page size zone\n");

		printf("zone_ro_basic_test: verify flags were set\n");
		assert(zsflags.z_submap_idx == Z_SUBMAP_IDX_READ_ONLY);

		printf("zone_ro_basic_test: zalloc an element\n");
		test_ptr = (zalloc_ro)(ZONE_ID__FIRST_RO, Z_WAITOK);
		assert(test_ptr);

		printf("zone_ro_basic_test: verify elem in the right submap\n");
		zone_require_ro_range_contains(ZONE_ID__FIRST_RO, test_ptr);

		printf("zone_ro_basic_test: verify we can't write to it\n");
		assert(verify_write(&x, test_ptr, sizeof(x)) == EFAULT);

		x = 4;
		printf("zone_ro_basic_test: test zalloc_ro_mut to assign value\n");
		zalloc_ro_mut(ZONE_ID__FIRST_RO, test_ptr, 0, &x, sizeof(uint32_t));
		assert(test_ptr);
		assert(*(uint32_t*)test_ptr == x);

		x = 5;
		printf("zone_ro_basic_test: test zalloc_ro_update_elem to assign value\n");
		zalloc_ro_update_elem(ZONE_ID__FIRST_RO, test_ptr, &x);
		assert(test_ptr);
		assert(*(uint32_t*)test_ptr == x);

		printf("zone_ro_basic_test: verify we can't write to it after assigning value\n");
		assert(verify_write(&x, test_ptr, sizeof(x)) == EFAULT);

		printf("zone_ro_basic_test: free elem\n");
		zfree_ro(ZONE_ID__FIRST_RO, test_ptr);
		assert(!test_ptr);
#else
		printf("zone_ro_basic_test: Read-only allocator n/a on 32bit platforms, test functionality of API\n");

		printf("zone_ro_basic_test: verify flags were set\n");
		assert(zsflags.z_submap_idx != Z_SUBMAP_IDX_READ_ONLY);

		printf("zone_ro_basic_test: zalloc an element\n");
		test_ptr = (zalloc_ro)(ZONE_ID__FIRST_RO, Z_WAITOK);
		assert(test_ptr);

		x = 4;
		printf("zone_ro_basic_test: test zalloc_ro_mut to assign value\n");
		zalloc_ro_mut(ZONE_ID__FIRST_RO, test_ptr, 0, &x, sizeof(uint32_t));
		assert(test_ptr);
		assert(*(uint32_t*)test_ptr == x);

		x = 5;
		printf("zone_ro_basic_test: test zalloc_ro_update_elem to assign value\n");
		zalloc_ro_update_elem(ZONE_ID__FIRST_RO, test_ptr, &x);
		assert(test_ptr);
		assert(*(uint32_t*)test_ptr == x);

		printf("zone_ro_basic_test: free elem\n");
		zfree_ro(ZONE_ID__FIRST_RO, test_ptr);
		assert(!test_ptr);
#endif /* !ZSECURITY_CONFIG(READ_ONLY) */
	}

	printf("zone_ro_basic_test: garbage collection\n");
	zone_gc(ZONE_GC_DRAIN);

	printf("zone_ro_basic_test: Test passed\n");

	*out = 1;
	os_atomic_store(&any_zone_test_running, false, relaxed);
	return 0;
}
SYSCTL_TEST_REGISTER(zone_ro_basic_test, zone_ro_basic_test_run);

static int
zone_basic_test_run(__unused int64_t in, int64_t *out)
{
	static zone_t test_zone_ptr = NULL;

	unsigned int i = 0, max_iter = 5;
	void * test_ptr;
	zone_t test_zone;
	int rc = 0;

	if (os_atomic_xchg(&any_zone_test_running, true, relaxed)) {
		printf("zone_basic_test: Test already running.\n");
		return EALREADY;
	}

	printf("zone_basic_test: Testing zinit(), zalloc(), zfree() and zdestroy() on zone \"test_zone_sysctl\"\n");

	/* zinit() and zdestroy() a zone with the same name a bunch of times, verify that we get back the same zone each time */
	do {
		test_zone = zinit(sizeof(uint64_t), 100 * sizeof(uint64_t), sizeof(uint64_t), "test_zone_sysctl");
		assert(test_zone);

#if KASAN_ZALLOC
		if (test_zone_ptr == NULL && test_zone->z_elems_free != 0)
#else
		if (test_zone->z_elems_free != 0)
#endif
		{
			printf("zone_basic_test: free count is not zero\n");
			rc = EIO;
			goto out;
		}

		if (test_zone_ptr == NULL) {
			/* Stash the zone pointer returned on the fist zinit */
			printf("zone_basic_test: zone created for the first time\n");
			test_zone_ptr = test_zone;
		} else if (test_zone != test_zone_ptr) {
			printf("zone_basic_test: old zone pointer and new zone pointer don't match\n");
			rc = EIO;
			goto out;
		}

		test_ptr = zalloc_flags(test_zone, Z_WAITOK | Z_NOFAIL);
		zfree(test_zone, test_ptr);

		zdestroy(test_zone);
		i++;

		printf("zone_basic_test: Iteration %d successful\n", i);
	} while (i < max_iter);

	/* test Z_VA_SEQUESTER */
#if ZSECURITY_CONFIG(SEQUESTER)
	{
		zone_t test_pcpu_zone;
		kern_return_t kr;
		int idx, num_allocs = 8;
		vm_size_t elem_size = 2 * PAGE_SIZE / num_allocs;
		void *allocs[num_allocs];
		void **allocs_pcpu;
		vm_offset_t phys_pages = os_atomic_load(&zone_pages_wired, relaxed);

		test_zone = zone_create("test_zone_sysctl", elem_size,
		    ZC_DESTRUCTIBLE);
		assert(test_zone);
		assert(zone_security_config(test_zone).z_va_sequester);

		test_pcpu_zone = zone_create("test_zone_sysctl.pcpu", sizeof(uint64_t),
		    ZC_DESTRUCTIBLE | ZC_PERCPU);
		assert(test_pcpu_zone);
		assert(zone_security_config(test_pcpu_zone).z_va_sequester);

		for (idx = 0; idx < num_allocs; idx++) {
			allocs[idx] = zalloc(test_zone);
			assert(NULL != allocs[idx]);
			printf("alloc[%d] %p\n", idx, allocs[idx]);
		}
		for (idx = 0; idx < num_allocs; idx++) {
			zfree(test_zone, allocs[idx]);
		}
		assert(!zone_pva_is_null(test_zone->z_pageq_empty));

		kr = kernel_memory_allocate(kernel_map,
		    (vm_address_t *)&allocs_pcpu, PAGE_SIZE,
		    0, KMA_ZERO | KMA_KOBJECT, VM_KERN_MEMORY_DIAG);
		assert(kr == KERN_SUCCESS);

		for (idx = 0; idx < PAGE_SIZE / sizeof(uint64_t); idx++) {
			allocs_pcpu[idx] = zalloc_percpu(test_pcpu_zone,
			    Z_WAITOK | Z_ZERO);
			assert(NULL != allocs_pcpu[idx]);
		}
		for (idx = 0; idx < PAGE_SIZE / sizeof(uint64_t); idx++) {
			zfree_percpu(test_pcpu_zone, allocs_pcpu[idx]);
		}
		assert(!zone_pva_is_null(test_pcpu_zone->z_pageq_empty));

		printf("vm_page_wire_count %d, vm_page_free_count %d, p to v %ld%%\n",
		    vm_page_wire_count, vm_page_free_count,
		    100L * phys_pages / zone_pages_wired_max);
		zone_gc(ZONE_GC_DRAIN);
		printf("vm_page_wire_count %d, vm_page_free_count %d, p to v %ld%%\n",
		    vm_page_wire_count, vm_page_free_count,
		    100L * phys_pages / zone_pages_wired_max);

		unsigned int allva = 0;

		zone_foreach(z) {
			zone_lock(z);
			allva += z->z_wired_cur;
			if (zone_pva_is_null(z->z_pageq_va)) {
				zone_unlock(z);
				continue;
			}
			unsigned count = 0;
			uint64_t size;
			zone_pva_t pg = z->z_pageq_va;
			struct zone_page_metadata *page_meta;
			while (pg.packed_address) {
				page_meta = zone_pva_to_meta(pg);
				count += z->z_percpu ? 1 : z->z_chunk_pages;
				if (page_meta->zm_chunk_len == ZM_SECONDARY_PAGE) {
					count -= page_meta->zm_page_index;
				}
				pg = page_meta->zm_page_next;
			}
			size = zone_size_wired(z);
			if (!size) {
				size = 1;
			}
			printf("%s%s: seq %d, res %d, %qd %%\n",
			    zone_heap_name(z), z->z_name, z->z_va_cur - z->z_wired_cur,
			    z->z_wired_cur, zone_size_allocated(z) * 100ULL / size);
			zone_unlock(z);
		}

		printf("total va: %d\n", allva);

		assert(zone_pva_is_null(test_zone->z_pageq_empty));
		assert(zone_pva_is_null(test_zone->z_pageq_partial));
		assert(!zone_pva_is_null(test_zone->z_pageq_va));
		assert(zone_pva_is_null(test_pcpu_zone->z_pageq_empty));
		assert(zone_pva_is_null(test_pcpu_zone->z_pageq_partial));
		assert(!zone_pva_is_null(test_pcpu_zone->z_pageq_va));

		for (idx = 0; idx < num_allocs; idx++) {
			assert(0 == pmap_find_phys(kernel_pmap, (addr64_t)(uintptr_t) allocs[idx]));
		}

		/* make sure the zone is still usable after a GC */

		for (idx = 0; idx < num_allocs; idx++) {
			allocs[idx] = zalloc(test_zone);
			assert(allocs[idx]);
			printf("alloc[%d] %p\n", idx, allocs[idx]);
		}
		for (idx = 0; idx < num_allocs; idx++) {
			zfree(test_zone, allocs[idx]);
		}

		for (idx = 0; idx < PAGE_SIZE / sizeof(uint64_t); idx++) {
			allocs_pcpu[idx] = zalloc_percpu(test_pcpu_zone,
			    Z_WAITOK | Z_ZERO);
			assert(NULL != allocs_pcpu[idx]);
		}
		for (idx = 0; idx < PAGE_SIZE / sizeof(uint64_t); idx++) {
			zfree_percpu(test_pcpu_zone, allocs_pcpu[idx]);
		}

		assert(!zone_pva_is_null(test_pcpu_zone->z_pageq_empty));

		kmem_free(kernel_map, (vm_address_t)allocs_pcpu, PAGE_SIZE);

		zdestroy(test_zone);
		zdestroy(test_pcpu_zone);
	}
#else
	printf("zone_basic_test: skipping sequester test (not enabled)\n");
#endif /* ZSECURITY_CONFIG(SEQUESTER) */

	printf("zone_basic_test: Test passed\n");


	*out = 1;
out:
	os_atomic_store(&any_zone_test_running, false, relaxed);
	return rc;
}
SYSCTL_TEST_REGISTER(zone_basic_test, zone_basic_test_run);

struct zone_stress_obj {
	TAILQ_ENTRY(zone_stress_obj) zso_link;
};

struct zone_stress_ctx {
	thread_t  zsc_leader;
	lck_mtx_t zsc_lock;
	zone_t    zsc_zone;
	uint64_t  zsc_end;
	uint32_t  zsc_workers;
};

static void
zone_stress_worker(void *arg, wait_result_t __unused wr)
{
	struct zone_stress_ctx *ctx = arg;
	bool leader = ctx->zsc_leader == current_thread();
	TAILQ_HEAD(zone_stress_head, zone_stress_obj) head = TAILQ_HEAD_INITIALIZER(head);
	struct zone_bool_gen bg = { };
	struct zone_stress_obj *obj;
	uint32_t allocs = 0;

	random_bool_init(&bg.zbg_bg);

	do {
		for (int i = 0; i < 2000; i++) {
			uint32_t what = random_bool_gen_bits(&bg.zbg_bg,
			    bg.zbg_entropy, ZONE_ENTROPY_CNT, 1);
			switch (what) {
			case 0:
			case 1:
				if (allocs < 10000) {
					obj = zalloc(ctx->zsc_zone);
					TAILQ_INSERT_HEAD(&head, obj, zso_link);
					allocs++;
				}
				break;
			case 2:
			case 3:
				if (allocs < 10000) {
					obj = zalloc(ctx->zsc_zone);
					TAILQ_INSERT_TAIL(&head, obj, zso_link);
					allocs++;
				}
				break;
			case 4:
				if (leader) {
					zone_gc(ZONE_GC_DRAIN);
				}
				break;
			case 5:
			case 6:
				if (!TAILQ_EMPTY(&head)) {
					obj = TAILQ_FIRST(&head);
					TAILQ_REMOVE(&head, obj, zso_link);
					zfree(ctx->zsc_zone, obj);
					allocs--;
				}
				break;
			case 7:
				if (!TAILQ_EMPTY(&head)) {
					obj = TAILQ_LAST(&head, zone_stress_head);
					TAILQ_REMOVE(&head, obj, zso_link);
					zfree(ctx->zsc_zone, obj);
					allocs--;
				}
				break;
			}
		}
	} while (mach_absolute_time() < ctx->zsc_end);

	while (!TAILQ_EMPTY(&head)) {
		obj = TAILQ_FIRST(&head);
		TAILQ_REMOVE(&head, obj, zso_link);
		zfree(ctx->zsc_zone, obj);
	}

	lck_mtx_lock(&ctx->zsc_lock);
	if (--ctx->zsc_workers == 0) {
		thread_wakeup(ctx);
	} else if (leader) {
		while (ctx->zsc_workers) {
			lck_mtx_sleep(&ctx->zsc_lock, LCK_SLEEP_DEFAULT, ctx,
			    THREAD_UNINT);
		}
	}
	lck_mtx_unlock(&ctx->zsc_lock);

	if (!leader) {
		thread_terminate_self();
		__builtin_unreachable();
	}
}

static int
zone_stress_test_run(__unused int64_t in, int64_t *out)
{
	struct zone_stress_ctx ctx = {
		.zsc_leader  = current_thread(),
		.zsc_workers = 3,
	};
	kern_return_t kr;
	thread_t th;

	if (os_atomic_xchg(&any_zone_test_running, true, relaxed)) {
		printf("zone_stress_test: Test already running.\n");
		return EALREADY;
	}

	lck_mtx_init(&ctx.zsc_lock, &zone_locks_grp, LCK_ATTR_NULL);
	ctx.zsc_zone = zone_create("test_zone_344", 344,
	    ZC_DESTRUCTIBLE | ZC_NOCACHING);
	assert(ctx.zsc_zone->z_chunk_pages > 1);

	clock_interval_to_deadline(5, NSEC_PER_SEC, &ctx.zsc_end);

	printf("zone_stress_test: Starting (leader %p)\n", current_thread());

	os_atomic_inc(&zalloc_simulate_vm_pressure, relaxed);

	for (uint32_t i = 1; i < ctx.zsc_workers; i++) {
		kr = kernel_thread_start_priority(zone_stress_worker, &ctx,
		    BASEPRI_DEFAULT, &th);
		if (kr == KERN_SUCCESS) {
			printf("zone_stress_test: thread %d: %p\n", i, th);
			thread_deallocate(th);
		} else {
			ctx.zsc_workers--;
		}
	}

	zone_stress_worker(&ctx, 0);

	lck_mtx_destroy(&ctx.zsc_lock, &zone_locks_grp);

	zdestroy(ctx.zsc_zone);

	printf("zone_stress_test: Done\n");

	*out = 1;
	os_atomic_dec(&zalloc_simulate_vm_pressure, relaxed);
	os_atomic_store(&any_zone_test_running, false, relaxed);
	return 0;
}
SYSCTL_TEST_REGISTER(zone_stress_test, zone_stress_test_run);

/*
 * Routines to test that zone garbage collection and zone replenish threads
 * running at the same time don't cause problems.
 */

static int
zone_gc_replenish_test(__unused int64_t in, int64_t *out)
{
	zone_gc(ZONE_GC_DRAIN);
	*out = 1;
	return 0;
}
SYSCTL_TEST_REGISTER(zone_gc_replenish_test, zone_gc_replenish_test);

static int
zone_alloc_replenish_test(__unused int64_t in, int64_t *out)
{
	zone_t z = vm_map_entry_zone;
	struct data { struct data *next; } *node, *list = NULL;

	if (z == NULL) {
		printf("Couldn't find a replenish zone\n");
		return EIO;
	}

	/* big enough to go past replenishment */
	for (uint32_t i = 0; i < 10 * z->z_elems_rsv; ++i) {
		node = zalloc(z);
		node->next = list;
		list = node;
	}

	/*
	 * release the memory we allocated
	 */
	while (list != NULL) {
		node = list;
		list = list->next;
		zfree(z, node);
	}

	*out = 1;
	return 0;
}
SYSCTL_TEST_REGISTER(zone_alloc_replenish_test, zone_alloc_replenish_test);

#endif /* DEBUG || DEVELOPMENT */
