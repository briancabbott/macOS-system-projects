//
//  file: XPCListener
//  project: BlockBlock (launch daemon)
//  description: XPC listener for connections for user components (header)
//
//  created by Patrick Wardle
//  copyright (c) 2017 Objective-See. All rights reserved.
//


@import Foundation;
#import "XPCDaemonProto.h"

@interface XPCListener : NSObject <NSXPCListenerDelegate>
{
    
}

/* PROPERTIES */

//XPC listener
@property(nonatomic, retain)NSXPCListener* listener;

//XPC connection for login item
@property(weak)NSXPCConnection* client;

/* METHODS */

//setup XPC listener
-(BOOL)initListener;

//automatically invoked
// allows NSXPCListener to configure/accept/resume a new incoming NSXPCConnection
// note: we only allow binaries signed by Objective-See to talk to this!
-(BOOL)listener:(NSXPCListener *)listener shouldAcceptNewConnection:(NSXPCConnection *)newConnection;

@end
