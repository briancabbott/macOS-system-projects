// RUN: %target-build-swift -Xfrontend %clang-importer-sdk %s -emit-ir
// REQUIRES: executable_test

// REQUIRES: objc_interop

// Test that we don't crash when producing IR.

import AppKit
class MyView: NSView {
    func drawRect() {
        var x = self.superview
        var l = self.layer
        self.layer = CALayer()
        self.nextKeyView = nil
        subviews = []
    }    
}
var m = MyView()
m.drawRect()
