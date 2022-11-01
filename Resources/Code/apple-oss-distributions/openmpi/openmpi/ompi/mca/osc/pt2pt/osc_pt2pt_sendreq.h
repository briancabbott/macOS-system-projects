/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University.
 *                         All rights reserved.
 * Copyright (c) 2004-2005 The Trustees of the University of Tennessee.
 *                         All rights reserved.
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

#ifndef OMPI_OSC_PT2PT_SENDREQ_H
#define OMPI_OSC_PT2PT_SENDREQ_H

#include "osc_pt2pt.h"
#include "osc_pt2pt_longreq.h"

#include "opal/class/opal_list.h"
#include "opal/threads/mutex.h"
#include "ompi/datatype/datatype.h"
#include "ompi/datatype/convertor.h"
#include "ompi/communicator/communicator.h"
#include "ompi/proc/proc.h"
#include "ompi/op/op.h"
#include "ompi/mca/pml/pml.h"

typedef enum {
    OMPI_OSC_PT2PT_GET,
    OMPI_OSC_PT2PT_ACC,
    OMPI_OSC_PT2PT_PUT
} ompi_osc_pt2pt_req_type_t;


struct ompi_osc_pt2pt_sendreq_t {
    ompi_request_t super;

    /** type of sendreq (from ompi_osc_pt2pt_req_type_t) */
    ompi_osc_pt2pt_req_type_t req_type;
    /** pointer to the module that created the sendreq */
    ompi_osc_pt2pt_module_t *req_module;

    /** Datatype for the origin side of the operation */
    struct ompi_datatype_t *req_origin_datatype;
    /** Convertor for the origin side of the operation.  Setup for
        either send (Put / Accumulate) or receive (Get) */
    ompi_convertor_t req_origin_convertor;
    /** packed size of message on the origin side */
    size_t req_origin_bytes_packed;

    /** rank in module's communicator for target of operation */
    int req_target_rank;
    /** pointer to the proc structure for the target of the operation */
    ompi_proc_t *req_target_proc;

    /** displacement on target */
    int req_target_disp;
    /** datatype count on target */
    int req_target_count;
    /** datatype on target */
    struct ompi_datatype_t *req_target_datatype;

    /** op index on the target */
    int req_op_id;
};
typedef struct ompi_osc_pt2pt_sendreq_t ompi_osc_pt2pt_sendreq_t;
OBJ_CLASS_DECLARATION(ompi_osc_pt2pt_sendreq_t);


/** allocate and populate a sendreq structure.  Both datatypes are
    RETAINed for the life of the sendreq */
int
ompi_osc_pt2pt_sendreq_alloc_init(ompi_osc_pt2pt_req_type_t req_type,
                                  void *origin_addr, int origin_count,
                                  struct ompi_datatype_t *origin_dt,
                                  int target, int target_disp, int target_count,
                                  struct ompi_datatype_t *target_datatype,
                                  ompi_osc_pt2pt_module_t *module,
                                  ompi_osc_pt2pt_sendreq_t **sendreq);

static inline int
ompi_osc_pt2pt_sendreq_alloc(ompi_osc_pt2pt_module_t *module,
                             int target_rank,
                             ompi_osc_pt2pt_sendreq_t **sendreq)
{
    int ret;
    opal_free_list_item_t *item;
    ompi_proc_t *proc = ompi_comm_peer_lookup( module->p2p_comm, target_rank );

    /* BWB - FIX ME - is this really the right return code? */
    if (NULL == proc) return OMPI_ERR_OUT_OF_RESOURCE;

    OPAL_FREE_LIST_GET(&mca_osc_pt2pt_component.p2p_c_sendreqs,
                       item, ret);
    if (OMPI_SUCCESS != ret) return ret;
    *sendreq = (ompi_osc_pt2pt_sendreq_t*) item;

    (*sendreq)->req_module = module;
    (*sendreq)->req_target_rank = target_rank;
    (*sendreq)->req_target_proc = proc;

    return OMPI_SUCCESS;
}


static inline int
ompi_osc_pt2pt_sendreq_init_origin(ompi_osc_pt2pt_sendreq_t *sendreq,
                                   ompi_osc_pt2pt_req_type_t req_type,
                                   void *origin_addr,
                                   int origin_count,
                                   struct ompi_datatype_t *origin_dt)
{
    OBJ_RETAIN(origin_dt);
    sendreq->req_origin_datatype = origin_dt;
    sendreq->req_type = req_type;

    if (req_type != OMPI_OSC_PT2PT_GET) {
        ompi_convertor_copy_and_prepare_for_send(sendreq->req_target_proc->proc_convertor,
                                                 origin_dt,
                                                 origin_count,
                                                 origin_addr,
                                                 0,
                                                 &(sendreq->req_origin_convertor));
        ompi_convertor_get_packed_size(&sendreq->req_origin_convertor,
                                       &sendreq->req_origin_bytes_packed);
    } else {
        ompi_convertor_copy_and_prepare_for_recv(sendreq->req_target_proc->proc_convertor,
                                                 origin_dt,
                                                 origin_count,
                                                 origin_addr,
                                                 0,
                                                 &(sendreq->req_origin_convertor));
        ompi_convertor_get_packed_size(&sendreq->req_origin_convertor,
                                       &sendreq->req_origin_bytes_packed);        
    }

    return OMPI_SUCCESS;
}


static inline int
ompi_osc_pt2pt_sendreq_init_target(ompi_osc_pt2pt_sendreq_t *sendreq,
                                   int target_disp,
                                   int target_count,
                                   struct ompi_datatype_t *target_datatype)
{
    OBJ_RETAIN(target_datatype);

    sendreq->req_target_disp = target_disp;
    sendreq->req_target_count = target_count;
    sendreq->req_target_datatype = target_datatype;

    return OMPI_SUCCESS;
}


static inline int
ompi_osc_pt2pt_sendreq_free(ompi_osc_pt2pt_sendreq_t *sendreq)
{
    ompi_convertor_cleanup(&sendreq->req_origin_convertor);

    OBJ_RELEASE(sendreq->req_target_datatype);
    OBJ_RELEASE(sendreq->req_origin_datatype);

    OPAL_FREE_LIST_RETURN(&mca_osc_pt2pt_component.p2p_c_sendreqs,
                          (opal_list_item_t*) sendreq);
 
    return OMPI_SUCCESS;
}

#endif /* OMPI_OSC_PT2PT_SENDREQ_H */
