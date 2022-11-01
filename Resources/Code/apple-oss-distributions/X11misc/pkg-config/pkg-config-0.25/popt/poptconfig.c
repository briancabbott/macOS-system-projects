/*
Copyright (c) 1998  Red Hat Software
 
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
 
The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.
 
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 
Except as contained in this notice, the name of the X Consortium shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the X Consortium.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_ALLOCA_H
# include <alloca.h>
#else
# ifdef _AIX
#  pragma alloca
# endif
#endif

#ifdef HAVE_MALLOC_H
# include <malloc.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "popt.h"
#include "poptint.h"

static void configLine(poptContext con, char * line) {
    int nameLength = strlen(con->appName);
    char * opt;
    struct poptAlias alias;
    char * entryType;
    char * longName = NULL;
    char shortName = '\0';
    
    if (strncmp(line, con->appName, nameLength)) return;
    line += nameLength;
    if (!*line || !isspace(*line)) return;
    while (*line && isspace(*line)) line++;
    entryType = line;

    while (!*line || !isspace(*line)) line++;
    *line++ = '\0';
    while (*line && isspace(*line)) line++;
    if (!*line) return;
    opt = line;

    while (!*line || !isspace(*line)) line++;
    *line++ = '\0';
    while (*line && isspace(*line)) line++;
    if (!*line) return;

    if (opt[0] == '-' && opt[1] == '-')
	longName = opt + 2;
    else if (opt[0] == '-' && !opt[2])
	shortName = opt[1];

    if (!strcmp(entryType, "alias")) {
	if (poptParseArgvString(line, &alias.argc, &alias.argv)) return;
	alias.longName = longName, alias.shortName = shortName;
	poptAddAlias(con, alias, 0);
    }
#ifndef _WIN32 /* exec stuff too complex to be worthwhile to port */
      else if (!strcmp(entryType, "exec")) {
	con->execs = realloc(con->execs, 
				sizeof(*con->execs) * (con->numExecs + 1));
	if (longName)
	    con->execs[con->numExecs].longName = strdup(longName);
	else
	    con->execs[con->numExecs].longName = NULL;

	con->execs[con->numExecs].shortName = shortName;
	con->execs[con->numExecs].script = strdup(line);
	
	con->numExecs++;
    }
#endif /* !_WIN32 */
}

int poptReadConfigFile(poptContext con, char * fn) {
    char * file, * chptr, * end;
    char * buf, * dst;
    int fd, rc;
    int fileLength;

    fd = open(fn, O_RDONLY);
    if (fd < 0) {
	if (errno == ENOENT)
	    return 0;
	else 
	    return POPT_ERROR_ERRNO;
    }

    fileLength = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, 0);

    file = alloca(fileLength + 1);
    if ((fd = read(fd, file, fileLength)) != fileLength) {
	rc = errno;
	close(fd);
	errno = rc;
	return POPT_ERROR_ERRNO;
    }
    close(fd);

    dst = buf = alloca(fileLength + 1);

    chptr = file;
    end = (file + fileLength);
    while (chptr < end) {
	switch (*chptr) {
	  case '\n':
	    *dst = '\0';
	    dst = buf;
	    while (*dst && isspace(*dst)) dst++;
	    if (*dst && *dst != '#') {
		configLine(con, dst);
	    }
	    chptr++;
	    break;
	  case '\\':
	    *dst++ = *chptr++;
	    if (chptr < end) {
		if (*chptr == '\n') 
		    dst--, chptr++;	
		    /* \ at the end of a line does not insert a \n */
		else
		    *dst++ = *chptr++;
	    }
	    break;
	  default:
	    *dst++ = *chptr++;
	}
    }

    return 0;
}

#ifndef _WIN32

int poptReadDefaultConfig(poptContext con, int useEnv) {
    char * fn, * home;
    int rc;

    if (!con->appName) return 0;

    rc = poptReadConfigFile(con, "/etc/popt");
    if (rc) return rc;
    if (getuid() != geteuid()) return 0;

    if ((home = getenv("HOME"))) {
	fn = alloca(strlen(home) + 20);
	strcpy(fn, home);
	strcat(fn, "/.popt");
	rc = poptReadConfigFile(con, fn);
	if (rc) return rc;
    }

    return 0;
}

#endif /* !_WIN32 */
