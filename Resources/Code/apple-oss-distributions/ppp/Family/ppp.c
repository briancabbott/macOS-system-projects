/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
 
/* -----------------------------------------------------------------------------
 *
 *  History :
 *
 *  Jun 2000 - 	add support for ppp generic interfaces

 *  Nov 1999 - 	Christophe Allie - created.
 *		basic support fo ppp family
 *
 *  Theory of operation :
 *
 *  this file creates is loaded as a Kernel Extension.
 *  it creates the necessary ppp components and plumbs everything together.
 *
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
  Includes
----------------------------------------------------------------------------- */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <kern/thread.h>
#include <sys/systm.h>
#include <kern/locks.h>
#include <net/if.h>
#include <netinet/in.h>

#include "ppp_defs.h"		// public ppp values
#include "if_ppp.h"		// public ppp API
#include "if_ppplink.h"		// public link API

#include "ppp_domain.h"
#include "ppp_if.h"
#include "ppp_link.h"
#include "ppp_comp.h"
#include "ppp_compress.h"

#include "ppp_serial.h"
#include "ppp_ip.h"
#include "ppp_ipv6.h"


/* -----------------------------------------------------------------------------
 Definitions
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
 Forward declarations
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
 PPP globals
----------------------------------------------------------------------------- */
static int 	ppp_inited = 0;
extern lck_mtx_t	*ppp_domain_mutex;

/* ----------------------------------------------------------------------------- 
 NKE entry point, start routine
----------------------------------------------------------------------------- */
int ppp_module_start(struct kmod_info *ki, void *data)
{
    int 	ret;

	if (ppp_inited)
        return KERN_SUCCESS;

    /* add the ppp domain */
    ppp_domain_init();
	
	lck_mtx_lock(ppp_domain_mutex);
	/* register the ppp network and ppp link module */
	ret = ppp_proto_add();
	lck_mtx_unlock(ppp_domain_mutex);
	LOGRETURN(ret, ret, "pppserial_init: ppp_proto_add error = 0x%x\n");
	
    /* now init the if and link structures */
    ppp_if_init();
    ppp_link_init();
    ppp_comp_init();

    /* init ip protocol */
    ppp_ip_init(0);
    ppp_ipv6_init(0);
    
    /* add the ppp serial link support */
    ret = pppserial_init();
    LOGRETURN(ret, KERN_FAILURE, "pppserial_init: ppp_fam_init error = 0x%x\n");

    /* NKE is ready ! */
    ppp_inited = 1;
    return KERN_SUCCESS;
}

/* -----------------------------------------------------------------------------
  NKE entry point, stop routine
----------------------------------------------------------------------------- */
int ppp_module_stop(struct kmod_info *ki, void *data)
{
    int ret;

    if (!ppp_inited)
        return(KERN_SUCCESS);
			
    /* remove the ppp serial link support */
    ret = pppserial_dispose();
    LOGRETURN(ret, ret, "ppp_terminate: pppserial_dispose error = 0x%x\n");

    /* remove ip protocol */
    ret = ppp_ipv6_dispose(0);
    LOGRETURN(ret, ret, "ppp_terminate: ppp_ipv6_dispose error = 0x%x\n");
    ret = ppp_ip_dispose(0);
    LOGRETURN(ret, ret, "ppp_terminate: ppp_ip_dispose error = 0x%x\n");

	lck_mtx_lock(ppp_domain_mutex);
    /* dispose the link and if layers */
    ret = ppp_if_dispose();
    LOGGOTOFAIL(ret, "ppp_terminate: ppp_if_dispose error = 0x%x\n");
    ret = ppp_link_dispose();
    LOGGOTOFAIL(ret, "ppp_terminate: ppp_link_dispose error = 0x%x\n");
    ret = ppp_comp_dispose();
    LOGGOTOFAIL(ret, "ppp_terminate: ppp_comp_dispose error = 0x%x\n");

	/* remove the pppdomain */
    ret = ppp_proto_remove();
	LOGGOTOFAIL(ret, "ppp_terminate: ppp_proto_remove error = 0x%x\n");
	
	lck_mtx_unlock(ppp_domain_mutex);
	
	/* remove the pppdomain */
    ret = ppp_domain_dispose();
    LOGRETURN(ret, KERN_FAILURE, "ppp_terminate: ppp_domain_dispose error = 0x%x\n");

    return KERN_SUCCESS;
fail:
	lck_mtx_unlock(ppp_domain_mutex);
	return KERN_FAILURE;
}
