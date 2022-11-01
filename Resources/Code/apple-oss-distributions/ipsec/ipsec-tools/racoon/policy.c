/*	$KAME: policy.c,v 1.46 2001/11/16 04:08:10 sakane Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>

#include <netinet/in.h>
#ifdef HAVE_NETINET6_IPSEC
#  include <netinet6/ipsec.h>
#else
#  include <netinet/ipsec.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "var.h"
#include "misc.h"
#include "vmbuf.h"
#include "plog.h"
#include "sockmisc.h"
#include "debug.h"

#include "policy.h"
#include "localconf.h"
#include "isakmp_var.h"
#include "isakmp.h"
#include "oakley.h"
#include "handler.h"
#include "strnames.h"
#include "gcmalloc.h"
#include "session.h"

static TAILQ_HEAD(_sptree, secpolicy) sptree;

/* perform exact match against security policy table. */
struct secpolicy *
getsp(spidx)
	struct policyindex *spidx;
{
	struct secpolicy *p;

	for (p = TAILQ_FIRST(&sptree); p; p = TAILQ_NEXT(p, chain)) {
		if (!cmpspidxstrict(spidx, &p->spidx))
			return p;
	}

	return NULL;
}

/*
 * perform non-exact match against security policy table, only if this is
 * transport mode SA negotiation.  for example, 0.0.0.0/0 -> 0.0.0.0/0
 * entry in policy.txt can be returned when we're negotiating transport
 * mode SA.  this is how the kernel works.
 */
#if 1
struct secpolicy *
getsp_r(spidx, iph2)
	struct policyindex *spidx;
	phase2_handle_t *iph2;
{
	struct secpolicy *p;
	int mismatched_outer_addr = 0;

	for (p = TAILQ_FIRST(&sptree); p; p = TAILQ_NEXT(p, chain)) {
		if (!cmpspidxwild(spidx, &p->spidx)) {
			if (spidx->dir != IPSEC_DIR_ANY) {
				struct ipsecrequest *isr;
				for (isr = p->req; isr != NULL; isr = isr->next) {
					if (isr->saidx.mode != IPSEC_MODE_TUNNEL) {
						plog(ASL_LEVEL_DEBUG, "%s, skipping policy. dir %d, mode %d\n",
							 __FUNCTION__, spidx->dir, isr->saidx.mode);
						continue;
					}

					// for tunnel mode: verify the outer ip addresses match the phase2's addresses
					if (spidx->dir == IPSEC_DIR_INBOUND) {
						// TODO: look out for wildcards
						if (!cmpsaddrwop(iph2->dst, &isr->saidx.src) &&
							!cmpsaddrwop(iph2->src, &isr->saidx.dst)) {
							plog(ASL_LEVEL_DEBUG, "%s, inbound policy outer addresses matched Phase 2 addresses\n",
								 __FUNCTION__);
							return p;
						} else {
							mismatched_outer_addr = 1;
						}
					} else if (spidx->dir == IPSEC_DIR_OUTBOUND) {
						// TODO: look out for wildcards
						if (!cmpsaddrwop(iph2->src, &isr->saidx.src) &&
							!cmpsaddrwop(iph2->dst, &isr->saidx.dst)) {
							plog(ASL_LEVEL_DEBUG, "%s, outbound policy outer addresses matched Phase 2 addresses\n",
								 __FUNCTION__);
							return p;
						} else {
							mismatched_outer_addr = 1;
						}
					} else {
						mismatched_outer_addr = 1;
					}
					if (mismatched_outer_addr) {
						plog(ASL_LEVEL_DEBUG, "%s, policy outer addresses matched Phase 2 addresses: dir %d\n",
							 __FUNCTION__, spidx->dir);
						plog(ASL_LEVEL_DEBUG, "src1: %s\n",
							 saddr2str((struct sockaddr *)iph2->src));
						plog(ASL_LEVEL_DEBUG, "src2: %s\n",
							 saddr2str((struct sockaddr *)&isr->saidx.src));
						plog(ASL_LEVEL_DEBUG, "dst1: %s\n",
							 saddr2str((struct sockaddr *)iph2->dst));
						plog(ASL_LEVEL_DEBUG, "dst2: %s\n",
							 saddr2str((struct sockaddr *)&isr->saidx.dst));
					}
				}
			}
			if (!mismatched_outer_addr) {
				return p;
			}
		}
	}

	return NULL;
}
#else
struct secpolicy *
getsp_r(spidx, iph2)
	struct policyindex *spidx;
	phase2_handle_t *iph2;
{
	struct secpolicy *p;
	u_int8_t prefixlen;

	plog(ASL_LEVEL_DEBUG, "checking for transport mode\n");

	if (spidx->src.ss_family != spidx->dst.ss_family) {
		plog(ASL_LEVEL_ERR, 
			"address family mismatch, src:%d dst:%d\n",
				spidx->src.ss_family,
				spidx->dst.ss_family);
		return NULL;
	}
	switch (spidx->src.ss_family) {
	case AF_INET:
		prefixlen = sizeof(struct in_addr) << 3;
		break;
#ifdef INET6
	case AF_INET6:
		prefixlen = sizeof(struct in6_addr) << 3;
		break;
#endif
	default:
		plog(ASL_LEVEL_ERR, 
			"invalid family: %d\n", spidx->src.ss_family);
		return NULL;
	}

	/* is it transport mode SA negotiation? */
	plog(ASL_LEVEL_DEBUG, "src1: %s\n",
		saddr2str(iph2->src));
	plog(ASL_LEVEL_DEBUG, "src2: %s\n",
		saddr2str(&spidx->src));
	if (cmpsaddrwop(iph2->src, &spidx->src)
	 || spidx->prefs != prefixlen)
		return NULL;

	plog(ASL_LEVEL_DEBUG, "dst1: %s\n",
		saddr2str(iph2->dst));
	plog(ASL_LEVEL_DEBUG, "dst2: %s\n",
		saddr2str(&spidx->dst));
	if (cmpsaddrwop(iph2->dst, &spidx->dst)
	 || spidx->prefd != prefixlen)
		return NULL;

	plog(ASL_LEVEL_DEBUG, "looks to be transport mode\n");

	for (p = TAILQ_FIRST(&sptree); p; p = TAILQ_NEXT(p, chain)) {
		if (!cmpspidx_wild(spidx, &p->spidx))
			return p;
	}

	return NULL;
}
#endif

struct secpolicy *
getspbyspid(spid)
	u_int32_t spid;
{
	struct secpolicy *p;

	for (p = TAILQ_FIRST(&sptree); p; p = TAILQ_NEXT(p, chain)) {
		if (p->id == spid)
			return p;
	}

	return NULL;
}

/*
 * compare policyindex.
 * a: subject b: db
 * OUT:	0:	equal
 *	1:	not equal
 */
int
cmpspidxstrict(a, b)
	struct policyindex *a, *b;
{

	/* XXX don't check direction now, but it's to be checked carefully. */
	if (a->dir != b->dir
	 || a->prefs != b->prefs
	 || a->prefd != b->prefd
	 || a->ul_proto != b->ul_proto)
		return 1;

	if (cmpsaddrstrict(&a->src, &b->src))
		return 1;
	if (cmpsaddrstrict(&a->dst, &b->dst))
		return 1;

	return 0;
}

/*
 * compare policyindex, with wildcard address/protocol match.
 * a: subject b: db, can contain wildcard things.
 * OUT:	0:	equal
 *	1:	not equal
 */
int
cmpspidxwild(a, b)
	struct policyindex *a, *b;
{
	struct sockaddr_storage sa1, sa2;

	if (!(b->dir == IPSEC_DIR_ANY || a->dir == b->dir))
		return 1;

	if (!(a->ul_proto == IPSEC_ULPROTO_ANY ||
	      b->ul_proto == IPSEC_ULPROTO_ANY ||
	      a->ul_proto == b->ul_proto))
		return 1;

	if (a->src.ss_family != b->src.ss_family)
		return 1;
	if (a->dst.ss_family != b->dst.ss_family)
		return 1;

	/* compare src address */
	if (sizeof(sa1) < a->src.ss_len || sizeof(sa2) < b->src.ss_len) {
		plog(ASL_LEVEL_ERR, 
			"unexpected error: "
			"src.ss_len:%d dst.ss_len:%d\n",
			a->src.ss_len, b->src.ss_len);
		return 1;
	}
	mask_sockaddr(&sa1, &a->src, b->prefs);
	mask_sockaddr(&sa2, &b->src, b->prefs);
	plog(ASL_LEVEL_DEBUG, "%p masked with /%d: %s\n",
		a, b->prefs, saddr2str((struct sockaddr *)&sa1));
	plog(ASL_LEVEL_DEBUG, "%p masked with /%d: %s\n",
		b, b->prefs, saddr2str((struct sockaddr *)&sa2));
	if (cmpsaddrwild(&sa1, &sa2))
		return 1;

	/* compare dst address */
	if (sizeof(sa1) < a->dst.ss_len || sizeof(sa2) < b->dst.ss_len) {
		plog(ASL_LEVEL_ERR, "unexpected error\n");
		exit(1);
	}
	mask_sockaddr(&sa1, &a->dst, b->prefd);
	mask_sockaddr(&sa2, &b->dst, b->prefd);
	plog(ASL_LEVEL_DEBUG, "%p masked with /%d: %s\n",
		a, b->prefd, saddr2str((struct sockaddr *)&sa1));
	plog(ASL_LEVEL_DEBUG, "%p masked with /%d: %s\n",
		b, b->prefd, saddr2str((struct sockaddr *)&sa2));
	if (cmpsaddrwild(&sa1, &sa2))
		return 1;

	return 0;
}

struct secpolicy *
newsp()
{
	struct secpolicy *new;

	new = racoon_calloc(1, sizeof(*new));
	if (new == NULL)
		return NULL;

	return new;
}

void
delsp(sp)
	struct secpolicy *sp;
{
	struct ipsecrequest *req = NULL, *next;

	for (req = sp->req; req; req = next) {
		next = req->next;
		racoon_free(req);
	}
	
	racoon_free(sp);
}

void
delsp_bothdir(spidx0)
	struct policyindex *spidx0;
{
	struct policyindex spidx;
	struct secpolicy *sp;
	struct sockaddr_storage src, dst;
	u_int8_t prefs, prefd;

	memcpy(&spidx, spidx0, sizeof(spidx));
	switch (spidx.dir) {
	case IPSEC_DIR_INBOUND:
#ifdef HAVE_POLICY_FWD
	case IPSEC_DIR_FWD:
#endif
		src   = spidx.src;
		dst   = spidx.dst;
		prefs = spidx.prefs;
		prefd = spidx.prefd;
		break;
	case IPSEC_DIR_OUTBOUND:
		src   = spidx.dst;
		dst   = spidx.src;
		prefs = spidx.prefd;
		prefd = spidx.prefs;
		break;
	default:
		return;
	}

	spidx.src   = src;
	spidx.dst   = dst;
	spidx.prefs = prefs;
	spidx.prefd = prefd;
	spidx.dir   = IPSEC_DIR_INBOUND;

	sp = getsp(&spidx);
	if (sp) {
		remsp(sp);
		delsp(sp);
	}

#ifdef HAVE_POLICY_FWD
	spidx.dir   = IPSEC_DIR_FWD;

	sp = getsp(&spidx);
	if (sp) {
		remsp(sp);
		delsp(sp);
	}
#endif

	spidx.src   = dst;
	spidx.dst   = src;
	spidx.prefs = prefd;
	spidx.prefd = prefs;
	spidx.dir   = IPSEC_DIR_OUTBOUND;

	sp = getsp(&spidx);
	if (sp) {
		remsp(sp);
		delsp(sp);
	}
}

void
inssp(new)
	struct secpolicy *new;
{
#ifdef HAVE_PFKEY_POLICY_PRIORITY
	struct secpolicy *p;

	TAILQ_FOREACH(p, &sptree, chain) {
		if (new->spidx.priority < p->spidx.priority) {
			TAILQ_INSERT_BEFORE(p, new, chain);
			return;
		}
	}
	if (p == NULL)
#endif
	        TAILQ_INSERT_HEAD(&sptree, new, chain);

	check_auto_exit();
	return;
}

void
remsp(sp)
	struct secpolicy *sp;
{
	TAILQ_REMOVE(&sptree, sp, chain);
	check_auto_exit();
}

void
flushsp()
{
	struct secpolicy *p, *next;

	for (p = TAILQ_FIRST(&sptree); p; p = next) {
		next = TAILQ_NEXT(p, chain);
		remsp(p);
		delsp(p);
	}
}

int
policies_installed(void)
{
	if (TAILQ_EMPTY(&sptree))
		return 0;
	else
		return 1;
}

void
initsp()
{
	TAILQ_INIT(&sptree);
}

struct ipsecrequest *
newipsecreq()
{
	struct ipsecrequest *new;

	new = racoon_calloc(1, sizeof(*new));
	if (new == NULL)
		return NULL;

	return new;
}

const char *
spidx2str(spidx)
	const struct policyindex *spidx;
{
	/* addr/pref[port] addr/pref[port] ul dir act */
	static char buf[256];
	char *p, *a, *b;
	int blen, i;

	blen = sizeof(buf) - 1;
	p = buf;

	a = saddr2str((const struct sockaddr *)&spidx->src);
	for (b = a; *b != '\0'; b++)
		if (*b == '[') {
			*b = '\0';
			b++;
			break;
		}
	i = snprintf(p, blen, "%s/%d[%s ", a, spidx->prefs, b);
	if (i < 0 || i >= blen)
		return NULL;
	p += i;
	blen -= i;

	a = saddr2str((const struct sockaddr *)&spidx->dst);
	for (b = a; *b != '\0'; b++)
		if (*b == '[') {
			*b = '\0';
			b++;
			break;
		}
	i = snprintf(p, blen, "%s/%d[%s ", a, spidx->prefd, b);
	if (i < 0 || i >= blen)
		return NULL;
	p += i;
	blen -= i;

	snprintf(p, blen, "proto=%s dir=%s",
		s_proto(spidx->ul_proto), s_direction(spidx->dir));

	return buf;
}
