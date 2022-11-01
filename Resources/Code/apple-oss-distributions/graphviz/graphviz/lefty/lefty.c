/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/

#pragma prototyped
/* Lefteris Koutsofios - AT&T Bell Laboratories */

#ifdef FEATURE_GTK
#include <gtk/gtk.h>
#include <glib/gmain.h>
#endif

#include "common.h"
#include "g.h"
#include "code.h"
#include "io.h"
#include "mem.h"
#include "tbl.h"
#include "parse.h"
#include "exec.h"
#include "str.h"
#include "display.h"
#include "internal.h"
#include "txtview.h"
#include "gfxview.h"
#ifdef FEATURE_GMAP
#include "gmap.h"
#include "gmap2l.h"
#endif
#ifndef FEATURE_MS
#include <sys/time.h>
#ifdef HAVE_UNISTD_H
#include <sys/types.h>
#include <unistd.h>
#endif
#ifdef STATS
#include <sys/resource.h>
#endif
#endif

/* the gnu way is to test for features explicitly */
#ifdef HAVE_FILE_R
#define canread(fp) ((fp)->_r > 0)
#else
#ifdef HAVE_FILE_IO_READ_END
#define canread(fp) ((fp)->_IO_read_end > (fp)->_IO_read_ptr)
#else
#ifdef HAVE_FILE_CNT
#define canread(fp) ((fp)->_cnt > 0)
#else
#ifdef HAVE_FILE_NEXT
#define canread(fp) ((fp)->next < (fp)->endb)
#else
#ifdef HAVE___FREADABLE
#define canread(fp) (__freadable(fp))

/* the other way is to presume a feature based on OS */
#else
#ifdef FEATURE_CS
#define canread(fp) ((fp)->next < (fp)->endb)
#else
#ifdef FEATURE_GNU
#define canread(fp) ((fp)->_IO_read_end > (fp)->_IO_read_ptr)
#else
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__Mac_OSX__)
#define canread(fp) ((fp)->_r > 0)
#endif
#endif
#endif
#endif

#endif
#endif
#endif
#endif

#ifdef FEATURE_GMAP
static int gmapon;
#endif

static Grect_t txtcoords = {
    { 1,  1 }, { 449, 599 },
};

#ifdef STATS
static time_t stime;
#endif

static int endflag = FALSE;

static char *exprstr;
static FILE *fp;

static int processinput (int);
static void processargs (int, char **);
static void processstr (char *);
static void printusage (void);

#if defined(FEATURE_X11) || defined(FEATURE_NONE) || defined(FEATURE_GTK)

int main (int argc, char **argv) {
    Tobj co;
    Psrc_t src;

#ifdef MTRACE
    extern int Mt_certify;
    Mt_certify = 1;
#endif
#ifdef STATS
    stime = time (NULL);
#endif

    idlerunmode = 0;
    exprstr = NULL;
    fp = NULL;
    init (argv[0]);
    Minit (GFXprune);
    Ginit ();
    FD_SET (Gxfd, &inputfds);

    Eerrlevel = 1;
    Estackdepth = 2;
    Eshowbody = 1;
    Eshowcalls = 1;

    processstr (leftyoptions);
    argv++, argc--;
    processargs (argc, argv);

    if (setjmp (exitljbuf))
        goto eop;

    Cinit ();
    IOinit ();
    Tinit ();
    Pinit ();
    Einit ();
    Sinit ();
    Dinit ();
    Iinit ();
    TXTinit (txtcoords);
    GFXinit ();
#ifdef FEATURE_GMAP
    gmapon = TRUE, G2Linit ();
#endif

    if (exprstr) {
        src.flag = CHARSRC, src.s = exprstr, src.fp = NULL;
        src.tok = -1, src.lnum = 1;
        while ((co = Punit (&src)))
            Eunit (co);
    }
    if (fp) {
        src.flag = FILESRC, src.s = NULL, src.fp = fp;
        src.tok = -1, src.lnum = 1;
        while ((co = Punit (&src)))
            Eunit (co);
    }
    if (endflag)
        goto eop;

    TXTupdate ();


#ifdef FEATURE_GTK
	while(gtk_events_pending()) {
		gtk_main_iteration();
	}

	Gneedredraw = FALSE;
	while(TRUE) {
		if(gtk_events_pending()) {
			GdkEvent *eve;

			gtk_main_iteration();
			if(Gneedredraw)
				GFXredraw(), Gneedredraw = FALSE;
			if(Gbuttonsdown > 0) {
				GFXmove(), Gprocessevents(FALSE, G_ONEEVENT);
				processinput(FALSE);
			} else {
				if(Mcouldgc) {
					if(!processinput(FALSE))
						Mdogc(M_GCINCR);
				} else if(idlerunmode) {
					if(!processinput(FALSE))
						GFXidle();
				} else
					processinput(TRUE);
			}
#ifdef FEATURE_GMAP
			if(gmapon)
				GMAPupdate();
#endif
			if(Erun)
				TXTupdate(), Erun = FALSE;
		}
	}
#else

    Gneedredraw = FALSE;
    for (;;) {
        if (Gneedredraw)
            GFXredraw (), Gneedredraw = FALSE;
        if (Gbuttonsdown > 0) {
            GFXmove (), Gprocessevents (FALSE, G_ONEEVENT);
            processinput (FALSE);
        } else {
            if (Mcouldgc) {
                if (!processinput (FALSE))
                    Mdogc (M_GCINCR);
            }
            if (idlerunmode) {
                if (!processinput (FALSE))
                    GFXidle ();
            } else
                processinput (TRUE);
        }
#ifdef FEATURE_GMAP
        if (gmapon)
            GMAPupdate ();
#endif
        if (Erun)
            TXTupdate (), Erun = FALSE;
    }
#endif   /* end of else part of FEATURE_GTK */

eop:
#ifdef PARANOID
#ifdef FEATURE_GMAP
    if (gmapon)
        G2Lterm ();
#endif
    GFXterm ();
    TXTterm ();
    Iterm ();
    Dterm ();
    Sterm ();
    Eterm ();
    Pterm ();
    Tterm ();
    IOterm ();
    Cterm ();
    Gterm ();
    Mterm ();
    FD_CLR (Gxfd, &inputfds);
    term ();
#endif
    printusage ();
    return 0;
}

#else

extern char ** __argv;
extern int __argc;

HANDLE hinstance, hprevinstance;

int PASCAL WinMain (HANDLE hInstance, HANDLE hPrevInstance,
        LPSTR lpCmdLine, int nCmdShow) {
    Tobj co;
    Psrc_t src;

    hinstance = hInstance;
    hprevinstance = hPrevInstance;
    idlerunmode = 0;
    exprstr = NULL;
    fp = NULL;
    init (NULL);
    Ginit ();
#ifndef FEATURE_MS
    FD_SET (Gxfd, &inputfds);
#endif

    Eerrlevel = 1;
    Estackdepth = 2;
    Eshowbody = 1;
    Eshowcalls = 1;

    processstr (leftyoptions);
    __argv++, __argc--;
    processargs (__argc, __argv);

    if (setjmp (exitljbuf))
        goto eop;

    Cinit ();
    IOinit ();
    Minit (GFXprune);
    Tinit ();
    Pinit ();
    Einit ();
    Sinit ();
    Dinit ();
    Iinit ();
    TXTinit (txtcoords);
    GFXinit ();

    if (exprstr) {
        src.flag = CHARSRC, src.s = exprstr, src.fp = NULL;
        src.tok = -1, src.lnum = 1;
        while ((co = Punit (&src)))
            Eunit (co);
    }
    if (fp) {
        src.flag = FILESRC, src.s = NULL, src.fp = fp;
        src.tok = -1, src.lnum = 1;
        while ((co = Punit (&src)))
            Eunit (co);
    }
    if (endflag)
        goto eop;

    TXTupdate ();

    Gneedredraw = FALSE;
    for (;;) {
        if (Gneedredraw)
            GFXredraw (), Gneedredraw = FALSE;
        if (Gbuttonsdown > 0)
            GFXmove (), Gprocessevents (FALSE, G_ONEEVENT);
        else {
            if (Mcouldgc) {
                if (!processinput (FALSE))
                    Mdogc (M_GCINCR);
            } else if (idlerunmode) {
                if (!processinput (FALSE))
                    GFXidle ();
            } else
                processinput (TRUE);
        }
        if (Erun)
            TXTupdate (), Erun = FALSE;
    }

eop:
#ifdef PARANOID
    GFXterm ();
    TXTterm ();
    Iterm ();
    Dterm ();
    Sterm ();
    Eterm ();
    Pterm ();
    Tterm ();
    Mterm ();
    IOterm ();
    Cterm ();
    Gterm ();
    term ();
#endif
    printusage ();
    exit (0);
}

#endif

#ifndef FEATURE_MS
static struct timeval longwait = { 1000, 0 };
static struct timeval zerowait = { 0, 0 };
#endif

static int processinput (int waitflag) {
    fd_set fdset;
#ifndef FEATURE_MS
    struct timeval tz, *tzp;
#else
    HANDLE evs[MAXIMUM_WAIT_OBJECTS];
    int evn;
#endif
    int n, rtn;
    int ioi;

    rtn = 0;
    while (Gprocessevents (FALSE, G_MANYEVENTS))
        rtn = 1;
#ifndef FEATURE_MS
    for (ioi = 0, n = 0; ioi < ion; ioi++)
        if (iop[ioi].inuse && iop[ioi].ismonitored &&
                FD_ISSET (fileno (iop[ioi].ifp), &inputfds) &&
                canread (iop[ioi].ifp))
            GFXmonitorfile (ioi), n++;
    if (n || rtn)
        return 1;
    if (Gneedredraw)
        return 0;
    tz = (waitflag && !rtn) ? longwait : zerowait;
    tzp = &tz;
    fdset = inputfds;
    if ((n = select (FD_SETSIZE, &fdset, NULL, NULL, tzp)) <= 0)
        return rtn;
    rtn = 1;
    if (FD_ISSET (Gxfd, &fdset))
        Gprocessevents (TRUE, G_MANYEVENTS), n--;
    if (!n)
        return rtn;
    Gsync ();
    for (ioi = 0; n > 0 && ioi < ion; ioi++)
        if (iop[ioi].inuse && iop[ioi].ismonitored &&
                FD_ISSET (fileno (iop[ioi].ifp), &fdset))
            GFXmonitorfile (ioi), n--;
#else
    for (ioi = 0, n = 0, evn = 0; ioi < ion; ioi++) {
        if (!iop[ioi].inuse || !IOismonitored (ioi, inputfds))
            continue;
        if ((iop[ioi].type == IO_FILE && canread (iop[ioi].ifp)) ||
                (iop[ioi].type == IO_PIPE && iop[ioi].buf[0]))
            GFXmonitorfile (ioi), n++;
        if (iop[ioi].type != IO_PIPE)
            continue;
        evs[evn++] = iop[ioi].ifp;
    }
    if (n)
        return 1;
    if (Gneedredraw)
        return 0;
    n = MsgWaitForMultipleObjects (evn, evs, FALSE,
            (waitflag && !rtn) ? 1 : 0, QS_ALLINPUT);
    if (n == WAIT_TIMEOUT || n < WAIT_OBJECT_0 || n > WAIT_OBJECT_0 + evn)
        return rtn;
    if (n == WAIT_OBJECT_0 + evn)
        Gprocessevents (TRUE, G_MANYEVENTS);
    Gsync ();
    for (ioi = 0; ioi < ion; ioi++)
        if (iop[ioi].inuse && IOismonitored (ioi) &&
                (iop[ioi].type == IO_FILE || (iop[ioi].type == IO_PIPE &&
                evs[n - WAIT_OBJECT_0] == iop[ioi].ifp)))
            GFXmonitorfile (ioi);
#endif
    return rtn;
}

static void processstr (char *buf) {
    char *words[100];
    char *s, *s1;
    int i;

    if (!(s = buf) || *s == 0)
        return;
    s = strdup (s);
    for (i = 0, s1 = s; *s1; ) {
        for (; *s1 && *s1 == ' '; s1++)
            ;
        if (!*s1)
            break;
        words[i++] = s1;
        for (; *s1 && *s1 != ' '; s1++)
            ;
        if (*s1)
            *s1 = '\000', s1++;
    }
    words[i] = NULL;
    processargs (i, words);
    free (s);
}

static void processargs (int argc, char *argv[]) {
    while (argc) {
        if (Strcmp (argv[0], "-x") == 0)
            endflag = TRUE;
        else if (Strcmp (argv[0], "-e") == 0)
            exprstr = argv[1], argv++, argc--;
        else if (Strcmp (argv[0], "-el") == 0)
            Eerrlevel = atoi (argv[1]), argv++, argc--;
        else if (Strcmp (argv[0], "-sd") == 0)
            Estackdepth = atoi (argv[1]), argv++, argc--;
        else if (Strcmp (argv[0], "-sb") == 0)
            Eshowbody = atoi (argv[1]), argv++, argc--;
        else if (Strcmp (argv[0], "-sc") == 0)
            Eshowcalls = atoi (argv[1]), argv++, argc--;
        else if (Strcmp (argv[0], "-df") == 0)
            Gdefaultfont = argv[1], argv++, argc--;
        else if (Strcmp (argv[0], "-w") == 0)
            warnflag = TRUE;
        else if (Strcmp (argv[0], "-ps") == 0)
            Gpscanvasname = argv[1], argv++, argc--;
        else if (Strcmp (argv[0], "-V") == 0)
            fprintf (stderr, "lefty version %s (%s)\n", VERSION, BUILDDATE);
        else if (Strcmp (argv[0], "-") == 0)
            fp = stdin;
        else {
            if ((fp = fopen (argv[0], "r")) == NULL)
                panic (POS, "main", "cannot open input file: %s", argv[0]);
        }
        argv++, argc--;
    }
    if (Eerrlevel > 1)
        Gerrflag = TRUE;
}

static void printusage (void) {
#ifdef STATS
    struct rusage ru;

    getrusage (RUSAGE_SELF, &ru);
    Mreport ();
#ifdef FEATURE_RUSAGE
    printf ("user   time %13.3lf\n",
            ru.ru_utime.tv_sec + ru.ru_utime.tv_nsec / 1000000000.0);
    printf ("system time %13.3lf\n",
            ru.ru_stime.tv_sec + ru.ru_stime.tv_nsec / 1000000000.0);
#else
    printf ("user   time %13.3lf\n",
            ru.ru_utime.tv_sec + ru.ru_utime.tv_usec / 1000000.0);
    printf ("system time %13.3lf\n",
            ru.ru_stime.tv_sec + ru.ru_stime.tv_usec / 1000000.0);
#endif
    printf ("resident size  %10d\n", ru.ru_maxrss * getpagesize ());
    printf ("input            %8d\n", ru.ru_inblock);
    printf ("output           %8d\n", ru.ru_oublock);
    printf ("socket msgs sent %8d\n", ru.ru_msgsnd);
    printf ("socket msgs rcvd %8d\n", ru.ru_msgrcv);
    printf ("real time        %8d\n", time (NULL) - stime);
    fflush (stdout);
#endif
}
