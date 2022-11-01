/*
 * Copyright (c) 2006 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
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

/**
 * Release a gss_OID
 *
 * This function should never be used, this is since many of the
 * gss_OID objects passed around are stack and data objected that are
 * not free-able.
 *
 * The function tries to find internal OIDs that are static and avoid
 * trying to free them.
 *
 * One could guess that gss_name_to_oid() might return an allocated
 * OID.  In this implementation it wont, so there is no need to call
 * gss_release_oid().
 *
 * @param minor_status minor status code returned
 * @param oid oid to be released/freed.
 *
 * @returns GSS major status code
 *
 * @ingroup gssapi
 */

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gss_release_oid(OM_uint32 *__nonnull minor_status,
		__nullable gss_OID * __nonnull oid)
{
    struct _gss_mech_switch *m;
    gss_OID o = *oid;
    size_t n;

    *oid = GSS_C_NO_OID;

    if (minor_status != NULL)
	*minor_status = 0;

    if (o == GSS_C_NO_OID)
	return GSS_S_COMPLETE;

    /*
     * Program broken and tries to release an static oid, don't let
     * it crash us, search static tables forst.
     */
    for (n = 0; _gss_ont_mech[n].oid; n++)
	if (_gss_ont_mech[n].oid == o)
	    return GSS_S_COMPLETE;
    for (n = 0; _gss_ont_ma[n].oid; n++)
	if (_gss_ont_ma[n].oid == o)
	    return GSS_S_COMPLETE;

    HEIM_SLIST_FOREACH(m, &_gss_mechs, gm_link) {
	if (&m->gm_mech.gm_mech_oid == o)
	    return GSS_S_COMPLETE;
    }

    /* ok, the program doesn't try to crash us, lets release it for real now */

    if (o->elements != NULL) {
	free(o->elements);
	o->elements = NULL;
    }
    o->length = 0;
    free(o);

    return GSS_S_COMPLETE;
}
