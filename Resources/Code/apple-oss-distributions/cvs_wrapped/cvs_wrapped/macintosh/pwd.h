/*********************************************************************
Project	:	GUSI				-	Grand Unified Socket Interface
File		:	pwd.h			-	Provide mission header ioctl.h for CodeWarrior
Author	:	Matthias Neeracher
Language	:	MPW C/C++
$Log: pwd.h,v $
Revision 1.3  2002/03/01 00:32:50  zarzycki
Reverting back to cvs-19

Revision 1.1.1.3  1999/09/02 04:55:07  wsanchez
Import of cvs-15 (Apple)

Revision 1.2  1998/04/07 07:13:37  wsanchez
Merged in cvs-1.9.26

Revision 1.1  1996/07/26 20:08:29  kingdon
Check in new macintosh directory from Mike Ladwig.  I believe that this
checkin contains exactly the tarfile he sent to me, with the exception
of the files SIOUX.c SIOUXGlobals.h SIOUXMenus.h SIOUXWindows.h which
are copyright Metrowerks and which we therefore cannot distribute.

*********************************************************************/

#include <sys/types.h>

struct  group { /* see getgrent(3) */
        char    *gr_name;
        char    *gr_passwd;
        gid_t   gr_gid;
        char    **gr_mem;
};

struct passwd {
        char    *pw_name;
        char    *pw_passwd;
        uid_t   pw_uid;
        gid_t   pw_gid;
        char    *pw_age;
        char    *pw_comment;
        char    *pw_gecos;
        char    *pw_dir;
        char    *pw_shell;
};
