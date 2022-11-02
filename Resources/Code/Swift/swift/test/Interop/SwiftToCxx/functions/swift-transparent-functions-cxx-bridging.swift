// RUN: %empty-directory(%t)
// RUN: %target-swift-frontend %s -typecheck -module-name Functions -clang-header-expose-decls=all-public -emit-clang-header-path %t/functions.h
// RUN: %FileCheck %s < %t/functions.h

// RUN: %check-interop-cxx-header-in-clang(%t/functions.h)

// CHECK: SWIFT_EXTERN ptrdiff_t $s9Functions24transparentPrimitiveFuncyS2iF(ptrdiff_t x) SWIFT_NOEXCEPT SWIFT_CALL; // transparentPrimitiveFunc(_:)

// CHECK:      inline swift::Int transparentPrimitiveFunc(swift::Int x) noexcept SWIFT_WARN_UNUSED_RESULT {
// CHECK-NEXT:   return _impl::$s9Functions24transparentPrimitiveFuncyS2iF(x);
// CHECK-NEXT: }

@_transparent
public func transparentPrimitiveFunc(_ x: Int) -> Int { return x * x }
