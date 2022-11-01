
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
/**
 * @file
 */
#ifndef MCA_BTL_TCP_H
#define MCA_BTL_TCP_H

/* Standard system includes */
#include "ompi_config.h"
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

/* Open MPI includes */
#include "opal/event/event.h"
#include "opal/util/output.h"
#include "ompi/class/ompi_bitmap.h"
#include "ompi/class/ompi_free_list.h"
#include "ompi/mca/pml/pml.h"
#include "ompi/mca/btl/btl.h"
#include "ompi/mca/btl/base/base.h"
#include "ompi/mca/mpool/mpool.h" 
#include "ompi/mca/btl/btl.h"
#include "opal/class/opal_hash_table.h"

#define MCA_BTL_TCP_STATISTICS 0
#if defined(c_plusplus) || defined(__cplusplus)
extern "C" {
#endif


/**
 * Infiniband (TCP) BTL component.
 */

struct mca_btl_tcp_component_t {
    mca_btl_base_component_1_0_1_t super;   /**< base BTL component */ 
    uint32_t tcp_num_btls;                  /**< number of hcas available to the TCP component */
    struct mca_btl_tcp_module_t **tcp_btls; /**< array of available BTL modules */
    struct mca_btl_tcp_proc_t* tcp_local;   /**< local proc struct */
    int tcp_free_list_num;                  /**< initial size of free lists */
    int tcp_free_list_max;                  /**< maximum size of free lists */
    int tcp_free_list_inc;                  /**< number of elements to alloc when growing free lists */
    int tcp_endpoint_cache;                 /**< amount of cache on each endpoint */
    opal_hash_table_t tcp_procs;            /**< hash table of tcp proc structures */
    opal_list_t tcp_events;                 /**< list of pending tcp events */
    opal_mutex_t tcp_lock;                  /**< lock for accessing module state */
    opal_event_t tcp_recv_event;            /**< recv event for listen socket */
    int tcp_listen_sd;                      /**< listen socket for incoming connection requests */
    unsigned short tcp_listen_port;         /**< listen port */
    char*  tcp_if_include;                  /**< comma seperated list of interface to include */
    char*  tcp_if_exclude;                  /**< comma seperated list of interface to exclude */
    int    tcp_sndbuf;                      /**< socket sndbuf size */
    int    tcp_rcvbuf;                      /**< socket rcvbuf size */

    /* free list of fragment descriptors */
    ompi_free_list_t tcp_frag_eager;
    ompi_free_list_t tcp_frag_max;
    ompi_free_list_t tcp_frag_user;
}; 
typedef struct mca_btl_tcp_component_t mca_btl_tcp_component_t;

OMPI_MODULE_DECLSPEC extern mca_btl_tcp_component_t mca_btl_tcp_component;

/**
 * BTL Module Interface
 */
struct mca_btl_tcp_module_t {
    mca_btl_base_module_t  super;  /**< base BTL interface */
    mca_btl_base_recv_reg_t tcp_reg[256]; 
    int                tcp_ifindex; /**< PTL interface index */
    struct sockaddr_in tcp_ifaddr;  /**< PTL interface address */
    struct sockaddr_in tcp_ifmask;  /**< PTL interface netmask */
    opal_list_t        tcp_endpoints;
#if MCA_BTL_TCP_STATISTICS
    size_t tcp_bytes_sent;
    size_t tcp_bytes_recv;
    size_t tcp_send_handler;
#endif
}; 
typedef struct mca_btl_tcp_module_t mca_btl_tcp_module_t;
extern mca_btl_tcp_module_t mca_btl_tcp_module;

#if defined(__WINDOWS__)
#define CLOSE_THE_SOCKET(socket)   closesocket(socket)
#else
#define CLOSE_THE_SOCKET(socket)   close(socket)
#endif  /* defined(__WINDOWS__) */

/**
 * Register TCP component parameters with the MCA framework
 */
extern int mca_btl_tcp_component_open(void);

/**
 * Any final cleanup before being unloaded.
 */
extern int mca_btl_tcp_component_close(void);

/**
 * TCP component initialization.
 * 
 * @param num_btl_modules (OUT)           Number of BTLs returned in BTL array.
 * @param allow_multi_user_threads (OUT)  Flag indicating wether BTL supports user threads (TRUE)
 * @param have_hidden_threads (OUT)       Flag indicating wether BTL uses threads (TRUE)
 */
extern mca_btl_base_module_t** mca_btl_tcp_component_init(
    int *num_btl_modules, 
    bool allow_multi_user_threads,
    bool have_hidden_threads
);

/**
 * TCP component control.
 */
int mca_btl_tcp_component_control(
    int param, 
    void* value, 
    size_t size
);


/**
 * TCP component progress.
 */
extern int mca_btl_tcp_component_progress(void);



/**
 * Cleanup any resources held by the BTL.
 * 
 * @param btl  BTL instance.
 * @return     OMPI_SUCCESS or error status on failure.
 */

extern int mca_btl_tcp_finalize(
    struct mca_btl_base_module_t* btl
);


/**
 * PML->BTL notification of change in the process list.
 * 
 * @param btl (IN)
 * @param nprocs (IN)     Number of processes
 * @param procs (IN)      Set of processes
 * @param peers (OUT)     Set of (optional) peer addressing info.
 * @param peers (IN/OUT)  Set of processes that are reachable via this BTL.
 * @return     OMPI_SUCCESS or error status on failure.
 * 
 */

extern int mca_btl_tcp_add_procs(
    struct mca_btl_base_module_t* btl,
    size_t nprocs,
    struct ompi_proc_t **procs,
    struct mca_btl_base_endpoint_t** peers,
    ompi_bitmap_t* reachable
);

/**
 * PML->BTL notification of change in the process list.
 *
 * @param btl (IN)     BTL instance
 * @param nproc (IN)   Number of processes.
 * @param procs (IN)   Set of processes.
 * @param peers (IN)   Set of peer data structures.
 * @return             Status indicating if cleanup was successful
 *
 */

extern int mca_btl_tcp_del_procs(
    struct mca_btl_base_module_t* btl,
    size_t nprocs,
    struct ompi_proc_t **procs,
    struct mca_btl_base_endpoint_t** peers
);


/**
 * Initiate an asynchronous send.
 *
 * @param btl (IN)         BTL module
 * @param endpoint (IN)    BTL addressing information
 * @param descriptor (IN)  Description of the data to be transfered
 * @param tag (IN)         The tag value used to notify the peer.
 */

extern int mca_btl_tcp_send(
    struct mca_btl_base_module_t* btl,
    struct mca_btl_base_endpoint_t* btl_peer,
    struct mca_btl_base_descriptor_t* descriptor,
    mca_btl_base_tag_t tag
);


/**
 * Initiate an asynchronous put.
 *
 * @param btl (IN)         BTL module
 * @param endpoint (IN)    BTL addressing information
 * @param descriptor (IN)  Description of the data to be transferred
 */

extern int mca_btl_tcp_put(
    struct mca_btl_base_module_t* btl,
    struct mca_btl_base_endpoint_t* btl_peer,
    struct mca_btl_base_descriptor_t* decriptor
);


/**
 * Initiate an asynchronous get.
 *
 * @param btl (IN)         BTL module
 * @param endpoint (IN)    BTL addressing information
 * @param descriptor (IN)  Description of the data to be transferred
 */

extern int mca_btl_tcp_get(
    struct mca_btl_base_module_t* btl,
    struct mca_btl_base_endpoint_t* btl_peer,
    struct mca_btl_base_descriptor_t* decriptor
);

/**
 * Register a callback function that is called on receipt
 * of a fragment.
 *
 * @param btl (IN)     BTL module
 * @return             Status indicating if registration was successful
 *
 */

extern int mca_btl_tcp_register(
    struct mca_btl_base_module_t* btl, 
    mca_btl_base_tag_t tag, 
    mca_btl_base_module_recv_cb_fn_t cbfunc, 
    void* cbdata); 
    
/**
 * Allocate a descriptor with a segment of the requested size.
 * Note that the BTL layer may choose to return a smaller size
 * if it cannot support the request.
 *
 * @param btl (IN)      BTL module
 * @param size (IN)     Request segment size.
 */

extern mca_btl_base_descriptor_t* mca_btl_tcp_alloc(
    struct mca_btl_base_module_t* btl, 
    size_t size); 


/**
 * Return a segment allocated by this BTL.
 *
 * @param btl (IN)      BTL module
 * @param descriptor (IN)  Allocated descriptor.
 */

extern int mca_btl_tcp_free(
    struct mca_btl_base_module_t* btl, 
    mca_btl_base_descriptor_t* des); 
    

/**
 * Prepare a descriptor for send/rdma using the supplied
 * convertor. If the convertor references data that is contigous,
 * the descriptor may simply point to the user buffer. Otherwise,
 * this routine is responsible for allocating buffer space and
 * packing if required.
 *
 * @param btl (IN)          BTL module
 * @param endpoint (IN)     BTL peer addressing
 * @param convertor (IN)    Data type convertor
 * @param reserve (IN)      Additional bytes requested by upper layer to precede user data
 * @param size (IN/OUT)     Number of bytes to prepare (IN), number of bytes actually prepared (OUT) 
*/

mca_btl_base_descriptor_t* mca_btl_tcp_prepare_src(
    struct mca_btl_base_module_t* btl,
    struct mca_btl_base_endpoint_t* peer,
    struct mca_mpool_base_registration_t*,
    struct ompi_convertor_t* convertor,
    size_t reserve,
    size_t* size
);

extern mca_btl_base_descriptor_t* mca_btl_tcp_prepare_dst( 
    struct mca_btl_base_module_t* btl, 
    struct mca_btl_base_endpoint_t* peer,
    struct mca_mpool_base_registration_t*,
    struct ompi_convertor_t* convertor,
    size_t reserve,
    size_t* size); 


#if defined(c_plusplus) || defined(__cplusplus)
}
#endif
#endif
