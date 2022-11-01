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

/* Small hack for FreeBSD */
#if defined(__FreeBSD__) && defined(_POSIX_C_SOURCE)
#    undef _POSIX_C_SOURCE
#endif

#include <commonp.h>
#include <stdio.h>
#include <dce/rpc.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>

#ifndef BUILD_RPC_NS_LDAP

#include <ctype.h>

void rpc_ns_binding_export(
	/* [in] */ unsigned32 entry_name_syntax ATTRIBUTE_UNUSED,
	/* [in] */ unsigned_char_p_t entry_name ATTRIBUTE_UNUSED,
	/* [in] */ rpc_if_handle_t if_spec ATTRIBUTE_UNUSED,
	/* [in] */ rpc_binding_vector_p_t binding_vector
	ATTRIBUTE_UNUSED,
	/* [in] */ uuid_vector_p_t object_uuid_vector ATTRIBUTE_UNUSED,
	/* [out] */ unsigned32 *status
)
{
	RPC_DBG_PRINTF(rpc_e_dbg_general, 1, ("rpc_ns_binding_export\n"));
	*status = rpc_s_name_service_unavailable;
}

void
rpc_ns_binding_import_begin(
    /* [in] */ unsigned32 entry_name_syntax ATTRIBUTE_UNUSED,
    /* [in] */ unsigned_char_p_t entry_name ATTRIBUTE_UNUSED,
    /* [in] */ rpc_if_handle_t if_spec ATTRIBUTE_UNUSED,
    /* [in] */ uuid_p_t object_uuid ATTRIBUTE_UNUSED,
    /* [out] */ rpc_ns_handle_t *import_context ATTRIBUTE_UNUSED,
    /* [out] */ unsigned32 *status
)
{
	RPC_DBG_PRINTF(rpc_e_dbg_general, 1, ("rpc_ns_import_begin\n"));
	*status = rpc_s_name_service_unavailable;
}

void
rpc_ns_binding_import_done(
    /* [in, out] */ rpc_ns_handle_t *import_context
	 ATTRIBUTE_UNUSED,
    /* [out] */ unsigned32 *status
)
{
	RPC_DBG_PRINTF(rpc_e_dbg_general, 1, ("rpc_ns_binding_import_done\n"));
	*status = rpc_s_name_service_unavailable;
}

void
rpc_ns_mgmt_handle_set_exp_age(
    /* [in] */ rpc_ns_handle_t ns_handle ATTRIBUTE_UNUSED,
    /* [in] */ unsigned32 expiration_age ATTRIBUTE_UNUSED,
    /* [out] */ unsigned32 *status

)
{
	RPC_DBG_PRINTF(rpc_e_dbg_general, 1,
		("rpc_ns_mgmt_handle_set_exp_age\n"));
	*status = rpc_s_ok;
}

void
rpc_ns_binding_import_next(
    /* [in] */ rpc_ns_handle_t import_context ATTRIBUTE_UNUSED,
    /* [out] */ rpc_binding_handle_t *binding ATTRIBUTE_UNUSED,
    /* [out] */ unsigned32 *status
	       )
{
	RPC_DBG_PRINTF(rpc_e_dbg_general, 1, ("rpc_ns_binding_import_next\n"));
	*status = rpc_s_name_service_unavailable ;
}

#endif /* BUILD_RPC_NS_LDAP */
