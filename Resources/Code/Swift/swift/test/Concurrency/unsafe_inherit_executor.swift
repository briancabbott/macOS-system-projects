// RUN: %target-swift-frontend -typecheck -verify -disable-availability-checking %s

// expected-error @+1 {{non-async functions cannot inherit an executor}}
@_unsafeInheritExecutor
func testNonAsync() {}

@_unsafeInheritExecutor
func testAsync() async {}

struct A {
  // expected-error @+1 {{@_unsafeInheritExecutor may only be used on 'func' declarations}}
  @_unsafeInheritExecutor
  init() async {}

  // expected-error @+1 {{non-async functions cannot inherit an executor}}
  @_unsafeInheritExecutor
  func testNonAsync() {}

  @_unsafeInheritExecutor
  func testAsync() async {}
}


class NonSendableObject {
  var property = 0
}

@_unsafeInheritExecutor
func useNonSendable(object: NonSendableObject) async {}

actor MyActor {
  var object = NonSendableObject()
  func foo() async {
    await useNonSendable(object: self.object)
  }
}
