//===- PerformanceInliner.cpp - Basic cost based inlining for performance -===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2015 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "sil-inliner"
#include "swift/SIL/SILInstruction.h"
#include "swift/SIL/Dominance.h"
#include "swift/SIL/SILModule.h"
#include "swift/SIL/Projection.h"
#include "swift/SILAnalysis/BasicCalleeAnalysis.h"
#include "swift/SILAnalysis/CallGraphAnalysis.h"
#include "swift/SILAnalysis/ColdBlockInfo.h"
#include "swift/SILAnalysis/DominanceAnalysis.h"
#include "swift/SILAnalysis/FunctionOrder.h"
#include "swift/SILAnalysis/LoopAnalysis.h"
#include "swift/SILPasses/Passes.h"
#include "swift/SILPasses/Transforms.h"
#include "swift/SILPasses/Utils/Local.h"
#include "swift/SILPasses/Utils/ConstantFolding.h"
#include "swift/SILPasses/Utils/Devirtualize.h"
#include "swift/SILPasses/Utils/Generics.h"
#include "swift/SILPasses/Utils/SILInliner.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/ADT/MapVector.h"
#include <functional>


using namespace swift;

STATISTIC(NumFunctionsInlined, "Number of functions inlined");

namespace {

  // Threshold for deterministic testing of the inline heuristic.
  // It specifies an instruction cost limit where a simplified model is used
  // for the instruction costs: only builtin instructions have a cost of exactly
  // 1.
  llvm::cl::opt<int> TestThreshold("sil-inline-test-threshold",
                                        llvm::cl::init(-1), llvm::cl::Hidden);

  llvm::cl::opt<int> TestOpt("sil-inline-test",
                                   llvm::cl::init(0), llvm::cl::Hidden);
  
  // The following constants define the cost model for inlining.
  
  // The base value for every call: it represents the benefit of removing the
  // call overhead.
  // This value can be overridden with the -sil-inline-threshold option.
  const unsigned RemovedCallBenefit = 80;
  
  // The benefit if the condition of a terminator instruction gets constant due
  // to inlining.
  const unsigned ConstTerminatorBenefit = 2;

  // Benefit if the operand of an apply gets constant, e.g. if a closure is
  // passed to an apply instruction in the callee.
  const unsigned ConstCalleeBenefit = 150;
  
  // Additional benefit for each loop level.
  const unsigned LoopBenefitFactor = 40;
  
  // Approximately up to this cost level a function can be inlined without
  // increasing the code size.
  const unsigned TrivialFunctionThreshold = 20;

  // Represents a value in integer constant evaluation.
  struct IntConst {
    IntConst() : isValid(false), isFromCaller(false) { }
    
    IntConst(const APInt &value, bool isFromCaller) :
    value(value), isValid(true), isFromCaller(isFromCaller) { }
    
    // The actual value.
    APInt value;
    
    // True if the value is valid, i.e. could be evaluated to a constant.
    bool isValid;
    
    // True if the value is only valid, because a constant is passed to the
    // callee. False if constant propagation could do the same job inside the
    // callee without inlining it.
    bool isFromCaller;
  };
  
  // Tracks constants in the caller and callee to get an estimation of what
  // values get constant if the callee is inlined.
  // This can be seen as a "simulation" of several optimizations: SROA, mem2reg
  // and constant propagation.
  // Note that this is only a simplified model and not correct in all cases.
  // For example aliasing information is not taken into account.
  class ConstantTracker {
    // Links between loaded and stored values.
    // The key is a load instruction, the value is the corresponding store
    // instruction which stores the loaded value. Both, key and value can also
    // be copy_addr instructions.
    llvm::DenseMap<SILInstruction *, SILInstruction *> links;
    
    // The current stored values at memory addresses.
    // The key is the base address of the memory (after skipping address
    // projections). The value are store (or copy_addr) instructions, which
    // store the current value.
    // This is only an estimation, because e.g. it does not consider potential
    // aliasing.
    llvm::DenseMap<SILValue, SILInstruction *> memoryContent;
    
    // Cache for evaluated constants.
    llvm::SmallDenseMap<BuiltinInst *, IntConst> constCache;

    // The caller/callee function which is tracked.
    SILFunction *F;
    
    // The constant tracker of the caller function (null if this is the
    // tracker of the callee).
    ConstantTracker *callerTracker;
    
    // The apply instruction in the caller (null if this is the tracker of the
    // callee).
    FullApplySite AI;
    
    // Walks through address projections and (optionally) collects them.
    // Returns the base address, i.e. the first address which is not a
    // projection.
    SILValue scanProjections(SILValue addr,
                             SmallVectorImpl<Projection> *Result = nullptr);
    
    // Get the stored value for a load. The loadInst can be either a real load
    // or a copy_addr.
    SILValue getStoredValue(SILInstruction *loadInst,
                            ProjectionPath &projStack);

    // Gets the parameter in the caller for a function argument.
    SILValue getParam(SILValue value) {
      if (SILArgument *arg = dyn_cast<SILArgument>(value)) {
        if (AI && arg->isFunctionArg() && arg->getFunction() == F) {
          // Continue at the caller.
          return AI.getArgument(arg->getIndex());
        }
      }
      return SILValue();
    }
    
    SILInstruction *getMemoryContent(SILValue addr) {
      // The memory content can be stored in this ConstantTracker or in the
      // caller's ConstantTracker.
      SILInstruction *storeInst = memoryContent[addr];
      if (storeInst)
        return storeInst;
      if (callerTracker)
        return callerTracker->getMemoryContent(addr);
      return nullptr;
    }
    
    // Gets the estimated definition of a value.
    SILInstruction *getDef(SILValue val, ProjectionPath &projStack);

    // Gets the estimated integer constant result of a builtin.
    IntConst getBuiltinConst(BuiltinInst *BI, int depth);
    
  public:
    
    // Constructor for the caller function.
    ConstantTracker(SILFunction *function) :
      F(function), callerTracker(nullptr), AI()
    { }
    
    // Constructor for the callee function.
    ConstantTracker(SILFunction *function, ConstantTracker *caller,
                    FullApplySite callerApply) :
       F(function), callerTracker(caller), AI(callerApply)
    { }
    
    void beginBlock() {
      // Currently we don't do any sophisticated dataflow analysis, so we keep
      // the memoryContent alive only for a single block.
      memoryContent.clear();
    }

    // Must be called for each instruction visited in dominance order.
    void trackInst(SILInstruction *inst);
    
    // Gets the estimated definition of a value.
    SILInstruction *getDef(SILValue val) {
      ProjectionPath projStack;
      return getDef(val, projStack);
    }
    
    // Gets the estimated definition of a value if it is in the caller.
    SILInstruction *getDefInCaller(SILValue val) {
      SILInstruction *def = getDef(val);
      if (def && def->getFunction() != F)
        return def;
      return nullptr;
    }
    
    // Gets the estimated integer constant of a value.
    IntConst getIntConst(SILValue val, int depth = 0);
  };

  // Controls the decision to inline functions with @_semantics, @effect and
  // global_init attributes.
  enum class InlineSelection {
    Everything,
    NoGlobalInit, // and no availability semantics calls
    NoSemanticsAndGlobalInit
  };

  class SILPerformanceInliner {
    /// The inline threashold.
    const int InlineCostThreshold;
    /// Specifies which functions not to inline, based on @_semantics and
    /// global_init attributes.
    InlineSelection WhatToInline;


    /// A set of pairs of function names. This set records a successful
    /// inlining operations and is used to prevent infinite inlining of
    /// recursive functions. The pair (A, B) records inlining of function
    /// B into A.
    llvm::DenseSet<std::pair<StringRef, StringRef>> InlinedFunctions;

    SILFunction *getEligibleFunction(FullApplySite AI);

    bool isProfitableToInline(FullApplySite AI, unsigned loopDepthOfAI,
                              DominanceAnalysis *DA,
                              SILLoopAnalysis *LA,
                              ConstantTracker &constTracker);

    void visitColdBlocks(SmallVectorImpl<FullApplySite> &AppliesToInline,
                         SILBasicBlock *root, DominanceInfo *DT);

    void collectAppliesToInline(SILFunction *Caller,
                                SmallVectorImpl<FullApplySite> &Applies,
                                DominanceAnalysis *DA, SILLoopAnalysis *LA);

    /// This method is responsible for breaking recursive cycles
    /// in the inliner (some of which are only exposed via devirtualization).
    /// \return true if the function \p Callee was previously inlined into
    /// \p Caller and the inliner needs to reject this inlining request.
    bool hasInliningCycle(SILFunction *Caller, SILFunction *Callee);

    FullApplySite devirtualizeUpdatingCallGraph(FullApplySite Apply,
                                                CallGraph &CG);

    bool devirtualizeAndSpecializeApplies(
      llvm::SmallVectorImpl<ApplySite> &Applies,
        CallGraphAnalysis *CGA,
        SILModuleTransform *MT,
        llvm::SmallVectorImpl<SILFunction *> &WorkList);

    ApplySite specializeGenericUpdatingCallGraph(ApplySite Apply,
                                                 CallGraph &CG,
                                  llvm::SmallVectorImpl<ApplySite> &NewApplies);

    bool inlineCallsIntoFunction(SILFunction *F, DominanceAnalysis *DA,
                                 SILLoopAnalysis *LA, CallGraph &CG,
                              llvm::SmallVectorImpl<FullApplySite> &NewApplies);

  public:
    SILPerformanceInliner(int threshold,
                          InlineSelection WhatToInline)
      : InlineCostThreshold(threshold),
    WhatToInline(WhatToInline) {}

    void inlineDevirtualizeAndSpecialize(SILFunction *WorkItem,
                                         SILModuleTransform *MT,
                                         CallGraphAnalysis *CGA,
                                         DominanceAnalysis *DA,
                                         SILLoopAnalysis *LA);
  };
}

//===----------------------------------------------------------------------===//
//                               ConstantTracker
//===----------------------------------------------------------------------===//


void ConstantTracker::trackInst(SILInstruction *inst) {
  if (LoadInst *LI = dyn_cast<LoadInst>(inst)) {
    SILValue baseAddr = scanProjections(LI->getOperand());
    if (SILInstruction *loadLink = getMemoryContent(baseAddr))
       links[LI] = loadLink;
  } else if (StoreInst *SI = dyn_cast<StoreInst>(inst)) {
    SILValue baseAddr = scanProjections(SI->getOperand(1));
    memoryContent[baseAddr] = SI;
  } else if (CopyAddrInst *CAI = dyn_cast<CopyAddrInst>(inst)) {
    if (!CAI->isTakeOfSrc()) {
      // Treat a copy_addr as a load + store
      SILValue loadAddr = scanProjections(CAI->getOperand(0));
      if (SILInstruction *loadLink = getMemoryContent(loadAddr)) {
        links[CAI] = loadLink;
        SILValue storeAddr = scanProjections(CAI->getOperand(1));
        memoryContent[storeAddr] = CAI;
      }
    }
  }
}

SILValue ConstantTracker::scanProjections(SILValue addr,
                                          SmallVectorImpl<Projection> *Result) {
  for (;;) {
    if (Projection::isAddrProjection(addr)) {
      SILInstruction *I = cast<SILInstruction>(addr.getDef());
      if (Result) {
        Optional<Projection> P = Projection::addressProjectionForInstruction(I);
        Result->push_back(P.getValue());
      }
      addr = I->getOperand(0);
      continue;
    }
    if (SILValue param = getParam(addr)) {
      // Go to the caller.
      addr = param;
      continue;
    }
    // Return the base address = the first address which is not a projection.
    return addr;
  }
}

SILValue ConstantTracker::getStoredValue(SILInstruction *loadInst,
                                  ProjectionPath &projStack) {
  SILInstruction *store = links[loadInst];
  if (!store && callerTracker)
    store = callerTracker->links[loadInst];
  if (!store) return SILValue();

  assert(isa<LoadInst>(loadInst) || isa<CopyAddrInst>(loadInst));

  // Push the address projections of the load onto the stack.
  SmallVector<Projection, 4> loadProjections;
  scanProjections(loadInst->getOperand(0), &loadProjections);
  for (const Projection &proj : loadProjections) {
    projStack.push_back(proj);
  }
  
  //  Pop the address projections of the store from the stack.
  SmallVector<Projection, 4> storeProjections;
  scanProjections(store->getOperand(1), &storeProjections);
  for (auto iter = storeProjections.rbegin(); iter != storeProjections.rend();
       ++iter) {
    const Projection &proj = *iter;
    // The corresponding load-projection must match the store-projection.
    if (projStack.empty() || projStack.back() != proj)
      return SILValue();
    projStack.pop_back();
  }
  
  if (isa<StoreInst>(store))
    return store->getOperand(0);

  // The copy_addr instruction is both a load and a store. So we follow the link
  // again.
  assert(isa<CopyAddrInst>(store));
  return getStoredValue(store, projStack);
}

// Get the aggregate member based on the top of the projection stack.
static SILValue getMember(SILInstruction *inst, ProjectionPath &projStack) {
  if (!projStack.empty()) {
    const Projection &proj = projStack.back();
    return proj.getOperandForAggregate(inst);
  }
  return SILValue();
}

SILInstruction *ConstantTracker::getDef(SILValue val,
                                          ProjectionPath &projStack) {
  
  // Track the value up the dominator tree.
  for (;;) {
    if (SILInstruction *inst = dyn_cast<SILInstruction>(val)) {
      if (auto proj = Projection::valueProjectionForInstruction(inst)) {
        // Extract a member from a struct/tuple/enum.
        projStack.push_back(proj.getValue());
        val = inst->getOperand(0);
        continue;
      } else if (SILValue member = getMember(inst, projStack)) {
        // The opposite of a projection instruction: composing a struct/tuple.
        projStack.pop_back();
        val = member;
        continue;
      } else if (SILValue loadedVal = getStoredValue(inst, projStack)) {
        // A value loaded from memory.
        val = loadedVal;
        continue;
      } else if (isa<ThinToThickFunctionInst>(inst)) {
        val = inst->getOperand(0);
        continue;
      }
      return inst;
    } else if (SILValue param = getParam(val)) {
      // Continue in the caller.
      val = param;
      continue;
    }
    return nullptr;
  }
}

IntConst ConstantTracker::getBuiltinConst(BuiltinInst *BI, int depth) {
  const BuiltinInfo &Builtin = BI->getBuiltinInfo();
  OperandValueArrayRef Args = BI->getArguments();
  switch (Builtin.ID) {
    default: break;
      
      // Fold comparison predicates.
#define BUILTIN(id, name, Attrs)
#define BUILTIN_BINARY_PREDICATE(id, name, attrs, overload) \
case BuiltinValueKind::id:
#include "swift/AST/Builtins.def"
    {
      IntConst lhs = getIntConst(Args[0], depth);
      IntConst rhs = getIntConst(Args[1], depth);
      if (lhs.isValid && rhs.isValid) {
        return IntConst(constantFoldComparison(lhs.value, rhs.value,
                                              Builtin.ID),
                        lhs.isFromCaller || rhs.isFromCaller);
      }
      break;
    }
      
      
    case BuiltinValueKind::SAddOver:
    case BuiltinValueKind::UAddOver:
    case BuiltinValueKind::SSubOver:
    case BuiltinValueKind::USubOver:
    case BuiltinValueKind::SMulOver:
    case BuiltinValueKind::UMulOver: {
      IntConst lhs = getIntConst(Args[0], depth);
      IntConst rhs = getIntConst(Args[1], depth);
      if (lhs.isValid && rhs.isValid) {
        bool IgnoredOverflow;
        return IntConst(constantFoldBinaryWithOverflow(lhs.value, rhs.value,
                        IgnoredOverflow,
                        getLLVMIntrinsicIDForBuiltinWithOverflow(Builtin.ID)),
                          lhs.isFromCaller || rhs.isFromCaller);
      }
      break;
    }
      
    case BuiltinValueKind::SDiv:
    case BuiltinValueKind::SRem:
    case BuiltinValueKind::UDiv:
    case BuiltinValueKind::URem: {
      IntConst lhs = getIntConst(Args[0], depth);
      IntConst rhs = getIntConst(Args[1], depth);
      if (lhs.isValid && rhs.isValid && rhs.value != 0) {
        bool IgnoredOverflow;
        return IntConst(constantFoldDiv(lhs.value, rhs.value,
                                        IgnoredOverflow, Builtin.ID),
                        lhs.isFromCaller || rhs.isFromCaller);
      }
      break;
    }
      
    case BuiltinValueKind::And:
    case BuiltinValueKind::AShr:
    case BuiltinValueKind::LShr:
    case BuiltinValueKind::Or:
    case BuiltinValueKind::Shl:
    case BuiltinValueKind::Xor: {
      IntConst lhs = getIntConst(Args[0], depth);
      IntConst rhs = getIntConst(Args[1], depth);
      if (lhs.isValid && rhs.isValid) {
        return IntConst(constantFoldBitOperation(lhs.value, rhs.value,
                                                 Builtin.ID),
                        lhs.isFromCaller || rhs.isFromCaller);
      }
      break;
    }
      
    case BuiltinValueKind::Trunc:
    case BuiltinValueKind::ZExt:
    case BuiltinValueKind::SExt:
    case BuiltinValueKind::TruncOrBitCast:
    case BuiltinValueKind::ZExtOrBitCast:
    case BuiltinValueKind::SExtOrBitCast: {
      IntConst val = getIntConst(Args[0], depth);
      if (val.isValid) {
        return IntConst(constantFoldCast(val.value, Builtin), val.isFromCaller);
      }
      break;
    }
  }
  return IntConst();
}

// Tries to evaluate the integer constant of a value. The \p depth is used
// to limit the complexity.
IntConst ConstantTracker::getIntConst(SILValue val, int depth) {
  
  // Don't spend too much time with constant evaluation.
  if (depth >= 10)
    return IntConst();
  
  SILInstruction *I = getDef(val);
  if (!I)
    return IntConst();
  
  if (auto *IL = dyn_cast<IntegerLiteralInst>(I)) {
    return IntConst(IL->getValue(), IL->getFunction() != F);
  }
  if (auto *BI = dyn_cast<BuiltinInst>(I)) {
    if (constCache.count(BI) != 0)
      return constCache[BI];
    
    IntConst builtinConst = getBuiltinConst(BI, depth + 1);
    constCache[BI] = builtinConst;
    return builtinConst;
  }
  return IntConst();
}

//===----------------------------------------------------------------------===//
//                           Performance Inliner
//===----------------------------------------------------------------------===//

bool SILPerformanceInliner::hasInliningCycle(SILFunction *Caller,
                                                SILFunction *Callee) {
  // Reject simple recursions.
  if (Caller == Callee) return true;

  StringRef CallerName = Caller->getName();
  StringRef CalleeName = Callee->getName();

  bool InlinedBefore = InlinedFunctions.count(std::make_pair(CallerName, CalleeName));

  // If the Callee was inlined into the Caller in previous inlining iterations then
  // we need to reject this inlining request to prevent a cycle.
  return InlinedBefore;
}

// Returns the callee of an apply_inst if it is basically inlinable.
SILFunction *SILPerformanceInliner::getEligibleFunction(FullApplySite AI) {

  SILFunction *Callee = AI.getCalleeFunction();
  
  if (!Callee) {
    DEBUG(llvm::dbgs() << "        FAIL: Cannot find inlineable callee.\n");
    return nullptr;
  }

  // Don't inline functions that are marked with the @_semantics or @effects
  // attribute if the inliner is asked not to inline them.
  if (Callee->hasDefinedSemantics() || Callee->hasEffectsKind()) {
    if (WhatToInline == InlineSelection::NoSemanticsAndGlobalInit) {
      DEBUG(llvm::dbgs() << "        FAIL: Function " << Callee->getName()
            << " has special semantics or effects attribute.\n");
      return nullptr;
    }
    // The "availability" semantics attribute is treated like global-init.
    if (Callee->hasDefinedSemantics() &&
        WhatToInline != InlineSelection::Everything &&
        Callee->getSemanticsString().startswith("availability")) {
      return nullptr;
    }
  } else if (Callee->isGlobalInit()) {
    if (WhatToInline != InlineSelection::Everything) {
      DEBUG(llvm::dbgs() << "        FAIL: Function " << Callee->getName()
            << " has the global-init attribute.\n");
      return nullptr;
    }
  }

  // We can't inline external declarations.
  if (Callee->empty() || Callee->isExternalDeclaration()) {
    DEBUG(llvm::dbgs() << "        FAIL: Cannot inline external " <<
          Callee->getName() << ".\n");
    return nullptr;
  }

  // Explicitly disabled inlining.
  if (Callee->getInlineStrategy() == NoInline) {
    DEBUG(llvm::dbgs() << "        FAIL: noinline attribute on " <<
          Callee->getName() << ".\n");
    return nullptr;
  }
  
  if (!Callee->shouldOptimize()) {
    DEBUG(llvm::dbgs() << "        FAIL: optimizations disabled on " <<
          Callee->getName() << ".\n");
    return nullptr;
  }

  // We don't support this yet.
  if (AI.hasSubstitutions()) {
    DEBUG(llvm::dbgs() << "        FAIL: Generic substitutions on " <<
          Callee->getName() << ".\n");
    return nullptr;
  }

  // We don't support inlining a function that binds dynamic self because we
  // have no mechanism to preserve the original function's local self metadata.
  if (computeMayBindDynamicSelf(Callee)) {
    DEBUG(llvm::dbgs() << "        FAIL: Binding dynamic Self in " <<
          Callee->getName() << ".\n");
    return nullptr;
  }

  SILFunction *Caller = AI.getFunction();

  // Detect inlining cycles.
  if (hasInliningCycle(Caller, Callee)) {
    DEBUG(llvm::dbgs() << "        FAIL: Detected a recursion inlining " <<
          Callee->getName() << ".\n");
    return nullptr;
  }

  // A non-fragile function may not be inlined into a fragile function.
  if (Caller->isFragile() && !Callee->isFragile()) {
    DEBUG(llvm::dbgs() << "        FAIL: Can't inline fragile " <<
          Callee->getName() << ".\n");
    return nullptr;
  }
  DEBUG(llvm::dbgs() << "        Eligible callee: " <<
        Callee->getName() << "\n");
  
  return Callee;
}

// Gets the cost of an instruction by using the simplified test-model: only
// builtin instructions have a cost and that's exactly 1.
static unsigned testCost(SILInstruction *I) {
  switch (I->getKind()) {
    case ValueKind::BuiltinInst:
      return 1;
    default:
      return 0;
  }
}

// Returns the taken block of a terminator instruction if the condition turns
// out to be constant.
static SILBasicBlock *getTakenBlock(TermInst *term,
                                    ConstantTracker &constTracker) {
  if (CondBranchInst *CBI = dyn_cast<CondBranchInst>(term)) {
    IntConst condConst = constTracker.getIntConst(CBI->getCondition());
    if (condConst.isFromCaller) {
      return condConst.value != 0 ? CBI->getTrueBB() : CBI->getFalseBB();
    }
    return nullptr;
  }
  if (SwitchValueInst *SVI = dyn_cast<SwitchValueInst>(term)) {
    IntConst switchConst = constTracker.getIntConst(SVI->getOperand());
    if (switchConst.isFromCaller) {
      for (unsigned Idx = 0; Idx < SVI->getNumCases(); ++Idx) {
        auto switchCase = SVI->getCase(Idx);
        if (auto *IL = dyn_cast<IntegerLiteralInst>(switchCase.first)) {
          if (switchConst.value == IL->getValue())
            return switchCase.second;
        } else {
          return nullptr;
        }
      }
      if (SVI->hasDefault())
          return SVI->getDefaultBB();
    }
    return nullptr;
  }
  if (SwitchEnumInst *SEI = dyn_cast<SwitchEnumInst>(term)) {
    if (SILInstruction *def = constTracker.getDefInCaller(SEI->getOperand())) {
      if (EnumInst *EI = dyn_cast<EnumInst>(def)) {
        for (unsigned Idx = 0; Idx < SEI->getNumCases(); ++Idx) {
          auto enumCase = SEI->getCase(Idx);
          if (enumCase.first == EI->getElement())
            return enumCase.second;
        }
        if (SEI->hasDefault())
          return SEI->getDefaultBB();
      }
    }
    return nullptr;
  }
  if (CheckedCastBranchInst *CCB = dyn_cast<CheckedCastBranchInst>(term)) {
    if (SILInstruction *def = constTracker.getDefInCaller(CCB->getOperand())) {
      if (UpcastInst *UCI = dyn_cast<UpcastInst>(def)) {
        SILType castType = UCI->getOperand()->getType(0);
        if (CCB->getCastType().isSuperclassOf(castType)) {
          return CCB->getSuccessBB();
        }
        if (!castType.isSuperclassOf(CCB->getCastType())) {
          return CCB->getFailureBB();
        }
      }
    }
  }
  return nullptr;
}

/// Return true if inlining this call site is profitable.
bool SILPerformanceInliner::isProfitableToInline(FullApplySite AI,
                                              unsigned loopDepthOfAI,
                                              DominanceAnalysis *DA,
                                              SILLoopAnalysis *LA,
                                              ConstantTracker &callerTracker) {
  SILFunction *Callee = AI.getCalleeFunction();
  
  if (Callee->getInlineStrategy() == AlwaysInline)
    return true;
  
  ConstantTracker constTracker(Callee, &callerTracker, AI);
  
  DominanceInfo *DT = DA->get(Callee);
  SILLoopInfo *LI = LA->get(Callee);

  DominanceOrder domOrder(&Callee->front(), DT, Callee->size());
  
  // Calculate the inlining cost of the callee.
  unsigned CalleeCost = 0;
  unsigned Benefit = InlineCostThreshold > 0 ? InlineCostThreshold :
                                               RemovedCallBenefit;
  Benefit += loopDepthOfAI * LoopBenefitFactor;
  int testThreshold = TestThreshold;

  while (SILBasicBlock *block = domOrder.getNext()) {
    constTracker.beginBlock();
    unsigned loopDepth = LI->getLoopDepth(block);
    for (SILInstruction &I : *block) {
      constTracker.trackInst(&I);
      
      auto ICost = instructionInlineCost(I);
      
      if (testThreshold >= 0) {
        // We are in test-mode: use a simplified cost model.
        CalleeCost += testCost(&I);
      } else {
        // Use the regular cost model.
        CalleeCost += unsigned(ICost);
      }
      
      if (ApplyInst *AI = dyn_cast<ApplyInst>(&I)) {
        
        // Check if the callee is passed as an argument. If so, increase the
        // threshold, because inlining will (probably) eliminate the closure.
        SILInstruction *def = constTracker.getDefInCaller(AI->getCallee());
        if (def && (isa<FunctionRefInst>(def) || isa<PartialApplyInst>(def))) {

          DEBUG(llvm::dbgs() << "        Boost: apply const function at" << *AI);
          Benefit += ConstCalleeBenefit + loopDepth * LoopBenefitFactor;
          testThreshold *= 2;
        }
      }
    }
    // Don't count costs in blocks which are dead after inlining.
    SILBasicBlock *takenBlock = getTakenBlock(block->getTerminator(),
                                              constTracker);
    if (takenBlock) {
      Benefit += ConstTerminatorBenefit + TestOpt;
      DEBUG(llvm::dbgs() << "      Take bb" << takenBlock->getDebugID() <<
            " of" << *block->getTerminator());
      domOrder.pushChildrenIf(block, [=] (SILBasicBlock *child) {
        return child->getSinglePredecessor() != block || child == takenBlock;
      });
    } else {
      domOrder.pushChildren(block);
    }
  }

  unsigned Threshold = Benefit; // The default.
  if (testThreshold >= 0) {
    // We are in testing mode.
    Threshold = testThreshold;
  } else if (AI.getFunction()->isThunk()) {
    // Only inline trivial functions into thunks (which will not increase the
    // code size).
    Threshold = TrivialFunctionThreshold;
  }

  if (CalleeCost > Threshold) {
    DEBUG(llvm::dbgs() << "        NO: Function too big to inline, "
          "cost: " << CalleeCost << ", threshold: " << Threshold << "\n");
    return false;
  }
  DEBUG(llvm::dbgs() << "        YES: ready to inline, "
        "cost: " << CalleeCost << ", threshold: " << Threshold << "\n");
  return true;
}

/// Return true if inlining this call site into a cold block is profitable.
static bool isProfitableInColdBlock(SILFunction *Callee) {
  if (Callee->getInlineStrategy() == AlwaysInline)
    return true;

  // Testing with the TestThreshold disables inlining into cold blocks.
  if (TestThreshold >= 0)
    return false;
  
  unsigned CalleeCost = 0;
  
  for (SILBasicBlock &Block : *Callee) {
    for (SILInstruction &I : Block) {
      auto ICost = instructionInlineCost(I);
      CalleeCost += (unsigned)ICost;

      if (CalleeCost > TrivialFunctionThreshold)
        return false;
    }
  }

  DEBUG(llvm::dbgs() << "        YES: ready to inline into cold block, cost:"
        << CalleeCost << "\n");
  return true;
}


// Attempt to devirtualize, maintaining the call graph if
// successful. When successful, replaces the old apply with the new
// one and returns the new one. When unsuccessful returns an empty
// apply site.
FullApplySite SILPerformanceInliner::devirtualizeUpdatingCallGraph(
                                                            FullApplySite Apply,
                                                                CallGraph &CG) {
  auto NewInstPair = tryDevirtualizeApply(Apply);
  if (!NewInstPair.second)
    return FullApplySite();

  auto NewAI = FullApplySite::isa(NewInstPair.second.getInstruction());
  CallGraphEditor(&CG).replaceApplyWithNew(Apply, NewAI);

  replaceDeadApply(Apply, NewInstPair.first);

  return NewAI;
}

ApplySite SILPerformanceInliner::specializeGenericUpdatingCallGraph(
                                                                ApplySite Apply,
                                                                CallGraph &CG,
                                 llvm::SmallVectorImpl<ApplySite> &NewApplies) {
  assert(NewApplies.empty() && "Expected out parameter for new applies!");

  if (!Apply.hasSubstitutions())
    return ApplySite();

  auto *Callee = Apply.getCalleeFunction();

  if (!Callee || Callee->isExternalDeclaration())
    return ApplySite();

  auto Filter = [](SILInstruction *I) -> bool {
    return ApplySite::isa(I) != ApplySite();
  };

  CloneCollector Collector(Filter);

  SILFunction *SpecializedFunction;
  auto Specialized = trySpecializeApplyOfGeneric(Apply,
                                                 SpecializedFunction,
                                                 Collector);

  if (!Specialized)
    return ApplySite();

  // Add the specialization to the call graph.
  CallGraphEditor Editor(&CG);
  if (SpecializedFunction)
    Editor.addNewFunction(SpecializedFunction);

  // Track the new applies from the specialization.
  for (auto NewCallSite : Collector.getInstructionPairs())
    if (auto NewApply = ApplySite::isa(NewCallSite.first))
      NewApplies.push_back(NewApply);

  auto FullApply = FullApplySite::isa(Apply.getInstruction());

  if (!FullApply) {
    assert(!FullApplySite::isa(Specialized.getInstruction()) &&
           "Unexpected full apply generated!");

    // Replace the old apply with the new and delete the old.
    replaceDeadApply(Apply, Specialized.getInstruction());
    Editor.updatePartialApplyUses(Specialized);

    return ApplySite(Specialized);
  }

  // Update call graph edges
  auto SpecializedFullApply = FullApplySite(Specialized.getInstruction());
  Editor.replaceApplyWithNew(FullApply, SpecializedFullApply);

  // Replace the old apply with the new and delete the old.
  replaceDeadApply(Apply, Specialized.getInstruction());

  return SpecializedFullApply;
}

static void collectAllAppliesInFunction(SILFunction *F,
                                llvm::SmallVectorImpl<ApplySite> &Applies) {
  assert(Applies.empty() && "Expected empty vector to store into!");

  for (auto &B : *F)
    for (auto &I : B)
      if (auto Apply = ApplySite::isa(&I))
        Applies.push_back(Apply);
}

// Devirtualize and specialize a group of applies, updating the call
// graph and returning a worklist of newly exposed function references
// that should be considered for inlining before continuing with the
// caller that has the passed-in applies.
//
// The returned worklist is stacked such that the last things we want
// to process are earlier on the list.
//
// Returns true if any changes were made.
bool SILPerformanceInliner::devirtualizeAndSpecializeApplies(
                                  llvm::SmallVectorImpl<ApplySite> &Applies,
                                  CallGraphAnalysis *CGA,
                                  SILModuleTransform *MT,
                               llvm::SmallVectorImpl<SILFunction *> &WorkList) {
  assert(WorkList.empty() && "Expected empty worklist for return results!");

  auto &CG = CGA->getCallGraph();
  bool ChangedAny = false;

  // The set of all new function references generated by
  // devirtualization and specialization.
  llvm::SetVector<SILFunction *> NewRefs;

  // Process all applies passed in, plus any new ones that are pushed
  // on as a result of specializing the referenced functions.
  while (!Applies.empty()) {
    auto Apply = Applies.back();
    Applies.pop_back();

    bool ChangedApply = false;
    if (auto FullApply = FullApplySite::isa(Apply.getInstruction())) {
      if (auto NewApply = devirtualizeUpdatingCallGraph(FullApply, CG)) {
        ChangedApply = true;

        Apply = ApplySite(NewApply.getInstruction());
      }
    }

    llvm::SmallVector<ApplySite, 4> NewApplies;
    if (auto NewApply = specializeGenericUpdatingCallGraph(Apply, CG,
                                                           NewApplies)) {
      ChangedApply = true;

      Apply = NewApply;
      Applies.insert(Applies.end(), NewApplies.begin(), NewApplies.end());
    }

    if (ChangedApply) {
      ChangedAny = true;

      auto *NewCallee = Apply.getCalleeFunction();
      assert(NewCallee && "Expected directly referenced function!");

      // Track all new references to function definitions.
      if (NewCallee->isDefinition())
        NewRefs.insert(NewCallee);


      // TODO: Do we need to invalidate everything at this point?
      // What about side-effects analysis? What about type analysis?
      CGA->lockInvalidation();
      MT->invalidateAnalysis(Apply.getFunction(),
                             SILAnalysis::InvalidationKind::Everything);
      CGA->unlockInvalidation();
    }
  }

  // Copy out all the new function references gathered.
  if (ChangedAny)
    WorkList.insert(WorkList.end(), NewRefs.begin(), NewRefs.end());

  return ChangedAny;
}

void SILPerformanceInliner::collectAppliesToInline(
    SILFunction *Caller, SmallVectorImpl<FullApplySite> &Applies,
    DominanceAnalysis *DA, SILLoopAnalysis *LA) {
  DominanceInfo *DT = DA->get(Caller);
  SILLoopInfo *LI = LA->get(Caller);

  ConstantTracker constTracker(Caller);
  DominanceOrder domOrder(&Caller->front(), DT, Caller->size());

  // Go through all instructions and find candidates for inlining.
  // We do this in dominance order for the constTracker.
  SmallVector<FullApplySite, 8> InitialCandidates;
  while (SILBasicBlock *block = domOrder.getNext()) {
    constTracker.beginBlock();
    unsigned loopDepth = LI->getLoopDepth(block);
    for (auto I = block->begin(), E = block->end(); I != E; ++I) {
      constTracker.trackInst(&*I);

      if (!FullApplySite::isa(&*I))
        continue;

      FullApplySite AI = FullApplySite(&*I);

      DEBUG(llvm::dbgs() << "    Check:" << *I);

      auto *Callee = getEligibleFunction(AI);
      if (Callee) {
        if (isProfitableToInline(AI, loopDepth, DA, LA, constTracker))
          InitialCandidates.push_back(AI);
      }
    }
    domOrder.pushChildrenIf(block, [&] (SILBasicBlock *child) {
      if (ColdBlockInfo::isSlowPath(block, child)) {
        // Handle cold blocks separately.
        visitColdBlocks(InitialCandidates, child, DT);
        return false;
      }
      return true;
    });
  }

  // Calculate how many times a callee is called from this caller.
  llvm::DenseMap<SILFunction *, unsigned> CalleeCount;
  for (auto AI : InitialCandidates) {
    SILFunction *Callee = AI.getCalleeFunction();
    assert(Callee && "apply_inst does not have a direct callee anymore");
    CalleeCount[Callee]++;
  }

  // Now copy each candidate callee that has a small enough number of
  // call sites into the final set of call sites.
  for (auto AI : InitialCandidates) {
    SILFunction *Callee = AI.getCalleeFunction();
    assert(Callee && "apply_inst does not have a direct callee anymore");

    const unsigned CallsToCalleeThreshold = 1024;
    if (CalleeCount[Callee] <= CallsToCalleeThreshold)
      Applies.push_back(AI);
  }
}

/// \brief Attempt to inline all calls smaller than our threshold.
/// returns True if a function was inlined.
bool SILPerformanceInliner::inlineCallsIntoFunction(SILFunction *Caller,
                                                    DominanceAnalysis *DA,
                                                    SILLoopAnalysis *LA,
                                                    CallGraph &CG,
                             llvm::SmallVectorImpl<FullApplySite> &NewApplies) {
  // Don't optimize functions that are marked with the opt.never attribute.
  if (!Caller->shouldOptimize())
    return false;

  // Construct a log of all of the names of the functions that we've inlined
  // in the current iteration.
  SmallVector<StringRef, 16> InlinedFunctionNames;
  StringRef CallerName = Caller->getName();

  DEBUG(llvm::dbgs() << "Visiting Function: " << CallerName << "\n");

  assert(NewApplies.empty() && "Expected empty vector to store results in!");

  // First step: collect all the functions we want to inline.  We
  // don't change anything yet so that the dominator information
  // remains valid.
  SmallVector<FullApplySite, 8> AppliesToInline;
  collectAppliesToInline(Caller, AppliesToInline, DA, LA);

  if (AppliesToInline.empty())
    return false;

  // Second step: do the actual inlining.
  for (auto AI : AppliesToInline) {
    SILFunction *Callee = AI.getCalleeFunction();
    assert(Callee && "apply_inst does not have a direct callee anymore");

    DEBUG(llvm::dbgs() << "    Inline:" <<  *AI.getInstruction());

    if (!Callee->shouldOptimize()) {
      DEBUG(llvm::dbgs() << "    Cannot inline function " << Callee->getName()
                         << " marked to be excluded from optimizations.\n");
      continue;
    }
    
    SmallVector<SILValue, 8> Args;
    for (const auto &Arg : AI.getArguments())
      Args.push_back(Arg);
    
    // As we inline and clone we need to collect instructions that
    // require updates to the call graph.
    auto Filter = [](SILInstruction *I) -> bool {
      return I->mayRelease();
    };

    CloneCollector Collector(Filter);

    // Notice that we will skip all of the newly inlined ApplyInsts. That's
    // okay because we will visit them in our next invocation of the inliner.
    TypeSubstitutionMap ContextSubs;
    SILInliner Inliner(*Caller, *Callee,
                       SILInliner::InlineKind::PerformanceInline,
                       ContextSubs, AI.getSubstitutions(),
                       Collector.getCallback());

    // Record the name of the inlined function (for cycle detection).
    InlinedFunctionNames.push_back(Callee->getName());

    auto Success = Inliner.inlineFunction(AI, Args);
    (void) Success;
    // We've already determined we should be able to inline this, so
    // we expect it to have happened.
    assert(Success && "Expected inliner to inline this function!");
    llvm::SmallVector<FullApplySite, 4> AppliesFromInlinee;
    llvm::SmallVector<SILInstruction *, 4> NewCallSites;
    for (auto &P : Collector.getInstructionPairs()) {
      NewCallSites.push_back(P.first);

      if (auto FullApply = FullApplySite::isa(P.first)) {
        AppliesFromInlinee.push_back(FullApply);
      }
    }

    CallGraphEditor Editor(&CG);
    Editor.replaceApplyWithCallSites(AI, NewCallSites);

    recursivelyDeleteTriviallyDeadInstructions(AI.getInstruction(), true);

    NewApplies.insert(NewApplies.end(), AppliesFromInlinee.begin(),
                      AppliesFromInlinee.end());
    DA->invalidate(Caller, SILAnalysis::InvalidationKind::Everything);
    NumFunctionsInlined++;
  }

  // Record the names of the functions that we inlined.
  // We'll use this list to detect cycles in future iterations of
  // the inliner.
  for (auto CalleeName : InlinedFunctionNames) {
    InlinedFunctions.insert(std::make_pair(CallerName, CalleeName));
  }

  DEBUG(llvm::dbgs() << "\n");
  return true;
}

void SILPerformanceInliner::inlineDevirtualizeAndSpecialize(
                                                          SILFunction *Caller,
                                                        SILModuleTransform *MT,
                                                         CallGraphAnalysis *CGA,
                                                          DominanceAnalysis *DA,
                                                          SILLoopAnalysis *LA) {
  assert(Caller->isDefinition() &&
         "Expected only defined functions in the call graph!");

  llvm::SmallVector<SILFunction *, 4> WorkList;
  WorkList.push_back(Caller);

  auto &CG = CGA->getOrBuildCallGraph();

  while (!WorkList.empty()) {
    llvm::SmallVector<ApplySite, 4> WorkItemApplies;
    SILFunction *CurrentCaller = WorkList.back();
    if (CurrentCaller->shouldOptimize())
      collectAllAppliesInFunction(CurrentCaller, WorkItemApplies);

    // Devirtualize and specialize any applies we've collected,
    // and collect new functions we should inline into as we do
    // so.
    llvm::SmallVector<SILFunction *, 4> NewFuncs;
    if (devirtualizeAndSpecializeApplies(WorkItemApplies, CGA, MT, NewFuncs)) {
      WorkList.insert(WorkList.end(), NewFuncs.begin(), NewFuncs.end());
      NewFuncs.clear();
    }
    assert(WorkItemApplies.empty() && "Expected all applies to be processed!");

    // We want to inline into each function on the worklist, starting
    // with any new ones that were exposed as a result of
    // devirtualization (to insure we're inlining into callees first).
    //
    // After inlining, we may have new opportunities for
    // devirtualization, e.g. as a result of exposing the dynamic type
    // of an object. When those opportunities arise we want to attempt
    // devirtualization and then again attempt to inline into the
    // newly exposed functions, etc. until we're back to the function
    // we began with.
    auto *Initial = WorkList.back();

    // In practice we rarely exceed 5, but in a perf test we iterate 51 times.
    const unsigned MaxLaps = 1500;
    unsigned Lap = 0;
    while (1) {
      auto *WorkItem = WorkList.back();
      assert(WorkItem->isDefinition() &&
        "Expected function definition on work list!");

      // Devirtualization and specialization might have exposed new
      // function references. We want to inline within those functions
      // before inlining within our original function.
      //
      // Inlining in turn might result in new applies that we should
      // consider for devirtualization and specialization.
      llvm::SmallVector<FullApplySite, 4> NewApplies;
      bool Inlined = inlineCallsIntoFunction(WorkItem, DA, LA, CG, NewApplies);
      if (Inlined) {
        // Invalidate analyses, but lock the call graph since we
        // maintain it.
        CGA->lockInvalidation();
        MT->invalidateAnalysis(WorkItem,
                               SILAnalysis::InvalidationKind::FunctionBody);
        CGA->unlockInvalidation();

        // FIXME: Update inlineCallsIntoFunction to collect all
        //        remaining applies after inlining, not just those
        //        resulting from inlining code.
        llvm::SmallVector<ApplySite, 4> WorkItemApplies;
        collectAllAppliesInFunction(WorkItem, WorkItemApplies);

        bool Modified = devirtualizeAndSpecializeApplies(WorkItemApplies, CGA,
                                                         MT, NewFuncs);
        if (Modified) {
          WorkList.insert(WorkList.end(), NewFuncs.begin(), NewFuncs.end());
          NewFuncs.clear();
          assert(WorkItemApplies.empty() &&
                 "Expected all applies to be processed!");
        } else if (WorkItem == Initial) {
          // We did not specialize generics or devirtualize calls and we
          // did not create new opportunities so we can bail out now.
         break;
        } else {
          WorkList.pop_back();
        }
      } else if (WorkItem == Initial) {
        // We did not inline any calls and did not create new opportunities
        // so we can bail out now.
        break;
      } else {
        WorkList.pop_back();
      }

      Lap++;
      // It's possible to construct real code where this will hit, but
      // it's more likely that there is an issue tracking recursive
      // inlining, in which case we want to know about it in internal
      // builds, and not hang on bots or user machines.
      assert(Lap <= MaxLaps && "Possible bug tracking recursion!");
      // Give up and move along.
      if (Lap > MaxLaps) {
        while (WorkList.back() != Initial)
          WorkList.pop_back();
        break;
      }
    }

    assert(WorkList.back() == Initial &&
           "Expected to exit with same element on top of stack!" );
    WorkList.pop_back();
  }
}

// Find functions in cold blocks which are forced to be inlined.
// All other functions are not inlined in cold blocks.
void SILPerformanceInliner::visitColdBlocks(
    SmallVectorImpl<FullApplySite> &AppliesToInline, SILBasicBlock *Root,
    DominanceInfo *DT) {
  DominanceOrder domOrder(Root, DT);
  while (SILBasicBlock *block = domOrder.getNext()) {
    for (SILInstruction &I : *block) {
      ApplyInst *AI = dyn_cast<ApplyInst>(&I);
      if (!AI)
        continue;

      auto *Callee = getEligibleFunction(AI);
      if (Callee && isProfitableInColdBlock(Callee)) {
        DEBUG(llvm::dbgs() << "    inline in cold block:" <<  *AI);
        AppliesToInline.push_back(AI);
      }
    }
    domOrder.pushChildren(block);
  }
}


//===----------------------------------------------------------------------===//
//                          Performane Inliner Pass
//===----------------------------------------------------------------------===//

namespace {
class SILPerformanceInlinerPass : public SILModuleTransform {
  /// Specifies which functions not to inline, based on @_semantics and
  /// global_init attributes.
  InlineSelection WhatToInline;
  std::string PassName;
public:
  SILPerformanceInlinerPass(InlineSelection WhatToInline, StringRef LevelName):
    WhatToInline(WhatToInline), PassName(LevelName) {
    PassName.append(" Performance Inliner");
  }

  void run() override {
    BasicCalleeAnalysis *BCA = PM->getAnalysis<BasicCalleeAnalysis>();
    CallGraphAnalysis *CGA = PM->getAnalysis<CallGraphAnalysis>();
    DominanceAnalysis *DA = PM->getAnalysis<DominanceAnalysis>();
    SILLoopAnalysis *LA = PM->getAnalysis<SILLoopAnalysis>();

    if (getOptions().InlineThreshold == 0) {
      DEBUG(llvm::dbgs() << "*** The Performance Inliner is disabled ***\n");
      return;
    }

    SILPerformanceInliner Inliner(getOptions().InlineThreshold,
                                  WhatToInline);

    BottomUpFunctionOrder BottomUpOrder(*getModule(), BCA);
    auto BottomUpFunctions = BottomUpOrder.getFunctions();

    // Copy the bottom-up function list into a worklist.
    llvm::SmallVector<SILFunction *, 32> WorkList;
    // FIXME: std::reverse_copy would be better, but it crashes.
    for (auto I = BottomUpFunctions.rbegin(), E = BottomUpFunctions.rend();
         I != E; ++I)
      if ((*I)->isDefinition())
        WorkList.push_back(*I);

    // Inline functions bottom up from the leafs.
    while (!WorkList.empty()) {
      Inliner.inlineDevirtualizeAndSpecialize(WorkList.back(), this, CGA, DA,
                                              LA);
      WorkList.pop_back();
    }
  }

  StringRef getName() override { return PassName; }
};
} // end anonymous namespace

/// Create an inliner pass that does not inline functions that are marked with
/// the @_semantics, @effects or global_init attributes.
SILTransform *swift::createEarlyInliner() {
  return new SILPerformanceInlinerPass(
    InlineSelection::NoSemanticsAndGlobalInit, "Early");
}

/// Create an inliner pass that does not inline functions that are marked with
/// the global_init attribute or have an "availability" semantics attribute.
SILTransform *swift::createPerfInliner() {
  return new SILPerformanceInlinerPass(InlineSelection::NoGlobalInit, "Middle");
}

/// Create an inliner pass that inlines all functions that are marked with
/// the @_semantics, @effects or global_init attributes.
SILTransform *swift::createLateInliner() {
  return new SILPerformanceInlinerPass(InlineSelection::Everything, "Late");
}
