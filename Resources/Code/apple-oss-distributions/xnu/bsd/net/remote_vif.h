/*
 * Copyright (c) 2010-2021 Apple Inc. All rights reserved.
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

#ifndef __REMOTE_VIF_H__
#define __REMOTE_VIF_H__

#include <sys/proc.h>
#include <net/if.h>
#include <net/bpf.h>

#include <net/pktap.h>

#define RVI_CONTROL_NAME        "com.apple.net.rvi_control"
#define RVI_BUFFERSZ            (64 * 1024)
#define RVI_VERSION_1           0x1
#define RVI_VERSION_2           0x2
#define RVI_VERSION_CURRENT     RVI_VERSION_2

enum  {
	RVI_COMMAND_OUT_PAYLOAD         = 0x01,
	RVI_COMMAND_IN_PAYLOAD          = 0x10,
	RVI_COMMAND_GET_INTERFACE       = 0x20,
	RVI_COMMAND_VERSION             = 0x40
};

#ifdef XNU_KERNEL_PRIVATE
int rvi_init(void);
#endif /* XNU_KERNEL_PRIVATE */

#endif /* __REMOTE_VIF_H__ */
