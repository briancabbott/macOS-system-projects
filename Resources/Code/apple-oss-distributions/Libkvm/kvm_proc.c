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
/*-
 * Copyright (c) 1989, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software developed by the Computer Systems
 * Engineering group at Lawrence Berkeley Laboratory under DARPA contract
 * BG 91-66 and contributed to Berkeley.
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

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)kvm_proc.c	8.4 (Berkeley) 8/20/94";
#endif /* LIBC_SCCS and not lint */

/*
 * Proc traversal interface for kvm.  ps and w are (probably) the exclusive
 * users of this code, so we've factored it out into a separate module.
 * Thus, we keep this grunge out of the other kvm applications (i.e.,
 * most other applications are interested only in open/close/read/nlist).
 */

#include <sys/param.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <unistd.h>
#include <nlist.h>
#include <kvm.h>

#if defined(__APPLE__)
#include <sys/vm.h>
#include <mach/machine/vm_param.h>
#include <mach/mach_types.h>
#include <machine/vmparam.h>
/* XXX: Don't know what this should be! */
#warning Using VM_MAX_USER_ADDRESS = VM_MAX_ADDRESS
#define VM_MAXUSER_ADDRESS	VM_MAX_ADDRESS
#else
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/swap_pager.h>
#endif /* !NeXT */

#include <sys/sysctl.h>

#include <limits.h>
#include <db.h>
#include <paths.h>

#include "kvm_private.h"

#if defined(__APPLE__)
#include <stdio.h>

static char *
kvm_readswap(kd, p, va, cnt)
	kvm_t *kd;
	const struct proc *p;
	u_long va;
	u_long *cnt;
{
#warning kvm_readswap: not yet implemented
    /*fprintf(stderr, "kvm_readswap: not yet implemented\n");*/
    return NULL;
}
#else
static char *
kvm_readswap(kd, p, va, cnt)
	kvm_t *kd;
	const struct proc *p;
	u_long va;
	u_long *cnt;
{
	register int ix;
	register u_long addr, head;
	register u_long offset, pagestart, sbstart, pgoff;
	register off_t seekpoint;
	struct vm_map_entry vme;
	struct vm_object vmo;
	struct pager_struct pager;
	struct swpager swap;
	struct swblock swb;
	static char page[NBPG];

	head = (u_long)&p->p_vmspace->vm_map.header;
	/*
	 * Look through the address map for the memory object
	 * that corresponds to the given virtual address.
	 * The header just has the entire valid range.
	 */
	addr = head;
	while (1) {
		if (kvm_read(kd, addr, (char *)&vme, sizeof(vme)) != 
		    sizeof(vme))
			return (0);

		if (va >= vme.start && va <= vme.end && 
		    vme.object.vm_object != 0)
			break;

		addr = (u_long)vme.next;
		if (addr == 0 || addr == head)
			return (0);
	}
	/*
	 * We found the right object -- follow shadow links.
	 */
	offset = va - vme.start + vme.offset;
	addr = (u_long)vme.object.vm_object;
	while (1) {
		if (kvm_read(kd, addr, (char *)&vmo, sizeof(vmo)) != 
		    sizeof(vmo))
			return (0);
		addr = (u_long)vmo.shadow;
		if (addr == 0)
			break;
		offset += vmo.shadow_offset;
	}
	if (vmo.pager == 0)
		return (0);

	offset += vmo.paging_offset;
	/*
	 * Read in the pager info and make sure it's a swap device.
	 */
	addr = (u_long)vmo.pager;
	if (kvm_read(kd, addr, (char *)&pager, sizeof(pager)) != sizeof(pager)
	    || pager.pg_type != PG_SWAP)
		return (0);

	/*
	 * Read in the swap_pager private data, and compute the
	 * swap offset.
	 */
	addr = (u_long)pager.pg_data;
	if (kvm_read(kd, addr, (char *)&swap, sizeof(swap)) != sizeof(swap))
		return (0);
	ix = offset / dbtob(swap.sw_bsize);
	if (swap.sw_blocks == 0 || ix >= swap.sw_nblocks)
		return (0);

	addr = (u_long)&swap.sw_blocks[ix];
	if (kvm_read(kd, addr, (char *)&swb, sizeof(swb)) != sizeof(swb))
		return (0);

	sbstart = (offset / dbtob(swap.sw_bsize)) * dbtob(swap.sw_bsize);
	sbstart /= NBPG;
	pagestart = offset / NBPG;
	pgoff = pagestart - sbstart;

	if (swb.swb_block == 0 || (swb.swb_mask & (1 << pgoff)) == 0)
		return (0);

	seekpoint = dbtob(swb.swb_block) + ctob(pgoff);
	errno = 0;
	if (lseek(kd->swfd, seekpoint, 0) == -1 && errno != 0)
		return (0);
	if (read(kd->swfd, page, sizeof(page)) != sizeof(page))
		return (0);

	offset %= NBPG;
	*cnt = NBPG - offset;
	return (&page[offset]);
}
#endif /* !NeXT */

#define KREAD(kd, addr, obj) \
	(kvm_read(kd, addr, (char *)(obj), sizeof(*obj)) != sizeof(*obj))

/*
 * Read proc's from memory file into buffer bp, which has space to hold
 * at most maxcnt procs.
 */
static int
kvm_proclist(kd, what, arg, p, bp, maxcnt)
	kvm_t *kd;
	int what, arg;
	struct proc *p;
	struct kinfo_proc *bp;
	int maxcnt;
{
	return (-1);
}

/*
 * Build proc info array by reading in proc list from a crash dump.
 * Return number of procs read.  maxcnt is the max we will read.
 */
static int
kvm_deadprocs(kd, what, arg, a_allproc, a_zombproc, maxcnt)
	kvm_t *kd;
	int what, arg;
	u_long a_allproc;
	u_long a_zombproc;
	int maxcnt;
{
	return (-1);
}

struct kinfo_proc *
kvm_getprocs(kd, op, arg, cnt)
	kvm_t *kd;
	int op, arg;
	int *cnt;
{
	int mib[4], size, st, nprocs;

	if (kd->procbase != 0) {
		free((void *)kd->procbase);
		/* 
		 * Clear this pointer in case this call fails.  Otherwise,
		 * kvm_close() will free it again.
		 */
		kd->procbase = 0;
	}
	if (ISALIVE(kd)) {
		size = 0;
		mib[0] = CTL_KERN;
		mib[1] = KERN_PROC;
		mib[2] = op;
		mib[3] = arg;
		st = sysctl(mib, 4, NULL, &size, NULL, 0);
		if (st == -1) {
			_kvm_syserr(kd, kd->program, "kvm_getprocs");
			return (0);
		}
		kd->procbase = (struct kinfo_proc *)_kvm_malloc(kd, size);
		if (kd->procbase == 0)
			return (0);
		st = sysctl(mib, 4, kd->procbase, &size, NULL, 0);
		if (st == -1) {
			_kvm_syserr(kd, kd->program, "kvm_getprocs");
			return (0);
		}
		if (size % sizeof(struct kinfo_proc) != 0) {
			_kvm_err(kd, kd->program,
				"proc size mismatch (%d total, %d chunks)",
				size, sizeof(struct kinfo_proc));
			return (0);
		}
		nprocs = size / sizeof(struct kinfo_proc);
	} else {
		struct nlist nl[4], *p;

		nl[0].n_name = "_nprocs";
		nl[1].n_name = "_allproc";
		nl[2].n_name = "_zombproc";
		nl[3].n_name = 0;

		if (kvm_nlist(kd, nl) != 0) {
			for (p = nl; p->n_type != 0; ++p)
				;
			_kvm_err(kd, kd->program,
				 "%s: no such symbol", p->n_name);
			return (0);
		}
		if (KREAD(kd, nl[0].n_value, &nprocs)) {
			_kvm_err(kd, kd->program, "can't read nprocs");
			return (0);
		}
		size = nprocs * sizeof(struct kinfo_proc);
		kd->procbase = (struct kinfo_proc *)_kvm_malloc(kd, size);
		if (kd->procbase == 0)
			return (0);

		nprocs = kvm_deadprocs(kd, op, arg, nl[1].n_value,
				      nl[2].n_value, nprocs);
#ifdef notdef
		size = nprocs * sizeof(struct kinfo_proc);
		(void)realloc(kd->procbase, size);
#endif
	}
	*cnt = nprocs;
	return (kd->procbase);
}

void
_kvm_freeprocs(kd)
	kvm_t *kd;
{
	if (kd->procbase) {
		free(kd->procbase);
		kd->procbase = 0;
	}
}

void *
_kvm_realloc(kd, p, n)
	kvm_t *kd;
	void *p;
	size_t n;
{
	void *np = (void *)realloc(p, n);

	if (np == 0)
		_kvm_err(kd, kd->program, "out of memory");
	return (np);
}

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

/*
 * Read in an argument vector from the user address space of process p.
 * addr if the user-space base address of narg null-terminated contiguous 
 * strings.  This is used to read in both the command arguments and
 * environment strings.  Read at most maxcnt characters of strings.
 */
static char **
kvm_argv(kd, p, addr, narg, maxcnt)
	kvm_t *kd;
	struct proc *p;
	register u_long addr;
	register int narg;
	register int maxcnt;
{
	register char *cp;
	register int len, cc;
	register char **argv;

	/*
	 * Check that there aren't an unreasonable number of agruments,
	 * and that the address is in user space.
	 */
	if (narg > 512 || addr < VM_MIN_ADDRESS || addr >= VM_MAXUSER_ADDRESS)
		return (0);

	if (kd->argv == 0) {
		/*
		 * Try to avoid reallocs.
		 */
		kd->argc = MAX(narg + 1, 32);
		kd->argv = (char **)_kvm_malloc(kd, kd->argc * 
						sizeof(*kd->argv));
		if (kd->argv == 0)
			return (0);
	} else if (narg + 1 > kd->argc) {
		kd->argc = MAX(2 * kd->argc, narg + 1);
		kd->argv = (char **)_kvm_realloc(kd, kd->argv, kd->argc * 
						sizeof(*kd->argv));
		if (kd->argv == 0)
			return (0);
	}
	if (kd->argspc == 0) {
		kd->argspc = (char *)_kvm_malloc(kd, NBPG);
		if (kd->argspc == 0)
			return (0);
		kd->arglen = NBPG;
	}
	cp = kd->argspc;
	argv = kd->argv;
	*argv = cp;
	len = 0;
	/*
	 * Loop over pages, filling in the argument vector.
	 */
	while (addr < VM_MAXUSER_ADDRESS) {
		cc = NBPG - (addr & PGOFSET);
		if (maxcnt > 0 && cc > maxcnt - len)
			cc = maxcnt - len;;
		if (len + cc > kd->arglen) {
			register int off;
			register char **pp;
			register char *op = kd->argspc;

			kd->arglen *= 2;
			kd->argspc = (char *)_kvm_realloc(kd, kd->argspc,
							  kd->arglen);
			if (kd->argspc == 0)
				return (0);
			cp = &kd->argspc[len];
			/*
			 * Adjust argv pointers in case realloc moved
			 * the string space.
			 */
			off = kd->argspc - op;
			for (pp = kd->argv; pp < argv; ++pp)
				*pp += off;
		}
		if (kvm_uread(kd, p, addr, cp, cc) != cc)
			/* XXX */
			return (0);
		len += cc;
		addr += cc;

		if (maxcnt == 0 && len > 16 * NBPG)
			/* sanity */
			return (0);

		while (--cc >= 0) {
			if (*cp++ == 0) {
				if (--narg <= 0) {
					*++argv = 0;
					return (kd->argv);
				} else
					*++argv = cp;
			}
		}
		if (maxcnt > 0 && len >= maxcnt) {
			/*
			 * We're stopping prematurely.  Terminate the
			 * argv and current string.
			 */
			*++argv = 0;
			*cp = 0;
			return (kd->argv);
		}
	}
}

static void
ps_str_a(p, addr, n)
	struct ps_strings *p;
	u_long *addr;
	int *n;
{
	*addr = (u_long)p->ps_argvstr;
	*n = p->ps_nargvstr;
}

static void
ps_str_e(p, addr, n)
	struct ps_strings *p;
	u_long *addr;
	int *n;
{
	*addr = (u_long)p->ps_envstr;
	*n = p->ps_nenvstr;
}

#if !defined(USRSTACK)
#warning USRSTACK not defined! using VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS
#define USRSTACK (VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS)
#endif

static char **
kvm_doargv(kd, kp, nchr, info)
	kvm_t *kd;
	const struct kinfo_proc *kp;
	int nchr;
	int (*info)(struct ps_strings*, u_long *, int *);
{
	return (0);
}

/*
 * Get the command args.  This code is now machine independent.
 */
char **
kvm_getargv(kd, kp, nchr)
	kvm_t *kd;
	const struct kinfo_proc *kp;
	int nchr;
{
	return (kvm_doargv(kd, kp, nchr, ps_str_a));
}

char **
kvm_getenvv(kd, kp, nchr)
	kvm_t *kd;
	const struct kinfo_proc *kp;
	int nchr;
{
	return (kvm_doargv(kd, kp, nchr, ps_str_e));
}

/*
 * Read from user space.  The user context is given by p.
 */
int
kvm_uread(kd, p, uva, buf, len)
	kvm_t *kd;
	register struct proc *p;
	register u_long uva;
	register char *buf;
	register size_t len;
{
	register char *cp;

	cp = buf;
	while (len > 0) {
		u_long pa;
		register int cc;
		
		cc = _kvm_uvatop(kd, p, uva, &pa);
		if (cc > 0) {
			if (cc > len)
				cc = len;
			errno = 0;
			if (lseek(kd->pmfd, (off_t)pa, 0) == -1 && errno != 0) {
				_kvm_err(kd, 0, "invalid address (%x)", uva);
				break;
			}
			cc = read(kd->pmfd, cp, cc);
			if (cc < 0) {
				_kvm_syserr(kd, 0, _PATH_MEM);
				break;
			} else if (cc < len) {
				_kvm_err(kd, kd->program, "short read");
				break;
			}
		} else if (ISALIVE(kd)) {
			/* try swap */
			register char *dp;
			int cnt;

			dp = kvm_readswap(kd, p, uva, &cnt);
			if (dp == 0) {
				_kvm_err(kd, 0, "invalid address (%x)", uva);
				return (0);
			}
			cc = MIN(cnt, len);
			bcopy(dp, cp, cc);
		} else
			break;
		cp += cc;
		uva += cc;
		len -= cc;
	}
	return (int)(cp - buf);
}
/*
 * Fill in an eproc structure for the specified process.
 */
#if 0
void
fill_externproc(p, exp)
	register struct proc *p;
	register struct extern_proc *exp;
{
	exp->p_ru  = p->p_ru ;
}
#endif
