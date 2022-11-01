/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2006 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#ifndef MCA_BTL_MX_FRAG_H
#define MCA_BTL_MX_FRAG_H


#define MCA_BTL_MX_FRAG_ALIGN (8)

#include "ompi_config.h"
#include "opal/class/opal_list.h"
#include "ompi/class/ompi_free_list.h"
#include "btl_mx.h" 

#if defined(c_plusplus) || defined(__cplusplus)
extern "C" {
#endif
    
    /**
     * MX send framxent derived type.
     */
    struct mca_btl_mx_frag_t {
        mca_btl_base_descriptor_t       base; 
        mca_btl_base_segment_t          segment[2]; 
        struct mca_btl_base_endpoint_t* endpoint; 
        mca_btl_base_tag_t              tag;
        mx_request_t                    mx_request;
        size_t                          size; 
        ompi_free_list_t*               mx_frag_list;
    }; 
    typedef struct mca_btl_mx_frag_t mca_btl_mx_frag_t;
    OBJ_CLASS_DECLARATION(mca_btl_mx_frag_t); 

    typedef struct mca_btl_mx_frag_t mca_btl_mx_frag_eager_t; 
    
    OBJ_CLASS_DECLARATION(mca_btl_mx_frag_eager_t); 
    
    typedef struct mca_btl_mx_frag_t mca_btl_mx_frag_max_t; 
    
    OBJ_CLASS_DECLARATION(mca_btl_mx_frag_max_t); 

    typedef struct mca_btl_mx_frag_t mca_btl_mx_frag_user_t; 
    
    OBJ_CLASS_DECLARATION(mca_btl_mx_frag_user_t); 

    /*
     * Macros to allocate/return descriptors from module specific
     * free list(s).
     */

#define MCA_BTL_MX_FRAG_ALLOC_EAGER(btl, frag, rc)                              \
{                                                                               \
    ompi_free_list_item_t *item;                                                \
    OMPI_FREE_LIST_WAIT( &mca_btl_mx_component.mx_send_eager_frags, item, rc);  \
    frag = (mca_btl_mx_frag_t*) item;                                           \
    frag->mx_frag_list = (ompi_free_list_t*)&(mca_btl_mx_component.mx_send_eager_frags);  \
    frag->segment[0].seg_addr.pval = (void*)(frag+1);                           \
    frag->segment[0].seg_len = mca_btl_mx_module.super.btl_eager_limit;         \
}

#if 0
#define MCA_BTL_MX_FRAG_ALLOC_USER(btl, frag, rc)                             \
{                                                                             \
    ompi_free_list_item_t *item;                                              \
    OMPI_FREE_LIST_WAIT( &mca_btl_mx_component.mx_send_user_frags, item, rc); \
    frag = (mca_btl_mx_frag_t*) item;                                         \
    frag->mx_frag_list = (ompi_free_list_t*)&(mca_btl_mx_component.mx_send_user_frags);  \
    frag->segment[0].seg_addr.pval = (void*)(frag+1);                         \
    frag->segment[0].seg_len = mca_btl_mx_module.super.btl_eager_limit;       \
}
#else
#define MCA_BTL_MX_FRAG_ALLOC_USER(btl, frag, rc)                             \
    MCA_BTL_MX_FRAG_ALLOC_EAGER( btl, frag, rc )
#endif

#define MCA_BTL_MX_FRAG_RETURN(btl, frag)                                   \
{                                                                           \
    /*opal_output( 0, "return item to %p\n", frag->mx_frag_list );*/            \
    OMPI_FREE_LIST_RETURN( frag->mx_frag_list, (ompi_free_list_item_t*)(frag));  \
}

#if defined(c_plusplus) || defined(__cplusplus)
}
#endif
#endif
