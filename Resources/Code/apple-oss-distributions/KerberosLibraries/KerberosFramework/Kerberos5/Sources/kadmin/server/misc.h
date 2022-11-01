/*
 * Copyright 1994 OpenVision Technologies, Inc., All Rights Reserved
 *
 */

kadm5_ret_t
chpass_principal_wrapper_4(void *server_handle,
			   krb5_principal principal,
			   krb5_boolean keepold,
			   int n_ks_tuple,
			   krb5_key_salt_tuple *ks_tuple,
			   char *password,
			   int is_admin);

kadm5_ret_t
randkey_principal_wrapper_3(void *server_handle,
			    krb5_principal principal,
			    krb5_boolean keepold,
			    int n_ks_tuple,
			    krb5_key_salt_tuple *ks_tuple,
			    krb5_keyblock **keys, int *n_keys);

kadm5_ret_t
schpw_util_wrapper(void *server_handle, krb5_principal princ,
		   char *new_pw, char **ret_pw,
		   char *msg_ret, unsigned int msg_len);

kadm5_ret_t check_min_life(void *server_handle, krb5_principal principal,
			   char *msg_ret, unsigned int msg_len);

kadm5_ret_t kadm5_get_principal_v1(void *server_handle,
				   krb5_principal principal, 
				   kadm5_principal_ent_t_v1 *ent);

kadm5_ret_t kadm5_get_policy_v1(void *server_handle, kadm5_policy_t name,
				kadm5_policy_ent_t *ent);


krb5_error_code
process_chpw_request(krb5_context context, 
		     void *server_handle,
		     char *realm, 
		     krb5_keytab keytab, 
		     struct sockaddr_in *sockto,
		     struct sockaddr_in *sockfrom,
		     krb5_data *req,
		     krb5_data *rep);

#ifdef SVC_GETARGS
void  kadm_1(struct svc_req *, SVCXPRT *);
#endif

void trunc_name(size_t *len, char **dots);

int
gss_to_krb5_name_1(struct svc_req *rqstp, krb5_context ctx, gss_name_t gss_name,
		   krb5_principal *princ, gss_buffer_t gss_str);
