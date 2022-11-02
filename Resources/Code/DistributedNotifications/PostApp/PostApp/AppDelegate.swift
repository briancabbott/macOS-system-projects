//
//  AppDelegate.swift
//  PostApp
//
//  Created by 寺家 篤史 on 2018/12/12.
//  Copyright © 2018 Yumemi Inc. All rights reserved.
//

import Cocoa

@NSApplicationMain
class AppDelegate: NSObject, NSApplicationDelegate {

    @IBOutlet weak var window: NSWindow!
    @IBOutlet weak var field: NSTextField!

    func applicationDidFinishLaunching(_ aNotification: Notification) {
        // Insert code here to initialize your application
    }

    func applicationWillTerminate(_ aNotification: Notification) {
        // Insert code here to tear down your application
    }

    @IBAction func post(_ sender: NSButton) {
        let userInfo: [AnyHashable : Any]? = field.stringValue.isEmpty ? nil : ["Message" : field.stringValue]
        DistributedNotificationCenter.default().postNotificationName(NSNotification.Name("DistributedNotifications.Post"), object: nil, userInfo: userInfo, deliverImmediately: true)
    }
}

