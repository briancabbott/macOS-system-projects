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
 * Copyright (c) 2006-2007 Los Alamos National Security, LLC. 
 *                         All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 *
 * In windows, many of the socket functions return an EWOULDBLOCK
 * instead of \ things like EAGAIN, EINPROGRESS, etc. It has been
 * verified that this will \ not conflict with other error codes that
 * are returned by these functions \ under UNIX/Linux environments 
 */

#include "orte_config.h"
#include "opal/opal_socket_errno.h"

#include "orte/class/orte_proc_table.h"
#include "orte/orte_constants.h"
#include "orte/mca/ns/ns.h"
#include "orte/mca/oob/tcp/oob_tcp.h"
#include "orte/mca/oob/tcp/oob_tcp_msg.h"


static void mca_oob_tcp_msg_construct(mca_oob_tcp_msg_t*);
static void mca_oob_tcp_msg_destruct(mca_oob_tcp_msg_t*);
static void mca_oob_tcp_msg_ident(mca_oob_tcp_msg_t* msg, mca_oob_tcp_peer_t* peer);
static bool mca_oob_tcp_msg_recv(mca_oob_tcp_msg_t* msg, mca_oob_tcp_peer_t* peer);
static void mca_oob_tcp_msg_data(mca_oob_tcp_msg_t* msg, mca_oob_tcp_peer_t* peer);
static void mca_oob_tcp_msg_ping(mca_oob_tcp_msg_t* msg, mca_oob_tcp_peer_t* peer);


OBJ_CLASS_INSTANCE(
    mca_oob_tcp_msg_t,
    opal_list_item_t,
    mca_oob_tcp_msg_construct,
    mca_oob_tcp_msg_destruct);


static void mca_oob_tcp_msg_construct(mca_oob_tcp_msg_t* msg)
{
    OBJ_CONSTRUCT(&msg->msg_lock, opal_mutex_t);
    OBJ_CONSTRUCT(&msg->msg_condition, opal_condition_t);
}


static void mca_oob_tcp_msg_destruct(mca_oob_tcp_msg_t* msg)
{
    OBJ_DESTRUCT(&msg->msg_lock);
    OBJ_DESTRUCT(&msg->msg_condition);
}


/*
 *  Wait for a msg to complete.
 *  @param  msg (IN)   Message to wait on.
 *  @param  rc (OUT)   Return code (number of bytes read on success or error code on failure).
 *  @retval ORTE_SUCCESS or error code on failure.
 */

int mca_oob_tcp_msg_wait(mca_oob_tcp_msg_t* msg, int* rc)
{
#if OMPI_ENABLE_PROGRESS_THREADS
    OPAL_THREAD_LOCK(&msg->msg_lock);
    while(msg->msg_complete == false) {
        if(opal_event_progress_thread()) {
            int rc;
            OPAL_THREAD_UNLOCK(&msg->msg_lock);
            rc = opal_event_loop(OPAL_EVLOOP_ONCE);
            assert(rc >= 0);
            OPAL_THREAD_LOCK(&msg->msg_lock);
        } else {
           opal_condition_wait(&msg->msg_condition, &msg->msg_lock);
        }
    }
    OPAL_THREAD_UNLOCK(&msg->msg_lock);

#else
    /* wait for message to complete */
    while(msg->msg_complete == false) {
        /* msg_wait() is used in the "barrier" at the completion of
           MPI_FINALIZE, during which time BTLs may still need to
           progress pending outgoing communication, so we need to
           call opal_progress() here to make sure that communication
           gets pushed out so others can enter finalize (and send us
           the message we're here waiting for).  However, if we're
           in a callback from the event library that was triggered
           from a call to opal_progress(), opal_progress() will
           think another thread is already progressing the event
           engine (in the case of mpi threads enabled) and not
           progress the engine, meaning we'll never receive our
           message.  So we also need to progress the event library
           expicitly.  We use EVLOOP_NONBLOCK so that we can
           progress both the registered callbacks and the event
           library, as EVLOOP_ONCE may block for a short period of
           time. */
        opal_progress();
        opal_event_loop(OPAL_EVLOOP_NONBLOCK);
    }
#endif

    /* return status */
    if(NULL != rc) {
        *rc = msg->msg_rc;
    }
    return ORTE_SUCCESS;
}

/*
 *  Wait up to a timeout for the message to complete.
 *  @param  msg (IN)   Message to wait on.
 *  @param  rc (OUT)   Return code (number of bytes read on success or error code on failure).
 *  @retval ORTE_SUCCESS or error code on failure.
 */

int mca_oob_tcp_msg_timedwait(mca_oob_tcp_msg_t* msg, int* rc, struct timespec* abstime)
{
    struct timeval tv;
    uint32_t secs = abstime->tv_sec;
    uint32_t usecs = abstime->tv_nsec * 1000;
    gettimeofday(&tv,NULL);

#if OMPI_ENABLE_PROGRESS_THREADS
    OPAL_THREAD_LOCK(&msg->msg_lock);
    while(msg->msg_complete == false && 
          ((uint32_t)tv.tv_sec <= secs ||
	   ((uint32_t)tv.tv_sec == secs && (uint32_t)tv.tv_usec < usecs))) {
        if(opal_event_progress_thread()) {
            int rc;
            OPAL_THREAD_UNLOCK(&msg->msg_lock);
            rc = opal_event_loop(OPAL_EVLOOP_ONCE);
            assert(rc >= 0);
            OPAL_THREAD_LOCK(&msg->msg_lock);
        } else {
           opal_condition_timedwait(&msg->msg_condition, &msg->msg_lock, abstime);
        }
        gettimeofday(&tv,NULL);
    }
    OPAL_THREAD_UNLOCK(&msg->msg_lock);
#else
    /* wait for message to complete */
    while(msg->msg_complete == false &&
          ((uint32_t)tv.tv_sec <= secs ||
	   ((uint32_t)tv.tv_sec == secs && (uint32_t)tv.tv_usec < usecs))) {
        /* see comment in tcp_msg_wait, above */
        opal_progress();
        opal_event_loop(OPAL_EVLOOP_NONBLOCK);
        gettimeofday(&tv,NULL);
    }
#endif

    /* return status */
    if(NULL != rc) {
        *rc = msg->msg_rc;
    }
    if(msg->msg_rc < 0)
        return msg->msg_rc;
    return (msg->msg_complete ? ORTE_SUCCESS : ORTE_ERR_TIMEOUT);
}

/*
 *  Signal that a message has completed.
 *  @param  msg (IN)   Message to wait on.
 *  @param peer (IN) the peer of the message
 *  @retval ORTE_SUCCESS or error code on failure.
 */
int mca_oob_tcp_msg_complete(mca_oob_tcp_msg_t* msg, orte_process_name_t * peer)
{
    OPAL_THREAD_LOCK(&msg->msg_lock);
    msg->msg_complete = true;
    if(NULL != msg->msg_cbfunc) {
        opal_list_item_t* item;
        OPAL_THREAD_UNLOCK(&msg->msg_lock);

        /* post to a global list of completed messages */
        OPAL_THREAD_LOCK(&mca_oob_tcp_component.tcp_lock);
        opal_list_append(&mca_oob_tcp_component.tcp_msg_completed, (opal_list_item_t*)msg);
        if(opal_list_get_size(&mca_oob_tcp_component.tcp_msg_completed) > 1) {
            OPAL_THREAD_UNLOCK(&mca_oob_tcp_component.tcp_lock);
            return ORTE_SUCCESS;
        }
        OPAL_THREAD_UNLOCK(&mca_oob_tcp_component.tcp_lock);

        /* invoke message callback */
        msg->msg_cbfunc(msg->msg_rc, peer, msg->msg_uiov, msg->msg_ucnt, msg->msg_hdr.msg_tag, msg->msg_cbdata);

        /* dispatch any completed events */
        OPAL_THREAD_LOCK(&mca_oob_tcp_component.tcp_lock);
        opal_list_remove_item(&mca_oob_tcp_component.tcp_msg_completed, (opal_list_item_t*)msg);
        MCA_OOB_TCP_MSG_RETURN(msg);
        while(NULL != 
            (item = opal_list_remove_first(&mca_oob_tcp_component.tcp_msg_completed))) {
            msg = (mca_oob_tcp_msg_t*)item;
            OPAL_THREAD_UNLOCK(&mca_oob_tcp_component.tcp_lock);
            msg->msg_cbfunc(
                msg->msg_rc, 
                &msg->msg_peer, 
                msg->msg_uiov, 
                msg->msg_ucnt, 
                msg->msg_hdr.msg_tag, 
                msg->msg_cbdata);
            OPAL_THREAD_LOCK(&mca_oob_tcp_component.tcp_lock);
            MCA_OOB_TCP_MSG_RETURN(msg);
        }
        OPAL_THREAD_UNLOCK(&mca_oob_tcp_component.tcp_lock);

    } else {
        opal_condition_broadcast(&msg->msg_condition);
        OPAL_THREAD_UNLOCK(&msg->msg_lock);
    }
    return ORTE_SUCCESS;
}

/*
 * The function that actually sends the data!
 * @param msg a pointer to the message to send
 * @param peer the peer we are sending to
 * @retval true if the entire message has been sent
 * @retval false if the entire message has not been sent
 */
bool mca_oob_tcp_msg_send_handler(mca_oob_tcp_msg_t* msg, struct mca_oob_tcp_peer_t * peer)
{
    int rc;
    while(1) {
        rc = writev(peer->peer_sd, msg->msg_rwptr, msg->msg_rwnum);
        if(rc < 0) {
            if(opal_socket_errno == EINTR)
                continue;
            /* In windows, many of the socket functions return an EWOULDBLOCK instead of \
               things like EAGAIN, EINPROGRESS, etc. It has been verified that this will \
               not conflict with other error codes that are returned by these functions \
               under UNIX/Linux environments */
            else if (opal_socket_errno == EAGAIN || opal_socket_errno == EWOULDBLOCK)
                return false;
            else {
                opal_output(0, "[%lu,%lu,%lu]-[%lu,%lu,%lu] mca_oob_tcp_msg_send_handler: writev failed: %s (%d)", 
                    ORTE_NAME_ARGS(orte_process_info.my_name), 
                    ORTE_NAME_ARGS(&(peer->peer_name)), 
                    strerror(opal_socket_errno),
                    opal_socket_errno);
                mca_oob_tcp_peer_close(peer);
                msg->msg_rc = ORTE_ERR_CONNECTION_FAILED;
                return true;
            }
        }

        msg->msg_rc += rc;
        do {/* while there is still more iovecs to write */
            if(rc < (int)msg->msg_rwptr->iov_len) {
                msg->msg_rwptr->iov_len -= rc;
                msg->msg_rwptr->iov_base = (ompi_iov_base_ptr_t)((char *) msg->msg_rwptr->iov_base + rc);
                break;
            } else {
                rc -= msg->msg_rwptr->iov_len;
                (msg->msg_rwnum)--;
                (msg->msg_rwptr)++;
                if(0 == msg->msg_rwnum) {
                    return true;
                }
            }
        } while(msg->msg_rwnum);
    }
}

/*
 * Receives message data.
 * @param msg the message to be recieved into
 * @param peer the peer to recieve from
 * @retval true if the whole message was received
 * @retval false if the whole message was not received
 */
bool mca_oob_tcp_msg_recv_handler(mca_oob_tcp_msg_t* msg, struct mca_oob_tcp_peer_t * peer)
{
    /* has entire header been received */
    if(msg->msg_rwptr == msg->msg_rwiov) {
        if(mca_oob_tcp_msg_recv(msg, peer) == false)
            return false;

        /* allocate a buffer for the receive */
        MCA_OOB_TCP_HDR_NTOH(&msg->msg_hdr);
        if(msg->msg_hdr.msg_size > 0) {
             msg->msg_rwbuf = malloc(msg->msg_hdr.msg_size);
             if(NULL == msg->msg_rwbuf) {
                 opal_output(0, "[%lu,%lu,%lu]-[%lu,%lu,%lu] mca_oob_tcp_msg_recv_handler: malloc(%d) failed\n", 
                     ORTE_NAME_ARGS(orte_process_info.my_name),
                     ORTE_NAME_ARGS(&(peer->peer_name)),
                     msg->msg_hdr.msg_size);
                 mca_oob_tcp_peer_close(peer);
                 return false;
             }
             msg->msg_rwiov[1].iov_base = (ompi_iov_base_ptr_t)msg->msg_rwbuf;
             msg->msg_rwiov[1].iov_len = msg->msg_hdr.msg_size;
             msg->msg_rwnum = 1;
        } else {
             msg->msg_rwiov[1].iov_base = NULL;
             msg->msg_rwiov[1].iov_len = 0;
             msg->msg_rwnum = 0;
        }
    }

    /* do the right thing based on the message type */
    switch(msg->msg_hdr.msg_type)  {
        case MCA_OOB_TCP_IDENT:
            /* done - there is nothing else to receive */
            return true; 
        case MCA_OOB_TCP_PING:
            /* done - there is nothing else to receive */
            return true;
        case MCA_OOB_TCP_DATA:
            /* finish receiving message */
            return mca_oob_tcp_msg_recv(msg, peer);
        default:
            return true;
    }
}

/**
 * Process the current iovec
 */

static bool mca_oob_tcp_msg_recv(mca_oob_tcp_msg_t* msg, mca_oob_tcp_peer_t* peer)
{
    int rc;
    while(msg->msg_rwnum) {
        rc = readv(peer->peer_sd, msg->msg_rwptr, msg->msg_rwnum);
        if(rc < 0) {
            if(opal_socket_errno == EINTR)
                continue;
            /* In windows, many of the socket functions return an EWOULDBLOCK instead of \
               things like EAGAIN, EINPROGRESS, etc. It has been verified that this will \
               not conflict with other error codes that are returned by these functions \
               under UNIX/Linux environments */
            else if (opal_socket_errno == EAGAIN || opal_socket_errno == EWOULDBLOCK)
                return false;
            else {
                opal_output(0, "[%lu,%lu,%lu]-[%lu,%lu,%lu] mca_oob_tcp_msg_recv: readv failed: %s (%d)", 
                    ORTE_NAME_ARGS(orte_process_info.my_name),
                    ORTE_NAME_ARGS(&(peer->peer_name)),
                    strerror(opal_socket_errno),
                    opal_socket_errno);
                mca_oob_tcp_peer_close(peer);
                mca_oob_call_exception_handlers(&peer->peer_name, MCA_OOB_PEER_DISCONNECTED);
                return false;
            }
        } else if (rc == 0)  {
            if(mca_oob_tcp_component.tcp_debug >= OOB_TCP_DEBUG_CONNECT_FAIL) {
                opal_output(0, "[%lu,%lu,%lu]-[%lu,%lu,%lu] mca_oob_tcp_msg_recv: peer closed connection", 
                   ORTE_NAME_ARGS(orte_process_info.my_name),
                   ORTE_NAME_ARGS(&(peer->peer_name)));
            }
            mca_oob_tcp_peer_close(peer);
            mca_oob_call_exception_handlers(&peer->peer_name, MCA_OOB_PEER_DISCONNECTED);
            return false;
        }

        do {
            if(rc < (int)msg->msg_rwptr->iov_len) {
                msg->msg_rwptr->iov_len -= rc;
                msg->msg_rwptr->iov_base = (ompi_iov_base_ptr_t)((char *) msg->msg_rwptr->iov_base + rc);
                break;
            } else {
                rc -= msg->msg_rwptr->iov_len;
                (msg->msg_rwnum)--;
                (msg->msg_rwptr)++;
                if(0 == msg->msg_rwnum) {
                    return true;
                }
            }
        } while(msg->msg_rwnum);
    }
    return true;
}

/**
 * Process a completed message.
 */

void mca_oob_tcp_msg_recv_complete(mca_oob_tcp_msg_t* msg, mca_oob_tcp_peer_t* peer)
{
    switch(msg->msg_hdr.msg_type)  {
        case MCA_OOB_TCP_IDENT:
            mca_oob_tcp_msg_ident(msg,peer);
            break;
        case MCA_OOB_TCP_PING:
            mca_oob_tcp_msg_ping(msg,peer);
            break;
        case MCA_OOB_TCP_DATA:
            mca_oob_tcp_msg_data(msg,peer);
            break;
        default:
            opal_output(0, "[%lu,%lu,%lu] mca_oob_tcp_msg_recv_complete: invalid message type: %d\n",
                 ORTE_NAME_ARGS(orte_process_info.my_name), msg->msg_hdr.msg_type);
            MCA_OOB_TCP_MSG_RETURN(msg);
            break;
    }
}

/**
 * Process an ident message. In this case, we insist that the two process names
 * exactly match - hence, we use the orte_ns.compare_fields function, which
 * checks each field in a literal manner (i.e., no wildcards).
 */

static void mca_oob_tcp_msg_ident(mca_oob_tcp_msg_t* msg, mca_oob_tcp_peer_t* peer)
{
    orte_process_name_t src = msg->msg_hdr.msg_src;
    
    OPAL_THREAD_LOCK(&mca_oob_tcp_component.tcp_lock);
    if (orte_ns.compare_fields(ORTE_NS_CMP_ALL, &peer->peer_name, &src) != ORTE_EQUAL) {
        orte_hash_table_remove_proc(&mca_oob_tcp_component.tcp_peers, &peer->peer_name);
        peer->peer_name = src;
        orte_hash_table_set_proc(&mca_oob_tcp_component.tcp_peers, &peer->peer_name, peer);
    }
    OPAL_THREAD_UNLOCK(&mca_oob_tcp_component.tcp_lock);
}


/**
 * Process a ping message.
 */

static void mca_oob_tcp_msg_ping(mca_oob_tcp_msg_t* msg, mca_oob_tcp_peer_t* peer)
{
    /* for now - we dont do anything - may want to send back a response at some poing */
}


/*
 * Progress a completed recv:
 * (1) signal a posted recv as complete
 * (2) queue an unexpected message in the recv list
 */

static void mca_oob_tcp_msg_data(mca_oob_tcp_msg_t* msg, mca_oob_tcp_peer_t* peer)
{
    /* attempt to match unexpected message to a posted recv */
    mca_oob_tcp_msg_t* post;
    OPAL_THREAD_LOCK(&mca_oob_tcp_component.tcp_match_lock);

    /* match msg against posted receives */
    post = mca_oob_tcp_msg_match_post(&peer->peer_name, msg->msg_hdr.msg_tag);
    if(NULL != post) {

        if(post->msg_flags & MCA_OOB_ALLOC) {

            /* set the users iovec struct to point to pre-allocated buffer */
            if(NULL == post->msg_uiov || 0 == post->msg_ucnt) {
                post->msg_rc = ORTE_ERR_BAD_PARAM;
            } else {
                /* first iovec of recv message contains the header -
                 * subsequent contain user data
                */
                post->msg_uiov[0].iov_base = (ompi_iov_base_ptr_t)msg->msg_rwbuf;
                post->msg_uiov[0].iov_len = msg->msg_hdr.msg_size;
		        post->msg_rc = msg->msg_hdr.msg_size;
                msg->msg_rwbuf = NULL;
            }

        } else {

            /* copy msg data into posted recv */
            post->msg_rc = mca_oob_tcp_msg_copy(msg, post->msg_uiov, post->msg_ucnt);
            if(post->msg_flags & MCA_OOB_TRUNC) {
                 int i, size = 0;
                 for(i=1; i<msg->msg_rwcnt+1; i++)
                     size += msg->msg_rwiov[i].iov_len;
                 post->msg_rc = size;
            }
        }

        if(post->msg_flags & MCA_OOB_PEEK) {
            /* will need message for actual receive */
            opal_list_append(&mca_oob_tcp_component.tcp_msg_recv, &msg->super.super);
        } else {
            MCA_OOB_TCP_MSG_RETURN(msg);
        }
        mca_oob_tcp_component.tcp_match_count++;
        OPAL_THREAD_UNLOCK(&mca_oob_tcp_component.tcp_match_lock);

        if(post->msg_flags & MCA_OOB_PERSISTENT) {
            post->msg_cbfunc(
                post->msg_rc, 
                &peer->peer_name, 
                post->msg_uiov, 
                post->msg_ucnt, 
                post->msg_hdr.msg_tag, 
                post->msg_cbdata);
        } else {
            mca_oob_tcp_msg_complete(post, &peer->peer_name);
        }

        OPAL_THREAD_LOCK(&mca_oob_tcp_component.tcp_match_lock);
        if(--mca_oob_tcp_component.tcp_match_count == 0)
            opal_condition_signal(&mca_oob_tcp_component.tcp_match_cond);
        OPAL_THREAD_UNLOCK(&mca_oob_tcp_component.tcp_match_lock);

    } else {
        opal_list_append(&mca_oob_tcp_component.tcp_msg_recv, (opal_list_item_t*)msg);
        OPAL_THREAD_UNLOCK(&mca_oob_tcp_component.tcp_match_lock);
    }
}
                                                                                                                              

/*
 *  Called to copy the results of a message into user supplied iovec array.
 *  @param  msg (IN)   Message send that is in progress.
 *  @param  iov (IN)   Iovec array of user supplied buffers.
 *  @retval count      Number of elements in iovec array.
 */

int mca_oob_tcp_msg_copy(mca_oob_tcp_msg_t* msg, struct iovec* iov, int count)
{
    int i;
    unsigned char* src_ptr = (unsigned char*)msg->msg_rwbuf;
    size_t src_len = msg->msg_hdr.msg_size;
    struct iovec *dst = iov;
    int rc = 0;

    for(i=0; i<count; i++) {
        unsigned char* dst_ptr = (unsigned char*)dst->iov_base;
        size_t dst_len = dst->iov_len;
        while(dst_len > 0) {
            size_t len = (dst_len <= src_len) ? dst_len : src_len;
            memcpy(dst_ptr, src_ptr, len);
            rc += len;
            dst_ptr += len;
            dst_len -= len;
            src_ptr += len;
            src_len -= len;
            if(src_len == 0) {
                return rc;
            }
        }
        dst++;
    }
    return rc;
}

/*
 *  Match name to a message that has been received asynchronously (unexpected).
 *
 *  @param  name (IN)  Name associated with peer or wildcard to match first posted recv.
 *  @return msg        Matched message or NULL.
 *
 *  Note - this routine requires the caller to be holding the module lock.
 */

mca_oob_tcp_msg_t* mca_oob_tcp_msg_match_recv(orte_process_name_t* name, int tag)
{
    mca_oob_tcp_msg_t* msg;
    for(msg =  (mca_oob_tcp_msg_t*) opal_list_get_first(&mca_oob_tcp_component.tcp_msg_recv);
        msg != (mca_oob_tcp_msg_t*) opal_list_get_end(&mca_oob_tcp_component.tcp_msg_recv);
        msg =  (mca_oob_tcp_msg_t*) opal_list_get_next(msg)) {

        if(ORTE_EQUAL == orte_dss.compare(name, &msg->msg_peer, ORTE_NAME)) {
            if (tag == msg->msg_hdr.msg_tag) {
                return msg;
            }
        }
    }
    return NULL;
}

/*
 *  Match name to a posted recv request.
 *
 *  @param  name (IN)  Name associated with peer or wildcard to match first posted recv.
 *  @return msg        Matched message or NULL.
 *
 *  Note - this routine requires the caller to be holding the module lock.
 */
                                                                                                                    
mca_oob_tcp_msg_t* mca_oob_tcp_msg_match_post(orte_process_name_t* name, int tag)
{
    mca_oob_tcp_msg_t* msg;
    for(msg =  (mca_oob_tcp_msg_t*) opal_list_get_first(&mca_oob_tcp_component.tcp_msg_post);
        msg != (mca_oob_tcp_msg_t*) opal_list_get_end(&mca_oob_tcp_component.tcp_msg_post);
        msg =  (mca_oob_tcp_msg_t*) opal_list_get_next(msg)) {

        if(ORTE_EQUAL == orte_dss.compare(name, &msg->msg_peer, ORTE_NAME)) {
            if (msg->msg_hdr.msg_tag == tag) {
                if((msg->msg_flags & MCA_OOB_PERSISTENT) == 0) {
                    opal_list_remove_item(&mca_oob_tcp_component.tcp_msg_post, &msg->super.super);
                }
                return msg;
            }
        }
    }
    return NULL;
}




