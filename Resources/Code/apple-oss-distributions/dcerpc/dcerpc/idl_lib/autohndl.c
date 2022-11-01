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
**
**  NAME:
**
**      autohndl.c
**
**  FACILITY:
**
**      IDL Stub Runtime Support
**
**  ABSTRACT:
**
**      Support for [auto_handle] client
**
**  VERSION: DCE 1.0
**
**
*/
#if HAVE_CONFIG_H
#include <config.h>
#endif

/* The ordering of the following 3 includes should NOT be changed! */
#include <dce/rpc.h>
#include <dce/stubbase.h>
#include <lsysdep.h>

/*******************************************************************************/
/*                                                                             */
/*   If there is not currently a valid import cursor, get one                  */
/*                                                                             */
/*******************************************************************************/
void rpc_ss_make_import_cursor_valid
(
    RPC_SS_THREADS_MUTEX_T *p_import_cursor_mutex,
    rpc_ns_handle_t *p_import_cursor,
    rpc_if_handle_t p_if_spec,
    error_status_t *p_import_cursor_status
)
{
    RPC_SS_THREADS_MUTEX_LOCK( p_import_cursor_mutex );
    DCETHREAD_TRY
    if ( *p_import_cursor_status != error_status_ok )
    {
        rpc_ns_binding_import_begin(rpc_c_ns_syntax_default, NULL,
                   p_if_spec, NULL, p_import_cursor, p_import_cursor_status);
    }
    DCETHREAD_FINALLY
    RPC_SS_THREADS_MUTEX_UNLOCK( p_import_cursor_mutex );
    DCETHREAD_ENDTRY
}

/*******************************************************************************/
/*                                                                             */
/*   Get next potential server                                                 */
/*                                                                             */
/*******************************************************************************/
void rpc_ss_import_cursor_advance
(
    RPC_SS_THREADS_MUTEX_T *p_import_cursor_mutex,
    ndr_boolean *p_cache_timeout_was_set_low, /* true if the cache time out */
                                              /* for this import context    */
                                              /* was set low at some point. */
    rpc_ns_handle_t *p_import_cursor,
    rpc_if_handle_t p_if_spec,
    ndr_boolean *p_binding_had_error,
        /* TRUE if an error occurred using the current binding */
    rpc_binding_handle_t *p_interface_binding,
        /* Ptr to binding currently being used for this interface */
    rpc_binding_handle_t *p_operation_binding,
      /* Ptr to location for binding operation is using, NULL if first attempt */
    error_status_t *p_import_cursor_status,
    error_status_t *p_st
)
{
    error_status_t st2;

    RPC_SS_THREADS_MUTEX_LOCK( p_import_cursor_mutex );
    DCETHREAD_TRY
    if ( ( ! rpc_binding_handle_equal(*p_operation_binding,
                                      *p_interface_binding, p_st) )
            && ( ! *p_binding_had_error ) )
    {
        /* Another thread has advanced the cursor
           and no error has yet been detected */
        if ( *p_interface_binding != NULL)
        {
            rpc_binding_handle_copy( *p_interface_binding,
                                    p_operation_binding, p_st);

        }
        else
        {
            /* No more servers to try */
            *p_st = rpc_s_no_more_bindings;
        }
        goto mutex_release;
    }

    /* If we currently have a binding, release it */
    if ( *p_interface_binding != NULL)
    {
        rpc_binding_free( p_interface_binding, p_st);
    }
    /* Any new binding will not yet have errors */
    *p_binding_had_error = ndr_false;

    /* Advance the cursor */
    rpc_ns_binding_import_next ( *p_import_cursor, p_interface_binding, p_st );
    while (( *p_st == rpc_s_no_more_bindings ) &&
           ( *p_cache_timeout_was_set_low == idl_false))
    {
        /* Make ready to restart */
        *p_interface_binding = NULL;
        rpc_ns_binding_import_done( p_import_cursor, p_import_cursor_status);
        rpc_ns_binding_import_begin(rpc_c_ns_syntax_default, NULL,
               p_if_spec, NULL, p_import_cursor, p_import_cursor_status);

        /*
         * If we have never lowered the cache timeout value, then we may
         * have gotten the no more bindings status because of a stale cache.
         * Lower the cache timeout and try to import again.
         */
        if (*p_cache_timeout_was_set_low == idl_false)
        {
            /* Note that we have reset the cache timeout */
            *p_cache_timeout_was_set_low = idl_true;

            /*
             *  Set a low cache timeout to force a refresh on this import
             *  context.
             */
            rpc_ns_mgmt_handle_set_exp_age(
                            (rpc_ns_handle_t) *p_import_cursor, 10, p_st );

            rpc_ns_binding_import_next
                           ( *p_import_cursor, p_interface_binding, p_st );
        }
    }

    if ( *p_interface_binding == NULL ) *p_operation_binding = NULL;
    else
    {
	rpc_binding_handle_copy( *p_interface_binding,
				p_operation_binding, &st2);

    }

 mutex_release:;
    DCETHREAD_FINALLY
    RPC_SS_THREADS_MUTEX_UNLOCK( p_import_cursor_mutex );
    DCETHREAD_ENDTRY
}

/*******************************************************************************/
/*                                                                             */
/*   Flag "error occurred when an operation used this binding"                 */
/*                                                                             */
/*******************************************************************************/
void rpc_ss_flag_error_on_binding
(
    RPC_SS_THREADS_MUTEX_T *p_import_cursor_mutex,
    ndr_boolean *p_binding_had_error,
    rpc_binding_handle_t *p_interface_binding,
        /* Ptr to binding currently being used for this interface */
    rpc_binding_handle_t *p_operation_binding
      /* Ptr to location for binding operation is using */
)
{
    RPC_SS_THREADS_MUTEX_LOCK( p_import_cursor_mutex );
    if ( *p_interface_binding == *p_operation_binding )
    {
        /* Nobody has advanced the cursor
           Flag that error occurred using this binding */
        *p_binding_had_error = ndr_true;
    }
    RPC_SS_THREADS_MUTEX_UNLOCK( p_import_cursor_mutex );
}
