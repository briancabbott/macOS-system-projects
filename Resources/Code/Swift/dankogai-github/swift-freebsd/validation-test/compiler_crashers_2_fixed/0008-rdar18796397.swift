// RUN: %target-swift-frontend %s -emit-ir -o /dev/null
public protocol OurProtocol {

  typealias T
  var myVar: T? {get set}
  var validator: (Int) -> (T) { get set }
}

class Parent {

}

class MyClass<T> : OurProtocol {
  var myVar: T?
  var validator: (Int) -> (T) = { (t) -> (T) in
    var t = t
    return t as! T
  }
}


var myClass = MyClass<Int>()
