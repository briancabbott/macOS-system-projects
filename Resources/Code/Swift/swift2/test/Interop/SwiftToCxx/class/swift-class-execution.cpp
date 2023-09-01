// RUN: %empty-directory(%t)

// RUN: %target-swift-frontend %S/swift-class-in-cxx.swift -typecheck -module-name Class -clang-header-expose-decls=all-public -emit-clang-header-path %t/class.h

// RUN: %target-interop-build-clangxx -c %s -I %t -o %t/swift-class-execution.o
// RUN: %target-interop-build-swift %S/swift-class-in-cxx.swift -o %t/swift-class-execution -Xlinker %t/swift-class-execution.o -module-name Class -Xfrontend -entry-point-function-name -Xfrontend swiftMain

// RUN: %target-codesign %t/swift-class-execution
// RUN: %target-run %t/swift-class-execution | %FileCheck %s

// RUN: %empty-directory(%t-evo)

// RUN: %target-swift-frontend %S/swift-class-in-cxx.swift -typecheck -module-name Class -clang-header-expose-decls=all-public -enable-library-evolution -emit-clang-header-path %t-evo/class.h

// RUN: %target-interop-build-clangxx -c %s -I %t-evo -o %t-evo/swift-class-execution.o
// RUN: %target-interop-build-swift %S/swift-class-in-cxx.swift -o %t-evo/swift-class-execution-evo -Xlinker %t-evo/swift-class-execution.o -module-name Class -enable-library-evolution -Xfrontend -entry-point-function-name -Xfrontend swiftMain

// RUN: %target-codesign %t-evo/swift-class-execution-evo
// RUN: %target-run %t-evo/swift-class-execution-evo | %FileCheck %s

// REQUIRES: executable_test

#include <assert.h>
#include "class.h"
#include <cstdio>

extern "C" size_t swift_retainCount(void * _Nonnull obj);

size_t getRetainCount(const Class::ClassWithIntField & swiftClass) {
  void *p = swift::_impl::_impl_RefCountedClass::getOpaquePointer(swiftClass);
  return swift_retainCount(p);
}

int main() {
  using namespace Class;

  // Ensure that the class is released.
  {
    auto x = returnClassWithIntField();
    assert(getRetainCount(x) == 1);
  }
// CHECK:      init ClassWithIntField
// CHECK-NEXT: destroy ClassWithIntField
    
  {
    auto x = returnClassWithIntField();
    {
      takeClassWithIntField(x);
      assert(getRetainCount(x) == 1);
      auto x2 = passThroughClassWithIntField(x);
      assert(getRetainCount(x) == 2);
      assert(getRetainCount(x2) == 2);
      takeClassWithIntField(x2);
      assert(getRetainCount(x) == 2);
    }
    assert(getRetainCount(x) == 1);
    takeClassWithIntField(x);
  }
// CHECK-NEXT: init ClassWithIntField
// CHECK-NEXT: ClassWithIntField: 0;
// CHECK-NEXT: ClassWithIntField: 42;
// CHECK-NEXT: ClassWithIntField: 42;
// CHECK-NEXT: destroy ClassWithIntField

  {
    auto x = returnClassWithIntField();
    assert(getRetainCount(x) == 1);
    takeClassWithIntFieldInout(x);
    assert(getRetainCount(x) == 1);
    takeClassWithIntField(x);
  }
// CHECK-NEXT: init ClassWithIntField
// CHECK-NEXT: init ClassWithIntField
// CHECK-NEXT: destroy ClassWithIntField
// CHECK-NEXT: ClassWithIntField: -11;
// CHECK-NEXT: destroy ClassWithIntField

  {
    auto x = returnClassWithIntField();
    {
      auto x2 = x;
      assert(getRetainCount(x) == 2);
    }
    assert(getRetainCount(x) == 1);
  }
// CHECK-NEXT: init ClassWithIntField
// CHECK-NEXT: destroy ClassWithIntField

  {
    auto x = returnClassWithIntField();
    {
      auto x2 = returnClassWithIntField();
      assert(getRetainCount(x2) == 1);
      assert(getRetainCount(x) == 1);
      x = x2;
      assert(getRetainCount(x) == 2);
    }
    takeClassWithIntField(x);
    assert(getRetainCount(x) == 1);
  }
// CHECK-NEXT: init ClassWithIntField
// CHECK-NEXT: init ClassWithIntField
// CHECK-NEXT: destroy ClassWithIntField
// CHECK-NEXT: ClassWithIntField: 0;
// CHECK-NEXT: destroy ClassWithIntField
  return 0;
}
