/*
 * Copyright (c) 2000, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2010 Apple Inc. All rights reserved.
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
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 * $Id: file.c,v 1.4 2004/12/13 00:25:21 lindak Exp $
 */
#include <netsmb/smb_lib.h>
#include <netsmb/smb_conn.h>

int
smb_read(struct smb_ctx *ctx, smbfh fh, off_t offset, uint32_t count, char *dst)
{
	struct smbioc_rw rwrq;

	bzero(&rwrq, sizeof(rwrq));
	rwrq.ioc_version = SMB_IOC_STRUCT_VERSION;
	rwrq.ioc_fh = fh;
	rwrq.ioc_base = dst;
	rwrq.ioc_cnt = count;
	rwrq.ioc_offset = offset;
	if (smb_ioctl_call(ctx->ct_fd, SMBIOC_READ, &rwrq) == -1) {
		return -1;
	}
	return rwrq.ioc_cnt;
}

int 
smb_write(struct smb_ctx *ctx, smbfh fh, off_t offset, uint32_t count, const char *src)
{
	struct smbioc_rw rwrq;

	bzero(&rwrq, sizeof(rwrq));
	rwrq.ioc_version = SMB_IOC_STRUCT_VERSION;
	rwrq.ioc_fh = fh;
	rwrq.ioc_base = (char *)src;
	rwrq.ioc_cnt = count;
	rwrq.ioc_offset = offset;
	/* 
	 * Curretly we don't support Write Modes from user land. We do support paasing
	 * it down, but until we see a requirement lets leave it zero out.
	 */
	if (smb_ioctl_call(ctx->ct_fd, SMBIOC_WRITE, &rwrq) == -1) {
		return -1;
	}
	return rwrq.ioc_cnt;
}
