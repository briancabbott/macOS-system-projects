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

/* at_proto.h --  Prototype Definitions for the AppleTalk API

   See the "AppleTalk Programming Interface" document for a full
   explanation of these functions and their parameters.

   Required header files are as follows:

   #include <netat/appletalk.h>
*/

#ifndef _AT_PROTO_H_
#define _AT_PROTO_H_

#include <AvailabilityMacros.h>
#include <sys/types.h>

#ifdef  __cplusplus
extern "C" {
#endif

/*
 *	***** DEPRECATED *****
 *
 *	The AppleTalk Library APIs have been deprecated in Mac OS X 
 *	version 10.4. It is recommended that clients of this API migrate 
 *	to Rendezvous APIs or use standard TCP/IP libraries.
 *
 */

#define at_socket void
#define at_nvestr_t void
#define at_nbptuple_t void
#define at_inet_t void
#define at_entity_t void
#define at_retry_t void
#define at_nvestr_t void
#define at_atpreq void
#define at_resp_t void
#define at_retry_t void
#define ddp_addr_t void

struct pap_state;
struct at_addr;

/* Appletalk Stack status Function. */

enum {
	  RUNNING
	, NOTLOADED
	, LOADED
	, OTHERERROR
};

int checkATStack()
AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;

/* Datagram Delivery Protocol (DDP) Functions */

   /* The functions ddp_open(), ddp_close(), atproto_open() and 
      adspproto_open() have been replaced by standard BSD socket 
      functions, e.g. socket(AF_APPLETALK, SOCK_RAW, [ddp type]);

      See AppleTalk/nbp_send.c for an example of how these functions 
      are used,
   */

/* Routing Table Maintenance Protocol (RTMP) Function */

   /* The rtmp_netinfo() function has been replaced by the 
      AIOCGETIFCFG ioctl.

      See AppleTalk/nbp_send.c for an example of how this ioctl is used.
   */
     
/* AppleTalk Transaction Protocol (ATP) Functions */

int atp_open(at_socket *socket)
AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int atp_close(int fd)
AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int atp_sendreq(int fd,
		at_inet_t *dest,
		char *buf,
		int len, 
		int userdata, 
		int xo, 
		int xo_relt,
		u_short *tid,
		at_resp_t *resp,
		at_retry_t *retry,
		int nowait)
		AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int atp_getreq(int fd,
	       at_inet_t *src,
	       char *buf,
	       int *len, 
	       int *userdata, 
	       int *xo,
	       u_short *tid,
	       u_char *bitmap,
	       int nowait)
	       AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int atp_sendrsp(int fd,
		at_inet_t *dest,
		int xo,
		u_short tid,
		at_resp_t *resp)
		AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int atp_getresp(int fd,
		u_short *tid,
		at_resp_t *resp)
		AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int atp_look(int fd)
AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int atp_abort(int fd,
	      at_inet_t *dest,
	      u_short tid)
	      AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;

/* Name Binding Protocol (NBP) Functions */

int nbp_parse_entity(at_entity_t *entity,
		     char *str)
		     AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int nbp_make_entity(at_entity_t *entity, 
		    char *obj, 
		    char *type, 
		    char *zone)
		    AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int nbp_confirm(at_entity_t *entity,
		at_inet_t *dest,
		at_retry_t *retry)
		AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int nbp_lookup(at_entity_t *entity,
	       at_nbptuple_t *buf,
	       int max,
	       at_retry_t *retry)
	       AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int nbp_register(at_entity_t *entity, 
		 int fd, 
		 at_retry_t *retry)
		 AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int nbp_remove(at_entity_t *entity, 
	       int fd)
	       AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4; /* fd is not currently used */

int nbp_reg_lookup(at_entity_t *entity,
		   at_retry_t *retry)
		   AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
/*
  Used to make sure an NBP entity does not exist before it is registered.
  Returns 1 	if the entity already exists, 
  	  0 	if the entry does not exist
	 -1	for an error; e.g.no local zones exist
  Does the right thing in multihoming mode, namely if the zone is
  "*" (the default), it does the lookup in the default zone for
  each interface.

*/




/* Printer Access Protocol (PAP) Functions */

int pap_open(at_nbptuple_t *tuple)
AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int pap_open2(at_nbptuple_t *tuple, int num_tries, long wait_time)
AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int pap_read(int fd,
	     u_char *data,
	     int len)
	     AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int pap_read_ignore(int fd)
AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
char *pap_status(at_nbptuple_t *tuple)
AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int pap_write(int fd,
	      char *data,
	      int len,
	      int eof,
	      int flush)
	      AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int pap_close(int fd)
AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;

/* AppleTalk Data Stream Protocol (ADSP) Functions: */

int ADSPaccept(int fd, 
	       void *name, 
	       int *namelen)
	       AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int ADSPbind(int fd, 
	     void *name, 
	     int namelen)
	     AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int ADSPclose(int fd)
AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int ADSPconnect(int fd, 
		void *name, 
		int namelen)
		AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int ADSPfwdreset(int fd)
AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int ADSPgetpeername(int fd, 
		    void *name, 
		    int *namelen)
		    AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int ADSPgetsockname(int fd, 
		    void *name, 
		    int *namelen)
		    AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int ADSPgetsockopt(int fd, 
		   int level, 
		   int optname, 
		   char *optval, 
		   int *optlen)
		   AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int ADSPlisten(int fd, 
	       int backlog)
	       AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int ADSPrecv(int fd, 
	     char *buf, 
	     int len, 
	     int flags)
	     AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int ADSPsend(int fd,
	     char *buf,
	     int len,
	     int flags)
	     AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int ADSPsetsockopt(int fd,
		   int level,
		   int optname,
		   char *optval,
		   int optlen)
		   AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int ADSPsocket(int fd,
	       int type,
	       int protocol)
	       AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int ASYNCread(int fd,
	      char *buf,
	      int len)
	      AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int ASYNCread_complete(int fd,
		       char *buf,
		       int len)
		       AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;

/* AppleTalk Session Protocol (ASP) Functions */

int SPAttention(int SessRefNum,
		unsigned short AttentionCode,
		int *SPError,
		int NoWait)
		AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int SPCloseSession(int SessRefNum,
		   int *SPError)
		   AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int SPCmdReply(int SessRefNum,
	       unsigned short ReqRefNum,
	       int CmdResult,
	       char *CmdReplyData,
	       int CmdReplyDataSize,
	       int *SPError)
	       AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
void SPConfigure(unsigned short TickleInterval,
		 unsigned short SessionTimer,
		 at_retry_t *Retry)
		 AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
void SPGetParms(int *MaxCmdSize,
		int *QuantumSize,
		int SessRefNum)
		AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int SPGetProtoFamily(int SessRefNum,
		      int *ProtoFamily,
		      int *SPError)
		      AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int SPGetRemEntity(int SessRefNum,
		   void *SessRemEntityIdentifier,
		   int *SPError)
		   AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int SPGetReply(int SessRefNum,
	       char *ReplyBuffer,
	       int ReplyBufferSize,
	       int *CmdResult,
	       int *ActRcvdReplyLen,
	       int *SPError)
	       AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int SPGetRequest(int SessRefNum,
		 char *ReqBuffer,
		 int ReqBufferSize,
		 unsigned short *ReqRefNum,
		 int *ReqType,
		 int *ActRcvdReqLen,
		 int *SPError)
		 AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int SPGetSession(int SLSRefNum,
		 int *SessRefNum,
		 int *SPError)
		 AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int SPInit(at_inet_t *SLSEntityIdentifier,
	   char *ServiceStatusBlock,
	   int ServiceStatusBlockSize,
	   int *SLSRefNum,
	   int *SPError)
	   AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int SPLook(int SessRefNum,
	   int *SPError)
	   AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int SPNewStatus(int SLSRefNum,
		char *ServiceStatusBlock,
		int ServiceStatusBlockSize,
		int *SPError)
		AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int SPRegister(at_entity_t *SLSEntity,
	       at_retry_t *Retry,
	       at_inet_t *SLSEntityIdentifier,
	       int *SPError)
	       AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
/*
 * the following API is added to fix bug 2285307;  It replaces SPRegister 
 * which now only behaves as asp over appletalk.
 */
int SPRegisterWithTCPPossiblity(at_entity_t *SLSEntity,
           at_retry_t *Retry,
           at_inet_t *SLSEntityIdentifier,
           int *SPError)
           AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int SPRemove(at_entity_t *SLSEntity,
	     at_inet_t *SLSEntityIdentifier,
	     int *SPError)
	     AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
/* *** Why do we need to be able to set the pid from the ASP API? *** */
int SPSetPid(int SessRefNum,
	     int SessPid,
	     int *SPError)
	     AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int SPWrtContinue(int SessRefNum,
		  unsigned short ReqRefNum,
		  char *Buff,
		  int BuffSize,
		  int *ActLenRcvd,
		  int *SPError,
		  int NoWait)
		  AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
int SPWrtReply(int SessRefNum,
	       unsigned short ReqRefNum,
	       int CmdResult,
	       char *CmdReplyData,
	       int CmdReplyDataSize,
	       int *SPError)
	       AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;

/* Zone Information Protocol (ZIP) Functions */

#define ZIP_FIRST_ZONE 1
#define ZIP_NO_MORE_ZONES 0
#define ZIP_DEF_INTERFACE NULL

/* zip_getzonelist() will return 0 on success, and -1 on failure. */

int zip_getmyzone(
	char *ifName,
		/* If ifName is a null pointer (ZIP_DEF_INTERFACE) the default
		   interface will be used.
		*/
	at_nvestr_t *zone
)
AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;

/* zip_getzonelist() will return the zone count on success, 
   and -1 on failure. */

int zip_getzonelist(
	char *ifName,
		/* If ifName is a null pointer (ZIP_DEF_INTERFACE) the default
		   interface will be used.
		*/
	int *context,
		/* *context should be set to ZIP_FIRST_ZONE for the first call.
		   The returned value may be used in the next call, unless it
		   is equal to ZIP_NO_MORE_ZONES.
		*/
	u_char *zones,
		/* Pointer to the beginning of the "zones" buffer.
		   Zone data returned will be a sequence of at_nvestr_t
		   Pascal-style strings, as it comes back from the 
		   ZIP_GETZONELIST request sent over ATP 
		*/
	int size
		/* Length of the "zones" buffer; must be at least 
		   (ATP_DATA_SIZE+1) bytes in length.
		*/
)
AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;

/* zip_getlocalzones() will return the zone count on success, 
   and -1 on failure. */

int zip_getlocalzones(
	char *ifName,
		/* If ifName is a null pointer (ZIP_DEF_INTERFACE) the default
		   interface will be used.
		*/
	int *context,
		/* *context should be set to ZIP_FIRST_ZONE for the first call.
		   The returned value may be used in the next call, unless it
		   is equal to ZIP_NO_MORE_ZONES.
		*/
	u_char *zones,
		/* Pointer to the beginning of the "zones" buffer.
		   Zone data returned will be a sequence of at_nvestr_t
		   Pascal-style strings, as it comes back from the 
		   ZIP_GETLOCALZONES request sent over ATP 
		*/
	int size
		/* Length of the "zones" buffer; must be at least 
		   (ATP_DATA_SIZE+1) bytes in length.
		*/
)
AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;

/* These functions are used to read/write defaultss in persistent storage,
   for now /etc/appletalk.nvram.[interface name, e.g. en0].
*/

/* at_getdefaultzone() returns
      0 on success
     -1 on error
*/
int at_getdefaultzone(
     char *ifName,
          /* ifName must not be a null pointer; a pointer to a valid 
	     interface name must be supplied. */
     at_nvestr_t *zone
          /* The return value for the default zone, from persistent 
             storage */
)
AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;

/* at_setdefaultzone() returns
      0 on success
     -1 on error
*/
int at_setdefaultzone(
     char *ifName,
          /* ifName must not be a null pointer; a pointer to a valid 
	     interface name must be supplied. */
     at_nvestr_t *zone
          /* The value of the default zone, to be set in persistent 
             storage */
)
AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;

/* at_getdefaultaddr() returns
      0 on success
     -1 on error
*/
int at_getdefaultaddr(
     char *ifName,
          /* ifName must not be a null pointer; a pointer to a valid 
	     interface name must be supplied. */
     struct at_addr *init_address
          /* The return value for the address hint, from persistent 
             storage */
)
AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;

/* at_setdefaultaddr() returns
      0 on success
     -1 on error
*/
int at_setdefaultaddr(
     char *ifName,
          /* ifName must not be a null pointer; a pointer to a valid 
	     interface name must be supplied. */
     struct at_addr *init_address
          /* The return value for the address hint, from persistent 
             storage */
)
AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;

/* Save the current configuration in persistent storage.

   at_savecfgdefaults() returns
      0 on success
     -1 on error
*/
int at_savecfgdefaults(
     int fd,
	/* An AppleTalk socket, if one is open, otherwise 0 */  
     char *ifName
	/* If ifName is a null pointer the name of the default
	   interface will be used. */
)
AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;

/* 
   ***
   The following routines are scheduled be replaced/discontinued soon:
   ***
*/

int at_send_to_dev(int fd, int cmd, char *dp, int *length)
AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
	/* Used to send an old-style (pre-BSD) IOC to the AppleTalk stack. */

int ddp_config(int fd, ddp_addr_t *addr)
AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4;
	/* Used to provide functionality similar to BSD getsockname().
	   Will be replaced with a sockopt as soon as the ATP and the ADSP
	   protocols have been socketized */
	   
#ifdef  __cplusplus
}
#endif

#endif _AT_PROTO_H_
