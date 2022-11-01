/*
 * mppe_keys.c
 *
 * Version:     $Id$
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 *
 * Copyright 2002  Axis Communications AB
 * Copyright 2006  The FreeRADIUS server project
 * Authors: Henrik Eriksson <henriken@axis.com> & Lars Viklund <larsv@axis.com>
 */

#include <freeradius-devel/ident.h>
RCSID("$Id$")

#include <openssl/hmac.h>
#include "eap_tls.h"

/*
 * Add value pair to reply
 */
static void add_reply(VALUE_PAIR** vp,
		      const char* name, const uint8_t * value, int len)
{
	VALUE_PAIR *reply_attr;
	reply_attr = pairmake(name, "", T_OP_EQ);
	if (!reply_attr) {
		DEBUG("rlm_eap_tls: "
		      "add_reply failed to create attribute %s: %s\n",
		      name, fr_strerror());
		return;
	}

	memcpy(reply_attr->vp_octets, value, len);
	reply_attr->length = len;
	pairadd(vp, reply_attr);
}

/*
 * TLS PRF from RFC 2246
 */
static void P_hash(const EVP_MD *evp_md,
		   const unsigned char *secret, unsigned int secret_len,
		   const unsigned char *seed,   unsigned int seed_len,
		   unsigned char *out, unsigned int out_len)
{
	HMAC_CTX ctx_a, ctx_out;
	unsigned char a[HMAC_MAX_MD_CBLOCK];
	unsigned int size;

	HMAC_CTX_init(&ctx_a);
	HMAC_CTX_init(&ctx_out);
	HMAC_Init_ex(&ctx_a, secret, secret_len, evp_md, NULL);
	HMAC_Init_ex(&ctx_out, secret, secret_len, evp_md, NULL);

	size = HMAC_size(&ctx_out);

	/* Calculate A(1) */
	HMAC_Update(&ctx_a, seed, seed_len);
	HMAC_Final(&ctx_a, a, NULL);

	while (1) {
		/* Calculate next part of output */
		HMAC_Update(&ctx_out, a, size);
		HMAC_Update(&ctx_out, seed, seed_len);

		/* Check if last part */
		if (out_len < size) {
			HMAC_Final(&ctx_out, a, NULL);
			memcpy(out, a, out_len);
			break;
		}

		/* Place digest in output buffer */
		HMAC_Final(&ctx_out, out, NULL);
		HMAC_Init_ex(&ctx_out, NULL, 0, NULL, NULL);
		out += size;
		out_len -= size;

		/* Calculate next A(i) */
		HMAC_Init_ex(&ctx_a, NULL, 0, NULL, NULL);
		HMAC_Update(&ctx_a, a, size);
		HMAC_Final(&ctx_a, a, NULL);
	}

	HMAC_CTX_cleanup(&ctx_a);
	HMAC_CTX_cleanup(&ctx_out);
	memset(a, 0, sizeof(a));
}

static void PRF(const unsigned char *secret, unsigned int secret_len,
		const unsigned char *seed,   unsigned int seed_len,
		unsigned char *out, unsigned char *buf, unsigned int out_len)
{
        unsigned int i;
        unsigned int len = (secret_len + 1) / 2;
	const unsigned char *s1 = secret;
	const unsigned char *s2 = secret + (secret_len - len);

	P_hash(EVP_md5(),  s1, len, seed, seed_len, out, out_len);
	P_hash(EVP_sha1(), s2, len, seed, seed_len, buf, out_len);

	for (i=0; i < out_len; i++) {
	        out[i] ^= buf[i];
	}
}

#define EAPTLS_MPPE_KEY_LEN     32

#define EAPTLS_PRF_LABEL "ttls keying material"

/*
 *	Generate keys according to RFC 2716 and add to reply
 */
void eaptls_gen_mppe_keys(VALUE_PAIR **reply_vps, SSL *s,
			  const char *prf_label)
{
	unsigned char out[4*EAPTLS_MPPE_KEY_LEN], buf[4*EAPTLS_MPPE_KEY_LEN];
	unsigned char seed[64 + 2*SSL3_RANDOM_SIZE];
	unsigned char *p = seed;
	size_t prf_size;

	if (!s->s3) {
		radlog(L_ERR, "ERROR: OpenSSL build / link incompatibility detected");
		return;
	}

	prf_size = strlen(prf_label);

	memcpy(p, prf_label, prf_size);
	p += prf_size;

	memcpy(p, s->s3->client_random, SSL3_RANDOM_SIZE);
	p += SSL3_RANDOM_SIZE;
	prf_size += SSL3_RANDOM_SIZE;

	memcpy(p, s->s3->server_random, SSL3_RANDOM_SIZE);
	prf_size += SSL3_RANDOM_SIZE;

	PRF(s->session->master_key, s->session->master_key_length,
	    seed, prf_size, out, buf, sizeof(out));

	p = out;
	add_reply(reply_vps, "MS-MPPE-Recv-Key", p, EAPTLS_MPPE_KEY_LEN);
	p += EAPTLS_MPPE_KEY_LEN;
	add_reply(reply_vps, "MS-MPPE-Send-Key", p, EAPTLS_MPPE_KEY_LEN);

	add_reply(reply_vps, "EAP-MSK", out, 64);
	add_reply(reply_vps, "EAP-EMSK", out + 64, 64);
}


#define EAPTLS_PRF_CHALLENGE        "ttls challenge"

/*
 *	Generate the TTLS challenge
 *
 *	It's in the TLS module simply because it's only a few lines
 *	of code, and it needs access to the TLS PRF functions.
 */
void eapttls_gen_challenge(SSL *s, uint8_t *buffer, size_t size)
{
	uint8_t out[32], buf[32];
	uint8_t seed[sizeof(EAPTLS_PRF_CHALLENGE)-1 + 2*SSL3_RANDOM_SIZE];
	uint8_t *p = seed;

	if (!s->s3) {
		radlog(L_ERR, "ERROR: OpenSSL build / link incompatibility detected");
		return;
	}

	memcpy(p, EAPTLS_PRF_CHALLENGE, sizeof(EAPTLS_PRF_CHALLENGE)-1);
	p += sizeof(EAPTLS_PRF_CHALLENGE)-1;
	memcpy(p, s->s3->client_random, SSL3_RANDOM_SIZE);
	p += SSL3_RANDOM_SIZE;
	memcpy(p, s->s3->server_random, SSL3_RANDOM_SIZE);

	PRF(s->session->master_key, s->session->master_key_length,
	    seed, sizeof(seed), out, buf, sizeof(out));

	memcpy(buffer, out, size);
}
