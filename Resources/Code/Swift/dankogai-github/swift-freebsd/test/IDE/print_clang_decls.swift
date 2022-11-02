// RUN: rm -rf %t
// RUN: mkdir -p %t

// XFAIL: linux

// This file deliberately does not use %clang-importer-sdk for most RUN lines.
// Instead, it generates custom overlay modules itself, and uses -I %t when it
// wants to use them.

// RUN: %target-swift-frontend(mock-sdk: %clang-importer-sdk -I %t) -emit-module -o %t %S/../Inputs/clang-importer-sdk/swift-modules/ObjectiveC.swift
// RUN: %target-swift-frontend(mock-sdk: %clang-importer-sdk -I %t) -emit-module -o %t %S/../Inputs/clang-importer-sdk/swift-modules/CoreGraphics.swift
// RUN: %target-swift-frontend(mock-sdk: %clang-importer-sdk -I %t) -emit-module -o %t %S/../Inputs/clang-importer-sdk/swift-modules/Foundation.swift
// RUN: %target-swift-ide-test(mock-sdk: -sdk %S/../Inputs/clang-importer-sdk -I %t) -print-module -source-filename %s -module-to-print=ctypes -function-definitions=false -prefer-type-repr=true > %t.printed.txt
// RUN: FileCheck %s -check-prefix=TAG_DECLS_AND_TYPEDEFS -strict-whitespace < %t.printed.txt
// RUN: FileCheck %s -check-prefix=NEGATIVE -strict-whitespace < %t.printed.txt

// RUN: %target-swift-ide-test(mock-sdk: -sdk %S/../Inputs/clang-importer-sdk -I %t) -print-module -source-filename %s -module-to-print=Foundation -function-definitions=false -prefer-type-repr=true > %t.printed.txt
// RUN: FileCheck %s -check-prefix=FOUNDATION -strict-whitespace < %t.printed.txt

// RUN: %target-swift-ide-test(mock-sdk: -sdk %S/../Inputs/clang-importer-sdk -I %t) -print-module -source-filename %s -module-to-print=ctypes.bits -function-definitions=false -prefer-type-repr=true > %t.printed.txt
// RUN: FileCheck %s -check-prefix=CTYPESBITS -strict-whitespace < %t.printed.txt

// RUN: %target-swift-ide-test(mock-sdk: -sdk %S/../Inputs/clang-importer-sdk -I %t) -print-module -source-filename %s -module-to-print=nullability -function-definitions=false -prefer-type-repr=true > %t.printed.txt
// RUN: FileCheck %s -check-prefix=CHECK-NULLABILITY -strict-whitespace < %t.printed.txt

// TAG_DECLS_AND_TYPEDEFS: {{^}}struct FooStruct1 {{{$}}
// TAG_DECLS_AND_TYPEDEFS: {{^}}  var x: Int32{{$}}
// TAG_DECLS_AND_TYPEDEFS: {{^}}  var y: Double{{$}}
// TAG_DECLS_AND_TYPEDEFS: {{^}}  init(){{$}}
// TAG_DECLS_AND_TYPEDEFS: {{^}}  init(x: Int32, y: Double){{$}}
// TAG_DECLS_AND_TYPEDEFS: {{^}}}{{$}}

// TAG_DECLS_AND_TYPEDEFS:      /*!
// TAG_DECLS_AND_TYPEDEFS-NEXT:   @keyword Foo2
// TAG_DECLS_AND_TYPEDEFS-NEXT: */
// TAG_DECLS_AND_TYPEDEFS-NEXT: {{^}}struct FooStruct2 {{{$}}
// TAG_DECLS_AND_TYPEDEFS-NEXT: {{^}}  var x: Int32{{$}}
// TAG_DECLS_AND_TYPEDEFS-NEXT: {{^}}  var y: Double{{$}}
// TAG_DECLS_AND_TYPEDEFS-NEXT: {{^}}  init(){{$}}
// TAG_DECLS_AND_TYPEDEFS-NEXT: {{^}}  init(x: Int32, y: Double){{$}}
// TAG_DECLS_AND_TYPEDEFS-NEXT: {{^}}}{{$}}

// TAG_DECLS_AND_TYPEDEFS-NEXT: {{^}}typealias FooStructTypedef1 = FooStruct2{{$}}

// TAG_DECLS_AND_TYPEDEFS-NEXT: {{^}}struct FooStructTypedef2 {{{$}}
// TAG_DECLS_AND_TYPEDEFS-NEXT: {{^}}  var x: Int32{{$}}
// TAG_DECLS_AND_TYPEDEFS-NEXT: {{^}}  var y: Double{{$}}
// TAG_DECLS_AND_TYPEDEFS-NEXT: {{^}}  init(){{$}}
// TAG_DECLS_AND_TYPEDEFS-NEXT: {{^}}  init(x: Int32, y: Double){{$}}
// TAG_DECLS_AND_TYPEDEFS-NEXT: {{^}}}{{$}}

// TAG_DECLS_AND_TYPEDEFS: {{^}}struct FooStruct3 {{{$}}
// TAG_DECLS_AND_TYPEDEFS-NEXT: {{^}}  var x: Int32{{$}}
// TAG_DECLS_AND_TYPEDEFS-NEXT: {{^}}  var y: Double{{$}}
// TAG_DECLS_AND_TYPEDEFS-NEXT: {{^}}  init(){{$}}
// TAG_DECLS_AND_TYPEDEFS-NEXT: {{^}}  init(x: Int32, y: Double){{$}}
// TAG_DECLS_AND_TYPEDEFS-NEXT: {{^}}}{{$}}

// TAG_DECLS_AND_TYPEDEFS: {{^}}struct FooStruct4 {{{$}}
// TAG_DECLS_AND_TYPEDEFS-NEXT: {{^}}  var x: Int32{{$}}
// TAG_DECLS_AND_TYPEDEFS-NEXT: {{^}}  var y: Double{{$}}
// TAG_DECLS_AND_TYPEDEFS-NEXT: {{^}}  init(){{$}}
// TAG_DECLS_AND_TYPEDEFS-NEXT: {{^}}  init(x: Int32, y: Double){{$}}
// TAG_DECLS_AND_TYPEDEFS-NEXT: {{^}}}{{$}}

// TAG_DECLS_AND_TYPEDEFS: {{^}}struct FooStruct5 {{{$}}
// TAG_DECLS_AND_TYPEDEFS-NEXT: {{^}}  var x: Int32{{$}}
// TAG_DECLS_AND_TYPEDEFS-NEXT: {{^}}  var y: Double{{$}}
// TAG_DECLS_AND_TYPEDEFS-NEXT: {{^}}  init(){{$}}
// TAG_DECLS_AND_TYPEDEFS-NEXT: {{^}}  init(x: Int32, y: Double){{$}}
// TAG_DECLS_AND_TYPEDEFS-NEXT: {{^}}}{{$}}

// TAG_DECLS_AND_TYPEDEFS: {{^}}struct FooStruct6 {{{$}}
// TAG_DECLS_AND_TYPEDEFS-NEXT: {{^}}  var x: Int32{{$}}
// TAG_DECLS_AND_TYPEDEFS-NEXT: {{^}}  var y: Double{{$}}
// TAG_DECLS_AND_TYPEDEFS-NEXT: {{^}}  init(){{$}}
// TAG_DECLS_AND_TYPEDEFS-NEXT: {{^}}  init(x: Int32, y: Double){{$}}
// TAG_DECLS_AND_TYPEDEFS-NEXT: {{^}}}{{$}}

// NEGATIVE-NOT: typealias FooStructTypedef2

// FOUNDATION-LABEL: {{^}}/// Aaa.  NSArray.  Bbb.{{$}}
// FOUNDATION-NEXT: {{^}}class NSArray : NSObject {{{$}}
// FOUNDATION-NEXT  func objectAtIndex(index: Int) -> AnyObject!

// FOUNDATION-LABEL: {{^}}/// Aaa.  NSRuncingMode.  Bbb.{{$}}
// FOUNDATION-NEXT: {{^}}enum NSRuncingMode : UInt {{{$}}
// FOUNDATION-NEXT: {{^}}  init?(rawValue: UInt){{$}}
// FOUNDATION-NEXT: {{^}}  var rawValue: UInt { get }{{$}}
// FOUNDATION-NEXT: {{^}}  case Mince{{$}}
// FOUNDATION-NEXT: {{^}}  case Quince{{$}}
// FOUNDATION-NEXT: {{^}}}{{$}}

// FOUNDATION-LABEL: {{^}}/// Aaa.  NSRuncingOptions.  Bbb.{{$}}
// FOUNDATION-NEXT: {{^}}struct NSRuncingOptions : OptionSetType {{{$}}
// FOUNDATION-NEXT: {{^}}  init(rawValue: UInt){{$}}
// FOUNDATION-NEXT: {{^}}  let rawValue: UInt{{$}}
// FOUNDATION-NEXT: {{^}}  static var EnableMince: NSRuncingOptions { get }{{$}}
// FOUNDATION-NEXT: {{^}}  static var EnableQuince: NSRuncingOptions { get }{{$}}
// FOUNDATION-NEXT: {{^}}}{{$}}

// FOUNDATION-LABEL: {{^}}/// Unavailable Global Functions{{$}}
// FOUNDATION-NEXT: @available(*, unavailable, message="Zone-based memory management is unavailable")
// FOUNDATION-NEXT: NSSetZoneName(zone: NSZone, _ name: String)

// CTYPESBITS-NOT: FooStruct1
// CTYPESBITS: {{^}}typealias DWORD = Int32{{$}}
// CTYPESBITS-NEXT: {{^}}var MY_INT: Int32 { get }{{$}}
// CTYPESBITS-NOT: FooStruct1

// CHECK-NULLABILITY: func getId1() -> AnyObject?
// CHECK-NULLABILITY: var global_id: AnyObject?
// CHECK-NULLABILITY: class SomeClass {
// CHECK-NULLABILITY:   class func methodA(obj: SomeClass?) -> AnyObject{{$}}
// CHECK-NULLABILITY:   func methodA(obj: SomeClass?) -> AnyObject{{$}}
// CHECK-NULLABILITY:   class func methodB(block: ((Int32, Int32) -> Int32)?) -> AnyObject{{$}}
// CHECK-NULLABILITY:   func methodB(block: ((Int32, Int32) -> Int32)?) -> AnyObject{{$}}
// CHECK-NULLABILITY:   func methodC() -> AnyObject?
// CHECK-NULLABILITY:   var property: AnyObject?
// CHECK-NULLABILITY:   func stringMethod() -> String{{$}}
// CHECK-NULLABILITY:   func optArrayMethod() -> [AnyObject]?
// CHECK-NULLABILITY: }
// CHECK-NULLABILITY: func compare_classes(sc1: SomeClass, _ sc2: SomeClass, _ sc3: SomeClass!)
