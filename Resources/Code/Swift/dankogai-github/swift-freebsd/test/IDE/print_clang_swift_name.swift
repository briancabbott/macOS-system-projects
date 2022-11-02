// RUN: rm -rf %t && mkdir -p %t

// RUN: %target-swift-ide-test(mock-sdk: %clang-importer-sdk) -print-module -source-filename %s -module-to-print=SwiftNameTests -function-definitions=false -print-regular-comments -F %S/Inputs/mock-sdk > %t.txt
// RUN: diff -u <(tail +9 %s) %t.txt

// REQUIRES: objc_interop

// EXPECTED OUTPUT STARTS BELOW THIS LINE.
@_exported import Foundation


class Test : NSObject {
  
  // "Factory methods" that we'd rather have as initializers.
  @available(*, unavailable, message="superseded by import of -[NSObject init]")
  convenience init()
  @available(*, unavailable, message="use object construction 'Test()'")
  class func a() -> Self
  convenience init(dummyParam: ())
  @available(*, unavailable, message="use object construction 'Test(dummyParam:)'")
  class func b() -> Self
  
  convenience init(cc x: AnyObject)
  @available(*, unavailable, message="use object construction 'Test(cc:)'")
  class func c(x: AnyObject) -> Self
  convenience init(_ x: AnyObject)
  @available(*, unavailable, message="use object construction 'Test(_:)'")
  class func d(x: AnyObject) -> Self
  
  convenience init(aa a: AnyObject, _ b: AnyObject, cc c: AnyObject)
  @available(*, unavailable, message="use object construction 'Test(aa:_:cc:)'")
  class func e(a: AnyObject, e b: AnyObject, e c: AnyObject) -> Self
  
  /*not inherited*/ init(fixedType: ())
  @available(*, unavailable, message="use object construction 'Test(fixedType:)'")
  class func f() -> Test
  
  // Would-be initializers.
  class func zz() -> Self
  class func yy(aa x: AnyObject) -> Self
  class func xx(x: AnyObject, bb xx: AnyObject) -> Self
  
  init()
}

class TestError : NSObject {
  // Factory methods with NSError.
  convenience init(error: ()) throws
  @available(*, unavailable, message="use object construction 'TestError(error:)'")
  class func err1() throws -> Self
  convenience init(aa x: AnyObject?, error: ()) throws
  @available(*, unavailable, message="use object construction 'TestError(aa:error:)'")
  class func err2(x: AnyObject?) throws -> Self
  convenience init(aa x: AnyObject?, error: (), block: () -> Void) throws
  @available(*, unavailable, message="use object construction 'TestError(aa:error:block:)'")
  class func err3(x: AnyObject?, callback block: () -> Void) throws -> Self
  convenience init(error: (), block: () -> Void) throws
  @available(*, unavailable, message="use object construction 'TestError(error:block:)'")
  class func err4(callback block: () -> Void) throws -> Self
  
  convenience init(aa x: AnyObject?) throws
  @available(*, unavailable, message="use object construction 'TestError(aa:)'")
  class func err5(x: AnyObject?) throws -> Self
  convenience init(aa x: AnyObject?, block: () -> Void) throws
  @available(*, unavailable, message="use object construction 'TestError(aa:block:)'")
  class func err6(x: AnyObject?, callback block: () -> Void) throws -> Self
  convenience init(block: () -> Void) throws
  @available(*, unavailable, message="use object construction 'TestError(block:)'")
  class func err7(callback block: () -> Void) throws -> Self
  
  // Would-be initializers.
  class func ww(x: AnyObject?) throws -> Self
  class func w2(x: AnyObject?, error: ()) throws -> Self
  class func vv() throws -> Self
  class func v2(error error: ()) throws -> Self
  init()
}

class TestSub : Test {
  @available(*, unavailable, message="superseded by import of -[NSObject init]")
  convenience init()
  convenience init(dummyParam: ())
  convenience init(cc x: AnyObject)
  convenience init(_ x: AnyObject)
  convenience init(aa a: AnyObject, _ b: AnyObject, cc c: AnyObject)
  init()
}

class TestErrorSub : TestError {
  convenience init(error: ()) throws
  convenience init(aa x: AnyObject?, error: ()) throws
  convenience init(aa x: AnyObject?, error: (), block: () -> Void) throws
  convenience init(error: (), block: () -> Void) throws
  convenience init(aa x: AnyObject?) throws
  convenience init(aa x: AnyObject?, block: () -> Void) throws
  convenience init(block: () -> Void) throws
  init()
}
