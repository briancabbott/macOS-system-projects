// RUN: %target-swift-frontend -emit-silgen %s | FileCheck %s

indirect enum TreeA<T> {
  // CHECK-LABEL: sil hidden [transparent] @_TFO13indirect_enum5TreeA3Nil{{.*}} : $@convention(thin) <T> (@thin TreeA<T>.Type) -> @owned TreeA<T> {
  // CHECK:         enum $TreeA<T>, #TreeA.Nil!enumelt
  case Nil


  // CHECK-LABEL: sil hidden [transparent] @_TFO13indirect_enum5TreeA4Leaf{{.*}} : $@convention(thin) <T> (@in T, @thin TreeA<T>.Type) -> @owned TreeA<T> {
  // CHECK:         [[BOX:%.*]] = alloc_box $T
  // CHECK:         copy_addr [take] %0 to [initialization] [[BOX]]#1 : $*T
  // CHECK:         enum $TreeA<T>, #TreeA.Leaf!enumelt.1, [[BOX]]#0 : $@box T
  case Leaf(T)

  // CHECK-LABEL: sil hidden [transparent] @_TFO13indirect_enum5TreeA6Branch{{.*}}
  // CHECK:         [[TUPLE:%.*]] = tuple $(left: TreeA<T>, right: TreeA<T>) (%0, %1)
  // CHECK:         [[BOX:%.*]] = alloc_box $(left: TreeA<T>, right: TreeA<T>)
  // CHECK:         store [[TUPLE]] to [[BOX]]#1 : $*(left: TreeA<T>, right: TreeA<T>)
  // CHECK:         enum $TreeA<T>, #TreeA.Branch!enumelt.1, [[BOX]]#0 : $@box (left: TreeA<T>, right: TreeA<T>)
  case Branch(left: TreeA<T>, right: TreeA<T>)
}

enum TreeB<T> {
  // CHECK-LABEL: sil hidden [transparent] @_TFO13indirect_enum5TreeB3Nil{{.*}} : $@convention(thin) <T> (@out TreeB<T>, @thin TreeB<T>.Type) -> () {
  // CHECK:         inject_enum_addr %0 : $*TreeB<T>, #TreeB.Nil!enumelt
  case Nil

  // CHECK-LABEL: sil hidden [transparent] @_TFO13indirect_enum5TreeB4Leaf{{.*}} : $@convention(thin) <T> (@out TreeB<T>, @in T, @thin TreeB<T>.Type) -> () {
  // CHECK:         [[ADDR:%.*]] = init_enum_data_addr %0 : $*TreeB<T>, #TreeB.Leaf!enumelt.1
  // CHECK:         copy_addr [take] %1 to [initialization] [[ADDR]] : $*T
  // CHECK:         inject_enum_addr %0 : $*TreeB<T>, #TreeB.Leaf!enumelt.1
  case Leaf(T)

  // CHECK-LABEL: sil hidden [transparent] @_TFO13indirect_enum5TreeB6Branch{{.*}} : $@convention(thin) <T> (@out TreeB<T>, @in TreeB<T>, @in TreeB<T>, @thin TreeB<T>.Type) -> ()
  // CHECK:         [[TUPLE:%.*]] = alloc_stack $(left: TreeB<T>, right: TreeB<T>
  // CHECK:         [[BOX:%.*]] = alloc_box $(left: TreeB<T>, right: TreeB<T>)
  // CHECK:         copy_addr [take] [[TUPLE]]#1 to [initialization] [[BOX]]#1 : $*(left: TreeB<T>, right: TreeB<T>)
  // CHECK:         [[ADDR:%.*]] = init_enum_data_addr %0 : $*TreeB<T>, #TreeB.Branch!enumelt.1
  // CHECK:         store [[BOX]]#0 to [[ADDR]] : $*@box (left: TreeB<T>, right: TreeB<T>)
  // CHECK:         inject_enum_addr %0 : $*TreeB<T>, #TreeB.Branch!enumelt.1
  indirect case Branch(left: TreeB<T>, right: TreeB<T>)
}

enum TreeInt {
  // CHECK-LABEL: sil hidden [transparent] @_TFO13indirect_enum7TreeInt3Nil{{.*}} : $@convention(thin) (@thin TreeInt.Type) -> @owned TreeInt {
  // CHECK:         enum $TreeInt, #TreeInt.Nil!enumelt
  case Nil

  // CHECK-LABEL: sil hidden [transparent] @_TFO13indirect_enum7TreeInt4Leaf{{.*}} : $@convention(thin) (Int, @thin TreeInt.Type) -> @owned TreeInt
  // CHECK:         enum $TreeInt, #TreeInt.Leaf!enumelt.1, %0 : $Int
  case Leaf(Int)

  // CHECK-LABEL: sil hidden [transparent] @_TFO13indirect_enum7TreeInt6Branch{{.*}} : $@convention(thin) (@owned TreeInt, @owned TreeInt, @thin TreeInt.Type) -> @owned TreeInt 
  // CHECK:         [[TUPLE:%.*]] = tuple $(left: TreeInt, right: TreeInt) (%0, %1)
  // CHECK:         [[BOX:%.*]] = alloc_box $(left: TreeInt, right: TreeInt)
  // CHECK:         store [[TUPLE]] to [[BOX]]#1 : $*(left: TreeInt, right: TreeInt)
  // CHECK:         enum $TreeInt, #TreeInt.Branch!enumelt.1, [[BOX]]#0 : $@box (left: TreeInt, right: TreeInt)
  indirect case Branch(left: TreeInt, right: TreeInt)
}

enum TrivialButIndirect {
  case Direct(Int)
  indirect case Indirect(Int)
}

func a() {}
func b<T>(x: T) {}
func c<T>(x: T, _ y: T) {}
func d() {}

// CHECK-LABEL: sil hidden @_TF13indirect_enum11switchTreeA
func switchTreeA<T>(x: TreeA<T>) {
  // --           x +2
  // CHECK:       retain_value %0
  // CHECK:       switch_enum %0 : $TreeA<T>
  switch x {
  // CHECK:     bb{{.*}}:
  // CHECK:       function_ref @_TF13indirect_enum1aFT_T_
  case .Nil:
    a()
  // CHECK:     bb{{.*}}([[LEAF_BOX:%.*]] : $@box T):
  // CHECK:       [[VALUE:%.*]] = project_box [[LEAF_BOX]]
  // CHECK:       copy_addr [[VALUE]] to [initialization] [[X:%.*]]#1 : $*T
  // CHECK:       function_ref @_TF13indirect_enum1b
  // CHECK:       destroy_addr [[X]]#1
  // CHECK:       dealloc_stack [[X]]#0
  // --           x +1
  // CHECK:       strong_release [[LEAF_BOX]]
  // CHECK:       br [[OUTER_CONT:bb[0-9]+]]
  case .Leaf(let x):
    b(x)

  // CHECK:     bb{{.*}}([[NODE_BOX:%.*]] : $@box (left: TreeA<T>, right: TreeA<T>)):
  // CHECK:       [[TUPLE_ADDR:%.*]] = project_box [[NODE_BOX]]
  // CHECK:       [[TUPLE:%.*]] = load [[TUPLE_ADDR]]
  // CHECK:       [[LEFT:%.*]] = tuple_extract [[TUPLE]] {{.*}}, 0
  // CHECK:       [[RIGHT:%.*]] = tuple_extract [[TUPLE]] {{.*}}, 1
  // CHECK:       switch_enum [[RIGHT]] {{.*}}, default [[FAIL_RIGHT:bb[0-9]+]]

  // CHECK:     bb{{.*}}([[RIGHT_LEAF_BOX:%.*]] : $@box T):
  // CHECK:       [[RIGHT_LEAF_VALUE:%.*]] = project_box [[RIGHT_LEAF_BOX]]
  // CHECK:       switch_enum [[LEFT]] {{.*}}, default [[FAIL_LEFT:bb[0-9]+]]
  
  // CHECK:     bb{{.*}}([[LEFT_LEAF_BOX:%.*]] : $@box T):
  // CHECK:       [[LEFT_LEAF_VALUE:%.*]] = project_box [[LEFT_LEAF_BOX]]
  // CHECK:       copy_addr [[LEFT_LEAF_VALUE]]
  // CHECK:       copy_addr [[RIGHT_LEAF_VALUE]]
  // --           x +1
  // CHECK:       strong_release [[NODE_BOX]]
  // CHECK:       br [[OUTER_CONT]]

  // CHECK:     [[FAIL_LEFT]]:
  // CHECK:       br [[DEFAULT:bb[0-9]+]]

  // CHECK:     [[FAIL_RIGHT]]:
  // CHECK:       br [[DEFAULT]]

  case .Branch(.Leaf(let x), .Leaf(let y)):
    c(x, y)

  // CHECK:     [[DEFAULT]]:
  // --           x +1
  // CHECK:       release_value %0
  default:
    d()
  }

  // CHECK:     [[OUTER_CONT:%.*]]:
  // --           x +0
  // CHECK:       release_value %0 : $TreeA<T>
}

// CHECK-LABEL: sil hidden @_TF13indirect_enum11switchTreeB
func switchTreeB<T>(x: TreeB<T>) {
  // CHECK:       copy_addr %0 to [initialization] [[SCRATCH:%.*]]#1
  // CHECK:       switch_enum_addr [[SCRATCH]]#1
  switch x {

  // CHECK:     bb{{.*}}:
  // CHECK:       destroy_addr [[SCRATCH]]
  // CHECK:       dealloc_stack [[SCRATCH]]
  // CHECK:       function_ref @_TF13indirect_enum1aFT_T_
  // CHECK:       br [[OUTER_CONT:bb[0-9]+]]
  case .Nil:
    a()

  // CHECK:     bb{{.*}}:
  // CHECK:       copy_addr [[SCRATCH]]#1 to [initialization] [[LEAF_COPY:%.*]]#1
  // CHECK:       [[LEAF_ADDR:%.*]] = unchecked_take_enum_data_addr [[LEAF_COPY]]
  // CHECK:       copy_addr [take] [[LEAF_ADDR]] to [initialization] [[LEAF:%.*]]#1
  // CHECK:       function_ref @_TF13indirect_enum1b
  // CHECK:       destroy_addr [[LEAF]]
  // CHECK:       dealloc_stack [[LEAF]]
  // CHECK-NOT:   destroy_addr [[LEAF_COPY]]
  // CHECK:       dealloc_stack [[LEAF_COPY]]
  // CHECK:       destroy_addr [[SCRATCH]]
  // CHECK:       dealloc_stack [[SCRATCH]]
  // CHECK:       br [[OUTER_CONT]]
  case .Leaf(let x):
    b(x)

  // CHECK:     bb{{.*}}:
  // CHECK:       copy_addr [[SCRATCH]]#1 to [initialization] [[TREE_COPY:%.*]]#1
  // CHECK:       [[TREE_ADDR:%.*]] = unchecked_take_enum_data_addr [[TREE_COPY]]
  // --           box +1 immutable
  // CHECK:       [[BOX:%.*]] = load [[TREE_ADDR]]
  // CHECK:       [[TUPLE:%.*]] = project_box [[BOX]]
  // CHECK:       [[LEFT:%.*]] = tuple_element_addr [[TUPLE]]
  // CHECK:       [[RIGHT:%.*]] = tuple_element_addr [[TUPLE]]
  // CHECK:       switch_enum_addr [[RIGHT]] {{.*}}, default [[RIGHT_FAIL:bb[0-9]+]]

  // CHECK:     bb{{.*}}:
  // CHECK:       copy_addr [[RIGHT]] to [initialization] [[RIGHT_COPY:%.*]]#1
  // CHECK:       [[RIGHT_LEAF:%.*]] = unchecked_take_enum_data_addr [[RIGHT_COPY]]#1 : $*TreeB<T>, #TreeB.Leaf
  // CHECK:       switch_enum_addr [[LEFT]] {{.*}}, default [[LEFT_FAIL:bb[0-9]+]]

  // CHECK:     bb{{.*}}:
  // CHECK:       copy_addr [[LEFT]] to [initialization] [[LEFT_COPY:%.*]]#1
  // CHECK:       [[LEFT_LEAF:%.*]] = unchecked_take_enum_data_addr [[LEFT_COPY]]#1 : $*TreeB<T>, #TreeB.Leaf
  // CHECK:       copy_addr [take] [[LEFT_LEAF]] to [initialization] [[X:%.*]]#1
  // CHECK:       copy_addr [take] [[RIGHT_LEAF]] to [initialization] [[Y:%.*]]#1
  // CHECK:       function_ref @_TF13indirect_enum1c
  // CHECK:       destroy_addr [[Y]]
  // CHECK:       dealloc_stack [[Y]]
  // CHECK:       destroy_addr [[X]]
  // CHECK:       dealloc_stack [[X]]
  // CHECK-NOT:   destroy_addr [[LEFT_COPY]]
  // CHECK:       dealloc_stack [[LEFT_COPY]]
  // CHECK-NOT:   destroy_addr [[RIGHT_COPY]]
  // CHECK:       dealloc_stack [[RIGHT_COPY]]
  // --           box +0
  // CHECK:       strong_release [[BOX]]
  // CHECK-NOT:   destroy_addr [[TREE_COPY]]
  // CHECK:       dealloc_stack [[TREE_COPY]]
  // CHECK:       destroy_addr [[SCRATCH]]
  // CHECK:       dealloc_stack [[SCRATCH]]
  case .Branch(.Leaf(let x), .Leaf(let y)):
    c(x, y)

  // CHECK:     [[LEFT_FAIL]]:
  // CHECK:       destroy_addr [[RIGHT_LEAF]]
  // CHECK-NOT:   destroy_addr [[RIGHT_COPY]]
  // CHECK:       dealloc_stack [[RIGHT_COPY]]
  // CHECK:       strong_release [[BOX]]
  // CHECK-NOT:   destroy_addr [[TREE_COPY]]
  // CHECK:       dealloc_stack [[TREE_COPY]]
  // CHECK:       br [[INNER_CONT:bb[0-9]+]]

  // CHECK:     [[RIGHT_FAIL]]:
  // CHECK:       strong_release [[BOX]]
  // CHECK-NOT:   destroy_addr [[TREE_COPY]]
  // CHECK:       dealloc_stack [[TREE_COPY]]
  // CHECK:       br [[INNER_CONT:bb[0-9]+]]

  // CHECK:     [[INNER_CONT]]:
  // CHECK:       destroy_addr [[SCRATCH]]
  // CHECK:       dealloc_stack [[SCRATCH]]
  // CHECK:       function_ref @_TF13indirect_enum1dFT_T_
  // CHECK:       br [[OUTER_CONT]]
  default:
    d()
  }
  // CHECK:     [[OUTER_CONT]]:
  // CHECK:       destroy_addr %0
}

// CHECK-LABEL: sil hidden @_TF13indirect_enum10guardTreeA
func guardTreeA<T>(tree: TreeA<T>) {
  do {
    // CHECK:   retain_value %0
    // CHECK:   switch_enum %0 : $TreeA<T>, case #TreeA.Nil!enumelt: [[YES:bb[0-9]+]], default [[NO:bb[0-9]+]]
    // CHECK: [[NO]]:
    // CHECK:   release_value %0
    // CHECK: [[YES]]:
    // CHECK:   release_value %0
    guard case .Nil = tree else { return }

    // CHECK:   [[X:%.*]] = alloc_stack $T
    // CHECK:   retain_value %0
    // CHECK:   switch_enum %0 : $TreeA<T>, case #TreeA.Leaf!enumelt.1: [[YES:bb[0-9]+]], default [[NO:bb[0-9]+]]
    // CHECK: [[NO]]:
    // CHECK:   release_value %0
    // CHECK: [[YES]]([[BOX:%.*]] : $@box T):
    // CHECK:   [[VALUE_ADDR:%.*]] = project_box [[BOX]]
    // CHECK:   [[TMP:%.*]] = alloc_stack
    // CHECK:   copy_addr [[VALUE_ADDR]] to [initialization] [[TMP]]#1
    // CHECK:   copy_addr [take] [[TMP]]#1 to [initialization] [[X]]#1
    // CHECK:   strong_release [[BOX]]
    guard case .Leaf(let x) = tree else { return }

    // CHECK:   retain_value %0
    // CHECK:   switch_enum %0 : $TreeA<T>, case #TreeA.Branch!enumelt.1: [[YES:bb[0-9]+]], default [[NO:bb[0-9]+]]
    // CHECK: [[NO]]:
    // CHECK:   release_value %0
    // CHECK: [[YES]]([[BOX:%.*]] : $@box (left: TreeA<T>, right: TreeA<T>)):
    // CHECK:   [[VALUE_ADDR:%.*]] = project_box [[BOX]]
    // CHECK:   [[TUPLE:%.*]] = load [[VALUE_ADDR]]
    // CHECK:   retain_value [[TUPLE]]
    // CHECK:   [[L:%.*]] = tuple_extract [[TUPLE]]
    // CHECK:   [[R:%.*]] = tuple_extract [[TUPLE]]
    // CHECK:   strong_release [[BOX]]
    guard case .Branch(left: let l, right: let r) = tree else { return }

    // CHECK:   release_value [[R]]
    // CHECK:   release_value [[L]]
    // CHECK:   destroy_addr [[X]]
  }

  do {
    // CHECK:   retain_value %0
    // CHECK:   switch_enum %0 : $TreeA<T>, case #TreeA.Nil!enumelt: [[YES:bb[0-9]+]], default [[NO:bb[0-9]+]]
    // CHECK: [[NO]]:
    // CHECK:   release_value %0
    // CHECK: [[YES]]:
    // CHECK:   release_value %0
    if case .Nil = tree { }

    // CHECK:   [[X:%.*]] = alloc_stack $T
    // CHECK:   retain_value %0
    // CHECK:   switch_enum %0 : $TreeA<T>, case #TreeA.Leaf!enumelt.1: [[YES:bb[0-9]+]], default [[NO:bb[0-9]+]]
    // CHECK: [[NO]]:
    // CHECK:   release_value %0
    // CHECK: [[YES]]([[BOX:%.*]] : $@box T):
    // CHECK:   [[VALUE_ADDR:%.*]] = project_box [[BOX]]
    // CHECK:   [[TMP:%.*]] = alloc_stack
    // CHECK:   copy_addr [[VALUE_ADDR]] to [initialization] [[TMP]]#1
    // CHECK:   copy_addr [take] [[TMP]]#1 to [initialization] [[X]]#1
    // CHECK:   strong_release [[BOX]]
    // CHECK:   destroy_addr [[X]]
    if case .Leaf(let x) = tree { }


    // CHECK:   retain_value %0
    // CHECK:   switch_enum %0 : $TreeA<T>, case #TreeA.Branch!enumelt.1: [[YES:bb[0-9]+]], default [[NO:bb[0-9]+]]
    // CHECK: [[NO]]:
    // CHECK:   release_value %0
    // CHECK: [[YES]]([[BOX:%.*]] : $@box (left: TreeA<T>, right: TreeA<T>)):
    // CHECK:   [[VALUE_ADDR:%.*]] = project_box [[BOX]]
    // CHECK:   [[TUPLE:%.*]] = load [[VALUE_ADDR]]
    // CHECK:   retain_value [[TUPLE]]
    // CHECK:   [[L:%.*]] = tuple_extract [[TUPLE]]
    // CHECK:   [[R:%.*]] = tuple_extract [[TUPLE]]
    // CHECK:   strong_release [[BOX]]
    // CHECK:   release_value [[R]]
    // CHECK:   release_value [[L]]
    if case .Branch(left: let l, right: let r) = tree { }
  }
}

// CHECK-LABEL: sil hidden @_TF13indirect_enum10guardTreeB
func guardTreeB<T>(tree: TreeB<T>) {
  do {
    // CHECK:   copy_addr %0 to [initialization] [[TMP:%.*]]#1
    // CHECK:   switch_enum_addr [[TMP]]#1 : $*TreeB<T>, case #TreeB.Nil!enumelt: [[YES:bb[0-9]+]], default [[NO:bb[0-9]+]]
    // CHECK: [[NO]]:
    // CHECK:   destroy_addr [[TMP]]
    // CHECK: [[YES]]:
    // CHECK:   destroy_addr [[TMP]]
    guard case .Nil = tree else { return }

    // CHECK:   [[X:%.*]] = alloc_stack $T
    // CHECK:   copy_addr %0 to [initialization] [[TMP:%.*]]#1
    // CHECK:   switch_enum_addr [[TMP]]#1 : $*TreeB<T>, case #TreeB.Leaf!enumelt.1: [[YES:bb[0-9]+]], default [[NO:bb[0-9]+]]
    // CHECK: [[NO]]:
    // CHECK:   destroy_addr [[TMP]]
    // CHECK: [[YES]]:
    // CHECK:   [[VALUE:%.*]] = unchecked_take_enum_data_addr [[TMP]]
    // CHECK:   copy_addr [take] [[VALUE]] to [initialization] [[X]]
    // CHECK:   dealloc_stack [[TMP]]
    guard case .Leaf(let x) = tree else { return }

    // CHECK:   [[L:%.*]] = alloc_stack $TreeB
    // CHECK:   [[R:%.*]] = alloc_stack $TreeB
    // CHECK:   copy_addr %0 to [initialization] [[TMP:%.*]]#1
    // CHECK:   switch_enum_addr [[TMP]]#1 : $*TreeB<T>, case #TreeB.Branch!enumelt.1: [[YES:bb[0-9]+]], default [[NO:bb[0-9]+]]
    // CHECK: [[NO]]:
    // CHECK:   destroy_addr [[TMP]]
    // CHECK: [[YES]]:
    // CHECK:   [[BOX_ADDR:%.*]] = unchecked_take_enum_data_addr [[TMP]]
    // CHECK:   [[BOX:%.*]] = load [[BOX_ADDR]]
    // CHECK:   [[TUPLE_ADDR:%.*]] = project_box [[BOX]]
    // CHECK:   copy_addr [[TUPLE_ADDR]] to [initialization] [[TUPLE_COPY:%.*]]#1
    // CHECK:   [[L_COPY:%.*]] = tuple_element_addr [[TUPLE_COPY]]
    // CHECK:   copy_addr [take] [[L_COPY]] to [initialization] [[L]]
    // CHECK:   [[R_COPY:%.*]] = tuple_element_addr [[TUPLE_COPY]]
    // CHECK:   copy_addr [take] [[R_COPY]] to [initialization] [[R]]
    // CHECK:   strong_release [[BOX]]
    guard case .Branch(left: let l, right: let r) = tree else { return }

    // CHECK:   destroy_addr [[R]]
    // CHECK:   destroy_addr [[L]]
    // CHECK:   destroy_addr [[X]]
  }

  do {
    // CHECK:   copy_addr %0 to [initialization] [[TMP:%.*]]#1
    // CHECK:   switch_enum_addr [[TMP]]#1 : $*TreeB<T>, case #TreeB.Nil!enumelt: [[YES:bb[0-9]+]], default [[NO:bb[0-9]+]]
    // CHECK: [[NO]]:
    // CHECK:   destroy_addr [[TMP]]
    // CHECK: [[YES]]:
    // CHECK:   destroy_addr [[TMP]]
    if case .Nil = tree { }

    // CHECK:   [[X:%.*]] = alloc_stack $T
    // CHECK:   copy_addr %0 to [initialization] [[TMP:%.*]]#1
    // CHECK:   switch_enum_addr [[TMP]]#1 : $*TreeB<T>, case #TreeB.Leaf!enumelt.1: [[YES:bb[0-9]+]], default [[NO:bb[0-9]+]]
    // CHECK: [[NO]]:
    // CHECK:   destroy_addr [[TMP]]
    // CHECK: [[YES]]:
    // CHECK:   [[VALUE:%.*]] = unchecked_take_enum_data_addr [[TMP]]
    // CHECK:   copy_addr [take] [[VALUE]] to [initialization] [[X]]
    // CHECK:   dealloc_stack [[TMP]]
    // CHECK:   destroy_addr [[X]]
    if case .Leaf(let x) = tree { }

    // CHECK:   [[L:%.*]] = alloc_stack $TreeB
    // CHECK:   [[R:%.*]] = alloc_stack $TreeB
    // CHECK:   copy_addr %0 to [initialization] [[TMP:%.*]]#1
    // CHECK:   switch_enum_addr [[TMP]]#1 : $*TreeB<T>, case #TreeB.Branch!enumelt.1: [[YES:bb[0-9]+]], default [[NO:bb[0-9]+]]
    // CHECK: [[NO]]:
    // CHECK:   destroy_addr [[TMP]]
    // CHECK: [[YES]]:
    // CHECK:   [[BOX_ADDR:%.*]] = unchecked_take_enum_data_addr [[TMP]]
    // CHECK:   [[BOX:%.*]] = load [[BOX_ADDR]]
    // CHECK:   [[TUPLE_ADDR:%.*]] = project_box [[BOX]]
    // CHECK:   copy_addr [[TUPLE_ADDR]] to [initialization] [[TUPLE_COPY:%.*]]#1
    // CHECK:   [[L_COPY:%.*]] = tuple_element_addr [[TUPLE_COPY]]
    // CHECK:   copy_addr [take] [[L_COPY]] to [initialization] [[L]]
    // CHECK:   [[R_COPY:%.*]] = tuple_element_addr [[TUPLE_COPY]]
    // CHECK:   copy_addr [take] [[R_COPY]] to [initialization] [[R]]
    // CHECK:   strong_release [[BOX]]
    // CHECK:   destroy_addr [[R]]
    // CHECK:   destroy_addr [[L]]
    if case .Branch(left: let l, right: let r) = tree { }
  }
}

func dontDisableCleanupOfIndirectPayload(x: TrivialButIndirect) {
  // CHECK:   switch_enum %0 : $TrivialButIndirect, case #TrivialButIndirect.Direct!enumelt.1:  [[YES:bb[0-9]+]], default [[NO:bb[0-9]+]]
  // CHECK: [[NO]]:
  // CHECK:   release_value %0
  guard case .Direct(let foo) = x else { return }

  // -- Cleanup isn't necessary on "no" path because .Direct is trivial
  // CHECK:   switch_enum %0 : $TrivialButIndirect, case #TrivialButIndirect.Indirect!enumelt.1:  [[YES:bb[0-9]+]], default [[NO:bb[0-9]+]]
  // CHECK-NOT: [[NO]]:
  // CHECK: [[YES]]({{.*}}):
  guard case .Indirect(let bar) = x else { return }
}
