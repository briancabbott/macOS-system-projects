/* $Id: vpn_control_var.h,v 1.7 2004/12/30 00:08:30 manubsd Exp $ */

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

#ifndef _VPN_CONTROL_VAR_H
#define _VPN_CONTROL_VAR_H

#include "vpn_control.h"
#include <dispatch/dispatch.h>
#include "localconf.h"

enum {
	VPN_STARTED_BY_API = 1,
	VPN_STARTED_BY_ADMIN,
	VPN_RESTARTED_BY_API,
};

extern void vpncontrol_handler (void *);
extern void vpncontrol_comm_handler (struct vpnctl_socket_elem *);
extern int vpncontrol_notify_ike_failed (u_int16_t, u_int16_t, u_int32_t, u_int16_t, u_int8_t*);
extern int vpncontrol_notify_phase_change (int, u_int16_t, phase1_handle_t*, phase2_handle_t*);
extern int vpncontrol_init (void);
extern void vpncontrol_close (void);
extern int vpn_control_connected (void);
extern int vpn_connect (struct bound_addr *, int);
extern int vpn_disconnect (struct bound_addr *, const char *);
extern void vpncontrol_disconnect_all (struct vpnctl_socket_elem *, const char *);
extern int vpn_start_ph2 (struct bound_addr *, struct vpnctl_cmd_start_ph2 *, size_t);
extern int vpncontrol_notify_need_authinfo (phase1_handle_t *, void*, size_t);
extern int vpncontrol_notify_peer_resp_ph1 (u_int16_t, phase1_handle_t*);
extern int vpncontrol_notify_peer_resp_ph2 (u_int16_t, phase2_handle_t*);
extern int vpn_assert (struct sockaddr_storage *, struct sockaddr_storage *);
extern bool vpncontrol_set_nat64_prefix(nw_nat64_prefix_t *prefix);

#endif /* _VPN_CONTROL_VAR_H */
