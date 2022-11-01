//
//  tls_sectrust_helpers.h
//  coretls_cfhelpers
//


/* Helper functions for coreTLS test */

#ifndef __TLS_HELPERS_H__
#define __TLS_HELPERS_H__

#include <tls_handshake.h>
#include <Security/SecTrust.h>
#include <Security/SecureTransport.h>

#include "tls_types.h"

#ifndef CORETLS_EXTERN
#   ifdef __cplusplus
#       define CORETLS_EXTERN extern "C" __attribute__((visibility ("default")))
#   else
#       define CORETLS_EXTERN extern __attribute__((visibility ("default")))
#   endif
#endif

CORETLS_EXTERN OSStatus
tls_helper_create_peer_trust(tls_handshake_t hdsk, bool server, SecTrustRef *trustRef);

CORETLS_EXTERN OSStatus
tls_helper_set_peer_pubkey(tls_handshake_t hdsk);

CORETLS_EXTERN tls_protocol_version
tls_helper_version_from_SSLProtocol(SSLProtocol protocol);

CORETLS_EXTERN SSLProtocol
tls_helper_SSLProtocol_from_version(tls_protocol_version version);

/* Create a CFArray of CFData from the list returned by tls_handshake_get_peer_acceptable_dn_list */
CORETLS_EXTERN CFArrayRef
tls_helper_create_peer_acceptable_dn_array(tls_handshake_t hdsk);

CORETLS_EXTERN OSStatus
tls_helper_set_identity_from_array(tls_handshake_t hdsk, CFArrayRef certchain);

#endif
