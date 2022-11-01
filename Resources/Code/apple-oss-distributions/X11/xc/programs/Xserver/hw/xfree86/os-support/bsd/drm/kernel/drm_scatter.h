/* drm_scatter.h -- IOCTLs to manage scatter/gather memory -*- linux-c -*-
 * Created: Mon Dec 18 23:20:54 2000 by gareth@valinux.com
 *
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
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *   Gareth Hughes <gareth@valinux.com>
 */

#include "drmP.h"

#define DEBUG_SCATTER 0

#if __REALLY_HAVE_SG

void DRM(sg_cleanup)( drm_sg_mem_t *entry )
{
	free( entry->virtual, DRM(M_DRM) );

	DRM(free)( entry->busaddr,
		   entry->pages * sizeof(*entry->busaddr),
		   DRM_MEM_PAGES );
	DRM(free)( entry->pagelist,
		   entry->pages * sizeof(*entry->pagelist),
		   DRM_MEM_PAGES );
	DRM(free)( entry,
		   sizeof(*entry),
		   DRM_MEM_SGLISTS );
}

int DRM(sg_alloc)( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_scatter_gather_t request;
	drm_sg_mem_t *entry;
	unsigned long pages;

	DRM_DEBUG( "%s\n", __FUNCTION__ );

	if ( dev->sg )
		return EINVAL;

	DRM_COPY_FROM_USER_IOCTL(request, (drm_scatter_gather_t *)data,
			     sizeof(request) );

	entry = DRM(alloc)( sizeof(*entry), DRM_MEM_SGLISTS );
	if ( !entry )
		return ENOMEM;

   	bzero( entry, sizeof(*entry) );

	pages = round_page(request.size) / PAGE_SIZE;
	DRM_DEBUG( "sg size=%ld pages=%ld\n", request.size, pages );

	entry->pages = pages;
	entry->pagelist = DRM(alloc)( pages * sizeof(*entry->pagelist),
				     DRM_MEM_PAGES );
	if ( !entry->pagelist ) {
		DRM(free)( entry, sizeof(*entry), DRM_MEM_SGLISTS );
		return ENOMEM;
	}

	bzero(entry->pagelist, pages * sizeof(*entry->pagelist));

	entry->busaddr = DRM(alloc)( pages * sizeof(*entry->busaddr),
				     DRM_MEM_PAGES );
	if ( !entry->busaddr ) {
		DRM(free)( entry->pagelist,
			   entry->pages * sizeof(*entry->pagelist),
			   DRM_MEM_PAGES );
		DRM(free)( entry,
			   sizeof(*entry),
			   DRM_MEM_SGLISTS );
		return ENOMEM;
	}
	bzero( (void *)entry->busaddr, pages * sizeof(*entry->busaddr) );

	entry->virtual = malloc( pages << PAGE_SHIFT, DRM(M_DRM), M_WAITOK );
	if ( !entry->virtual ) {
		DRM(free)( entry->busaddr,
			   entry->pages * sizeof(*entry->busaddr),
			   DRM_MEM_PAGES );
		DRM(free)( entry->pagelist,
			   entry->pages * sizeof(*entry->pagelist),
			   DRM_MEM_PAGES );
		DRM(free)( entry,
			   sizeof(*entry),
			   DRM_MEM_SGLISTS );
		return ENOMEM;
	}

	bzero( entry->virtual, pages << PAGE_SHIFT );

	entry->handle = (unsigned long)entry->virtual;

	DRM_DEBUG( "sg alloc handle  = %08lx\n", entry->handle );
	DRM_DEBUG( "sg alloc virtual = %p\n", entry->virtual );

	request.handle = entry->handle;

	DRM_COPY_TO_USER_IOCTL( (drm_scatter_gather_t *)data,
			   request,
			   sizeof(request) );

	dev->sg = entry;

#if DEBUG_SCATTER
	/* Verify that each page points to its virtual address, and vice
	 * versa.
	 */
	{
	int error = 0;

	for ( i = 0 ; i < pages ; i++ ) {
		unsigned long *tmp;

		tmp = page_address( entry->pagelist[i] );
		for ( j = 0 ;
		      j < PAGE_SIZE / sizeof(unsigned long) ;
		      j++, tmp++ ) {
			*tmp = 0xcafebabe;
		}
		tmp = (unsigned long *)((u8 *)entry->virtual +
					(PAGE_SIZE * i));
		for( j = 0 ;
		     j < PAGE_SIZE / sizeof(unsigned long) ;
		     j++, tmp++ ) {
			if ( *tmp != 0xcafebabe && error == 0 ) {
				error = 1;
				DRM_ERROR( "Scatter allocation error, "
					   "pagelist does not match "
					   "virtual mapping\n" );
			}
		}
		tmp = page_address( entry->pagelist[i] );
		for(j = 0 ;
		    j < PAGE_SIZE / sizeof(unsigned long) ;
		    j++, tmp++) {
			*tmp = 0;
		}
	}
	if (error == 0)
		DRM_ERROR( "Scatter allocation matches pagelist\n" );
	}
#endif

	return 0;

	DRM(sg_cleanup)( entry );
	return ENOMEM;
}

int DRM(sg_free)( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_scatter_gather_t request;
	drm_sg_mem_t *entry;

	DRM_COPY_FROM_USER_IOCTL( request, (drm_scatter_gather_t *)data,
			     sizeof(request) );

	entry = dev->sg;
	dev->sg = NULL;

	if ( !entry || entry->handle != request.handle )
		return EINVAL;

	DRM_DEBUG( "sg free virtual  = %p\n", entry->virtual );

	DRM(sg_cleanup)( entry );

	return 0;
}

#else /* __REALLY_HAVE_SG */

int DRM(sg_alloc)( DRM_IOCTL_ARGS )
{
	return DRM_ERR(EINVAL);
}
int DRM(sg_free)( DRM_IOCTL_ARGS )
{
	return DRM_ERR(EINVAL);
}

#endif
