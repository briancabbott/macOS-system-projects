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


/* --------------------------------------------------------------------------
Includes 
-------------------------------------------------------------------------- */

#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <stdarg.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOCFUnserialize.h>
#include <SystemConfiguration/SystemConfiguration.h>

#include "CCLArgFunctions.h"
#include "cclkeys.h"

// import System Configuration schema for keys that are prefs
#include <SystemConfiguration/SCSchemaDefinitions.h>

#include "CCLEngine.h"
#include "CCLEngine_defs.h"
#include <AssertMacros.h>

/* --------------------------------------------------------------------------
Defines
-------------------------------------------------------------------------- */

#define min(a,b)			(((a)<=(b))?(a):(b))
#define kNormalDialMode     0
#define kBlindDialMode      1       // CCL term; SC uses is "IgnoreDialTone"
#define kManualDialMode     2       // user does the dialing

#define DIR_MODEMS_USER     "/Library/Modem Scripts"
#define DIR_MODEMS_SYS      "/System/Library/Modem Scripts"


enum {
    kUserMsgFLog = 1,
    kUserMsgFStatus = 2
};

#define infd 	STDIN_FILENO
#define outfd 	STDOUT_FILENO
#define PPP_ARG_FD	3

enum
{
    kInvalidTimer = 0,
    kMatchReadTimer,
    kPauseTimer,
    kCharDelayTimer,
    kBreakTimer
};

enum enginemode {
    mode_connect = 0,
    mode_disconnect,
    mode_listen
};

struct	callout {
    struct timeval	c_time;		/* time at which to call routine */
    void		*c_arg;		/* argument to routine */
    void		(*c_func)(void *); /* routine */
    struct		callout *c_next;
};

/* --------------------------------------------------------------------------
Globals
-------------------------------------------------------------------------- */

u_int8_t gNullString[2];

char	*Commands[] =
{
    "!\0",			/*  1 */
    "@CCLSCRIPT\0",		/*  2 */
    "@ORIGINATE\0",		/*  3 */
    "@ANSWER\0",		/*  4 */
    "@HANGUP\0",		/*  5 */
    "@LABEL\0",			/*  6 */
    "ASK\0",			/*  7 */
    "CHRDELAY\0",		/*  8 */
    "COMMUNICATINGAT\0",	/*  9 */
    "DECTRIES\0",		/* 10 */
    "DTRSET\0",			/* 11 */
    "DTRCLEAR",			/* 12 */
    "EXIT",			/* 13 */
    "FLUSH",			/* 14 */
    "HSRESET",			/* 15 */
    "IFANSWER",			/* 16 */
    "IFORIGINATE",		/* 17 */
    "IFSTR",			/* 18 */
    "IFTRIES",			/* 19 */
    "INCTRIES",			/* 20 */
    "JUMP",			/* 21 */
    "JSR",			/* 22 */
    "LBREAK",			/* 23 */
    "LOGMSG",			/* 24 */		// undocumented.  use cNote.  KW
    "MATCHCLR",			/* 25 */
    "MATCHREAD",		/* 26 */
    "MATCHSTR",			/* 27 */
    "NOTE",			/* 28 */
    "PAUSE",			/* 29 */
    "RETURN",			/* 30 */
    "SBREAK",			/* 31 */
    "SERRESET",			/* 32 */
    "SETSPEED",			/* 33 */
    "SETTRIES",			/* 34 */
    "USERHOOK",			/* 35 */
    "WRITE",			/* 36 */
    "MONITORLINE",		/* 37 */
    "DEBUGLEVEL"		/* 38 */
};


TRScriptVars	SV;			// script variables structure
u_int8_t	*VarStrings[vsMax+1];


//	The following fields are set from values retrieved from the
//	CCL script 'mlts' resource. The values are received in one
//	or more Script_ccl_setup messages. See ReadVARMessages().
//	NOTE: if the script file has no 'mlts' resource, the setup
//	message is created using a default 'mlts' stored in the
//	ScriptMod configurator (CCL configurator).
u_int8_t	DialString1MaxLen;
u_int8_t	DialString2MaxLen;
u_int8_t	DialString3MaxLen;

u_int8_t	VerboseBuffer[256];//	Buffer for forming text for verbose log messages.
u_int32_t	LastExitError;

// Spackle a security hole.  Was the last ask masked?  if so,
// the next write we do should log masked.
// Don't forget to set and clear this flag in later asks and
// after the write
u_int32_t	LastAskedMasked;

FILE* filefd   = (FILE *) 0;
char *filename    = NULL;
char *phone_num   = "";
char *username  = "";
char *password  = "";
char *alertname  = "";
char *cancelname  = "";
char *iconurl  = "";
CFStringRef	alertNameRef = 0;
CFStringRef	cancelNameRef = 0;
CFURLRef	iconURL = 0;

char*		localBunPath  = "";
CFURLRef	localBundleURL = NULL;

CFBundleRef cclBundle = NULL;
CFURLRef	cclBundleURL= NULL;
CFURLRef	appBundleURL= NULL;



int pulse = 0;
int dialmode = 0; // 0 = normal, 1 = blind(ignoredialtone), 2 = manual
int speaker = 1;
int errorcorrection = 1;
int datacompression = 1;
u_char *serviceID   = NULL;

/*
int cellphoneCID= 0;
int cellphoneQofS= 0;
int limitBandwidth= 0;
*/

int verbose 	  = 0;
u_int32_t debuglevel 	  = 0;
int sysloglevel = LOG_NOTICE;
int syslogfacility = 0; //LOG_RAS; Kevin- variable not defined, punt...
int usestderr 	= 1;
int signalerror = 0;

struct callout *callout = NULL;	/* Callout list */
struct timeval timenow;		/* Current time */
enum enginemode enginemode = mode_connect;

fd_set	allset;
int 	maxfd;

/* --------------------------------------------------------------------------
Forward Declarations
-------------------------------------------------------------------------- */
void SetVarString(u_int32_t vs, u_int8_t * data);
u_int8_t *GetVarString(u_int32_t vs);
int PrepScript();
u_int8_t NextLine();
int NextCommand();
int NextInt(u_int32_t *theIntPtr);		// XXX returns signed numbers!
void PrepStr(u_int8_t *destStr, u_int32_t *isVarString, u_int32_t *varIndex, int varSubstitution);
void SkipBlanks();
void RunScript();
int MatchStr();
void Note();
u_int8_t Write();
void CommunicatingAt();
u_int8_t Ask();
void WriteContinue();
void UserHook();
void ScheduleTimer(long type, u_int32_t milliseconds);
int IfStr();

void MatchClr();
void Break();
void SetSpeed(void);
void SerReset(void);
void MonitorLine();
void DebugLevel();
void HSReset();
void Flush();
void DTRCommand(short DTRCode);

void terminate(int exitError);
void InitScript();
void Play();
void calltimeout(void);
struct timeval *timeleft(struct timeval *);
void timeout(void (*func)(void *), void *arg, u_long time);
void untimeout(void (*func)(void *), void *arg);
void ReceiveMatchData(u_int8_t nextChar);
int publish_entry(u_char *serviceid, CFStringRef entry, CFTypeRef value);
int unpublish_entry(u_char *serviceid, CFStringRef entry);
bool SetVarStringFromDict(CFDictionaryRef dict, CFStringRef key, int varStringIndex);
void sLog(char *fmt, ...);



/*************** Micro getopt() *********************************************/
#define	OPTION(c,v)	(_O&2&&**v?*(*v)++:!c||_O&4?0:(!(_O&1)&& \
                                (--c,++v),_O=4,c&&**v=='-'&&v[0][1]?*++*v=='-'\
                                &&!v[0][1]?(--c,++v,0):(_O=2,*(*v)++):0))
#define	OPTARG(c,v)	(_O&2?**v||(++v,--c)?(_O=1,--c,*v++): \
                                (_O=4,(char*)0):(char*)0)
#define	OPTONLYARG(c,v)	(_O&2&&**v?(_O=1,--c,*v++):(char*)0)
#define	ARG(c,v)	(c?(--c,*v++):(char*)0)

/*************** Micro getopt() *********************************************/


/* --------------------------------------------------------------------------
-------------------------------------------------------------------------- */
void badsignal(int signo)
{
    signalerror = cclErr_NoMemErr;
}

/* --------------------------------------------------------------------------
-------------------------------------------------------------------------- */
void hangup(int signo)
{
	// 7341084: ignore signals during disconnection
	if (enginemode != mode_disconnect)
		signalerror = cclErr_ScriptCancelled;
}


/* --------------------------------------------------------------------------
-------------------------------------------------------------------------- */
void StartRead()
{
    FD_SET(infd, &allset);
    maxfd = 1;
}

/* --------------------------------------------------------------------------
-------------------------------------------------------------------------- */
void StopRead()
{
    FD_ZERO(&allset);
    maxfd = 0;
}

#pragma mark Bundle Configuration/Startup

/* --------------------------------------------------------------------------
SetVarStringFromDict
- handles pulling the data out and setting the var value for the passed in key
- Returns success (true) even if the value is missing from the input dict
- fails only if the data is not of string type or if it can't be extracted
-------------------------------------------------------------------------- */
bool SetVarStringFromDict(CFDictionaryRef dict, CFStringRef key, int varStringIndex)
{
    bool retVal= false;
	u_char 	text[256];
	CFStringRef varString;

	varString= (CFStringRef)CFDictionaryGetValue(dict, key);
	if(varString == NULL) {
		retVal = true;		// missing values are okay (will return gNullString)
	} else if(CFStringGetTypeID() == CFGetTypeID(varString)) {
		// X varStrings are defined to be in the system encoding?
		if(CFStringGetPascalString(varString,text,256,CFStringGetSystemEncoding()))
		{
			SetVarString(varStringIndex, text);
			retVal= true;
		}
	} else {
		sLog("SetVarStringFromDict: value with non-string type");
		CFShow(key);
		CFShow(varString);
	}
	return retVal;
}

/* -------------------------------------------------------------------------- 
// check to make sure that the CCL version is the one we know
-------------------------------------------------------------------------- */
bool VerifyCCLBundle(CFBundleRef cclBun)
{
	CFDictionaryRef infoDict= CFBundleGetInfoDictionary(cclBun);
	int verNum = -1;

	if(!infoDict || !GetIntFromDict(infoDict, &verNum, kCCLVersionKey))
		return false;

	return (verNum == kCCLBundleVersion);
}

/* -------------------------------------------------------------------------- 
XX: should merge in keys in case we want to store CCLParameters in SC
Missing values are ignored by SetVarStringFromDict
-------------------------------------------------------------------------- */
bool ConfigureCCLParameters(CFDictionaryRef personalityDict)
{
	bool rval = false;
	CFDictionaryRef cclParms= GetCFDictionaryFromDict(personalityDict, kCCLParametersKey);
	if(cclParms)
	{
		rval = SetVarStringFromDict(cclParms, kCCLVarString27Key, vsString27);
		rval &= SetVarStringFromDict(cclParms, kCCLVarString28Key, vsString28);
		rval &= SetVarStringFromDict(cclParms, kCCLVarString29Key, vsString29);
		rval &= SetVarStringFromDict(cclParms, kCCLVarString30Key, vsString30);
		rval&=SetVarStringFromDict(cclParms,kCCLConnectSpeedKey,vsConnectSpeed);
		rval &= SetVarStringFromDict(cclParms, kCCLInitStringKey, vsInit);

		// Preferred* keys are handled by libccl's cclparser + SC.
		// The user has to confirm that they want those values.
	}
	return rval;
}

/* -------------------------------------------------------------------------- 
-------------------------------------------------------------------------- */
CFURLRef 
CopyScriptWithPersonality(CFBundleRef bundleRef, CFStringRef personalityKey)
{
	CFURLRef rval = NULL;
	CFDictionaryRef infoDict;
	CFDictionaryRef persDict;
	CFDictionaryRef persEntry = NULL;

	if (!(infoDict = CFBundleGetInfoDictionary(bundleRef)))
		goto finish;
	if (!(persDict = GetCFDictionaryFromDict(infoDict, kCCLPersonalitiesKey)))
		goto finish;

	// check the provided personality key
	if (personalityKey)
		persEntry = GetCFDictionaryFromDict(persDict, personalityKey);
	// or try the default personality (X could mask missing personality)
	if (!persEntry)
		persEntry = GetCFDictionaryFromDict(persDict,kCCLDefaultPersonalityKey);

	// if we found something, get the script name
	if (persEntry) {
		CFStringRef scriptName = NULL;

		if (!GetCFStringFromDict(persEntry,&scriptName,kCCLScriptNameKey))
			goto finish;
		if (!scriptName)
			goto finish;

		rval = CFBundleCopyResourceURL(bundleRef, scriptName, NULL, NULL);
		if (rval) {
			// We do this here to avoid having to determinepersonality twice...
			if (!ConfigureCCLParameters(persEntry)) {
				CFRelease(rval);
				rval = NULL;
			}
			// modemDict CCLParameters (if any) merged in later
		}
	}

finish:
	return rval;
}

/* -------------------------------------------------------------------------- 
-------------------------------------------------------------------------- */
bool ConfigureWithBundle(CFBundleRef bundleRef, CFStringRef personality)
{
	bool retVal= false;
	UInt8 pathBuffer[MAXPATHLEN];

	if(bundleRef!= NULL)
	{
		if(VerifyCCLBundle(bundleRef))
		{
			CFURLRef scriptURL= CopyScriptWithPersonality(bundleRef, personality);
			if(scriptURL!= NULL)
			{
				if(CFURLGetFileSystemRepresentation(scriptURL, true, pathBuffer, MAXPATHLEN))
				{
					if (filename)	free(filename);
					filename = strdup((char*)pathBuffer);
					retVal= true;
					CFRetain(bundleRef);
					cclBundle= bundleRef;
				}
				CFRelease(scriptURL);
			}
		}
	}
	return retVal;
}

/* -------------------------------------------------------------------------- 
ResolveCCLPath() resolves the full path with the following preferences:
- /full/path/to/foo.ccl
- /full/path/to/foo
- /System/Library/Modem Scripts/foo.ccl
- /System/Library/Modem Scripts/foo
- /Library/Modem Scripts/foo.ccl
- /Library/Modem Scripts/foo
Returns true if it found something
Side Effects: fullpath used as scratch space

Fairly common case should (eventually) be fully-resolved .ccl path.
Note: even if foo exists; prefers foo.ccl
-------------------------------------------------------------------------- */
bool ResolveCCLPath(char *path, char fullpath[PATH_MAX], struct stat *sb)
{
	char *extptr = path + (strlen(path) - (kCCL_BundleExtLen + sizeof('.')));
	bool cclExt;

	// try to avoid an unnecessary stat()
	cclExt = (*extptr == '.') && (strcmp(extptr+1, kCCL_BundleExtension) == 0);

	if (path[0] == '/') {
		if (!cclExt) {
			snprintf(fullpath, PATH_MAX, "%s.%s", path, kCCL_BundleExtension);
			if (stat(fullpath, sb) == 0) 	return true;
		}
		strlcpy(fullpath, path, PATH_MAX);
		if (stat(fullpath, sb) == 0) 		return true;
	} else {
		// path is not absolute so try the common directories
		if (!cclExt) {
			snprintf(fullpath, PATH_MAX, "%s/%s.%s", DIR_MODEMS_SYS, path,
					kCCL_BundleExtension);
			if (stat(fullpath, sb) == 0)	return true;
		}
		snprintf(fullpath, PATH_MAX, "%s/%s", DIR_MODEMS_SYS, path);
		if (stat(fullpath, sb) == 0)		return true;

		if (!cclExt) {
			snprintf(fullpath, PATH_MAX, "%s/%s.%s", DIR_MODEMS_USER, path,
					kCCL_BundleExtension);
			if (stat(fullpath, sb) == 0)	return true;
		}
		snprintf(fullpath, PATH_MAX, "%s/%s", DIR_MODEMS_USER, path);
		if (stat(fullpath, sb) == 0)		return true;
	}

	return false;
}

#define LOG_MAX 1024
/* -------------------------------------------------------------------------- 
sLog(): capture syslog/stderr idiom that was scattered throughout the code
-------------------------------------------------------------------------- */
void sLog(char *fmt, ...)
{
	va_list ap;
	char taggedfmt[LOG_MAX], s[64];
	time_t t;


	if (sysloglevel) {
		va_start(ap, fmt);
		vsyslog(sysloglevel, fmt, ap);
		va_end(ap);
	}
	if (usestderr) {
		time(&t);
		strftime(s, sizeof(s), "%c : ", localtime(&t));
		strlcpy(taggedfmt, s, LOG_MAX);
		strlcat(taggedfmt, fmt, LOG_MAX);
		strlcat(taggedfmt, "\n", LOG_MAX);
		va_start(ap, fmt);
		vfprintf(stderr, taggedfmt, ap);
		va_end(ap);
	}
}

/* -------------------------------------------------------------------------- 
validate path and see if it's a bundle; otherwise it's an old-school flat CCL
-------------------------------------------------------------------------- */
bool ConfigureCCL(char* path, CFStringRef personality)
{
	bool retVal = false;
	CFURLRef url;
	CFBundleRef  bundleRef;
	char fullpath[PATH_MAX];
	struct stat sb;

	// it's possible we have only a name
	// we need to turn it into a path and perhaps add .ccl to it
	if (!ResolveCCLPath(path, fullpath, &sb)) {
		sLog("couldn't find CCL '%s'", path);
		goto finish;
	}

	url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
		(UInt8*)fullpath, strlen(fullpath), S_ISDIR(sb.st_mode));
	if (!url)	goto finish;

	bundleRef= CFBundleCreate(kCFAllocatorDefault, url);
	if(bundleRef) {
		// retains cclBundle, copies filename, etc
		retVal = ConfigureWithBundle(bundleRef, personality);
		CFRelease(bundleRef);
		if (retVal)
			cclBundleURL = url;		// keep for loc
		else {
			CFRelease(url);
			sLog("%s does not appear to be a CCL bundle", fullpath);
		}
	} else {
		// maybe it was "old school"
		if (S_ISREG(sb.st_mode)) {
			CFRelease(url);
			if (filename)	free(filename);
			filename = strdup(fullpath);
			retVal = true;
		} else {
			sLog("%s is neither a bundle nor a file", filename);
		}
	}

finish:
	return retVal;
}

/* -------------------------------------------------------------------------- 
-------------------------------------------------------------------------- */
bool ParseEngineDict(CFDictionaryRef argDict)
{
	bool success= false;

	CFDictionaryRef engineDict= GetCFDictionaryFromDict(argDict, kCCLEngineDictKey);
	if(engineDict!= NULL)
	{
		CFStringRef modeString= (CFStringRef) CFDictionaryGetValue(engineDict, kCCLEngineModeKey);
		if(modeString!= NULL)
		{	
			// we assumed mode_connect when we initialized enginemode
			if((CFStringGetTypeID()== CFGetTypeID(modeString)) && (CFStringCompare(modeString, kCCLEngineModeDisconnect, 0)== kCFCompareEqualTo))
			{
				enginemode = mode_disconnect;
			}
			// Get*FromDict return success if the value was missing
			// i.e. the only errors are from conversion problems
			success = CopyCStringFromDict(engineDict, &alertname, kCCLEngineAlertNameKey);
			success &= CopyCStringFromDict(engineDict, &localBunPath, kCCLEngineBundlePathKey);
			success &= CopyCStringFromDict(engineDict, &cancelname, kCCLEngineCancelNameKey);
			success &= CopyCStringFromDict(engineDict, &iconurl, kCCLEngineIconPathKey);
			success &= GetIntFromDict(engineDict, &usestderr, kCCLEngineLogToStdErrKey);
			success &= CopyCStringFromDict(engineDict, (char**) &serviceID, kCCLEngineServiceIDKey);
			success &= GetIntFromDict(engineDict, &syslogfacility, kCCLEngineSyslogFacilityKey); 
			success &= GetIntFromDict(engineDict, &sysloglevel, kCCLEngineSyslogLevelKey); 
			success &= GetIntFromDict(engineDict, &verbose, kCCLEngineVerboseLoggingKey);
		}
	}

	return success;
}

/* -------------------------------------------------------------------------- 
-------------------------------------------------------------------------- */
bool ParseModemDict(CFDictionaryRef argDict)
{
	bool success= false;
	CFDictionaryRef modemDict= GetCFDictionaryFromDict(argDict, kSCEntNetModem);

	// CFShow(modemDict);
	if(modemDict!= NULL)
	{
		char* tempFilePath = NULL;
		CFStringRef personality = NULL;

		if (!CopyCStringFromDict(modemDict, &tempFilePath, kSCPropNetModemConnectionScript))
			goto finish;
		if (!tempFilePath)
			goto finish;
		if (!GetCFStringFromDict(modemDict, &personality, kSCPropNetModemConnectionPersonality))
			goto finish;

		// ConfigureCCL extracts default CCLParameters, handles NULL personality
		success = ConfigureCCL(tempFilePath, personality);
		free(tempFilePath);
		if (!success)
			goto finish;
					
		// dialmode is a string which we convert
		dialmode= kNormalDialMode;		// assume normal dial
		CFStringRef dialModeStr = (CFStringRef) CFDictionaryGetValue(modemDict, kSCPropNetModemDialMode);
		if(dialModeStr && CFStringGetTypeID()==CFGetTypeID(dialModeStr)){
			if (kCFCompareEqualTo == CFStringCompare(dialModeStr,
					kSCValNetModemDialModeIgnoreDialTone,0))
				dialmode = kBlindDialMode;
			else if (kCFCompareEqualTo == CFStringCompare(dialModeStr,
					kSCValNetModemDialModeManual,0))
				dialmode = kManualDialMode;
		}

		// only needed for mode_connect
		if (enginemode == mode_connect) {
			// expected, but we have reasonable defaults
			success &= GetIntFromDict(modemDict, &datacompression, kSCPropNetModemDataCompression);
			success &= GetIntFromDict(modemDict, &errorcorrection, kSCPropNetModemErrorCorrection);
			success &= CopyCStringFromDict(modemDict,&phone_num,kModemPhoneNumberKey);
			success &= GetIntFromDict(modemDict, &pulse, kSCPropNetModemPulseDial);
			success &= GetIntFromDict(modemDict, &speaker, kSCPropNetModemSpeaker);
			success &= CopyCStringFromDict(modemDict, &username, kSCPropNetPPPAuthName);
			success &= CopyCStringFromDict(modemDict, &password, kSCPropNetPPPAuthPassword);

	
			// optional, and SetVarStringFromDict sets to gNull if missing
			success &= SetVarStringFromDict(modemDict, kSCPropNetModemAccessPointName, vsAPN);
			success &= SetVarStringFromDict(modemDict, kSCPropNetModemDeviceContextID, vsCID);
		}

		// speed might be needed for disconnect?
		success &= SetVarStringFromDict(modemDict, kSCPropNetModemConnectSpeed, vsConnectSpeed);
		// stored kSCPropNetModemConnectSpeed could override CCL's static config
		// X could call ConfigureCCLParameters with modem dict (SC) version here
	}

finish:
	return success;
}


/* -------------------------------------------------------------------------- 
-------------------------------------------------------------------------- */
bool ParseArgDictionary(CFDictionaryRef argDict)
{
	CFBundleRef mainBundle= NULL;

	if(ParseEngineDict(argDict)) {
		if(ParseModemDict(argDict)) {	
			mainBundle= CFBundleGetMainBundle();
			if(mainBundle)
				appBundleURL= CFBundleCopyBundleURL(mainBundle);
			return true;
		}
	}

	return false;
}


/* -------------------------------------------------------------------------- 
-------------------------------------------------------------------------- */
#define kParseBufferSize 4096
bool ParsePipeArgs()
{
    ssize_t  bRead= 0;
    ssize_t  tbRead= 0;

    bool retVal= false;
    
    void* tempBuf;
    char* dataBuf= malloc(kParseBufferSize);
    if (!dataBuf)   goto finish;

    bRead= read(PPP_ARG_FD, dataBuf,  kParseBufferSize);
    tbRead= bRead;
    while (bRead == kParseBufferSize) {
        tempBuf= realloc(dataBuf, (tbRead+ kParseBufferSize));
        if (tempBuf) {
            dataBuf= tempBuf;
            bRead= read(PPP_ARG_FD, &dataBuf[tbRead], kParseBufferSize);
            tbRead+= bRead;
        } else {
            goto finish;
        }
        
    }
    CFDictionaryRef argDict= IOCFUnserialize(dataBuf, kCFAllocatorDefault, kCFPropertyListImmutable, NULL);
    if(argDict!= NULL)
    {
        retVal= ParseArgDictionary(argDict);
    }

finish:
    if (dataBuf)    free(dataBuf);
    close(PPP_ARG_FD);
    return retVal;
}

/* --------------------------------------------------------------------------
-------------------------------------------------------------------------- */
int main(int argc, char **argv)
{
    char            c;
    int             ret, nready, status, i;
    struct stat     statbuf;
    fd_set          rset;
    struct timeval  timo;

/* move DEBUG stuff (below) here if ppp/CCLEngine interactions need debugging */
    
    signal(SIGHUP, hangup);			/* Hangup */
    signal(SIGINT, hangup);			/* Interrupt */
    signal(SIGTERM, hangup);		/* Terminate */
    signal(SIGCHLD, badsignal);
    signal(SIGUSR1, badsignal);
    signal(SIGUSR2, badsignal);
    signal(SIGABRT, badsignal);
    signal(SIGALRM, badsignal);
    signal(SIGFPE, badsignal);
    signal(SIGILL, badsignal);
    signal(SIGPIPE, badsignal);
    signal(SIGQUIT, badsignal);
    signal(SIGSEGV, badsignal);

	// zero all of the strings (removed from InitScript)
    for (i = 0; i <= vsMax; i++)
        VarStrings[i] = 0;

    openlog("ccl", LOG_PID | LOG_NDELAY, syslogfacility);

    if(!ParsePipeArgs())
        terminate(cclErr_BadParameter);

/* DEBUG for attach on connect only (don't sweat the PID;
   (gdb) attach CCLEngine * /
if (enginemode == mode_connect) {
fprintf(stderr, "CCLEngine pid %d\n", getpid());
fflush(stderr);
sleep(3);
}
*/

    InitScript();       // sets whatever variables PPA() didn't

    alertNameRef = CFStringCreateWithCString(NULL, alertname, kCFStringEncodingUTF8);
    cancelNameRef = CFStringCreateWithCString(NULL, cancelname, kCFStringEncodingUTF8);
    if (*iconurl) 
        iconURL = CFURLCreateFromFileSystemRepresentation(nil, (UInt8*) iconurl, strlen(iconurl), false /*notDir*/);
    if (*localBunPath) 
        localBundleURL = CFURLCreateFromFileSystemRepresentation(NULL, (UInt8*) localBunPath, strlen(localBunPath), true /*isDir*/);

    // figure out file size
	ret = stat(filename, &statbuf);
	if (ret < 0)
		terminate(cclErr_BadParameter);

    SV.script = malloc((int)statbuf.st_size);
    if (!SV.script)
        terminate(cclErr_NoMemErr);

    SV.scriptSize = (int)statbuf.st_size;

    filefd = fopen(filename, "r");
    if (filefd) {
        ret = fread(SV.script, statbuf.st_size, 1, filefd);
        if (ret < 0)
            terminate(cclErr_BadParameter);
    }

    /* start by unpublishing any remaining information */
    if (serviceID) {
        unpublish_entry((u_char*) serviceID, kSCPropNetModemConnectSpeed);
    }
    StopRead();
    Play();

    for ( ; ; ) {

        rset = allset;
        nready = select(maxfd, &rset, NULL, NULL, timeleft(&timo));
        if(signalerror)
			terminate(signalerror);
        if (FD_ISSET(infd, &rset)) {
            status = read(infd, &c, 1);
            switch (status) {
                case 1:
                    ReceiveMatchData(c);
                    break;
                default:
                    if (errno != EINTR)
                        terminate(cclErr);
            }
        }

        calltimeout();

    }

    // NOT REACHED
    exit(0);       // insure the process exit status is 0
    return 0;      // ...and make main fit the ANSI spec.
}

/* --------------------------------------------------------------------------
 init everything
-------------------------------------------------------------------------- */
void InitScript()
{
    u_char 	text[256], text1[256];
    
    gNullString[0] = 1;
    gNullString[1] = ' ';
    
    //
    // Initialize script variables and varStrings
    //
    bzero(&SV, sizeof(struct TRScriptVars));

    LastExitError		= 0;

    DialString1MaxLen		= 40;
    DialString2MaxLen		= 40;
    DialString3MaxLen		= 40;

    SV.topOfStack		= cclNestingLimit;	// this stack is for JSR and RETURN commands.
    SV.theAbortErr		= 0;
    SV.lineCount		= 0;
    SV.scriptPrepped		= 0;		// PrepScript has not yet been called
    SV.scriptPrepFailed		= 0;		// PrepScript has not yet rejected the script
    SV.scriptAllocSize		= 0;
    SV.scriptSize		= 0;
    SV.script			= NULL;
    SV.askLabel			= 0;
    SV.indexTable		= NULL;
    SV.commands			= NULL;

    LastAskedMasked		= 0;


    text[0] = strlen(username);
    bcopy(username, &text[1], text[0]);
    SetVarString(vsUserName, text);

    text[0] = strlen(password);
    bcopy(password, &text[1], text[0]);
    SetVarString(vsPassWord, text);

    sprintf((char*) text, "%d", speaker);
    text1[0] =  strlen((char*) text);
    bcopy(text, &text1[1], text[0]);
    SetVarString(vsModemSpeaker, text1);

    sprintf((char*) text, "%d", errorcorrection);
    text1[0] =  strlen((char*) text);
    bcopy(text, &text1[1], text[0]);
    SetVarString(vsErrorCorrection, text1);

    sprintf((char*) text, "%d", datacompression);
    text1[0] =  strlen((char*) text);
    bcopy(text, &text1[1], text[0]);
    SetVarString(vsDataCompression, text1);

    sprintf((char*) text, "%d", dialmode);
    text1[0] =  strlen((char*) text);
    bcopy(text, &text1[1], text[0]);
    SetVarString(vsDialMode, text1);

    text1[0] =  1;
    text1[1] =  pulse ? 'P' : 'T';
    SetVarString(vsTonePulse, text1);
}

/* --------------------------------------------------------------------------
 dispose everything
-------------------------------------------------------------------------- */
void DisposeScript()
{
    int 	i;
    
    if (SV.script) {
        free(SV.script);
        SV.script = 0;
    }

    if (SV.commands) {
        free(SV.commands);
        SV.commands = 0;
    }

    if (SV.indexTable) {
        free(SV.indexTable);
        SV.indexTable = 0;
    }

    for (i = 0; i <= vsMax; i++) {
        if (VarStrings[i]) {
            free(VarStrings[i]);
            VarStrings[i] = 0;
        }
    }
}

/* --------------------------------------------------------------------------
varStrings are held in the global VarStrings[].  Each element is NULL
until SetVarString is called, in which case memory is allocated and the
data copied in.  If Set has not previously succeeded, Get will return a
pointer to gNullString, a zero length pstring.  Memory errors don't abort
the script but they may break it if the empty string wasn't acceptable
for that particular varString's value.
-------------------------------------------------------------------------- */
void SetVarString(u_int32_t vs, u_int8_t * data)
{
    if (vs > vsMax)
        return;

    if (VarStrings[vs]) {
        free(VarStrings[vs]);
        VarStrings[vs] = 0;
    }

    if (data) {
        VarStrings[vs] = malloc(*data + 1);
        if (VarStrings[vs])
            bcopy(data, VarStrings[vs], *data + 1 );
		else
			sLog("SetVarString: %s", strerror(errno));	// should be ENOMEM
    }
}

/* --------------------------------------------------------------------------
-------------------------------------------------------------------------- */
u_int8_t *GetVarString(u_int32_t vs)
{
    if (vs > vsMax)
	{
        return gNullString;
	}
    if (VarStrings[vs])
	{
        return VarStrings[vs];
	}
    return gNullString;
}


/* --------------------------------------------------------------------------
called to start execution of the script.  this is true regardless as
to the connect/answer mode set by SetAnswering().
-------------------------------------------------------------------------- */
void Play(void)  
{	
    u_int32_t	remaining;
    u_int8_t	*src, len = 0, buf[256];
    u_char	text[256];

    LastExitError = 0;

    if (!SV.scriptPrepped ) { // if the Script has not yet been preflighted...
        if (SV.theAbortErr = PrepScript()) { // if the preflight returns an error...
            SV.scriptPrepFailed = 1;		// so Disconnect won't try to re-preflight the script.
	    sLog("Script parsing failed after %d lines", SV.scriptLine);
            terminate(SV.theAbortErr);	// a script syntax error was detected
            return;
        }
        else
            SV.scriptPrepped = 1;
    }

    SV.chrDelayValue	= 0;

    switch (enginemode) {
        case mode_connect:
            SV.ctlFlags		&= ~cclHangupMode;
            SV.ctlFlags |= cclOriginateMode;		// turn on OriginateMode
            SV.scriptLine = SV.originateLine;		// set the script entry point

            // if the dial string has not yet arrived in a Script_ccl_setup message,
            // fetch the dial string from ScriptMod, which received it from either a
            // T_conn_req or a Script_execute message.

			// XX should an empty phone number string -> gNullString?
            text[0] =  strlen(phone_num);
            bcopy(phone_num, &text[1], text[0]);
            if (GetVarString(vsDialString) == gNullString)
                SetVarString(vsDialString, text);

            SetVarString(vsDialString1, NULL);
            SetVarString(vsDialString2, NULL);
            SetVarString(vsDialString3, NULL);

            if (src = GetVarString(vsDialString)) {

                remaining = *src++;

                if (len = min(DialString1MaxLen, remaining)) {
                    *buf = len;
                    bcopy(src, buf+1, len);
                    SetVarString(vsDialString1, buf);
                    src += len;
                    remaining -= len;
                }

                if (len = min(DialString2MaxLen, remaining)) {
                    *buf = len;
                    bcopy(src, buf+1, len);
                    SetVarString(vsDialString2, buf);
                    src += len;
                    remaining -= len;
                }

                if (len = min(DialString3MaxLen, remaining)) {
                    *buf = len;
                    bcopy(src, buf+1, len);
                    SetVarString(vsDialString3, buf);
                    src += len;
                    remaining -= len;
                }

                // and ignore any remaining characters
            }

           break;
        case mode_disconnect:
            SV.ctlFlags	&= ~cclAnswerMode;		// though we'll return to Answering, on disconnect
            SV.ctlFlags	|= cclHangupMode;
            SV.scriptLine = SV.hangUpLine;		// set the script entry point
            break;
        case mode_listen:
            SV.ctlFlags |= cclAnswerMode;		// turn on AnswerMode
            SV.ctlFlags		&= ~cclHangupMode;
            SV.scriptLine = SV.answerLine;		// set the script entry point
            break;
    }

    RunScript();	// plays the script from @entrypoint until Exit
}

/* --------------------------------------------------------------------------
This routine prepares a script to be played.
CCL variablesa and labels are set up for playing the script, and commands are checked.
-------------------------------------------------------------------------- */
int PrepScript()
{
    u_int8_t	*bp, *d, *s, c;
    u_int16_t	*indexp, Lindex, i, cmd;
    u_int32_t	labelIndex;
    int			result = 0;

    // The following "do {} while (false);" construct
    // prepares for the task of looping through the lines
    // of script. If there is something wrong with script
    // or if it can't allocate memory, it breaks immediately
    // with an error value in "result".

    do {
        if (SV.scriptSize > MAX_SCRIPT_SIZE) {
            result = cclErr_ScriptTooBig;
            break;
        }

        Lindex = 1;
        bp = SV.script;

        // count the lines in the script:
        for( i = SV.scriptSize; i; --i ) {
            if ((*bp == chrCR) || (*bp == chrNL))
                Lindex++;
            bp++;
        }

        if (Lindex > MAX_SCRIPT_LINES) {
            result = cclErr_TooManyLines;
            break;
        }

		// Lindex is 1-based; lineCount is zero-based
		// However, if the last character wasn't a line terminator; bump
        SV.lineCount = Lindex - 1;
        c = SV.script[SV.scriptSize-1];
        if (c != chrCR && c != chrNL)
            SV.lineCount++;

        bp = malloc(500);
        if (!bp) {
            result = ENOMEM;
            break;
        }

        SV.commands = bp;
        *((u_int16_t *)bp) = cLastCmd;
        bp += 2;
        for( i = 0; i < cLastCmd; ) {
            s = (u_int8_t*) Commands[i++];
            d = bp + 1;
            while( c = *s++ )
                *d++ = c;
            *bp = (d - bp - 1);		// set count for pascal string
            bp = d;
        }

        SV.indexTable = malloc(SV.lineCount * sizeof(u_int16_t *));
        if (!SV.indexTable) {
            result = ENOMEM;
            break;
        }

        indexp = SV.indexTable;
        for( i = 0; ++i <= SV.lineCount; )
            *indexp++ = 0;

        /* adjust the size of line index array	*/
        indexp = SV.indexTable;

        /* fill line index array, for each line,		*/
        /* SV.indexTable->lineIndex == byte offset	*/
        /* from start of script to start of line.		*/
        bp = SV.script;
        Lindex = 0;
        *indexp++ = 0;
        for (i = 0; i < SV.scriptSize; i++) {
            if ((*bp == chrCR) || (*bp == chrNL))
                *indexp++ = i + 1;
            bp++;
        }

        SV.scriptLine = 0;
    }
    while (0);

    // The initialization loop above may have set
    // result != noErr. If so, the following while
    // loop is not entered.

    while (result == 0 && NextLine()) {	/*process the next line of the script*/
        cmd = NextCommand();
		switch (cmd)  {
            case cNoCmd:
                result = cclErr_BadCommand;
                break;

            case cOriginateLabel:
                if( SV.originateLine )
                    result = cclErr_DuplicateLabel;
                else
                    SV.originateLine = SV.scriptLine;
                break;

            case cAnswerLabel:
                if( SV.answerLine )
                    result = cclErr_DuplicateLabel;
                else
                    SV.answerLine = SV.scriptLine;
                break;

            case cHangUpLabel:
                if( SV.hangUpLine )
                    result = cclErr_DuplicateLabel;
                else
                    SV.hangUpLine = SV.scriptLine;
                break;

            case cScriptLabel:
                result = NextInt(&labelIndex);
                if (result == 0) {
                    labelIndex--;
                    if ((labelIndex < 0) || (labelIndex >= MAXLABELS))
                        result = cclErr_BadLabel;
                    else {
                        if (SV.labels[labelIndex])
                            result = cclErr_DuplicateLabel;
                        else
                            SV.labels[labelIndex] = SV.scriptLine;
                    }
                }
                break;

                // The following cases are commands for which we check the syntax
                // to verify as much a possible that the parameters are OK
            case cExit:
                // check for exit result parameter
                result = NextInt(&labelIndex);
                // check for result in appropriate range?
                break;

            case cHSReset:
                for (i = 0; result == 0 && i < kHSResetParamCount; i++)
                    result = NextInt(&labelIndex);
                break;

            case cAsk:				// check for a integer maskflag value
            case cChrDelay:			// check for a integer delay value
            case cCommunicatingAt:		// check for a integer modem speed value
            case cIfAnswer:			// 				ditto
            case cIfOriginate:			// 				ditto
            case cJSR:				// 				ditto
            case cJump:				// 				mega ditto
            case cMatchRead:			// check for a integer timeout value
            case cPause:			// check for a integer pause time value
            case cSetSpeed:			// check for a integer interface speed value
            case cSetTries:			// check for a integer tries value
            case cUserHook:			// check for a integer opcode value
            case cMonitorLine:			// check for a integer bits mask value
            case cDebugLevel:			// check for a integer bits mask value
                result = NextInt(&labelIndex);
                break;

            case cIfStr:
                for (i = 0; result == 0 && i < (kIfStrParamCount - 1); i++)
                    result = NextInt(&labelIndex);
                break;

            case cMatchStr:
                if (NextInt(&labelIndex) == 0 && labelIndex > maxMatch) {
		    result = cclErr_MatchStrIndxErr;
		    break;
		}
		result = NextInt(&labelIndex);
                SkipBlanks();
                PrepStr(SV.strBuf, NULL, NULL, 1);
                //
                // Don't allow to match empty string
                //
                if (SV.strBuf[0] == 0)
                    result = cclErr_BadParameter;
                break;

            case cIfTries:
                for (i = 0; result == 0 && i < (kIfTriesParamCount - 1); i++)
                    result = NextInt(&labelIndex);
                break;

            case cLogMsg:			// undocumented.  use cNote.  KW
            case cNote:
            case cWrite:
                SkipBlanks();
                PrepStr(SV.strBuf, NULL, NULL, 1);
                if( SV.strBuf[0] == 0 )
                    result = cclErr_BadParameter;
				break;

            case cSerReset:
                for (i = 0; result == 0 && i < kSerResetParamCount; i++)
                    result = NextInt(&labelIndex);
                break;

            default:
                break;
        }/* end SWITCH on command type */
    }/* end WHILE */

    if (result == 0) {
        if (SV.originateLine == 0)	result = cclErr_NoOriginateLabel;
        else if (SV.answerLine == 0)	result = cclErr_NoAnswerLabel;
        else if (SV.hangUpLine == 0)	result = cclErr_NoHangUpLabel;
        else SV.scriptLine = 0;

        // process the script again to check that all
        // destination labels are valid:
        while (result == 0 && NextLine()) {
            cmd = NextCommand();
            switch (cmd) {
                case cIfAnswer:
                case cIfOriginate:
                case cJump:
                case cJSR:
                    result = NextInt(&labelIndex);
                    if (result == 0) {
                        labelIndex--;
                        if ((labelIndex < 0) || (labelIndex >= MAXLABELS))
                            result = cclErr_BadLabel;
                        else {
                            if (SV.labels[ labelIndex ] == 0)
                                result = cclErr_LabelUndefined;
                        }
                    }
                    break;

                case cIfStr:
                case cIfTries:
                case cMatchStr:
                    // discard the first parameter
                    result = NextInt(&labelIndex);
                    if( result == 0 ) {
                        result = NextInt(&labelIndex);
                        if (result == 0) {
                            labelIndex--;
                            if ((labelIndex < 0) || (labelIndex >= MAXLABELS))
                                result = cclErr_BadLabel;
                            else {
                                if( SV.labels[ labelIndex ] == 0)
                                    result = cclErr_LabelUndefined;
                            }
                        }
                    }
                    break;

                case cAsk:
                    result = NextInt(&labelIndex);		// check for a integer maskflag value
                    if (result == 0) {
                        SkipBlanks();
                        PrepStr(SV.strBuf, 0, 0, 1);	// fetch the message Pascal string

                        // this is the optional third parameter, added for ARA 3.0, for Manual Dialing.
                        // NextInt() will return cclErr_BadParameter if the third parameter doesn't exist.

                        if (NextInt(&labelIndex) == 0) {
                            labelIndex--;
                            if ((labelIndex < 0) || (labelIndex >= MAXLABELS))
                                result = cclErr_BadLabel;
                            else {
                                if (SV.labels[labelIndex] == 0)
                                    result = cclErr_LabelUndefined;
                            }	
                        }
                    }
                    break;

                default:
                    break;
            }
        }/* end WHILE */
    }/* end IF no error */
    return result;
}

/* --------------------------------------------------------------------------
Advance to the next line of the script.
-------------------------------------------------------------------------- */
u_int8_t NextLine()
{
    u_int8_t	*bp;
    short	i;

    SV.scriptLineIndex = 0;			/* reset line index			*/
    SV.scriptLine++;				/* check for out of bounds	*/
    if ((SV.scriptLine < 1) || (SV.scriptLine > SV.lineCount))
        return 0;

    /* update the line ptr	*/
    i = SV.indexTable[SV.scriptLine-1];
    SV.scriptLinePtr = SV.script + i;

    SV.scriptLineSize = 0;
    bp = SV.scriptLinePtr;
    while ((i++ < SV.scriptSize) &&
		   (*bp != chrCR) && (*bp != chrNL) &&
           (SV.scriptLineSize < 255)) {
        bp++;
		SV.scriptLineSize++;
    }
    return 1;
}

/* --------------------------------------------------------------------------
-------------------------------------------------------------------------- */
int equalstring(u_int8_t *s1, u_int8_t *s2)
{
    int 	l1 = *s1, l2 = *s2;

    s1++; s2++;
    while (l1 && l2 && (toupper(*s1) == toupper(*s2))) {
       l1--; l2--;
       s1++; s2++;
    }
    return ((l1 == 0) && (l2 == 0));
}


/* --------------------------------------------------------------------------
Fetch the next command from the script, and set it up for processing.
Return the command number of the command found, or a helpful error
code if no command is found
-------------------------------------------------------------------------- */
int NextCommand()
{
    u_int8_t	*bp, *cmdStrPtr, cmdFound;
    int		cmdIndex, result;
    char 	text[256];
    
    result = cComment;				/* assume a blank line		*/
    SkipBlanks();				/* skip any initial blanks	*/

    bp	= SV.scriptLinePtr + SV.scriptLineIndex;
    cmdStrPtr	= &SV.strBuf[1];
    SV.strBuf[0] = 0;

    while ((SV.scriptLineIndex < SV.scriptLineSize)
           && ((*bp != chrSpace) && (*bp != chrHT))) {
        SV.scriptLineIndex++;
        SV.strBuf[0]++;
        *cmdStrPtr++ = *bp++;
    }
    bcopy(&SV.strBuf[1], &text[0], SV.strBuf[0]);
    text[SV.strBuf[0]] = 0;
    //    printf("nextcmd = strBuf = %s\n", text);
    //syslog(LOG_INFO, "nextcmd = strBuf = %s\n", text);

    if (SV.strBuf[0]) {		/* search for this command in command list */
        bp		= SV.commands + 2;
        cmdIndex	= cFirstCmd;
        result		= cNoCmd;			/*assume that we don't have a command*/
        cmdFound	= 0;

        while (cmdIndex <= cLastCmd && !cmdFound) {
            if (cmdIndex == cComment && SV.strBuf[1] == bp[1]) {
                result = cComment;
                cmdFound = 1;
            }
            if( equalstring(&SV.strBuf[0], bp)) {
                result = cmdIndex;
                cmdFound = 1;
            }
            else {
                bp += *bp + 1;
                cmdIndex++;
            }
        }
    }

    return result;
}

/* --------------------------------------------------------------------------
Parse and prepare srcStr (from srcNdx) and place result in destStr;
srcStr and destStr are both pascal strings (Str255).

If the string is derived by substituting for a varString, return true
in "isVarString" and the string index in "varIndex.
-------------------------------------------------------------------------- */
void PrepStr(u_int8_t *destStr, u_int32_t *isVarString, u_int32_t *varIndex, int varSubstitution)
{
    u_int8_t	*s, *d, *maskPtr;
    u_int16_t	srcStrIndex;
    u_int8_t	done, chDelimiter, escChar, dstStrLen, centerDot = '�';

    //
    // Assume the string is not a varString.
    //
    if ( isVarString != NULL )
        *isVarString = 0;

	// make sure we don't walk off the end of the line
	if (SV.scriptLineIndex > SV.scriptLineSize) {
		destStr[0] = '\0';
		return;
	}

    srcStrIndex	= SV.scriptLineIndex;
    s		= SV.scriptLinePtr + srcStrIndex;

    //
    // determine string terminators
    // default is space, tab, comma, semicolon
    //
    chDelimiter = 0;
    if ((*s == chrDblQuote) || (*s == chrQuote)) {
        chDelimiter = *s++;
        require_action(srcStrIndex < SV.scriptLineSize, exit, dstStrLen = 0);
        srcStrIndex++;
    }

    d		= destStr + 1;
    dstStrLen	= 0;
    done	= 0;

    while ((srcStrIndex < SV.scriptLineSize) && !done) {

        // copy & prepare the string
        maskPtr = 0;
        switch (*s) {
            // special delimiters
            case chrDblQuote:
            case chrQuote:
                if (chDelimiter == *s)
                    done = 1;
                else {
                    require_action(srcStrIndex < SV.scriptLineSize, exit, dstStrLen = 0);
                    srcStrIndex++;
                    require_action(dstStrLen < UINT8_MAX, exit, dstStrLen = 0);
                    dstStrLen++;
                    *d++ = *s++;
                }
                break;

                // default delimiters
            case chrHT:
            case chrSpace:
            case chrComma:
            case chrSemiColon:
                if (!chDelimiter)
                    done = 1;
                else {
                    require_action(srcStrIndex < SV.scriptLineSize, exit, dstStrLen = 0);
                    srcStrIndex++;
                    require_action(dstStrLen < UINT8_MAX, exit, dstStrLen = 0);
                    dstStrLen++;
                    *d++ = *s++;
                }
                break;

            // copy escape character into the destStr
            case chrBackSlash:
                s++;
                require_action(dstStrLen < UINT8_MAX, exit, dstStrLen = 0);
                dstStrLen++;

                if ((*s == chrBackSlash) || (*s == chrCaret)) {
                    require_action(srcStrIndex < (SV.scriptLineSize - 1), exit, dstStrLen = 0);
                    srcStrIndex += 2;
                    *d++ = *s++;
                }
                else if (*s == 'x') {
                    require_action(srcStrIndex < (SV.scriptLineSize - 3), exit, dstStrLen = 0);
                    srcStrIndex += 4;
                    s++;                   
                    escChar = ((*s - ((*s <= '9') ? '0' : ('A' - 10))) * 16);
					s++;
					escChar += (*s - ((*s <= '9') ? '0' : ('A' - 10)));
					s++;
                    *d++ = escChar;
                }
                else {
                    require_action(srcStrIndex < (SV.scriptLineSize - 2), exit, dstStrLen = 0);
                    srcStrIndex += 3;
                    escChar = ((*s++ - '0') * 10);
                    escChar += (*s++ - '0');
                    *d++ = escChar;
                }

                break;

                // copy the varString into the destStr
            case chrCaret:
            {
                u_int8_t	vs, *vsp;
                int 	i;
                
                if (varSubstitution == 0) {
                    require_action(srcStrIndex < SV.scriptLineSize, exit, dstStrLen = 0);
                    srcStrIndex++;
                    require_action(dstStrLen < UINT8_MAX, exit, dstStrLen = 0);
                    dstStrLen++;
                    *d++ = *s++;
                    break;
                }
                
                s++;
                require_action(srcStrIndex < (SV.scriptLineSize - 1), exit, dstStrLen = 0);
                srcStrIndex += 2;

                switch (vs = *s++) {
                    case '*':		vs = vsAsk;		break;
                    case 'u': case 'U':
						vs = vsUserName;
						break;
                    case 'p': case 'P':	vs = vsPassWord;	break;
                    default: {
                        vs -= '0';
                        if (*s >= '0' && *s <= '9') {
                            require_action(srcStrIndex < SV.scriptLineSize, exit, dstStrLen = 0);
                            srcStrIndex++;
                            vs = 10 * vs + *s++ - '0';
                        }
                        break;
                    }
                }

                if (isVarString)
                    *isVarString = 1;

                if (varIndex)
                    *varIndex = vs;

                if (vsp = GetVarString(vs)) {
                    if (SV.logMaskOn && vs == (SV.maskStringId - 1)  && SV.maskStart) {
                        maskPtr = d + SV.maskStart - 1;
                    }

                    for (i = 1; i <= *vsp; i++, dstStrLen++) {
                        require_action(dstStrLen < UINT8_MAX, exit, dstStrLen = 0);
                        *d++ = vsp[i];
                    }

                    if (maskPtr) {
                        // %% centerDot = GetPasswordBulletChar();
                        for (i = SV.maskStop - SV.maskStart; i >= 0; i--)
                            *maskPtr++ = centerDot;
                    }
                }
                break;
            }

            // copy srcStr byte into the destStr
            default:
                require_action(srcStrIndex < SV.scriptLineSize, exit, dstStrLen = 0);
                srcStrIndex++;
                require_action(dstStrLen < UINT8_MAX, exit, dstStrLen = 0);
                dstStrLen++;
                *d++ = *s++;
                break;
        }
    }

exit:
    *destStr = dstStrLen;			// pascal string - set length
    SV.scriptLineIndex = srcStrIndex + 1;	// skip over the string terminator
	
}

/* --------------------------------------------------------------------------
parse a string and substitute variable with actual values
-------------------------------------------------------------------------- */
void varSubstitution(u_int8_t *src, u_int8_t *dst, int dstmaxlen)
{
    u_int8_t	*s = src + 1, *d = dst + 1;
    u_int16_t	srclen = src[0];
    u_int8_t	len = 0, dstlen = 0;

    while ((len < srclen) && (dstlen < dstmaxlen)) {
        // copy & prepare the string
        switch (*s) {

            // copy the varString into the destStr
            case chrCaret:
            {
                u_int8_t	vs, *vsp;
                int 	i;
                
                s++;
                len += 2;

                switch (vs = *s++) {
                    case '*':		vs = vsAsk;		break;
                    case 'u': case 'U':	vs = vsUserName;	break;
                    case 'p': case 'P':	vs = vsPassWord;	break;
                    default: {
                        vs -= '0';
                        if (*s >= '0' && *s <= '9') {
                            len++;
                            vs = 10 * vs + *s++ - '0';
                        }
                        break;
                    }
                }
                if (vsp = GetVarString(vs)) {
                    for (i = 1; (i <= *vsp) && (dstlen < dstmaxlen); i++, dstlen++)
                        *d++ = vsp[i];
                }
                break;
            }

            // copy srcStr byte into the dst
            default:
				len++;
                dstlen++;
                *d++ = *s++;
                break;
        }
    }

    *dst = dstlen;			// pascal string - set length
}

/* --------------------------------------------------------------------------
Skip over white space (spaces and tabs)
-------------------------------------------------------------------------- */
void SkipBlanks()
{
    u_int8_t	*s;

    s = SV.scriptLinePtr + SV.scriptLineIndex;
    while ((SV.scriptLineIndex < SV.scriptLineSize)
           && ((*s == chrSpace) || (*s == chrHT))) {
        s++;
        SV.scriptLineIndex++;
    }
}

/* --------------------------------------------------------------------------
Read the next text from the command line and convert the ascii to
an int, and set theIntPtr to the result.
If there is no int to be read, return cclErr_BadParameter.
If everything is OK, return noErr
-------------------------------------------------------------------------- */
int NextInt(u_int32_t *theIntPtr)
{
    int		sign;
    u_int8_t	*intStrPtr, intStrNdx, intStrLen;
    u_int8_t	intStr[256];
	u_int32_t	isVarString = 0;

    /* skip over blanks in script line, and pull out the number string	*/
    SkipBlanks();
    PrepStr( intStr, &isVarString, NULL, 1);

    *theIntPtr = 0;
    sign = 1;
    intStrLen = *intStr;
    intStrPtr = intStr + 1;
    if( intStrLen == 0 )
        return cclErr_BadParameter;
	// if it's a variable that is null (gNullString is a pstring = " "),
	// substitute zero instead of failing later
	if (isVarString && (intStrLen == 1) && *intStrPtr == ' ')
		*intStrPtr = '0';

    /* check for sign operator	*/
    intStrNdx = 1;
    if (*intStrPtr == '-') {
        sign = -1;
        intStrNdx++;
        intStrPtr++;
    }
    if (intStrNdx > intStrLen)
        return cclErr_BadParameter;

    /* parse out the number	*/
    while ((intStrNdx++ <= intStrLen) && (*intStrPtr >= '0') && (*intStrPtr <= '9'))
           *theIntPtr = (*theIntPtr * 10) + (*intStrPtr++ - '0');

    if (intStrPtr == (intStr + 1))	// the string did not contain numbers
        return cclErr_BadParameter;

    *theIntPtr *= sign;				// set appropriate sign
    return 0;
}

/* --------------------------------------------------------------------------
Run the script, one line at a time, by reading the command and
dispatching to the proper routine. This continues until some command
defers the script execution. This could be due to encountering
the end of the script, or waiting for an asynchronous command to complete.
-------------------------------------------------------------------------- */
void RunScript()  
{
    u_int8_t	running, cmd;
    int         code, result = 0;
    u_int32_t   i, lval, jumpLabel;

    if (SV.scriptLine == 0) {			// no entry point, so terminate the script.
        SV.ctlFlags &= ~cclPlaying;				// don't play this script.
        SV.theAbortErr = cclErr_EndOfScriptErr;	// for WrapScript()->ScriptComplete().
        terminate(cclErr_EndOfScriptErr);	// a script syntax error was detected
        return;
    }

    SV.ctlFlags |= cclPlaying;		// turn on the cclPlaying flag
    running = 1;			// keep running until SerReset or other interruption
    while (running) {			// find the next script line
        if (!NextLine()) {		// no more lines in the script, so we've hit the end without an exit. 
        
            SV.ctlFlags &= ~cclPlaying;			// don't play this script.
            SV.theAbortErr = cclErr_EndOfScriptErr;	// for WrapScript()->ScriptComplete().
            terminate(cclErr_EndOfScriptErr);	// a script syntax error was detected
            return;
        }

        /* process script line	*/
        cmd = NextCommand();			// get the next CCL Command from the script.
		switch (cmd) {
            case cAsk:
                running = !Ask();		// wait until User data arrives in ReceiveDataFromAbove().
                break;

            case cChrDelay:
                NextInt( &i );			// delay is specified in tenths of a second
               // if (i > 0)		// CA : Why is it impossible to reset delay ?
                SV.chrDelayValue = i * 100;
                break;

            case cCommunicatingAt:
                CommunicatingAt();		// tell upstairs what our modulation speed is
                break;

            case cDecTries:
                SV.loopCounter--;		// decrement loop counter
                break;

            case cDTRClear:
                DTRCommand(DTR_CLEAR);		// Issue the DTR Clear command
                break;

            case cDTRSet:
                DTRCommand(DTR_SET);		// Issue the DTR Set command
                break;

            case cExit:
                running = 0;
                SV.ctlFlags &= ~cclPlaying;

                NextInt((u_int32_t*)&code);		// fetch the Exit code
				SV.theAbortErr = code;			// not currently used

				Note();							// handle optional string

				terminate(code);	// send an ARA_Notify message upstream.
                break;

            case cFlush:
                Flush();
                break;

            case cHSReset:
                HSReset();				// Configure the serial driver handshake options
                break;

            case cIfAnswer:
                if (SV.ctlFlags & cclAnswerMode) {
                    NextInt(&i);
                    SV.scriptLine = SV.labels[i - 1];
                }
                break;

            case cIfOriginate:
                if (SV.ctlFlags & cclOriginateMode) {
                    NextInt(&i);
                    SV.scriptLine = SV.labels[i - 1];
                }
                break;

            case cIfStr:
                // IfStr() will return either 0 if the comparison fails,
                // or the label to jump to if the comparison succeeds.
                jumpLabel = IfStr();
                if (jumpLabel > 0)
                    SV.scriptLine = jumpLabel;
                break;

            case cIfTries:
                NextInt(&i);
                if (SV.loopCounter >= i) {
                    NextInt(&i);
                    SV.scriptLine = SV.labels[i - 1];
                }
                break;

            case cIncTries:
                SV.loopCounter++;					// increment the loop counter
                break;

            case cJump:
               NextInt(&i);
                SV.scriptLine = SV.labels[i - 1];
                break;

            case cJSR:
                if (SV.topOfStack == 0) {
                    running = 0;
                    SV.ctlFlags &= ~cclPlaying;
                    SV.theAbortErr = cclErr_SubroutineOverFlow;	// for WrapScript()->ScriptComplete().
                    terminate(cclErr_SubroutineOverFlow);
                }
                else {
                    SV.stack[--SV.topOfStack] = SV.scriptLine;	// save return line
                    NextInt(&i);
                    SV.scriptLine = SV.labels[i - 1];
                }
                break;

            case cLogMsg:			// undocumented.  use cNote.  KW
            case cNote:
                Note();				// post a Note to the Status window and/or Log File.
                break;

            case cMatchClr:
                MatchClr();			// clear the match buffer.
                break;

            case cMatchRead:
                SV.ctlFlags |= cclMatchPending;	// any Serial data is for CCL.
                for(i = 0; i < maxMatch; i++)	{	// reset match string indices
                    SV.matchStr[i].matchStrIndex	= 0;
                    SV.matchStr[i].inVarStr		= 0;
                }

                NextInt(&i);	// get the timeout value
                if( i > 0 ) {
                    // post read to serial driver and set timer:
                    running = 0;	// stop running script til match or timeout
                    ScheduleTimer(kMatchReadTimer, i * 100);
                    StartRead();
                }
                break;

            case cMatchStr:
                result = MatchStr();		// add a string to the match buffer
                if (result) {			// bad command and/or matchstr index
                    running = 0;
                    SV.ctlFlags &= ~cclPlaying;
                    SV.theAbortErr = result;	// for WrapScript()->ScriptComplete().
                    terminate(result);
                }
                break;

            case cPause:
                NextInt(&i);
                if (i > 0) {
                    lval = SV.pauseTimer = i;
                    lval *= 100; // get it in milliseconds
                    usleep(lval * 1000);
                }
                break;

            case cReturn:
                if (SV.topOfStack == cclNestingLimit) {
                    running = 0;
                    SV.ctlFlags &= ~cclPlaying;
                    SV.theAbortErr = cclErr_SubroutineOverFlow;	// for WrapScript()->ScriptComplete().
                    terminate(cclErr_SubroutineOverFlow);
                }
                else
                    SV.scriptLine = SV.stack[SV.topOfStack++];
                break;

            case cLBreak:
                Break(LONGBREAK);			
                break;

            case cSBreak:
                Break(SHORTBREAK);
                break;

            case cMonitorLine:
                MonitorLine();
                break;

            case cDebugLevel:
                DebugLevel();
                break;

            case cSerReset:
                SerReset();
                break;

            case cSetSpeed:
                SetSpeed();
                break;

            case cSetTries:
                NextInt(&i);
                SV.loopCounter = i;
                break;

            case cUserHook:
                UserHook();			// get an event and pass it up to the Client.
                break;

            case cWrite:
                running = Write();
                break;

            default:		// "!", "@CCLSCRIPT", "@ORIGINATE", "@ANSWER", "@HANGUP", "@LABEL" (cmd 1-6).
            {
				break;
			}

        }/* end SWITCH */
    }/* end WHILE running */
}

/* --------------------------------------------------------------------------
Copy the match string into the approptiate MatchStr variable
-------------------------------------------------------------------------- */
int MatchStr()
{
    u_int32_t		matchIndex, i;
    TPMatchStrInfo	matchInfo;
    int			result;

    result = NextInt(&matchIndex);			// Get the matchStr index
    if (result == 0) {
        matchIndex--;
        if ((matchIndex < 0) || (matchIndex >= maxMatch)) {	// index out of bounds, abort the script play
            return cclErr_MatchStrIndxErr;
        }
        
        matchInfo = &SV.matchStr[matchIndex];
        // get script line index from specified label
        result = NextInt(&i);
        if (result == 0) {
            matchInfo->matchLine = SV.labels[i - 1];

            SkipBlanks();

            /* determine string terminators - default is space, tab, comma, semicolon */
            if ((SV.scriptLinePtr[SV.scriptLineIndex] == chrDblQuote)
                || (SV.scriptLinePtr[SV.scriptLineIndex] == chrQuote)) {
                matchInfo->delimiterChar = SV.scriptLinePtr[SV.scriptLineIndex];
                SV.scriptLineIndex++;
            }
            else
                matchInfo->delimiterChar = 0;
            matchInfo->matchStr = &SV.scriptLinePtr[SV.scriptLineIndex];
        }
    }

    return result;
}

/* --------------------------------------------------------------------------
Clear all the match strings
-------------------------------------------------------------------------- */
void MatchClr()
{
    int			matchIndex;
    TPMatchStrInfo	matchInfo;

    for (matchIndex = 0; matchIndex < maxMatch; matchIndex++) {
        matchInfo = &SV.matchStr[matchIndex];
        matchInfo->matchStr		= 0;
        matchInfo->matchStrIndex	= 0;
        matchInfo->delimiterChar	= 0;
        matchInfo->varStr		= 0;
        matchInfo->inVarStr		= 0;
        matchInfo->varStrIndex		= 0;
        matchInfo->matchLine		= 0;
    }
}

/* --------------------------------------------------------------------------
Search each match string, at it's current index, for a match with
newChar.  If newChar matches, andvance the strings index and see if
if the whole string has been matched.  Else, reset the match string's
index.  Returns which matchstring matched, if any
-------------------------------------------------------------------------- */
int MatchFind(u_int8_t newChar)
{
    int			i, matchFound;
    TPMatchStrInfo	matchInfo = NULL;
    char 		text[256];
    u_int8_t		matchStrChar, c;

    matchFound = 0;								// assume no match found
    newChar &= 0x7f;							// some PADs have bogus high bit
    for (i = 0; i < maxMatch && !matchFound; i++) {
        matchInfo = &SV.matchStr[i];
       if (matchInfo->matchStr) {		// we have a matchStr, so test it
            if (matchInfo->inVarStr) {
                matchStrChar = matchInfo->varStr[matchInfo->varStrIndex];
                matchInfo->varStrIndex++;
                if (matchStrChar == chrBackSlash) {
                    matchStrChar = matchInfo->varStr[matchInfo->varStrIndex];
                    matchInfo->varStrIndex++;
                    if ((matchStrChar !=  chrBackSlash)
                        && (matchStrChar != chrCaret)) {
                        if (matchStrChar == 'x') {
                            c = matchInfo->matchStr[matchInfo->varStrIndex++];
                            matchStrChar = ((c - ((c <= '9') ? '0' : ('A' - 10))) * 16);
                            c = matchInfo->matchStr[matchInfo->varStrIndex++];
                            matchStrChar += (c - ((c <= '9') ? '0' : ('A' - 10)));
                       }
                       else {
                            matchStrChar = (matchStrChar - 0x30) * 10;
                            matchStrChar +=  matchInfo->varStr[matchInfo->varStrIndex] - 0x30;
                            matchInfo->varStrIndex++;
                        }
                    }
                }
                if (matchInfo->varStrIndex == (matchInfo->varStrSize + 1))
                    matchInfo->inVarStr = 0;
            }
            else {
                matchStrChar = matchInfo->matchStr[matchInfo->matchStrIndex];
                matchInfo->matchStrIndex++;			// move index to next char
                if (matchStrChar == chrBackSlash) {
                    matchStrChar = matchInfo->matchStr[matchInfo->matchStrIndex];
                    matchInfo->matchStrIndex++;		// update index for next char
                    if ((matchStrChar != chrBackSlash)
                        && (matchStrChar != chrCaret)) {
                        if (matchStrChar == 'x') {
                            c = matchInfo->matchStr[matchInfo->matchStrIndex++];
                            matchStrChar = ((c - ((c <= '9') ? '0' : ('A' - 10))) * 16);
                            c = matchInfo->matchStr[matchInfo->matchStrIndex++];
                            matchStrChar += (c - ((c <= '9') ? '0' : ('A' - 10)));
                       }
                       else {
                            matchStrChar = (matchStrChar - 0x30) * 10;
                            matchStrChar += matchInfo->matchStr[matchInfo->matchStrIndex] - 0x30;
                            matchInfo->matchStrIndex++;
                        }
                    }
                }
                else if (matchStrChar == chrCaret) {
                    u_int8_t	vs;

                   matchStrChar = matchInfo->matchStr[matchInfo->matchStrIndex];
                    matchInfo->matchStrIndex++;		// skip past var string index
                    if (matchStrChar == '*')
                        vs = vsAsk;
                    else if (matchStrChar == 'u' ||  matchStrChar == 'U')
                        vs = vsUserName;
                    else if (matchStrChar == 'p' ||  matchStrChar == 'P')
                        vs = vsPassWord;
                    else {
                        vs = matchStrChar - '0';
                        matchStrChar = matchInfo->matchStr[matchInfo->matchStrIndex];
                        if (matchStrChar >= '0' && matchStrChar <= '9') {
                            matchInfo->matchStrIndex++;		// skip past var string index
                            vs = 10 * vs + matchStrChar - '0';
                        }
                    }
                    matchInfo->varStr = GetVarString(vs);
                    if (matchInfo->varStr) {
                        matchStrChar = matchInfo->varStr[1];
                        matchInfo->varStrIndex	= 2;
                        matchInfo->inVarStr	= 1;
                        matchInfo->varStrSize	= matchInfo->varStr[0];
                        if( (matchInfo->varStrSize + 1) == matchInfo->varStrIndex)
                            // we are done with the var string, clear inVarStr flag
                            matchInfo->inVarStr = 0;
                    }
                    else {
                        matchStrChar = matchInfo->matchStr[matchInfo->matchStrIndex];
                        matchInfo->matchStrIndex++;
                    }
                }
            }

            //printf("MatchFind, expect = %d, newchar = %d\n", matchStrChar, newChar);
            //syslog(LOG_INFO, "MatchFind, expect = 0x%x '%c', newchar = 0x%x '%c'\n", matchStrChar, matchStrChar, newChar, newChar);
            if (newChar == matchStrChar) {		// check to see if whole string matched
                if (!matchInfo->inVarStr)
                    switch (matchInfo->matchStr[ matchInfo->matchStrIndex ]) {
                        // special delimiters
                        case chrDblQuote:
                        case chrQuote:
                            if (matchInfo->delimiterChar)
                                matchFound = i + 1;
                            break;

                            // default delimiters
                        case chrHT:
                        case chrSpace:
                        case chrComma:
                        case chrSemiColon:
                            if (!matchInfo->delimiterChar)
                                matchFound = i + 1;
                            break;

                        case chrCR:
                        case chrNL:
                            matchFound = i + 1;
                            break;
                    }/* end SWITCH */
            }
            else {		// newChar does not match, reset matchInfo
                matchInfo->matchStrIndex = 0;
                matchInfo->inVarStr = 0;
                matchInfo->varStr = 0;
            }
        }
    }

    if (matchFound) {
        int x;

        if (matchInfo->inVarStr) {
            for (x = 0; x < matchInfo->varStrIndex; x++)
                VerboseBuffer[x+1] = matchInfo->varStr[x];
            VerboseBuffer[0] = x;
        }
        else {
            for (x = 0; x < matchInfo->matchStrIndex; x++)
               VerboseBuffer[x+1] = matchInfo->matchStr[x];
            VerboseBuffer[0] = x;
        }

        //
        // Use the kUserMsgFAny flag. That allows the message
        // resource to determine the destination and log level.
        //

        bcopy(&VerboseBuffer[1], &text[0], VerboseBuffer[0]);
        text[VerboseBuffer[0]] = 0;
        if (verbose) {
			sLog("CCLMatched : %s", text);
        }
    }

    return matchFound;
}

/* --------------------------------------------------------------------------
-------------------------------------------------------------------------- */
void Break(u_int32_t len)
{
    /*
     Man pages : 
     The tcsendbreak function transmits a continuous stream of zero-valued
    bits for four-tenths of a second to the terminal referenced by fd. The
    len parameter is ignored in this implementation. */
    /* we need to fix the question of Short break vs. Long Break */

    tcsendbreak(outfd, len);
}

/* --------------------------------------------------------------------------
A macro to return a Boolean result for a given baud number. Only those
listed are valid
-------------------------------------------------------------------------- */
#define IsValidBaudRate(a)		\
        (	((a) == 300)	||		\
                ((a) == 600)	||		\
                ((a) == 1200)	||		\
                ((a) == 1800)	||		\
                ((a) == 2400)	||		\
                ((a) == 3600)	||		\
                ((a) == 4800)	||		\
                ((a) == 7200)	||		\
                ((a) == 9600)	||		\
                ((a) == 14400)	||		\
                ((a) == 19200)  ||      \
                ((a) == 28800)  ||      \
                ((a) == 38400)  ||      \
                ((a) == 57600)  ||      \
                ((a) == 115200) ||      \
                ((a) == 230400) ||      \
                ((a) == 460800) ||      \
                ((a) == 921600))

/* --------------------------------------------------------------------------
Reset the Serial driver transmission rate, parity, number of data bits,
and number of stop bits as specified on the command line
-------------------------------------------------------------------------- */
void SerReset(void)
{
    struct termios 	tios;
    u_int32_t 		temp;

    if (tcgetattr(infd, &tios) < 0)
        return;

    // Get the baud rate from the script and filter out invalid values.
    NextInt(&temp);
    SV.serialSpeed	= IsValidBaudRate(temp) ? temp : 9600;
    // sLog("SerReset: setting serial speed to %d", SV.serialSpeed);
    cfsetispeed(&tios, SV.serialSpeed);
    cfsetospeed(&tios, SV.serialSpeed);

    // Get the parity setting from the script;
    NextInt(&temp);
    switch (temp) {
        case 1:
            tios.c_cflag |= PARENB + PARODD;     // parity enable + odd parity
            break;
        case 2:
            tios.c_cflag |= PARENB;
            tios.c_cflag &= ~PARODD;     // parity enable + even parity
            break;
        default:
            tios.c_cflag &= ~(PARENB + PARODD);     // parity enable + even parity
            break;
    }

    // Get the data bits setting from the script.
    tios.c_cflag &= ~CSIZE;     // parity enable + even parity
    NextInt(&temp);
    switch (temp) {
        case 5:
            tios.c_cflag |= CS5;     // 5 data bits
            break;
        case 6:
            tios.c_cflag |= CS6;     // 6 data bits
            break;
        case 7:
            tios.c_cflag |= CS7;     // 7 data bits
            break;
        default:
            tios.c_cflag |= CS8;     // 8 data bits
    }

    // Get the stop bits setting...
    NextInt(&temp);
    switch (temp) {
        case 2:
            tios.c_cflag |= CSTOPB;     // 2 stop bits
            break;
        default:
            tios.c_cflag &= ~CSTOPB;     // 1 stop bit
            break;
    }

    // set the same settings for inout and output fd
    tcsetattr(infd, TCSAFLUSH, &tios);
    tcsetattr(outfd, TCSAFLUSH, &tios);
}

/* --------------------------------------------------------------------------
Set the CLOCAL flag to receive or not SIGHUP when the line drops
-------------------------------------------------------------------------- */
void MonitorLine()
{
    struct termios 	tios;
    u_int32_t 		temp;

    if (tcgetattr(infd, &tios) < 0)
        return;

    // get the value. 
    // bit mask : 1st flag -> on/off line drop 
    NextInt(&temp);
    if (temp)
        tios.c_cflag &= ~CLOCAL;
    else 
        tios.c_cflag |= CLOCAL;

    // set the same settings for inout and output fd
    tcsetattr(infd, TCSAFLUSH, &tios);
    tcsetattr(outfd, TCSAFLUSH, &tios);
}

/* --------------------------------------------------------------------------
Set the debug level for the script
-------------------------------------------------------------------------- */
void DebugLevel()
{
    u_int32_t 		temp;

    // get the value. 
    // bit mask : 1st flag -> print incoming data 
    NextInt(&temp);

    debuglevel = temp;
}

/* --------------------------------------------------------------------------
*	Get the serial speed from the CCL script and send it down as an
*	XTI option to the layer below.
*
*	Called by TCCLScript::RunScript().
*
*	NOTE on OT handling of speeds:
*	For speeds that return false from the IsValidBaudRate macro, the OT
*	serial module will usually return T_PARTSUCCESS and return the value
*	it likes closest to the requested value.  For OT 1.1 the exceptions are:
*		14400: sets rate to 19,200, returns T_PARTSUCCESS
*		any speed btw 100k and 230k: sets rate to 19,200, returns T_FAILURE
*		230,400: T_SUCCESS
*		any speed other speed btw 230k and 235,930, sets rate to 230,400 and
*			returns T_PARTSUCCESS
*		any speed > 235,930, set rate to 19,200 and returns T_FAILURE
-------------------------------------------------------------------------- */
void SetSpeed(void)
{
    u_int32_t 		temp;
    struct termios 	tios;

    NextInt(&temp);
    SV.serialSpeed = IsValidBaudRate(temp) ? temp : 2400;

    if (tcgetattr(infd, &tios) >= 0) {
        cfsetispeed(&tios, SV.serialSpeed);
        cfsetospeed(&tios, SV.serialSpeed);
        tcsetattr(infd, TCSAFLUSH, &tios);
        tcsetattr(outfd, TCSAFLUSH, &tios);
    }
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
CFStringRef copyUserLocalizedString(CFBundleRef bundle,
    CFStringRef key, CFStringRef value, CFArrayRef userLanguages) 
{
    CFStringRef 	result = NULL, errStr= NULL;
    CFDictionaryRef 	stringTable;
    CFDataRef 		tableData;
    SInt32 		errCode;
    CFURLRef 		tableURL;
    CFArrayRef		locArray, prefArray;

    if (userLanguages == NULL)
        return CFBundleCopyLocalizedString(bundle, key, value, NULL);

    if (key == NULL)
        return (value ? CFRetain(value) : CFRetain(CFSTR("")));

    locArray = CFBundleCopyBundleLocalizations(bundle);
    if (locArray) {
        prefArray = CFBundleCopyLocalizationsForPreferences(locArray, userLanguages);
        if (prefArray) {
            if (CFArrayGetCount(prefArray)) {
                tableURL = CFBundleCopyResourceURLForLocalization(bundle, CFSTR("Localizable"), CFSTR("strings"), NULL, 
                                    CFArrayGetValueAtIndex(prefArray, 0));
                if (tableURL) {
                    if (CFURLCreateDataAndPropertiesFromResource(NULL, tableURL, &tableData, NULL, NULL, &errCode)) {
                        stringTable = CFPropertyListCreateFromXMLData(NULL, tableData, kCFPropertyListImmutable, &errStr);
                        if (errStr)
                            CFRelease(errStr);
                        if (stringTable) {
                            result = CFDictionaryGetValue(stringTable, key);
                            if (result)
                                CFRetain(result);
                            CFRelease(stringTable);
                        }
                        CFRelease(tableData);
                    }
                    CFRelease(tableURL);
                }
            }
            CFRelease(prefArray);
        }
        CFRelease(locArray);
    }
        
    if (result == NULL)
        result = (value && !CFEqual(value, CFSTR(""))) ?  CFRetain(value) : CFRetain(key);
    
    return result;
}

/* --------------------------------------------------------------------------
-------------------------------------------------------------------------- */
bool localizeStringWithBundle(u_char* inString, u_char* outString, CFIndex outSize, CFURLRef curURL)
{
	bool retVal= false;
	if(curURL && inString && outString)
	{
		CFStringRef			ref, loggedInUser, msg;
		CFPropertyListRef	langRef;
		CFBundleRef			bdl;

		loggedInUser = SCDynamicStoreCopyConsoleUser(NULL, 0, 0);
		if (loggedInUser) 
		{
			CFPreferencesSynchronize(kCFPreferencesAnyApplication, loggedInUser, kCFPreferencesAnyHost);
			langRef = CFPreferencesCopyValue(CFSTR("AppleLanguages"), kCFPreferencesAnyApplication, 
							loggedInUser, kCFPreferencesAnyHost);
			if (langRef) 
			{
				ref = CFStringCreateWithPascalString(NULL, inString, kCFStringEncodingUTF8);
				if (ref) 
				{
					bdl = CFBundleCreate(0, curURL);
					if (bdl) 
					{
						msg = copyUserLocalizedString(bdl, ref, ref, langRef);
						if (msg) 
						{
							retVal= CFStringGetPascalString(msg, outString, outSize, kCFStringEncodingUTF8);
							CFRelease(msg);
						}
						CFRelease(bdl);
					}
					CFRelease(ref);
				}
				CFRelease(langRef);
			}
			CFRelease(loggedInUser);
		}
	}
	return retVal;
}

/* --------------------------------------------------------------------------
-------------------------------------------------------------------------- */
void localizeString(u_char* inString, u_char* outString, CFIndex outSize)
{
	bool success = true;
	outString[0]= 0;

	if(!localizeStringWithBundle(inString, outString, outSize, cclBundleURL))
	{
		if(!localizeStringWithBundle(inString, outString, outSize, appBundleURL))
		{
			success = localizeStringWithBundle(inString, outString, outSize, localBundleURL);
		}
	}

	// if we couldn't localize, just copy the non-localized version
	// (the routines above used to do that, but they're busted)
	if (!success)
		bcopy(inString, outString, inString[0]+1);
}

#pragma mark -

/* --------------------------------------------------------------------------
pass a string up to the Client, along with where it should be displayed
-------------------------------------------------------------------------- */
void Note()
{
    u_char 	text[256];
	u_char	localText[256];
    u_int32_t	msgDestination, msgLevel = 0;	// code -> destination
    CFStringRef	ref;
    
    SkipBlanks();
    /* first get the string, do not perform variable substitution */
    PrepStr(text, 0, 0, 0);
    /* then get the localized version of the string and perform variable substitution */
    localizeString(text, localText, 256);

    varSubstitution(localText, SV.strBuf, sizeof(SV.strBuf));

    NextInt(&msgLevel);				// get the destination, if present
    switch (msgLevel) {
        case 1:
            msgDestination = kUserMsgFLog;
            break;
        case 2:
        default:
            msgDestination = kUserMsgFStatus;
            break;
        case 3:
            msgDestination = kUserMsgFLog | kUserMsgFStatus;
            break;
    }

    // localText switching from P-string to C-string
    bcopy(&SV.strBuf[1], &localText[0], SV.strBuf[0]);
    localText[SV.strBuf[0]] = '\0';

    if (serviceID && (msgDestination & 2)) {
        ref = CFStringCreateWithCString(NULL, (char*) localText, kCFStringEncodingUTF8);
        if (ref) {
            publish_entry(serviceID, kSCPropNetModemNote, ref);
            CFRelease(ref);
        }
    }
    
    if (msgDestination & 1) {
		sLog("%s", localText);
    }
}


/* --------------------------------------------------------------------------
Returns "true" if entire buffer is written with character delays. Returns
"false" if write processing continues in WriteContinue because character
delays are being inserted
-------------------------------------------------------------------------- */
u_int8_t Write()
{
    u_int32_t	isVarString;
    u_int16_t	i, j;
    u_int32_t	varIndex;
    char 	text[256];

    SkipBlanks();
    PrepStr(SV.strBuf, &isVarString, &varIndex, 1);

    if (isVarString && (varIndex == vsPassWord || ((varIndex == vsAsk) && LastAskedMasked))) {
        VerboseBuffer[0] = SV.strBuf[0];
        for (i = 1; i <= SV.strBuf[0]; i++)
            VerboseBuffer[i] = '�';
    }
    else {
       for (i = 1,  j = 1; i <= SV.strBuf[0] && j < 256; i++) {
            u_int8_t c = SV.strBuf[i];
            if (c < 0x20) {
                VerboseBuffer[j++] = '\\';
                VerboseBuffer[j++] = '0' + c / 10;
                VerboseBuffer[j++] = '0' + c % 10;
            }
            else {
                VerboseBuffer[j++] = c;
            }
        }
        VerboseBuffer[0] = j - 1;
    }

	bcopy(&VerboseBuffer[1], &text[0], VerboseBuffer[0]);
	text[VerboseBuffer[0]] = 0;
    if (verbose) {
		sLog("CCLWrite : %s", text);
    }
    
    //
    // skip the pascal string length byte
    //
    SV.writeBufIndex = 1;

    if (SV.chrDelayValue) {
        // complete asynchronously through WriteContinue
        WriteContinue();
        ScheduleTimer(kCharDelayTimer, SV.chrDelayValue);
        return 0;
    }
    else {
    
        write(outfd, &SV.strBuf[1], SV.strBuf[0]);
		if(signalerror) 
			terminate(signalerror);
        return 1;
    }
}

/* --------------------------------------------------------------------------
Continue the current write command.  This routine is called when
the char delay timer expires
-------------------------------------------------------------------------- */
void WriteContinue()
{

    //syslog(LOG_INFO, " ----> delayed '%c'\n", SV.strBuf[SV.writeBufIndex]);

    write(outfd, &SV.strBuf[SV.writeBufIndex], 1);
	if(signalerror)
		terminate(signalerror);
    if (SV.writeBufIndex < SV.strBuf[0] )		// if this char is not the last char
        SV.writeBufIndex++;			// 		bump index to the next char

    return;
}

/* --------------------------------------------------------------------------
the timer scheduled by fScript -> ScriptStartTimer() has expired.
only one timer can be active at a time.  the timer type specified in
ScriptStartTimer is fed into TimerExpired as the long parameter
-------------------------------------------------------------------------- */
void TimerExpired(long type)
{

    if (SV.ctlFlags & cclPlaying) {
        switch (type)  {

            case kMatchReadTimer:
                StopRead();
                RunScript();					// go get the next ccl cmd
                break;

            case kCharDelayTimer:
                if (SV.writeBufIndex == SV.strBuf[0] ) {		// this is the last char to write
                    WriteContinue();			// write the char
                    RunScript();				// go get the next ccl cmd
                }
                else {
                    if (SV.writeBufIndex < SV.strBuf[0]) {	// this char is not the last char
                        WriteContinue();	// write the char
                        ScheduleTimer(kCharDelayTimer, SV.chrDelayValue);
                    }
                }

                break;
        }
    }
}

/* --------------------------------------------------------------------------
-------------------------------------------------------------------------- */
u_int8_t Ask()
{
    u_int32_t			maskflag, flags = 0, label;
    CFStringRef			ref, resp;
    CFUserNotificationRef 	alert;
    CFOptionFlags 		alertflags;
    CFMutableDictionaryRef 	dict;
    SInt32 			error;
    CFMutableArrayRef 		array;
    u_char 		        text[256];
    u_char 		        localText[256];
    
    if (alertname[0] == 0)
        return 0;	// no alert to display

#define kCmdAskAllowCancel 	1
#define kCmdAskAllowEntry 	2
#define kCmdAskMaskEntry 	4

    if (NextInt(&maskflag))
        maskflag = 0;

    SkipBlanks();
    /* first get the string, do not perform variable substitution */
    PrepStr(text, 0, 0, 0);
    
    /* then get the localized version of the string and perform variable substitution */
    localizeString(text, localText, 256);
	
    varSubstitution(localText, SV.strBuf, sizeof(SV.strBuf));

    if (NextInt(&label)) {
        SV.askLabel = 0;
        flags &= ~kCmdAskAllowCancel;
    }
    else {
        SV.askLabel = (u_int16_t) label;
        flags |=  kCmdAskAllowCancel;
    }

    // default: allow user data entry
    flags |= kCmdAskAllowEntry;                         
    switch (maskflag) {
        // echo user's input
        case 0:    	
            flags &= ~kCmdAskMaskEntry;   
            break;    
        // mask user's input with bullets
        case 1:    
            flags |=  kCmdAskMaskEntry;              
            // Set the fLastAskMasked here (gcg)
            LastAskedMasked = maskflag;
            break;
        // do not allow data entry
        case 2:    
            flags &= ~kCmdAskAllowEntry;  
            break;    
    }

    ref = CFStringCreateWithPascalString(NULL, SV.strBuf, kCFStringEncodingUTF8);
    if (ref) {
        dict = CFDictionaryCreateMutable(NULL, 0, 
                &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if (dict) {
        
            alertflags = 0;

            if (alertNameRef) 
                CFDictionaryAddValue(dict, kCFUserNotificationAlertHeaderKey, alertNameRef);
            
            // it is either a text field, or a message
            if (flags & kCmdAskAllowEntry) {
                array = CFArrayCreateMutable(NULL, 0, NULL);   
                if (array) {
                    CFArrayAppendValue(array, ref);
                    CFDictionaryAddValue(dict, kCFUserNotificationTextFieldTitlesKey, array);
                    CFRelease(array);
                    if (flags & kCmdAskMaskEntry)
                        alertflags = CFUserNotificationSecureTextField(0);
                }
            }
            else 
                CFDictionaryAddValue(dict, kCFUserNotificationAlertMessageKey, ref);

            if (cancelNameRef && (flags & kCmdAskAllowCancel)) 
                CFDictionaryAddValue(dict, kCFUserNotificationAlternateButtonTitleKey, cancelNameRef);
                    
            if (iconURL) 
                CFDictionaryAddValue(dict, kCFUserNotificationIconURLKey, iconURL);

            if (localBundleURL)
                CFDictionaryAddValue(dict, kCFUserNotificationLocalizationURLKey, localBundleURL);
            
            alert = CFUserNotificationCreate(NULL, 0, alertflags, &error, dict);
            if (alert) {
                CFUserNotificationReceiveResponse(alert, 0, &alertflags);
                // the 2 lower bits of the response flags will give the button pressed
                // 0 --> default
                // 1 --> alternate
                // 3 --> exit on time out
                if ((alertflags & 3) == 1) {
                    // user cancelled
                    if (SV.askLabel) {
                        SV.scriptLine = SV.labels[SV.askLabel - 1];
                        SV.askLabel = 0;
                    }
                }
                else { 
                    // user clicked OK
                    if (flags & kCmdAskAllowEntry) {
                        resp = CFUserNotificationGetResponseValue(alert, kCFUserNotificationTextFieldValuesKey, 0);
                        if (resp) {
                            CFStringGetPascalString(resp, text, sizeof(text), kCFStringEncodingUTF8);
                            SetVarString(vsAsk, text);
                        }
                    }
                }
                
                CFRelease(alert);
            }
            CFRelease(dict);
        }
        CFRelease(ref);
    }

    return 0;
}

/* --------------------------------------------------------------------------
report modem speed & negotiated compression to client
-------------------------------------------------------------------------- */
void CommunicatingAt()
{
    u_int32_t		speed;
    CFNumberRef		num;

    NextInt(&speed);		// modem speed reported by script
    sLog("Communicating at %d bps.", speed);

    if (serviceID) {
        num = CFNumberCreate(NULL, kCFNumberIntType, &speed);
        if (num) {
            publish_entry(serviceID, kSCPropNetModemConnectSpeed, num);
            CFRelease(num);
        }
    }
}

/* --------------------------------------------------------------------------
report various events to client
// UserHook 1 == claim the Serial Port for an incoming/outgoing call.
// UserHook 2 == report that the modem is doing error correction (other than MNP 10).
// UserHook 3 == request that ARA turn off data compression.
// UserHook 4 == report that the modem is doing MNP 10 error correction.
-------------------------------------------------------------------------- */
void UserHook()
{
    u_int32_t	event;			// event reported by script

    NextInt( &event );			// get the event

#if 0
    // now use a PrivateMessage to send 'event' up the Stream, to whom it may concern.

    UInt8	len = 0;
    TStreamMessage	*mp;

    len = sizeof( ARA_notify_msg ) + sizeof( UInt32 );
    mp = new ( len ) TStreamMessage;
    if( !mp )
    {
        DebugStop("\p TCCLScript::UserHook: no memory for message!");
        return;		// as if everything is OK, so we expect to fail gracefully later.
    }
    mp->Reset( len );
    mp->SetType( M_PCPROTO );

    ARA_notify_msg* PrvMsg = (ARA_notify_msg*) mp->GetDataPointer();

    PrvMsg->PRIM_type	= kARAPrvMsg;		// always.
    PrvMsg->CODE_type	= kNotifyMsg;		// always.
    PrvMsg->MODL_dst	= kAnyModuleId;		// 'any '.
    PrvMsg->MODL_src	= kScriptModuleId;	// 'scri'.
    PrvMsg->MSG_type	= kNotifyUserHook;
    PrvMsg->MSG_error	= 0;
    PrvMsg->MSG_flags	= 0;

    *((UInt32*) (PrvMsg + 1)) = event;		// put the event type just behind the header

    fScriptMod -> ScriptSendDataUp( mp );	// send this up to the Controller.
#endif
}

/* --------------------------------------------------------------------------
-------------------------------------------------------------------------- */
void ScheduleTimer(long type, u_int32_t milliseconds)
{
    if (milliseconds)
        timeout((void*)TimerExpired, (void*)type, milliseconds);
    else
        untimeout((void*)TimerExpired, (void*)type);
}

/* --------------------------------------------------------------------------
Compare the specified VarStr with the parsed string.
If the strings match, return the parsed label, else return 0
-------------------------------------------------------------------------- */
int IfStr()
{
    u_int8_t		noMatch = 0, *src;
    u_int32_t		i = 0, strIndex, labelIndex;

    NextInt( &strIndex );				// this is which varString to check

    src = GetVarString(strIndex);

    /* pull out the search string - if nil return state = continue	*/

    NextInt(&i);					// parse the jump label, if strings match
    labelIndex = SV.labels[i - 1];			// save the jump label (??)
    SkipBlanks();					// skip to the match string
    PrepStr(SV.strBuf, 0, 0, 1);				// fetch the match string

    if (equalstring(&SV.strBuf[0], src)) {	// returns TRUE if they match...
        return labelIndex;
    }
    else {
        return noMatch;
    }
}

/* --------------------------------------------------------------------------
-------------------------------------------------------------------------- */
void ReceiveMatchData(u_int8_t nextChar)
{
    int32_t		matchIndex;

    if (debuglevel & 0x1) {
        if (usestderr) {
            fprintf(stderr, "%c", nextChar);
        }
    }

    if ((matchIndex = MatchFind(nextChar)) != 0) {
        // On a match we clear the pending flag and cancel the match timer
        SV.scriptLine = SV.matchStr[--matchIndex].matchLine;
        SV.ctlFlags &= ~cclMatchPending;
        ScheduleTimer(kMatchReadTimer, 0);
	StopRead();
        RunScript();
    }
}

/* --------------------------------------------------------------------------
flush the read queue.  free all messages going upstream.
-------------------------------------------------------------------------- */
void Flush()
{
    tcflush(infd, TCIFLUSH);
}

/* --------------------------------------------------------------------------
Issue an asychronous call to the serial driver to set the handshaking options
-------------------------------------------------------------------------- */
void HSReset()
{
    u_int32_t	outputXON_OFF, outputCTS, XonChar, XoffChar, inputXON_OFF, inputDTR;
    struct termios 	tios;

    if (tcgetattr(outfd, &tios) < 0)
        return;

    NextInt(&outputXON_OFF);
    // if this bit is set, use the XON/XOFF chars for in-band, output flow control.
    // users shouldn't be doing this.  output of compressor could be the XOFF
    // flow control character, and thereby throttle the output stream.
    if (outputXON_OFF) 
        tios.c_iflag |= IXON;
    else 
        tios.c_iflag &= ~IXON;
    
    NextInt(&outputCTS);
    // if this bit is set, use CTS for out-band, output flow control.
    // ARA calls this signal outputCTS, while Open Transport calls it kOTSrlCTSInputHandshake.
    // we both mean that if the modem deasserts CTS, the Mac
    // shouldn't send it more data, until the modem reasserts CTS.
    // this scheme depends on a correctly wired modem cable.
    if (outputCTS)
        tios.c_cflag |= CCTS_OFLOW;
    else
        tios.c_cflag &= ~CCTS_OFLOW;

    // default onChar is 0x11, offChar is 0x13.
    // users should NEVER be specifying an alternate XonChar or XoffChar.
    // when the script is pre-flighted, any attempt to specify a char,
    // whether it's an alternate character or a default character, will barf.
    // therefore no error checking is done here.  assume these parms will be zero.  skip 'em.
    NextInt(&XonChar);
    NextInt(&XoffChar);
    tios.c_cc[VSTOP] = XoffChar;	/* DC3 = XOFF = ^S */
    tios.c_cc[VSTART] = XonChar;	/* DC1 = XON  = ^Q */

    NextInt(&inputXON_OFF);
    // if this bit is set, use the XON/XOFF chars for in-band, input flow control.
    // users shouldn't be doing this.  output of compressor could be the XOFF
    // flow control character, and thereby throttle the input stream.
    if (inputXON_OFF)
        tios.c_iflag |= IXOFF;
    else
        tios.c_iflag &= ~IXOFF;

    NextInt(&inputDTR);
    // if this bit is set, use DTR for out-band, input flow control.
    // ARA calls this signal inputDTR, while Open Transport calls it kOTSrlDTROutputHandshake.
    // we both mean that if the Mac deasserts DTR,
    // the modem shouldn't send it more data, until the Mac reasserts DTR.
    // this scheme depends on a correctly wired modem cable.
    if (inputDTR)
        tios.c_cflag |= CRTS_IFLOW;
    else
        tios.c_cflag &= ~CRTS_IFLOW;

    // set the same settings for inout and output fd
    tcsetattr(infd, TCSAFLUSH, &tios);
    tcsetattr(outfd, TCSAFLUSH, &tios);
}

/* --------------------------------------------------------------------------
Issue an asychronous call to the serial driver to set
DTR or Clear DTR, depending on the opcode passed in
-------------------------------------------------------------------------- */
void DTRCommand(short DTRCode)
{
    ioctl(infd, DTRCode == DTR_SET ? TIOCSDTR : TIOCCDTR);
}

/* --------------------------------------------------------------------------
Send up a user notification about the current CCL exit error.
-------------------------------------------------------------------------- */
static char* cclErrStrings[] =
{
    "internal error used to abort match read",	// cclErr_AbortMatchRead = -6000
    "Bad parameter given to the engine",		// cclErr_BadParameter = -6001
    "Duplicate label",							// cclErr_DuplicateLabel = -6002
    "Label undefined",							// cclErr_LabelUndefined = -6003
    "Subroutine overflow",					// cclErr_SubroutineOverFlow = -6004
    "No memory...",								// cclErr_NoMemErr = -6005
    "CCL error base",						// (in the middle?) cclErr = -6006
    "There is at least one script open",		// cclErr_CloseError = -6007
    "Script Canceled",						// cclErr_ScriptCancelled = -6008
    "Script contains too many lines",			// cclErr_TooManyLines = -6009
    "Script contains too many characters",		// cclErr_ScriptTooBig = -6010
    "CCL has not been initialized",				// cclErr_NotInitialized = -6011
    "Cancel in progress.",					// cclErr_CancelInProgress = -6012
    "Play command already in progress.",		// cclErr_PlayInProgress = -6013
    "Exit with no error.",						// cclErr_ExitOK = -6014
    "Label out of range.",						// cclErr_BadLabel = -6015
    "Bad command.",								// cclErr_BadCommand = -6016
    "End of script reached, expecting Exit.",	// cclErr_EndOfScriptErr = -6017
    "Match string index is out of bounds.",	// cclErr_MatchStrIndxErr = -6018
    "Modem error, modem not responding.",		// cclErr_ModemErr = -6019
    "No dial tone.",							// cclErr_NoDialTone = -6020
    "No carrier.",								// cclErr_NoCarrierErr = -6021
    "Line busy.",								// cclErr_LineBusyErr = -6022
    "No answer.",								// cclErr_NoAnswerErr = -6023
    "No @ORIGINATE label",					// cclErr_NoOriginateLabel = -6024
    "No @ANSWER label",							// cclErr_NoAnswerLabel = -6025
    "No @HANGUP label",							// cclErr_NoHangUpLabel = -6026
    "Can't connect because number is empty.",	// cclErr_NoNumberErr = -6027
    "Incorrect script for the modem."			// cclErr_BadScriptErr = -6028 
};

void terminate(int exitError)
{
    //u_long m = exitError;
#if 0
    CFNumberRef		num;
#endif
#if 1
    if (verbose || exitError != 0) {
		char *errStr = NULL;

		if (exitError == cclErr_DuplicateLabel) {
			// null terminate the P-string :P
			if (SV.strBuf[0]) {
				SV.strBuf[SV.strBuf[0]+1] = '\0';
				errStr = (char*)(&SV.strBuf[0] + 1);
			} else {
				// empty custom error
				errStr = "<no custom error>";
			}
		} else if (exitError <= cclErr_AbortMatchRead &&
				exitError >= cclErr_BadScriptErr)
			errStr = cclErrStrings[cclErr_AbortMatchRead - exitError];

		if (errStr)
			sLog("CCLExit: %d (%s)", exitError, errStr);
		else
			sLog("CCLExit: %d", exitError);
    }
#endif
    //if (ppplink != -1) 
    //    sys_send_confd(csockfd, PPP_CCLRESULT, (u_char *)&m, sizeof(m), ppplink, 0);

     // connect and listen mode always publish last cause
     // disconnect mode publish only non-null cause
     // last cause displays connection error, even when plays disconnect sequence
#if 0
    if (serviceID && (exitError || (enginemode != mode_disconnect))) {
        num = CFNumberCreate(NULL, kCFNumberIntType, &exitError);
        if (num) {
            publish_entry(serviceID, kSCPropNetPPPLastCause, num);
            CFRelease(num);
        }
    }
#endif

    /* unpublish unnecessary information */
    if (serviceID) {
        unpublish_entry(serviceID, kSCPropNetModemNote);
    }

    close(infd);
    close(outfd);
    exit(exitError);
}

/* --------------------------------------------------------------------------
* timeout - Schedule a timeout.
* Note that this timeout takes the number of milliseconds, NOT hz (as in
* the kernel).
-------------------------------------------------------------------------- */
void timeout(void (*func)(void *), void *arg, u_long time)
{
    struct callout *newp, *p, **pp;

    /* Allocate timeout. */
    if ((newp = (struct callout *) malloc(sizeof(struct callout))) == NULL)
        terminate(cclErr_NoMemErr);

    newp->c_arg = arg;
    newp->c_func = func;
    gettimeofday(&timenow, NULL);
    newp->c_time.tv_sec = timenow.tv_sec + time/1000;
    newp->c_time.tv_usec = timenow.tv_usec + ((time%1000)*1000);
    if (newp->c_time.tv_usec >= 1000000) {
        newp->c_time.tv_sec++;
        newp->c_time.tv_usec -= 1000000;
    }
        
    /*
     * Find correct place and link it in.
     */
    for (pp = &callout; (p = *pp); pp = &p->c_next)
        if (newp->c_time.tv_sec < p->c_time.tv_sec
            || (newp->c_time.tv_sec == p->c_time.tv_sec
                && newp->c_time.tv_usec < p->c_time.tv_usec))
            break;
    newp->c_next = p;
    *pp = newp;
}


/* --------------------------------------------------------------------------
* untimeout - Unschedule a timeout.
-------------------------------------------------------------------------- */
void untimeout(void (*func)(void *), void *arg)
{
    struct callout **copp, *freep;

    /*
     * Find first matching timeout and remove it from the list.
     */
    for (copp = &callout; (freep = *copp); copp = &freep->c_next)
        if (freep->c_func == func && freep->c_arg == arg) {
            *copp = freep->c_next;
            //printf("untimeout [ppp%d]\n", ((struct fsm *)arg)->unit);
            free((char *) freep);
            break;
        }
}

/* --------------------------------------------------------------------------
* calltimeout - Call any timeout routines which are now due.
-------------------------------------------------------------------------- */
void calltimeout()
{
    struct callout *p;

    while (callout != NULL) {
        p = callout;

        if (gettimeofday(&timenow, NULL) < 0)
            terminate(cclErr_NoMemErr);

        if (!(p->c_time.tv_sec < timenow.tv_sec
              || (p->c_time.tv_sec == timenow.tv_sec
                  && p->c_time.tv_usec <= timenow.tv_usec)))
            break;		/* no, it's not time yet */

        callout = p->c_next;
        (*p->c_func)(p->c_arg);

        free((char *) p);
    }
}

/* --------------------------------------------------------------------------
* timeleft - return the length of time until the next timeout is due.
-------------------------------------------------------------------------- */
struct timeval *timeleft(struct timeval *tvp)
{
    if (callout == NULL)
        return NULL;

    gettimeofday(&timenow, NULL);
    tvp->tv_sec = callout->c_time.tv_sec - timenow.tv_sec;
    tvp->tv_usec = callout->c_time.tv_usec - timenow.tv_usec;
    if (tvp->tv_usec < 0) {
        tvp->tv_usec += 1000000;
        tvp->tv_sec -= 1;
    }
    if (tvp->tv_sec < 0)
        tvp->tv_sec = tvp->tv_usec = 0;

    return tvp;
}

/* -----------------------------------------------------------------------------
publish a dictionnary entry in the cache
----------------------------------------------------------------------------- */
int publish_entry(u_char *serviceid, CFStringRef entry, CFTypeRef value)
{
    CFMutableDictionaryRef	dict;
    CFStringRef			key;
    SCDynamicStoreRef		cfgCache;
    CFPropertyListRef		ref;
    int				ret = 0;

    if (cfgCache = SCDynamicStoreCreate(0, CFSTR("CCLEngine"), 0, 0)) {
        if (key = SCDynamicStoreKeyCreate(0, CFSTR("%@/%@/%@/%s/%@"), 
                    kSCDynamicStoreDomainState, kSCCompNetwork, kSCCompService, serviceid, kSCEntNetModem)) {

            if (ref = SCDynamicStoreCopyValue(cfgCache, key)) {
                dict = CFDictionaryCreateMutableCopy(0, 0, ref);
                CFRelease(ref);
            }
            else
                dict = CFDictionaryCreateMutable(0, 0, 
                            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

            if (dict) {
                CFDictionarySetValue(dict, entry, value);
                ret = SCDynamicStoreSetValue(cfgCache, key, dict);
                CFRelease(dict);
            }

            CFRelease(key);
        }
        CFRelease(cfgCache);
    }
    return ret;
}

/* -----------------------------------------------------------------------------
unpublish a disctionnary entry from the cache
----------------------------------------------------------------------------- */
int unpublish_entry(u_char *serviceid, CFStringRef entry)
{
    CFPropertyListRef		ref;
    CFMutableDictionaryRef	dict;
    CFStringRef			key;
    SCDynamicStoreRef		cfgCache;
    int 			ret = 0;
    
    if (cfgCache = SCDynamicStoreCreate(0, CFSTR("CCLEngine"), 0, 0)) {
            
        if (key = SCDynamicStoreKeyCreate(0, CFSTR("%@/%@/%@/%s/%@"), 
                    kSCDynamicStoreDomainState, kSCCompNetwork, kSCCompService, serviceid, kSCEntNetModem)) {
        
            if (ref = SCDynamicStoreCopyValue(cfgCache, key)) {
                if (dict = CFDictionaryCreateMutableCopy(0, 0, ref)) {
                    CFDictionaryRemoveValue(dict, entry);
                    ret = SCDynamicStoreSetValue(cfgCache, key, dict);
                    CFRelease(dict);
                }
                CFRelease(ref);
            }
            CFRelease(key);
        }
        CFRelease(cfgCache);
    }
    return ret;
}


