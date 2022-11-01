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
**  NAME
**
**      noauthcn.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC)
**
**  ABSTRACT:
**
**  The noauth CN authentication module.
**
**
*/

#include <noauth.h>

#include <pwd.h>                /* for getpwuid, etc, for "level_none" */

#include <noauthcn.h>

INTERNAL boolean32 rpc__noauth_cn_three_way (void);

INTERNAL boolean32 rpc__noauth_cn_context_valid (
        rpc_cn_sec_context_p_t           /*sec*/,
        unsigned32                      * /*st*/
    );

INTERNAL void rpc__noauth_cn_create_info (
       rpc_authn_level_t                 /*authn_level*/,
       rpc_auth_info_p_t                * /*auth_info*/,
       unsigned32                       * /*st*/
    );

INTERNAL boolean32 rpc__noauth_cn_cred_changed (
        rpc_cn_sec_context_p_t           /*sec*/,
        unsigned32                      * /*st*/
    );

INTERNAL void rpc__noauth_cn_cred_refresh (
        rpc_auth_info_p_t                /*auth_info*/,
        unsigned32                      * /*st*/
    );

INTERNAL void rpc__noauth_cn_fmt_client_req (
        rpc_cn_assoc_sec_context_p_t     /*assoc_sec*/,
        rpc_cn_sec_context_p_t           /*sec*/,
        dce_pointer_t                        /*auth_value*/,
        unsigned32                      * /*auth_value_len*/,
        unsigned32                      * /*st*/
    );

INTERNAL void rpc__noauth_cn_fmt_srvr_resp (
        unsigned32                       /*verify_st*/,
        rpc_cn_assoc_sec_context_p_t     /*assoc_sec*/,
        rpc_cn_sec_context_p_t           /*sec*/,
        dce_pointer_t                        /*req_auth_value*/,
        unsigned32                       /*req_auth_value_len*/,
        dce_pointer_t                        /*auth_value*/,
        unsigned32                      * /*auth_value_len*/
    );

INTERNAL void rpc__noauth_cn_free_prot_info (
        rpc_auth_info_p_t                /*info*/,
        rpc_cn_auth_info_p_t            * /*cn_info*/
    );

INTERNAL void rpc__noauth_cn_get_prot_info (
        rpc_auth_info_p_t                /*info*/,
        rpc_cn_auth_info_p_t            * /*cn_info*/,
        unsigned32                      * /*st*/
    );

INTERNAL void rpc__noauth_cn_pre_call (
        rpc_cn_assoc_sec_context_p_t     /*assoc_sec*/,
        rpc_cn_sec_context_p_t           /*sec*/,
        dce_pointer_t                        /*auth_value*/,
        unsigned32                      * /*auth_value_len*/,
        unsigned32                      * /*st*/
    );

INTERNAL void rpc__noauth_cn_pre_send (
        rpc_cn_assoc_sec_context_p_t     /*assoc_sec*/,
        rpc_cn_sec_context_p_t           /*sec*/,
        rpc_socket_iovec_p_t             /*iov*/,
        unsigned32                       /*iovlen*/,
	rpc_socket_iovec_p_t             /*out_iov*/,
        unsigned32                      * /*st*/
    );

INTERNAL void rpc__noauth_cn_recv_check (
        rpc_cn_assoc_sec_context_p_t     /*assoc_sec*/,
        rpc_cn_sec_context_p_t           /*sec*/,
        rpc_cn_common_hdr_p_t            /*pdu*/,
        unsigned32                       /*pdu_len*/,
        unsigned32                       /*cred_len*/,
        rpc_cn_auth_tlr_p_t              /*auth_tlr*/,
        boolean32                        /*unpack_ints*/,
        unsigned32                      * /*st*/
    );

INTERNAL void rpc__noauth_cn_tlr_uuid_crc (
        dce_pointer_t                /*auth_value*/,
        unsigned32               /*auth_value_len*/,
        unsigned32              * /*uuid_crc*/
    );

INTERNAL void rpc__noauth_cn_tlr_unpack (
        rpc_cn_packet_p_t        /*pkt_p*/,
        unsigned32               /*auth_value_len*/,
        unsigned8               * /*packed_drep*/
    );

INTERNAL void rpc__noauth_cn_vfy_client_req (
        rpc_cn_assoc_sec_context_p_t     /*assoc_sec*/,
        rpc_cn_sec_context_p_t           /*sec*/,
        dce_pointer_t                        /*auth_value*/,
        unsigned32                       /*auth_value_len*/,
        unsigned32                      * /*st*/
    );

INTERNAL void rpc__noauth_cn_vfy_srvr_resp (
        rpc_cn_assoc_sec_context_p_t     /*assoc_sec*/,
        rpc_cn_sec_context_p_t           /*sec*/,
        dce_pointer_t                        /*auth_value*/,
        unsigned32                       /*auth_value_len*/,
        unsigned32                      * /*st*/
    );

INTERNAL const rpc_cn_auth_epv_t rpc_g_noauth_cn_epv =
{
    .three_way	    = rpc__noauth_cn_three_way,
    .context_valid  = rpc__noauth_cn_context_valid,
    .create_info    = rpc__noauth_cn_create_info,
    .cred_changed   = rpc__noauth_cn_cred_changed,
    .cred_refresh   = rpc__noauth_cn_cred_refresh,
    .fmt_client_req = rpc__noauth_cn_fmt_client_req,
    .fmt_srvr_resp  = rpc__noauth_cn_fmt_srvr_resp,
    .free_prot_info = rpc__noauth_cn_free_prot_info,
    .get_prot_info  = rpc__noauth_cn_get_prot_info,
    .pre_call	    = rpc__noauth_cn_pre_call,
    .pre_send	    = rpc__noauth_cn_pre_send,
    .recv_check	    = rpc__noauth_cn_recv_check,
    .tlr_uuid_crc   = rpc__noauth_cn_tlr_uuid_crc,
    .tlr_unpack	    = rpc__noauth_cn_tlr_unpack,
    .vfy_client_req = rpc__noauth_cn_vfy_client_req,
    .vfy_srvr_resp  = rpc__noauth_cn_vfy_srvr_resp
};

/*****************************************************************************/
/*
**++
**
**  ROUTINE NAME:       rpc__noauth_cn_three_way
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**
**      Determine whether the authentication protocol requires a
**      3-way authentication handshake. If true the client is expected to
**      provide an rpc_auth3 PDU before the security context is fully
**      established and a call can be made.
**
**  INPUTS:             none
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:            none
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     boolean32
**
**      True if the authentication protocol requires a 3-way
**      authentication handshake.
**
**  SIDE EFFECTS:       none
**
**--
**/

INTERNAL boolean32 rpc__noauth_cn_three_way (void)
{
    RPC_DBG_PRINTF (rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_ROUTINE_TRACE,
                    ("(rpc__noauth_cn_three_way)\n"));

    return (false);
}

/*****************************************************************************/
/*
**++
**
**  ROUTINE NAME:       rpc__noauth_cn_context_valid
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**
**      Determine whether the established security context will be
**      valid (i. e. timely) for the next 300 seconds. If
**      not this routine will try to renew the context.
**      If it cannot be renewed false is returned. This is
**      called from the client side.
**
**  INPUTS:
**
**      sec             A pointer to security context element which includes
**                      the key ID, auth information rep and RPC auth
**                      information rep.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      st              The return status of this routine.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     boolean32
**
**      True if security context identified by the auth
**      information rep and RPC auth information rep will
**      still be valid in 300 seconds, false if not.
**
**  SIDE EFFECTS:
**
**      The context may be renewed.
**
**--
**/

INTERNAL boolean32 rpc__noauth_cn_context_valid
(
    rpc_cn_sec_context_p_t          sec,
    unsigned32                      *st
)
{
    CODING_ERROR (st);
    RPC_DBG_PRINTF (rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_ROUTINE_TRACE,
                    ("(rpc__noauth_cn_context_valid)\n"));

    RPC_DBG_PRINTF (rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_PKT,
                    ("(rpc__noauth_cn_context_valid) prot->%x level->%x key_id->%x\n",
                    rpc_c_authn_dce_dummy,
                    sec->sec_info->authn_level,
                    sec->sec_key_id));

#ifdef DEBUG
    if (RPC_DBG_EXACT(rpc_es_dbg_cn_errors,
                      RPC_C_CN_DBG_AUTH_CONTEXT_VALID))
    {
        *st = RPC_S_CN_DBG_AUTH_FAILURE;
        return (false);
    }
#endif

    *st = rpc_s_ok;
    return (true);
}


/*****************************************************************************/
/*
**++
**
**  ROUTINE NAME:       rpc__noauth_cn_create_info
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**
**      Create an auth information rep data structure with and
**      add a reference to it. This is called on the server
**      side. The fields will be initialized to NULL values.
**      The caller should fill them in based on information
**      decrypted from the authenticator passed in the bind
**      request.
**
**  INPUTS:
**
**      authn_level     The authentication level to be applied over this
**                      security context.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      info            A pointer to the auth information rep structure
**                      containing RPC protocol indenpendent information.
**      st              The return status of this routine.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:
**
**      The newly create auth info will have a reference count of 1.
**
**--
**/

INTERNAL void rpc__noauth_cn_create_info
(
    rpc_authn_level_t                authn_level,
    rpc_auth_info_p_t                *auth_info,
    unsigned32                       *st
)
{
    rpc_noauth_info_p_t noauth_info;

    CODING_ERROR (st);
    RPC_DBG_PRINTF (rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_ROUTINE_TRACE,
                    ("(rpc__noauth_cn_create_info)\n"));

    RPC_DBG_PRINTF (rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_PKT,
                    ("(rpc__noauth_cn_create_info) prot->%x level->%x\n",
                    rpc_c_authn_dce_dummy,
                    authn_level));

#ifdef DEBUG
    if (RPC_DBG_EXACT(rpc_es_dbg_cn_errors,
                      RPC_C_CN_DBG_AUTH_CREATE_INFO))
    {
        *st = RPC_S_CN_DBG_AUTH_FAILURE;
        return;
    }
#endif

    /*
     * Allocate storage for a noauth info structure from heap.
     */
    RPC_MEM_ALLOC (noauth_info,
                   rpc_noauth_info_p_t,
                   sizeof (rpc_noauth_info_t),
                   RPC_C_MEM_NOAUTH_INFO,
                   RPC_C_MEM_WAITOK);

    /*
     * Initialize it.
     */
    memset (noauth_info, 0, sizeof(rpc_noauth_info_t));
    RPC_MUTEX_INIT (noauth_info->lock);

    /*
     * Initialize the common auth_info stuff.
     */
    noauth_info->auth_info.refcount = 1;
    noauth_info->auth_info.server_princ_name = '\0';
    noauth_info->auth_info.authn_level = authn_level;
    noauth_info->auth_info.authn_protocol = rpc_c_authn_dce_dummy;
    noauth_info->auth_info.authz_protocol = rpc_c_authz_name;
    noauth_info->auth_info.is_server = true;

    *auth_info = (rpc_auth_info_t *) noauth_info;
    *st = rpc_s_ok;
}


/*****************************************************************************/
/*
**++
**
**  ROUTINE NAME:       rpc__noauth_cn_cred_changed
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**
**      Determine whether the client's credentials stored in the
**      security context are different from those in the auth info.
**      If they are not the same return true, else false.
**
**  INPUTS:
**
**      sec             A pointer to security context element which includes
**                      the key ID, auth information rep and RPC auth
**                      information rep.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      st              The return status of this routine.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     boolean32
**
**      True if the credentials identified by the auth
**      information rep and RPC auth information rep are different,
**      false if not.
**
**      The md5 checksum algorithm requires the use of the session key
**      to encrypt the CRC(assoc_uuid).  Since the session key will
**      change when the credential changes, this routine sets the flag
**      indicating that a (potentially) valid encrypted crc is now
**      invalid, forcing a recomputation.
**
**  SIDE EFFECTS:       none
**
**--
**/

INTERNAL boolean32 rpc__noauth_cn_cred_changed
(
    rpc_cn_sec_context_p_t          sec,
    unsigned32                      *st
)
{
    rpc_noauth_cn_info_t        *noauth_cn_info;
    rpc_noauth_info_p_t         noauth_info;
    boolean32                   different_creds;

    CODING_ERROR (st);
    RPC_DBG_PRINTF (rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_ROUTINE_TRACE,
                    ("(rpc__noauth_cn_cred_changed)\n"));

    RPC_DBG_PRINTF (rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_PKT,
                    ("(rpc__noauth_cn_cred_changed) prot->%x level->%x key_id->%x\n",
                    rpc_c_authn_dce_private,
                    sec->sec_info->authn_level,
                    sec->sec_key_id));

#ifdef DEBUG
    if (RPC_DBG_EXACT(rpc_es_dbg_cn_errors,
                      RPC_C_CN_DBG_AUTH_CRED_CHANGED))
    {
        *st = RPC_S_CN_DBG_AUTH_FAILURE;
        return (false);
    }
#endif

    /*
     * Assume that cred is already valid.
     */
    different_creds = false;
    *st = rpc_s_ok;
    return (different_creds);
}


/*****************************************************************************/
/*
**++
**
**  ROUTINE NAME:       rpc__noauth_cn_cred_refresh
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**
**      Determine whether the client's credentials are still
**      valid. If not this routine will try to renew the credentials.
**      If they cannot be renewed an error is returned. This routine
**      is called from the client side.
**
**  INPUTS:
**
**      auth_info       A pointer to the auth information rep
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      st              The return status of this routine.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:       none
**
**--
**/

INTERNAL void rpc__noauth_cn_cred_refresh
(
    rpc_auth_info_p_t               auth_info,
    unsigned32                      *st
)
{
    rpc_noauth_cn_info_t        *noauth_cn_info;
    rpc_noauth_info_p_t         noauth_info;

    CODING_ERROR (st);
    RPC_DBG_PRINTF (rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_ROUTINE_TRACE,
                    ("(rpc__noauth_cn_cred_refresh)\n"));

    RPC_DBG_PRINTF (rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_PKT,
                    ("(rpc__noauth_cn_cred_refresh) prot->%x level->%x\n",
                    rpc_c_authn_dce_private,
                    auth_info->authn_level));

#ifdef DEBUG
    if (RPC_DBG_EXACT(rpc_es_dbg_cn_errors,
                      RPC_C_CN_DBG_AUTH_CRED_REFRESH))
    {
        *st = RPC_S_CN_DBG_AUTH_FAILURE;
        return;
    }
#endif

    /*
     * Assume that cred is already valid.
     */
    *st = rpc_s_ok;
}

/*****************************************************************************/
/*
**++
**
**  ROUTINE NAME:       rpc__noauth_cn_fmt_client_req
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**
**      This routine will format the auth_value field of
**      either an rpc_bind or rpc_alter_context PDU. This is
**      called from the client side association state machine.
**
**  INPUTS:
**
**      assoc_sec       A pointer to per-association security context
**                      including association UUID CRC and sequence numbers.
**      sec             A pointer to security context element which includes
**                      the key ID, auth information rep and RPC auth
**                      information rep.
**      auth_value      A pointer to the auth_value field in the rpc_bind or
**                      rpc_alter_context PDU authentication trailer.
**
**  INPUTS/OUTPUTS:
**
**      auth_value_len  On input, the lenght, in bytes of the available space
**                      for the auth_value field. On output, the lenght in
**                      bytes used in encoding the auth_value field. Zero if
**                      an error status is returned.
**
**  OUTPUTS:
**
**      st              The return status of this routine.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:       none
**
**--
**/

INTERNAL void rpc__noauth_cn_fmt_client_req
(
    rpc_cn_assoc_sec_context_p_t    assoc_sec,
    rpc_cn_sec_context_p_t          sec,
    dce_pointer_t                       auth_value,
    unsigned32                      *auth_value_len,
    unsigned32                      *st
)
{
    rpc_cn_bind_auth_value_priv_t       *priv_auth_value;

    CODING_ERROR (st);
    RPC_DBG_PRINTF (rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_ROUTINE_TRACE,
                    ("(rpc__noauth_cn_fmt_client_req)\n"));

    RPC_DBG_PRINTF (rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_PKT,
                    ("(rpc__noauth_cn_fmt_client_req) prot->%x level->%x key_id->%x assoc_uuid_crc->%x xmit_seq->%x recv_seq->%x\n",
                    rpc_c_authn_dce_dummy,
                    sec->sec_info->authn_level,
                    sec->sec_key_id,
                    assoc_sec->assoc_uuid_crc,
                    assoc_sec->assoc_next_snd_seq,
                    assoc_sec->assoc_next_rcv_seq));

#ifdef DEBUG
    if (RPC_DBG_EXACT(rpc_es_dbg_cn_errors,
                      RPC_C_CN_DBG_AUTH_FMT_CLIENT_REQ))
    {
        *st = RPC_S_CN_DBG_AUTH_FAILURE;
        return;
    }
#endif

    if (*auth_value_len < RPC_CN_PKT_SIZEOF_BIND_AUTH_VAL)
    {
        *st = rpc_s_credentials_too_large;
        return;
    }
    else
    {
        *auth_value_len = RPC_CN_PKT_SIZEOF_BIND_AUTH_VAL;
    }

    priv_auth_value = (rpc_cn_bind_auth_value_priv_t *)auth_value;
    priv_auth_value->assoc_uuid_crc = assoc_sec->assoc_uuid_crc;
    priv_auth_value->sub_type = RPC_C_CN_DCE_SUB_TYPE;
    priv_auth_value->checksum_length = 0;
    priv_auth_value->cred_length = 0;
    *st = rpc_s_ok;
}

/*****************************************************************************/
/*
**++
**
**  ROUTINE NAME:       rpc__noauth_cn_fmt_srvr_resp
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**
**      This routine will format the auth_value field of
**      either an rpc_bind_ack or rpc_alter_context_response
**      PDU. The authentication protocol encoding in the
**      credentials field of auth_value should correspond
**      to the status returned from rpc__auth_cn_vfy_
**      client_req()  routine. This credentials field, when
**      decoded by rpc__auth_cn_vfy_srvr_resp(),  should
**      result in the same error code being returned. If the
**      memory provided is not large enough an authentication
**      protocol specific error message will be encoded in
**      the credentials field indicating this error. This is
**      called from the server side association state machine.
**
**  INPUTS:
**
**      verify_st       The status code returned by rpc__auth_cn_verify_
**                      client_req().
**      assoc_sec       A pointer to per-association security context
**                      including association UUID CRC and sequence numbers.
**      sec             A pointer to security context element which includes
**                      the key ID, auth information rep and RPC auth
**                      information rep.
**      req_auth_value  A pointer to the auth_value field in the
**                      rpc_bind or rpc_alter_context PDU authentication trailer.
**      req_auth_value_len The length, in bytes, of the
**                      req_auth_value field.
**      auth_value      A pointer to the auth_value field in the rpc_bind or
**                      rpc_alter_context PDU authentication trailer.
**
**  INPUTS/OUTPUTS:
**
**      auth_value_len  On input, the length, in bytes of the available space
**                      for the auth_value field. On output, the length in
**                      bytes used in encoding the auth_value field. Zero if
**                      an error status is returned.
**
**  OUTPUTS:            none
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:       none
**
**--
**/

INTERNAL void rpc__noauth_cn_fmt_srvr_resp
(
    unsigned32                      verify_st,
    rpc_cn_assoc_sec_context_p_t    assoc_sec,
    rpc_cn_sec_context_p_t          sec,
    dce_pointer_t                       req_auth_value,
    unsigned32                      req_auth_value_len,
    dce_pointer_t                       auth_value,
    unsigned32                      *auth_value_len
)
{
    rpc_cn_bind_auth_value_priv_t       *priv_auth_value;

    RPC_DBG_PRINTF (rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_ROUTINE_TRACE,
                    ("(rpc__noauth_cn_fmt_srvr_resp)\n"));

    RPC_DBG_PRINTF (rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_PKT,
                    ("(rpc__noauth_cn_fmt_srvr_resp) prot->%x level->%x key_id->%x assoc_uuid_crc->%x xmit_seq->%x recv_seq->%x, vfy_client_st->%x\n",
                    rpc_c_authn_dce_dummy,
                    sec->sec_info->authn_level,
                    sec->sec_key_id,
                    assoc_sec->assoc_uuid_crc,
                    assoc_sec->assoc_next_snd_seq,
                    assoc_sec->assoc_next_rcv_seq,
                    verify_st));

#ifdef DEBUG
    if (RPC_DBG_EXACT(rpc_es_dbg_cn_errors,
                      RPC_C_CN_DBG_AUTH_FMT_SERVER_RESP))
    {
        verify_st = RPC_S_CN_DBG_AUTH_FAILURE;
    }
#endif

    assert (verify_st == rpc_s_ok);
    assert (*auth_value_len >= RPC_CN_PKT_SIZEOF_BIND_AUTH_VAL);
    *auth_value_len = RPC_CN_PKT_SIZEOF_BIND_AUTH_VAL;

    priv_auth_value = (rpc_cn_bind_auth_value_priv_t *)auth_value;
    priv_auth_value->assoc_uuid_crc = assoc_sec->assoc_uuid_crc;
    priv_auth_value->sub_type = RPC_C_CN_DCE_SUB_TYPE;
    priv_auth_value->checksum_length = 0;
    priv_auth_value->cred_length = 0;
}

/*****************************************************************************/
/*
**++
**
**  ROUTINE NAME:       rpc__noauth_cn_free_prot_info
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**
**      This routine will free an NCA Connection RPC auth
**      information rep.
**
**  INPUTS:
**
**      info            A pointer to the auth information rep structure
**                      containing RPC protocol indenpendent information.
**
**  INPUTS/OUTPUTS:
**
**      cn_info         A pointer to the RPC auth information rep structure
**                      containing NCA Connection specific
**                      information. NULL on output.
**
**  OUTPUTS:            none
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:       none
**
**--
**/

INTERNAL void rpc__noauth_cn_free_prot_info
(
    rpc_auth_info_p_t               info,
    rpc_cn_auth_info_p_t            *cn_info
)
{
    RPC_DBG_PRINTF (rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_ROUTINE_TRACE,
                    ("(rpc__noauth_cn_free_prot_info)\n"));

    RPC_DBG_PRINTF (rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_PKT,
                    ("(rpc__noauth_cn_free_prot_info) prot->%x level->%x \n",
                    rpc_c_authn_dce_dummy,
                    info->authn_level));

#ifdef DEBUG
    memset (*cn_info, 0, sizeof (rpc_cn_auth_info_t));
#endif

    RPC_MEM_FREE (*cn_info, RPC_C_MEM_NOAUTH_CN_INFO);
    *cn_info = NULL;
}

/*****************************************************************************/
/*
**++
**
**  ROUTINE NAME:       rpc__noauth_cn_get_prot_info
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**
**      This routine will create and return an NCA Connection
**      RPC auth information rep.
**
**  INPUTS:
**
**      info            A pointer to the auth information rep structure
**                      containing RPC protocol indenpendent information.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      cn_info         A pointer to the RPC auth information rep structure
**                      containing NCA Connection specific information.
**      st              The return status of this routine.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:       none
**
**--
**/

INTERNAL void rpc__noauth_cn_get_prot_info
(
    rpc_auth_info_p_t               info,
    rpc_cn_auth_info_p_t            *cn_info,
    unsigned32                      *st
)
{
    rpc_noauth_cn_info_t        *noauth_cn_info;

    CODING_ERROR (st);
    RPC_DBG_PRINTF (rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_ROUTINE_TRACE,
                    ("(rpc__noauth_cn_get_prot_info)\n"));

    RPC_DBG_PRINTF (rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_PKT,
                    ("(rpc__noauth_cn_get_prot_info) prot->%x level->%x \n",
                    rpc_c_authn_dce_dummy,
                    info->authn_level));

#ifdef DEBUG
    if (RPC_DBG_EXACT(rpc_es_dbg_cn_errors,
                      RPC_C_CN_DBG_AUTH_GET_PROT_INFO))
    {
        *st = RPC_S_CN_DBG_AUTH_FAILURE;
        return;
    }
#endif

    /*
     * Allocate storage for a noauth cn info structure from heap.
     */
    RPC_MEM_ALLOC (noauth_cn_info,
                   rpc_noauth_cn_info_p_t,
                   sizeof (rpc_noauth_cn_info_t),
                   RPC_C_MEM_NOAUTH_CN_INFO,
                   RPC_C_MEM_WAITOK);

    /*
     * Initialize it.
     */
    memset (noauth_cn_info, 0, sizeof(rpc_noauth_cn_info_t));

    *cn_info = (rpc_cn_auth_info_t *)noauth_cn_info;
    *st = rpc_s_ok;
}

/*****************************************************************************/
/*
**++
**
**  ROUTINE NAME:       rpc__noauth_cn_pre_call
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**
**      This routine will format the auth_value field of
**      a call level PDU, namely an rpc_request, rpc_response
**      rpc_fault, rpc_remote_alert or rpc_orphaned PDU. It will
**      also format the auth_value field of the association level
**      rpc_shutdown PDU. This does
**      not include generating any checksums in the auth_value_field
**      or encrypting of data corresponding to the authentication
**      level. That will be done in the rpc__cn_auth_pre_send
**      routine. This is called on both client and server when a the
**      data structures and header template for a call is being set up.
**
**  INPUTS:
**
**      assoc_sec       A pointer to per-association security context
**                      including association UUID CRC and sequence numbers.
**      sec             A pointer to security context element which includes
**                      the key ID, auth information rep and RPC auth
**                      information rep.
**      auth_value      A pointer to the auth_value field in the rpc_bind or
**                      rpc_alter_context PDU authentication trailer.
**
**  INPUTS/OUTPUTS:
**
**      auth_value_len  On input, the lenght, in bytes of the available space
**                      for the auth_value field. On output, the lenght in
**                      bytes used in encoding the auth_value field. Zero if
**                      an error status is returned.
**
**  OUTPUTS:
**
**      st              The return status of this routine.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:       none
**
**--
**/

INTERNAL void rpc__noauth_cn_pre_call
(
    rpc_cn_assoc_sec_context_p_t    assoc_sec,
    rpc_cn_sec_context_p_t          sec,
    dce_pointer_t                       auth_value,
    unsigned32                      *auth_value_len,
    unsigned32                      *st
)
{
    rpc_cn_auth_value_priv_t    *priv_auth_value;

    CODING_ERROR(st);

    RPC_DBG_PRINTF (rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_ROUTINE_TRACE,
                    ("(rpc__noauth_cn_pre_call)\n"));

    RPC_DBG_PRINTF (rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_PKT,
                    ("(rpc__noauth_cn_pre_call) prot->%x level->%x key_id->%x assoc_uuid_crc->%x xmit_seq->%x recv_seq->%x\n",
                    rpc_c_authn_dce_dummy,
                    sec->sec_info->authn_level,
                    sec->sec_key_id,
                    assoc_sec->assoc_uuid_crc,
                    assoc_sec->assoc_next_snd_seq,
                    assoc_sec->assoc_next_rcv_seq));

#ifdef DEBUG
    if (RPC_DBG_EXACT(rpc_es_dbg_cn_errors,
                      RPC_C_CN_DBG_AUTH_PRE_CALL))
    {
        *st = RPC_S_CN_DBG_AUTH_FAILURE;
        return;
    }
#endif

    if (sec->sec_info->authn_level >= rpc_c_authn_level_pkt)
    {
        if (*auth_value_len < RPC_CN_PKT_SIZEOF_AUTH_VAL_CKSM)
        {
            *st = rpc_s_credentials_too_large;
            return;
        }
        else
        {
            *auth_value_len = RPC_CN_PKT_SIZEOF_AUTH_VAL_CKSM;
        }
    }
    else
    {
        if (*auth_value_len < RPC_CN_PKT_SIZEOF_AUTH_VAL)
        {
            *st = rpc_s_credentials_too_large;
            return;
        }
        else
        {
            *auth_value_len = RPC_CN_PKT_SIZEOF_AUTH_VAL;
        }
    }

    priv_auth_value = (rpc_cn_auth_value_priv_t *)auth_value;
    priv_auth_value->sub_type = RPC_C_CN_DCE_SUB_TYPE;
    priv_auth_value->checksum_length = 0;
    *st = rpc_s_ok;
}

/*****************************************************************************/
/*
**++
**
**  ROUTINE NAME:       rpc__noauth_cn_pre_send
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**
**      This routine will perform per-packet security
**      processing on a packet before it is sent. This
**      includes checksumming and encryption.
**
**      Note that in some cases, the data is copied to
**      a contiguous buffer for checksumming and
**      encryption.  In these cases, the contiguous
**      iov element should be used instead of the original
**      iovector.
**
**  INPUTS:
**
**      assoc_sec       A pointer to per-association security context
**                      including association UUID CRC and sequence numbers.
**      sec             A pointer to security context element which includes
**                      the key ID, auth information rep and RPC auth
**                      information rep.
**      iov             A pointer to the iovector containing the PDU
**                      about to be sent. The appropriate per-packet security
**                      services will be applied to it.
**      iovlen          The length, in bytes, of the PDU.
**      out_iov         An iovector element.  This iovector element
**                      will describe packet if the original iov
**                      had to be copied.  If the original iov was
**                      copied, out_iov->base will be non-NULL.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      st              The return status of this routine.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:       none
**
**--
**/

INTERNAL void rpc__noauth_cn_pre_send
(
    rpc_cn_assoc_sec_context_p_t    assoc_sec,
    rpc_cn_sec_context_p_t          sec,
    rpc_socket_iovec_p_t            iov,
    unsigned32                      iovlen,
    rpc_socket_iovec_p_t            out_iov,
    unsigned32                      *st
)
{
    unsigned32          ptype;

    CODING_ERROR (st);
    RPC_DBG_PRINTF (rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_ROUTINE_TRACE,
                    ("(rpc__noauth_cn_pre_send)\n"));

    ptype = ((rpc_cn_common_hdr_t *)(iov[0].base))->ptype;
    RPC_DBG_PRINTF (rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_GENERAL,
                    ("(rpc__noauth_cn_pre_send) authn level->%x packet type->%x\n", sec->sec_info->authn_level, ptype));

    RPC_DBG_PRINTF (rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_PKT,
                    ("(rpc__noauth_cn_pre_send) prot->%x level->%x key_id->%x assoc_uuid_crc->%x xmit_seq->%x recv_seq->%x ptype->%x\n",
                    rpc_c_authn_dce_dummy,
                    sec->sec_info->authn_level,
                    sec->sec_key_id,
                    assoc_sec->assoc_uuid_crc,
                    assoc_sec->assoc_next_snd_seq,
                    assoc_sec->assoc_next_rcv_seq,
                    ptype));

#ifdef DEBUG
    if (RPC_DBG_EXACT(rpc_es_dbg_cn_errors,
                      RPC_C_CN_DBG_AUTH_PRE_SEND))
    {
        *st = RPC_S_CN_DBG_AUTH_FAILURE;
        return;
    }
#endif

    out_iov->base = NULL;
    switch (ptype)
    {
        case RPC_C_CN_PKT_REQUEST:
        case RPC_C_CN_PKT_RESPONSE:
        case RPC_C_CN_PKT_FAULT:
        {
            break;
        }

        case RPC_C_CN_PKT_BIND:
        case RPC_C_CN_PKT_BIND_ACK:
        case RPC_C_CN_PKT_BIND_NAK:
        case RPC_C_CN_PKT_ALTER_CONTEXT:
        case RPC_C_CN_PKT_ALTER_CONTEXT_RESP:
        case RPC_C_CN_PKT_AUTH3:
        case RPC_C_CN_PKT_SHUTDOWN:
        case RPC_C_CN_PKT_REMOTE_ALERT:
        case RPC_C_CN_PKT_ORPHANED:
        default:
        {
            break;
        }
    }

    *st = rpc_s_ok;
}

/*****************************************************************************/
/*
**++
**
**  ROUTINE NAME:       rpc__noauth_cn_recv_check
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**
**      This routine will perform per-packet security
**      processing on a packet after it is received. This
**      includes decryption and verification of checksums.
**
**  INPUTS:
**
**      assoc_sec       A pointer to per-association security context
**                      including association UUID CRC and sequence numbers.
**      sec             A pointer to security context element which includes
**                      the key ID, auth information rep and RPC auth
**                      information rep.
**      pdu             A pointer to the PDU about to be sent. The appropriate
**                      per-packet security services will be applied to it.
**      pdu_len         The length, in bytes, of the PDU.
**      cred_len        The length, in bytes, of the credentials.
**      auth_tlr        A pointer to the auth trailer.
**      unpack_ints     A boolean indicating whether the integer rep
**                      of fields in the pdu need to be adjusted for
**                      endian differences.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      st              The return status of this routine.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:       none
**
**--
**/

INTERNAL void rpc__noauth_cn_recv_check
(
    rpc_cn_assoc_sec_context_p_t    assoc_sec,
    rpc_cn_sec_context_p_t          sec,
    rpc_cn_common_hdr_p_t           pdu,
    unsigned32                      pdu_len,
    unsigned32                      cred_len,
    rpc_cn_auth_tlr_p_t             auth_tlr,
    boolean32                       unpack_ints,
    unsigned32                      *st
)
{
    rpc_cn_auth_value_priv_t    *priv_auth_value;
    unsigned32                  ptype;
    unsigned32                  authn_level;
    unsigned32                  assoc_uuid_crc;

    CODING_ERROR (st);
    RPC_DBG_PRINTF (rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_ROUTINE_TRACE,
                    ("(rpc__noauth_cn_recv_check)\n"));

    ptype = pdu->ptype;
    priv_auth_value = (rpc_cn_auth_value_priv_t *)(auth_tlr->auth_value);
    authn_level = auth_tlr->auth_level;
    if (ptype == RPC_C_CN_PKT_BIND)
    {
        assoc_uuid_crc = ((rpc_cn_bind_auth_value_priv_t *)priv_auth_value)->assoc_uuid_crc;
    }
    else
    {
        assoc_uuid_crc = assoc_sec->assoc_uuid_crc;
    }

    RPC_DBG_PRINTF (rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_GENERAL,
                    ("(rpc__noauth_cn_recv_check) authn level->%x packet type->%x\n", authn_level, ptype));

    RPC_DBG_PRINTF (rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_PKT,
                    ("(rpc__noauth_cn_recv_check) prot->%x level->%x key_id->%x assoc_uuid_crc->%x xmit_seq->%x recv_seq->%x ptype->%x\n",
                    rpc_c_authn_dce_dummy,
                    authn_level,
                    sec->sec_key_id,
                    assoc_uuid_crc,
                    assoc_sec->assoc_next_snd_seq,
                    assoc_sec->assoc_next_rcv_seq,
                    ptype));

#ifdef DEBUG
    if (RPC_DBG_EXACT(rpc_es_dbg_cn_errors,
                      RPC_C_CN_DBG_AUTH_RECV_CHECK))
    {
        *st = RPC_S_CN_DBG_AUTH_FAILURE;
        return;
    }
#endif

    switch (ptype)
    {
        case RPC_C_CN_PKT_REQUEST:
        case RPC_C_CN_PKT_RESPONSE:
        case RPC_C_CN_PKT_FAULT:
        case RPC_C_CN_PKT_BIND:
        case RPC_C_CN_PKT_BIND_ACK:
        case RPC_C_CN_PKT_BIND_NAK:
        case RPC_C_CN_PKT_ALTER_CONTEXT:
        case RPC_C_CN_PKT_ALTER_CONTEXT_RESP:
        case RPC_C_CN_PKT_AUTH3:
        case RPC_C_CN_PKT_SHUTDOWN:
        case RPC_C_CN_PKT_REMOTE_ALERT:
        case RPC_C_CN_PKT_ORPHANED:
        default:
        {
            break;
        }
    }

    *st = rpc_s_ok;
}

/*****************************************************************************/
/*
**++
**
**  ROUTINE NAME:       rpc__noauth_cn_tlr_uuid_crc
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**
**      This routine will locate and return the association
**      UUID CRC contained in the auth_value field of an
**      authentication trailer of an rpc_bind, rpc_bind_ack,
**      rpc_alter_context or rpc_alter_context_response PDU.
**
**  INPUTS:
**
**      auth_value      A pointer to the auth_value field in an authentication
**                      trailer.
**      auth_value_len  The length, in bytes, of the auth_value field.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      assoc_uuid_crc  The association UUID CRC contained in the auth_value
**                      field.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:       none
**
**--
**/

INTERNAL void rpc__noauth_cn_tlr_uuid_crc
(
    dce_pointer_t               auth_value,
    unsigned32              auth_value_len,
    unsigned32              *uuid_crc
)
{
    rpc_cn_bind_auth_value_priv_t       *priv_auth_value;

    RPC_DBG_PRINTF (rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_ROUTINE_TRACE,
                    ("(rpc__noauth_cn_tlr_uuid_crc)\n"));

    assert (auth_value_len >= RPC_CN_PKT_SIZEOF_BIND_AUTH_VAL);
    priv_auth_value = (rpc_cn_bind_auth_value_priv_t *)auth_value;
    *uuid_crc = priv_auth_value->assoc_uuid_crc;

    RPC_DBG_PRINTF (rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_GENERAL,
                    ("(rpc__noauth_cn_tlr_uuid_crc) assoc_uuid_crc->%x\n", *uuid_crc));
}

/*****************************************************************************/
/*
**++
**
**  ROUTINE NAME:       rpc__noauth_cn_tlr_unpack
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**
**      This routine will byte swap all the appropriate fields
**      of the the auth_value field of an authentication
**      trailer. It will also convert any characters from
**      the remote representation into the local, for example,
**      ASCII to EBCDIC.
**
**  INPUTS:
**
**      auth_value_len  The length, in bytes, of the auth_value field.
**      packed_drep     The packed Networ Data Representation, (see NCA RPC
**                      RunTime Extensions Specification Version OSF TX1.0.9
**                      pre 1003 for details), of the remote machine.
**
**  INPUTS/OUTPUTS:
**
**      pkt_p           A pointer to the entire packet.
**
**  OUTPUTS:            none
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:       none
**
**--
**/

INTERNAL void rpc__noauth_cn_tlr_unpack
(
    rpc_cn_packet_p_t       pkt_p,
    unsigned32              auth_value_len,
    unsigned8               *packed_drep
)
{
    RPC_DBG_PRINTF (rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_ROUTINE_TRACE,
                    ("(rpc__noauth_cn_tlr_unpack)\n"));
}

/*****************************************************************************/
/*
**++
**
**  ROUTINE NAME:       rpc__noauth_cn_vfy_client_req
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**
**      This routine will decode the auth_value field of
**      either an rpc_bind or rpc_alter_context PDU. Any
**      error encountered while authenticating the client
**      will result in an error status return. The contents
**      of the credentials field includes the authorization
**      data. This is called from the server side association
**      state machine. Note that upon successful return
**      the auth information rep will contain the client's
**      authorization protocol and data.
**
**  INPUTS:
**
**      assoc_sec       A pointer to per-association security context
**                      including association UUID CRC and sequence numbers.
**      sec             A pointer to security context element which includes
**                      the key ID, auth information rep and RPC auth
**                      information rep.
**      auth_value      A pointer to the auth_value field in the rpc_bind or
**                      rpc_alter_context PDU authentication trailer.
**      auth_value_len  The length, in bytes, of auth_value.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      st              The return status of this routine.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:       none
**
**--
**/

INTERNAL void rpc__noauth_cn_vfy_client_req
(
    rpc_cn_assoc_sec_context_p_t    assoc_sec,
    rpc_cn_sec_context_p_t          sec,
    dce_pointer_t                       auth_value,
    unsigned32                      auth_value_len,
    unsigned32                      *st
)
{
    rpc_cn_bind_auth_value_priv_t       *priv_auth_value;

    CODING_ERROR (st);
    RPC_DBG_PRINTF (rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_ROUTINE_TRACE,
                    ("(rpc__noauth_cn_vfy_client_req)\n"));

    priv_auth_value = (rpc_cn_bind_auth_value_priv_t *)auth_value;

    RPC_DBG_PRINTF (rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_PKT,
                    ("(rpc__noauth_cn_vfy_client_req) prot->%x level->%x key_id->%x assoc_uuid_crc->%x xmit_seq->%x recv_seq->%x\n",
                    rpc_c_authn_dce_dummy,
                    sec->sec_info->authn_level,
                    sec->sec_key_id,
                    assoc_sec->assoc_uuid_crc,
                    assoc_sec->assoc_next_snd_seq,
                    assoc_sec->assoc_next_rcv_seq));

#ifdef DEBUG
    if (RPC_DBG_EXACT(rpc_es_dbg_cn_errors,
                      RPC_C_CN_DBG_AUTH_VFY_CLIENT_REQ))
    {
        *st = RPC_S_CN_DBG_AUTH_FAILURE;
        return;
    }
#endif

    *st = rpc_s_ok;
}

/*****************************************************************************/
/*
**++
**
**  ROUTINE NAME:       rpc__noauth_cn_vfy_srvr_resp
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**
**      This routine will decode auth_value field either an
**      rpc_bind_ack or rpc_alter_context_response PDU. If the
**      credentials field of the auth_value field contains an
**      authentication protocol specific encoding of an error
**      this will be returned as an error status code. This is
**      called from the client side association state machine.
**      Note that upon successful return the auth information
**      rep will contain the client's authorization protocol
**      and data.
**
**  INPUTS:
**
**      assoc_sec       A pointer to per-association security context
**                      including association UUID CRC and sequence numbers.
**      sec             A pointer to security context element which includes
**                      the key ID, auth information rep and RPC auth
**                      information rep.
**      auth_value      A pointer to the auth_value field in the rpc_bind or
**                      rpc_alter_context PDU authentication trailer.
**      auth_value_len  The length, in bytes, of auth_value.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      st              The return status of this routine.
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:       none
**
**--
**/

INTERNAL void rpc__noauth_cn_vfy_srvr_resp
(
    rpc_cn_assoc_sec_context_p_t    assoc_sec,
    rpc_cn_sec_context_p_t          sec,
    dce_pointer_t                       auth_value,
    unsigned32                      auth_value_len,
    unsigned32                      *st
)
{
    CODING_ERROR (st);
    RPC_DBG_PRINTF (rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_ROUTINE_TRACE,
                    ("(rpc__noauth_cn_vfy_srvr_resp)\n"));

    RPC_DBG_PRINTF (rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_PKT,
                    ("(rpc__noauth_cn_vfy_server_resp) prot->%x level->%x key_id->%x assoc_uuid_crc->%x xmit_seq->%x recv_seq->%x\n",
                    rpc_c_authn_dce_dummy,
                    sec->sec_info->authn_level,
                    sec->sec_key_id,
                    assoc_sec->assoc_uuid_crc,
                    assoc_sec->assoc_next_snd_seq,
                    assoc_sec->assoc_next_rcv_seq));

#ifdef DEBUG
    if (RPC_DBG_EXACT(rpc_es_dbg_cn_errors,
                      RPC_C_CN_DBG_AUTH_VFY_SERVER_RESP))
    {
        *st = RPC_S_CN_DBG_AUTH_FAILURE;
        return;
    }
#endif

    *st = rpc_s_ok;
}

PRIVATE rpc_protocol_id_t       rpc__noauth_cn_init
(
    rpc_auth_rpc_prot_epv_p_t       *epv,
    unsigned32                      *st
)
{
    CODING_ERROR (st);
    RPC_DBG_PRINTF (rpc_e_dbg_auth, RPC_C_CN_DBG_AUTH_ROUTINE_TRACE,
                    ("(rpc__noauth_cn_init)\n"));

    *epv = (rpc_auth_rpc_prot_epv_p_t) (&rpc_g_noauth_cn_epv);
    *st = rpc_s_ok;
    return (RPC_C_PROTOCOL_ID_NCACN);
}
