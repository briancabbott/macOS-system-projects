//==-------- PerformanceDiagnostics.cpp - Diagnose performance issues ------==//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2021 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "performance-diagnostics"
#include "swift/AST/DiagnosticsSIL.h"
#include "swift/AST/SemanticAttrs.h"
#include "swift/SIL/BasicBlockDatastructures.h"
#include "swift/SIL/InstructionUtils.h"
#include "swift/SIL/ApplySite.h"
#include "swift/SILOptimizer/Analysis/ArraySemantic.h"
#include "swift/SILOptimizer/Analysis/BasicCalleeAnalysis.h"
#include "swift/SILOptimizer/PassManager/Transforms.h"
#include "swift/SILOptimizer/Utils/BasicBlockOptUtils.h"
#include "llvm/Support/Debug.h"

using namespace swift;

namespace {

/// Issues performance diagnostics for functions which are annotated with
/// performance annotations, like @_noLocks, @_noAllocation.
///
/// This is done recursively for all functions which are called from
/// performance-annotated functions.
class PerformanceDiagnostics {

  /// A source location with a link to the location from the call-site.
  /// Used to print the whole back trace of a location.
  struct LocWithParent {
    SourceLoc loc;
    
    /// Null if this is the top-leve location.
    LocWithParent *parent;
    
    LocWithParent(SourceLoc loc, LocWithParent *parent) :
      loc(loc), parent(parent) {}
  };

  SILModule &module;
  BasicCalleeAnalysis *bca;
  llvm::DenseMap<SILFunction *, PerformanceConstraints> visitedFuncs;

public:
  PerformanceDiagnostics(SILModule &module, BasicCalleeAnalysis *bca) :
    module(module), bca(bca) {}

  /// Check a function with performance annotation(s) and all called functions
  /// recursively.
  bool visitFunction(SILFunction *function, PerformanceConstraints perfConstr) {
    return visitFunction(function, perfConstr, /*parentLoc*/ nullptr);
  }

  /// Check functions _without_ performance annotations.
  ///
  /// This is need to check closure arguments of called performance-annotated
  /// functions.
  void checkNonAnnotatedFunction(SILFunction *function);

private:
  bool visitFunction(SILFunction *function, PerformanceConstraints perfConstr,
                        LocWithParent *parentLoc);

  bool visitInst(SILInstruction *inst, PerformanceConstraints perfConstr,
                    LocWithParent *parentLoc);

  /// Check if `as` has any non-escaping closure arguments and if all those
  /// passed closures meet the `perfConstr`.
  ///
  /// If `acceptFunctionArgs` is true it is assumed that calls to the function,
  /// which contains `as`, are already checked to have correct closure arguments.
  bool checkClosureArguments(ApplySite as,
                             bool acceptFunctionArgs,
                             PerformanceConstraints perfConstr,
                             LocWithParent *parentLoc);

  /// Check if `closure` meets the `perfConstr`.
  ///
  /// If `acceptFunctionArgs` is true it is assumed that calls to the function,
  /// which contains `callInst`, are already checked to have correct closure
  /// arguments.
  bool checkClosureValue(SILValue closure,
                         bool acceptFunctionArgs,
                         SILInstruction *callInst,
                         PerformanceConstraints perfConstr,
                         LocWithParent *parentLoc);

  bool visitCallee(SILInstruction *callInst, CalleeList callees,
                   PerformanceConstraints perfConstr, LocWithParent *parentLoc);
                      
  template<typename ...ArgTypes>
  void diagnose(LocWithParent loc, Diag<ArgTypes...> ID,
                typename detail::PassArgument<ArgTypes>::type... Args) {
    diagnose(loc, Diagnostic(ID, std::move(Args)...));
  }
  
  void diagnose(LocWithParent loc, Diagnostic &&D);
};

static bool isEffectFreeArraySemanticCall(SILInstruction *inst) {
  ArraySemanticsCall semCall(inst);
  switch (semCall.getKind()) {
  case ArrayCallKind::kGetElement:
    return cast<ApplyInst>(inst)->getType().isTrivial(*inst->getFunction());
  default:
    return false;
  }
}

/// Prints performance diagnostics for \p function.
bool PerformanceDiagnostics::visitFunction(SILFunction *function,
                                              PerformanceConstraints perfConstr,
                                              LocWithParent *parentLoc) {
  if (!function->isDefinition())
    return false;

  ReachingReturnBlocks rrBlocks(function);
  NonErrorHandlingBlocks neBlocks(function);
                                       
  for (SILBasicBlock &block : *function) {
    if (!rrBlocks.reachesReturn(&block) || !neBlocks.isNonErrorHandling(&block))
      continue;
    for (SILInstruction &inst : block) {
      if (visitInst(&inst, perfConstr, parentLoc))
        return true;

      if (auto as = FullApplySite::isa(&inst)) {
        if (isEffectFreeArraySemanticCall(&inst))
          continue;

        // Check if closures, which are passed to `as` are okay.
        if (checkClosureArguments(as, /*acceptFunctionArgs=*/ true,
                                  perfConstr, parentLoc)) {
          return true;
        }

        if (as.getOrigCalleeType()->isNoEscape()) {
          // This is a call of a non-escaping closure. Check if the closure is
          // okay. In this case we don't need to `visitCallee`.
          if (checkClosureValue(as.getCallee(), /*acceptFunctionArgs=*/ true,
                                &inst, perfConstr, parentLoc)) {
            return true;
          }
          continue;
        }

        // Recursively walk into the callees.
        if (visitCallee(&inst, bca->getCalleeList(as), perfConstr, parentLoc))
          return true;
      } else if (auto *bi = dyn_cast<BuiltinInst>(&inst)) {
        switch (bi->getBuiltinInfo().ID) {
          case BuiltinValueKind::Once:
          case BuiltinValueKind::OnceWithContext:
            if (auto *fri = dyn_cast<FunctionRefInst>(bi->getArguments()[1])) {
              if (visitCallee(bi, fri->getReferencedFunction(), perfConstr, parentLoc))
                return true;
            } else {
              LocWithParent loc(inst.getLoc().getSourceLoc(), parentLoc);
              diagnose(loc, diag::performance_unknown_callees);
              return true;
            }
            break;
          default:
            break;
        }
      }
    }
  }
  return false;
}

bool PerformanceDiagnostics::checkClosureArguments(ApplySite as,
                                            bool acceptFunctionArgs,
                                            PerformanceConstraints perfConstr,
                                            LocWithParent *parentLoc) {
  if (SILFunction *knownCallee = as.getReferencedFunctionOrNull()) {
    if (knownCallee->hasSemanticsAttr(semantics::NO_PERFORMANCE_ANALYSIS))
      return false;
  }

  for (SILValue arg : as.getArguments()) {
    auto fTy = arg->getType().getAs<SILFunctionType>();
    if (!fTy || !fTy->isNoEscape())
      continue;

    if (checkClosureValue(arg, acceptFunctionArgs, as.getInstruction(),
                          perfConstr, parentLoc)) {
      return true;
    }
  }
  return false;
}

bool PerformanceDiagnostics::checkClosureValue(SILValue closure,
                                            bool acceptFunctionArgs,
                                            SILInstruction *callInst,
                                            PerformanceConstraints perfConstr,
                                            LocWithParent *parentLoc) {
  // Walk through the definition of the closure until we find the "underlying"
  // function_ref instruction.
  while (!isa<FunctionRefInst>(closure)) {
    if (auto *pai = dyn_cast<PartialApplyInst>(closure)) {
      if (checkClosureArguments(ApplySite::isa(pai), acceptFunctionArgs,
                                perfConstr, parentLoc)) {
        return true;
      }
      closure = pai->getCallee();
    } else if (auto *tfi = dyn_cast<ThinToThickFunctionInst>(closure)) {
      closure = tfi->getOperand();
    } else if (acceptFunctionArgs && isa<SILFunctionArgument>(closure)) {
      // We can assume that a function closure argument is already checked at
      // the call site.
      return false;
    } else {
      diagnose(LocWithParent(callInst->getLoc().getSourceLoc(), parentLoc), diag::performance_unknown_callees);
      return true;
    }
  }
  // Check what's happening inside the closure body.
  auto *fri = cast<FunctionRefInst>(closure);
  if (visitCallee(callInst, fri->getReferencedFunction(),
                  perfConstr, parentLoc)) {
    return true;
  }
  return false;
}

bool PerformanceDiagnostics::visitCallee(SILInstruction *callInst,
                                         CalleeList callees,
                                         PerformanceConstraints perfConstr,
                                         LocWithParent *parentLoc) {
  LocWithParent asLoc(callInst->getLoc().getSourceLoc(), parentLoc);
  LocWithParent *loc = &asLoc;
  if (parentLoc && asLoc.loc == callInst->getFunction()->getLocation().getSourceLoc())
    loc = parentLoc;

  if (callees.isIncomplete()) {
    diagnose(*loc, diag::performance_unknown_callees);
    return true;
  }
  for (SILFunction *callee : callees) {
    if (callee->hasSemanticsAttr(semantics::NO_PERFORMANCE_ANALYSIS))
      continue;
  
    // If the callee has a defined performance constraint which is at least as
    // strong as the required constraint, we are done.
    PerformanceConstraints calleeConstr = callee->getPerfConstraints();
    if (calleeConstr >= perfConstr)
      return false;

    if (!callee->isDefinition()) {
      diagnose(*loc, diag::performance_callee_unavailable);
      return true;
    }

    // Check if we already visited the callee while checking a constraint which
    // is at least as strong as  the required constraint.
    PerformanceConstraints &computedConstr = visitedFuncs[callee];
    if (computedConstr >= perfConstr)
      return false;
    computedConstr = perfConstr;
    
    if (visitFunction(callee, perfConstr, loc))
      return true;
  }
  return false;
}

static bool metatypeUsesAreNotRelevant(MetatypeInst *mt) {
  for (Operand *use : mt->getUses()) {
    if (auto  *bi = dyn_cast<BuiltinInst>(use->getUser())) {
      switch (bi->getBuiltinInfo().ID) {
        case BuiltinValueKind::Sizeof:
        case BuiltinValueKind::Strideof:
        case BuiltinValueKind::Alignof:
        case BuiltinValueKind::IsPOD:
        case BuiltinValueKind::IsConcrete:
        case BuiltinValueKind::IsBitwiseTakable:
          continue;
        default:
          break;
      }
    }
    return false;
  }
  return true;
}

bool PerformanceDiagnostics::visitInst(SILInstruction *inst,
                                          PerformanceConstraints perfConstr,
                                          LocWithParent *parentLoc) {
  SILType impactType;
  RuntimeEffect impact = getRuntimeEffect(inst, impactType);
  LocWithParent loc(inst->getLoc().getSourceLoc(), parentLoc);
  
  if (impact & RuntimeEffect::Casting) {
    // TODO: be more specific on casting.
    // E.g. distinguish locking and allocating dynamic casts, etc.
    diagnose(loc, diag::performance_dynamic_casting);
    return true;
  }
  if (impact & RuntimeEffect::MetaData) {
    // TODO: be more specific on metadata.
    // E.g. distinguish locking and allocating metadata operations, etc.
  
    // Try to give a good error message by looking which type of code it is.
    switch (inst->getKind()) {
    case SILInstructionKind::KeyPathInst:
      diagnose(loc, diag::performance_metadata, "using KeyPath");
      break;
    case SILInstructionKind::AllocGlobalInst:
    case SILInstructionKind::GlobalValueInst:
      diagnose(loc, diag::performance_metadata, "global or static variables");
      break;
    case SILInstructionKind::PartialApplyInst: {
      diagnose(loc, diag::performance_metadata,
               "generic closures or local functions");
      break;
    }
    case SILInstructionKind::ApplyInst:
    case SILInstructionKind::TryApplyInst:
    case SILInstructionKind::BeginApplyInst: {
      diagnose(loc, diag::performance_metadata, "generic function calls");
      break;
    }
    case SILInstructionKind::MetatypeInst:
      if (metatypeUsesAreNotRelevant(cast<MetatypeInst>(inst)))
        break;
      LLVM_FALLTHROUGH;
    default:
      // We didn't recognize the instruction, so try to give an error message
      // based on the involved type.
      if (impactType) {
        diagnose(loc, diag::performance_metadata_type, impactType.getASTType());
        break;
      }
      // The default error message.
      diagnose(loc, diag::performance_metadata, "this code pattern");
      break;
    }
    return true;
  }
  if (impact & RuntimeEffect::Allocating) {
    switch (inst->getKind()) {
    case SILInstructionKind::BeginApplyInst:
      // Not all begin_applys necessarily allocate. But it's difficult to
      // estimate the effect on SIL level.
      diagnose(loc, diag::performance_allocating, "co-routine calls");
      break;
    case SILInstructionKind::PartialApplyInst:
      diagnose(loc, diag::performance_allocating, "closure captures");
      break;
    case SILInstructionKind::AllocRefInst:
    case SILInstructionKind::AllocRefDynamicInst:
      diagnose(loc, diag::performance_allocating, "class instance construction");
      break;
    default:
      diagnose(loc, diag::performance_allocating, "this code pattern");
      break;
    }
    return true;
  }
  if (impact & RuntimeEffect::Deallocating) {
    if (impactType) {
      switch (inst->getKind()) {
      case SILInstructionKind::StoreInst:
      case SILInstructionKind::CopyAddrInst:
        diagnose(loc, diag::performance_deallocating_type,
                 "storing", impactType.getASTType());
        return true;
      case SILInstructionKind::DestroyAddrInst:
      case SILInstructionKind::DestroyValueInst:
        diagnose(loc, diag::performance_deallocating_type,
                 "ending the lifetime of", impactType.getASTType());
        return true;
      default:
        break;
      }
    }
    diagnose(loc, diag::performance_deallocating, "this code pattern");
    return true;
  }
  if (impact & RuntimeEffect::ObjectiveC) {
    diagnose(loc, diag::performance_objectivec);
    return true;
  }

  if (perfConstr == PerformanceConstraints::NoAllocation)
    return false;

  // Handle locking-only effects.

  if (impact & RuntimeEffect::Locking) {
    if (inst->getFunction()->isGlobalInit()) {
      diagnose(loc, diag::performance_locking,
               "global/static variable initialization");
    } else {
      diagnose(loc, diag::performance_locking, "this code pattern");
    }
    return true;
  }
  if (impact & RuntimeEffect::RefCounting) {
    diagnose(loc, diag::performance_arc);
    return true;
  }
  return false;
}

void PerformanceDiagnostics::checkNonAnnotatedFunction(SILFunction *function) {
  for (SILBasicBlock &block : *function) {
    for (SILInstruction &inst : block) {
      auto as = FullApplySite::isa(&inst);
      if (!as)
        continue;
        
      // We only consider direct calls.
      // TODO: this is a hole in the verification because we are not catching
      // cases where a "bad" closure is passed to a performance-annotated
      // v-table, witness table or closure function.
      SILFunction *callee = as.getReferencedFunctionOrNull();
      if (!callee)
        continue;

      if (callee->getPerfConstraints() == PerformanceConstraints::None)
        continue;

      if (checkClosureArguments(as, /*acceptFunctionArgs=*/ false,
                                callee->getPerfConstraints(),
                                /*LocWithParent*/ nullptr)) {
        return;
      }
    }
  }
}

void PerformanceDiagnostics::diagnose(LocWithParent loc, Diagnostic &&D) {
  // Start with a valid location in the call tree.
  LocWithParent *validLoc = &loc;
  while (!validLoc->loc.isValid() && validLoc->parent) {
    validLoc = validLoc->parent;
  }
  module.getASTContext().Diags.diagnose(validLoc->loc, D);
  
  // Print the whole back trace. Otherwise it would be difficult for the user
  // to know which annotated function caused the error.
  LocWithParent *parentLoc = validLoc->parent;
  while (parentLoc) {
    if (parentLoc->loc.isValid()) {
      module.getASTContext().Diags.diagnose(parentLoc->loc,
                                            diag::performance_called_from);
    }
    parentLoc = parentLoc->parent;
  }
}

//===----------------------------------------------------------------------===//
//                            The function pass
//===----------------------------------------------------------------------===//

class PerformanceDiagnosticsPass : public SILModuleTransform {
public:
  PerformanceDiagnosticsPass() {}

private:
  void run() override {
    SILModule *module = getModule();

    PerformanceDiagnostics diagnoser(*module, getAnalysis<BasicCalleeAnalysis>());
    bool annotatedFunctionsFound = false;

    for (SILFunction &function : *module) {
      if (function.getPerfConstraints() != PerformanceConstraints::None) {
        annotatedFunctionsFound = true;
      
        // Don't rerun diagnostics on deserialized functions.
        if (function.wasDeserializedCanonical())
          continue;

        if (!module->getOptions().EnablePerformanceAnnotations) {
          module->getASTContext().Diags.diagnose(
            function.getLocation().getSourceLoc(),
            diag::performance_annotations_not_enabled);
          return;
        }

        diagnoser.visitFunction(&function, function.getPerfConstraints());
      }
    }

    if (!annotatedFunctionsFound)
      return;

    for (SILFunction &function : *module) {
      // Don't rerun diagnostics on deserialized functions.
      if (function.wasDeserializedCanonical())
        continue;

      if (function.getPerfConstraints() == PerformanceConstraints::None) {
        diagnoser.checkNonAnnotatedFunction(&function);
      }
    }
  }
};

} // end anonymous namespace

SILTransform *swift::createPerformanceDiagnostics() {
  return new PerformanceDiagnosticsPass();
}
