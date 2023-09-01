// REQUIRES: asserts

// RUN: %empty-directory(%t)
// RUN: %empty-directory(%t-scratch)
// RUN: %target-swift-frontend -emit-module -o %t/def_macros.swiftmodule %S/Inputs/def_macros.swift -module-name def_macros -enable-experimental-feature Macros
// RUN: %target-swift-frontend -typecheck -I%t -verify %s -verify-ignore-unknown  -enable-experimental-feature Macros
// REQUIRES: OS=macosx

import def_macros

func test(a: Int, b: Int) {
  _ = #publicStringify(a + b)

  _ = #internalStringify(a + b)
  // expected-error@-1{{macro 'internalStringify' is undefined}}
}
