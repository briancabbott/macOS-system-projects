/* Copyright 1988,1990,1993,1994 by Paul Vixie
 * All rights reserved
 *
 * Distribute freely, except: don't remove my name from the source or
 * documentation (don't take credit for my work), mark your changes (don't
 * get me blamed for your possible bugs), don't alter or remove this
 * notice.  May be sold if buildable source is provided to buyer.  No
 * warrantee of any kind, express or implied, is included with this
 * software; use at your own risk, responsibility for damages (if any) to
 * anyone resulting from the use of this software rests entirely with the
 * user.
 *
 * Send bug reports, bug fixes, enhancements, requests, flames, etc., and
 * I'll try to keep a version up to date.  I can be reached as follows:
 * Paul Vixie          <paul@vix.com>          uunet!decwrl!vixie!paul
 */

#if !defined(lint) && !defined(LINT)
static const char rcsid[] =
  "$FreeBSD: src/usr.sbin/cron/cron/do_command.c,v 1.26 2006/06/11 21:13:49 maxim Exp $";
#endif


#include "cron.h"
#include <sys/signal.h>
#if defined(sequent)
# include <sys/universe.h>
#endif
#if defined(SYSLOG)
# include <syslog.h>
#endif
#if defined(LOGIN_CAP)
# include <login_cap.h>
#endif

#ifdef __APPLE__
#include <stdlib.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/IOReturn.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach_init.h>        /* for bootstrap_port */
#include <vproc.h>
#include <vproc_priv.h>
#include <bootstrap_priv.h>
#endif /* __APPLE__ */

static void		child_process __P((entry *, user *)),
			do_univ __P((user *));

#ifdef __APPLE__
extern vproc_err_t _vproc_post_fork_ping(void);
#endif

void
do_command(e, u)
	entry	*e;
	user	*u;
{
#ifdef __APPLE__
	CFArrayRef cfarray;
	static mach_port_t master = 0;
	static io_connect_t pmcon = 0;

	Debug(DPROC, ("[%d] do_command(%s, (%s,%s,%s))\n",
		getpid(), e->cmd, u->name, e->uname, e->gname))

	if( e->flags & NOT_BATTERY ) {
		if( master == 0 ) {
			IOMasterPort(bootstrap_port, &master);
			pmcon = IOPMFindPowerManagement(master);
		}

		if( IOPMCopyBatteryInfo(master, &cfarray) == kIOReturnSuccess) {
			CFDictionaryRef dict;
			CFNumberRef cfnum;
			int flags;
	
			dict = CFArrayGetValueAtIndex(cfarray, 0);
			cfnum = CFDictionaryGetValue(dict, CFSTR(kIOBatteryFlagsKey));
			CFNumberGetValue(cfnum, kCFNumberLongType, &flags);
	
			if( !(flags & kIOBatteryChargerConnect) ) {
				return;
			} 
		}
	}
#else
	Debug(DPROC, ("[%d] do_command(%s, (%s,%d,%d))\n",
		getpid(), e->cmd, u->name, e->uid, e->gid))
#endif

	/* fork to become asynchronous -- parent process is done immediately,
	 * and continues to run the normal cron code, which means return to
	 * tick().  the child and grandchild don't leave this function, alive.
	 *
	 * vfork() is unsuitable, since we have much to do, and the parent
	 * needs to be able to run off and fork other processes.
	 */
	switch (fork()) {
	case -1:
		log_it("CRON",getpid(),"error","can't fork");
		break;
	case 0:
		/* child process */
		pidfile_close(pfh);
		child_process(e, u);
		Debug(DPROC, ("[%d] child process done, exiting\n", getpid()))
		_exit(OK_EXIT);
		break;
	default:
		/* parent process */
		break;
	}
	Debug(DPROC, ("[%d] main process returning to work\n", getpid()))
}


static void
child_process(e, u)
	entry	*e;
	user	*u;
{
	int		stdin_pipe[2], stdout_pipe[2];
	register char	*input_data;
	char		*usernm, *mailto;
#ifdef __APPLE__
	uid_t		uid = -1;
	gid_t		gid = -1;
	struct passwd	*pwd;
#endif
	int		children = 0;
# if defined(LOGIN_CAP)
	struct passwd	*pwd;
	login_cap_t *lc;
# endif

	Debug(DPROC, ("[%d] child_process('%s')\n", getpid(), e->cmd))

	/* mark ourselves as different to PS command watchers by upshifting
	 * our program name.  This has no effect on some kernels.
	 */
#ifdef __APPLE__
	setprogname("running job");
#else
	setproctitle("running job");
#endif

	/* discover some useful and important environment settings
	 */
#ifdef __APPLE__
	usernm = e->uname;
#else
	usernm = env_get("LOGNAME", e->envp);
#endif
	mailto = env_get("MAILTO", e->envp);

#ifdef USE_SIGCHLD
	/* our parent is watching for our death by catching SIGCHLD.  we
	 * do not care to watch for our children's deaths this way -- we
	 * use wait() explictly.  so we have to disable the signal (which
	 * was inherited from the parent).
	 */
	(void) signal(SIGCHLD, SIG_DFL);
#else
	/* on system-V systems, we are ignoring SIGCLD.  we have to stop
	 * ignoring it now or the wait() in cron_pclose() won't work.
	 * because of this, we have to wait() for our children here, as well.
	 */
	(void) signal(SIGCLD, SIG_DFL);
#endif /*BSD*/

	/* create some pipes to talk to our future child
	 */
	pipe(stdin_pipe);	/* child's stdin */
	pipe(stdout_pipe);	/* child's stdout */

	/* since we are a forked process, we can diddle the command string
	 * we were passed -- nobody else is going to use it again, right?
	 *
	 * if a % is present in the command, previous characters are the
	 * command, and subsequent characters are the additional input to
	 * the command.  Subsequent %'s will be transformed into newlines,
	 * but that happens later.
	 *
	 * If there are escaped %'s, remove the escape character.
	 */
	/*local*/{
		register int escaped = FALSE;
		register int ch;
		register char *p;

		for (input_data = p = e->cmd; (ch = *input_data);
		     input_data++, p++) {
			if (p != input_data)
			    *p = ch;
			if (escaped) {
				if (ch == '%' || ch == '\\')
					*--p = ch;
				escaped = FALSE;
				continue;
			}
			if (ch == '\\') {
				escaped = TRUE;
				continue;
			}
			if (ch == '%') {
				*input_data++ = '\0';
				break;
			}
		}
		*p = '\0';
	}

	/* fork again, this time so we can exec the user's command.
	 */
#ifdef __APPLE__
	switch (fork()) {
#else
	switch (vfork()) {
#endif
	case -1:
		log_it("CRON",getpid(),"error","can't vfork");
		exit(ERROR_EXIT);
		/*NOTREACHED*/
	case 0:
		Debug(DPROC, ("[%d] grandchild process Vfork()'ed\n",
			      getpid()))

#ifndef __APPLE__
		if (e->uid == ROOT_UID)
			Jitter = RootJitter;
		if (Jitter != 0) {
			srandom(getpid());
			sleep(random() % Jitter);
		}
#endif

		/* write a log message.  we've waited this long to do it
		 * because it was not until now that we knew the PID that
		 * the actual user command shell was going to get and the
		 * PID is part of the log message.
		 */
		/*local*/{
			char *x = mkprints((u_char *)e->cmd, strlen(e->cmd));

			log_it(usernm, getpid(), "CMD", x);
			free(x);
		}

		/* that's the last thing we'll log.  close the log files.
		 */
#ifdef SYSLOG
		closelog();
#endif

		/* get new pgrp, void tty, etc.
		 */
		(void) setsid();

		/* close the pipe ends that we won't use.  this doesn't affect
		 * the parent, who has to read and write them; it keeps the
		 * kernel from recording us as a potential client TWICE --
		 * which would keep it from sending SIGPIPE in otherwise
		 * appropriate circumstances.
		 */
		close(stdin_pipe[WRITE_PIPE]);
		close(stdout_pipe[READ_PIPE]);

		/* grandchild process.  make std{in,out} be the ends of
		 * pipes opened by our daddy; make stderr go to stdout.
		 */
		close(STDIN);	dup2(stdin_pipe[READ_PIPE], STDIN);
		close(STDOUT);	dup2(stdout_pipe[WRITE_PIPE], STDOUT);
		close(STDERR);	dup2(STDOUT, STDERR);

		/* close the pipes we just dup'ed.  The resources will remain.
		 */
		close(stdin_pipe[READ_PIPE]);
		close(stdout_pipe[WRITE_PIPE]);

		/* set our login universe.  Do this in the grandchild
		 * so that the child can invoke /usr/lib/sendmail
		 * without surprises.
		 */
		do_univ(u);

#ifdef __APPLE__
		/* Set user's entire context, but skip the environment
		 * as cron provides a separate interface for this
		 */
		if ((pwd = getpwnam(e->uname))) {
			char envstr[MAXPATHLEN + sizeof "HOME="];

			uid = pwd->pw_uid;
			gid = pwd->pw_gid;

			if (pwd->pw_expire && time(NULL) >= pwd->pw_expire) {
				warn("user account expired: %s", e->uname);
				_exit(ERROR_EXIT);
			}

			sprintf(envstr, "HOME=%s", pwd->pw_dir);
			e->envp = env_set(e->envp, envstr);
			if (e->envp == NULL) {
				warn("env_set(%s)", envstr);
				_exit(ERROR_EXIT);
			}       
		} else {
			warn("getpwnam(\"%s\")", e->uname);
			_exit(ERROR_EXIT);
		}

		if (strlen(e->gname) > 0) {
			struct group *gr = getgrnam(e->gname);
			if (gr) {
				gid = gr->gr_gid;
			} else {
				warn("getgrnam(\"%s\")", e->gname);
				_exit(ERROR_EXIT);
			}
		}

		/* move to the correct bootstrap */
		/* similar to but simpler than pam_launchd */
		mach_port_t puc = MACH_PORT_NULL;
		kern_return_t kr = bootstrap_look_up_per_user(bootstrap_port, NULL, uid, &puc);
		if (kr != BOOTSTRAP_SUCCESS) {
			warnx("could not look up per-user bootstrap for uid %u", uid);
			_exit(ERROR_EXIT);
		}
		mach_port_mod_refs(mach_task_self(), bootstrap_port, MACH_PORT_RIGHT_SEND, -1);
		task_set_bootstrap_port(mach_task_self(), puc);
		bootstrap_port = puc;
		if (_vproc_post_fork_ping() != NULL) {
			warnx("failed to setup exception ports");
			_exit(ERROR_EXIT);
		}
#endif /* __APPLE__ */
# if defined(LOGIN_CAP)
		/* Set user's entire context, but skip the environment
		 * as cron provides a separate interface for this
		 */
		if ((pwd = getpwnam(usernm)) == NULL)
			pwd = getpwuid(e->uid);
		lc = NULL;
		if (pwd != NULL) {
			pwd->pw_gid = e->gid;
			if (e->class != NULL)
				lc = login_getclass(e->class);
		}
		if (pwd &&
		    setusercontext(lc, pwd, e->uid,
			    LOGIN_SETALL & ~(LOGIN_SETPATH|LOGIN_SETENV)) == 0)
			(void) endpwent();
		else {
			/* fall back to the old method */
			(void) endpwent();
# endif
			/* set our directory, uid and gid.  Set gid first,
			 * since once we set uid, we've lost root privileges.
			 */
#ifdef __APPLE__
			if (setgid(gid) != 0) {
#else
			if (setgid(e->gid) != 0) {
#endif
				log_it(usernm, getpid(),
				    "error", "setgid failed");
				exit(ERROR_EXIT);
			}
# if defined(BSD)
#ifdef __APPLE__
			if (initgroups(usernm, gid) != 0) {
#else
			if (initgroups(usernm, e->gid) != 0) {
#endif
				log_it(usernm, getpid(),
				    "error", "initgroups failed");
				exit(ERROR_EXIT);
			}
# endif
			if (setlogin(usernm) != 0) {
				log_it(usernm, getpid(),
				    "error", "setlogin failed");
				exit(ERROR_EXIT);
			}
#ifdef __APPLE__
			if (setuid(uid) != 0) {
#else
			if (setuid(e->uid) != 0) {
#endif
				log_it(usernm, getpid(),
				    "error", "setuid failed");
				exit(ERROR_EXIT);
			}
			/* we aren't root after this..*/
#if defined(LOGIN_CAP)
		}
		if (lc != NULL)
			login_close(lc);
#endif
		chdir(env_get("HOME", e->envp));

		/* exec the command.
		 */
		{
			char	*shell = env_get("SHELL", e->envp);

# if DEBUGGING
			if (DebugFlags & DTEST) {
				fprintf(stderr,
				"debug DTEST is on, not exec'ing command.\n");
				fprintf(stderr,
				"\tcmd='%s' shell='%s'\n", e->cmd, shell);
				_exit(OK_EXIT);
			}
# endif /*DEBUGGING*/
			execle(shell, shell, "-c", e->cmd, (char *)0, e->envp);
			warn("execl: couldn't exec `%s'", shell);
			_exit(ERROR_EXIT);
		}
		break;
	default:
		/* parent process */
		break;
	}

	children++;

	/* middle process, child of original cron, parent of process running
	 * the user's command.
	 */

	Debug(DPROC, ("[%d] child continues, closing pipes\n", getpid()))

	/* close the ends of the pipe that will only be referenced in the
	 * grandchild process...
	 */
	close(stdin_pipe[READ_PIPE]);
	close(stdout_pipe[WRITE_PIPE]);

	/*
	 * write, to the pipe connected to child's stdin, any input specified
	 * after a % in the crontab entry.  while we copy, convert any
	 * additional %'s to newlines.  when done, if some characters were
	 * written and the last one wasn't a newline, write a newline.
	 *
	 * Note that if the input data won't fit into one pipe buffer (2K
	 * or 4K on most BSD systems), and the child doesn't read its stdin,
	 * we would block here.  thus we must fork again.
	 */

	if (*input_data && fork() == 0) {
		register FILE	*out = fdopen(stdin_pipe[WRITE_PIPE], "w");
		register int	need_newline = FALSE;
		register int	escaped = FALSE;
		register int	ch;

		if (out == NULL) {
			warn("fdopen failed in child2");
			_exit(ERROR_EXIT);
		}

		Debug(DPROC, ("[%d] child2 sending data to grandchild\n", getpid()))

		/* close the pipe we don't use, since we inherited it and
		 * are part of its reference count now.
		 */
		close(stdout_pipe[READ_PIPE]);

		/* translation:
		 *	\% -> %
		 *	%  -> \n
		 *	\x -> \x	for all x != %
		 */
		while ((ch = *input_data++)) {
			if (escaped) {
				if (ch != '%')
					putc('\\', out);
			} else {
				if (ch == '%')
					ch = '\n';
			}

			if (!(escaped = (ch == '\\'))) {
				putc(ch, out);
				need_newline = (ch != '\n');
			}
		}
		if (escaped)
			putc('\\', out);
		if (need_newline)
			putc('\n', out);

		/* close the pipe, causing an EOF condition.  fclose causes
		 * stdin_pipe[WRITE_PIPE] to be closed, too.
		 */
		fclose(out);

		Debug(DPROC, ("[%d] child2 done sending to grandchild\n", getpid()))
		exit(0);
	}

	/* close the pipe to the grandkiddie's stdin, since its wicked uncle
	 * ernie back there has it open and will close it when he's done.
	 */
	close(stdin_pipe[WRITE_PIPE]);

	children++;

	/*
	 * read output from the grandchild.  it's stderr has been redirected to
	 * it's stdout, which has been redirected to our pipe.  if there is any
	 * output, we'll be mailing it to the user whose crontab this is...
	 * when the grandchild exits, we'll get EOF.
	 */

	Debug(DPROC, ("[%d] child reading output from grandchild\n", getpid()))

	/*local*/{
		register FILE	*in = fdopen(stdout_pipe[READ_PIPE], "r");
		register int	ch;

		if (in == NULL) {
			warn("fdopen failed in child");
			_exit(ERROR_EXIT);
		}

		ch = getc(in);
		if (ch != EOF) {
			register FILE	*mail = NULL;
			register int	bytes = 1;
			int		status = 0;

			Debug(DPROC|DEXT,
				("[%d] got data (%x:%c) from grandchild\n",
					getpid(), ch, ch))

			/* get name of recipient.  this is MAILTO if set to a
			 * valid local username; USER otherwise.
			 */
			if (mailto) {
				/* MAILTO was present in the environment
				 */
				if (!*mailto) {
					/* ... but it's empty. set to NULL
					 */
					mailto = NULL;
				}
			} else {
				/* MAILTO not present, set to USER.
				 */
				mailto = usernm;
			}

			/* if we are supposed to be mailing, MAILTO will
			 * be non-NULL.  only in this case should we set
			 * up the mail command and subjects and stuff...
			 */

			if (mailto) {
				register char	**env;
				auto char	mailcmd[MAX_COMMAND];
				auto char	hostname[MAXHOSTNAMELEN];

				(void) gethostname(hostname, MAXHOSTNAMELEN);
				(void) snprintf(mailcmd, sizeof(mailcmd),
					       MAILARGS, MAILCMD);
				if (!(mail = cron_popen(mailcmd, "w", e))) {
					warn("%s", MAILCMD);
					(void) _exit(ERROR_EXIT);
				}
				fprintf(mail, "From: %s (Cron Daemon)\n", usernm);
				fprintf(mail, "To: %s\n", mailto);
				fprintf(mail, "Subject: Cron <%s@%s> %s\n",
					usernm, first_word(hostname, "."),
					e->cmd);
# if defined(MAIL_DATE)
				fprintf(mail, "Date: %s\n",
					arpadate(&TargetTime));
# endif /* MAIL_DATE */
				for (env = e->envp;  *env;  env++)
					fprintf(mail, "X-Cron-Env: <%s>\n",
						*env);
				fprintf(mail, "\n");

				/* this was the first char from the pipe
				 */
				putc(ch, mail);
			}

			/* we have to read the input pipe no matter whether
			 * we mail or not, but obviously we only write to
			 * mail pipe if we ARE mailing.
			 */

			while (EOF != (ch = getc(in))) {
				bytes++;
				if (mailto)
					putc(ch, mail);
			}

			/* only close pipe if we opened it -- i.e., we're
			 * mailing...
			 */

			if (mailto) {
				Debug(DPROC, ("[%d] closing pipe to mail\n",
					getpid()))
				/* Note: the pclose will probably see
				 * the termination of the grandchild
				 * in addition to the mail process, since
				 * it (the grandchild) is likely to exit
				 * after closing its stdout.
				 */
				status = cron_pclose(mail);
			}

			/* if there was output and we could not mail it,
			 * log the facts so the poor user can figure out
			 * what's going on.
			 */
			if (mailto && status) {
				char buf[MAX_TEMPSTR];

				snprintf(buf, sizeof(buf),
			"mailed %d byte%s of output but got status 0x%04x\n",
					bytes, (bytes==1)?"":"s",
					status);
				log_it(usernm, getpid(), "MAIL", buf);
			}

		} /*if data from grandchild*/

		Debug(DPROC, ("[%d] got EOF from grandchild\n", getpid()))

		fclose(in);	/* also closes stdout_pipe[READ_PIPE] */
	}

	/* wait for children to die.
	 */
	for (;  children > 0;  children--)
	{
		WAIT_T		waiter;
		PID_T		pid;

		Debug(DPROC, ("[%d] waiting for grandchild #%d to finish\n",
			getpid(), children))
		pid = wait(&waiter);
		if (pid < OK) {
			Debug(DPROC, ("[%d] no more grandchildren--mail written?\n",
				getpid()))
			break;
		}
		Debug(DPROC, ("[%d] grandchild #%d finished, status=%04x",
			getpid(), pid, WEXITSTATUS(waiter)))
		if (WIFSIGNALED(waiter) && WCOREDUMP(waiter))
			Debug(DPROC, (", dumped core"))
		Debug(DPROC, ("\n"))
	}
}


static void
do_univ(u)
	user	*u;
{
#ifdef __APPLE__
	u = u; // avoid unused argument warning
#endif
#if defined(sequent)
/* Dynix (Sequent) hack to put the user associated with
 * the passed user structure into the ATT universe if
 * necessary.  We have to dig the gecos info out of
 * the user's password entry to see if the magic
 * "universe(att)" string is present.
 */

	struct	passwd	*p;
	char	*s;
	int	i;

	p = getpwuid(u->uid);
	(void) endpwent();

	if (p == NULL)
		return;

	s = p->pw_gecos;

	for (i = 0; i < 4; i++)
	{
		if ((s = strchr(s, ',')) == NULL)
			return;
		s++;
	}
	if (strcmp(s, "universe(att)"))
		return;

	(void) universe(U_ATT);
#endif
}
