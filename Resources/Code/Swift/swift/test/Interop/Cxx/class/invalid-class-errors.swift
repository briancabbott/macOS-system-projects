// RUN: rm -rf %t
// RUN: split-file %s %t
// RUN: not %target-swift-frontend -typecheck -I %t/Inputs  %t/test.swift  -enable-experimental-cxx-interop 2>&1 | %FileCheck %s

//--- Inputs/module.modulemap
module Test {
    header "test.h"
    requires cplusplus
}

//--- Inputs/test.h
struct A {
        A(const A&) = delete;
};

struct __attribute__((swift_attr("import_unsafe"))) B {
    B(const B&) = delete;
};

namespace Namespace {
struct Nested {
  Nested(const Nested&) = delete;
};
}

//--- test.swift

import Test

// CHECK: note: record 'A' is not automatically importable: does not have a copy constructor or destructor. Refer to the C++ Interop User Manual to classify this type.
public func test(x: A) { }
// CHECK: note: record 'B' is not automatically importable: does not have a copy constructor or destructor. Refer to the C++ Interop User Manual to classify this type.
public func test(x: B) { }
// CHECK: note: record 'Nested' is not automatically importable: does not have a copy constructor or destructor. Refer to the C++ Interop User Manual to classify this type.
public func test(x: Namespace.Nested) { }