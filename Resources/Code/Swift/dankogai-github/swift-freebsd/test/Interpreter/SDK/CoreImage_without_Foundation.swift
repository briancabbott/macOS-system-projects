// RUN: %target-run-simple-swift | FileCheck %s
// REQUIRES: executable_test

// REQUIRES: objc_interop
// UNSUPPORTED: OS=watchos

import CoreImage
// Do NOT add anything that publicly imports Foundation here!

var v = CIVector(x:7);
// CHECK: x = 7
print("x = \(v.X)")
