/*
 * Copyright (c) 2004-2006 The Trustees of Indiana University and Indiana
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

#include "ompi_config.h"

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#include "orte/class/orte_proc_table.h"
#include "ompi/mca/btl/base/btl_base_error.h"
#include "ompi/mca/pml/base/pml_base_module_exchange.h"
#include "ompi/datatype/dt_arch.h"

#include "btl_tcp.h"
#include "btl_tcp_proc.h"

static void mca_btl_tcp_proc_construct(mca_btl_tcp_proc_t* proc);
static void mca_btl_tcp_proc_destruct(mca_btl_tcp_proc_t* proc);
static bool is_private_ipv4(struct in_addr *in);


OBJ_CLASS_INSTANCE(
    mca_btl_tcp_proc_t, 
    opal_list_item_t, 
    mca_btl_tcp_proc_construct, 
    mca_btl_tcp_proc_destruct);


void mca_btl_tcp_proc_construct(mca_btl_tcp_proc_t* proc)
{
    proc->proc_ompi = 0;
    proc->proc_addrs = NULL;
    proc->proc_addr_count = 0;
    proc->proc_endpoints = NULL;
    proc->proc_endpoint_count = 0;
    OBJ_CONSTRUCT(&proc->proc_lock, opal_mutex_t);
}

/*
 * Cleanup ib proc instance
 */

void mca_btl_tcp_proc_destruct(mca_btl_tcp_proc_t* proc)
{
    /* remove from list of all proc instances */
    OPAL_THREAD_LOCK(&mca_btl_tcp_component.tcp_lock);
    orte_hash_table_remove_proc(&mca_btl_tcp_component.tcp_procs, &proc->proc_name);
    OPAL_THREAD_UNLOCK(&mca_btl_tcp_component.tcp_lock);

    /* release resources */
    if(NULL != proc->proc_endpoints) {
        free(proc->proc_endpoints);
        OBJ_DESTRUCT(&proc->proc_lock);
    }
}


/*
 * Check to see if an IPv4 struct in_addr is public or private.  We
 * can only do IPv4 here because some of the TCP BTL endpoint structs
 * only hold the struct in_addr, not the upper-level sin_family that
 * would indicate if the address is IPv6.
 */
static bool is_private_ipv4(struct in_addr *in)
{
    /* There are definitely ways to do this more efficiently, but
       since this is not performance-critical code, it seems better to
       use clear code (vs. clever code) */

    uint32_t addr = ntohl((uint32_t) in->s_addr);
    unsigned int a = (addr & 0xff000000) >> 24;
    unsigned int b = (addr & 0x00ff0000) >> 16;

    return ((10 == a) ||
            (192 == a && 168 == b) ||
            (172 == a && 16 == b)) ? true : false;
}


/*
 * Create a TCP process structure. There is a one-to-one correspondence
 * between a ompi_proc_t and a mca_btl_tcp_proc_t instance. We cache
 * additional data (specifically the list of mca_btl_tcp_endpoint_t instances, 
 * and published addresses) associated w/ a given destination on this
 * datastructure.
 */

mca_btl_tcp_proc_t* mca_btl_tcp_proc_create(ompi_proc_t* ompi_proc)
{
    int rc;
    size_t size;
    mca_btl_tcp_proc_t* btl_proc;

    OPAL_THREAD_LOCK(&mca_btl_tcp_component.tcp_lock);
    btl_proc = (mca_btl_tcp_proc_t*)orte_hash_table_get_proc(
         &mca_btl_tcp_component.tcp_procs, &ompi_proc->proc_name);
    if(NULL != btl_proc) {
        OPAL_THREAD_UNLOCK(&mca_btl_tcp_component.tcp_lock);
        return btl_proc;
     }
                                                                                                                 
    btl_proc = OBJ_NEW(mca_btl_tcp_proc_t);
    if(NULL == btl_proc)
        return NULL;
    btl_proc->proc_ompi = ompi_proc;
    btl_proc->proc_name = ompi_proc->proc_name;
                                                                                                                 
    /* add to hash table of all proc instance */
    orte_hash_table_set_proc(
        &mca_btl_tcp_component.tcp_procs,
        &btl_proc->proc_name,
         btl_proc);
    OPAL_THREAD_UNLOCK(&mca_btl_tcp_component.tcp_lock);
                                                                                                                 
    /* lookup tcp parameters exported by this proc */
    rc = mca_pml_base_modex_recv( &mca_btl_tcp_component.super.btl_version,
                                  ompi_proc,
                                  (void**)&btl_proc->proc_addrs,
                                  &size );
    if(rc != OMPI_SUCCESS) {
        BTL_ERROR(("mca_base_modex_recv: failed with return value=%d", rc));
        OBJ_RELEASE(btl_proc);
        return NULL;
    }
    if(0 != (size % sizeof(mca_btl_tcp_addr_t))) {
        BTL_ERROR(("mca_base_modex_recv: invalid size %d\n", size));
        return NULL;
    }
    btl_proc->proc_addr_count = size / sizeof(mca_btl_tcp_addr_t);
                                                                                                                 
    /* allocate space for endpoint array - one for each exported address */
    btl_proc->proc_endpoints = (mca_btl_base_endpoint_t**)
        malloc(btl_proc->proc_addr_count * sizeof(mca_btl_base_endpoint_t*));
    if(NULL == btl_proc->proc_endpoints) {
        OBJ_RELEASE(btl_proc);
        return NULL;
    }
    if(NULL == mca_btl_tcp_component.tcp_local && ompi_proc == ompi_proc_local())
        mca_btl_tcp_component.tcp_local = btl_proc;
    return btl_proc;
}


/*
 * Note that this routine must be called with the lock on the process
 * already held.  Insert a btl instance into the proc array and assign 
 * it an address.
 */
int mca_btl_tcp_proc_insert(
    mca_btl_tcp_proc_t* btl_proc, 
    mca_btl_base_endpoint_t* btl_endpoint)
{
    struct mca_btl_tcp_module_t *btl_tcp = btl_endpoint->endpoint_btl;
    size_t i;
    unsigned long net1;

#ifndef WORDS_BIGENDIAN
    /* if we are little endian and our peer is not so lucky, then we
       need to put all information sent to him in big endian (aka
       Network Byte Order) and expect all information received to
       be in NBO.  Since big endian machines always send and receive
       in NBO, we don't care so much about that case. */
    if (btl_proc->proc_ompi->proc_arch & OMPI_ARCH_ISBIGENDIAN) {
        btl_endpoint->endpoint_nbo = true;
    }
#endif

    /* insert into endpoint array */
    btl_endpoint->endpoint_proc = btl_proc;
    btl_proc->proc_endpoints[btl_proc->proc_endpoint_count++] = btl_endpoint;

    net1 = btl_tcp->tcp_ifaddr.sin_addr.s_addr & btl_tcp->tcp_ifmask.sin_addr.s_addr;

    /*
     * Look through the proc instance for an address that is on the
     * directly attached network. If we don't find one, pick the first
     * unused address.
    */
    for(i=0; i<btl_proc->proc_addr_count; i++) {
        mca_btl_tcp_addr_t* endpoint_addr = btl_proc->proc_addrs + i;
        unsigned long net2 = endpoint_addr->addr_inet.s_addr & btl_tcp->tcp_ifmask.sin_addr.s_addr;
        if(endpoint_addr->addr_inuse != 0)
            continue;
        if(net1 == net2) {
            btl_endpoint->endpoint_addr = endpoint_addr;
            break;
        } else if(btl_endpoint->endpoint_addr != 0) {
            btl_endpoint->endpoint_addr = endpoint_addr;
	}
    }
    
    /* Make sure there is a common interface */
    if( NULL != btl_endpoint->endpoint_addr ) {
        btl_endpoint->endpoint_addr->addr_inuse++;
        return OMPI_SUCCESS;
    }

    /* There was no common interface.  So what do we do?  For the
       moment, we'll do enough to cover 2 common cases:

       1. Running MPI processes on two computers that are not on the
          same subnet, but still have routable addresses to each
          other.  In this case, the above subnet matching will fail,
          but since the addresses are routable, the
          OS/networking/routers will make it all work ok.  So we need
          to make this function *not* return OMPI_ERR_UNREACH.

       2. Running MPI processes on a typical cluster configuration
          where a head node has 2 TCP NICs (one public IP address one
          private IP address) and all the back-end compute nodes have
          only private IP addresses.  In this scenario, the MPI
          process on the head node will have 2 TCP BTL modules (one
          for the public, one for the private).  The module with the
          private IP address will match the subnet and all will work
          fine.  The module with the public IP address will not match
          anything and fall through to here -- we want it to return
          OMPI_ERR_UNREACH so that that module will effectively have
          no peers that it can communicate with.

       To support these two scenarios, do the following:

       - if my address is private (10., 192.168., or 172.16.), return
         UNREACH.
       - if my address is public, return the first public address from
         my peer (and hope for the best), or UNREACH if there are none
         available.

       This does not cover some other scenarios that we'll likely need
       to support in the future, such as:

       - Flat neighborhood networks -- where all the IP's in question
         are private, the subnet masking won't necessarily match, but
         they're routable to each other.
       - Really large, private TCP-based clusters, such as a 1024 node
         TCP-based cluster.  Depending on how the subnet masks are set
         by the admins, there may be a subnet mask that effectively
         spans the entire cluster, or (for example) subnet masks may
         be set such that only nodes on the same switches are on the
         same subnet.  This latter scenario will not be supported
         by the above cases.

       To support these kinds of scenarios, we really need "something
       better", such as allowing the user to specify a config file
       indicating which subnets are reachable by which interface, etc.
    */

    else {
        /* If my address is private, return UNREACH */
        if (is_private_ipv4(&(btl_tcp->tcp_ifaddr.sin_addr))) {
            return OMPI_ERR_UNREACH;
        }

        /* Find the first public peer address */
        for (i = 0; i < btl_proc->proc_addr_count; ++i) {
            mca_btl_tcp_addr_t* endpoint_addr = btl_proc->proc_addrs + i;
            if (!is_private_ipv4(&(endpoint_addr->addr_inet))) {
                btl_endpoint->endpoint_addr = endpoint_addr;
                btl_endpoint->endpoint_addr->addr_inuse++;
                return OMPI_SUCCESS;
            }
        }

        /* Didn't find any peer addresses that were public, so return
           UNREACH */
        return OMPI_ERR_UNREACH;
    }
}

/*
 * Remove an endpoint from the proc array and indicate the address is
 * no longer in use.
 */
                                                                                                                 
int mca_btl_tcp_proc_remove(mca_btl_tcp_proc_t* btl_proc, mca_btl_base_endpoint_t* btl_endpoint)
{
    size_t i;
    OPAL_THREAD_LOCK(&btl_proc->proc_lock);
    for(i=0; i<btl_proc->proc_endpoint_count; i++) {
        if(btl_proc->proc_endpoints[i] == btl_endpoint) {
            memmove(btl_proc->proc_endpoints+i, btl_proc->proc_endpoints+i+1,
                (btl_proc->proc_endpoint_count-i-1)*sizeof(mca_btl_base_endpoint_t*));
            if(--btl_proc->proc_endpoint_count == 0) {
                OPAL_THREAD_UNLOCK(&btl_proc->proc_lock);
                OBJ_RELEASE(btl_proc);
                return OMPI_SUCCESS;
            }
            /* The endpoint_addr may still be NULL if this enpoint is
               being removed early in the wireup sequence (e.g., if it
               is unreachable by all other procs) */
            if (NULL != btl_endpoint->endpoint_addr) {
                btl_endpoint->endpoint_addr->addr_inuse--;
            }
            break;
        }
    }
    OPAL_THREAD_UNLOCK(&btl_proc->proc_lock);
    return OMPI_SUCCESS;
}

/*
 * Look for an existing TCP process instance based on the globally unique
 * process identifier.
 */
mca_btl_tcp_proc_t* mca_btl_tcp_proc_lookup(const orte_process_name_t *name)
{
    mca_btl_tcp_proc_t* proc;
    OPAL_THREAD_LOCK(&mca_btl_tcp_component.tcp_lock);
    proc = (mca_btl_tcp_proc_t*)orte_hash_table_get_proc(
         &mca_btl_tcp_component.tcp_procs, name);
    OPAL_THREAD_UNLOCK(&mca_btl_tcp_component.tcp_lock);
    return proc;
}

/*
 * loop through all available PTLs for one matching the source address
 * of the request.
 */
bool mca_btl_tcp_proc_accept(mca_btl_tcp_proc_t* btl_proc, struct sockaddr_in* addr, int sd)
{
    size_t i;
    OPAL_THREAD_LOCK(&btl_proc->proc_lock);
    for(i=0; i<btl_proc->proc_endpoint_count; i++) {
        mca_btl_base_endpoint_t* btl_endpoint = btl_proc->proc_endpoints[i];
        if(mca_btl_tcp_endpoint_accept(btl_endpoint, addr, sd)) {
            OPAL_THREAD_UNLOCK(&btl_proc->proc_lock);
            return true;
        }
    }
    OPAL_THREAD_UNLOCK(&btl_proc->proc_lock);
    return false;
}
                                                                                                                                

