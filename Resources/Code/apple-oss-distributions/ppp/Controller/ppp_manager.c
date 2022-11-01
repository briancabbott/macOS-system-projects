/*
 * Copyright (c) 2000, 2014, 2018 Apple Inc. All rights reserved.
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


/* -----------------------------------------------------------------------------
includes
----------------------------------------------------------------------------- */

#include <fcntl.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <paths.h>
#include <net/if.h>
#include <net/ndrv.h>
#include <mach/message.h>
#include <mach/boolean.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <CoreFoundation/CFMachPort.h>
#include <mach/mach_time.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFUserNotification.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCDPlugin.h>
#include <SystemConfiguration/SCPrivate.h>      // for SCLog() and VPN private keys
#include <SystemConfiguration/SCValidation.h>
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/IOBSD.h>
#include <pwd.h>

#include "ppp_msg.h"
#include "ppp_privmsg.h"
#include "../Family/ppp_domain.h"
#include "../Helpers/pppd/pppd.h"

#include "scnc_main.h"
#include "scnc_client.h"
#include "ppp_manager.h"
#include "ppp_option.h"
#include "ppp_socket_server.h"
#include "scnc_utils.h"
#include "controller_options.h"
#include "ne_sm_bridge_private.h"

#include "../Drivers/L2TP/L2TP-plugin/l2tp.h"
#include "../Drivers/PPPoE/PPPoE-extension/PPPoE.h"

/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */

enum {
	READ	= 0,	// read end of standard UNIX pipe
	WRITE	= 1	// write end of standard UNIX pipe
};

#define MAX_EXTRACONNECTTIME 20 /* allows 20 seconds after wakeup */
#define MIN_EXTRACONNECTTIME 3 /* if we do give extra time, give at lease 3 seconds */

/* -----------------------------------------------------------------------------
globals
----------------------------------------------------------------------------- */

extern TAILQ_HEAD(, service) 	service_head;

static char *empty_str_s = "";
static u_char *empty_str = (u_char*)"";

/* -----------------------------------------------------------------------------
Forward declarations
----------------------------------------------------------------------------- */

static void display_error(struct service *serv);
static void exec_callback(pid_t pid, int status, struct rusage *rusage, void *context);
static void exec_postfork(pid_t pid, void *arg);
static int send_pppd_params(struct service *serv, CFDictionaryRef service, CFDictionaryRef options, u_int8_t onTraffic);
static int change_pppd_params(struct service *serv, CFDictionaryRef service, CFDictionaryRef options);

static void setup_PPPoE(struct service *serv);
static void dispose_PPPoE(struct service *serv);
static void MT_checkForProxies(const void *, const void *, PPPSession_t *);
static void MT_getModemInformation(struct service *, SCPreferencesRef, PPPSession_t *);

/* -----------------------------------------------------------------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
    service management and control
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
----------------------------------------------------------------------------- */

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_int16_t ppp_subtype(CFStringRef subtypeRef) 
{

   if (my_CFEqual(subtypeRef, kSCValNetInterfaceSubTypePPPSerial)) 
        return PPP_TYPE_SERIAL;
    else if (my_CFEqual(subtypeRef, kSCValNetInterfaceSubTypePPPoE))
        return PPP_TYPE_PPPoE;
    else if (my_CFEqual(subtypeRef, kSCValNetInterfaceSubTypeL2TP))
        return PPP_TYPE_L2TP;

	return PPP_TYPE_OTHER;
}

/* -----------------------------------------------------------------------------
an interface structure needs to be created
unit is the ppp managed unit
----------------------------------------------------------------------------- */
int ppp_new_service(struct service *serv)
{
    CFURLRef		url;
    u_char			str[MAXPATHLEN], str2[32];

   //  scnc_log(LOG_INFO, CFSTR("ppp_new_service, subtype = %%@, serviceID = %@."), serv->subtypeRef, serv->serviceID);

    serv->u.ppp.ndrv_socket = -1;
    serv->u.ppp.phase = PPP_IDLE;
    serv->u.ppp.statusfd[READ] = -1;
    serv->u.ppp.statusfd[WRITE] = -1;
    serv->u.ppp.controlfd[READ] = -1;
    serv->u.ppp.controlfd[WRITE] = -1;
    serv->u.ppp.pid = -1;

	if (serv->subtypeRef) {
		strlcpy((char*)str, DIR_KEXT, sizeof(str));
		str2[0] = 0;
		CFStringGetCString(serv->subtypeRef, (char*)str2, sizeof(str2), kCFStringEncodingUTF8);
		strlcat((char*)str, (char*)str2, sizeof(str));
		strlcat((char*)str, ".ppp", sizeof(str));	// add plugin suffix
		url = CFURLCreateFromFileSystemRepresentation(NULL, str, strlen((char*)str), TRUE);
		if (url) {
			my_CFRelease(&serv->u.ppp.bundleRef);
			serv->u.ppp.bundleRef = CFBundleCreate(0, url);
			CFRelease(url);
		}
	}
	
    return 0;
}

/* -----------------------------------------------------------------------------
an interface is come down, dispose the ppp structure
----------------------------------------------------------------------------- */
int ppp_dispose_service(struct service *serv)
{

    if (serv->u.ppp.phase != PPP_IDLE)
        return 1;
    
    // then free the structure
	dispose_PPPoE(serv);
    my_CFRelease(&serv->connection_nid);
    my_CFRelease(&serv->connection_nap);
	my_CFRelease(&serv->systemprefs);		
    return 0;
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
void ppp_user_notification_callback(struct service *serv, CFUserNotificationRef userNotification, CFOptionFlags responseFlags)
{
	
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
static
void display_error(struct service *serv) 
{
    CFStringRef 	msg = NULL;
    CFMutableDictionaryRef 	dict = NULL;
    SInt32 			err;

    serv->connecttime = 0;
    serv->establishtime = 0;

    STOP_TRACKING_VPN_LOCATION(serv);

    if (serv->u.ppp.laststatus == EXIT_USER_REQUEST)
        return;
	
	if (serv->flags & FLAG_ONDEMAND)
		return;

    if ((serv->flags & FLAG_ALERTERRORS) == 0)
        return;
	
#if TARGET_OS_OSX
    if (serv->flags & FLAG_DARKWAKE)
        return;
#endif
	/* <rdar://6701793>, we do not want to display error if the device is gone and ppp ONTRAFFIC mode is enabled */
    if ((serv->flags & FLAG_ONTRAFFIC) && (serv->u.ppp.laststatus == EXIT_DEVICE_ERROR))
        return;

    if (serv->u.ppp.lastdevstatus) {
		
		switch (serv->subtype) {
			case PPP_TYPE_L2TP:
				// filter out the following messages
				switch (serv->u.ppp.lastdevstatus) {
						/* Error 6 */
					case EXIT_L2TP_NETWORKCHANGED: /* L2TP Error 6 */
						return;
				}
				break;
		}
		
		msg = CFStringCreateWithFormat(0, 0, CFSTR("%@ Error %d"), serv->subtypeRef, serv->u.ppp.lastdevstatus);
    }
	
    if (msg == NULL) {
		
		// filter out the following messages
		switch (serv->u.ppp.laststatus) {
			case EXIT_USER_REQUEST: /* PPP Error 5 */
				return;
		}

		msg = CFStringCreateWithFormat(0, 0, CFSTR("PPP Error %d"), serv->u.ppp.laststatus);
	}
	
    if (!msg || !CFStringGetLength(msg))
		goto done;
		
    dict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!dict)
		goto done;
	
	if (gIconURLRef)
		CFDictionaryAddValue(dict, kCFUserNotificationIconURLKey, gIconURLRef);
	if (gBundleURLRef)
		CFDictionaryAddValue(dict, kCFUserNotificationLocalizationURLKey, gBundleURLRef);
	
	CFDictionaryAddValue(dict, kCFUserNotificationAlertMessageKey, msg);
	CFDictionaryAddValue(dict, kCFUserNotificationAlertHeaderKey, (serv->subtype == PPP_TYPE_L2TP) ? CFSTR("VPN Connection") : CFSTR("Network Connection"));
	
	if (serv->userNotificationRef) {
		err = CFUserNotificationUpdate(serv->userNotificationRef, 0, kCFUserNotificationNoteAlertLevel, dict);
	}
	else {
		serv->userNotificationRef = CFUserNotificationCreate(NULL, 0, kCFUserNotificationNoteAlertLevel, &err, dict);
		if (!serv->userNotificationRef)
			goto done;
		
		serv->userNotificationRLS = CFUserNotificationCreateRunLoopSource(NULL, serv->userNotificationRef, 
																		  user_notification_callback, 0);
		if (!serv->userNotificationRLS) {
			my_CFRelease(&serv->userNotificationRef);
			goto done;
		}
		CFRunLoopAddSource(CFRunLoopGetCurrent(), serv->userNotificationRLS, kCFRunLoopDefaultMode);
	}

	
done:
    my_CFRelease(&dict);
    my_CFRelease(&msg);
}

/* -----------------------------------------------------------------------------
	for PPPoE over Airport or Ethernet services, install a protocol to enable the interface
	even when PPPoE is not connected
----------------------------------------------------------------------------- */
static 
void setup_PPPoE(struct service *serv)
{
    CFDictionaryRef	interface;
	CFStringRef		hardware, device;
    struct sockaddr_ndrv 	ndrv;

	if (serv->subtype == PPP_TYPE_PPPoE) {

		interface = copyEntity(gDynamicStore, kSCDynamicStoreDomainSetup, serv->serviceID, kSCEntNetInterface);
		if (interface) {
		
			device = CFDictionaryGetValue(interface, kSCPropNetInterfaceDeviceName);
			hardware = CFDictionaryGetValue(interface, kSCPropNetInterfaceHardware);

			if (isA_CFString(hardware) && isA_CFString(device) 
				&& ((CFStringCompare(hardware, kSCEntNetAirPort, 0) == kCFCompareEqualTo)
					|| (CFStringCompare(hardware, kSCEntNetEthernet, 0) == kCFCompareEqualTo))) {

				if (serv->device 
					&& (CFStringCompare(device, serv->device, 0) != kCFCompareEqualTo)) {
					dispose_PPPoE(serv);
				}
				
				if (!serv->device) {
					serv->u.ppp.ndrv_socket = socket(PF_NDRV, SOCK_RAW, 0);
					if (serv->u.ppp.ndrv_socket >= 0) {
						serv->device = device;
						CFRetain(serv->device);
						CFStringGetCString(device, (char*)ndrv.snd_name, sizeof(ndrv.snd_name), kCFStringEncodingMacRoman);
						ndrv.snd_len = sizeof(ndrv);
						ndrv.snd_family = AF_NDRV;
						if (bind(serv->u.ppp.ndrv_socket, (struct sockaddr *)&ndrv, sizeof(ndrv)) < 0) {
							dispose_PPPoE(serv);
						}
					}
				}
			}
			else {
				/* not an Airport device */
				dispose_PPPoE(serv);
			}

			CFRelease(interface);
		}
	}
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static 
void dispose_PPPoE(struct service *serv)
{
	if (serv->u.ppp.ndrv_socket != -1) {
		close(serv->u.ppp.ndrv_socket);
		serv->u.ppp.ndrv_socket = -1;
	}
	if (serv->device) {
		CFRelease(serv->device);
		serv->device = 0;
	}
}

/* -----------------------------------------------------------------------------
changed for this ppp occured in configd cache
----------------------------------------------------------------------------- */
int ppp_setup_service(struct service *serv)
{
    u_int32_t 		lval;
	CFDictionaryRef serviceConfig = NULL;
    
	/* get some general setting flags first */
	serv->flags &= ~(
		FLAG_SETUP_ONTRAFFIC +
		FLAG_SETUP_DISCONNECTONLOGOUT +
		FLAG_SETUP_DISCONNECTONSLEEP +
		FLAG_SETUP_PREVENTIDLESLEEP +
		FLAG_SETUP_DISCONNECTONFASTUSERSWITCH +
		FLAG_SETUP_ONDEMAND +
		FLAG_DARKWAKE + 
		FLAG_SETUP_PERSISTCONNECTION +
		FLAG_SETUP_DISCONNECTONWAKE);

	my_CFRelease(&serv->systemprefs);
	if (serv->ne_sm_bridge != NULL) {
		serviceConfig = ne_sm_bridge_copy_configuration(serv->ne_sm_bridge);
		if (serviceConfig != NULL) {
			CFDictionaryRef pppDict = CFDictionaryGetValue(serviceConfig, kSCEntNetPPP);
			if (pppDict != NULL) {
				serv->systemprefs = CFRetain(pppDict);
			}
		}
	} else {
		serv->systemprefs = copyEntity(gDynamicStore, kSCDynamicStoreDomainSetup, serv->serviceID, kSCEntNetPPP);
	}
	if (serv->systemprefs) {
		lval = 0;
		getNumber(serv->systemprefs, kSCPropNetPPPDialOnDemand, &lval);
		if (lval) serv->flags |= FLAG_SETUP_ONTRAFFIC;

		lval = 0;
		getNumber(serv->systemprefs, kSCPropNetPPPDisconnectOnLogout, &lval);
		if (lval) serv->flags |= FLAG_SETUP_DISCONNECTONLOGOUT;

		// by default, vpn connection don't disconnect on sleep
		switch (serv->subtype) {
			case PPP_TYPE_L2TP:
				lval = 0;
				break;
			default :
				lval = 1;
		}
		getNumber(serv->systemprefs, kSCPropNetPPPDisconnectOnSleep, &lval);
		if (lval) serv->flags |= FLAG_SETUP_DISCONNECTONSLEEP;
			
		/* Currently, OnDemand imples network detection */
		lval = 0;
		getNumber(serv->systemprefs, kSCPropNetPPPOnDemandEnabled, &lval);
		if (lval)
			serv->flags |= (FLAG_SETUP_ONDEMAND | FLAG_SETUP_NETWORKDETECTION);
		else if (CFDictionaryGetValue(serv->systemprefs, kSCPropNetVPNOnDemandRules)) {
			if (controller_options_is_useVODDisconnectRulesWhenVODDisabled())
				serv->flags |= FLAG_SETUP_NETWORKDETECTION;
		}
		
		// by default, vpn connection don't prevent idle sleep
		switch (serv->subtype) {
			case PPP_TYPE_L2TP:
				lval = 0;
				break;
			default :
				lval = 1;
		}
		getNumber(serv->systemprefs, CFSTR("PreventIdleSleep"), &lval);
		if (lval) serv->flags |= FLAG_SETUP_PREVENTIDLESLEEP;

		/* if the DisconnectOnFastUserSwitch key does not exist, use kSCPropNetPPPDisconnectOnLogout */
		lval = (serv->flags & FLAG_SETUP_DISCONNECTONLOGOUT);
		getNumber(serv->systemprefs, CFSTR("DisconnectOnFastUserSwitch"), &lval);
		if (lval) serv->flags |= FLAG_SETUP_DISCONNECTONFASTUSERSWITCH;

		/* "disconnect on wake" is enabled by default for PPP */
		lval = 1;
		serv->sleepwaketimeout = 0;
		getNumber(serv->systemprefs, kSCPropNetPPPDisconnectOnWake, &lval);
		if (lval) {
			serv->flags |= FLAG_SETUP_DISCONNECTONWAKE;
			getNumber(serv->systemprefs, kSCPropNetPPPDisconnectOnWakeTimer, &serv->sleepwaketimeout);
		}

		/* enable "ConnectionPersist" */
		lval = 0;
		getNumber(serv->systemprefs, CFSTR("ConnectionPersist"), &lval);
		if (lval) serv->flags |= FLAG_SETUP_PERSISTCONNECTION;

	}

	setup_PPPoE(serv);

	switch (serv->u.ppp.phase) {

		case PPP_IDLE:
			//printf("ppp_updatesetup : unit %d, PPP_IDLE\n", ppp->unit);
			if (serv->flags & FLAG_SETUP_ONTRAFFIC
				 && (!(serv->flags & FLAG_SETUP_DISCONNECTONLOGOUT) || gLoggedInUser)) {
					ppp_start(serv, 0, 0, 0, serv->bootstrap, serv->au_session, 1, 0);
			}
			break;

		case PPP_DORMANT:
		case PPP_HOLDOFF:
			// config has changed, dialontraffic will need to be restarted
			serv->flags |= FLAG_CONFIGCHANGEDNOW;
			serv->flags &= ~FLAG_CONFIGCHANGEDLATER;
			scnc_stop(serv, 0, SIGTERM, SCNC_STOP_NONE);
			break;

		default :
			// config has changed, dialontraffic will need to be restarted
			serv->flags |= FLAG_CONFIGCHANGEDLATER;
			serv->flags &= ~FLAG_CONFIGCHANGEDNOW;
			/* if ppp was started in dialontraffic mode, then stop it */
//                    if (ppp->dialontraffic)
//                        ppp_disconnect(ppp, 0, SIGTERM);

			if (serviceConfig == NULL) {
				serviceConfig = copyService(gDynamicStore, kSCDynamicStoreDomainSetup, serv->serviceID);
			}
			if (serviceConfig) {
				change_pppd_params(serv, serviceConfig, serv->connectopts);
			}
			break;
	}

	my_CFRelease(&serviceConfig);
	return 0;
}

/* -----------------------------------------------------------------------------
system is asking permission to sleep
return if sleep is authorized
----------------------------------------------------------------------------- */
int ppp_can_sleep(struct service *serv)
{
    // I refuse idle sleep if ppp is connected
	if (serv->u.ppp.phase == PPP_RUNNING
		&& (serv->flags & FLAG_SETUP_PREVENTIDLESLEEP))
		return 0;
	
    return 1;
}

/* -----------------------------------------------------------------------------
system is going to sleep
disconnect services and return if a delay is needed
----------------------------------------------------------------------------- */ 
int ppp_will_sleep(struct service *serv, int checking)
{
    u_int32_t			delay = 0, alert = 0;
            
	if (serv->u.ppp.phase != PPP_IDLE
		&& (serv->flags & FLAG_SETUP_DISCONNECTONSLEEP)) { 
		
		delay = 1;
		if (serv->u.ppp.phase != PPP_DORMANT && serv->u.ppp.phase != PPP_HOLDOFF)
			alert = 2;
		if (!checking)
			scnc_stop(serv, 0, SIGTERM, SCNC_STOP_SYS_SLEEP);
	}
        
    return delay + alert;
}

/* -----------------------------------------------------------------------------
ipv4 state has changed 
----------------------------------------------------------------------------- */
void ppp_ipv4_state_changed(struct service *serv)
{
}

/* -----------------------------------------------------------------------------
system is waking up
need to check the dialontraffic flag again
----------------------------------------------------------------------------- */
void ppp_wake_up(struct service	*serv)
{

	if (serv->u.ppp.phase == PPP_IDLE) {
		if ((serv->flags & FLAG_SETUP_ONTRAFFIC)
				&& (!(serv->flags & FLAG_SETUP_DISCONNECTONLOGOUT) || gLoggedInUser)) {
				ppp_start(serv, 0, 0, 0, serv->bootstrap, serv->au_session, 1, 0);
		}
	} else {
		if (DISCONNECT_VPN_IFOVERSLEPT(__FUNCTION__, serv, serv->if_name)) {
			return;
		} else if (DISCONNECT_VPN_IFLOCATIONCHANGED(serv)) {
			return;
		}
	}
}

/* -----------------------------------------------------------------------------
user has looged out
need to check the disconnect on logout flag for the ppp interfaces
----------------------------------------------------------------------------- */
void ppp_log_out(struct service	*serv)
{

	if (serv->u.ppp.phase != PPP_IDLE
		&& (serv->flags & FLAG_SETUP_DISCONNECTONLOGOUT))
		scnc_stop(serv, 0, SIGTERM, SCNC_STOP_USER_LOGOUT);
}

/* -----------------------------------------------------------------------------
user has logged in
need to check the dialontraffic flag again
----------------------------------------------------------------------------- */
void ppp_log_in(struct service	*serv)
{

	if (serv->u.ppp.phase == PPP_IDLE
		&& (serv->flags & FLAG_SETUP_ONTRAFFIC))
		ppp_start(serv, 0, 0, 0, serv->bootstrap, serv->au_session, 1, 0);
}

/* -----------------------------------------------------------------------------
user has switched
need to check the disconnect on logout and dial on traffic 
flags for the ppp interfaces
----------------------------------------------------------------------------- */
void ppp_log_switch(struct service *serv)
{
	switch (serv->u.ppp.phase) {
		case PPP_IDLE:
			// rearm dial on demand
			if (serv->flags & FLAG_SETUP_ONTRAFFIC)
				ppp_start(serv, 0, 0, 0, serv->bootstrap, serv->au_session, 1, 0);
			break;
			
		default:
			if (serv->flags & FLAG_SETUP_DISCONNECTONFASTUSERSWITCH) {
					
				// if dialontraffic is set, it will need to be restarted
				serv->flags &= ~FLAG_CONFIGCHANGEDLATER;
				if (serv->flags & FLAG_SETUP_ONTRAFFIC)
					serv->flags |= FLAG_CONFIGCHANGEDNOW;
				else
					serv->flags &= ~FLAG_CONFIGCHANGEDNOW;
				scnc_stop(serv, 0, SIGTERM, SCNC_STOP_USER_SWITCH);
			}
	}
}

/* ----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
int ppp_ondemand_add_service_data(struct service *serv, CFMutableDictionaryRef ondemand_dict)
{
	CFArrayRef			array;
	CFStringRef			string;
	
	if (serv->systemprefs == NULL)
		return 0;

#ifndef kSCPropNetPPPOnDemandMatchDomainsAlways
#define	kSCPropNetPPPOnDemandMatchDomainsAlways kSCPropNetVPNOnDemandMatchDomainsAlways
#endif
#ifndef kSCPropNetPPPOnDemandMatchDomainsNever
#define	kSCPropNetPPPOnDemandMatchDomainsNever kSCPropNetVPNOnDemandMatchDomainsNever
#endif
#ifndef kSCPropNetPPPOnDemandMatchDomainsOnRetry
#define	kSCPropNetPPPOnDemandMatchDomainsOnRetry kSCPropNetVPNOnDemandMatchDomainsOnRetry
#endif
	
	array = CFDictionaryGetValue(serv->systemprefs, kSCPropNetPPPOnDemandMatchDomainsAlways);
	if (isArray(array))
		CFDictionarySetValue(ondemand_dict, kSCNetworkConnectionOnDemandMatchDomainsAlways, array);
	array = CFDictionaryGetValue(serv->systemprefs, kSCPropNetPPPOnDemandMatchDomainsOnRetry);
	if (isArray(array))
		CFDictionarySetValue(ondemand_dict, kSCNetworkConnectionOnDemandMatchDomainsOnRetry, array);
	array = CFDictionaryGetValue(serv->systemprefs, kSCPropNetPPPOnDemandMatchDomainsNever);
	if (isArray(array))
		CFDictionarySetValue(ondemand_dict, kSCNetworkConnectionOnDemandMatchDomainsNever, array);
	
	string = CFDictionaryGetValue(serv->systemprefs, kSCPropNetPPPCommRemoteAddress);
	if (isString(string))
		CFDictionarySetValue(ondemand_dict, kSCNetworkConnectionOnDemandRemoteAddress, string);

	return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static 
void addparam(char **arg, u_int32_t *argi, char *param)
{
    unsigned long len = strlen(param);

    if (len && (arg[*argi] = malloc(len + 1))) {
        strlcpy(arg[*argi], param, (len + 1));
        (*argi)++;
    }
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static 
void writeparam(int fd, char *param)
{
    
    write(fd, param, strlen(param));
    write(fd, " ", 1);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static 
void writeintparam(int fd, char *param, u_int32_t val)
{
    u_char	str[32];
    
    writeparam(fd, param);
    snprintf((char*)str, sizeof(str), "%d", val);
    writeparam(fd, (char*)str);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static 
void writedataparam(int fd, char *param, void *data, int len)
{
    
    writeintparam(fd, param, len);
    write(fd, data, len);
    write(fd, " ", 1);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static 
void writestrparam(int fd, char *param, char *val)
{
    
    write(fd, param, strlen(param));

    /* we need to quote and escape the parameter */
    write(fd, " \"", 2);
    while (*val) {
        switch (*val) {
            case '\\':
            case '\"':
				write(fd, "\\", 1);
				break;
            case '\'':
            case ' ':
                //write(fd, "\\", 1);
                ;
        }
        write(fd, val, 1);
        val++;
    }
    write(fd, "\" ", 2);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */

static 
int send_pppd_params(struct service *serv, CFDictionaryRef service, CFDictionaryRef options, u_int8_t onTraffic)
{
    char 			str[MAXPATHLEN], str2[256];
    int 			needpasswd = 0, tokendone = 0, auth_default = 1, from_service, optfd, awaketime, overrideprimary = 0;
    u_int32_t			auth_bits = 0xF; /* PAP + CHAP + MSCHAP1 + MPCHAP2 */
    u_int32_t			len, lval, lval1, i;
    u_char 			sopt[OPT_STR_LEN];
    CFDictionaryRef		pppdict = NULL, dict, modemdict;
    CFArrayRef			array = NULL;
    CFStringRef			string = NULL;
    CFDataRef			dataref = 0;
    void			*dataptr = 0;
    u_int32_t			datalen = 0;
	u_int32_t           ccp_enabled = 0;

    pppdict = CFDictionaryGetValue(service, kSCEntNetPPP);
    if ((pppdict == 0) || (CFGetTypeID(pppdict) != CFDictionaryGetTypeID()))
        return -1; 	// that's bad...
    
    optfd = serv->u.ppp.controlfd[WRITE];

    writeparam(optfd, "[OPTIONS]");

    // -----------------
    // add the dialog plugin
    if (gPluginsDir) {
        CFStringGetCString(gPluginsDir, str, sizeof(str), kCFStringEncodingUTF8);
        strlcat(str, "PPPDialogs.ppp", sizeof(str));
        writestrparam(optfd, "plugin", str);
		if (serv->subtype == PPP_TYPE_L2TP) {
			writeintparam(optfd, "dialogtype", 1);
		}
	}
	
    // -----------------
    // verbose logging 
    get_int_option(serv, kSCEntNetPPP, kSCPropNetPPPVerboseLogging, options, service, &lval, 0);
    if (lval)
        writeparam(optfd, "debug");

    // -----------------
    // alert flags 
    serv->flags &= ~(FLAG_ALERTERRORS + FLAG_ALERTPASSWORDS);
    ppp_getoptval(serv, options, service, PPP_OPT_ALERTENABLE, &lval, sizeof(lval), &len);
    if (lval & PPP_ALERT_ERRORS)
        serv->flags |= FLAG_ALERTERRORS;
    if (lval & PPP_ALERT_PASSWORDS)
        serv->flags |= FLAG_ALERTPASSWORDS;
                
    if (ppp_getoptval(serv, options, service, PPP_OPT_LOGFILE, sopt, sizeof(sopt), &len) && sopt[0]) {
        // if logfile start with /, it's a full path
        // otherwise it's relative to the logs folder (convention)
        // we also strongly advise to name the file with the link number
        // for example ppplog0
        // the default path is /var/log
        // it's useful to have the debug option with the logfile option
        // with debug option, pppd will log the negociation
        // debug option is different from kernel debug trace

        snprintf(str, sizeof(str), "%s%s", sopt[0] == '/' ? "" : DIR_LOGS, sopt);
        writestrparam(optfd, "logfile", str);
    }

    // -----------------
    // connection plugin
    if (serv->subtypeRef) {
		CFStringGetCString(serv->subtypeRef, str2, sizeof(str2) - 4, kCFStringEncodingUTF8);
		strlcat(str2, ".ppp", sizeof(str2));	// add plugin suffix
		writestrparam(optfd, "plugin", str2);
	}
	
    // -----------------
    // device name 
    if (ppp_getoptval(serv, options, service, PPP_OPT_DEV_NAME, sopt, sizeof(sopt), &len) && sopt[0])
        writestrparam(optfd, "device", (char*)sopt);

    // -----------------
    // device speed 
    if (ppp_getoptval(serv, options, service, PPP_OPT_DEV_SPEED, &lval, sizeof(lval), &len) && lval) {
        snprintf(str, sizeof(str), "%d", lval);
        writeparam(optfd, str);
    }
        
    // Scoped interface
    char outgoingInterfaceString[IFXNAMSIZ];
    if (options && GetStrFromDict(options, CFSTR(NESessionStartOptionOutgoingInterface), outgoingInterfaceString, IFXNAMSIZ, "")) {
        writestrparam(optfd, "ifscope", outgoingInterfaceString);
    }
    
    // -----------------
    // subtype specific parameters 

    switch (serv->subtype) {
    
        case PPP_TYPE_SERIAL: 
    
            // the controller has a built-in knowledge of serial connections
    
            /*  PPPSerial currently supports Modem
                in case of Modem, the DeviceName key will contain the actual device,
                while the Modem dictionnary will contain the modem settings.
                if Hardware is undefined, or different from Modem, 
                we expect to find external configuration files. 
                (This is the case for ppp over ssh) 
            */
            get_str_option(serv, kSCEntNetInterface, kSCPropNetInterfaceHardware, options, 0, sopt, sizeof(sopt), &lval, empty_str);
            if (strcmp((char*)sopt, "Modem")) {
                // we are done
                break;
            }
        
	#if 1
			/* merge all modem options into modemdict */
			modemdict = copyEntity(gDynamicStore, kSCDynamicStoreDomainSetup, serv->serviceID, kSCEntNetModem);
			if (!modemdict) {
                break;
			}

			if (options) {
				dict = CFDictionaryGetValue(options, kSCEntNetModem);
				if (dict && 
					(CFGetTypeID(dict) == CFDictionaryGetTypeID()) && 
					CFDictionaryGetCount(dict)) {
				
					CFMutableDictionaryRef modemdict_mutable = CFDictionaryCreateMutableCopy(NULL, 0, modemdict);
					if (modemdict_mutable) {
						CFTypeRef value;

						// merge it into modemdict only some of the keys
						
						if ((value = CFDictionaryGetValue(dict, kSCPropNetModemConnectionScript)))
							CFDictionarySetValue(modemdict_mutable, kSCPropNetModemConnectionScript, value);

						if ((value = CFDictionaryGetValue(dict, kSCPropNetModemSpeaker)))
							CFDictionarySetValue(modemdict_mutable, kSCPropNetModemSpeaker, value);
						if ((value = CFDictionaryGetValue(dict, kSCPropNetModemErrorCorrection)))
							CFDictionarySetValue(modemdict_mutable, kSCPropNetModemErrorCorrection, value);
						if ((value = CFDictionaryGetValue(dict, kSCPropNetModemDataCompression)))
							CFDictionarySetValue(modemdict_mutable, kSCPropNetModemDataCompression, value);
						if ((value = CFDictionaryGetValue(dict, kSCPropNetModemPulseDial)))
							CFDictionarySetValue(modemdict_mutable, kSCPropNetModemPulseDial, value);
						if ((value = CFDictionaryGetValue(dict, kSCPropNetModemDialMode)))
							CFDictionarySetValue(modemdict_mutable, kSCPropNetModemDialMode, value);
							
						CFRelease(modemdict);
						modemdict = modemdict_mutable;
					}
				}
			}
			
			/* serialize the modem dictionary, and pass it as a parameter */
			if ((dataref = Serialize(modemdict, &dataptr, &datalen))) {

				writedataparam(optfd, "modemdict", dataptr, datalen);
				CFRelease(dataref);
			}

			CFRelease(modemdict);
#endif
	#if 0
	
            if (ppp_getoptval(ppp, options, 0, PPP_OPT_DEV_CONNECTSCRIPT, sopt, sizeof(sopt), &len) && sopt[0]) {
                // ---------- connect script parameter ----------
                writestrparam(optfd, "modemscript", sopt);
                
                // add all the ccl flags
                get_int_option(ppp, kSCEntNetModem, kSCPropNetModemSpeaker, options, 0, &lval, 1);
                writeparam(optfd, lval ? "modemsound" : "nomodemsound");
        
                get_int_option(ppp, kSCEntNetModem, kSCPropNetModemErrorCorrection, options, 0, &lval, 1);
                writeparam(optfd, lval ? "modemreliable" : "nomodemreliable");
    
                get_int_option(ppp, kSCEntNetModem, kSCPropNetModemDataCompression, options, 0, &lval, 1);
                writeparam(optfd, lval ? "modemcompress" : "nomodemcompress");
    
                get_int_option(ppp, kSCEntNetModem, kSCPropNetModemPulseDial, options, 0, &lval, 0);
                writeparam(optfd, lval ? "modempulse" : "modemtone");
        
                // dialmode : 0 = normal, 1 = blind(ignoredialtone), 2 = manual
                lval = 0;
                ppp_getoptval(ppp, options, 0, PPP_OPT_DEV_DIALMODE, &lval, sizeof(lval), &len);
                writeintparam(optfd, "modemdialmode", lval);
            }
#endif
            break;
            
        case PPP_TYPE_L2TP: 
        
            string = get_cf_option(kSCEntNetL2TP, kSCPropNetL2TPTransport, CFStringGetTypeID(), options, service, 0);
            if (string) {
                if (CFStringCompare(string, kSCValNetL2TPTransportIP, 0) == kCFCompareEqualTo)
                    writeparam(optfd, "l2tpnoipsec");
            }
    
			/* check for SharedSecret keys in L2TP dictionary */
            get_str_option(serv, kSCEntNetL2TP, kSCPropNetL2TPIPSecSharedSecret, options, service, sopt, sizeof(sopt), &lval, empty_str);
            if (sopt[0]) {
                writestrparam(optfd, "l2tpipsecsharedsecret", (char*)sopt);                        

				string = get_cf_option(kSCEntNetL2TP, kSCPropNetL2TPIPSecSharedSecretEncryption, CFStringGetTypeID(), options, service, 0);
				if (string) {
					if (CFStringCompare(string, CFSTR("Key"), 0) == kCFCompareEqualTo)
						writestrparam(optfd, "l2tpipsecsharedsecrettype", "key");                        
					else if (CFStringCompare(string, kSCValNetL2TPIPSecSharedSecretEncryptionKeychain, 0) == kCFCompareEqualTo)
						writestrparam(optfd, "l2tpipsecsharedsecrettype", "keychain");                        
				}
            } 
			/* then check IPSec dictionary */
			else {		
				get_str_option(serv, kSCEntNetIPSec, kSCPropNetIPSecSharedSecret, options, service, sopt, sizeof(sopt), &lval, empty_str);
				if (sopt[0]) {
					writestrparam(optfd, "l2tpipsecsharedsecret", (char*)sopt);                        
					string = get_cf_option(kSCEntNetL2TP, kSCPropNetIPSecSharedSecretEncryption, CFStringGetTypeID(), options, service, 0);
					if (string) {
						if (CFStringCompare(string, CFSTR("Key"), 0) == kCFCompareEqualTo)
							writestrparam(optfd, "l2tpipsecsharedsecrettype", "key");                        
						else if (CFStringCompare(string, kSCValNetIPSecSharedSecretEncryptionKeychain, 0) == kCFCompareEqualTo)
							writestrparam(optfd, "l2tpipsecsharedsecrettype", "keychain");                        
					}
				}
			}
			
            get_int_option(serv, kSCEntNetL2TP, CFSTR("UDPPort"), options, service, &lval, 0 /* Dynamic port */);
            writeintparam(optfd, "l2tpudpport", lval);
            break;
    }
    
    // -----------------
    // terminal option
    if (ppp_getoptval(serv, options, service, PPP_OPT_COMM_TERMINALMODE, &lval, sizeof(lval), &len)) {

        /* add the PPPSerial plugin if not already present
         Fix me : terminal mode is only supported in PPPSerial types of connection
         but subtype using ptys can use it the same way */    
        if (lval != PPP_COMM_TERM_NONE && serv->subtype != PPP_TYPE_SERIAL)
            writestrparam(optfd, "plugin", "PPPSerial.ppp");

        if (lval == PPP_COMM_TERM_WINDOW)
            writeparam(optfd, "terminalwindow");
        else if (lval == PPP_COMM_TERM_SCRIPT)
            if (ppp_getoptval(serv, options, service, PPP_OPT_COMM_TERMINALSCRIPT, sopt, sizeof(sopt), &len) && sopt[0])
                writestrparam(optfd, "terminalscript", (char*)sopt);            
    }

    // -----------------
    // generic phone number option
    if (ppp_getoptval(serv, options, service, PPP_OPT_COMM_REMOTEADDR, sopt, sizeof(sopt), &len) && sopt[0])
        writestrparam(optfd, "remoteaddress", (char*)sopt);
    
    // -----------------
    // redial options 
    get_int_option(serv, kSCEntNetPPP, kSCPropNetPPPCommRedialEnabled, options, service, &lval, 0);
    if (lval) {
            
        get_str_option(serv, kSCEntNetPPP, kSCPropNetPPPCommAlternateRemoteAddress, options, service, sopt, sizeof(sopt), &lval, empty_str);
        if (sopt[0])
            writestrparam(optfd, "altremoteaddress", (char*)sopt);
        
        get_int_option(serv, kSCEntNetPPP, kSCPropNetPPPCommRedialCount, options, service, &lval, 0);
        if (lval)
            writeintparam(optfd, "redialcount", lval);

        get_int_option(serv, kSCEntNetPPP, kSCPropNetPPPCommRedialInterval, options, service, &lval, 0);
        if (lval)
            writeintparam(optfd, "redialtimer", lval);
    }

	awaketime = gSleeping ? 0 : ((mach_absolute_time() - gWakeUpTime) * gTimeScaleSeconds);
	if (awaketime < MAX_EXTRACONNECTTIME) {
        writeintparam(optfd, "extraconnecttime", MAX(MAX_EXTRACONNECTTIME - awaketime, MIN_EXTRACONNECTTIME));
	}
	
	// -----------------
    // idle options 
    if (ppp_getoptval(serv, options, service, PPP_OPT_COMM_IDLETIMER, &lval, sizeof(lval), &len) && lval) {
        writeintparam(optfd, "idle", lval);
        writeparam(optfd, "noidlerecv");
    }

    // -----------------
    // connection time option 
    if (ppp_getoptval(serv, options, service, PPP_OPT_COMM_SESSIONTIMER, &lval, sizeof(lval), &len) && lval)
        writeintparam(optfd, "maxconnect", lval);
    
    // -----------------
    // dial on demand options 
    if (onTraffic) {
        writeparam(optfd, "demand");
        get_int_option(serv, kSCEntNetPPP, CFSTR("HoldOffTime"), 0, service, &lval, 30);
        writeintparam(optfd, "holdoff", lval);
		if ((onTraffic & 0x2) && lval)
			writeparam(optfd, "holdfirst");
        get_int_option(serv, kSCEntNetPPP, CFSTR("MaxFailure"), 0, service, &lval, 3);
        writeintparam(optfd, "maxfail", lval);
    } else {
#if TARGET_OS_OSX
        // if reconnecting, add option to wait for successful resolver
        if (serv->persist_connect) {
            writeintparam(optfd, "retrylinkcheck", 10);
        }
#endif
    }

    // -----------------
    // lcp echo options 
    // set echo option, so ppp hangup if we pull the modem cable
    // echo option is 2 bytes for interval + 2 bytes for failure
    if (ppp_getoptval(serv, options, service, PPP_OPT_LCP_ECHO, &lval, sizeof(lval), &len) && lval) {
        if (lval >> 16)
            writeintparam(optfd, "lcp-echo-interval", lval >> 16);

        if (lval & 0xffff)
            writeintparam(optfd, "lcp-echo-failure", lval & 0xffff);
    }
    
    // -----------------
    // address and protocol field compression options 
    if (ppp_getoptval(serv, options, service, PPP_OPT_LCP_HDRCOMP, &lval, sizeof(lval), &len)) {
        if (!(lval & 1))
            writeparam(optfd, "nopcomp");
        if (!(lval & 2))
            writeparam(optfd, "noaccomp");
    }

    // -----------------
    // mru option 
    if (ppp_getoptval(serv, options, service, PPP_OPT_LCP_MRU, &lval, sizeof(lval), &len) && lval)
        writeintparam(optfd, "mru", lval);

    // -----------------
    // mtu option 
    if (ppp_getoptval(serv, options, service, PPP_OPT_LCP_MTU, &lval, sizeof(lval), &len) && lval)
        writeintparam(optfd, "mtu", lval);

    // -----------------
    // receive async map option 
    if (ppp_getoptval(serv, options, service, PPP_OPT_LCP_RCACCM, &lval, sizeof(lval), &len)) {
        if (lval)
			writeintparam(optfd, "asyncmap", lval);
		else 
			writeparam(optfd, "receive-all");
	} 
	else 
		writeparam(optfd, "default-asyncmap");

    // -----------------
    // send async map option 
     if (ppp_getoptval(serv, options, service, PPP_OPT_LCP_TXACCM, &lval, sizeof(lval), &len) && lval) {
            writeparam(optfd, "escape");
            str[0] = 0;
            for (lval1 = 0; lval1 < 32; lval1++) {
                if ((lval >> lval1) & 1) {
                    snprintf(str2, sizeof(str2), "%d,", lval1);
                    strlcat(str, str2, sizeof(str));
               }
            }
            str[strlen(str)-1] = 0; // remove last ','
            writeparam(optfd, str);
       }

    // -----------------
    // ipcp options 
	if (!CFDictionaryContainsKey(service, kSCEntNetIPv4)) {
        writeparam(optfd, "noip");
    }
    else {
    
        // -----------------
        // set ip param to be the router address 
        if (getStringFromEntity(gDynamicStore, kSCDynamicStoreDomainState, 0, 
            kSCEntNetIPv4, kSCPropNetIPv4Router, sopt, OPT_STR_LEN) && sopt[0])
            writestrparam(optfd, "ipparam", (char*)sopt);
        
        // OverridePrimary option not handled yet in Setup by IPMonitor
        get_int_option(serv, kSCEntNetIPv4, kSCPropNetOverridePrimary, 0 /* don't look in options */, service, &lval, 0);
        if (lval) {
			overrideprimary = 1;
            writeparam(optfd, "defaultroute");
		}
    
        // -----------------
        // vj compression option 
        if (! (ppp_getoptval(serv, options, service, PPP_OPT_IPCP_HDRCOMP, &lval, sizeof(lval), &len) && lval))
            writeparam(optfd, "novj");
    
        // -----------------
        // XXX  enforce the source address
        if (serv->subtype == PPP_TYPE_L2TP) {
            writeintparam(optfd, "ip-src-address-filter", 2);
        }
        
        // -----------------
        // ip addresses options
        if (ppp_getoptval(serv, options, service, PPP_OPT_IPCP_LOCALADDR, &lval, sizeof(lval), &len) && lval)
            snprintf(str2, sizeof(str2), "%d.%d.%d.%d", lval >> 24, (lval >> 16) & 0xFF, (lval >> 8) & 0xFF, lval & 0xFF);
        else 
            strlcpy(str2, "0", sizeof(str2));
    
        strlcpy(str, str2, sizeof(str));
        strlcat(str, ":", sizeof(str));
        if (ppp_getoptval(serv, options, service, PPP_OPT_IPCP_REMOTEADDR, &lval, sizeof(lval), &len) && lval) 
            snprintf(str2, sizeof(str2), "%d.%d.%d.%d", lval >> 24, (lval >> 16) & 0xFF, (lval >> 8) & 0xFF, lval & 0xFF);
        else 
            strlcpy(str2, "0", sizeof(str2));
        strlcat(str, str2, sizeof(str));
        writeparam(optfd, str);
    
        writeparam(optfd, "noipdefault");
        writeparam(optfd, "ipcp-accept-local");
        writeparam(optfd, "ipcp-accept-remote");
    

    /* ************************************************************************* */
    
        // usepeerdns option
		get_int_option(serv, kSCEntNetPPP, CFSTR("IPCPUsePeerDNS"), options, service, &lval, 1);
        if (lval)
            writeparam(optfd, "usepeerdns");

		// usepeerwins if a SMB dictionary is present
		// but make sure it is not disabled in PPP
#if TARGET_OS_OSX
		if (CFDictionaryContainsKey(service, kSCEntNetSMB)) {
			get_int_option(serv, kSCEntNetPPP, CFSTR("IPCPUsePeerWINS"), options, service, &lval, 1);
			if (lval)
				writeparam(optfd, "usepeerwins");
		}
#endif
		
		// -----------------
		// add a route for the interface subnet, if L2TP or PPTP (with encryption) VPN enabled
		
		switch (serv->subtype) {
			case PPP_TYPE_L2TP:
				writeparam(optfd, "addifroute");				
				break;
				
			default:
				break;
		}		
    } // of existEntity IPv4
    
    // -----------------
    // ip6cp options 
	if (!CFDictionaryContainsKey(service, kSCEntNetIPv6)) {
        // ipv6 is not started by default
    }
    else {
        writeparam(optfd, "+ipv6");
        writeparam(optfd, "ipv6cp-use-persistent");
    }

	// -----------------
	// acsp options and DHCP options

	if (overrideprimary) {
		// acsp and dhcp not need when all traffic is sent over PPP
		writeparam(optfd, "noacsp"); 
		writeparam(optfd, "no-use-dhcp"); 
	}
	else {
		// acsp options
		get_int_option(serv, kSCEntNetPPP, kSCPropNetPPPACSPEnabled, options, service, &lval, 0);
		if (lval == 0)
			writeparam(optfd, "noacsp");
		
		// dhcp is on by default for vpn, and off for everything else 
		get_int_option(serv, kSCEntNetPPP, CFSTR("UseDHCP"), options, service, &lval,  (serv->subtype == PPP_TYPE_L2TP) ? 1 : 0);
		if (lval == 1)
			writeparam(optfd, "use-dhcp");
	}

    // -----------------
    // authentication options 

    // don't want authentication on our side...
    writeparam(optfd, "noauth");

     if (ppp_getoptval(serv, options, service, PPP_OPT_AUTH_PROTO, &lval, sizeof(lval), &len) && (lval != PPP_AUTH_NONE)) {

		CFStringRef			encryption = NULL;
        
		if (ppp_getoptval(serv, options, service, PPP_OPT_AUTH_NAME, sopt, sizeof(sopt), &len) && sopt[0]) {


            writestrparam(optfd, "user", (char*)sopt);
			needpasswd = 1;

            lval1 = get_str_option(serv, kSCEntNetPPP, kSCPropNetPPPAuthPassword, options, service, sopt, sizeof(sopt), &lval, empty_str);
            if (sopt[0]) {
			
                /* get the encryption method at the same place the password is coming from. */
				encryption = get_cf_option(kSCEntNetPPP, kSCPropNetPPPAuthPasswordEncryption, CFStringGetTypeID(), 
					(lval1 == 3) ? NULL : options, (lval1 == 3) ? service : NULL , NULL);

				if (encryption && (CFStringCompare(encryption, kSCValNetPPPAuthPasswordEncryptionKeychain, 0) == kCFCompareEqualTo)) {
					writestrparam(optfd, (lval1 == 3) ? "keychainpassword" : "userkeychainpassword", (char*)sopt);
				}
				else if (encryption && (CFStringCompare(encryption, kSCValNetPPPAuthPasswordEncryptionToken, 0) == kCFCompareEqualTo)) {
					writeintparam(optfd, "tokencard", 1);
					tokendone = 1;
				}
				else {
					CFStringRef aString = CFStringCreateWithCString(NULL, (char*)sopt, kCFStringEncodingUTF8);
					if (aString) {
						CFStringGetCString(aString, (char*)sopt, OPT_STR_LEN, kCFStringEncodingWindowsLatin1);
						CFRelease(aString);
					}
					writestrparam(optfd, "password", (char*)sopt);
				}
            }
            else { 
				encryption = get_cf_option(kSCEntNetPPP, kSCPropNetPPPAuthPasswordEncryption, CFStringGetTypeID(), options, service, NULL);
				if (encryption && (CFStringCompare(encryption, kSCValNetPPPAuthPasswordEncryptionToken, 0) == kCFCompareEqualTo)) {
					writeintparam(optfd, "tokencard", 1);
					tokendone = 1;
				}
            }
        }
		else {
			encryption = get_cf_option(kSCEntNetPPP, kSCPropNetPPPAuthPasswordEncryption, CFStringGetTypeID(), options, service, NULL);
			if (encryption && (CFStringCompare(encryption, kSCValNetPPPAuthPasswordEncryptionToken, 0) == kCFCompareEqualTo)) {
				writeintparam(optfd, "tokencard", 1);
				tokendone = 1;
				needpasswd = 1;
			}
			else {
				// keep the same behavior for modems.
				// prompt for username + password for VPN
				if (serv->subtype == PPP_TYPE_L2TP) {
					needpasswd = 1;
				}
			}
		}
    }

    // load authentication protocols
    array = 0;
    if (options) {
        dict = CFDictionaryGetValue(options, kSCEntNetPPP);
        if (dict && (CFGetTypeID(dict) == CFDictionaryGetTypeID()))
            array = CFDictionaryGetValue(dict, kSCPropNetPPPAuthProtocol);            
    }
    if ((array == 0) || (CFGetTypeID(array) != CFArrayGetTypeID())) {
        array = CFDictionaryGetValue(pppdict, kSCPropNetPPPAuthProtocol);
    }
    if (array && (CFGetTypeID(array) == CFArrayGetTypeID()) && (lval = (u_int32_t)CFArrayGetCount(array))) {
        auth_default = 0;
        auth_bits = 0; // clear bits
        for (i = 0; i < lval; i++) {
            string = CFArrayGetValueAtIndex(array, i);
            if (string && (CFGetTypeID(string) == CFStringGetTypeID())) {
                if (CFStringCompare(string, kSCValNetPPPAuthProtocolPAP, 0) == kCFCompareEqualTo)
                    auth_bits |= 1;
                else if (CFStringCompare(string, kSCValNetPPPAuthProtocolCHAP, 0) == kCFCompareEqualTo)
                    auth_bits |= 2;
                else if (CFStringCompare(string, CFSTR("MSCHAP1") /*kSCValNetPPPAuthProtocolMSCHAP1*/ , 0) == kCFCompareEqualTo)
                    auth_bits |= 4;
                else if (CFStringCompare(string,  CFSTR("MSCHAP2") /*kSCValNetPPPAuthProtocolMSCHAP2*/, 0) == kCFCompareEqualTo)
                    auth_bits |= 8;
                else if (CFStringCompare(string,  CFSTR("EAP") /*kSCValNetPPPAuthProtocolEAP*/, 0) == kCFCompareEqualTo)
                    auth_bits |= 0x10;
            }
        }
    }

	

	if (!tokendone) {
		// authentication variation for token card support...
		get_int_option(serv, kSCEntNetPPP, CFSTR("TokenCard"), options, service, &lval, 0);
		if (lval) {
			writeintparam(optfd, "tokencard", lval);
			needpasswd = 1;
		}
	}
	
    // -----------------
    // eap options 

    if (auth_bits & 0x10) {
        auth_bits &= ~0x10; // clear EAP flag
        array = 0;
        from_service = 0;
        if (options) {
            dict = CFDictionaryGetValue(options, kSCEntNetPPP);
            if (dict && (CFGetTypeID(dict) == CFDictionaryGetTypeID()))
                array = CFDictionaryGetValue(dict, CFSTR("AuthEAPPlugins") /*kSCPropNetPPPAuthEAPPlugins*/);
        }
        if ((array == 0) || (CFGetTypeID(array) != CFArrayGetTypeID())) {
            array = CFDictionaryGetValue(pppdict, CFSTR("AuthEAPPlugins") /*kSCPropNetPPPAuthEAPPlugins*/);
            from_service = 1;
        }
        if (array && (CFGetTypeID(array) == CFArrayGetTypeID()) && (lval = (u_int32_t)CFArrayGetCount(array))) {
            for (i = 0; i < lval; i++) {
                string = CFArrayGetValueAtIndex(array, i);
                if (string && (CFGetTypeID(string) == CFStringGetTypeID())) {
                    CFStringGetCString(string, str, sizeof(str) - 4, kCFStringEncodingUTF8);
                    // for user options, we only accept plugin in the EAP directory (/System/Library/SystemConfiguration/PPPController.bundle/Contents/PlugIns)
                    if (from_service || strchr(str, '\\') == 0) {
                        strlcat(str, ".ppp", sizeof(str));	// add plugin suffix
                        writestrparam(optfd, "eapplugin", str);
                        auth_bits |= 0x10; // confirm EAP flag
                    }
                }
            }
        }
    }

    // -----------------
    // ccp options
	if(ccp_enabled &&
        // Fix me : to enforce use of MS-CHAP, refuse any alteration of default auth proto 
        // a dialer specifying PAP or CHAP will work without CCP/MPPE
        // even is CCP is enabled in the configuration.
        // Will be revisited when addition compression modules and
        // authentication modules will be added 
	   ppp_getoptval(serv, options, service, PPP_OPT_AUTH_PROTO, &lval, sizeof(lval), &len) 
                && (lval == OPT_AUTH_PROTO_DEF)) {

        // Fix me : mppe is the only currently supported compression 
        // if the CCPAccepted and CCPRequired array are not there, 
        // assume we accept all types of compression we support

        writeparam(optfd, "mppe-stateless");
		get_int_option(serv, kSCEntNetPPP, CFSTR("CCPMPPE128Enabled"), options, service, &lval, 1);
		writeparam(optfd, lval ? "mppe-128" : "nomppe-128");        
		get_int_option(serv, kSCEntNetPPP, CFSTR("CCPMPPE40Enabled"), options, service, &lval, 1);
        writeparam(optfd, lval ? "mppe-40" : "nomppe-40");        

        // No authentication specified, also enforce the use of MS-CHAP
        if (auth_default)
            auth_bits = 0xc; /* MSCHAP 1 and 2 only */
    }
    else {
        // no compression protocol
        writeparam(optfd, "noccp");	
    }
    
    // set authentication protocols parameters
    if ((auth_bits & 1) == 0)
        writeparam(optfd, "refuse-pap");
    if ((auth_bits & 2) == 0)
        writeparam(optfd, "refuse-chap-md5");
    if ((auth_bits & 4) == 0)
        writeparam(optfd, "refuse-mschap");
    if ((auth_bits & 8) == 0)
        writeparam(optfd, "refuse-mschap-v2");
    if ((auth_bits & 0x10) == 0)
        writeparam(optfd, "refuse-eap");
        
    // if EAP is the only method, pppd doesn't need to ask for the password
    // let the EAP plugin handle that.
    // if there is an other protocol than EAP, then we still need to prompt for password 
    if (auth_bits == 0x10)
        needpasswd = 0;

    // loop local traffic destined to the local ip address
    // Radar #3124639.
    //writeparam(optfd, "looplocal");       

#if TARGET_OS_OSX
    if (!(serv->flags & FLAG_ALERTPASSWORDS) || !needpasswd || serv->flags & FLAG_DARKWAKE)
#else
    if (!(serv->flags & FLAG_ALERTPASSWORDS) || !needpasswd)
#endif
        writeparam(optfd, "noaskpassword");

    get_str_option(serv, kSCEntNetPPP, kSCPropNetPPPAuthPrompt, options, service, sopt, sizeof(sopt), &lval, empty_str);
    if (sopt[0]) {
        str2[0] = 0;
        CFStringGetCString(kSCValNetPPPAuthPromptAfter, str2, sizeof(str2), kCFStringEncodingUTF8);
        if (!strcmp((char *)sopt, str2))
            writeparam(optfd, "askpasswordafter");
    }
    
    // -----------------
    // no need for pppd to detach.
    writeparam(optfd, "nodetach");

    // -----------------
    // reminder option must be specified after PPPDialogs plugin option
    get_int_option(serv, kSCEntNetPPP, kSCPropNetPPPIdleReminder, options, service, &lval, 0);
    if (lval) {
        get_int_option(serv, kSCEntNetPPP, kSCPropNetPPPIdleReminderTimer, options, service, &lval, 0);
        if (lval)
            writeintparam(optfd, "reminder", lval);
    }

    // -----------------
    // add any additional plugin we want to load
    array = CFDictionaryGetValue(pppdict, kSCPropNetPPPPlugins);
    if (array && (CFGetTypeID(array) == CFArrayGetTypeID())) {
        lval = (u_int32_t)CFArrayGetCount(array);
        for (i = 0; i < lval; i++) {
            string = CFArrayGetValueAtIndex(array, i);
            if (string && (CFGetTypeID(string) == CFStringGetTypeID())) {
                CFStringGetCString(string, str, sizeof(str) - 4, kCFStringEncodingUTF8);
                strlcat(str, ".ppp", sizeof(str));	// add plugin suffix
                writestrparam(optfd, "plugin", str);
            }
        }
    }
    
    // -----------------
     // always try to use options defined in /etc/ppp/peers/[service provider] 
    // they can override what have been specified by the PPPController
    // look first in ppp dictionary, then in service
	if (GetStrFromDict(pppdict, kSCPropUserDefinedName, (char*)sopt, OPT_STR_LEN, empty_str_s)
		|| GetStrFromDict(service, kSCPropUserDefinedName, (char*)sopt, OPT_STR_LEN, empty_str_s)) 
        writestrparam(optfd, "call", (char*)sopt);
	
    writeparam(optfd, "[EOP]");

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static 
int change_pppd_params(struct service *serv, CFDictionaryRef service, CFDictionaryRef options)
{
    int 			optfd;
    u_int32_t			lval, len;
    CFDictionaryRef		pppdict = NULL;

    pppdict = CFDictionaryGetValue(service, kSCEntNetPPP);
    if ((pppdict == 0) || (CFGetTypeID(pppdict) != CFDictionaryGetTypeID()))
        return -1; 	// that's bad...
    
    optfd = serv->u.ppp.controlfd[WRITE];

    writeparam(optfd, "[OPTIONS]");

    // -----------------
    // reminder option must be specified after PPPDialogs plugin option
    get_int_option(serv, kSCEntNetPPP, kSCPropNetPPPIdleReminder, options, service, &lval, 0);
    if (lval)
        get_int_option(serv, kSCEntNetPPP, kSCPropNetPPPIdleReminderTimer, options, service, &lval, 0);
    writeintparam(optfd, "reminder", lval);

    // -----------------
    ppp_getoptval(serv, options, service, PPP_OPT_COMM_IDLETIMER, &lval, sizeof(lval), &len);
    writeintparam(optfd, "idle", lval);

    // Scoped interface
    char outgoingInterfaceString[IFXNAMSIZ];
    if (options && GetStrFromDict(options, CFSTR(NESessionStartOptionOutgoingInterface), outgoingInterfaceString, IFXNAMSIZ, "")) {
        writestrparam(optfd, "ifscope", outgoingInterfaceString);
    }
		
    writeparam(optfd, "[EOP]");

    return 0;
}

int ppp_install(struct service *serv)
{
	int optfd;
	
	optfd = serv->u.ppp.controlfd[WRITE];
	
	writeparam(optfd, "[INSTALL]");
	return 0;
}

int ppp_uninstall(struct service *serv)
{
	int optfd;
	
	optfd = serv->u.ppp.controlfd[WRITE];
	
	writeparam(optfd, "[UNINSTALL]");
	return 0;
}

static void MT_checkForProxies (const void *key, const void *value, PPPSession_t *pppSession)
{
	if (CFStringHasSuffix(key, CFSTR("Enable"))) {
		int val = 0;
		if ((CFGetTypeID(value) == CFNumberGetTypeID()) && CFNumberGetValue(value, kCFNumberIntType, &val) && val != 0) {
			snprintf(pppSession->proxiesEnabled, OPT_LEN, "1");
		}
	}
}

void MT_pppGetTracerOptions(struct service *serv, PPPSession_t *pppSession)
{
	CFDictionaryRef dict = NULL;
	CFDictionaryRef service = NULL;
	char buffer[MT_STR_LEN] = {0};
	char vendor[MT_STR_LEN] = { 0 };
	char model[MT_STR_LEN] = { 0 };
	u_int32_t val = 0;
	
	memset(pppSession, '\0', sizeof(PPPSession_t));
	
	service = copyService(gDynamicStore, kSCDynamicStoreDomainSetup, serv->serviceID);
	
	if (service == NULL) {
		return;
	}

	/* Get Modem vendor */
	dict = CFDictionaryGetValue(service, kSCEntNetModem);
	if (dict != NULL) {
		if (getString(dict, kSCPropNetModemDeviceVendor, (u_char *)buffer, MT_STR_LEN)) {
			memset(vendor, 0, MT_STR_LEN);
			strlcpy(vendor, buffer, MT_STR_LEN);
			memset(buffer, 0, MT_STR_LEN);
		}
		
		/* Get modem model */
		if (getString(dict, kSCPropNetModemDeviceModel, (u_char *)buffer, MT_STR_LEN)) {
			memset(model, 0, MT_STR_LEN);
			strlcpy(model, buffer, MT_STR_LEN);
			memset(buffer, 0, MT_STR_LEN);
		}
		
		if (strlen(vendor) > 0 && strlen(model) > 0) {
			memset(pppSession->modem, 0, MT_STR_LEN);
			strlcat(pppSession->modem, vendor, MT_STR_LEN);
			strlcat(pppSession->modem, "/", MT_STR_LEN);
			strlcat(pppSession->modem, model, MT_STR_LEN);
		}
	}
	
	dict = CFDictionaryGetValue(service, kSCEntNetPPP);
	
	if (dict != NULL) {
		/* Get DialOnDemand status		- "Connect automatically when needed" */
		getNumber(dict, kSCPropNetPPPDialOnDemand, &val);
		
		snprintf(pppSession->dialOnDemand, OPT_LEN ,"%u", val);
		val = 0;
		
		/* Get Prompt status			- "Prompt every X minutes to maintain connection" */
		getNumber(dict, kSCPropNetPPPIdleReminder, &val);
		
		snprintf(pppSession->idleReminder, OPT_LEN, "%u", val);
		val = 0;
		
		/* Get disconnect status		- "Disconnect when user logs out */
		getNumber(dict, kSCPropNetPPPDisconnectOnLogout, &val);
		
		snprintf(pppSession->disconnectOnLogout, OPT_LEN, "%u", val);
		val = 0;
		
		/* Get disconnect status		- "Disconnect when switching user account */
		getNumber(dict, kSCPropNetPPPDisconnectOnFastUserSwitch, &val);
		
		snprintf(pppSession->disconnectOnUserSwitch, OPT_LEN, "%u", val);
		val = 0;
		
		/* Get password prompt status	- "Prompt for password after dialling" */
		if (getString(dict, kSCPropNetPPPAuthPrompt, (u_char *)buffer, MT_STR_LEN)) {
			memset(pppSession->authPrompt, 0, MT_STR_LEN);
			strlcpy(pppSession->authPrompt, buffer, MT_STR_LEN);
			memset(buffer, 0, MT_STR_LEN);
		}
		
		/* Get Redial status			- "Redial X times if busy, ..." */
		getNumber(dict, kSCPropNetPPPCommRedialEnabled, &val);
		
		snprintf(pppSession->redialEnabled, OPT_LEN, "%u", val);
		val = 0;
		
		/* Get EchoEnabled status		- "Send PPP Echo Packet" */
		getNumber(dict, kSCPropNetPPPLCPEchoEnabled, &val);
		
		snprintf(pppSession->echoEnabled, OPT_LEN, "%u", val);
		val = 0;
		
		/* Get verbose logging status	- "Use verbose logging" */
		getNumber(dict, kSCPropNetPPPVerboseLogging, &val);
		
		snprintf(pppSession->verboseLogging, OPT_LEN, "%u", val);
		val = 0;

		/* Get VJ Compression status	- "Use TCP header compression" */
		getNumber(dict, kSCPropNetPPPIPCPCompressionVJ, &val);
		
		snprintf(pppSession->vjCompression, OPT_LEN, "%u", val);
		val = 0;
		
		/* Get Terminal window status	- "Connect using a terminal window (command line)" */
		getNumber(dict, kSCPropNetPPPCommDisplayTerminalWindow, &val);
		
		snprintf(pppSession->useTerminal, OPT_LEN, "%u", val);
		val = 0;
	}
	
	//Check if the connection uses manual DNS
	dict = CFDictionaryGetValue(service, kSCEntNetDNS);

	/* Check if any Search Domains/Server Addresses are specified */
	if (dict != NULL) {
		CFArrayRef serverAddresses	= CFDictionaryGetValue(dict, kSCPropNetDNSServerAddresses);
		if (serverAddresses != NULL) {
			CFIndex count = CFArrayGetCount(serverAddresses);
			if (count > 0) {
				val = 1;
			}
		}
	}
	
	snprintf(pppSession->manualDNS, OPT_LEN, "%u", val);
	val = 0;
	
	//Check if the connection uses manual IPv4
	dict = CFDictionaryGetValue(service, kSCEntNetIPv4);
	
	if (dict != NULL &&
		getString(dict, kSCPropNetIPv4ConfigMethod, (u_char *)buffer, MT_STR_LEN)) {
		CFStringRef temp_buffer;
		temp_buffer = CFStringCreateWithCString(NULL, buffer, kCFStringEncodingUTF8);
		
		if (temp_buffer != NULL) {
			if (CFEqual(temp_buffer, kSCValNetIPv4ConfigMethodManual)) {
				val = 1;
			}
			CFRelease(temp_buffer);
		}
		memset(buffer, 0, MT_STR_LEN);
	}
	
	snprintf(pppSession->manualIPv4, OPT_LEN, "%u", val);
	val = 0;
	
	//Check if the connection uses manual IPv6
	dict = CFDictionaryGetValue(service, kSCEntNetIPv6);
	
	if (dict != NULL &&
		getString(dict, kSCPropNetIPv6ConfigMethod, (u_char *)buffer, MT_STR_LEN)) {
		CFStringRef temp_buffer;
		temp_buffer = CFStringCreateWithCString(NULL, buffer, kCFStringEncodingUTF8);
		
		if (temp_buffer != NULL) {
			if (CFEqual(temp_buffer, kSCValNetIPv6ConfigMethodManual)) {
				val = 1;
			}
			CFRelease(temp_buffer);
		}
		memset(buffer, 0, MT_STR_LEN);
	}
	
	snprintf(pppSession->manualIPv6, OPT_LEN, "%u", val);
	val = 0;

#if TARGET_OS_OSX
	//Check if the connection uses WINS
	dict = CFDictionaryGetValue(service, kSCEntNetSMB);
	
	if (dict != NULL) {
		CFArrayRef winsAddresses = CFDictionaryGetValue(dict, kSCPropNetSMBWINSAddresses);
		if (winsAddresses != NULL) {
			CFIndex count = CFArrayGetCount(winsAddresses);
			if (count > 0) {
				val = 1;
			}
		}
	}
#endif /* TARGET_OS_OSX */

	snprintf(pppSession->winsEnabled, OPT_LEN, "%u", val);
	val = 0;


	//Check if the connection uses Proxies
	//Take this dictionary fron SCPreferences, as it is not a part of the service dict.
	SCPreferencesRef prefsRef = SCPreferencesCreate(NULL, CFSTR("Plugin:PPPController"), CFSTR("preferences.plist"));
	CFDictionaryRef networkServices = NULL;
	CFDictionaryRef myService = NULL;
	dict = NULL;
	
	if (prefsRef != NULL) {
		networkServices = SCPreferencesGetValue(prefsRef, kSCPrefNetworkServices);
	}
	
	if (networkServices != NULL) {
		myService = CFDictionaryGetValue(networkServices, serv->serviceID);
	}

	if (myService != NULL) {
		dict = CFDictionaryGetValue(myService, kSCEntNetProxies);
	}
	
	snprintf(pppSession->proxiesEnabled, OPT_LEN, "%u", val);
	
	if (dict != NULL) {
		CFDictionaryApplyFunction(dict, (void (*)(const void *, const void *, void *))MT_checkForProxies, pppSession);
	}

	/* Get Modem Hardware information from IOKit */
	if (serv->subtype != PPP_TYPE_PPPoE) {
		MT_getModemInformation(serv, prefsRef , pppSession);
	}
	
	if (prefsRef != NULL) {
		CFRelease(prefsRef);
	}
	
	CFRelease(service);
	
	return;
}

static void MT_getModemInformation(struct service *serv, SCPreferencesRef prefs, PPPSession_t *pppSess)
{
	if (serv == NULL || prefs == NULL || pppSess == NULL) {
		return;
	}
	
	SCNetworkServiceRef networkServ = SCNetworkServiceCopy(prefs, serv->serviceID);
	if (networkServ == NULL) {
		return;
	}
	
	SCNetworkInterfaceRef intf = SCNetworkServiceGetInterface(networkServ);
	if (intf == NULL) {
		return;
	}
	
	kern_return_t kr;
	
	/* Get the leaf interface */
	SCNetworkInterfaceRef temp_intf;
	while (1) {
		temp_intf = SCNetworkInterfaceGetInterface(intf);
		if (temp_intf == NULL) {
			break;
		}
		intf = temp_intf;
	}
	
	/* Get the BSD Name of the interface */
	CFStringRef bsdName = SCNetworkInterfaceGetBSDName(intf);
	if (bsdName == NULL) {
		return;
	}
	
	/* Query IOKit for the Hardware information about the modem */
	CFMutableDictionaryRef matchingDictionary = IOServiceMatching(kIOSerialBSDServiceValue);
	CFDictionarySetValue(matchingDictionary, CFSTR(kIOSerialBSDTypeKey), CFSTR(kIOSerialBSDAllTypes));
	
	io_iterator_t myIterator;
	io_service_t service;
	char hardwareStr[MT_STR_LEN] = {0};
	
	kr = IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDictionary, &myIterator);
	if (kr != KERN_SUCCESS) {
		return;
	}
	
	for ( ; (service = IOIteratorNext(myIterator)) ; IOObjectRelease(service)) {
		CFMutableDictionaryRef properties;
		kr = IORegistryEntryCreateCFProperties(service, &properties, kCFAllocatorDefault, kNilOptions);
		if (kr != KERN_SUCCESS || properties == NULL) {
			continue;
		}
		
		CFStringRef deviceKey = CFDictionaryGetValue(properties, CFSTR(kIOTTYDeviceKey));
		
		if (deviceKey == NULL || !CFEqual(deviceKey, bsdName)) {
			CFRelease(properties);
			continue;
		}
		
		/* If we are here, then we have found the modem being used */
		CFStringRef key;
		CFTypeRef val;
		char vid[MT_STR_LEN] = {0};
		char pid[MT_STR_LEN] = {0};
		
		/* Check if the transport bus is USB/PCI/Bluetooth */
		CFStringRef baseName = CFDictionaryGetValue(properties, CFSTR(kIOTTYBaseNameKey));
		
		if (CFEqual(baseName, CFSTR("usbmodem"))) {
			/* We have a USB modem */
			
			strlcat(hardwareStr, "usbmodem", MT_STR_LEN);
			
			/* Get VID */
			key = CFStringCreateWithFormat(NULL, NULL, CFSTR("idVendor"));
			val = IORegistryEntrySearchCFProperty(service,
												  kIOServicePlane,
												  key,
												  NULL,
												  kIORegistryIterateRecursively | kIORegistryIterateParents);
			
			if (val != NULL && isA_CFNumber(val)) {
				int num;
				CFNumberGetValue(val, kCFNumberIntType, &num);
				sprintf(vid, "%0.4x", num);
				strlcat(hardwareStr, ".", MT_STR_LEN);
				strlcat(hardwareStr, vid, MT_STR_LEN);
				CFRelease(val);
			}
			else {
				strlcat(hardwareStr, ".", MT_STR_LEN);
				strlcat(hardwareStr, "X", MT_STR_LEN);
			}
			
			CFRelease(key);
			
			/* Get PID */
			key = CFStringCreateWithFormat(NULL, NULL, CFSTR("idProduct"));
			val = IORegistryEntrySearchCFProperty(service,
												  kIOServicePlane,
												  key,
												  NULL,
												  kIORegistryIterateRecursively | kIORegistryIterateParents);
			
			if (val != NULL && isA_CFNumber(val)) {
				int num;
				CFNumberGetValue(val, kCFNumberIntType, &num);
				sprintf(pid, "%0.4x", num);
				strlcat(hardwareStr, ".", MT_STR_LEN);
				strlcat(hardwareStr, pid, MT_STR_LEN);
				CFRelease(val);
			}
			else {
				strlcat(hardwareStr, ".", MT_STR_LEN);
				strlcat(hardwareStr, "X", MT_STR_LEN);
			}
			
			CFRelease(key);
		}
		else if (CFEqual(baseName, CFSTR("pci-serial"))) {
			/* We have a pci-serial modem */
			
			strlcat(hardwareStr, "pci-serial", MT_STR_LEN);
			
			/* Get VID */
			key = CFStringCreateWithFormat(NULL, NULL, CFSTR("vendor-id"));
			val = IORegistryEntrySearchCFProperty(service,
												  kIOServicePlane,
												  key,
												  NULL,
												  kIORegistryIterateRecursively | kIORegistryIterateParents);
			
			if (val != NULL && isA_CFData(val)) {
				UInt8 num[4] = {0};
				CFDataGetBytes(val, CFRangeMake(0, CFDataGetLength(val)), num);
				sprintf(vid, "%0.2x%0.2x", num[1], num[0]);
				strlcat(hardwareStr, ".", MT_STR_LEN);
				strlcat(hardwareStr, vid, MT_STR_LEN);
				CFRelease(val);
			}
			else {
				strlcat(hardwareStr, ".", MT_STR_LEN);
				strlcat(hardwareStr, "X", MT_STR_LEN);
			}
			
			CFRelease(key);
			
			/* Get PID */
			key = CFStringCreateWithFormat(NULL, NULL, CFSTR("device-id"));
			val = IORegistryEntrySearchCFProperty(service,
												  kIOServicePlane,
												  key,
												  NULL,
												  kIORegistryIterateRecursively | kIORegistryIterateParents);
			
			if (val != NULL && isA_CFData(val)) {
				UInt8 num[4] = {0};
				CFDataGetBytes(val, CFRangeMake(0, CFDataGetLength(val)), num);
				sprintf(pid, "%0.2x%0.2x", num[1], num[0]);
				strlcat(hardwareStr, ".", MT_STR_LEN);
				strlcat(hardwareStr, pid, MT_STR_LEN);
				CFRelease(val);
			}
			else {
				strlcat(hardwareStr, ".", MT_STR_LEN);
				strlcat(hardwareStr, "X", MT_STR_LEN);
			}
			
			CFRelease(key);
		}
		else if (CFEqual(baseName, CFSTR("Bluetooth-Modem"))) {
			/* We have a bluetooth modem */
			strlcat(hardwareStr, "Bluetooth-Modem", MT_STR_LEN);
		}
		
		if (properties != NULL) {
			CFRelease(properties);
		}
		break;
	}
	
	if (*hardwareStr != 0) {
		memset(pppSess->hardwareInfo, 0, MT_STR_LEN);
		strlcpy(pppSess->hardwareInfo, hardwareStr, MT_STR_LEN);
	}
	else {
		memset(pppSess->hardwareInfo, 0, MT_STR_LEN);
		strlcpy(pppSess->hardwareInfo, "No modem information", MT_STR_LEN);
	}
		
	if (networkServ != NULL) {
		CFRelease(networkServ);
	}
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_start(struct service *serv, CFDictionaryRef options, uid_t uid, gid_t gid, mach_port_t bootstrap, mach_port_t au_session, u_int8_t onTraffic, u_int8_t onDemand)
{
#define MAXARG 10
	char 			*cmdarg[MAXARG];
	u_int32_t		i, argi = 0;
	CFDictionaryRef		service;
	CFDictionaryRef		environmentVars = NULL;
	int yes = 1;
	
	// reset setup flag
	serv->flags &= ~FLAG_CONFIGCHANGEDNOW;
	serv->flags &= ~FLAG_CONFIGCHANGEDLATER;
	
	// reset autodial flag;
	serv->flags &= ~FLAG_FIRSTDIAL;
	
	switch (serv->u.ppp.phase) {
		case PPP_IDLE:
			break;
			
		case PPP_DORMANT:	// kill dormant process and post connection flag
		case PPP_HOLDOFF:
			my_CFRelease(&serv->u.ppp.newconnectopts);
			serv->u.ppp.newconnectopts = options;
			serv->u.ppp.newconnectuid = uid;
			serv->u.ppp.newconnectgid = gid;
			serv->u.ppp.newconnectbootstrap = bootstrap;
			serv->u.ppp.newconnectausession = au_session;
			my_CFRetain(serv->u.ppp.newconnectopts);
			
			scnc_stop(serv, 0, SIGTERM, SCNC_STOP_NONE);
			serv->flags |= FLAG_CONNECT;
			return 0;
			
		default:
			if (my_CFEqual(options, serv->connectopts)) {
				// notify client, so at least then can get the status if they were waiting got it
				phase_changed(serv, serv->u.ppp.phase);
				return 0;
			}
			return EIO;	// not the right time to dial
	}
	
#if DEBUG
	scnc_log(LOG_DEBUG, CFSTR("PPP Controller: VPN System Prefs %@"), serv->systemprefs);
	scnc_log(LOG_DEBUG, CFSTR("PPP Controller: VPN User Options %@"), options);
#endif
    
	/* remove any pending notification */
	if (serv->userNotificationRef) {
		CFUserNotificationCancel(serv->userNotificationRef);
		CFRunLoopRemoveSource(CFRunLoopGetCurrent(), serv->userNotificationRLS, kCFRunLoopDefaultMode);
		my_CFRelease(&serv->userNotificationRef);
		my_CFRelease(&serv->userNotificationRLS);			
	}
	
	serv->u.ppp.laststatus =  EXIT_FATAL_ERROR;
    serv->u.ppp.lastdevstatus = 0;

	if (serv->ne_sm_bridge != NULL) {
		service = ne_sm_bridge_copy_configuration(serv->ne_sm_bridge);
	} else {
		service = copyService(gDynamicStore, kSCDynamicStoreDomainSetup, serv->serviceID);
	}
    if (!service)
        goto end;	// that's bad...

    // create arguments and fork pppd 
    for (i = 0; i < MAXARG; i++) 
        cmdarg[i] = 0;
    addparam(cmdarg, &argi, PPPD_PRGM);
    addparam(cmdarg, &argi, "serviceid");
    addparam(cmdarg, &argi, (char*)serv->sid);
    addparam(cmdarg, &argi, "controlled");

    if ((socketpair(AF_LOCAL, SOCK_STREAM, 0, serv->u.ppp.controlfd) == -1) 
		|| (socketpair(AF_LOCAL, SOCK_STREAM, 0, serv->u.ppp.statusfd) == -1))
        goto end;

    if (setsockopt(serv->u.ppp.controlfd[WRITE], SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(yes)) == -1) {
        goto end;
    }

    if (onDemand)
        serv->flags |= FLAG_ONDEMAND;
	else
		serv->flags &= ~FLAG_ONDEMAND;

    serv->uid = uid;
    serv->gid = gid;

    scnc_bootstrap_retain(serv, bootstrap);
    scnc_ausession_retain(serv, au_session);
    
	if (serv->ne_sm_bridge != NULL) {
		CFDictionaryRef vars = CFDictionaryGetValue(service, CFSTR("EnvironmentVariables"));
		if (vars) {
			environmentVars = CFRetain(vars);
		}
	} else {
		environmentVars = collectEnvironmentVariables(gDynamicStore, serv->serviceID);
	}

    if (environmentVars) {
        extractEnvironmentVariables(environmentVars, serv);
        CFRelease(environmentVars);
    }
    
    serv->u.ppp.pid = SCNCPluginExecCommand2(NULL,
					     exec_callback, 
					     (void*)(uintptr_t)makeref(serv), 
					     geteuid(), 
					     getegid(), 
					     PATH_PPPD, 
					     cmdarg, 
					     exec_postfork, 
					     (void*)(uintptr_t)makeref(serv));
    if (serv->u.ppp.pid == -1)
        goto end;

    // send options to pppd using the pipe
    if (send_pppd_params(serv, service, options, onTraffic) == -1) {
        kill(serv->u.ppp.pid, SIGTERM);
        goto end;
    }
    
    // all options have been sent, close the pipe.
    //my_close(ppp->controlfd[WRITE]);
    //ppp->controlfd[WRITE] = -1;

    // add the pipe to runloop
    ppp_socket_create_client(serv->u.ppp.statusfd[READ], 1, 0, 0);

    serv->u.ppp.laststatus = EXIT_OK;
    ppp_updatephase(serv, PPP_INITIALIZE, 0);
	serv->was_running = 0;
	service_started(serv);

    if (onTraffic)
        serv->flags |= FLAG_ONTRAFFIC;
	else
		serv->flags &= ~FLAG_ONTRAFFIC;

    serv->connectopts = options;
    my_CFRetain(serv->connectopts);
    TRACK_VPN_LOCATION(serv);

end:
    
    if (service)
        CFRelease(service);

    for (i = 0; i < argi; i++)
        free(cmdarg[i]);

    if (serv->u.ppp.pid == -1) {
        
        my_close(serv->u.ppp.statusfd[READ]);
        serv->u.ppp.statusfd[READ] = -1;
        my_close(serv->u.ppp.statusfd[WRITE]);
        serv->u.ppp.statusfd[WRITE] = -1;
        my_close(serv->u.ppp.controlfd[READ]);
        serv->u.ppp.controlfd[READ] = -1;
        my_close(serv->u.ppp.controlfd[WRITE]);
        serv->u.ppp.controlfd[WRITE] = -1;
        
        display_error(serv);
    }

    return serv->u.ppp.laststatus;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void
exec_postfork(pid_t pid, void *arg)
{
    struct service 	*serv = findbyref(TYPE_PPP, (u_int32_t)(uintptr_t)arg);

    if (pid) {
        /* if parent */

        int	yes	= 1;

        my_close(serv->u.ppp.controlfd[READ]);
        serv->u.ppp.controlfd[READ] = -1;
        my_close(serv->u.ppp.statusfd[WRITE]);
        serv->u.ppp.statusfd[WRITE] = -1;
        if (ioctl(serv->u.ppp.controlfd[WRITE], FIONBIO, &yes) == -1) {
//           printf("ioctl(,FIONBIO,): %s\n", strerror(errno));
        }

    } else {
        /* if child */

        uid_t	euid;
        int	i;

        my_close(serv->u.ppp.controlfd[WRITE]);
        serv->u.ppp.controlfd[WRITE] = -1;
        my_close(serv->u.ppp.statusfd[READ]);
        serv->u.ppp.statusfd[READ] = -1;

        if (serv->u.ppp.controlfd[READ] != STDIN_FILENO) {
            dup2(serv->u.ppp.controlfd[READ], STDIN_FILENO);
        }

        if (serv->u.ppp.statusfd[WRITE] != STDOUT_FILENO) {
            dup2(serv->u.ppp.statusfd[WRITE], STDOUT_FILENO);
        }

        close(STDERR_FILENO);
        open(_PATH_DEVNULL, O_RDWR, 0);

        /* close any other open FDs */
        for (i = getdtablesize() - 1; i > STDERR_FILENO; i--) close(i);

        /* Careful here and with the gid/uid params passed to _SCDPluginExecCommand2()
         * so that the uid/gid setup code in _SCDPluginExecCommand would be skipped
         * after this callback function returns. We want this function to be in
         * full control of UID and GID settings.
         */
        euid = geteuid();
        if (euid != serv->uid) {
            // only set real (and not effective) UID for pppd which needs to have
            // its euid to be root so as to toggle between root and user to access
            // system and user keychain (e.g., SSL certificate private key in system
            // keychain and user password in user keychain).
            (void) setruid(serv->uid);
        }

        applyEnvironmentVariables(serv);
    }

    return;
}

static Boolean
ppp_persist_connection_exec_callback (struct service *serv, int exitcode)
{
    Boolean pppRestart = FALSE;
    
#if TARGET_OS_OSX
	if (serv->persist_connect) {
		if (serv->persist_connect_status ||
			serv->persist_connect_devstatus ||
			((serv->u.ppp.laststatus && serv->u.ppp.laststatus != EXIT_USER_REQUEST && serv->u.ppp.laststatus != EXIT_FATAL_ERROR) || serv->u.ppp.lastdevstatus) ||
			((exitcode == EXIT_HANGUP || exitcode == EXIT_PEER_DEAD) && serv->u.ppp.laststatus != EXIT_USER_REQUEST && serv->u.ppp.laststatus != EXIT_FATAL_ERROR)) {

			scnc_log(LOG_ERR, CFSTR("PPP Controller: disconnected with status  %d.%d. Will try reconnect shortly."),
				  serv->persist_connect_status? serv->persist_connect_status: serv->u.ppp.laststatus,
				  serv->persist_connect_devstatus? serv->persist_connect_devstatus : serv->u.ppp.lastdevstatus);

			scnc_log(LOG_ERR, CFSTR("PPP Controller: reconnecting"));
			// start over
			serv->connecttime = 0;
			serv->establishtime = 0;
			my_CFRelease(&serv->connection_nid);
			my_CFRelease(&serv->connection_nap);
			STOP_TRACKING_VPN_LOCATION(serv);
			serv->u.ppp.laststatus = 0;
			serv->u.ppp.lastdevstatus = 0;
			pppRestart = TRUE;
			ppp_start(serv, serv->persist_connect_opts, serv->uid, serv->gid, serv->bootstrap, serv->au_session, 0, (serv->flags & FLAG_ONDEMAND));
		} else if (serv->u.ppp.laststatus != EXIT_USER_REQUEST && serv->u.ppp.laststatus != EXIT_FATAL_ERROR) {
			serv->flags |= FLAG_ALERTERRORS;
			display_error(serv);
			serv->u.ppp.laststatus = 0;
			serv->u.ppp.lastdevstatus = 0;
		}
		my_CFRelease(&serv->persist_connect_opts);
		serv->persist_connect = 0;
		serv->persist_connect_status = 0;
		serv->persist_connect_devstatus = 0;
	}
#endif
    return pppRestart;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static 
void exec_callback(pid_t pid, int status, struct rusage *rusage, void *context)
{
    struct service 	*serv = findbyref(TYPE_PPP, (u_int32_t)(uintptr_t)context);
    Boolean pppRestart = FALSE;
 
	if (serv == NULL)
		return;

	u_int32_t	failed = 0;
	int exitcode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
	
	if (exitcode < 0) {
		// ignore this case, just change phase
	}
	else if (exitcode > EXIT_PEER_UNREACHABLE) {
		// pppd exited because of a crash
		ppp_updatestatus(serv, 127, 0);
	}
	else if (serv->u.ppp.phase == PPP_INITIALIZE) {
        // an error occured and status has not been updated
        // happens for example when an error is encountered while parsing arguments
		failed = 1;
        ppp_updatestatus(serv, exitcode, 0);
    }

    // call the change phae function
    ppp_updatephase(serv, PPP_IDLE, 0);
	serv->was_running = 0;
	service_ended(serv);

    // close file descriptors
    //statusfd is closed by the run loop
    //my_close(ppp->statusfd[READ]);
    serv->u.ppp.statusfd[READ] = -1;
    my_close(serv->u.ppp.controlfd[WRITE]);
    serv->u.ppp.controlfd[WRITE] = -1;
    my_CFRelease(&serv->connectopts);
    serv->connectopts = 0;

	/* clean up dynamic store */
	cleanup_dynamicstore((void*)serv);

	if (serv->ne_sm_bridge != NULL) {
		ne_sm_bridge_acknowledge_sleep(serv->ne_sm_bridge);
	} else {
		allow_sleep();
	}

	if (allow_dispose(serv))
		serv = 0;

    if (serv == 0)
        return;
        
    // check if configd is going away
    if (allow_stop())
		return;
	
    // now reconnect if necessary    
	
    if (serv->flags & FLAG_CONNECT) {        
        pppRestart = TRUE;
        ppp_start(serv, serv->u.ppp.newconnectopts, serv->u.ppp.newconnectuid, serv->u.ppp.newconnectgid, serv->u.ppp.newconnectbootstrap, serv->u.ppp.newconnectausession, 0, 0);
        my_CFRelease(&serv->u.ppp.newconnectopts);
        serv->u.ppp.newconnectopts = 0;
        serv->u.ppp.newconnectuid = 0;
        serv->u.ppp.newconnectgid = 0;
        serv->u.ppp.newconnectbootstrap = 0;
        serv->u.ppp.newconnectausession = 0;
        serv->flags &= ~FLAG_CONNECT;
    }
    else {
        // if config has changed, or ppp was previously a manual connection, then rearm onTraffic if necessary
        if (failed == 0
	    && ((serv->flags & (FLAG_CONFIGCHANGEDNOW + FLAG_CONFIGCHANGEDLATER)) || !(serv->flags & FLAG_ONTRAFFIC))
	    && ((serv->flags & FLAG_SETUP_ONTRAFFIC) && (!(serv->flags & FLAG_SETUP_DISCONNECTONLOGOUT)|| gLoggedInUser))) {
            pppRestart = TRUE;
            ppp_start(serv, 0, 0, 0, serv->bootstrap, serv->au_session, serv->flags & FLAG_CONFIGCHANGEDNOW ? 1 : 3, 0);
       } else {
            pppRestart = ppp_persist_connection_exec_callback(serv, exitcode);
       }
    }

    if (!pppRestart) {
        scnc_bootstrap_dealloc(serv);
        scnc_ausession_dealloc(serv);
    }
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_stop(struct service *serv, int signal)
{
    
	/* 
		signal is either SIGHUP or SIGTERM 
		SIGHUP will only disconnect the link
		SIGTERM will terminate pppd
	*/
	if (serv->flags & (FLAG_CONFIGCHANGEDNOW + FLAG_CONFIGCHANGEDLATER))
		signal = SIGTERM;
	
    // anticipate next phase
    switch (serv->u.ppp.phase) {
    
        case PPP_IDLE:
            return 0;

        case PPP_DORMANT:
			/* SIGHUP has no effect on effect on DORMANT phase */
			if (signal == SIGHUP)
				return 0;
            break;

        case PPP_HOLDOFF:
			/* we don't want SIGHUP to stop the HOLDOFF phase */
			if (signal == SIGHUP)
				return 0;
            break;
            
        case PPP_DISCONNECTLINK:
        case PPP_TERMINATE:
            break;
            
        case PPP_INITIALIZE:
            serv->flags &= ~FLAG_CONNECT;
            // no break;
            
        case PPP_CONNECTLINK:
            ppp_updatephase(serv, PPP_DISCONNECTLINK, 0);
            break;
        
        default:
            ppp_updatephase(serv, PPP_TERMINATE, 0);
    }

    if (serv->u.ppp.controlfd[WRITE] != -1){
        if (signal == SIGTERM)
            writeparam(serv->u.ppp.controlfd[WRITE], "[TERMINATE]");
        else if (signal == SIGHUP)
            writeparam(serv->u.ppp.controlfd[WRITE], "[DISCONNECT]");
        else {
            kill(serv->u.ppp.pid, signal);
        }
    }else
        kill(serv->u.ppp.pid, signal);

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_suspend(struct service *serv)
{

    if (serv->u.ppp.phase != PPP_IDLE)
        kill(serv->u.ppp.pid, SIGTSTP);

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_resume(struct service *serv)
{
    if (serv->u.ppp.phase != PPP_IDLE)
        kill(serv->u.ppp.pid, SIGCONT);

    return 0;
}

/* -----------------------------------------------------------------------------
 detects disconnects caused by recoverable errors and flags the connection for 
 auto reconnect (i.e. persistence) and avoid UI dialog
 ----------------------------------------------------------------------------- */
static int
ppp_check_status_for_disconnect_by_recoverable_error (struct service *serv, int status, int devstatus)
{
#if TARGET_OS_OSX
    /* try to catch a disconnection early, avoid displaying dialog and flag for reconnection */
    if ((serv->subtype == PPP_TYPE_L2TP) &&
		(serv->u.ppp.phase == PPP_RUNNING || (serv->was_running && serv->u.ppp.phase == PPP_WAITING)) &&
		!serv->u.ppp.laststatus &&
		!serv->u.ppp.lastdevstatus &&
		(status  && status != EXIT_USER_REQUEST)) {
        if (!serv->persist_connect &&
            (serv->flags & (FLAG_FREE | FLAG_ONTRAFFIC | FLAG_ONDEMAND | FLAG_CONNECT | FLAG_SETUP_PERSISTCONNECTION)) == FLAG_SETUP_PERSISTCONNECTION) {
            // prevent error dialog from popping up during this disconnect
            serv->flags &= ~FLAG_ALERTERRORS;
            serv->persist_connect_opts = serv->connectopts;
            serv->connectopts = NULL;
            serv->persist_connect = 1;
            serv->persist_connect_status = status;
            serv->persist_connect_devstatus = devstatus;
			scnc_log(LOG_INFO, CFSTR("PPP Controller: status-checked, preparing for persistence status  %d.%d."),
				  serv->persist_connect_status,
				  serv->persist_connect_devstatus);
            return TRUE;
        }
	}
#endif
    return FALSE;
}

/* -----------------------------------------------------------------------------
status change for this ppp occured
----------------------------------------------------------------------------- */
void ppp_updatestatus(struct service *serv, int status, int devstatus)
{

    (void)ppp_check_status_for_disconnect_by_recoverable_error(serv,status,devstatus);
    serv->u.ppp.laststatus = status;
    serv->u.ppp.lastdevstatus = devstatus;

    display_error(serv);
}


/* -----------------------------------------------------------------------------
 detects disconnects caused by recoverable errors and flags the connection for 
 auto reconnect (i.e. persistence) and avoid UI dialog
 ----------------------------------------------------------------------------- */
static int
ppp_check_phase_for_disconnect_by_recoverable_error (struct service *serv, int phase)
{
#if TARGET_OS_OSX
    /* try to catch a disconnection early, avoid displaying dialog and flag for reconnection */
    if (serv->was_running) {
        if (!serv->persist_connect &&
            (serv->flags & (FLAG_FREE | FLAG_ONTRAFFIC | FLAG_ONDEMAND | FLAG_CONNECT | FLAG_SETUP_PERSISTCONNECTION)) == FLAG_SETUP_PERSISTCONNECTION &&
            (serv->subtype == PPP_TYPE_L2TP)) {
            // prevent error dialog from popping up during this disconnect
            serv->flags &= ~FLAG_ALERTERRORS;
            serv->persist_connect_opts = serv->connectopts;
            serv->connectopts = NULL;
            serv->persist_connect = 1;
            if (serv->u.ppp.laststatus && serv->u.ppp.laststatus != EXIT_USER_REQUEST && serv->u.ppp.laststatus != EXIT_FATAL_ERROR) {
                serv->persist_connect_status = serv->u.ppp.laststatus;
            } else {
                serv->persist_connect_status = 0;
            }
            if (serv->u.ppp.lastdevstatus) {
				serv->persist_connect_devstatus = serv->u.ppp.lastdevstatus;
            } else {
				serv->persist_connect_devstatus = 0;
            }
			scnc_log(LOG_INFO, CFSTR("PPP Controller: phase-checked, preparing for persistence status  %d.%d."),
				  serv->persist_connect_status,
				  serv->persist_connect_devstatus);
			return TRUE;
        }
	}
#endif
	return FALSE;
}

/* -----------------------------------------------------------------------------
 detects location changes that require disconnection.
 returns true if 
 ----------------------------------------------------------------------------- */
static int
ppp_disconnect_if_location_changed (struct service *serv, int phase)
{
#if TARGET_OS_OSX
	if (serv->was_running && (phase == PPP_WAITING || phase == PPP_RUNNING) && (serv->subtype == PPP_TYPE_L2TP)) {
		if (DISCONNECT_VPN_IFLOCATIONCHANGED(serv)) {
			scnc_log(LOG_NOTICE, CFSTR("PPP Controller: the underlying interface has changed networks."));
			return TRUE;
		}
	}
#endif
	return FALSE;
}

/* -----------------------------------------------------------------------------
phase change for this ppp occured
----------------------------------------------------------------------------- */
void ppp_updatephase(struct service *serv, int phase, int ifunit)
{

    /* check if update is received pppd has  exited */
    if (serv->u.ppp.statusfd[READ] == -1) {
        serv->u.ppp.phase = PPP_IDLE;
        phase_changed(serv, serv->u.ppp.phase);
        return;
    }

    /* check for new phase */
    if (phase == serv->u.ppp.phase)
        return;

	/* special-case disconnect transitions? */
    if (ppp_check_phase_for_disconnect_by_recoverable_error(serv, phase) == FALSE) {
		if (ppp_disconnect_if_location_changed(serv, phase) == TRUE) {
			return;
		}
	}
    
    serv->u.ppp.phase = phase;
    phase_changed(serv, phase);
    /*
     * Don't publish ppp status here in the controller because we have pppd that's also
     * publishing the PPP dictionary, causing a race condition. It can happen that the controller
     * has not received the pppd update at this point, and thus is to write only a status
     * value to the dynamic store, but by the time the write takes place, pppd just updated
     * the store with more info which would get wiped out by the write from the controller.
     *
     * Instead, we'll rely on ppp status udpates from pppd.
     *
    publish_dictnumentry(gDynamicStore, serv->serviceID, kSCEntNetPPP, kSCPropNetPPPStatus, phase);
     */

    switch (serv->u.ppp.phase) {
        case PPP_INITIALIZE:
            serv->connecttime = mach_absolute_time() * gTimeScaleSeconds;
            serv->connectionslepttime = 0;
            break;

        case PPP_RUNNING:
            serv->if_name[0] = 0;
            snprintf(serv->if_name, sizeof(serv->if_name), "ppp%d", ifunit);

            serv->was_running = 1;
	    if (serv->establishtime == 0) {
		    serv->establishtime = mach_absolute_time() * gTimeScaleSeconds;
	    }
            break;
            
        case PPP_DORMANT:
            serv->if_name[0] = 0;
            getStringFromEntity(gDynamicStore, kSCDynamicStoreDomainState, serv->serviceID, 
                    kSCEntNetPPP, kSCPropInterfaceName, (u_char *)serv->if_name, sizeof(serv->if_name));
            // no break;

        case PPP_HOLDOFF:

            /* check if setup has changed */
            if (serv->flags & FLAG_CONFIGCHANGEDLATER)
                scnc_stop(serv, 0, SIGTERM, SCNC_STOP_NONE);
            break;
    }
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_is_pid(struct service *serv, int pid)
{
	return (serv->u.ppp.pid == pid);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
SCNetworkConnectionStatus ppp_getstatus(struct service *serv)
{
	SCNetworkConnectionStatus	status = kSCNetworkConnectionDisconnected;

	switch (serv->u.ppp.phase) {
		case PPP_INITIALIZE:
		case PPP_CONNECTLINK:
		case PPP_ESTABLISH:
		case PPP_AUTHENTICATE:
		case PPP_CALLBACK:
		case PPP_NETWORK:
		case PPP_WAITONBUSY:
			status = kSCNetworkConnectionConnecting;
			break;
		case PPP_TERMINATE:
		case PPP_DISCONNECTLINK:
		case PPP_WAITING:
			status = kSCNetworkConnectionDisconnecting;
			break;
		case PPP_RUNNING:
		case PPP_ONHOLD:
			status = kSCNetworkConnectionConnected;
			break;
		case PPP_IDLE:
		case PPP_DORMANT:
		case PPP_HOLDOFF:
		default:
			status = kSCNetworkConnectionDisconnected;
	}
	
	return status;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_getstatus1(struct service *serv, void **reply, u_int16_t *replylen)
{
    struct ppp_status 		*stat;
    struct ifpppstatsreq 	rq;
    int		 		s;
    u_int32_t			retrytime, conntime, disconntime, curtime;

    *reply = my_Allocate(sizeof(struct ppp_status));
    if (*reply == 0) {
        return ENOMEM;
    }
    stat = (struct ppp_status *)*reply;

    bzero (stat, sizeof (struct ppp_status));
    switch (serv->u.ppp.phase) {
        case PPP_DORMANT:
        case PPP_HOLDOFF:
            stat->status = PPP_IDLE;		// Dial on demand does not exist in the api
            break;
        default:
            stat->status = serv->u.ppp.phase;
    }

    switch (stat->status) {
        case PPP_RUNNING:
        case PPP_ONHOLD:

            s = socket(AF_INET, SOCK_DGRAM, 0);
            if (s < 0) {
            	my_Deallocate(*reply, sizeof(struct ppp_status));
                return errno;
            }
    
            bzero (&rq, sizeof (rq));
    
            strncpy(rq.ifr_name, (char*)serv->if_name, IFNAMSIZ);
            if (ioctl(s, SIOCGPPPSTATS, &rq) < 0) {
                close(s);
            	my_Deallocate(*reply, sizeof(struct ppp_status));
                return errno;
            }
    
            close(s);

            conntime = 0;
            getNumberFromEntity(gDynamicStore, kSCDynamicStoreDomainState, serv->serviceID, 
                kSCEntNetPPP, kSCPropNetPPPConnectTime, &conntime);
            disconntime = 0;
            getNumberFromEntity(gDynamicStore, kSCDynamicStoreDomainState, serv->serviceID, 
                kSCEntNetPPP, kSCPropNetPPPDisconnectTime, &disconntime);

            curtime = mach_absolute_time() * gTimeScaleSeconds;
            if (conntime)
				stat->s.run.timeElapsed = curtime - conntime;
            if (!disconntime)	// no limit...
     	       stat->s.run.timeRemaining = 0xFFFFFFFF;
            else
      	      stat->s.run.timeRemaining = (disconntime > curtime) ? disconntime - curtime : 0;

            stat->s.run.outBytes = rq.stats.p.ppp_obytes;
            stat->s.run.inBytes = rq.stats.p.ppp_ibytes;
            stat->s.run.inPackets = rq.stats.p.ppp_ipackets;
            stat->s.run.outPackets = rq.stats.p.ppp_opackets;
            stat->s.run.inErrors = rq.stats.p.ppp_ierrors;
            stat->s.run.outErrors = rq.stats.p.ppp_ierrors;
            break;
            
        case PPP_WAITONBUSY:
        
            stat->s.busy.timeRemaining = 0;
            retrytime = 0;
            getNumberFromEntity(gDynamicStore, kSCDynamicStoreDomainState, serv->serviceID, 
                kSCEntNetPPP, kSCPropNetPPPRetryConnectTime, &retrytime);
            if (retrytime) {
                curtime = mach_absolute_time() * gTimeScaleSeconds;
                stat->s.busy.timeRemaining = (curtime < retrytime) ? retrytime - curtime : 0;
            }
            break;
         
        default:
            stat->s.disc.lastDiscCause = ppp_translate_error(serv->subtype, serv->u.ppp.laststatus, serv->u.ppp.lastdevstatus);
    }

    *replylen = sizeof(struct ppp_status);
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_copyextendedstatus(struct service *serv, CFDictionaryRef *statusdict)
{
    CFMutableDictionaryRef dict = NULL;
	CFMutableDictionaryRef pppdict = NULL;
	int error = 0;

	*statusdict = NULL;
    
    if ((dict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)) == 0) {
		error = ENOMEM;
        goto fail;
	}

    /* create and add PPP dictionary */
    if ((pppdict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)) == 0) {
		error = ENOMEM;
        goto fail;
	}
    
    AddNumber(pppdict, kSCPropNetPPPStatus, serv->u.ppp.phase);
    
    if (serv->u.ppp.phase != PPP_IDLE)
        AddStringFromState(gDynamicStore, serv->serviceID, kSCEntNetPPP, kSCPropNetPPPCommRemoteAddress, pppdict);

    switch (serv->u.ppp.phase) {
        case PPP_RUNNING:
        case PPP_ONHOLD:

			if (serv->ne_sm_bridge != NULL) {
				AddNumber64(pppdict, kSCPropNetPPPConnectTime, ne_sm_bridge_get_connect_time(serv->ne_sm_bridge));
			} else {
				AddNumberFromState(gDynamicStore, serv->serviceID, kSCEntNetPPP, kSCPropNetPPPConnectTime, pppdict);
			}
            AddNumberFromState(gDynamicStore, serv->serviceID, kSCEntNetPPP, kSCPropNetPPPDisconnectTime, pppdict);
            AddNumberFromState(gDynamicStore, serv->serviceID, kSCEntNetPPP, kSCPropNetPPPLCPCompressionPField, pppdict);
            AddNumberFromState(gDynamicStore, serv->serviceID, kSCEntNetPPP, kSCPropNetPPPLCPCompressionACField, pppdict);
            AddNumberFromState(gDynamicStore, serv->serviceID, kSCEntNetPPP, kSCPropNetPPPLCPMRU, pppdict);
            AddNumberFromState(gDynamicStore, serv->serviceID, kSCEntNetPPP, kSCPropNetPPPLCPMTU, pppdict);
            AddNumberFromState(gDynamicStore, serv->serviceID, kSCEntNetPPP, kSCPropNetPPPLCPReceiveACCM, pppdict);
            AddNumberFromState(gDynamicStore, serv->serviceID, kSCEntNetPPP, kSCPropNetPPPLCPTransmitACCM, pppdict);
            AddNumberFromState(gDynamicStore, serv->serviceID, kSCEntNetPPP, kSCPropNetPPPIPCPCompressionVJ, pppdict);
            break;
            
        case PPP_WAITONBUSY:
			if (serv->ne_sm_bridge != NULL) {
				AddNumber64(pppdict, kSCPropNetPPPConnectTime, ne_sm_bridge_get_connect_time(serv->ne_sm_bridge));
			} else {
				AddNumberFromState(gDynamicStore, serv->serviceID, kSCEntNetPPP, kSCPropNetPPPRetryConnectTime, pppdict);
			}
            break;
         
        case PPP_DORMANT:
            break;
            
        default:
            AddNumber(pppdict, kSCPropNetPPPLastCause, serv->u.ppp.laststatus);
            AddNumber(pppdict, kSCPropNetPPPDeviceLastCause, serv->u.ppp.lastdevstatus);
    }

    CFDictionaryAddValue(dict, kSCEntNetPPP, pppdict);
    my_CFRelease(&pppdict);

    /* create and add Modem dictionary */
    if (serv->subtype == PPP_TYPE_SERIAL
        && (serv->u.ppp.phase == PPP_RUNNING || serv->u.ppp.phase == PPP_ONHOLD)) {
        if ((pppdict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)) == 0) {
			error = ENOMEM;
            goto fail;
		}

        AddNumberFromState(gDynamicStore, serv->serviceID, kSCEntNetModem, kSCPropNetModemConnectSpeed, pppdict);
            
        CFDictionaryAddValue(dict, kSCEntNetModem, pppdict);
        my_CFRelease(&pppdict);
    }

    /* create and add IPv4 dictionary */
    if (serv->u.ppp.phase == PPP_RUNNING || serv->u.ppp.phase == PPP_ONHOLD) {
        pppdict = (CFMutableDictionaryRef)copyEntity(gDynamicStore, kSCDynamicStoreDomainState, serv->serviceID, kSCEntNetIPv4);
        if (pppdict) {
            CFDictionaryAddValue(dict, kSCEntNetIPv4, pppdict);
            my_CFRelease(&pppdict);
        }
    }
    
    AddNumber(dict, kSCNetworkConnectionStatus, ppp_getstatus(serv));

	*statusdict = CFRetain(dict);
    
fail:
	my_CFRelease(&pppdict);
	my_CFRelease(&dict);

    return error;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_copystatistics(struct service *serv, CFDictionaryRef *statsdict)
{
    CFMutableDictionaryRef dict = NULL;
    CFMutableDictionaryRef pppdict = NULL;
    int s = -1;
    struct ifpppstatsreq rq;
	int	error = 0;

	*statsdict = NULL;
	
	if (serv->u.ppp.phase != PPP_RUNNING
		&& serv->u.ppp.phase != PPP_ONHOLD)
			return EINVAL;
			
    if ((dict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)) == 0) {
		error = ENOMEM;
		goto fail;
	}

    /* create and add PPP dictionary */
    if ((pppdict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)) == 0) {
		error = ENOMEM;
		goto fail;
	}
    
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		error = errno;
        goto fail;
	}

	bzero (&rq, sizeof (rq));

	strncpy(rq.ifr_name, (char*)serv->if_name, IFNAMSIZ);
	if (ioctl(s, SIOCGPPPSTATS, &rq) < 0) {
		error = errno;
        goto fail;
	}

	AddNumber(pppdict, kSCNetworkConnectionBytesIn, rq.stats.p.ppp_ibytes);
	AddNumber(pppdict, kSCNetworkConnectionBytesOut, rq.stats.p.ppp_obytes);
	AddNumber(pppdict, kSCNetworkConnectionPacketsIn, rq.stats.p.ppp_ipackets);
	AddNumber(pppdict, kSCNetworkConnectionPacketsOut, rq.stats.p.ppp_opackets);
	AddNumber(pppdict, kSCNetworkConnectionErrorsIn, rq.stats.p.ppp_ierrors);
	AddNumber(pppdict, kSCNetworkConnectionErrorsOut, rq.stats.p.ppp_ierrors);

    CFDictionaryAddValue(dict, kSCEntNetPPP, pppdict);

	*statsdict = CFRetain(dict);

fail:
    my_CFRelease(&pppdict);
    my_CFRelease(&dict);
	if (s != -1) {
		close(s);
	}
    return error;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_getconnectsystemdata(struct service *serv, void **reply, u_int16_t *replylen)
{
	CFDictionaryRef	service = NULL;
    CFDataRef			dataref = NULL;
    void			*dataptr = 0;
    u_int32_t			datalen = 0;
	int				err = 0;

	service = copyService(gDynamicStore, kSCDynamicStoreDomainSetup, serv->serviceID);
    if (service == 0) {
        // no data
        *replylen = 0;
        return 0;
    }
    
    if ((dataref = Serialize(service, &dataptr, &datalen)) == 0) {
		err = ENOMEM;
		goto end;
    }
    
    *reply = my_Allocate(datalen);
    if (*reply == 0) {
		err = ENOMEM;
		goto end;
    }
    else {
        bcopy(dataptr, *reply, datalen);
        *replylen = datalen;
    }

end:
    if (service)
		CFRelease(service);
    if (dataref)
		CFRelease(dataref);
    return err;
 }

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_getconnectdata(struct service *serv, CFDictionaryRef *options, int all)
{
    CFDictionaryRef		opts;
    CFMutableDictionaryRef	mdict = NULL;
    CFDictionaryRef	dict;
	int err = 0;

	*options = NULL;
        
    /* return saved data */
    opts = serv->connectopts;

    if (opts == 0) {
        // no data
        return 0;
    }
    
	if (!all) {
		/* special code to remove secret information */
		CFMutableDictionaryRef mdict1 = NULL;

		mdict = CFDictionaryCreateMutableCopy(0, 0, opts);
		if (mdict == 0) {
			// no data
			return 0;
		}
		
		dict = CFDictionaryGetValue(mdict, kSCEntNetPPP);
		if (dict && (CFGetTypeID(dict) == CFDictionaryGetTypeID())) {
			mdict1 = CFDictionaryCreateMutableCopy(0, 0, dict);
			if (mdict1) {
				CFDictionaryRemoveValue(mdict1, kSCPropNetPPPAuthPassword);
				CFDictionarySetValue(mdict, kSCEntNetPPP, mdict1);
				CFRelease(mdict1);
			}
		}

		dict = CFDictionaryGetValue(mdict, kSCEntNetL2TP);
		if (dict && (CFGetTypeID(dict) == CFDictionaryGetTypeID())) {
			mdict1 = CFDictionaryCreateMutableCopy(0, 0, dict);
			if (mdict1) {
				CFDictionaryRemoveValue(mdict1, kSCPropNetL2TPIPSecSharedSecret);
				CFDictionarySetValue(mdict, kSCEntNetL2TP, mdict1);
				CFRelease(mdict1);
			}
		}

		dict = CFDictionaryGetValue(mdict, kSCEntNetIPSec);
		if (dict && (CFGetTypeID(dict) == CFDictionaryGetTypeID())) {
			mdict1 = CFDictionaryCreateMutableCopy(0, 0, dict);
			if (mdict1) {
				CFDictionaryRemoveValue(mdict1, kSCPropNetIPSecSharedSecret);
				CFDictionarySetValue(mdict, kSCEntNetIPSec, mdict1);
				CFRelease(mdict1);
			}
		}
		*options = CFRetain(mdict);
	} else {
		*options = CFRetain(opts);
	}

    if (mdict)
		CFRelease(mdict);
    return err;
}

/* -----------------------------------------------------------------------------
translate a pppd native cause into a PPP API cause
----------------------------------------------------------------------------- */
u_int32_t ppp_translate_error(u_int16_t subtype, u_int32_t native_ppp_error, u_int32_t native_dev_error)
{
    u_int32_t	error = PPP_ERR_GEN_ERROR; 
    
    switch (native_ppp_error) {
        case EXIT_USER_REQUEST:
            error = 0;
            break;
        case EXIT_CONNECT_FAILED:
            error = PPP_ERR_GEN_ERROR;
            break;
        case EXIT_TERMINAL_FAILED:
            error = PPP_ERR_TERMSCRIPTFAILED;
            break;
        case EXIT_NEGOTIATION_FAILED:
            error = PPP_ERR_LCPFAILED;
            break;
        case EXIT_AUTH_TOPEER_FAILED:
            error = PPP_ERR_AUTHFAILED;
            break;
        case EXIT_IDLE_TIMEOUT:
            error = PPP_ERR_IDLETIMEOUT;
            break;
        case EXIT_CONNECT_TIME:
            error = PPP_ERR_SESSIONTIMEOUT;
            break;
        case EXIT_LOOPBACK:
            error = PPP_ERR_LOOPBACK;
            break;
        case EXIT_PEER_DEAD:
            error = PPP_ERR_PEERDEAD;
            break;
        case EXIT_OK:
            error = PPP_ERR_DISCBYPEER;
            break;
        case EXIT_HANGUP:
            error = PPP_ERR_DISCBYDEVICE;
            break;
    }
    
    // override with a more specific error
    if (native_dev_error) {
        switch (subtype) {
            case PPP_TYPE_SERIAL:
                switch (native_dev_error) {
                    case EXIT_PPPSERIAL_NOCARRIER:
                        error = PPP_ERR_MOD_NOCARRIER;
                        break;
                    case EXIT_PPPSERIAL_NONUMBER:
                        error = PPP_ERR_MOD_NONUMBER;
                        break;
                    case EXIT_PPPSERIAL_BADSCRIPT:
                        error = PPP_ERR_MOD_BADSCRIPT;
                        break;
                    case EXIT_PPPSERIAL_BUSY:
                        error = PPP_ERR_MOD_BUSY;
                        break;
                    case EXIT_PPPSERIAL_NODIALTONE:
                        error = PPP_ERR_MOD_NODIALTONE;
                        break;
                    case EXIT_PPPSERIAL_ERROR:
                        error = PPP_ERR_MOD_ERROR;
                        break;
                    case EXIT_PPPSERIAL_NOANSWER:
                        error = PPP_ERR_MOD_NOANSWER;
                        break;
                    case EXIT_PPPSERIAL_HANGUP:
                        error = PPP_ERR_MOD_HANGUP;
                        break;
                    default :
                        error = PPP_ERR_CONNSCRIPTFAILED;
                }
                break;
    
            case PPP_TYPE_PPPoE:
                // need to handle PPPoE specific error codes
                break;
        }
    }
    
    return error;
}

