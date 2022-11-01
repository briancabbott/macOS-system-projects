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
 * options.c - handles option processing for PPP.
 *
 * Copyright (c) 1984-2000 Carnegie Mellon University. All rights reserved.
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
 *    prior written permission. For permission or any legal
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

#define RCSID	"$Id: options.c,v 1.24 2005/11/03 05:25:59 lindak Exp $"

#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <pwd.h>
#ifdef PLUGIN
#ifdef __APPLE__
#include <sys/un.h>
#include <sys/ucred.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <libgen.h>
#else
#include <dlfcn.h>
#endif
#endif
#ifdef PPP_FILTER
#include <pcap.h>
#include <pcap-int.h>	/* XXX: To get struct pcap */
#endif

#include "pppd.h"
#include "pathnames.h"

#if defined(ultrix) || defined(NeXT)
char *strdup __P((char *));
#endif

#ifndef lint
static const char rcsid[] = RCSID;
#endif

struct option_value {
    struct option_value *next;
    const char *source;
    char value[1];
};

/*
 * Option variables and default values.
 */
#ifdef PPP_FILTER
int	dflag = 0;		/* Tell libpcap we want debugging */
#endif
int	debug = 0;		/* Debug flag */
int	kdebugflag = 0;		/* Tell kernel to print debug messages */
int	default_device = 1;	/* Using /dev/tty or equivalent */
char	devnam[MAXPATHLEN] = { 0 };	/* Device name */
bool	nodetach = 0;		/* Don't detach from controlling tty */
bool	updetach = 0;		/* Detach once link is up */
int	maxconnect = 0;		/* Maximum connect time */
char	user[MAXNAMELEN] = { 0 };	/* Username for PAP */
#ifdef __APPLE__
bool	controlled = 0;		/* Is pppd controlled by the PPPController ?  */
FILE 	*controlfile = NULL;	/* file descriptor for options and control */
int 	controlfd = -1;		/* file descriptor for options and control */
int 	statusfd = -1;		/* file descriptor status update */
char	username[MAXNAMELEN] = { 0 };	/* copy original user */
char	new_passwd[MAXSECRETLEN] = { 0 };	/* new password for protocol supporting changing password */
int		passwdfrom = PASSWDFROM_UNKNOWN;
char	passwdkey[MAXSECRETLEN] = { 0 };	/* keychain key where the password is located, when itcomes from the keychain  */
#endif
char	passwd[MAXSECRETLEN] = { 0 };	/* Password for PAP */
bool	persist = 0;		/* Reopen link after it goes down */
char	our_name[MAXNAMELEN] = { 0 };	/* Our name for authentication purposes */
bool	demand = 0;		/* do dial-on-demand */
char	*ipparam = NULL;	/* Extra parameter for ip up/down scripts */
int	idle_time_limit = 0;	/* Disconnect if idle for this many seconds */
bool   	noidlerecv = 0;         /* Disconnect if idle only for outgoing traffic */
bool   	noidlesend = 0;         /* Disconnect if idle only for incoming traffic */
int	holdoff = 30;		/* # seconds to pause before reconnecting */
bool	holdoff_specified = FALSE;	/* true if a holdoff value has been given */
int	log_to_fd = 1;		/* send log messages to this fd too */
bool	log_default = 1;	/* log_to_fd is default (stdout) */
int	maxfail = 10;		/* max # of unsuccessful connection attempts */
char	linkname[MAXPATHLEN] = { 0 };	/* logical name for link */
bool	tune_kernel = FALSE;		/* may alter kernel settings */
int	connect_delay = 1000;	/* wait this many ms after connect script */
int	req_unit = -1;		/* requested interface unit */
bool	multilink = 0;		/* Enable multilink operation */
char	*bundle_name = NULL;	/* bundle name for multilink */
bool	dump_options = FALSE;		/* print out option values */
bool	dryrun = FALSE;			/* print out option values and exit */
char	*domain = NULL;		/* domain name set by domain option */

#ifdef MAXOCTETS
unsigned int  maxoctets = 0;    /* default - no limit */
int maxoctets_dir = 0;       /* default - sum of traffic */
int maxoctets_timeout = 1;   /* default 1 second */ 
#endif

#ifdef __APPLE__
char 	*device = NULL; 	/* device we are using (can be use as a generic device container) */
char 	*remoteaddress = NULL; 	/* remoteaddress we are connecting to (can be use as a generic address container) */
char 	*altremoteaddress = NULL; /* alternate remoteaddress we are connecting to */
int 	redialcount = 0;	/* number of time to redial */
int 	redialtimer = 30;	/* delay in seconds to wait before to redial */
bool 	redialalternate = 0;  	/* do we redial alternate number */
int 	busycode = -1;		/* busy error code that triggers the redial */
bool	hasbusystate = 1;	/* change phase to report busy state */
int 	cancelcode = -1;	/* cancel error code for connectors */
int 	extraconnecttime = 0;	/* allows extra connection time to the connection sequence */
bool	holdfirst = 0;	/* apply holdoff timer when starting pppd, useful to delay dialondemand */
char	*ifscope = NULL; /* set by ifscope option */
#endif


extern option_t auth_options[];
extern struct stat devstat;

#ifdef PPP_FILTER
struct	bpf_program pass_filter;/* Filter program for packets to pass */
struct	bpf_program active_filter; /* Filter program for link-active pkts */
pcap_t  pc;			/* Fake struct pcap so we can compile expr */
#endif

char *current_option = NULL;		/* the name of the option being parsed */
int  privileged_option = 0;		/* set iff the current option came from root */
char *option_source = NULL;		/* string saying where the option came from */
int  option_priority = OPRIO_CFGFILE; /* priority of the current options */
bool devnam_fixed = FALSE;		/* can no longer change device name */

static int logfile_fd = -1;	/* fd opened for log file */
static char logfile_name[MAXPATHLEN];	/* name of log file */

/*
 * Prototypes
 */
static int setdomain __P((char **));
static int readfile __P((char **));
static int callfile __P((char **));
static int showversion __P((char **));
static int showhelp __P((char **));
static void usage __P((void));
static int setlogfile __P((char **));
#ifdef PLUGIN
static int loadplugin __P((char **));
static int loadplugin2 __P((char **));
#endif
#ifdef __APPLE__
static int controlled_connection __P((char **));
#endif

#ifdef PPP_FILTER
static int setpassfilter __P((char **));
static int setactivefilter __P((char **));
#endif

#ifdef MAXOCTETS
static int setmodir __P((char **));
#endif

static option_t *find_option __P((const char *name));
static int process_option __P((option_t *, char *, char **));
static int n_arguments __P((option_t *));
static int number_option __P((char *, u_int32_t *, int));

/*
 * Structure to store extra lists of options.
 */
struct option_list {
    option_t *options;
    struct option_list *next;
};

static struct option_list *extra_options = NULL;

/*
 * Valid arguments.
 */
option_t general_options[] = {
    { "debug", o_int, &debug,
      "Increase debugging level", OPT_INC | OPT_NOARG | 1 },
    { "-d", o_int, &debug,
      "Increase debugging level",
      OPT_ALIAS | OPT_INC | OPT_NOARG | 1 },

    { "kdebug", o_int, &kdebugflag,
      "Set kernel driver debug level", OPT_PRIO },

    { "nodetach", o_bool, &nodetach,
      "Don't detach from controlling tty", OPT_PRIO | 1 },
    { "-detach", o_bool, &nodetach,
      "Don't detach from controlling tty", OPT_ALIAS | OPT_PRIOSUB | 1 },
    { "updetach", o_bool, &updetach,
      "Detach from controlling tty once link is up",
      OPT_PRIOSUB | OPT_A2CLR | 1, &nodetach },

    { "holdoff", o_int, &holdoff,
      "Set time in seconds before retrying connection", OPT_PRIO },

    { "idle", o_int, &idle_time_limit,
      "Set time in seconds before disconnecting idle link", OPT_PRIO, 0, 0, 0xFFFFFFFF, 0, 0, 0, option_change_idle },
#ifdef __APPLE__
    { "holdfirst", o_bool, &holdfirst,
      "Apply holdoff timer first", OPT_PRIO | 1 },
    { "noidlerecv", o_bool, &noidlerecv,
      "Don't check receive traffic for idle timer", OPT_PRIO | 1 },
    { "noidlesend", o_bool, &noidlesend,
      "Don't check send traffic for idle timer", OPT_PRIO | 1 },
    { "ifscope", o_string, &ifscope,
      "Scope connections to interface", OPT_PRIO | 1 },
#endif

    { "maxconnect", o_int, &maxconnect,
      "Set connection time limit",
      OPT_PRIO | OPT_LLIMIT | OPT_NOINCR | OPT_ZEROINF },

    { "domain", o_special, (void *)setdomain,
      "Add given domain name to hostname",
      OPT_PRIO | OPT_PRIV | OPT_A2STRVAL, &domain },

    { "file", o_special, (void *)readfile,
      "Take options from a file", OPT_NOPRINT },
    { "call", o_special, (void *)callfile,
      "Take options from a privileged file", OPT_NOPRINT },

    { "persist", o_bool, &persist,
      "Keep on reopening connection after close", OPT_PRIO | 1 },
    { "nopersist", o_bool, &persist,
      "Turn off persist option", OPT_PRIOSUB },

    { "demand", o_bool, &demand,
      "Dial on demand", OPT_INITONLY | 1, &persist },

    { "--version", o_special_noarg, (void *)showversion,
      "Show version number" },
    { "--help", o_special_noarg, (void *)showhelp,
      "Show brief listing of options" },
    { "-h", o_special_noarg, (void *)showhelp,
      "Show brief listing of options", OPT_ALIAS },

    { "logfile", o_special, (void *)setlogfile,
      "Append log messages to this file",
      OPT_PRIO | OPT_A2STRVAL | OPT_STATIC, &logfile_name },
    { "logfd", o_int, &log_to_fd,
      "Send log messages to this file descriptor",
      OPT_PRIOSUB | OPT_A2CLR, &log_default },
    { "nolog", o_int, &log_to_fd,
      "Don't send log messages to any file",
      OPT_PRIOSUB | OPT_NOARG | OPT_VAL(-1) },
    { "nologfd", o_int, &log_to_fd,
      "Don't send log messages to any file descriptor",
      OPT_PRIOSUB | OPT_ALIAS | OPT_NOARG | OPT_VAL(-1) },

    { "linkname", o_string, linkname,
      "Set logical name for link",
      OPT_PRIO | OPT_PRIV | OPT_STATIC, NULL, MAXPATHLEN },

    { "maxfail", o_int, &maxfail,
      "Maximum number of unsuccessful connection attempts to allow",
      OPT_PRIO },

    { "ktune", o_bool, &tune_kernel,
      "Alter kernel settings as necessary", OPT_PRIO | 1 },
    { "noktune", o_bool, &tune_kernel,
      "Don't alter kernel settings", OPT_PRIOSUB },

    { "connect-delay", o_int, &connect_delay,
      "Maximum time (in ms) to wait after connect script finishes",
      OPT_PRIO },

    { "unit", o_int, &req_unit,
      "PPP interface unit number to use if possible",
      OPT_PRIO | OPT_LLIMIT | OPT_ULIMIT, 0, 0x7fff },

    { "dump", o_bool, &dump_options,
      "Print out option values after parsing all options", 1 },
    { "dryrun", o_bool, &dryrun,
      "Stop after parsing, printing, and checking options", 1 },

#ifdef HAVE_MULTILINK
    { "multilink", o_bool, &multilink,
      "Enable multilink operation", OPT_PRIO | 1 },
    { "mp", o_bool, &multilink,
      "Enable multilink operation", OPT_PRIOSUB | OPT_ALIAS | 1 },
    { "nomultilink", o_bool, &multilink,
      "Disable multilink operation", OPT_PRIOSUB | 0 },
    { "nomp", o_bool, &multilink,
      "Disable multilink operation", OPT_PRIOSUB | OPT_ALIAS | 0 },

    { "bundle", o_string, &bundle_name,
      "Bundle name for multilink", OPT_PRIO },
#endif /* HAVE_MULTILINK */

#ifdef PLUGIN
	{ "plugin", o_special, (void *)loadplugin,
	"Load a plug-in module into pppd", OPT_PRIV | OPT_A2LIST },
	{ "plugin2", o_special, (void *)loadplugin2,
	"Load a plug-in module into pppd (plugin is required)", OPT_PRIV | OPT_A2LIST },
#endif

#ifdef PPP_FILTER
    { "pdebug", o_int, &dflag,
      "libpcap debugging", OPT_PRIO },

    { "pass-filter", 1, setpassfilter,
      "set filter for packets to pass", OPT_PRIO },

    { "active-filter", 1, setactivefilter,
      "set filter for active pkts", OPT_PRIO },
#endif

#ifdef MAXOCTETS
    { "maxoctets", o_int, &maxoctets,
      "Set connection traffic limit",
      OPT_PRIO | OPT_LLIMIT | OPT_NOINCR | OPT_ZEROINF },
    { "mo", o_int, &maxoctets,
      "Set connection traffic limit",
      OPT_ALIAS | OPT_PRIO | OPT_LLIMIT | OPT_NOINCR | OPT_ZEROINF },
    { "mo-direction", o_special, setmodir,
      "Set direction for limit traffic (sum,in,out,max)" },
    { "mo-timeout", o_int, &maxoctets_timeout,
      "Check for traffic limit every N seconds", OPT_PRIO | OPT_LLIMIT | 1 },
#endif

#ifdef __APPLE__
    { "controlled", o_special_noarg, (void *)controlled_connection,
      "pppd is controlled by PPPController"},
    { "device", o_string, &device,
      "Device we are using"},
    { "remoteaddress", o_string, &remoteaddress,
      "Remote address we are connecting to"},
    { "altremoteaddress", o_string, &altremoteaddress,
      "Alternate remote address we are connecting to"},
    { "redialcount", o_int, &redialcount,
      "Number of times to redial" },
    { "redialtimer", o_int, &redialtimer,
      "Delay in seconds to wait before to redial"},
    { "redialalternate", o_bool, &redialalternate,
      "Redial also atlernate number", 1},
    { "busycode", o_int, &busycode,
      "Busy signal error code that will trigger the redial"},
    { "cancelcode", o_int, &cancelcode,
      "Cancel error code that for connectors"},
    { "extraconnecttime", o_int, &extraconnecttime,
      "Allows extra conneciton time to the connection sequence"},
    { "retrylinkcheck", o_int, &retry_pre_start_link_check,
		"Maximum number of retries for the pre-connection reachability check"},
#endif

    { NULL }
};

#ifndef IMPLEMENTATION
#define IMPLEMENTATION ""
#endif

static char *usage_string = "\
pppd version %s\n\
Usage: %s [ options ], where options are:\n\
	<device>	Communicate over the named device\n\
	<speed>		Set the baud rate to <speed>\n\
	<loc>:<rem>	Set the local and/or remote interface IP\n\
			addresses.  Either one may be omitted.\n\
	asyncmap <n>	Set the desired async map to hex <n>\n\
	auth		Require authentication from peer\n\
        connect <p>     Invoke shell command <p> to set up the serial line\n\
	crtscts		Use hardware RTS/CTS flow control\n\
	defaultroute	Add default route through interface\n\
	file <f>	Take options from file <f>\n\
	modem		Use modem control lines\n\
	mru <n>		Set MRU value to <n> for negotiation\n\
See pppd(8) for more options.\n\
";

/*
 * parse_args - parse a string of arguments from the command line.
 */
int
parse_args(argc, argv)
    int argc;
    char **argv;
{
    char *arg;
    option_t *opt;
    int n;

    privileged_option = privileged;
    option_source = "command line";
    option_priority = OPRIO_CMDLINE;
    while (argc > 0) {
	arg = *argv++;
	--argc;
	opt = find_option(arg);
	if (opt == NULL) {
	    option_error("unrecognized option '%s'", arg);
	    usage();
	    return 0;
	}
	n = n_arguments(opt);
	if (argc < n) {
	    option_error("too few parameters for option %s", arg);
	    return 0;
	}
	if (!process_option(opt, arg, argv))
	    return 0;
	argc -= n;
	argv += n;
    }
    return 1;
}

/*
 * options_from_file - Read a string of options from a file,
 * and interpret them.
 */
int
options_from_file(filename, must_exist, check_prot, priv)
    char *filename;
    int must_exist;
    int check_prot;
    int priv;
{
    FILE *f;
    int i, newline, ret, err;
    option_t *opt;
    int oldpriv, n;
    char *oldsource;
    char *argv[MAXARGS];
    char args[MAXARGS][MAXWORDLEN];
    char cmd[MAXWORDLEN];
#ifdef __APPLE__
    char *tofree;
#endif

    if (check_prot)
	seteuid(getuid());
    f = fopen(filename, "r");
    err = errno;
    if (check_prot)
	seteuid(0);
    if (f == NULL) {
	errno = err;
	if (!must_exist) {
	    if (err != ENOENT && err != ENOTDIR)
		warning("Warning: can't open options file %s: %m", filename);
	    return 1;
	}
	option_error("Can't open options file %s: %m", filename);
	return 0;
    }

    oldpriv = privileged_option;
    privileged_option = priv;
    oldsource = option_source;
    option_source = strdup(filename);
#ifdef __APPLE__
    tofree = option_source;
#endif
    if (option_source == NULL)
	option_source = "file";
    ret = 0;
    while (getword(f, cmd, &newline, filename)) {
	opt = find_option(cmd);
	if (opt == NULL) {
	    option_error("In file %s: unrecognized option '%s'",
			 filename, cmd);
	    goto err;
	}
	n = n_arguments(opt);
	for (i = 0; i < n; ++i) {
	    if (!getword(f, args[i], &newline, filename)) {
		option_error(
			"In file %s: too few parameters for option '%s'",
			filename, cmd);
		goto err;
	    }
	    argv[i] = args[i];
	}
	if (!process_option(opt, cmd, argv))
	    goto err;
#ifdef __APPLE__        
        // option source is saved by process option, don't free it 
        tofree = 0;
#endif
    }
    ret = 1;

err:
    fclose(f);
    privileged_option = oldpriv;
    option_source = oldsource;
#ifdef __APPLE__
    if (tofree)
        free(tofree);
#endif
    return ret;
}

/*
 * options_from_user - See if the use has a ~/.ppprc file,
 * and if so, interpret options from it.
 */
int
options_from_user()
{
    char *user, *path, *file;
    int ret;
    struct passwd *pw;
    size_t pl;

    pw = getpwuid(getuid());
    if (pw == NULL || (user = pw->pw_dir) == NULL || user[0] == 0)
	return 1;
    file = _PATH_USEROPT;
    pl = strlen(user) + strlen(file) + 2;
    path = malloc(pl);
    if (path == NULL)
	novm("init file name");
    slprintf(path, (int)pl, "%s/%s", user, file);
    option_priority = OPRIO_CFGFILE;
    ret = options_from_file(path, 0, 1, privileged);
    free(path);
    return ret;
}

/*
 * options_for_tty - See if an options file exists for the serial
 * device, and if so, interpret options from it.
 * We only allow the per-tty options file to override anything from
 * the command line if it is something that the user can't override
 * once it has been set by root; this is done by giving configuration
 * files a lower priority than the command line.
 */
int
options_for_tty()
{
    char *dev, *path, *p;
    int ret;
    size_t pl;

    dev = devnam;
    if ((p = strstr(dev, "/dev/")) != NULL)
	dev = p + 5;
    if (dev[0] == 0 || strcmp(dev, "tty") == 0)
	return 1;		/* don't look for /etc/ppp/options.tty */
    pl = strlen(_PATH_TTYOPT) + strlen(dev) + 1;
    path = malloc(pl);
    if (path == NULL)
	novm("tty init file name");
    slprintf(path, (int)pl, "%s%s", _PATH_TTYOPT, dev);
    /* Turn slashes into dots, for Solaris case (e.g. /dev/term/a) */
    for (p = path + strlen(_PATH_TTYOPT); *p != 0; ++p)
	if (*p == '/')
	    *p = '.';
    option_priority = OPRIO_CFGFILE;
    ret = options_from_file(path, 0, 0, 1);
    free(path);
    return ret;
}

/*
 * options_from_list - process a string of options in a wordlist.
 */
int
options_from_list(w, priv)
    struct wordlist *w;
    int priv;
{
    char *argv[MAXARGS];
    option_t *opt;
    int i, n, ret = 0;
    struct wordlist *w0;

    privileged_option = priv;
    option_source = "secrets file";
    option_priority = OPRIO_SECFILE;

    while (w != NULL) {
	opt = find_option(w->word);
	if (opt == NULL) {
	    option_error("In secrets file: unrecognized option '%s'",
			 w->word);
	    goto err;
	}
	n = n_arguments(opt);
	w0 = w;
	for (i = 0; i < n; ++i) {
	    w = w->next;
	    if (w == NULL) {
		option_error(
			"In secrets file: too few parameters for option '%s'",
			w0->word);
		goto err;
	    }
	    argv[i] = w->word;
	}
	if (!process_option(opt, w0->word, argv))
	    goto err;
	w = w->next;
    }
    ret = 1;

err:
    return ret;
}

/*
 * match_option - see if this option matches an option_t structure.
 */
static int
match_option(name, opt, dowild)
    const char *name;
    option_t *opt;
    int dowild;
{
	int (*match) __P((char *, char **, int));

	if (dowild != (opt->type == o_wild))
		return 0;
	if (!dowild)
		return strcmp(name, opt->name) == 0;
	match = (int (*) __P((char *, char **, int))) opt->addr;
	return (*match)((char *)name, NULL, 0);
}

/*
 * find_option - scan the option lists for the various protocols
 * looking for an entry with the given name.
 * This could be optimized by using a hash table.
 */
static option_t *
find_option(name)
    const char *name;
{
	option_t *opt;
	struct option_list *list;
	int i, dowild;

	for (dowild = 0; dowild <= 1; ++dowild) {
		for (opt = general_options; opt->name != NULL; ++opt)
			if (match_option(name, opt, dowild))
				return opt;
		for (opt = auth_options; opt->name != NULL; ++opt)
			if (match_option(name, opt, dowild))
				return opt;
		for (list = extra_options; list != NULL; list = list->next)
			for (opt = list->options; opt->name != NULL; ++opt)
				if (match_option(name, opt, dowild))
					return opt;
		for (opt = the_channel->options; opt->name != NULL; ++opt)
			if (match_option(name, opt, dowild))
				return opt;
		for (i = 0; protocols[i] != NULL; ++i)
			if ((opt = protocols[i]->options) != NULL)
				for (; opt->name != NULL; ++opt)
					if (match_option(name, opt, dowild))
						return opt;
	}
	return NULL;
}

/*
 * process_option - process one new-style option.
 */
static int
process_option(opt, cmd, argv)
    option_t *opt;
    char *cmd;
    char **argv;
{
    u_int32_t v;
    int iv, a;
#ifdef __APPLE__
    int i, len;
    void (*change) __P((void));
 #endif
    char *sv;
    int (*parser) __P((char **));
    int (*wildp) __P((char *, char **, int));
    char *optopt = (opt->type == o_wild)? "": " option";
    int prio = option_priority;
    option_t *mainopt = opt;

    current_option = opt->name;
    if ((opt->flags & OPT_PRIVFIX) && privileged_option)
	prio += OPRIO_ROOT;
    while (mainopt->flags & OPT_PRIOSUB)
	--mainopt;
    if (mainopt->flags & OPT_PRIO) {
	if (prio < mainopt->priority) {
	    /* new value doesn't override old */
	    if (prio == OPRIO_CMDLINE && mainopt->priority > OPRIO_ROOT) {
		option_error("%s%s set in %s cannot be overridden\n",
			     opt->name, optopt, mainopt->source);
		return 0;
	    }
	    return 1;
	}
	if (prio > OPRIO_ROOT && mainopt->priority == OPRIO_CMDLINE)
	    warning("%s%s from %s overrides command line",
		 opt->name, optopt, option_source);
    }

    if ((opt->flags & OPT_INITONLY) && phase != PHASE_INITIALIZE) {
	option_error("%s%s cannot be changed after initialization",
		     opt->name, optopt);
	return 0;
    }
    if ((opt->flags & OPT_PRIV) && !privileged_option) {
	option_error("using the %s%s requires root privilege",
		     opt->name, optopt);
	return 0;
    }
    if ((opt->flags & OPT_ENABLE) && *(bool *)(opt->addr2) == 0) {
	option_error("%s%s is disabled", opt->name, optopt);
	return 0;
    }
    if ((opt->flags & OPT_DEVEQUIV) && devnam_fixed) {
	option_error("the %s%s may not be changed in %s",
		     opt->name, optopt, option_source);
	return 0;
    }

    switch (opt->type) {
    case o_bool:
	v = opt->flags & OPT_VALUE;
	*(bool *)(opt->addr) = v;
	if (opt->addr2 && (opt->flags & OPT_A2COPY))
	    *(bool *)(opt->addr2) = v;
	else if (opt->addr2 && (opt->flags & OPT_A2CLR))
	    *(bool *)(opt->addr2) = 0;
	else if (opt->addr2 && (opt->flags & OPT_A2CLRB))
	    *(u_char *)(opt->addr2) &= ~v;
	else if (opt->addr2 && (opt->flags & OPT_A2OR))
	    *(u_char *)(opt->addr2) |= v;
	break;

    case o_int:
	iv = 0;
	if ((opt->flags & OPT_NOARG) == 0) {
	    if (!int_option(*argv, &iv))
		return 0;
	    if ((((opt->flags & OPT_LLIMIT) && iv < opt->lower_limit)
		 || ((opt->flags & OPT_ULIMIT) && iv > opt->upper_limit))
		&& !((opt->flags & OPT_ZEROOK && iv == 0))) {
		char *zok = (opt->flags & OPT_ZEROOK)? " zero or": "";
		switch (opt->flags & OPT_LIMITS) {
		case OPT_LLIMIT:
		    option_error("%s value must be%s >= %d",
				 opt->name, zok, opt->lower_limit);
		    break;
		case OPT_ULIMIT:
		    option_error("%s value must be%s <= %d",
				 opt->name, zok, opt->upper_limit);
		    break;
		case OPT_LIMITS:
		    option_error("%s value must be%s between %d and %d",
				opt->name, zok, opt->lower_limit, opt->upper_limit);
		    break;
		}
		return 0;
	    }
	}
	a = opt->flags & OPT_VALUE;
	if (a >= 128)
	    a -= 256;		/* sign extend */
	iv += a;
	if (opt->flags & OPT_INC)
	    iv += *(int *)(opt->addr);
	if ((opt->flags & OPT_NOINCR) && !privileged_option) {
	    int oldv = *(int *)(opt->addr);
	    if ((opt->flags & OPT_ZEROINF) ?
		(oldv != 0 && (iv == 0 || iv > oldv)) : (iv > oldv)) {
		option_error("%s value cannot be increased", opt->name);
		return 0;
	    }
	}
	*(int *)(opt->addr) = iv;
	if (opt->addr2 && (opt->flags & OPT_A2COPY))
	    *(int *)(opt->addr2) = iv;
	break;

    case o_uint32:
	if (opt->flags & OPT_NOARG) {
	    v = opt->flags & OPT_VALUE;
	    if (v & 0x80)
		    v |= 0xffffff00U;
	} else if (!number_option(*argv, &v, 16))
	    return 0;
	if (opt->flags & OPT_OR)
	    v |= *(u_int32_t *)(opt->addr);
	*(u_int32_t *)(opt->addr) = v;
	if (opt->addr2 && (opt->flags & OPT_A2COPY))
	    *(u_int32_t *)(opt->addr2) = v;
	break;

    case o_string:
	if (opt->flags & OPT_STATIC) {
	    strlcpy((char *)(opt->addr), *argv, opt->upper_limit);
#ifdef __APPLE__
            if (opt->addr2 && (opt->flags & OPT_A2COPY))
                strlcpy((char *)(opt->addr2), *argv, opt->upper_limit);
#endif
	} else {
	    sv = strdup(*argv);
	    if (sv == NULL)
		novm("option argument");
	    *(char **)(opt->addr) = sv;
	}
	break;

    case o_special_noarg:
	case o_special_cfarg:
    case o_special:
	parser = (int (*) __P((char **))) opt->addr;
	if (!(*parser)(argv))
	    return 0;
	if (opt->flags & OPT_A2LIST) {
	    struct option_value *ovp, **pp;

	    ovp = malloc(sizeof(*ovp) + strlen(*argv) + 1);
	    if (ovp != 0) {
		strlcpy(ovp->value, *argv, strlen(*argv) + 1);
		ovp->source = option_source;
		ovp->next = NULL;
		pp = (struct option_value **) &opt->addr2;
		while (*pp != 0)
		    pp = &(*pp)->next;
		*pp = ovp;
	    }
	}
	break;

    case o_wild:
	wildp = (int (*) __P((char *, char **, int))) opt->addr;
	if (!(*wildp)(cmd, argv, 1))
	    return 0;
	break;
    }

    if (opt->addr2 && (opt->flags & (OPT_A2COPY|OPT_ENABLE
		|OPT_A2PRINTER|OPT_A2STRVAL|OPT_A2LIST|OPT_A2OR)) == 0)
	*(bool *)(opt->addr2) = !(opt->flags & OPT_A2CLR);

#ifdef __APPLE__
    if (!mainopt->source)
		mainopt->source = option_source;
	else {
		if (!mainopt->other_source) {
			mainopt->other_source = malloc(sizeof(char*));
			if (!mainopt->other_source)
				novm("option other source");
			mainopt->other_source[0] = option_source;
			mainopt->nb_other_source = 1;
		}
		else {
			char **p = (char **)realloc(mainopt->other_source, (mainopt->nb_other_source + 1) * sizeof(char*));
			if (p) {
				mainopt->other_source = p;
				mainopt->other_source[mainopt->nb_other_source] = option_source;
				mainopt->nb_other_source++;
			}
		}
	}
#else
	mainopt->source = option_source;
#endif
    mainopt->priority = prio;
    mainopt->winner = opt - mainopt;

#ifdef __APPLE__
    if (opt->type == o_string && opt->flags & OPT_HIDE)
        for (i = 0, len = (int)strlen(*argv); i < len; (*argv)[i++] = '*');
        
    // call the change function
    if (phase != PHASE_INITIALIZE && opt->addr3) {
	change = (void (*) __P((void))) opt->addr3;
	(*change)();
    }
 #endif

    return 1;
}

/*
 * override_value - if the option priorities would permit us to
 * override the value of option, return 1 and update the priority
 * and source of the option value.  Otherwise returns 0.
 */
int
override_value(option, priority, source)
    const char *option;
    int priority;
    const char *source;
{
	option_t *opt;

	opt = find_option(option);
	if (opt == NULL)
		return 0;
	while (opt->flags & OPT_PRIOSUB)
		--opt;
	if ((opt->flags & OPT_PRIO) && priority < opt->priority)
		return 0;
	opt->priority = priority;
	opt->source = source;
	opt->winner = -1;
	return 1;
}

/*
 * n_arguments - tell how many arguments an option takes
 */
static int
n_arguments(opt)
    option_t *opt;
{
	return (opt->type == o_bool || opt->type == o_special_noarg
		|| (opt->flags & OPT_NOARG))? 0: 1;
}

/*
 * add_options - add a list of options to the set we grok.
 */
void
add_options(opt)
    option_t *opt;
{
    struct option_list *list;

    list = malloc(sizeof(*list));
    if (list == 0)
	novm("option list entry");
    list->options = opt;
    list->next = extra_options;
    extra_options = list;
}

/*
 * check_options - check that options are valid and consistent.
 */
void
check_options()
{
	if (logfile_fd >= 0 && logfile_fd != log_to_fd)
		close(logfile_fd);
}

/*
 * print_option - print out an option and its value
 */
static void
print_option(opt, mainopt, printer, arg)
    option_t *opt, *mainopt;
    void (*printer) __P((void *, char *, ...));
    void *arg;
{
	int i, v;
	char *p;

	if (opt->flags & OPT_NOPRINT)
		return;
	switch (opt->type) {
	case o_bool:
		v = opt->flags & OPT_VALUE;
		if (*(bool *)opt->addr != v &&
			!(opt->addr2 && (opt->flags & OPT_A2OR) && (*(bool *)opt->addr2 & v)))
			/* this can happen legitimately, e.g. lock
			   option turned off for default device */
			break;
		printer(arg, "%s", opt->name);
		break;
	case o_int:
		v = opt->flags & OPT_VALUE;
		if (v >= 128)
			v -= 256;
		i = *(int *)opt->addr;
		if (opt->flags & OPT_NOARG) {
			printer(arg, "%s", opt->name);
			if (i != v) {
				if (opt->flags & OPT_INC) {
					for (; i > v; i -= v)
						printer(arg, " %s", opt->name);
				} else
					printer(arg, " # oops: %d not %d\n",
						i, v);
			}
		} else {
			printer(arg, "%s %d", opt->name, i);
		}
		break;
	case o_uint32:
		printer(arg, "%s", opt->name);
		if ((opt->flags & OPT_NOARG) == 0)
			printer(arg, " %x", *(u_int32_t *)opt->addr);
		break;

	case o_string:
		if (opt->flags & OPT_HIDE) {
			p = "??????";
		} else {
			p = (char *) opt->addr;
			if ((opt->flags & OPT_STATIC) == 0)
				p = *ALIGNED_CAST(char **)p;
		}
		printer(arg, "%s %q", opt->name, p);
		break;

	case o_special:
	case o_special_noarg:
	case o_special_cfarg:
	case o_wild:
		if (opt->type != o_wild) {
			printer(arg, "%s", opt->name);
			if (n_arguments(opt) == 0)
				break;
			printer(arg, " ");
		}
		if (opt->flags & OPT_A2PRINTER) {
			void (*oprt) __P((option_t *,
					  void ((*)__P((void *, char *, ...))),
					  void *));
			oprt = (void (*) __P((option_t *,
					 void ((*)__P((void *, char *, ...))),
					 void *)))opt->addr2;
			(*oprt)(opt, printer, arg);
		} else if (opt->flags & OPT_A2STRVAL) {
			p = (char *) opt->addr2;
			if ((opt->flags & OPT_STATIC) == 0)
				p = *ALIGNED_CAST(char **)p;
			printer("%q", p);
		} else if (opt->flags & OPT_A2LIST) {
			struct option_value *ovp;

			ovp = (struct option_value *) opt->addr2;
			for (;;) {
				printer(arg, "%q", ovp->value);
				if ((ovp = ovp->next) == NULL)
					break;
				printer(arg, "\t\t# (from %s)\n%s ",
					ovp->source, opt->name);
			}
		} else {
			printer(arg, "xxx # [don't know how to print value]");
		}
		break;

	default:
		printer(arg, "# %s value (type %d\?\?)", opt->name, opt->type);
		break;
	}

#ifdef __APPLE__
	printer(arg, "\t\t# (from %s", mainopt->source);
	for (i = 0; i < mainopt->nb_other_source; i++)
		printer(arg, ", %s", mainopt->other_source[i]);
	printer(arg, ")\n", mainopt->source);
#else
	printer(arg, "\t\t# (from %s)\n", mainopt->source);
#endif
}

/*
 * print_option_list - print out options in effect from an
 * array of options.
 */
static void
print_option_list(opt, printer, arg)
    option_t *opt;
    void (*printer) __P((void *, char *, ...));
    void *arg;
{
	while (opt->name != NULL) {
		if (opt->priority != OPRIO_DEFAULT
		    && opt->winner != (short int) -1)
			print_option(opt + opt->winner, opt, printer, arg);
		do {
			++opt;
		} while (opt->flags & OPT_PRIOSUB);
	}
}

/*
 * print_options - print out what options are in effect.
 */
void
print_options(printer, arg)
    void (*printer) __P((void *, char *, ...));
    void *arg;
{
	struct option_list *list;
	int i;

	printer(arg, "pppd options in effect:\n");
	print_option_list(general_options, printer, arg);
	print_option_list(auth_options, printer, arg);
	for (list = extra_options; list != NULL; list = list->next)
		print_option_list(list->options, printer, arg);
	print_option_list(the_channel->options, printer, arg);
	for (i = 0; protocols[i] != NULL; ++i)
		print_option_list(protocols[i]->options, printer, arg);
}

/*
 * usage - print out a message telling how to use the program.
 */
static void
usage()
{
    if (phase == PHASE_INITIALIZE)
	fprintf(stderr, usage_string, VERSION, progname);
}

/*
 * showhelp - print out usage message and exit.
 */
static int
showhelp(argv)
    char **argv;
{
    if (phase == PHASE_INITIALIZE) {
	usage();
	exit(0);
    }
    return 0;
}

/*
 * showversion - print out the version number and exit.
 */
static int
showversion(argv)
    char **argv;
{
    if (phase == PHASE_INITIALIZE) {
#ifdef __APPLE__
	fprintf(stderr, "pppd version %s (Apple version %s)\n", VERSION, PPP_VERSION);
#else
	fprintf(stderr, "pppd version %s\n", VERSION);
#endif
	exit(0);
    }
    return 0;
}

/*
 * option_error - print a message about an error in an option.
 * The message is logged, and also sent to
 * stderr if phase == PHASE_INITIALIZE.
 */
void
option_error __V((char *fmt, ...))
{
    va_list args;
    char buf[1024];

#if defined(__STDC__)
    va_start(args, fmt);
#else
    char *fmt;
    va_start(args);
    fmt = va_arg(args, char *);
#endif
    vslprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (phase == PHASE_INITIALIZE)
	fprintf(stderr, "%s: %s\n", progname, buf);
    sys_log(LOG_ERR, "%s", buf);
}

#if 0
/*
 * readable - check if a file is readable by the real user.
 */
int
readable(fd)
    int fd;
{
    uid_t uid;
    int i;
    struct stat sbuf;

    uid = getuid();
    if (uid == 0)
	return 1;
    if (fstat(fd, &sbuf) != 0)
	return 0;
    if (sbuf.st_uid == uid)
	return sbuf.st_mode & S_IRUSR;
    if (sbuf.st_gid == getgid())
	return sbuf.st_mode & S_IRGRP;
    for (i = 0; i < ngroups; ++i)
	if (sbuf.st_gid == groups[i])
	    return sbuf.st_mode & S_IRGRP;
    return sbuf.st_mode & S_IROTH;
}
#endif

/*
 * Read a word from a file.
 * Words are delimited by white-space or by quotes (" or ').
 * Quotes, white-space and \ may be escaped with \.
 * \<newline> is ignored.
 */
int
getword(f, word, newlinep, filename)
    FILE *f;
    char *word;
    int *newlinep;
    char *filename;
{
    int c, len, escape;
    int quoted, comment;
    int value, digit, got, n;

#define isoctal(c) ((c) >= '0' && (c) < '8')

    *newlinep = 0;
    len = 0;
    escape = 0;
    comment = 0;

    /*
     * First skip white-space and comments.
     */
    for (;;) {
	c = getc(f);
	if (c == EOF)
	    break;

	/*
	 * A newline means the end of a comment; backslash-newline
	 * is ignored.  Note that we cannot have escape && comment.
	 */
	if (c == '\n') {
	    if (!escape) {
		*newlinep = 1;
		comment = 0;
	    } else
		escape = 0;
	    continue;
	}

	/*
	 * Ignore characters other than newline in a comment.
	 */
	if (comment)
	    continue;

	/*
	 * If this character is escaped, we have a word start.
	 */
	if (escape)
	    break;

	/*
	 * If this is the escape character, look at the next character.
	 */
	if (c == '\\') {
	    escape = 1;
	    continue;
	}

	/*
	 * If this is the start of a comment, ignore the rest of the line.
	 */
	if (c == '#') {
	    comment = 1;
	    continue;
	}

	/*
	 * A non-whitespace character is the start of a word.
	 */
	if (!isspace(c))
	    break;
    }

    /*
     * Save the delimiter for quoted strings.
     */
    if (!escape && (c == '"' || c == '\'')) {
        quoted = c;
	c = getc(f);
    } else
        quoted = 0;

    /*
     * Process characters until the end of the word.
     */
    while (c != EOF) {
	if (escape) {
	    /*
	     * This character is escaped: backslash-newline is ignored,
	     * various other characters indicate particular values
	     * as for C backslash-escapes.
	     */
	    escape = 0;
	    if (c == '\n') {
	        c = getc(f);
		continue;
	    }

	    got = 0;
	    switch (c) {
	    case 'a':
		value = '\a';
		break;
	    case 'b':
		value = '\b';
		break;
	    case 'f':
		value = '\f';
		break;
	    case 'n':
		value = '\n';
		break;
	    case 'r':
		value = '\r';
		break;
	    case 's':
		value = ' ';
		break;
	    case 't':
		value = '\t';
		break;

	    default:
		if (isoctal(c)) {
		    /*
		     * \ddd octal sequence
		     */
		    value = 0;
		    for (n = 0; n < 3 && isoctal(c); ++n) {
			value = (value << 3) + (c & 07);
			c = getc(f);
		    }
		    got = 1;
		    break;
		}

		if (c == 'x') {
		    /*
		     * \x<hex_string> sequence
		     */
		    value = 0;
		    c = getc(f);
		    for (n = 0; n < 2 && isxdigit(c); ++n) {
			digit = toupper(c) - '0';
			if (digit > 10)
			    digit += '0' + 10 - 'A';
			value = (value << 4) + digit;
			c = getc (f);
		    }
		    got = 1;
		    break;
		}

		/*
		 * Otherwise the character stands for itself.
		 */
		value = c;
		break;
	    }

	    /*
	     * Store the resulting character for the escape sequence.
	     */
	    if (len < MAXWORDLEN-1)
		word[len] = value;
	    ++len;

	    if (!got)
		c = getc(f);
	    continue;

	}

	/*
	 * Not escaped: see if we've reached the end of the word.
	 */
	if (quoted) {
	    if (c == quoted)
		break;
	} else {
	    if (isspace(c) || c == '#') {
		ungetc (c, f);
		break;
	    }
	}

	/*
	 * Backslash starts an escape sequence.
	 */
	if (c == '\\') {
	    escape = 1;
	    c = getc(f);
	    continue;
	}

	/*
	 * An ordinary character: store it in the word and get another.
	 */
	if (len < MAXWORDLEN-1)
	    word[len] = c;
	++len;

	c = getc(f);
    }

    /*
     * End of the word: check for errors.
     */
    if (c == EOF) {
	if (ferror(f)) {
	    if (errno == 0)
		errno = EIO;
	    option_error("Error reading %s: %m", filename);
	    die(1);
	}
	/*
	 * If len is zero, then we didn't find a word before the
	 * end of the file.
	 */
	if (len == 0)
	    return 0;
    }

    /*
     * Warn if the word was too long, and append a terminating null.
     */
    if (len >= MAXWORDLEN) {
	option_error("warning: word in file %s too long (%.20s...)",
		     filename, word);
	len = MAXWORDLEN - 1;
    }
    word[len] = 0;

    return 1;

#undef isoctal

}

/*
 * number_option - parse an unsigned numeric parameter for an option.
 */
static int
number_option(str, valp, base)
    char *str;
    u_int32_t *valp;
    int base;
{
    char *ptr;

    *valp = (u_int32_t)strtoul(str, &ptr, base);
    if (ptr == str) {
	option_error("invalid numeric parameter '%s' for %s option",
		     str, current_option);
	return 0;
    }
    return 1;
}


/*
 * int_option - like number_option, but valp is int *,
 * the base is assumed to be 0, and *valp is not changed
 * if there is an error.
 */
int
int_option(str, valp)
    char *str;
    int *valp;
{
    u_int32_t v;

    if (!number_option(str, &v, 0))
	return 0;
    *valp = (int) v;
    return 1;
}


/*
 * The following procedures parse options.
 */

/*
 * readfile - take commands from a file.
 */
static int
readfile(argv)
    char **argv;
{
    return options_from_file(*argv, 1, 1, privileged_option);
}

/*
 * callfile - take commands from /etc/ppp/peers/<name>.
 * Name may not contain /../, start with / or ../, or end in /..
 */
static int
callfile(argv)
    char **argv;
{
    char *fname, *arg, *p;
    int l, ok;

    arg = *argv;
    ok = 1;
    if (arg[0] == '/' || arg[0] == 0)
	ok = 0;
    else {
	for (p = arg; *p != 0; ) {
	    if (p[0] == '.' && p[1] == '.' && (p[2] == '/' || p[2] == 0)) {
		ok = 0;
		break;
	    }
	    while (*p != '/' && *p != 0)
		++p;
	    if (*p == '/')
		++p;
	}
    }
    if (!ok) {
	option_error("call option value may not contain .. or start with /");
	return 0;
    }

    l = (int)(strlen(arg) + strlen(_PATH_PEERFILES) + 1);
    if ((fname = (char *) malloc(l)) == NULL)
	novm("call file name");
    slprintf(fname, l, "%s%s", _PATH_PEERFILES, arg);

#ifdef __APPLE__
    ok = options_from_file(fname, 0, 1, 1);
#else
    ok = options_from_file(fname, 1, 1, 1);
#endif

    free(fname);
    return ok;
}

#ifdef PPP_FILTER
/*
 * setpassfilter - Set the pass filter for packets
 */
static int
setpassfilter(argv)
    char **argv;
{
    pc.linktype = DLT_PPP;
    pc.snapshot = PPP_HDRLEN;
 
    if (pcap_compile(&pc, &pass_filter, *argv, 1, netmask) == 0)
	return 1;
    option_error("error in pass-filter expression: %s\n", pcap_geterr(&pc));
    return 0;
}

/*
 * setactivefilter - Set the active filter for packets
 */
static int
setactivefilter(argv)
    char **argv;
{
    pc.linktype = DLT_PPP;
    pc.snapshot = PPP_HDRLEN;
 
    if (pcap_compile(&pc, &active_filter, *argv, 1, netmask) == 0)
	return 1;
    option_error("error in active-filter expression: %s\n", pcap_geterr(&pc));
    return 0;
}
#endif

/*
 * setdomain - Set domain name to append to hostname 
 */
static int
setdomain(argv)
    char **argv;
{
    gethostname(hostname, MAXNAMELEN);
    if (**argv != 0) {
	if (**argv != '.')
	    strncat(hostname, ".", MAXNAMELEN - strlen(hostname) - 1);
	domain = hostname + strlen(hostname);
	strncat(hostname, *argv, MAXNAMELEN - strlen(hostname) - 1);
    }
    hostname[MAXNAMELEN-1] = 0;
    return (1);
}

/* -----------------------------------------------------------------------------
 Create directories and intermediate directories as required.
 ----------------------------------------------------------------------------- */
#ifdef __APPLE__
static int makepath( char *path)
{
	char	*c;
	char	*thepath;
	int		slen=0;
	int		done = 0;
	mode_t	oldmask, newmask;
	struct stat sb;
	int		error=0;
	
	oldmask = umask(0);
	newmask = S_IRWXU | S_IRGRP | S_IROTH | S_IXGRP | S_IXOTH; 
	
	slen = (int)strlen(path);
	if  ( !(thepath =  malloc( slen+1) ))
		return -1;
	strlcpy( thepath, path, slen+1);
	c = thepath;
	if ( *c == '/' )
		c++;		
	for(  ; !done; c++){
		if ( (*c == '/') || ( *c == '\0' )){
			if ( *c == '\0' )
				done = 1;
			else 
				*c = '\0';
			if ( mkdir( thepath, newmask) ){
				if ( errno == EEXIST || errno == EISDIR){
					if ( stat(thepath, &sb) < 0){
						error = -1;
						break;
					}
				} else {
					error = -1;
					break;
				}
			}
			*c = '/';
		}
	}
	free(thepath);
	umask(oldmask);
	return error;
}
#endif

static int
setlogfile(argv)
    char **argv;
{
    int fd, err;

    if (!privileged_option)
	seteuid(getuid());
    fd = open(*argv, O_WRONLY | O_APPEND | O_CREAT | O_EXCL, 0644);
	
    if (fd < 0) 
	{
		if(errno == EEXIST)
			fd = open(*argv, O_WRONLY | O_APPEND);
#ifdef __APPLE__
		else {
			if(privileged_option) {
				char *dirPath = dirname(*argv);
				sys_log(LOG_WARNING, "Warning: Creating directory for log file = %s\n", *argv);
				makepath(dirPath);
				fd = open(*argv, O_WRONLY | O_APPEND | O_CREAT | O_EXCL, 0644);	
			}
		}
#endif
	}
	
	err = errno;
    
	if (!privileged_option)
	seteuid(0);
		
	if(fd < 0)
	{
		errno = err;
		option_error("Can't open log file %s: %m", *argv);
#ifdef __APPLE__
	// ignore the error
	return 1;
#else
        return 0;
#endif
    }
    strlcpy(logfile_name, *argv, sizeof(logfile_name));
    if (logfile_fd >= 0)
	close(logfile_fd);
    logfile_fd = fd;
    log_to_fd = fd;
    log_default = 0;
    return 1;
}

#ifdef MAXOCTETS
static int
setmodir(argv)
    char **argv;
{
    if(*argv == NULL)
	return 0;
    if(!strcmp(*argv,"in")) {
        maxoctets_dir = PPP_OCTETS_DIRECTION_IN;
    } else if (!strcmp(*argv,"out")) {
        maxoctets_dir = PPP_OCTETS_DIRECTION_OUT;
    } else if (!strcmp(*argv,"max")) {
        maxoctets_dir = PPP_OCTETS_DIRECTION_MAXOVERAL;
    } else {
        maxoctets_dir = PPP_OCTETS_DIRECTION_SUM;
    }
    return 1;
}
#endif

#ifdef PLUGIN

#ifdef __APPLE__
static int
loadplugin(argv)
    char **argv;
{
    char *arg = *argv;
    int err;
    
    err = sys_loadplugin(*argv);
    if (err) {
	option_error("Couldn't load plugin %s", arg);
		// continue without loading plugin
        return 1;
    }

    //info("Plugin %s loaded.", arg);

    return 1;
}

static int
loadplugin2(argv)
	char **argv;
{
    char *arg = *argv;
    int err;
    
    err = sys_loadplugin(*argv);
    if (err) {
		option_error("Couldn't load plugin %s", arg);
		// fatal error
        return 0;
    }
	
    //info("Plugin %s loaded.", arg);
	
    return 1;
}	

/*
 * options_from_file - Read a string of options from controller file descriptor,
 * and interpret them.
 */
int
options_from_controller()
{
    int i, newline, ret;
    option_t *opt;
    int n, oldpriv;
    char *argv[MAXARGS+1]; // +1 because of cfarg
    char args[MAXARGS][MAXWORDLEN];
    char cmd[MAXWORDLEN];
	char *data = NULL;

    oldpriv = privileged_option;
    privileged_option = controlled;
    option_source = "controller";
    option_priority = OPRIO_CMDLINE;
    ret = 0;

    while (getword(controlfile, cmd, &newline, "controller")) {
    
        if (!strcmp(cmd, "[OPTIONS]"))
            continue;
        if (!strcmp(cmd, "[EOP]"))
            break;

	opt = find_option(cmd);
	if (opt == NULL) {
	    option_error("In controller file descriptor: unrecognized option '%s'",
			 cmd);
	    goto err;
	}
	bzero(argv, sizeof(argv));
	bzero(args, sizeof(args));
	n = n_arguments(opt);
	for (i = 0; i < n; ++i) {
	    if (!getword(controlfile, args[i], &newline, "controller")) {
		option_error(
			"In controller file descriptor: too few parameters for option '%s'",
			cmd);
		goto err;
	    }
	    argv[i] = args[i];
	}

	if (opt->type == o_special_cfarg) {

		int iv;
		if (!int_option(*argv, &iv))
			goto err;

		data = malloc(iv);
		fread(data, iv, 1, controlfile);
		argv[1] = data;
	}
	if (!process_option(opt, cmd, argv))
	    goto err;
    }
    ret = 1;

err:
	if (data) free(data);
	privileged_option = oldpriv;
    return ret;
}

/*
 * controlled_connection - Prepare control and status file descriptors
 */
static int
controlled_connection(argv)
    char **argv;
{
    
	if (!sys_check_controller()) {
		option_error("Can't verify the controller started the connection");
		goto err;
	}
	
	/* first pipe STDIN */
    controlfd = dup(STDIN_FILENO);
    if (controlfd == -1) {
	option_error("Can't duplicate control file descripor: %m");
	goto err;
    }
        
    controlfile = fdopen(controlfd, "r");
    if (controlfile == NULL) {
        close(controlfd);
	option_error("Can't open control file descripor: %m");
	goto err;
    }
    
    /* then pipe STDOUT */

    statusfd = dup(STDOUT_FILENO);
    if (statusfd == -1) {
	option_error("Can't duplicate status file descripor: %m");
	goto err;
    }

	/* can'	t send logs to stdout when controlled */
	if (log_default) {
		log_to_fd = -1;
		log_default = 0;
	}

    controlled = 1;
    return 1;
    
err:
    if (controlfile) {
        fclose(controlfile); // also closes controlfd
        controlfile = 0;
        controlfd = -1;
    }
    
    if (statusfd != -1) {
        close(statusfd); 
        statusfd = -1;
    }
    return 0;
}

/* ----------------------------------------------------------------------------- 
----------------------------------------------------------------------------- */
void options_close()
{
    if (controlfile) {
        fclose(controlfile); // also closes controlfd
        controlfile = 0;
        controlfd = -1;
    }
    
    if (statusfd != -1) {
        close(statusfd); 
        statusfd = -1;
    }
	controlled = 0;
}

#else
static int
loadplugin(argv)
    char **argv;
{
    char *arg = *argv;
    void *handle;
    const char *err;
    void (*init) __P((void));
    char *path = arg;
    const char *vers;

    if (strchr(arg, '/') == 0) {
	const char *base = _PATH_PLUGIN;
	int l = strlen(base) + strlen(arg) + 2;
	path = malloc(l);
	if (path == 0)
	    novm("plugin file path");
	strlcpy(path, base, l);
	strlcat(path, "/", l);
	strlcat(path, arg, l);
    }
    handle = dlopen(path, RTLD_GLOBAL | RTLD_NOW);
    if (handle == 0) {
	err = dlerror();
	if (err != 0)
	    option_error("%s", err);
	option_error("Couldn't load plugin %s", arg);
	goto err;
    }
    init = (void (*)(void))dlsym(handle, "plugin_init");
    if (init == 0) {
	option_error("%s has no initialization entry point", arg);
	goto errclose;
    }
    vers = (const char *) dlsym(handle, "pppd_version");
    if (vers == 0) {
	warn("Warning: plugin %s has no version information", arg);
    } else if (strcmp(vers, VERSION) != 0) {
	option_error("Plugin %s is for pppd version %s, this is %s",
		     arg, vers, VERSION);
	goto errclose;
    }
    info("Plugin %s loaded.", arg);
    (*init)();
    return 1;

 errclose:
    dlclose(handle);
 err:
    if (path != arg)
	free(path);
    return 0;
}
#endif
#endif /* PLUGIN */
