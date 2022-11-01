//===-- MipsAsmParser.cpp - Parse Mips assembly to MCInst instructions ----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/MipsMCTargetDesc.h"
#include "MipsRegisterInfo.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/MCParser/MCAsmLexer.h"
#include "llvm/MC/MCParser/MCParsedAsmOperand.h"
#include "llvm/MC/MCTargetAsmParser.h"
#include "llvm/Support/TargetRegistry.h"

using namespace llvm;

namespace {

class MipsAsmParser : public MCTargetAsmParser {

  enum FpFormatTy {
    FP_FORMAT_NONE = -1,
    FP_FORMAT_S,
    FP_FORMAT_D,
    FP_FORMAT_L,
    FP_FORMAT_W
  } FpFormat;

  MCSubtargetInfo &STI;
  MCAsmParser &Parser;

#define GET_ASSEMBLER_HEADER
#include "MipsGenAsmMatcher.inc"

  bool MatchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                               SmallVectorImpl<MCParsedAsmOperand*> &Operands,
                               MCStreamer &Out, unsigned &ErrorInfo,
                               bool MatchingInlineAsm);

  bool ParseRegister(unsigned &RegNo, SMLoc &StartLoc, SMLoc &EndLoc);

  bool ParseInstruction(ParseInstructionInfo &Info, StringRef Name,
                        SMLoc NameLoc,
                        SmallVectorImpl<MCParsedAsmOperand*> &Operands);

  bool parseMathOperation(StringRef Name, SMLoc NameLoc,
                        SmallVectorImpl<MCParsedAsmOperand*> &Operands);

  bool ParseDirective(AsmToken DirectiveID);

  MipsAsmParser::OperandMatchResultTy
  parseMemOperand(SmallVectorImpl<MCParsedAsmOperand*>&);

  bool ParseOperand(SmallVectorImpl<MCParsedAsmOperand*> &,
                    StringRef Mnemonic);

  int tryParseRegister(StringRef Mnemonic);

  bool tryParseRegisterOperand(SmallVectorImpl<MCParsedAsmOperand*> &Operands,
                               StringRef Mnemonic);

  bool parseMemOffset(const MCExpr *&Res);
  bool parseRelocOperand(const MCExpr *&Res);
  MCSymbolRefExpr::VariantKind getVariantKind(StringRef Symbol);

  bool isMips64() const {
    return (STI.getFeatureBits() & Mips::FeatureMips64) != 0;
  }

  bool isFP64() const {
    return (STI.getFeatureBits() & Mips::FeatureFP64Bit) != 0;
  }

  int matchRegisterName(StringRef Symbol);

  int matchRegisterByNumber(unsigned RegNum, StringRef Mnemonic);

  void setFpFormat(FpFormatTy Format) {
    FpFormat = Format;
  }

  void setDefaultFpFormat();

  void setFpFormat(StringRef Format);

  FpFormatTy getFpFormat() {return FpFormat;}

  bool requestsDoubleOperand(StringRef Mnemonic);

  unsigned getReg(int RC,int RegNo);

public:
  MipsAsmParser(MCSubtargetInfo &sti, MCAsmParser &parser)
    : MCTargetAsmParser(), STI(sti), Parser(parser) {
    // Initialize the set of available features.
    setAvailableFeatures(ComputeAvailableFeatures(STI.getFeatureBits()));
  }

  MCAsmParser &getParser() const { return Parser; }
  MCAsmLexer &getLexer() const { return Parser.getLexer(); }

};
}

namespace {

/// MipsOperand - Instances of this class represent a parsed Mips machine
/// instruction.
class MipsOperand : public MCParsedAsmOperand {

  enum KindTy {
    k_CondCode,
    k_CoprocNum,
    k_Immediate,
    k_Memory,
    k_PostIndexRegister,
    k_Register,
    k_Token
  } Kind;

  MipsOperand(KindTy K) : MCParsedAsmOperand(), Kind(K) {}

  union {
    struct {
      const char *Data;
      unsigned Length;
    } Tok;

    struct {
      unsigned RegNum;
    } Reg;

    struct {
      const MCExpr *Val;
    } Imm;

    struct {
      unsigned Base;
      const MCExpr *Off;
    } Mem;
  };

  SMLoc StartLoc, EndLoc;

public:
  void addRegOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    Inst.addOperand(MCOperand::CreateReg(getReg()));
  }

  void addExpr(MCInst &Inst, const MCExpr *Expr) const{
    // Add as immediate when possible.  Null MCExpr = 0.
    if (Expr == 0)
      Inst.addOperand(MCOperand::CreateImm(0));
    else if (const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(Expr))
      Inst.addOperand(MCOperand::CreateImm(CE->getValue()));
    else
      Inst.addOperand(MCOperand::CreateExpr(Expr));
  }

  void addImmOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    const MCExpr *Expr = getImm();
    addExpr(Inst,Expr);
  }

  void addMemOperands(MCInst &Inst, unsigned N) const {
    assert(N == 2 && "Invalid number of operands!");

    Inst.addOperand(MCOperand::CreateReg(getMemBase()));

    const MCExpr *Expr = getMemOff();
    addExpr(Inst,Expr);
  }

  bool isReg() const { return Kind == k_Register; }
  bool isImm() const { return Kind == k_Immediate; }
  bool isToken() const { return Kind == k_Token; }
  bool isMem() const { return Kind == k_Memory; }

  StringRef getToken() const {
    assert(Kind == k_Token && "Invalid access!");
    return StringRef(Tok.Data, Tok.Length);
  }

  unsigned getReg() const {
    assert((Kind == k_Register) && "Invalid access!");
    return Reg.RegNum;
  }

  const MCExpr *getImm() const {
    assert((Kind == k_Immediate) && "Invalid access!");
    return Imm.Val;
  }

  unsigned getMemBase() const {
    assert((Kind == k_Memory) && "Invalid access!");
    return Mem.Base;
  }

  const MCExpr *getMemOff() const {
    assert((Kind == k_Memory) && "Invalid access!");
    return Mem.Off;
  }

  static MipsOperand *CreateToken(StringRef Str, SMLoc S) {
    MipsOperand *Op = new MipsOperand(k_Token);
    Op->Tok.Data = Str.data();
    Op->Tok.Length = Str.size();
    Op->StartLoc = S;
    Op->EndLoc = S;
    return Op;
  }

  static MipsOperand *CreateReg(unsigned RegNum, SMLoc S, SMLoc E) {
    MipsOperand *Op = new MipsOperand(k_Register);
    Op->Reg.RegNum = RegNum;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }

  static MipsOperand *CreateImm(const MCExpr *Val, SMLoc S, SMLoc E) {
    MipsOperand *Op = new MipsOperand(k_Immediate);
    Op->Imm.Val = Val;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }

  static MipsOperand *CreateMem(unsigned Base, const MCExpr *Off,
                                 SMLoc S, SMLoc E) {
    MipsOperand *Op = new MipsOperand(k_Memory);
    Op->Mem.Base = Base;
    Op->Mem.Off = Off;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }

  /// getStartLoc - Get the location of the first token of this operand.
  SMLoc getStartLoc() const { return StartLoc; }
  /// getEndLoc - Get the location of the last token of this operand.
  SMLoc getEndLoc() const { return EndLoc; }

  virtual void print(raw_ostream &OS) const {
    llvm_unreachable("unimplemented!");
  }
};
}

bool MipsAsmParser::
MatchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                        SmallVectorImpl<MCParsedAsmOperand*> &Operands,
                        MCStreamer &Out, unsigned &ErrorInfo,
                        bool MatchingInlineAsm) {
  MCInst Inst;
  unsigned MatchResult = MatchInstructionImpl(Operands, Inst, ErrorInfo,
                                              MatchingInlineAsm);

  switch (MatchResult) {
  default: break;
  case Match_Success: {
    Inst.setLoc(IDLoc);
    Out.EmitInstruction(Inst);
    return false;
  }
  case Match_MissingFeature:
    Error(IDLoc, "instruction requires a CPU feature not currently enabled");
    return true;
  case Match_InvalidOperand: {
    SMLoc ErrorLoc = IDLoc;
    if (ErrorInfo != ~0U) {
      if (ErrorInfo >= Operands.size())
        return Error(IDLoc, "too few operands for instruction");

      ErrorLoc = ((MipsOperand*)Operands[ErrorInfo])->getStartLoc();
      if (ErrorLoc == SMLoc()) ErrorLoc = IDLoc;
    }

    return Error(ErrorLoc, "invalid operand for instruction");
  }
  case Match_MnemonicFail:
    return Error(IDLoc, "invalid instruction");
  }
  return true;
}

int MipsAsmParser::matchRegisterName(StringRef Name) {

   int CC = StringSwitch<unsigned>(Name)
    .Case("zero",  Mips::ZERO)
    .Case("a0",  Mips::A0)
    .Case("a1",  Mips::A1)
    .Case("a2",  Mips::A2)
    .Case("a3",  Mips::A3)
    .Case("v0",  Mips::V0)
    .Case("v1",  Mips::V1)
    .Case("s0",  Mips::S0)
    .Case("s1",  Mips::S1)
    .Case("s2",  Mips::S2)
    .Case("s3",  Mips::S3)
    .Case("s4",  Mips::S4)
    .Case("s5",  Mips::S5)
    .Case("s6",  Mips::S6)
    .Case("s7",  Mips::S7)
    .Case("k0",  Mips::K0)
    .Case("k1",  Mips::K1)
    .Case("sp",  Mips::SP)
    .Case("fp",  Mips::FP)
    .Case("gp",  Mips::GP)
    .Case("ra",  Mips::RA)
    .Case("t0",  Mips::T0)
    .Case("t1",  Mips::T1)
    .Case("t2",  Mips::T2)
    .Case("t3",  Mips::T3)
    .Case("t4",  Mips::T4)
    .Case("t5",  Mips::T5)
    .Case("t6",  Mips::T6)
    .Case("t7",  Mips::T7)
    .Case("t8",  Mips::T8)
    .Case("t9",  Mips::T9)
    .Case("at",  Mips::AT)
    .Case("fcc0",  Mips::FCC0)
    .Default(-1);

  if (CC != -1) {
    //64 bit register in Mips are following 32 bit definitions.
    if (isMips64())
      CC++;
    return CC;
  }

  if (Name[0] == 'f') {
    StringRef NumString = Name.substr(1);
    unsigned IntVal;
    if( NumString.getAsInteger(10, IntVal))
      return -1; //not integer
    if (IntVal > 31)
      return -1;

    FpFormatTy Format = getFpFormat();

    if (Format == FP_FORMAT_S || Format == FP_FORMAT_W)
      return getReg(Mips::FGR32RegClassID, IntVal);
    if (Format == FP_FORMAT_D) {
      if(isFP64()) {
        return getReg(Mips::FGR64RegClassID, IntVal);
      }
      //only even numbers available as register pairs
      if (( IntVal > 31) || (IntVal%2 !=  0))
        return -1;
      return getReg(Mips::AFGR64RegClassID, IntVal/2);
    }
  }

  return -1;
}
void MipsAsmParser::setDefaultFpFormat() {

  if (isMips64() || isFP64())
    FpFormat = FP_FORMAT_D;
  else
    FpFormat = FP_FORMAT_S;
}

bool MipsAsmParser::requestsDoubleOperand(StringRef Mnemonic){

  bool IsDouble = StringSwitch<bool>(Mnemonic.lower())
    .Case("ldxc1", true)
    .Case("ldc1",  true)
    .Case("sdxc1", true)
    .Case("sdc1",  true)
    .Default(false);

  return IsDouble;
}
void MipsAsmParser::setFpFormat(StringRef Format) {

  FpFormat = StringSwitch<FpFormatTy>(Format.lower())
    .Case(".s",  FP_FORMAT_S)
    .Case(".d",  FP_FORMAT_D)
    .Case(".l",  FP_FORMAT_L)
    .Case(".w",  FP_FORMAT_W)
    .Default(FP_FORMAT_NONE);
}

unsigned MipsAsmParser::getReg(int RC,int RegNo){
  return *(getContext().getRegisterInfo().getRegClass(RC).begin() + RegNo);
}

int MipsAsmParser::matchRegisterByNumber(unsigned RegNum,StringRef Mnemonic) {

  if (Mnemonic.lower() == "rdhwr") {
    //at the moment only hwreg29 is supported
    if (RegNum != 29)
      return -1;
    return Mips::HWR29;
  }

  if (RegNum > 31)
    return -1;

  return getReg(Mips::CPURegsRegClassID,RegNum);
}

int MipsAsmParser::tryParseRegister(StringRef Mnemonic) {
  const AsmToken &Tok = Parser.getTok();
  int RegNum = -1;

  if (Tok.is(AsmToken::Identifier)) {
    std::string lowerCase = Tok.getString().lower();
    RegNum = matchRegisterName(lowerCase);
  } else if (Tok.is(AsmToken::Integer))
    RegNum = matchRegisterByNumber(static_cast<unsigned> (Tok.getIntVal()),
                                   Mnemonic.lower());
    else
      return RegNum;  //error
  //64 bit div operations require Mips::ZERO instead of MIPS::ZERO_64
  if (isMips64() && RegNum == Mips::ZERO_64) {
    if (Mnemonic.find("ddiv") != StringRef::npos)
      RegNum = Mips::ZERO;
  }
  return RegNum;
}

bool MipsAsmParser::
  tryParseRegisterOperand(SmallVectorImpl<MCParsedAsmOperand*> &Operands,
                          StringRef Mnemonic){

  SMLoc S = Parser.getTok().getLoc();
  int RegNo = -1;

  //FIXME: we should make a more generic method for CCR
  if ((Mnemonic == "cfc1" || Mnemonic == "ctc1")
      && Operands.size() == 2 && Parser.getTok().is(AsmToken::Integer)){
    RegNo = Parser.getTok().getIntVal();  //get the int value
    //at the moment only fcc0 is supported
    if (RegNo ==  0)
      RegNo = Mips::FCC0;
  } else
    RegNo = tryParseRegister(Mnemonic);
  if (RegNo == -1)
    return true;

  Operands.push_back(MipsOperand::CreateReg(RegNo, S,
      Parser.getTok().getLoc()));
  Parser.Lex(); // Eat register token.
  return false;
}

bool MipsAsmParser::ParseOperand(SmallVectorImpl<MCParsedAsmOperand*>&Operands,
                                 StringRef Mnemonic) {
  //Check if the current operand has a custom associated parser, if so, try to
  //custom parse the operand, or fallback to the general approach.
  OperandMatchResultTy ResTy = MatchOperandParserImpl(Operands, Mnemonic);
  if (ResTy == MatchOperand_Success)
    return false;
  // If there wasn't a custom match, try the generic matcher below. Otherwise,
  // there was a match, but an error occurred, in which case, just return that
  // the operand parsing failed.
  if (ResTy == MatchOperand_ParseFail)
    return true;

  switch (getLexer().getKind()) {
  default:
    Error(Parser.getTok().getLoc(), "unexpected token in operand");
    return true;
  case AsmToken::Dollar: {
    //parse register
    SMLoc S = Parser.getTok().getLoc();
    Parser.Lex(); // Eat dollar token.
    //parse register operand
    if (!tryParseRegisterOperand(Operands,Mnemonic)) {
      if (getLexer().is(AsmToken::LParen)) {
        //check if it is indexed addressing operand
        Operands.push_back(MipsOperand::CreateToken("(", S));
        Parser.Lex(); //eat parenthesis
        if (getLexer().isNot(AsmToken::Dollar))
          return true;

        Parser.Lex(); //eat dollar
        if (tryParseRegisterOperand(Operands,Mnemonic))
          return true;

        if (!getLexer().is(AsmToken::RParen))
          return true;

        S = Parser.getTok().getLoc();
        Operands.push_back(MipsOperand::CreateToken(")", S));
        Parser.Lex();
      }
      return false;
    }
    //maybe it is a symbol reference
    StringRef Identifier;
    if (Parser.ParseIdentifier(Identifier))
      return true;

    SMLoc E = SMLoc::getFromPointer(Parser.getTok().getLoc().getPointer() - 1);

    MCSymbol *Sym = getContext().GetOrCreateSymbol("$" + Identifier);

    // Otherwise create a symbol ref.
    const MCExpr *Res = MCSymbolRefExpr::Create(Sym, MCSymbolRefExpr::VK_None,
                                                getContext());

    Operands.push_back(MipsOperand::CreateImm(Res, S, E));
    return false;
  }
  case AsmToken::Identifier:
  case AsmToken::LParen:
  case AsmToken::Minus:
  case AsmToken::Plus:
  case AsmToken::Integer:
  case AsmToken::String: {
     // quoted label names
    const MCExpr *IdVal;
    SMLoc S = Parser.getTok().getLoc();
    if (getParser().ParseExpression(IdVal))
      return true;
    SMLoc E = SMLoc::getFromPointer(Parser.getTok().getLoc().getPointer() - 1);
    Operands.push_back(MipsOperand::CreateImm(IdVal, S, E));
    return false;
  }
  case AsmToken::Percent: {
    //it is a symbol reference or constant expression
    const MCExpr *IdVal;
    SMLoc S = Parser.getTok().getLoc(); //start location of the operand
    if (parseRelocOperand(IdVal))
      return true;

    SMLoc E = SMLoc::getFromPointer(Parser.getTok().getLoc().getPointer() - 1);

    Operands.push_back(MipsOperand::CreateImm(IdVal, S, E));
    return false;
  }//case AsmToken::Percent
  }//switch(getLexer().getKind())
  return true;
}

bool MipsAsmParser::parseRelocOperand(const MCExpr *&Res) {

  Parser.Lex(); //eat % token
  const AsmToken &Tok = Parser.getTok(); //get next token, operation
  if (Tok.isNot(AsmToken::Identifier))
    return true;

  std::string Str = Tok.getIdentifier().str();

  Parser.Lex(); //eat identifier
  //now make expression from the rest of the operand
  const MCExpr *IdVal;
  SMLoc EndLoc;

  if (getLexer().getKind() == AsmToken::LParen) {
    while (1) {
      Parser.Lex(); //eat '(' token
      if (getLexer().getKind() == AsmToken::Percent) {
        Parser.Lex(); //eat % token
        const AsmToken &nextTok = Parser.getTok();
        if (nextTok.isNot(AsmToken::Identifier))
          return true;
        Str += "(%";
        Str += nextTok.getIdentifier();
        Parser.Lex(); //eat identifier
        if (getLexer().getKind() != AsmToken::LParen)
          return true;
      } else
        break;
    }
    if (getParser().ParseParenExpression(IdVal,EndLoc))
      return true;

    while (getLexer().getKind() == AsmToken::RParen)
      Parser.Lex(); //eat ')' token

  } else
    return true; //parenthesis must follow reloc operand

  //Check the type of the expression
  if (const MCConstantExpr *MCE = dyn_cast<MCConstantExpr>(IdVal)) {
    //it's a constant, evaluate lo or hi value
    int Val = MCE->getValue();
    if (Str == "lo") {
      Val = Val & 0xffff;
    } else if (Str == "hi") {
      Val = (Val & 0xffff0000) >> 16;
    }
    Res = MCConstantExpr::Create(Val, getContext());
    return false;
  }

  if (const MCSymbolRefExpr *MSRE = dyn_cast<MCSymbolRefExpr>(IdVal)) {
    //it's a symbol, create symbolic expression from symbol
    StringRef Symbol = MSRE->getSymbol().getName();
    MCSymbolRefExpr::VariantKind VK = getVariantKind(Str);
    Res = MCSymbolRefExpr::Create(Symbol,VK,getContext());
    return false;
  }
  return true;
}

bool MipsAsmParser::ParseRegister(unsigned &RegNo, SMLoc &StartLoc,
                                  SMLoc &EndLoc) {

  StartLoc = Parser.getTok().getLoc();
  RegNo = tryParseRegister("");
  EndLoc = Parser.getTok().getLoc();
  return (RegNo == (unsigned)-1);
}

bool MipsAsmParser::parseMemOffset(const MCExpr *&Res) {

  SMLoc S;

  switch(getLexer().getKind()) {
  default:
    return true;
  case AsmToken::Integer:
  case AsmToken::Minus:
  case AsmToken::Plus:
    return (getParser().ParseExpression(Res));
  case AsmToken::Percent:
    return parseRelocOperand(Res);
  case AsmToken::LParen:
    return false;  //it's probably assuming 0
  }
  return true;
}

MipsAsmParser::OperandMatchResultTy MipsAsmParser::parseMemOperand(
               SmallVectorImpl<MCParsedAsmOperand*>&Operands) {

  const MCExpr *IdVal = 0;
  SMLoc S;
  //first operand is the offset
  S = Parser.getTok().getLoc();

  if (parseMemOffset(IdVal))
    return MatchOperand_ParseFail;

  const AsmToken &Tok = Parser.getTok(); //get next token
  if (Tok.isNot(AsmToken::LParen)) {
    Error(Parser.getTok().getLoc(), "'(' expected");
    return MatchOperand_ParseFail;
  }

  Parser.Lex(); // Eat '(' token.

  const AsmToken &Tok1 = Parser.getTok(); //get next token
  if (Tok1.is(AsmToken::Dollar)) {
    Parser.Lex(); // Eat '$' token.
    if (tryParseRegisterOperand(Operands,"")) {
      Error(Parser.getTok().getLoc(), "unexpected token in operand");
      return MatchOperand_ParseFail;
    }

  } else {
    Error(Parser.getTok().getLoc(),"unexpected token in operand");
    return MatchOperand_ParseFail;
  }

  const AsmToken &Tok2 = Parser.getTok(); //get next token
  if (Tok2.isNot(AsmToken::RParen)) {
    Error(Parser.getTok().getLoc(), "')' expected");
    return MatchOperand_ParseFail;
  }

  SMLoc E = SMLoc::getFromPointer(Parser.getTok().getLoc().getPointer() - 1);

  Parser.Lex(); // Eat ')' token.

  if (IdVal == 0)
    IdVal = MCConstantExpr::Create(0, getContext());

  //now replace register operand with the mem operand
  MipsOperand* op = static_cast<MipsOperand*>(Operands.back());
  int RegNo = op->getReg();
  //remove register from operands
  Operands.pop_back();
  //and add memory operand
  Operands.push_back(MipsOperand::CreateMem(RegNo, IdVal, S, E));
  delete op;
  return MatchOperand_Success;
}

MCSymbolRefExpr::VariantKind MipsAsmParser::getVariantKind(StringRef Symbol) {

  MCSymbolRefExpr::VariantKind VK
                   = StringSwitch<MCSymbolRefExpr::VariantKind>(Symbol)
    .Case("hi",          MCSymbolRefExpr::VK_Mips_ABS_HI)
    .Case("lo",          MCSymbolRefExpr::VK_Mips_ABS_LO)
    .Case("gp_rel",      MCSymbolRefExpr::VK_Mips_GPREL)
    .Case("call16",      MCSymbolRefExpr::VK_Mips_GOT_CALL)
    .Case("got",         MCSymbolRefExpr::VK_Mips_GOT)
    .Case("tlsgd",       MCSymbolRefExpr::VK_Mips_TLSGD)
    .Case("tlsldm",      MCSymbolRefExpr::VK_Mips_TLSLDM)
    .Case("dtprel_hi",   MCSymbolRefExpr::VK_Mips_DTPREL_HI)
    .Case("dtprel_lo",   MCSymbolRefExpr::VK_Mips_DTPREL_LO)
    .Case("gottprel",    MCSymbolRefExpr::VK_Mips_GOTTPREL)
    .Case("tprel_hi",    MCSymbolRefExpr::VK_Mips_TPREL_HI)
    .Case("tprel_lo",    MCSymbolRefExpr::VK_Mips_TPREL_LO)
    .Case("got_disp",    MCSymbolRefExpr::VK_Mips_GOT_DISP)
    .Case("got_page",    MCSymbolRefExpr::VK_Mips_GOT_PAGE)
    .Case("got_ofst",    MCSymbolRefExpr::VK_Mips_GOT_OFST)
    .Case("hi(%neg(%gp_rel",    MCSymbolRefExpr::VK_Mips_GPOFF_HI)
    .Case("lo(%neg(%gp_rel",    MCSymbolRefExpr::VK_Mips_GPOFF_LO)
    .Default(MCSymbolRefExpr::VK_None);

  return VK;
}

static int ConvertCcString(StringRef CondString) {
  int CC = StringSwitch<unsigned>(CondString)
      .Case(".f",    0)
      .Case(".un",   1)
      .Case(".eq",   2)
      .Case(".ueq",  3)
      .Case(".olt",  4)
      .Case(".ult",  5)
      .Case(".ole",  6)
      .Case(".ule",  7)
      .Case(".sf",   8)
      .Case(".ngle", 9)
      .Case(".seq",  10)
      .Case(".ngl",  11)
      .Case(".lt",   12)
      .Case(".nge",  13)
      .Case(".le",   14)
      .Case(".ngt",  15)
      .Default(-1);

  return CC;
}

bool MipsAsmParser::
parseMathOperation(StringRef Name, SMLoc NameLoc,
                        SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  //split the format
  size_t Start = Name.find('.'), Next = Name.rfind('.');
  StringRef Format1 = Name.slice(Start, Next);
  //and add the first format to the operands
  Operands.push_back(MipsOperand::CreateToken(Format1, NameLoc));
  //now for the second format
  StringRef Format2 = Name.slice(Next, StringRef::npos);
  Operands.push_back(MipsOperand::CreateToken(Format2, NameLoc));

  //set the format for the first register
  setFpFormat(Format1);

  // Read the remaining operands.
  if (getLexer().isNot(AsmToken::EndOfStatement)) {
    // Read the first operand.
    if (ParseOperand(Operands, Name)) {
      SMLoc Loc = getLexer().getLoc();
      Parser.EatToEndOfStatement();
      return Error(Loc, "unexpected token in argument list");
    }

    if (getLexer().isNot(AsmToken::Comma)) {
      SMLoc Loc = getLexer().getLoc();
      Parser.EatToEndOfStatement();
      return Error(Loc, "unexpected token in argument list");

    }
    Parser.Lex();  // Eat the comma.

    //set the format for the first register
    setFpFormat(Format2);

    // Parse and remember the operand.
    if (ParseOperand(Operands, Name)) {
      SMLoc Loc = getLexer().getLoc();
      Parser.EatToEndOfStatement();
      return Error(Loc, "unexpected token in argument list");
    }
  }

  if (getLexer().isNot(AsmToken::EndOfStatement)) {
    SMLoc Loc = getLexer().getLoc();
    Parser.EatToEndOfStatement();
    return Error(Loc, "unexpected token in argument list");
  }

  Parser.Lex(); // Consume the EndOfStatement
  return false;
}

bool MipsAsmParser::
ParseInstruction(ParseInstructionInfo &Info, StringRef Name, SMLoc NameLoc,
                 SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  //floating point instructions: should register be treated as double?
  if (requestsDoubleOperand(Name)) {
    setFpFormat(FP_FORMAT_D);
  Operands.push_back(MipsOperand::CreateToken(Name, NameLoc));
  }
  else {
    setDefaultFpFormat();
    // Create the leading tokens for the mnemonic, split by '.' characters.
    size_t Start = 0, Next = Name.find('.');
    StringRef Mnemonic = Name.slice(Start, Next);

    Operands.push_back(MipsOperand::CreateToken(Mnemonic, NameLoc));

    if (Next != StringRef::npos) {
      //there is a format token in mnemonic
      //StringRef Rest = Name.slice(Next, StringRef::npos);
      size_t Dot = Name.find('.', Next+1);
      StringRef Format = Name.slice(Next, Dot);
      if (Dot == StringRef::npos) //only one '.' in a string, it's a format
        Operands.push_back(MipsOperand::CreateToken(Format, NameLoc));
      else {
        if (Name.startswith("c.")){
          // floating point compare, add '.' and immediate represent for cc
          Operands.push_back(MipsOperand::CreateToken(".", NameLoc));
          int Cc = ConvertCcString(Format);
          if (Cc == -1) {
            return Error(NameLoc, "Invalid conditional code");
          }
          SMLoc E = SMLoc::getFromPointer(
              Parser.getTok().getLoc().getPointer() -1 );
          Operands.push_back(MipsOperand::CreateImm(
              MCConstantExpr::Create(Cc, getContext()), NameLoc, E));
        } else {
          //trunc, ceil, floor ...
          return parseMathOperation(Name, NameLoc, Operands);
        }

        //the rest is a format
        Format = Name.slice(Dot, StringRef::npos);
        Operands.push_back(MipsOperand::CreateToken(Format, NameLoc));
      }

      setFpFormat(Format);
    }
  }

  // Read the remaining operands.
  if (getLexer().isNot(AsmToken::EndOfStatement)) {
    // Read the first operand.
    if (ParseOperand(Operands, Name)) {
      SMLoc Loc = getLexer().getLoc();
      Parser.EatToEndOfStatement();
      return Error(Loc, "unexpected token in argument list");
    }

    while (getLexer().is(AsmToken::Comma) ) {
      Parser.Lex();  // Eat the comma.

      // Parse and remember the operand.
      if (ParseOperand(Operands, Name)) {
        SMLoc Loc = getLexer().getLoc();
        Parser.EatToEndOfStatement();
        return Error(Loc, "unexpected token in argument list");
      }
    }
  }

  if (getLexer().isNot(AsmToken::EndOfStatement)) {
    SMLoc Loc = getLexer().getLoc();
    Parser.EatToEndOfStatement();
    return Error(Loc, "unexpected token in argument list");
  }

  Parser.Lex(); // Consume the EndOfStatement
  return false;
}

bool MipsAsmParser::
ParseDirective(AsmToken DirectiveID) {

  if (DirectiveID.getString() == ".ent") {
    //ignore this directive for now
    Parser.Lex();
    return false;
  }

  if (DirectiveID.getString() == ".end") {
    //ignore this directive for now
    Parser.Lex();
    return false;
  }

  if (DirectiveID.getString() == ".frame") {
    //ignore this directive for now
    Parser.EatToEndOfStatement();
    return false;
  }

  if (DirectiveID.getString() == ".set") {
    //ignore this directive for now
    Parser.EatToEndOfStatement();
    return false;
  }

  if (DirectiveID.getString() == ".fmask") {
    //ignore this directive for now
    Parser.EatToEndOfStatement();
    return false;
  }

  if (DirectiveID.getString() == ".mask") {
    //ignore this directive for now
    Parser.EatToEndOfStatement();
    return false;
  }

  if (DirectiveID.getString() == ".gpword") {
    //ignore this directive for now
    Parser.EatToEndOfStatement();
    return false;
  }

  return true;
}

extern "C" void LLVMInitializeMipsAsmParser() {
  RegisterMCAsmParser<MipsAsmParser> X(TheMipsTarget);
  RegisterMCAsmParser<MipsAsmParser> Y(TheMipselTarget);
  RegisterMCAsmParser<MipsAsmParser> A(TheMips64Target);
  RegisterMCAsmParser<MipsAsmParser> B(TheMips64elTarget);
}

#define GET_REGISTER_MATCHER
#define GET_MATCHER_IMPLEMENTATION
#include "MipsGenAsmMatcher.inc"
