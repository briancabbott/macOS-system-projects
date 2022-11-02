// RUN: %target-swift-frontend -parse-stdlib -emit-silgen -verify %s | FileCheck %s

import Swift

class Cat {}

enum HomeworkError : ErrorType {
  case TooHard
  case TooMuch
  case CatAteIt(Cat)
}

// CHECK: sil hidden @_TF6errors10make_a_cat{{.*}} : $@convention(thin) () -> (@owned Cat, @error ErrorType) {
// CHECK:      [[T0:%.*]] = function_ref @_TFC6errors3CatC{{.*}} : $@convention(thin) (@thick Cat.Type) -> @owned Cat
// CHECK-NEXT: [[T1:%.*]] = metatype $@thick Cat.Type 
// CHECK-NEXT: [[T2:%.*]] = apply [[T0]]([[T1]])
// CHECK-NEXT: return [[T2]] : $Cat
func make_a_cat() throws -> Cat {
  return Cat()
}

// CHECK: sil hidden @_TF6errors15dont_make_a_cat{{.*}} : $@convention(thin) () -> (@owned Cat, @error ErrorType) {
// CHECK:      [[BOX:%.*]] = alloc_existential_box $ErrorType, $HomeworkError
// CHECK:      [[T0:%.*]] = function_ref @_TFO6errors13HomeworkError7TooHardFMS0_S0_ : $@convention(thin) (@thin HomeworkError.Type) -> @owned HomeworkError
// CHECK-NEXT: [[T1:%.*]] = metatype $@thin HomeworkError.Type
// CHECK-NEXT: [[T2:%.*]] = apply [[T0]]([[T1]])
// CHECK-NEXT: store [[T2]] to [[BOX]]#1
// CHECK-NEXT: builtin "willThrow"
// CHECK-NEXT: throw [[BOX]]#0
func dont_make_a_cat() throws -> Cat {
  throw HomeworkError.TooHard
}

// CHECK: sil hidden @_TF6errors11dont_return{{.*}} : $@convention(thin) <T> (@out T, @in T) -> @error ErrorType {
// CHECK:      [[BOX:%.*]] = alloc_existential_box $ErrorType, $HomeworkError
// CHECK:      [[T0:%.*]] = function_ref @_TFO6errors13HomeworkError7TooMuchFMS0_S0_ : $@convention(thin) (@thin HomeworkError.Type) -> @owned HomeworkError
// CHECK-NEXT: [[T1:%.*]] = metatype $@thin HomeworkError.Type
// CHECK-NEXT: [[T2:%.*]] = apply [[T0]]([[T1]])
// CHECK-NEXT: store [[T2]] to [[BOX]]#1
// CHECK-NEXT: builtin "willThrow"
// CHECK-NEXT: destroy_addr %1 : $*T
// CHECK-NEXT: throw [[BOX]]#0
func dont_return<T>(argument: T) throws -> T {
  throw HomeworkError.TooMuch
}

// CHECK:    sil hidden @_TF6errors16all_together_nowFSbCS_3Cat : $@convention(thin) (Bool) -> @owned Cat {
// CHECK:    bb0(%0 : $Bool):
// CHECK:      [[DR_FN:%.*]] = function_ref @_TF6errors11dont_returnur{{.*}} :

//   Branch on the flag.
// CHECK:      cond_br {{%.*}}, [[FLAG_TRUE:bb[0-9]+]], [[FLAG_FALSE:bb[0-9]+]]

//   In the true case, call make_a_cat().
// CHECK:    [[FLAG_TRUE]]:
// CHECK:      [[MAC_FN:%.*]] = function_ref @_TF6errors10make_a_catFzT_CS_3Cat : $@convention(thin) () -> (@owned Cat, @error ErrorType)
// CHECK-NEXT: try_apply [[MAC_FN]]() : $@convention(thin) () -> (@owned Cat, @error ErrorType), normal [[MAC_NORMAL:bb[0-9]+]], error [[MAC_ERROR:bb[0-9]+]]
// CHECK:    [[MAC_NORMAL]]([[T0:%.*]] : $Cat):
// CHECK-NEXT: br [[TERNARY_CONT:bb[0-9]+]]([[T0]] : $Cat)

//   In the false case, call dont_make_a_cat().
// CHECK:    [[FLAG_FALSE]]:
// CHECK:      [[DMAC_FN:%.*]] = function_ref @_TF6errors15dont_make_a_catFzT_CS_3Cat : $@convention(thin) () -> (@owned Cat, @error ErrorType)
// CHECK-NEXT: try_apply [[DMAC_FN]]() : $@convention(thin) () -> (@owned Cat, @error ErrorType), normal [[DMAC_NORMAL:bb[0-9]+]], error [[DMAC_ERROR:bb[0-9]+]]
// CHECK:    [[DMAC_NORMAL]]([[T0:%.*]] : $Cat):
// CHECK-NEXT: br [[TERNARY_CONT]]([[T0]] : $Cat)

//   Merge point for the ternary operator.  Call dont_return with the result.
// CHECK:    [[TERNARY_CONT]]([[T0:%.*]] : $Cat):
// CHECK-NEXT: [[ARG_TEMP:%.*]] = alloc_stack $Cat
// CHECK-NEXT: store [[T0]] to [[ARG_TEMP]]#1
// CHECK-NEXT: [[RET_TEMP:%.*]] = alloc_stack $Cat
// CHECK-NEXT: try_apply [[DR_FN]]<Cat>([[RET_TEMP]]#1, [[ARG_TEMP]]#1) : $@convention(thin) <τ_0_0> (@out τ_0_0, @in τ_0_0) -> @error ErrorType, normal [[DR_NORMAL:bb[0-9]+]], error [[DR_ERROR:bb[0-9]+]]
// CHECK:    [[DR_NORMAL]]({{%.*}} : $()):
// CHECK-NEXT: [[T0:%.*]] = load [[RET_TEMP]]#1 : $*Cat
// CHECK-NEXT: dealloc_stack [[RET_TEMP]]#0
// CHECK-NEXT: dealloc_stack [[ARG_TEMP]]#0
// CHECK-NEXT: br [[RETURN:bb[0-9]+]]([[T0]] : $Cat)

//   Return block.
// CHECK:    [[RETURN]]([[T0:%.*]] : $Cat):
// CHECK-NEXT: return [[T0]] : $Cat

//   Catch dispatch block.
// CHECK:    [[CATCH:bb[0-9]+]]([[ERROR:%.*]] : $ErrorType):
// CHECK-NEXT: [[SRC_TEMP:%.*]] = alloc_stack $ErrorType
// CHECK-NEXT: store [[ERROR]] to [[SRC_TEMP]]#1
// CHECK-NEXT: [[DEST_TEMP:%.*]] = alloc_stack $HomeworkError
// CHECK-NEXT: checked_cast_addr_br copy_on_success ErrorType in [[SRC_TEMP]]#1 : $*ErrorType to HomeworkError in [[DEST_TEMP]]#1 : $*HomeworkError, [[IS_HWE:bb[0-9]+]], [[NOT_HWE:bb[0-9]+]]

//   Catch HomeworkError.
// CHECK:    [[IS_HWE]]:
// CHECK-NEXT: [[T0:%.*]] = load [[DEST_TEMP]]#1 : $*HomeworkError
// CHECK-NEXT: switch_enum [[T0]] : $HomeworkError, case #HomeworkError.CatAteIt!enumelt.1: [[MATCH:bb[0-9]+]], default [[NO_MATCH:bb[0-9]+]]

//   Catch HomeworkError.CatAteIt.
// CHECK:    [[MATCH]]([[T0:%.*]] : $Cat):
// CHECK-NEXT: debug_value
// CHECK-NEXT: dealloc_stack [[DEST_TEMP]]#0
// CHECK-NEXT: destroy_addr [[SRC_TEMP]]#1
// CHECK-NEXT: dealloc_stack [[SRC_TEMP]]#0
// CHECK-NEXT: br [[RETURN]]([[T0]] : $Cat)

//   Catch other HomeworkErrors.
// CHECK:    [[NO_MATCH]]:
// CHECK-NEXT: dealloc_stack [[DEST_TEMP]]#0
// CHECK-NEXT: dealloc_stack [[SRC_TEMP]]#0
// CHECK-NEXT: br [[CATCHALL:bb[0-9]+]]

//   Catch other types.
// CHECK:    [[NOT_HWE]]:
// CHECK-NEXT: dealloc_stack [[DEST_TEMP]]#0
// CHECK-NEXT: dealloc_stack [[SRC_TEMP]]#0
// CHECK-NEXT: br [[CATCHALL:bb[0-9]+]]

//   Catch all.
// CHECK:    [[CATCHALL]]:
// CHECK:      [[T0:%.*]] = function_ref @_TFC6errors3CatC{{.*}} : $@convention(thin) (@thick Cat.Type) -> @owned Cat
// CHECK-NEXT: [[T1:%.*]] = metatype $@thick Cat.Type
// CHECK-NEXT: [[T2:%.*]] = apply [[T0]]([[T1]])
// CHECK-NEXT: strong_release [[ERROR]] : $ErrorType
// CHECK-NEXT: br [[RETURN]]([[T2]] : $Cat)

//   Landing pad.
// CHECK:    [[MAC_ERROR]]([[T0:%.*]] : $ErrorType):
// CHECK-NEXT: br [[CATCH]]([[T0]] : $ErrorType)
// CHECK:    [[DMAC_ERROR]]([[T0:%.*]] : $ErrorType):
// CHECK-NEXT: br [[CATCH]]([[T0]] : $ErrorType)
// CHECK:    [[DR_ERROR]]([[T0:%.*]] : $ErrorType):
// CHECK-NEXT: dealloc_stack
// CHECK-NEXT: dealloc_stack
// CHECK-NEXT: br [[CATCH]]([[T0]] : $ErrorType)
func all_together_now(flag: Bool) -> Cat {
  do {
    return try dont_return(flag ? make_a_cat() : dont_make_a_cat())
  } catch HomeworkError.CatAteIt(let cat) {
    return cat
  } catch _ {
    return Cat()
  }
}

//   Catch in non-throwing context.
// CHECK-LABEL: sil hidden @_TF6errors11catch_a_catFT_CS_3Cat : $@convention(thin) () -> @owned Cat
// CHECK-NEXT: bb0:
// CHECK:      [[F:%.*]] = function_ref @_TFC6errors3CatC{{.*}} : $@convention(thin) (@thick Cat.Type) -> @owned Cat
// CHECK-NEXT: [[M:%.*]] = metatype $@thick Cat.Type
// CHECK-NEXT: [[V:%.*]] = apply [[F]]([[M]])
// CHECK-NEXT: return [[V]] : $Cat
func catch_a_cat() -> Cat {
  do {
    return Cat()
  } catch _ as HomeworkError {}  // expected-warning {{'catch' block is unreachable because no errors are thrown in 'do' block}}
}

// Initializers.
class HasThrowingInit {
  var field: Int
  init(value: Int) throws {
    field = value
  }
}
// CHECK-LABEL: sil hidden @_TFC6errors15HasThrowingInitc{{.*}} : $@convention(method) (Int, @owned HasThrowingInit) -> (@owned HasThrowingInit, @error ErrorType) {
// CHECK:      [[T0:%.*]] = mark_uninitialized [rootself] %1 : $HasThrowingInit
// CHECK-NEXT: [[T1:%.*]] = ref_element_addr [[T0]] : $HasThrowingInit
// CHECK-NEXT: assign %0 to [[T1]] : $*Int
// CHECK-NEXT: return [[T0]] : $HasThrowingInit

// CHECK-LABEL: sil hidden @_TFC6errors15HasThrowingInitC{{.*}} : $@convention(thin) (Int, @thick HasThrowingInit.Type) -> (@owned HasThrowingInit, @error ErrorType)
// CHECK:      [[SELF:%.*]] = alloc_ref $HasThrowingInit
// CHECK:      [[T0:%.*]] = function_ref @_TFC6errors15HasThrowingInitc{{.*}} : $@convention(method) (Int, @owned HasThrowingInit) -> (@owned HasThrowingInit, @error ErrorType)
// CHECK-NEXT: try_apply [[T0]](%0, [[SELF]]) : $@convention(method) (Int, @owned HasThrowingInit) -> (@owned HasThrowingInit, @error ErrorType), normal bb1, error bb2
// CHECK:    bb1([[SELF:%.*]] : $HasThrowingInit):
// CHECK-NEXT: return [[SELF]]
// CHECK:    bb2([[ERROR:%.*]] : $ErrorType):
// CHECK-NEXT: builtin "willThrow"
// CHECK-NEXT: throw [[ERROR]]


enum ColorError : ErrorType {
  case Red, Green, Blue
}

//CHECK-LABEL: sil hidden @_TF6errors6IThrowFzT_Vs5Int32
//CHECK: builtin "willThrow"
//CHECK-NEXT: throw
func IThrow() throws -> Int32 {
  throw ColorError.Red
  return 0  // expected-warning {{will never be executed}}
}

// Make sure that we are not emitting calls to 'willThrow' on rethrow sites.
//CHECK-LABEL: sil hidden @_TF6errors12DoesNotThrowFzT_Vs5Int32
//CHECK-NOT: builtin "willThrow"
//CHECK: return
func DoesNotThrow() throws -> Int32 {
  try IThrow()
  return 2
}

// rdar://20782111
protocol Doomed {
  func check() throws
}

// CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWV6errors12DoomedStructS_6DoomedS_FS1_5check{{.*}} : $@convention(witness_method) (@in_guaranteed DoomedStruct) -> @error ErrorType
// CHECK:      [[TEMP:%.*]] = alloc_stack $DoomedStruct
// CHECK:      copy_addr %0 to [initialization] [[TEMP]]#1
// CHECK:      [[SELF:%.*]] = load [[TEMP]]#1 : $*DoomedStruct
// CHECK:      [[T0:%.*]] = function_ref @_TFV6errors12DoomedStruct5check{{.*}} : $@convention(method) (DoomedStruct) -> @error ErrorType
// CHECK-NEXT: try_apply [[T0]]([[SELF]])
// CHECK:    bb1([[T0:%.*]] : $()):
// CHECK:      dealloc_stack [[TEMP]]#0
// CHECK:      return [[T0]] : $()
// CHECK:    bb2([[T0:%.*]] : $ErrorType):
// CHECK:      builtin "willThrow"([[T0]] : $ErrorType)
// CHECK:      dealloc_stack [[TEMP]]#0
// CHECK:      throw [[T0]] : $ErrorType
struct DoomedStruct : Doomed {
  func check() throws {}
}

// CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWC6errors11DoomedClassS_6DoomedS_FS1_5check{{.*}} : $@convention(witness_method) (@in_guaranteed DoomedClass) -> @error ErrorType {
// CHECK:      [[TEMP:%.*]] = alloc_stack $DoomedClass
// CHECK:      copy_addr %0 to [initialization] [[TEMP]]#1
// CHECK:      [[SELF:%.*]] = load [[TEMP]]#1 : $*DoomedClass
// CHECK:      [[T0:%.*]] = class_method [[SELF]] : $DoomedClass, #DoomedClass.check!1 : DoomedClass -> () throws -> () , $@convention(method) (@guaranteed DoomedClass) -> @error ErrorType
// CHECK-NEXT: try_apply [[T0]]([[SELF]])
// CHECK:    bb1([[T0:%.*]] : $()):
// CHECK:      strong_release [[SELF]] : $DoomedClass
// CHECK:      dealloc_stack [[TEMP]]#0
// CHECK:      return [[T0]] : $()
// CHECK:    bb2([[T0:%.*]] : $ErrorType):
// CHECK:      builtin "willThrow"([[T0]] : $ErrorType)
// CHECK:      strong_release [[SELF]] : $DoomedClass
// CHECK:      dealloc_stack [[TEMP]]#0
// CHECK:      throw [[T0]] : $ErrorType
class DoomedClass : Doomed {
  func check() throws {}
}

// CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWV6errors11HappyStructS_6DoomedS_FS1_5check{{.*}} : $@convention(witness_method) (@in_guaranteed HappyStruct) -> @error ErrorType
// CHECK:      [[TEMP:%.*]] = alloc_stack $HappyStruct
// CHECK:      copy_addr %0 to [initialization] [[TEMP]]#1
// CHECK:      [[SELF:%.*]] = load [[TEMP]]#1 : $*HappyStruct
// CHECK:      [[T0:%.*]] = function_ref @_TFV6errors11HappyStruct5check{{.*}} : $@convention(method) (HappyStruct) -> ()
// CHECK:      [[T1:%.*]] = apply [[T0]]([[SELF]])
// CHECK:      dealloc_stack [[TEMP]]#0
// CHECK:      return [[T1]] : $()
struct HappyStruct : Doomed {
  func check() {}
}

// CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWC6errors10HappyClassS_6DoomedS_FS1_5check{{.*}} : $@convention(witness_method) (@in_guaranteed HappyClass) -> @error ErrorType
// CHECK:      [[TEMP:%.*]] = alloc_stack $HappyClass
// CHECK:      copy_addr %0 to [initialization] [[TEMP]]#1
// CHECK:      [[SELF:%.*]] = load [[TEMP]]#1 : $*HappyClass
// CHECK:      [[T0:%.*]] = class_method [[SELF]] : $HappyClass, #HappyClass.check!1 : HappyClass -> () -> () , $@convention(method) (@guaranteed HappyClass) -> ()
// CHECK:      [[T1:%.*]] = apply [[T0]]([[SELF]])
// CHECK:      strong_release [[SELF]] : $HappyClass
// CHECK:      dealloc_stack [[TEMP]]#0
// CHECK:      return [[T1]] : $()
class HappyClass : Doomed {
  func check() {}
}

func create<T>(fn: () throws -> T) throws -> T {
  return try fn()
}
func testThunk(fn: () throws -> Int) throws -> Int {
  return try create(fn)
}
// CHECK-LABEL: sil shared [transparent] [reabstraction_thunk] @_TTRXFo__dSizoPs9ErrorType__XFo__iSizoPS___ : $@convention(thin) (@out Int, @owned @callee_owned () -> (Int, @error ErrorType)) -> @error ErrorType
// CHECK: bb0(%0 : $*Int, %1 : $@callee_owned () -> (Int, @error ErrorType)):
// CHECK:   try_apply %1()
// CHECK: bb1([[T0:%.*]] : $Int):
// CHECK:   store [[T0]] to %0 : $*Int
// CHECK:   [[T0:%.*]] = tuple ()
// CHECK:   return [[T0]]
// CHECK: bb2([[T0:%.*]] : $ErrorType):
// CHECK:   builtin "willThrow"([[T0]] : $ErrorType)
// CHECK:   throw [[T0]] : $ErrorType

func createInt(fn: () -> Int) throws {}
func testForceTry(fn: () -> Int) {
  try! createInt(fn)
}
// CHECK-LABEL: sil hidden @_TF6errors12testForceTryFFT_SiT_ : $@convention(thin) (@owned @callee_owned () -> Int) -> ()
// CHECK: [[T0:%.*]] = function_ref @_TF6errors9createIntFzFT_SiT_ : $@convention(thin) (@owned @callee_owned () -> Int) -> @error ErrorType
// CHECK: try_apply [[T0]](%0)
// CHECK: strong_release
// CHECK: return
// CHECK: builtin "unexpectedError"
// CHECK: unreachable

func testForceTryMultiple() {
  _ = try! (make_a_cat(), make_a_cat())
}

// CHECK-LABEL: sil hidden @_TF6errors20testForceTryMultipleFT_T_
// CHECK-NEXT: bb0:
// CHECK: [[FN_1:%.+]] = function_ref @_TF6errors10make_a_catFzT_CS_3Cat
// CHECK-NEXT: try_apply [[FN_1]]() : $@convention(thin) () -> (@owned Cat, @error ErrorType), normal [[SUCCESS_1:[^ ]+]], error [[CLEANUPS_1:[^ ]+]]
// CHECK: [[SUCCESS_1]]([[VALUE_1:%.+]] : $Cat)
// CHECK: [[FN_2:%.+]] = function_ref @_TF6errors10make_a_catFzT_CS_3Cat
// CHECK-NEXT: try_apply [[FN_2]]() : $@convention(thin) () -> (@owned Cat, @error ErrorType), normal [[SUCCESS_2:[^ ]+]], error [[CLEANUPS_2:[^ ]+]]
// CHECK: [[SUCCESS_2]]([[VALUE_2:%.+]] : $Cat)
// CHECK-NEXT: strong_release [[VALUE_2]] : $Cat
// CHECK-NEXT: strong_release [[VALUE_1]] : $Cat
// CHECK-NEXT: [[VOID:%.+]] = tuple ()
// CHECK-NEXT: return [[VOID]] : $()
// CHECK: [[FAILURE:.+]]([[ERROR:%.+]] : $ErrorType):
// CHECK-NEXT: = builtin "unexpectedError"([[ERROR]] : $ErrorType)
// CHECK-NEXT: unreachable
// CHECK: [[CLEANUPS_1]]([[ERROR:%.+]] : $ErrorType):
// CHECK-NEXT: br [[FAILURE]]([[ERROR]] : $ErrorType)
// CHECK: [[CLEANUPS_2]]([[ERROR:%.+]] : $ErrorType):
// CHECK-NEXT: strong_release [[VALUE_1]] : $Cat
// CHECK-NEXT: br [[FAILURE]]([[ERROR]] : $ErrorType)
// CHECK: {{^}$}}

// Make sure we balance scopes correctly inside a switch.
// <rdar://problem/20923654>
enum CatFood {
  case Canned
  case Dry
}

// Something we can switch on that throws.
func preferredFood() throws -> CatFood {
  return CatFood.Canned
}

func feedCat() throws -> Int {
  switch try preferredFood() {
  case .Canned:
    return 0
  case .Dry:
    return 1
  }
}
// CHECK-LABEL: sil hidden @_TF6errors7feedCatFzT_Si : $@convention(thin) () -> (Int, @error ErrorType)
// CHECK:   %0 = function_ref @_TF6errors13preferredFoodFzT_OS_7CatFood : $@convention(thin) () -> (CatFood, @error ErrorType)
// CHECK:   try_apply %0() : $@convention(thin) () -> (CatFood, @error ErrorType), normal bb1, error bb5
// CHECK: bb1([[VAL:%.*]] : $CatFood):
// CHECK:   switch_enum [[VAL]] : $CatFood, case #CatFood.Canned!enumelt: bb2, case #CatFood.Dry!enumelt: bb3
// CHECK: bb5([[ERROR:%.*]] : $ErrorType)
// CHECK:   throw [[ERROR]] : $ErrorType

// Throwing statements inside cases.
func getHungryCat(food: CatFood) throws -> Cat {
  switch food {
  case .Canned:
    return try make_a_cat()
  case .Dry:
    return try dont_make_a_cat()
  }
}
// errors.getHungryCat  throws (errors.CatFood) -> errors.Cat
// CHECK-LABEL: sil hidden @_TF6errors12getHungryCatFzOS_7CatFoodCS_3Cat : $@convention(thin) (CatFood) -> (@owned Cat, @error ErrorType)
// CHECK: bb0(%0 : $CatFood):
// CHECK:   switch_enum %0 : $CatFood, case #CatFood.Canned!enumelt: bb1, case #CatFood.Dry!enumelt: bb3
// CHECK: bb1:
// CHECK:   [[FN:%.*]] = function_ref @_TF6errors10make_a_catFzT_CS_3Cat : $@convention(thin) () -> (@owned Cat, @error ErrorType)
// CHECK:   try_apply [[FN]]() : $@convention(thin) () -> (@owned Cat, @error ErrorType), normal bb2, error bb6
// CHECK: bb3:
// CHECK:   [[FN:%.*]] = function_ref @_TF6errors15dont_make_a_catFzT_CS_3Cat : $@convention(thin) () -> (@owned Cat, @error ErrorType)
// CHECK:   try_apply [[FN]]() : $@convention(thin) () -> (@owned Cat, @error ErrorType), normal bb4, error bb7
// CHECK: bb6([[ERROR:%.*]] : $ErrorType):
// CHECK:   br bb8([[ERROR:%.*]] : $ErrorType)
// CHECK: bb7([[ERROR:%.*]] : $ErrorType):
// CHECK:   br bb8([[ERROR]] : $ErrorType)
// CHECK: bb8([[ERROR:%.*]] : $ErrorType):
// CHECK:   throw [[ERROR]] : $ErrorType

func take_many_cats(cats: Cat...) throws {}
func test_variadic(cat: Cat) throws {
  try take_many_cats(make_a_cat(), cat, make_a_cat(), make_a_cat())
}
// CHECK-LABEL: sil hidden @_TF6errors13test_variadicFzCS_3CatT_
// CHECK:       [[TAKE_FN:%.*]] = function_ref @_TF6errors14take_many_catsFztGSaCS_3Cat__T_ : $@convention(thin) (@owned Array<Cat>) -> @error ErrorType
// CHECK:       [[N:%.*]] = integer_literal $Builtin.Word, 4
// CHECK:       [[T0:%.*]] = function_ref @_TFs27_allocateUninitializedArray
// CHECK:       [[T1:%.*]] = apply [[T0]]<Cat>([[N]])
// CHECK:       [[ARRAY:%.*]] = tuple_extract [[T1]] :  $(Array<Cat>, Builtin.RawPointer), 0
// CHECK:       [[T2:%.*]] = tuple_extract [[T1]] :  $(Array<Cat>, Builtin.RawPointer), 1
// CHECK:       [[ELT0:%.*]] = pointer_to_address [[T2]] : $Builtin.RawPointer to $*Cat
//   Element 0.
// CHECK:       [[T0:%.*]] = function_ref @_TF6errors10make_a_catFzT_CS_3Cat : $@convention(thin) () -> (@owned Cat, @error ErrorType)
// CHECK:       try_apply [[T0]]() : $@convention(thin) () -> (@owned Cat, @error ErrorType), normal [[NORM_0:bb[0-9]+]], error [[ERR_0:bb[0-9]+]]
// CHECK:     [[NORM_0]]([[CAT0:%.*]] : $Cat):
// CHECK-NEXT:  store [[CAT0]] to [[ELT0]]
//   Element 1.
// CHECK-NEXT:  [[T0:%.*]] = integer_literal $Builtin.Word, 1
// CHECK-NEXT:  [[ELT1:%.*]] = index_addr [[ELT0]] : $*Cat, [[T0]]
// CHECK-NEXT:  strong_retain %0
// CHECK-NEXT:  store %0 to [[ELT1]]
//   Element 2.
// CHECK-NEXT:  [[T0:%.*]] = integer_literal $Builtin.Word, 2
// CHECK-NEXT:  [[ELT2:%.*]] = index_addr [[ELT0]] : $*Cat, [[T0]]
// CHECK-NEXT:  // function_ref
// CHECK-NEXT:  [[T0:%.*]] = function_ref @_TF6errors10make_a_catFzT_CS_3Cat : $@convention(thin) () -> (@owned Cat, @error ErrorType)
// CHECK-NEXT:  try_apply [[T0]]() : $@convention(thin) () -> (@owned Cat, @error ErrorType), normal [[NORM_2:bb[0-9]+]], error [[ERR_2:bb[0-9]+]]
// CHECK:     [[NORM_2]]([[CAT2:%.*]] : $Cat):
// CHECK-NEXT:  store [[CAT2]] to [[ELT2]]
//   Element 3.
// CHECK-NEXT:  [[T0:%.*]] = integer_literal $Builtin.Word, 3
// CHECK-NEXT:  [[ELT3:%.*]] = index_addr [[ELT0]] : $*Cat, [[T0]]
// CHECK-NEXT:  // function_ref
// CHECK-NEXT:  [[T0:%.*]] = function_ref @_TF6errors10make_a_catFzT_CS_3Cat : $@convention(thin) () -> (@owned Cat, @error ErrorType)
// CHECK-NEXT:  try_apply [[T0]]() : $@convention(thin) () -> (@owned Cat, @error ErrorType), normal [[NORM_3:bb[0-9]+]], error [[ERR_3:bb[0-9]+]]
// CHECK:     [[NORM_3]]([[CAT3:%.*]] : $Cat):
// CHECK-NEXT:  store [[CAT3]] to [[ELT3]]
//   Complete the call and return.
// CHECK-NEXT:  try_apply [[TAKE_FN]]([[ARRAY]]) : $@convention(thin) (@owned Array<Cat>) -> @error ErrorType, normal [[NORM_CALL:bb[0-9]+]], error [[ERR_CALL:bb[0-9]+]]
// CHECK:     [[NORM_CALL]]([[T0:%.*]] : $()):
// CHECK-NEXT:  strong_release %0 : $Cat
// CHECK-NEXT:  [[T0:%.*]] = tuple ()
// CHECK-NEXT:  return
//   Failure from element 0.
// CHECK:     [[ERR_0]]([[ERROR:%.*]] : $ErrorType):
// CHECK-NEXT:  // function_ref
// CHECK-NEXT:  [[T0:%.*]] = function_ref @_TFs29_deallocateUninitializedArray
// CHECK-NEXT:  apply [[T0]]<Cat>([[ARRAY]])
// CHECK-NEXT:  br [[RETHROW:.*]]([[ERROR]] : $ErrorType)
//   Failure from element 2.
// CHECK:     [[ERR_2]]([[ERROR:%.*]] : $ErrorType):
// CHECK-NEXT:  destroy_addr [[ELT1]]
// CHECK-NEXT:  destroy_addr [[ELT0]]
// CHECK-NEXT:  // function_ref
// CHECK-NEXT:  [[T0:%.*]] = function_ref @_TFs29_deallocateUninitializedArray
// CHECK-NEXT:  apply [[T0]]<Cat>([[ARRAY]])
// CHECK-NEXT:  br [[RETHROW]]([[ERROR]] : $ErrorType)
//   Failure from element 3.
// CHECK:     [[ERR_3]]([[ERROR:%.*]] : $ErrorType):
// CHECK-NEXT:  destroy_addr [[ELT2]]
// CHECK-NEXT:  destroy_addr [[ELT1]]
// CHECK-NEXT:  destroy_addr [[ELT0]]
// CHECK-NEXT:  // function_ref
// CHECK-NEXT:  [[T0:%.*]] = function_ref @_TFs29_deallocateUninitializedArray
// CHECK-NEXT:  apply [[T0]]<Cat>([[ARRAY]])
// CHECK-NEXT:  br [[RETHROW]]([[ERROR]] : $ErrorType)
//   Failure from call.
// CHECK:     [[ERR_CALL]]([[ERROR:%.*]] : $ErrorType):
// CHECK-NEXT:  br [[RETHROW]]([[ERROR]] : $ErrorType)
//   Rethrow.
// CHECK:     [[RETHROW]]([[ERROR:%.*]] : $ErrorType):
// CHECK-NEXT:  strong_release %0 : $Cat
// CHECK-NEXT:  throw [[ERROR]]

// rdar://20861374
// Clear out the self box before delegating.
class BaseThrowingInit : HasThrowingInit {
  var subField: Int
  init(value: Int, subField: Int) throws {
    self.subField = subField
    try super.init(value: value)
  }
}
// CHECK: sil hidden @_TFC6errors16BaseThrowingInitc{{.*}} : $@convention(method) (Int, Int, @owned BaseThrowingInit) -> (@owned BaseThrowingInit, @error ErrorType)
// CHECK:      [[BOX:%.*]] = alloc_box $BaseThrowingInit
// CHECK:      [[MARKED_BOX:%.*]] = mark_uninitialized [derivedself] [[BOX]]#1
//   Initialize subField.
// CHECK:      [[T0:%.*]] = load [[MARKED_BOX]]
// CHECK-NEXT: [[T1:%.*]] = ref_element_addr [[T0]] : $BaseThrowingInit, #BaseThrowingInit.subField
// CHECK-NEXT: assign %1 to [[T1]]
//   Super delegation.
// CHECK-NEXT: [[T0:%.*]] = load [[MARKED_BOX]]
// CHECK-NEXT: [[T2:%.*]] = upcast [[T0]] : $BaseThrowingInit to $HasThrowingInit
// CHECK-NEXT: function_ref
// CHECK-NEXT: [[T3:%.*]] = function_ref @_TFC6errors15HasThrowingInitc
// CHECK-NEXT: apply [[T3]](%0, [[T2]])

// Cleanups for writebacks.

protocol Supportable {
  mutating func support() throws
}
protocol Buildable {
  typealias Structure : Supportable
  var firstStructure: Structure { get set }
  subscript(name: String) -> Structure { get set }
}
func supportFirstStructure<B: Buildable>(inout b: B) throws {
  try b.firstStructure.support()
}
// CHECK-LABEL: sil hidden @_TF6errors21supportFirstStructure{{.*}} : $@convention(thin) <B where B : Buildable, B.Structure : Supportable> (@inout B) -> @error ErrorType {
// CHECK: [[SUPPORT:%.*]] = witness_method $B.Structure, #Supportable.support!1 :
// CHECK: [[MATBUFFER:%.*]] = alloc_stack $Builtin.UnsafeValueBuffer
// CHECK: [[BUFFER:%.*]] = alloc_stack $B.Structure
// CHECK: [[BUFFER_CAST:%.*]] = address_to_pointer [[BUFFER]]#1 : $*B.Structure to $Builtin.RawPointer
// CHECK: [[MAT:%.*]] = witness_method $B, #Buildable.firstStructure!materializeForSet.1 :
// CHECK: [[T1:%.*]] = apply [[MAT]]<B, B.Structure>([[BUFFER_CAST]], [[MATBUFFER]]#1, [[BASE:%.*#1]])
// CHECK: [[T2:%.*]] = tuple_extract [[T1]] : {{.*}}, 0
// CHECK: [[T3:%.*]] = pointer_to_address [[T2]] : $Builtin.RawPointer to $*B.Structure
// CHECK: [[CALLBACK:%.*]] = tuple_extract [[T1]] : {{.*}}, 1
// CHECK: [[T4:%.*]] = mark_dependence [[T3]] : $*B.Structure on [[BASE]] : $*B
// CHECK: try_apply [[SUPPORT]]<B.Structure>([[T4]]) : {{.*}}, normal [[BB_NORMAL:bb[0-9]+]], error [[BB_ERROR:bb[0-9]+]]

// CHECK: [[BB_NORMAL]]
// CHECK: switch_enum [[CALLBACK]]
// CHECK: apply
// CHECK: dealloc_stack [[BUFFER]]
// CHECK: dealloc_stack [[MATBUFFER]]
// CHECK: return

// CHECK: [[BB_ERROR]]([[ERROR:%.*]] : $ErrorType):
// CHECK: switch_enum [[CALLBACK]]
// CHECK: apply
// CHECK: dealloc_stack [[BUFFER]]
// CHECK: dealloc_stack [[MATBUFFER]]
// CHECK: throw [[ERROR]]

func supportStructure<B: Buildable>(inout b: B, name: String) throws {
  try b[name].support()
}
// CHECK-LABEL: sil hidden @_TF6errors16supportStructure
// CHECK: [[SUPPORT:%.*]] = witness_method $B.Structure, #Supportable.support!1 :
// CHECK: retain_value [[INDEX:%1]] : $String
// CHECK: [[MATBUFFER:%.*]] = alloc_stack $Builtin.UnsafeValueBuffer
// CHECK: [[BUFFER:%.*]] = alloc_stack $B.Structure
// CHECK: [[BUFFER_CAST:%.*]] = address_to_pointer [[BUFFER]]#1 : $*B.Structure to $Builtin.RawPointer
// CHECK: [[MAT:%.*]] = witness_method $B, #Buildable.subscript!materializeForSet.1 :
// CHECK: [[T1:%.*]] = apply [[MAT]]<B, B.Structure>([[BUFFER_CAST]], [[MATBUFFER]]#1, [[INDEX]], [[BASE:%.*#1]])
// CHECK: [[T2:%.*]] = tuple_extract [[T1]] : {{.*}}, 0
// CHECK: [[T3:%.*]] = pointer_to_address [[T2]] : $Builtin.RawPointer to $*B.Structure
// CHECK: [[CALLBACK:%.*]] = tuple_extract [[T1]] : {{.*}}, 1
// CHECK: [[T4:%.*]] = mark_dependence [[T3]] : $*B.Structure on [[BASE]] : $*B
// CHECK: try_apply [[SUPPORT]]<B.Structure>([[T4]]) : {{.*}}, normal [[BB_NORMAL:bb[0-9]+]], error [[BB_ERROR:bb[0-9]+]]

// CHECK: [[BB_NORMAL]]
// CHECK: switch_enum [[CALLBACK]]
// CHECK: apply
// CHECK: dealloc_stack [[BUFFER]]
// CHECK: dealloc_stack [[MATBUFFER]]
// CHECK: release_value [[INDEX]] : $String
// CHECK: return

// CHECK: [[BB_ERROR]]([[ERROR:%.*]] : $ErrorType):
// CHECK: switch_enum [[CALLBACK]]
// CHECK: apply
// CHECK: dealloc_stack [[BUFFER]]
// CHECK: dealloc_stack [[MATBUFFER]]
// CHECK: release_value [[INDEX]] : $String
// CHECK: throw [[ERROR]]

struct Pylon {
  var name: String
  mutating func support() throws {}
}
struct Bridge {
  var mainPylon : Pylon
  subscript(name: String) -> Pylon {
    get {
      return mainPylon
    }
    set {}
  }
}
func supportStructure(inout b: Bridge, name: String) throws {
  try b[name].support()
}
// CHECK:    sil hidden @_TF6errors16supportStructureFzTRVS_6Bridge4nameSS_T_ :
// CHECK:      [[SUPPORT:%.*]] = function_ref @_TFV6errors5Pylon7support
// CHECK:      retain_value [[INDEX:%1]] : $String
// CHECK-NEXT: retain_value [[INDEX]] : $String
// CHECK-NEXT: [[TEMP:%.*]] = alloc_stack $Pylon
// CHECK-NEXT: [[BASE:%.*]] = load [[B:%2#1]] : $*Bridge
// CHECK-NEXT: retain_value [[BASE]]
// CHECK-NEXT: function_ref
// CHECK-NEXT: [[GETTER:%.*]] = function_ref @_TFV6errors6Bridgeg9subscriptFSSVS_5Pylon :
// CHECK-NEXT: [[T0:%.*]] = apply [[GETTER]]([[INDEX]], [[BASE]])
// CHECK-NEXT: release_value [[BASE]]
// CHECK-NEXT: store [[T0]] to [[TEMP]]#1
// CHECK-NEXT: try_apply [[SUPPORT]]([[TEMP]]#1) : {{.*}}, normal [[BB_NORMAL:bb[0-9]+]], error [[BB_ERROR:bb[0-9]+]]

// CHECK:    [[BB_NORMAL]]
// CHECK-NEXT: [[T0:%.*]] = load [[TEMP]]#1
// CHECK-NEXT: function_ref
// CHECK-NEXT: [[SETTER:%.*]] = function_ref @_TFV6errors6Bridges9subscriptFSSVS_5Pylon :
// CHECK-NEXT: apply [[SETTER]]([[T0]], [[INDEX]], [[B]])
// CHECK-NEXT: dealloc_stack [[TEMP]]#0
// CHECK-NEXT: release_value [[INDEX]] : $String
// CHECK-NEXT: copy_addr
// CHECK-NEXT: strong_release
// CHECK-NEXT: tuple ()
// CHECK-NEXT: return

//   We end up with ugly redundancy here because we don't want to
//   consume things during cleanup emission.  It's questionable.
// CHECK:    [[BB_ERROR]]([[ERROR:%.*]] : $ErrorType):
// CHECK-NEXT: [[T0:%.*]] = load [[TEMP]]#1
// CHECK-NEXT: retain_value [[T0]]
// CHECK-NEXT: retain_value [[INDEX]] : $String
// CHECK-NEXT: function_ref
// CHECK-NEXT: [[SETTER:%.*]] = function_ref @_TFV6errors6Bridges9subscriptFSSVS_5Pylon :
// CHECK-NEXT: apply [[SETTER]]([[T0]], [[INDEX]], [[B]])
// CHECK-NEXT: destroy_addr [[TEMP]]#1
// CHECK-NEXT: dealloc_stack [[TEMP]]#0
// CHECK-NEXT: release_value [[INDEX]] : $String
// CHECK-NEXT: release_value [[INDEX]] : $String
// CHECK-NEXT: copy_addr
// CHECK-NEXT: strong_release
// CHECK-NEXT: throw [[ERROR]]

struct OwnedBridge {
  var owner : Builtin.UnknownObject
  subscript(name: String) -> Pylon {
    addressWithOwner { return (nil, owner) }
    mutableAddressWithOwner { return (nil, owner) }
  }
}
func supportStructure(inout b: OwnedBridge, name: String) throws {
  try b[name].support()
}
// CHECK: sil hidden @_TF6errors16supportStructureFzTRVS_11OwnedBridge4nameSS_T_ :
// CHECK:      [[SUPPORT:%.*]] = function_ref @_TFV6errors5Pylon7support
// CHECK:      retain_value [[INDEX:%1]] : $String
// CHECK-NEXT: function_ref
// CHECK-NEXT: [[ADDRESSOR:%.*]] = function_ref @_TFV6errors11OwnedBridgeaO9subscriptFSSVS_5Pylon :
// CHECK-NEXT: [[T0:%.*]] = apply [[ADDRESSOR]]([[INDEX]], [[BASE:%2#1]])
// CHECK-NEXT: [[T1:%.*]] = tuple_extract [[T0]] : {{.*}}, 0
// CHECK-NEXT: [[OWNER:%.*]] = tuple_extract [[T0]] : {{.*}}, 1
// CHECK-NEXT: [[T3:%.*]] = struct_extract [[T1]]
// CHECK-NEXT: [[T4:%.*]] = pointer_to_address [[T3]]
// CHECK-NEXT: [[T5:%.*]] = mark_dependence [[T4]] : $*Pylon on [[OWNER]]
// CHECK-NEXT: try_apply [[SUPPORT]]([[T5]]) : {{.*}}, normal [[BB_NORMAL:bb[0-9]+]], error [[BB_ERROR:bb[0-9]+]]
// CHECK:    [[BB_NORMAL]]
// CHECK-NEXT: strong_release [[OWNER]] : $Builtin.UnknownObject
// CHECK-NEXT: release_value [[INDEX]] : $String
// CHECK-NEXT: copy_addr
// CHECK-NEXT: strong_release
// CHECK-NEXT: tuple ()
// CHECK-NEXT: return
// CHECK:    [[BB_ERROR]]([[ERROR:%.*]] : $ErrorType):
// CHECK-NEXT: strong_release [[OWNER]] : $Builtin.UnknownObject
// CHECK-NEXT: release_value [[INDEX]] : $String
// CHECK-NEXT: copy_addr
// CHECK-NEXT: strong_release
// CHECK-NEXT: throw [[ERROR]]

struct PinnedBridge {
  var owner : Builtin.NativeObject
  subscript(name: String) -> Pylon {
    addressWithPinnedNativeOwner { return (nil, owner) }
    mutableAddressWithPinnedNativeOwner { return (nil, owner) }
  }
}
func supportStructure(inout b: PinnedBridge, name: String) throws {
  try b[name].support()
}
// CHECK: sil hidden @_TF6errors16supportStructureFzTRVS_12PinnedBridge4nameSS_T_ :
// CHECK:      [[SUPPORT:%.*]] = function_ref @_TFV6errors5Pylon7support
// CHECK:      retain_value [[INDEX:%1]] : $String
// CHECK-NEXT: function_ref
// CHECK-NEXT: [[ADDRESSOR:%.*]] = function_ref @_TFV6errors12PinnedBridgeap9subscriptFSSVS_5Pylon :
// CHECK-NEXT: [[T0:%.*]] = apply [[ADDRESSOR]]([[INDEX]], [[BASE:%2#1]])
// CHECK-NEXT: [[T1:%.*]] = tuple_extract [[T0]] : {{.*}}, 0
// CHECK-NEXT: [[OWNER:%.*]] = tuple_extract [[T0]] : {{.*}}, 1
// CHECK-NEXT: [[T3:%.*]] = struct_extract [[T1]]
// CHECK-NEXT: [[T4:%.*]] = pointer_to_address [[T3]]
// CHECK-NEXT: [[T5:%.*]] = mark_dependence [[T4]] : $*Pylon on [[OWNER]]
// CHECK-NEXT: try_apply [[SUPPORT]]([[T5]]) : {{.*}}, normal [[BB_NORMAL:bb[0-9]+]], error [[BB_ERROR:bb[0-9]+]]
// CHECK:    [[BB_NORMAL]]
// CHECK-NEXT: strong_unpin [[OWNER]] : $Optional<Builtin.NativeObject>
// CHECK-NEXT: release_value [[INDEX]] : $String
// CHECK-NEXT: copy_addr
// CHECK-NEXT: strong_release
// CHECK-NEXT: tuple ()
// CHECK-NEXT: return
// CHECK:    [[BB_ERROR]]([[ERROR:%.*]] : $ErrorType):
// CHECK-NEXT: retain_value [[OWNER]]
// CHECK-NEXT: strong_unpin [[OWNER]] : $Optional<Builtin.NativeObject>
// CHECK-NEXT: release_value [[OWNER]]
// CHECK-NEXT: release_value [[INDEX]] : $String
// CHECK-NEXT: copy_addr
// CHECK-NEXT: strong_release
// CHECK-NEXT: throw [[ERROR]]

// ! peepholes its argument with getSemanticsProvidingExpr().
// Test that that doesn't look through try!.
// rdar://21515402
func testForcePeephole(f: () throws -> Int?) -> Int {
  let x = (try! f())!
  return x
}

// CHECK-LABEL: sil hidden @_TF6errors15testOptionalTryFT_T_
// CHECK-NEXT: bb0:
// CHECK: [[FN:%.+]] = function_ref @_TF6errors10make_a_catFzT_CS_3Cat
// CHECK-NEXT: try_apply [[FN]]() : $@convention(thin) () -> (@owned Cat, @error ErrorType), normal [[SUCCESS:[^ ]+]], error [[CLEANUPS:[^ ]+]]
// CHECK: [[SUCCESS]]([[VALUE:%.+]] : $Cat)
// CHECK-NEXT: [[WRAPPED:%.+]] = enum $Optional<Cat>, #Optional.Some!enumelt.1, [[VALUE]]
// CHECK-NEXT: br [[DONE:[^ ]+]]([[WRAPPED]] : $Optional<Cat>)
// CHECK: [[DONE]]([[RESULT:%.+]] : $Optional<Cat>):
// CHECK-NEXT: release_value [[RESULT]] : $Optional<Cat>
// CHECK-NEXT: [[VOID:%.+]] = tuple ()
// CHECK-NEXT: return [[VOID]] : $()
// CHECK: [[FAILURE:.+]]({{%.+}} : $ErrorType):
// CHECK-NEXT: [[NONE:%.+]] = enum $Optional<Cat>, #Optional.None!enumelt
// CHECK-NEXT: br [[DONE]]([[NONE]] : $Optional<Cat>)
// CHECK: [[CLEANUPS]]([[ERROR:%.+]] : $ErrorType):
// CHECK-NEXT: br [[FAILURE]]([[ERROR]] : $ErrorType)
// CHECK: {{^}$}}
func testOptionalTry() {
  _ = try? make_a_cat()
}

// CHECK-LABEL: sil hidden @_TF6errors18testOptionalTryVarFT_T_
// CHECK-NEXT: bb0:
// CHECK-NEXT: [[BOX:%.+]] = alloc_box $Optional<Cat>
// CHECK-NEXT: [[BOX_DATA:%.+]] = init_enum_data_addr [[BOX]]#1 : $*Optional<Cat>, #Optional.Some!enumelt.1
// CHECK: [[FN:%.+]] = function_ref @_TF6errors10make_a_catFzT_CS_3Cat
// CHECK-NEXT: try_apply [[FN]]() : $@convention(thin) () -> (@owned Cat, @error ErrorType), normal [[SUCCESS:[^ ]+]], error [[CLEANUPS:[^ ]+]]
// CHECK: [[SUCCESS]]([[VALUE:%.+]] : $Cat)
// CHECK-NEXT: store [[VALUE]] to [[BOX_DATA]] : $*Cat
// CHECK-NEXT: inject_enum_addr [[BOX]]#1 : $*Optional<Cat>, #Optional.Some!enumelt.1
// CHECK-NEXT: br [[DONE:[^ ]+]]
// CHECK: [[DONE]]:
// CHECK-NEXT: strong_release [[BOX]]#0 : $@box Optional<Cat>
// CHECK-NEXT: [[VOID:%.+]] = tuple ()
// CHECK-NEXT: return [[VOID]] : $()
// CHECK: [[FAILURE:.+]]({{%.+}} : $ErrorType):
// CHECK-NEXT: inject_enum_addr [[BOX]]#1 : $*Optional<Cat>, #Optional.None!enumelt
// CHECK-NEXT: br [[DONE]]
// CHECK: [[CLEANUPS]]([[ERROR:%.+]] : $ErrorType):
// CHECK-NEXT: br [[FAILURE]]([[ERROR]] : $ErrorType)
// CHECK: {{^}$}}
func testOptionalTryVar() {
  var cat = try? make_a_cat() // expected-warning {{initialization of variable 'cat' was never used; consider replacing with assignment to '_' or removing it}}
}

// CHECK-LABEL: sil hidden @_TF6errors26testOptionalTryAddressOnly
// CHECK: bb0(%0 : $*T):
// CHECK: [[BOX:%.+]] = alloc_stack $Optional<T>
// CHECK-NEXT: [[BOX_DATA:%.+]] = init_enum_data_addr [[BOX]]#1 : $*Optional<T>, #Optional.Some!enumelt.1
// CHECK: [[FN:%.+]] = function_ref @_TF6errors11dont_return
// CHECK-NEXT: [[ARG_BOX:%.+]] = alloc_stack $T
// CHECK-NEXT: copy_addr %0 to [initialization] [[ARG_BOX]]#1 : $*T
// CHECK-NEXT: try_apply [[FN]]<T>([[BOX_DATA]], [[ARG_BOX]]#1) : $@convention(thin) <τ_0_0> (@out τ_0_0, @in τ_0_0) -> @error ErrorType, normal [[SUCCESS:[^ ]+]], error [[CLEANUPS:[^ ]+]]
// CHECK: [[SUCCESS]]({{%.+}} : $()):
// CHECK-NEXT: inject_enum_addr [[BOX]]#1 : $*Optional<T>, #Optional.Some!enumelt.1
// CHECK-NEXT: dealloc_stack [[ARG_BOX]]#0 : $*@local_storage T
// CHECK-NEXT: br [[DONE:[^ ]+]]
// CHECK: [[DONE]]:
// CHECK-NEXT: destroy_addr [[BOX]]#1 : $*Optional<T>
// CHECK-NEXT: dealloc_stack [[BOX]]#0 : $*@local_storage Optional<T>
// CHECK-NEXT: destroy_addr %0 : $*T
// CHECK-NEXT: [[VOID:%.+]] = tuple ()
// CHECK-NEXT: return [[VOID]] : $()
// CHECK: [[FAILURE:.+]]({{%.+}} : $ErrorType):
// CHECK-NEXT: inject_enum_addr [[BOX]]#1 : $*Optional<T>, #Optional.None!enumelt
// CHECK-NEXT: br [[DONE]]
// CHECK: [[CLEANUPS]]([[ERROR:%.+]] : $ErrorType):
// CHECK-NEXT: dealloc_stack [[ARG_BOX]]#0 : $*@local_storage T
// CHECK-NEXT: br [[FAILURE]]([[ERROR]] : $ErrorType)
// CHECK: {{^}$}}
func testOptionalTryAddressOnly<T>(obj: T) {
  _ = try? dont_return(obj)
}

// CHECK-LABEL: sil hidden @_TF6errors29testOptionalTryAddressOnlyVar
// CHECK: bb0(%0 : $*T):
// CHECK: [[BOX:%.+]] = alloc_box $Optional<T>
// CHECK-NEXT: [[BOX_DATA:%.+]] = init_enum_data_addr [[BOX]]#1 : $*Optional<T>, #Optional.Some!enumelt.1
// CHECK: [[FN:%.+]] = function_ref @_TF6errors11dont_return
// CHECK-NEXT: [[ARG_BOX:%.+]] = alloc_stack $T
// CHECK-NEXT: copy_addr %0 to [initialization] [[ARG_BOX]]#1 : $*T
// CHECK-NEXT: try_apply [[FN]]<T>([[BOX_DATA]], [[ARG_BOX]]#1) : $@convention(thin) <τ_0_0> (@out τ_0_0, @in τ_0_0) -> @error ErrorType, normal [[SUCCESS:[^ ]+]], error [[CLEANUPS:[^ ]+]]
// CHECK: [[SUCCESS]]({{%.+}} : $()):
// CHECK-NEXT: inject_enum_addr [[BOX]]#1 : $*Optional<T>, #Optional.Some!enumelt.1
// CHECK-NEXT: dealloc_stack [[ARG_BOX]]#0 : $*@local_storage T
// CHECK-NEXT: br [[DONE:[^ ]+]]
// CHECK: [[DONE]]:
// CHECK-NEXT: strong_release [[BOX]]#0 : $@box Optional<T>
// CHECK-NEXT: destroy_addr %0 : $*T
// CHECK-NEXT: [[VOID:%.+]] = tuple ()
// CHECK-NEXT: return [[VOID]] : $()
// CHECK: [[FAILURE:.+]]({{%.+}} : $ErrorType):
// CHECK-NEXT: inject_enum_addr [[BOX]]#1 : $*Optional<T>, #Optional.None!enumelt
// CHECK-NEXT: br [[DONE]]
// CHECK: [[CLEANUPS]]([[ERROR:%.+]] : $ErrorType):
// CHECK-NEXT: dealloc_stack [[ARG_BOX]]#0 : $*@local_storage T
// CHECK-NEXT: br [[FAILURE]]([[ERROR]] : $ErrorType)
// CHECK: {{^}$}}
func testOptionalTryAddressOnlyVar<T>(obj: T) {
  var copy = try? dont_return(obj) // expected-warning {{initialization of variable 'copy' was never used; consider replacing with assignment to '_' or removing it}}
}

// CHECK-LABEL: sil hidden @_TF6errors23testOptionalTryMultipleFT_T_
// CHECK: bb0:
// CHECK: [[FN_1:%.+]] = function_ref @_TF6errors10make_a_catFzT_CS_3Cat
// CHECK-NEXT: try_apply [[FN_1]]() : $@convention(thin) () -> (@owned Cat, @error ErrorType), normal [[SUCCESS_1:[^ ]+]], error [[CLEANUPS_1:[^ ]+]]
// CHECK: [[SUCCESS_1]]([[VALUE_1:%.+]] : $Cat)
// CHECK: [[FN_2:%.+]] = function_ref @_TF6errors10make_a_catFzT_CS_3Cat
// CHECK-NEXT: try_apply [[FN_2]]() : $@convention(thin) () -> (@owned Cat, @error ErrorType), normal [[SUCCESS_2:[^ ]+]], error [[CLEANUPS_2:[^ ]+]]
// CHECK: [[SUCCESS_2]]([[VALUE_2:%.+]] : $Cat)
// CHECK-NEXT: [[TUPLE:%.+]] = tuple ([[VALUE_1]] : $Cat, [[VALUE_2]] : $Cat)
// CHECK-NEXT: [[WRAPPED:%.+]] = enum $Optional<(Cat, Cat)>, #Optional.Some!enumelt.1, [[TUPLE]]
// CHECK-NEXT: br [[DONE:[^ ]+]]([[WRAPPED]] : $Optional<(Cat, Cat)>)
// CHECK: [[DONE]]([[RESULT:%.+]] : $Optional<(Cat, Cat)>):
// CHECK-NEXT: release_value [[RESULT]] : $Optional<(Cat, Cat)>
// CHECK-NEXT: [[VOID:%.+]] = tuple ()
// CHECK-NEXT: return [[VOID]] : $()
// CHECK: [[FAILURE:.+]]({{%.+}} : $ErrorType):
// CHECK-NEXT: [[NONE:%.+]] = enum $Optional<(Cat, Cat)>, #Optional.None!enumelt
// CHECK-NEXT: br [[DONE]]([[NONE]] : $Optional<(Cat, Cat)>)
// CHECK: [[CLEANUPS_1]]([[ERROR:%.+]] : $ErrorType):
// CHECK-NEXT: br [[FAILURE]]([[ERROR]] : $ErrorType)
// CHECK: [[CLEANUPS_2]]([[ERROR:%.+]] : $ErrorType):
// CHECK-NEXT: strong_release [[VALUE_1]] : $Cat
// CHECK-NEXT: br [[FAILURE]]([[ERROR]] : $ErrorType)
// CHECK: {{^}$}}
func testOptionalTryMultiple() {
  _ = try? (make_a_cat(), make_a_cat())
}

// CHECK-LABEL: sil hidden @_TF6errors25testOptionalTryNeverFailsFT_T_
// CHECK: bb0:
// CHECK-NEXT:   [[VALUE:%.+]] = tuple ()
// CHECK-NEXT:   = enum $Optional<()>, #Optional.Some!enumelt.1, [[VALUE]]
// CHECK-NEXT:   [[VOID:%.+]] = tuple ()
// CHECK-NEXT:   return [[VOID]] : $()
// CHECK: {{^}$}}
func testOptionalTryNeverFails() {
  _ = try? () // expected-warning {{no calls to throwing functions occur within 'try' expression}}
}

// CHECK-LABEL: sil hidden @_TF6errors28testOptionalTryNeverFailsVarFT_T_
// CHECK: bb0:
// CHECK-NEXT:   [[BOX:%.+]] = alloc_box $Optional<()>
// CHECK-NEXT:   = init_enum_data_addr [[BOX]]#1 : $*Optional<()>, #Optional.Some!enumelt.1
// CHECK-NEXT:   inject_enum_addr [[BOX]]#1 : $*Optional<()>, #Optional.Some!enumelt.1
// CHECK-NEXT:   strong_release [[BOX]]#0 : $@box Optional<()>
// CHECK-NEXT:   [[VOID:%.+]] = tuple ()
// CHECK-NEXT:   return [[VOID]] : $()
// CHECK-NEXT: {{^}$}}
func testOptionalTryNeverFailsVar() {
  var unit: ()? = try? () // expected-warning {{no calls to throwing functions occur within 'try' expression}} expected-warning {{initialization of variable 'unit' was never used; consider replacing with assignment to '_' or removing it}}
}

// CHECK-LABEL: sil hidden @_TF6errors36testOptionalTryNeverFailsAddressOnly
// CHECK: bb0(%0 : $*T):
// CHECK:   [[BOX:%.+]] = alloc_stack $Optional<T>
// CHECK-NEXT:   [[BOX_DATA:%.+]] = init_enum_data_addr [[BOX]]#1 : $*Optional<T>, #Optional.Some!enumelt.1
// CHECK-NEXT:   copy_addr %0 to [initialization] [[BOX_DATA]] : $*T
// CHECK-NEXT:   inject_enum_addr [[BOX]]#1 : $*Optional<T>, #Optional.Some!enumelt.1
// CHECK-NEXT:   destroy_addr [[BOX]]#1 : $*Optional<T>
// CHECK-NEXT:   dealloc_stack [[BOX]]#0 : $*@local_storage Optional<T>
// CHECK-NEXT:   destroy_addr %0 : $*T
// CHECK-NEXT:   [[VOID:%.+]] = tuple ()
// CHECK-NEXT:   return [[VOID]] : $()
// CHECK-NEXT: {{^}$}}
func testOptionalTryNeverFailsAddressOnly<T>(obj: T) {
  _ = try? obj // expected-warning {{no calls to throwing functions occur within 'try' expression}}
}

// CHECK-LABEL: sil hidden @_TF6errors39testOptionalTryNeverFailsAddressOnlyVar
// CHECK: bb0(%0 : $*T):
// CHECK:   [[BOX:%.+]] = alloc_box $Optional<T>
// CHECK-NEXT:   [[BOX_DATA:%.+]] = init_enum_data_addr [[BOX]]#1 : $*Optional<T>, #Optional.Some!enumelt.1
// CHECK-NEXT:   copy_addr %0 to [initialization] [[BOX_DATA]] : $*T
// CHECK-NEXT:   inject_enum_addr [[BOX]]#1 : $*Optional<T>, #Optional.Some!enumelt.1
// CHECK-NEXT:   strong_release [[BOX]]#0 : $@box Optional<T>
// CHECK-NEXT:   destroy_addr %0 : $*T
// CHECK-NEXT:   [[VOID:%.+]] = tuple ()
// CHECK-NEXT:   return [[VOID]] : $()
// CHECK-NEXT: {{^}$}}
func testOptionalTryNeverFailsAddressOnlyVar<T>(obj: T) {
  var copy = try? obj // expected-warning {{no calls to throwing functions occur within 'try' expression}} expected-warning {{initialization of variable 'copy' was never used; consider replacing with assignment to '_' or removing it}}
}
