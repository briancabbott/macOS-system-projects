/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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



#ifndef __PPP_MANAGER__
#define __PPP_MANAGER__


u_int16_t ppp_subtype(CFStringRef subtypeRef);

int ppp_new_service(struct service *serv);
int ppp_dispose_service(struct service *serv);
int ppp_setup_service(struct service *serv);

int ppp_start(struct service *serv, CFDictionaryRef options, uid_t uid, gid_t gid, mach_port_t bootstrap, mach_port_t au_session, u_int8_t onTraffic, u_int8_t onDemand);
int ppp_stop(struct service *serv, int signal);
int ppp_suspend(struct service *serv);
int ppp_resume(struct service *serv);
SCNetworkConnectionStatus ppp_getstatus(struct service *serv);
int ppp_getstatus1(struct service *serv, void **reply, u_int16_t *replylen);
int ppp_copyextendedstatus(struct service *serv, CFDictionaryRef *statusdict);
int ppp_copystatistics(struct service *serv, CFDictionaryRef *statsdict);
int ppp_getconnectdata(struct service *serv, CFDictionaryRef *options, int all);
int ppp_getconnectsystemdata(struct service *serv, void **reply, u_int16_t *replylen);

void ppp_updatephase(struct service *serv, int phase, int ifunit);
void ppp_updatestatus(struct service *serv, int status, int devstatus);
u_int32_t ppp_translate_error(u_int16_t subtype, u_int32_t native_ppp_error, u_int32_t native_dev_error);

int ppp_can_sleep(struct service *serv);
int ppp_will_sleep(struct service *serv, int checking);
void ppp_wake_up(struct service *serv);
void ppp_log_out(struct service *serv);
void ppp_log_in(struct service *serv);
void ppp_log_switch(struct service *serv);
void ppp_ipv4_state_changed(struct service *serv);
void ppp_user_notification_callback(struct service *serv, CFUserNotificationRef userNotification, CFOptionFlags responseFlags);
int ppp_ondemand_add_service_data(struct service *serv, CFMutableDictionaryRef ondemand_dict);
int ppp_is_pid(struct service *serv, int pid);

int ppp_install(struct service *serv);
int ppp_uninstall(struct service *serv);

#define OPT_LEN 4
#define MT_STR_LEN 256

typedef struct sessionDetails {
	/* Modem Option */
	char modem[MT_STR_LEN];					//Modem
	
	/* Hardware Info */
	char hardwareInfo[MT_STR_LEN];
	
	/* DNS Option */
	char manualDNS[OPT_LEN];
	
	/* Proxies Options */
	
	/* PPP Options */
	char dialOnDemand[OPT_LEN];				//Connect automatically when needed
	char idleReminder[OPT_LEN];				//Prompt every X minutes to maintain connection
	char disconnectOnLogout[OPT_LEN];		//Disconnect when user logs out
	char disconnectOnUserSwitch[OPT_LEN];	//Disconnect when switiching user accounts
	char authPrompt[MT_STR_LEN];			//Password prompt before/after dialling
	char redialEnabled[OPT_LEN];			//Redial X times if busy...
	char echoEnabled[OPT_LEN];				//Send PPP echo packet
	char verboseLogging[OPT_LEN];			//Use verbose logging
	char vjCompression[OPT_LEN];			//TCP Header compression
	char useTerminal[OPT_LEN];				//Connect using terminal window
	
	/* TCP/IP Options */
	char manualIPv4[OPT_LEN];
	char manualIPv6[OPT_LEN];
	
	/* WINS options */
	char winsEnabled[OPT_LEN];
	
	/* Proxies options */
	char proxiesEnabled[OPT_LEN];
} PPPSession_t;

void MT_pppGetTracerOptions(struct service *serv, PPPSession_t *pppSess);


#endif
