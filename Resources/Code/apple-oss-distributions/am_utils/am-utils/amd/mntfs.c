/*
 * Copyright (c) 1997-2002 Erez Zadok
 * Copyright (c) 1990 Jan-Simon Pendry
 * Copyright (c) 1990 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry at Imperial College, London.
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
 *    must display the following acknowledgment:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
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
 *
 *
 * $Id: mntfs.c,v 1.1.1.2 2002/07/15 19:42:38 zarzycki Exp $
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amd.h>

qelem mfhead = {&mfhead, &mfhead};

int mntfs_allocated;


mntfs *
dup_mntfs(mntfs *mf)
{
  if (mf->mf_refc == 0) {
    if (mf->mf_cid)
      untimeout(mf->mf_cid);
    mf->mf_cid = 0;
  }
  mf->mf_refc++;

  return mf;
}


static void
init_mntfs(mntfs *mf, am_ops *ops, am_opts *mo, char *mp, char *info, char *auto_opts, char *mopts, char *remopts)
{
  mf->mf_ops = ops;
  mf->mf_fsflags = ops->nfs_fs_flags;
  mf->mf_fo = mo;
  mf->mf_mount = strdup(mp);
  mf->mf_real_mount = strdup(mp);
  mf->mf_info = strdup(info);
  mf->mf_auto = strdup(auto_opts);
  mf->mf_mopts = strdup(mopts);
  mf->mf_remopts = strdup(remopts);
  mf->mf_loopdev = NULL;
  mf->mf_refc = 1;
#ifdef HAVE_FS_AUTOFS
  /* Note: mo can be NULL for the root pseudo-mountpoint */
  if (mo && mo->opt_mount_type &&
      STREQ(mo->opt_mount_type, "autofs") &&
      amd_use_autofs)
    mf->mf_flags = MFF_AUTOFS;
  else
#endif /* HAVE_FS_AUTOFS */
    mf->mf_flags = 0;
  mf->mf_error = -1;
  mf->mf_cid = 0;
#ifdef HAVE_FS_AUTOFS
  mf->mf_autofs_fh = 0;
#endif /* HAVE_FS_AUTOFS */
  mf->mf_private = 0;
  mf->mf_prfree = 0;

  if (ops->ffserver)
    mf->mf_server = (*ops->ffserver) (mf);
  else
    mf->mf_server = 0;
}


static mntfs *
alloc_mntfs(am_ops *ops, am_opts *mo, char *mp, char *info, char *auto_opts, char *mopts, char *remopts)
{
  mntfs *mf = ALLOC(struct mntfs);

  init_mntfs(mf, ops, mo, mp, info, auto_opts, mopts, remopts);
  ins_que(&mf->mf_q, &mfhead);
  mntfs_allocated++;

  return mf;
}


mntfs *
find_mntfs(am_ops *ops, am_opts *mo, char *mp, char *info, char *auto_opts, char *mopts, char *remopts)
{
  mntfs *mf;

  dlog("Locating mntfs reference to %s", mp);

  ITER(mf, mntfs, &mfhead) {
    if (STREQ(mf->mf_mount, mp) && STREQ(mf->mf_info, info)) {
      /*
       * Handle cases where error ops are involved
       */
      if (ops == &amfs_error_ops) {
	/*
	 * If the existing ops are not amfs_error_ops
	 * then continue...
	 */
	if (mf->mf_ops != &amfs_error_ops)
	  continue;
	else
	  return dup_mntfs(mf);
      } else {			/* ops != &amfs_error_ops */
	/*
	 * If the existing ops are amfs_error_ops
	 * then continue...
	 */
	if (mf->mf_ops == &amfs_error_ops)
	  continue;
      }

      if ((mf->mf_flags & MFF_RESTART) && amd_state == Run) {
	/*
	 * Restart a previously mounted filesystem.
	 */
	mntfs *mf2 = alloc_mntfs(&amfs_inherit_ops, mo, mp, info, auto_opts, mopts, remopts);
	dlog("Restarting filesystem %s", mf->mf_mount);

	/*
	 * Remember who we are restarting
	 */
	mf2->mf_private = (voidp) dup_mntfs(mf);
	mf2->mf_prfree = free_mntfs;
	return mf2;
      }

      mf->mf_fo = mo;
      if (!(mf->mf_flags & (MFF_MOUNTED | MFF_MOUNTING | MFF_UNMOUNTING))) {
	fserver *fs;
	mf->mf_flags &= ~MFF_ERROR;
	mf->mf_error = -1;
	mf->mf_auto = strealloc(mf->mf_auto, auto_opts);
	mf->mf_mopts = strealloc(mf->mf_mopts, mopts);
	mf->mf_remopts = strealloc(mf->mf_remopts, remopts);
	mf->mf_info = strealloc(mf->mf_info, info);

	if (mf->mf_private && mf->mf_prfree) {
	  mf->mf_prfree(mf->mf_private);
	  mf->mf_private = 0;
	}

	fs = ops->ffserver ? (*ops->ffserver) (mf) : (fserver *) NULL;
	if (mf->mf_server)
	  free_srvr(mf->mf_server);
	mf->mf_server = fs;
      }
      return dup_mntfs(mf);
    } /* end of "if (STREQ(mf-> ..." */
  } /* end of ITER */

  return alloc_mntfs(ops, mo, mp, info, auto_opts, mopts, remopts);
}


mntfs *
new_mntfs(void)
{
  return alloc_mntfs(&amfs_error_ops, (am_opts *) 0, "//nil//", ".", "", "", "");
}


static void
uninit_mntfs(mntfs *mf)
{
  if (mf->mf_auto)
    XFREE(mf->mf_auto);
  if (mf->mf_mopts)
    XFREE(mf->mf_mopts);
  if (mf->mf_remopts)
    XFREE(mf->mf_remopts);
  if (mf->mf_info)
    XFREE(mf->mf_info);
#ifdef HAVE_FS_AUTOFS
  /* shouldn't be necessary, but we do it just in case */
  if (mf->mf_autofs_fh) {
    autofs_release_fh(mf->mf_autofs_fh);
    mf->mf_autofs_fh = 0;
  }
#endif /* HAVE_FS_AUTOFS */
  if (mf->mf_private && mf->mf_prfree)
    (*mf->mf_prfree) (mf->mf_private);

  if (mf->mf_mount)
    XFREE(mf->mf_mount);
  if (mf->mf_real_mount)
    XFREE(mf->mf_real_mount);

  /*
   * Clean up the file server
   */
  if (mf->mf_server)
    free_srvr(mf->mf_server);

  /*
   * Don't do a callback on this mount
   */
  if (mf->mf_cid) {
    untimeout(mf->mf_cid);
    mf->mf_cid = 0;
  }
}


static void
discard_mntfs(voidp v)
{
  mntfs *mf = v;

  rem_que(&mf->mf_q);

  /*
   * Free memory
   */
  uninit_mntfs(mf);
  XFREE(mf);

  --mntfs_allocated;
}


void
flush_mntfs(void)
{
  mntfs *mf;

  mf = AM_FIRST(mntfs, &mfhead);
  while (mf != HEAD(mntfs, &mfhead)) {
    mntfs *mf2 = mf;
    mf = NEXT(mntfs, mf);
    if (mf2->mf_refc == 0 && mf2->mf_cid)
      discard_mntfs(mf2);
  }
}


void
free_mntfs(voidp v)
{
  mntfs *mf = v;

  if (--mf->mf_refc == 0) {
    if (mf->mf_flags & MFF_MOUNTED) {
      int quoted;
      mf->mf_flags &= ~MFF_MOUNTED;

      /*
       * Record for posterity
       */
      quoted = strchr(mf->mf_info, ' ') != 0;	/* cheap */
      plog(XLOG_INFO, "%s%s%s %sed fstype %s from %s",
	   quoted ? "\"" : "",
	   mf->mf_info,
	   quoted ? "\"" : "",
	   mf->mf_error ? "discard" : "unmount",
	   mf->mf_ops->fs_type, mf->mf_mount);
    }

    if (mf->mf_fsflags & FS_DISCARD) {
      dlog("Immediately discarding mntfs for %s", mf->mf_mount);
      discard_mntfs(mf);

    } else {

      if (mf->mf_flags & MFF_RESTART) {
	dlog("Discarding remount hook for %s", mf->mf_mount);
      } else {
	dlog("Discarding last mntfs reference to %s fstype %s",
	     mf->mf_mount, mf->mf_ops->fs_type);
      }
      if (mf->mf_flags & (MFF_MOUNTED | MFF_MOUNTING | MFF_UNMOUNTING))
	dlog("mntfs reference for %s still active", mf->mf_mount);
      mf->mf_cid = timeout(ALLOWED_MOUNT_TIME, discard_mntfs, (voidp) mf);
    }
  }
}


mntfs *
realloc_mntfs(mntfs *mf, am_ops *ops, am_opts *mo, char *mp, char *info, char *auto_opts, char *mopts, char *remopts)
{
  mntfs *mf2;

  if (mf->mf_refc == 1 &&
      mf->mf_ops == &amfs_inherit_ops &&
      STREQ(mf->mf_mount, mp)) {
    /*
     * If we are inheriting then just return
     * the same node...
     */
    return mf;
  }

  /*
   * Re-use the existing mntfs if it is mounted.
   * This traps a race in nfsx.
   */
  if (mf->mf_ops != &amfs_error_ops &&
      (mf->mf_flags & MFF_MOUNTED) &&
      !FSRV_ISDOWN(mf->mf_server)) {
    mf->mf_fo = mo;
    return mf;
  }

  mf2 = find_mntfs(ops, mo, mp, info, auto_opts, mopts, remopts);
  free_mntfs(mf);
#if 0
  /*
   * XXX: EZK IS THIS RIGHT???
   * The next "if" statement is what supposedly fixes bgmount() in
   * that it will actually use the ops structure of the next mount
   * entry, if the previous one failed.
   */
  if (mf2 &&
      ops &&
      mf2->mf_ops != ops &&
      mf2->mf_ops != &amfs_inherit_ops &&
      mf2->mf_ops != &amfs_toplvl_ops &&
      mf2->mf_ops != &amfs_error_ops) {
    plog(XLOG_WARNING, "realloc_mntfs: copy fallback ops \"%s\" over \"%s\"",
	 ops->fs_type, mf2->mf_ops->fs_type);
    mf2->mf_ops = ops;
  }
#endif
  return mf2;
}
