/*
 * proxy.h - proxy support functions
 *
 * Copyright (c) 1998-2003 Carnegie Mellon University.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name "Carnegie Mellon University" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For permission or any other legal
 *    details, please contact  
 *      Office of Technology Transfer
 *      Carnegie Mellon University
 *      5000 Forbes Avenue
 *      Pittsburgh, PA  15213-3890
 *      (412) 268-4387, fax: (412) 268-7395
 *      tech-transfer@andrew.cmu.edu
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    "This product includes software developed by Computing Services
 *    acknowledgment:
 *     at Carnegie Mellon University (http://www.cmu.edu/computing/)."
 *
 * CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: proxy.h,v 1.2 2006/11/30 17:11:19 murch Exp $
 */

#ifndef _PROXY_H
#define _PROXY_H

#include "backend.h"
#include "protocol.h"
#include "prot.h"

#define IDLE_TIMEOUT (5 * 60)

/* a final destination for a message */
struct rcpt {
    char rcpt[MAX_MAILBOX_NAME+1]; /* where? */
    int rcpt_num;		    /* credit this to who? */
    struct rcpt *next;
};

struct dest {
    char server[MAX_MAILBOX_NAME+1];  /* where? */
    char authas[MAX_MAILBOX_NAME+1];  /* as who? */
    int rnum;			      /* number of rcpts */
    struct rcpt *to;
    struct dest *next;
};

void proxy_adddest(struct dest **dlist, const char *rcpt, int rcpt_num,
		   char *server, const char *authas);

struct backend *
proxy_findserver(const char *server, struct protocol_t *prot,
		 const char *userid, struct backend ***cache,
		 struct backend **current, struct backend **inbox,
		 struct protstream *clientin);

void proxy_downserver(struct backend *s);

int proxy_check_input(struct protgroup *protin,
		      struct protstream *clientin,
		      struct protstream *clientout,
		      struct protstream *serverin,
		      struct protstream *serverout,
		      unsigned long timeout_sec);

#endif /* _PROXY_H */
