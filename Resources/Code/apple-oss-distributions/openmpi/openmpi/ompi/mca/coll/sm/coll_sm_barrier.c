/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
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
/** @file */

#include "ompi_config.h"

#include "ompi/constants.h"
#include "ompi/communicator/communicator.h"
#include "ompi/mca/coll/coll.h"
#include "opal/sys/atomic.h"
#include "coll_sm.h"

/**
 * Shared memory barrier.
 *
 * Tree-based algorithm for a barrier: a fan in to rank 0 followed by
 * a fan out using the barrier segments in the shared memory area.
 *
 * There are 2 sets of barrier buffers -- since there can only be, at
 * most, 2 outstanding barriers at any time, there is no need for more
 * than this.  The generalized in-use flags, control, and data
 * segments are not used.
 *
 * The general algorithm is for a given process to wait for its N
 * children to fan in by monitoring a uint32_t in its barrier "in"
 * buffer.  When this value reaches N (i.e., each of the children have
 * atomically incremented the value), then the process atomically
 * increases the uint32_t in its parent's "in" buffer.  Then the
 * process waits for the parent to set a "1" in the process' "out"
 * buffer.  Once this happens, the process writes a "1" in each of its
 * children's "out" buffers, and returns.
 *
 * There's corner cases, of course, such as the root that has no
 * parent, and the leaves that have no children.  But that's the
 * general idea.
 */
int mca_coll_sm_barrier_intra(struct ompi_communicator_t *comm)
{
    int rank, buffer_set;
    mca_coll_base_comm_t *data;
    uint32_t i, num_children;
    volatile uint32_t *me_in, *me_out, *parent, *children = NULL;
    int uint_control_size;

    uint_control_size = 
        mca_coll_sm_component.sm_control_size / sizeof(uint32_t);
    data = comm->c_coll_selected_data;
    rank = ompi_comm_rank(comm);
    num_children = data->mcb_tree[rank].mcstn_num_children;
    buffer_set = ((data->mcb_barrier_count++) % 2) * 2;
    me_in = &data->mcb_barrier_control_me[buffer_set];
    me_out = me_in + uint_control_size;
    me_out = (uint32_t*)
        (((char*) me_in) + mca_coll_sm_component.sm_control_size);
    D(("rank %d barrier set %d: in %p, out %p\n", rank, buffer_set, me_in, me_out));

    /* Wait for my children to write to my *in* buffer */

    if (0 != num_children) {
        /* Get children *out* buffer */
        children = data->mcb_barrier_control_children + buffer_set + 
            uint_control_size;
        D(("rank %d waiting for fan in from %d children...\n", rank, num_children));
        while (*me_in != num_children) {
            continue;
        }
        *me_in = 0;
        D(("rank %d got fan in\n", rank));
    }

    /* Send to my parent and wait for a response (don't poll on
       parent's out buffer -- that would cause a lot of network
       traffic / contention / faults / etc.  Instead, children poll on
       local memory and therefore only num_children messages are sent
       across the network [vs. num_children *each* time all the
       children poll] -- i.e., the memory is only being polled by one
       process, and it is only changed *once* by an external
       process) */

    if (0 != rank) {
        /* Get parent *in* buffer */
        parent = &data->mcb_barrier_control_parent[buffer_set];
        D(("rank %d writing to parent\n", rank));
        opal_atomic_add(parent, 1);

        D(("rank %d waiting for fan out from parent: %p\n", rank, me_out));
        while (0 == *me_out) {
            continue;
        }
        D(("rank %d got fan out from parent\n", rank));
        *me_out = 0;
    }

    /* Send to my children */

    for (i = 0; i < num_children; ++i) {
        D(("rank %d sending to child %d: %p\n", rank, i,
           children + (i * uint_control_size * 4)));
        children[i * uint_control_size * 4] = 1;
    }
    D(("rank %d done with barrier\n", rank));

    /* All done!  End state of the control segment:

       me_in: 0
       me_out: 0
    */

    return OMPI_SUCCESS;
}
