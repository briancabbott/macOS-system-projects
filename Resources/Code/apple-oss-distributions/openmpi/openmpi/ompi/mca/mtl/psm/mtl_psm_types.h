/*
 * Copyright (c) 2004-2006 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2006 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2006      QLogic Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef MTL_PSM_TYPES_H_HAS_BEEN_INCLUDED
#define MTL_PSM_TYPS_H_HAS_BEEN_INCLUDED

#include "mtl_psm.h"

#include "opal/threads/threads.h"
#include "opal/threads/condition.h"
#include "ompi/class/ompi_free_list.h"
#include "opal/util/cmd_line.h"
#include "ompi/request/request.h"
#include "ompi/mca/mtl/mtl.h"
#include "ompi/mca/mtl/base/base.h"
#include "ompi/datatype/datatype.h"
#include "ompi/datatype/convertor.h"
#include "mtl_psm_endpoint.h" 

#include "psm.h"


#if defined(c_plusplus) || defined(__cplusplus)
extern "C" {
#endif

/** 
 * MTL Module Interface
 */
struct mca_mtl_psm_module_t { 
    mca_mtl_base_module_t super; /**< base MTL interface */

    int32_t connect_timeout;

    psm_ep_t	 ep;
    psm_mq_t	 mq;
    psm_epid_t	 epid;
    psm_epaddr_t epaddr;
}; 

typedef struct mca_mtl_psm_module_t mca_mtl_psm_module_t;

extern mca_mtl_psm_module_t ompi_mtl_psm;

struct mca_mtl_psm_component_t { 
    mca_mtl_base_component_1_0_0_t          super;  /**< base MTL component */ 
};
typedef struct mca_mtl_psm_component_t mca_mtl_psm_component_t;

extern mca_mtl_psm_component_t mca_mtl_psm_component;
    
#define PSM_MAKE_MQTAG(ctxt,rank,utag)		    \
        ( (((ctxt)&0xffffULL)<<48)| (((rank)&0xffffULL)<<32)| \
	  (((utag)&0xffffffffULL)) )

#define PSM_GET_MQRANK(tag_u64)	((int)(((tag_u64)>>32)&0xffff))
#define PSM_GET_MQUTAG(tag_u64)	((int)((tag_u64)&0xffffffffULL))

#define PSM_MAKE_TAGSEL(user_rank, user_tag, user_ctxt, tag, tagsel)		\
	do {									\
	    (tagsel) = 0xffffffffffffffffULL;					\
	    (tag)    = PSM_MAKE_MQTAG((user_ctxt),(user_rank),(user_tag));	\
	    if ((user_tag) == MPI_ANY_TAG) {  					\
		(tagsel) &= ~0x7fffffffULL;					\
		(tag) &= ~0xffffffffULL;					\
	    }									\
	    if ((user_rank) == MPI_ANY_SOURCE)					\
		(tagsel) &= ~0xffff00000000ULL;					\
	} while (0)

#if defined(c_plusplus) || defined(__cplusplus)
}
#endif

#endif  /* MTL_PSM_TYPES_H_HAS_BEEN_INCLUDED */

