/* -*- c-file-style: "java"; indent-tabs-mode: nil; tab-width: 4; fill-column: 78 -*-
 *
 * distcc -- A simple distributed compiler system
 *
 * Copyright (C) 2002, 2003, 2004 by Martin Pool <mbp@samba.org>
 * Copyright 2007 Google Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */


/* dopt.c -- Parse and apply server options. */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <popt.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "types.h"
#include "distcc.h"
#include "trace.h"
#include "dopt.h"
#include "exitcode.h"
#include "daemon.h"
#include "access.h"
#include "exec.h"

#ifdef XCODE_INTEGRATION
  #include "xci.h"
#endif

int opt_niceness = 5;           /* default */

/**
 * Number of children running jobs on this machine.  If zero (recommended),
 * then dynamically set from the number of CPUs.
 **/
int arg_max_jobs = 0;

int arg_port = DISTCC_DEFAULT_PORT;
int arg_stats = DISTCC_DEFAULT_STATS_ENABLED;
int arg_stats_port = DISTCC_DEFAULT_STATS_PORT;

/** If true, serve all requests directly from listening process
    without forking.  Better for debugging. **/
int opt_no_fork = 0;

int opt_daemon_mode = 0;
int opt_inetd_mode = 0;
int opt_no_fifo = 0;

/** If non-NULL, listen on only this address. **/
char *opt_listen_addr = NULL;

struct dcc_allow_list *opt_allowed = NULL;

/**
 * If true, don't detach from the parent.  This is probably necessary
 * for use with daemontools or other monitoring programs, and is also
 * used by the test suite.
 **/
int opt_no_detach = 0;

int opt_log_stderr = 0;

int opt_log_level_num = RS_LOG_NOTICE;

/**
 * Daemon exits after this many seconds.  Intended mainly for testing, to make
 * sure daemons don't persist for too long.
 */
int opt_lifetime = 0;

const char *arg_pid_file = NULL;
const char *arg_log_file = NULL;

int opt_job_lifetime = 0;

/* Enumeration values for options that don't have single-letter name.  These
 * must be numerically above all the ascii letters. */
enum {
    opt_log_to_file = 300,
    opt_log_level
};

#if defined(HAVE_AVAHI) || (defined(XCODE_INTEGRATION) && defined(HAVE_DNSSD))
/* Flag for enabling/disabling Zeroconf */
int opt_zeroconf = 0;
#endif

#ifdef XCODE_INTEGRATION
/* The priority of this distccd server relative to others.  This is set by
 * the --priority argument.  It is not used by distcc, but it is used by
 * Xcode when sorting the hosts in DISTCC_HOSTS.  The Xcode UI maps
 * "low" to 20, "medium" to 10, and "high" to 0 when configuring launchd to
 * start distccd. */
int arg_priority = 10;

/* System version string override.  If unset, the default will be used.
 * This is handled in xci_versinfo.c. */
char *arg_system_version = NULL;
#endif

const struct poptOption options[] = {
    { "allow", 'a',      POPT_ARG_STRING, 0, 'a', 0, 0 },
    { "jobs", 'j',       POPT_ARG_INT, &arg_max_jobs, 'j', 0, 0 },
    { "daemon", 0,       POPT_ARG_NONE, &opt_daemon_mode, 0, 0, 0 },
    { "help", 0,         POPT_ARG_NONE, 0, '?', 0, 0 },
    { "inetd", 0,        POPT_ARG_NONE, &opt_inetd_mode, 0, 0, 0 },
    { "lifetime", 0,     POPT_ARG_INT, &opt_lifetime, 0, 0, 0 },
    { "listen", 0,       POPT_ARG_STRING, &opt_listen_addr, 0, 0, 0 },
    { "log-file", 0,     POPT_ARG_STRING, &arg_log_file, 0, 0, 0 },
    { "log-level", 0,    POPT_ARG_STRING, 0, opt_log_level, 0, 0 },
    { "log-stderr", 0,   POPT_ARG_NONE, &opt_log_stderr, 0, 0, 0 },
    { "job-lifetime", 0, POPT_ARG_INT, &opt_job_lifetime, 'l', 0, 0 },
    { "nice", 'N',       POPT_ARG_INT,  &opt_niceness,  0, 0, 0 },
    { "no-detach", 0,    POPT_ARG_NONE, &opt_no_detach, 0, 0, 0 },
    { "no-fifo", 0,      POPT_ARG_NONE, &opt_no_fifo, 0, 0, 0 },
    { "no-fork", 0,      POPT_ARG_NONE, &opt_no_fork, 0, 0, 0 },
    { "pid-file", 'P',   POPT_ARG_STRING, &arg_pid_file, 0, 0, 0 },
    { "port", 'p',       POPT_ARG_INT, &arg_port, 0, 0, 0 },
    { "user", 0,         POPT_ARG_STRING, &opt_user, 'u', 0, 0 },
    { "verbose", 0,      POPT_ARG_NONE, 0, 'v', 0, 0 },
    { "version", 0,      POPT_ARG_NONE, 0, 'V', 0, 0 },
    { "wizard", 'W',     POPT_ARG_NONE, 0, 'W', 0, 0 },
    { "stats", 0,        POPT_ARG_NONE, &arg_stats, 0, 0, 0 },
    { "stats-port", 0,   POPT_ARG_INT, &arg_stats_port, 0, 0, 0 },
#if defined(HAVE_AVAHI) || (defined(XCODE_INTEGRATION) && defined(HAVE_DNSSD))
    { "zeroconf", 0,     POPT_ARG_NONE, &opt_zeroconf, 0, 0, 0 },
#endif
#ifdef XCODE_INTEGRATION
    { "host-info", 'I',  POPT_ARG_NONE, 0, 'I', 0, 0 },
    { "priority", 0,     POPT_ARG_INT, &arg_priority, 0, 0, 0 },
    { "system-version", 0, POPT_ARG_STRING, &arg_system_version, 0, 0, 0},
    { "xcode-dir", 0,    POPT_ARG_STRING, &arg_xcode_dir, 0, 0, 0},
#endif
    { 0, 0, 0, 0, 0, 0, 0 }
};


static void distccd_show_usage(void)
{
    dcc_show_version("distccd");
    printf (
"Usage:\n"
"   distccd [OPTIONS]\n"
"\n"
"Options:\n"
"    --help                     explain usage and exit\n"
"    --version                  show version and exit\n"
"    -P, --pid-file FILE        save daemon process id to file\n"
"    -N, --nice LEVEL           lower priority, 20=most nice\n"
"    --user USER                if run by root, change to this persona\n"
"    --jobs, -j LIMIT           maximum tasks at any time\n"
"    --job-lifetime SECONDS     maximum lifetime of a compile request\n"
#ifdef XCODE_INTEGRATION
"    --host-info                display system/compiler information and exit\n"
"    --system-version           override system version reported to Xcode\n"
"    --xcode-dir                override Xcode Developer directory\n"
#endif
"  Networking:\n"
"    -p, --port PORT            TCP port to listen on\n"
"    --listen ADDRESS           IP address to listen on\n"
"    -a, --allow IP[/BITS]      client address access control\n"
"    --stats                    enable statistics reporting via HTTP server\n"
"    --stats-port PORT          TCP port to listen on for statistics requests\n"
#if defined(HAVE_AVAHI) || (defined(XCODE_INTEGRATION) && defined(HAVE_DNSSD))
"    --zeroconf                 register via mDNS/DNS-SD\n"
#endif
#ifdef XCODE_INTEGRATION
"    --priority                 Xcode selection priority (lower preferred)\n"
#endif
"  Debug and trace:\n"
"    --log-level=LEVEL          set detail level for log file\n"
"      levels: critical, error, warning, notice, info, debug\n"
"    --verbose                  set log level to \"debug\"\n"
"    --no-detach                don't detach from parent (for daemontools, etc)\n"
"    --log-file=FILE            send messages here instead of syslog\n"
"    --log-stderr               send messages to stderr\n"
"    --wizard                   for running under gdb\n"
"  Mode of operation:\n"
"    --inetd                    serve client connected to stdin\n"
"    --daemon                   bind and listen on socket\n"
"\n"
"distccd runs either from inetd or as a standalone daemon to compile\n"
"files submitted by the distcc client.\n"
"\n"
"distccd should only run on trusted networks.\n"
);
}


int distccd_parse_options(int argc, const char **argv)
{
    poptContext po;
    int po_err, exitcode;
#ifdef XCODE_INTEGRATION
    const char *host_info;
#endif

    po = poptGetContext("distccd", argc, argv, options, 0);

    while ((po_err = poptGetNextOpt(po)) != -1) {
        switch (po_err) {
        case '?':
            distccd_show_usage();
            exitcode = 0;
            goto out_exit;

        case 'a': {
            /* TODO: Allow this to be a hostname, which is resolved to an address. */
            /* TODO: Split this into a small function. */
            struct dcc_allow_list *new;
            new = malloc(sizeof *new);
            if (!new) {
                rs_log_crit("malloc failed");
                exitcode = EXIT_OUT_OF_MEMORY;
                goto out_exit;
            }
            new->next = opt_allowed;
            opt_allowed = new;
            if ((exitcode = dcc_parse_mask(poptGetOptArg(po), &new->addr, &new->mask)))
                goto out_exit;
        }
            break;

        case 'j':
            if (arg_max_jobs < 1 || arg_max_jobs > 200) {
                rs_log_error("--jobs argument must be between 1 and 200");
                exitcode = EXIT_BAD_ARGUMENTS;
                goto out_exit;
            }
            break;

        case 'l':
            if (opt_job_lifetime < 0) {
                opt_job_lifetime = 0;
            }
            dcc_job_lifetime = opt_job_lifetime;
            break;

        case 'u':
            if (getuid() != 0 && geteuid() != 0) {
                rs_log_warning("--user is ignored when distccd is not run by root");
                /* continue */
            }
            break;

        case 'V':
            dcc_show_version("distccd");
            exitcode = EXIT_SUCCESS;
            goto out_exit;

        case opt_log_level:
            {
                int level;
                const char *level_name;

                level_name = poptGetOptArg(po);
                level = rs_loglevel_from_name(level_name);
                if (level == -1) {
                    rs_log_warning("invalid --log-level argument \"%s\"",
                                   level_name);
                } else {
                    rs_trace_set_level(level);
                    opt_log_level_num = level;
                }
            }
            break;

        case 'v':
            rs_trace_set_level(RS_LOG_DEBUG);
            opt_log_level_num = RS_LOG_DEBUG;
            break;

        case 'W':
            /* catchall for running under gdb */
            opt_log_stderr = 1;
            opt_daemon_mode = 1;
            opt_no_detach = 1;
            opt_no_fork = 1;
            opt_no_fifo = 1;
            rs_trace_set_level(RS_LOG_DEBUG);
            opt_log_level_num = RS_LOG_DEBUG;
            break;

#ifdef XCODE_INTEGRATION
        case 'I':
            /* For --host-info.  Having this as a distccd option seems kind
             * of weird, but Xcode runs "distccd --host-info localhost" to
             * determine information about the running system,
             * locally-installed compilers, and distcc version.  (The
             * localhost" argument is ignored.) */

            if ((host_info = dcc_xci_host_info_string())) {
                printf("%s", host_info);
                exitcode = 0;
            } else {
                exitcode = EXIT_DISTCC_FAILED;
            }
            goto out_exit;
#endif /* XCODE_INTEGRATION */

        default:                /* bad? */
            rs_log(RS_LOG_NONAME|RS_LOG_ERR|RS_LOG_NO_PID, "%s: %s",
                   poptBadOption(po, POPT_BADOPTION_NOALIAS),
                   poptStrerror(po_err));
            exitcode = EXIT_BAD_ARGUMENTS;
            goto out_exit;
        }
    }

    poptFreeContext(po);
    return 0;

    out_exit:
    poptFreeContext(po);
    exit(exitcode);
}
