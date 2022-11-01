/*
 * Copyright (c) 2010 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "mech_locl.h"

OM_uint32
gss_cred_hold(OM_uint32 * __nonnull min_stat, __nonnull gss_cred_id_t cred_handle)
{
    struct _gss_cred *cred = (struct _gss_cred *)cred_handle;
    struct _gss_mechanism_cred *mc;

    *min_stat = 0;

    if (cred == NULL)
	return GSS_S_NO_CRED;

    HEIM_SLIST_FOREACH(mc, &cred->gc_mc, gmc_link) {

	if (mc->gmc_mech->gm_cred_hold == NULL)
	    continue;

	(void)mc->gmc_mech->gm_cred_hold(min_stat, mc->gmc_cred);
    }

    return GSS_S_COMPLETE;
}


OM_uint32
gss_cred_unhold(OM_uint32 * __nonnull min_stat, __nonnull gss_cred_id_t cred_handle)
{
    struct _gss_cred *cred = (struct _gss_cred *)cred_handle;
    struct _gss_mechanism_cred *mc;

    *min_stat = 0;

    if (cred == NULL)
	return GSS_S_NO_CRED;

    HEIM_SLIST_FOREACH(mc, &cred->gc_mc, gmc_link) {

	if (mc->gmc_mech->gm_cred_unhold == NULL)
	    continue;

	(void)mc->gmc_mech->gm_cred_unhold(min_stat, mc->gmc_cred);
    }

    return GSS_S_COMPLETE;
}
