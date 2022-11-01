/* drm_drv.h -- Generic driver template -*- linux-c -*-
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

/*
 * To use this template, you must at least define the following (samples
 * given for the MGA driver):
 *
 * #define DRIVER_AUTHOR	"VA Linux Systems, Inc."
 *
 * #define DRIVER_NAME		"mga"
 * #define DRIVER_DESC		"Matrox G200/G400"
 * #define DRIVER_DATE		"20001127"
 *
 * #define DRIVER_MAJOR		2
 * #define DRIVER_MINOR		0
 * #define DRIVER_PATCHLEVEL	2
 *
 * #define DRIVER_IOCTL_COUNT	DRM_ARRAY_SIZE( mga_ioctls )
 *
 * #define DRM(x)		mga_##x
 */

#ifndef __MUST_HAVE_AGP
#define __MUST_HAVE_AGP			0
#endif
#ifndef __HAVE_CTX_BITMAP
#define __HAVE_CTX_BITMAP		0
#endif
#ifndef __HAVE_DMA_IRQ
#define __HAVE_DMA_IRQ			0
#endif
#ifndef __HAVE_DMA_QUEUE
#define __HAVE_DMA_QUEUE		0
#endif
#ifndef __HAVE_MULTIPLE_DMA_QUEUES
#define __HAVE_MULTIPLE_DMA_QUEUES	0
#endif
#ifndef __HAVE_DMA_SCHEDULE
#define __HAVE_DMA_SCHEDULE		0
#endif
#ifndef __HAVE_DMA_FLUSH
#define __HAVE_DMA_FLUSH		0
#endif
#ifndef __HAVE_DMA_READY
#define __HAVE_DMA_READY		0
#endif
#ifndef __HAVE_DMA_QUIESCENT
#define __HAVE_DMA_QUIESCENT		0
#endif
#ifndef __HAVE_RELEASE
#define __HAVE_RELEASE			0
#endif
#ifndef __HAVE_COUNTERS
#define __HAVE_COUNTERS			0
#endif
#ifndef __HAVE_SG
#define __HAVE_SG			0
#endif
#ifndef __HAVE_KERNEL_CTX_SWITCH
#define __HAVE_KERNEL_CTX_SWITCH	0
#endif
#ifndef PCI_ANY_ID
#define PCI_ANY_ID	~0
#endif

#ifndef DRIVER_PREINIT
#define DRIVER_PREINIT()
#endif
#ifndef DRIVER_POSTINIT
#define DRIVER_POSTINIT()
#endif
#ifndef DRIVER_PRERELEASE
#define DRIVER_PRERELEASE()
#endif
#ifndef DRIVER_PRETAKEDOWN
#define DRIVER_PRETAKEDOWN()
#endif
#ifndef DRIVER_POSTCLEANUP
#define DRIVER_POSTCLEANUP()
#endif
#ifndef DRIVER_PRESETUP
#define DRIVER_PRESETUP()
#endif
#ifndef DRIVER_POSTSETUP
#define DRIVER_POSTSETUP()
#endif
#ifndef DRIVER_IOCTLS
#define DRIVER_IOCTLS
#endif
#ifndef DRIVER_FOPS
#endif

/*
 * The default number of instances (minor numbers) to initialize.
 */
#ifndef DRIVER_NUM_CARDS
#define DRIVER_NUM_CARDS 1
#endif

#ifdef __FreeBSD__
static int DRM(init)(device_t nbdev);
static void DRM(cleanup)(device_t nbdev);
#elif defined(__NetBSD__)
static int DRM(init)(drm_device_t *);
static void DRM(cleanup)(drm_device_t *);
#endif

#ifdef __FreeBSD__
#define CDEV_MAJOR	145
#define DRIVER_SOFTC(unit) \
	((drm_device_t *) devclass_get_softc(DRM(devclass), unit))

#if __REALLY_HAVE_AGP
MODULE_DEPEND(DRIVER_NAME, agp, 1, 1, 1);
#endif
#if DRM_LINUX
MODULE_DEPEND(DRIVER_NAME, linux, 1, 1, 1);
#endif
#endif /* __FreeBSD__ */

#ifdef __NetBSD__
#define CDEV_MAJOR	90
#define DRIVER_SOFTC(unit) \
	((drm_device_t *) device_lookup(&DRM(_cd), unit))
#endif /* __NetBSD__ */

static drm_ioctl_desc_t		  DRM(ioctls)[] = {
	[DRM_IOCTL_NR(DRM_IOCTL_VERSION)]       = { DRM(version),     0, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_UNIQUE)]    = { DRM(getunique),   0, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_MAGIC)]     = { DRM(getmagic),    0, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_IRQ_BUSID)]     = { DRM(irq_busid),   0, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_MAP)]       = { DRM(getmap),      0, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_CLIENT)]    = { DRM(getclient),   0, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_STATS)]     = { DRM(getstats),    0, 0 },

	[DRM_IOCTL_NR(DRM_IOCTL_SET_UNIQUE)]    = { DRM(setunique),   1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_BLOCK)]         = { DRM(block),       1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_UNBLOCK)]       = { DRM(unblock),     1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AUTH_MAGIC)]    = { DRM(authmagic),   1, 1 },

	[DRM_IOCTL_NR(DRM_IOCTL_ADD_MAP)]       = { DRM(addmap),      1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_RM_MAP)]        = { DRM(rmmap),       1, 0 },

#if __HAVE_CTX_BITMAP
	[DRM_IOCTL_NR(DRM_IOCTL_SET_SAREA_CTX)] = { DRM(setsareactx), 1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_SAREA_CTX)] = { DRM(getsareactx), 1, 0 },
#endif

	[DRM_IOCTL_NR(DRM_IOCTL_ADD_CTX)]       = { DRM(addctx),      1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_RM_CTX)]        = { DRM(rmctx),       1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_MOD_CTX)]       = { DRM(modctx),      1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_CTX)]       = { DRM(getctx),      1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_SWITCH_CTX)]    = { DRM(switchctx),   1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_NEW_CTX)]       = { DRM(newctx),      1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_RES_CTX)]       = { DRM(resctx),      1, 0 },

	[DRM_IOCTL_NR(DRM_IOCTL_ADD_DRAW)]      = { DRM(adddraw),     1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_RM_DRAW)]       = { DRM(rmdraw),      1, 1 },

	[DRM_IOCTL_NR(DRM_IOCTL_LOCK)]	        = { DRM(lock),        1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_UNLOCK)]        = { DRM(unlock),      1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_FINISH)]        = { DRM(finish),      1, 0 },

#if __HAVE_DMA
	[DRM_IOCTL_NR(DRM_IOCTL_ADD_BUFS)]      = { DRM(addbufs),     1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_MARK_BUFS)]     = { DRM(markbufs),    1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_INFO_BUFS)]     = { DRM(infobufs),    1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_MAP_BUFS)]      = { DRM(mapbufs),     1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_FREE_BUFS)]     = { DRM(freebufs),    1, 0 },

	/* The DRM_IOCTL_DMA ioctl should be defined by the driver.
	 */
	[DRM_IOCTL_NR(DRM_IOCTL_CONTROL)]       = { DRM(control),     1, 1 },
#endif

#if __REALLY_HAVE_AGP
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_ACQUIRE)]   = { DRM(agp_acquire), 1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_RELEASE)]   = { DRM(agp_release), 1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_ENABLE)]    = { DRM(agp_enable),  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_INFO)]      = { DRM(agp_info),    1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_ALLOC)]     = { DRM(agp_alloc),   1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_FREE)]      = { DRM(agp_free),    1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_BIND)]      = { DRM(agp_bind),    1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_UNBIND)]    = { DRM(agp_unbind),  1, 1 },
#endif

#if __HAVE_SG
	[DRM_IOCTL_NR(DRM_IOCTL_SG_ALLOC)]      = { DRM(sg_alloc),    1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_SG_FREE)]       = { DRM(sg_free),     1, 1 },
#endif

#if __HAVE_VBL_IRQ
	[DRM_IOCTL_NR(DRM_IOCTL_WAIT_VBLANK)]   = { DRM(wait_vblank), 0, 0 },
#endif

	DRIVER_IOCTLS
};

#define DRIVER_IOCTL_COUNT	DRM_ARRAY_SIZE( DRM(ioctls) )

const char *DRM(find_description)(int vendor, int device);

#ifdef __FreeBSD__
static int DRM(probe)(device_t dev)
{
	const char *s = NULL;

	int pciid=pci_get_devid(dev);
	int vendor = (pciid & 0x0000ffff);
	int device = (pciid & 0xffff0000) >> 16;
	
	s = DRM(find_description)(vendor, device);
	if (s) {
		device_set_desc(dev, s);
		return 0;
	}

	return ENXIO;
}

static int DRM(attach)(device_t dev)
{
	return DRM(init)(dev);
}

static int DRM(detach)(device_t dev)
{
	DRM(cleanup)(dev);
	return 0;
}
static device_method_t DRM(methods)[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		DRM( probe)),
	DEVMETHOD(device_attach,	DRM( attach)),
	DEVMETHOD(device_detach,	DRM( detach)),

	{ 0, 0 }
};

static driver_t DRM(driver) = {
	"drm",
	DRM(methods),
	sizeof(drm_device_t),
};

static devclass_t DRM( devclass);

static struct cdevsw DRM( cdevsw) = {
	/* open */	DRM( open ),
	/* close */	DRM( close ),
	/* read */	DRM( read ),
	/* write */	DRM( write ),
	/* ioctl */	DRM( ioctl ),
	/* poll */	DRM( poll ),
	/* mmap */	DRM( mmap ),
	/* strategy */	nostrategy,
	/* name */	DRIVER_NAME,
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_TTY | D_TRACKCLOSE,
#if __FreeBSD_version >= 500000
	/* kqfilter */	0
#else
	/* bmaj */	-1
#endif
};

#elif defined(__NetBSD__)
int DRM(probe)(struct device *parent, struct cfdata *match, void *aux);
void DRM(attach)(struct device *parent, struct device *self, void *aux);
int DRM(detach)(struct device *self, int flags);
int DRM(activate)(struct device *self, enum devact act);

struct cfattach DRM(_ca) = {
	sizeof(drm_device_t), DRM(probe), 
	DRM(attach), DRM(detach), DRM(activate) };
	
int DRM(probe)(struct device *parent, struct cfdata *match, void *aux)
{
	struct pci_attach_args *pa = aux;
	const char *desc;

	desc = DRM(find_description)(PCI_VENDOR(pa->pa_id), PCI_PRODUCT(pa->pa_id));
	if (desc != NULL)
		return 10;
	return 0;
}

void DRM(attach)(struct device *parent, struct device *self, void *aux)
{
	struct pci_attach_args *pa = aux;
	drm_device_t *dev = (drm_device_t *)self;
	
	memcpy(&dev->pa, aux, sizeof(dev->pa));
	
	DRM_INFO("%s", DRM(find_description)(PCI_VENDOR(pa->pa_id), PCI_PRODUCT(pa->pa_id)));
	DRM(init)(dev);
}

int DRM(detach)(struct device *self, int flags)
{
	DRM(cleanup)((drm_device_t *)self);
	return 0;
}

int DRM(activate)(struct device *self, enum devact act)
{
	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);
		break;

	case DVACT_DEACTIVATE:
		/* FIXME */
		break;
	}
	return (0);
}

#endif

const char *DRM(find_description)(int vendor, int device) {
	const char *s = NULL;
	int i=0, done=0;
	
	while ( !done && (DRM(devicelist)[i].vendor != 0 ) ) {
		if ( (DRM(devicelist)[i].vendor == vendor) &&
		     (DRM(devicelist)[i].device == device) ) {
			done=1;
			if ( DRM(devicelist)[i].supported )
				s = DRM(devicelist)[i].name;
			else
				DRM_INFO("%s not supported\n", DRM(devicelist)[i].name);
		}
		i++;
	}
	return s;
}

static int DRM(setup)( drm_device_t *dev )
{
	int i;

	DRIVER_PRESETUP();
	atomic_set( &dev->ioctl_count, 0 );
	atomic_set( &dev->vma_count, 0 );
	dev->buf_use = 0;
	atomic_set( &dev->buf_alloc, 0 );

#if __HAVE_DMA
	i = DRM(dma_setup)( dev );
	if ( i < 0 )
		return i;
#endif

	dev->counters  = 6 + __HAVE_COUNTERS;
	dev->types[0]  = _DRM_STAT_LOCK;
	dev->types[1]  = _DRM_STAT_OPENS;
	dev->types[2]  = _DRM_STAT_CLOSES;
	dev->types[3]  = _DRM_STAT_IOCTLS;
	dev->types[4]  = _DRM_STAT_LOCKS;
	dev->types[5]  = _DRM_STAT_UNLOCKS;
#ifdef __HAVE_COUNTER6
	dev->types[6]  = __HAVE_COUNTER6;
#endif
#ifdef __HAVE_COUNTER7
	dev->types[7]  = __HAVE_COUNTER7;
#endif
#ifdef __HAVE_COUNTER8
	dev->types[8]  = __HAVE_COUNTER8;
#endif
#ifdef __HAVE_COUNTER9
	dev->types[9]  = __HAVE_COUNTER9;
#endif
#ifdef __HAVE_COUNTER10
	dev->types[10] = __HAVE_COUNTER10;
#endif
#ifdef __HAVE_COUNTER11
	dev->types[11] = __HAVE_COUNTER11;
#endif
#ifdef __HAVE_COUNTER12
	dev->types[12] = __HAVE_COUNTER12;
#endif
#ifdef __HAVE_COUNTER13
	dev->types[13] = __HAVE_COUNTER13;
#endif
#ifdef __HAVE_COUNTER14
	dev->types[14] = __HAVE_COUNTER14;
#endif
#ifdef __HAVE_COUNTER15
	dev->types[14] = __HAVE_COUNTER14;
#endif

	for ( i = 0 ; i < DRM_ARRAY_SIZE(dev->counts) ; i++ )
		atomic_set( &dev->counts[i], 0 );

	for ( i = 0 ; i < DRM_HASH_SIZE ; i++ ) {
		dev->magiclist[i].head = NULL;
		dev->magiclist[i].tail = NULL;
	}

	dev->maplist = DRM(alloc)(sizeof(*dev->maplist),
				  DRM_MEM_MAPS);
	if(dev->maplist == NULL) return DRM_ERR(ENOMEM);
	memset(dev->maplist, 0, sizeof(*dev->maplist));
	TAILQ_INIT(dev->maplist);
	dev->map_count = 0;

	dev->vmalist = NULL;
	dev->lock.hw_lock = NULL;
	dev->lock.lock_queue = 0;
	dev->queue_count = 0;
	dev->queue_reserved = 0;
	dev->queue_slots = 0;
	dev->queuelist = NULL;
	dev->irq = 0;
	dev->context_flag = 0;
	dev->interrupt_flag = 0;
	dev->dma_flag = 0;
	dev->last_context = 0;
	dev->last_switch = 0;
	dev->last_checked = 0;
#if __FreeBSD_version >= 500000
	callout_init( &dev->timer, 1 );
#else
	callout_init( &dev->timer );
#endif
	dev->context_wait = 0;

	dev->ctx_start = 0;
	dev->lck_start = 0;

	dev->buf_rp = dev->buf;
	dev->buf_wp = dev->buf;
	dev->buf_end = dev->buf + DRM_BSZ;
#ifdef __FreeBSD__
	dev->buf_sigio = NULL;
#elif defined(__NetBSD__)
	dev->buf_pgid = 0;
#endif
	dev->buf_readers = 0;
	dev->buf_writers = 0;
	dev->buf_selecting = 0;

	DRM_DEBUG( "\n" );

	/* The kernel's context could be created here, but is now created
	 * in drm_dma_enqueue.	This is more resource-efficient for
	 * hardware that does not do DMA, but may mean that
	 * drm_select_queue fails between the time the interrupt is
	 * initialized and the time the queues are initialized.
	 */
	DRIVER_POSTSETUP();
	return 0;
}


static int DRM(takedown)( drm_device_t *dev )
{
	drm_magic_entry_t *pt, *next;
	drm_map_t *map;
	drm_map_list_entry_t *list;
	drm_vma_entry_t *vma, *vma_next;
	int i;

	DRM_DEBUG( "\n" );

	DRIVER_PRETAKEDOWN();
#if __HAVE_DMA_IRQ
	if ( dev->irq ) DRM(irq_uninstall)( dev );
#endif

	DRM_LOCK;
	callout_stop( &dev->timer );

	if ( dev->devname ) {
		DRM(free)( dev->devname, strlen( dev->devname ) + 1,
			   DRM_MEM_DRIVER );
		dev->devname = NULL;
	}

	if ( dev->unique ) {
		DRM(free)( dev->unique, strlen( dev->unique ) + 1,
			   DRM_MEM_DRIVER );
		dev->unique = NULL;
		dev->unique_len = 0;
	}
				/* Clear pid list */
	for ( i = 0 ; i < DRM_HASH_SIZE ; i++ ) {
		for ( pt = dev->magiclist[i].head ; pt ; pt = next ) {
			next = pt->next;
			DRM(free)( pt, sizeof(*pt), DRM_MEM_MAGIC );
		}
		dev->magiclist[i].head = dev->magiclist[i].tail = NULL;
	}

#if __REALLY_HAVE_AGP
				/* Clear AGP information */
	if ( dev->agp ) {
		drm_agp_mem_t *entry;
		drm_agp_mem_t *nexte;

				/* Remove AGP resources, but leave dev->agp
                                   intact until drv_cleanup is called. */
		for ( entry = dev->agp->memory ; entry ; entry = nexte ) {
			nexte = entry->next;
			if ( entry->bound ) DRM(unbind_agp)( entry->handle );
			DRM(free_agp)( entry->handle, entry->pages );
			DRM(free)( entry, sizeof(*entry), DRM_MEM_AGPLISTS );
		}
		dev->agp->memory = NULL;

		if ( dev->agp->acquired ) DRM(agp_do_release)();

		dev->agp->acquired = 0;
		dev->agp->enabled  = 0;
	}
#endif

				/* Clear vma list (only built for debugging) */
	if ( dev->vmalist ) {
		for ( vma = dev->vmalist ; vma ; vma = vma_next ) {
			vma_next = vma->next;
			DRM(free)( vma, sizeof(*vma), DRM_MEM_VMAS );
		}
		dev->vmalist = NULL;
	}

	if( dev->maplist ) {
		while ((list=TAILQ_FIRST(dev->maplist))) {
			map = list->map;
			switch ( map->type ) {
			case _DRM_REGISTERS:
			case _DRM_FRAME_BUFFER:
#if __REALLY_HAVE_MTRR
				if ( map->mtrr >= 0 ) {
					int retcode;
#ifdef __FreeBSD__
					int act;
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
					mtrrmap.flags = 0;
					/*mtrrmap.owner = p->p_pid;*/
					/* XXX: Use curproc here? */
					retcode = mtrr_set( &mtrrmap, &one, 
						DRM_CURPROC, MTRR_GETSET_KERNEL);
#endif
					DRM_DEBUG( "mtrr_del=%d\n", retcode );
				}
#endif
				DRM(ioremapfree)( map->handle, map->size );
				break;
			case _DRM_SHM:
				DRM(free)(map->handle,
					       map->size,
					       DRM_MEM_SAREA);
				break;

			case _DRM_AGP:
				/* Do nothing here, because this is all
				 * handled in the AGP/GART driver.
				 */
				break;
                       case _DRM_SCATTER_GATHER:
				/* Handle it, but do nothing, if REALLY_HAVE_SG
				 * isn't defined.
				 */
#if __REALLY_HAVE_SG
				if(dev->sg) {
					DRM(sg_cleanup)(dev->sg);
					dev->sg = NULL;
				}
#endif
				break;
			}
			TAILQ_REMOVE(dev->maplist, list, link);
			DRM(free)(list, sizeof(*list), DRM_MEM_MAPS);
			DRM(free)(map, sizeof(*map), DRM_MEM_MAPS);
		}
		DRM(free)(dev->maplist, sizeof(*dev->maplist), DRM_MEM_MAPS);
		dev->maplist   = NULL;
 	}

#if __HAVE_DMA_QUEUE || __HAVE_MULTIPLE_DMA_QUEUES
	if ( dev->queuelist ) {
		for ( i = 0 ; i < dev->queue_count ; i++ ) {
			DRM(waitlist_destroy)( &dev->queuelist[i]->waitlist );
			if ( dev->queuelist[i] ) {
				DRM(free)( dev->queuelist[i],
					  sizeof(*dev->queuelist[0]),
					  DRM_MEM_QUEUES );
				dev->queuelist[i] = NULL;
			}
		}
		DRM(free)( dev->queuelist,
			  dev->queue_slots * sizeof(*dev->queuelist),
			  DRM_MEM_QUEUES );
		dev->queuelist = NULL;
	}
	dev->queue_count = 0;
#endif

#if __HAVE_DMA
	DRM(dma_takedown)( dev );
#endif
	if ( dev->lock.hw_lock ) {
		dev->lock.hw_lock = NULL; /* SHM removed */
		dev->lock.pid = 0;
		DRM_WAKEUP_INT((void *)&dev->lock.lock_queue);
	}
	DRM_UNLOCK;

	return 0;
}

/* linux: drm_init is called via init_module at module load time, or via
 *        linux/init/main.c (this is not currently supported).
 * bsd:   drm_init is called via the attach function per device.
 */
#ifdef __FreeBSD__
static int DRM(init)( device_t nbdev )
#elif defined(__NetBSD__)
static int DRM(init)( drm_device_t *dev )
#endif
{
	int unit;
#ifdef __FreeBSD__
	drm_device_t *dev;
#endif
#if __HAVE_CTX_BITMAP
	int retcode;
#endif
	DRM_DEBUG( "\n" );
	DRIVER_PREINIT();

#ifdef __FreeBSD__
	unit = device_get_unit(nbdev);
	dev = device_get_softc(nbdev);
	memset( (void *)dev, 0, sizeof(*dev) );
	dev->device = nbdev;
	dev->devnode = make_dev( &DRM(cdevsw),
			unit,
			DRM_DEV_UID,
			DRM_DEV_GID,
			DRM_DEV_MODE,
			"dri/card%d", unit );
#elif defined(__NetBSD__)
	unit = minor(dev->device.dv_unit);
#endif
	DRM_SPININIT(dev->count_lock, "drm device");
	lockinit(&dev->dev_lock, PZERO, "drmlk", 0, 0);
	dev->name = DRIVER_NAME;
	DRM(mem_init)();
	DRM(sysctl_init)(dev);
	TAILQ_INIT(&dev->files);

#if __REALLY_HAVE_AGP
	dev->agp = DRM(agp_init)();
#if __MUST_HAVE_AGP
	if ( dev->agp == NULL ) {
		DRM_ERROR( "Cannot initialize the agpgart module.\n" );
		DRM(sysctl_cleanup)( dev );
#ifdef __FreeBSD__
		destroy_dev(dev->devnode);
#endif
		DRM(takedown)( dev );
		return DRM_ERR(ENOMEM);
	}
#endif /* __MUST_HAVE_AGP */
#if __REALLY_HAVE_MTRR
	if (dev->agp) {
#ifdef __FreeBSD__
		int retcode = 0, act;
		struct mem_range_desc mrdesc;
		mrdesc.mr_base = dev->agp->info.ai_aperture_base;
		mrdesc.mr_len = dev->agp->info.ai_aperture_size;
		mrdesc.mr_flags = MDF_WRITECOMBINE;
		act = MEMRANGE_SET_UPDATE;
		bcopy(DRIVER_NAME, &mrdesc.mr_owner, strlen(DRIVER_NAME));
		retcode = mem_range_attr_set(&mrdesc, &act);
		dev->agp->agp_mtrr=1;
#elif defined __NetBSD__
		struct mtrr mtrrmap;
		int one = 1;
		mtrrmap.base = dev->agp->info.ai_aperture_base;
		/* Might need a multiplier here XXX */
		mtrrmap.len = dev->agp->info.ai_aperture_size;
		mtrrmap.type = MTRR_TYPE_WC;
		mtrrmap.flags = MTRR_VALID;
		dev->agp->agp_mtrr = mtrr_set( &mtrrmap, &one, NULL, MTRR_GETSET_KERNEL);
#endif /* __NetBSD__ */
	}
#endif /* __REALLY_HAVE_MTRR */
#endif /* __REALLY_HAVE_AGP */

#if __HAVE_CTX_BITMAP
	retcode = DRM(ctxbitmap_init)( dev );
	if( retcode ) {
		DRM_ERROR( "Cannot allocate memory for context bitmap.\n" );
		DRM(sysctl_cleanup)( dev );
#ifdef __FreeBSD__
		destroy_dev(dev->devnode);
#endif
		DRM(takedown)( dev );
		return retcode;
	}
#endif
	DRM_INFO( "Initialized %s %d.%d.%d %s on minor %d\n",
	  	DRIVER_NAME,
	  	DRIVER_MAJOR,
	  	DRIVER_MINOR,
	  	DRIVER_PATCHLEVEL,
	  	DRIVER_DATE,
	  	unit );

	DRIVER_POSTINIT();

	return 0;
}

/* linux: drm_cleanup is called via cleanup_module at module unload time.
 * bsd:   drm_cleanup is called per device at module unload time.
 * FIXME: NetBSD
 */
#ifdef __FreeBSD__
static void DRM(cleanup)(device_t nbdev)
#elif defined(__NetBSD__)
static void DRM(cleanup)(drm_device_t *dev)
#endif
{
#ifdef __FreeBSD__
	drm_device_t *dev;
#endif
#if __REALLY_HAVE_MTRR
#ifdef __NetBSD__
	struct mtrr mtrrmap;
	int one = 1;
#endif /* __NetBSD__ */
#endif /* __REALLY_HAVE_MTRR */

	DRM_DEBUG( "\n" );

#ifdef __FreeBSD__
	dev = device_get_softc(nbdev);
#endif
	DRM(sysctl_cleanup)( dev );
#ifdef __FreeBSD__
	destroy_dev(dev->devnode);
#endif
#if __HAVE_CTX_BITMAP
	DRM(ctxbitmap_cleanup)( dev );
#endif

#if __REALLY_HAVE_AGP && __REALLY_HAVE_MTRR
	if ( dev->agp && dev->agp->agp_mtrr >= 0) {
#if defined(__NetBSD__)
		mtrrmap.base = dev->agp->info.ai_aperture_base;
		mtrrmap.len = dev->agp->info.ai_aperture_size;
		mtrrmap.type = 0;
		mtrrmap.flags = 0;
		retval = mtrr_set( &mtrrmap, &one, NULL, MTRR_GETSET_KERNEL);
#endif
	}
#endif

	DRM(takedown)( dev );

#if __REALLY_HAVE_AGP
	if ( dev->agp ) {
		DRM(agp_uninit)();
		DRM(free)( dev->agp, sizeof(*dev->agp), DRM_MEM_AGPLISTS );
		dev->agp = NULL;
	}
#endif
	DRIVER_POSTCLEANUP();
	DRM(mem_uninit)();
	DRM_SPINUNINIT(dev->count_lock);
}


int DRM(version)( DRM_IOCTL_ARGS )
{
	drm_version_t version;
	int len;

	DRM_COPY_FROM_USER_IOCTL( version, (drm_version_t *)data, sizeof(version) );

#define DRM_COPY( name, value )						\
	len = strlen( value );						\
	if ( len > name##_len ) len = name##_len;			\
	name##_len = strlen( value );					\
	if ( len && name ) {						\
		if ( DRM_COPY_TO_USER( name, value, len ) )		\
			return DRM_ERR(EFAULT);				\
	}

	version.version_major = DRIVER_MAJOR;
	version.version_minor = DRIVER_MINOR;
	version.version_patchlevel = DRIVER_PATCHLEVEL;

	DRM_COPY( version.name, DRIVER_NAME );
	DRM_COPY( version.date, DRIVER_DATE );
	DRM_COPY( version.desc, DRIVER_DESC );

	DRM_COPY_TO_USER_IOCTL( (drm_version_t *)data, version, sizeof(version) );

	return 0;
}

int DRM(open)(dev_t kdev, int flags, int fmt, DRM_STRUCTPROC *p)
{
	drm_device_t *dev = NULL;
	int retcode = 0;

	dev = DRIVER_SOFTC(minor(kdev));

	DRM_DEBUG( "open_count = %d\n", dev->open_count );

	retcode = DRM(open_helper)(kdev, flags, fmt, p, dev);

	if ( !retcode ) {
		atomic_inc( &dev->counts[_DRM_STAT_OPENS] );
		DRM_SPINLOCK( &dev->count_lock );
#ifdef __FreeBSD__
		device_busy(dev->device);
#endif
		if ( !dev->open_count++ )
			retcode = DRM(setup)( dev );
		DRM_SPINUNLOCK( &dev->count_lock );
	}

	return retcode;
}

int DRM(close)(dev_t kdev, int flags, int fmt, DRM_STRUCTPROC *p)
{
	drm_file_t *priv;
	DRM_DEVICE;
	int retcode = 0;

	DRM_DEBUG( "open_count = %d\n", dev->open_count );
	priv = DRM(find_file_by_proc)(dev, p);
	if (!priv) {
		DRM_DEBUG("can't find authenticator\n");
		return EINVAL;
	}

	DRIVER_PRERELEASE();

	/* ========================================================
	 * Begin inline drm_release
	 */

#ifdef __FreeBSD__
	DRM_DEBUG( "pid = %d, device = 0x%lx, open_count = %d\n",
		   DRM_CURRENTPID, (long)dev->device, dev->open_count );
#elif defined(__NetBSD__)
	DRM_DEBUG( "pid = %d, device = 0x%lx, open_count = %d\n",
		   DRM_CURRENTPID, (long)&dev->device, dev->open_count);
#endif

	if (dev->lock.hw_lock && _DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock)
	    && dev->lock.pid == DRM_CURRENTPID) {
		DRM_DEBUG("Process %d dead, freeing lock for context %d\n",
			  DRM_CURRENTPID,
			  _DRM_LOCKING_CONTEXT(dev->lock.hw_lock->lock));
#if HAVE_DRIVER_RELEASE
		DRIVER_RELEASE();
#endif
		DRM(lock_free)(dev,
			      &dev->lock.hw_lock->lock,
			      _DRM_LOCKING_CONTEXT(dev->lock.hw_lock->lock));
		
				/* FIXME: may require heavy-handed reset of
                                   hardware at this point, possibly
                                   processed via a callback to the X
                                   server. */
	}
#if __HAVE_RELEASE
	else if ( dev->lock.hw_lock ) {
		/* The lock is required to reclaim buffers */
		for (;;) {
			if ( !dev->lock.hw_lock ) {
				/* Device has been unregistered */
				retcode = DRM_ERR(EINTR);
				break;
			}
			if ( DRM(lock_take)( &dev->lock.hw_lock->lock,
					     DRM_KERNEL_CONTEXT ) ) {
				dev->lock.pid       = p->p_pid;
				dev->lock.lock_time = jiffies;
                                atomic_inc( &dev->counts[_DRM_STAT_LOCKS] );
				break;	/* Got lock */
			}
				/* Contention */
#if 0
			atomic_inc( &dev->total_sleeps );
#endif
			retcode = tsleep(&dev->lock.lock_queue,
					PZERO|PCATCH,
					"drmlk2",
					0);
			if (retcode)
				break;
		}
		if( !retcode ) {
			DRIVER_RELEASE();
			DRM(lock_free)( dev, &dev->lock.hw_lock->lock,
					DRM_KERNEL_CONTEXT );
		}
	}
#elif __HAVE_DMA
	DRM(reclaim_buffers)( dev, priv->pid );
#endif

#if defined (__FreeBSD__) && (__FreeBSD_version >= 500000)
	funsetown(&dev->buf_sigio);
#elif defined(__FreeBSD__)
	funsetown(dev->buf_sigio);
#elif defined(__NetBSD__)
	dev->buf_pgid = 0;
#endif /* __NetBSD__ */

	DRM_LOCK;
	priv = DRM(find_file_by_proc)(dev, p);
	if (priv) {
		priv->refs--;
		if (!priv->refs) {
			TAILQ_REMOVE(&dev->files, priv, link);
			DRM(free)( priv, sizeof(*priv), DRM_MEM_FILES );
		}
	}
	DRM_UNLOCK;


	/* ========================================================
	 * End inline drm_release
	 */

	atomic_inc( &dev->counts[_DRM_STAT_CLOSES] );
	DRM_SPINLOCK( &dev->count_lock );
#ifdef __FreeBSD__
	device_unbusy(dev->device);
#endif
	if ( !--dev->open_count ) {
		if ( atomic_read( &dev->ioctl_count ) || dev->blocked ) {
			DRM_ERROR( "Device busy: %ld %d\n",
				(unsigned long)atomic_read( &dev->ioctl_count ),
				   dev->blocked );
			DRM_SPINUNLOCK( &dev->count_lock );
			return DRM_ERR(EBUSY);
		}
		DRM_SPINUNLOCK( &dev->count_lock );
		return DRM(takedown)( dev );
	}
	DRM_SPINUNLOCK( &dev->count_lock );
	
	return retcode;
}

/* DRM(ioctl) is called whenever a process performs an ioctl on /dev/drm.
 */
int DRM(ioctl)( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	int retcode = 0;
	drm_ioctl_desc_t *ioctl;
	d_ioctl_t *func;
	int nr = DRM_IOCTL_NR(cmd);
	DRM_PRIV;

	atomic_inc( &dev->ioctl_count );
	atomic_inc( &dev->counts[_DRM_STAT_IOCTLS] );
	++priv->ioctl_count;

#ifdef __FreeBSD__
	DRM_DEBUG( "pid=%d, cmd=0x%02lx, nr=0x%02x, dev 0x%lx, auth=%d\n",
		 DRM_CURRENTPID, cmd, nr, (long)dev->device, priv->authenticated );
#elif defined(__NetBSD__)
	DRM_DEBUG( "pid=%d, cmd=0x%02lx, nr=0x%02x, dev 0x%lx, auth=%d\n",
		 DRM_CURRENTPID, cmd, nr, (long)&dev->device, priv->authenticated );
#endif

	switch (cmd) {
	case FIONBIO:
		atomic_dec(&dev->ioctl_count);
		return 0;

	case FIOASYNC:
		atomic_dec(&dev->ioctl_count);
		dev->flags |= FASYNC;
		return 0;

#ifdef __FreeBSD__
	case FIOSETOWN:
		atomic_dec(&dev->ioctl_count);
		return fsetown(*(int *)data, &dev->buf_sigio);

	case FIOGETOWN:
		atomic_dec(&dev->ioctl_count);
#if (__FreeBSD_version >= 500000)
		*(int *) data = fgetown(&dev->buf_sigio);
#else
		*(int *) data = fgetown(dev->buf_sigio);
#endif
		return 0;
	}
#endif /* __FreeBSD__ */
#ifdef __NetBSD__
	case TIOCSPGRP:
		atomic_dec(&dev->ioctl_count);
		dev->buf_pgid = *(int *)data;
		return 0;

	case TIOCGPGRP:
		atomic_dec(&dev->ioctl_count);
		*(int *)data = dev->buf_pgid;
		return 0;
#endif /* __NetBSD__ */

	if ( nr >= DRIVER_IOCTL_COUNT ) {
		retcode = EINVAL;
	} else {
		ioctl = &DRM(ioctls)[nr];
		func = ioctl->func;

		if ( !func ) {
			DRM_DEBUG( "no function\n" );
			retcode = EINVAL;
		} else if ( ( ioctl->root_only && DRM_SUSER(p) ) 
			 || ( ioctl->auth_needed && !priv->authenticated ) ) {
			retcode = EACCES;
		} else {
			retcode = func( kdev, cmd, data, flags, p );
		}
	}

	atomic_dec( &dev->ioctl_count );
	return DRM_ERR(retcode);
}

int DRM(lock)( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
        drm_lock_t lock;
        int ret = 0;
#if __HAVE_MULTIPLE_DMA_QUEUES
	drm_queue_t *q;
#endif
#if __HAVE_DMA_HISTOGRAM
        cycles_t start;

        dev->lck_start = start = get_cycles();
#endif

	DRM_COPY_FROM_USER_IOCTL( lock, (drm_lock_t *)data, sizeof(lock) );

        if ( lock.context == DRM_KERNEL_CONTEXT ) {
                DRM_ERROR( "Process %d using kernel context %d\n",
			   DRM_CURRENTPID, lock.context );
                return DRM_ERR(EINVAL);
        }

        DRM_DEBUG( "%d (pid %d) requests lock (0x%08x), flags = 0x%08x\n",
		   lock.context, DRM_CURRENTPID,
		   dev->lock.hw_lock->lock, lock.flags );

#if __HAVE_DMA_QUEUE
        if ( lock.context < 0 )
                return DRM_ERR(EINVAL);
#elif __HAVE_MULTIPLE_DMA_QUEUES
        if ( lock.context < 0 || lock.context >= dev->queue_count )
                return DRM_ERR(EINVAL);
	q = dev->queuelist[lock.context];
#endif

#if __HAVE_DMA_FLUSH
	ret = DRM(flush_block_and_flush)( dev, lock.context, lock.flags );
#endif
        if ( !ret ) {
                for (;;) {
                        if ( !dev->lock.hw_lock ) {
                                /* Device has been unregistered */
                                ret = EINTR;
                                break;
                        }
                        if ( DRM(lock_take)( &dev->lock.hw_lock->lock,
					     lock.context ) ) {
                                dev->lock.pid       = DRM_CURRENTPID;
                                dev->lock.lock_time = jiffies;
                                atomic_inc( &dev->counts[_DRM_STAT_LOCKS] );
                                break;  /* Got lock */
                        }

                                /* Contention */
			ret = tsleep((void *)&dev->lock.lock_queue,
					PZERO|PCATCH,
					"drmlk2",
					0);
			if (ret)
				break;
                }
        }

#if __HAVE_DMA_FLUSH
	DRM(flush_unblock)( dev, lock.context, lock.flags ); /* cleanup phase */
#endif

        if ( !ret ) {

#if __HAVE_DMA_READY
                if ( lock.flags & _DRM_LOCK_READY ) {
			DRIVER_DMA_READY();
		}
#endif
#if __HAVE_DMA_QUIESCENT
                if ( lock.flags & _DRM_LOCK_QUIESCENT ) {
			DRIVER_DMA_QUIESCENT();
		}
#endif
#if __HAVE_KERNEL_CTX_SWITCH
		if ( dev->last_context != lock.context ) {
			DRM(context_switch)(dev, dev->last_context,
					    lock.context);
		}
#endif
        }

        DRM_DEBUG( "%d %s\n", lock.context, ret ? "interrupted" : "has lock" );

#if __HAVE_DMA_HISTOGRAM
        atomic_inc(&dev->histo.lacq[DRM(histogram_slot)(get_cycles()-start)]);
#endif

	return DRM_ERR(ret);
}


int DRM(unlock)( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_lock_t lock;

	DRM_COPY_FROM_USER_IOCTL( lock, (drm_lock_t *)data, sizeof(lock) ) ;

	if ( lock.context == DRM_KERNEL_CONTEXT ) {
		DRM_ERROR( "Process %d using kernel context %d\n",
			   DRM_CURRENTPID, lock.context );
		return DRM_ERR(EINVAL);
	}

	atomic_inc( &dev->counts[_DRM_STAT_UNLOCKS] );

#if __HAVE_KERNEL_CTX_SWITCH
	/* We no longer really hold it, but if we are the next
	 * agent to request it then we should just be able to
	 * take it immediately and not eat the ioctl.
	 */
	dev->lock.pid = 0;
	{
		__volatile__ unsigned int *plock = &dev->lock.hw_lock->lock;
		unsigned int old, new, prev, ctx;

		ctx = lock.context;
		do {
			old  = *plock;
			new  = ctx;
			prev = cmpxchg(plock, old, new);
		} while (prev != old);
	}
	wake_up_interruptible(&dev->lock.lock_queue);
#else
	DRM(lock_transfer)( dev, &dev->lock.hw_lock->lock,
			    DRM_KERNEL_CONTEXT );
#if __HAVE_DMA_SCHEDULE
	DRM(dma_schedule)( dev, 1 );
#endif

	/* FIXME: Do we ever really need to check this?
	 */
	if ( 1 /* !dev->context_flag */ ) {
		if ( DRM(lock_free)( dev, &dev->lock.hw_lock->lock,
				     DRM_KERNEL_CONTEXT ) ) {
			DRM_ERROR( "\n" );
		}
	}
#endif /* !__HAVE_KERNEL_CTX_SWITCH */

	return 0;
}

#if DRM_LINUX
#define LINUX_IOCTL_DRM_MIN		0x6400
#define LINUX_IOCTL_DRM_MAX		0x64ff

static linux_ioctl_function_t DRM( linux_ioctl);
static struct linux_ioctl_handler DRM( handler) = {DRM( linux_ioctl), LINUX_IOCTL_DRM_MIN, LINUX_IOCTL_DRM_MAX};
SYSINIT  (DRM( register),   SI_SUB_KLD, SI_ORDER_MIDDLE, linux_ioctl_register_handler, &DRM( handler));
SYSUNINIT(DRM( unregister), SI_SUB_KLD, SI_ORDER_MIDDLE, linux_ioctl_unregister_handler, &DRM( handler));

#define LINUX_IOC_VOID	IOC_VOID
#define LINUX_IOC_IN	IOC_OUT		/* Linux has the values the other way around */
#define LINUX_IOC_OUT	IOC_IN

/*
 * Linux emulation IOCTL
 */
static int
DRM(linux_ioctl)(DRM_STRUCTPROC *p, struct linux_ioctl_args* args)
{
	u_long		cmd = args->cmd;
#define STK_PARAMS	128
	union {
	    char stkbuf[STK_PARAMS];
	    long align;
	} ubuf;
	caddr_t		data=NULL, memp=NULL;
	u_int		size = IOCPARM_LEN(cmd);
	int		error;
#if (__FreeBSD_version >= 500000)
	struct file	*fp;
#else
	struct file	*fp = p->p_fd->fd_ofiles[args->fd];
#endif
	if ( size > STK_PARAMS ) {
		if ( size > IOCPARM_MAX )
			return EINVAL;
		memp = malloc( (u_long)size, DRM(M_DRM), M_WAITOK );
		data = memp;
	} else {
		data = ubuf.stkbuf;
	}

	if ( cmd & LINUX_IOC_IN ) {
		if ( size ) {
			error = copyin( (caddr_t)args->arg, data, (u_int)size );
			if (error) {
				if ( memp )
					free( data, DRM(M_DRM) );
				return error;
			}
		} else {
			data = (caddr_t)args->arg;
		}
	} else if ( (cmd & LINUX_IOC_OUT) && size ) {
		/*
		 * Zero the buffer so the user always
		 * gets back something deterministic.
		 */
		bzero( data, size );
	} else if ( cmd & LINUX_IOC_VOID ) {
		*(caddr_t *)data = (caddr_t)args->arg;
	}

#if (__FreeBSD_version >= 500000)
	if ( (error = fget( p, args->fd, &fp )) != 0 ) {
		if ( memp )
			free( memp, DRM(M_DRM) );
		return (error);
	}
	error = fo_ioctl( fp, cmd, data, p->td_ucred, p );
	fdrop( fp, p );
#else
	error = fo_ioctl( fp, cmd, data, p );
#endif
	if ( error == 0 && (cmd & LINUX_IOC_OUT) && size )
		error = copyout( data, (caddr_t)args->arg, (u_int)size );
	if ( memp )
		free( memp, DRM(M_DRM) );
	return error;
}
#endif /* DRM_LINUX */
