/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
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

#include "opal/mca/mca.h"
#include "ompi/mca/mtl/mtl.h"
#include "ompi/mca/mtl/base/base.h"
#include "ompi/constants.h"
#include "ompi/datatype/convertor.h"
#include "ompi/datatype/datatype.h"
#include "ompi/datatype/datatype_internal.h"

static inline int
ompi_mtl_datatype_pack(struct ompi_convertor_t *convertor,
                       void **buffer,
                       size_t *buffer_len,
                       bool *freeAfter)
{
    struct iovec iov;
    uint32_t iov_count = 1;

    ompi_convertor_get_packed_size(convertor, buffer_len);
    *freeAfter  = false;
    if( 0 == *buffer_len ) {
        *buffer     = NULL;
        return OMPI_SUCCESS;
    }
    iov.iov_len = *buffer_len;
    iov.iov_base = NULL;
    if (ompi_convertor_need_buffers(convertor)) {
        iov.iov_base = malloc(*buffer_len);
        if (NULL == iov.iov_base) return OMPI_ERR_OUT_OF_RESOURCE;
        *freeAfter = true;
    }
    
    ompi_convertor_pack( convertor, &iov, &iov_count, buffer_len );
    
    *buffer = iov.iov_base;

    return OMPI_SUCCESS;
}


static inline int
ompi_mtl_datatype_recv_buf(struct ompi_convertor_t *convertor,
                           void ** buffer,
                           size_t *buffer_len,
                           bool *free_on_error)
{
    ompi_convertor_get_packed_size(convertor, buffer_len);
    *free_on_error = false;
    if( 0 == *buffer_len ) {
        *buffer = NULL;
        *buffer_len = 0;
        return OMPI_SUCCESS;
    }
    if (ompi_convertor_need_buffers(convertor)) {
        *buffer = malloc(*buffer_len);
        *free_on_error = true;
    } else {
        *buffer = convertor->pBaseBuf + 
            convertor->use_desc->desc[convertor->use_desc->used].end_loop.first_elem_disp;;
    }
    return OMPI_SUCCESS;
}


static inline int
ompi_mtl_datatype_unpack(struct ompi_convertor_t *convertor,
                         void *buffer,
                         size_t buffer_len)
{
    struct iovec iov;
    uint32_t iov_count = 1;

    if (buffer_len > 0 && ompi_convertor_need_buffers(convertor)) {
        iov.iov_len = buffer_len;
        iov.iov_base = buffer;

        ompi_convertor_unpack(convertor, &iov, &iov_count, &buffer_len );

        free(buffer);
    }

    return OMPI_ERROR;
}
