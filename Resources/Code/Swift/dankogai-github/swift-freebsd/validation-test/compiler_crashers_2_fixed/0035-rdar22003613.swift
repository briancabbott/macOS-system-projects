// RUN: not %target-swift-frontend %s -parse

class CFArray {}
struct U<T> {}

func yyy<T, Result>(inout arg: T, @noescape _ body: U<T> -> Result) -> Result {
  return body(U<T>())
}

enum YYY: Int, OptionSetType {
  case A = 1
  
  init(rawValue: Int) {
    self = .A
  }
}

func XXX(flags: YYY, _ outItems: U<CFArray?>) -> Int
{
return 0
}

func f() {
  var importArray: CFArray? = nil
  yyy(&importArray) { importArrayPtr in
    XXX(0, importArrayPtr)
  }
}
