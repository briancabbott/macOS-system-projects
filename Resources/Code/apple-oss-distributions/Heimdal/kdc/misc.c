/*
 * Copyright (c) 1997 - 2001 Kungliga Tekniska Högskolan
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

#include "kdc_locl.h"

struct timeval _kdc_now;

krb5_error_code
_kdc_db_fetch(krb5_context context,
	      krb5_kdc_configuration *config,
	      krb5_const_principal principal,
	      unsigned flags,
	      krb5int32 *kvno_ptr,
	      HDB **db,
	      hdb_entry_ex **h)
{
    hdb_entry_ex *ent;
    krb5_error_code ret = HDB_ERR_NOENTRY;
    unsigned i;
    unsigned kvno = 0;
    krb5_principal enterprise_principal = NULL;
    krb5_const_principal princ = principal;

    *h = NULL;

    if (kvno_ptr) {
	    kvno = *kvno_ptr;
	    flags |= HDB_F_KVNO_SPECIFIED;
    }

    ent = calloc(1, sizeof (*ent));
    if (ent == NULL)
        return krb5_enomem(context);

    if (principal->name.name_type == KRB5_NT_ENTERPRISE_PRINCIPAL) {
        if (principal->name.name_string.len != 1) {
            ret = KRB5_PARSE_MALFORMED;
            krb5_set_error_message(context, ret,
                                   "malformed request: "
                                   "enterprise name with %d name components",
                                   principal->name.name_string.len);
            goto out;
        }
        ret = krb5_parse_name(context, principal->name.name_string.val[0],
                              &enterprise_principal);
        if (ret)
            goto out;
    }

    for (i = 0; i < config->num_db; i++) {
	ret = config->db[i]->hdb_open(context, config->db[i], O_RDONLY, 0);
	if (ret) {
	    const char *msg = krb5_get_error_message(context, ret);
	    kdc_log(context, config, 0, "Failed to open database: %s", msg);
	    krb5_free_error_message(context, msg);
	    continue;
	}

        if (config->db[i]->hdb_capability_flags & HDB_CAP_F_HANDLE_ENTERPRISE_PRINCIPAL)
            princ = principal;
        else if (enterprise_principal)
            princ = enterprise_principal;

	ret = config->db[i]->hdb_fetch_kvno(context,
					    config->db[i],
					    princ,
					    flags | HDB_F_DECRYPT,
					    kvno,
					    ent);
	config->db[i]->hdb_close(context, config->db[i]);

	if (ret == 0) {
	    if (db)
		*db = config->db[i];
	    *h = ent;
            ent = NULL;
            goto out;
	}
    }

    ret = HDB_ERR_NOENTRY;
    krb5_set_error_message(context, ret, "no such entry found in hdb");

out:
    krb5_free_principal(context, enterprise_principal);
    free(ent);
    return ret;
}

void
_kdc_free_ent(krb5_context context, hdb_entry_ex *ent)
{
    hdb_free_entry (context, ent);
    free (ent);
}

/*
 * Use the order list of preferred encryption types and sort the
 * available keys and return the most preferred key.
 */

krb5_error_code
_kdc_get_preferred_key(krb5_context context,
		       krb5_kdc_configuration *config,
		       hdb_entry_ex *h,
		       const char *name,
		       krb5_enctype *enctype,
		       Key **key)
{
    krb5_error_code ret;
    unsigned i;

    if (config->use_strongest_server_key) {
	const krb5_enctype *p = krb5_kerberos_enctypes(context);

	for (i = 0; p[i] != (krb5_enctype)ETYPE_NULL; i++) {
	    if (krb5_enctype_valid(context, p[i]) != 0)
		continue;
	    ret = hdb_enctype2key(context, &h->entry, p[i], key);
	    if (ret != 0)
		continue;
	    if (enctype != NULL)
		*enctype = p[i];
	    return 0;
	}
    } else {
	*key = NULL;

	for (i = 0; i < h->entry.keys.len; i++) {
	    if (krb5_enctype_valid(context, h->entry.keys.val[i].key.keytype)
		!= 0)
		continue;
	    ret = hdb_enctype2key(context, &h->entry,
		h->entry.keys.val[i].key.keytype, key);
	    if (ret != 0)
		continue;
	    if (enctype != NULL)
		*enctype = (*key)->key.keytype;
	    return 0;
	}
    }

    krb5_set_error_message(context, EINVAL,
			   "No valid kerberos key found for %s", name);
    return EINVAL; /* XXX */
}

/* 
 * If we have entry->etypes, use that, otherwise require there is
 * symmetric keys for the enctype needed.
 */

static krb5_error_code
entry_check_enctype(krb5_context context,
		    const hdb_entry_ex *entry,
		    krb5_enctype etype)
{
    krb5_error_code ret;

    ret = krb5_enctype_valid(context, etype);
    if (ret)
	return ret;

    if (entry->entry.etypes) {
	size_t n;

	for (n = 0; n < entry->entry.etypes->len; n++)
	    if (entry->entry.etypes->val[n] == (unsigned int)etype)
		return 0;
    } else {
	Key *key;

	ret = hdb_enctype2key(context, &entry->entry, etype, &key);
	if (ret == 0)
	    return 0;
    }
    return KRB5KDC_ERR_ETYPE_NOSUPP;
}

krb5_error_code
_kdc_get_preferred_enctype(krb5_context context,
			   krb5_kdc_configuration *config,
			   const hdb_entry_ex *entry,
			   const char *name,
			   krb5_enctype *etypes,
			   unsigned num_etypes,
			   krb5_enctype *etype)
{
    unsigned n, m;

    if (config->use_strongest_server_key) {
	const krb5_enctype *p = krb5_kerberos_enctypes(context);

	for (n = 0; p[n] != (krb5_enctype)ETYPE_NULL; n++) {

	    if (entry_check_enctype(context, entry, p[n]) != 0)
		continue;

	    for (m = 0; m < num_etypes; m++) {
		if (etypes[m] == p[n]) {
		    *etype = p[n];
		    return 0;
		}
	    }
	}
    } else {
	for (n = 0; n < num_etypes; n++) {

	    if (entry_check_enctype(context, entry, etypes[n]) != 0)
		continue;

	    *etype = etypes[n];
	    return 0;
	}
    }

    krb5_set_error_message(context, KRB5KDC_ERR_ETYPE_NOSUPP,
			   "No valid enctype found for %s", name);
    return KRB5KDC_ERR_ETYPE_NOSUPP;
}


/*
 * verify the flags on `client' and `server', returning 0
 * if they are OK and generating an error messages and returning
 * and error code otherwise.
 */

krb5_error_code
kdc_check_flags(krb5_context context,
		krb5_kdc_configuration *config,
		hdb_entry_ex *client_ex, const char *client_name,
		hdb_entry_ex *server_ex, const char *server_name,
		krb5_boolean is_as_req)
{
    if(client_ex != NULL) {
	hdb_entry *client = &client_ex->entry;

	/* check client */
	if (client->flags.locked_out) {
	    kdc_log(context, config, 0,
		    "Client (%s) is locked out", client_name);
	    return KRB5KDC_ERR_POLICY;
	}

	if (client->flags.invalid) {
	    kdc_log(context, config, 0,
		    "Client (%s) has invalid bit set", client_name);
	    return KRB5KDC_ERR_POLICY;
	}

	if(!client->flags.client){
	    kdc_log(context, config, 0,
		    "Principal may not act as client -- %s", client_name);
	    return KRB5KDC_ERR_POLICY;
	}

	if (client->valid_start && *client->valid_start > kdc_time) {
	    char starttime_str[100];
	    krb5_format_time(context, *client->valid_start,
			     starttime_str, sizeof(starttime_str), TRUE);
	    kdc_log(context, config, 0,
		    "Client not yet valid until %s -- %s",
		    starttime_str, client_name);
	    return KRB5KDC_ERR_CLIENT_NOTYET;
	}

	if (client->valid_end && *client->valid_end < kdc_time) {
	    char endtime_str[100];
	    krb5_format_time(context, *client->valid_end,
			     endtime_str, sizeof(endtime_str), TRUE);
	    kdc_log(context, config, 0,
		    "Client expired at %s -- %s",
		    endtime_str, client_name);
	    return KRB5KDC_ERR_NAME_EXP;
	}

	if (client->flags.require_pwchange &&
	    (server_ex == NULL || !server_ex->entry.flags.change_pw)) {
	    kdc_log(context, config, 0,
		    "Client's key must be changed -- %s", client_name);
	    return KRB5KDC_ERR_KEY_EXPIRED;
	}

	if (client->pw_end && *client->pw_end < kdc_time
	    && (server_ex == NULL || !server_ex->entry.flags.change_pw)) {
	    char pwend_str[100];
	    krb5_format_time(context, *client->pw_end,
			     pwend_str, sizeof(pwend_str), TRUE);
	    kdc_log(context, config, 0,
		    "Client's key has expired at %s -- %s",
		    pwend_str, client_name);
	    return KRB5KDC_ERR_KEY_EXPIRED;
	}
    }

    /* check server */

    if (server_ex != NULL) {
	hdb_entry *server = &server_ex->entry;

	if (server->flags.locked_out) {
	    kdc_log(context, config, 0,
		    "Client server locked out -- %s", server_name);
	    return KRB5KDC_ERR_POLICY;
	}
	if (server->flags.invalid) {
	    kdc_log(context, config, 0,
		    "Server has invalid flag set -- %s", server_name);
	    return KRB5KDC_ERR_POLICY;
	}

	if(!server->flags.server){
	    kdc_log(context, config, 0,
		    "Principal may not act as server -- %s", server_name);
	    return KRB5KDC_ERR_POLICY;
	}

	if(!is_as_req && server->flags.initial) {
	    kdc_log(context, config, 0,
		    "AS-REQ is required for server -- %s", server_name);
	    return KRB5KDC_ERR_POLICY;
	}

	if (server->valid_start && *server->valid_start > kdc_time) {
	    char starttime_str[100];
	    krb5_format_time(context, *server->valid_start,
			     starttime_str, sizeof(starttime_str), TRUE);
	    kdc_log(context, config, 0,
		    "Server not yet valid until %s -- %s",
		    starttime_str, server_name);
	    return KRB5KDC_ERR_SERVICE_NOTYET;
	}

	if (server->valid_end && *server->valid_end < kdc_time) {
	    char endtime_str[100];
	    krb5_format_time(context, *server->valid_end,
			     endtime_str, sizeof(endtime_str), TRUE);
	    kdc_log(context, config, 0,
		    "Server expired at %s -- %s",
		    endtime_str, server_name);
	    return KRB5KDC_ERR_SERVICE_EXP;
	}

	if (server->pw_end && *server->pw_end < kdc_time) {
	    char pwend_str[100];
	    krb5_format_time(context, *server->pw_end,
			     pwend_str, sizeof(pwend_str), TRUE);
	    kdc_log(context, config, 0,
		    "Server's key has expired at %s -- %s",
		    pwend_str, server_name);
	    return KRB5KDC_ERR_KEY_EXPIRED;
	}
    }
    return 0;
}
