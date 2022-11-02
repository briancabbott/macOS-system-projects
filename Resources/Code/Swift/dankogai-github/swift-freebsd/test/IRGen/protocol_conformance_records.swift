// RUN: rm -rf %t && mkdir %t
// RUN: %build-irgen-test-overlays
// RUN: %target-swift-frontend(mock-sdk: -sdk %S/Inputs -I %t) -emit-module -o %t %S/Inputs/objc_protocols_Bas.swift
// RUN: %target-swift-frontend(mock-sdk: -sdk %S/Inputs -I %t) -primary-file %s -emit-ir | FileCheck %s
// RUN: %target-swift-frontend(mock-sdk: -sdk %S/Inputs -I %t) %s -emit-ir -num-threads 8 | FileCheck %s

// XFAIL: linux

import gizmo

protocol Runcible {
  func runce()
}

// CHECK-LABEL: @"\01l_protocol_conformances" = private constant [

// CHECK:         %swift.protocol_conformance {
// -- protocol descriptor
// CHECK:           [[RUNCIBLE:%swift.protocol\* @_TMp28protocol_conformance_records8Runcible]]
// -- type metadata
// CHECK:           @_TMfV28protocol_conformance_records15NativeValueType
// -- witness table
// CHECK:           @_TWPV28protocol_conformance_records15NativeValueTypeS_8Runcible
// -- flags 0x01: unique direct metadata
// CHECK:           i32 1
// CHECK:         },
struct NativeValueType: Runcible {
  func runce() {}
}

// -- TODO class refs should be indirected through their ref variable
// CHECK:         %swift.protocol_conformance {
// -- protocol descriptor
// CHECK:           [[RUNCIBLE]]
// -- class object (TODO should be class ref variable)
// CHECK:           @_TMfC28protocol_conformance_records15NativeClassType
// -- witness table
// CHECK:           @_TWPC28protocol_conformance_records15NativeClassTypeS_8Runcible
// -- flags 0x01: unique direct metadata (TODO should be 0x03 indirect class)
// CHECK:           i32 1
// CHECK:         },
class NativeClassType: Runcible {
  func runce() {}
}

// CHECK:         %swift.protocol_conformance {
// -- protocol descriptor
// CHECK:           [[RUNCIBLE]]
// -- generic metadata pattern
// CHECK:           @_TMPV28protocol_conformance_records17NativeGenericType
// -- witness table
// CHECK:           @_TWPurGV28protocol_conformance_records17NativeGenericTypex_S_8RuncibleS_
// -- flags 0x04: unique direct generic metadata pattern
// CHECK:           i32 4
// CHECK:         },
struct NativeGenericType<T>: Runcible {
  func runce() {}
}

// CHECK:         %swift.protocol_conformance {
// -- protocol descriptor
// CHECK:           [[RUNCIBLE]]
// -- type metadata
// CHECK:           @_TMVSC6NSRect
// -- witness table
// CHECK:           @_TWPVSC6NSRect28protocol_conformance_records8Runcible
// -- flags 0x02: nonunique direct metadata
// CHECK:           i32 2
// CHECK:         },
extension NSRect: Runcible {
  func runce() {}
}

// -- TODO class refs should be indirected through their ref variable
// CHECK:         %swift.protocol_conformance {
// -- protocol descriptor
// CHECK:           [[RUNCIBLE]]
// -- class object (TODO should be class ref variable)
// CHECK:           @"got.OBJC_CLASS_$_Gizmo"
// -- witness table
// CHECK:           @_TWPCSo5Gizmo28protocol_conformance_records8Runcible
// -- flags 0x01: unique direct metadata (TODO should be 0x03 indirect class)
// CHECK:           i32 1
// CHECK:         },
extension Gizmo: Runcible {
  func runce() {}
}

// CHECK:         %swift.protocol_conformance {
// -- protocol descriptor
// CHECK:           [[RUNCIBLE]]
// -- type metadata
// CHECK:           @got._TMSi
// -- witness table
// CHECK:           @_TWPSi28protocol_conformance_records8Runcible
// -- flags 0x01: unique direct metadata
// CHECK:           i32 1
// CHECK:         }
extension Int: Runcible {
  func runce() {}
}

// TODO: conformances that need lazy initialization


