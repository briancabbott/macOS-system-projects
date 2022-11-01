/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2006 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2006 The Regents of the University of California.
 *                         All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "ompi_config.h"

#include <string.h>

#include "ompi/mca/bml/bml.h"
#include "bml_base_btl.h"
#include "opal/util/crc.h"

static void mca_bml_base_btl_array_construct(mca_bml_base_btl_array_t* array)
{
    array->bml_btls = NULL;
    array->arr_size = 0;
    array->arr_index = 0;
    array->arr_reserve = 0;
}


static void mca_bml_base_btl_array_destruct(mca_bml_base_btl_array_t* array)
{
    if(NULL != array->bml_btls)
        free(array->bml_btls);
}

OBJ_CLASS_INSTANCE(
    mca_bml_base_btl_array_t,
    opal_object_t,
    mca_bml_base_btl_array_construct,
    mca_bml_base_btl_array_destruct
);

int mca_bml_base_btl_array_reserve(mca_bml_base_btl_array_t* array, size_t size)
{
    size_t old_len = sizeof(mca_bml_base_btl_t)*array->arr_reserve;
    size_t new_len = sizeof(mca_bml_base_btl_t)*size;
    if(old_len >= new_len)
        return OMPI_SUCCESS;
    
    array->bml_btls = (mca_bml_base_btl_t*)realloc(array->bml_btls, new_len);
    if(NULL == array->bml_btls)
        return OMPI_ERR_OUT_OF_RESOURCE;
    memset((unsigned char*)array->bml_btls + old_len, 0, new_len-old_len);
    array->arr_reserve = size;
    return OMPI_SUCCESS;
}


#if OMPI_ENABLE_DEBUG_RELIABILITY

extern double mca_bml_base_error_rate_floor;
extern double mca_bml_base_error_rate_ceiling;
extern int    mca_bml_base_error_count;

struct mca_bml_base_context_t {
    size_t index;
    mca_btl_base_completion_fn_t cbfunc;
    void* cbdata;
};
typedef struct mca_bml_base_context_t mca_bml_base_context_t;

static void mca_bml_base_completion(
                                    struct mca_btl_base_module_t* btl,
                                    struct mca_btl_base_endpoint_t* ep,
                                    struct mca_btl_base_descriptor_t* des,
                                    int status)
{
    mca_bml_base_context_t* ctx = (mca_bml_base_context_t*) des->des_cbdata;
    /* restore original state */
    ((unsigned char*)des->des_src[0].seg_addr.pval)[ctx->index] ^= ~0;
    des->des_cbdata = ctx->cbdata;
    des->des_cbfunc = ctx->cbfunc;
    free(ctx);
    /* invoke original callback */
    des->des_cbfunc(btl,ep,des,status);
}

int mca_bml_base_send(
    mca_bml_base_btl_t* bml_btl, 
    mca_btl_base_descriptor_t* des, 
    mca_btl_base_tag_t tag) 
{ 
    des->des_context = bml_btl;
    if(mca_bml_base_error_count <= 0 && mca_bml_base_error_rate_ceiling > 0) {
        mca_bml_base_error_count = (int) ((mca_bml_base_error_rate_ceiling * rand())/(RAND_MAX+1.0));
        if(mca_bml_base_error_count < mca_bml_base_error_rate_floor) { 
            mca_bml_base_error_count = mca_bml_base_error_rate_floor;
        }
        /* if(mca_bml_base_error_count % 3) {  */
/*             /\* no local completion - network 'hangs' *\/  */
/*             opal_output(0, "%s:%d: dropping dat, NO local completion\n", __FILE__, __LINE__);  */
/*             return OMPI_SUCCESS;  */
/*         } else */ 
        if(mca_bml_base_error_count % 2) {
            /* local completion - network "drops" packet */
            opal_output(0, "%s:%d: dropping data, with local completion\n", __FILE__, __LINE__);
            des->des_cbfunc(bml_btl->btl, bml_btl->btl_endpoint, des, OMPI_SUCCESS);
            return OMPI_SUCCESS;
        } else {
            /* corrupt data */
            mca_bml_base_context_t* ctx = (mca_bml_base_context_t*) 
                malloc(sizeof(mca_bml_base_context_t));
            if(NULL != ctx) {
                opal_output(0, "%s:%d: corrupting data\n", __FILE__, __LINE__);
                ctx->index = (size_t) ((des->des_src[0].seg_len * rand() * 1.0) / (RAND_MAX + 1.0));
                ctx->cbfunc = des->des_cbfunc;
                ctx->cbdata = des->des_cbdata;
                ((unsigned char*)des->des_src[0].seg_addr.pval)[ctx->index] ^= ~0;
                des->des_cbdata = ctx;
                des->des_cbfunc = mca_bml_base_completion;
            }
        }
    }
    mca_bml_base_error_count--;
    des->des_context = (void*) bml_btl; 
    return bml_btl->btl_send(
                             bml_btl->btl,
                             bml_btl->btl_endpoint, 
                             des,
                             tag);
}

#endif
