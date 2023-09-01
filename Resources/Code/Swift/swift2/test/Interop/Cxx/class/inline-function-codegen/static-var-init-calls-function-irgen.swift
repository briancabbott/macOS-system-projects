// RUN: %target-swift-emit-ir %s -I %S/Inputs -enable-experimental-cxx-interop -validate-tbd-against-ir=none | %FileCheck %s

// TODO: See why -validate-tbd-against-ir=none is needed here
// (https://github.com/apple/swift/issues/56458).

import StaticVarInitCallsFunction

public func getInitializedStaticVar() -> CInt {
  return initializeStaticVar()
}

// CHECK: define {{.*}}i32 @{{_Z9incrementi|"\?increment@@YAHH@Z"}}(i32{{.*}})
