/* This is a generated file */
#ifndef __kdc_private_h__
#define __kdc_private_h__

#include <stdarg.h>

krb5_error_code
_kdc_add_KRB5SignedPath (
	krb5_context /*context*/,
	krb5_kdc_configuration */*config*/,
	hdb_entry_ex */*krbtgt*/,
	krb5_enctype /*enctype*/,
	krb5_principal /*client*/,
	krb5_const_principal /*server*/,
	krb5_principals /*principals*/,
	EncTicketPart */*tkt*/);

krb5_error_code
_kdc_add_inital_verified_cas (
	krb5_context /*context*/,
	krb5_kdc_configuration */*config*/,
	pk_client_params */*cp*/,
	EncTicketPart */*tkt*/);

krb5_error_code
_kdc_as_rep(kdc_request_t r,
	    krb5_data *reply,
	    const char *from,
	    struct sockaddr *from_addr,
	    size_t max_reply_size);

krb5_error_code
_kdc_check_access(krb5_context context,
		  krb5_kdc_configuration *config,
		  hdb_entry_ex *client_ex, const char *client_name,
		  hdb_entry_ex *server_ex, const char *server_name,
		  KDC_REQ *req,
		  METHOD_DATA *method_data);

krb5_boolean
_kdc_check_addresses (
	krb5_context /*context*/,
	krb5_kdc_configuration */*config*/,
	HostAddresses */*addresses*/,
	const struct sockaddr */*from*/);

krb5_error_code
_kdc_db_fetch (
	krb5_context /*context*/,
	krb5_kdc_configuration */*config*/,
	krb5_const_principal /*principal*/,
	unsigned /*flags*/,
	krb5int32 */*kvno_ptr*/,
	HDB **/*db*/,
	hdb_entry_ex **/*h*/);

krb5_error_code
_kdc_do_digest (
	krb5_context /*context*/,
	krb5_kdc_configuration */*config*/,
	const struct DigestREQ */*req*/,
	krb5_data */*reply*/,
	const char */*from*/,
	struct sockaddr */*addr*/);

krb5_error_code
_kdc_do_kx509 (
	krb5_context /*context*/,
	krb5_kdc_configuration */*config*/,
	const struct Kx509Request */*req*/,
	krb5_data */*reply*/,
	const char */*from*/,
	struct sockaddr */*addr*/);

krb5_error_code
_kdc_encode_reply(krb5_context context,
		  krb5_kdc_configuration *config,
		  kdc_request_t r,
		  uint32_t nonce,
		  KDC_REP *rep, EncTicketPart *et, EncKDCRepPart *ek,
		  krb5_enctype etype,
		  int skvno, const EncryptionKey *skey,
		  int ckvno,
		  int rk_is_subkey,
		  const char **e_text,
		  krb5_data *reply);

krb5_error_code
_kdc_find_etype (
	krb5_context /*context*/,
	krb5_boolean /*use_strongest_session_key*/,
	krb5_boolean /*is_preauth*/,
	hdb_entry_ex */*princ*/,
	krb5_enctype */*etypes*/,
	unsigned /*len*/,
	krb5_enctype */*ret_enctype*/,
	Key **/*ret_key*/);

const PA_DATA*
_kdc_find_padata (
	const KDC_REQ */*req*/,
	int */*start*/,
	int /*type*/);

void
_kdc_fix_time (time_t **/*t*/);

void
_kdc_free_ent (
	krb5_context /*context*/,
	hdb_entry_ex */*ent*/);

krb5_error_code
_kdc_get_preferred_key (
	krb5_context /*context*/,
	krb5_kdc_configuration */*config*/,
	hdb_entry_ex */*h*/,
	const char */*name*/,
	krb5_enctype */*enctype*/,
	Key **/*key*/);

krb5_boolean
_kdc_is_anonymous (
	krb5_context /*context*/,
	krb5_principal /*principal*/);

krb5_boolean
_kdc_is_weak_exception (
	krb5_principal /*principal*/,
	krb5_enctype /*etype*/);

void
_kdc_log_timestamp (
	krb5_context /*context*/,
	krb5_kdc_configuration */*config*/,
	const char */*type*/,
	KerberosTime /*authtime*/,
	KerberosTime */*starttime*/,
	KerberosTime /*endtime*/,
	KerberosTime */*renew_till*/);

krb5_error_code
_kdc_make_anonymous_principalname (PrincipalName */*pn*/);

krb5_error_code
_kdc_pac_generate (
	krb5_context /*context*/,
	hdb_entry_ex */*client*/,
	krb5_pac */*pac*/);

krb5_error_code
_kdc_pac_verify (
	krb5_context /*context*/,
	const krb5_principal /*client_principal*/,
	const krb5_principal /*delegated_proxy_principal*/,
	hdb_entry_ex */*client*/,
	hdb_entry_ex */*server*/,
	hdb_entry_ex */*krbtgt*/,
	krb5_pac */*pac*/,
	int */*verified*/);

krb5_error_code
_kdc_pk_check_client(krb5_context context,
		     krb5_kdc_configuration *config,
		     HDB *clientdb,
		     hdb_entry_ex *client,
		     InitiatorName *pku2uInitiatorAssertion,
		     pk_client_params *cp,
		     char **subject_name);

void
_kdc_pk_free_client_param (
	krb5_context /*context*/,
	pk_client_params */*cp*/);

krb5_error_code
_kdc_pk_mk_pa_reply (
	kdc_request_t /* r */,
	pk_client_params */*cp*/);

krb5_error_code
_kdc_pk_rd_padata (
	krb5_context /*context*/,
	krb5_kdc_configuration */*config*/,
	const KDC_REQ */*req*/,
	const PA_DATA */*pa*/,
	hdb_entry_ex */*client*/,
	pk_client_params **/*ret_params*/);

krb5_error_code
_kdc_tgs_rep(kdc_request_t r,
	     krb5_data *data,
	     const char *from,
	     struct sockaddr *from_addr,
	     size_t max_reply_size);

krb5_error_code
_kdc_tkt_add_if_relevant_ad (
	krb5_context /*context*/,
	EncTicketPart */*tkt*/,
	int /*type*/,
	const krb5_data */*data*/);

krb5_error_code
_kdc_try_kx509_request (
	void */*ptr*/,
	size_t /*len*/,
	struct Kx509Request */*req*/,
	size_t */*size*/);

krb5_error_code
_kdc_fast_mk_error(krb5_context context,
		   kdc_request_t r,
		   METHOD_DATA *error_method,
		   krb5_crypto armor_crypto,
		   const KDC_REQ_BODY *req_body,
		   krb5_error_code outer_error,
		   const char *e_text,
		   krb5_principal error_client,
		   krb5_principal error_server,
		   time_t *csec, int *cusec,
		   krb5_data *error_msg);

krb5_error_code
_kdc_fast_mk_response(krb5_context context,
		      krb5_crypto armor_crypto,
		      METHOD_DATA *pa_data,
		      krb5_keyblock *strengthen_key,
		      KrbFastFinished *finished,
		      krb5uint32 nonce,
		      krb5_data *data);

krb5_error_code
_kdc_fast_unwrap_request(kdc_request_t r,
			 krb5_ticket *tgs_ticket,
			 krb5_auth_context tgs_ac);


krb5_error_code
_kdc_fast_strengthen_reply_key(kdc_request_t r);

krb5_error_code
_kdc_get_preferred_enctype(krb5_context context,
			   krb5_kdc_configuration *config,
			   const hdb_entry_ex *entry,
			   const char *name,
			   krb5_enctype *etypes,
			   unsigned num_etypes,
			   krb5_enctype *etype);

#endif /* __kdc_private_h__ */
