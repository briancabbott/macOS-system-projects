/* This work is part of OpenLDAP Software <http://www.openldap.org/>.
 * 
 * Copyright 1998-2006 The OpenLDAP Foundation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted only as authorized by the OpenLDAP
 * Public License.
 *
 * A copy of this license is available in file LICENSE in the
 * top-level directory of the distribution or, alternatively, at
 * <http://www.OpenLDAP.org/license.html>.
 */
/* Portions Copyright (c) 1990 Regents of the University of Michigan.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of Michigan at Ann Arbor. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission. This software
 * is provided ``as is'' without express or implied warranty.
 */

#ifndef _LDAP_PRIVATE_H
#define _LDAP_PRIVATE_H

#include <ldap.h>

LDAP_BEGIN_DECL

/* Apple specific options */
#define LDAP_OPT_NOADDRERR		0x7100
#define LDAP_OPT_NOTIFYDESC_PROC		0x7101
#define LDAP_OPT_NOTIFYDESC_PARAMS		0x7102

/* LDAP_OPT_NOREVERSE_LOOKUP       0x7103 is public */

/* option that returns an error if using the session will cause an abort */
#define LDAP_OPT_TEST_SESSION			0x7104

/* specify static hostname for connection */
#define LDAP_OPT_SASL_FQDN			0x7105

typedef void (LDAP_NOTIFYDESC_PROC) LDAP_P((
	LDAP *ld, int desc, int opening,
	void *params ));


#ifdef __APPLE__
/* Callback type for asynchronous search results. */
typedef void
(*LDAPSearchResultsCallback) LDAP_P((
	LDAP *ld,
	LDAPMessage *result,
	int rc,
	void *context ));

/* Setting a callback on ld will make *all* searches on that ld asynchronous.
 * Calling ldap_result() after the callback has been set will result in
 * in error.
 */
LDAP_F( void )
ldap_set_search_results_callback LDAP_P((
	LDAP *ld,
	LDAPSearchResultsCallback cb,
	void *context ));
#endif

/* Apple TLSspecific error codes*/
#define LDAP_TLS_CACERTFILE_NOTFOUND       (-30)
#define LDAP_TLS_CERTFILE_NOTFOUND     	   (-31)
#define LDAP_TLS_CERTKEYFILE_NOTFOUND      (-32)
#define LDAP_TLS_PASSPHRASE_NOTFOUND 	   (-33)
#define LDAP_TLS_KEYCHAIN_CERT_NOTFOUND	   (-34)

LDAP_END_DECL

#endif /* _LDAP_PRIVATE_H */
