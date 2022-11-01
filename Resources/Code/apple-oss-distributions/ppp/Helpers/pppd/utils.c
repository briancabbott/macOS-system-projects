/*
 * Copyright (c) 2003, 2018 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * utils.c - various utility functions used in pppd.
 *
 * Copyright (c) 1999-2002 Paul Mackerras. All rights reserved.
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
 * 3. The name(s) of the authors of this software must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission.
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Paul Mackerras
 *     <paulus@samba.org>".
 *
 * THE AUTHORS OF THIS SOFTWARE DISCLAIM ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define RCSID	"$Id: utils.c,v 1.8 2005/12/13 06:30:15 lindak Exp $"

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <syslog.h>
#include <netdb.h>
#include <time.h>
#include <pwd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#ifdef SVR4
#include <sys/mkdev.h>
#endif
#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_media.h>
#include <netinet/in_var.h>
#include <sys/kern_event.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <CoreFoundation/CFBundle.h>

#include "pppd.h"
#include "fsm.h"
#include "lcp.h"

#ifndef lint
static const char rcsid[] = RCSID;
#endif

#if defined(SUNOS4)
extern char *strerror();
#endif

static void logit __P((int, char *, va_list));
static void log_write __P((int, char *));
static void vslp_printer __P((void *, char *, ...));
static void format_packet __P((u_char *, int, void (*) (void *, char *, ...),
			       void *));

struct buffer_info {
    char *ptr;
    int len;
};

#ifdef NO_SRTLXXX

/*
 * strlcpy - like strcpy/strncpy, doesn't overflow destination buffer,
 * always leaves destination null-terminated (for len > 0).
 */
size_t
strlcpy(dest, src, len)
    char *dest;
    const char *src;
    size_t len;
{
    size_t ret = strlen(src);

    if (len != 0) {
	if (ret < len)
	    strcpy(dest, src);
	else {
	    strncpy(dest, src, len - 1);
	    dest[len-1] = 0;
	}
    }
    return ret;
}

/*
 * strlcat - like strcat/strncat, doesn't overflow destination buffer,
 * always leaves destination null-terminated (for len > 0).
 */
size_t
strlcat(dest, src, len)
    char *dest;
    const char *src;
    size_t len;
{
    size_t dlen = strlen(dest);

    return dlen + strlcpy(dest + dlen, src, (len > dlen? len - dlen: 0));
}

#endif /* NO_SRTLXXX */

/*
 * slprintf - format a message into a buffer.  Like sprintf except we
 * also specify the length of the output buffer, and we handle
 * %m (error message), %v (visible string),
 * %q (quoted string), %t (current time) and %I (IP address) formats.
 * Doesn't do floating-point formats.
 * Returns the number of chars put into buf.
 */
int
slprintf __V((char *buf, int buflen, char *fmt, ...))
{
    va_list args;
    int n;

#if defined(__STDC__)
    va_start(args, fmt);
#else
    char *buf;
    int buflen;
    char *fmt;
    va_start(args);
    buf = va_arg(args, char *);
    buflen = va_arg(args, int);
    fmt = va_arg(args, char *);
#endif
    n = vslprintf(buf, buflen, fmt, args);
    va_end(args);
    return n;
}

/*
 * vslprintf - like slprintf, takes a va_list instead of a list of args.
 */
#define OUTCHAR(c)	(buflen > 0? (--buflen, *buf++ = (c)): 0)

int
vslprintf(buf, buflen, fmt, args)
    char *buf;
    int buflen;
    char *fmt;
    va_list args;
{
    int c, i, n;
    int width, prec, fillch;
    int base, len, neg, quoted;
    unsigned long val = 0;
    char *str, *f, *buf0;
    unsigned char *p;
    char num[32];
    time_t t;
    u_int32_t ip;
    static char hexchars[] = "0123456789abcdef";
    struct buffer_info bufinfo;

    buf0 = buf;
    --buflen;
    while (buflen > 0) {
	for (f = fmt; *f != '%' && *f != 0; ++f)
	    ;
	if (f > fmt) {
	    len = (int)(f - fmt);
	    if (len > buflen)
		len = buflen;
	    memcpy(buf, fmt, len);
	    buf += len;
	    buflen -= len;
	    fmt = f;
	}
	if (*fmt == 0)
	    break;
	c = *++fmt;
	width = 0;
	prec = -1;
	fillch = ' ';
	if (c == '0') {
	    fillch = '0';
	    c = *++fmt;
	}
	if (c == '*') {
	    width = va_arg(args, int);
	    c = *++fmt;
	} else {
	    while (isdigit(c)) {
		width = width * 10 + c - '0';
		c = *++fmt;
	    }
	}
	if (c == '.') {
	    c = *++fmt;
	    if (c == '*') {
		prec = va_arg(args, int);
		c = *++fmt;
	    } else {
		prec = 0;
		while (isdigit(c)) {
		    prec = prec * 10 + c - '0';
		    c = *++fmt;
		}
	    }
	}
	str = 0;
	base = 0;
	neg = 0;
	++fmt;
	switch (c) {
	case 'l':
	    c = *fmt++;
	    switch (c) {
	    case 'd':
		val = va_arg(args, long);
		if ((long)val < 0) {
		    neg = 1;
		    val = -val;
		}
		base = 10;
		break;
	    case 'u':
		val = va_arg(args, unsigned long);
		base = 10;
		break;
	    default:
		*buf++ = '%'; --buflen;
		*buf++ = 'l'; --buflen;
		--fmt;		/* so %lz outputs %lz etc. */
		continue;
	    }
	    break;
	case 'd':
	    i = va_arg(args, int);
	    if (i < 0) {
		neg = 1;
		val = -i;
	    } else
		val = i;
	    base = 10;
	    break;
	case 'u':
	    val = va_arg(args, unsigned int);
	    base = 10;
	    break;
	case 'o':
	    val = va_arg(args, unsigned int);
	    base = 8;
	    break;
	case 'x':
	case 'X':
	    val = va_arg(args, unsigned int);
	    base = 16;
	    break;
	case 'p':
	    val = (unsigned long) va_arg(args, void *);
	    base = 16;
	    neg = 2;
	    break;
	case 's':
	    str = va_arg(args, char *);
	    break;
	case 'c':
	    num[0] = va_arg(args, int);
	    num[1] = 0;
	    str = num;
	    break;
	case 'm':
	    str = strerror(errno);
	    break;
	case 'I':
	    ip = va_arg(args, u_int32_t);
	    ip = ntohl(ip);
	    slprintf(num, sizeof(num), "%d.%d.%d.%d", (ip >> 24) & 0xff,
		     (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff);
	    str = num;
	    break;
#if 0	/* not used, and breaks on S/390, apparently */
	case 'r':
	    f = va_arg(args, char *);
#ifndef __powerpc__
	    n = vslprintf(buf, buflen + 1, f, va_arg(args, va_list));
#else
	    /* On the powerpc, a va_list is an array of 1 structure */
	    n = vslprintf(buf, buflen + 1, f, va_arg(args, void *));
#endif
	    buf += n;
	    buflen -= n;
	    continue;
#endif
	case 't':
	    time(&t);
	    str = ctime(&t);
	    str += 4;		/* chop off the day name */
	    str[15] = 0;	/* chop off year and newline */
	    break;
	case 'v':		/* "visible" string */
	case 'q':		/* quoted string */
	    quoted = c == 'q';
	    p = va_arg(args, unsigned char *);
	    if (fillch == '0' && prec >= 0) {
		n = prec;
	    } else {
		n = (int)strlen((char *)p);
		if (prec >= 0 && n > prec)
		    n = prec;
	    }
	    while (n > 0 && buflen > 0) {
		c = *p++;
		--n;
		if (!quoted && c >= 0x80) {
		    OUTCHAR('M');
		    OUTCHAR('-');
		    c -= 0x80;
		}
		if (quoted && (c == '"' || c == '\\'))
		    OUTCHAR('\\');
		if (c < 0x20 || (0x7f <= c && c < 0xa0)) {
		    if (quoted) {
			OUTCHAR('\\');
			switch (c) {
			case '\t':	OUTCHAR('t');	break;
			case '\n':	OUTCHAR('n');	break;
			case '\b':	OUTCHAR('b');	break;
			case '\f':	OUTCHAR('f');	break;
			default:
			    OUTCHAR('x');
			    OUTCHAR(hexchars[c >> 4]);
			    OUTCHAR(hexchars[c & 0xf]);
			}
		    } else {
			if (c == '\t')
			    OUTCHAR(c);
			else {
			    OUTCHAR('^');
			    OUTCHAR(c ^ 0x40);
			}
		    }
		} else
		    OUTCHAR(c);
	    }
	    continue;
	case 'P':		/* print PPP packet */
	    bufinfo.ptr = buf;
	    bufinfo.len = buflen + 1;
	    p = va_arg(args, unsigned char *);
	    n = va_arg(args, int);
	    format_packet(p, n, vslp_printer, &bufinfo);
	    buf = bufinfo.ptr;
	    buflen = bufinfo.len - 1;
	    continue;
	case 'B':
	    p = va_arg(args, unsigned char *);
	    for (n = prec; n > 0; --n) {
		c = *p++;
		if (fillch == ' ')
		    OUTCHAR(' ');
		OUTCHAR(hexchars[(c >> 4) & 0xf]);
		OUTCHAR(hexchars[c & 0xf]);
	    }
	    continue;
	default:
	    *buf++ = '%';
	    if (c != '%')
		--fmt;		/* so %z outputs %z etc. */
	    --buflen;
	    continue;
	}
	if (base != 0) {
	    str = num + sizeof(num);
	    *--str = 0;
	    while (str > num + neg) {
		*--str = hexchars[val % base];
		val = val / base;
		if (--prec <= 0 && val == 0)
		    break;
	    }
	    switch (neg) {
	    case 1:
		*--str = '-';
		break;
	    case 2:
		*--str = 'x';
		*--str = '0';
		break;
	    }
	    len = (int)(num + sizeof(num) - 1 - str);
	} else {
	    len = (int)strlen(str);
	    if (prec >= 0 && len > prec)
		len = prec;
	}
	if (width > 0) {
	    if (width > buflen)
		width = buflen;
	    if ((n = width - len) > 0) {
		buflen -= n;
		for (; n > 0; --n)
		    *buf++ = fillch;
	    }
	}
	if (len > buflen)
	    len = buflen;
	memcpy(buf, str, len);
	buf += len;
	buflen -= len;
    }
    *buf = 0;
    return (int)(buf - buf0);
}

/*
 * vslp_printer - used in processing a %P format
 */
static void
vslp_printer __V((void *arg, char *fmt, ...))
{
    int n;
    va_list pvar;
    struct buffer_info *bi;

#if defined(__STDC__)
    va_start(pvar, fmt);
#else
    void *arg;
    char *fmt;
    va_start(pvar);
    arg = va_arg(pvar, void *);
    fmt = va_arg(pvar, char *);
#endif

    bi = (struct buffer_info *) arg;
    n = vslprintf(bi->ptr, bi->len, fmt, pvar);
    va_end(pvar);

    bi->ptr += n;
    bi->len -= n;
}

#ifdef unused
/*
 * log_packet - format a packet and log it.
 */

void
log_packet(p, len, prefix, level)
    u_char *p;
    int len;
    char *prefix;
    int level;
{
	init_pr_log(prefix, level);
	format_packet(p, len, pr_log, &level);
	end_pr_log();
}
#endif /* unused */

/*
 * format_packet - make a readable representation of a packet,
 * calling `printer(arg, format, ...)' to output it.
 */
static void
format_packet(p, len, printer, arg)
    u_char *p;
    int len;
    void (*printer) __P((void *, char *, ...));
    void *arg;
{
    int i, n;
    u_short proto;
    struct protent *protp;

    if (len >= PPP_HDRLEN && p[0] == PPP_ALLSTATIONS && p[1] == PPP_UI) {
	p += 2;
	GETSHORT(proto, p);
	len -= PPP_HDRLEN;
	for (i = 0; (protp = protocols[i]) != NULL; ++i)
	    if (proto == protp->protocol)
		break;
	if (protp != NULL) {
	    printer(arg, "[%s", protp->name);
	    n = (*protp->printpkt)(p, len, printer, arg);
	    printer(arg, "]");
	    p += n;
	    len -= n;
	} else {
	    for (i = 0; (protp = protocols[i]) != NULL; ++i)
		if (proto == (protp->protocol & ~0x8000))
		    break;
	    if (protp != 0 && protp->data_name != 0) {
#ifdef __APPLE__
				printer(arg, "[%s data", protp->data_name);
				if (protp->printdatapkt) {
					n = (*protp->printdatapkt)(p, len, printer, arg);
					p += n;
					len -= n;
				}
				printer(arg, "]\n");
				
                while (len > 0) {
                    int lcount = (len > 16) ? 16 : len;
            
                    /* output line (hex 1st, then ascii) */
                    printer(arg, "  ");
                    for(i = 0; i < lcount; i++) {
                        if (i == 8) printer(arg, "  ");
                        printer(arg, "%.1B", &p[i]);
                    }
                    for( ; i < 16; i++) {
                        if (i == 8) printer(arg, "  ");
                        printer(arg, "   ");
                    }
                    printer(arg, "   '");
                    for(i = 0; i < lcount; i++)
                        printer(arg, "%c",(p[i]>=040 && p[i]<=0176)?p[i]:'.');
                    printer(arg, "'\n");
                    
                    len -= 16;
                    p += 16;
                }
#else
		printer(arg, "[%s data]\n", protp->data_name);
		if (len > 8)
		    printer(arg, "%.8B ...", p);
		else
		    printer(arg, "%.*B", len, p);
#endif
		len = 0;
	    } else
		printer(arg, "[proto=0x%x]", proto);
	}
    }

    if (len > 32)
	printer(arg, "%.32B ...", p);
    else
	printer(arg, "%.*B", len, p);
}

/*
 * init_pr_log, end_pr_log - initialize and finish use of pr_log.
 */

static char line[256];		/* line to be logged accumulated here */
static char *linep;		/* current pointer within line */
static int llevel;		/* level for logging */

void
init_pr_log(prefix, level)
     char *prefix;
     int level;
{
	linep = line;
	if (prefix != NULL) {
		strlcpy(line, prefix, sizeof(line));
		linep = line + strlen(line);
	}
	llevel = level;
}

void
end_pr_log()
{
	if (linep != line) {
		*linep = 0;
		log_write(llevel, line);
	}
}

/*
 * pr_log - printer routine for outputting to syslog
 */
void
pr_log __V((void *arg, char *fmt, ...))
{
	int l, n;
	va_list pvar;
	char *p, *eol;
	char buf[256];

#if defined(__STDC__)
	va_start(pvar, fmt);
#else
	void *arg;
	char *fmt;
	va_start(pvar);
	arg = va_arg(pvar, void *);
	fmt = va_arg(pvar, char *);
#endif

	n = vslprintf(buf, sizeof(buf), fmt, pvar);
	va_end(pvar);

	p = buf;
	eol = strchr(buf, '\n');
	if (linep != line) {
		l = (eol == NULL)? n: (int)(eol - buf);
		if (linep + l < line + sizeof(line)) {
			if (l > 0) {
				memcpy(linep, buf, l);
				linep += l;
			}
			if (eol == NULL)
				return;
			p = eol + 1;
			eol = strchr(p, '\n');
		}
		*linep = 0;
		log_write(llevel, line);
		linep = line;
	}

	while (eol != NULL) {
		*eol = 0;
		log_write(llevel, p);
		p = eol + 1;
		eol = strchr(p, '\n');
	}

	/* assumes sizeof(buf) <= sizeof(line) */
	l = (int)(buf + n - p);
	if (l > 0) {
		memcpy(line, p, n);
		linep = line + l;
	}
}

/*
 * print_string - print a readable representation of a string using
 * printer.
 */
void
print_string(p, len, printer, arg)
    char *p;
    int len;
    void (*printer) __P((void *, char *, ...));
    void *arg;
{
    int c;

    printer(arg, "\"");
    for (; len > 0; --len) {
	c = *p++;
	if (' ' <= c && c <= '~') {
	    if (c == '\\' || c == '"')
		printer(arg, "\\");
	    printer(arg, "%c", c);
	} else {
	    switch (c) {
	    case '\n':
		printer(arg, "\\n");
		break;
	    case '\r':
		printer(arg, "\\r");
		break;
	    case '\t':
		printer(arg, "\\t");
		break;
	    default:
		printer(arg, "\\%.3o", c);
	    }
	}
    }
    printer(arg, "\"");
}

/*
 * logit - does the hard work for fatal et al.
 */
static void
logit(level, fmt, args)
    int level;
    char *fmt;
    va_list args;
{
    int n;
#ifdef __APPLE__
    char buf[4096];
#else
    char buf[1024];
#endif

    n = vslprintf(buf, sizeof(buf), fmt, args);
    log_write(level, buf);
}

static void
log_write(level, buf)
    int level;
    char *buf;
{
#ifdef __APPLE__
	time_t t;
	int ns;
	char s[64];
#endif

	sys_log(level, "%s", buf);
	if (log_to_fd >= 0 && (level != LOG_DEBUG || debug)) {
		int n = (int)strlen(buf);

#ifdef __APPLE__
		time(&t);
		ns = (int)strftime(s, sizeof(s), "%c : ", localtime(&t));
		if (write(log_to_fd, s, ns) != ns)
			log_to_fd = -1;
#endif

		if (n > 0 && buf[n-1] == '\n')
			--n;
		if (write(log_to_fd, buf, n) != n
			|| write(log_to_fd, "\n", 1) != 1)
			log_to_fd = -1;
	}
}

/*
 * fatal - log an error message and die horribly.
 */
void
fatal __V((char *fmt, ...))
{
    va_list pvar;

#if defined(__STDC__)
    va_start(pvar, fmt);
#else
    char *fmt;
    va_start(pvar);
    fmt = va_arg(pvar, char *);
#endif

    logit(LOG_ERR, fmt, pvar);
    va_end(pvar);

    die(1);			/* as promised */
}

/*
 * error - log an error message.
 */
void
error __V((char *fmt, ...))
{
    va_list pvar;

#if defined(__STDC__)
    va_start(pvar, fmt);
#else
    char *fmt;
    va_start(pvar);
    fmt = va_arg(pvar, char *);
#endif

    logit(LOG_ERR, fmt, pvar);
    va_end(pvar);
    ++error_count;
}

/*
 * warn - log a warning message.
 */ 
void
#ifdef __APPLE__
warning
#else 
warn
#endif
 __V((char *fmt, ...))
{
    va_list pvar;

#if defined(__STDC__)
    va_start(pvar, fmt);
#else
    char *fmt;
    va_start(pvar);
    fmt = va_arg(pvar, char *);
#endif

    logit(LOG_WARNING, fmt, pvar);
    va_end(pvar);
}

/*
 * notice - log a notice-level message.
 */
void
notice __V((char *fmt, ...))
{
    va_list pvar;

#if defined(__STDC__)
    va_start(pvar, fmt);
#else
    char *fmt;
    va_start(pvar);
    fmt = va_arg(pvar, char *);
#endif

    logit(LOG_NOTICE, fmt, pvar);
    va_end(pvar);
}

/*
 * info - log an informational message.
 */
void
info __V((char *fmt, ...))
{
    va_list pvar;

#if defined(__STDC__)
    va_start(pvar, fmt);
#else
    char *fmt;
    va_start(pvar);
    fmt = va_arg(pvar, char *);
#endif

    logit(LOG_INFO, fmt, pvar);
    va_end(pvar);
}

/*
 * dbglog - log a debug message.
 */
void
dbglog __V((char *fmt, ...))
{
    va_list pvar;

#if defined(__STDC__)
    va_start(pvar, fmt);
#else
    char *fmt;
    va_start(pvar);
    fmt = va_arg(pvar, char *);
#endif

    logit(LOG_DEBUG, fmt, pvar);
    va_end(pvar);
}

/*
 * dump_packet - print out a packet in readable form if it is interesting.
 * Assumes len >= PPP_HDRLEN.
 */
void
dump_packet(const char *tag, unsigned char *p, int len)
{
    int proto;

    if (!debug)
	return;

    /*
     * don't print LCP echo request/reply packets if debug <= 1
     * and the link is up.
     */
    proto = (p[2] << 8) + p[3];
    if (debug <= 1 && unsuccess == 0 && proto == PPP_LCP
	&& len >= PPP_HDRLEN + HEADERLEN) {
	unsigned char *lcp = p + PPP_HDRLEN;
	int l = (lcp[2] << 8) + lcp[3];
	if ((lcp[0] == ECHOREQ || lcp[0] == ECHOREP
#ifdef __APPLE__
            || lcp[0] == TIMEREMAINING
#endif
        )
	    && l >= HEADERLEN && l <= len - PPP_HDRLEN)
	    return;
    }

    dbglog("%s %P", tag, p, len);
}

/*
 * complete_read - read a full `count' bytes from fd,
 * unless end-of-file or an error other than EINTR is encountered.
 */
ssize_t
complete_read(int fd, void *buf, size_t count)
{
	size_t done;
	ssize_t nb;
	char *ptr = buf;

	for (done = 0; done < count; ) {
		nb = read(fd, ptr, count - done);
		if (nb < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		if (nb == 0)
			break;
		done += nb;
		ptr += nb;
	}
	return done;
}

/* Procedures for locking the serial device using a lock file. */
#ifndef LOCK_DIR
#ifdef __linux__
#define LOCK_DIR	"/var/lock"
#else
#ifdef SVR4
#define LOCK_DIR	"/var/spool/locks"
#else
#define LOCK_DIR	"/var/spool/lock"
#endif
#endif
#endif /* LOCK_DIR */

static char lock_file[MAXPATHLEN];

/*
 * lock - create a lock file for the named device
 */
int
lock(dev)
    char *dev;
{
#ifdef LOCKLIB
    int result;

    result = mklock (dev, (void *) 0);
    if (result == 0) {
	strlcpy(lock_file, dev, sizeof(lock_file));
	return 0;
    }

    if (result > 0)
        notice("Device %s is locked by pid %d", dev, result);
    else
	error("Can't create lock file %s", lock_file);
    return -1;

#else /* LOCKLIB */

    char lock_buffer[12];
    int fd, pid, n;

#ifdef SVR4
    struct stat sbuf;

    if (stat(dev, &sbuf) < 0) {
	error("Can't get device number for %s: %m", dev);
	return -1;
    }
    if ((sbuf.st_mode & S_IFMT) != S_IFCHR) {
	error("Can't lock %s: not a character device", dev);
	return -1;
    }
    slprintf(lock_file, sizeof(lock_file), "%s/LK.%03d.%03d.%03d",
	     LOCK_DIR, major(sbuf.st_dev),
	     major(sbuf.st_rdev), minor(sbuf.st_rdev));
#else
    char *p;
    char lockdev[MAXPATHLEN];

    if ((p = strstr(dev, "dev/")) != NULL) {
	dev = p + 4;
	strncpy(lockdev, dev, MAXPATHLEN-1);
	lockdev[MAXPATHLEN-1] = 0;
	while ((p = strrchr(lockdev, '/')) != NULL) {
	    *p = '_';
	}
	dev = lockdev;
    } else
	if ((p = strrchr(dev, '/')) != NULL)
	    dev = p + 1;

    slprintf(lock_file, sizeof(lock_file), "%s/LCK..%s", LOCK_DIR, dev);
#endif

    while ((fd = open(lock_file, O_EXCL | O_CREAT | O_RDWR, 0644)) < 0) {
	if (errno != EEXIST) {
	    error("Can't create lock file %s: %m", lock_file);
	    break;
	}

	/* Read the lock file to find out who has the device locked. */
	fd = open(lock_file, O_RDONLY, 0);
	if (fd < 0) {
	    if (errno == ENOENT) /* This is just a timing problem. */
		continue;
	    error("Can't open existing lock file %s: %m", lock_file);
	    break;
	}
#ifndef LOCK_BINARY
	n = (int)read(fd, lock_buffer, 11);
#else
	n = (int)read(fd, &pid, sizeof(pid));
#endif /* LOCK_BINARY */
	close(fd);
	fd = -1;
	if (n <= 0) {
	    error("Can't read pid from lock file %s", lock_file);
	    break;
	}

	/* See if the process still exists. */
#ifndef LOCK_BINARY
	lock_buffer[n] = 0;
	pid = atoi(lock_buffer);
#endif /* LOCK_BINARY */
	if (pid == getpid())
	    return 1;		/* somebody else locked it for us */
	if (pid == 0
	    || (kill(pid, 0) == -1 && errno == ESRCH)) {
	    if (unlink (lock_file) == 0) {
		notice("Removed stale lock on %s (pid %d)", dev, pid);
		continue;
	    }
	    warning("Couldn't remove stale lock on %s", dev);
	} else
	    notice("Device %s is locked by pid %d", dev, pid);
	break;
    }

    if (fd < 0) {
	lock_file[0] = 0;
	return -1;
    }

    pid = getpid();
#ifndef LOCK_BINARY
    slprintf(lock_buffer, sizeof(lock_buffer), "%10d\n", pid);
    write (fd, lock_buffer, 11);
#else
    write(fd, &pid, sizeof (pid));
#endif
    close(fd);
    return 0;

#endif
}

/*
 * relock - called to update our lockfile when we are about to detach,
 * thus changing our pid (we fork, the child carries on, and the parent dies).
 * Note that this is called by the parent, with pid equal to the pid
 * of the child.  This avoids a potential race which would exist if
 * we had the child rewrite the lockfile (the parent might die first,
 * and another process could think the lock was stale if it checked
 * between when the parent died and the child rewrote the lockfile).
 */
int
relock(pid)
    int pid;
{
#ifdef LOCKLIB
    /* XXX is there a way to do this? */
    return -1;
#else /* LOCKLIB */

    int fd;
    char lock_buffer[12];

    if (lock_file[0] == 0)
	return -1;
    fd = open(lock_file, O_WRONLY, 0);
    if (fd < 0) {
	error("Couldn't reopen lock file %s: %m", lock_file);
	lock_file[0] = 0;
	return -1;
    }

#ifndef LOCK_BINARY
    slprintf(lock_buffer, sizeof(lock_buffer), "%10d\n", pid);
    write (fd, lock_buffer, 11);
#else
    write(fd, &pid, sizeof(pid));
#endif /* LOCK_BINARY */
    close(fd);
    return 0;

#endif /* LOCKLIB */
}

/*
 * unlock - remove our lockfile
 */
void
unlock()
{
    if (lock_file[0]) {
#ifdef LOCKLIB
	(void) rmlock(lock_file, (void *) 0);
#else
	unlink(lock_file);
#endif
	lock_file[0] = 0;
    }
}

static char unknown_if_family_str[16];
static char *
if_family2ascii (u_int32_t if_family)
{
	switch (if_family) {
		case APPLE_IF_FAM_LOOPBACK:
			return("Loopback");
		case APPLE_IF_FAM_ETHERNET:
			return("Ether");
		case APPLE_IF_FAM_SLIP:
			return("SLIP");
		case APPLE_IF_FAM_TUN:
			return("TUN");
		case APPLE_IF_FAM_VLAN:
			return("VLAN");
		case APPLE_IF_FAM_PPP:
			return("PPP");
		case APPLE_IF_FAM_PVC:
			return("PVC");
		case APPLE_IF_FAM_DISC:
			return("DISC");
		case APPLE_IF_FAM_MDECAP:
			return("MDECAP");
		case APPLE_IF_FAM_GIF:
			return("GIF");
		case APPLE_IF_FAM_FAITH:
			return("FAITH");
		case APPLE_IF_FAM_STF:
			return("STF");
		case APPLE_IF_FAM_FIREWIRE:
			return("FireWire");
		case APPLE_IF_FAM_BOND:
			return("Bond");
		default:
			snprintf(unknown_if_family_str, sizeof(unknown_if_family_str), "%d", if_family);
			return(unknown_if_family_str);
	}
}

void
log_vpn_interface_address_event (const char                  *location,
								 struct kern_event_msg *ev_msg,
								 int                    wait_interface_timeout,
								 u_char                  *interface,
								 struct in_addr        *our_address)
{
	struct in_addr mask;
	char   our_addr_str[INET_ADDRSTRLEN];

	if (!ev_msg) {
		notice("%s: %d secs TIMEOUT waiting for interface to be reconfigured. previous setting (name: %s, address: %s).",
			   location,
			   wait_interface_timeout,
			   interface,
			   addr2ascii(AF_INET, our_address, sizeof(*our_address), our_addr_str));
		return;
	} else {
		struct kev_in_data      *inetdata = (struct kev_in_data *) &ev_msg->event_data[0];
		struct kev_in_collision *inetdata_coll = (struct kev_in_collision *) &ev_msg->event_data[0];
		char                     new_addr_str[INET_ADDRSTRLEN];
		char                     new_mask_str[INET_ADDRSTRLEN];
		char                     dst_addr_str[INET_ADDRSTRLEN];		

		mask.s_addr = ntohl(inetdata->ia_subnetmask);

		switch (ev_msg->event_code) {
			case KEV_INET_NEW_ADDR:
				notice("%s: Address added. previous interface setting (name: %s, address: %s), current interface setting (name: %s%d, family: %s, address: %s, subnet: %s, destination: %s).",
					   location,
					   interface,
					   addr2ascii(AF_INET, our_address, sizeof(*our_address), our_addr_str),
					   inetdata->link_data.if_name, inetdata->link_data.if_unit,
					   if_family2ascii(inetdata->link_data.if_family),
					   addr2ascii(AF_INET, &inetdata->ia_addr, sizeof(inetdata->ia_addr), new_addr_str),
					   addr2ascii(AF_INET, &mask, sizeof(mask), new_mask_str),
					   addr2ascii(AF_INET, &inetdata->ia_dstaddr, sizeof(inetdata->ia_dstaddr), dst_addr_str));
				break;
			case KEV_INET_CHANGED_ADDR:
				notice("%s: Address changed. previous interface setting (name: %s, address: %s), current interface setting (name: %s%d, family: %s, address: %s, subnet: %s, destination: %s).",
					   location,
					   interface,
					   addr2ascii(AF_INET, our_address, sizeof(*our_address), our_addr_str),
					   inetdata->link_data.if_name, inetdata->link_data.if_unit,
					   if_family2ascii(inetdata->link_data.if_family),
					   addr2ascii(AF_INET, &inetdata->ia_addr, sizeof(inetdata->ia_addr), new_addr_str),
					   addr2ascii(AF_INET, &mask, sizeof(mask), new_mask_str),
					   addr2ascii(AF_INET, &inetdata->ia_dstaddr, sizeof(inetdata->ia_dstaddr), dst_addr_str));
				break;
			case KEV_INET_ADDR_DELETED:
				notice("%s: Address deleted. previous interface setting (name: %s, address: %s), deleted interface setting (name: %s%d, family: %s, address: %s, subnet: %s, destination: %s).",
					   location,
					   interface,
					   addr2ascii(AF_INET, our_address, sizeof(*our_address), our_addr_str),
					   inetdata->link_data.if_name, inetdata->link_data.if_unit,
					   if_family2ascii(inetdata->link_data.if_family),
					   addr2ascii(AF_INET, &inetdata->ia_addr, sizeof(inetdata->ia_addr), new_addr_str),
					   addr2ascii(AF_INET, &mask, sizeof(mask), new_mask_str),
					   addr2ascii(AF_INET, &inetdata->ia_dstaddr, sizeof(inetdata->ia_dstaddr), dst_addr_str));
				break;
			case KEV_INET_ARPCOLLISION:
				notice("%s: ARP collided. previous interface setting (name: %s, address: %s), conflicting interface setting (name: %s%d, family: %s, address: %s, mac: %x:%x:%x:%x:%x:%x).",
					   location,
					   interface,
					   addr2ascii(AF_INET, our_address, sizeof(*our_address), our_addr_str),
					   inetdata_coll->link_data.if_name,
					   inetdata_coll->link_data.if_unit,
					   if_family2ascii(inetdata_coll->link_data.if_family),
					   addr2ascii(AF_INET, &inetdata_coll->ia_ipaddr, sizeof(inetdata_coll->ia_ipaddr), new_addr_str),
					   inetdata_coll->hw_addr[5],inetdata_coll->hw_addr[4],inetdata_coll->hw_addr[3],inetdata_coll->hw_addr[2],inetdata_coll->hw_addr[1],inetdata_coll->hw_addr[0]);
				break;
			default:
				notice("%s: Other Address event (%d). previous interface setting (name: %s, address: %s), other interface setting (name: %s%d, family: %s, address: %s, subnet: %s, destination: %s).",
					   location,
					   ev_msg->event_code,
					   interface,
					   addr2ascii(AF_INET, our_address, sizeof(*our_address), our_addr_str),
					   inetdata->link_data.if_name, inetdata->link_data.if_unit,
					   if_family2ascii(inetdata->link_data.if_family),
					   addr2ascii(AF_INET, &inetdata->ia_addr, sizeof(inetdata->ia_addr), new_addr_str),
					   addr2ascii(AF_INET, &mask, sizeof(mask), new_mask_str),
					   addr2ascii(AF_INET, &inetdata->ia_dstaddr, sizeof(inetdata->ia_dstaddr), dst_addr_str));
				break;
		}
	}
}

int
check_vpn_interface_or_service_unrecoverable (SCDynamicStoreRef dynamicStoreRef,
					      const char                  *location,
					      struct kern_event_msg *ev_msg,
						  char                  *interface_buf)
{
	//SCDynamicStoreRef dynamicStoreRef = (SCDynamicStoreRef)dynamicStore;

	// return 1, if this is a delete event, and;
	// TODO: add support for IPv6 <rdar://problem/5920237>
	// walk Setup:/Network/Service/* and check if there are service entries referencing this interface. e.g. Setup:/Network/Service/44DB8790-0177-4F17-8D4E-37F9413D1D87/Interface:DeviceName == interface, other_serv_found = 1
	// Setup:/Network/Interface/"interface"/AirPort:'PowerEnable' == 0 || Setup:/Network/Interface/"interface"/IPv4 is missing, interf_down = 1
	if (!dynamicStoreRef)
		dbglog("%s: invalid SCDynamicStore reference", location);

	if (dynamicStoreRef &&
	    (ev_msg->event_code == KEV_INET_ADDR_DELETED || ev_msg->event_code == KEV_INET_CHANGED_ADDR)) {
		CFStringRef       interf_key;
		CFMutableArrayRef interf_keys;
		CFStringRef       pattern;
		CFMutableArrayRef patterns;
		CFDictionaryRef   dict = NULL;
		CFIndex           i;
		const void *      keys_q[128];
		const void **     keys = keys_q;
		const void *      values_q[128];
		const void **     values = values_q;
		CFIndex           n;
		CFStringRef       vpn_if;
		int               other_serv_found = 0, interf_down = 0;

		vpn_if = CFStringCreateWithCStringNoCopy(NULL,
							 interface_buf,
							 kCFStringEncodingASCII,
							 kCFAllocatorNull);
		if (!vpn_if) {
			// if we could not initialize interface CFString
			notice("%s: failed to initialize interface CFString", location);
			goto done;
		}

		interf_keys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		patterns = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		// get Setup:/Network/Interface/<vpn_if>/Airport
		interf_key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
									   kSCDynamicStoreDomainSetup,
									   vpn_if,
									   kSCEntNetAirPort);
		CFArrayAppendValue(interf_keys, interf_key);
		CFRelease(interf_key);
		// get State:/Network/Interface/<vpn_if>/Airport
		interf_key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
									   kSCDynamicStoreDomainState,
									   vpn_if,
									   kSCEntNetAirPort);
		CFArrayAppendValue(interf_keys, interf_key);
		CFRelease(interf_key);
		// get Setup:/Network/Service/*/Interface
		pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
								      kSCDynamicStoreDomainSetup,
								      kSCCompAnyRegex,
								      kSCEntNetInterface);
		CFArrayAppendValue(patterns, pattern);
		CFRelease(pattern);
		// get Setup:/Network/Service/*/IPv4
		pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
								      kSCDynamicStoreDomainSetup,
								      kSCCompAnyRegex,
								      kSCEntNetIPv4);
		CFArrayAppendValue(patterns, pattern);
		CFRelease(pattern);
		dict = SCDynamicStoreCopyMultiple(dynamicStoreRef, interf_keys, patterns);
		CFRelease(interf_keys);
		CFRelease(patterns);

		if (!dict) {
			// if we could not access the SCDynamicStore
			notice("%s: failed to initialize SCDynamicStore dictionary", location);
			CFRelease(vpn_if);
			goto done;
		}
		// look for the service which matches the provided prefixes
		n = CFDictionaryGetCount(dict);
		if (n <= 0) {
			notice("%s: empty SCDynamicStore dictionary", location);
			CFRelease(vpn_if);
			goto done;
		}
		if (n > (CFIndex)(sizeof(keys_q) / sizeof(CFTypeRef))) {
			keys   = CFAllocatorAllocate(NULL, n * sizeof(CFTypeRef), 0);
			values = CFAllocatorAllocate(NULL, n * sizeof(CFTypeRef), 0);
		}
		CFDictionaryGetKeysAndValues(dict, keys, values);
		for (i=0; i < n; i++) {
			CFStringRef     s_key  = (CFStringRef)keys[i];
			CFDictionaryRef s_dict = (CFDictionaryRef)values[i];
			CFStringRef     s_if;

			if (!isA_CFString(s_key) || !isA_CFDictionary(s_dict)) {
				continue;
			}

			if (CFStringHasSuffix(s_key, kSCEntNetInterface)) {
				// is a Service Interface entity
				s_if = CFDictionaryGetValue(s_dict, kSCPropNetInterfaceDeviceName);
				if (isA_CFString(s_if) && CFEqual(vpn_if, s_if)) {
					CFArrayRef        components;
					CFStringRef       serviceIDRef = NULL, serviceKey = NULL;
					CFPropertyListRef serviceRef = NULL;

					other_serv_found = 1;
					// extract service ID
					components = CFStringCreateArrayBySeparatingStrings(NULL, s_key, CFSTR("/"));
					if (CFArrayGetCount(components) > 3) {
						serviceIDRef = CFArrayGetValueAtIndex(components, 3);
						//if (new key) Setup:/Network/Service/service_id/IPv4 is missing, then interf_down = 1
						serviceKey = SCDynamicStoreKeyCreateNetworkServiceEntity(0, kSCDynamicStoreDomainSetup, serviceIDRef, kSCEntNetIPv4);
						if (!serviceKey ||
						    !(serviceRef = CFDictionaryGetValue(dict, serviceKey))) {
							notice("%s: detected disabled IPv4 Config", location);
							interf_down = 1;
						}
						if (serviceKey) CFRelease(serviceKey);
					}
					if (components) CFRelease(components);
					if (interf_down) break;
				}
				continue;
			} else if (CFStringHasSuffix(s_key, kSCEntNetAirPort)) {
				// Interface/<vpn_if>/Airport entity
				if (CFStringHasPrefix(s_key, kSCDynamicStoreDomainSetup)) {
					CFBooleanRef powerEnable = CFDictionaryGetValue(s_dict, SC_AIRPORT_POWERENABLED_KEY);
					if (isA_CFBoolean(powerEnable) &&
					    CFEqual(powerEnable, kCFBooleanFalse)) {
						notice("%s: detected AirPort, PowerEnable == FALSE", location);
						interf_down = 1;
						break;
					}
				} else if (CFStringHasPrefix(s_key, kSCDynamicStoreDomainState)) {
					UInt16      temp;
					CFNumberRef airStatus = CFDictionaryGetValue(s_dict, SC_AIRPORT_POWERSTATUS_KEY);
					if (isA_CFNumber(airStatus) &&
					    CFNumberGetValue(airStatus, kCFNumberShortType, &temp)) {
						if (temp ==0) {
							notice("%s: detected AirPort, PowerStatus == 0", location);
						}
					}
				}
				continue;
			}
		}
		if (vpn_if) CFRelease(vpn_if);
		if (keys != keys_q) {
			CFAllocatorDeallocate(NULL, keys);
			CFAllocatorDeallocate(NULL, values);
		}
done :
		if (dict) CFRelease(dict);

		return (other_serv_found == 0 || interf_down == 1);             
	}
	return 0;
}

int
check_vpn_interface_address_change (int                    transport_down,
                                    struct kern_event_msg *ev_msg,
                                    char                  *interface_buf,
									int                    interface_media,
                                    struct in_addr        *our_address)
{
    struct kev_in_data *inetdata;
    
    /* if transport is still down: ignore deletes, and check if the underlying interface's address has changed (ignore link-local addresses) */
    if (transport_down &&
        (ev_msg->event_code == KEV_INET_NEW_ADDR || ev_msg->event_code == KEV_INET_CHANGED_ADDR)) {
 		inetdata = (struct kev_in_data *) &ev_msg->event_data[0];
#if 0
        notice("%s: checking for interface address change. underlying %s, old-addr %x, new-addr %x\n",
               __FUNCTION__, interface_buf, our_address->s_addr, inetdata->ia_addr.s_addr);
#endif
        /* check if address changed */
        if (our_address->s_addr != inetdata->ia_addr.s_addr &&
            !IN_LINKLOCAL(ntohl(inetdata->ia_addr.s_addr))) {
            return 1;
        }
    }
    
    return 0;
}

int
check_vpn_interface_alternate (int                    transport_down,
                               struct kern_event_msg *ev_msg,
                               char                  *interface_buf)
{
    struct kev_in_data *inetdata;

    /* if transport is still down: ignore deletes, and check if an alternative interface has a valid address (ignore link-local) */
    if (transport_down &&
        (ev_msg->event_code == KEV_INET_NEW_ADDR || ev_msg->event_code == KEV_INET_CHANGED_ADDR)) {
 		inetdata = (struct kev_in_data *) &ev_msg->event_data[0];
#if 0        
        notice("%s: checking for alternate interface. underlying %s, new-addr %x\n",
               __FUNCTION__, interface_buf, inetdata->ia_addr.s_addr);
#endif
        if (!IN_LINKLOCAL(ntohl(inetdata->ia_addr.s_addr))) {
            return 1;
        }
    }

    return 0;
}

#if 0
/* 
 * print_hex - hexdump (in out buffer) of binary data (in buffer), for count bytes
 */
static void
print_hex (unsigned char *out, unsigned char *in, int count)
{
	register unsigned char next_ch;
	static char hex[] = "0123456789abcdef";
	
	while (count-- > 0) {
		next_ch = *in++;
		*out++ = hex[(next_ch >> 4) & 0x0F];
		*out++ = hex[next_ch & 0x0F];
	}
	
	*out = '\0';
}

/*
 * dump_mppe_keys - print out mppe send/recv keys 
 */
void
dump_buffer(char *caller, unsigned char* binbuf, int size)
{	
	if(binbuf)
	{
		static unsigned char buf[65];
		
		print_hex(buf, binbuf, size);
		error("%s: data (%d bits) = %s",caller, size<<3, buf);
	}
}
#endif
