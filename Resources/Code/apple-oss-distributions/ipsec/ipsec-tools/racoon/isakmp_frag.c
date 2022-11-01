/*	$NetBSD: isakmp_frag.c,v 1.4 2006/09/09 16:22:09 manu Exp $	*/

/* Id: isakmp_frag.c,v 1.4 2004/11/13 17:31:36 manubsd Exp */

/*
 * Copyright (C) 2004 Emmanuel Dreyfus
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/queue.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef HAVE_OPENSSL
#include <openssl/md5.h> 
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#include <netdb.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <ctype.h>

#include "var.h"
#include "misc.h"
#include "vmbuf.h"
#include "plog.h"
#include "sockmisc.h"
#include "schedule.h"
#include "debug.h"

#include "isakmp_var.h"
#include "isakmp.h"
#include "handler.h"
#include "isakmp_frag.h"
#include "strnames.h"
#include "nattraversal.h"
#include "grabmyaddr.h"
#include "localconf.h"
#include "crypto_openssl.h"

int
isakmp_sendfrags(iph1, buf) 
	phase1_handle_t *iph1;
	vchar_t *buf;
{
	struct isakmp *hdr;
	struct isakmp_frag *fraghdr;
	caddr_t data;
	caddr_t sdata;
	size_t datalen;
	size_t max_datalen;
	size_t fraglen;
	vchar_t *frag;
	unsigned int fragnum = 0;
	size_t len;
	int etype;
#ifdef ENABLE_NATT
	size_t extralen = NON_ESP_MARKER_USE(iph1)? NON_ESP_MARKER_LEN : 0;
#else
	size_t extralen = 0;
#endif
	int s;
	vchar_t *vbuf;


	/* select the socket to be sent */
	s = getsockmyaddr((struct sockaddr *)iph1->local);
	if (s == -1){
		return -1;
	}

	/*
	 * Catch the exchange type for later: the fragments and the
	 * fragmented packet must have the same exchange type.
	 */
	hdr = (struct isakmp *)buf->v;
	etype = hdr->etype;

	/*
	 * We want to send a a packet smaller than ISAKMP_FRAG_MAXLEN
	 * First compute the maximum data length that will fit in it
	 */
	max_datalen = ISAKMP_FRAG_MAXLEN - 
	    (sizeof(*hdr) + sizeof(*fraghdr));

	sdata = buf->v;
	len = buf->l;

	while (len > 0) {
		fragnum++;

		if (len > max_datalen)
			datalen = max_datalen;
		else
			datalen = len;

		fraglen = sizeof(*hdr) + sizeof(*fraghdr) + datalen;

		if ((frag = vmalloc(fraglen)) == NULL) {
			plog(ASL_LEVEL_ERR, 
			    "Cannot allocate memory\n");
			return -1;
		}

		set_isakmp_header1(frag, iph1, ISAKMP_NPTYPE_FRAG);
		hdr = (struct isakmp *)frag->v;
		hdr->etype = etype;

		fraghdr = (struct isakmp_frag *)(hdr + 1);
		fraghdr->unknown0 = 0;
		fraghdr->len = htons(fraglen - sizeof(*hdr));
		fraghdr->unknown1 = htons(1);
		fraghdr->index = fragnum;
		if (len == datalen)
			fraghdr->flags = ISAKMP_FRAG_LAST;
		else
			fraghdr->flags = 0;

		data = (caddr_t)(fraghdr + 1);
		memcpy(data, sdata, datalen);

#ifdef ENABLE_NATT
		/* If NAT-T port floating is in use, 4 zero bytes (non-ESP marker) 
		 must added just before the packet itself. For this we must 
		 allocate a new buffer and release it at the end. */
		if (extralen) {
			if ((vbuf = vmalloc(frag->l + extralen)) == NULL) {
				plog(ASL_LEVEL_ERR, 
					 "%s: vbuf allocation failed\n", __FUNCTION__);
				vfree(frag);
				return -1;
			}
			*ALIGNED_CAST(u_int32_t *)vbuf->v = 0; // non-esp marker
			memcpy(vbuf->v + extralen, frag->v, frag->l);
			vfree(frag);
			frag = vbuf;
		}
#endif

		if (sendfromto(s, frag->v, frag->l,
					   iph1->local, iph1->remote, lcconf->count_persend) == -1) {
			plog(ASL_LEVEL_ERR, "%s: sendfromto failed\n", __FUNCTION__);
			vfree(frag);
			return -1;
		}
		
		vfree(frag);

		len -= datalen;
		sdata += datalen;
	}

	plog(ASL_LEVEL_DEBUG, 
		 "%s: processed %d fragments\n", __FUNCTION__, fragnum);

	return fragnum;
}

unsigned int 
vendorid_frag_cap(gen)
	struct isakmp_gen *gen;
{
	int *hp;
	int hashlen_bytes = eay_md5_hashlen() >> 3;

	hp = ALIGNED_CAST(int *)(gen + 1);

	return ntohl(hp[hashlen_bytes / sizeof(*hp)]);
}

int 
isakmp_frag_extract(iph1, msg)
	phase1_handle_t *iph1;
	vchar_t *msg;
{
	struct isakmp *isakmp;
	struct isakmp_frag *frag;
	struct isakmp_frag_item *item;
	vchar_t *buf;
	int last_frag = 0;
	char *data;
	int i;

	if (msg->l < sizeof(*isakmp) + sizeof(*frag)) {
		plog(ASL_LEVEL_ERR, "Message too short\n");
		return -1;
	}

	isakmp = (struct isakmp *)msg->v;
	frag = (struct isakmp_frag *)(isakmp + 1);

	/* 
	 * frag->len is the frag payload data plus the frag payload header,
	 * whose size is sizeof(*frag) 
	 */
	if (msg->l < sizeof(*isakmp) + ntohs(frag->len) ||
		ntohs(frag->len) < sizeof(*frag) + 1) {
		plog(ASL_LEVEL_ERR, "Fragment too short\n");
		return -1;
	}

	if (ntohs(frag->len) < sizeof(*frag)) {
		plog(ASL_LEVEL_ERR, 
			 "invalid Frag, frag-len %d\n",
			 ntohs(frag->len));
		return -1;
	}

	if ((buf = vmalloc(ntohs(frag->len) - sizeof(*frag))) == NULL) {
		plog(ASL_LEVEL_ERR, "Cannot allocate memory\n");
		return -1;
	}

	if ((item = racoon_malloc(sizeof(*item))) == NULL) {
		plog(ASL_LEVEL_ERR, "Cannot allocate memory\n");
		vfree(buf);
		return -1;
	}
    bzero(item, sizeof(*item));
    
	data = (char *)(frag + 1);
	memcpy(buf->v, data, buf->l);

	item->frag_num = frag->index;
	item->frag_last = (frag->flags & ISAKMP_FRAG_LAST);
	item->frag_next = NULL;
	item->frag_packet = buf;
	item->frag_id = ntohs(frag->unknown1);
    
    plog(ASL_LEVEL_DEBUG,
		 "%s: received fragment #%d  frag ID=%d  last frag=%d\n",
            __FUNCTION__, item->frag_num, item->frag_id, item->frag_last);

    /* Insert if new and find the last frag num if present */
    struct isakmp_frag_item *current;

    last_frag = (item->frag_last ? item->frag_num : 0);
    current = iph1->frag_chain;
    while (current) {
        if (current->frag_num == item->frag_num) {  // duplicate?
            vfree(item->frag_packet);
            racoon_free(item);
            return 0;   // already have it
        }
        if (current->frag_last)
            last_frag = current->frag_num;
        current = current->frag_next;
    }
    /* no dup - insert it */
    item->frag_next = iph1->frag_chain;
    iph1->frag_chain = item;

    /* Check if the chain is complete   */
    if (last_frag == 0)
        return 0;       /* if last_frag not found - chain is not complete */
    for (i = 1; i <= last_frag; i++) {
        current = iph1->frag_chain;
        while (current) {
            if (current->frag_num == i)
                break;
            current = current->frag_next;
        };
        if (!current)
            return 0;   /* chain not complete */
    }

	plog(ASL_LEVEL_DEBUG, 
		 "%s: processed fragment %d\n", __FUNCTION__, frag->index);
	return 1;   /* chain is complete */
}

vchar_t *
isakmp_frag_reassembly(iph1)
	phase1_handle_t *iph1;
{
	struct isakmp_frag_item *item;
	size_t len = 0;
	vchar_t *buf = NULL;
	int frag_count = 0, frag_max = 0;
	int i;
	char *data;

	if ((item = iph1->frag_chain) == NULL) {
		plog(ASL_LEVEL_ERR, "No fragment to reassemble\n");
		goto out;
	}

	do {
		frag_count++;
		if (item->frag_num > frag_max && item->frag_last) {
			frag_max = item->frag_num;
		}
		len += item->frag_packet->l;
		item = item->frag_next;
	} while (item != NULL);
	
	if ((buf = vmalloc(len)) == NULL) {
		plog(ASL_LEVEL_ERR, "Cannot allocate memory\n");
		goto out;
	}
	data = buf->v;

	for (i = 1; i <= frag_max; i++) {
		item = iph1->frag_chain;
		do {
			if (item->frag_num == i)
				break;
			item = item->frag_next;
		} while (item != NULL);

		if (item == NULL) {
			plog(ASL_LEVEL_ERR, 
			    "Missing fragment #%d\n", i);
			vfree(buf);
			buf = NULL;
			return buf;
        }
		memcpy(data, item->frag_packet->v, item->frag_packet->l);
		data += item->frag_packet->l;
	}

	plog(ASL_LEVEL_DEBUG, 
		 "%s: processed %d fragments\n", __FUNCTION__, frag_count);

out:
	item = iph1->frag_chain;		
	
	while (item != NULL) {
		struct isakmp_frag_item *next_item;

		next_item = item->frag_next;

		vfree(item->frag_packet);
		racoon_free(item);

		item = next_item;
	} 

	iph1->frag_chain = NULL;

    //plogdump(ASL_LEVEL_DEBUG, buf->v, buf->l, "re-assembled fragements:\n");
	return buf;
}

vchar_t *
isakmp_frag_addcap(buf, cap)
	vchar_t *buf;
	int cap;
{
	int val, *capp;
	size_t len;
	int hashlen_bytes = eay_md5_hashlen() >> 3;

	/* If the capability has not been added, add room now */
	len = buf->l;
	if (len == hashlen_bytes) {
		if ((buf = vrealloc(buf, len + sizeof(cap))) == NULL) {
			plog(ASL_LEVEL_ERR, 
			    "Cannot allocate memory\n");
			return NULL;
		}
        val = 0;                                    
        memcpy(buf->v + len, &val, sizeof(val));        // Wcast_lign fix - copy instead of assign for unaligned move
    }
    capp = (int *)(void*)(buf->v + hashlen_bytes);      // Wcast_lign fix - copy instead of assign for unaligned move
    memcpy(&val, capp, sizeof(val));
    val |= htonl(cap);
    memcpy(capp, &val, sizeof(val));
           
	return buf;
}

int
sendfragsfromto(s, buf, local, remote, count_persend, frag_flags) 
	int              s;
	vchar_t         *buf;
	struct sockaddr_storage *local;
	struct sockaddr_storage *remote;
	int              count_persend;
	u_int32_t        frag_flags;
{
	struct isakmp *main_hdr;
	struct isakmp *hdr;
	struct isakmp_frag *fraghdr;
	caddr_t data;
	caddr_t sdata;
	size_t datalen;
	size_t max_datalen;
	size_t fraglen;
	vchar_t *frag;
	unsigned int fragnum = 0;
	size_t len;
#ifdef ENABLE_NATT
	size_t extralen = (frag_flags & FRAG_PUT_NON_ESP_MARKER)? NON_ESP_MARKER_LEN : 0;
#else
	size_t extralen = 0;
#endif
	
	/*
	 * fragmented packet must have the same exchange type (amongst other fields in the header).
	 */
	main_hdr = (struct isakmp *)buf->v;

	/*
	 * We want to send a a packet smaller than ISAKMP_FRAG_MAXLEN
	 * First compute the maximum data length that will fit in it
	 */
	max_datalen = ISAKMP_FRAG_MAXLEN - 
	(sizeof(*main_hdr) + sizeof(*fraghdr));

	sdata = buf->v;
	len = buf->l;
	
	while (len > 0) {
		fragnum++;

		if (len > max_datalen)
			datalen = max_datalen;
		else
			datalen = len;

		fraglen = sizeof(*hdr) + sizeof(*fraghdr) + datalen;

		if ((frag = vmalloc(fraglen)) == NULL) {
			plog(ASL_LEVEL_ERR, 
				 "Cannot allocate memory\n");
			return -1;
		}

		hdr = (struct isakmp *)frag->v;
		bcopy(main_hdr, hdr, sizeof(*hdr));
		hdr->len = htonl(frag->l);
		hdr->np = ISAKMP_NPTYPE_FRAG;

		fraghdr = (struct isakmp_frag *)(hdr + 1);
		fraghdr->unknown0 = 0;
		fraghdr->len = htons(fraglen - sizeof(*hdr));
		fraghdr->unknown1 = htons(1);
		fraghdr->index = fragnum;
		if (len == datalen)
			fraghdr->flags = ISAKMP_FRAG_LAST;
		else
			fraghdr->flags = 0;

		data = (caddr_t)(fraghdr + 1);
		memcpy(data, sdata, datalen);

#ifdef ENABLE_NATT
		/* If NAT-T port floating is in use, 4 zero bytes (non-ESP marker) 
		 must added just before the packet itself. For this we must 
		 allocate a new buffer and release it at the end. */
		if (extralen) {
			vchar_t *vbuf;
			
			if ((vbuf = vmalloc(frag->l + extralen)) == NULL) {
				plog(ASL_LEVEL_ERR, 
					 "%s: vbuf allocation failed\n", __FUNCTION__);
				vfree(frag);
				return -1;
			}
			*ALIGNED_CAST(u_int32_t *)vbuf->v = 0; // non-esp marker
			memcpy(vbuf->v + extralen, frag->v, frag->l);
			vfree(frag);
			frag = vbuf;
		}
#endif

		if (sendfromto(s, frag->v, frag->l, local, remote, count_persend) == -1) {
			plog(ASL_LEVEL_ERR, "sendfromto failed\n");
			vfree(frag);
			return -1;
		}

		vfree(frag);

		len -= datalen;
		sdata += datalen;
	}

	plog(ASL_LEVEL_DEBUG, 
		 "%s: processed %d fragments\n", __FUNCTION__, fragnum);

	return fragnum;
}
