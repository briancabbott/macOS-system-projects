/* $Id: sockmisc.h,v 1.5.10.4 2005/10/04 09:54:27 manubsd Exp $ */

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

#ifndef _SOCKMISC_H
#define _SOCKMISC_H

struct netaddr {
	union {
		struct sockaddr_storage sa;
		struct sockaddr_in sin;
		struct sockaddr_in6 sin6;
	} sa;
	unsigned long prefix;
};

extern const int niflags;

extern int cmpsaddrwop (const struct sockaddr_storage *, const struct sockaddr_storage *);
extern int cmpsaddrwop_withprefix(const struct sockaddr_storage *, const struct sockaddr_storage *, int);
extern int cmpsaddrwild (const struct sockaddr_storage *, const struct sockaddr_storage *);
extern int cmpsaddrstrict (const struct sockaddr_storage *, const struct sockaddr_storage *);
extern int cmpsaddrstrict_withprefix(const struct sockaddr_storage *, const struct sockaddr_storage *, int);

#ifdef ENABLE_NATT 
#define CMPSADDR(saddr1, saddr2) cmpsaddrstrict((saddr1), (saddr2))
#define CMPSADDR2(saddr1, saddr2) cmpsaddrwild((saddr1), (saddr2))
#else 
#define CMPSADDR(saddr1, saddr2) cmpsaddrwop((saddr1), (saddr2))
#define CMPSADDR2(saddr1, saddr2) cmpsaddrwop((saddr1), (saddr2))
#endif

extern struct sockaddr_storage *getlocaladdr (struct sockaddr *);

extern int recvfromto (int, void *, size_t, int,
	struct sockaddr_storage *, socklen_t *, struct sockaddr_storage *, unsigned int *);
extern int sendfromto (int, const void *, size_t,
	struct sockaddr_storage *, struct sockaddr_storage *, int);

extern int setsockopt_bypass (int, int);

extern struct sockaddr_storage *newsaddr (int);
extern struct sockaddr_storage *dupsaddr (struct sockaddr_storage *);
extern char *saddr2str (const struct sockaddr *);
extern char *saddr2str_with_prefix __P((const struct sockaddr *, int));
extern char *saddrwop2str (const struct sockaddr *);
extern char *saddr2str_fromto (const char *format, 
				   const struct sockaddr *saddr, 
				   const struct sockaddr *daddr);
extern struct sockaddr_storage *str2saddr (char *, char *);
extern void mask_sockaddr (struct sockaddr_storage *, const struct sockaddr_storage *,
	size_t);

/* struct netaddr functions */
extern char *naddrwop2str (const struct netaddr *naddr);
extern char *naddrwop2str_fromto (const char *format, const struct netaddr *saddr,
				      const struct netaddr *daddr);
extern int naddr_score(const struct netaddr *naddr, const struct sockaddr_storage *saddr);

/* Some usefull functions for sockaddr port manipulations. */
extern u_int16_t extract_port (const struct sockaddr_storage *addr);
extern u_int16_t *set_port (struct sockaddr_storage *addr, u_int16_t new_port);
extern u_int16_t *get_port_ptr (struct sockaddr_storage *addr);

#endif /* _SOCKMISC_H */
