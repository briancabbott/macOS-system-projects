// REQUIRES: asserts

// RUN: %empty-directory(%t)

// RUN: %target-swift-frontend -typecheck -module-name Macros -emit-module-interface-path %t/Macros.swiftinterface -enable-experimental-feature Macros %s
// RUN: %FileCheck %s < %t/Macros.swiftinterface --check-prefix CHECK
// RUN: %target-swift-frontend -compile-module-from-interface %t/Macros.swiftinterface -o %t/Macros.swiftmodule

// CHECK: #if compiler(>=5.3) && $Macros
// CHECK-NEXT: public macro publicStringify<T>(_ value: T) -> (T, Swift.String) = _SwiftSyntaxMacros.StringifyMacro
// CHECK-NEXT: #endif
public macro publicStringify<T>(_ value: T) -> (T, String) = _SwiftSyntaxMacros.StringifyMacro

// CHECK: #if compiler(>=5.3) && $Macros
// CHECK: public macro publicLine<T>: T = _SwiftSyntaxMacros.Line where T : Swift.ExpressibleByIntegerLiteral
// CHECK-NEXT: #endif
public macro publicLine<T: ExpressibleByIntegerLiteral>: T = _SwiftSyntaxMacros.Line

// CHECK-NOT: internalStringify
macro internalStringify<T>(_ value: T) -> (T, String) = _SwiftSyntaxMacros.StringifyMacro
