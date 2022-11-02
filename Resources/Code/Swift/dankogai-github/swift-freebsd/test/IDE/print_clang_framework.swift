// RUN: rm -rf %t
// RUN: mkdir -p %t

// RUN: %target-swift-ide-test(mock-sdk: -F %S/Inputs/mock-sdk) -print-module -source-filename %s -module-to-print=Foo -function-definitions=false -print-regular-comments > %t/Foo.printed.txt
// RUN: diff -u %S/Inputs/mock-sdk/Foo.printed.txt %t/Foo.printed.txt

// RUN: %target-swift-ide-test(mock-sdk: -F %S/Inputs/mock-sdk) -print-interface -print-module -source-filename %s -module-to-print=Foo -function-definitions=false -print-regular-comments > %t/Foo.interface.printed.txt
// RUN: FileCheck %s -check-prefix=INTERFACE1 < %t/Foo.interface.printed.txt

// RUN: %target-swift-ide-test(mock-sdk: -F %S/Inputs/mock-sdk) -print-module -source-filename %s -module-to-print=Foo -function-definitions=false -prefer-type-repr=true -module-print-submodules > %t/Foo.printed.recursive.txt
// RUN: diff -u %S/Inputs/mock-sdk/Foo.printed.recursive.txt %t/Foo.printed.recursive.txt

// RUN: %target-swift-ide-test(mock-sdk: -F %S/Inputs/mock-sdk) -print-module -source-filename %s -module-to-print=Foo.FooSub -function-definitions=false -prefer-type-repr=true > %t/Foo.FooSub.printed.txt
// RUN: diff -u %S/Inputs/mock-sdk/Foo.FooSub.printed.txt %t/Foo.FooSub.printed.txt

// RUN: %target-swift-ide-test(mock-sdk: -F %S/Inputs/mock-sdk) -print-module -source-filename %s -module-to-print=FooHelper -function-definitions=false -prefer-type-repr=true > %t/FooHelper.printed.txt
// RUN: diff -u %S/Inputs/mock-sdk/FooHelper.printed.txt %t/FooHelper.printed.txt

// RUN: %target-swift-ide-test(mock-sdk: -F %S/Inputs/mock-sdk) -print-module -source-filename %s -module-to-print=FooHelper.FooHelperSub -function-definitions=false -prefer-type-repr=true > %t/FooHelper.FooHelperSub.printed.txt
// RUN: diff -u %S/Inputs/mock-sdk/FooHelper.FooHelperSub.printed.txt %t/FooHelper.FooHelperSub.printed.txt

// RUN: %target-swift-ide-test(mock-sdk: -F %S/Inputs/mock-sdk) -print-module -source-filename %s -module-to-print=FooHelper.FooHelperExplicit -function-definitions=false -prefer-type-repr=true > %t/FooHelper.FooHelperExplicit.printed.txt
// RUN: diff -u %S/Inputs/mock-sdk/FooHelper.FooHelperExplicit.printed.txt %t/FooHelper.FooHelperExplicit.printed.txt

// RUN: %target-swift-ide-test(mock-sdk: -F %S/Inputs/mock-sdk) -print-module -source-filename %s -module-to-print=Foo -function-definitions=false -prefer-type-repr=true -annotate-print > %t/Foo.annotated.txt
// RUN: diff -u %S/Inputs/mock-sdk/Foo.annotated.txt %t/Foo.annotated.txt

// RUN: %target-swift-frontend(mock-sdk: %clang-importer-sdk -I %t) -emit-module -o %t %S/../Inputs/clang-importer-sdk/swift-modules/Foundation.swift
// RUN: %target-swift-ide-test(mock-sdk: -sdk %S/../Inputs/clang-importer-sdk -I %t) -print-module -source-filename %s -module-to-print=Foundation -function-definitions=false -prefer-type-repr=true > %t.printed.txt
// RUN: FileCheck %s -check-prefix=FOUNDATION -strict-whitespace < %t.printed.txt

// This test is in general platform-independent, but it happens to check
// printing of @available attributes for OS X, and those are not printed on
// iOS.
//
// FIXME: split OS X parts into a separate test.
//
// REQUIRES: OS=macosx

// FOUNDATION-LABEL: {{^}}/// Aaa.  NSAvailableOnOSX10_10AndIOS8_0.  Bbb.
// FOUNDATION-NEXT: {{^}}@available(OSX 10.10, *){{$}}

// FOUNDATION-LABEL: {{^}}/// Aaa.  NSPotentiallyUnavailableOptions.  Bbb.
// FOUNDATION-NEXT: {{^}}@available(OSX 10.10, *){{$}}
// FOUNDATION-NEXT: {{^}}struct NSPotentiallyUnavailableOptions : OptionSetType {{{$}}

// FOUNDATION-LABEL: {{^}}/// Aaa.  NSOptionsWithUnavailableElement.  Bbb.
// FOUNDATION-NEXT: {{^}}struct NSOptionsWithUnavailableElement : OptionSetType {{{$}}
// FOUNDATION-NEXT: {{^}}  init(rawValue: UInt){{$}}
// FOUNDATION-NEXT: {{^}}  let rawValue: UInt{{$}}
// FOUNDATION-NEXT: {{^}}  static var First: NSOptionsWithUnavailableElement { get }{{$}}
// FOUNDATION-NEXT: {{^}}  static var Second: NSOptionsWithUnavailableElement { get }{{$}}
// FOUNDATION-NEXT: {{^}}  @available(OSX 10.10, *){{$}}
// FOUNDATION-NEXT: {{^}}  static var Third: NSOptionsWithUnavailableElement { get }{{$}}

// FOUNDATION-LABEL: {{^}}/// Aaa.  NSUnavailableEnum.  Bbb.
// FOUNDATION-NEXT: {{^}}@available(OSX 10.10, *){{$}}
// FOUNDATION-NEXT: {{^}}enum NSUnavailableEnum : UInt {{{$}}

// FOUNDATION-LABEL: {{^}}/// Aaa.  NSEnumWithUnavailableElement.  Bbb.
// FOUNDATION-NEXT: {{^}}enum NSEnumWithUnavailableElement : UInt {{{$}}
// FOUNDATION-NEXT: {{^}}  init?(rawValue: UInt){{$}}
// FOUNDATION-NEXT: {{^}}  var rawValue: UInt { get }{{$}}
// FOUNDATION-NEXT: {{^}}  case First{{$}}
// FOUNDATION-NEXT: {{^}}  case Second{{$}}
// FOUNDATION-NEXT: {{^}}  @available(OSX 10.10, *){{$}}
// FOUNDATION-NEXT: {{^}}  case Third{{$}}

// FOUNDATION-LABEL: {{^}}/// Aaa.  UnannotatedFrameworkProtocol.  Bbb.
// FOUNDATION-NEXT: {{^}}protocol UnannotatedFrameworkProtocol {{{$}}
// FOUNDATION-NEXT: {{^}}  @available(OSX 10.10, *){{$}}
// FOUNDATION-NEXT: {{^}}  func doSomethingWithClass(k: AnnotatedFrameworkClass?){{$}}
// FOUNDATION-NEXT: {{^}}  @available(OSX 10.10, *){{$}}
// FOUNDATION-NEXT: {{^}}  func doSomethingWithNonNullableClass(k: AnnotatedFrameworkClass){{$}}
// FOUNDATION-NEXT: {{^}}  @available(OSX 10.10, *){{$}}
// FOUNDATION-NEXT: {{^}}  func doSomethingWithIUOClass(k: AnnotatedFrameworkClass!){{$}}
// FOUNDATION-NEXT: {{^}}  @available(OSX 10.10, *){{$}}
// FOUNDATION-NEXT: {{^}}  func returnSomething() -> AnnotatedFrameworkClass?{{$}}
// FOUNDATION-NEXT: {{^}}  func noUnavailableTypesInSignature(){{$}}
// FOUNDATION-NEXT: {{^}}  @available(OSX 10.11, *){{$}}
// FOUNDATION-NEXT: {{^}}  func doSomethingWithClass(k: AnnotatedFrameworkClass, andLaterClass lk: AnnotatedLaterFrameworkClass){{$}}
// FOUNDATION-NEXT: {{^}}  @available(OSX 10.12, *)
// FOUNDATION-NEXT: {{^}}  func someMethodWithAvailability()
// FOUNDATION-NEXT: {{^}}  @available(OSX 10.10, *){{$}}
// FOUNDATION-NEXT: {{^}}  var someProperty: AnnotatedFrameworkClass { get set }{{$}}

// FOUNDATION-LABEL: {{^}}/// Aaa.  AnnotatedFrameworkProtocol.  Bbb.
// FOUNDATION-NEXT: {{^}}@available(OSX 10.9, *){{$}}
// FOUNDATION-NEXT: {{^}}protocol AnnotatedFrameworkProtocol {{{$}}
// FOUNDATION-NEXT: {{^}}  func returnSomething() -> AnnotatedFrameworkClass?{{$}}

// FOUNDATION-LABEL: /// Aaa.  FrameworkClassConformingToUnannotatedFrameworkProtocol.  Bbb.
// FOUNDATION-NEXT: {{^}}class FrameworkClassConformingToUnannotatedFrameworkProtocol : NSObject, UnannotatedFrameworkProtocol {{{$}}
// FOUNDATION-NEXT: {{^}}  init(){{$}}
// FOUNDATION-NEXT: {{^}}  @available(OSX 10.10, *){{$}}
// FOUNDATION-NEXT: {{^}}  func doSomethingWithClass(k: AnnotatedFrameworkClass?){{$}}
// FOUNDATION-NEXT: {{^}}  @available(OSX 10.10, *){{$}}
// FOUNDATION-NEXT: {{^}}  func doSomethingWithNonNullableClass(k: AnnotatedFrameworkClass){{$}}
// FOUNDATION-NEXT: {{^}}  @available(OSX 10.10, *){{$}}
// FOUNDATION-NEXT: {{^}}  func doSomethingWithIUOClass(k: AnnotatedFrameworkClass!){{$}}
// FOUNDATION-NEXT: {{^}}  @available(OSX 10.10, *){{$}}
// FOUNDATION-NEXT: {{^}}  func returnSomething() -> AnnotatedFrameworkClass?{{$}}
// FOUNDATION-NEXT: {{^}}  func noUnavailableTypesInSignature(){{$}}
// FOUNDATION-NEXT: {{^}}  @available(OSX 10.11, *){{$}}
// FOUNDATION-NEXT: {{^}}  func doSomethingWithClass(k: AnnotatedFrameworkClass, andLaterClass lk: AnnotatedLaterFrameworkClass){{$}}
// FOUNDATION-NEXT: {{^}}  @available(OSX 10.12, *)
// FOUNDATION-NEXT: {{^}}  func someMethodWithAvailability()
// FOUNDATION-NEXT: {{^}}  @available(OSX 10.10, *){{$}}
// FOUNDATION-NEXT: {{^}}  var someProperty: AnnotatedFrameworkClass{{$}}

// FOUNDATION-LABEL: /// Aaa.  LaterFrameworkClassConformingToUnannotatedFrameworkProtocol.  Bbb.
// FOUNDATION-NEXT: {{^}}@available(OSX 10.11, *){{$}}
// FOUNDATION-NEXT: {{^}}class LaterFrameworkClassConformingToUnannotatedFrameworkProtocol : NSObject, UnannotatedFrameworkProtocol {{{$}}
// FOUNDATION-NEXT: {{^}}  init(){{$}}
// FOUNDATION-NEXT: {{^}}  @available(OSX 10.11, *){{$}}
// FOUNDATION-NEXT: {{^}}  func doSomethingWithClass(k: AnnotatedFrameworkClass?){{$}}
// FOUNDATION-NEXT: {{^}}  @available(OSX 10.11, *){{$}}
// FOUNDATION-NEXT: {{^}}  func doSomethingWithNonNullableClass(k: AnnotatedFrameworkClass){{$}}
// FOUNDATION-NEXT: {{^}}  @available(OSX 10.11, *){{$}}
// FOUNDATION-NEXT: {{^}}  func doSomethingWithIUOClass(k: AnnotatedFrameworkClass!){{$}}
// FOUNDATION-NEXT: {{^}}  @available(OSX 10.11, *){{$}}
// FOUNDATION-NEXT: {{^}}  func returnSomething() -> AnnotatedFrameworkClass?{{$}}
// FOUNDATION-NEXT: {{^}}  @available(OSX 10.11, *){{$}}
// FOUNDATION-NEXT: {{^}}  func noUnavailableTypesInSignature(){{$}}
// FOUNDATION-NEXT: {{^}}  @available(OSX 10.11, *){{$}}
// FOUNDATION-NEXT: {{^}}  func doSomethingWithClass(k: AnnotatedFrameworkClass, andLaterClass lk: AnnotatedLaterFrameworkClass){{$}}
// FOUNDATION-NEXT: {{^}}  @available(OSX 10.12, *)
// FOUNDATION-NEXT: {{^}}  func someMethodWithAvailability()
// FOUNDATION-NEXT: {{^}}  @available(OSX 10.11, *){{$}}
// FOUNDATION-NEXT: {{^}}  var someProperty: AnnotatedFrameworkClass{{$}}
}

// FOUNDATION-LABEL: /// Aaa.  FrameworkClassConformingToLaterAnnotatedFrameworkProtocol.  Bbb.
// FOUNDATION-NEXT: {{^}}class FrameworkClassConformingToLaterAnnotatedFrameworkProtocol : NSObject, LaterAnnotatedFrameworkProtocol {
// FOUNDATION-NEXT: {{^}}  init()
// FOUNDATION-NEXT: {{^}}  @available(OSX 10.11, *)
// FOUNDATION-NEXT: {{^}}  func returnSomething() -> AnnotatedFrameworkClass?
// FOUNDATION-NEXT: {{^}}  @available(OSX 10.11, *)
// FOUNDATION-NEXT: {{^}}  func doSomethingWithClass(k: AnnotatedFrameworkClass, andLaterClass lk: AnnotatedLaterFrameworkClass)
// FOUNDATION-NEXT: {{^}}  @available(OSX 10.11, *)
// FOUNDATION-NEXT: {{^}}  func noUnavailableTypesInSignature()
// FOUNDATION-NEXT: {{^}}  @available(OSX 10.12, *)
// FOUNDATION-NEXT: {{^}}  func someMethodWithAvailability()
// FOUNDATION-NEXT: {{^}}  @available(OSX 10.11, *)
// FOUNDATION-NEXT: {{^}}  var someProperty: AnnotatedFrameworkClass
}

// INTERFACE1-NOT: unavailable
// INTERFACE1-NOT: This comment should not show without decl.
