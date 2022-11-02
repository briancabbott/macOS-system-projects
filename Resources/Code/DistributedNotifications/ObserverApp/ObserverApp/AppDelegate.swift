//
//  AppDelegate.swift
//  ObserverApp
//
//  Created by 寺家 篤史 on 2018/12/12.
//  Copyright © 2018 Yumemi Inc. All rights reserved.
//

import Cocoa

@NSApplicationMain
class AppDelegate: NSObject, NSApplicationDelegate {

    @IBOutlet weak var window: NSWindow!
    @IBOutlet weak var label: NSTextField!

    func applicationDidFinishLaunching(_ aNotification: Notification) {
        DistributedNotificationCenter.default().addObserver(self, selector: #selector(distributedNotificationsPost), name: NSNotification.Name("DistributedNotifications.Post"), object: nil)
    }

    func applicationWillTerminate(_ aNotification: Notification) {
        // Insert code here to tear down your application
    }

    @objc private func distributedNotificationsPost(notification: Notification) {
        if let message = notification.userInfo?["Message"] as? String {
            label.stringValue = message
        }
    }
}

