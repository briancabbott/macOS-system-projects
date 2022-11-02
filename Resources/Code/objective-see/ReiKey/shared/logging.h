//
//  logging.h
//  ReiKey
//
//  Created by Patrick Wardle on 12/24/18.
//  Copyright © 2018 Objective-See. All rights reserved.
//

#ifndef Logging_h
#define Logging_h

#import <syslog.h>

@import Cocoa;
@import Foundation;

//log a msg to syslog
// also disk, if error
void logMsg(int level, NSString* msg);

//prep/open log file
BOOL initLogging(NSString* logPath);

//get path to log file
NSString* logFilePath(void);

//de-init logging
void deinitLogging(void);

//log to file
void log2File(NSString* msg);

#endif
