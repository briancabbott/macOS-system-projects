// RUN: %target-parse-verify-swift

struct X { }

// Simple examples
struct X1 {
  var stored : Int

  subscript (i : Int) -> Int {
    get {
      return stored
    }
    mutating
    set {
      stored = newValue
    }
  }
}

struct X2 {
  var stored : Int

  subscript (i : Int) -> Int {
    get {
      return stored + i
    }
    set(v) {
      stored = v - i
    }
  }
}

struct X3 {
  var stored : Int

  subscript (_ : Int) -> Int {
    get {
      return stored
    }
    set(v) {
      stored = v
    }
  }
}

struct X4 {
  var stored : Int

  subscript (i : Int, j : Int) -> Int {
    get {
      return stored + i + j
    }
    set(v) {
      stored = v + i - j
    }
  }
}

// Semantic errors
struct Y1 {
  var x : X

  subscript(i: Int) -> Int {
    get {
      return x // expected-error{{cannot convert return expression of type 'X' to return type 'Int'}}
    }
    set {
      x = newValue // expected-error{{cannot assign value of type 'Int' to type 'X'}}
    }
  }
}

struct Y2 {
  subscript(idx: Int) -> TypoType { // expected-error 3{{use of undeclared type 'TypoType'}}
    get { repeat {} while true }
    set {}
  }
}

class Y3 {
  subscript(idx: Int) -> TypoType { // expected-error 3{{use of undeclared type 'TypoType'}}
    get { repeat {} while true }
    set {}
  }
}


protocol ProtocolGetSet0 {
  subscript(i: Int) -> Int {} // expected-error {{subscript declarations must have a getter}}
}
protocol ProtocolGetSet1 {
  subscript(i: Int) -> Int { get }
}
protocol ProtocolGetSet2 {
  subscript(i: Int) -> Int { set } // expected-error {{subscript declarations must have a getter}}
}
protocol ProtocolGetSet3 {
  subscript(i: Int) -> Int { get set }
}
protocol ProtocolGetSet4 {
  subscript(i: Int) -> Int { set get }
}

protocol ProtocolWillSetDidSet1 {
  subscript(i: Int) -> Int { willSet } // expected-error {{expected get or set in a protocol property}}
}
protocol ProtocolWillSetDidSet2 {
  subscript(i: Int) -> Int { didSet } // expected-error {{expected get or set in a protocol property}}
}
protocol ProtocolWillSetDidSet3 {
  subscript(i: Int) -> Int { willSet didSet } // expected-error {{expected get or set in a protocol property}}
}
protocol ProtocolWillSetDidSet4 {
  subscript(i: Int) -> Int { didSet willSet } // expected-error {{expected get or set in a protocol property}}
}

class DidSetInSubscript {
  subscript(_: Int) -> Int {
    didSet { // expected-error {{didSet is not allowed in subscripts}}
      print("eek")
    }
    get {}
  }
}

class WillSetInSubscript {
  subscript(_: Int) -> Int {
    willSet { // expected-error {{willSet is not allowed in subscripts}}
      print("eek")
    }
    get {}
  }
}

subscript(i: Int) -> Int { // expected-error{{'subscript' functions may only be declared within a type}}
  get {}
}

func f() {
  subscript (i: Int) -> Int { // expected-error{{'subscript' functions may only be declared within a type}}
    get {}
  }
}

struct NoSubscript { }

struct OverloadedSubscript {
  subscript(i: Int) -> Int {
    get {
      return i
    }
    set {}
  }

  subscript(i: Int, j: Int) -> Int {
    get { return i }
    set {}
  }
}

struct RetOverloadedSubscript {
  subscript(i: Int) -> Int {  // expected-note {{found this candidate}}
    get { return i }
    set {}
  }

  subscript(i: Int) -> Float {  // expected-note {{found this candidate}}
    get { return Float(i) }
    set {}
  }
}

struct MissingGetterSubscript1 {
  subscript (i : Int) -> Int {
  } // expected-error {{computed property must have accessors specified}}
}
struct MissingGetterSubscript2 {
  subscript (i : Int, j : Int) -> Int { // expected-error{{subscript declarations must have a getter}}
    set {}
  }
}

func test_subscript(inout x2: X2, i: Int, j: Int, inout value: Int, no: NoSubscript,
                    inout ovl: OverloadedSubscript, inout ret: RetOverloadedSubscript) {
  no[i] = value // expected-error{{type 'NoSubscript' has no subscript members}}

  value = x2[i]
  x2[i] = value

  value = ovl[i]
  ovl[i] = value

  value = ovl[(i, j)]
  ovl[(i, j)] = value

  value = ovl[(i, j, i)] // expected-error{{cannot subscript a value of type 'OverloadedSubscript' with an index of type '(Int, Int, Int)'}}
  // expected-note @-1 {{expected an argument list of type '(Int)'}}

  ret[i] // expected-error{{ambiguous use of 'subscript'}}

  value = ret[i]
  ret[i] = value
}

func subscript_rvalue_materialize(inout i: Int) {
  i = X1(stored: 0)[i]
}

func subscript_coerce(fn: ([UnicodeScalar], [UnicodeScalar]) -> Bool) {}
func test_subscript_coerce() {
  subscript_coerce({ $0[$0.count-1] < $1[$1.count-1] })
}

struct no_index {
  subscript () -> Int { return 42 }
  func test() -> Int {
    return self[]
  }
}

struct tuple_index {
  subscript (x : Int, y : Int) -> (Int, Int) { return (x, y) }
  func test() -> (Int, Int) {
    return self[123, 456]
  }
}



struct SubscriptTest1 {
  subscript(keyword:String) -> Bool { return true }  // expected-note 3 {{found this candidate}}
  subscript(keyword:String) -> String? {return nil }  // expected-note 3 {{found this candidate}}
}

func testSubscript1(s1 : SubscriptTest1) {
  let _ : Int = s1["hello"]  // expected-error {{ambiguous subscript with base type 'SubscriptTest1' and index type 'String'}}
  
  // FIXME: This is a sema bug, it should not be ambiguous due to its contextual boolean type. 
  // rdar://18741539
  if s1["hello"] {}  // expected-error {{ambiguous subscript with base type 'SubscriptTest1' and index type 'String'}}
  
  
  let _ = s1["hello"]  // expected-error {{ambiguous use of 'subscript'}}
}

struct SubscriptTest2 {
  subscript(a : String, b : Int) -> Int { return 0 }
  subscript(a : String, b : String) -> Int { return 0 }
}

func testSubscript1(s2 : SubscriptTest2) {
  _ = s2["foo"] // expected-error {{cannot subscript a value of type 'SubscriptTest2' with an index of type 'String'}}
  // expected-note @-1 {{overloads for 'subscript' exist with these partially matching parameter lists: (String, Int), (String, String)}}
  
  let a = s2["foo", 1.0] // expected-error {{cannot subscript a value of type 'SubscriptTest2' with an index of type '(String, Double)'}}
  // expected-note @-1 {{overloads for 'subscript' exist with these partially matching parameter lists: (String, Int), (String, String)}}
  
  
  let b = s2[1, "foo"] // expected-error {{cannot subscript a value of type 'SubscriptTest2' with an index of type '(Int, String)'}}
  // expected-note @-1 {{expected an argument list of type '(String, String)'}}
}
