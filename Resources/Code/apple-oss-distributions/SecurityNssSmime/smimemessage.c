/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/*
 * SMIME message methods
 */

#include "cmslocal.h"
#include "smime.h"

//#include "cert.h"
//#include "key.h"
#include "secasn1.h"
#include "SECItem.h"
#include "secoid.h"
//#include "pk11func.h"
#include "secerr.h"


#if 0
/*
 * NSS_SMIMEMessage_CreateEncrypted - start an S/MIME encrypting context.
 *
 * "scert" is the cert for the sender.  It will be checked for validity.
 * "rcerts" are the certs for the recipients.  They will also be checked.
 *
 * "certdb" is the cert database to use for verifying the certs.
 * It can be NULL if a default database is available (like in the client).
 *
 * This function already does all of the stuff specific to S/MIME protocol
 * and local policy; the return value just needs to be passed to
 * SEC_PKCS7Encode() or to SEC_PKCS7EncoderStart() to create the encoded data,
 * and finally to SEC_PKCS7DestroyContentInfo().
 *
 * An error results in a return value of NULL and an error set.
 * (Retrieve specific errors via PORT_GetError()/XP_GetError().)
 */
SecCmsMessage *
NSS_SMIMEMessage_CreateEncrypted(SecCertificateRef scert,
			SecCertificateRef *rcerts,
			SecKeychainRef *certdb,
			PK11PasswordFunc pwfn,
			void *pwfn_arg)
{
    SecCmsMessage *cmsg;
    long cipher;
    SECOidTag encalg;
    int keysize;
    int mapi, rci;

    cipher = smime_choose_cipher (scert, rcerts);
    if (cipher < 0)
	return NULL;

    mapi = smime_mapi_by_cipher (cipher);
    if (mapi < 0)
	return NULL;

    /*
     * XXX This is stretching it -- CreateEnvelopedData should probably
     * take a cipher itself of some sort, because we cannot know what the
     * future will bring in terms of parameters for each type of algorithm.
     * For example, just an algorithm and keysize is *not* sufficient to
     * fully specify the usage of RC5 (which also needs to know rounds and
     * block size).  Work this out into a better API!
     */
    encalg = smime_cipher_map[mapi].algtag;
    keysize = smime_keysize_by_cipher (cipher);
    if (keysize < 0)
	return NULL;

    cinfo = SEC_PKCS7CreateEnvelopedData (scert, certUsageEmailRecipient,
					  certdb, encalg, keysize,
					  pwfn, pwfn_arg);
    if (cinfo == NULL)
	return NULL;

    for (rci = 0; rcerts[rci] != NULL; rci++) {
	if (rcerts[rci] == scert)
	    continue;
	if (SEC_PKCS7AddRecipient (cinfo, rcerts[rci], certUsageEmailRecipient,
				   NULL) != SECSuccess) {
	    SEC_PKCS7DestroyContentInfo (cinfo);
	    return NULL;
	}
    }

    return cinfo;
}


/*
 * Start an S/MIME signing context.
 *
 * "scert" is the cert that will be used to sign the data.  It will be
 * checked for validity.
 *
 * "ecert" is the signer's encryption cert.  If it is different from
 * scert, then it will be included in the signed message so that the
 * recipient can save it for future encryptions.
 *
 * "certdb" is the cert database to use for verifying the cert.
 * It can be NULL if a default database is available (like in the client).
 * 
 * "digestalg" names the digest algorithm (e.g. SEC_OID_SHA1).
 * XXX There should be SECMIME functions for hashing, or the hashing should
 * be built into this interface, which we would like because we would
 * support more smartcards that way, and then this argument should go away.)
 *
 * "digest" is the actual digest of the data.  It must be provided in
 * the case of detached data or NULL if the content will be included.
 *
 * This function already does all of the stuff specific to S/MIME protocol
 * and local policy; the return value just needs to be passed to
 * SEC_PKCS7Encode() or to SEC_PKCS7EncoderStart() to create the encoded data,
 * and finally to SEC_PKCS7DestroyContentInfo().
 *
 * An error results in a return value of NULL and an error set.
 * (Retrieve specific errors via PORT_GetError()/XP_GetError().)
 */

SecCmsMessage *
NSS_SMIMEMessage_CreateSigned(SecCertificateRef scert,
		      SecCertificateRef ecert,
		      SecKeychainRef *certdb,
		      SECOidTag digestalgtag,
		      CSSM_DATA *digest,
		      PK11PasswordFunc pwfn,
		      void *pwfn_arg)
{
    SecCmsMessage *cmsg;
    SecCmsSignedData *sigd;
    SecCmsSignerInfo *signerinfo;

    /* See note in header comment above about digestalg. */
    /* Doesn't explain this.  PORT_Assert (digestalgtag == SEC_OID_SHA1); */

    cmsg = SecCmsMessageCreate(NULL);
    if (cmsg == NULL)
	return NULL;

    sigd = SecCmsSignedDataCreate(cmsg);
    if (sigd == NULL)
	goto loser;

    /* create just one signerinfo */
    signerinfo = SecCmsSignerInfoCreate(cmsg, scert, digestalgtag);
    if (signerinfo == NULL)
	goto loser;

    /* Add the signing time to the signerinfo.  */
    if (SecCmsSignerInfoAddSigningTime(signerinfo, CFAbsoluteTimeGetCurrent()) != SECSuccess)
	goto loser;
    
    /* and add the SMIME profile */
    if (NSS_SMIMESignerInfo_AddSMIMEProfile(signerinfo, scert) != SECSuccess)
	goto loser;

    /* now add the signerinfo to the signeddata */
    if (SecCmsSignedDataAddSignerInfo(sigd, signerinfo) != SECSuccess)
	goto loser;

    /* include the signing cert and its entire chain */
    /* note that there are no checks for duplicate certs in place, as all the */
    /* essential data structures (like set of certificate) are not there */
    if (SecCmsSignedDataAddCertChain(sigd, scert) != SECSuccess)
	goto loser;

    /* If the encryption cert and the signing cert differ, then include
     * the encryption cert too. */
    if ( ( ecert != NULL ) && ( ecert != scert ) ) {
	if (SecCmsSignedDataAddCertificate(sigd, ecert) != SECSuccess)
	    goto loser;
    }

    return cmsg;
loser:
    if (cmsg)
	SecCmsMessageDestroy(cmsg);
    return NULL;
}
#endif
