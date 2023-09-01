// RUN: %target-swift-frontend -typecheck -disable-availability-checking -swift-version 6 %s -verify

// REQUIRES: asserts

// Even though enable-experimental-async-top-level is enabled, there are no
// 'await's made from the top-level, thus the top-level is not an asynchronous
// context. `a` is just a normal top-level global variable with no actor
// isolation.

var a = 10 // expected-note 2 {{var declared here}}
// expected-note@-1 2{{mutation of this var is only permitted within the actor}}

// expected-note@+1 3{{add '@MainActor' to make global function 'nonIsolatedSync()' part of global actor 'MainActor'}}
func nonIsolatedSync() {
    print(a) // expected-error {{main actor-isolated var 'a' can not be referenced from a non-isolated context}}
    a = a + 10 // expected-error{{main actor-isolated var 'a' can not be referenced from a non-isolated context}}
  // expected-error@-1{{main actor-isolated var 'a' can not be mutated from a non-isolated context}}
}

@MainActor
func isolatedSync() {
    print(a)
    a = a + 10
}

func nonIsolatedAsync() async {
  await print(a)
  a = a + 10 // expected-error{{main actor-isolated var 'a' can not be mutated from a non-isolated context}}
  // expected-note@-1{{property access is 'async'}}
  // expected-error@-2{{expression is 'async' but is not marked with 'await'}}
}

@MainActor
func isolatedAsync() async {
    print(a)
    a = a + 10
}

nonIsolatedSync()
isolatedSync()
nonIsolatedAsync() // expected-error {{'async' call in a function that does not support concurrency}}
isolatedAsync()
// expected-error@-1 {{'async' call in a function that does not support concurrency}}

print(a)

if a > 10 {
    nonIsolatedSync()
    isolatedSync()
    nonIsolatedAsync() // expected-error {{'async' call in a function that does not support concurrency}}
    isolatedAsync()
    // expected-error@-1 {{'async' call in a function that does not support concurrency}}

    print(a)
}
