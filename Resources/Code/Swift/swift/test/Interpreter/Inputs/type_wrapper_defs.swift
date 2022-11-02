@typeWrapper
public struct Wrapper<S> {
  var underlying: S

  public init(memberwise: S) {
    print("Wrapper.init(\(memberwise))")
    self.underlying = memberwise
  }

  public subscript<V>(storageKeyPath path: WritableKeyPath<S, V>) -> V {
    get {
      print("in getter")
      return underlying[keyPath: path]
    }
    set {
      print("in setter => \(newValue)")
      underlying[keyPath: path] = newValue
    }
  }
}

@propertyWrapper
public struct PropWrapper<Value> {
  public var value: Value

  public init(wrappedValue: Value) {
    self.value = wrappedValue
  }

  public var projectedValue: Self { return self }

  public var wrappedValue: Value {
    get {
      self.value
    }
    set {
      self.value = newValue
    }
  }
}

@propertyWrapper
public struct PropWrapperWithoutInit<Value> {
  public var value: Value

  public init(value: Value) {
    self.value = value
  }

  public var projectedValue: Self { return self }

  public var wrappedValue: Value {
    get {
      self.value
    }
    set {
      self.value = newValue
    }
  }
}

@propertyWrapper
public struct PropWrapperWithoutProjection<Value> {
  public var value: Value

  public init(wrappedValue: Value) {
    self.value = wrappedValue
  }

  public var wrappedValue: Value {
    get {
      self.value
    }
    set {
      self.value = newValue
    }
  }
}

@Wrapper
public class Person<T> {
  public var name: String
  public var projects: [T]
}

@Wrapper
public struct PersonWithDefaults {
  public var name: String = "<no name>"
  public var age: Int = 99
}

@Wrapper
public struct PropWrapperTest {
  @PropWrapper public var test: Int
}

@Wrapper
public struct DefaultedPropWrapperTest {
  @PropWrapper public var test: Int = 0
}

@Wrapper
public struct DefaultedPropWrapperWithArgTest {
  @PropWrapper(wrappedValue: 3) public var test: Int
}

@Wrapper
public struct PropWrapperNoInitTest {
  @PropWrapperWithoutInit public var a: Int
  @PropWrapperWithoutInit(value: "b") public var b: String
}

@Wrapper
public struct PropWrapperNoProjectionTest {
  @PropWrapperWithoutProjection public var a: Int = 0
  @PropWrapperWithoutProjection(wrappedValue: PropWrapper(wrappedValue: "b")) @PropWrapper public var b: String
}

@Wrapper
public struct ComplexPropWrapperTest {
  @PropWrapper public var a: [String] = ["a"]
  @PropWrapperWithoutInit(value: PropWrapper(wrappedValue: [1, 2, 3])) @PropWrapper public var b: [Int]
}

@Wrapper
public struct PersonWithUnmanagedTest {
  public let name: String

  public lazy var age: Int = {
    30
  }()

  public var placeOfBirth: String {
    get { "Earth" }
  }

  @PropWrapper public var favoredColor: String = "red"
}

@Wrapper
public class ClassWithDesignatedInit {
  public var a: Int
  @PropWrapperWithoutInit public var b: [Int]

  public init(a: Int, b: [Int] = [1, 2, 3]) {
    self.a = a
    self._b = PropWrapperWithoutInit(value: b)
  }
}

@Wrapper
public class PersonWithIgnoredAge {
  public var name: String
  @typeWrapperIgnored public var age: Int = 0
  @typeWrapperIgnored @PropWrapper public var manufacturer: String?
}

/// Empty class to make sure that $storage is initialized
/// even without any type wrapper managed properties.
@Wrapper
public class EmptyUserDefinedInitClassTest {
  public init() {}
}

/// Empty struct to make sure that $storage is initialized
/// even without any type wrapper managed properties.
@Wrapper
public class EmptyUserDefinedInitStructTest {
  public init() {}
}

@Wrapper
public class TrivialUserDefinedInitClassTest {
  public var a: Int

  public init(a: Int) {
    self.a = a
  }

  public init(withReassign: Int) {
    self.a = 0
    print(self.a)
    self.a = withReassign
    print(self.a)
  }
}

@Wrapper
public struct TrivialUserDefinedInitStructTest {
  public var a: Int

  public init(a: Int) {
    self.a = a
  }

  public init(withReassign: Int) {
    self.a = 0
    print(self.a)
    self.a = withReassign
    print(self.a)
  }
}

@Wrapper
public class ContextUserDefinedInitClassTest<K: Hashable, V> {
  public var a: Int
  @PropWrapper public var b: (String, (Int, [V]))
  public var c: [K: V]

  public init(
    a: Int = 0,
    b: (String, (Int, [V])) = ("", (0, [1, 2, 3])),
    c: [K: V] = [:],
    placeholder: (K, V)? = nil
  ) {
    self.a = a
    self.b = b
    self.c = c

    print(self.c)

    if let placeholder {
      self.c[placeholder.0] = placeholder.1
    }

    print(self.c)
  }
}

@Wrapper
public class ContextUserDefinedInitStructTest<K: Hashable, V> {
  public var a: Int
  @PropWrapper public var b: (String, (Int, [V]))
  public var c: [K: V]

  public init(
    a: Int = 0,
    b: (String, (Int, [V])) = ("", (0, [1.0, 2.0, 3.0])),
    c: [K: V] = [:],
    placeholder: (K, V)? = nil
  ) {
    self.a = a
    self.b = b
    self.c = c

    print(self.c)

    if let placeholder {
      self.c[placeholder.0] = placeholder.1
    }

    print(self.c)
  }
}

@Wrapper
public class UserDefinedInitWithConditionalTest<T> {
  var val: T?

  public init(cond: Bool = true, initialValue: T? = nil) {
    if (cond) {
      if let initialValue {
        self.val = initialValue
      } else {
        self.val = nil
      }
    } else {
      self.val = nil
    }

    print(self.val)
  }
}

@Wrapper
public class ClassWithConvenienceInit<T> {
  public var a: T?
  public var b: String

  init(a: T?, b: String) {
    // Just to test that conditionals work properly
    if let a {
      self.a = a
    } else {
      self.a = nil
    }

    self.b = b
  }

  public convenience init() {
    self.init(a: nil)
    print(self.a)
    print(self.b)
  }

  public convenience init(a: T?) {
    self.init(a: a, b: "<placeholder>")
    print(self.a)
    print(self.b)

    self.b = "<modified>"
    print(self.b)
  }
}
