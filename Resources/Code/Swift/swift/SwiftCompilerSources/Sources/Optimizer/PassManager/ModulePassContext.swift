//===--- ModulePassContext.swift ------------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2022 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

import SIL
import OptimizerBridging

/// The context which is passed to a `ModulePass`'s run-function.
///
/// It provides access to all functions, v-tables and witness tables of a module,
/// but it doesn't provide any APIs to modify functions.
/// In order to modify a function, a module pass must use `transform(function:)`.
struct ModulePassContext {
  let _bridged: BridgedPassContext

  struct FunctionList : CollectionLikeSequence, IteratorProtocol {
    private var currentFunction: Function?
    
    fileprivate init(first: Function?) { currentFunction = first }

    mutating func next() -> Function? {
      if let f = currentFunction {
        currentFunction = PassContext_nextFunctionInModule(f.bridged).function
        return f
      }
      return nil
    }
  }

  struct VTableArray : BridgedRandomAccessCollection {
    fileprivate let bridged: BridgedVTableArray
    
    var startIndex: Int { return 0 }
    var endIndex: Int { return Int(bridged.count) }
    
    subscript(_ index: Int) -> VTable {
      assert(index >= 0 && index < bridged.count)
      return VTable(bridged: bridged.vTables![index])
    }
  }

  struct WitnessTableList : CollectionLikeSequence, IteratorProtocol {
    private var currentTable: WitnessTable?
    
    fileprivate init(first: WitnessTable?) { currentTable = first }

    mutating func next() -> WitnessTable? {
      if let t = currentTable {
        currentTable = PassContext_nextWitnessTableInModule(t.bridged).table
        return t
      }
      return nil
    }
  }

  struct DefaultWitnessTableList : CollectionLikeSequence, IteratorProtocol {
    private var currentTable: DefaultWitnessTable?
    
    fileprivate init(first: DefaultWitnessTable?) { currentTable = first }

    mutating func next() -> DefaultWitnessTable? {
      if let t = currentTable {
        currentTable = PassContext_nextDefaultWitnessTableInModule(t.bridged).table
        return t
      }
      return nil
    }
  }

  var functions: FunctionList {
    FunctionList(first: PassContext_firstFunctionInModule(_bridged).function)
  }
  
  var vTables: VTableArray {
    VTableArray(bridged: PassContext_getVTables(_bridged))
  }
  
  var witnessTables: WitnessTableList {
    WitnessTableList(first: PassContext_firstWitnessTableInModule(_bridged).table)
  }

  var defaultWitnessTables: DefaultWitnessTableList {
    DefaultWitnessTableList(first: PassContext_firstDefaultWitnessTableInModule(_bridged).table)
  }

  var options: Options { Options(_bridged: _bridged) }

  /// Run a closure with a `PassContext` for a function, which allows to modify that function.
  ///
  /// Only a single `transform` can be alive at the same time, i.e. it's not allowed to nest
  /// calls to `transform`.
  func transform(function: Function, _ runOnFunction: (PassContext) -> ()) {
    PassContext_beginTransformFunction(function.bridged, _bridged)
    runOnFunction(PassContext(_bridged: _bridged))
    PassContext_endTransformFunction(_bridged);
  }
}
