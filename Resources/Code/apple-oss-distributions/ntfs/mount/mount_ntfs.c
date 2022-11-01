/*
 * Copyright (c) 2006-2008 Apple Inc. All rights reserved.
 *
 * This file contains Original Code and/or Modifications of Original Code as
 * defined in and that are subject to the Apple Public Source License Version
 * 2.0 (the 'License'). You may not use this file except in compliance with the
 * License.
 *
 * Please obtain a copy of the License at http://www.opensource.apple.com/apsl/
 * and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR
 * A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the
 * License for the specific language governing rights and limitations under the
 * License.
 */

#include <sys/mount.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <mntopts.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "ntfs.h"
#include "ntfs_types.h"

static struct mntopt mopts[] = {
	MOPT_STDOPTS,
	MOPT_FSTAB_COMPAT,
	MOPT_ASYNC,
	MOPT_SYNC,
	MOPT_FORCE,
	MOPT_UPDATE,
	MOPT_RELOAD,
	{ NULL, 0, 0, 0 }
};

static void usage(const char *progname) __attribute__((noreturn));
static void usage(const char *progname)
{
	errx(EX_USAGE, "usage: %s [-s] [-o options] special-device "
			"filesystem-node\n", progname);
}

/**
 * do_exec - Execute an external command.
 */
static int do_exec(const char *progname, char *const args[])
{
	pid_t pid;
	union wait status;
	int eo;

	pid = fork();
	if (pid == -1) {
		fprintf(stderr, "%s: fork failed: %s\n", progname,
				strerror(errno));
		return -1;
	}
	if (!pid) {
		/* In child process, execute external command. */
		(void)execv(args[0], args);
		/* We only get here if the execv() failed. */
		eo = errno;
		fprintf(stderr, "%s: execv %s failed: %s\n", progname, args[0],
				strerror(eo));
		exit(eo);
	}
	/* In parent process, wait for exernal command to finish. */
	if (wait4(pid, (int*)&status, 0, NULL) != pid) {
		fprintf(stderr, "%s: BUG executing %s command.\n", progname,
				args[0]);
		return -1;
	}
	if (!WIFEXITED(status)) {
		fprintf(stderr, "%s: %s command aborted by signal %d.\n",
				progname, args[0], WTERMSIG(status));
		return -1;
	}
	eo = WEXITSTATUS(status);
	if (eo) {
		fprintf(stderr, "%s: %s command failed: %s\n", progname,
				args[0], strerror(eo));
		return -1;
	}
	return 0;
}

static void rmslashes(char *rrpin, char *rrpout)
{
	char *rrpoutstart;

	*rrpout = *rrpin;
    for (rrpoutstart = rrpout; (*rrpin != '\0') && (rrpout - rrpoutstart < MAXPATHLEN-1); *rrpout++ = *rrpin++) {
		/* Skip all double slashes. */
		while (*rrpin == '/' && *(rrpin + 1) == '/')
			 rrpin++;
	}
	/* Remove trailing slash if necessary. */
	if (rrpout - rrpoutstart > 1 && *(rrpout - 1) == '/')
		*(rrpout - 1) = '\0';
	else
		*rrpout = '\0';
}

static void checkpath(int flags, const char *path, char *resolved)
{
	struct stat sb;

	if (flags & MNT_NOFOLLOW) {
		size_t sc = strlcpy(resolved, path, MAXPATHLEN);
		if (sc >= MAXPATHLEN)
			err(EX_USAGE, "%s", resolved);
	} else if (!realpath(path, resolved))
		err(EX_USAGE, "%s", resolved);
	if (stat(resolved, &sb))
		err(EX_USAGE, "%s", resolved);
	if (!S_ISDIR(sb.st_mode)) 
		errx(EX_USAGE, "%s: not a directory", resolved);
}

int main(int argc, char **argv)
{
	char *progname, *dev;
	ntfs_mount_options_header *opts_hdr;
	ntfs_mount_options_1_0 *opts;
	int ch, dummy, flags = 0;
	char dir[MAXPATHLEN];
	const char ntfs[] = "ntfs";
	char *const kextargs[] = { "/sbin/kextload",
			"/System/Library/Extensions/ntfs.kext", NULL };
	struct vfsconf vfc;
	BOOL case_sensitive;

	/* Default to mounting read-only. */
	flags = MNT_RDONLY;
	
	/* Save & strip off program name. */
	progname = argv[0];
	/* Set up default options. */
	case_sensitive = FALSE;
	/* Parse the options. */
	while ((ch = getopt(argc, argv, "so:h?")) != -1) {
		switch (ch) {
		case 's':
			case_sensitive = TRUE;
			break;
		case 'o': {
			mntoptparse_t tmp;

			tmp = getmntopts(optarg, mopts, &flags, &dummy);
			if (!tmp)
				err(EX_OSERR, "getmntopts() failed");
			freemntopts(tmp);
			break;
		}
		case 'h':
		case '?':
		default:
			usage(progname);
			break;
		}
	}
	argc -= optind;
	argv += optind;
	/* Parse the device to mount and the directory to mount it on. */
	if (argc != 2)
		usage(progname);
	dev = argv[0];
	checkpath(flags, argv[1], dir);
	rmslashes(dev, dev);
	/*
	 * Set up the NTFS mount options structure for the mount(2) call.
	 *
	 * We currently implement version 1.0, which only has the flags option
	 * and the only currently defined flag is NTFS_OPT_CASE_SENSITIVE.
	 */
	opts_hdr = valloc(((sizeof(*opts_hdr) + 7) & ~7) + sizeof(*opts));
	if (!opts_hdr)
		err(EX_OSERR, "valloc() failed");
	*opts_hdr = (ntfs_mount_options_header) {
		.fspec = dev,
		.major_ver = 1,
		.minor_ver = 0,
	};
	opts = (ntfs_mount_options_1_0*)((char*)opts_hdr +
			((sizeof(*opts_hdr) + 7) & ~7));
	*opts = (ntfs_mount_options_1_0) {
		.flags = case_sensitive ? NTFS_MNT_OPT_CASE_SENSITIVE : 0,
	};
	/* If the kext is not loaded, load it now. */
	if (getvfsbyname(ntfs, &vfc)) {
		/*
		 * Ignore errors from the load attempt and instead simply check
		 * that NTFS is now loaded and if not bail out now.
		 */
		(void)do_exec(progname, kextargs);
		if (getvfsbyname(ntfs, &vfc))
			errx(EX_OSERR, "Failed to load NTFS file system kext.");
	}
	if (mount(ntfs, dir, flags, opts_hdr) < 0)
		err(EX_OSERR, "%s on %s", dev, dir);
	free(opts_hdr);
	return 0;
}
