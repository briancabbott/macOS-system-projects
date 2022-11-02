// RUN: %complete-test -tok=INT_OPERATORS %s | FileCheck %s
// RUN: %complete-test -add-inner-results -tok=INT_OPERATORS_INNER %s | FileCheck %s -check-prefix=INNER
// RUN: %complete-test -raw -hide-none -tok=INT_OPERATORS %s | FileCheck %s -check-prefix=RAW

let xxxx = 1
func test1(x: Int) {
  var x = x
  x#^INT_OPERATORS^#
}
// CHECK: .
// CHECK: !=
// CHECK: +
// CHECK: ++

func test2(x: Int) {
  var x = x
  #^INT_OPERATORS_INNER,x^#
}
// INNER: x.
// INNER: x+
// INNER: x++
// INNER: xxxx
// INNER: x.bigEndian

// RAW: {
// RAW:   key.kind: source.lang.swift.decl.function.operator.infix,
// RAW:   key.name: "!=",
// RAW:   key.sourcetext: " != <#T##Int#>",
// RAW:   key.description: "!=",
// RAW:   key.typename: "Bool",
// RAW: {
// RAW:   key.kind: source.lang.swift.decl.function.operator.infix,
// RAW:   key.name: "+",
// RAW:   key.sourcetext: " + <#T##Int#>",
// RAW:   key.description: "+",
// RAW:   key.typename: "Int",
// RAW: },
// RAW: {
// RAW:   key.kind: source.lang.swift.decl.function.operator.postfix,
// RAW:   key.name: "++",
// RAW:   key.sourcetext: "++",
// RAW:   key.description: "++",
// RAW:   key.typename: "Int",
// RAW: },
