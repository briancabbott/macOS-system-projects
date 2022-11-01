/* drm_memory.h -- Memory management wrappers for DRM -*- linux-c -*-
 * Created: Thu Feb  4 14:00:34 1999 by faith@valinux.com
 *
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Rickard E. (Rik) Faith <faith@valinux.com>
 *    Gareth Hughes <gareth@valinux.com>
 */

#include "drmP.h"

#ifdef __FreeBSD__
#define malloctype DRM(M_DRM)
/* The macros confliced in the MALLOC_DEFINE */

MALLOC_DEFINE(malloctype, "drm", "DRM Data Structures");

#undef malloctype
#endif

typedef struct drm_mem_stats {
	const char	  *name;
	int		  succeed_count;
	int		  free_count;
	int		  fail_count;
	unsigned long	  bytes_allocated;
	unsigned long	  bytes_freed;
} drm_mem_stats_t;

static DRM_SPINTYPE	  DRM(mem_lock);
static unsigned long	  DRM(ram_available) = 0; /* In pages */
static unsigned long	  DRM(ram_used)      = 0;
static drm_mem_stats_t	  DRM(mem_stats)[]   = {
	[DRM_MEM_DMA]	    = { "dmabufs"  },
	[DRM_MEM_SAREA]	    = { "sareas"   },
	[DRM_MEM_DRIVER]    = { "driver"   },
	[DRM_MEM_MAGIC]	    = { "magic"	   },
	[DRM_MEM_IOCTLS]    = { "ioctltab" },
	[DRM_MEM_MAPS]	    = { "maplist"  },
	[DRM_MEM_VMAS]	    = { "vmalist"  },
	[DRM_MEM_BUFS]	    = { "buflist"  },
	[DRM_MEM_SEGS]	    = { "seglist"  },
	[DRM_MEM_PAGES]	    = { "pagelist" },
	[DRM_MEM_FILES]	    = { "files"	   },
	[DRM_MEM_QUEUES]    = { "queues"   },
	[DRM_MEM_CMDS]	    = { "commands" },
	[DRM_MEM_MAPPINGS]  = { "mappings" },
	[DRM_MEM_BUFLISTS]  = { "buflists" },
	[DRM_MEM_AGPLISTS]  = { "agplist"  },
	[DRM_MEM_SGLISTS]   = { "sglist"   },
	[DRM_MEM_TOTALAGP]  = { "totalagp" },
	[DRM_MEM_BOUNDAGP]  = { "boundagp" },
	[DRM_MEM_CTXBITMAP] = { "ctxbitmap"},
	[DRM_MEM_STUB]      = { "stub"     },
	{ NULL, 0, }		/* Last entry must be null */
};

void DRM(mem_init)(void)
{
	drm_mem_stats_t *mem;

	DRM_SPININIT(DRM(mem_lock), "drm memory");

	for (mem = DRM(mem_stats); mem->name; ++mem) {
		mem->succeed_count   = 0;
		mem->free_count	     = 0;
		mem->fail_count	     = 0;
		mem->bytes_allocated = 0;
		mem->bytes_freed     = 0;
	}

	DRM(ram_available) = 0; /* si.totalram */
	DRM(ram_used)	   = 0;
}

void DRM(mem_uninit)(void)
{
	DRM_SPINUNINIT(DRM(mem_lock));
}

#ifdef __FreeBSD__
/* drm_mem_info is called whenever a process reads /dev/drm/mem. */
static int DRM(_mem_info) DRM_SYSCTL_HANDLER_ARGS
{
	drm_mem_stats_t *pt;
	char buf[128];
	int error;

	DRM_SYSCTL_PRINT("		  total counts			"
		       " |    outstanding  \n");
	DRM_SYSCTL_PRINT("type	   alloc freed fail	bytes	   freed"
		       " | allocs      bytes\n\n");
	DRM_SYSCTL_PRINT("%-9.9s %5d %5d %4d %10lu	    |\n",
		       "system", 0, 0, 0, DRM(ram_available));
	DRM_SYSCTL_PRINT("%-9.9s %5d %5d %4d %10lu	    |\n",
		       "locked", 0, 0, 0, DRM(ram_used));
	DRM_SYSCTL_PRINT("\n");
	for (pt = DRM(mem_stats); pt->name; pt++) {
		DRM_SYSCTL_PRINT("%-9.9s %5d %5d %4d %10lu %10lu | %6d %10ld\n",
			       pt->name,
			       pt->succeed_count,
			       pt->free_count,
			       pt->fail_count,
			       pt->bytes_allocated,
			       pt->bytes_freed,
			       pt->succeed_count - pt->free_count,
			       (long)pt->bytes_allocated
			       - (long)pt->bytes_freed);
	}
	SYSCTL_OUT(req, "", 1);
	
	return 0;
}

int DRM(mem_info) DRM_SYSCTL_HANDLER_ARGS
{
	int ret;

	DRM_SPINLOCK(&DRM(mem_lock));
	ret = DRM(_mem_info)(oidp, arg1, arg2, req);
	DRM_SPINUNLOCK(&DRM(mem_lock));
	return ret;
}
#endif

void *DRM(alloc)(size_t size, int area)
{
	void *pt;

	if (!size) {
		DRM_MEM_ERROR(area, "Allocating 0 bytes\n");
		return NULL;
	}

#ifdef __FreeBSD__
	if (!(pt = malloc(size, DRM(M_DRM), M_NOWAIT))) {
#elif defined(__NetBSD__)
	if (!(pt = malloc(size, M_DEVBUF, M_NOWAIT))) {
#endif
		DRM_SPINLOCK(&DRM(mem_lock));
		++DRM(mem_stats)[area].fail_count;
		DRM_SPINUNLOCK(&DRM(mem_lock));
		return NULL;
	}
	DRM_SPINLOCK(&DRM(mem_lock));
	++DRM(mem_stats)[area].succeed_count;
	DRM(mem_stats)[area].bytes_allocated += size;
	DRM_SPINUNLOCK(&DRM(mem_lock));
	return pt;
}

void *DRM(realloc)(void *oldpt, size_t oldsize, size_t size, int area)
{
	void *pt;

	if (!(pt = DRM(alloc)(size, area))) return NULL;
	if (oldpt && oldsize) {
		memcpy(pt, oldpt, oldsize);
		DRM(free)(oldpt, oldsize, area);
	}
	return pt;
}

char *DRM(strdup)(const char *s, int area)
{
	char *pt;
	int	 length = s ? strlen(s) : 0;

	if (!(pt = DRM(alloc)(length+1, area))) return NULL;
	strcpy(pt, s);
	return pt;
}

void DRM(strfree)(char *s, int area)
{
	unsigned int size;

	if (!s) return;

	size = 1 + strlen(s);
	DRM(free)((void *)s, size, area);
}

void DRM(free)(void *pt, size_t size, int area)
{
	int alloc_count;
	int free_count;

	if (!pt) DRM_MEM_ERROR(area, "Attempt to free NULL pointer\n");
	else
#ifdef __FreeBSD__
	free(pt, DRM(M_DRM));
#elif defined(__NetBSD__)
	free(pt, M_DEVBUF);
#endif
	DRM_SPINLOCK(&DRM(mem_lock));
	DRM(mem_stats)[area].bytes_freed += size;
	free_count  = ++DRM(mem_stats)[area].free_count;
	alloc_count =	DRM(mem_stats)[area].succeed_count;
	DRM_SPINUNLOCK(&DRM(mem_lock));
	if (free_count > alloc_count) {
		DRM_MEM_ERROR(area, "Excess frees: %d frees, %d allocs\n",
			      free_count, alloc_count);
	}
}

void *DRM(ioremap)(unsigned long offset, unsigned long size)
{
	void *pt;

	if (!size) {
		DRM_MEM_ERROR(DRM_MEM_MAPPINGS,
			      "Mapping 0 bytes at 0x%08lx\n", offset);
		return NULL;
	}

	if (!(pt = pmap_mapdev(offset, size))) {
		DRM_SPINLOCK(&DRM(mem_lock));
		++DRM(mem_stats)[DRM_MEM_MAPPINGS].fail_count;
		DRM_SPINUNLOCK(&DRM(mem_lock));
		return NULL;
	}
	DRM_SPINLOCK(&DRM(mem_lock));
	++DRM(mem_stats)[DRM_MEM_MAPPINGS].succeed_count;
	DRM(mem_stats)[DRM_MEM_MAPPINGS].bytes_allocated += size;
	DRM_SPINUNLOCK(&DRM(mem_lock));
	return pt;
}

/* unused so far */
#if 0
void *DRM(ioremap_nocache)(unsigned long offset, unsigned long size)
{
	void *pt;

	if (!size) {
		DRM_MEM_ERROR(DRM_MEM_MAPPINGS,
			      "Mapping 0 bytes at 0x%08lx\n", offset);
		return NULL;
	}

	/* FIXME FOR BSD */
	if (!(pt = ioremap_nocache(offset, size))) {
		DRM_SPINLOCK(&DRM(mem_lock));
		++DRM(mem_stats)[DRM_MEM_MAPPINGS].fail_count;
		DRM_SPINUNLOCK(&DRM(mem_lock));
		return NULL;
	}
	DRM_SPINLOCK(&DRM(mem_lock));
	++DRM(mem_stats)[DRM_MEM_MAPPINGS].succeed_count;
	DRM(mem_stats)[DRM_MEM_MAPPINGS].bytes_allocated += size;
	DRM_SPINUNLOCK(&DRM(mem_lock));
	return pt;
}
#endif

void DRM(ioremapfree)(void *pt, unsigned long size)
{
	int alloc_count;
	int free_count;

	if (!pt)
		DRM_MEM_ERROR(DRM_MEM_MAPPINGS,
			      "Attempt to free NULL pointer\n");
	else
		pmap_unmapdev((vm_offset_t) pt, size);

	DRM_SPINLOCK(&DRM(mem_lock));
	DRM(mem_stats)[DRM_MEM_MAPPINGS].bytes_freed += size;
	free_count  = ++DRM(mem_stats)[DRM_MEM_MAPPINGS].free_count;
	alloc_count =	DRM(mem_stats)[DRM_MEM_MAPPINGS].succeed_count;
	DRM_SPINUNLOCK(&DRM(mem_lock));
	if (free_count > alloc_count) {
		DRM_MEM_ERROR(DRM_MEM_MAPPINGS,
			      "Excess frees: %d frees, %d allocs\n",
			      free_count, alloc_count);
	}
}

#if __REALLY_HAVE_AGP
agp_memory *DRM(alloc_agp)(int pages, u32 type)
{
	agp_memory *handle;

	if (!pages) {
		DRM_MEM_ERROR(DRM_MEM_TOTALAGP, "Allocating 0 pages\n");
		return NULL;
	}

	if ((handle = DRM(agp_allocate_memory)(pages, type))) {
		DRM_SPINLOCK(&DRM(mem_lock));
		++DRM(mem_stats)[DRM_MEM_TOTALAGP].succeed_count;
		DRM(mem_stats)[DRM_MEM_TOTALAGP].bytes_allocated
			+= pages << PAGE_SHIFT;
		DRM_SPINUNLOCK(&DRM(mem_lock));
		return handle;
	}
	DRM_SPINLOCK(&DRM(mem_lock));
	++DRM(mem_stats)[DRM_MEM_TOTALAGP].fail_count;
	DRM_SPINUNLOCK(&DRM(mem_lock));
	return NULL;
}

int DRM(free_agp)(agp_memory *handle, int pages)
{
	int           alloc_count;
	int           free_count;

	if (!handle) {
		DRM_MEM_ERROR(DRM_MEM_TOTALAGP,
			      "Attempt to free NULL AGP handle\n");
		return DRM_ERR(EINVAL);
	}

	if (DRM(agp_free_memory)(handle)) {
		DRM_SPINLOCK(&DRM(mem_lock));
		free_count  = ++DRM(mem_stats)[DRM_MEM_TOTALAGP].free_count;
		alloc_count =   DRM(mem_stats)[DRM_MEM_TOTALAGP].succeed_count;
		DRM(mem_stats)[DRM_MEM_TOTALAGP].bytes_freed
			+= pages << PAGE_SHIFT;
		DRM_SPINUNLOCK(&DRM(mem_lock));
		if (free_count > alloc_count) {
			DRM_MEM_ERROR(DRM_MEM_TOTALAGP,
				      "Excess frees: %d frees, %d allocs\n",
				      free_count, alloc_count);
		}
		return 0;
	}
	return DRM_ERR(EINVAL);
}

int DRM(bind_agp)(agp_memory *handle, unsigned int start)
{
	int retcode;
	device_t dev = DRM_AGP_FIND_DEVICE();
	struct agp_memory_info info;

	if (!dev)
		return EINVAL;

	if (!handle) {
		DRM_MEM_ERROR(DRM_MEM_BOUNDAGP,
			      "Attempt to bind NULL AGP handle\n");
		return DRM_ERR(EINVAL);
	}

	if (!(retcode = DRM(agp_bind_memory)(handle, start))) {
		DRM_SPINLOCK(&DRM(mem_lock));
		++DRM(mem_stats)[DRM_MEM_BOUNDAGP].succeed_count;
		agp_memory_info(dev, handle, &info);
		DRM(mem_stats)[DRM_MEM_BOUNDAGP].bytes_allocated
			+= info.ami_size;
		DRM_SPINUNLOCK(&DRM(mem_lock));
		return DRM_ERR(0);
	}
	DRM_SPINLOCK(&DRM(mem_lock));
	++DRM(mem_stats)[DRM_MEM_BOUNDAGP].fail_count;
	DRM_SPINUNLOCK(&DRM(mem_lock));
	return DRM_ERR(retcode);
}

int DRM(unbind_agp)(agp_memory *handle)
{
	int alloc_count;
	int free_count;
	int retcode = EINVAL;
	device_t dev = DRM_AGP_FIND_DEVICE();
	struct agp_memory_info info;

	if (!dev)
		return EINVAL;

	if (!handle) {
		DRM_MEM_ERROR(DRM_MEM_BOUNDAGP,
			      "Attempt to unbind NULL AGP handle\n");
		return DRM_ERR(retcode);
	}

	agp_memory_info(dev, handle, &info);

	if ((retcode = DRM(agp_unbind_memory)(handle))) 
		return DRM_ERR(retcode);

	DRM_SPINLOCK(&DRM(mem_lock));
	free_count  = ++DRM(mem_stats)[DRM_MEM_BOUNDAGP].free_count;
	alloc_count = DRM(mem_stats)[DRM_MEM_BOUNDAGP].succeed_count;
	DRM(mem_stats)[DRM_MEM_BOUNDAGP].bytes_freed
		+= info.ami_size;
	DRM_SPINUNLOCK(&DRM(mem_lock));
	if (free_count > alloc_count) {
		DRM_MEM_ERROR(DRM_MEM_BOUNDAGP,
			      "Excess frees: %d frees, %d allocs\n",
			      free_count, alloc_count);
	}
	return DRM_ERR(retcode);
}
#endif
