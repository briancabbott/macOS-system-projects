/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 *      Copyright (c) 1997 Apple Computer, Inc.
 *
 *      The information contained herein is subject to change without
 *      notice and  should not be  construed as a commitment by Apple
 *      Computer, Inc. Apple Computer, Inc. assumes no responsibility
 *      for any errors that may appear.
 *
 *      Confidential and Proprietary to Apple Computer, Inc.
 *
 */

/* at_paths.h --  Pathname Definitions for the AppleTalk Library and
   		  Commands
*/

#ifndef _AT_PATHS_H_
#define _AT_PATHS_H_

#define AT_DEF_ET_INTERFACE     "en0"
#define NVRAM			"/etc/appletalk.nvram"

#define AT_CFG_FILE		"/etc/appletalk.cfg"
#define MH_CFG_FILE		"/etc/appletalk.cfg"
				/* was /etc/appletalkmh.cfg */
     
#define AURP_CFGFILENAME	"/etc/aurp_tunnel.cfg"

#define DATA_DIR		"/etc/atalk"    
	/* this dir is defined in packaging, */

#define IFCONFIG_CMD		"/sbin/ifconfig"  
	/* It's "/etc/ifconfig" on many non-Rhapsody systems. */

#endif _AT_PATHS_H_
