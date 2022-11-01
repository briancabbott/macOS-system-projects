/* -*- mode: c; indent-tabs-mode: nil -*- */
/*
 * Copyright 1993 by OpenVision Technologies, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of OpenVision not be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission. OpenVision makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * OPENVISION DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL OPENVISION BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "gssapiP_krb5.h"

/*
 * $Id: unseal.c 20876 2008-10-15 21:58:43Z tlyu $
 */

OM_uint32
krb5_gss_unseal(minor_status, context_handle,
                input_message_buffer, output_message_buffer,
                conf_state, qop_state)
    OM_uint32 *minor_status;
    gss_ctx_id_t context_handle;
    gss_buffer_t input_message_buffer;
    gss_buffer_t output_message_buffer;
    int *conf_state;
    int *qop_state;
{
    return(kg_unseal(minor_status, context_handle,
                     input_message_buffer, output_message_buffer,
                     conf_state, qop_state, KG_TOK_SEAL_MSG));
}

/* V2 interface */
OM_uint32
krb5_gss_unwrap(minor_status, context_handle,
                input_message_buffer, output_message_buffer,
                conf_state, qop_state)
    OM_uint32           *minor_status;
    gss_ctx_id_t        context_handle;
    gss_buffer_t        input_message_buffer;
    gss_buffer_t        output_message_buffer;
    int                 *conf_state;
    gss_qop_t           *qop_state;
{
    OM_uint32           rstat;
    int                 qstate;

    rstat = kg_unseal(minor_status, context_handle,
                      input_message_buffer, output_message_buffer,
                      conf_state, &qstate, KG_TOK_WRAP_MSG);
    if (!rstat && qop_state)
        *qop_state = (gss_qop_t) qstate;
    return(rstat);
}
