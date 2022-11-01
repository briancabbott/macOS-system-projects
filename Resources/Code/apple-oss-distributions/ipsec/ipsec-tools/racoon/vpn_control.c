/* $Id: vpn_control.c,v 1.17.2.4 2005/07/12 11:49:44 manubsd Exp $ */

/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/un.h>

#include <net/pfkeyv2.h>

#include <netinet/in.h>
#ifndef HAVE_NETINET6_IPSEC
#include <netinet/ipsec.h>
#else
#include <netinet6/ipsec.h>
#endif


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <launch.h>
#include <fcntl.h>

#include "var.h"
#include "misc.h"
#include "vmbuf.h"
#include "plog.h"
#include "sockmisc.h"
#include "debug.h"

#include "schedule.h"
#include "localconf.h"
#include "remoteconf.h"
#include "grabmyaddr.h"
#include "isakmp_var.h"
#include "isakmp.h"
#include "oakley.h"
#include "handler.h"
#include "pfkey.h"
#include "ipsec_doi.h"
#include "vpn_control.h"
#include "vpn_control_var.h"
#include "isakmp_inf.h"
#include "session.h"
#include "gcmalloc.h"
#include "isakmp_cfg.h"
#include "sainfo.h"

#ifdef ENABLE_VPNCONTROL_PORT
char *vpncontrolsock_path = VPNCONTROLSOCK_PATH;
uid_t vpncontrolsock_owner = 0;
gid_t vpncontrolsock_group = 0;
mode_t vpncontrolsock_mode = 0600;

static struct sockaddr_un sunaddr;
static int vpncontrol_process (struct vpnctl_socket_elem *, char *, size_t);
static int vpncontrol_reply (int, char *);
static void vpncontrol_close_comm (struct vpnctl_socket_elem *);
static int checklaunchd (void);
extern int vpn_get_config (phase1_handle_t *, struct vpnctl_status_phase_change **, size_t *);
extern int vpn_xauth_reply (u_int32_t, void *, size_t);


int
checklaunchd()
{
	int returnval = 0;
	int *listening_fd_array = NULL;
	size_t fd_count = 0;

	int result = launch_activate_socket("Listeners", &listening_fd_array, &fd_count);
	if (result != 0) {
		plog(ASL_LEVEL_ERR, "failed to launch_activate_socket with error %s.\n", strerror(result));
		return returnval;
	}

	if (listening_fd_array != NULL) {
		if (fd_count > 0) {
			returnval = listening_fd_array[0];
		}
		free(listening_fd_array);
		listening_fd_array = NULL;
	}

	return returnval;
}

		
void
vpncontrol_handler(void *unused)
{
	struct sockaddr_storage from;
	socklen_t fromlen = sizeof(from);
    int sock;

	struct vpnctl_socket_elem *sock_elem;

	
	sock_elem = racoon_calloc(1, sizeof(struct vpnctl_socket_elem));
	if (sock_elem == NULL) {
		plog(ASL_LEVEL_ERR,
			"memory error: %s\n", strerror(errno));
		return; //%%%%%% terminate
	}
	LIST_INIT(&sock_elem->bound_addresses);

	sock_elem->sock = accept(lcconf->sock_vpncontrol, (struct sockaddr *)&from, &fromlen);
	if (sock_elem->sock < 0) {
		plog(ASL_LEVEL_ERR,
			"failed to accept vpn_control command: %s\n", strerror(errno));
		racoon_free(sock_elem);
		return; //%%%%% terminate
	}
	LIST_INSERT_HEAD(&lcconf->vpnctl_comm_socks, sock_elem, chain);

    sock_elem->source = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, sock_elem->sock, 0, dispatch_get_main_queue());
    if (sock_elem->source == NULL) {
		plog(ASL_LEVEL_ERR, "could not create comm socket source.");
		racoon_free(sock_elem);
		return; //%%%%% terminate
    }
    dispatch_source_set_event_handler(sock_elem->source,
                                        ^{
                                                vpncontrol_comm_handler(sock_elem);
                                        });
    sock = sock_elem->sock;
	
    dispatch_source_t the_source = sock_elem->source;
    dispatch_source_set_cancel_handler(sock_elem->source,
                                       ^{
                                           close(sock);
                                           dispatch_release(the_source); /* Release the source on cancel */
                                       });
    dispatch_resume(sock_elem->source);

	plog(ASL_LEVEL_NOTICE,
		"accepted connection on vpn control socket.\n");		
	check_auto_exit();
		
	return;
}

void
vpncontrol_comm_handler(struct vpnctl_socket_elem *elem)
{
	struct vpnctl_hdr hdr;
	ssize_t len;

	/* get buffer length */
	if (elem->buffer == NULL) {
		while ((len = recv(elem->sock, (char *)&hdr, sizeof(hdr), MSG_PEEK)) < 0) {
			if (errno == EINTR)
				continue;
			plog(ASL_LEVEL_ERR, "failed to recv vpn_control command: %s\n", strerror(errno));
			return;
		}
		if (len == 0) {
			plog(ASL_LEVEL_NOTICE, "vpn_control socket closed by peer.\n");
			/* kill all related connections */
			vpncontrol_disconnect_all(elem, ike_session_stopped_by_controller_comm_lost);
			vpncontrol_close_comm(elem);
			return; // %%%%%% terminate
		}

		/* sanity check */
		if (len < sizeof(hdr)) {
			plog(ASL_LEVEL_ERR,
				 "invalid header length of vpn_control command - len=%ld - expected %ld\n", len, sizeof(hdr));
			return;
		}

		elem->read_bytes_len = 0; // Sanity
		elem->pending_bytes_len = ntohs(hdr.len) + sizeof(hdr);

		/* get buffer to receive */
		elem->buffer = racoon_malloc(elem->pending_bytes_len);
		if (elem->buffer == NULL) {
			plog(ASL_LEVEL_ERR,
				 "failed to alloc buffer for vpn_control command\n");
			return;
		}
	}

	/* get real data */
	while ((len = recv(elem->sock, elem->buffer + elem->read_bytes_len, elem->pending_bytes_len, 0)) < 0) {
		if (errno == EINTR)
			continue;
		plog(ASL_LEVEL_ERR, "failed to recv vpn_control command: %s\n",
			 strerror(errno));
		return;
	}

	if (len == 0) {
		plog(ASL_LEVEL_NOTICE, "vpn_control socket closed by peer while reading packet\n");
		/* kill all related connections */
		vpncontrol_disconnect_all(elem, ike_session_stopped_by_controller_comm_lost);
		vpncontrol_close_comm(elem);
		return;
	}

	elem->read_bytes_len += len;

	if (len < elem->pending_bytes_len) {
		plog(ASL_LEVEL_NOTICE,
			 "received partial vpn_control command - len=%ld - expected %u\n", len, elem->pending_bytes_len);
		elem->pending_bytes_len -= len;
		return;
	} else {
		(void)vpncontrol_process(elem, elem->buffer, elem->read_bytes_len);
		free(elem->buffer);
		elem->buffer = NULL;
		elem->read_bytes_len = 0;
		elem->pending_bytes_len = 0;
	}
}

static int
vpncontrol_process(struct vpnctl_socket_elem *elem, char *combuf, size_t combuf_len)
{
	u_int16_t	error = 0;
	struct vpnctl_hdr *hdr = ALIGNED_CAST(struct vpnctl_hdr *)combuf;

	switch (ntohs(hdr->msg_type)) {
	
		case VPNCTL_CMD_BIND:
			{
				if (combuf_len < sizeof(struct vpnctl_cmd_bind)) {
					plog(ASL_LEVEL_ERR, "invalid header length for vpnctl bind cmd - len=%ld - expected %ld\n", combuf_len, sizeof(struct vpnctl_cmd_bind));
					error = -1;
					break;
				}

				struct vpnctl_cmd_bind *pkt = ALIGNED_CAST(struct vpnctl_cmd_bind *)combuf;
				struct bound_addr *addr;

				if (combuf_len < (sizeof(struct vpnctl_cmd_bind) + ntohs(pkt->vers_len))) {
					plog(ASL_LEVEL_ERR, "invalid length for vpnctl bind cmd - len=%ld - expected %ld\n", combuf_len, (sizeof(struct vpnctl_cmd_bind) + ntohs(pkt->vers_len)));
					error = -1;
					break;
				}
			
				plog(ASL_LEVEL_NOTICE,
					"received bind command on vpn control socket.\n");
				addr = racoon_calloc(1, sizeof(struct bound_addr));
				if (addr == NULL) {
					plog(ASL_LEVEL_ERR, 	
						"memory error: %s\n", strerror(errno));
					error = -1;
					break;
				}
				if (ntohs(pkt->vers_len)) {
					addr->version = vmalloc(ntohs(pkt->vers_len));
					if (addr->version == NULL) {
						plog(ASL_LEVEL_ERR, 	
							"memory error: %s\n", strerror(errno));
						error = -1;
						racoon_free(addr);
						break;
					}
					memcpy(addr->version->v, pkt + 1, ntohs(pkt->vers_len));
				}
				addr->address = pkt->address;
				LIST_INSERT_HEAD(&elem->bound_addresses, addr, chain);
				lcconf->auto_exit_state |= LC_AUTOEXITSTATE_CLIENT;	/* client side */
			}
			break;
			
		case VPNCTL_CMD_UNBIND:
			{
				if (combuf_len < sizeof(struct vpnctl_cmd_unbind)) {
					plog(ASL_LEVEL_ERR, "invalid header length for vpnctl unbind cmd - len=%ld - expected %ld\n", combuf_len, sizeof(struct vpnctl_cmd_unbind));
					error = -1;
					break;
				}

				struct vpnctl_cmd_unbind *pkt = ALIGNED_CAST(struct vpnctl_cmd_unbind *)combuf;
				struct bound_addr *addr;
				struct bound_addr *t_addr;

				plog(ASL_LEVEL_NOTICE,
					"received unbind command on vpn control socket.\n");
				LIST_FOREACH_SAFE(addr, &elem->bound_addresses, chain, t_addr) {
					if (pkt->address == 0xFFFFFFFF ||
						pkt->address == addr->address) {
						flushsainfo_dynamic(addr->address);
						LIST_REMOVE(addr, chain);
						if (addr->version)
							vfree(addr->version);
						racoon_free(addr);
					}
				}
			}
			break;

		case VPNCTL_CMD_REDIRECT:
			{
				if (combuf_len < sizeof(struct vpnctl_cmd_redirect)) {
					plog(ASL_LEVEL_ERR, "invalid header length for vpnctl redirect cmd - len=%ld - expected %ld\n", combuf_len, sizeof(struct vpnctl_cmd_redirect));
					error = -1;
					break;
				}

				struct vpnctl_cmd_redirect *redirect_msg = ALIGNED_CAST(struct vpnctl_cmd_redirect *)combuf;
				struct redirect *raddr;
				struct redirect *t_raddr;
				int found = 0;
				
				plog(ASL_LEVEL_NOTICE,
					"received redirect command on vpn control socket - address = %x.\n", ntohl(redirect_msg->redirect_address));
				
				LIST_FOREACH_SAFE(raddr, &lcconf->redirect_addresses, chain, t_raddr) {
					if (raddr->cluster_address == redirect_msg->address) {
						if (redirect_msg->redirect_address == 0) {
							LIST_REMOVE(raddr, chain);
							racoon_free(raddr);
						} else {
							raddr->redirect_address = redirect_msg->redirect_address;
							raddr->force = ntohs(redirect_msg->force);
						}
						found = 1;
						break;
					}
				}
				if (!found) {
					raddr = racoon_malloc(sizeof(struct redirect));
					if (raddr == NULL) {
						plog(ASL_LEVEL_ERR,
							"cannot allcoate memory for redirect address.\n");					
						error = -1;
						break;
					}
					raddr->cluster_address = redirect_msg->address;
					raddr->redirect_address = redirect_msg->redirect_address;
					raddr->force = ntohs(redirect_msg->force);
					LIST_INSERT_HEAD(&lcconf->redirect_addresses, raddr, chain);
					
				}
			}
			break;
			
		case VPNCTL_CMD_PING:
			break;	/* just reply for now */

		case VPNCTL_CMD_XAUTH_INFO:
			{
				if (combuf_len < sizeof(struct vpnctl_cmd_xauth_info)) {
					plog(ASL_LEVEL_ERR, "invalid header length for vpnctl xauth info cmd - len=%ld - expected %ld\n", combuf_len, sizeof(struct vpnctl_cmd_xauth_info));
					error = -1;
					break;
				}

				struct vpnctl_cmd_xauth_info *pkt = ALIGNED_CAST(struct vpnctl_cmd_xauth_info *)combuf;
				struct bound_addr *addr;
				struct bound_addr *t_addr;
				void *attr_list;

				if (combuf_len < (sizeof(struct vpnctl_cmd_xauth_info) + ntohs(pkt->hdr.len) - sizeof(u_int32_t))) {
					plog(ASL_LEVEL_ERR, "invalid length for vpnctl xauth info cmd - len=%ld - expected %ld\n", combuf_len, (sizeof(struct vpnctl_cmd_xauth_info) + ntohs(pkt->hdr.len) - sizeof(u_int32_t)));
					error = -1;
					break;
				}

				plog(ASL_LEVEL_NOTICE,
					"received xauth info command vpn control socket.\n");
				LIST_FOREACH_SAFE(addr, &elem->bound_addresses, chain, t_addr) {
					if (pkt->address == addr->address) {
						/* reply to the last xauth request */
						attr_list = pkt + 1;
						error = vpn_xauth_reply(pkt->address, attr_list, ntohs(pkt->hdr.len) - sizeof(u_int32_t));
						break;
					}
				}
			}
			break;

		case VPNCTL_CMD_SET_NAT64_PREFIX:
			{
				if (combuf_len < sizeof(struct vpnctl_cmd_set_nat64_prefix)) {
					plog(ASL_LEVEL_ERR, "invalid header length for vpnctl nat64 prefix cmd - len=%ld - expected %ld\n", combuf_len, sizeof(struct vpnctl_cmd_set_nat64_prefix));
					error = -1;
					break;
				}

				struct vpnctl_cmd_set_nat64_prefix *pkt = ALIGNED_CAST(struct vpnctl_cmd_set_nat64_prefix *)combuf;
				struct bound_addr *addr;
				struct bound_addr *t_addr;

				plog(ASL_LEVEL_NOTICE,
						"received set v6 prefix of len %u command on vpn control socket, adding to all addresses.\n", pkt->nat64_prefix.length);
				LIST_FOREACH_SAFE(addr, &elem->bound_addresses, chain, t_addr) {
					memcpy(&addr->nat64_prefix, &pkt->nat64_prefix, sizeof(addr->nat64_prefix));
				}
			}
			break;

		case VPNCTL_CMD_CONNECT:
			{
				if (combuf_len < sizeof(struct vpnctl_cmd_connect)) {
					plog(ASL_LEVEL_ERR, "invalid header length for vpnctl connect cmd - len=%ld - expected %ld\n", combuf_len, sizeof(struct vpnctl_cmd_connect));
					error = -1;
					break;
				}

				struct vpnctl_cmd_connect *pkt = ALIGNED_CAST(struct vpnctl_cmd_connect *)combuf;
				struct bound_addr *addr;
				struct bound_addr *t_addr;

				if (pending_signal_handle) {
					/*
					 * This check is done to ensure that a SIGUSR1 signal to re-read the configuration file
					 * is completed before calling a connect. This is to fix the issue seen in (rdar://problem/25641686)
					 */
					check_sigreq();
					pending_signal_handle = 0;
				}

				plog(ASL_LEVEL_NOTICE,
					"received connect command on vpn control socket.\n");
				LIST_FOREACH_SAFE(addr, &elem->bound_addresses, chain, t_addr) {
					if (pkt->address == addr->address) {
						/* start the connection */
						error = vpn_connect(addr, VPN_STARTED_BY_API);
						break;
					}
				}
			}
			break;
			
		case VPNCTL_CMD_DISCONNECT:
			{
				if (combuf_len < sizeof(struct vpnctl_cmd_connect)) {
					plog(ASL_LEVEL_ERR, "invalid header length for vpnctl disconnect cmd - len=%ld - expected %ld\n", combuf_len, sizeof(struct vpnctl_cmd_connect));
					error = -1;
					break;
				}

				struct vpnctl_cmd_connect *pkt = ALIGNED_CAST(struct vpnctl_cmd_connect *)combuf;
				struct bound_addr *addr;
				struct bound_addr *t_addr;

				plog(ASL_LEVEL_NOTICE,
					"received disconnect command on vpn control socket.\n");
				LIST_FOREACH_SAFE(addr, &elem->bound_addresses, chain, t_addr) {
					if (pkt->address == addr->address) {
						/* stop the connection */
						error = vpn_disconnect(addr, ike_session_stopped_by_vpn_disconnect);
						break;
					}
				}
			}
			break;
			
		case VPNCTL_CMD_START_PH2:
			{
				if (combuf_len < sizeof(struct vpnctl_cmd_start_ph2)) {
					plog(ASL_LEVEL_ERR, "invalid header length for vpnctl start ph2 cmd - len=%ld - expected %ld\n", combuf_len, sizeof(struct vpnctl_cmd_start_ph2));
					error = -1;
					break;
				}

				struct vpnctl_cmd_start_ph2 *pkt = ALIGNED_CAST(struct vpnctl_cmd_start_ph2 *)combuf;
				struct bound_addr *addr;
				struct bound_addr *t_addr;

				plog(ASL_LEVEL_NOTICE, "received start_ph2 command on vpn control socket.\n");
				LIST_FOREACH_SAFE(addr, &elem->bound_addresses, chain, t_addr) {
					if (pkt->address == addr->address) {
						/* start the connection */
						error = vpn_start_ph2(addr, pkt, combuf_len);
						break;
					}
				}
			}
			break;

		case VPNCTL_CMD_START_DPD:
            {
				if (combuf_len < sizeof(struct vpnctl_cmd_start_dpd)) {
					plog(ASL_LEVEL_ERR, "invalid header length for vpnctl start dpd cmd - len=%ld - expected %ld\n", combuf_len, sizeof(struct vpnctl_cmd_start_dpd));
					error = -1;
					break;
				}

                struct vpnctl_cmd_start_dpd *pkt = ALIGNED_CAST(struct vpnctl_cmd_start_dpd *)combuf;
                struct bound_addr *srv;
                struct bound_addr *t_addr;

                plog(ASL_LEVEL_NOTICE,
                     "received start_dpd command on vpn control socket.\n");
                LIST_FOREACH_SAFE(srv, &elem->bound_addresses, chain, t_addr) {
                    if (pkt->address == srv->address) {
                        union {                             // Wcast-align fix - force alignment
                            struct sockaddr_storage ss;
                            struct sockaddr_in	addr_in;
                        } daddr;

                        bzero(&daddr, sizeof(struct sockaddr_in));
                        daddr.addr_in.sin_len = sizeof(struct sockaddr_in);
                        daddr.addr_in.sin_addr.s_addr = srv->address;
                        daddr.addr_in.sin_port = 0;
                        daddr.addr_in.sin_family = AF_INET;

                        /* start the dpd */
                        error = ike_session_ph1_force_dpd(&daddr.ss);
                        break;
                    }
                }
            }
			break;

		case VPNCTL_CMD_ASSERT:
			{
				if (combuf_len < sizeof(struct vpnctl_cmd_assert)) {
					plog(ASL_LEVEL_ERR, "invalid header length for vpnctl assert cmd - len=%ld - expected %ld\n", combuf_len, sizeof(struct vpnctl_cmd_assert));
					error = -1;
					break;
				}

				struct vpnctl_cmd_assert *pkt = ALIGNED_CAST(struct vpnctl_cmd_assert *)combuf;
//				struct bound_addr *addr;
//				struct bound_addr *t_addr;
				struct sockaddr_in saddr;
				struct sockaddr_in daddr;

				plogdump(ASL_LEVEL_NOTICE, pkt, ntohs(hdr->len) + sizeof(struct vpnctl_hdr), "received assert command on vpn control socket.\n");
//				LIST_FOREACH_SAFE(addr, &elem->bound_addresses, chain, t_addr) {
//					if (pkt->dst_address == addr->address) {
						bzero(&saddr, sizeof(saddr));
						saddr.sin_len = sizeof(saddr);
						saddr.sin_addr.s_addr = pkt->src_address;
						saddr.sin_port = 0;
						saddr.sin_family = AF_INET;
						bzero(&daddr, sizeof(daddr));
						daddr.sin_len = sizeof(daddr);
						daddr.sin_addr.s_addr = pkt->dst_address;
						daddr.sin_port = 0;
						daddr.sin_family = AF_INET;

						error = vpn_assert(ALIGNED_CAST(struct sockaddr_storage *)&saddr, ALIGNED_CAST(struct sockaddr_storage *)&daddr);
						break;
//					}
//				}
			}
			break;

		case VPNCTL_CMD_RECONNECT:
			{
				if (combuf_len < sizeof(struct vpnctl_cmd_connect)) {
					plog(ASL_LEVEL_ERR, "invalid header length for vpnctl reconnect cmd - len=%ld - expected %ld\n", combuf_len, sizeof(struct vpnctl_cmd_connect));
					error = -1;
					break;
				}

				struct vpnctl_cmd_connect *pkt = ALIGNED_CAST(struct vpnctl_cmd_connect *)combuf;
				struct bound_addr *addr;
				struct bound_addr *t_addr;

				plog(ASL_LEVEL_NOTICE,
					 "received reconnect command on vpn control socket.\n");
				LIST_FOREACH_SAFE(addr, &elem->bound_addresses, chain, t_addr) {
					if (pkt->address == addr->address) {
						/* start the connection */
						error = vpn_connect(addr, VPN_RESTARTED_BY_API);
						break;
					}
				}
			}
			break;

		default:
			plog(ASL_LEVEL_ERR,
				"invalid command: %d\n", ntohs(hdr->msg_type));
			error = -1;		// for now
			break;
	}

	hdr->len = 0;
	hdr->result = htons(error);
	if (vpncontrol_reply(elem->sock, combuf) < 0)
		return -1;

	return 0;

}

static int
vpncontrol_reply(int so, char *combuf)
{
	ssize_t tlen;

	tlen = send(so, combuf, sizeof(struct vpnctl_hdr), 0);
	if (tlen < 0) {
		plog(ASL_LEVEL_ERR,
			"failed to send vpn_control message: %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

bool
vpncontrol_set_nat64_prefix(nw_nat64_prefix_t *prefix)
{
	struct vpnctl_socket_elem *sock_elem;
	struct bound_addr *bound_addr;

	LIST_FOREACH(sock_elem, &lcconf->vpnctl_comm_socks, chain) {
		LIST_FOREACH(bound_addr, &sock_elem->bound_addresses, chain) {
			if (bound_addr->nat64_prefix.length != 0) {
				memcpy(prefix, &bound_addr->nat64_prefix, sizeof(*prefix));
				return true;
			}
		}
	}
	return false;
}

int
vpncontrol_notify_need_authinfo(phase1_handle_t *iph1, void* attr_list, size_t attr_len)
{
	struct vpnctl_status_need_authinfo *msg = NULL;
	struct vpnctl_socket_elem *sock_elem;
	struct bound_addr *bound_addr;
	size_t msg_size;
	ssize_t tlen;
	u_int32_t address;
	void *ptr;
	
	if (!iph1)
		goto end;

	plog(ASL_LEVEL_NOTICE,
		"sending vpn_control xauth need info status\n");

	msg = (struct vpnctl_status_need_authinfo *)racoon_malloc(msg_size = sizeof(struct vpnctl_status_need_authinfo) + attr_len);
	if (msg == NULL) {
		plog(ASL_LEVEL_ERR,
			"unable to allocate space for vpn control message.\n");
		return -1;
	}
	msg->hdr.flags = 0;

	address = iph1_get_remote_v4_address(iph1);
	if (address == 0) {
		goto end;
	}

	msg->hdr.cookie = msg->hdr.reserved = msg->hdr.result = 0;
	msg->hdr.len = htons((msg_size) - sizeof(struct vpnctl_hdr));	
	if (!ike_session_is_client_ph1_rekey(iph1)) {
		msg->hdr.msg_type = htons(VPNCTL_STATUS_NEED_AUTHINFO);
	} else {
		msg->hdr.msg_type = htons(VPNCTL_STATUS_NEED_REAUTHINFO);
	}
	msg->address = iph1_get_remote_v4_address(iph1);
	ptr = msg + 1;
	memcpy(ptr, attr_list, attr_len);

	LIST_FOREACH(sock_elem, &lcconf->vpnctl_comm_socks, chain) {
		LIST_FOREACH(bound_addr, &sock_elem->bound_addresses, chain) {
			if (bound_addr->address == 0xFFFFFFFF ||
				bound_addr->address == address) {
				plog(ASL_LEVEL_DEBUG, "vpn control writing %zu bytes\n", msg_size);
				tlen = send(sock_elem->sock, msg, msg_size, 0);
				if (tlen < 0) {
					plog(ASL_LEVEL_ERR,
						"failed to send vpn_control need authinfo status: %s\n", strerror(errno));
				}
				break;
			}
		}
	}

end:
	if (msg)
		racoon_free(msg);
	return 0;
}

int
vpncontrol_notify_ike_failed(u_int16_t notify_code, u_int16_t from, u_int32_t address, u_int16_t data_len, u_int8_t *data)
{
	struct vpnctl_status_failed *msg = NULL;
	struct vpnctl_socket_elem *sock_elem;
	struct bound_addr *bound_addr;
	size_t len;
    ssize_t tlen;
	
	len = sizeof(struct vpnctl_status_failed) + data_len;
	
	msg = (struct vpnctl_status_failed *)racoon_malloc(len);
	if (msg == NULL) {
		plog(ASL_LEVEL_ERR,
				"unable to allcate memory for vpn control status message.\n");
		return -1;
	}
		
	msg->hdr.msg_type = htons(VPNCTL_STATUS_IKE_FAILED);
	msg->hdr.flags = msg->hdr.cookie = msg->hdr.reserved = msg->hdr.result = 0;
	msg->hdr.len = htons(len - sizeof(struct vpnctl_hdr));
	msg->address = address;
	msg->ike_code = htons(notify_code);
	msg->from = htons(from);
	if (data_len > 0)
		memcpy(msg->data, data, data_len);	
	plog(ASL_LEVEL_ERR,
			"sending vpn_control ike failed message - code=%d  from=%s.\n", notify_code,
					(from == FROM_LOCAL ? "local" : "remote"));

	LIST_FOREACH(sock_elem, &lcconf->vpnctl_comm_socks, chain) {
		LIST_FOREACH(bound_addr, &sock_elem->bound_addresses, chain) {
			if (bound_addr->address == 0xFFFFFFFF ||
				bound_addr->address == address) {
				tlen = send(sock_elem->sock, msg, len, 0);
				if (tlen < 0) {
					plog(ASL_LEVEL_ERR,
						"Unable to send vpn_control ike notify failed: %s\n", strerror(errno));
				} else {
					plog(ASL_LEVEL_DEBUG,
						 "Sent %zd/%zu bytes\n", tlen, len);
				}
				break;
			}
		}
	}

	if (msg)
		racoon_free(msg);
	return 0;
}

char *
vpncontrol_status_2_str(u_int16_t msg_type)
{
    switch (msg_type) {
        case VPNCTL_STATUS_IKE_FAILED:
            return "IKE failed";
        case VPNCTL_STATUS_PH1_START_US:
            return "Phase 1 started by us";
        case VPNCTL_STATUS_PH1_START_PEER:
            return "Phase 1 started by peer";
        case VPNCTL_STATUS_PH1_ESTABLISHED:
            return "Phase 1 established";
        case VPNCTL_STATUS_PH2_START:
            return "Phase 2 started";
        case VPNCTL_STATUS_PH2_ESTABLISHED:
            return "Phase 2 established";
        case VPNCTL_STATUS_NEED_AUTHINFO:
            return "Need authentication info";
        case VPNCTL_STATUS_NEED_REAUTHINFO:
            return "Need re-authentication info";
        default:
            return "";
    }
}


int
vpncontrol_notify_phase_change(int start, u_int16_t from, phase1_handle_t *iph1, phase2_handle_t *iph2)
{
	struct vpnctl_status_phase_change *msg;
	struct vpnctl_socket_elem *sock_elem;
	struct bound_addr *bound_addr;
	ssize_t tlen;
	size_t msg_size;	
	u_int32_t address;
	
    if (iph1 && !start && iph1->mode_cfg && iph1->mode_cfg->xauth.status != XAUTHST_OK) {
		if (vpn_get_config(iph1, &msg, &msg_size) == 1)
			return 0;	/* mode config not finished yet */
	} else {
		msg = racoon_malloc(msg_size = sizeof(struct vpnctl_status_phase_change));
		msg->hdr.flags = 0;
	}
		
	if (msg == NULL) {
		plog(ASL_LEVEL_ERR,
						"unable to allocate space for vpn control message.\n");
		return -1;
	}
	if (iph1) {
		address = iph1_get_remote_v4_address(iph1);
		if (address == 0) {
			plog(ASL_LEVEL_ERR, "bad address for ph1 status change.\n");
			goto end;
		}
		msg->hdr.msg_type = htons(start ?
			(from == FROM_LOCAL ? VPNCTL_STATUS_PH1_START_US : VPNCTL_STATUS_PH1_START_PEER)
			: VPNCTL_STATUS_PH1_ESTABLISHED);
		// TODO: indicate version
	} else {
		address = iph2_get_remote_v4_address(iph2);
		if (address == 0) {
			plog(ASL_LEVEL_ERR, "bad address for ph2 status change.\n");
			goto end;
		}
		msg->hdr.msg_type = htons(start ? VPNCTL_STATUS_PH2_START : VPNCTL_STATUS_PH2_ESTABLISHED);
		// TODO: indicate version
	}
    plog(ASL_LEVEL_NOTICE,
         ">>>>> phase change status = %s\n", vpncontrol_status_2_str(ntohs(msg->hdr.msg_type)));

	msg->hdr.cookie = msg->hdr.reserved = msg->hdr.result = 0;
	msg->hdr.len = htons((msg_size) - sizeof(struct vpnctl_hdr));
	msg->address = address;

	LIST_FOREACH(sock_elem, &lcconf->vpnctl_comm_socks, chain) {
		LIST_FOREACH(bound_addr, &sock_elem->bound_addresses, chain) {
			if (bound_addr->address == 0xFFFFFFFF ||
				bound_addr->address == address) {
				plog(ASL_LEVEL_DEBUG, "vpn control writing %zu bytes\n", msg_size);
				tlen = send(sock_elem->sock, msg, msg_size, 0);
				if (tlen < 0) {
					plog(ASL_LEVEL_ERR,
						"failed to send vpn_control phase change status: %s\n", strerror(errno));
				}
				break;
			}
		}
	}

end:
	if (msg)
		racoon_free(msg);
	return 0;
}

static int
vpncontrol_notify_peer_resp (u_int16_t notify_code, u_int32_t address)
{
	struct vpnctl_status_peer_resp msg;
	struct vpnctl_socket_elem *sock_elem;
	struct bound_addr *bound_addr;
	ssize_t tlen;
	int    rc = -1;

	bzero(&msg, sizeof(msg));
	msg.hdr.msg_type = htons(VPNCTL_STATUS_PEER_RESP);
	msg.hdr.cookie = msg.hdr.reserved = msg.hdr.result = 0;
	msg.hdr.len = htons(sizeof(msg) - sizeof(msg.hdr));
	msg.address = address;
	msg.ike_code = notify_code;
	plog(ASL_LEVEL_NOTICE,
		 "sending vpn_control status (peer response) message - code=%d  addr=%x.\n", notify_code, address);
	
	LIST_FOREACH(sock_elem, &lcconf->vpnctl_comm_socks, chain) {
		LIST_FOREACH(bound_addr, &sock_elem->bound_addresses, chain) {
			if (bound_addr->address == 0xFFFFFFFF ||
				bound_addr->address == address) {
				tlen = send(sock_elem->sock, &msg, sizeof(msg), 0);
				if (tlen < 0) {
					plog(ASL_LEVEL_ERR,
						 "unable to send vpn_control status (peer response): %s\n", strerror(errno));
				} else {
					rc = 0;
				}
				break;
			}
		}
	}

	return rc;
}

int
vpncontrol_notify_peer_resp_ph1 (u_int16_t notify_code, phase1_handle_t *iph1)
{
	if (iph1 && iph1->parent_session && iph1->parent_session->controller_awaiting_peer_resp) {
		int rc;
		if ((rc = vpncontrol_notify_peer_resp(notify_code, iph1_get_remote_v4_address(iph1))) == 0) {
			iph1->parent_session->controller_awaiting_peer_resp = 0;
		}
		return rc;
	} else {
		return 0;
	}
}
	
int
vpncontrol_notify_peer_resp_ph2 (u_int16_t notify_code, phase2_handle_t *iph2)
{
	if (iph2 && iph2->parent_session && iph2->parent_session->controller_awaiting_peer_resp) {
		int rc;
		if ((rc = vpncontrol_notify_peer_resp(notify_code, iph2_get_remote_v4_address(iph2))) == 0) {
			iph2->parent_session->controller_awaiting_peer_resp = 0;
		}
		return rc;
	} else {
		return 0;
	}
}

int
vpncontrol_init(void)
{
    int sock;

	if (vpncontrolsock_path == NULL) {
		lcconf->sock_vpncontrol = -1;
		return 0;
	}

	if ( (lcconf->sock_vpncontrol = checklaunchd()) == 0 ) {
		memset(&sunaddr, 0, sizeof(sunaddr));
		sunaddr.sun_family = AF_UNIX;
		snprintf(sunaddr.sun_path, sizeof(sunaddr.sun_path),
			"%s", vpncontrolsock_path);

		lcconf->sock_vpncontrol = socket(AF_UNIX, SOCK_STREAM, 0);
		if (lcconf->sock_vpncontrol == -1) {
			plog(ASL_LEVEL_ERR,
				"socket: %s\n", strerror(errno));
			return -1;
		}

		if (fcntl(lcconf->sock_vpncontrol, F_SETFL, O_NONBLOCK) == -1) {
			plog(ASL_LEVEL_ERR, "failed to put VPN-Control socket in non-blocking mode\n");
		}

		unlink(sunaddr.sun_path);
		if (bind(lcconf->sock_vpncontrol, (struct sockaddr *)&sunaddr,
				sizeof(sunaddr)) != 0) {
			plog(ASL_LEVEL_ERR,
				"bind(sockname:%s): %s\n",
				sunaddr.sun_path, strerror(errno));
			(void)close(lcconf->sock_vpncontrol);
			return -1;
		}

		if (chown(sunaddr.sun_path, vpncontrolsock_owner, vpncontrolsock_group) != 0) {
			plog(ASL_LEVEL_ERR,
				"chown(%s, %d, %d): %s\n",
				sunaddr.sun_path, vpncontrolsock_owner,
				vpncontrolsock_group, strerror(errno));
			(void)close(lcconf->sock_vpncontrol);
			return -1;
		}

		if (chmod(sunaddr.sun_path, vpncontrolsock_mode) != 0) {
			plog(ASL_LEVEL_ERR,
				"chmod(%s, 0%03o): %s\n",
				sunaddr.sun_path, vpncontrolsock_mode, strerror(errno));
			(void)close(lcconf->sock_vpncontrol);
			return -1;
		}

		if (listen(lcconf->sock_vpncontrol, 5) != 0) {
			plog(ASL_LEVEL_ERR,
				"listen(sockname:%s): %s\n",
				sunaddr.sun_path, strerror(errno));
			(void)close(lcconf->sock_vpncontrol);
			return -1;
		}
		plog(ASL_LEVEL_NOTICE,
			"opened %s as racoon management.\n", sunaddr.sun_path);
	}
    lcconf->vpncontrol_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, lcconf->sock_vpncontrol, 0, dispatch_get_main_queue());
    if (lcconf->vpncontrol_source == NULL) {
        plog(ASL_LEVEL_ERR, "could not create vpncontrol socket source.");
        return -1;
    }
    dispatch_source_set_event_handler_f(lcconf->vpncontrol_source, vpncontrol_handler);
    sock = lcconf->sock_vpncontrol;
    dispatch_source_set_cancel_handler(lcconf->vpncontrol_source,
                                         ^{
                                                close(sock);
                                         });
    dispatch_resume(lcconf->vpncontrol_source);
    return 0;
}

void
vpncontrol_disconnect_all(struct vpnctl_socket_elem *elem, const char *reason)
{
    struct bound_addr *addr;
    struct bound_addr *t_addr;

    plog(ASL_LEVEL_NOTICE,
         "received disconnect all command.\n");

    LIST_FOREACH_SAFE(addr, &elem->bound_addresses, chain, t_addr) {
        /* stop any connections */
        vpn_disconnect(addr, reason);
    }
}

void
vpncontrol_close()
{
    struct vpnctl_socket_elem *elem;
	struct vpnctl_socket_elem *t_elem;
	
    plog(ASL_LEVEL_NOTICE,
         "vpncontrol_close.\n");

    dispatch_source_cancel(lcconf->vpncontrol_source);
    lcconf->vpncontrol_source = NULL;

    lcconf->sock_vpncontrol = -1;
    LIST_FOREACH_SAFE(elem, &lcconf->vpnctl_comm_socks, chain, t_elem)
        vpncontrol_close_comm(elem);
}

static void
vpncontrol_close_comm(struct vpnctl_socket_elem *elem)
{
	struct bound_addr *addr;
	struct bound_addr *t_addr;

	plog(ASL_LEVEL_NOTICE,
		"vpncontrol_close_comm.\n");
	
	LIST_REMOVE(elem, chain);
	if (elem->sock != -1) {
		dispatch_source_cancel(elem->source);
		elem->sock = -1;
	}
	if (elem->buffer != NULL) {
		free(elem->buffer);
		elem->buffer = NULL;
		elem->pending_bytes_len = 0;
		elem->read_bytes_len = 0;
	}
	LIST_FOREACH_SAFE(addr, &elem->bound_addresses, chain, t_addr) {
		flushsainfo_dynamic(addr->address);
		LIST_REMOVE(addr, chain);
		if (addr->version)
			vfree(addr->version);
		racoon_free(addr);
	}
	racoon_free(elem);
	check_auto_exit();

}

int
vpn_control_connected(void)
{
	if (LIST_EMPTY(&lcconf->vpnctl_comm_socks))
		return 0;
	else
		return 1;
}

#endif
