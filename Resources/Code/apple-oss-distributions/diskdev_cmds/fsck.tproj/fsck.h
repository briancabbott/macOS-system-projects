/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
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
 * Copyright (c) 1980, 1986, 1993
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

#ifndef __FSCK_H__
#define __FSCK_H__

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fstab.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>

/* Typedef Structures */

/* 
 * Describes the current partition and 
 * contains a link to the next one if avail. 
 */
typedef struct part {
	struct	part *next;		/* forward link of partitions on disk */
	char	*name;			/* device name */
	char	*fsname;		/* mounted filesystem name */
	char	*vfstype;		/* file system type (eg., "hfs" or "msdos") */
} part_t;

/*
 * Disk structure that describes the device and contains the list 
 * of partitions available to the disk. 
 */
typedef struct disk {
	char	*name;			/* disk base name */
	struct	disk *next;		/* forward link for list of disks */
	struct	part *part;		/* head of list of partitions on disk */
	int	pid;			/* If != 0, pid of proc working on */
} disk_t;


#define EEXIT			8				/* Standard Error exit code for fsck */
#define _PATH_SBIN		"/sbin"			/* Path prefix used for fork/exec */

#define ROOT_PASSNO		1		/* passno field for root FSes */
#define NONROOT_PASSNO	2 		/* passno field for non-root FSes */


/* Function prototypes */
int checkfstab(void);
void catchquit(int sig);
void catchsig(int sig);
int fs_checkable (struct fstab *fsp);
char *blockcheck(char *origname);
char *rawname(char *name);
char *unrawname(char *name);
void addpart(char *name, char *fsname, char *vfstype);
void destroy_part (part_t* part);
disk_t *finddisk (char *name);
int checkfilesys(disk_t *disk, int child);
int check_fs(char *vfstype, char *filesys);
void ignore_single_quit (int sig);
int build_disklist(void);
int do_diskchecks(void);


#endif



