/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/* outdbg.c	output routines for the Netwide Assembler to produce
 *		a debugging trace
 *
 * The Netwide Assembler is copyright (C) 1996 Simon Tatham and
 * Julian Hall. All rights reserved. The software is
 * redistributable under the licence given in the file "Licence"
 * distributed in the NASM archive.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "nasm.h"
#include "nasmlib.h"
#include "outform.h"

#ifdef OF_DBG

struct Section {
    struct Section *next;
    long number;
    char *name;
} *dbgsect;

FILE *dbgf;
efunc dbgef;

static void dbg_init(FILE *fp, efunc errfunc, ldfunc ldef, evalfunc eval)
{
    dbgf = fp;
    dbgef = errfunc;
    dbgsect = NULL;
    (void) ldef;
    fprintf(fp,"NASM Output format debug dump\n");
}

static void dbg_cleanup(void)
{
    while (dbgsect) {
	struct Section *tmp = dbgsect;
	dbgsect = dbgsect->next;
	nasm_free (tmp->name);
	nasm_free (tmp);
    }
    fclose(dbgf);
}

static long dbg_section_names (char *name, int pass, int *bits)
{
    int seg;

    /*
     * We must have an initial default: let's make it 16.
     */
    if (!name)
	*bits = 16;

    if (!name)
	fprintf(dbgf, "section_name on init: returning %d\n",
		seg = seg_alloc());
    else {
	int n = strcspn(name, " \t");
	char *sname = nasm_strndup(name, n);
	struct Section *s;

	seg = NO_SEG;
	for (s = dbgsect; s; s = s->next)
	    if (!strcmp(s->name, sname))
		seg = s->number;
	
	if (seg == NO_SEG) {
	    s = nasm_malloc(sizeof(*s));
	    s->name = sname;
	    s->number = seg = seg_alloc();
	    s->next = dbgsect;
	    dbgsect = s;
	    fprintf(dbgf, "section_name %s (pass %d): returning %d\n",
		    name, pass, seg);
	}
    }
    return seg;
}

static void dbg_deflabel (char *name, long segment, long offset,
			  int is_global, char *special) {
    fprintf(dbgf,"deflabel %s := %08lx:%08lx %s (%d)%s%s\n",
	    name, segment, offset,
	    is_global == 2 ? "common" : is_global ? "global" : "local",
	    is_global,
	    special ? ": " : "", special);
}

static void dbg_out (long segto, void *data, unsigned long type,
		     long segment, long wrt) {
    long realbytes = type & OUT_SIZMASK;
    long ldata;
    int id;

    type &= OUT_TYPMASK;

    fprintf(dbgf,"out to %lx, len = %ld: ",segto,realbytes);

    switch(type) {
      case OUT_RESERVE:
	fprintf(dbgf,"reserved.\n"); break;
      case OUT_RAWDATA:
	fprintf(dbgf,"raw data = ");
	while (realbytes--) {
	    id = *(unsigned char *)data;
	    data = (char *)data + 1;
	    fprintf(dbgf,"%02x ",id);
	}
	fprintf(dbgf,"\n"); break;
      case OUT_ADDRESS:
	ldata = 0; /* placate gcc */
	if (realbytes == 1)
	    ldata = *((char *)data);
	else if (realbytes == 2)
	    ldata = *((short *)data);
	else if (realbytes == 4)
	    ldata = *((long *)data);
	fprintf(dbgf,"addr %08lx (seg %08lx, wrt %08lx)\n",ldata,
		segment,wrt);break;
      case OUT_REL2ADR:
	fprintf(dbgf,"rel2adr %04x (seg %08lx)\n",(int)*(short *)data,segment);
	break;
      case OUT_REL4ADR:
	fprintf(dbgf,"rel4adr %08lx (seg %08lx)\n",*(long *)data,segment);
	break;
      default:
	fprintf(dbgf,"unknown\n");
	break;
    }
}

static long dbg_segbase(long segment) {
    return segment;
}

static int dbg_directive (char *directive, char *value, int pass) {
    fprintf(dbgf, "directive [%s] value [%s] (pass %d)\n",
	    directive, value, pass);
    return 1;
}

static void dbg_filename (char *inname, char *outname, efunc error) {
    standard_extension (inname, outname, ".dbg", error);
}

struct ofmt of_dbg = {
    "Trace of all info passed to output stage",
    "dbg",
    NULL,
    dbg_init,
    dbg_out,
    dbg_deflabel,
    dbg_section_names,
    dbg_segbase,
    dbg_directive,
    dbg_filename,
    dbg_cleanup
};

#endif /* OF_DBG */
