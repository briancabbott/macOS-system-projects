/*
 * Copyright (c) 2013-2020 Apple Inc. All rights reserved.
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

#include <mach/mach_types.h>
#include <mach/vm_param.h>
#include <mach/mach_vm.h>
#include <mach/clock_types.h>
#include <sys/errno.h>
#include <sys/stackshot.h>
#ifdef IMPORTANCE_INHERITANCE
#include <ipc/ipc_importance.h>
#endif
#include <sys/appleapiopts.h>
#include <kern/debug.h>
#include <kern/block_hint.h>
#include <uuid/uuid.h>

#include <kdp/kdp_dyld.h>
#include <kdp/kdp_en_debugger.h>

#include <libsa/types.h>
#include <libkern/version.h>
#include <libkern/section_keywords.h>

#include <string.h> /* bcopy */

#include <kern/backtrace.h>
#include <kern/coalition.h>
#include <kern/processor.h>
#include <kern/host_statistics.h>
#include <kern/counter.h>
#include <kern/thread.h>
#include <kern/thread_group.h>
#include <kern/task.h>
#include <kern/telemetry.h>
#include <kern/clock.h>
#include <kern/policy_internal.h>
#include <kern/socd_client.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_pageout.h>
#include <vm/vm_fault.h>
#include <vm/vm_shared_region.h>
#include <vm/vm_compressor.h>
#include <libkern/OSKextLibPrivate.h>
#include <os/log.h>

#if defined(__x86_64__)
#include <i386/mp.h>
#include <i386/cpu_threads.h>
#endif

#include <pexpert/pexpert.h>

#if MONOTONIC
#include <kern/monotonic.h>
#endif /* MONOTONIC */

#include <san/kasan.h>

#if DEBUG || DEVELOPMENT
# define STACKSHOT_COLLECTS_LATENCY_INFO 1
#else
# define STACKSHOT_COLLECTS_LATENCY_INFO 0
#endif /* DEBUG || DEVELOPMENT */

extern unsigned int not_in_kdp;

/* indicate to the compiler that some accesses are unaligned */
typedef uint64_t unaligned_u64 __attribute__((aligned(1)));

extern addr64_t kdp_vtophys(pmap_t pmap, addr64_t va);

int kdp_snapshot                            = 0;
static kern_return_t stack_snapshot_ret     = 0;
static uint32_t stack_snapshot_bytes_traced = 0;
static uint32_t stack_snapshot_bytes_uncompressed  = 0;

#if STACKSHOT_COLLECTS_LATENCY_INFO
static bool collect_latency_info = true;
#endif
static kcdata_descriptor_t stackshot_kcdata_p = NULL;
static void *stack_snapshot_buf;
static uint32_t stack_snapshot_bufsize;
int stack_snapshot_pid;
static uint64_t stack_snapshot_flags;
static uint64_t stack_snapshot_delta_since_timestamp;
static uint32_t stack_snapshot_pagetable_mask;
static boolean_t panic_stackshot;

static boolean_t stack_enable_faulting = FALSE;
static struct stackshot_fault_stats fault_stats;

/*
 * Experimentally, our current estimates are 20% short 96% of the time; 40 gets
 * us into 99.9%+ territory.  In the longer run, we need to make stackshot
 * estimates use a better approach (rdar://78880038); this is intended to be a
 * short-term fix.
 */
uint32_t stackshot_estimate_adj = 40; /* experiment factor: 0-100, adjust our estimate up by this amount */

static uint32_t stackshot_initial_estimate;
static uint32_t stackshot_initial_estimate_adj;
static uint64_t stackshot_duration_prior_abs;   /* prior attempts, abs */
static unaligned_u64 * stackshot_duration_outer;
static uint64_t stackshot_microsecs;

void * kernel_stackshot_buf   = NULL; /* Pointer to buffer for stackshots triggered from the kernel and retrieved later */
int kernel_stackshot_buf_size = 0;

void * stackshot_snapbuf = NULL; /* Used by stack_snapshot2 (to be removed) */

__private_extern__ void stackshot_init( void );
static boolean_t memory_iszero(void *addr, size_t size);
uint32_t                get_stackshot_estsize(uint32_t prev_size_hint, uint32_t adj);
kern_return_t           kern_stack_snapshot_internal(int stackshot_config_version, void *stackshot_config,
    size_t stackshot_config_size, boolean_t stackshot_from_user);
kern_return_t           do_stackshot(void *);
void                    kdp_snapshot_preflight(int pid, void * tracebuf, uint32_t tracebuf_size, uint64_t flags, kcdata_descriptor_t data_p, uint64_t since_timestamp, uint32_t pagetable_mask);
boolean_t               stackshot_thread_is_idle_worker_unsafe(thread_t thread);
static int              kdp_stackshot_kcdata_format(int pid, uint64_t trace_flags, uint32_t *pBytesTraced, uint32_t *pBytesUncompressed);
uint32_t                kdp_stack_snapshot_bytes_traced(void);
uint32_t                kdp_stack_snapshot_bytes_uncompressed(void);
static void             kdp_mem_and_io_snapshot(struct mem_and_io_snapshot *memio_snap);
static boolean_t        kdp_copyin(vm_map_t map, uint64_t uaddr, void *dest, size_t size, boolean_t try_fault, uint32_t *kdp_fault_result);
static int              kdp_copyin_string(task_t task, uint64_t addr, char *buf, int buf_sz, boolean_t try_fault, uint32_t *kdp_fault_results);
static boolean_t        kdp_copyin_word(task_t task, uint64_t addr, uint64_t *result, boolean_t try_fault, uint32_t *kdp_fault_results);
static uint64_t         proc_was_throttled_from_task(task_t task);
static void             stackshot_thread_wait_owner_info(thread_t thread, thread_waitinfo_v2_t * waitinfo);
static int              stackshot_thread_has_valid_waitinfo(thread_t thread);
static void             stackshot_thread_turnstileinfo(thread_t thread, thread_turnstileinfo_v2_t *tsinfo);
static int              stackshot_thread_has_valid_turnstileinfo(thread_t thread);

#if CONFIG_COALITIONS
static void             stackshot_coalition_jetsam_count(void *arg, int i, coalition_t coal);
static void             stackshot_coalition_jetsam_snapshot(void *arg, int i, coalition_t coal);
#endif /* CONFIG_COALITIONS */

#if CONFIG_THREAD_GROUPS
static void             stackshot_thread_group_count(void *arg, int i, struct thread_group *tg);
static void             stackshot_thread_group_snapshot(void *arg, int i, struct thread_group *tg);
#endif /* CONFIG_THREAD_GROUPS */

extern uint32_t         workqueue_get_pwq_state_kdp(void *proc);

struct proc;
extern int              proc_pid(struct proc *p);
extern uint64_t         proc_uniqueid(void *p);
extern uint64_t         proc_was_throttled(void *p);
extern uint64_t         proc_did_throttle(void *p);
extern int              proc_exiting(void *p);
extern int              proc_in_teardown(void *p);
static uint64_t         proc_did_throttle_from_task(task_t task);
extern void             proc_name_kdp(struct proc *p, char * buf, int size);
extern int              proc_threadname_kdp(void * uth, char * buf, size_t size);
extern void             proc_starttime_kdp(void * p, uint64_t * tv_sec, uint64_t * tv_usec, uint64_t * abstime);
extern void             proc_archinfo_kdp(void* p, cpu_type_t* cputype, cpu_subtype_t* cpusubtype);
extern boolean_t        proc_binary_uuid_kdp(task_t task, uuid_t uuid);
extern int              memorystatus_get_pressure_status_kdp(void);
extern void             memorystatus_proc_flags_unsafe(void * v, boolean_t *is_dirty, boolean_t *is_dirty_tracked, boolean_t *allow_idle_exit);

extern int count_busy_buffers(void); /* must track with declaration in bsd/sys/buf_internal.h */
extern void bcopy_phys(addr64_t, addr64_t, vm_size_t);

#if CONFIG_TELEMETRY
extern kern_return_t stack_microstackshot(user_addr_t tracebuf, uint32_t tracebuf_size, uint32_t flags, int32_t *retval);
#endif /* CONFIG_TELEMETRY */

extern kern_return_t kern_stack_snapshot_with_reason(char* reason);
extern kern_return_t kern_stack_snapshot_internal(int stackshot_config_version, void *stackshot_config, size_t stackshot_config_size, boolean_t stackshot_from_user);

static size_t _stackshot_get_page_size(vm_map_t, size_t *page_mask_out);
static size_t stackshot_plh_est_size(void);

/*
 * Validates that the given address for a word is both a valid page and has
 * default caching attributes for the current map.
 */
bool machine_trace_thread_validate_kva(vm_offset_t);
/*
 * Validates a region that stackshot will potentially inspect.
 */
static bool _stackshot_validate_kva(vm_offset_t, size_t);
/*
 * Must be called whenever stackshot is re-driven.
 */
static void _stackshot_validation_reset(void);

#define KDP_FAULT_RESULT_PAGED_OUT   0x1 /* some data was unable to be retrieved */
#define KDP_FAULT_RESULT_TRIED_FAULT 0x2 /* tried to fault in data */
#define KDP_FAULT_RESULT_FAULTED_IN  0x4 /* successfully faulted in data */

/*
 * Looks up the physical translation for the given address in the target map, attempting
 * to fault data in if requested and it is not resident. Populates thread_trace_flags if requested
 * as well.
 */
vm_offset_t kdp_find_phys(vm_map_t map, vm_offset_t target_addr, boolean_t try_fault, uint32_t *kdp_fault_results);

static long _stackshot_strlen(const char *s, size_t maxlen);    /* -1 if no \0 before fault or maxlen */
static size_t _stackshot_strlcpy(char *dst, const char *src, size_t maxlen);
void stackshot_memcpy(void *dst, const void *src, size_t len);

#define MAX_FRAMES 1000
#define MAX_LOADINFOS 500
#define TASK_IMP_WALK_LIMIT 20

typedef struct thread_snapshot *thread_snapshot_t;
typedef struct task_snapshot *task_snapshot_t;

#if CONFIG_KDP_INTERACTIVE_DEBUGGING
extern kdp_send_t    kdp_en_send_pkt;
#endif

/*
 * Stackshot locking and other defines.
 */
static LCK_GRP_DECLARE(stackshot_subsys_lck_grp, "stackshot_subsys_lock");
static LCK_MTX_DECLARE(stackshot_subsys_mutex, &stackshot_subsys_lck_grp);

#define STACKSHOT_SUBSYS_LOCK() lck_mtx_lock(&stackshot_subsys_mutex)
#define STACKSHOT_SUBSYS_TRY_LOCK() lck_mtx_try_lock(&stackshot_subsys_mutex)
#define STACKSHOT_SUBSYS_UNLOCK() lck_mtx_unlock(&stackshot_subsys_mutex)

#define SANE_BOOTPROFILE_TRACEBUF_SIZE (64ULL * 1024ULL * 1024ULL)
#define SANE_TRACEBUF_SIZE (8ULL * 1024ULL * 1024ULL)

#define TRACEBUF_SIZE_PER_GB (1024ULL * 1024ULL)
#define GIGABYTES (1024ULL * 1024ULL * 1024ULL)

SECURITY_READ_ONLY_LATE(static uint32_t) max_tracebuf_size = SANE_TRACEBUF_SIZE;

/*
 * We currently set a ceiling of 3 milliseconds spent in the kdp fault path
 * for non-panic stackshots where faulting is requested.
 */
#define KDP_FAULT_PATH_MAX_TIME_PER_STACKSHOT_NSECS (3 * NSEC_PER_MSEC)

#define STACKSHOT_SUPP_SIZE (16 * 1024) /* Minimum stackshot size */
#define TASK_UUID_AVG_SIZE (16 * sizeof(uuid_t)) /* Average space consumed by UUIDs/task */

#ifndef ROUNDUP
#define ROUNDUP(x, y)            ((((x)+(y)-1)/(y))*(y))
#endif

#define STACKSHOT_QUEUE_LABEL_MAXSIZE  64

/*
 * Initialize the mutex governing access to the stack snapshot subsystem
 * and other stackshot related bits.
 */
__private_extern__ void
stackshot_init( void )
{
	mach_timebase_info_data_t timebase;

	clock_timebase_info(&timebase);
	fault_stats.sfs_system_max_fault_time = ((KDP_FAULT_PATH_MAX_TIME_PER_STACKSHOT_NSECS * timebase.denom) / timebase.numer);

	max_tracebuf_size = MAX(max_tracebuf_size, ((ROUNDUP(max_mem, GIGABYTES) / GIGABYTES) * TRACEBUF_SIZE_PER_GB));

	PE_parse_boot_argn("stackshot_maxsz", &max_tracebuf_size, sizeof(max_tracebuf_size));
}

/*
 * Method for grabbing timer values safely, in the sense that no infinite loop will occur
 * Certain flavors of the timer_grab function, which would seem to be the thing to use,
 * can loop infinitely if called while the timer is in the process of being updated.
 * Unfortunately, it is (rarely) possible to get inconsistent top and bottom halves of
 * the timer using this method. This seems insoluble, since stackshot runs in a context
 * where the timer might be half-updated, and has no way of yielding control just long
 * enough to finish the update.
 */

static uint64_t
safe_grab_timer_value(struct timer *t)
{
#if   defined(__LP64__)
	return t->all_bits;
#else
	uint64_t time = t->high_bits; /* endian independent grab */
	time = (time << 32) | t->low_bits;
	return time;
#endif
}

/*
 * Called with interrupts disabled after stackshot context has been
 * initialized. Updates stack_snapshot_ret.
 */
static kern_return_t
stackshot_trap()
{
	kern_return_t   rv;

#if defined(__x86_64__)
	/*
	 * Since mp_rendezvous and stackshot both attempt to capture cpus then perform an
	 * operation, it's essential to apply mutual exclusion to the other when one
	 * mechanism is in operation, lest there be a deadlock as the mechanisms race to
	 * capture CPUs.
	 *
	 * Further, we assert that invoking stackshot from mp_rendezvous*() is not
	 * allowed, so we check to ensure there there is no rendezvous in progress before
	 * trying to grab the lock (if there is, a deadlock will occur when we try to
	 * grab the lock).  This is accomplished by setting cpu_rendezvous_in_progress to
	 * TRUE in the mp rendezvous action function.  If stackshot_trap() is called by
	 * a subordinate of the call chain within the mp rendezvous action, this flag will
	 * be set and can be used to detect the inevitable deadlock that would occur
	 * if this thread tried to grab the rendezvous lock.
	 */

	if (current_cpu_datap()->cpu_rendezvous_in_progress == TRUE) {
		panic("Calling stackshot from a rendezvous is not allowed!");
	}

	mp_rendezvous_lock();
#endif

	rv = DebuggerTrapWithState(DBOP_STACKSHOT, NULL, NULL, NULL, 0, NULL, FALSE, 0);

#if defined(__x86_64__)
	mp_rendezvous_unlock();
#endif
	return rv;
}


kern_return_t
stack_snapshot_from_kernel(int pid, void *buf, uint32_t size, uint64_t flags, uint64_t delta_since_timestamp, uint32_t pagetable_mask, unsigned *bytes_traced)
{
	kern_return_t error = KERN_SUCCESS;
	boolean_t istate;

#if DEVELOPMENT || DEBUG
	if (kern_feature_override(KF_STACKSHOT_OVRD) == TRUE) {
		error = KERN_NOT_SUPPORTED;
		goto out;
	}
#endif
	if ((buf == NULL) || (size <= 0) || (bytes_traced == NULL)) {
		return KERN_INVALID_ARGUMENT;
	}

	/* cap in individual stackshot to max_tracebuf_size */
	if (size > max_tracebuf_size) {
		size = max_tracebuf_size;
	}

	/* Serialize tracing */
	if (flags & STACKSHOT_TRYLOCK) {
		if (!STACKSHOT_SUBSYS_TRY_LOCK()) {
			return KERN_LOCK_OWNED;
		}
	} else {
		STACKSHOT_SUBSYS_LOCK();
	}

	struct kcdata_descriptor kcdata;
	uint32_t hdr_tag = (flags & STACKSHOT_COLLECT_DELTA_SNAPSHOT) ?
	    KCDATA_BUFFER_BEGIN_DELTA_STACKSHOT : KCDATA_BUFFER_BEGIN_STACKSHOT;

	error = kcdata_memory_static_init(&kcdata, (mach_vm_address_t)buf, hdr_tag, size,
	    KCFLAG_USE_MEMCOPY | KCFLAG_NO_AUTO_ENDBUFFER);
	if (error) {
		goto out;
	}

	stackshot_initial_estimate = 0;
	stackshot_duration_prior_abs = 0;
	stackshot_duration_outer = NULL;

	KDBG_RELEASE(MACHDBG_CODE(DBG_MACH_STACKSHOT, STACKSHOT_KERN_RECORD) | DBG_FUNC_START,
	    flags, size, pid, delta_since_timestamp);

	istate = ml_set_interrupts_enabled(FALSE);
	uint64_t time_start      = mach_absolute_time();

	/* Emit a SOCD tracepoint that we are initiating a stackshot */
	SOCD_TRACE_XNU_START(STACKSHOT);

	/* Preload trace parameters*/
	kdp_snapshot_preflight(pid, buf, size, flags, &kcdata,
	    delta_since_timestamp, pagetable_mask);

	/*
	 * Trap to the debugger to obtain a coherent stack snapshot; this populates
	 * the trace buffer
	 */
	error = stackshot_trap();

	uint64_t time_end               = mach_absolute_time();

	/* Emit a SOCD tracepoint that we have completed the stackshot */
	SOCD_TRACE_XNU_END(STACKSHOT);

	ml_set_interrupts_enabled(istate);

	if (stackshot_duration_outer) {
		*stackshot_duration_outer = time_end - time_start;
	}
	*bytes_traced = kdp_stack_snapshot_bytes_traced();

	KDBG_RELEASE(MACHDBG_CODE(DBG_MACH_STACKSHOT, STACKSHOT_KERN_RECORD) | DBG_FUNC_END,
	    error, (time_end - time_start), size, *bytes_traced);
out:
	stackshot_kcdata_p = NULL;
	STACKSHOT_SUBSYS_UNLOCK();
	return error;
}

#if CONFIG_TELEMETRY
kern_return_t
stack_microstackshot(user_addr_t tracebuf, uint32_t tracebuf_size, uint32_t flags, int32_t *retval)
{
	int error = KERN_SUCCESS;
	uint32_t bytes_traced = 0;

	*retval = -1;

	/*
	 * Control related operations
	 */
	if (flags & STACKSHOT_GLOBAL_MICROSTACKSHOT_ENABLE) {
		telemetry_global_ctl(1);
		*retval = 0;
		goto exit;
	} else if (flags & STACKSHOT_GLOBAL_MICROSTACKSHOT_DISABLE) {
		telemetry_global_ctl(0);
		*retval = 0;
		goto exit;
	}

	/*
	 * Data related operations
	 */
	*retval = -1;

	if ((((void*)tracebuf) == NULL) || (tracebuf_size == 0)) {
		error = KERN_INVALID_ARGUMENT;
		goto exit;
	}

	STACKSHOT_SUBSYS_LOCK();

	if (flags & STACKSHOT_GET_MICROSTACKSHOT) {
		if (tracebuf_size > max_tracebuf_size) {
			error = KERN_INVALID_ARGUMENT;
			goto unlock_exit;
		}

		bytes_traced = tracebuf_size;
		error = telemetry_gather(tracebuf, &bytes_traced,
		    (flags & STACKSHOT_SET_MICROSTACKSHOT_MARK) ? true : false);
		*retval = (int)bytes_traced;
		goto unlock_exit;
	}

unlock_exit:
	STACKSHOT_SUBSYS_UNLOCK();
exit:
	return error;
}
#endif /* CONFIG_TELEMETRY */

/*
 * Return the estimated size of a stackshot based on the
 * number of currently running threads and tasks.
 *
 * adj is an adjustment in units of percentage
 *
 * This function is mostly unhinged from reality; struct thread_snapshot and
 * struct task_stackshot are legacy, much larger versions of the structures we
 * actually use, and there's no accounting for how we actually generate
 * task & thread information.  rdar://78880038 intends to replace this all.
 */
uint32_t
get_stackshot_estsize(uint32_t prev_size_hint, uint32_t adj)
{
	vm_size_t thread_total;
	vm_size_t task_total;
	uint64_t size;
	uint32_t estimated_size;
	size_t est_thread_size = sizeof(struct thread_snapshot);
	size_t est_task_size = sizeof(struct task_snapshot) + TASK_UUID_AVG_SIZE;

	adj = MIN(adj, 100u);   /* no more than double our estimate */

#if STACKSHOT_COLLECTS_LATENCY_INFO
	if (collect_latency_info) {
		est_thread_size += sizeof(struct stackshot_latency_thread);
		est_task_size += sizeof(struct stackshot_latency_task);
	}
#endif

	thread_total = (threads_count * est_thread_size);
	task_total = (tasks_count  * est_task_size);

	size = thread_total + task_total + STACKSHOT_SUPP_SIZE;                 /* estimate */
	size += (size * adj) / 100;                                                                     /* add adj */
	size = MAX(size, prev_size_hint);                                                               /* allow hint to increase */
	size += stackshot_plh_est_size(); /* add space for the port label hash */
	size = MIN(size, VM_MAP_TRUNC_PAGE(UINT32_MAX, PAGE_MASK));             /* avoid overflow */
	estimated_size = (uint32_t) VM_MAP_ROUND_PAGE(size, PAGE_MASK); /* round to pagesize */

	return estimated_size;
}

/*
 * stackshot_remap_buffer:	Utility function to remap bytes_traced bytes starting at stackshotbuf
 *				into the current task's user space and subsequently copy out the address
 *				at which the buffer has been mapped in user space to out_buffer_addr.
 *
 * Inputs:			stackshotbuf - pointer to the original buffer in the kernel's address space
 *				bytes_traced - length of the buffer to remap starting from stackshotbuf
 *				out_buffer_addr - pointer to placeholder where newly mapped buffer will be mapped.
 *				out_size_addr - pointer to be filled in with the size of the buffer
 *
 * Outputs:			ENOSPC if there is not enough free space in the task's address space to remap the buffer
 *				EINVAL for all other errors returned by task_remap_buffer/mach_vm_remap
 *				an error from copyout
 */
static kern_return_t
stackshot_remap_buffer(void *stackshotbuf, uint32_t bytes_traced, uint64_t out_buffer_addr, uint64_t out_size_addr)
{
	int                     error = 0;
	mach_vm_offset_t        stackshotbuf_user_addr = (mach_vm_offset_t)NULL;
	vm_prot_t               cur_prot, max_prot;

	error = mach_vm_remap_kernel(get_task_map(current_task()), &stackshotbuf_user_addr, bytes_traced, 0,
	    VM_FLAGS_ANYWHERE, VM_KERN_MEMORY_NONE, kernel_map, (mach_vm_offset_t)stackshotbuf, FALSE, &cur_prot, &max_prot, VM_INHERIT_DEFAULT);
	/*
	 * If the call to mach_vm_remap fails, we return the appropriate converted error
	 */
	if (error == KERN_SUCCESS) {
		/*
		 * If we fail to copy out the address or size of the new buffer, we remove the buffer mapping that
		 * we just made in the task's user space.
		 */
		error = copyout(CAST_DOWN(void *, &stackshotbuf_user_addr), (user_addr_t)out_buffer_addr, sizeof(stackshotbuf_user_addr));
		if (error != KERN_SUCCESS) {
			mach_vm_deallocate(get_task_map(current_task()), stackshotbuf_user_addr, (mach_vm_size_t)bytes_traced);
			return error;
		}
		error = copyout(&bytes_traced, (user_addr_t)out_size_addr, sizeof(bytes_traced));
		if (error != KERN_SUCCESS) {
			mach_vm_deallocate(get_task_map(current_task()), stackshotbuf_user_addr, (mach_vm_size_t)bytes_traced);
			return error;
		}
	}
	return error;
}

kern_return_t
kern_stack_snapshot_internal(int stackshot_config_version, void *stackshot_config, size_t stackshot_config_size, boolean_t stackshot_from_user)
{
	int error = 0;
	boolean_t prev_interrupt_state;
	uint32_t bytes_traced = 0;
	uint32_t stackshot_estimate = 0;
	uint32_t stackshotbuf_size = 0;
	void * stackshotbuf = NULL;
	kcdata_descriptor_t kcdata_p = NULL;

	void * buf_to_free = NULL;
	int size_to_free = 0;
	bool is_traced = false;    /* has FUNC_START tracepoint fired? */
	uint64_t tot_interrupts_off_abs = 0; /* sum(time with interrupts off) */

	/* Parsed arguments */
	uint64_t                out_buffer_addr;
	uint64_t                out_size_addr;
	int                     pid = -1;
	uint64_t                flags;
	uint64_t                since_timestamp;
	uint32_t                size_hint = 0;
	uint32_t                pagetable_mask = STACKSHOT_PAGETABLES_MASK_ALL;

	if (stackshot_config == NULL) {
		return KERN_INVALID_ARGUMENT;
	}
#if DEVELOPMENT || DEBUG
	/* TBD: ask stackshot clients to avoid issuing stackshots in this
	 * configuration in lieu of the kernel feature override.
	 */
	if (kern_feature_override(KF_STACKSHOT_OVRD) == TRUE) {
		return KERN_NOT_SUPPORTED;
	}
#endif

	switch (stackshot_config_version) {
	case STACKSHOT_CONFIG_TYPE:
		if (stackshot_config_size != sizeof(stackshot_config_t)) {
			return KERN_INVALID_ARGUMENT;
		}
		stackshot_config_t *config = (stackshot_config_t *) stackshot_config;
		out_buffer_addr = config->sc_out_buffer_addr;
		out_size_addr = config->sc_out_size_addr;
		pid = config->sc_pid;
		flags = config->sc_flags;
		since_timestamp = config->sc_delta_timestamp;
		if (config->sc_size <= max_tracebuf_size) {
			size_hint = config->sc_size;
		}
		/*
		 * Retain the pre-sc_pagetable_mask behavior of STACKSHOT_PAGE_TABLES,
		 * dump every level if the pagetable_mask is not set
		 */
		if (flags & STACKSHOT_PAGE_TABLES && config->sc_pagetable_mask) {
			pagetable_mask = config->sc_pagetable_mask;
		}
		break;
	default:
		return KERN_NOT_SUPPORTED;
	}

	/*
	 * Currently saving a kernel buffer and trylock are only supported from the
	 * internal/KEXT API.
	 */
	if (stackshot_from_user) {
		if (flags & (STACKSHOT_TRYLOCK | STACKSHOT_SAVE_IN_KERNEL_BUFFER | STACKSHOT_FROM_PANIC)) {
			return KERN_NO_ACCESS;
		}
#if !DEVELOPMENT && !DEBUG
		if (flags & (STACKSHOT_DO_COMPRESS)) {
			return KERN_NO_ACCESS;
		}
#endif
	} else {
		if (!(flags & STACKSHOT_SAVE_IN_KERNEL_BUFFER)) {
			return KERN_NOT_SUPPORTED;
		}
	}

	if (!((flags & STACKSHOT_KCDATA_FORMAT) || (flags & STACKSHOT_RETRIEVE_EXISTING_BUFFER))) {
		return KERN_NOT_SUPPORTED;
	}

	/* Compresssed delta stackshots or page dumps are not yet supported */
	if (((flags & STACKSHOT_COLLECT_DELTA_SNAPSHOT) || (flags & STACKSHOT_PAGE_TABLES))
	    && (flags & STACKSHOT_DO_COMPRESS)) {
		return KERN_NOT_SUPPORTED;
	}

	/*
	 * If we're not saving the buffer in the kernel pointer, we need a place to copy into.
	 */
	if ((!out_buffer_addr || !out_size_addr) && !(flags & STACKSHOT_SAVE_IN_KERNEL_BUFFER)) {
		return KERN_INVALID_ARGUMENT;
	}

	if (since_timestamp != 0 && ((flags & STACKSHOT_COLLECT_DELTA_SNAPSHOT) == 0)) {
		return KERN_INVALID_ARGUMENT;
	}

#if MONOTONIC
	if (!mt_core_supported) {
		flags &= ~STACKSHOT_INSTRS_CYCLES;
	}
#else /* MONOTONIC */
	flags &= ~STACKSHOT_INSTRS_CYCLES;
#endif /* !MONOTONIC */

	STACKSHOT_SUBSYS_LOCK();

	if (flags & STACKSHOT_SAVE_IN_KERNEL_BUFFER) {
		/*
		 * Don't overwrite an existing stackshot
		 */
		if (kernel_stackshot_buf != NULL) {
			error = KERN_MEMORY_PRESENT;
			goto error_exit;
		}
	} else if (flags & STACKSHOT_RETRIEVE_EXISTING_BUFFER) {
		if ((kernel_stackshot_buf == NULL) || (kernel_stackshot_buf_size <= 0)) {
			error = KERN_NOT_IN_SET;
			goto error_exit;
		}
		error = stackshot_remap_buffer(kernel_stackshot_buf, kernel_stackshot_buf_size,
		    out_buffer_addr, out_size_addr);
		/*
		 * If we successfully remapped the buffer into the user's address space, we
		 * set buf_to_free and size_to_free so the prior kernel mapping will be removed
		 * and then clear the kernel stackshot pointer and associated size.
		 */
		if (error == KERN_SUCCESS) {
			buf_to_free = kernel_stackshot_buf;
			size_to_free = (int) VM_MAP_ROUND_PAGE(kernel_stackshot_buf_size, PAGE_MASK);
			kernel_stackshot_buf = NULL;
			kernel_stackshot_buf_size = 0;
		}

		goto error_exit;
	}

	if (flags & STACKSHOT_GET_BOOT_PROFILE) {
		void *bootprofile = NULL;
		uint32_t len = 0;
#if CONFIG_TELEMETRY
		bootprofile_get(&bootprofile, &len);
#endif
		if (!bootprofile || !len) {
			error = KERN_NOT_IN_SET;
			goto error_exit;
		}
		error = stackshot_remap_buffer(bootprofile, len, out_buffer_addr, out_size_addr);
		goto error_exit;
	}

	stackshot_duration_prior_abs = 0;
	stackshot_initial_estimate_adj = os_atomic_load(&stackshot_estimate_adj, relaxed);
	stackshotbuf_size = stackshot_estimate =
	    get_stackshot_estsize(size_hint, stackshot_initial_estimate_adj);
	stackshot_initial_estimate = stackshot_estimate;

	KDBG_RELEASE(MACHDBG_CODE(DBG_MACH_STACKSHOT, STACKSHOT_RECORD) | DBG_FUNC_START,
	    flags, stackshotbuf_size, pid, since_timestamp);
	is_traced = true;

	for (; stackshotbuf_size <= max_tracebuf_size; stackshotbuf_size <<= 1) {
		if (kernel_memory_allocate(kernel_map, (vm_offset_t *)&stackshotbuf, stackshotbuf_size,
		    0, KMA_ZERO, VM_KERN_MEMORY_DIAG) != KERN_SUCCESS) {
			error = KERN_RESOURCE_SHORTAGE;
			goto error_exit;
		}


		uint32_t hdr_tag = (flags & STACKSHOT_COLLECT_DELTA_SNAPSHOT) ? KCDATA_BUFFER_BEGIN_DELTA_STACKSHOT
		    : (flags & STACKSHOT_DO_COMPRESS) ? KCDATA_BUFFER_BEGIN_COMPRESSED
		    : KCDATA_BUFFER_BEGIN_STACKSHOT;
		kcdata_p = kcdata_memory_alloc_init((mach_vm_address_t)stackshotbuf, hdr_tag, stackshotbuf_size,
		    KCFLAG_USE_MEMCOPY | KCFLAG_NO_AUTO_ENDBUFFER);

		stackshot_duration_outer = NULL;

		/* if compression was requested, allocate the extra zlib scratch area */
		if (flags & STACKSHOT_DO_COMPRESS) {
			hdr_tag = (flags & STACKSHOT_COLLECT_DELTA_SNAPSHOT) ? KCDATA_BUFFER_BEGIN_DELTA_STACKSHOT
			    : KCDATA_BUFFER_BEGIN_STACKSHOT;
			error = kcdata_init_compress(kcdata_p, hdr_tag, stackshot_memcpy, KCDCT_ZLIB);
			if (error != KERN_SUCCESS) {
				os_log(OS_LOG_DEFAULT, "failed to initialize compression: %d!\n",
				    (int) error);
				goto error_exit;
			}
		}

		/*
		 * Disable interrupts and save the current interrupt state.
		 */
		prev_interrupt_state = ml_set_interrupts_enabled(FALSE);
		uint64_t time_start      = mach_absolute_time();

		/* Emit a SOCD tracepoint that we are initiating a stackshot */
		SOCD_TRACE_XNU_START(STACKSHOT);

		/*
		 * Load stackshot parameters.
		 */
		kdp_snapshot_preflight(pid, stackshotbuf, stackshotbuf_size, flags, kcdata_p, since_timestamp,
		    pagetable_mask);

		error = stackshot_trap();

		/* record the duration that interupts were disabled */
		uint64_t time_end = mach_absolute_time();

		/* Emit a SOCD tracepoint that we have completed the stackshot */
		SOCD_TRACE_XNU_END(STACKSHOT);
		ml_set_interrupts_enabled(prev_interrupt_state);

		if (stackshot_duration_outer) {
			*stackshot_duration_outer = time_end - time_start;
		}
		tot_interrupts_off_abs += time_end - time_start;

		if (error != KERN_SUCCESS) {
			if (kcdata_p != NULL) {
				kcdata_memory_destroy(kcdata_p);
				kcdata_p = NULL;
				stackshot_kcdata_p = NULL;
			}
			kmem_free(kernel_map, (vm_offset_t)stackshotbuf, stackshotbuf_size);
			stackshotbuf = NULL;
			if (error == KERN_INSUFFICIENT_BUFFER_SIZE) {
				/*
				 * If we didn't allocate a big enough buffer, deallocate and try again.
				 */
				KDBG_RELEASE(MACHDBG_CODE(DBG_MACH_STACKSHOT, STACKSHOT_RECORD_SHORT) | DBG_FUNC_NONE,
				    time_end - time_start, stackshot_estimate, stackshotbuf_size);
				stackshot_duration_prior_abs += (time_end - time_start);
				continue;
			} else {
				goto error_exit;
			}
		}

		bytes_traced = kdp_stack_snapshot_bytes_traced();
		if (bytes_traced <= 0) {
			error = KERN_ABORTED;
			goto error_exit;
		}

		assert(bytes_traced <= stackshotbuf_size);
		if (!(flags & STACKSHOT_SAVE_IN_KERNEL_BUFFER)) {
			error = stackshot_remap_buffer(stackshotbuf, bytes_traced, out_buffer_addr, out_size_addr);
			goto error_exit;
		}

		/*
		 * Save the stackshot in the kernel buffer.
		 */
		kernel_stackshot_buf = stackshotbuf;
		kernel_stackshot_buf_size =  bytes_traced;
		/*
		 * Figure out if we didn't use all the pages in the buffer. If so, we set buf_to_free to the beginning of
		 * the next page after the end of the stackshot in the buffer so that the kmem_free clips the buffer and
		 * update size_to_free for kmem_free accordingly.
		 */
		size_to_free = stackshotbuf_size - (int) VM_MAP_ROUND_PAGE(bytes_traced, PAGE_MASK);

		assert(size_to_free >= 0);

		if (size_to_free != 0) {
			buf_to_free = (void *)((uint64_t)stackshotbuf + stackshotbuf_size - size_to_free);
		}

		stackshotbuf = NULL;
		stackshotbuf_size = 0;
		goto error_exit;
	}

	if (stackshotbuf_size > max_tracebuf_size) {
		error = KERN_RESOURCE_SHORTAGE;
	}

error_exit:
	if (is_traced) {
		KDBG_RELEASE(MACHDBG_CODE(DBG_MACH_STACKSHOT, STACKSHOT_RECORD) | DBG_FUNC_END,
		    error, tot_interrupts_off_abs, stackshotbuf_size, bytes_traced);
	}
	if (kcdata_p != NULL) {
		kcdata_memory_destroy(kcdata_p);
		kcdata_p = NULL;
		stackshot_kcdata_p = NULL;
	}

	if (stackshotbuf != NULL) {
		kmem_free(kernel_map, (vm_offset_t)stackshotbuf, stackshotbuf_size);
	}
	if (buf_to_free != NULL) {
		kmem_free(kernel_map, (vm_offset_t)buf_to_free, size_to_free);
	}
	STACKSHOT_SUBSYS_UNLOCK();
	return error;
}

/*
 * Cache stack snapshot parameters in preparation for a trace.
 */
void
kdp_snapshot_preflight(int pid, void * tracebuf, uint32_t tracebuf_size, uint64_t flags,
    kcdata_descriptor_t data_p, uint64_t since_timestamp, uint32_t pagetable_mask)
{
	uint64_t microsecs = 0, secs = 0;
	clock_get_calendar_microtime((clock_sec_t *)&secs, (clock_usec_t *)&microsecs);

	stackshot_microsecs = microsecs + (secs * USEC_PER_SEC);
	stack_snapshot_pid = pid;
	stack_snapshot_buf = tracebuf;
	stack_snapshot_bufsize = tracebuf_size;
	stack_snapshot_flags = flags;
	stack_snapshot_delta_since_timestamp = since_timestamp;
	stack_snapshot_pagetable_mask = pagetable_mask;

	panic_stackshot = ((flags & STACKSHOT_FROM_PANIC) != 0);

	assert(data_p != NULL);
	assert(stackshot_kcdata_p == NULL);
	stackshot_kcdata_p = data_p;

	stack_snapshot_bytes_traced = 0;
	stack_snapshot_bytes_uncompressed = 0;
}

void
panic_stackshot_reset_state()
{
	stackshot_kcdata_p = NULL;
}

boolean_t
stackshot_active()
{
	return stackshot_kcdata_p != NULL;
}

uint32_t
kdp_stack_snapshot_bytes_traced(void)
{
	return stack_snapshot_bytes_traced;
}

uint32_t
kdp_stack_snapshot_bytes_uncompressed(void)
{
	return stack_snapshot_bytes_uncompressed;
}

static boolean_t
memory_iszero(void *addr, size_t size)
{
	char *data = (char *)addr;
	for (size_t i = 0; i < size; i++) {
		if (data[i] != 0) {
			return FALSE;
		}
	}
	return TRUE;
}

/*
 * Keep a simple cache of the most recent validation done at a page granularity
 * to avoid the expensive software KVA-to-phys translation in the VM.
 */

struct _stackshot_validation_state {
	vm_offset_t last_valid_page_kva;
	size_t last_valid_size;
} g_validation_state;

static void
_stackshot_validation_reset(void)
{
	g_validation_state.last_valid_page_kva = -1;
	g_validation_state.last_valid_size = 0;
}

static bool
_stackshot_validate_kva(vm_offset_t addr, size_t size)
{
	vm_offset_t page_addr = atop_kernel(addr);
	if (g_validation_state.last_valid_page_kva == page_addr &&
	    g_validation_state.last_valid_size <= size) {
		return true;
	}

	if (ml_validate_nofault(addr, size)) {
		g_validation_state.last_valid_page_kva = page_addr;
		g_validation_state.last_valid_size = size;
		return true;
	}
	return false;
}

#define kcd_end_address(kcd) ((void *)((uint64_t)((kcd)->kcd_addr_begin) + kcdata_memory_get_used_bytes((kcd))))
#define kcd_max_address(kcd) ((void *)((kcd)->kcd_addr_begin + (kcd)->kcd_length))
/*
 * Use of the kcd_exit_on_error(action) macro requires a local
 * 'kern_return_t error' variable and 'error_exit' label.
 */
#define kcd_exit_on_error(action)                      \
	do {                                               \
	        if (KERN_SUCCESS != (error = (action))) {      \
	                if (error == KERN_RESOURCE_SHORTAGE) {     \
	                        error = KERN_INSUFFICIENT_BUFFER_SIZE; \
	                }                                          \
	                goto error_exit;                           \
	        }                                              \
	} while (0); /* end kcd_exit_on_error */


/*
 * For port labels, we have a small hash table we use to track the
 * struct ipc_service_port_label pointers we see along the way.
 * This structure encapsulates the global state.
 *
 * The hash table is insert-only, similar to "intern"ing strings.  It's
 * only used an manipulated in during the stackshot collection.  We use
 * seperate chaining, with the hash elements and chains being int16_ts
 * indexes into the parallel arrays, with -1 ending the chain.  Array indices are
 * allocated using a bump allocator.
 *
 * The parallel arrays contain:
 *      - plh_array[idx]	the pointer entered
 *      - plh_chains[idx]	the hash chain
 *      - plh_gen[idx]		the last 'generation #' seen
 *
 * Generation IDs are used to track entries looked up in the current
 * task; 0 is never used, and the plh_gen array is cleared to 0 on
 * rollover.
 *
 * The portlabel_ids we report externally are just the index in the array,
 * plus 1 to avoid 0 as a value.  0 is NONE, -1 is UNKNOWN (e.g. there is
 * one, but we ran out of space)
 */
struct port_label_hash {
	uint16_t                plh_size;       /* size of allocations; 0 disables tracking */
	uint16_t                plh_count;      /* count of used entries in plh_array */
	struct ipc_service_port_label **plh_array; /* _size allocated, _count used */
	int16_t                *plh_chains;    /* _size allocated */
	uint8_t                *plh_gen;       /* last 'gen #' seen in */
	int16_t                *plh_hash;      /* (1 << STACKSHOT_PLH_SHIFT) entry hash table: hash(ptr) -> array index */
	int16_t                 plh_curgen_min; /* min idx seen for this gen */
	int16_t                 plh_curgen_max; /* max idx seen for this gen */
	uint8_t                 plh_curgen;     /* current gen */
#if DEVELOPMENT || DEBUG
	/* statistics */
	uint32_t                plh_lookups;    /* # lookups or inserts */
	uint32_t                plh_found;
	uint32_t                plh_found_depth;
	uint32_t                plh_insert;
	uint32_t                plh_insert_depth;
	uint32_t                plh_bad;
	uint32_t                plh_bad_depth;
	uint32_t                plh_lookup_send;
	uint32_t                plh_lookup_receive;
#define PLH_STAT_OP(...)    (void)(__VA_ARGS__)
#else /* DEVELOPMENT || DEBUG */
#define PLH_STAT_OP(...)    (void)(0)
#endif /* DEVELOPMENT || DEBUG */
} port_label_hash;

#define STACKSHOT_PLH_SHIFT    7
#define STACKSHOT_PLH_SIZE_MAX ((kdp_ipc_have_splabel)? 1024 : 0)
size_t stackshot_port_label_size = (2 * (1u << STACKSHOT_PLH_SHIFT));
#define STASKSHOT_PLH_SIZE(x) MIN((x), STACKSHOT_PLH_SIZE_MAX)

static size_t
stackshot_plh_est_size(void)
{
	struct port_label_hash *plh = &port_label_hash;
	size_t size = STASKSHOT_PLH_SIZE(stackshot_port_label_size);

	if (size == 0) {
		return 0;
	}
#define SIZE_EST(x) ROUNDUP((x), sizeof (uintptr_t))
	return SIZE_EST(size * sizeof(*plh->plh_array)) +
	       SIZE_EST(size * sizeof(*plh->plh_chains)) +
	       SIZE_EST(size * sizeof(*plh->plh_gen)) +
	       SIZE_EST((1ul << STACKSHOT_PLH_SHIFT) * sizeof(*plh->plh_hash));
#undef SIZE_EST
}

static void
stackshot_plh_reset(void)
{
	port_label_hash = (struct port_label_hash){.plh_size = 0};  /* structure assignment */
}

static void
stackshot_plh_setup(kcdata_descriptor_t data)
{
	struct port_label_hash plh = {
		.plh_size = STASKSHOT_PLH_SIZE(stackshot_port_label_size),
		.plh_count = 0,
		.plh_curgen = 1,
		.plh_curgen_min = STACKSHOT_PLH_SIZE_MAX,
		.plh_curgen_max = 0,
	};
	stackshot_plh_reset();
	size_t size = plh.plh_size;
	if (size == 0) {
		return;
	}
	plh.plh_array = kcdata_endalloc(data, size * sizeof(*plh.plh_array));
	plh.plh_chains = kcdata_endalloc(data, size * sizeof(*plh.plh_chains));
	plh.plh_gen = kcdata_endalloc(data, size * sizeof(*plh.plh_gen));
	plh.plh_hash = kcdata_endalloc(data, (1ul << STACKSHOT_PLH_SHIFT) * sizeof(*plh.plh_hash));
	if (plh.plh_array == NULL || plh.plh_chains == NULL || plh.plh_gen == NULL || plh.plh_hash == NULL) {
		PLH_STAT_OP(port_label_hash.plh_bad++);
		return;
	}
	for (int x = 0; x < size; x++) {
		plh.plh_array[x] = NULL;
		plh.plh_chains[x] = -1;
		plh.plh_gen[x] = 0;
	}
	for (int x = 0; x < (1ul << STACKSHOT_PLH_SHIFT); x++) {
		plh.plh_hash[x] = -1;
	}
	port_label_hash = plh;  /* structure assignment */
}

static int16_t
stackshot_plh_hash(struct ipc_service_port_label *ispl)
{
	uintptr_t ptr = (uintptr_t)ispl;
	static_assert(STACKSHOT_PLH_SHIFT < 16, "plh_hash must fit in 15 bits");
#define PLH_HASH_STEP(ptr, x) \
	    ((((x) * STACKSHOT_PLH_SHIFT) < (sizeof(ispl) * CHAR_BIT)) ? ((ptr) >> ((x) * STACKSHOT_PLH_SHIFT)) : 0)
	ptr ^= PLH_HASH_STEP(ptr, 16);
	ptr ^= PLH_HASH_STEP(ptr, 8);
	ptr ^= PLH_HASH_STEP(ptr, 4);
	ptr ^= PLH_HASH_STEP(ptr, 2);
	ptr ^= PLH_HASH_STEP(ptr, 1);
#undef PLH_HASH_STEP
	return (int16_t)(ptr & ((1ul << STACKSHOT_PLH_SHIFT) - 1));
}

enum stackshot_plh_lookup_type {
	STACKSHOT_PLH_LOOKUP_UNKNOWN,
	STACKSHOT_PLH_LOOKUP_SEND,
	STACKSHOT_PLH_LOOKUP_RECEIVE,
};

static void
stackshot_plh_resetgen(void)
{
	struct port_label_hash *plh = &port_label_hash;
	if (plh->plh_curgen_min == STACKSHOT_PLH_SIZE_MAX && plh->plh_curgen_max == 0) {
		return;  // no lookups, nothing using the current generation
	}
	plh->plh_curgen++;
	plh->plh_curgen_min = STACKSHOT_PLH_SIZE_MAX;
	plh->plh_curgen_max = 0;
	if (plh->plh_curgen == 0) { // wrapped, zero the array and increment the generation
		for (int x = 0; x < plh->plh_size; x++) {
			plh->plh_gen[x] = 0;
		}
		plh->plh_curgen = 1;
	}
}

static int16_t
stackshot_plh_lookup(struct ipc_service_port_label *ispl, enum stackshot_plh_lookup_type type)
{
	struct port_label_hash *plh = &port_label_hash;
	int depth;
	int16_t cur;
	if (ispl == NULL) {
		return STACKSHOT_PORTLABELID_NONE;
	}
	switch (type) {
	case STACKSHOT_PLH_LOOKUP_SEND:
		PLH_STAT_OP(plh->plh_lookup_send++);
		break;
	case STACKSHOT_PLH_LOOKUP_RECEIVE:
		PLH_STAT_OP(plh->plh_lookup_receive++);
		break;
	default:
		break;
	}
	PLH_STAT_OP(plh->plh_lookups++);
	if (plh->plh_size == 0) {
		return STACKSHOT_PORTLABELID_MISSING;
	}
	int16_t hash = stackshot_plh_hash(ispl);
	assert(hash >= 0 && hash < (1ul << STACKSHOT_PLH_SHIFT));
	depth = 0;
	for (cur = plh->plh_hash[hash]; cur >= 0; cur = plh->plh_chains[cur]) {
		/* cur must be in-range, and chain depth can never be above our # allocated */
		if (cur >= plh->plh_count || depth > plh->plh_count || depth > plh->plh_size) {
			PLH_STAT_OP((plh->plh_bad++), (plh->plh_bad_depth += depth));
			return STACKSHOT_PORTLABELID_MISSING;
		}
		assert(cur < plh->plh_count);
		if (plh->plh_array[cur] == ispl) {
			PLH_STAT_OP((plh->plh_found++), (plh->plh_found_depth += depth));
			goto found;
		}
		depth++;
	}
	/* not found in hash table, so alloc and insert it */
	if (cur != -1) {
		PLH_STAT_OP((plh->plh_bad++), (plh->plh_bad_depth += depth));
		return STACKSHOT_PORTLABELID_MISSING; /* bad end of chain */
	}
	PLH_STAT_OP((plh->plh_insert++), (plh->plh_insert_depth += depth));
	if (plh->plh_count >= plh->plh_size) {
		return STACKSHOT_PORTLABELID_MISSING; /* no space */
	}
	cur = plh->plh_count;
	plh->plh_count++;
	plh->plh_array[cur] = ispl;
	plh->plh_chains[cur] = plh->plh_hash[hash];
	plh->plh_hash[hash] = cur;
found:
	plh->plh_gen[cur] = plh->plh_curgen;
	if (plh->plh_curgen_min > cur) {
		plh->plh_curgen_min = cur;
	}
	if (plh->plh_curgen_max < cur) {
		plh->plh_curgen_max = cur;
	}
	return cur + 1;   /* offset to avoid 0 */
}

// record any PLH referenced since the last stackshot_plh_resetgen() call
static kern_return_t
kdp_stackshot_plh_record(void)
{
	kern_return_t error = KERN_SUCCESS;
	struct port_label_hash *plh = &port_label_hash;
	uint16_t count = plh->plh_count;
	uint8_t curgen = plh->plh_curgen;
	int16_t curgen_min = plh->plh_curgen_min;
	int16_t curgen_max = plh->plh_curgen_max;
	if (curgen_min <= curgen_max && curgen_max < count &&
	    count <= plh->plh_size && plh->plh_size <= STACKSHOT_PLH_SIZE_MAX) {
		struct ipc_service_port_label **arr = plh->plh_array;
		size_t ispl_size, max_namelen;
		kdp_ipc_splabel_size(&ispl_size, &max_namelen);
		for (int idx = curgen_min; idx <= curgen_max; idx++) {
			struct ipc_service_port_label *ispl = arr[idx];
			struct portlabel_info spl = {
				.portlabel_id = (idx + 1),
			};
			const char *name = NULL;
			long name_sz = 0;
			if (plh->plh_gen[idx] != curgen) {
				continue;
			}
			if (_stackshot_validate_kva((vm_offset_t)ispl, ispl_size)) {
				kdp_ipc_fill_splabel(ispl, &spl, &name);
			}
			kcd_exit_on_error(kcdata_add_container_marker(stackshot_kcdata_p, KCDATA_TYPE_CONTAINER_BEGIN,
			    STACKSHOT_KCCONTAINER_PORTLABEL, idx + 1));
			if (name != NULL && (name_sz = _stackshot_strlen(name, max_namelen)) > 0) {   /* validates the kva */
				kcd_exit_on_error(kcdata_push_data(stackshot_kcdata_p, STACKSHOT_KCTYPE_PORTLABEL_NAME, name_sz + 1, name));
			} else {
				spl.portlabel_flags |= STACKSHOT_PORTLABEL_READFAILED;
			}
			kcd_exit_on_error(kcdata_push_data(stackshot_kcdata_p, STACKSHOT_KCTYPE_PORTLABEL, sizeof(spl), &spl));
			kcd_exit_on_error(kcdata_add_container_marker(stackshot_kcdata_p, KCDATA_TYPE_CONTAINER_END,
			    STACKSHOT_KCCONTAINER_PORTLABEL, idx + 1));
		}
	}

error_exit:
	return error;
}

#if DEVELOPMENT || DEBUG
static kern_return_t
kdp_stackshot_plh_stats(void)
{
	kern_return_t error = KERN_SUCCESS;
	struct port_label_hash *plh = &port_label_hash;

#define PLH_STAT(x) do { if (plh->x != 0) { \
	kcd_exit_on_error(kcdata_add_uint32_with_description(stackshot_kcdata_p, plh->x, "stackshot_" #x)); \
} } while (0)
	PLH_STAT(plh_size);
	PLH_STAT(plh_lookups);
	PLH_STAT(plh_found);
	PLH_STAT(plh_found_depth);
	PLH_STAT(plh_insert);
	PLH_STAT(plh_insert_depth);
	PLH_STAT(plh_bad);
	PLH_STAT(plh_bad_depth);
	PLH_STAT(plh_lookup_send);
	PLH_STAT(plh_lookup_receive);
#undef PLH_STAT

error_exit:
	return error;
}
#endif /* DEVELOPMENT || DEBUG */

static uint64_t
kcdata_get_task_ss_flags(task_t task)
{
	uint64_t ss_flags = 0;
	boolean_t task_64bit_addr = task_has_64Bit_addr(task);

	if (task_64bit_addr) {
		ss_flags |= kUser64_p;
	}
	if (!task->active || task_is_a_corpse(task) || proc_exiting(task->bsd_info)) {
		ss_flags |= kTerminatedSnapshot;
	}
	if (task->pidsuspended) {
		ss_flags |= kPidSuspended;
	}
	if (task->frozen) {
		ss_flags |= kFrozen;
	}
	if (task->effective_policy.tep_darwinbg == 1) {
		ss_flags |= kTaskDarwinBG;
	}
	if (task->requested_policy.trp_role == TASK_FOREGROUND_APPLICATION) {
		ss_flags |= kTaskIsForeground;
	}
	if (task->requested_policy.trp_boosted == 1) {
		ss_flags |= kTaskIsBoosted;
	}
	if (task->effective_policy.tep_sup_active == 1) {
		ss_flags |= kTaskIsSuppressed;
	}
#if CONFIG_MEMORYSTATUS

	boolean_t dirty = FALSE, dirty_tracked = FALSE, allow_idle_exit = FALSE;
	memorystatus_proc_flags_unsafe(task->bsd_info, &dirty, &dirty_tracked, &allow_idle_exit);
	if (dirty) {
		ss_flags |= kTaskIsDirty;
	}
	if (dirty_tracked) {
		ss_flags |= kTaskIsDirtyTracked;
	}
	if (allow_idle_exit) {
		ss_flags |= kTaskAllowIdleExit;
	}

#endif
	if (task->effective_policy.tep_tal_engaged) {
		ss_flags |= kTaskTALEngaged;
	}

	ss_flags |= (0x7 & workqueue_get_pwq_state_kdp(task->bsd_info)) << 17;

#if IMPORTANCE_INHERITANCE
	if (task->task_imp_base) {
		if (task->task_imp_base->iit_donor) {
			ss_flags |= kTaskIsImpDonor;
		}
		if (task->task_imp_base->iit_live_donor) {
			ss_flags |= kTaskIsLiveImpDonor;
		}
	}
#endif
	return ss_flags;
}

static kern_return_t
kcdata_record_shared_cache_info(kcdata_descriptor_t kcd, task_t task, unaligned_u64 *task_snap_ss_flags)
{
	kern_return_t error = KERN_SUCCESS;

	uint64_t shared_cache_slide = 0;
	uint64_t shared_cache_first_mapping = 0;
	uint32_t kdp_fault_results = 0;
	struct dyld_shared_cache_loadinfo shared_cache_data = {0};


	assert(task_snap_ss_flags != NULL);

	/* Get basic info about the shared region pointer, regardless of any failures */
	if (task->shared_region == NULL) {
		*task_snap_ss_flags |= kTaskSharedRegionNone;
	} else if (task->shared_region == primary_system_shared_region) {
		*task_snap_ss_flags |= kTaskSharedRegionSystem;
	} else {
		*task_snap_ss_flags |= kTaskSharedRegionOther;
	}

	if (task->shared_region && _stackshot_validate_kva((vm_offset_t)task->shared_region, sizeof(struct vm_shared_region))) {
		struct vm_shared_region *sr = task->shared_region;
		shared_cache_first_mapping = sr->sr_base_address + sr->sr_first_mapping;

	} else {
		*task_snap_ss_flags |= kTaskSharedRegionInfoUnavailable;
		goto error_exit;
	}

	/* We haven't copied in the shared region UUID yet as part of setup */
	if (!shared_cache_first_mapping || !task->shared_region->sr_uuid_copied) {
		goto error_exit;
	}


	/*
	 * No refcounting here, but we are in debugger context, so that should be safe.
	 */
	shared_cache_slide = task->shared_region->sr_slide;

	if (task->shared_region == primary_system_shared_region) {
		/* skip adding shared cache info -- it's the same as the system level one */
		goto error_exit;
	}

	/*
	 * Historically, this data was in a dyld_uuid_info_64 structure, but the
	 * naming of both the structure and fields for this use wasn't great.  The
	 * dyld_shared_cache_loadinfo structure has better names, but the same
	 * layout and content as the original.
	 *
	 * The imageSlidBaseAddress/sharedCacheUnreliableSlidBaseAddress field
	 * has been used inconsistently for STACKSHOT_COLLECT_SHAREDCACHE_LAYOUT
	 * entries; here, it's the slid first mapping, and we leave it that way
	 * for backwards compatibility.
	 */
	shared_cache_data.sharedCacheSlide = shared_cache_slide;
	stackshot_memcpy(&shared_cache_data.sharedCacheUUID, task->shared_region->sr_uuid, sizeof(task->shared_region->sr_uuid));
	shared_cache_data.sharedCacheUnreliableSlidBaseAddress = shared_cache_first_mapping;
	shared_cache_data.sharedCacheSlidFirstMapping = shared_cache_first_mapping;
	kcd_exit_on_error(kcdata_push_data(kcd, STACKSHOT_KCTYPE_SHAREDCACHE_LOADINFO, sizeof(shared_cache_data), &shared_cache_data));

error_exit:
	if (kdp_fault_results & KDP_FAULT_RESULT_PAGED_OUT) {
		*task_snap_ss_flags |= kTaskUUIDInfoMissing;
	}

	if (kdp_fault_results & KDP_FAULT_RESULT_TRIED_FAULT) {
		*task_snap_ss_flags |= kTaskUUIDInfoTriedFault;
	}

	if (kdp_fault_results & KDP_FAULT_RESULT_FAULTED_IN) {
		*task_snap_ss_flags |= kTaskUUIDInfoFaultedIn;
	}

	return error;
}

static kern_return_t
kcdata_record_uuid_info(kcdata_descriptor_t kcd, task_t task, uint64_t trace_flags, boolean_t have_pmap, unaligned_u64 *task_snap_ss_flags)
{
	boolean_t save_loadinfo_p         = ((trace_flags & STACKSHOT_SAVE_LOADINFO) != 0);
	boolean_t save_kextloadinfo_p     = ((trace_flags & STACKSHOT_SAVE_KEXT_LOADINFO) != 0);
	boolean_t should_fault            = (trace_flags & STACKSHOT_ENABLE_UUID_FAULTING);

	kern_return_t error        = KERN_SUCCESS;
	mach_vm_address_t out_addr = 0;

	uint32_t uuid_info_count         = 0;
	mach_vm_address_t uuid_info_addr = 0;
	uint64_t uuid_info_timestamp     = 0;
	uint32_t kdp_fault_results       = 0;


	assert(task_snap_ss_flags != NULL);

	int task_pid     = pid_from_task(task);
	boolean_t task_64bit_addr = task_has_64Bit_addr(task);

	if (save_loadinfo_p && have_pmap && task->active && task_pid > 0) {
		/* Read the dyld_all_image_infos struct from the task memory to get UUID array count and location */
		if (task_64bit_addr) {
			struct user64_dyld_all_image_infos task_image_infos;
			if (kdp_copyin(task->map, task->all_image_info_addr, &task_image_infos,
			    sizeof(struct user64_dyld_all_image_infos), should_fault, &kdp_fault_results)) {
				uuid_info_count = (uint32_t)task_image_infos.uuidArrayCount;
				uuid_info_addr = task_image_infos.uuidArray;
				if (task_image_infos.version >= DYLD_ALL_IMAGE_INFOS_TIMESTAMP_MINIMUM_VERSION) {
					uuid_info_timestamp = task_image_infos.timestamp;
				}

			}
		} else {
			struct user32_dyld_all_image_infos task_image_infos;
			if (kdp_copyin(task->map, task->all_image_info_addr, &task_image_infos,
			    sizeof(struct user32_dyld_all_image_infos), should_fault, &kdp_fault_results)) {
				uuid_info_count = task_image_infos.uuidArrayCount;
				uuid_info_addr = task_image_infos.uuidArray;
				if (task_image_infos.version >= DYLD_ALL_IMAGE_INFOS_TIMESTAMP_MINIMUM_VERSION) {
					uuid_info_timestamp = task_image_infos.timestamp;
				}
			}
		}

		/*
		 * If we get a NULL uuid_info_addr (which can happen when we catch dyld in the middle of updating
		 * this data structure), we zero the uuid_info_count so that we won't even try to save load info
		 * for this task.
		 */
		if (!uuid_info_addr) {
			uuid_info_count = 0;
		}


	}

	if (have_pmap && task_pid == 0) {
		if (save_kextloadinfo_p && _stackshot_validate_kva((vm_offset_t)(gLoadedKextSummaries), sizeof(OSKextLoadedKextSummaryHeader))) {
			uuid_info_count = gLoadedKextSummaries->numSummaries + 1; /* include main kernel UUID */
		} else {
			uuid_info_count = 1; /* include kernelcache UUID (embedded) or kernel UUID (desktop) */
		}
	}

	if (save_loadinfo_p && task_pid > 0 && (uuid_info_count < MAX_LOADINFOS)) {
		uint32_t copied_uuid_count = 0;
		uint32_t uuid_info_size = (uint32_t)(task_64bit_addr ? sizeof(struct user64_dyld_uuid_info) : sizeof(struct user32_dyld_uuid_info));
		uint32_t uuid_info_array_size = 0;

		/* Open a compression window to avoid overflowing the stack */
		kcdata_compression_window_open(kcd);

		/* If we found some UUID information, first try to copy it in -- this will only be non-zero if we had a pmap above */
		if (uuid_info_count > 0) {
			uuid_info_array_size = uuid_info_count * uuid_info_size;

			kcd_exit_on_error(kcdata_get_memory_addr_for_array(kcd, (task_64bit_addr ? KCDATA_TYPE_LIBRARY_LOADINFO64 : KCDATA_TYPE_LIBRARY_LOADINFO),
			    uuid_info_size, uuid_info_count, &out_addr));

			if (!kdp_copyin(task->map, uuid_info_addr, (void *)out_addr, uuid_info_array_size, should_fault, &kdp_fault_results)) {
				bzero((void *)out_addr, uuid_info_array_size);
			} else {
				copied_uuid_count = uuid_info_count;
			}
		}

		uuid_t binary_uuid;
		if (!copied_uuid_count && proc_binary_uuid_kdp(task, binary_uuid)) {
			/* We failed to copyin the UUID information, try to store the UUID of the main binary we have in the proc */
			if (uuid_info_array_size == 0) {
				/* We just need to store one UUID */
				uuid_info_array_size = uuid_info_size;
				kcd_exit_on_error(kcdata_get_memory_addr_for_array(kcd, (task_64bit_addr ? KCDATA_TYPE_LIBRARY_LOADINFO64 : KCDATA_TYPE_LIBRARY_LOADINFO),
				    uuid_info_size, 1, &out_addr));
			}

			if (task_64bit_addr) {
				struct user64_dyld_uuid_info *uuid_info = (struct user64_dyld_uuid_info *)out_addr;
				uint64_t image_load_address = task->mach_header_vm_address;

				stackshot_memcpy(&uuid_info->imageUUID, binary_uuid, sizeof(uuid_t));
				stackshot_memcpy(&uuid_info->imageLoadAddress, &image_load_address, sizeof(image_load_address));
			} else {
				struct user32_dyld_uuid_info *uuid_info = (struct user32_dyld_uuid_info *)out_addr;
				uint32_t image_load_address = (uint32_t) task->mach_header_vm_address;

				stackshot_memcpy(&uuid_info->imageUUID, binary_uuid, sizeof(uuid_t));
				stackshot_memcpy(&uuid_info->imageLoadAddress, &image_load_address, sizeof(image_load_address));
			}
		}

		kcd_exit_on_error(kcdata_compression_window_close(kcd));
	} else if (task_pid == 0 && uuid_info_count > 0 && uuid_info_count < MAX_LOADINFOS) {
		uintptr_t image_load_address;

		do {
#if defined(__arm__) || defined(__arm64__)
			if (kernelcache_uuid_valid && !save_kextloadinfo_p) {
				struct dyld_uuid_info_64 kc_uuid = {0};
				kc_uuid.imageLoadAddress = VM_MIN_KERNEL_AND_KEXT_ADDRESS;
				stackshot_memcpy(&kc_uuid.imageUUID, &kernelcache_uuid, sizeof(uuid_t));
				kcd_exit_on_error(kcdata_push_data(kcd, STACKSHOT_KCTYPE_KERNELCACHE_LOADINFO, sizeof(struct dyld_uuid_info_64), &kc_uuid));
				break;
			}
#endif /* defined(__arm__) || defined(__arm64__) */

			if (!kernel_uuid || !_stackshot_validate_kva((vm_offset_t)kernel_uuid, sizeof(uuid_t))) {
				/* Kernel UUID not found or inaccessible */
				break;
			}

			uint32_t uuid_type = KCDATA_TYPE_LIBRARY_LOADINFO;
			if ((sizeof(kernel_uuid_info) == sizeof(struct user64_dyld_uuid_info))) {
				uuid_type = KCDATA_TYPE_LIBRARY_LOADINFO64;
#if  defined(__arm64__)
				kc_format_t primary_kc_type = KCFormatUnknown;
				if (PE_get_primary_kc_format(&primary_kc_type) && (primary_kc_type == KCFormatFileset)) {
					/* return TEXT_EXEC based load information on arm devices running with fileset kernelcaches */
					uuid_type = STACKSHOT_KCTYPE_LOADINFO64_TEXT_EXEC;
				}
#endif
			}

			/*
			 * The element count of the array can vary - avoid overflowing the
			 * stack by opening a window.
			 */
			kcdata_compression_window_open(kcd);
			kcd_exit_on_error(kcdata_get_memory_addr_for_array(kcd, uuid_type,
			    sizeof(kernel_uuid_info), uuid_info_count, &out_addr));
			kernel_uuid_info *uuid_info_array = (kernel_uuid_info *)out_addr;

			image_load_address = (uintptr_t)VM_KERNEL_UNSLIDE(vm_kernel_stext);
#if defined(__arm64__)
			if (uuid_type == STACKSHOT_KCTYPE_LOADINFO64_TEXT_EXEC) {
				/* If we're reporting TEXT_EXEC load info, populate the TEXT_EXEC base instead */
				extern vm_offset_t segTEXTEXECB;
				image_load_address = (uintptr_t)VM_KERNEL_UNSLIDE(segTEXTEXECB);
			}
#endif
			uuid_info_array[0].imageLoadAddress = image_load_address;
			stackshot_memcpy(&uuid_info_array[0].imageUUID, kernel_uuid, sizeof(uuid_t));

			if (save_kextloadinfo_p &&
			    _stackshot_validate_kva((vm_offset_t)(gLoadedKextSummaries), sizeof(OSKextLoadedKextSummaryHeader)) &&
			    _stackshot_validate_kva((vm_offset_t)(&gLoadedKextSummaries->summaries[0]),
			    gLoadedKextSummaries->entry_size * gLoadedKextSummaries->numSummaries)) {
				uint32_t kexti;
				for (kexti = 0; kexti < gLoadedKextSummaries->numSummaries; kexti++) {
					image_load_address = (uintptr_t)VM_KERNEL_UNSLIDE(gLoadedKextSummaries->summaries[kexti].address);
#if defined(__arm64__)
					if (uuid_type == STACKSHOT_KCTYPE_LOADINFO64_TEXT_EXEC) {
						/* If we're reporting TEXT_EXEC load info, populate the TEXT_EXEC base instead */
						image_load_address = (uintptr_t)VM_KERNEL_UNSLIDE(gLoadedKextSummaries->summaries[kexti].text_exec_address);
					}
#endif
					uuid_info_array[kexti + 1].imageLoadAddress = image_load_address;
					stackshot_memcpy(&uuid_info_array[kexti + 1].imageUUID, &gLoadedKextSummaries->summaries[kexti].uuid, sizeof(uuid_t));
				}
			}
			kcd_exit_on_error(kcdata_compression_window_close(kcd));
		} while (0);
	}

error_exit:
	if (kdp_fault_results & KDP_FAULT_RESULT_PAGED_OUT) {
		*task_snap_ss_flags |= kTaskUUIDInfoMissing;
	}

	if (kdp_fault_results & KDP_FAULT_RESULT_TRIED_FAULT) {
		*task_snap_ss_flags |= kTaskUUIDInfoTriedFault;
	}

	if (kdp_fault_results & KDP_FAULT_RESULT_FAULTED_IN) {
		*task_snap_ss_flags |= kTaskUUIDInfoFaultedIn;
	}

	return error;
}

static kern_return_t
kcdata_record_task_iostats(kcdata_descriptor_t kcd, task_t task)
{
	kern_return_t error = KERN_SUCCESS;
	mach_vm_address_t out_addr = 0;

	/* I/O Statistics if any counters are non zero */
	assert(IO_NUM_PRIORITIES == STACKSHOT_IO_NUM_PRIORITIES);
	if (task->task_io_stats && !memory_iszero(task->task_io_stats, sizeof(struct io_stat_info))) {
		/* struct io_stats_snapshot is quite large - avoid overflowing the stack. */
		kcdata_compression_window_open(kcd);
		kcd_exit_on_error(kcdata_get_memory_addr(kcd, STACKSHOT_KCTYPE_IOSTATS, sizeof(struct io_stats_snapshot), &out_addr));
		struct io_stats_snapshot *_iostat = (struct io_stats_snapshot *)out_addr;
		_iostat->ss_disk_reads_count = task->task_io_stats->disk_reads.count;
		_iostat->ss_disk_reads_size = task->task_io_stats->disk_reads.size;
		_iostat->ss_disk_writes_count = (task->task_io_stats->total_io.count - task->task_io_stats->disk_reads.count);
		_iostat->ss_disk_writes_size = (task->task_io_stats->total_io.size - task->task_io_stats->disk_reads.size);
		_iostat->ss_paging_count = task->task_io_stats->paging.count;
		_iostat->ss_paging_size = task->task_io_stats->paging.size;
		_iostat->ss_non_paging_count = (task->task_io_stats->total_io.count - task->task_io_stats->paging.count);
		_iostat->ss_non_paging_size = (task->task_io_stats->total_io.size - task->task_io_stats->paging.size);
		_iostat->ss_metadata_count = task->task_io_stats->metadata.count;
		_iostat->ss_metadata_size = task->task_io_stats->metadata.size;
		_iostat->ss_data_count = (task->task_io_stats->total_io.count - task->task_io_stats->metadata.count);
		_iostat->ss_data_size = (task->task_io_stats->total_io.size - task->task_io_stats->metadata.size);
		for (int i = 0; i < IO_NUM_PRIORITIES; i++) {
			_iostat->ss_io_priority_count[i] = task->task_io_stats->io_priority[i].count;
			_iostat->ss_io_priority_size[i] = task->task_io_stats->io_priority[i].size;
		}
		kcd_exit_on_error(kcdata_compression_window_close(kcd));
	}


error_exit:
	return error;
}

#if MONOTONIC
static kern_return_t
kcdata_record_task_instrs_cycles(kcdata_descriptor_t kcd, task_t task)
{
	struct instrs_cycles_snapshot instrs_cycles = {0};
	uint64_t ics_instructions;
	uint64_t ics_cycles;

	mt_stackshot_task(task, &ics_instructions, &ics_cycles);
	instrs_cycles.ics_instructions = ics_instructions;
	instrs_cycles.ics_cycles = ics_cycles;

	return kcdata_push_data(kcd, STACKSHOT_KCTYPE_INSTRS_CYCLES, sizeof(instrs_cycles), &instrs_cycles);
}
#endif /* MONOTONIC */

static kern_return_t
kcdata_record_task_cpu_architecture(kcdata_descriptor_t kcd, task_t task)
{
	struct stackshot_cpu_architecture cpu_architecture = {0};
	int32_t cputype;
	int32_t cpusubtype;

	proc_archinfo_kdp(task->bsd_info, &cputype, &cpusubtype);
	cpu_architecture.cputype = cputype;
	cpu_architecture.cpusubtype = cpusubtype;

	return kcdata_push_data(kcd, STACKSHOT_KCTYPE_TASK_CPU_ARCHITECTURE, sizeof(struct stackshot_cpu_architecture), &cpu_architecture);
}

static kern_return_t
kcdata_record_transitioning_task_snapshot(kcdata_descriptor_t kcd, task_t task, unaligned_u64 task_snap_ss_flags, uint64_t transition_type)
{
	kern_return_t error                 = KERN_SUCCESS;
	mach_vm_address_t out_addr          = 0;
	struct transitioning_task_snapshot * cur_tsnap = NULL;

	int task_pid           = pid_from_task(task);
	/* Is returning -1 ok for terminating task ok ??? */
	uint64_t task_uniqueid = get_task_uniqueid(task);

	if (task_pid && (task_did_exec_internal(task) || task_is_exec_copy_internal(task))) {
		/*
		 * if this task is a transit task from another one, show the pid as
		 * negative
		 */
		task_pid = 0 - task_pid;
	}

	/* the task_snapshot_v2 struct is large - avoid overflowing the stack */
	kcdata_compression_window_open(kcd);
	kcd_exit_on_error(kcdata_get_memory_addr(kcd, STACKSHOT_KCTYPE_TRANSITIONING_TASK_SNAPSHOT, sizeof(struct transitioning_task_snapshot), &out_addr));
	cur_tsnap = (struct transitioning_task_snapshot *)out_addr;
	bzero(cur_tsnap, sizeof(*cur_tsnap));

	cur_tsnap->tts_unique_pid = task_uniqueid;
	cur_tsnap->tts_ss_flags = kcdata_get_task_ss_flags(task);
	cur_tsnap->tts_ss_flags |= task_snap_ss_flags;
	cur_tsnap->tts_transition_type = transition_type;
	cur_tsnap->tts_pid = task_pid;

	/* Add the BSD process identifiers */
	if (task_pid != -1 && task->bsd_info != NULL) {
		proc_name_kdp(task->bsd_info, cur_tsnap->tts_p_comm, sizeof(cur_tsnap->tts_p_comm));
	} else {
		cur_tsnap->tts_p_comm[0] = '\0';
	}

	kcd_exit_on_error(kcdata_compression_window_close(kcd));

error_exit:
	return error;
}

static kern_return_t
#if STACKSHOT_COLLECTS_LATENCY_INFO
kcdata_record_task_snapshot(kcdata_descriptor_t kcd, task_t task, uint64_t trace_flags, boolean_t have_pmap, unaligned_u64 task_snap_ss_flags, struct stackshot_latency_task *latency_info)
#else
kcdata_record_task_snapshot(kcdata_descriptor_t kcd, task_t task, uint64_t trace_flags, boolean_t have_pmap, unaligned_u64 task_snap_ss_flags)
#endif /* STACKSHOT_COLLECTS_LATENCY_INFO */
{
	boolean_t collect_delta_stackshot = ((trace_flags & STACKSHOT_COLLECT_DELTA_SNAPSHOT) != 0);
	boolean_t collect_iostats         = !collect_delta_stackshot && !(trace_flags & STACKSHOT_NO_IO_STATS);
#if MONOTONIC
	boolean_t collect_instrs_cycles   = ((trace_flags & STACKSHOT_INSTRS_CYCLES) != 0);
#endif /* MONOTONIC */
#if __arm__ || __arm64__
	boolean_t collect_asid            = ((trace_flags & STACKSHOT_ASID) != 0);
#endif
	boolean_t collect_pagetables       = ((trace_flags & STACKSHOT_PAGE_TABLES) != 0);


	kern_return_t error                 = KERN_SUCCESS;
	mach_vm_address_t out_addr          = 0;
	struct task_snapshot_v2 * cur_tsnap = NULL;
#if STACKSHOT_COLLECTS_LATENCY_INFO
	latency_info->cur_tsnap_latency = mach_absolute_time();
#endif /* STACKSHOT_COLLECTS_LATENCY_INFO */

	int task_pid           = pid_from_task(task);
	uint64_t task_uniqueid = get_task_uniqueid(task);
	uint64_t proc_starttime_secs = 0;

	if (task_pid && (task_did_exec_internal(task) || task_is_exec_copy_internal(task))) {
		/*
		 * if this task is a transit task from another one, show the pid as
		 * negative
		 */
		task_pid = 0 - task_pid;
	}

	/* the task_snapshot_v2 struct is large - avoid overflowing the stack */
	kcdata_compression_window_open(kcd);
	kcd_exit_on_error(kcdata_get_memory_addr(kcd, STACKSHOT_KCTYPE_TASK_SNAPSHOT, sizeof(struct task_snapshot_v2), &out_addr));
	cur_tsnap = (struct task_snapshot_v2 *)out_addr;
	bzero(cur_tsnap, sizeof(*cur_tsnap));

	cur_tsnap->ts_unique_pid = task_uniqueid;
	cur_tsnap->ts_ss_flags = kcdata_get_task_ss_flags(task);
	cur_tsnap->ts_ss_flags |= task_snap_ss_flags;
	cur_tsnap->ts_user_time_in_terminated_threads = task->total_user_time;
	cur_tsnap->ts_system_time_in_terminated_threads = task->total_system_time;

	proc_starttime_kdp(task->bsd_info, &proc_starttime_secs, NULL, NULL);
	cur_tsnap->ts_p_start_sec = proc_starttime_secs;
	cur_tsnap->ts_task_size = have_pmap ? get_task_phys_footprint(task) : 0;
	cur_tsnap->ts_max_resident_size = get_task_resident_max(task);
	cur_tsnap->ts_was_throttled = (uint32_t) proc_was_throttled_from_task(task);
	cur_tsnap->ts_did_throttle = (uint32_t) proc_did_throttle_from_task(task);

	cur_tsnap->ts_suspend_count = task->suspend_count;
	cur_tsnap->ts_faults = counter_load(&task->faults);
	cur_tsnap->ts_pageins = counter_load(&task->pageins);
	cur_tsnap->ts_cow_faults = counter_load(&task->cow_faults);
	cur_tsnap->ts_latency_qos = (task->effective_policy.tep_latency_qos == LATENCY_QOS_TIER_UNSPECIFIED) ?
	    LATENCY_QOS_TIER_UNSPECIFIED : ((0xFF << 16) | task->effective_policy.tep_latency_qos);
	cur_tsnap->ts_pid = task_pid;

	/* Add the BSD process identifiers */
	if (task_pid != -1 && task->bsd_info != NULL) {
		proc_name_kdp(task->bsd_info, cur_tsnap->ts_p_comm, sizeof(cur_tsnap->ts_p_comm));
	} else {
		cur_tsnap->ts_p_comm[0] = '\0';
#if IMPORTANCE_INHERITANCE && (DEVELOPMENT || DEBUG)
		if (task->task_imp_base != NULL) {
			_stackshot_strlcpy(cur_tsnap->ts_p_comm, &task->task_imp_base->iit_procname[0],
			    MIN((int)sizeof(task->task_imp_base->iit_procname), (int)sizeof(cur_tsnap->ts_p_comm)));
		}
#endif /* IMPORTANCE_INHERITANCE && (DEVELOPMENT || DEBUG) */
	}

	kcd_exit_on_error(kcdata_compression_window_close(kcd));

#if CONFIG_COALITIONS
	if (task_pid != -1 && task->bsd_info != NULL &&
	    ((trace_flags & STACKSHOT_SAVE_JETSAM_COALITIONS) && (task->coalition[COALITION_TYPE_JETSAM] != NULL))) {
		uint64_t jetsam_coal_id = coalition_id(task->coalition[COALITION_TYPE_JETSAM]);
		kcd_exit_on_error(kcdata_push_data(kcd, STACKSHOT_KCTYPE_JETSAM_COALITION, sizeof(jetsam_coal_id), &jetsam_coal_id));
	}
#endif /* CONFIG_COALITIONS */

#if __arm__ || __arm64__
	if (collect_asid && have_pmap) {
		uint32_t asid = PMAP_VASID(task->map->pmap);
		kcd_exit_on_error(kcdata_push_data(kcd, STACKSHOT_KCTYPE_ASID, sizeof(asid), &asid));
	}
#endif

#if STACKSHOT_COLLECTS_LATENCY_INFO
	latency_info->cur_tsnap_latency = mach_absolute_time() - latency_info->cur_tsnap_latency;
	latency_info->pmap_latency = mach_absolute_time();
#endif /* STACKSHOT_COLLECTS_LATENCY_INFO */

	if (collect_pagetables && have_pmap) {
#if INTERRUPT_MASKED_DEBUG
		// pagetable dumps can be large; reset the interrupt timeout to avoid a panic
		ml_spin_debug_clear_self();
#endif
		size_t bytes_dumped = 0;
		error = pmap_dump_page_tables(task->map->pmap, kcd_end_address(kcd), kcd_max_address(kcd), stack_snapshot_pagetable_mask, &bytes_dumped);
		if (error != KERN_SUCCESS) {
			goto error_exit;
		} else {
			/* Variable size array - better not have it on the stack. */
			kcdata_compression_window_open(kcd);
			kcd_exit_on_error(kcdata_get_memory_addr_for_array(kcd, STACKSHOT_KCTYPE_PAGE_TABLES,
			    sizeof(uint64_t), (uint32_t)(bytes_dumped / sizeof(uint64_t)), &out_addr));
			kcd_exit_on_error(kcdata_compression_window_close(kcd));
		}
	}

#if STACKSHOT_COLLECTS_LATENCY_INFO
	latency_info->pmap_latency = mach_absolute_time() - latency_info->pmap_latency;
	latency_info->bsd_proc_ids_latency = mach_absolute_time();
#endif /* STACKSHOT_COLLECTS_LATENCY_INFO */

#if STACKSHOT_COLLECTS_LATENCY_INFO
	latency_info->bsd_proc_ids_latency = mach_absolute_time() - latency_info->bsd_proc_ids_latency;
	latency_info->end_latency = mach_absolute_time();
#endif /* STACKSHOT_COLLECTS_LATENCY_INFO */

	if (collect_iostats) {
		kcd_exit_on_error(kcdata_record_task_iostats(kcd, task));
	}

#if MONOTONIC
	if (collect_instrs_cycles) {
		kcd_exit_on_error(kcdata_record_task_instrs_cycles(kcd, task));
	}
#endif /* MONOTONIC */

	kcd_exit_on_error(kcdata_record_task_cpu_architecture(kcd, task));

#if STACKSHOT_COLLECTS_LATENCY_INFO
	latency_info->end_latency = mach_absolute_time() - latency_info->end_latency;
#endif /* STACKSHOT_COLLECTS_LATENCY_INFO */

error_exit:
	return error;
}

static kern_return_t
kcdata_record_task_delta_snapshot(kcdata_descriptor_t kcd, task_t task, uint64_t trace_flags, boolean_t have_pmap, unaligned_u64 task_snap_ss_flags)
{
#if !MONOTONIC
#pragma unused(trace_flags)
#endif /* !MONOTONIC */
	kern_return_t error                       = KERN_SUCCESS;
	struct task_delta_snapshot_v2 * cur_tsnap = NULL;
	mach_vm_address_t out_addr                = 0;
	(void) trace_flags;
#if __arm__ || __arm64__
	boolean_t collect_asid                    = ((trace_flags & STACKSHOT_ASID) != 0);
#endif
#if MONOTONIC
	boolean_t collect_instrs_cycles           = ((trace_flags & STACKSHOT_INSTRS_CYCLES) != 0);
#endif /* MONOTONIC */

	uint64_t task_uniqueid = get_task_uniqueid(task);

	kcd_exit_on_error(kcdata_get_memory_addr(kcd, STACKSHOT_KCTYPE_TASK_DELTA_SNAPSHOT, sizeof(struct task_delta_snapshot_v2), &out_addr));

	cur_tsnap = (struct task_delta_snapshot_v2 *)out_addr;

	cur_tsnap->tds_unique_pid = task_uniqueid;
	cur_tsnap->tds_ss_flags = kcdata_get_task_ss_flags(task);
	cur_tsnap->tds_ss_flags |= task_snap_ss_flags;

	cur_tsnap->tds_user_time_in_terminated_threads = task->total_user_time;
	cur_tsnap->tds_system_time_in_terminated_threads = task->total_system_time;

	cur_tsnap->tds_task_size = have_pmap ? get_task_phys_footprint(task) : 0;

	cur_tsnap->tds_max_resident_size = get_task_resident_max(task);
	cur_tsnap->tds_suspend_count = task->suspend_count;
	cur_tsnap->tds_faults            = counter_load(&task->faults);
	cur_tsnap->tds_pageins           = counter_load(&task->pageins);
	cur_tsnap->tds_cow_faults        = counter_load(&task->cow_faults);
	cur_tsnap->tds_was_throttled     = (uint32_t)proc_was_throttled_from_task(task);
	cur_tsnap->tds_did_throttle      = (uint32_t)proc_did_throttle_from_task(task);
	cur_tsnap->tds_latency_qos       = (task->effective_policy.tep_latency_qos == LATENCY_QOS_TIER_UNSPECIFIED)
	    ? LATENCY_QOS_TIER_UNSPECIFIED
	    : ((0xFF << 16) | task->effective_policy.tep_latency_qos);

#if __arm__ || __arm64__
	if (collect_asid && have_pmap) {
		uint32_t asid = PMAP_VASID(task->map->pmap);
		kcd_exit_on_error(kcdata_get_memory_addr(kcd, STACKSHOT_KCTYPE_ASID, sizeof(uint32_t), &out_addr));
		stackshot_memcpy((void*)out_addr, &asid, sizeof(asid));
	}
#endif

#if MONOTONIC
	if (collect_instrs_cycles) {
		kcd_exit_on_error(kcdata_record_task_instrs_cycles(kcd, task));
	}
#endif /* MONOTONIC */

error_exit:
	return error;
}

static kern_return_t
kcdata_record_thread_iostats(kcdata_descriptor_t kcd, thread_t thread)
{
	kern_return_t error = KERN_SUCCESS;
	mach_vm_address_t out_addr = 0;

	/* I/O Statistics */
	assert(IO_NUM_PRIORITIES == STACKSHOT_IO_NUM_PRIORITIES);
	if (thread->thread_io_stats && !memory_iszero(thread->thread_io_stats, sizeof(struct io_stat_info))) {
		kcd_exit_on_error(kcdata_get_memory_addr(kcd, STACKSHOT_KCTYPE_IOSTATS, sizeof(struct io_stats_snapshot), &out_addr));
		struct io_stats_snapshot *_iostat = (struct io_stats_snapshot *)out_addr;
		_iostat->ss_disk_reads_count = thread->thread_io_stats->disk_reads.count;
		_iostat->ss_disk_reads_size = thread->thread_io_stats->disk_reads.size;
		_iostat->ss_disk_writes_count = (thread->thread_io_stats->total_io.count - thread->thread_io_stats->disk_reads.count);
		_iostat->ss_disk_writes_size = (thread->thread_io_stats->total_io.size - thread->thread_io_stats->disk_reads.size);
		_iostat->ss_paging_count = thread->thread_io_stats->paging.count;
		_iostat->ss_paging_size = thread->thread_io_stats->paging.size;
		_iostat->ss_non_paging_count = (thread->thread_io_stats->total_io.count - thread->thread_io_stats->paging.count);
		_iostat->ss_non_paging_size = (thread->thread_io_stats->total_io.size - thread->thread_io_stats->paging.size);
		_iostat->ss_metadata_count = thread->thread_io_stats->metadata.count;
		_iostat->ss_metadata_size = thread->thread_io_stats->metadata.size;
		_iostat->ss_data_count = (thread->thread_io_stats->total_io.count - thread->thread_io_stats->metadata.count);
		_iostat->ss_data_size = (thread->thread_io_stats->total_io.size - thread->thread_io_stats->metadata.size);
		for (int i = 0; i < IO_NUM_PRIORITIES; i++) {
			_iostat->ss_io_priority_count[i] = thread->thread_io_stats->io_priority[i].count;
			_iostat->ss_io_priority_size[i] = thread->thread_io_stats->io_priority[i].size;
		}
	}

error_exit:
	return error;
}

bool
machine_trace_thread_validate_kva(vm_offset_t addr)
{
	return _stackshot_validate_kva(addr, sizeof(uintptr_t));
}

struct _stackshot_backtrace_context {
	vm_map_t sbc_map;
	vm_offset_t sbc_prev_page;
	vm_offset_t sbc_prev_kva;
	uint32_t sbc_flags;
	bool sbc_allow_faulting;
};

static errno_t
_stackshot_backtrace_copy(void *vctx, void *dst, user_addr_t src, size_t size)
{
	struct _stackshot_backtrace_context *ctx = vctx;
	size_t map_page_mask = 0;
	size_t __assert_only map_page_size = _stackshot_get_page_size(ctx->sbc_map,
	    &map_page_mask);
	assert(size < map_page_size);
	if (src & (size - 1)) {
		// The source should be aligned to the size passed in, like a stack
		// frame or word.
		return EINVAL;
	}

	vm_offset_t src_page = src & ~map_page_mask;
	vm_offset_t src_kva = 0;

	if (src_page != ctx->sbc_prev_page) {
		uint32_t res = 0;
		uint32_t flags = 0;
		vm_offset_t src_pa = kdp_find_phys(ctx->sbc_map, src,
		    ctx->sbc_allow_faulting, &res);

		flags |= (res & KDP_FAULT_RESULT_PAGED_OUT) ? kThreadTruncatedBT : 0;
		flags |= (res & KDP_FAULT_RESULT_TRIED_FAULT) ? kThreadTriedFaultBT : 0;
		flags |= (res & KDP_FAULT_RESULT_FAULTED_IN) ? kThreadFaultedBT : 0;
		ctx->sbc_flags |= flags;
		if (src_pa == 0) {
			return EFAULT;
		}

		src_kva = phystokv(src_pa);
		ctx->sbc_prev_page = src_page;
		ctx->sbc_prev_kva = (src_kva & ~map_page_mask);
	} else {
		src_kva = ctx->sbc_prev_kva + (src & map_page_mask);
	}

#if KASAN
	kasan_notify_address(src_kva, size);
#endif
	memcpy(dst, (const void *)src_kva, size);

	return 0;
}

static kern_return_t
kcdata_record_thread_snapshot(
	kcdata_descriptor_t kcd, thread_t thread, task_t task, uint64_t trace_flags, boolean_t have_pmap, boolean_t thread_on_core)
{
	boolean_t dispatch_p              = ((trace_flags & STACKSHOT_GET_DQ) != 0);
	boolean_t active_kthreads_only_p  = ((trace_flags & STACKSHOT_ACTIVE_KERNEL_THREADS_ONLY) != 0);
	boolean_t collect_delta_stackshot = ((trace_flags & STACKSHOT_COLLECT_DELTA_SNAPSHOT) != 0);
	boolean_t collect_iostats         = !collect_delta_stackshot && !(trace_flags & STACKSHOT_NO_IO_STATS);
#if MONOTONIC
	boolean_t collect_instrs_cycles   = ((trace_flags & STACKSHOT_INSTRS_CYCLES) != 0);
#endif /* MONOTONIC */
	kern_return_t error        = KERN_SUCCESS;

#if STACKSHOT_COLLECTS_LATENCY_INFO
	struct stackshot_latency_thread latency_info;
	latency_info.cur_thsnap1_latency = mach_absolute_time();
#endif /* STACKSHOT_COLLECTS_LATENCY_INFO */

	mach_vm_address_t out_addr = 0;
	int saved_count            = 0;

	struct thread_snapshot_v4 * cur_thread_snap = NULL;
	char cur_thread_name[STACKSHOT_MAX_THREAD_NAME_SIZE];
	uint64_t tval    = 0;

	kcd_exit_on_error(kcdata_get_memory_addr(kcd, STACKSHOT_KCTYPE_THREAD_SNAPSHOT, sizeof(struct thread_snapshot_v4), &out_addr));
	cur_thread_snap = (struct thread_snapshot_v4 *)out_addr;

	/* Populate the thread snapshot header */
	cur_thread_snap->ths_ss_flags = 0;
	cur_thread_snap->ths_thread_id = thread_tid(thread);
	cur_thread_snap->ths_wait_event = VM_KERNEL_UNSLIDE_OR_PERM(thread->wait_event);
	cur_thread_snap->ths_continuation = VM_KERNEL_UNSLIDE(thread->continuation);
	cur_thread_snap->ths_total_syscalls = thread->syscalls_mach + thread->syscalls_unix;

	if (IPC_VOUCHER_NULL != thread->ith_voucher) {
		cur_thread_snap->ths_voucher_identifier = VM_KERNEL_ADDRPERM(thread->ith_voucher);
	} else {
		cur_thread_snap->ths_voucher_identifier = 0;
	}

#if STACKSHOT_COLLECTS_LATENCY_INFO
	latency_info.cur_thsnap1_latency = mach_absolute_time() - latency_info.cur_thsnap1_latency;
	latency_info.dispatch_serial_latency = mach_absolute_time();
	latency_info.dispatch_label_latency = 0;
#endif /* STACKSHOT_COLLECTS_LATENCY_INFO */

	cur_thread_snap->ths_dqserialnum = 0;
	if (dispatch_p && (task != kernel_task) && (task->active) && have_pmap) {
		uint64_t dqkeyaddr = thread_dispatchqaddr(thread);
		if (dqkeyaddr != 0) {
			uint64_t dqaddr = 0;
			boolean_t copyin_ok = kdp_copyin_word(task, dqkeyaddr, &dqaddr, FALSE, NULL);
			if (copyin_ok && dqaddr != 0) {
				uint64_t dqserialnumaddr = dqaddr + get_task_dispatchqueue_serialno_offset(task);
				uint64_t dqserialnum = 0;
				copyin_ok = kdp_copyin_word(task, dqserialnumaddr, &dqserialnum, FALSE, NULL);
				if (copyin_ok) {
					cur_thread_snap->ths_ss_flags |= kHasDispatchSerial;
					cur_thread_snap->ths_dqserialnum = dqserialnum;
				}

#if STACKSHOT_COLLECTS_LATENCY_INFO
				latency_info.dispatch_serial_latency = mach_absolute_time() - latency_info.dispatch_serial_latency;
				latency_info.dispatch_label_latency = mach_absolute_time();
#endif /* STACKSHOT_COLLECTS_LATENCY_INFO */

				/* try copying in the queue label */
				uint64_t label_offs = get_task_dispatchqueue_label_offset(task);
				if (label_offs) {
					uint64_t dqlabeladdr = dqaddr + label_offs;
					uint64_t actual_dqlabeladdr = 0;

					copyin_ok = kdp_copyin_word(task, dqlabeladdr, &actual_dqlabeladdr, FALSE, NULL);
					if (copyin_ok && actual_dqlabeladdr != 0) {
						char label_buf[STACKSHOT_QUEUE_LABEL_MAXSIZE];
						int len;

						bzero(label_buf, STACKSHOT_QUEUE_LABEL_MAXSIZE * sizeof(char));
						len = kdp_copyin_string(task, actual_dqlabeladdr, label_buf, STACKSHOT_QUEUE_LABEL_MAXSIZE, FALSE, NULL);
						if (len > 0) {
							mach_vm_address_t label_addr = 0;
							kcd_exit_on_error(kcdata_get_memory_addr(kcd, STACKSHOT_KCTYPE_THREAD_DISPATCH_QUEUE_LABEL, len, &label_addr));
							_stackshot_strlcpy((char*)label_addr, &label_buf[0], len);
						}
					}
				}
#if STACKSHOT_COLLECTS_LATENCY_INFO
				latency_info.dispatch_label_latency = mach_absolute_time() - latency_info.dispatch_label_latency;
#endif /* STACKSHOT_COLLECTS_LATENCY_INFO */
			}
		}
	}

#if STACKSHOT_COLLECTS_LATENCY_INFO
	if ((cur_thread_snap->ths_ss_flags & kHasDispatchSerial) == 0) {
		latency_info.dispatch_serial_latency = 0;
	}
	latency_info.cur_thsnap2_latency = mach_absolute_time();
#endif /* STACKSHOT_COLLECTS_LATENCY_INFO */

	tval = safe_grab_timer_value(&thread->user_timer);
	cur_thread_snap->ths_user_time = tval;
	tval = safe_grab_timer_value(&thread->system_timer);

	if (thread->precise_user_kernel_time) {
		cur_thread_snap->ths_sys_time = tval;
	} else {
		cur_thread_snap->ths_user_time += tval;
		cur_thread_snap->ths_sys_time = 0;
	}

	if (thread->thread_tag & THREAD_TAG_MAINTHREAD) {
		cur_thread_snap->ths_ss_flags |= kThreadMain;
	}
	if (thread->effective_policy.thep_darwinbg) {
		cur_thread_snap->ths_ss_flags |= kThreadDarwinBG;
	}
	if (proc_get_effective_thread_policy(thread, TASK_POLICY_PASSIVE_IO)) {
		cur_thread_snap->ths_ss_flags |= kThreadIOPassive;
	}
	if (thread->suspend_count > 0) {
		cur_thread_snap->ths_ss_flags |= kThreadSuspended;
	}
	if (thread->options & TH_OPT_GLOBAL_FORCED_IDLE) {
		cur_thread_snap->ths_ss_flags |= kGlobalForcedIdle;
	}
	if (thread_on_core) {
		cur_thread_snap->ths_ss_flags |= kThreadOnCore;
	}
	if (stackshot_thread_is_idle_worker_unsafe(thread)) {
		cur_thread_snap->ths_ss_flags |= kThreadIdleWorker;
	}

	/* make sure state flags defined in kcdata.h still match internal flags */
	static_assert(SS_TH_WAIT == TH_WAIT);
	static_assert(SS_TH_SUSP == TH_SUSP);
	static_assert(SS_TH_RUN == TH_RUN);
	static_assert(SS_TH_UNINT == TH_UNINT);
	static_assert(SS_TH_TERMINATE == TH_TERMINATE);
	static_assert(SS_TH_TERMINATE2 == TH_TERMINATE2);
	static_assert(SS_TH_IDLE == TH_IDLE);

	cur_thread_snap->ths_last_run_time           = thread->last_run_time;
	cur_thread_snap->ths_last_made_runnable_time = thread->last_made_runnable_time;
	cur_thread_snap->ths_state                   = thread->state;
	cur_thread_snap->ths_sched_flags             = thread->sched_flags;
	cur_thread_snap->ths_base_priority = thread->base_pri;
	cur_thread_snap->ths_sched_priority = thread->sched_pri;
	cur_thread_snap->ths_eqos = thread->effective_policy.thep_qos;
	cur_thread_snap->ths_rqos = thread->requested_policy.thrp_qos;
	cur_thread_snap->ths_rqos_override = MAX(thread->requested_policy.thrp_qos_override,
	    thread->requested_policy.thrp_qos_workq_override);
	cur_thread_snap->ths_io_tier = (uint8_t) proc_get_effective_thread_policy(thread, TASK_POLICY_IO);
	cur_thread_snap->ths_thread_t = VM_KERNEL_UNSLIDE_OR_PERM(thread);

	static_assert(sizeof(thread->effective_policy) == sizeof(uint64_t));
	static_assert(sizeof(thread->requested_policy) == sizeof(uint64_t));
	cur_thread_snap->ths_requested_policy = *(unaligned_u64 *) &thread->requested_policy;
	cur_thread_snap->ths_effective_policy = *(unaligned_u64 *) &thread->effective_policy;

#if STACKSHOT_COLLECTS_LATENCY_INFO
	latency_info.cur_thsnap2_latency = mach_absolute_time()  - latency_info.cur_thsnap2_latency;
	latency_info.thread_name_latency = mach_absolute_time();
#endif /* STACKSHOT_COLLECTS_LATENCY_INFO */

	/* if there is thread name then add to buffer */
	cur_thread_name[0] = '\0';
	proc_threadname_kdp(get_bsdthread_info(thread), cur_thread_name, STACKSHOT_MAX_THREAD_NAME_SIZE);
	if (strnlen(cur_thread_name, STACKSHOT_MAX_THREAD_NAME_SIZE) > 0) {
		kcd_exit_on_error(kcdata_get_memory_addr(kcd, STACKSHOT_KCTYPE_THREAD_NAME, sizeof(cur_thread_name), &out_addr));
		stackshot_memcpy((void *)out_addr, (void *)cur_thread_name, sizeof(cur_thread_name));
	}

#if STACKSHOT_COLLECTS_LATENCY_INFO
	latency_info.thread_name_latency = mach_absolute_time()  - latency_info.thread_name_latency;
	latency_info.sur_times_latency = mach_absolute_time();
#endif /* STACKSHOT_COLLECTS_LATENCY_INFO */

	/* record system, user, and runnable times */
	time_value_t user_time, system_time, runnable_time;
	thread_read_times(thread, &user_time, &system_time, &runnable_time);
	kcd_exit_on_error(kcdata_get_memory_addr(kcd, STACKSHOT_KCTYPE_CPU_TIMES, sizeof(struct stackshot_cpu_times_v2), &out_addr));
	struct stackshot_cpu_times_v2 *stackshot_cpu_times = (struct stackshot_cpu_times_v2 *)out_addr;
	*stackshot_cpu_times = (struct stackshot_cpu_times_v2){
		.user_usec = (uint64_t)user_time.seconds * USEC_PER_SEC + user_time.microseconds,
		.system_usec = (uint64_t)system_time.seconds * USEC_PER_SEC + system_time.microseconds,
		.runnable_usec = (uint64_t)runnable_time.seconds * USEC_PER_SEC + runnable_time.microseconds,
	};

#if STACKSHOT_COLLECTS_LATENCY_INFO
	latency_info.sur_times_latency = mach_absolute_time()  - latency_info.sur_times_latency;
	latency_info.user_stack_latency = mach_absolute_time();
#endif /* STACKSHOT_COLLECTS_LATENCY_INFO */

	/* Trace user stack, if any */
	if (!active_kthreads_only_p && task->active && task->map != kernel_map) {
		uint32_t user_ths_ss_flags = 0;

		/*
		 * This relies on knowing the "end" address points to the start of the
		 * next elements data and, in the case of arrays, the elements.
		 */
		out_addr = (mach_vm_address_t)kcd_end_address(kcd);
		mach_vm_address_t max_addr = (mach_vm_address_t)kcd_max_address(kcd);
		assert(out_addr <= max_addr);
		size_t avail_frames = (max_addr - out_addr) / sizeof(uintptr_t);
		size_t max_frames = MIN(avail_frames, MAX_FRAMES);
		if (max_frames == 0) {
			error = KERN_RESOURCE_SHORTAGE;
			goto error_exit;
		}
		struct _stackshot_backtrace_context ctx = {
			.sbc_map = task->map,
			.sbc_allow_faulting = stack_enable_faulting,
			.sbc_prev_page = -1,
			.sbc_prev_kva = -1,
		};
		struct backtrace_control ctl = {
			.btc_user_thread = thread,
			.btc_user_copy = _stackshot_backtrace_copy,
			.btc_user_copy_context = &ctx,
		};
		struct backtrace_user_info info = BTUINFO_INIT;

		saved_count = backtrace_user((uintptr_t *)out_addr, max_frames, &ctl,
		    &info);
		if (saved_count > 0) {
#if __LP64__
#define STACKLR_WORDS STACKSHOT_KCTYPE_USER_STACKLR64
#else // __LP64__
#define STACKLR_WORDS STACKSHOT_KCTYPE_USER_STACKLR
#endif // !__LP64__
			mach_vm_address_t out_addr_array;
			kcd_exit_on_error(kcdata_get_memory_addr_for_array(kcd,
			    STACKLR_WORDS, sizeof(uintptr_t), saved_count,
			    &out_addr_array));
			/*
			 * Ensure the kcd_end_address (above) trick worked.
			 */
			assert(out_addr == out_addr_array);
			if (info.btui_info & BTI_64_BIT) {
				user_ths_ss_flags |= kUser64_p;
			}
			if ((info.btui_info & BTI_TRUNCATED) ||
			    (ctx.sbc_flags & kThreadTruncatedBT)) {
				user_ths_ss_flags |= kThreadTruncatedBT;
				user_ths_ss_flags |= kThreadTruncUserBT;
			}
			user_ths_ss_flags |= ctx.sbc_flags;
			ctx.sbc_flags = 0;
#if __LP64__
			/* We only support async stacks on 64-bit kernels */
			if (info.btui_async_frame_addr != 0) {
				uint32_t async_start_offset = info.btui_async_start_index;
				kcd_exit_on_error(kcdata_push_data(kcd, STACKSHOT_KCTYPE_USER_ASYNC_START_INDEX,
				    sizeof(async_start_offset), &async_start_offset));
				out_addr = (mach_vm_address_t)kcd_end_address(kcd);
				assert(out_addr <= max_addr);

				avail_frames = (max_addr - out_addr) / sizeof(uintptr_t);
				max_frames = MIN(avail_frames, MAX_FRAMES);
				if (max_frames == 0) {
					error = KERN_RESOURCE_SHORTAGE;
					goto error_exit;
				}
				ctl.btc_frame_addr = info.btui_async_frame_addr;
				ctl.btc_addr_offset = BTCTL_ASYNC_ADDR_OFFSET;
				info = BTUINFO_INIT;
				unsigned int async_count = backtrace_user((uintptr_t *)out_addr, max_frames, &ctl,
				    &info);
				if (async_count > 0) {
					mach_vm_address_t async_out_addr;
					kcd_exit_on_error(kcdata_get_memory_addr_for_array(kcd,
					    STACKSHOT_KCTYPE_USER_ASYNC_STACKLR64, sizeof(uintptr_t), async_count,
					    &async_out_addr));
					/*
					 * Ensure the kcd_end_address (above) trick worked.
					 */
					assert(out_addr == async_out_addr);
					if ((info.btui_info & BTI_TRUNCATED) ||
					    (ctx.sbc_flags & kThreadTruncatedBT)) {
						user_ths_ss_flags |= kThreadTruncatedBT;
						user_ths_ss_flags |= kThreadTruncUserAsyncBT;
					}
					user_ths_ss_flags |= ctx.sbc_flags;
				}
			}
#endif /* _LP64 */
		}
		if (user_ths_ss_flags != 0) {
			cur_thread_snap->ths_ss_flags |= user_ths_ss_flags;
		}
	}

#if STACKSHOT_COLLECTS_LATENCY_INFO
	latency_info.user_stack_latency = mach_absolute_time()  - latency_info.user_stack_latency;
	latency_info.kernel_stack_latency = mach_absolute_time();
#endif /* STACKSHOT_COLLECTS_LATENCY_INFO */

	/* Call through to the machine specific trace routines
	 * Frames are added past the snapshot header.
	 */
	if (thread->kernel_stack != 0) {
		uint32_t kern_ths_ss_flags = 0;
		out_addr = (mach_vm_address_t)kcd_end_address(kcd);
#if defined(__LP64__)
		uint32_t stack_kcdata_type = STACKSHOT_KCTYPE_KERN_STACKLR64;
		extern int machine_trace_thread64(thread_t thread, char *tracepos,
		    char *tracebound, int nframes, uint32_t *thread_trace_flags);
		saved_count = machine_trace_thread64(
#else
		uint32_t stack_kcdata_type = STACKSHOT_KCTYPE_KERN_STACKLR;
		extern int machine_trace_thread(thread_t thread, char *tracepos,
		    char *tracebound, int nframes, uint32_t *thread_trace_flags);
		saved_count = machine_trace_thread(
#endif
			thread, (char *)out_addr, (char *)kcd_max_address(kcd), MAX_FRAMES,
			&kern_ths_ss_flags);
		if (saved_count > 0) {
			int frame_size = sizeof(uintptr_t);
#if defined(__LP64__)
			cur_thread_snap->ths_ss_flags |= kKernel64_p;
#endif
			kcd_exit_on_error(kcdata_get_memory_addr_for_array(kcd, stack_kcdata_type,
			    frame_size, saved_count / frame_size, &out_addr));
		}
		if (kern_ths_ss_flags & kThreadTruncatedBT) {
			kern_ths_ss_flags |= kThreadTruncKernBT;
		}
		if (kern_ths_ss_flags != 0) {
			cur_thread_snap->ths_ss_flags |= kern_ths_ss_flags;
		}
	}

#if STACKSHOT_COLLECTS_LATENCY_INFO
	latency_info.kernel_stack_latency = mach_absolute_time()  - latency_info.kernel_stack_latency;
	latency_info.misc_latency = mach_absolute_time();
#endif /* STACKSHOT_COLLECTS_LATENCY_INFO */

#if CONFIG_THREAD_GROUPS
	if (trace_flags & STACKSHOT_THREAD_GROUP) {
		uint64_t thread_group_id = thread->thread_group ? thread_group_get_id(thread->thread_group) : 0;
		kcd_exit_on_error(kcdata_get_memory_addr(kcd, STACKSHOT_KCTYPE_THREAD_GROUP, sizeof(thread_group_id), &out_addr));
		stackshot_memcpy((void*)out_addr, &thread_group_id, sizeof(uint64_t));
	}
#endif /* CONFIG_THREAD_GROUPS */

	if (collect_iostats) {
		kcd_exit_on_error(kcdata_record_thread_iostats(kcd, thread));
	}

#if MONOTONIC
	if (collect_instrs_cycles) {
		uint64_t instrs = 0, cycles = 0;
		mt_stackshot_thread(thread, &instrs, &cycles);

		kcd_exit_on_error(kcdata_get_memory_addr(kcd, STACKSHOT_KCTYPE_INSTRS_CYCLES, sizeof(struct instrs_cycles_snapshot), &out_addr));
		struct instrs_cycles_snapshot *instrs_cycles = (struct instrs_cycles_snapshot *)out_addr;
		    instrs_cycles->ics_instructions = instrs;
		    instrs_cycles->ics_cycles = cycles;
	}
#endif /* MONOTONIC */

#if STACKSHOT_COLLECTS_LATENCY_INFO
	latency_info.misc_latency = mach_absolute_time() - latency_info.misc_latency;
	if (collect_latency_info) {
		kcd_exit_on_error(kcdata_push_data(kcd, STACKSHOT_KCTYPE_LATENCY_INFO_THREAD, sizeof(latency_info), &latency_info));
	}
#endif /* STACKSHOT_COLLECTS_LATENCY_INFO */

error_exit:
	return error;
}

static int
kcdata_record_thread_delta_snapshot(struct thread_delta_snapshot_v3 * cur_thread_snap, thread_t thread, boolean_t thread_on_core)
{
	cur_thread_snap->tds_thread_id = thread_tid(thread);
	if (IPC_VOUCHER_NULL != thread->ith_voucher) {
		cur_thread_snap->tds_voucher_identifier  = VM_KERNEL_ADDRPERM(thread->ith_voucher);
	} else {
		cur_thread_snap->tds_voucher_identifier = 0;
	}

	cur_thread_snap->tds_ss_flags = 0;
	if (thread->effective_policy.thep_darwinbg) {
		cur_thread_snap->tds_ss_flags |= kThreadDarwinBG;
	}
	if (proc_get_effective_thread_policy(thread, TASK_POLICY_PASSIVE_IO)) {
		cur_thread_snap->tds_ss_flags |= kThreadIOPassive;
	}
	if (thread->suspend_count > 0) {
		cur_thread_snap->tds_ss_flags |= kThreadSuspended;
	}
	if (thread->options & TH_OPT_GLOBAL_FORCED_IDLE) {
		cur_thread_snap->tds_ss_flags |= kGlobalForcedIdle;
	}
	if (thread_on_core) {
		cur_thread_snap->tds_ss_flags |= kThreadOnCore;
	}
	if (stackshot_thread_is_idle_worker_unsafe(thread)) {
		cur_thread_snap->tds_ss_flags |= kThreadIdleWorker;
	}

	cur_thread_snap->tds_last_made_runnable_time = thread->last_made_runnable_time;
	cur_thread_snap->tds_state                   = thread->state;
	cur_thread_snap->tds_sched_flags             = thread->sched_flags;
	cur_thread_snap->tds_base_priority           = thread->base_pri;
	cur_thread_snap->tds_sched_priority          = thread->sched_pri;
	cur_thread_snap->tds_eqos                    = thread->effective_policy.thep_qos;
	cur_thread_snap->tds_rqos                    = thread->requested_policy.thrp_qos;
	cur_thread_snap->tds_rqos_override           = MAX(thread->requested_policy.thrp_qos_override,
	    thread->requested_policy.thrp_qos_workq_override);
	cur_thread_snap->tds_io_tier                 = (uint8_t) proc_get_effective_thread_policy(thread, TASK_POLICY_IO);

	static_assert(sizeof(thread->effective_policy) == sizeof(uint64_t));
	static_assert(sizeof(thread->requested_policy) == sizeof(uint64_t));
	cur_thread_snap->tds_requested_policy = *(unaligned_u64 *) &thread->requested_policy;
	cur_thread_snap->tds_effective_policy = *(unaligned_u64 *) &thread->effective_policy;

	return 0;
}

/*
 * Why 12?  12 strikes a decent balance between allocating a large array on
 * the stack and having large kcdata item overheads for recording nonrunable
 * tasks.
 */
#define UNIQUEIDSPERFLUSH 12

struct saved_uniqueids {
	uint64_t ids[UNIQUEIDSPERFLUSH];
	unsigned count;
};

enum thread_classification {
	tc_full_snapshot,  /* take a full snapshot */
	tc_delta_snapshot, /* take a delta snapshot */
};

static enum thread_classification
classify_thread(thread_t thread, boolean_t * thread_on_core_p, boolean_t collect_delta_stackshot)
{
	processor_t last_processor = thread->last_processor;

	boolean_t thread_on_core =
	    (last_processor != PROCESSOR_NULL &&
	    (last_processor->state == PROCESSOR_SHUTDOWN || last_processor->state == PROCESSOR_RUNNING) &&
	    last_processor->active_thread == thread);

	*thread_on_core_p = thread_on_core;

	/* Capture the full thread snapshot if this is not a delta stackshot or if the thread has run subsequent to the
	 * previous full stackshot */
	if (!collect_delta_stackshot || thread_on_core || (thread->last_run_time > stack_snapshot_delta_since_timestamp)) {
		return tc_full_snapshot;
	} else {
		return tc_delta_snapshot;
	}
}

struct stackshot_context {
	int pid;
	uint64_t trace_flags;
};

static kern_return_t
kdp_stackshot_record_task(struct stackshot_context *ctx, task_t task)
{
	boolean_t active_kthreads_only_p  = ((ctx->trace_flags & STACKSHOT_ACTIVE_KERNEL_THREADS_ONLY) != 0);
	boolean_t save_donating_pids_p    = ((ctx->trace_flags & STACKSHOT_SAVE_IMP_DONATION_PIDS) != 0);
	boolean_t collect_delta_stackshot = ((ctx->trace_flags & STACKSHOT_COLLECT_DELTA_SNAPSHOT) != 0);
	boolean_t save_owner_info         = ((ctx->trace_flags & STACKSHOT_THREAD_WAITINFO) != 0);

	kern_return_t error = KERN_SUCCESS;
	mach_vm_address_t out_addr = 0;
	int saved_count = 0;

	int task_pid                   = 0;
	uint64_t task_uniqueid         = 0;
	int num_delta_thread_snapshots = 0;
	int num_waitinfo_threads       = 0;
	int num_turnstileinfo_threads  = 0;

	uint64_t task_start_abstime    = 0;
	boolean_t have_map = FALSE, have_pmap = FALSE;
	boolean_t some_thread_ran = FALSE;
	unaligned_u64 task_snap_ss_flags = 0;

#if STACKSHOT_COLLECTS_LATENCY_INFO
	struct stackshot_latency_task latency_info;
	latency_info.setup_latency = mach_absolute_time();
#endif /* STACKSHOT_COLLECTS_LATENCY_INFO */

#if INTERRUPT_MASKED_DEBUG && MONOTONIC
	uint64_t task_begin_cpu_cycle_count = 0;
	if (!panic_stackshot) {
		task_begin_cpu_cycle_count = mt_cur_cpu_cycles();
	}
#endif

	if ((task == NULL) || !_stackshot_validate_kva((vm_offset_t)task, sizeof(struct task))) {
		error = KERN_FAILURE;
		goto error_exit;
	}

	boolean_t task_in_teardown        = (task->bsd_info == NULL) || proc_in_teardown(task->bsd_info);// has P_LPEXIT set during proc_exit()
	boolean_t task_in_transition      = task_in_teardown;         // here we can add other types of transition.
	uint32_t  container_type          = (task_in_transition) ? STACKSHOT_KCCONTAINER_TRANSITIONING_TASK : STACKSHOT_KCCONTAINER_TASK;
	uint32_t  transition_type         = (task_in_teardown) ? kTaskIsTerminated : 0;

	if (task_in_transition) {
		collect_delta_stackshot = FALSE;
	}

	have_map = (task->map != NULL) && (_stackshot_validate_kva((vm_offset_t)(task->map), sizeof(struct _vm_map)));
	have_pmap = have_map && (task->map->pmap != NULL) && (_stackshot_validate_kva((vm_offset_t)(task->map->pmap), sizeof(struct pmap)));

	task_pid = pid_from_task(task);
	/* Is returning -1 ok for terminating task ok ??? */
	task_uniqueid = get_task_uniqueid(task);

	if (!task->active || task_is_a_corpse(task) || task_is_a_corpse_fork(task)) {
		/*
		 * Not interested in terminated tasks without threads.
		 */
		if (queue_empty(&task->threads) || task_pid == -1) {
			return KERN_SUCCESS;
		}
	}

	/* All PIDs should have the MSB unset */
	assert((task_pid & (1ULL << 31)) == 0);

#if STACKSHOT_COLLECTS_LATENCY_INFO
	latency_info.setup_latency = mach_absolute_time() - latency_info.setup_latency;
	latency_info.task_uniqueid = task_uniqueid;
#endif /* STACKSHOT_COLLECTS_LATENCY_INFO */

	/* Trace everything, unless a process was specified */
	if ((ctx->pid == -1) || (ctx->pid == task_pid)) {
		/* add task snapshot marker */
		kcd_exit_on_error(kcdata_add_container_marker(stackshot_kcdata_p, KCDATA_TYPE_CONTAINER_BEGIN,
		    container_type, task_uniqueid));

		if (collect_delta_stackshot) {
			/*
			 * For delta stackshots we need to know if a thread from this task has run since the
			 * previous timestamp to decide whether we're going to record a full snapshot and UUID info.
			 */
			thread_t thread = THREAD_NULL;
			queue_iterate(&task->threads, thread, thread_t, task_threads)
			{
				if ((thread == NULL) || !_stackshot_validate_kva((vm_offset_t)thread, sizeof(struct thread))) {
					error = KERN_FAILURE;
					goto error_exit;
				}

				if (active_kthreads_only_p && thread->kernel_stack == 0) {
					continue;
				}

				boolean_t thread_on_core;
				enum thread_classification thread_classification = classify_thread(thread, &thread_on_core, collect_delta_stackshot);

				switch (thread_classification) {
				case tc_full_snapshot:
					some_thread_ran = TRUE;
					break;
				case tc_delta_snapshot:
					num_delta_thread_snapshots++;
					break;
				}
			}
		}

		if (collect_delta_stackshot) {
			proc_starttime_kdp(task->bsd_info, NULL, NULL, &task_start_abstime);
		}

		/* Next record any relevant UUID info and store the task snapshot */
		if (task_in_transition ||
		    !collect_delta_stackshot ||
		    (task_start_abstime == 0) ||
		    (task_start_abstime > stack_snapshot_delta_since_timestamp) ||
		    some_thread_ran) {
			/*
			 * Collect full task information in these scenarios:
			 *
			 * 1) a full stackshot or the task is in transition
			 * 2) a delta stackshot where the task started after the previous full stackshot
			 * 3) a delta stackshot where any thread from the task has run since the previous full stackshot
			 *
			 * because the task may have exec'ed, changing its name, architecture, load info, etc
			 */

			kcd_exit_on_error(kcdata_record_shared_cache_info(stackshot_kcdata_p, task, &task_snap_ss_flags));
			kcd_exit_on_error(kcdata_record_uuid_info(stackshot_kcdata_p, task, ctx->trace_flags, have_pmap, &task_snap_ss_flags));
#if STACKSHOT_COLLECTS_LATENCY_INFO
			if (!task_in_transition) {
				kcd_exit_on_error(kcdata_record_task_snapshot(stackshot_kcdata_p, task, ctx->trace_flags, have_pmap, task_snap_ss_flags, &latency_info));
			} else {
				kcd_exit_on_error(kcdata_record_transitioning_task_snapshot(stackshot_kcdata_p, task, task_snap_ss_flags, transition_type));
			}
#else
			if (!task_in_transition) {
				kcd_exit_on_error(kcdata_record_task_snapshot(stackshot_kcdata_p, task, ctx->trace_flags, have_pmap, task_snap_ss_flags));
			} else {
				kcd_exit_on_error(kcdata_record_transitioning_task_snapshot(stackshot_kcdata_p, task, task_snap_ss_flags, transition_type));
			}
#endif /* STACKSHOT_COLLECTS_LATENCY_INFO */
		} else {
			kcd_exit_on_error(kcdata_record_task_delta_snapshot(stackshot_kcdata_p, task, ctx->trace_flags, have_pmap, task_snap_ss_flags));
		}

#if STACKSHOT_COLLECTS_LATENCY_INFO
		latency_info.misc_latency = mach_absolute_time();
#endif /* STACKSHOT_COLLECTS_LATENCY_INFO */

		struct thread_delta_snapshot_v3 * delta_snapshots = NULL;
		int current_delta_snapshot_index                  = 0;
		if (num_delta_thread_snapshots > 0) {
			kcd_exit_on_error(kcdata_get_memory_addr_for_array(stackshot_kcdata_p, STACKSHOT_KCTYPE_THREAD_DELTA_SNAPSHOT,
			    sizeof(struct thread_delta_snapshot_v3),
			    num_delta_thread_snapshots, &out_addr));
			delta_snapshots = (struct thread_delta_snapshot_v3 *)out_addr;
		}


#if STACKSHOT_COLLECTS_LATENCY_INFO
		latency_info.task_thread_count_loop_latency = mach_absolute_time();
#endif
		/*
		 * Iterate over the task threads to save thread snapshots and determine
		 * how much space we need for waitinfo and turnstile info
		 */
		thread_t thread = THREAD_NULL;
		queue_iterate(&task->threads, thread, thread_t, task_threads)
		{
			if ((thread == NULL) || !_stackshot_validate_kva((vm_offset_t)thread, sizeof(struct thread))) {
				error = KERN_FAILURE;
				goto error_exit;
			}

			uint64_t thread_uniqueid;
			if (active_kthreads_only_p && thread->kernel_stack == 0) {
				continue;
			}
			thread_uniqueid = thread_tid(thread);

			boolean_t thread_on_core;
			enum thread_classification thread_classification = classify_thread(thread, &thread_on_core, collect_delta_stackshot);

			switch (thread_classification) {
			case tc_full_snapshot:
				/* add thread marker */
				kcd_exit_on_error(kcdata_add_container_marker(stackshot_kcdata_p, KCDATA_TYPE_CONTAINER_BEGIN,
				    STACKSHOT_KCCONTAINER_THREAD, thread_uniqueid));

				/* thread snapshot can be large, including strings, avoid overflowing the stack. */
				kcdata_compression_window_open(stackshot_kcdata_p);

				kcd_exit_on_error(kcdata_record_thread_snapshot(stackshot_kcdata_p, thread, task, ctx->trace_flags, have_pmap, thread_on_core));

				kcd_exit_on_error(kcdata_compression_window_close(stackshot_kcdata_p));

				/* mark end of thread snapshot data */
				kcd_exit_on_error(kcdata_add_container_marker(stackshot_kcdata_p, KCDATA_TYPE_CONTAINER_END,
				    STACKSHOT_KCCONTAINER_THREAD, thread_uniqueid));
				break;
			case tc_delta_snapshot:
				kcd_exit_on_error(kcdata_record_thread_delta_snapshot(&delta_snapshots[current_delta_snapshot_index++], thread, thread_on_core));
				break;
			}

			/*
			 * We want to report owner information regardless of whether a thread
			 * has changed since the last delta, whether it's a normal stackshot,
			 * or whether it's nonrunnable
			 */
			if (save_owner_info) {
				if (stackshot_thread_has_valid_waitinfo(thread)) {
					num_waitinfo_threads++;
				}

				if (stackshot_thread_has_valid_turnstileinfo(thread)) {
					num_turnstileinfo_threads++;
				}
			}
		}
#if STACKSHOT_COLLECTS_LATENCY_INFO
		latency_info.task_thread_count_loop_latency = mach_absolute_time() - latency_info.task_thread_count_loop_latency;
#endif /* STACKSHOT_COLLECTS_LATENCY_INFO */


		thread_waitinfo_v2_t *thread_waitinfo           = NULL;
		thread_turnstileinfo_v2_t *thread_turnstileinfo = NULL;
		int current_waitinfo_index              = 0;
		int current_turnstileinfo_index         = 0;
		/* allocate space for the wait and turnstil info */
		if (num_waitinfo_threads > 0 || num_turnstileinfo_threads > 0) {
			/* thread waitinfo and turnstileinfo can be quite large, avoid overflowing the stack */
			kcdata_compression_window_open(stackshot_kcdata_p);

			if (num_waitinfo_threads > 0) {
				kcd_exit_on_error(kcdata_get_memory_addr_for_array(stackshot_kcdata_p, STACKSHOT_KCTYPE_THREAD_WAITINFO,
				    sizeof(thread_waitinfo_v2_t), num_waitinfo_threads, &out_addr));
				thread_waitinfo = (thread_waitinfo_v2_t *)out_addr;
			}

			if (num_turnstileinfo_threads > 0) {
				/* get space for the turnstile info */
				kcd_exit_on_error(kcdata_get_memory_addr_for_array(stackshot_kcdata_p, STACKSHOT_KCTYPE_THREAD_TURNSTILEINFO,
				    sizeof(thread_turnstileinfo_v2_t), num_turnstileinfo_threads, &out_addr));
				thread_turnstileinfo = (thread_turnstileinfo_v2_t *)out_addr;
			}

			stackshot_plh_resetgen();  // so we know which portlabel_ids are referenced
		}

#if STACKSHOT_COLLECTS_LATENCY_INFO
		latency_info.misc_latency = mach_absolute_time() - latency_info.misc_latency;
		latency_info.task_thread_data_loop_latency = mach_absolute_time();
#endif /* STACKSHOT_COLLECTS_LATENCY_INFO */

		/* Iterate over the task's threads to save the wait and turnstile info */
		queue_iterate(&task->threads, thread, thread_t, task_threads)
		{
			uint64_t thread_uniqueid;

			if (active_kthreads_only_p && thread->kernel_stack == 0) {
				continue;
			}

			thread_uniqueid = thread_tid(thread);

			/* If we want owner info, we should capture it regardless of its classification */
			if (save_owner_info) {
				if (stackshot_thread_has_valid_waitinfo(thread)) {
					stackshot_thread_wait_owner_info(
						thread,
						&thread_waitinfo[current_waitinfo_index++]);
				}

				if (stackshot_thread_has_valid_turnstileinfo(thread)) {
					stackshot_thread_turnstileinfo(
						thread,
						&thread_turnstileinfo[current_turnstileinfo_index++]);
				}
			}
		}

#if STACKSHOT_COLLECTS_LATENCY_INFO
		latency_info.task_thread_data_loop_latency = mach_absolute_time() - latency_info.task_thread_data_loop_latency;
		latency_info.misc2_latency = mach_absolute_time();
#endif /* STACKSHOT_COLLECTS_LATENCY_INFO */

#if DEBUG || DEVELOPMENT
		if (current_delta_snapshot_index != num_delta_thread_snapshots) {
			panic("delta thread snapshot count mismatch while capturing snapshots for task %p. expected %d, found %d", task,
			    num_delta_thread_snapshots, current_delta_snapshot_index);
		}
		if (current_waitinfo_index != num_waitinfo_threads) {
			panic("thread wait info count mismatch while capturing snapshots for task %p. expected %d, found %d", task,
			    num_waitinfo_threads, current_waitinfo_index);
		}
#endif

		if (num_waitinfo_threads > 0 || num_turnstileinfo_threads > 0) {
			kcd_exit_on_error(kcdata_compression_window_close(stackshot_kcdata_p));
			// now, record the portlabel hashes.
			kcd_exit_on_error(kdp_stackshot_plh_record());
		}

#if IMPORTANCE_INHERITANCE
		if (save_donating_pids_p) {
			kcd_exit_on_error(
				((((mach_vm_address_t)kcd_end_address(stackshot_kcdata_p) + (TASK_IMP_WALK_LIMIT * sizeof(int32_t))) <
				(mach_vm_address_t)kcd_max_address(stackshot_kcdata_p))
				? KERN_SUCCESS
				: KERN_RESOURCE_SHORTAGE));
			saved_count = task_importance_list_pids(task, TASK_IMP_LIST_DONATING_PIDS,
			    (void *)kcd_end_address(stackshot_kcdata_p), TASK_IMP_WALK_LIMIT);
			if (saved_count > 0) {
				/* Variable size array - better not have it on the stack. */
				kcdata_compression_window_open(stackshot_kcdata_p);
				kcd_exit_on_error(kcdata_get_memory_addr_for_array(stackshot_kcdata_p, STACKSHOT_KCTYPE_DONATING_PIDS,
				    sizeof(int32_t), saved_count, &out_addr));
				kcd_exit_on_error(kcdata_compression_window_close(stackshot_kcdata_p));
			}
		}
#endif

#if INTERRUPT_MASKED_DEBUG && MONOTONIC
		if (!panic_stackshot) {
			kcd_exit_on_error(kcdata_add_uint64_with_description(stackshot_kcdata_p, (mt_cur_cpu_cycles() - task_begin_cpu_cycle_count),
			    "task_cpu_cycle_count"));
		}
#endif

#if STACKSHOT_COLLECTS_LATENCY_INFO
		latency_info.misc2_latency = mach_absolute_time() - latency_info.misc2_latency;
		if (collect_latency_info) {
			kcd_exit_on_error(kcdata_push_data(stackshot_kcdata_p, STACKSHOT_KCTYPE_LATENCY_INFO_TASK, sizeof(latency_info), &latency_info));
		}
#endif /* STACKSHOT_COLLECTS_LATENCY_INFO */

		/* mark end of task snapshot data */
		kcd_exit_on_error(kcdata_add_container_marker(stackshot_kcdata_p, KCDATA_TYPE_CONTAINER_END, container_type,
		    task_uniqueid));
	}


error_exit:
	return error;
}


static kern_return_t
kdp_stackshot_kcdata_format(int pid, uint64_t trace_flags, uint32_t * pBytesTraced, uint32_t * pBytesUncompressed)
{
	kern_return_t error        = KERN_SUCCESS;
	mach_vm_address_t out_addr = 0;
	uint64_t abs_time = 0, abs_time_end = 0;
	uint64_t system_state_flags = 0;
	task_t task = TASK_NULL;
	mach_timebase_info_data_t timebase = {0, 0};
	uint32_t length_to_copy = 0, tmp32 = 0;
	abs_time = mach_absolute_time();
	uint64_t last_task_start_time = 0;

#if STACKSHOT_COLLECTS_LATENCY_INFO
	struct stackshot_latency_collection latency_info;
#endif

#if INTERRUPT_MASKED_DEBUG && MONOTONIC
	uint64_t stackshot_begin_cpu_cycle_count = 0;

	if (!panic_stackshot) {
		stackshot_begin_cpu_cycle_count = mt_cur_cpu_cycles();
	}
#endif

#if STACKSHOT_COLLECTS_LATENCY_INFO
	collect_latency_info = trace_flags & STACKSHOT_DISABLE_LATENCY_INFO ? false : true;
#endif

	/* process the flags */
	boolean_t collect_delta_stackshot = ((trace_flags & STACKSHOT_COLLECT_DELTA_SNAPSHOT) != 0);
	boolean_t use_fault_path          = ((trace_flags & (STACKSHOT_ENABLE_UUID_FAULTING | STACKSHOT_ENABLE_BT_FAULTING)) != 0);
	stack_enable_faulting = (trace_flags & (STACKSHOT_ENABLE_BT_FAULTING));

	/* Currently we only support returning explicit KEXT load info on fileset kernels */
	kc_format_t primary_kc_type = KCFormatUnknown;
	if (PE_get_primary_kc_format(&primary_kc_type) && (primary_kc_type != KCFormatFileset)) {
		trace_flags &= ~(STACKSHOT_SAVE_KEXT_LOADINFO);
	}

	struct stackshot_context ctx = {};
	ctx.trace_flags = trace_flags;
	ctx.pid = pid;

	if (use_fault_path) {
		fault_stats.sfs_pages_faulted_in = 0;
		fault_stats.sfs_time_spent_faulting = 0;
		fault_stats.sfs_stopped_faulting = (uint8_t) FALSE;
	}

	if (sizeof(void *) == 8) {
		system_state_flags |= kKernel64_p;
	}

	if (stackshot_kcdata_p == NULL || pBytesTraced == NULL) {
		error = KERN_INVALID_ARGUMENT;
		goto error_exit;
	}

	_stackshot_validation_reset();
	stackshot_plh_setup(stackshot_kcdata_p); /* set up port label hash */

	/* setup mach_absolute_time and timebase info -- copy out in some cases and needed to convert since_timestamp to seconds for proc start time */
	clock_timebase_info(&timebase);

	/* begin saving data into the buffer */
	*pBytesTraced = 0;
	if (pBytesUncompressed) {
		*pBytesUncompressed = 0;
	}
	kcd_exit_on_error(kcdata_add_uint64_with_description(stackshot_kcdata_p, trace_flags, "stackshot_in_flags"));
	kcd_exit_on_error(kcdata_add_uint32_with_description(stackshot_kcdata_p, (uint32_t)pid, "stackshot_in_pid"));
	kcd_exit_on_error(kcdata_add_uint64_with_description(stackshot_kcdata_p, system_state_flags, "system_state_flags"));
	if (trace_flags & STACKSHOT_PAGE_TABLES) {
		kcd_exit_on_error(kcdata_add_uint32_with_description(stackshot_kcdata_p, stack_snapshot_pagetable_mask, "stackshot_pagetable_mask"));
	}
	if (stackshot_initial_estimate != 0) {
		kcd_exit_on_error(kcdata_add_uint32_with_description(stackshot_kcdata_p, stackshot_initial_estimate, "stackshot_size_estimate"));
		kcd_exit_on_error(kcdata_add_uint32_with_description(stackshot_kcdata_p, stackshot_initial_estimate_adj, "stackshot_size_estimate_adj"));
	}

#if STACKSHOT_COLLECTS_LATENCY_INFO
	latency_info.setup_latency = mach_absolute_time();
#endif /* STACKSHOT_COLLECTS_LATENCY_INFO */

#if CONFIG_JETSAM
	tmp32 = memorystatus_get_pressure_status_kdp();
	kcd_exit_on_error(kcdata_push_data(stackshot_kcdata_p, STACKSHOT_KCTYPE_JETSAM_LEVEL, sizeof(uint32_t), &tmp32));
#endif

	if (!collect_delta_stackshot) {
		tmp32 = THREAD_POLICY_INTERNAL_STRUCT_VERSION;
		kcd_exit_on_error(kcdata_push_data(stackshot_kcdata_p, STACKSHOT_KCTYPE_THREAD_POLICY_VERSION, sizeof(uint32_t), &tmp32));

		tmp32 = PAGE_SIZE;
		kcd_exit_on_error(kcdata_push_data(stackshot_kcdata_p, STACKSHOT_KCTYPE_KERN_PAGE_SIZE, sizeof(uint32_t), &tmp32));

		/* save boot-args and osversion string */
		length_to_copy =  MIN((uint32_t)(strlen(version) + 1), OSVERSIZE);
		kcd_exit_on_error(kcdata_push_data(stackshot_kcdata_p, STACKSHOT_KCTYPE_OSVERSION, length_to_copy, (const void *)version));


		length_to_copy =  MIN((uint32_t)(strlen(PE_boot_args()) + 1), BOOT_LINE_LENGTH);
		kcd_exit_on_error(kcdata_push_data(stackshot_kcdata_p, STACKSHOT_KCTYPE_BOOTARGS, length_to_copy, PE_boot_args()));

		kcd_exit_on_error(kcdata_push_data(stackshot_kcdata_p, KCDATA_TYPE_TIMEBASE, sizeof(timebase), &timebase));
	} else {
		kcd_exit_on_error(kcdata_push_data(stackshot_kcdata_p, STACKSHOT_KCTYPE_DELTA_SINCE_TIMESTAMP, sizeof(uint64_t), &stack_snapshot_delta_since_timestamp));
	}

	kcd_exit_on_error(kcdata_push_data(stackshot_kcdata_p, KCDATA_TYPE_MACH_ABSOLUTE_TIME, sizeof(uint64_t), &abs_time));

	kcd_exit_on_error(kcdata_push_data(stackshot_kcdata_p, KCDATA_TYPE_USECS_SINCE_EPOCH, sizeof(uint64_t), &stackshot_microsecs));

	/* record system level shared cache load info (if available) */
	if (!collect_delta_stackshot && primary_system_shared_region &&
	    _stackshot_validate_kva((vm_offset_t)primary_system_shared_region, sizeof(struct vm_shared_region))) {
		struct dyld_shared_cache_loadinfo sys_shared_cache_info = {0};

		/*
		 * Historically, this data was in a dyld_uuid_info_64 structure, but the
		 * naming of both the structure and fields for this use isn't great.  The
		 * dyld_shared_cache_loadinfo structure has better names, but the same
		 * layout and content as the original.
		 *
		 * The imageSlidBaseAddress/sharedCacheUnreliableSlidBaseAddress field
		 * has been used inconsistently for STACKSHOT_COLLECT_SHAREDCACHE_LAYOUT
		 * entries; here, it's the slid base address, and we leave it that way
		 * for backwards compatibility.
		 */
		stackshot_memcpy(sys_shared_cache_info.sharedCacheUUID, &primary_system_shared_region->sr_uuid, sizeof(primary_system_shared_region->sr_uuid));
		sys_shared_cache_info.sharedCacheSlide =
		    primary_system_shared_region->sr_slide;
		sys_shared_cache_info.sharedCacheUnreliableSlidBaseAddress =
		    primary_system_shared_region->sr_slide + primary_system_shared_region->sr_base_address;
		sys_shared_cache_info.sharedCacheSlidFirstMapping =
		    primary_system_shared_region->sr_base_address + primary_system_shared_region->sr_first_mapping;

		kcd_exit_on_error(kcdata_push_data(stackshot_kcdata_p, STACKSHOT_KCTYPE_SHAREDCACHE_LOADINFO,
		    sizeof(sys_shared_cache_info), &sys_shared_cache_info));

		if (trace_flags & STACKSHOT_COLLECT_SHAREDCACHE_LAYOUT) {
			/*
			 * Include a map of the system shared cache layout if it has been populated
			 * (which is only when the system is using a custom shared cache).
			 */
			if (primary_system_shared_region->sr_images && _stackshot_validate_kva((vm_offset_t)primary_system_shared_region->sr_images,
			    (primary_system_shared_region->sr_images_count * sizeof(struct dyld_uuid_info_64)))) {
				assert(primary_system_shared_region->sr_images_count != 0);
				kcd_exit_on_error(kcdata_push_array(stackshot_kcdata_p, STACKSHOT_KCTYPE_SYS_SHAREDCACHE_LAYOUT, sizeof(struct dyld_uuid_info_64), primary_system_shared_region->sr_images_count, primary_system_shared_region->sr_images));
			}
		}
	}

	/* Add requested information first */
	if (trace_flags & STACKSHOT_GET_GLOBAL_MEM_STATS) {
		struct mem_and_io_snapshot mais = {0};
		kdp_mem_and_io_snapshot(&mais);
		kcd_exit_on_error(kcdata_push_data(stackshot_kcdata_p, STACKSHOT_KCTYPE_GLOBAL_MEM_STATS, sizeof(mais), &mais));
	}

#if CONFIG_THREAD_GROUPS
	struct thread_group_snapshot_v3 *thread_groups = NULL;
	int num_thread_groups = 0;

#if INTERRUPT_MASKED_DEBUG && MONOTONIC
	uint64_t thread_group_begin_cpu_cycle_count = 0;

	if (!panic_stackshot && (trace_flags & STACKSHOT_THREAD_GROUP)) {
		thread_group_begin_cpu_cycle_count = mt_cur_cpu_cycles();
	}
#endif


	/* Iterate over thread group names */
	if (trace_flags & STACKSHOT_THREAD_GROUP) {
		/* Variable size array - better not have it on the stack. */
		kcdata_compression_window_open(stackshot_kcdata_p);

		if (thread_group_iterate_stackshot(stackshot_thread_group_count, &num_thread_groups) != KERN_SUCCESS) {
			trace_flags &= ~(STACKSHOT_THREAD_GROUP);
		}

		if (num_thread_groups > 0) {
			kcd_exit_on_error(kcdata_get_memory_addr_for_array(stackshot_kcdata_p, STACKSHOT_KCTYPE_THREAD_GROUP_SNAPSHOT, sizeof(struct thread_group_snapshot_v3), num_thread_groups, &out_addr));
			thread_groups = (struct thread_group_snapshot_v3 *)out_addr;
		}

		if (thread_group_iterate_stackshot(stackshot_thread_group_snapshot, thread_groups) != KERN_SUCCESS) {
			error = KERN_FAILURE;
			goto error_exit;
		}

		kcd_exit_on_error(kcdata_compression_window_close(stackshot_kcdata_p));
	}

#if INTERRUPT_MASKED_DEBUG && MONOTONIC
	if (!panic_stackshot && (thread_group_begin_cpu_cycle_count != 0)) {
		kcd_exit_on_error(kcdata_add_uint64_with_description(stackshot_kcdata_p, (mt_cur_cpu_cycles() - thread_group_begin_cpu_cycle_count),
		    "thread_groups_cpu_cycle_count"));
	}
#endif
#else
	trace_flags &= ~(STACKSHOT_THREAD_GROUP);
#endif /* CONFIG_THREAD_GROUPS */


#if STACKSHOT_COLLECTS_LATENCY_INFO
	latency_info.setup_latency = mach_absolute_time() - latency_info.setup_latency;
	latency_info.total_task_iteration_latency = mach_absolute_time();
#endif /* STACKSHOT_COLLECTS_LATENCY_INFO */

	/* Iterate over tasks */
	queue_iterate(&tasks, task, task_t, tasks)
	{
		if (collect_delta_stackshot) {
			uint64_t abstime;
			proc_starttime_kdp(task->bsd_info, NULL, NULL, &abstime);

			if (abstime > last_task_start_time) {
				last_task_start_time = abstime;
			}
		}

		error = kdp_stackshot_record_task(&ctx, task);
		if (error) {
			goto error_exit;
		}
	}


#if STACKSHOT_COLLECTS_LATENCY_INFO
	latency_info.total_task_iteration_latency = mach_absolute_time() - latency_info.total_task_iteration_latency;
#endif /* STACKSHOT_COLLECTS_LATENCY_INFO */

#if CONFIG_COALITIONS
	/* Don't collect jetsam coalition data in delta stakshots - these don't change */
	if (!collect_delta_stackshot || (last_task_start_time > stack_snapshot_delta_since_timestamp)) {
		int num_coalitions = 0;
		struct jetsam_coalition_snapshot *coalitions = NULL;

#if INTERRUPT_MASKED_DEBUG && MONOTONIC
		uint64_t coalition_begin_cpu_cycle_count = 0;

		if (!panic_stackshot && (trace_flags & STACKSHOT_SAVE_JETSAM_COALITIONS)) {
			coalition_begin_cpu_cycle_count = mt_cur_cpu_cycles();
		}
#endif /* INTERRUPT_MASKED_DEBUG && MONOTONIC */

		/* Iterate over coalitions */
		if (trace_flags & STACKSHOT_SAVE_JETSAM_COALITIONS) {
			if (coalition_iterate_stackshot(stackshot_coalition_jetsam_count, &num_coalitions, COALITION_TYPE_JETSAM) != KERN_SUCCESS) {
				trace_flags &= ~(STACKSHOT_SAVE_JETSAM_COALITIONS);
			}
		}
		if (trace_flags & STACKSHOT_SAVE_JETSAM_COALITIONS) {
			if (num_coalitions > 0) {
				/* Variable size array - better not have it on the stack. */
				kcdata_compression_window_open(stackshot_kcdata_p);
				kcd_exit_on_error(kcdata_get_memory_addr_for_array(stackshot_kcdata_p, STACKSHOT_KCTYPE_JETSAM_COALITION_SNAPSHOT, sizeof(struct jetsam_coalition_snapshot), num_coalitions, &out_addr));
				coalitions = (struct jetsam_coalition_snapshot*)out_addr;

				if (coalition_iterate_stackshot(stackshot_coalition_jetsam_snapshot, coalitions, COALITION_TYPE_JETSAM) != KERN_SUCCESS) {
					error = KERN_FAILURE;
					goto error_exit;
				}

				kcd_exit_on_error(kcdata_compression_window_close(stackshot_kcdata_p));
			}
		}
#if INTERRUPT_MASKED_DEBUG && MONOTONIC
		if (!panic_stackshot && (coalition_begin_cpu_cycle_count != 0)) {
			kcd_exit_on_error(kcdata_add_uint64_with_description(stackshot_kcdata_p, (mt_cur_cpu_cycles() - coalition_begin_cpu_cycle_count),
			    "coalitions_cpu_cycle_count"));
		}
#endif /* INTERRUPT_MASKED_DEBUG && MONOTONIC */
	}
#else
	trace_flags &= ~(STACKSHOT_SAVE_JETSAM_COALITIONS);
#endif /* CONFIG_COALITIONS */

#if STACKSHOT_COLLECTS_LATENCY_INFO
	latency_info.total_terminated_task_iteration_latency = mach_absolute_time();
#endif /* STACKSHOT_COLLECTS_LATENCY_INFO */

	/*
	 * Iterate over the tasks in the terminated tasks list. We only inspect
	 * tasks that have a valid bsd_info pointer. The check for task transition
	 * like past P_LPEXIT during proc_exit() is now checked for inside the
	 * kdp_stackshot_record_task(), and then a safer and minimal
	 * transitioning_task_snapshot struct is collected via
	 * kcdata_record_transitioning_task_snapshot()
	 */
	queue_iterate(&terminated_tasks, task, task_t, tasks)
	{
		error = kdp_stackshot_record_task(&ctx, task);
		if (error) {
			goto error_exit;
		}
	}
#if DEVELOPMENT || DEBUG
	kcd_exit_on_error(kdp_stackshot_plh_stats());
#endif /* DEVELOPMENT || DEBUG */

#if STACKSHOT_COLLECTS_LATENCY_INFO
	latency_info.total_terminated_task_iteration_latency = mach_absolute_time() - latency_info.total_terminated_task_iteration_latency;
#endif /* STACKSHOT_COLLECTS_LATENCY_INFO */

	if (use_fault_path) {
		kcdata_push_data(stackshot_kcdata_p, STACKSHOT_KCTYPE_STACKSHOT_FAULT_STATS,
		    sizeof(struct stackshot_fault_stats), &fault_stats);
	}

#if STACKSHOT_COLLECTS_LATENCY_INFO
	if (collect_latency_info) {
		latency_info.latency_version = 1;
		kcd_exit_on_error(kcdata_push_data(stackshot_kcdata_p, STACKSHOT_KCTYPE_LATENCY_INFO, sizeof(latency_info), &latency_info));
	}
#endif /* STACKSHOT_COLLECTS_LATENCY_INFO */

	/* update timestamp of the stackshot */
	abs_time_end = mach_absolute_time();
	struct stackshot_duration_v2 stackshot_duration = {
		.stackshot_duration         = (abs_time_end - abs_time),
		.stackshot_duration_outer   = 0,
		.stackshot_duration_prior   = stackshot_duration_prior_abs,
	};

	if ((trace_flags & STACKSHOT_DO_COMPRESS) == 0) {
		kcd_exit_on_error(kcdata_get_memory_addr(stackshot_kcdata_p, STACKSHOT_KCTYPE_STACKSHOT_DURATION,
		    sizeof(struct stackshot_duration_v2), &out_addr));
		struct stackshot_duration_v2 *duration_p = (void *) out_addr;
		stackshot_memcpy(duration_p, &stackshot_duration, sizeof(*duration_p));
		stackshot_duration_outer                   = (unaligned_u64 *)&duration_p->stackshot_duration_outer;
	} else {
		kcd_exit_on_error(kcdata_push_data(stackshot_kcdata_p, STACKSHOT_KCTYPE_STACKSHOT_DURATION, sizeof(stackshot_duration), &stackshot_duration));
		stackshot_duration_outer = NULL;
	}

#if INTERRUPT_MASKED_DEBUG && MONOTONIC
	if (!panic_stackshot) {
		kcd_exit_on_error(kcdata_add_uint64_with_description(stackshot_kcdata_p, (mt_cur_cpu_cycles() - stackshot_begin_cpu_cycle_count),
		    "stackshot_total_cpu_cycle_cnt"));
	}
#endif

	kcd_finalize_compression(stackshot_kcdata_p);
	kcd_exit_on_error(kcdata_add_uint64_with_description(stackshot_kcdata_p, trace_flags, "stackshot_out_flags"));

	kcd_exit_on_error(kcdata_write_buffer_end(stackshot_kcdata_p));

	/*  === END of populating stackshot data === */

	*pBytesTraced = (uint32_t) kcdata_memory_get_used_bytes(stackshot_kcdata_p);
	*pBytesUncompressed = (uint32_t) kcdata_memory_get_uncompressed_bytes(stackshot_kcdata_p);

error_exit:;

#if INTERRUPT_MASKED_DEBUG
	bool disable_interrupts_masked_check = kern_feature_override(
		KF_INTERRUPT_MASKED_DEBUG_STACKSHOT_OVRD) ||
	    (trace_flags & STACKSHOT_DO_COMPRESS) != 0;

#if STACKSHOT_INTERRUPTS_MASKED_CHECK_DISABLED
	disable_interrupts_masked_check = true;
#endif /* STACKSHOT_INTERRUPTS_MASKED_CHECK_DISABLED */

	if (disable_interrupts_masked_check) {
		ml_spin_debug_clear_self();
	}

	if (!panic_stackshot && interrupt_masked_debug_mode) {
		/*
		 * Try to catch instances where stackshot takes too long BEFORE returning from
		 * the debugger
		 */
		ml_handle_stackshot_interrupt_disabled_duration(current_thread());
	}
#endif /* INTERRUPT_MASKED_DEBUG */
	stackshot_plh_reset();
	stack_enable_faulting = FALSE;

	return error;
}

static uint64_t
proc_was_throttled_from_task(task_t task)
{
	uint64_t was_throttled = 0;

	if (task->bsd_info) {
		was_throttled = proc_was_throttled(task->bsd_info);
	}

	return was_throttled;
}

static uint64_t
proc_did_throttle_from_task(task_t task)
{
	uint64_t did_throttle = 0;

	if (task->bsd_info) {
		did_throttle = proc_did_throttle(task->bsd_info);
	}

	return did_throttle;
}

static void
kdp_mem_and_io_snapshot(struct mem_and_io_snapshot *memio_snap)
{
	unsigned int pages_reclaimed;
	unsigned int pages_wanted;
	kern_return_t kErr;

	uint64_t compressions = 0;
	uint64_t decompressions = 0;

	compressions = counter_load(&vm_statistics_compressions);
	decompressions = counter_load(&vm_statistics_decompressions);

	memio_snap->snapshot_magic = STACKSHOT_MEM_AND_IO_SNAPSHOT_MAGIC;
	memio_snap->free_pages = vm_page_free_count;
	memio_snap->active_pages = vm_page_active_count;
	memio_snap->inactive_pages = vm_page_inactive_count;
	memio_snap->purgeable_pages = vm_page_purgeable_count;
	memio_snap->wired_pages = vm_page_wire_count;
	memio_snap->speculative_pages = vm_page_speculative_count;
	memio_snap->throttled_pages = vm_page_throttled_count;
	memio_snap->busy_buffer_count = count_busy_buffers();
	memio_snap->filebacked_pages = vm_page_pageable_external_count;
	memio_snap->compressions = (uint32_t)compressions;
	memio_snap->decompressions = (uint32_t)decompressions;
	memio_snap->compressor_size = VM_PAGE_COMPRESSOR_COUNT;
	kErr = mach_vm_pressure_monitor(FALSE, VM_PRESSURE_TIME_WINDOW, &pages_reclaimed, &pages_wanted);

	if (!kErr) {
		memio_snap->pages_wanted = (uint32_t)pages_wanted;
		memio_snap->pages_reclaimed = (uint32_t)pages_reclaimed;
		memio_snap->pages_wanted_reclaimed_valid = 1;
	} else {
		memio_snap->pages_wanted = 0;
		memio_snap->pages_reclaimed = 0;
		memio_snap->pages_wanted_reclaimed_valid = 0;
	}
}

void
stackshot_memcpy(void *dst, const void *src, size_t len)
{
#if defined(__arm__) || defined(__arm64__)
	if (panic_stackshot) {
		uint8_t *dest_bytes = (uint8_t *)dst;
		const uint8_t *src_bytes = (const uint8_t *)src;
		for (size_t i = 0; i < len; i++) {
			dest_bytes[i] = src_bytes[i];
		}
	} else
#endif
	memcpy(dst, src, len);
}

static long
_stackshot_strlen(const char *s, size_t maxlen)
{
	size_t len = 0;
	for (len = 0; _stackshot_validate_kva((vm_offset_t)s, 1); len++, s++) {
		if (*s == 0) {
			return len;
		}
		if (len >= maxlen) {
			return -1;
		}
	}
	return -1; /* failed before end of string */
}
static size_t
_stackshot_strlcpy(char *dst, const char *src, size_t maxlen)
{
	const size_t srclen = strlen(src);

	if (srclen < maxlen) {
		stackshot_memcpy(dst, src, srclen + 1);
	} else if (maxlen != 0) {
		stackshot_memcpy(dst, src, maxlen - 1);
		dst[maxlen - 1] = '\0';
	}

	return srclen;
}

/*
 * Sets the appropriate page mask and size to use for dealing with pages --
 * it's important that this is a "min" of page size to account for both K16/U4
 * (Rosetta) and K4/U16 (armv7k) environments.
 */
static inline size_t
_stackshot_get_page_size(vm_map_t map, size_t *effective_page_mask)
{
	if (VM_MAP_PAGE_SHIFT(map) < PAGE_SHIFT) {
		*effective_page_mask = VM_MAP_PAGE_MASK(map);
		return VM_MAP_PAGE_SIZE(map);
	} else {
		*effective_page_mask = PAGE_MASK;
		return PAGE_SIZE;
	}
}

/*
 * Returns the physical address of the specified map:target address,
 * using the kdp fault path if requested and the page is not resident.
 */
vm_offset_t
kdp_find_phys(vm_map_t map, vm_offset_t target_addr, boolean_t try_fault, uint32_t *kdp_fault_results)
{
	vm_offset_t cur_phys_addr;

	if (map == VM_MAP_NULL) {
		return 0;
	}

	cur_phys_addr = kdp_vtophys(map->pmap, target_addr);
	if (!pmap_valid_page((ppnum_t) atop(cur_phys_addr))) {
		if (!try_fault || fault_stats.sfs_stopped_faulting) {
			if (kdp_fault_results) {
				*kdp_fault_results |= KDP_FAULT_RESULT_PAGED_OUT;
			}

			return 0;
		}

		/*
		 * The pmap doesn't have a valid page so we start at the top level
		 * vm map and try a lightweight fault. Update fault path usage stats.
		 */
		uint64_t fault_start_time = mach_absolute_time();
		size_t effective_page_mask;
		(void)_stackshot_get_page_size(map, &effective_page_mask);

		cur_phys_addr = kdp_lightweight_fault(map, (target_addr & ~effective_page_mask));
		fault_stats.sfs_time_spent_faulting += (mach_absolute_time() - fault_start_time);

		if ((fault_stats.sfs_time_spent_faulting >= fault_stats.sfs_system_max_fault_time) && !panic_stackshot) {
			fault_stats.sfs_stopped_faulting = (uint8_t) TRUE;
		}

		cur_phys_addr += (target_addr & effective_page_mask);

		if (!pmap_valid_page((ppnum_t) atop(cur_phys_addr))) {
			if (kdp_fault_results) {
				*kdp_fault_results |= (KDP_FAULT_RESULT_TRIED_FAULT | KDP_FAULT_RESULT_PAGED_OUT);
			}

			return 0;
		}

		if (kdp_fault_results) {
			*kdp_fault_results |= KDP_FAULT_RESULT_FAULTED_IN;
		}

		fault_stats.sfs_pages_faulted_in++;
	} else {
		/*
		 * This check is done in kdp_lightweight_fault for the fault path.
		 */
		unsigned int cur_wimg_bits = pmap_cache_attributes((ppnum_t) atop(cur_phys_addr));

		if ((cur_wimg_bits & VM_WIMG_MASK) != VM_WIMG_DEFAULT) {
			return 0;
		}
	}

	return cur_phys_addr;
}

boolean_t
kdp_copyin_word(
	task_t task, uint64_t addr, uint64_t *result, boolean_t try_fault, uint32_t *kdp_fault_results)
{
	if (task_has_64Bit_addr(task)) {
		return kdp_copyin(task->map, addr, result, sizeof(uint64_t), try_fault, kdp_fault_results);
	} else {
		uint32_t buf;
		boolean_t r = kdp_copyin(task->map, addr, &buf, sizeof(uint32_t), try_fault, kdp_fault_results);
		*result = buf;
		return r;
	}
}

static int
kdp_copyin_string_slowpath(
	task_t task, uint64_t addr, char *buf, int buf_sz, boolean_t try_fault, uint32_t *kdp_fault_results)
{
	int i;
	uint64_t validated = 0, valid_from;
	uint64_t phys_src, phys_dest;
	vm_map_t map = task->map;
	size_t effective_page_mask;
	size_t effective_page_size = _stackshot_get_page_size(map, &effective_page_mask);

	for (i = 0; i < buf_sz; i++) {
		if (validated == 0) {
			valid_from = i;
			phys_src = kdp_find_phys(map, addr + i, try_fault, kdp_fault_results);
			phys_dest = kvtophys((vm_offset_t)&buf[i]);
			uint64_t src_rem = effective_page_size - (phys_src & effective_page_mask);
			uint64_t dst_rem = PAGE_SIZE - (phys_dest & PAGE_MASK);
			if (phys_src && phys_dest) {
				validated = MIN(src_rem, dst_rem);
				if (validated) {
					bcopy_phys(phys_src, phys_dest, 1);
					validated--;
				} else {
					return 0;
				}
			} else {
				return 0;
			}
		} else {
			bcopy_phys(phys_src + (i - valid_from), phys_dest + (i - valid_from), 1);
			validated--;
		}

		if (buf[i] == '\0') {
			return i + 1;
		}
	}

	/* ran out of space */
	return -1;
}

int
kdp_copyin_string(
	task_t task, uint64_t addr, char *buf, int buf_sz, boolean_t try_fault, uint32_t *kdp_fault_results)
{
	/* try to opportunistically copyin 32 bytes, most strings should fit */
	char optbuffer[32];
	boolean_t res;

	bzero(optbuffer, sizeof(optbuffer));
	res = kdp_copyin(task->map, addr, optbuffer, sizeof(optbuffer), try_fault, kdp_fault_results);
	if (res == FALSE || strnlen(optbuffer, sizeof(optbuffer)) == sizeof(optbuffer)) {
		/* try the slowpath */
		return kdp_copyin_string_slowpath(task, addr, buf, buf_sz, try_fault, kdp_fault_results);
	}

	/* success */
	return (int) strlcpy(buf, optbuffer, buf_sz) + 1;
}

boolean_t
kdp_copyin(vm_map_t map, uint64_t uaddr, void *dest, size_t size, boolean_t try_fault, uint32_t *kdp_fault_results)
{
	size_t rem = size;
	char *kvaddr = dest;
	size_t effective_page_mask;
	size_t effective_page_size = _stackshot_get_page_size(map, &effective_page_mask);

#if defined(__arm__) || defined(__arm64__)
	/* Identify if destination buffer is in panic storage area */
	if (panic_stackshot && ((vm_offset_t)dest >= gPanicBase) && ((vm_offset_t)dest < (gPanicBase + gPanicSize))) {
		if (((vm_offset_t)dest + size) > (gPanicBase + gPanicSize)) {
			return FALSE;
		}
	}
#endif

	while (rem) {
		uint64_t phys_src = kdp_find_phys(map, uaddr, try_fault, kdp_fault_results);
		uint64_t phys_dest = kvtophys((vm_offset_t)kvaddr);
		uint64_t src_rem = effective_page_size - (phys_src & effective_page_mask);
		uint64_t dst_rem = PAGE_SIZE - (phys_dest & PAGE_MASK);
		size_t cur_size = (uint32_t) MIN(src_rem, dst_rem);
		cur_size = MIN(cur_size, rem);

		if (phys_src && phys_dest) {
#if defined(__arm__) || defined(__arm64__)
			/*
			 * On arm devices the panic buffer is mapped as device memory and doesn't allow
			 * unaligned accesses. To prevent these, we copy over bytes individually here.
			 */
			if (panic_stackshot) {
				stackshot_memcpy(kvaddr, (const void *)phystokv(phys_src), cur_size);
			} else
#endif /* defined(__arm__) || defined(__arm64__) */
			bcopy_phys(phys_src, phys_dest, cur_size);
		} else {
			break;
		}

		uaddr += cur_size;
		kvaddr += cur_size;
		rem -= cur_size;
	}

	return rem == 0;
}

kern_return_t
do_stackshot(void *context)
{
#pragma unused(context)
	kdp_snapshot++;

	stack_snapshot_ret = kdp_stackshot_kcdata_format(stack_snapshot_pid,
	    stack_snapshot_flags,
	    &stack_snapshot_bytes_traced,
	    &stack_snapshot_bytes_uncompressed);

	if (stack_snapshot_ret == KERN_SUCCESS) {
		/* releases and zeros and kcdata_end_alloc()s done */
		kcdata_finish(stackshot_kcdata_p);
	}

	kdp_snapshot--;
	return stack_snapshot_ret;
}

boolean_t
stackshot_thread_is_idle_worker_unsafe(thread_t thread)
{
	/* When the pthread kext puts a worker thread to sleep, it will
	 * set kThreadWaitParkedWorkQueue in the block_hint of the thread
	 * struct. See parkit() in kern/kern_support.c in libpthread.
	 */
	return (thread->state & TH_WAIT) &&
	       (thread->block_hint == kThreadWaitParkedWorkQueue);
}

#if CONFIG_COALITIONS
static void
stackshot_coalition_jetsam_count(void *arg, int i, coalition_t coal)
{
#pragma unused(i, coal)
	unsigned int *coalition_count = (unsigned int*)arg;
	(*coalition_count)++;
}

static void
stackshot_coalition_jetsam_snapshot(void *arg, int i, coalition_t coal)
{
	if (coalition_type(coal) != COALITION_TYPE_JETSAM) {
		return;
	}

	struct jetsam_coalition_snapshot *coalitions = (struct jetsam_coalition_snapshot*)arg;
	struct jetsam_coalition_snapshot *jcs = &coalitions[i];
	task_t leader = TASK_NULL;
	jcs->jcs_id = coalition_id(coal);
	jcs->jcs_flags = 0;
	jcs->jcs_thread_group = 0;

	if (coalition_term_requested(coal)) {
		jcs->jcs_flags |= kCoalitionTermRequested;
	}
	if (coalition_is_terminated(coal)) {
		jcs->jcs_flags |= kCoalitionTerminated;
	}
	if (coalition_is_reaped(coal)) {
		jcs->jcs_flags |= kCoalitionReaped;
	}
	if (coalition_is_privileged(coal)) {
		jcs->jcs_flags |= kCoalitionPrivileged;
	}

#if CONFIG_THREAD_GROUPS
	struct thread_group *thread_group = kdp_coalition_get_thread_group(coal);
	if (thread_group) {
		jcs->jcs_thread_group = thread_group_get_id(thread_group);
	}
#endif /* CONFIG_THREAD_GROUPS */

	leader = kdp_coalition_get_leader(coal);
	if (leader) {
		jcs->jcs_leader_task_uniqueid = get_task_uniqueid(leader);
	} else {
		jcs->jcs_leader_task_uniqueid = 0;
	}
}
#endif /* CONFIG_COALITIONS */

#if CONFIG_THREAD_GROUPS
static void
stackshot_thread_group_count(void *arg, int i, struct thread_group *tg)
{
#pragma unused(i, tg)
	unsigned int *n = (unsigned int*)arg;
	(*n)++;
}

static void
stackshot_thread_group_snapshot(void *arg, int i, struct thread_group *tg)
{
	struct thread_group_snapshot_v3 *thread_groups = arg;
	struct thread_group_snapshot_v3 *tgs = &thread_groups[i];
	const char *name = thread_group_get_name(tg);
	uint32_t flags = thread_group_get_flags(tg);
	tgs->tgs_id = thread_group_get_id(tg);
	static_assert(THREAD_GROUP_MAXNAME > sizeof(tgs->tgs_name));
	stackshot_memcpy(tgs->tgs_name, name, sizeof(tgs->tgs_name));
	stackshot_memcpy(tgs->tgs_name_cont, name + sizeof(tgs->tgs_name),
	    sizeof(tgs->tgs_name_cont));
	tgs->tgs_flags = ((flags & THREAD_GROUP_FLAGS_EFFICIENT) ? kThreadGroupEfficient : 0) |
	    ((flags & THREAD_GROUP_FLAGS_UI_APP) ? kThreadGroupUIApp : 0);
}
#endif /* CONFIG_THREAD_GROUPS */

/* Determine if a thread has waitinfo that stackshot can provide */
static int
stackshot_thread_has_valid_waitinfo(thread_t thread)
{
	if (!(thread->state & TH_WAIT)) {
		return 0;
	}

	switch (thread->block_hint) {
	// If set to None or is a parked work queue, ignore it
	case kThreadWaitParkedWorkQueue:
	case kThreadWaitNone:
		return 0;
	// There is a short window where the pthread kext removes a thread
	// from its ksyn wait queue before waking the thread up
	case kThreadWaitPThreadMutex:
	case kThreadWaitPThreadRWLockRead:
	case kThreadWaitPThreadRWLockWrite:
	case kThreadWaitPThreadCondVar:
		return kdp_pthread_get_thread_kwq(thread) != NULL;
	// All other cases are valid block hints if in a wait state
	default:
		return 1;
	}
}

/* Determine if a thread has turnstileinfo that stackshot can provide */
static int
stackshot_thread_has_valid_turnstileinfo(thread_t thread)
{
	struct turnstile *ts = thread_get_waiting_turnstile(thread);

	return stackshot_thread_has_valid_waitinfo(thread) &&
	       ts != TURNSTILE_NULL;
}

static void
stackshot_thread_turnstileinfo(thread_t thread, thread_turnstileinfo_v2_t *tsinfo)
{
	struct turnstile *ts;
	struct ipc_service_port_label *ispl = NULL;

	/* acquire turnstile information and store it in the stackshot */
	ts = thread_get_waiting_turnstile(thread);
	tsinfo->waiter = thread_tid(thread);
	kdp_turnstile_fill_tsinfo(ts, tsinfo, &ispl);
	tsinfo->portlabel_id = stackshot_plh_lookup(ispl,
	    (tsinfo->turnstile_flags & STACKSHOT_TURNSTILE_STATUS_SENDPORT) ? STACKSHOT_PLH_LOOKUP_SEND :
	    (tsinfo->turnstile_flags & STACKSHOT_TURNSTILE_STATUS_RECEIVEPORT) ? STACKSHOT_PLH_LOOKUP_RECEIVE :
	    STACKSHOT_PLH_LOOKUP_UNKNOWN);
}

static void
stackshot_thread_wait_owner_info(thread_t thread, thread_waitinfo_v2_t *waitinfo)
{
	thread_waitinfo_t *waitinfo_v1 = (thread_waitinfo_t *)waitinfo;
	struct ipc_service_port_label *ispl = NULL;

	waitinfo->waiter        = thread_tid(thread);
	waitinfo->wait_type     = thread->block_hint;
	waitinfo->wait_flags    = 0;

	switch (waitinfo->wait_type) {
	case kThreadWaitKernelMutex:
		kdp_lck_mtx_find_owner(thread->waitq.wq_q, thread->wait_event, waitinfo_v1);
		break;
	case kThreadWaitPortReceive:
		kdp_mqueue_recv_find_owner(thread->waitq.wq_q, thread->wait_event, waitinfo, &ispl);
		waitinfo->portlabel_id  = stackshot_plh_lookup(ispl, STACKSHOT_PLH_LOOKUP_RECEIVE);
		break;
	case kThreadWaitPortSend:
		kdp_mqueue_send_find_owner(thread->waitq.wq_q, thread->wait_event, waitinfo, &ispl);
		waitinfo->portlabel_id  = stackshot_plh_lookup(ispl, STACKSHOT_PLH_LOOKUP_SEND);
		break;
	case kThreadWaitSemaphore:
		kdp_sema_find_owner(thread->waitq.wq_q, thread->wait_event, waitinfo_v1);
		break;
	case kThreadWaitUserLock:
		kdp_ulock_find_owner(thread->waitq.wq_q, thread->wait_event, waitinfo_v1);
		break;
	case kThreadWaitKernelRWLockRead:
	case kThreadWaitKernelRWLockWrite:
	case kThreadWaitKernelRWLockUpgrade:
		kdp_rwlck_find_owner(thread->waitq.wq_q, thread->wait_event, waitinfo_v1);
		break;
	case kThreadWaitPThreadMutex:
	case kThreadWaitPThreadRWLockRead:
	case kThreadWaitPThreadRWLockWrite:
	case kThreadWaitPThreadCondVar:
		kdp_pthread_find_owner(thread, waitinfo_v1);
		break;
	case kThreadWaitWorkloopSyncWait:
		kdp_workloop_sync_wait_find_owner(thread, thread->wait_event, waitinfo_v1);
		break;
	case kThreadWaitOnProcess:
		kdp_wait4_find_process(thread, thread->wait_event, waitinfo_v1);
		break;
	case kThreadWaitSleepWithInheritor:
		kdp_sleep_with_inheritor_find_owner(thread->waitq.wq_q, thread->wait_event, waitinfo_v1);
		break;
	case kThreadWaitEventlink:
		kdp_eventlink_find_owner(thread->waitq.wq_q, thread->wait_event, waitinfo_v1);
		break;
	case kThreadWaitCompressor:
		kdp_compressor_busy_find_owner(thread->wait_event, waitinfo_v1);
		break;
	default:
		waitinfo->owner = 0;
		waitinfo->context = 0;
		break;
	}
}
