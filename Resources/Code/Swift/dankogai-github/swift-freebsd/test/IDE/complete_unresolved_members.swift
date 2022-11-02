// RUN: %target-swift-ide-test -code-completion -source-filename %s -code-completion-token=UNRESOLVED_1 | FileCheck %s -check-prefix=UNRESOLVED_1
// RUN: %target-swift-ide-test -code-completion -source-filename %s -code-completion-token=UNRESOLVED_2 | FileCheck %s -check-prefix=UNRESOLVED_1
// RUN: %target-swift-ide-test -code-completion -source-filename %s -code-completion-token=UNRESOLVED_3 | FileCheck %s -check-prefix=UNRESOLVED_1
// RUN: %target-swift-ide-test -code-completion -source-filename %s -code-completion-token=UNRESOLVED_4 | FileCheck %s -check-prefix=UNRESOLVED_1
// RUN: %target-swift-ide-test -code-completion -source-filename %s -code-completion-token=UNRESOLVED_5 | FileCheck %s -check-prefix=UNRESOLVED_1

// RUN: %target-swift-ide-test -code-completion -source-filename %s -code-completion-token=UNRESOLVED_6 | FileCheck %s -check-prefix=UNRESOLVED_2
// RUN: %target-swift-ide-test -code-completion -source-filename %s -code-completion-token=UNRESOLVED_7 | FileCheck %s -check-prefix=UNRESOLVED_2
// RUN: %target-swift-ide-test -code-completion -source-filename %s -code-completion-token=UNRESOLVED_10 | FileCheck %s -check-prefix=UNRESOLVED_2
// RUN: %target-swift-ide-test -code-completion -source-filename %s -code-completion-token=UNRESOLVED_11 | FileCheck %s -check-prefix=UNRESOLVED_2

// RUN: %target-swift-ide-test -code-completion -source-filename %s -code-completion-token=UNRESOLVED_8 | FileCheck %s -check-prefix=UNRESOLVED_3
// RUN: %target-swift-ide-test -code-completion -source-filename %s -code-completion-token=UNRESOLVED_9 | FileCheck %s -check-prefix=UNRESOLVED_3

// RUN: %target-swift-ide-test -code-completion -source-filename %s -code-completion-token=UNRESOLVED_12 | FileCheck %s -check-prefix=UNRESOLVED_3
// RUN: %target-swift-ide-test -code-completion -source-filename %s -code-completion-token=UNRESOLVED_13 | FileCheck %s -check-prefix=UNRESOLVED_3
// RUN: %target-swift-ide-test -code-completion -source-filename %s -code-completion-token=UNRESOLVED_14 | FileCheck %s -check-prefix=UNRESOLVED_1
// RUN: %target-swift-ide-test -code-completion -source-filename %s -code-completion-token=UNRESOLVED_15 | FileCheck %s -check-prefix=UNRESOLVED_1

// RUN: %target-swift-ide-test -code-completion -source-filename %s -code-completion-token=UNRESOLVED_16 | FileCheck %s -check-prefix=UNRESOLVED_4
// RUN: %target-swift-ide-test -code-completion -source-filename %s -code-completion-token=UNRESOLVED_17 | FileCheck %s -check-prefix=UNRESOLVED_4
// RUN: %target-swift-ide-test -code-completion -source-filename %s -code-completion-token=UNRESOLVED_18 | FileCheck %s -check-prefix=UNRESOLVED_1

// RUN: %target-swift-ide-test -code-completion -source-filename %s -code-completion-token=UNRESOLVED_19 | FileCheck %s -check-prefix=UNRESOLVED_1
// RUN: %target-swift-ide-test -code-completion -source-filename %s -code-completion-token=UNRESOLVED_20 | FileCheck %s -check-prefix=UNRESOLVED_3
// RUN: %target-swift-ide-test -code-completion -source-filename %s -code-completion-token=UNRESOLVED_21 | FileCheck %s -check-prefix=UNRESOLVED_1
// RUN: %target-swift-ide-test -code-completion -source-filename %s -code-completion-token=UNRESOLVED_22 | FileCheck %s -check-prefix=UNRESOLVED_1
// RUN: %target-swift-ide-test -code-completion -source-filename %s -code-completion-token=UNRESOLVED_23 | FileCheck %s -check-prefix=UNRESOLVED_1
// RUN: %target-swift-ide-test -code-completion -source-filename %s -code-completion-token=UNRESOLVED_24 | FileCheck %s -check-prefix=UNRESOLVED_3
// RUN: %target-swift-ide-test -code-completion -source-filename %s -code-completion-token=UNRESOLVED_25 | FileCheck %s -check-prefix=UNRESOLVED_3
// RUN: %target-swift-ide-test -code-completion -source-filename %s -code-completion-token=UNRESOLVED_26 | FileCheck %s -check-prefix=UNRESOLVED_3
// RUN: %target-swift-ide-test -code-completion -source-filename %s -code-completion-token=UNRESOLVED_27 | FileCheck %s -check-prefix=UNRESOLVED_3
// RUN: %target-swift-ide-test -code-completion -source-filename %s -code-completion-token=UNRESOLVED_28 | FileCheck %s -check-prefix=UNRESOLVED_1
// RUN: %target-swift-ide-test -code-completion -source-filename %s -code-completion-token=UNRESOLVED_29 | FileCheck %s -check-prefix=UNRESOLVED_1
// RUN: %target-swift-ide-test -code-completion -source-filename %s -code-completion-token=UNRESOLVED_30 | FileCheck %s -check-prefix=UNRESOLVED_2

enum SomeEnum1 {
  case South
  case North
}

enum SomeEnum2 {
  case East
  case West
}

enum SomeEnum3 {
  case Payload(SomeEnum1)
}

struct NotOptions1 {
  static let NotSet = 1;
}

struct SomeOptions1 : OptionSetType {
  let rawValue : Int
  static let Option1 = SomeOptions1(rawValue: 1 << 1)
  static let Option2 = SomeOptions1(rawValue: 1 << 2)
  static let Option3 = SomeOptions1(rawValue: 1 << 3)
  let NotStaticOption = SomeOptions1(rawValue: 1 << 4)
  static let NotOption = 1
}

struct SomeOptions2 : OptionSetType {
  let rawValue : Int
  static let Option4 = SomeOptions2(rawValue: 1 << 1)
  static let Option5 = SomeOptions2(rawValue: 1 << 2)
  static let Option6 = SomeOptions2(rawValue: 1 << 3)
}

func OptionSetTaker1(Op : SomeOptions1) {}

func OptionSetTaker2(Op : SomeOptions2) {}

func OptionSetTaker3(Op1: SomeOptions1, Op2: SomeOptions2) {}

func OptionSetTaker4(Op1: SomeOptions2, Op2: SomeOptions1) {}

func OptionSetTaker5(Op1: SomeOptions1, Op2: SomeOptions2, En1 : SomeEnum1, En2: SomeEnum2) {}

func OptionSetTaker6(Op1: SomeOptions1, Op2: SomeOptions2) {}

func OptionSetTaker6(Op1: SomeOptions2, Op2: SomeOptions1) {}

func OptionSetTaker7(Op1: SomeOptions1, Op2: SomeOptions2) -> Int {return 0}

func EnumTaker1(E : SomeEnum1) {}

class OptionTakerContainer1 {
  func OptionSetTaker1(op : SomeOptions1) {}
  func EnumTaker1(E : SomeEnum1) {}
}

class C1 {
  func f1() {
    var f : SomeOptions1
    f = .#^UNRESOLVED_1^#
  }
  func f2() {
    var f : SomeOptions1
    f = [.#^UNRESOLVED_2^#
  }
  func f3() {
    var f : SomeOptions1
    f = [.Option1, .#^UNRESOLVED_3^#
  }
}
class C2 {
  func f1() {
    OptionSetTaker1(.#^UNRESOLVED_4^#)
  }
  func f2() {
    OptionSetTaker1([.Option1, .#^UNRESOLVED_5^#])
  }
// UNRESOLVED_1:  Begin completions
// UNRESOLVED_1-NOT:  SomeEnum1
// UNRESOLVED_1-NOT:  SomeEnum2
// UNRESOLVED_1-DAG:  Decl[StaticVar]/CurrNominal: Option1[#SomeOptions1#]; name=Option1
// UNRESOLVED_1-DAG:  Decl[StaticVar]/CurrNominal: Option2[#SomeOptions1#]; name=Option2
// UNRESOLVED_1-DAG:  Decl[StaticVar]/CurrNominal: Option3[#SomeOptions1#]; name=Option3
// UNRESOLVED_1-NOT:  SomeOptions2
// UNRESOLVED_1-NOT:  Not
}

class C3 {
  func f1() {
    OptionSetTaker2(.#^UNRESOLVED_6^#)
  }
  func f2() {
    OptionSetTaker2([.Option4, .#^UNRESOLVED_7^#])
  }
// UNRESOLVED_2:  Begin completions
// UNRESOLVED_2-NOT:  SomeEnum1
// UNRESOLVED_2-NOT:  SomeEnum2
// UNRESOLVED_2-DAG:  Decl[StaticVar]/CurrNominal: Option4[#SomeOptions2#]; name=Option4
// UNRESOLVED_2-DAG:  Decl[StaticVar]/CurrNominal: Option5[#SomeOptions2#]; name=Option5
// UNRESOLVED_2-DAG:  Decl[StaticVar]/CurrNominal: Option6[#SomeOptions2#]; name=Option6
// UNRESOLVED_2-NOT:  SomeOptions1
// UNRESOLVED_2-NOT:  Not
}

class C4 {
  func f1() {
    var E : SomeEnum1
    E = .#^UNRESOLVED_8^#
  }
  func f2() {
    EnumTaker1(.#^UNRESOLVED_9^#)
  }
  func f3() {
    OptionSetTaker5(.Option1, .Option4, .#^UNRESOLVED_12^#, .West)
  }
}
// UNRESOLVED_3: Begin completions
// UNRESOLVED_3-DAG: Decl[EnumElement]/ExprSpecific:     North[#SomeEnum1#]; name=North
// UNRESOLVED_3-DAG: Decl[EnumElement]/ExprSpecific:     South[#SomeEnum1#]; name=South
// UNRESOLVED_3-NOT: SomeEnum2
// UNRESOLVED_3-NOT: SomeOptions1
// UNRESOLVED_3-NOT: SomeOptions2

class C5 {
  func f1() {
    OptionSetTaker3(.Option1, .#^UNRESOLVED_10^#)
  }
  func f2() {
    OptionSetTaker4(.#^UNRESOLVED_11^#, .Option1)
  }
}

OptionSetTaker5(.Option1, .Option4, .#^UNRESOLVED_13^#, .West)
OptionSetTaker5(.#^UNRESOLVED_14^#, .Option4, .South, .West)
OptionSetTaker5([.#^UNRESOLVED_15^#], .Option4, .South, .West)

// FIXME: Overload needs to be handled.
OptionSetTaker6(.#^UNRESOLVED_16^#, .Option4)
OptionSetTaker6(.Option4, .#^UNRESOLVED_17^#,)

var a = {() in
  OptionSetTaker5([.#^UNRESOLVED_18^#], .Option4, .South, .West)
}
var Containner = OptionTakerContainer1()
Container.OptionSetTaker1(.#^UNRESOLVED_19^#
Container.EnumTaker1(.#^UNRESOLVED_20^#

// UNRESOLVED_4: Begin completions
// UNRESOLVED_4-DAG: Decl[StaticVar]/CurrNominal:        Option1[#SomeOptions1#]; name=Option1
// UNRESOLVED_4-DAG: Decl[StaticVar]/CurrNominal:        Option2[#SomeOptions1#]; name=Option2
// UNRESOLVED_4-DAG: Decl[StaticVar]/CurrNominal:        Option3[#SomeOptions1#]; name=Option3
// UNRESOLVED_4-DAG: Decl[StaticVar]/CurrNominal:        Option4[#SomeOptions2#]; name=Option4
// UNRESOLVED_4-DAG: Decl[StaticVar]/CurrNominal:        Option5[#SomeOptions2#]; name=Option5
// UNRESOLVED_4-DAG: Decl[StaticVar]/CurrNominal:        Option6[#SomeOptions2#]; name=Option6

var OpIns1 : SomeOptions1 = .#^UNRESOLVED_21^#

var c1 = {() -> SomeOptions1 in
  return .#^UNRESOLVED_22^#
}

class C6 {
  func f1() -> SomeOptions1 {
    return .#^UNRESOLVED_23^#
  }
  func f2(P : SomeEnum3) {
  switch p {
  case .Payload(.#^UNRESOLVED_24^#)
  }
  }
}

class C6 {
  func f1(e: SomeEnum1) {
    if let x = Optional(e) where x == .#^UNRESOLVED_25^#
  }
}
class C7 {}
extension C7 {
  func extendedf1(e :SomeEnum1) {}
}

var cInst1 = C7()
cInst1.extendedf1(.#^UNRESOLVED_26^#

func f() -> SomeEnum1 {
  return .#^UNRESOLVED_27^#
}

let TopLevelVar1 = OptionSetTaker7([.#^UNRESOLVED_28^#], Op2: [.Option4])

let TopLevelVar2 = OptionSetTaker1([.#^UNRESOLVED_29^#])

let TopLevelVar3 = OptionSetTaker7([.Option1], Op2: [.#^UNRESOLVED_30^#])
