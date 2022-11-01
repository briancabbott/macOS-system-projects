
/*
 * Copyright (c) 2002, 2018 Apple Inc. All rights reserved.
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

#ifndef _PPP_SOCKET_SERVER_H
#define _PPP_SOCKET_SERVER_H

int ppp_socket_start_server(void);
int ppp_socket_create_client(int s, int priviledged, uid_t uid, gid_t gid);
void socket_client_notify(CFSocketRef ref, u_char *sid, u_int32_t link, u_long event, u_long error, u_int32_t flags);

#endif /* _PPP_SOCKET_SERVER_H */



