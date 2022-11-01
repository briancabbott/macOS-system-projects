//===-- ARM64TargetObjectFile.cpp - ARM64 Object Info ---------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "ARM64TargetObjectFile.h"
#include "ARM64TargetMachine.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/Target/Mangler.h"
#include "llvm/Support/Dwarf.h"
using namespace llvm;
using namespace dwarf;

void ARM64_ELFTargetObjectFile::Initialize(MCContext &Ctx,
                                           const TargetMachine &TM) {
  TargetLoweringObjectFileELF::Initialize(Ctx, TM);
  InitializeELF(TM.Options.UseInitArray);
}

const MCExpr *ARM64_MachoTargetObjectFile::
getTTypeGlobalReference(const GlobalValue *GV, Mangler *Mang,
                        MachineModuleInfo *MMI, unsigned Encoding,
                        MCStreamer &Streamer) const {

  // On Darwin, we can reference dwarf symbols with foo@GOT-., which
  // is an indirect pc-relative reference. The default implementation
  // won't reference using the GOT, so we need this target-specific
  // version.
  if (Encoding & (DW_EH_PE_indirect | DW_EH_PE_pcrel)) {
    const MCSymbol *Sym = getSymbol(*Mang, GV);
    const MCExpr *Res =
      MCSymbolRefExpr::Create(Sym, MCSymbolRefExpr::VK_GOT, getContext());
    MCSymbol *PCSym = getContext().CreateTempSymbol();
    Streamer.EmitLabel(PCSym);
    const MCExpr *PC = MCSymbolRefExpr::Create(PCSym, getContext());
    return MCBinaryExpr::CreateSub(Res, PC, getContext());
  }

  return TargetLoweringObjectFileMachO::
    getTTypeGlobalReference(GV, Mang, MMI, Encoding, Streamer);
}

MCSymbol *ARM64_MachoTargetObjectFile::
getCFIPersonalitySymbol(const GlobalValue *GV, Mangler *Mang,
                        MachineModuleInfo *MMI) const {
  return getSymbol(*Mang, GV);
}
