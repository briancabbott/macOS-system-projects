/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Portions of this software have been released under the following terms:
 *
 * (c) Copyright 1989-1993 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1989-1993 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1989-1993 DIGITAL EQUIPMENT CORPORATION
 *
 * To anyone who acknowledges that this file is provided "AS IS"
 * without any express or implied warranty:
 * permission to use, copy, modify, and distribute this file for any
 * purpose is hereby granted without fee, provided that the above
 * copyright notices and this notice appears in all source code copies,
 * and that none of the names of Open Software Foundation, Inc., Hewlett-
 * Packard Company or Digital Equipment Corporation be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  Neither Open Software
 * Foundation, Inc., Hewlett-Packard Company nor Digital
 * Equipment Corporation makes any representations about the suitability
 * of this software for any purpose.
 *
 * Copyright (c) 2007, Novell, Inc. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Novell Inc. nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
**
**  NAME:
**
**      eenodtbl.c
**
**  FACILITY:
**
**      IDL Stub Runtime Support
**
**  ABSTRACT:
**
**      Multi-threading support for callee nodes
**
**  VERSION: DCE 1.0
**
*/
#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <dce/rpc.h>
#include <dce/stubbase.h>
#include <lsysdep.h>
#include <assert.h>

static idl_void_p_t rpc_ss_allocate_ex
(
    idl_void_p_t context ATTRIBUTE_UNUSED,
    idl_size_t size
)
{
    assert(context == NULL);
    return rpc_ss_allocate(size);
}

static void rpc_ss_free_ex
(
    idl_void_p_t context ATTRIBUTE_UNUSED,
    idl_void_p_t ptr
)
{
    assert(context == NULL);
    rpc_ss_free(ptr);
}

/******************************************************************************/
/*                                                                            */
/*    Fill in a support pointers structure and create an indirection          */
/*      for this thread                                                       */
/*                                                                            */
/******************************************************************************/
void rpc_ss_build_indirection_struct
(
    rpc_ss_thread_support_ptrs_t *p_thread_support_ptrs,
    rpc_ss_mem_handle *p_mem_handle,
    idl_boolean free_referents
)
{
    rpc_ss_thread_indirection_t *helper_thread_indirection_ptr;

    /* If a context exists, destroy it */
    RPC_SS_THREADS_KEY_GET_CONTEXT( rpc_ss_thread_supp_key,
                                       &helper_thread_indirection_ptr );
    if ( helper_thread_indirection_ptr != NULL )
    {
        free( helper_thread_indirection_ptr );
    }

    RPC_SS_THREADS_MUTEX_CREATE(&(p_thread_support_ptrs->mutex));
    p_thread_support_ptrs->p_mem_h = p_mem_handle;
    p_thread_support_ptrs->allocator.p_allocate = rpc_ss_allocate_ex;
    p_thread_support_ptrs->allocator.p_free = rpc_ss_free_ex;
    p_thread_support_ptrs->allocator.p_context = NULL;

    helper_thread_indirection_ptr = (rpc_ss_thread_indirection_t *)
                            malloc(sizeof(rpc_ss_thread_indirection_t));
    helper_thread_indirection_ptr->indirection = p_thread_support_ptrs;
    helper_thread_indirection_ptr->free_referents = free_referents;
    RPC_SS_THREADS_KEY_SET_CONTEXT( rpc_ss_thread_supp_key,
                                       helper_thread_indirection_ptr );
}

/******************************************************************************/
/*                                                                            */
/*    Create a support pointers structure for this thread                     */
/*      Only called from server stub                                          */
/*                                                                            */
/******************************************************************************/
void rpc_ss_create_support_ptrs
(
    rpc_ss_thread_support_ptrs_t *p_thread_support_ptrs,
    rpc_ss_mem_handle *p_mem_handle
)
{
    rpc_ss_build_indirection_struct(p_thread_support_ptrs, p_mem_handle,
                                    idl_false);
}

/******************************************************************************/
/*                                                                            */
/*    Return the address of the support pointers structure for this thread    */
/*                                                                            */
/******************************************************************************/
void rpc_ss_get_support_ptrs
(
    rpc_ss_thread_support_ptrs_t **p_p_thread_support_ptrs
)
{
    rpc_ss_thread_indirection_t *helper_thread_indirection_ptr;

    RPC_SS_THREADS_KEY_GET_CONTEXT( rpc_ss_thread_supp_key,
                                       &helper_thread_indirection_ptr );
    *p_p_thread_support_ptrs = helper_thread_indirection_ptr->indirection;
}

/******************************************************************************/
/*                                                                            */
/*    Destroy the support pointers structure for this thread                  */
/*      Only called from server stub                                          */
/*                                                                            */
/******************************************************************************/
void rpc_ss_destroy_support_ptrs(
    void
)
{
    rpc_ss_thread_indirection_t *helper_thread_indirection_ptr;
    rpc_ss_thread_support_ptrs_t *p_thread_support_ptrs;

    /* Destroy the mutex that the context points at */
    RPC_SS_THREADS_KEY_GET_CONTEXT( rpc_ss_thread_supp_key,
                                       &helper_thread_indirection_ptr );
    if (helper_thread_indirection_ptr == NULL)
        return;
    p_thread_support_ptrs = helper_thread_indirection_ptr->indirection;
    RPC_SS_THREADS_MUTEX_DELETE( &(p_thread_support_ptrs->mutex) );
    /* There is no need to delete the support_ptrs structure as it is
        on the server stub stack */

    /* free the ptr context storage */
    free( helper_thread_indirection_ptr );

    /* And destroy the context - this is required for Kernel RPC */
    RPC_SS_THREADS_KEY_SET_CONTEXT( rpc_ss_thread_supp_key, NULL );
}

/*
 *  The rpc_sm_... routines have the same functionality as the corresponding
 *  rpc_ss_... routines but have an extra, error_status_t, parameter, which
 *  is used instead of exceptions for error reporting.
 */

/******************************************************************************/
/*                                                                            */
/*    rpc_sm_allocate                                                         */
/*                                                                            */
/******************************************************************************/
idl_void_p_t rpc_sm_allocate
(
    idl_size_t size,
    error_status_t *p_st
)
{
    idl_void_p_t volatile result = NULL;

    // DO_NOT_CLOBBER(result);

    *p_st = error_status_ok;
    DCETHREAD_TRY	{
        result = rpc_ss_allocate(size);
	 }
    DCETHREAD_CATCH(rpc_x_no_memory)	{
        *p_st = rpc_s_no_memory;
	 }
    DCETHREAD_ENDTRY;
    return result;
}

/****************************************************************************/
/*                                                                          */
/* rpc_sm_client_free                                                       */
/*                                                                          */
/****************************************************************************/
void rpc_sm_client_free
(
    rpc_void_p_t p_mem,
    error_status_t *p_st
)
{
    *p_st = error_status_ok;
    rpc_ss_client_free(p_mem);
}

/******************************************************************************/
/*                                                                            */
/* Function to be called by user to release unusable context_handle           */
/*                                                                            */
/******************************************************************************/
void rpc_sm_destroy_client_context
(
    rpc_ss_context_t *p_unusable_context_handle,
    error_status_t *p_st
)
{
    *p_st = error_status_ok;
    DCETHREAD_TRY
        rpc_ss_destroy_client_context(p_unusable_context_handle);
    DCETHREAD_CATCH(rpc_x_no_memory)
        *p_st = rpc_s_no_memory;
    DCETHREAD_ENDTRY
}

/******************************************************************************/
/*                                                                            */
/*    rpc_sm_disable_allocate                                                 */
/*                                                                            */
/******************************************************************************/
void rpc_sm_disable_allocate
(error_status_t *p_st )
{
    *p_st = error_status_ok;
    rpc_ss_disable_allocate();
}

/******************************************************************************/
/*                                                                            */
/*    rpc_sm_enable_allocate                                                  */
/*                                                                            */
/******************************************************************************/
void rpc_sm_enable_allocate
(error_status_t *p_st)
{
    *p_st = error_status_ok;
    DCETHREAD_TRY
        rpc_ss_enable_allocate();
    DCETHREAD_CATCH(rpc_x_no_memory)
        *p_st = rpc_s_no_memory;
    DCETHREAD_ENDTRY
}

/******************************************************************************/
/*                                                                            */
/*    rpc_sm_free                                                             */
/*                                                                            */
/******************************************************************************/
void rpc_sm_free
(
    rpc_void_p_t node_to_free,
    error_status_t *p_st
)
{
    *p_st = error_status_ok;
    rpc_ss_free(node_to_free);
}

/******************************************************************************/
/*                                                                            */
/*    rpc_sm_get_thread_handle                                                */
/*                                                                            */
/******************************************************************************/
#if 0
/* Removed unused symbol for rdar://problem/26430747 */
rpc_ss_thread_handle_t rpc_sm_get_thread_handle
( error_status_t *p_st )
{
    *p_st = error_status_ok;
    return rpc_ss_get_thread_handle();
}
#endif

/******************************************************************************/
/*                                                                            */
/*    Create thread context with references to named alloc and free rtns      */
/*                                                                            */
/******************************************************************************/
void rpc_sm_set_client_alloc_free
(
    rpc_ss_p_alloc_t p_allocate,
    rpc_ss_p_free_t p_free,
    error_status_t *p_st
)
{
    *p_st = error_status_ok;
    DCETHREAD_TRY
        rpc_ss_set_client_alloc_free(p_allocate, p_free);
    DCETHREAD_CATCH(rpc_x_no_memory)
        *p_st = rpc_s_no_memory;
    DCETHREAD_ENDTRY
}

/******************************************************************************/
/*                                                                            */
/*    rpc_sm_set_thread_handle                                                */
/*                                                                            */
/******************************************************************************/
#if 0
/* Removed unused symbol for rdar://problem/26430747 */
void rpc_sm_set_thread_handle
(
    rpc_ss_thread_handle_t thread_handle,
    error_status_t *p_st
)
{
    *p_st = error_status_ok;
    DCETHREAD_TRY
        rpc_ss_set_thread_handle(thread_handle);
    DCETHREAD_CATCH(rpc_x_no_memory)
        *p_st = rpc_s_no_memory;
    DCETHREAD_ENDTRY
}
#endif

/******************************************************************************/
/*                                                                            */
/*    Get the existing allocate, free routines and replace them with new ones */
/*                                                                            */
/******************************************************************************/
void rpc_sm_swap_client_alloc_free
(
    rpc_ss_p_alloc_t p_allocate,
    rpc_ss_p_free_t p_free,
    rpc_ss_p_alloc_t * p_p_old_allocate,
    rpc_ss_p_free_t  * p_p_old_free,
    error_status_t *p_st
)
{
    *p_st = error_status_ok;
    DCETHREAD_TRY
        rpc_ss_swap_client_alloc_free(p_allocate, p_free, p_p_old_allocate,
                                                                p_p_old_free);
    DCETHREAD_CATCH(rpc_x_no_memory)
        *p_st = rpc_s_no_memory;
    DCETHREAD_ENDTRY
}
