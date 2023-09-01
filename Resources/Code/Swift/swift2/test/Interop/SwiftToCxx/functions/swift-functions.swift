// RUN: %empty-directory(%t)
// RUN: %target-swift-frontend %s -typecheck -module-name Functions -clang-header-expose-decls=all-public -emit-clang-header-path %t/functions.h
// RUN: %FileCheck %s < %t/functions.h

// RUN: %check-interop-cxx-header-in-clang(%t/functions.h)

// CHECK-LABEL: namespace Functions __attribute__((swift_private)) {

// CHECK-LABEL: namespace _impl {

// CHECK: SWIFT_EXTERN void $s9Functions17passIntReturnVoid1xys5Int32V_tF(int x) SWIFT_NOEXCEPT SWIFT_CALL; // passIntReturnVoid(x:)
// CHECK: SWIFT_EXTERN int $s9Functions016passTwoIntReturnD01x1ys5Int32VAF_AFtF(int x, int y) SWIFT_NOEXCEPT SWIFT_CALL; // passTwoIntReturnInt(x:y:)
// CHECK: SWIFT_EXTERN int $s9Functions016passTwoIntReturnD10NoArgLabelys5Int32VAD_ADtF(int, int) SWIFT_NOEXCEPT SWIFT_CALL; // passTwoIntReturnIntNoArgLabel(_:_:)
// CHECK: SWIFT_EXTERN int $s9Functions016passTwoIntReturnD19NoArgLabelParamNameys5Int32VAD_ADtF(int x2, int y2) SWIFT_NOEXCEPT SWIFT_CALL; // passTwoIntReturnIntNoArgLabelParamName(_:_:)
// CHECK: SWIFT_EXTERN void $s9Functions014passVoidReturnC0yyF(void) SWIFT_NOEXCEPT SWIFT_CALL; // passVoidReturnVoid()

// CHECK: }

public func passIntReturnVoid(x: CInt) { print("passIntReturnVoid \(x)") }

// CHECK: inline void passIntReturnVoid(int x) noexcept {
// CHECK: return _impl::$s9Functions17passIntReturnVoid1xys5Int32V_tF(x);
// CHECK: }

public func passTwoIntReturnInt(x: CInt, y: CInt) -> CInt { return x + y }

// CHECK: inline int passTwoIntReturnInt(int x, int y) noexcept SWIFT_WARN_UNUSED_RESULT {
// CHECK: return _impl::$s9Functions016passTwoIntReturnD01x1ys5Int32VAF_AFtF(x, y);
// CHECK: }

public func passTwoIntReturnIntNoArgLabel(_: CInt, _: CInt) -> CInt {
  print("passTwoIntReturnIntNoArgLabel")
  return 42
}

// CHECK: inline int passTwoIntReturnIntNoArgLabel(int _1, int _2) noexcept SWIFT_WARN_UNUSED_RESULT {
// CHECK: return _impl::$s9Functions016passTwoIntReturnD10NoArgLabelys5Int32VAD_ADtF(_1, _2);
// CHECK: }

public func passTwoIntReturnIntNoArgLabelParamName(_ x2: CInt, _ y2: CInt) -> CInt { return x2 + y2 }

// CHECK: inline int passTwoIntReturnIntNoArgLabelParamName(int x2, int y2) noexcept SWIFT_WARN_UNUSED_RESULT {
// CHECK:   return _impl::$s9Functions016passTwoIntReturnD19NoArgLabelParamNameys5Int32VAD_ADtF(x2, y2);
// CHECK: }

public func passVoidReturnVoid() { print("passVoidReturnVoid") }

// CHECK: inline void passVoidReturnVoid() noexcept {
// CHECK: return _impl::$s9Functions014passVoidReturnC0yyF();
// CHECK: }
