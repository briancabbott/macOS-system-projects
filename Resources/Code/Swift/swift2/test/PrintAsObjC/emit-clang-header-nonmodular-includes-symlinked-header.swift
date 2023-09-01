// REQUIRES: objc_interop

// RUN: %empty-directory(%t)
// RUN: %target-swift-frontend(mock-sdk: %clang-importer-sdk) -typecheck -Xcc -fmodule-map-file=%S/Inputs/custom-modules/module.map -Xcc -I%S/Inputs/custom-modules/header_subdirectory/ -I%S/Inputs/custom-modules/ -emit-objc-header-path %t/textual-imports.h -emit-clang-header-nonmodular-includes %s
// RUN: %FileCheck %s < %t/textual-imports.h

// Make sure the include we get is not based on the resolved symlink, but rather the path to the symlink itself.

import EmitClangHeaderNonmodularIncludesStressTest

public class Bar : Foo {}


// CHECK:      #if __has_feature(objc_modules)
// CHECK-NEXT: #if __has_warning("-Watimport-in-framework-header")
// CHECK-NEXT: #pragma clang diagnostic ignored "-Watimport-in-framework-header"
// CHECK-NEXT: #endif
// CHECK-NEXT: @import EmitClangHeaderNonmodularIncludesStressTest;
// CHECK-NEXT: #else
// CHECK: #import <header-symlink.h>
// CHECK: #endif
