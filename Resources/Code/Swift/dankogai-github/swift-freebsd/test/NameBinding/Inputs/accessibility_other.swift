import has_accessibility

public let a = 0
internal let b = 0
private let c = 0

extension Foo {
  public static func a() {}
  internal static func b() {}
  private static func c() {}
}

struct PrivateInit {
  private init() {}
}

extension Foo {
  private func method() {}
  private typealias TheType = Float
}

extension OriginallyEmpty {
  func method() {}
  typealias TheType = Float
}

private func privateInBothFiles() {}
func privateInPrimaryFile() {} // expected-note {{previously declared here}}
private func privateInOtherFile() {} // expected-error {{invalid redeclaration}}
