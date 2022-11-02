//
//  file: XPCProtocol.h
//  project: BlockBlock (shared)
//  description: protocol for talking to the daemon
//
//  created by Patrick Wardle
//  copyright (c) 2017 Objective-See. All rights reserved.
//

#ifndef userCommsInterface_h
#define userCommsInterface_h

@import Foundation;

@protocol XPCProtocol

//install
-(void)install:(NSString*)app reply:(void (^)(NSNumber*))reply;

//load/unload launch daemon
-(void)toggleDaemon:(BOOL)shouldLoad reply:(void (^)(NSNumber*))reply;

//uninstall
-(void)uninstall:(NSString*)app full:(BOOL)full reply:(void (^)(NSNumber*))reply;

//cleanup
// remove self
-(void)cleanup:(void (^)(NSNumber*))reply;

@end

#endif
