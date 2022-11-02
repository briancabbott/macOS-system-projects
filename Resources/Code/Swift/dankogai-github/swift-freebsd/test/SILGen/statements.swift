// RUN: %target-swift-frontend -parse-as-library -emit-silgen -verify %s | FileCheck %s

class MyClass { 
  func foo() { }
}

func markUsed<T>(t: T) {}

func marker_1() {}
func marker_2() {}
func marker_3() {}

class BaseClass {}
class DerivedClass : BaseClass {}

var global_cond: Bool = false

func bar(x: Int) {}
func foo(x: Int, _ y: Bool) {}

@noreturn
func abort() { abort() }



func assignment(x: Int, y: Int) {
  var x = x
  var y = y
  x = 42
  y = 57
  _ = x
  _ = y
  (x, y) = (1,2)
}

// CHECK-LABEL: sil hidden  @{{.*}}assignment
// CHECK: integer_literal $Builtin.Int2048, 42
// CHECK: assign
// CHECK: integer_literal $Builtin.Int2048, 57
// CHECK: assign

func if_test(x: Int, y: Bool) {
  if (y) {
   bar(x);
  }
  bar(x);
}

// CHECK-LABEL: sil hidden  @_TF10statements7if_test

func if_else(x: Int, y: Bool) {
  if (y) {
   bar(x);
  } else {
   foo(x, y);
  }
  bar(x);
}

// CHECK-LABEL: sil hidden  @_TF10statements7if_else

func nested_if(x: Int, y: Bool, z: Bool) {
  if (y) {
    if (z) {
      bar(x);
    }
  } else {
    if (z) {
      foo(x, y);
    }
  }
  bar(x);
}

// CHECK-LABEL: sil hidden  @_TF10statements9nested_if

func nested_if_merge_noret(x: Int, y: Bool, z: Bool) {
  if (y) {
    if (z) {
      bar(x);
    }
  } else {
    if (z) {
      foo(x, y);
    }
  }
}

// CHECK-LABEL: sil hidden  @_TF10statements21nested_if_merge_noret

func nested_if_merge_ret(x: Int, y: Bool, z: Bool) -> Int {
  if (y) {
    if (z) {
      bar(x);
    }
    return 1;
  } else {
    if (z) {
      foo(x, y);
    }
  }
  return 2;
}

// CHECK-LABEL: sil hidden  @_TF10statements19nested_if_merge_ret

func else_break(x: Int, y: Bool, z: Bool) {
  while z {
    if y {
    } else {
      break
    }
  }
}

// CHECK-LABEL: sil hidden  @_TF10statements10else_break

func loop_with_break(x: Int, _ y: Bool, _ z: Bool) -> Int {
  while (x > 2) {
   if (y) {
     bar(x);
     break;
   }
  }
}

// CHECK-LABEL: sil hidden  @_TF10statements15loop_with_break

func loop_with_continue(x: Int, y: Bool, z: Bool) -> Int {
  while (x > 2) {
    if (y) {
     bar(x);
     continue;
    }
    loop_with_break(x, y, z);
  }
  bar(x);
}

// CHECK-LABEL: sil hidden  @_TF10statements18loop_with_continue

func do_loop_with_continue(x: Int, y: Bool, z: Bool) -> Int {
  repeat {
    if (x < 42) {
     bar(x);
     continue;
    }
    loop_with_break(x, y, z);
  }
  while (x > 2);
  bar(x);
}

// CHECK-LABEL: sil hidden  @_TF10statements21do_loop_with_continue 


// CHECK-LABEL: sil hidden  @{{.*}}for_loops1
func for_loops1(x: Int, c: Bool) {
  var x = x
  for i in 1..<100 {
    markUsed(i)
  }
  
  for ; x < 40;  {
   markUsed(x)
   ++x
  }
  
  for var i = 0; i < 100; ++i {
  }
  
  for let i = 0; i < 100; i {
  }
}

// CHECK-LABEL: sil hidden  @{{.*}}for_loops2
func for_loops2() {
  // rdar://problem/19316670
  // CHECK: [[NEXT:%[0-9]+]] = function_ref @_TFVs17IndexingGenerator4next
  // CHECK-NEXT: alloc_stack $Optional<MyClass>
  // CHECK-NEXT: apply [[NEXT]]<Array<MyClass>,
  // CHECK: class_method [[OBJ:%[0-9]+]] : $MyClass, #MyClass.foo!1
  let objects = [MyClass(), MyClass() ]
  for obj in objects {
    obj.foo()
  }

  return 
}

func void_return() {
  let b:Bool
  if b {
    return
  }
}
// CHECK-LABEL: sil hidden  @_TF10statements11void_return
// CHECK: cond_br {{%[0-9]+}}, [[BB1:bb[0-9]+]], [[BB2:bb[0-9]+]]
// CHECK: [[BB1]]:
// CHECK:   br [[EPILOG:bb[0-9]+]]
// CHECK: [[BB2]]:
// CHECK:   br [[EPILOG]]
// CHECK: [[EPILOG]]:
// CHECK:   [[R:%[0-9]+]] = tuple ()
// CHECK:   return [[R]]

func foo() {}

// <rdar://problem/13549626>
// CHECK-LABEL: sil hidden  @_TF10statements14return_from_if
func return_from_if(a: Bool) -> Int {
  // CHECK: bb0(%0 : $Bool):
  // CHECK: cond_br {{.*}}, [[THEN:bb[0-9]+]], [[ELSE:bb[0-9]+]]
  if a {
    // CHECK: [[THEN]]:
    // CHECK: br [[EPILOG:bb[0-9]+]]({{%.*}})
    return 1
  } else {
    // CHECK: [[ELSE]]:
    // CHECK: br [[EPILOG]]({{%.*}})
    return 0
  }
  // CHECK-NOT: function_ref @foo
  // CHECK: [[EPILOG]]([[RET:%.*]] : $Int):
  // CHECK:   return [[RET]]
  foo()  // expected-warning {{will never be executed}}
}

class C {}

func use(c: C) {}

func for_each_loop(x: [C]) {
  for i in x {
    use(i)
  }
  _ = 0
}

// CHECK-LABEL: sil hidden @{{.*}}test_break
func test_break(i : Int) {
  switch i {
  case (let x) where x != 17: 
    if x == 42 { break } 
    markUsed(x)
  default:
    break
  }
}


// <rdar://problem/19150249> Allow labeled "break" from an "if" statement

// CHECK-LABEL: sil hidden @_TF10statements13test_if_breakFGSqCS_1C_T_
func test_if_break(c : C?) {
label1:
  // CHECK: switch_enum %0 : $Optional<C>, case #Optional.Some!enumelt.1: [[TRUE:bb[0-9]+]], default [[FALSE:bb[0-9]+]]
  if let x = c {
// CHECK: [[TRUE]]({{.*}} : $C):

    // CHECK: apply
    foo()

    // CHECK: strong_release
    // CHECK: br [[FALSE:bb[0-9]+]]
    break label1
    use(x)  // expected-warning {{will never be executed}}
  }

  // CHECK: [[FALSE]]:
  // CHECK: return
}

// CHECK-LABEL: sil hidden @_TF10statements18test_if_else_breakFGSqCS_1C_T_
func test_if_else_break(c : C?) {
label2:
  // CHECK: switch_enum %0 : $Optional<C>, case #Optional.Some!enumelt.1: [[TRUE:bb[0-9]+]], default [[FALSE:bb[0-9]+]]
  if let x = c {
    // CHECK: [[TRUE]]({{.*}} : $C):
    use(x)
    // CHECK: br [[CONT:bb[0-9]+]]
  } else {
    // CHECK: [[FALSE]]:
    // CHECK: apply
    // CHECK: br [[CONT]]
    foo()
    break label2
    foo() // expected-warning {{will never be executed}}
  }
  // CHECK: [[CONT]]:
  // CHECK: return
}

// CHECK-LABEL: sil hidden @_TF10statements23test_if_else_then_breakFTSbGSqCS_1C__T_
func test_if_else_then_break(a : Bool, _ c : C?) {
  label3:
  // CHECK: switch_enum %1 : $Optional<C>, case #Optional.Some!enumelt.1: [[TRUE:bb[0-9]+]], default [[FALSE:bb[0-9]+]]
  if let x = c {
    // CHECK: [[TRUE]]({{.*}} : $C):
    use(x)
    // CHECK: br [[CONT:bb[0-9]+]]
  } else if a {
    // CHECK: [[FALSE]]:
    // CHECK: cond_br {{.*}}, [[TRUE2:bb[0-9]+]], [[FALSE2:bb[0-9]+]]
    // CHECK: apply
    // CHECK: br [[CONT]]
    foo()
    break label3
    foo()    // expected-warning {{will never be executed}}
  }

  // CHECK: [[FALSE2]]:
  // CHECK: br [[CONT]]
  // CHECK: [[CONT]]:
  // CHECK: return


}


// CHECK-LABEL: sil hidden @_TF10statements13test_if_breakFSbT_
func test_if_break(a : Bool) {
  // CHECK: br [[LOOP:bb[0-9]+]]
  // CHECK: [[LOOP]]:
  // CHECK: function_ref @_TFSb21_getBuiltinLogicValue
  // CHECK-NEXT: apply
  // CHECK-NEXT: cond_br {{.*}}, [[LOOPTRUE:bb[0-9]+]], [[OUT:bb[0-9]+]]
  while a {
    if a {
      foo()
      break  // breaks out of while, not if.
    }
    foo()
  }

  // CHECK: [[LOOPTRUE]]:
  // CHECK: function_ref @_TFSb21_getBuiltinLogicValue
  // CHECK-NEXT: apply
  // CHECK-NEXT: cond_br {{.*}}, [[IFTRUE:bb[0-9]+]], [[IFFALSE:bb[0-9]+]]

  // [[IFTRUE]]:
  // CHECK: function_ref statements.foo
  // CHECK: br [[OUT]]

  // CHECK: [[IFFALSE]]:
  // CHECK: function_ref statements.foo
  // CHECK: br [[LOOP]]

  // CHECK: [[OUT]]:
  // CHECK:   return
}

// CHECK-LABEL: sil hidden @_TF10statements7test_doFT_T_
func test_do() {
  // CHECK: [[BAR:%.*]] = function_ref @_TF10statements3barFSiT_
  // CHECK: integer_literal $Builtin.Int2048, 0
  // CHECK: apply [[BAR]](
  bar(0)
  // CHECK-NOT: br bb
  do {
    // CHECK: [[CTOR:%.*]] = function_ref @_TFC10statements7MyClassC
    // CHECK: [[OBJ:%.*]] = apply [[CTOR]](
    let obj = MyClass()
    _ = obj
    
    // CHECK: [[BAR:%.*]] = function_ref @_TF10statements3barFSiT_
    // CHECK: integer_literal $Builtin.Int2048, 1
    // CHECK: apply [[BAR]](
    bar(1)

    // CHECK-NOT: br bb
    // CHECK: strong_release [[OBJ]]
    // CHECK-NOT: br bb
  }

  // CHECK: [[BAR:%.*]] = function_ref @_TF10statements3barFSiT_
  // CHECK: integer_literal $Builtin.Int2048, 2
  // CHECK: apply [[BAR]](
  bar(2)
}

// CHECK-LABEL: sil hidden @_TF10statements15test_do_labeledFT_T_
func test_do_labeled() {
  // CHECK: [[BAR:%.*]] = function_ref @_TF10statements3barFSiT_
  // CHECK: integer_literal $Builtin.Int2048, 0
  // CHECK: apply [[BAR]](
  bar(0)
  // CHECK: br bb1
  // CHECK: bb1:
  lbl: do {
    // CHECK: [[CTOR:%.*]] = function_ref @_TFC10statements7MyClassC
    // CHECK: [[OBJ:%.*]] = apply [[CTOR]](
    let obj = MyClass()
    _ = obj

    // CHECK: [[BAR:%.*]] = function_ref @_TF10statements3barFSiT_
    // CHECK: integer_literal $Builtin.Int2048, 1
    // CHECK: apply [[BAR]](
    bar(1)

    // CHECK: [[GLOBAL:%.*]] = function_ref @_TF10statementsau11global_condSb
    // CHECK: cond_br {{%.*}}, bb2, bb3
    if (global_cond) {
      // CHECK: bb2:
      // CHECK: strong_release [[OBJ]]
      // CHECK: br bb1
      continue lbl
    }

    // CHECK: bb3:
    // CHECK: [[BAR:%.*]] = function_ref @_TF10statements3barFSiT_
    // CHECK: integer_literal $Builtin.Int2048, 2
    // CHECK: apply [[BAR]](
    bar(2)

    // CHECK: [[GLOBAL:%.*]] = function_ref @_TF10statementsau11global_condSb
    // CHECK: cond_br {{%.*}}, bb4, bb5
    if (global_cond) {
      // CHECK: bb4:
      // CHECK: strong_release [[OBJ]]
      // CHECK: br bb6
      break lbl
    }

    // CHECK: bb5:
    // CHECK: [[BAR:%.*]] = function_ref @_TF10statements3barFSiT_
    // CHECK: integer_literal $Builtin.Int2048, 3
    // CHECK: apply [[BAR]](
    bar(3)

    // CHECK: strong_release [[OBJ]]
    // CHECK: br bb6
  }

  // CHECK: [[BAR:%.*]] = function_ref @_TF10statements3barFSiT_
  // CHECK: integer_literal $Builtin.Int2048, 4
  // CHECK: apply [[BAR]](
  bar(4)
}


func callee1() {}
func callee2() {}
func callee3() {}

// CHECK-LABEL: sil hidden @_TF10statements11defer_test1FT_T_
func defer_test1() {
  defer { callee1() }
  defer { callee2() }
  callee3()
  
  // CHECK: [[C3:%.*]] = function_ref @{{.*}}callee3FT_T_
  // CHECK: apply [[C3]]
  // CHECK: [[C2:%.*]] = function_ref @{{.*}}_TFF10statements11defer_test1FT_T_L0_6$deferFT_T_
  // CHECK: apply [[C2]]
  // CHECK: [[C1:%.*]] = function_ref @{{.*}}_TFF10statements11defer_test1FT_T_L_6$deferFT_T_
  // CHECK: apply [[C1]]
}
// CHECK: sil shared @_TFF10statements11defer_test1FT_T_L_6$deferFT_T_
// CHECK: function_ref @{{.*}}callee1FT_T_

// CHECK: sil shared @_TFF10statements11defer_test1FT_T_L0_6$deferFT_T_
// CHECK: function_ref @{{.*}}callee2FT_T_

// CHECK-LABEL: sil hidden @_TF10statements11defer_test2FSbT_
func defer_test2(cond : Bool) {
  // CHECK: [[C3:%.*]] = function_ref @{{.*}}callee3FT_T_
  // CHECK: apply [[C3]]
  // CHECK: br [[LOOP:bb[0-9]+]]
  callee3()
  
// CHECK: [[LOOP]]:
// test the condition.
// CHECK:  [[CONDTRUE:%.*]] = apply {{.*}}(%0)
// CHECK: cond_br [[CONDTRUE]], [[BODY:bb[0-9]+]], [[EXIT:bb[0-9]+]]
  while cond {
// CHECK: [[BODY]]:
  // CHECK: [[C2:%.*]] = function_ref @{{.*}}callee2FT_T_
  // CHECK: apply [[C2]]

  // CHECK: [[C1:%.*]] = function_ref @_TFF10statements11defer_test2FSbT_L_6$deferFT_T_
  // CHECK: apply [[C1]]
  // CHECK: br [[EXIT]]
    defer { callee1() }
    callee2()
    break
  }
  
// CHECK: [[EXIT]]:
// CHECK: [[C3:%.*]] = function_ref @{{.*}}callee3FT_T_
// CHECK: apply [[C3]]

  callee3()
}

func generic_callee_1<T>(_: T) {}
func generic_callee_2<T>(_: T) {}
func generic_callee_3<T>(_: T) {}

// CHECK-LABEL: sil hidden @_TF10statements16defer_in_generic
func defer_in_generic<T>(x: T) {
  // CHECK: [[C3:%.*]] = function_ref @_TF10statements16generic_callee_3
  // CHECK: apply [[C3]]<T>
  // CHECK: [[C2:%.*]] = function_ref @_TFF10statements16defer_in_generic
  // CHECK: apply [[C2]]<T>
  // CHECK: [[C1:%.*]] = function_ref @_TFF10statements16defer_in_generic
  // CHECK: apply [[C1]]<T>
  defer { generic_callee_1(x) }
  defer { generic_callee_2(x) }
  generic_callee_3(x)
}

// CHECK-LABEL: sil hidden @_TF10statements13defer_mutableFSiT_
func defer_mutable(x: Int) {
  var x = x
  // CHECK: [[BOX:%.*]] = alloc_box $Int
  // CHECK-NOT: [[BOX]]#0
  // CHECK: function_ref @_TFF10statements13defer_mutableFSiT_L_6$deferfT_T_ : $@convention(thin) (@inout Int) -> ()
  // CHECK-NOT: [[BOX]]#0
  // CHECK: strong_release [[BOX]]#0
  defer { _ = x }
}

protocol StaticFooProtocol { static func foo() }

func testDeferOpenExistential(b: Bool, type: StaticFooProtocol.Type) {
  defer { type.foo() }
  if b { return }
  return
}




// CHECK-LABEL: sil hidden @_TF10statements22testRequireExprPatternFSiT_

func testRequireExprPattern(a : Int) {
  marker_1()
  // CHECK: [[M1:%[0-9]+]] = function_ref @_TF10statements8marker_1FT_T_ : $@convention(thin) () -> ()
  // CHECK-NEXT: apply [[M1]]() : $@convention(thin) () -> ()

  // CHECK: function_ref static Swift.~= infix <A where A: Swift.Equatable> (A, A) -> Swift.Bool
  // CHECK: cond_br {{.*}}, bb1, bb2
  guard case 4 = a else { marker_2(); return }

  // Fall through case comes first.

  // CHECK: bb1:
  // CHECK: [[M3:%[0-9]+]] = function_ref @_TF10statements8marker_3FT_T_ : $@convention(thin) () -> ()
  // CHECK-NEXT: apply [[M3]]() : $@convention(thin) () -> ()
  // CHECK-NEXT: br bb3
  marker_3()

  // CHECK: bb2:
  // CHECK: [[M2:%[0-9]+]] = function_ref @_TF10statements8marker_2FT_T_ : $@convention(thin) () -> ()
  // CHECK-NEXT: apply [[M2]]() : $@convention(thin) () -> ()
  // CHECK-NEXT: br bb3

  // CHECK: bb3:
  // CHECK-NEXT: tuple ()
  // CHECK-NEXT: return
}


// CHECK-LABEL: sil hidden @_TF10statements20testRequireOptional1FGSqSi_Si
// CHECK: bb0(%0 : $Optional<Int>):
// CHECK-NEXT:   debug_value %0 : $Optional<Int>  // let a
// CHECK-NEXT:   switch_enum %0 : $Optional<Int>, case #Optional.Some!enumelt.1: bb1, default bb2
func testRequireOptional1(a : Int?) -> Int {

  // CHECK: bb1(%3 : $Int):
  // CHECK-NEXT:   debug_value %3 : $Int  // let t
  // CHECK-NEXT:   return %3 : $Int
  guard let t = a else { abort() }

  // CHECK:  bb2:
  // CHECK-NEXT:    // function_ref statements.abort () -> ()
  // CHECK-NEXT:    %6 = function_ref @_TF10statements5abortFT_T_
  // CHECK-NEXT:    %7 = apply %6() : $@convention(thin) @noreturn () -> ()
  // CHECK-NEXT:    unreachable
  return t
}

// CHECK-LABEL: sil hidden @_TF10statements20testRequireOptional2FGSqSS_SS
// CHECK: bb0(%0 : $Optional<String>):
// CHECK-NEXT:   debug_value %0 : $Optional<String>  // let a
// CHECK-NEXT:   retain_value %0 : $Optional<String>
// CHECK-NEXT:   switch_enum %0 : $Optional<String>, case #Optional.Some!enumelt.1: bb1, default bb2
func testRequireOptional2(a : String?) -> String {
  guard let t = a else { abort() }

  // CHECK:  bb1(%4 : $String):
  // CHECK-NEXT:   debug_value %4 : $String  // let t
  // CHECK-NEXT:   release_value %0 : $Optional<String>
  // CHECK-NEXT:   return %4 : $String

  // CHECK:        bb2:
  // CHECK-NEXT:   // function_ref statements.abort () -> ()
  // CHECK-NEXT:   %8 = function_ref @_TF10statements5abortFT_T_
  // CHECK-NEXT:   %9 = apply %8()
  // CHECK-NEXT:   unreachable
  return t
}

enum MyOpt<T> {
  case None, Some(T)
}

// CHECK-LABEL: sil hidden @_TF10statements28testAddressOnlyEnumInRequire
// CHECK: bb0(%0 : $*T, %1 : $*MyOpt<T>):
// CHECK-NEXT: debug_value_addr %1 : $*MyOpt<T>  // let a
// CHECK-NEXT: %3 = alloc_stack $T  // let t
// CHECK-NEXT: %4 = alloc_stack $MyOpt<T>
// CHECK-NEXT: copy_addr %1 to [initialization] %4#1 : $*MyOpt<T>
// CHECK-NEXT: switch_enum_addr %4#1 : $*MyOpt<T>, case #MyOpt.Some!enumelt.1: bb2, default bb1
func testAddressOnlyEnumInRequire<T>(a : MyOpt<T>) -> T {
  // CHECK:  bb1:
  // CHECK-NEXT:   dealloc_stack %4#0
  // CHECK-NEXT:   dealloc_stack %3#0
  // CHECK-NEXT:   br bb3
  guard let t = a else { abort() }

  // CHECK:    bb2:
  // CHECK-NEXT:     %10 = unchecked_take_enum_data_addr %4#1 : $*MyOpt<T>, #MyOpt.Some!enumelt.1
  // CHECK-NEXT:     copy_addr [take] %10 to [initialization] %3#1 : $*T
  // CHECK-NEXT:     dealloc_stack %4#0
  // CHECK-NEXT:     copy_addr [take] %3#1 to [initialization] %0 : $*T
  // CHECK-NEXT:     dealloc_stack %3#0
  // CHECK-NEXT:     destroy_addr %1 : $*MyOpt<T>
  // CHECK-NEXT:     tuple ()
  // CHECK-NEXT:     return

  // CHECK:    bb3:
  // CHECK-NEXT:     // function_ref statements.abort () -> ()
  // CHECK-NEXT:     %18 = function_ref @_TF10statements5abortFT_T_
  // CHECK-NEXT:     %19 = apply %18() : $@convention(thin) @noreturn () -> ()
  // CHECK-NEXT:     unreachable

  return t
}



// CHECK-LABEL: sil hidden @_TF10statements19testCleanupEmission
// <rdar://problem/20563234> let-else problem: cleanups for bound patterns shouldn't be run in the else block
protocol MyProtocol {}
func testCleanupEmission<T>(x: T) {
  // SILGen shouldn't crash/verify abort on this example.
  guard let x2 = x as? MyProtocol else { return }
  _ = x2
}


// CHECK-LABEL: sil hidden @_TF10statements15test_is_patternFCS_9BaseClassT_
func test_is_pattern(y : BaseClass) {
  // checked_cast_br %0 : $BaseClass to $DerivedClass
  guard case is DerivedClass = y else { marker_1(); return }

  marker_2()
}

// CHECK-LABEL: sil hidden @_TF10statements15test_as_patternFCS_9BaseClassCS_12DerivedClass
func test_as_pattern(y : BaseClass) -> DerivedClass {
  // checked_cast_br %0 : $BaseClass to $DerivedClass
  guard case let result as DerivedClass = y else {  }
  // CHECK: bb{{.*}}({{.*}} : $DerivedClass):


  // CHECK: bb{{.*}}([[PTR:%[0-9]+]] : $DerivedClass):
  // CHECK-NEXT: debug_value [[PTR]] : $DerivedClass  // let result
  // CHECK-NEXT: strong_release %0 : $BaseClass
  // CHECK-NEXT: return [[PTR]] : $DerivedClass
  return result
}
// CHECK-LABEL: sil hidden @_TF10statements22let_else_tuple_bindingFGSqTSiSi__Si
func let_else_tuple_binding(a : (Int, Int)?) -> Int {

  // CHECK: bb0(%0 : $Optional<(Int, Int)>):
  // CHECK-NEXT:   debug_value %0 : $Optional<(Int, Int)>  // let a
  // CHECK-NEXT:   switch_enum %0 : $Optional<(Int, Int)>, case #Optional.Some!enumelt.1: bb1, default bb2

  guard let (x, y) = a else { }
  _ = y
  return x

  // CHECK: bb1(%3 : $(Int, Int)):
  // CHECK-NEXT:   %4 = tuple_extract %3 : $(Int, Int), 0
  // CHECK-NEXT:   debug_value %4 : $Int  // let x
  // CHECK-NEXT:   %6 = tuple_extract %3 : $(Int, Int), 1
  // CHECK-NEXT:   debug_value %6 : $Int  // let y
  // CHECK-NEXT:   return %4 : $Int
}

