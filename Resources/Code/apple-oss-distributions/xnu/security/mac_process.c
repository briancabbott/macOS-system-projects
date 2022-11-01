/*
 * Copyright (c) 2007-2010 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

/*-
 * Copyright (c) 1999, 2000, 2001, 2002 Robert N. M. Watson
 * Copyright (c) 2001 Ilmar S. Habibulin
 * Copyright (c) 2001, 2002, 2003, 2004 Networks Associates Technology, Inc.
 *
 * This software was developed by Robert Watson and Ilmar Habibulin for the
 * TrustedBSD Project.
 *
 * This software was developed for the FreeBSD Project in part by Network
 * Associates Laboratories, the Security Research Division of Network
 * Associates, Inc. under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"),
 * as part of the DARPA CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <string.h>
#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/malloc.h>
#include <sys/sbuf.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/proc_internal.h>
#include <sys/kauth.h>
#include <sys/imgact.h>
#include <mach/mach_types.h>
#include <kern/task.h>

#include <os/hash.h>

#include <security/mac_internal.h>
#include <security/mac_mach_internal.h>

#include <bsd/security/audit/audit.h>

struct label *
mac_cred_label_alloc(void)
{
	struct label *label;

	label = mac_labelzone_alloc(MAC_WAITOK);
	if (label == NULL) {
		return NULL;
	}
	MAC_PERFORM(cred_label_init, label);
	return label;
}

void
mac_cred_label_init(struct ucred *cred)
{
	cred->cr_label = mac_cred_label_alloc();
}

void
mac_cred_label_free(struct label *label)
{
	MAC_PERFORM(cred_label_destroy, label);
	mac_labelzone_free(label);
}

struct label *
mac_cred_label(struct ucred *cred)
{
	return cred->cr_label;
}

bool
mac_cred_label_is_equal(const struct label *a, const struct label *b)
{
	return memcmp(a->l_perpolicy, b->l_perpolicy, sizeof(a->l_perpolicy)) == 0;
}

uint32_t
mac_cred_label_hash_update(const struct label *a, uint32_t hash)
{
	return os_hash_jenkins_update(a->l_perpolicy, sizeof(a->l_perpolicy), hash);
}

int
mac_cred_label_externalize_audit(struct proc *p, struct mac *mac)
{
	kauth_cred_t cr;
	int error;

	cr = kauth_cred_proc_ref(p);

	error = MAC_EXTERNALIZE_AUDIT(cred, mac_cred_label(cr),
	    mac->m_string, mac->m_buflen);

	kauth_cred_unref(&cr);
	return error;
}

void
mac_cred_label_destroy(kauth_cred_t cred)
{
	struct label *label = mac_cred_label(cred);
	cred->cr_label = NULL;
	mac_cred_label_free(label);
}

int
mac_cred_label_externalize(struct label *label, char *elements,
    char *outbuf, size_t outbuflen, int flags __unused)
{
	int error = 0;

	error = MAC_EXTERNALIZE(cred, label, elements, outbuf, outbuflen);

	return error;
}

int
mac_cred_label_internalize(struct label *label, char *string)
{
	int error;

	error = MAC_INTERNALIZE(cred, label, string);

	return error;
}

/*
 * By default, fork just adds a reference to the parent
 * credential.  Policies may need to know about this reference
 * if they are tracking exit calls to know when to free the
 * label.
 */
void
mac_cred_label_associate_fork(kauth_cred_t cred, proc_t proc)
{
	MAC_PERFORM(cred_label_associate_fork, cred, proc);
}

/*
 * Initialize MAC label for the first kernel process, from which other
 * kernel processes and threads are spawned.
 */
void
mac_cred_label_associate_kernel(kauth_cred_t cred)
{
	MAC_PERFORM(cred_label_associate_kernel, cred);
}

/*
 * Initialize MAC label for the first userland process, from which other
 * userland processes and threads are spawned.
 */
void
mac_cred_label_associate_user(kauth_cred_t cred)
{
	MAC_PERFORM(cred_label_associate_user, cred);
}

/*
 * When a new process is created, its label must be initialized.  Generally,
 * this involves inheritence from the parent process, modulo possible
 * deltas.  This function allows that processing to take place.
 */
void
mac_cred_label_associate(struct ucred *parent_cred, struct ucred *child_cred)
{
	MAC_PERFORM(cred_label_associate, parent_cred, child_cred);
}

int
mac_execve_enter(user_addr_t mac_p, struct image_params *imgp)
{
	if (mac_p == USER_ADDR_NULL) {
		return 0;
	}

	return mac_do_set(current_proc(), mac_p,
	           ^(char *input, __unused size_t len) {
		struct label *execlabel;
		int error;

		execlabel = mac_cred_label_alloc();
		if ((error = mac_cred_label_internalize(execlabel, input))) {
		        mac_cred_label_free(execlabel);
		        execlabel = NULL;
		}

		imgp->ip_execlabelp = execlabel;
		return error;
	});
}

/*
 * When the subject's label changes, it may require revocation of privilege
 * to mapped objects.  This can't be done on-the-fly later with a unified
 * buffer cache.
 *
 * XXX:		CRF_MAC_ENFORCE should be in a kauth_cred_t field, rather
 * XXX:		than a posix_cred_t field.
 */
void
mac_cred_label_update(kauth_cred_t cred, struct label *newlabel)
{
	posix_cred_t pcred = posix_cred_get(cred);

	/* force label to be part of "matching" for credential */
	pcred->cr_flags |= CRF_MAC_ENFORCE;

	/* inform the policies of the update */
	MAC_PERFORM(cred_label_update, cred, newlabel);
}

int
mac_cred_check_label_update(kauth_cred_t cred, struct label *newlabel)
{
	int error;

#if SECURITY_MAC_CHECK_ENFORCE
	/* 21167099 - only check if we allow write */
	if (!mac_proc_enforce) {
		return 0;
	}
#endif

	MAC_CHECK(cred_check_label_update, cred, newlabel);

	return error;
}

int
mac_cred_check_visible(kauth_cred_t u1, kauth_cred_t u2)
{
	int error;

#if SECURITY_MAC_CHECK_ENFORCE
	/* 21167099 - only check if we allow write */
	if (!mac_proc_enforce) {
		return 0;
	}
#endif

	MAC_CHECK(cred_check_visible, u1, u2);

	return error;
}

int
mac_proc_check_debug(proc_ident_t tracing_ident, kauth_cred_t tracing_cred, proc_ident_t traced_ident)
{
	int error;
	bool enforce;
	proc_t tracingp;

#if SECURITY_MAC_CHECK_ENFORCE
	/* 21167099 - only check if we allow write */
	if (!mac_proc_enforce) {
		return 0;
	}
#endif
	/*
	 * Once all mac hooks adopt proc_ident_t, finding proc_t and releasing
	 * it below should go to mac_proc_check_enforce().
	 */
	if ((tracingp = proc_find_ident(tracing_ident)) == PROC_NULL) {
		return ESRCH;
	}
	enforce = mac_proc_check_enforce(tracingp);
	proc_rele(tracingp);

	if (!enforce) {
		return 0;
	}
	MAC_CHECK(proc_check_debug, tracing_cred, traced_ident);

	return error;
}

int
mac_proc_check_dump_core(struct proc *proc)
{
	int error;

#if SECURITY_MAC_CHECK_ENFORCE
	/* 21167099 - only check if we allow write */
	if (!mac_proc_enforce) {
		return 0;
	}
#endif
	if (!mac_proc_check_enforce(proc)) {
		return 0;
	}

	MAC_CHECK(proc_check_dump_core, proc);

	return error;
}

int
mac_proc_check_remote_thread_create(struct task *task, int flavor, thread_state_t new_state, mach_msg_type_number_t new_state_count)
{
	proc_t curp = current_proc();
	proc_t proc;
	kauth_cred_t cred;
	int error;

#if SECURITY_MAC_CHECK_ENFORCE
	/* 21167099 - only check if we allow write */
	if (!mac_proc_enforce) {
		return 0;
	}
#endif
	if (!mac_proc_check_enforce(curp)) {
		return 0;
	}

	proc = proc_find(task_pid(task));
	if (proc == PROC_NULL) {
		return ESRCH;
	}

	cred = kauth_cred_proc_ref(curp);
	MAC_CHECK(proc_check_remote_thread_create, cred, proc, flavor, new_state, new_state_count);
	kauth_cred_unref(&cred);
	proc_rele(proc);

	return error;
}

int
mac_proc_check_fork(proc_t curp)
{
	kauth_cred_t cred;
	int error;

#if SECURITY_MAC_CHECK_ENFORCE
	/* 21167099 - only check if we allow write */
	if (!mac_proc_enforce) {
		return 0;
	}
#endif
	if (!mac_proc_check_enforce(curp)) {
		return 0;
	}

	cred = kauth_cred_proc_ref(curp);
	MAC_CHECK(proc_check_fork, cred, curp);
	kauth_cred_unref(&cred);

	return error;
}

int
mac_proc_check_get_task(struct ucred *cred, proc_ident_t pident, mach_task_flavor_t flavor)
{
	int error;

	assert(flavor <= TASK_FLAVOR_NAME);

	/* Also call the old hook for compatability, deprecating in rdar://66356944. */
	if (flavor == TASK_FLAVOR_CONTROL) {
		MAC_CHECK(proc_check_get_task, cred, pident);
		if (error) {
			return error;
		}
	}

	if (flavor == TASK_FLAVOR_NAME) {
		MAC_CHECK(proc_check_get_task_name, cred, pident);
		if (error) {
			return error;
		}
	}

	MAC_CHECK(proc_check_get_task_with_flavor, cred, pident, flavor);

	return error;
}

int
mac_proc_check_expose_task(struct ucred *cred, proc_ident_t pident, mach_task_flavor_t flavor)
{
	int error;

	assert(flavor <= TASK_FLAVOR_NAME);

	/* Also call the old hook for compatability, deprecating in rdar://66356944. */
	if (flavor == TASK_FLAVOR_CONTROL) {
		MAC_CHECK(proc_check_expose_task, cred, pident);
		if (error) {
			return error;
		}
	}

	MAC_CHECK(proc_check_expose_task_with_flavor, cred, pident, flavor);

	return error;
}

int
mac_proc_check_inherit_ipc_ports(struct proc *p, struct vnode *cur_vp, off_t cur_offset, struct vnode *img_vp, off_t img_offset, struct vnode *scriptvp)
{
	int error;

	MAC_CHECK(proc_check_inherit_ipc_ports, p, cur_vp, cur_offset, img_vp, img_offset, scriptvp);

	return error;
}

/*
 * The type of maxprot in proc_check_map_anon must be equivalent to vm_prot_t
 * (defined in <mach/vm_prot.h>). mac_policy.h does not include any header
 * files, so cannot use the typedef itself.
 */
int
mac_proc_check_map_anon(proc_t proc, user_addr_t u_addr,
    user_size_t u_size, int prot, int flags, int *maxprot)
{
	kauth_cred_t cred;
	int error;

#if SECURITY_MAC_CHECK_ENFORCE
	/* 21167099 - only check if we allow write */
	if (!mac_vm_enforce) {
		return 0;
	}
#endif
	if (!mac_proc_check_enforce(proc)) {
		return 0;
	}

	cred = kauth_cred_proc_ref(proc);
	MAC_CHECK(proc_check_map_anon, proc, cred, u_addr, u_size, prot, flags, maxprot);
	kauth_cred_unref(&cred);

	return error;
}


int
mac_proc_check_memorystatus_control(proc_t proc, uint32_t command, pid_t pid)
{
	kauth_cred_t cred;
	int error;

#if SECURITY_MAC_CHECK_ENFORCE
	/* 21167099 - only check if we allow write */
	if (!mac_proc_enforce) {
		return 0;
	}
#endif
	if (!mac_proc_check_enforce(proc)) {
		return 0;
	}

	cred = kauth_cred_proc_ref(proc);
	MAC_CHECK(proc_check_memorystatus_control, cred, command, pid);
	kauth_cred_unref(&cred);

	return error;
}

int
mac_proc_check_mprotect(proc_t proc,
    user_addr_t addr, user_size_t size, int prot)
{
	kauth_cred_t cred;
	int error;

#if SECURITY_MAC_CHECK_ENFORCE
	/* 21167099 - only check if we allow write */
	if (!mac_vm_enforce) {
		return 0;
	}
#endif
	if (!mac_proc_check_enforce(proc)) {
		return 0;
	}

	cred = kauth_cred_proc_ref(proc);
	MAC_CHECK(proc_check_mprotect, cred, proc, addr, size, prot);
	kauth_cred_unref(&cred);

	return error;
}

int
mac_proc_check_run_cs_invalid(proc_t proc)
{
	int error;

#if SECURITY_MAC_CHECK_ENFORCE
	/* 21167099 - only check if we allow write */
	if (!mac_vm_enforce) {
		return 0;
	}
#endif

	MAC_CHECK(proc_check_run_cs_invalid, proc);

	return error;
}

void
mac_proc_notify_cs_invalidated(proc_t proc)
{
	MAC_PERFORM(proc_notify_cs_invalidated, proc);
}

int
mac_proc_check_sched(proc_t curp, struct proc *proc)
{
	kauth_cred_t cred;
	int error;

#if SECURITY_MAC_CHECK_ENFORCE
	/* 21167099 - only check if we allow write */
	if (!mac_proc_enforce) {
		return 0;
	}
#endif
	if (!mac_proc_check_enforce(curp)) {
		return 0;
	}

	cred = kauth_cred_proc_ref(curp);
	MAC_CHECK(proc_check_sched, cred, proc);
	kauth_cred_unref(&cred);

	return error;
}

int
mac_proc_check_signal(proc_t curp, struct proc *proc, int signum)
{
	kauth_cred_t cred;
	int error;

#if SECURITY_MAC_CHECK_ENFORCE
	/* 21167099 - only check if we allow write */
	if (!mac_proc_enforce) {
		return 0;
	}
#endif
	if (!mac_proc_check_enforce(curp)) {
		return 0;
	}

	cred = kauth_cred_proc_ref(curp);
	MAC_CHECK(proc_check_signal, cred, proc, signum);
	kauth_cred_unref(&cred);

	return error;
}

int
mac_proc_check_syscall_unix(proc_t curp, int scnum)
{
	int error;

#if SECURITY_MAC_CHECK_ENFORCE
	/* 21167099 - only check if we allow write */
	if (!mac_proc_enforce) {
		return 0;
	}
#endif
	if (!mac_proc_check_enforce(curp)) {
		return 0;
	}

	MAC_CHECK(proc_check_syscall_unix, curp, scnum);

	return error;
}

int
mac_proc_check_wait(proc_t curp, struct proc *proc)
{
	kauth_cred_t cred;
	int error;

#if SECURITY_MAC_CHECK_ENFORCE
	/* 21167099 - only check if we allow write */
	if (!mac_proc_enforce) {
		return 0;
	}
#endif
	if (!mac_proc_check_enforce(curp)) {
		return 0;
	}

	cred = kauth_cred_proc_ref(curp);
	MAC_CHECK(proc_check_wait, cred, proc);
	kauth_cred_unref(&cred);

	return error;
}

void
mac_proc_notify_exit(struct proc *proc)
{
	MAC_PERFORM(proc_notify_exit, proc);
}

int
mac_proc_check_suspend_resume(proc_t proc, int sr)
{
	kauth_cred_t cred;
	int error;

#if SECURITY_MAC_CHECK_ENFORCE
	/* 21167099 - only check if we allow write */
	if (!mac_proc_enforce) {
		return 0;
	}
#endif
	if (!mac_proc_check_enforce(current_proc())) {
		return 0;
	}

	cred = kauth_cred_proc_ref(current_proc());
	MAC_CHECK(proc_check_suspend_resume, cred, proc, sr);
	kauth_cred_unref(&cred);

	return error;
}

int
mac_proc_check_ledger(proc_t curp, proc_t proc, int ledger_op)
{
	kauth_cred_t cred;
	int error = 0;

#if SECURITY_MAC_CHECK_ENFORCE
	/* 21167099 - only check if we allow write */
	if (!mac_proc_enforce) {
		return 0;
	}
#endif
	if (!mac_proc_check_enforce(curp)) {
		return 0;
	}

	cred = kauth_cred_proc_ref(curp);
	MAC_CHECK(proc_check_ledger, cred, proc, ledger_op);
	kauth_cred_unref(&cred);

	return error;
}

int
mac_proc_check_proc_info(proc_t curp, proc_t target, int callnum, int flavor)
{
	kauth_cred_t cred;
	int error = 0;

#if SECURITY_MAC_CHECK_ENFORCE
	/* 21167099 - only check if we allow write */
	if (!mac_proc_enforce) {
		return 0;
	}
#endif
	if (!mac_proc_check_enforce(curp)) {
		return 0;
	}

	cred = kauth_cred_proc_ref(curp);
	MAC_CHECK(proc_check_proc_info, cred, target, callnum, flavor);
	kauth_cred_unref(&cred);

	return error;
}

int
mac_proc_check_get_cs_info(proc_t curp, proc_t target, unsigned int op)
{
	kauth_cred_t cred;
	int error = 0;

#if SECURITY_MAC_CHECK_ENFORCE
	/* 21167099 - only check if we allow write */
	if (!mac_proc_enforce) {
		return 0;
	}
#endif
	if (!mac_proc_check_enforce(curp)) {
		return 0;
	}

	cred = kauth_cred_proc_ref(curp);
	MAC_CHECK(proc_check_get_cs_info, cred, target, op);
	kauth_cred_unref(&cred);

	return error;
}

int
mac_proc_check_set_cs_info(proc_t curp, proc_t target, unsigned int op)
{
	kauth_cred_t cred;
	int error = 0;

#if SECURITY_MAC_CHECK_ENFORCE
	/* 21167099 - only check if we allow write */
	if (!mac_proc_enforce) {
		return 0;
	}
#endif
	if (!mac_proc_check_enforce(curp)) {
		return 0;
	}

	cred = kauth_cred_proc_ref(curp);
	MAC_CHECK(proc_check_set_cs_info, cred, target, op);
	kauth_cred_unref(&cred);

	return error;
}

int
mac_proc_check_setuid(proc_t curp, kauth_cred_t cred, uid_t uid)
{
	int error = 0;

#if SECURITY_MAC_CHECK_ENFORCE
	/* 21167099 - only check if we allow write */
	if (!mac_proc_enforce) {
		return 0;
	}
#endif
	if (!mac_proc_check_enforce(curp)) {
		return 0;
	}

	MAC_CHECK(proc_check_setuid, cred, uid);

	return error;
}

int
mac_proc_check_seteuid(proc_t curp, kauth_cred_t cred, uid_t euid)
{
	int error = 0;

#if SECURITY_MAC_CHECK_ENFORCE
	/* 21167099 - only check if we allow write */
	if (!mac_proc_enforce) {
		return 0;
	}
#endif
	if (!mac_proc_check_enforce(curp)) {
		return 0;
	}

	MAC_CHECK(proc_check_seteuid, cred, euid);

	return error;
}

int
mac_proc_check_setreuid(proc_t curp, kauth_cred_t cred, uid_t ruid, uid_t euid)
{
	int error = 0;

#if SECURITY_MAC_CHECK_ENFORCE
	/* 21167099 - only check if we allow write */
	if (!mac_proc_enforce) {
		return 0;
	}
#endif
	if (!mac_proc_check_enforce(curp)) {
		return 0;
	}

	MAC_CHECK(proc_check_setreuid, cred, ruid, euid);

	return error;
}

int
mac_proc_check_setgid(proc_t curp, kauth_cred_t cred, gid_t gid)
{
	int error = 0;

#if SECURITY_MAC_CHECK_ENFORCE
	/* 21167099 - only check if we allow write */
	if (!mac_proc_enforce) {
		return 0;
	}
#endif
	if (!mac_proc_check_enforce(curp)) {
		return 0;
	}

	MAC_CHECK(proc_check_setgid, cred, gid);

	return error;
}

int
mac_proc_check_setegid(proc_t curp, kauth_cred_t cred, gid_t egid)
{
	int error = 0;

#if SECURITY_MAC_CHECK_ENFORCE
	/* 21167099 - only check if we allow write */
	if (!mac_proc_enforce) {
		return 0;
	}
#endif
	if (!mac_proc_check_enforce(curp)) {
		return 0;
	}

	MAC_CHECK(proc_check_setegid, cred, egid);

	return error;
}

int
mac_proc_check_setregid(proc_t curp, kauth_cred_t cred, gid_t rgid, gid_t egid)
{
	int error = 0;

#if SECURITY_MAC_CHECK_ENFORCE
	/* 21167099 - only check if we allow write */
	if (!mac_proc_enforce) {
		return 0;
	}
#endif
	if (!mac_proc_check_enforce(curp)) {
		return 0;
	}

	MAC_CHECK(proc_check_setregid, cred, rgid, egid);

	return error;
}

int
mac_proc_check_settid(proc_t curp, uid_t uid, gid_t gid)
{
	kauth_cred_t pcred, tcred;
	int error = 0;

#if SECURITY_MAC_CHECK_ENFORCE
	/* 21167099 - only check if we allow write */
	if (!mac_proc_enforce) {
		return 0;
	}
#endif
	if (!mac_proc_check_enforce(curp)) {
		return 0;
	}

	pcred = kauth_cred_proc_ref(curp);
	tcred = kauth_cred_get_with_ref();
	MAC_CHECK(proc_check_settid, pcred, tcred, uid, gid);
	kauth_cred_unref(&tcred);
	kauth_cred_unref(&pcred);

	return error;
}
