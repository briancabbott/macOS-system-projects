//===-- DwarfEHPrepare - Prepare exception handling for code generation ---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass mulches exception handling code into a form adapted to code
// generation. Required if using dwarf exception handling.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "dwarfehprepare"
#include "llvm/Function.h"
#include "llvm/Instructions.h"
#include "llvm/IntrinsicInst.h"
#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/Target/TargetLowering.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/PromoteMemToReg.h"
using namespace llvm;

STATISTIC(NumLandingPadsSplit,     "Number of landing pads split");
STATISTIC(NumUnwindsLowered,       "Number of unwind instructions lowered");
STATISTIC(NumExceptionValuesMoved, "Number of eh.exception calls moved");
STATISTIC(NumStackTempsIntroduced, "Number of stack temporaries introduced");

namespace {
  class DwarfEHPrepare : public FunctionPass {
    const TargetLowering *TLI;
    bool CompileFast;

    // The eh.exception intrinsic.
    Function *ExceptionValueIntrinsic;

    // The eh.selector intrinsic.
    Function *SelectorIntrinsic;

    // _Unwind_Resume_or_Rethrow call.
    Constant *URoR;

    // The EH language-specific catch-all type.
    GlobalVariable *EHCatchAllValue;

    // _Unwind_Resume or the target equivalent.
    Constant *RewindFunction;

    // Dominator info is used when turning stack temporaries into registers.
    DominatorTree *DT;
    DominanceFrontier *DF;

    // The function we are running on.
    Function *F;

    // The landing pads for this function.
    typedef SmallPtrSet<BasicBlock*, 8> BBSet;
    BBSet LandingPads;

    // Stack temporary used to hold eh.exception values.
    AllocaInst *ExceptionValueVar;

    bool NormalizeLandingPads();
    bool LowerUnwinds();
    bool MoveExceptionValueCalls();
    bool FinishStackTemporaries();
    bool PromoteStackTemporaries();

    Instruction *CreateExceptionValueCall(BasicBlock *BB);
    Instruction *CreateValueLoad(BasicBlock *BB);

    /// CreateReadOfExceptionValue - Return the result of the eh.exception
    /// intrinsic by calling the intrinsic if in a landing pad, or loading it
    /// from the exception value variable otherwise.
    Instruction *CreateReadOfExceptionValue(BasicBlock *BB) {
      return LandingPads.count(BB) ?
        CreateExceptionValueCall(BB) : CreateValueLoad(BB);
    }

    /// CleanupSelectors - Any remaining eh.selector intrinsic calls which still
    /// use the "llvm.eh.catch.all.value" call need to convert to using its
    /// initializer instead.
    bool CleanupSelectors();

    bool HasCatchAllInSelector(IntrinsicInst *);

    /// FindAllCleanupSelectors - Find all eh.selector calls that are clean-ups.
    void FindAllCleanupSelectors(SmallPtrSet<IntrinsicInst*, 32> &Sels);

    /// FindAllURoRInvokes - Find all URoR invokes in the function.
    void FindAllURoRInvokes(SmallPtrSet<InvokeInst*, 32> &URoRInvokes);

    /// HandleURoRInvokes - Handle invokes of "_Unwind_Resume_or_Rethrow"
    /// calls. The "unwind" part of these invokes jump to a landing pad within
    /// the current function. This is a candidate to merge the selector
    /// associated with the URoR invoke with the one from the URoR's landing
    /// pad.
    bool HandleURoRInvokes();

    /// FindSelectorAndURoR - Find the eh.selector call and URoR call associated
    /// with the eh.exception call. This recursively looks past instructions
    /// which don't change the EH pointer value, like casts or PHI nodes.
    bool FindSelectorAndURoR(Instruction *Inst, bool &URoRInvoke,
                             SmallPtrSet<IntrinsicInst*, 8> &SelCalls,
                             SmallPtrSet<PHINode*, 32> &SeenPHIs);
      
    /// DoMem2RegPromotion - Take an alloca call and promote it from memory to a
    /// register.
    bool DoMem2RegPromotion(Value *V) {
      AllocaInst *AI = dyn_cast<AllocaInst>(V);
      if (!AI || !isAllocaPromotable(AI)) return false;

      // Turn the alloca into a register.
      std::vector<AllocaInst*> Allocas(1, AI);
      PromoteMemToReg(Allocas, *DT, *DF);
      return true;
    }

    /// PromoteStoreInst - Perform Mem2Reg on a StoreInst.
    bool PromoteStoreInst(StoreInst *SI) {
      if (!SI || !DT || !DF) return false;
      if (DoMem2RegPromotion(SI->getOperand(1)))
        return true;
      return false;
    }

    /// PromoteEHPtrStore - Promote the storing of an EH pointer into a
    /// register. This should get rid of the store and subsequent loads.
    bool PromoteEHPtrStore(IntrinsicInst *II) {
      if (!DT || !DF) return false;

      bool Changed = false;
      StoreInst *SI;

      while (1) {
        SI = 0;
        for (Value::use_iterator
               I = II->use_begin(), E = II->use_end(); I != E; ++I) {
          SI = dyn_cast<StoreInst>(I);
          if (SI) break;
        }

        if (!PromoteStoreInst(SI))
          break;

        Changed = true;
      }

      return false;
    }

  public:
    static char ID; // Pass identification, replacement for typeid.
    DwarfEHPrepare(const TargetLowering *tli, bool fast) :
      FunctionPass(&ID), TLI(tli), CompileFast(fast),
      ExceptionValueIntrinsic(0), SelectorIntrinsic(0),
      URoR(0), EHCatchAllValue(0), RewindFunction(0) {}

    virtual bool runOnFunction(Function &Fn);

    // getAnalysisUsage - We need dominance frontiers for memory promotion.
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      if (!CompileFast)
        AU.addRequired<DominatorTree>();
      AU.addPreserved<DominatorTree>();
      if (!CompileFast)
        AU.addRequired<DominanceFrontier>();
      AU.addPreserved<DominanceFrontier>();
    }

    const char *getPassName() const {
      return "Exception handling preparation";
    }

  };
} // end anonymous namespace

char DwarfEHPrepare::ID = 0;

FunctionPass *llvm::createDwarfEHPass(const TargetLowering *tli, bool fast) {
  return new DwarfEHPrepare(tli, fast);
}

/// HasCatchAllInSelector - Return true if the intrinsic instruction has a
/// catch-all.
bool DwarfEHPrepare::HasCatchAllInSelector(IntrinsicInst *II) {
  if (!EHCatchAllValue) return false;

  unsigned OpIdx = II->getNumOperands() - 1;
  GlobalVariable *GV = dyn_cast<GlobalVariable>(II->getOperand(OpIdx));
  return GV == EHCatchAllValue;
}

/// FindAllCleanupSelectors - Find all eh.selector calls that are clean-ups.
void DwarfEHPrepare::
FindAllCleanupSelectors(SmallPtrSet<IntrinsicInst*, 32> &Sels) {
  for (Value::use_iterator
         I = SelectorIntrinsic->use_begin(),
         E = SelectorIntrinsic->use_end(); I != E; ++I) {
    IntrinsicInst *II = cast<IntrinsicInst>(I);

    if (II->getParent()->getParent() != F)
      continue;

    if (!HasCatchAllInSelector(II))
      Sels.insert(II);
  }
}

/// FindAllURoRInvokes - Find all URoR invokes in the function.
void DwarfEHPrepare::
FindAllURoRInvokes(SmallPtrSet<InvokeInst*, 32> &URoRInvokes) {
  for (Value::use_iterator
         I = URoR->use_begin(),
         E = URoR->use_end(); I != E; ++I) {
    if (InvokeInst *II = dyn_cast<InvokeInst>(I))
      URoRInvokes.insert(II);
  }
}

/// CleanupSelectors - Any remaining eh.selector intrinsic calls which still use
/// the "llvm.eh.catch.all.value" call need to convert to using its
/// initializer instead.
bool DwarfEHPrepare::CleanupSelectors() {
  if (!EHCatchAllValue) return false;

  if (!SelectorIntrinsic) {
    SelectorIntrinsic =
      Intrinsic::getDeclaration(F->getParent(), Intrinsic::eh_selector);
    if (!SelectorIntrinsic) return false;
  }

  bool Changed = false;
  for (Value::use_iterator
         I = SelectorIntrinsic->use_begin(),
         E = SelectorIntrinsic->use_end(); I != E; ++I) {
    IntrinsicInst *Sel = dyn_cast<IntrinsicInst>(I);
    if (!Sel || Sel->getParent()->getParent() != F) continue;

    // Index of the "llvm.eh.catch.all.value" variable.
    unsigned OpIdx = Sel->getNumOperands() - 1;
    GlobalVariable *GV = dyn_cast<GlobalVariable>(Sel->getOperand(OpIdx));
    if (GV != EHCatchAllValue) continue;
    Sel->setOperand(OpIdx, EHCatchAllValue->getInitializer());
    Changed = true;
  }

  return Changed;
}

/// FindSelectorAndURoR - Find the eh.selector call associated with the
/// eh.exception call. And indicate if there is a URoR "invoke" associated with
/// the eh.exception call. This recursively looks past instructions which don't
/// change the EH pointer value, like casts or PHI nodes.
bool
DwarfEHPrepare::FindSelectorAndURoR(Instruction *Inst, bool &URoRInvoke,
                                    SmallPtrSet<IntrinsicInst*, 8> &SelCalls,
                                    SmallPtrSet<PHINode*, 32> &SeenPHIs) {
  bool Changed = false;

 restart:
  for (Value::use_iterator
         I = Inst->use_begin(), E = Inst->use_end(); I != E; ++I) {
    Instruction *II = dyn_cast<Instruction>(I);
    if (!II || II->getParent()->getParent() != F) continue;
    
    if (IntrinsicInst *Sel = dyn_cast<IntrinsicInst>(II)) {
      if (Sel->getIntrinsicID() == Intrinsic::eh_selector)
        SelCalls.insert(Sel);
    } else if (InvokeInst *Invoke = dyn_cast<InvokeInst>(II)) {
      if (Invoke->getCalledFunction() == URoR)
        URoRInvoke = true;
    } else if (CastInst *CI = dyn_cast<CastInst>(II)) {
      Changed |= FindSelectorAndURoR(CI, URoRInvoke, SelCalls, SeenPHIs);
    } else if (StoreInst *SI = dyn_cast<StoreInst>(II)) {
      if (!PromoteStoreInst(SI)) continue;
      Changed = true;
      SeenPHIs.clear();
      goto restart;             // Uses may have changed, restart loop.
    } else if (PHINode *PN = dyn_cast<PHINode>(II)) {
      if (SeenPHIs.insert(PN))
        // Don't process a PHI node more than once.
        Changed |= FindSelectorAndURoR(PN, URoRInvoke, SelCalls, SeenPHIs);
    }
  }

  return Changed;
}

/// HandleURoRInvokes - Handle invokes of "_Unwind_Resume_or_Rethrow" calls. The
/// "unwind" part of these invokes jump to a landing pad within the current
/// function. This is a candidate to merge the selector associated with the URoR
/// invoke with the one from the URoR's landing pad.
bool DwarfEHPrepare::HandleURoRInvokes() {
  if (!DT) return CleanupSelectors(); // We require DominatorTree information.

  if (!EHCatchAllValue) {
    EHCatchAllValue =
      F->getParent()->getNamedGlobal("llvm.eh.catch.all.value");
    if (!EHCatchAllValue) return false;
  }

  if (!SelectorIntrinsic) {
    SelectorIntrinsic =
      Intrinsic::getDeclaration(F->getParent(), Intrinsic::eh_selector);
    if (!SelectorIntrinsic) return false;
  }

  if (!URoR) {
    URoR = F->getParent()->getFunction("_Unwind_Resume_or_Rethrow");
    if (!URoR) return CleanupSelectors();
  }

  SmallPtrSet<IntrinsicInst*, 32> Sels;
  SmallPtrSet<InvokeInst*, 32> URoRInvokes;
  FindAllCleanupSelectors(Sels);
  FindAllURoRInvokes(URoRInvokes);

  SmallPtrSet<IntrinsicInst*, 32> SelsToConvert;

  for (SmallPtrSet<IntrinsicInst*, 32>::iterator
         SI = Sels.begin(), SE = Sels.end(); SI != SE; ++SI) {
    const BasicBlock *SelBB = (*SI)->getParent();
    for (SmallPtrSet<InvokeInst*, 32>::iterator
           UI = URoRInvokes.begin(), UE = URoRInvokes.end(); UI != UE; ++UI) {
      const BasicBlock *URoRBB = (*UI)->getParent();
      if (SelBB == URoRBB || DT->dominates(SelBB, URoRBB)) {
        SelsToConvert.insert(*SI);
        break;
      }
    }
  }

  bool Changed = false;

  if (Sels.size() != SelsToConvert.size()) {
    // If we haven't been able to convert all of the clean-up selectors, then
    // loop through the slow way to see if they still need to be converted.
    if (!ExceptionValueIntrinsic) {
      ExceptionValueIntrinsic =
        Intrinsic::getDeclaration(F->getParent(), Intrinsic::eh_exception);
      if (!ExceptionValueIntrinsic) return CleanupSelectors();
    }

    for (Value::use_iterator
           I = ExceptionValueIntrinsic->use_begin(),
           E = ExceptionValueIntrinsic->use_end(); I != E; ++I) {
      IntrinsicInst *EHPtr = dyn_cast<IntrinsicInst>(I);
      if (!EHPtr || EHPtr->getParent()->getParent() != F) continue;

      Changed |= PromoteEHPtrStore(EHPtr);

      bool URoRInvoke = false;
      SmallPtrSet<IntrinsicInst*, 8> SelCalls;
      SmallPtrSet<PHINode*, 32> SeenPHIs;
      Changed |= FindSelectorAndURoR(EHPtr, URoRInvoke, SelCalls, SeenPHIs);

      if (URoRInvoke) {
        // This EH pointer is being used by an invoke of an URoR instruction and
        // an eh.selector intrinsic call. If the eh.selector is a 'clean-up', we
        // need to convert it to a 'catch-all'.
        for (SmallPtrSet<IntrinsicInst*, 8>::iterator
               SI = SelCalls.begin(), SE = SelCalls.end(); SI != SE; ++SI)
          if (!HasCatchAllInSelector(*SI))
              SelsToConvert.insert(*SI);
      }
    }
  }

  if (!SelsToConvert.empty()) {
    // Convert all clean-up eh.selectors, which are associated with "invokes" of
    // URoR calls, into catch-all eh.selectors.
    Changed = true;

    for (SmallPtrSet<IntrinsicInst*, 8>::iterator
           SI = SelsToConvert.begin(), SE = SelsToConvert.end();
         SI != SE; ++SI) {
      IntrinsicInst *II = *SI;
      SmallVector<Value*, 8> Args;

      // Use the exception object pointer and the personality function
      // from the original selector.
      Args.push_back(II->getOperand(1)); // Exception object pointer.
      Args.push_back(II->getOperand(2)); // Personality function.

      unsigned I = 3;
      unsigned E = II->getNumOperands() -
        (isa<ConstantInt>(II->getOperand(II->getNumOperands() - 1)) ? 1 : 0);

      // Add in any filter IDs.
      for (; I < E; ++I)
        Args.push_back(II->getOperand(I));

      Args.push_back(EHCatchAllValue->getInitializer()); // Catch-all indicator.

      CallInst *NewSelector =
        CallInst::Create(SelectorIntrinsic, Args.begin(), Args.end(),
                         "eh.sel.catch.all", II);

      NewSelector->setTailCall(II->isTailCall());
      NewSelector->setAttributes(II->getAttributes());
      NewSelector->setCallingConv(II->getCallingConv());

      II->replaceAllUsesWith(NewSelector);
      II->eraseFromParent();
    }
  }

  Changed |= CleanupSelectors();
  return Changed;
}

/// NormalizeLandingPads - Normalize and discover landing pads, noting them
/// in the LandingPads set.  A landing pad is normal if the only CFG edges
/// that end at it are unwind edges from invoke instructions. If we inlined
/// through an invoke we could have a normal branch from the previous
/// unwind block through to the landing pad for the original invoke.
/// Abnormal landing pads are fixed up by redirecting all unwind edges to
/// a new basic block which falls through to the original.
bool DwarfEHPrepare::NormalizeLandingPads() {
  bool Changed = false;

  const MCAsmInfo *MAI = TLI->getTargetMachine().getMCAsmInfo();
  bool usingSjLjEH = MAI->getExceptionHandlingType() == ExceptionHandling::SjLj;

  for (Function::iterator I = F->begin(), E = F->end(); I != E; ++I) {
    TerminatorInst *TI = I->getTerminator();
    if (!isa<InvokeInst>(TI))
      continue;
    BasicBlock *LPad = TI->getSuccessor(1);
    // Skip landing pads that have already been normalized.
    if (LandingPads.count(LPad))
      continue;

    // Check that only invoke unwind edges end at the landing pad.
    bool OnlyUnwoundTo = true;
    bool SwitchOK = usingSjLjEH;
    for (pred_iterator PI = pred_begin(LPad), PE = pred_end(LPad);
         PI != PE; ++PI) {
      TerminatorInst *PT = (*PI)->getTerminator();
      // The SjLj dispatch block uses a switch instruction. This is effectively
      // an unwind edge, so we can disregard it here. There will only ever
      // be one dispatch, however, so if there are multiple switches, one
      // of them truly is a normal edge, not an unwind edge.
      if (SwitchOK && isa<SwitchInst>(PT)) {
        SwitchOK = false;
        continue;
      }
      if (!isa<InvokeInst>(PT) || LPad == PT->getSuccessor(0)) {
        OnlyUnwoundTo = false;
        break;
      }
    }

    if (OnlyUnwoundTo) {
      // Only unwind edges lead to the landing pad.  Remember the landing pad.
      LandingPads.insert(LPad);
      continue;
    }

    // At least one normal edge ends at the landing pad.  Redirect the unwind
    // edges to a new basic block which falls through into this one.

    // Create the new basic block.
    BasicBlock *NewBB = BasicBlock::Create(F->getContext(),
                                           LPad->getName() + "_unwind_edge");

    // Insert it into the function right before the original landing pad.
    LPad->getParent()->getBasicBlockList().insert(LPad, NewBB);

    // Redirect unwind edges from the original landing pad to NewBB.
    for (pred_iterator PI = pred_begin(LPad), PE = pred_end(LPad); PI != PE; ) {
      TerminatorInst *PT = (*PI++)->getTerminator();
      if (isa<InvokeInst>(PT) && PT->getSuccessor(1) == LPad)
        // Unwind to the new block.
        PT->setSuccessor(1, NewBB);
    }

    // If there are any PHI nodes in LPad, we need to update them so that they
    // merge incoming values from NewBB instead.
    for (BasicBlock::iterator II = LPad->begin(); isa<PHINode>(II); ++II) {
      PHINode *PN = cast<PHINode>(II);
      pred_iterator PB = pred_begin(NewBB), PE = pred_end(NewBB);

      // Check to see if all of the values coming in via unwind edges are the
      // same.  If so, we don't need to create a new PHI node.
      Value *InVal = PN->getIncomingValueForBlock(*PB);
      for (pred_iterator PI = PB; PI != PE; ++PI) {
        if (PI != PB && InVal != PN->getIncomingValueForBlock(*PI)) {
          InVal = 0;
          break;
        }
      }

      if (InVal == 0) {
        // Different unwind edges have different values.  Create a new PHI node
        // in NewBB.
        PHINode *NewPN = PHINode::Create(PN->getType(), PN->getName()+".unwind",
                                         NewBB);
        // Add an entry for each unwind edge, using the value from the old PHI.
        for (pred_iterator PI = PB; PI != PE; ++PI)
          NewPN->addIncoming(PN->getIncomingValueForBlock(*PI), *PI);

        // Now use this new PHI as the common incoming value for NewBB in PN.
        InVal = NewPN;
      }

      // Revector exactly one entry in the PHI node to come from NewBB
      // and delete all other entries that come from unwind edges.  If
      // there are both normal and unwind edges from the same predecessor,
      // this leaves an entry for the normal edge.
      for (pred_iterator PI = PB; PI != PE; ++PI)
        PN->removeIncomingValue(*PI);
      PN->addIncoming(InVal, NewBB);
    }

    // Add a fallthrough from NewBB to the original landing pad.
    BranchInst::Create(LPad, NewBB);

    // Now update DominatorTree and DominanceFrontier analysis information.
    if (DT)
      DT->splitBlock(NewBB);
    if (DF)
      DF->splitBlock(NewBB);

    // Remember the newly constructed landing pad.  The original landing pad
    // LPad is no longer a landing pad now that all unwind edges have been
    // revectored to NewBB.
    LandingPads.insert(NewBB);
    ++NumLandingPadsSplit;
    Changed = true;
  }

  return Changed;
}

/// LowerUnwinds - Turn unwind instructions into calls to _Unwind_Resume,
/// rethrowing any previously caught exception.  This will crash horribly
/// at runtime if there is no such exception: using unwind to throw a new
/// exception is currently not supported.
bool DwarfEHPrepare::LowerUnwinds() {
  SmallVector<TerminatorInst*, 16> UnwindInsts;

  for (Function::iterator I = F->begin(), E = F->end(); I != E; ++I) {
    TerminatorInst *TI = I->getTerminator();
    if (isa<UnwindInst>(TI))
      UnwindInsts.push_back(TI);
  }

  if (UnwindInsts.empty()) return false;

  // Find the rewind function if we didn't already.
  if (!RewindFunction) {
    LLVMContext &Ctx = UnwindInsts[0]->getContext();
    std::vector<const Type*>
      Params(1, Type::getInt8PtrTy(Ctx));
    FunctionType *FTy = FunctionType::get(Type::getVoidTy(Ctx),
                                          Params, false);
    const char *RewindName = TLI->getLibcallName(RTLIB::UNWIND_RESUME);
    RewindFunction = F->getParent()->getOrInsertFunction(RewindName, FTy);
  }

  bool Changed = false;

  for (SmallVectorImpl<TerminatorInst*>::iterator
         I = UnwindInsts.begin(), E = UnwindInsts.end(); I != E; ++I) {
    TerminatorInst *TI = *I;

    // Replace the unwind instruction with a call to _Unwind_Resume (or the
    // appropriate target equivalent) followed by an UnreachableInst.

    // Create the call...
    CallInst *CI = CallInst::Create(RewindFunction,
                                    CreateReadOfExceptionValue(TI->getParent()),
                                    "", TI);
    CI->setCallingConv(TLI->getLibcallCallingConv(RTLIB::UNWIND_RESUME));
    // ...followed by an UnreachableInst.
    new UnreachableInst(TI->getContext(), TI);

    // Nuke the unwind instruction.
    TI->eraseFromParent();
    ++NumUnwindsLowered;
    Changed = true;
  }

  return Changed;
}

/// MoveExceptionValueCalls - Ensure that eh.exception is only ever called from
/// landing pads by replacing calls outside of landing pads with loads from a
/// stack temporary.  Move eh.exception calls inside landing pads to the start
/// of the landing pad (optional, but may make things simpler for later passes).
bool DwarfEHPrepare::MoveExceptionValueCalls() {
  // If the eh.exception intrinsic is not declared in the module then there is
  // nothing to do.  Speed up compilation by checking for this common case.
  if (!ExceptionValueIntrinsic &&
      !F->getParent()->getFunction(Intrinsic::getName(Intrinsic::eh_exception)))
    return false;

  bool Changed = false;

  for (Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB) {
    for (BasicBlock::iterator II = BB->begin(), E = BB->end(); II != E;)
      if (IntrinsicInst *CI = dyn_cast<IntrinsicInst>(II++))
        if (CI->getIntrinsicID() == Intrinsic::eh_exception) {
          if (!CI->use_empty()) {
            Value *ExceptionValue = CreateReadOfExceptionValue(BB);
            if (CI == ExceptionValue) {
              // The call was at the start of a landing pad - leave it alone.
              assert(LandingPads.count(BB) &&
                     "Created eh.exception call outside landing pad!");
              continue;
            }
            CI->replaceAllUsesWith(ExceptionValue);
          }
          CI->eraseFromParent();
          ++NumExceptionValuesMoved;
          Changed = true;
        }
  }

  return Changed;
}

/// FinishStackTemporaries - If we introduced a stack variable to hold the
/// exception value then initialize it in each landing pad.
bool DwarfEHPrepare::FinishStackTemporaries() {
  if (!ExceptionValueVar)
    // Nothing to do.
    return false;

  bool Changed = false;

  // Make sure that there is a store of the exception value at the start of
  // each landing pad.
  for (BBSet::iterator LI = LandingPads.begin(), LE = LandingPads.end();
       LI != LE; ++LI) {
    Instruction *ExceptionValue = CreateReadOfExceptionValue(*LI);
    Instruction *Store = new StoreInst(ExceptionValue, ExceptionValueVar);
    Store->insertAfter(ExceptionValue);
    Changed = true;
  }

  return Changed;
}

/// PromoteStackTemporaries - Turn any stack temporaries we introduced into
/// registers if possible.
bool DwarfEHPrepare::PromoteStackTemporaries() {
  if (ExceptionValueVar && DT && DF && isAllocaPromotable(ExceptionValueVar)) {
    // Turn the exception temporary into registers and phi nodes if possible.
    std::vector<AllocaInst*> Allocas(1, ExceptionValueVar);
    PromoteMemToReg(Allocas, *DT, *DF);
    return true;
  }
  return false;
}

/// CreateExceptionValueCall - Insert a call to the eh.exception intrinsic at
/// the start of the basic block (unless there already is one, in which case
/// the existing call is returned).
Instruction *DwarfEHPrepare::CreateExceptionValueCall(BasicBlock *BB) {
  Instruction *Start = BB->getFirstNonPHIOrDbg();
  // Is this a call to eh.exception?
  if (IntrinsicInst *CI = dyn_cast<IntrinsicInst>(Start))
    if (CI->getIntrinsicID() == Intrinsic::eh_exception)
      // Reuse the existing call.
      return Start;

  // Find the eh.exception intrinsic if we didn't already.
  if (!ExceptionValueIntrinsic)
    ExceptionValueIntrinsic = Intrinsic::getDeclaration(F->getParent(),
                                                       Intrinsic::eh_exception);

  // Create the call.
  return CallInst::Create(ExceptionValueIntrinsic, "eh.value.call", Start);
}

/// CreateValueLoad - Insert a load of the exception value stack variable
/// (creating it if necessary) at the start of the basic block (unless
/// there already is a load, in which case the existing load is returned).
Instruction *DwarfEHPrepare::CreateValueLoad(BasicBlock *BB) {
  Instruction *Start = BB->getFirstNonPHIOrDbg();
  // Is this a load of the exception temporary?
  if (ExceptionValueVar)
    if (LoadInst* LI = dyn_cast<LoadInst>(Start))
      if (LI->getPointerOperand() == ExceptionValueVar)
        // Reuse the existing load.
        return Start;

  // Create the temporary if we didn't already.
  if (!ExceptionValueVar) {
    ExceptionValueVar = new AllocaInst(PointerType::getUnqual(
           Type::getInt8Ty(BB->getContext())), "eh.value", F->begin()->begin());
    ++NumStackTempsIntroduced;
  }

  // Load the value.
  return new LoadInst(ExceptionValueVar, "eh.value.load", Start);
}

bool DwarfEHPrepare::runOnFunction(Function &Fn) {
  bool Changed = false;

  // Initialize internal state.
  DT = getAnalysisIfAvailable<DominatorTree>();
  DF = getAnalysisIfAvailable<DominanceFrontier>();
  ExceptionValueVar = 0;
  F = &Fn;

  // Ensure that only unwind edges end at landing pads (a landing pad is a
  // basic block where an invoke unwind edge ends).
  Changed |= NormalizeLandingPads();

  // Turn unwind instructions into libcalls.
  Changed |= LowerUnwinds();

  // TODO: Move eh.selector calls to landing pads and combine them.

  // Move eh.exception calls to landing pads.
  Changed |= MoveExceptionValueCalls();

  // Initialize any stack temporaries we introduced.
  Changed |= FinishStackTemporaries();

  // Turn any stack temporaries into registers if possible.
  if (!CompileFast)
    Changed |= PromoteStackTemporaries();

  Changed |= HandleURoRInvokes();

  LandingPads.clear();

  return Changed;
}
