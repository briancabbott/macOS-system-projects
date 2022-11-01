/* $Id: localconf.h,v 1.9.2.3 2005/11/06 17:18:26 monas Exp $ */

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

#ifndef _LOCALCONF_H
#define _LOCALCONF_H

#if !(TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
#include <vproc.h>
#endif // !(TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
#include <dispatch/dispatch.h>
#if __has_include(<nw/private.h>)
#include <nw/private.h>
#else
#include <network/nat64.h>
#endif
#include "vmbuf.h"
#include "ipsec_doi.h"

/* local configuration */

#define LC_DEFAULT_CF	SYSCONFDIR "/racoon.conf"

#define LC_PATHTYPE_INCLUDE	0
#define LC_PATHTYPE_PSK		1
#define LC_PATHTYPE_CERT	2
#define LC_PATHTYPE_PIDFILE	3
#define LC_PATHTYPE_LOGFILE	4
#define LC_PATHTYPE_MAX		5

#define LC_DEFAULT_PAD_MAXSIZE		20
#define LC_DEFAULT_PAD_RANDOM		TRUE
#define LC_DEFAULT_PAD_RANDOMLEN	FALSE
#define LC_DEFAULT_PAD_STRICT		FALSE
#define LC_DEFAULT_PAD_EXCLTAIL		TRUE
#define LC_DEFAULT_RETRY_COUNTER	5
#define LC_DEFAULT_RETRY_INTERVAL	2
#define LC_DEFAULT_COUNT_PERSEND	1
#define LC_DEFAULT_RETRY_CHECKPH1	30
#define LC_DEFAULT_WAIT_PH2COMPLETE	30
#define LC_DEFAULT_NATT_KA_INTERVAL	20

#define LC_DEFAULT_SECRETSIZE	16	/* 128 bits */

#define	LC_GSSENC_UTF16LE	0	/* GSS ID in UTF-16LE */
#define	LC_GSSENC_LATIN1	1	/* GSS ID in ISO-Latin-1 */
#define	LC_GSSENC_MAX		2

#define LC_AUTOEXITSTATE_SET		0x00000001
#define LC_AUTOEXITSTATE_CLIENT		0x00000010
#define LC_AUTOEXITSTATE_ENABLED	0x00000011	/* both VPN client and set */


struct vpnctl_socket_elem {
	LIST_ENTRY(vpnctl_socket_elem) chain;
	int                 sock;
    dispatch_source_t   source;
	uint32_t			read_bytes_len;
	uint32_t			pending_bytes_len;
	uint8_t				*buffer;
	LIST_HEAD(_bound_addrs, bound_addr) bound_addresses;
};

struct bound_addr {
	LIST_ENTRY(bound_addr) chain;
	u_int32_t	address;
	nw_nat64_prefix_t nat64_prefix;
	vchar_t		*user_id;
	vchar_t		*user_pw;
	vchar_t		*version;	/* our version string - if present */
};

struct redirect {
	LIST_ENTRY(redirect) chain;
	u_int32_t	cluster_address;
	u_int32_t	redirect_address;
	u_int16_t	force;
};

struct saved_msg_elem {
	TAILQ_ENTRY(saved_msg_elem) chain;
	void* msg;
};


struct localconf {
	char *racoon_conf;		/* configuration filename */

	uid_t uid;
	gid_t gid;
	u_int16_t port_isakmp;		/* port for isakmp as default */
	u_int16_t port_isakmp_natt;	/* port for NAT-T use */
	u_int16_t port_admin;		/* port for admin */
	int default_af;			/* default address family */

	int sock_vpncontrol;
	int sock_pfkey;
	int rtsock;			/* routing socket */
    dispatch_source_t vpncontrol_source;
    dispatch_source_t pfkey_source;
    dispatch_source_t rt_source;

	LIST_HEAD(_vpnctl_socket_elem_, vpnctl_socket_elem) vpnctl_comm_socks;
	LIST_HEAD(_redirect_, redirect) redirect_addresses;
	int auto_exit_state;		/* auto exit state */
	int	auto_exit_delay;		/* auto exit delay until exit */
	schedule_ref auto_exit_sched;	/* auto exit schedule */
	
	TAILQ_HEAD(_saved_msg_elem, saved_msg_elem) saved_msg_queue;
	int autograbaddr;
	struct myaddrs *myaddrs;

	char *logfile_param;		/* from command line */
	char *pathinfo[LC_PATHTYPE_MAX];
	vchar_t *ident[IDTYPE_MAX]; /* base of Identifier payload. */

	int pad_random;
	int pad_randomlen;
	int pad_maxsize;
	int pad_strict;
	int pad_excltail;

	int retry_counter;		/* times to retry. */
	int retry_interval;		/* interval each retry. */
	int count_persend;		/* the number of packets each retry. */
				/* above 3 values are copied into a handler. */

	int retry_checkph1;
	int wait_ph2complete;

	int natt_ka_interval;		/* NAT-T keepalive interval. */
	vchar_t *ext_nat_id;		/* our address id for our nat address */

	int secret_size;
	int strict_address;		/* strictly check addresses. */

	int complex_bundle;
		/*
		 * If we want to make a packet "IP2 AH ESP IP1 ULP",
		 * the SPD in KAME expresses AH transport + ESP tunnel.
		 * So racoon sent the proposal contained such the order.
		 * But lots of implementation interprets AH tunnel + ESP
		 * tunnel in this case.  racoon has changed the format,
		 * usually uses this format.  If the option, 'complex_bundle'
		 * is enable, racoon uses old format.
		 */

#if !(TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
	vproc_transaction_t vt;	/* returned by vproc_transaction_begin */
#endif // !(TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
};


extern struct localconf *lcconf;

extern void initlcconf(void);
extern void flushlcconf(void);
extern void savelcconf(void);
extern void restorelcconf(void);
extern vchar_t *getpskbyname(vchar_t *);
extern vchar_t *getpskbyaddr(struct sockaddr_storage *);
#if HAVE_KEYCHAIN
extern vchar_t *getpskfromkeychain(const char *, u_int8_t, int, vchar_t *);
#endif
extern void getpathname(char *, int, int, const char *);
extern int sittype2doi(int);
extern int doitype2doi(int);
extern vchar_t *getpsk(const char *, const int); 


#endif /* _LOCALCONF_H */
