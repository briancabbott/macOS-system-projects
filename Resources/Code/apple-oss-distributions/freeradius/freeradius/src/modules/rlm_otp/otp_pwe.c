/*
 * $Id$
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
 * Copyright 2001,2002  Google, Inc.
 * Copyright 2005,2006 TRI-D Systems, Inc.
 */

/*
 * This file implements passcode (password) checking functions for each
 * supported encoding (PAP, CHAP, etc.).  The current libradius interface
 * is not sufficient for X9.9 use.
 */

#include <freeradius-devel/ident.h>
RCSID("$Id$")

/* avoid inclusion of these FR headers which conflict w/ OpenSSL */
#define _FR_MD4_H
#define _FR_SHA1_H
#include <freeradius-devel/radiusd.h>
#include <freeradius-devel/rad_assert.h>

#include "extern.h"

#include <openssl/des.h>
#include <openssl/md4.h>
#include <openssl/md5.h>
#include <openssl/sha.h>

#include <string.h>

/* Attribute IDs for supported password encodings. */
#define SIZEOF_PWATTR (4 * 2)
int pwattr[SIZEOF_PWATTR];


/* Initialize the pwattr array for supported password encodings. */
void
otp_pwe_init(void)
{
  DICT_ATTR *da;

  /*
   * Setup known password types.  These are pairs.
   * NB: Increase pwattr array size when adding a type.
   *     It should be sized as (number of password types * 2)
   * NB: Array indices must match otp_pwe_t! (see otp.h)
   */
  (void) memset(pwattr, 0, sizeof(pwattr));

  /* PAP */
  if ((da = dict_attrbyname("User-Password")) != NULL) {
    pwattr[0] = da->attr;
    pwattr[1] = da->attr;
  }

  /* CHAP */
  if ((da = dict_attrbyname("CHAP-Challenge")) != NULL) {
    pwattr[2] = da->attr;
    if ((da = dict_attrbyname("CHAP-Password")) != NULL)
      pwattr[3] = da->attr;
    else
      pwattr[2] = 0;
  }

#if 0
  /* MS-CHAP (recommended not to use) */
  if ((da = dict_attrbyname("MS-CHAP-Challenge")) != NULL) {
    pwattr[4] = da->attr;
    if ((da = dict_attrbyname("MS-CHAP-Response")) != NULL)
      pwattr[5] = da->attr;
    else
      pwattr[4] = 0;
  }
#endif /* 0 */

  /* MS-CHAPv2 */
  if ((da = dict_attrbyname("MS-CHAP-Challenge")) != NULL) {
    pwattr[6] = da->attr;
    if ((da = dict_attrbyname("MS-CHAP2-Response")) != NULL)
      pwattr[7] = da->attr;
    else
      pwattr[6] = 0;
  }
}


/*
 * Test for password presence in an Access-Request packet.
 * Returns 0 for "no supported password present", or the
 * password encoding type.
 */
otp_pwe_t
otp_pwe_present(const REQUEST *request)
{
  unsigned i;

  for (i = 0; i < SIZEOF_PWATTR; i += 2) {
    if (pairfind(request->packet->vps, pwattr[i]) &&
        pairfind(request->packet->vps, pwattr[i + 1])) {
      DEBUG("rlm_otp: %s: password attributes %d, %d", __func__,
             pwattr[i], pwattr[i + 1]);
      return i + 1; /* Can't return 0 (indicates failure) */
    }
  }

  DEBUG("rlm_otp: %s: no password attributes present", __func__);
  return 0;
}
