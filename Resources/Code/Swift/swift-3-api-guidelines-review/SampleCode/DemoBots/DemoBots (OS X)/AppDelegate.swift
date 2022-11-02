/*
    Copyright (C) 2015 Apple Inc. All Rights Reserved.
    See LICENSE.txt for this sample’s licensing information
    
    Abstract:
    Application delegate for the OS X version of DemoBots.
*/

import Cocoa

@NSApplicationMain
class AppDelegate: Object, NSApplicationDelegate {
    func applicationShouldTerminateAfterLastWindowClosed(sender: NSApplication) -> Bool {
        return true
    }
}
