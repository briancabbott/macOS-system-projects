// RUN: %target-swift-frontend -parse -verify %s

// XFAIL: linux

import Foundation

// Swift and Foundation types should work.

func available_Int(a: Int) {}
func available_NSDateFormatter(a: NSDateFormatter) {}


// Some traditional Objective-C types should fail with fixits.

func unavailable_id(a: id) {} // expected-error {{use of undeclared type 'id'; did you mean to use 'AnyObject'?}} {{24-26=AnyObject}}
func unavailable_Class(a: Class) {} // expected-error {{use of undeclared type 'Class'; did you mean to use 'AnyClass'?}} {{27-32=AnyClass}}
func unavailable_BOOL(a: BOOL) {} // expected-error {{use of undeclared type 'BOOL'; did you mean to use 'ObjCBool'?}} {{26-30=ObjCBool}}
func unavailable_SEL(a: SEL) {} // expected-error {{use of undeclared type 'SEL'; did you mean to use 'Selector'?}} {{25-28=Selector}}
func unavailable_NSUInteger(a: NSUInteger) {} // expected-error {{use of undeclared type 'NSUInteger'; did you mean to use 'Int'?}} {{32-42=Int}} expected-note {{did you mean to use 'UInt'?}} {{32-42=UInt}}

