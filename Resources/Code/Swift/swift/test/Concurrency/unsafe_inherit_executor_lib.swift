// RUN: %target-swift-frontend -typecheck -verify -disable-availability-checking %s -strict-concurrency=complete

actor A {
  func g() { }
  func h() throws { }
  
  func f() async throws {
    await withTaskGroup(of: Int.self, returning: Void.self) { group in
      g()
    }

    try await withThrowingTaskGroup(of: Int.self, returning: Void.self) { group in
      try h()
    }
  }
}
