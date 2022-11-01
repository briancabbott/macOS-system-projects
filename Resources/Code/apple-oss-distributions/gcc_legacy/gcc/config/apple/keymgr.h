/* Copyright (C) 1989, 92-97, 1998, Free Software Foundation, Inc.

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */


/*
 * This file added by Apple Computer Inc. for its OS X 
 * environment.
 */

#ifndef __KEYMGR_H
#define __KEYMGR_H

#ifdef MACOSX

#ifdef __cplusplus
extern "C" {
#endif



/*
 * keymgr - Create and maintain process-wide global data known to 
 *	    all threads across all dynamic libraries. 
 *
 */
 
typedef enum node_kinds {
	NODE_THREAD_SPECIFIC_DATA=1,
	NODE_PROCESSWIDE_PTR=2,
	NODE_LAST_KIND
	} TnodeKind ;
	
/*
 * These enum members are bits or combination of bits.
 */
	
typedef enum node_mode {
	NM_ALLOW_RECURSION=1,
	NM_RECURSION_ILLEGAL=2,
	NM_ENHANCED_LOCKING=3,
	NM_LOCKED=4
	} TnodeMode ;



extern void * _keymgr_get_per_thread_data(unsigned int key) ;
extern void _keymgr_set_per_thread_data(unsigned int key, void *keydata) ;
extern void *_keymgr_get_and_lock_processwide_ptr(unsigned int key) ;
extern void _keymgr_set_and_unlock_processwide_ptr(unsigned int key, void *ptr) ;
extern void _keymgr_unlock_processwide_ptr(unsigned int key) ;
extern void _keymgr_set_lockmode_processwide_ptr(unsigned int key, unsigned int mode) ;
extern unsigned int  _keymgr_get_lockmode_processwide_ptr(unsigned int key) ;
extern int _keymgr_get_lock_count_processwide_ptr(unsigned int key) ;

#ifndef NULL
#define NULL (0)
#endif

/*
 * Keys currently in use:
 */

#define KEYMGR_EH_CONTEXT_KEY		1	/*stores handle for root pointer of exception context node.*/

#define KEYMGR_NEW_HANLDER_KEY		2	/*store handle for new handler pointer.*/

#define KEYMGR_UNEXPECTED_HANDLER_KEY	3	/*store handle for unexpected exception pointer.*/

#define KEYMGR_TERMINATE_HANDLER_KEY	4	/*store handle for terminate handler pointer. */

#define KEYMGR_MODE_BITS		5	/*stores handle for runtime mode bits.*/

#define KEYMGR_IO_LIST			6	/*Root pointer to the list of open streams.*/

#define KEYMGR_IO_STDIN			7	/*GNU stdin.*/

#define KEYMGR_IO_STDOUT		8	/*GNU stdout.*/

#define KEYMGR_IO_STDERR		9	/*GNU stderr.*/

#define KEYMGR_IO_REFCNT		10	/*How many plugins/main program currently using streams.*/

#define KEYMGR_IO_MODE_BITS		11	/*Flags controlling the behavior of C++ I/O.*/

#define KEYMGR_ZOE_IMAGE_LIST		12	/*Head pointer for list of per image dwarf2 unwind sections.*/

/* KeyMgr 3.x is the first one supporting GCC3 stuff natively.  */
#define KEYMGR_API_MAJOR_GCC3		3
/* ... with these keys.  */
#define KEYMGR_GCC3_LIVE_IMAGE_LIST	301	/* loaded images  */
#define KEYMGR_GCC3_DW2_OBJ_LIST	302	/* Dwarf2 object list  */

/*
 * Other important data.
 */
 
#define KEYMGR_API_REV_MAJOR		3	/*Major revision number of the keymgr API.*/
#define KEYMGR_API_REV_MINOR		0	/*Minor revision number of the keymgr API.*/



#ifdef __cplusplus
}
#endif

#endif /* MACOSX */

#endif /* __KEYMGR_H */
