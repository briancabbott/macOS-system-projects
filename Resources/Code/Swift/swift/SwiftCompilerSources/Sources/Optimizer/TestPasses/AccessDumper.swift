//===--- AccessDumper.swift - Dump access information  --------------------===//
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

import Basic
import SIL

/// Dumps access information for memory accesses (`load` and `store`)
/// instructions.
/// Also verifies that `AccessPath.isDistinct(from:)` is correct. This does not actually
/// dumps anything, but aborts if the result is wrong.
///
/// This pass is used for testing `AccessUtils`.
let accessDumper = FunctionPass(name: "dump-access", {
  (function: Function, context: PassContext) in
  print("Accesses for \(function.name)")

  for block in function.blocks {
    for instr in block.instructions {
      switch instr {
      case let st as StoreInst:
        printAccessInfo(address: st.destination)
      case let load as LoadInst:
        printAccessInfo(address: load.operand)
      case let apply as ApplyInst:
        guard let callee = apply.referencedFunction else {
          break
        }
        if callee.name == "_isDistinct" {
          checkAliasInfo(forArgumentsOf: apply, expectDistinct: true)
        } else if callee.name == "_isNotDistinct" {
          checkAliasInfo(forArgumentsOf: apply, expectDistinct: false)
        }
      default:
        break
      }
    }
  }

  print("End accesses for \(function.name)")
})

private struct AccessStoragePathVisitor : AccessStoragePathWalker {
  var walkUpCache = WalkerCache<Path>()
  mutating func visit(accessStoragePath: ProjectedValue) {
    print("    Storage: \(accessStoragePath.value)")
    print("    Path: \"\(accessStoragePath.path)\"")
  }
}

private func printAccessInfo(address: Value) {
  print("Value: \(address)")

  var apw = AccessPathWalker()
  let (ap, scope) = apw.getAccessPathWithScope(of: address)
  if let beginAccess = scope {
    print("  Scope: \(beginAccess)")
  } else {
    print("  Scope: base")
  }

  print("  Base: \(ap.base)")
  print("  Path: \"\(ap.projectionPath)\"")

  var arw = AccessStoragePathVisitor()
  if !arw.visitAccessStorageRoots(of: ap) {
    print("   no Storage paths")
  }
}

private func checkAliasInfo(forArgumentsOf apply: ApplyInst, expectDistinct: Bool) {
  let address1 = apply.arguments[0]
  let address2 = apply.arguments[1]
  var apw = AccessPathWalker()
  let path1 = apw.getAccessPath(of: address1)
  let path2 = apw.getAccessPath(of: address2)

  if path1.isDistinct(from: path2) != expectDistinct {
    print("wrong isDistinct result of \(apply)")
  } else if path2.isDistinct(from: path1) != expectDistinct {
    print("wrong reverse isDistinct result of \(apply)")
  } else {
    return
  }
  
  print("in function")
  print(apply.function)
  fatalError()
}
