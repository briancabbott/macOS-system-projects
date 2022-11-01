/* 
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
 *    acknowledgment:
 *    "This product includes software developed by Computing Services
 *     at Carnegie Mellon University (http://www.cmu.edu/computing/)."
 *
 * CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: service.h,v 1.16 2003/10/22 18:50:14 rjs3 Exp $ */

#ifndef SERVICE_H
#define SERVICE_H

enum {
    STATUS_FD = 3,
    LISTEN_FD = 4
};

enum {
    MASTER_SERVICE_AVAILABLE = 0x01,
    MASTER_SERVICE_UNAVAILABLE = 0x02,
    MASTER_SERVICE_CONNECTION = 0x03,
#ifndef APPLE_OS_X_SERVER
    MASTER_SERVICE_CONNECTION_MULTI = 0x04
#else
    MASTER_SERVICE_CONNECTION_MULTI = 0x04,
    MASTER_SERVICE_STATUS_ADD_USER = 0x05,
    MASTER_SERVICE_STATUS_REMOVE_USER = 0x06
#endif
};

extern int service_init(int argc, char **argv, char **envp);
extern int service_main(int argc, char **argv, char **envp);
extern int service_main_fd(int fd, int argc, char **argv, char **envp);
extern int service_abort(int error);

enum {
    MAX_USE = 250,
    REUSE_TIMEOUT = 60
};

struct notify_message {
    int message;
    pid_t service_pid;
#ifdef APPLE_OS_X_SERVER
	time_t	s_start_time;
	char s_user_buf[64+1];
	char s_host_buf[64+1];
#endif
};

#endif
