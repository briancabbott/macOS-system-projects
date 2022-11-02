import FooClangModule

protocol Prot {
  func meth()
}

class Cls : S1, Prot {
  func meth() {}
}

class SubCls : Cls {
  func meth() {}
}

func goo(x: SubCls) {
  x.meth()
}

// RUN: %sourcekitd-test -req=cursor -pos=16:7 %s -- -triple x86_64-apple-macosx10.9 -I %S/Inputs/cursor-overrides %mcp_opt %s | FileCheck -check-prefix=CHECK1 %s
// CHECK1: source.lang.swift.ref.function.method.instance (12:8-12:14)
// CHECK1: s:FC16cursor_overrides6SubCls4methFT_T_
// CHECK1: SubCls -> () -> ()
// CHECK1:      OVERRIDES BEGIN
// CHECK1-NEXT: s:FC16cursor_overrides3Cls4methFT_T_
// CHECK1-NEXT: s:FP16cursor_overrides4Prot4methFT_T_
// CHECK1-NEXT: c:objc(cs)S1(im)meth
// CHECK1-NEXT: c:objc(cs)B1(im)meth
// CHECK1-NEXT: c:objc(pl)P1(im)meth
// CHECK1-NEXT: OVERRIDES END
