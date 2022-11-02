//
//  file: logging.m
//  project: BlockBlock (shared)
//  description: logging functions
//
//  created by Patrick Wardle
//  copyright (c) 2017 Objective-See. All rights reserved.
//

#import "consts.h"
#import "logging.h"

//global log file handle
NSFileHandle* logFileHandle = nil;

//get path to log file
NSString* logFilePath()
{
    return [INSTALL_DIRECTORY stringByAppendingPathComponent:LOG_FILE_NAME];
}


//log a msg
// default to syslog, and if an err msg, to disk
void logMsg(int level, NSString* msg)
{
    //flag for logging
    BOOL shouldLog = NO;
    
    //log prefix
    NSMutableString* logPrefix = nil;
    
    //first grab logging flag
    shouldLog = (LOG_TO_FILE == (level & LOG_TO_FILE));
    
    //then remove it
    // make sure syslog is happy
    level &= ~LOG_TO_FILE;
    
    //alloc/init
    // always start w/ name + pid
    logPrefix = [NSMutableString stringWithFormat:@"%s(%d)", LAUNCH_DAEMON.UTF8String, getpid()];
    
    //if its error, add error to prefix
    if(LOG_ERR == level)
    {
        //add
        [logPrefix appendString:@" ERROR"];
    }
    
    //debug mode logic
    #ifdef DEBUG
    
    //in debug mode promote debug msgs to LOG_NOTICE
    // OS X only shows LOG_NOTICE and above
    if(LOG_DEBUG == level)
    {
        //promote
        level = LOG_NOTICE;
    }
    
    #endif
    
    //dump to syslog?
    // function can be invoked just to log to file...
    if(0 != level)
    {
        //syslog
        syslog(level, "%s: %s", [logPrefix UTF8String], [msg UTF8String]);
    }
    
    //log to file?
    if(YES == shouldLog)
    {
        //log
        log2File(msg);
    }
    
    return;
}

//log to file
void log2File(NSString* msg)
{
    //sanity check
    if(nil == logFileHandle) return;
    
    //sync
    @synchronized(logFileHandle)
    {
        //wrap in try
        @try
        {
            //append timestamp
            // write msg out to disk
            [logFileHandle writeData:[[NSString stringWithFormat:@"%@: %@\n", [NSDate date], msg] dataUsingEncoding:NSUTF8StringEncoding]];
        }
        //ignore exceptions
        @catch (NSException *e)
        {
            ;
        }
    }
    
    return;
}

//de-init logging
void deinitLogging()
{
    //dbg msg
    // ->and to file
    logMsg(LOG_DEBUG|LOG_TO_FILE, @"logging ending");
    
    //sync
    @synchronized(logFileHandle)
    {
        //close file handle
        [logFileHandle closeFile];
        
        //unset
        logFileHandle = nil;
    }
    
    return;
}

//prep/open log file
BOOL initLogging(NSString* logPath)
{
    //ret var
    BOOL bRet = NO;
    
    //first time
    //  create file
    if(YES != [[NSFileManager defaultManager] fileExistsAtPath:logPath])
    {
        //create
        if(YES != [[NSFileManager defaultManager] createFileAtPath:logPath contents:nil attributes:nil])
        {
            //err msg
            logMsg(LOG_ERR, [NSString stringWithFormat:@"failed to create log file, %@", logPath]);
            
            //bail
            goto bail;
        }
    }
    
    //get file handle
    logFileHandle = [NSFileHandle fileHandleForWritingAtPath:logPath];
    if(nil == logFileHandle)
    {
        //err msg
        logMsg(LOG_ERR, [NSString stringWithFormat:@"failed to get log file handle to %@", logPath]);
        
        //bail
        goto bail;
    }
    
    //dbg msg
    logMsg(LOG_DEBUG, [NSString stringWithFormat:@"opened log file; %@", logPath]);
    
    //seek to end
    [logFileHandle seekToEndOfFile];
    
    //dbg msg
    // ->and to file
    logMsg(LOG_DEBUG|LOG_TO_FILE, @"logging intialized");
    
    //happy
    bRet = YES;
    
bail:
    
    return bRet;
}
