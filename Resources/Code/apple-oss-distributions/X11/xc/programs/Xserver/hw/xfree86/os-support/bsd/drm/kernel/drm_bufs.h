/* drm_bufs.h -- Generic buffer template -*- linux-c -*-
 * Created: Thu Nov 23 03:10:50 2000 by gareth@valinux.com
 *
 * Copyright 1999, 2000 Precision Insight, Inc., Cedar Park, Texas.
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

#ifndef __HAVE_PCI_DMA
#define __HAVE_PCI_DMA		0
#endif

#ifndef __HAVE_SG
#define __HAVE_SG		0
#endif

#ifndef DRIVER_BUF_PRIV_T
#define DRIVER_BUF_PRIV_T		u32
#endif
#ifndef DRIVER_AGP_BUFFERS_MAP
#if __HAVE_AGP && __HAVE_DMA
#error "You must define DRIVER_AGP_BUFFERS_MAP()"
#else
#define DRIVER_AGP_BUFFERS_MAP( dev )	NULL
#endif
#endif

/*
 * Compute order.  Can be made faster.
 */
int DRM(order)( unsigned long size )
{
	int order;
	unsigned long tmp;

	for ( order = 0, tmp = size ; tmp >>= 1 ; ++order );

	if ( size & ~(1 << order) )
		++order;

	return order;
}

int DRM(addmap)( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_map_t *map;
	drm_map_list_entry_t *list;
	
	if (!(dev->flags & (FREAD|FWRITE)))
		return DRM_ERR(EACCES); /* Require read/write */

	map = (drm_map_t *) DRM(alloc)( sizeof(*map), DRM_MEM_MAPS );
	if ( !map )
		return DRM_ERR(ENOMEM);

	*map = *(drm_map_t *)data;

	/* Only allow shared memory to be removable since we only keep enough
	 * book keeping information about shared memory to allow for removal
	 * when processes fork.
	 */
	if ( (map->flags & _DRM_REMOVABLE) && map->type != _DRM_SHM ) {
		DRM(free)( map, sizeof(*map), DRM_MEM_MAPS );
		return DRM_ERR(EINVAL);
	}
	DRM_DEBUG( "offset = 0x%08lx, size = 0x%08lx, type = %d\n",
		   map->offset, map->size, map->type );
	if ( (map->offset & PAGE_MASK) || (map->size & PAGE_MASK) ) {
		DRM(free)( map, sizeof(*map), DRM_MEM_MAPS );
		return DRM_ERR(EINVAL);
	}
	map->mtrr   = -1;
	map->handle = 0;

	switch ( map->type ) {
	case _DRM_REGISTERS:
	case _DRM_FRAME_BUFFER:
#if !defined(__sparc__) && !defined(__alpha__)
		if ( map->offset + map->size < map->offset
		) {
			DRM(free)( map, sizeof(*map), DRM_MEM_MAPS );
			return DRM_ERR(EINVAL);
		}
#endif
#ifdef __alpha__
		map->offset += dev->hose->mem_space->start;
#endif
#if __REALLY_HAVE_MTRR
		if ( map->type == _DRM_FRAME_BUFFER ||
		     (map->flags & _DRM_WRITE_COMBINING) ) {
#ifdef __FreeBSD__
			int retcode = 0, act;
			struct mem_range_desc mrdesc;
			mrdesc.mr_base = map->offset;
			mrdesc.mr_len = map->size;
			mrdesc.mr_flags = MDF_WRITECOMBINE;
			act = MEMRANGE_SET_UPDATE;
			bcopy(DRIVER_NAME, &mrdesc.mr_owner, strlen(DRIVER_NAME));
			retcode = mem_range_attr_set(&mrdesc, &act);
			map->mtrr=1;
#elif defined __NetBSD__
			struct mtrr mtrrmap;
			int one = 1;
			mtrrmap.base = map->offset;
			mtrrmap.len = map->size;
			mtrrmap.type = MTRR_TYPE_WC;
			mtrrmap.flags = MTRR_PRIVATE | MTRR_VALID;
			mtrrmap.owner = p->p_pid;
			/* USER? KERNEL? XXX */
			map->mtrr = mtrr_get( &mtrrmap, &one, p, MTRR_GETSET_KERNEL );
#endif
		}
#endif /* __REALLY_HAVE_MTRR */
		map->handle = DRM(ioremap)( map->offset, map->size );
		break;

	case _DRM_SHM:
		map->handle = (void *)DRM(alloc)(map->size, DRM_MEM_SAREA);
		DRM_DEBUG( "%ld %d %p\n",
			   map->size, DRM(order)( map->size ), map->handle );
		if ( !map->handle ) {
			DRM(free)( map, sizeof(*map), DRM_MEM_MAPS );
			return DRM_ERR(ENOMEM);
		}
		map->offset = (unsigned long)map->handle;
		if ( map->flags & _DRM_CONTAINS_LOCK ) {
			dev->lock.hw_lock = map->handle; /* Pointer to lock */
		}
		break;
#if __REALLY_HAVE_AGP
	case _DRM_AGP:
#ifdef __alpha__
		map->offset += dev->hose->mem_space->start;
#endif
		map->offset += dev->agp->base;
		map->mtrr   = dev->agp->agp_mtrr; /* for getmap */
		break;
#endif
	case _DRM_SCATTER_GATHER:
		if (!dev->sg) {
			DRM(free)(map, sizeof(*map), DRM_MEM_MAPS);
			return DRM_ERR(EINVAL);
		}
		map->offset = map->offset + dev->sg->handle;
		break;

	default:
		DRM(free)( map, sizeof(*map), DRM_MEM_MAPS );
		return DRM_ERR(EINVAL);
	}

	list = DRM(alloc)(sizeof(*list), DRM_MEM_MAPS);
	if(!list) {
		DRM(free)(map, sizeof(*map), DRM_MEM_MAPS);
		return DRM_ERR(EINVAL);
	}
	memset(list, 0, sizeof(*list));
	list->map = map;

	DRM_LOCK;
	TAILQ_INSERT_TAIL(dev->maplist, list, link);
	DRM_UNLOCK;

	*(drm_map_t *)data = *map;

	if ( map->type != _DRM_SHM ) {
		((drm_map_t *)data)->handle = (void *)map->offset;
	}
	return 0;
}


/* Remove a map private from list and deallocate resources if the mapping
 * isn't in use.
 */

int DRM(rmmap)( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_map_list_entry_t *list;
	drm_map_t *map;
	drm_map_t request;
	int found_maps = 0;

	DRM_COPY_FROM_USER_IOCTL( request, (drm_map_t *)data, sizeof(request) );

	DRM_LOCK;
	TAILQ_FOREACH(list, dev->maplist, link) {
		map = list->map;
		if(map->handle == request.handle &&
		   map->flags & _DRM_REMOVABLE) break;
	}

	/* List has wrapped around to the head pointer, or its empty we didn't
	 * find anything.
	 */
	if(list == NULL) {
		DRM_UNLOCK;
		return DRM_ERR(EINVAL);
	}
	TAILQ_REMOVE(dev->maplist, list, link);
	DRM(free)(list, sizeof(*list), DRM_MEM_MAPS);


	if(!found_maps) {
		switch (map->type) {
		case _DRM_REGISTERS:
		case _DRM_FRAME_BUFFER:
#if __REALLY_HAVE_MTRR
			if (map->mtrr >= 0) {
				int retcode;
#ifdef __FreeBSD__
				int act;
				struct mem_range_desc mrdesc;
				mrdesc.mr_base = map->offset;
				mrdesc.mr_len = map->size;
				mrdesc.mr_flags = MDF_WRITECOMBINE;
				act = MEMRANGE_SET_REMOVE;
				bcopy(DRIVER_NAME, &mrdesc.mr_owner, strlen(DRIVER_NAME));
				retcode = mem_range_attr_set(&mrdesc, &act);
#elif defined __NetBSD__
				struct mtrr mtrrmap;
				int one = 1;
				mtrrmap.base = map->offset;
				mtrrmap.len = map->size;
				mtrrmap.type = 0;
				mtrrmap.flags = 0;
				mtrrmap.owner = p->p_pid;
				retcode = mtrr_set( &mtrrmap, &one, p, MTRR_GETSET_KERNEL);
				DRM_DEBUG("mtrr_del = %d\n", retcode);
#endif
			}
#endif
			DRM(ioremapfree)(map->handle, map->size);
			break;
		case _DRM_SHM:
			DRM(free)( map->handle, map->size, DRM_MEM_SAREA );
			break;
		case _DRM_AGP:
		case _DRM_SCATTER_GATHER:
			break;
		}
		DRM(free)(map, sizeof(*map), DRM_MEM_MAPS);
	}
	DRM_UNLOCK;
	return 0;
}

#if __HAVE_DMA


static void DRM(cleanup_buf_error)(drm_buf_entry_t *entry)
{
	int i;

	if (entry->seg_count) {
		for (i = 0; i < entry->seg_count; i++) {
			DRM(free)((void *)entry->seglist[i],
					entry->buf_size,
					DRM_MEM_DMA);
		}
		DRM(free)(entry->seglist,
			  entry->seg_count *
			  sizeof(*entry->seglist),
			  DRM_MEM_SEGS);

		entry->seg_count = 0;
	}

   	if(entry->buf_count) {
	   	for(i = 0; i < entry->buf_count; i++) {
			if(entry->buflist[i].dev_private) {
				DRM(free)(entry->buflist[i].dev_private,
					  entry->buflist[i].dev_priv_size,
					  DRM_MEM_BUFS);
			}
		}
		DRM(free)(entry->buflist,
			  entry->buf_count *
			  sizeof(*entry->buflist),
			  DRM_MEM_BUFS);

#if __HAVE_DMA_FREELIST
	   	DRM(freelist_destroy)(&entry->freelist);
#endif

		entry->buf_count = 0;
	}
}

#if __REALLY_HAVE_AGP
int DRM(addbufs_agp)( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_desc_t request;
	drm_buf_entry_t *entry;
	drm_buf_t *buf;
	unsigned long offset;
	unsigned long agp_offset;
	int count;
	int order;
	int size;
	int alignment;
	int page_order;
	int total;
	int byte_count;
	int i;
	drm_buf_t **temp_buflist;

	if ( !dma ) return DRM_ERR(EINVAL);

	DRM_COPY_FROM_USER_IOCTL( request, (drm_buf_desc_t *)data, sizeof(request) );

	count = request.count;
	order = DRM(order)( request.size );
	size = 1 << order;

	alignment  = (request.flags & _DRM_PAGE_ALIGN)
		? round_page(size) : size;
	page_order = order - PAGE_SHIFT > 0 ? order - PAGE_SHIFT : 0;
	total = PAGE_SIZE << page_order;

	byte_count = 0;
	agp_offset = dev->agp->base + request.agp_start;

	DRM_DEBUG( "count:      %d\n",  count );
	DRM_DEBUG( "order:      %d\n",  order );
	DRM_DEBUG( "size:       %d\n",  size );
	DRM_DEBUG( "agp_offset: 0x%lx\n", agp_offset );
	DRM_DEBUG( "alignment:  %d\n",  alignment );
	DRM_DEBUG( "page_order: %d\n",  page_order );
	DRM_DEBUG( "total:      %d\n",  total );

	if ( order < DRM_MIN_ORDER || order > DRM_MAX_ORDER ) 
		return DRM_ERR(EINVAL);
	if ( dev->queue_count ) 
		return DRM_ERR(EBUSY); /* Not while in use */

	DRM_SPINLOCK( &dev->count_lock );
	if ( dev->buf_use ) {
		DRM_SPINUNLOCK( &dev->count_lock );
		return DRM_ERR(EBUSY);
	}
	atomic_inc( &dev->buf_alloc );
	DRM_SPINUNLOCK( &dev->count_lock );

	DRM_LOCK;
	entry = &dma->bufs[order];
	if ( entry->buf_count ) {
		DRM_UNLOCK;
		atomic_dec( &dev->buf_alloc );
		return DRM_ERR(ENOMEM); /* May only call once for each order */
	}

	if (count < 0 || count > 4096) {
		DRM_UNLOCK;
		atomic_dec( &dev->buf_alloc );
		return DRM_ERR(EINVAL);
	}

	entry->buflist = DRM(alloc)( count * sizeof(*entry->buflist),
				    DRM_MEM_BUFS );
	if ( !entry->buflist ) {
		DRM_UNLOCK;
		atomic_dec( &dev->buf_alloc );
		return DRM_ERR(ENOMEM);
	}
	memset( entry->buflist, 0, count * sizeof(*entry->buflist) );

	entry->buf_size = size;
	entry->page_order = page_order;

	offset = 0;

	while ( entry->buf_count < count ) {
		buf          = &entry->buflist[entry->buf_count];
		buf->idx     = dma->buf_count + entry->buf_count;
		buf->total   = alignment;
		buf->order   = order;
		buf->used    = 0;

		buf->offset  = (dma->byte_count + offset);
		buf->bus_address = agp_offset + offset;
		buf->address = (void *)(agp_offset + offset);
		buf->next    = NULL;
		buf->waiting = 0;
		buf->pending = 0;
		buf->dma_wait = 0;
		buf->pid     = 0;

		buf->dev_priv_size = sizeof(DRIVER_BUF_PRIV_T);
		buf->dev_private = DRM(alloc)( sizeof(DRIVER_BUF_PRIV_T),
					       DRM_MEM_BUFS );
		if(!buf->dev_private) {
			/* Set count correctly so we free the proper amount. */
			entry->buf_count = count;
			DRM(cleanup_buf_error)(entry);
		}
		memset( buf->dev_private, 0, buf->dev_priv_size );

#if __HAVE_DMA_HISTOGRAM
		buf->time_queued = 0;
		buf->time_dispatched = 0;
		buf->time_completed = 0;
		buf->time_freed = 0;
#endif

		offset += alignment;
		entry->buf_count++;
		byte_count += PAGE_SIZE << page_order;
	}

	DRM_DEBUG( "byte_count: %d\n", byte_count );

	temp_buflist = DRM(realloc)( dma->buflist,
				     dma->buf_count * sizeof(*dma->buflist),
				     (dma->buf_count + entry->buf_count)
				     * sizeof(*dma->buflist),
				     DRM_MEM_BUFS );
	if(!temp_buflist) {
		/* Free the entry because it isn't valid */
		DRM(cleanup_buf_error)(entry);
		DRM_UNLOCK;
		atomic_dec( &dev->buf_alloc );
		return DRM_ERR(ENOMEM);
	}
	dma->buflist = temp_buflist;

	for ( i = 0 ; i < entry->buf_count ; i++ ) {
		dma->buflist[i + dma->buf_count] = &entry->buflist[i];
	}

	dma->buf_count += entry->buf_count;
	dma->byte_count += byte_count;

	DRM_DEBUG( "dma->buf_count : %d\n", dma->buf_count );
	DRM_DEBUG( "entry->buf_count : %d\n", entry->buf_count );

#if __HAVE_DMA_FREELIST
	DRM(freelist_create)( &entry->freelist, entry->buf_count );
	for ( i = 0 ; i < entry->buf_count ; i++ ) {
		DRM(freelist_put)( dev, &entry->freelist, &entry->buflist[i] );
	}
#endif
	DRM_UNLOCK;

	request.count = entry->buf_count;
	request.size = size;

	DRM_COPY_TO_USER_IOCTL( (drm_buf_desc_t *)data, request, sizeof(request) );

	dma->flags = _DRM_DMA_USE_AGP;

	atomic_dec( &dev->buf_alloc );
	return 0;
}
#endif /* __REALLY_HAVE_AGP */

#if __HAVE_PCI_DMA
int DRM(addbufs_pci)( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_desc_t request;
	int count;
	int order;
	int size;
	int total;
	int page_order;
	drm_buf_entry_t *entry;
	unsigned long page;
	drm_buf_t *buf;
	int alignment;
	unsigned long offset;
	int i;
	int byte_count;
	int page_count;
	unsigned long *temp_pagelist;
	drm_buf_t **temp_buflist;

	if ( !dma ) return DRM_ERR(EINVAL);

	DRM_COPY_FROM_USER_IOCTL( request, (drm_buf_desc_t *)data, sizeof(request) );

	count = request.count;
	order = DRM(order)( request.size );
	size = 1 << order;

	DRM_DEBUG( "count=%d, size=%d (%d), order=%d, queue_count=%d\n",
		   request.count, request.size, size,
		   order, dev->queue_count );

	if ( order < DRM_MIN_ORDER || order > DRM_MAX_ORDER ) 
		return DRM_ERR(EINVAL);
	if ( dev->queue_count ) 
		return DRM_ERR(EBUSY); /* Not while in use */

	alignment = (request.flags & _DRM_PAGE_ALIGN)
		? round_page(size) : size;
	page_order = order - PAGE_SHIFT > 0 ? order - PAGE_SHIFT : 0;
	total = PAGE_SIZE << page_order;

	DRM_SPINLOCK( &dev->count_lock );
	if ( dev->buf_use ) {
		DRM_SPINUNLOCK( &dev->count_lock );
		return DRM_ERR(EBUSY);
	}
	atomic_inc( &dev->buf_alloc );
	DRM_SPINUNLOCK( &dev->count_lock );

	DRM_LOCK;
	entry = &dma->bufs[order];
	if ( entry->buf_count ) {
		DRM_UNLOCK;
		atomic_dec( &dev->buf_alloc );
		return DRM_ERR(ENOMEM);	/* May only call once for each order */
	}

	if (count < 0 || count > 4096) {
		DRM_UNLOCK;
		atomic_dec( &dev->buf_alloc );
		return DRM_ERR(EINVAL);
	}

	entry->buflist = DRM(alloc)( count * sizeof(*entry->buflist),
				    DRM_MEM_BUFS );
	if ( !entry->buflist ) {
		DRM_UNLOCK;
		atomic_dec( &dev->buf_alloc );
		return DRM_ERR(ENOMEM);
	}
	memset( entry->buflist, 0, count * sizeof(*entry->buflist) );

	entry->seglist = DRM(alloc)( count * sizeof(*entry->seglist),
				    DRM_MEM_SEGS );
	if ( !entry->seglist ) {
		DRM(free)( entry->buflist,
			  count * sizeof(*entry->buflist),
			  DRM_MEM_BUFS );
		DRM_UNLOCK;
		atomic_dec( &dev->buf_alloc );
		return DRM_ERR(ENOMEM);
	}
	memset( entry->seglist, 0, count * sizeof(*entry->seglist) );

	temp_pagelist = DRM(realloc)( dma->pagelist,
				      dma->page_count * sizeof(*dma->pagelist),
				      (dma->page_count + (count << page_order))
				      * sizeof(*dma->pagelist),
				      DRM_MEM_PAGES );
	if(!temp_pagelist) {
		DRM(free)( entry->buflist,
			   count * sizeof(*entry->buflist),
			   DRM_MEM_BUFS );
		DRM(free)( entry->seglist,
			   count * sizeof(*entry->seglist),
			   DRM_MEM_SEGS );
		DRM_UNLOCK;
		atomic_dec( &dev->buf_alloc );
		return DRM_ERR(ENOMEM);
	}

	dma->pagelist = temp_pagelist;
	DRM_DEBUG( "pagelist: %d entries\n",
		   dma->page_count + (count << page_order) );

	entry->buf_size	= size;
	entry->page_order = page_order;
	byte_count = 0;
	page_count = 0;

	while ( entry->buf_count < count ) {
		page = (unsigned long)DRM(alloc)( size, DRM_MEM_DMA );
		if ( !page ) break;
		entry->seglist[entry->seg_count++] = page;
		for ( i = 0 ; i < (1 << page_order) ; i++ ) {
			DRM_DEBUG( "page %d @ 0x%08lx\n",
				   dma->page_count + page_count,
				   page + PAGE_SIZE * i );
			dma->pagelist[dma->page_count + page_count++]
				= page + PAGE_SIZE * i;
		}
		for ( offset = 0 ;
		      offset + size <= total && entry->buf_count < count ;
		      offset += alignment, ++entry->buf_count ) {
			buf	     = &entry->buflist[entry->buf_count];
			buf->idx     = dma->buf_count + entry->buf_count;
			buf->total   = alignment;
			buf->order   = order;
			buf->used    = 0;
			buf->offset  = (dma->byte_count + byte_count + offset);
			buf->address = (void *)(page + offset);
			buf->next    = NULL;
			buf->waiting = 0;
			buf->pending = 0;
			buf->dma_wait = 0;
			buf->pid     = 0;
#if __HAVE_DMA_HISTOGRAM
			buf->time_queued     = 0;
			buf->time_dispatched = 0;
			buf->time_completed  = 0;
			buf->time_freed      = 0;
#endif
			DRM_DEBUG( "buffer %d @ %p\n",
				   entry->buf_count, buf->address );
		}
		byte_count += PAGE_SIZE << page_order;
	}

	temp_buflist = DRM(realloc)( dma->buflist,
				     dma->buf_count * sizeof(*dma->buflist),
				     (dma->buf_count + entry->buf_count)
				     * sizeof(*dma->buflist),
				     DRM_MEM_BUFS );
	if(!temp_buflist) {
		/* Free the entry because it isn't valid */
		DRM(cleanup_buf_error)(entry);
		DRM_UNLOCK;
		atomic_dec( &dev->buf_alloc );
		return DRM_ERR(ENOMEM);
	}
	dma->buflist = temp_buflist;

	for ( i = 0 ; i < entry->buf_count ; i++ ) {
		dma->buflist[i + dma->buf_count] = &entry->buflist[i];
	}

	dma->buf_count += entry->buf_count;
	dma->seg_count += entry->seg_count;
	dma->page_count += entry->seg_count << page_order;
	dma->byte_count += PAGE_SIZE * (entry->seg_count << page_order);

#if __HAVE_DMA_FREELIST
	DRM(freelist_create)( &entry->freelist, entry->buf_count );
	for ( i = 0 ; i < entry->buf_count ; i++ ) {
		DRM(freelist_put)( dev, &entry->freelist, &entry->buflist[i] );
	}
#endif
	DRM_UNLOCK;

	request.count = entry->buf_count;
	request.size = size;

	DRM_COPY_TO_USER_IOCTL( (drm_buf_desc_t *)data, request, sizeof(request) );

	atomic_dec( &dev->buf_alloc );
	return 0;

}
#endif /* __HAVE_PCI_DMA */

#if __REALLY_HAVE_SG
int DRM(addbufs_sg)( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_desc_t request;
	drm_buf_entry_t *entry;
	drm_buf_t *buf;
	unsigned long offset;
	unsigned long agp_offset;
	int count;
	int order;
	int size;
	int alignment;
	int page_order;
	int total;
	int byte_count;
	int i;
	drm_buf_t **temp_buflist;

	if ( !dma ) return DRM_ERR(EINVAL);

	DRM_COPY_FROM_USER_IOCTL( request, (drm_buf_desc_t *)data, sizeof(request) );

	count = request.count;
	order = DRM(order)( request.size );
	size = 1 << order;

	alignment  = (request.flags & _DRM_PAGE_ALIGN)
		? round_page(size) : size;
	page_order = order - PAGE_SHIFT > 0 ? order - PAGE_SHIFT : 0;
	total = PAGE_SIZE << page_order;

	byte_count = 0;
	agp_offset = request.agp_start;

	DRM_DEBUG( "count:      %d\n",  count );
	DRM_DEBUG( "order:      %d\n",  order );
	DRM_DEBUG( "size:       %d\n",  size );
	DRM_DEBUG( "agp_offset: %ld\n", agp_offset );
	DRM_DEBUG( "alignment:  %d\n",  alignment );
	DRM_DEBUG( "page_order: %d\n",  page_order );
	DRM_DEBUG( "total:      %d\n",  total );

	if ( order < DRM_MIN_ORDER || order > DRM_MAX_ORDER ) 
		return DRM_ERR(EINVAL);
	if ( dev->queue_count ) return DRM_ERR(EBUSY); /* Not while in use */

	DRM_SPINLOCK( &dev->count_lock );
	if ( dev->buf_use ) {
		DRM_SPINUNLOCK( &dev->count_lock );
		return DRM_ERR(EBUSY);
	}
	atomic_inc( &dev->buf_alloc );
	DRM_SPINUNLOCK( &dev->count_lock );

	DRM_LOCK;
	entry = &dma->bufs[order];
	if ( entry->buf_count ) {
		DRM_UNLOCK;
		atomic_dec( &dev->buf_alloc );
		return DRM_ERR(ENOMEM); /* May only call once for each order */
	}

	if (count < 0 || count > 4096) {
		DRM_UNLOCK;
		atomic_dec( &dev->buf_alloc );
		return DRM_ERR(EINVAL);
	}

	entry->buflist = DRM(alloc)( count * sizeof(*entry->buflist),
				     DRM_MEM_BUFS );
	if ( !entry->buflist ) {
		DRM_UNLOCK;
		atomic_dec( &dev->buf_alloc );
		return DRM_ERR(ENOMEM);
	}
	memset( entry->buflist, 0, count * sizeof(*entry->buflist) );

	entry->buf_size = size;
	entry->page_order = page_order;

	offset = 0;

	while ( entry->buf_count < count ) {
		buf          = &entry->buflist[entry->buf_count];
		buf->idx     = dma->buf_count + entry->buf_count;
		buf->total   = alignment;
		buf->order   = order;
		buf->used    = 0;

		buf->offset  = (dma->byte_count + offset);
		buf->bus_address = agp_offset + offset;
		buf->address = (void *)(agp_offset + offset + dev->sg->handle);
		buf->next    = NULL;
		buf->waiting = 0;
		buf->pending = 0;
		buf->dma_wait = 0;
		buf->pid     = 0;

		buf->dev_priv_size = sizeof(DRIVER_BUF_PRIV_T);
		buf->dev_private = DRM(alloc)( sizeof(DRIVER_BUF_PRIV_T),
					       DRM_MEM_BUFS );
		if(!buf->dev_private) {
			/* Set count correctly so we free the proper amount. */
			entry->buf_count = count;
			DRM(cleanup_buf_error)(entry);
			DRM_UNLOCK;
			atomic_dec( &dev->buf_alloc );
			return DRM_ERR(ENOMEM);
		}

		memset( buf->dev_private, 0, buf->dev_priv_size );

# if __HAVE_DMA_HISTOGRAM
		buf->time_queued = 0;
		buf->time_dispatched = 0;
		buf->time_completed = 0;
		buf->time_freed = 0;
# endif
		DRM_DEBUG( "buffer %d @ %p\n",
			   entry->buf_count, buf->address );

		offset += alignment;
		entry->buf_count++;
		byte_count += PAGE_SIZE << page_order;
	}

	DRM_DEBUG( "byte_count: %d\n", byte_count );

	temp_buflist = DRM(realloc)( dma->buflist,
				     dma->buf_count * sizeof(*dma->buflist),
				     (dma->buf_count + entry->buf_count)
				     * sizeof(*dma->buflist),
				     DRM_MEM_BUFS );
	if(!temp_buflist) {
		/* Free the entry because it isn't valid */
		DRM(cleanup_buf_error)(entry);
		DRM_UNLOCK;
		atomic_dec( &dev->buf_alloc );
		return DRM_ERR(ENOMEM);
	}
	dma->buflist = temp_buflist;

	for ( i = 0 ; i < entry->buf_count ; i++ ) {
		dma->buflist[i + dma->buf_count] = &entry->buflist[i];
	}

	dma->buf_count += entry->buf_count;
	dma->byte_count += byte_count;

	DRM_DEBUG( "dma->buf_count : %d\n", dma->buf_count );
	DRM_DEBUG( "entry->buf_count : %d\n", entry->buf_count );

#if __HAVE_DMA_FREELIST
	DRM(freelist_create)( &entry->freelist, entry->buf_count );
	for ( i = 0 ; i < entry->buf_count ; i++ ) {
		DRM(freelist_put)( dev, &entry->freelist, &entry->buflist[i] );
	}
#endif
	DRM_UNLOCK;

	request.count = entry->buf_count;
	request.size = size;

	DRM_COPY_TO_USER_IOCTL( (drm_buf_desc_t *)data, request, sizeof(request) );

	dma->flags = _DRM_DMA_USE_SG;

	atomic_dec( &dev->buf_alloc );
	return 0;
}
#endif /* __REALLY_HAVE_SG */

int DRM(addbufs)( DRM_IOCTL_ARGS )
{
	drm_buf_desc_t request;

	DRM_COPY_FROM_USER_IOCTL( request, (drm_buf_desc_t *)data, sizeof(request) );

#if __REALLY_HAVE_AGP
	if ( request.flags & _DRM_AGP_BUFFER )
		return DRM(addbufs_agp)( kdev, cmd, data, flags, p );
	else
#endif
#if __REALLY_HAVE_SG
	if ( request.flags & _DRM_SG_BUFFER )
		return DRM(addbufs_sg)( kdev, cmd, data, flags, p );
	else
#endif
#if __HAVE_PCI_DMA
		return DRM(addbufs_pci)( kdev, cmd, data, flags, p );
#else
		return DRM_ERR(EINVAL);
#endif
}

int DRM(infobufs)( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_info_t request;
	int i;
	int count;

	if ( !dma ) return DRM_ERR(EINVAL);

	DRM_SPINLOCK( &dev->count_lock );
	if ( atomic_read( &dev->buf_alloc ) ) {
		DRM_SPINUNLOCK( &dev->count_lock );
		return DRM_ERR(EBUSY);
	}
	++dev->buf_use;		/* Can't allocate more after this call */
	DRM_SPINUNLOCK( &dev->count_lock );

	DRM_COPY_FROM_USER_IOCTL( request, (drm_buf_info_t *)data, sizeof(request) );

	for ( i = 0, count = 0 ; i < DRM_MAX_ORDER + 1 ; i++ ) {
		if ( dma->bufs[i].buf_count ) ++count;
	}

	DRM_DEBUG( "count = %d\n", count );

	if ( request.count >= count ) {
		for ( i = 0, count = 0 ; i < DRM_MAX_ORDER + 1 ; i++ ) {
			if ( dma->bufs[i].buf_count ) {
				drm_buf_desc_t *to = &request.list[count];
				drm_buf_entry_t *from = &dma->bufs[i];
				drm_freelist_t *list = &dma->bufs[i].freelist;
				if ( DRM_COPY_TO_USER( &to->count,
						   &from->buf_count,
						   sizeof(from->buf_count) ) ||
				     DRM_COPY_TO_USER( &to->size,
						   &from->buf_size,
						   sizeof(from->buf_size) ) ||
				     DRM_COPY_TO_USER( &to->low_mark,
						   &list->low_mark,
						   sizeof(list->low_mark) ) ||
				     DRM_COPY_TO_USER( &to->high_mark,
						   &list->high_mark,
						   sizeof(list->high_mark) ) )
					return DRM_ERR(EFAULT);

				DRM_DEBUG( "%d %d %d %d %d\n",
					   i,
					   dma->bufs[i].buf_count,
					   dma->bufs[i].buf_size,
					   dma->bufs[i].freelist.low_mark,
					   dma->bufs[i].freelist.high_mark );
				++count;
			}
		}
	}
	request.count = count;

	DRM_COPY_TO_USER_IOCTL( (drm_buf_info_t *)data, request, sizeof(request) );

	return 0;
}

int DRM(markbufs)( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_desc_t request;
	int order;
	drm_buf_entry_t *entry;

	if ( !dma ) return DRM_ERR(EINVAL);

	DRM_COPY_FROM_USER_IOCTL( request, (drm_buf_desc_t *)data, sizeof(request) );

	DRM_DEBUG( "%d, %d, %d\n",
		   request.size, request.low_mark, request.high_mark );
	order = DRM(order)( request.size );
	if ( order < DRM_MIN_ORDER || order > DRM_MAX_ORDER ) 
		return DRM_ERR(EINVAL);
	entry = &dma->bufs[order];

	if ( request.low_mark < 0 || request.low_mark > entry->buf_count )
		return DRM_ERR(EINVAL);
	if ( request.high_mark < 0 || request.high_mark > entry->buf_count )
		return DRM_ERR(EINVAL);

	entry->freelist.low_mark  = request.low_mark;
	entry->freelist.high_mark = request.high_mark;

	return 0;
}

int DRM(freebufs)( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_free_t request;
	int i;
	int idx;
	drm_buf_t *buf;

	if ( !dma ) return DRM_ERR(EINVAL);

	DRM_COPY_FROM_USER_IOCTL( request, (drm_buf_free_t *)data, sizeof(request) );

	DRM_DEBUG( "%d\n", request.count );
	for ( i = 0 ; i < request.count ; i++ ) {
		if ( DRM_COPY_FROM_USER( &idx,
				     &request.list[i],
				     sizeof(idx) ) )
			return DRM_ERR(EFAULT);
		if ( idx < 0 || idx >= dma->buf_count ) {
			DRM_ERROR( "Index %d (of %d max)\n",
				   idx, dma->buf_count - 1 );
			return DRM_ERR(EINVAL);
		}
		buf = dma->buflist[idx];
		if ( buf->pid != DRM_CURRENTPID ) {
			DRM_ERROR( "Process %d freeing buffer owned by %d\n",
				   DRM_CURRENTPID, buf->pid );
			return DRM_ERR(EINVAL);
		}
		DRM(free_buffer)( dev, buf );
	}

	return 0;
}

int DRM(mapbufs)( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_device_dma_t *dma = dev->dma;
	int retcode = 0;
	const int zero = 0;
	vm_offset_t virtual, address;
#ifdef __FreeBSD__
#if __FreeBSD_version >= 500000
	struct vmspace *vms = p->td_proc->p_vmspace;
#else
	struct vmspace *vms = p->p_vmspace;
#endif
#endif /* __FreeBSD__ */
#ifdef __NetBSD__
	struct vnode *vn;
#endif /* __NetBSD__ */

	drm_buf_map_t request;
	int i;

	if ( !dma ) return DRM_ERR(EINVAL);

	DRM_SPINLOCK( &dev->count_lock );
	if ( atomic_read( &dev->buf_alloc ) ) {
		DRM_SPINUNLOCK( &dev->count_lock );
		return DRM_ERR(EBUSY);
	}
	dev->buf_use++;		/* Can't allocate more after this call */
	DRM_SPINUNLOCK( &dev->count_lock );

	DRM_COPY_FROM_USER_IOCTL( request, (drm_buf_map_t *)data, sizeof(request) );

#ifdef __NetBSD__
	if(!vfinddev(kdev, VCHR, &vn))
		return 0;	/* FIXME: Shouldn't this be EINVAL or something? */
#endif /* __NetBSD__ */

	if ( request.count >= dma->buf_count ) {
		if ( (__HAVE_AGP && (dma->flags & _DRM_DMA_USE_AGP)) ||
		     (__HAVE_SG && (dma->flags & _DRM_DMA_USE_SG)) ) {
			drm_map_t *map = DRIVER_AGP_BUFFERS_MAP( dev );

			if ( !map ) {
				retcode = EINVAL;
				goto done;
			}

#ifdef __FreeBSD__
			virtual = round_page((vm_offset_t)vms->vm_daddr + MAXDSIZ);
			retcode = vm_mmap(&vms->vm_map,
					  &virtual,
					  round_page(map->size),
					  PROT_READ|PROT_WRITE, VM_PROT_ALL,
					  MAP_SHARED,
					  SLIST_FIRST(&kdev->si_hlist),
					  (unsigned long)map->offset );
#elif defined(__NetBSD__)
			virtual = round_page((vaddr_t)vms->vm_daddr + MAXDSIZ);
			retcode = uvm_mmap(&vms->vm_map,
					   (vaddr_t *)&virtual,
					   round_page(map->size),
					   UVM_PROT_READ | UVM_PROT_WRITE,
					   UVM_PROT_ALL, MAP_SHARED,
					   &vn->v_uobj, map->offset,
					   p->p_rlimit[RLIMIT_MEMLOCK].rlim_cur);
#endif /* __NetBSD__ */
		} else {
#ifdef __FreeBSD__
			virtual = round_page((vm_offset_t)vms->vm_daddr + MAXDSIZ);
			retcode = vm_mmap(&vms->vm_map,
					  &virtual,
					  round_page(dma->byte_count),
					  PROT_READ|PROT_WRITE, VM_PROT_ALL,
					  MAP_SHARED,
					  SLIST_FIRST(&kdev->si_hlist),
					  0);
#elif defined(__NetBSD__)
			virtual = round_page((vaddr_t)vms->vm_daddr + MAXDSIZ);
			retcode = uvm_mmap(&vms->vm_map,
					   (vaddr_t *)&virtual,
					   round_page(dma->byte_count),
					   UVM_PROT_READ | UVM_PROT_WRITE,
					   UVM_PROT_ALL, MAP_SHARED,
					   &vn->v_uobj, 0,
					   p->p_rlimit[RLIMIT_MEMLOCK].rlim_cur);
#endif /* __NetBSD__ */
		}
		if (retcode)
			goto done;
		request.virtual = (void *)virtual;

		for ( i = 0 ; i < dma->buf_count ; i++ ) {
			if ( DRM_COPY_TO_USER( &request.list[i].idx,
					   &dma->buflist[i]->idx,
					   sizeof(request.list[0].idx) ) ) {
				retcode = EFAULT;
				goto done;
			}
			if ( DRM_COPY_TO_USER( &request.list[i].total,
					   &dma->buflist[i]->total,
					   sizeof(request.list[0].total) ) ) {
				retcode = EFAULT;
				goto done;
			}
			if ( DRM_COPY_TO_USER( &request.list[i].used,
					   &zero,
					   sizeof(zero) ) ) {
				retcode = EFAULT;
				goto done;
			}
			address = virtual + dma->buflist[i]->offset; /* *** */
			if ( DRM_COPY_TO_USER( &request.list[i].address,
					   &address,
					   sizeof(address) ) ) {
				retcode = EFAULT;
				goto done;
			}
		}
	}
 done:
	request.count = dma->buf_count;

	DRM_DEBUG( "%d buffers, retcode = %d\n", request.count, retcode );

	DRM_COPY_TO_USER_IOCTL( (drm_buf_map_t *)data, request, sizeof(request) );

	return DRM_ERR(retcode);
}

#endif /* __HAVE_DMA */

