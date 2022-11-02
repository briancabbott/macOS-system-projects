//
//  file: XPCDaemonClient.h
//  project: lulu (shared)
//  description: talk to daemon via XPC (header)
//
//  created by Patrick Wardle
//  copyright (c) 2017 Objective-See. All rights reserved.
//

@import Foundation;

#import "XPCDaemonProto.h"

@interface XPCDaemonClient : NSObject
{
    
}

//xpc connection to daemon
@property (atomic, strong, readwrite)NSXPCConnection* daemon;

//get preferences
// note: synchronous
-(NSDictionary*)getPreferences;

//update (save) preferences
// note: synchronous, as then returns latest preferences
-(NSDictionary*)updatePreferences:(NSDictionary*)preferences;

//get rules
// note: synchronous
-(NSDictionary*)getRules;

//add rule
-(void)addRule:(NSDictionary*)info;

//delete rule
-(void)deleteRule:(NSString*)key rule:(NSString*)uuid;

//uninstall
-(BOOL)uninstall;

@end
