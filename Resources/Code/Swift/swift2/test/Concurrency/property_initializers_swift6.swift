// RUN: %target-typecheck-verify-swift -swift-version 6 -disable-availability-checking -warn-concurrency
// REQUIRES: concurrency

// REQUIRES: asserts

@globalActor
actor GlobalActor {
    static let shared = GlobalActor()
}

@GlobalActor
func globalActorFn() -> Int { return 0 } // expected-note {{calls to global function 'globalActorFn()' from outside of its actor context are implicitly asynchronous}}

@GlobalActor
class C {
  var x: Int = globalActorFn()

  lazy var y: Int = globalActorFn()

  static var z: Int = globalActorFn()
}

var x: Int = globalActorFn() // expected-error {{call to global actor 'GlobalActor'-isolated global function 'globalActorFn()' in a synchronous main actor-isolated context}}
