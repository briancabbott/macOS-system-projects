/* $XFree86: xc/lib/GL/mesa/src/drv/ffb/ffb_xmesa.h,v 1.2 2002/02/22 21:32:59 dawes Exp $ */

#ifndef _FFB_XMESA_H_
#define _FFB_XMESA_H_

#ifdef GLX_DIRECT_RENDERING

#include <sys/time.h>
#include "dri_util.h"
#include "mtypes.h"
#include "ffb_drishare.h"
#include "ffb_regs.h"
#include "ffb_dac.h"
#include "ffb_fifo.h"

typedef struct {
	__DRIscreenPrivate		*sPriv;
	ffb_fbcPtr			regs;
	ffb_dacPtr			dac;
	volatile char			*sfb8r;
	volatile char			*sfb32;
	volatile char			*sfb64;

	int				fifo_cache;
	int				rp_active;
} ffbScreenPrivate;

extern void __driRegisterExtensions(void);

#endif /* GLX_DIRECT_RENDERING */

#endif /* !(_FFB_XMESA_H) */
