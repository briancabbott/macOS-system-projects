/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1990, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)portmap.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
  "$FreeBSD: src/usr.sbin/portmap/portmap.c,v 1.10.2.3 2002/05/06 18:18:21 dwmalone Exp $";
#endif /* not lint */

/*
@(#)portmap.c	2.3 88/08/11 4.0 RPCSRC
static char sccsid[] = "@(#)portmap.c 1.32 87/08/06 Copyr 1984 Sun Micro";
*/

/*
 * portmap.c, Implements the program,version to port number mapping for
 * rpc.
 */

/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 *
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <launch.h>
#ifdef INSTANT_OFF
#include <vproc.h>
#include <rpcsvc/nlm_prot.h>
#include <rpcsvc/sm_inter.h>
#endif /* INSTANT_OFF */

#include "pmap_check.h"

extern void get_myaddress(struct sockaddr_in *);

static void reg_service __P((struct svc_req *, SVCXPRT *));
static void callit __P((struct svc_req *, SVCXPRT *));
static void usage __P((void));
static bool should_timeout(void);

struct pmaplist *pmaplist;
int debugging = 0;
static bool use_launchd = false;
#ifdef INSTANT_OFF
vproc_transaction_t vt = NULL;
sigset_t vt_set;
#endif /* INSTANT_OFF */

extern int svc_maxfd;

int
main(argc, argv)
	int argc;
	char **argv;
{
	SVCXPRT *xprt;
	int sock, c, l_on_d_fd = -1, sock_opt = 1;
	char **hosts = NULL;
	int nhosts = 0;
	struct sockaddr_in addr;
	int len = sizeof(struct sockaddr_in);
	register struct pmaplist *pml;
	launch_data_t msg, resp;
        fd_set readfds;

	openlog(getprogname(), LOG_PID | LOG_PERROR, LOG_DAEMON);

	signal(SIGPIPE, SIG_IGN);
	
	if (1 == getppid()) {
		use_launchd = true;
	} else {
		msg = launch_data_alloc(LAUNCH_DATA_DICTIONARY);
		launch_data_dict_insert(msg, launch_data_new_string("com.apple.portmap"), LAUNCH_KEY_GETJOB);

		resp = launch_msg(msg);
		if (resp == NULL) {
			syslog(LOG_ERR, "launch_msg(): %m");
			exit(1);
		}
		launch_data_free(msg);
		switch (launch_data_get_type(resp)) {
		case LAUNCH_DATA_DICTIONARY:
			syslog(LOG_ERR, "PID %d started this second instance of portmap (the first instance may not be running, as it launches on demand via launchd), exiting!", getppid());
			exit(1);
		case LAUNCH_DATA_ERRNO:
			errno = launch_data_get_errno(resp);
			/* if we couldn't find the job, then portmap really isn't running via launchd and we can proceed */
			if (ESRCH != errno) {
				syslog(LOG_ERR, "launch_msg(GETJOB...): %m");
				exit(1);
			}
			break;
		default:
			syslog(LOG_ERR, "unknown response while attempting to find out if portmap is registered with launchd");
			exit(1);
		}
		launch_data_free(resp);
	}

	while ((c = getopt(argc, argv, "dvh:")) != -1) {
		switch (c) {

		case 'd':
			debugging = 1;
			break;

		case 'v':
			verboselog = 1;
			break;

		case 'h':
			++nhosts;
			hosts = realloc(hosts, nhosts * sizeof(char *));
			hosts[nhosts - 1] = optarg;
			break;

		default:
			usage();
		}
	}

	closelog();

	if (!debugging && !use_launchd && daemon(0, 0))
		err(1, "fork");

	openlog(getprogname(), debugging ? LOG_PID | LOG_PERROR : LOG_PID,
			LOG_DAEMON);

	if (use_launchd) {
		msg = launch_data_alloc(LAUNCH_DATA_STRING);
		launch_data_set_string(msg, LAUNCH_KEY_CHECKIN);

		resp = launch_msg(msg);
		if (resp == NULL) {
			syslog(LOG_ERR, "launch_msg(): %m");
			exit(1);
		}
		launch_data_free(msg);
		if (launch_data_get_type(resp) == LAUNCH_DATA_DICTIONARY) {
			msg = launch_data_dict_lookup(resp, LAUNCH_JOBKEY_SOCKETS);
			msg = launch_data_dict_lookup(msg, "OnDemandTickler");
			msg = launch_data_array_get_index(msg, 0);
			l_on_d_fd = launch_data_get_fd(msg);
		} else if (launch_data_get_type(resp) == LAUNCH_DATA_STRING) {
			syslog(LOG_ERR, "launch_msg(): %s", launch_data_get_string(resp));
			exit(1);
		} else if (launch_data_get_type(resp) == LAUNCH_DATA_ERRNO) {
			syslog(LOG_ERR, "launch_msg(): resp == %s", strerror(launch_data_get_errno(resp)));
			exit(1);
		} else {
			syslog(LOG_ERR, "unknown response to launch_msg()");
		}
		launch_data_free(resp);
	}

	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PMAPPORT);

	/*
	 * If no hosts were specified, just bind to INADDR_ANY.  Otherwise
	 * make sure 127.0.0.1 is added to the list.
	 */
	++nhosts;
	hosts = realloc(hosts, nhosts * sizeof(char *));
	if (nhosts == 1)
		hosts[0] = "0.0.0.0";
	else
		hosts[nhosts - 1] = "127.0.0.1";

	/*
	 * Add UDP socket(s) - bind to specific IPs if asked to
	 */
	while (nhosts > 0) {
	    --nhosts;

	    if (!inet_aton(hosts[nhosts], &addr.sin_addr)) {
		    syslog(LOG_ERR, "bad IP address: %s", hosts[nhosts]);
		    exit(1);
	    }
	    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		    syslog(LOG_ERR, "cannot create udp socket: %m");
		    exit(1);
	    }
	    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *)&sock_opt, sizeof(sock_opt)) == -1) {
		    syslog(LOG_ERR, "setsockopt(SO_REUSEADDR): %m");
		    exit(1);
	    }

	    if (bind(sock, (struct sockaddr *)&addr, len) != 0) {
		    syslog(LOG_ERR, "cannot bind udp: %m");
		    exit(1);
	    }

	    if ((xprt = svcudp_create(sock)) == (SVCXPRT *)NULL) {
		    syslog(LOG_ERR, "couldn't do udp_create");
		    exit(1);
	    }
	}
	free(hosts);
	/* make an entry for ourself */
	pml = (struct pmaplist *)malloc((u_int)sizeof(struct pmaplist));
	pml->pml_next = 0;
	pml->pml_map.pm_prog = PMAPPROG;
	pml->pml_map.pm_vers = PMAPVERS;
	pml->pml_map.pm_prot = IPPROTO_UDP;
	pml->pml_map.pm_port = PMAPPORT;
	pmaplist = pml;

	/*
	 * Add TCP socket
	 */
	addr.sin_addr.s_addr = 0;
	if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		syslog(LOG_ERR, "cannot create tcp socket: %m");
		exit(1);
	}
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *)&sock_opt, sizeof(sock_opt)) == -1) {
		syslog(LOG_ERR, "setsockopt(SO_REUSEADDR): %m");
		exit(1);
	}
	if (bind(sock, (struct sockaddr *)&addr, len) != 0) {
		syslog(LOG_ERR, "cannot bind tcp: %m");
		exit(1);
	}
	if ((xprt = svctcp_create(sock, RPCSMALLMSGSIZE, RPCSMALLMSGSIZE))
	    == (SVCXPRT *)NULL) {
		syslog(LOG_ERR, "couldn't do tcp_create");
		exit(1);
	}
	/* make an entry for ourself */
	pml = (struct pmaplist *)malloc((u_int)sizeof(struct pmaplist));
	pml->pml_map.pm_prog = PMAPPROG;
	pml->pml_map.pm_vers = PMAPVERS;
	pml->pml_map.pm_prot = IPPROTO_TCP;
	pml->pml_map.pm_port = PMAPPORT;
	pml->pml_next = pmaplist;
	pmaplist = pml;

	(void)svc_register(xprt, PMAPPROG, PMAPVERS, reg_service, FALSE);

	/* additional initializations */
	check_startup();
	signal(SIGCHLD, SIG_IGN);

	for (;;) {
		struct timeval timeout = { 30, 0 };
		int r, maxfd;
		readfds = svc_fdset;
		if (l_on_d_fd != -1) {
			maxfd = (svc_maxfd > l_on_d_fd ? svc_maxfd : l_on_d_fd) + 1;
			FD_SET(l_on_d_fd, &readfds);
		} else {
			maxfd = svc_maxfd + 1;
		}
		r = select(maxfd, &readfds, NULL, NULL, should_timeout() ? &timeout : NULL);
		if (r == -1) {
                        if (errno == EINTR)
                                continue;
                        syslog(LOG_ERR, "cannot bind tcp: %m");
			abort();
		} else if (r > 0) {
			if (l_on_d_fd != -1 && FD_ISSET(l_on_d_fd, &readfds)) {
				struct sockaddr_un sun;
				socklen_t sl = sizeof(sun);
				char b = '\0';
				int j = accept(l_on_d_fd, (struct sockaddr *)&sun, &sl);

				if (j == -1) {
					syslog(LOG_WARNING, "accept(): %m");
				} else {
					write(j, &b, sizeof(b));
					close(j);
				}
				FD_CLR(l_on_d_fd, &readfds);
			}
                        svc_getreqset(&readfds);
                } else {
			if (debugging)
				(void) fprintf(stderr, "timed out, exiting\n");
			exit(EXIT_SUCCESS);
		}
        }
}

static void
usage()
{
	fprintf(stderr, "usage: portmap [-dv] [-h bindip]\n");
	exit(1);
}

#ifndef lint
/* need to override perror calls in rpc library */
void
perror(what)
	const char *what;
{
	syslog(LOG_ERR, "%s: %m", what);
}
#endif

static struct pmaplist *
find_service(prog, vers, prot)
	u_long prog, vers, prot;
{
	register struct pmaplist *hit = NULL;
	register struct pmaplist *pml;

	for (pml = pmaplist; pml != NULL; pml = pml->pml_next) {
		if ((pml->pml_map.pm_prog != prog) ||
			(pml->pml_map.pm_prot != prot))
			continue;
		hit = pml;
		if (pml->pml_map.pm_vers == vers)
		    break;
	}
	return (hit);
}

static bool
should_timeout(void)
{
	bool r = true;
	struct pmaplist *pml;

	if (!use_launchd)
		return false;

	for (pml = pmaplist; pml != NULL; pml = pml->pml_next) {
		if (pml->pml_map.pm_prog != PMAPPROG) {
			r = false;
			break;
		}
	}
	return r;
}

#ifdef INSTANT_OFF
static void
toggle_instantoff()
{
	struct pmaplist *pml;
	int count = 0;
	sigset_t nset;

	for (pml = pmaplist; pml != NULL; pml = pml->pml_next) {
		/* 6985009: we only care about the nfs services */
		if (pml->pml_map.pm_prog == NLM_PROG || pml->pml_map.pm_prog == SM_PROG)
			++count;
	}

	if (count > 0 && vt == NULL) {
		sigemptyset(&nset);
		sigaddset(&nset, SIGTERM);
		sigprocmask(SIG_BLOCK, &nset, &vt_set);
		vt = vproc_transaction_begin(NULL);
	} else if (count == 0 && vt != NULL) {
		vproc_transaction_end(NULL, vt);
		vt = NULL;
		sigprocmask(SIG_SETMASK, &vt_set, NULL);
	}
}
#endif /* INSTANT_OFF */

/*
 * 1 OK, 0 not
 */
static void
reg_service(rqstp, xprt)
	struct svc_req *rqstp;
	SVCXPRT *xprt;
{
	struct pmap reg;
	struct pmaplist *pml, *prevpml, *fnd;
	int ans, port;
	caddr_t t;

	/*
	 * Later wrappers change the logging severity on the fly. Reset to
	 * defaults before handling the next request.
	 */
	allow_severity = LOG_INFO;
	deny_severity = LOG_WARNING;

	if (debugging)
		(void) fprintf(stderr, "server: about to do a switch\n");
	switch (rqstp->rq_proc) {

	case PMAPPROC_NULL:
		/*
		 * Null proc call
		 */
		/* remote host authorization check */
		check_default(svc_getcaller(xprt), rqstp->rq_proc, (u_long) 0);
		if (!svc_sendreply(xprt, (xdrproc_t)xdr_void, (caddr_t)0) && debugging) {
			abort();
		}
		break;

	case PMAPPROC_SET:
		/*
		 * Set a program,version to port mapping
		 */
		if (!svc_getargs(xprt, xdr_pmap, (caddr_t)&reg))
			svcerr_decode(xprt);
		else {
			/* reject non-local requests, protect priv. ports */
			if (!check_setunset(svc_getcaller(xprt),
			    rqstp->rq_proc, reg.pm_prog, reg.pm_port)) {
				ans = 0;
				goto done;
			}
			/*
			 * check to see if already used
			 * find_service returns a hit even if
			 * the versions don't match, so check for it
			 */
			fnd = find_service(reg.pm_prog, reg.pm_vers, reg.pm_prot);
			if (fnd && fnd->pml_map.pm_vers == reg.pm_vers) {
				if (fnd->pml_map.pm_port == reg.pm_port) {
					ans = 1;
					goto done;
				}
				else {
					ans = 0;
					goto done;
				}
			} else {
				/*
				 * add to END of list
				 */
				pml = (struct pmaplist *)
				    malloc((u_int)sizeof(struct pmaplist));
				pml->pml_map = reg;
				pml->pml_next = 0;
				if (pmaplist == 0) {
					pmaplist = pml;
				} else {
					for (fnd= pmaplist; fnd->pml_next != 0;
					    fnd = fnd->pml_next);
					fnd->pml_next = pml;
				}
				ans = 1;
			}
		done:
			if ((!svc_sendreply(xprt, (xdrproc_t)xdr_long, (caddr_t)&ans)) &&
			    debugging) {
				(void) fprintf(stderr, "svc_sendreply\n");
				abort();
			}
#ifdef INSTANT_OFF
			toggle_instantoff();
#endif /* INSTANT_OFF */
		}
		break;

	case PMAPPROC_UNSET:
		/*
		 * Remove a program,version to port mapping.
		 */
		if (!svc_getargs(xprt, xdr_pmap, (caddr_t)&reg))
			svcerr_decode(xprt);
		else {
			ans = 0;
			/* reject non-local requests */
			if (!check_setunset(svc_getcaller(xprt),
			    rqstp->rq_proc, reg.pm_prog, (u_long) 0))
				goto done;
			for (prevpml = NULL, pml = pmaplist; pml != NULL; ) {
				if ((pml->pml_map.pm_prog != reg.pm_prog) ||
					(pml->pml_map.pm_vers != reg.pm_vers)) {
					/* both pml & prevpml move forwards */
					prevpml = pml;
					pml = pml->pml_next;
					continue;
				}
				/* found it; pml moves forward, prevpml stays */
				/* privileged port check */
				if (!check_privileged_port(svc_getcaller(xprt),
				    rqstp->rq_proc,
				    reg.pm_prog,
				    pml->pml_map.pm_port)) {
					ans = 0;
					break;
				}
				ans = 1;
				t = (caddr_t)pml;
				pml = pml->pml_next;
				if (prevpml == NULL)
					pmaplist = pml;
				else
					prevpml->pml_next = pml;
				free(t);
			}
			if ((!svc_sendreply(xprt, (xdrproc_t)xdr_long, (caddr_t)&ans)) &&
			    debugging) {
				(void) fprintf(stderr, "svc_sendreply\n");
				abort();
			}
#ifdef INSTANT_OFF
			toggle_instantoff();
#endif /* INSTANT_OFF */
		}
		break;

	case PMAPPROC_GETPORT:
		/*
		 * Lookup the mapping for a program,version and return its port
		 */
		if (!svc_getargs(xprt, xdr_pmap, (caddr_t)&reg))
			svcerr_decode(xprt);
		else {
			/* remote host authorization check */
			if (!check_default(svc_getcaller(xprt),
			    rqstp->rq_proc,
			    reg.pm_prog)) {
				ans = 0;
				goto done;
			}
			fnd = find_service(reg.pm_prog, reg.pm_vers, reg.pm_prot);
			if (fnd)
				port = fnd->pml_map.pm_port;
			else
				port = 0;
			if ((!svc_sendreply(xprt, (xdrproc_t)xdr_long, (caddr_t)&port)) &&
			    debugging) {
				(void) fprintf(stderr, "svc_sendreply\n");
				abort();
			}
		}
		break;

	case PMAPPROC_DUMP:
		/*
		 * Return the current set of mapped program,version
		 */
		if (!svc_getargs(xprt, xdr_void, NULL))
			svcerr_decode(xprt);
		else {
			/* remote host authorization check */
			struct pmaplist *p;
			if (!check_default(svc_getcaller(xprt),
			    rqstp->rq_proc, (u_long) 0)) {
				p = 0;	/* send empty list */
			} else {
				p = pmaplist;
			}
			if ((!svc_sendreply(xprt, (xdrproc_t)xdr_pmaplist,
			    (caddr_t)&p)) && debugging) {
				(void) fprintf(stderr, "svc_sendreply\n");
				abort();
			}
		}
		break;

	case PMAPPROC_CALLIT:
		/*
		 * Calls a procedure on the local machine.  If the requested
		 * procedure is not registered this procedure does not return
		 * error information!!
		 * This procedure is only supported on rpc/udp and calls via
		 * rpc/udp.  It passes null authentication parameters.
		 */
		callit(rqstp, xprt);
		break;

	default:
		/* remote host authorization check */
		check_default(svc_getcaller(xprt), rqstp->rq_proc, (u_long) 0);
		svcerr_noproc(xprt);
		break;
	}
}


/*
 * Stuff for the rmtcall service
 */
#define ARGSIZE 9000

struct encap_parms {
	u_int arglen;
	char *args;
};

static bool_t
xdr_encap_parms(xdrs, epp)
	XDR *xdrs;
	struct encap_parms *epp;
{

	return (xdr_bytes(xdrs, &(epp->args), &(epp->arglen), ARGSIZE));
}

struct rmtcallargs {
#ifdef __LP64__
	unsigned int	rmt_prog;
	unsigned int	rmt_vers;
	unsigned int	rmt_port;
	unsigned int	rmt_proc;
#else
	u_long	rmt_prog;
	u_long	rmt_vers;
	u_long	rmt_port;
	u_long	rmt_proc;
#endif
	struct encap_parms rmt_args;
};

static bool_t
xdr_rmtcall_args(xdrs, cap)
	XDR *xdrs;
	struct rmtcallargs *cap;
{

	/* does not get a port number */
	if (xdr_u_long(xdrs, &(cap->rmt_prog)) &&
	    xdr_u_long(xdrs, &(cap->rmt_vers)) &&
	    xdr_u_long(xdrs, &(cap->rmt_proc))) {
		return (xdr_encap_parms(xdrs, &(cap->rmt_args)));
	}
	return (FALSE);
}

static bool_t
xdr_rmtcall_result(xdrs, cap)
	XDR *xdrs;
	struct rmtcallargs *cap;
{
	if (xdr_u_long(xdrs, &(cap->rmt_port)))
		return (xdr_encap_parms(xdrs, &(cap->rmt_args)));
	return (FALSE);
}

/*
 * only worries about the struct encap_parms part of struct rmtcallargs.
 * The arglen must already be set!!
 */
static bool_t
xdr_opaque_parms(xdrs, cap)
	XDR *xdrs;
	struct rmtcallargs *cap;
{
	return (xdr_opaque(xdrs, cap->rmt_args.args, cap->rmt_args.arglen));
}

/*
 * This routine finds and sets the length of incoming opaque paraters
 * and then calls xdr_opaque_parms.
 */
static bool_t
xdr_len_opaque_parms(xdrs, cap)
	XDR *xdrs;
	struct rmtcallargs *cap;
{
	register u_int beginpos, lowpos, highpos, currpos, pos;

	beginpos = lowpos = pos = xdr_getpos(xdrs);
	highpos = lowpos + ARGSIZE;
	while ((int)(highpos - lowpos) >= 0) {
		currpos = (lowpos + highpos) / 2;
		if (xdr_setpos(xdrs, currpos)) {
			pos = currpos;
			lowpos = currpos + 1;
		} else {
			highpos = currpos - 1;
		}
	}
	xdr_setpos(xdrs, beginpos);
	cap->rmt_args.arglen = pos - beginpos;
	return (xdr_opaque_parms(xdrs, cap));
}

/*
 * Call a remote procedure service
 * This procedure is very quiet when things go wrong.
 * The proc is written to support broadcast rpc.  In the broadcast case,
 * a machine should shut-up instead of complain, less the requestor be
 * overrun with complaints at the expense of not hearing a valid reply ...
 *
 * This now forks so that the program & process that it calls can call
 * back to the portmapper.
 */
static void
callit(rqstp, xprt)
	struct svc_req *rqstp;
	SVCXPRT *xprt;
{
	struct rmtcallargs a;
	struct pmaplist *pml;
	u_short port;
	struct sockaddr_in me;
	int pid, so = -1;
	CLIENT *client;
	struct authunix_parms *au = (struct authunix_parms *)rqstp->rq_clntcred;
	struct timeval timeout;
	char buf[ARGSIZE];

	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	a.rmt_args.args = buf;
	if (!svc_getargs(xprt, xdr_rmtcall_args, (caddr_t)&a))
		return;
	/* host and service access control */
	if (!check_callit(svc_getcaller(xprt),
	    rqstp->rq_proc, a.rmt_prog, a.rmt_proc))
		return;
	if ((pml = find_service(a.rmt_prog, a.rmt_vers,
	    (u_long)IPPROTO_UDP)) == NULL)
		return;
	/*
	 * fork a child to do the work.  Parent immediately returns.
	 * Child exits upon completion.
	 */
	if ((pid = fork()) != 0) {
		if (pid < 0)
			syslog(LOG_ERR, "CALLIT (prog %lu): fork: %m",
			    a.rmt_prog);
		return;
	}
	port = pml->pml_map.pm_port;
	get_myaddress(&me);
	me.sin_port = htons(port);
	client = clntudp_create(&me, a.rmt_prog, a.rmt_vers, timeout, &so);
	if (client != (CLIENT *)NULL) {
		if (rqstp->rq_cred.oa_flavor == AUTH_UNIX) {
			client->cl_auth = authunix_create(au->aup_machname,
			   au->aup_uid, au->aup_gid, au->aup_len, au->aup_gids);
		}
		a.rmt_port = (u_long)port;
		if (clnt_call(client, a.rmt_proc, (xdrproc_t)xdr_opaque_parms, &a,
		    (xdrproc_t)xdr_len_opaque_parms, &a, timeout) == RPC_SUCCESS) {
			svc_sendreply(xprt, (xdrproc_t)xdr_rmtcall_result, (caddr_t)&a);
		}
		AUTH_DESTROY(client->cl_auth);
		clnt_destroy(client);
	}
	(void)close(so);
	exit(0);
}
