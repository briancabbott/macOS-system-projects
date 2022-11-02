//===--- IRGenDebugInfo.h - Debug Info Support-----------------------------===//
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
//
//  This file implements IR debug info generation for Swift.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "debug-info"
#include "IRGenDebugInfo.h"
#include "GenOpaque.h"
#include "GenType.h"
#include "Linking.h"
#include "swift/AST/Expr.h"
#include "swift/AST/IRGenOptions.h"
#include "swift/AST/Mangle.h"
#include "swift/AST/Module.h"
#include "swift/AST/ModuleLoader.h"
#include "swift/AST/Pattern.h"
#include "swift/Basic/Dwarf.h"
#include "swift/Basic/Punycode.h"
#include "swift/Basic/SourceManager.h"
#include "swift/Basic/Version.h"
#include "swift/ClangImporter/ClangImporter.h"
#include "swift/SIL/SILArgument.h"
#include "swift/SIL/SILBasicBlock.h"
#include "swift/SIL/SILDebugScope.h"
#include "swift/SIL/SILModule.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/Basic/Module.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/TargetInfo.h"
#include "llvm/Config/config.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Local.h"

using namespace swift;
using namespace irgen;

/// Strdup a raw char array using the bump pointer.
StringRef IRGenDebugInfo::BumpAllocatedString(const char *Data, size_t Length) {
  char *Ptr = DebugInfoNames.Allocate<char>(Length+1);
  memcpy(Ptr, Data, Length);
  *(Ptr+Length) = 0;
  return StringRef(Ptr, Length);
}

/// Strdup S using the bump pointer.
StringRef IRGenDebugInfo::BumpAllocatedString(std::string S) {
  return BumpAllocatedString(S.c_str(), S.length());
}

/// Strdup StringRef S using the bump pointer.
StringRef IRGenDebugInfo::BumpAllocatedString(StringRef S) {
  return BumpAllocatedString(S.data(), S.size());
}

/// Return the size reported by a type.
static unsigned getSizeInBits(llvm::DIType *Ty, const TrackingDIRefMap &Map) {
  // Follow derived types until we reach a type that
  // reports back a size.
  while (isa<llvm::DIDerivedType>(Ty) && !Ty->getSizeInBits()) {
    auto *DT = cast<llvm::DIDerivedType>(Ty);
    Ty = DT->getBaseType().resolve(Map);
    if (!Ty)
      return 0;
  }
  return Ty->getSizeInBits();
}

/// Return the size reported by the variable's type.
static unsigned getSizeInBits(const llvm::DILocalVariable *Var,
                              const TrackingDIRefMap &Map) {
  llvm::DIType *Ty = Var->getType().resolve(Map);
  return getSizeInBits(Ty, Map);
}

IRGenDebugInfo::IRGenDebugInfo(const IRGenOptions &Opts,
                               ClangImporter &CI,
                               IRGenModule &IGM,
                               llvm::Module &M,
                               SourceFile *SF)
  : Opts(Opts),
    CI(CI),
    SM(IGM.Context.SourceMgr),
    M(M),
    DBuilder(M),
    IGM(IGM),
    EntryPointFn(nullptr),
    MetadataTypeDecl(nullptr),
    InternalType(nullptr),
    LastDebugLoc({}),
    LastScope(nullptr)
{
  assert(Opts.DebugInfoKind > IRGenDebugInfoKind::None
         && "no debug info should be generated");
  StringRef SourceFileName = SF ? SF->getFilename() :
                                  StringRef(Opts.MainInputFilename);
  StringRef Dir;
  llvm::SmallString<256> AbsMainFile;
  if (SourceFileName.empty())
    AbsMainFile = "<unknown>";
  else {
    AbsMainFile = SourceFileName;
    llvm::sys::fs::make_absolute(AbsMainFile);
  }

  unsigned Lang = llvm::dwarf::DW_LANG_Swift;
  std::string Producer = version::getSwiftFullVersion();
  bool IsOptimized = Opts.Optimize;
  StringRef Flags = Opts.DWARFDebugFlags;
  unsigned Major, Minor;
  std::tie(Major, Minor) = version::getSwiftNumericVersion();
  unsigned RuntimeVersion = Major*100 + Minor;

  // No split DWARF on Darwin.
  StringRef SplitName = StringRef();
  // Note that File + Dir need not result in a valid path.
  // Clang is doing the same thing here.
  TheCU = DBuilder.createCompileUnit(
      Lang, AbsMainFile, Opts.DebugCompilationDir, Producer, IsOptimized,
      Flags, RuntimeVersion, SplitName,
      Opts.DebugInfoKind == IRGenDebugInfoKind::LineTables
          ? llvm::DIBuilder::LineTablesOnly
          : llvm::DIBuilder::FullDebug);
  MainFile = getOrCreateFile(BumpAllocatedString(AbsMainFile).data());

  if (auto *MainFunc = IGM.SILMod->lookUpFunction(SWIFT_ENTRY_POINT_FUNCTION)) {
    IsLibrary = false;
    auto *MainIGM = IGM.dispatcher.getGenModule(MainFunc->getDeclContext());
    
    // Don't create the function type if we are in a different llvm module than
    // the module where @main is defined. This is the case for non-primary
    // modules when doing multi-threaded whole-module compilation.
    if (MainIGM == &IGM) {
      EntryPointFn = DBuilder.createReplaceableCompositeType(
          llvm::dwarf::DW_TAG_subroutine_type, SWIFT_ENTRY_POINT_FUNCTION,
          MainFile, MainFile, 0);
    }
  }

  // Because the swift compiler relies on Clang to setup the Module,
  // the clang CU is always created first.  Several dwarf-reading
  // tools (older versions of ld64, and lldb) can get confused if the
  // first CU in an object is empty, so ensure that the Swift CU comes
  // first by rearranging the list of CUs in the LLVM module.
  llvm::NamedMDNode *CU_Nodes = M.getNamedMetadata("llvm.dbg.cu");
  SmallVector<llvm::DICompileUnit *, 2> CUs;
  for (auto *N : CU_Nodes->operands())
    CUs.push_back(cast<llvm::DICompileUnit>(N));
  CU_Nodes->dropAllReferences();
  for (auto CU = CUs.rbegin(), CE = CUs.rend(); CU != CE; ++CU)
    CU_Nodes->addOperand(*CU);

  // Create a module for the current compile unit.
  llvm::sys::path::remove_filename(AbsMainFile);
  MainModule =
      getOrCreateModule(Opts.ModuleName, TheCU, Opts.ModuleName, AbsMainFile);
  DBuilder.createImportedModule(MainFile, MainModule, 1);
}

static const char *getFilenameFromDC(const DeclContext *DC) {
  if (auto LF = dyn_cast<LoadedFile>(DC)) {
    // FIXME: Today, the subclasses of LoadedFile happen to return StringRefs
    // that are backed by null-terminated strings, but that's certainly not
    // guaranteed in the future.
    StringRef Fn = LF->getFilename();
    assert(((Fn.size() == 0) ||
            (Fn.data()[Fn.size()] == '\0')) && "not a C string");
    return Fn.data();
  }
  if (auto SF = dyn_cast<SourceFile>(DC))
    return SF->getFilename().data();
  else if (auto M = dyn_cast<Module>(DC))
    return M->getModuleFilename().data();
  else
    return nullptr;
}


Location getDeserializedLoc(Pattern*) { return {}; }
Location getDeserializedLoc(Expr*)    { return {}; }
Location getDeserializedLoc(Stmt*)    { return {}; }
Location getDeserializedLoc(Decl* D)  {
  Location L = {};
  const DeclContext *DC = D->getDeclContext()->getModuleScopeContext();
  if (const char *Filename = getFilenameFromDC(DC)) {
    L.Filename = Filename;
    L.Line = 0;
  }
  return L;
}

Location getLoc(SourceManager &SM, SourceLoc Loc) {
  Location L = {};
  if (Loc.isValid()) {
    L.Filename = SM.getBufferIdentifierForLoc(Loc);
    std::tie(L.Line, L.Col) = SM.getLineAndColumn(Loc);
  }
  return L;
}

/// Use the SM to figure out the actual line/column of a SourceLoc.
template <typename WithLoc>
Location getLoc(SourceManager &SM, WithLoc *S, bool End = false) {
  Location L = {};
  if (S == nullptr)
    return L;

  SourceLoc Loc = End ? S->getEndLoc() : S->getStartLoc();
  if (Loc.isInvalid())
    // This may be a deserialized or clang-imported decl. And modules
    // don't come with SourceLocs right now. Get at least the name of
    // the module.
    return getDeserializedLoc(S);

  return getLoc(SM, Loc);
}

/// \brief Return the start of the location's source range.
static Location getStartLocation(SourceManager &SM,
                                 Optional<SILLocation> OptLoc) {
  if (!OptLoc) return {};
  return getLoc(SM, OptLoc->getStartSourceLoc());
}

/// \brief Return the debug location from a SILLocation.
static Location getDebugLocation(SourceManager &SM,
                                 Optional<SILLocation> OptLoc) {
  if (!OptLoc || OptLoc->isInPrologue())
    return {};
  return getLoc(SM, OptLoc->getDebugSourceLoc());
}


/// \brief Extract the start location from a SILLocation.
///
/// This returns a FullLocation, which contains the location that
/// should be used for the linetable and the "true" AST location (used
/// for, e.g., variable declarations).
static FullLocation getLocation(SourceManager &SM,
                                Optional<SILLocation> OptLoc) {
  if (!OptLoc) return {};

  SILLocation Loc = OptLoc.getValue();
  return { getLoc(SM, Loc.getDebugSourceLoc()),
           getLoc(SM, Loc.getSourceLoc())};
}

/// Determine whether this debug scope belongs to an explicit closure.
static bool isExplicitClosure(const SILDebugScope *DS) {
  if (DS)
    if (Expr *E = DS->Loc.getAsASTNode<Expr>())
      if (isa<ClosureExpr>(E))
        return true;
  return false;
}

/// Determine whether this location is some kind of closure.
static bool isAbstractClosure(const SILLocation &Loc) {
  if (Expr *E = Loc.getAsASTNode<Expr>())
    if (isa<AbstractClosureExpr>(E))
      return true;
  return false;
}

/// Construct an inlined-at location from a SILScope.
llvm::MDNode* IRGenDebugInfo::createInlinedAt(const SILDebugScope *InlinedScope) {
  assert(InlinedScope);
  assert(InlinedScope->InlinedCallSite && "not an inlined scope");
  const SILDebugScope *CallSite = InlinedScope->InlinedCallSite;

#ifndef NDEBUG
  auto *S = getOrCreateScope(InlinedScope);
  while (!isa<llvm::DISubprogram>(S)) {
    auto *LB = dyn_cast<llvm::DILexicalBlockBase>(S);
    S = LB->getScope();
    assert(S && "Lexical block parent chain must contain a subprogram");
  }
#endif

  auto ParentScope = getOrCreateScope(CallSite->Parent);
  llvm::MDNode *InlinedAt = nullptr;

  // If this is itself an inlined location, recursively create the
  // inlined-at location for it.
  if (CallSite->InlinedCallSite)
    InlinedAt = createInlinedAt(CallSite);

  auto InlineLoc = getLoc(SM, CallSite->Loc.getDebugSourceLoc());
  return llvm::DebugLoc::get(InlineLoc.Line, InlineLoc.Col, ParentScope,
                             InlinedAt);
}

#ifndef NDEBUG
/// Perform a couple of sanity checks on scopes.
static bool parentScopesAreSane(const SILDebugScope *DS) {
  const SILDebugScope *Parent = DS->Parent;
  while (Parent) {
    if (!DS->InlinedCallSite) {
      assert(!Parent->InlinedCallSite &&
             "non-inlined scope has an inlined parent");
      assert(DS->SILFn == Parent->SILFn
             && "non-inlined parent scope from different function?");
    }
    Parent = Parent->Parent;
  }
  return true;
}

bool IRGenDebugInfo::lineNumberIsSane(IRBuilder &Builder, unsigned Line) {
  if (IGM.Opts.Optimize)
    return true;

  // Assert monotonically increasing line numbers within the same basic block;
  llvm::BasicBlock *CurBasicBlock = Builder.GetInsertBlock();
  if (CurBasicBlock == LastBasicBlock) {
    return Line >= LastDebugLoc.Line;
  }
  LastBasicBlock = CurBasicBlock;
  return true;
}
#endif

void IRGenDebugInfo::setCurrentLoc(IRBuilder &Builder, const SILDebugScope *DS,
                                   Optional<SILLocation> Loc) {
  assert(DS && "empty scope");
  auto *Scope = getOrCreateScope(DS);
  if (!Scope)
    return;

  Location L = getDebugLocation(SM, Loc);
  auto *File = getOrCreateFile(L.Filename);
  if (File->getFilename() != Scope->getFilename()) {
    // We changed files in the middle of a scope. This happens, for
    // example, when constructors are inlined. Create a new scope to
    // reflect this.
    auto File = getOrCreateFile(L.Filename);
    Scope = DBuilder.createLexicalBlockFile(Scope, File);
  }

  // Both the code that is used to set up a closure object and the
  // (beginning of) the closure itself has the AbstractClosureExpr as
  // location. We are only interested in the latter case and want to
  // ignore the setup code.
  //
  // callWithClosure(
  //  { // <-- a breakpoint here should only stop inside of the closure.
  //    foo();
  //  })
  //
  // The actual closure has a closure expression as scope.
  if (Loc && isAbstractClosure(*Loc) && DS && !isAbstractClosure(DS->Loc)
      && Loc->getKind() != SILLocation::ImplicitReturnKind)
    return;

  if (L.Line == 0 && DS == LastScope) {
    // Reuse the last source location if we are still in the same
    // scope to get a more contiguous line table.
    L = LastDebugLoc;
  }

  // FIXME: Enable this assertion.
  //assert(lineNumberIsSane(Builder, L.Line) &&
  //       "-Onone, but line numbers are not monotonically increasing within bb");
  LastDebugLoc = L;
  LastScope = DS;

  llvm::MDNode *InlinedAt = nullptr;
  if (DS->InlinedCallSite) {
    assert(Scope && "Inlined location without a lexical scope");
    InlinedAt = createInlinedAt(DS);
  }

  assert(((!InlinedAt) || (InlinedAt && Scope)) && "inlined w/o scope");
  assert(parentScopesAreSane(DS) && "parent scope sanity check failed");
  auto DL = llvm::DebugLoc::get(L.Line, L.Col, Scope, InlinedAt);
  // TODO: Write a strongly-worded letter to the person that came up
  // with a pair of functions spelled "get" and "Set".
  Builder.SetCurrentDebugLocation(DL);
}

/// getOrCreateScope - Translate a SILDebugScope into an llvm::DIDescriptor.
llvm::DIScope *IRGenDebugInfo::getOrCreateScope(const SILDebugScope *DS) {
  if (DS == 0)
    return MainFile;

  // Try to find it in the cache first.
  auto CachedScope = ScopeCache.find(DS);
  if (CachedScope != ScopeCache.end())
    return cast<llvm::DIScope>(CachedScope->second);

  // If this is a (inlined) function scope, the function may
  // not have been created yet.
  if (!DS->Parent ||
      DS->Loc.getKind() == SILLocation::SILFileKind ||
      DS->Loc.isASTNode<AbstractFunctionDecl>() ||
      DS->Loc.isASTNode<AbstractClosureExpr>() ||
      DS->Loc.isASTNode<EnumElementDecl>()) {

    auto *FnScope = DS->SILFn->getDebugScope();
    // FIXME: This is a bug in the SIL deserialization.
    if (!FnScope)
      DS->SILFn->setDebugScope(DS);

    auto CachedScope = ScopeCache.find(FnScope);
    if (CachedScope != ScopeCache.end())
      return cast<llvm::DIScope>(CachedScope->second);

    // Force the debug info for the function to be emitted, even if it
    // is external or has been inlined.
    llvm::Function *Fn = nullptr;
    if (!DS->SILFn->getName().empty() && !DS->SILFn->isZombie())
      Fn = IGM.getAddrOfSILFunction(DS->SILFn, NotForDefinition);
    auto *SP = emitFunction(*DS->SILFn, Fn);

    // Cache it.
    ScopeCache[DS] = llvm::TrackingMDNodeRef(SP);
    return SP;
  }

  llvm::DIScope *Parent = getOrCreateScope(DS->Parent);
  if (Opts.DebugInfoKind == IRGenDebugInfoKind::LineTables)
    return Parent;

  assert(DS->Parent && "lexical block must have a parent subprogram");
  Location L = getStartLocation(SM, DS->Loc);
  llvm::DIFile *File = getOrCreateFile(L.Filename);
  auto *DScope = DBuilder.createLexicalBlock(Parent, File, L.Line, L.Col);

  // Cache it.
  ScopeCache[DS] = llvm::TrackingMDNodeRef(DScope);

  return DScope;
}

/// getOrCreateFile - Translate filenames into DIFiles.
llvm::DIFile *IRGenDebugInfo::getOrCreateFile(const char *Filename) {
  if (!Filename)
    return MainFile;

  if (MainFile) {
    SmallString<256> AbsMainFile, ThisFile;
    AbsMainFile = Filename;
    llvm::sys::fs::make_absolute(AbsMainFile);
    llvm::sys::path::append(ThisFile, MainFile->getDirectory(),
                            MainFile->getFilename());
    if (ThisFile == AbsMainFile) {
      DIFileCache[Filename] = llvm::TrackingMDNodeRef(MainFile);
      return MainFile;
    }
  }

  // Look in the cache first.
  auto CachedFile = DIFileCache.find(Filename);

  if (CachedFile != DIFileCache.end()) {
    // Verify that the information still exists.
    if (llvm::Metadata *V = CachedFile->second)
      return cast<llvm::DIFile>(V);
  }

  // Create a new one.
  StringRef File = llvm::sys::path::filename(Filename);
  llvm::SmallString<512> Path(Filename);
  llvm::sys::path::remove_filename(Path);
  llvm::DIFile *F = DBuilder.createFile(File, Path);

  // Cache it.
  DIFileCache[Filename] = llvm::TrackingMDNodeRef(F);
  return F;
}

/// Attempt to figure out the unmangled name of a function.
StringRef IRGenDebugInfo::getName(const FuncDecl &FD) {
  // Getters and Setters are anonymous functions, so we forge a name
  // using its parent declaration.
  if (FD.isAccessor())
    if (ValueDecl *VD = FD.getAccessorStorageDecl()) {
      const char *Kind;
      switch (FD.getAccessorKind()) {
      case AccessorKind::NotAccessor: llvm_unreachable("this is an accessor");
      case AccessorKind::IsGetter: Kind = ".get"; break;
      case AccessorKind::IsSetter: Kind = ".set"; break;
      case AccessorKind::IsWillSet: Kind = ".willset"; break;
      case AccessorKind::IsDidSet: Kind = ".didset"; break;
      case AccessorKind::IsMaterializeForSet: Kind = ".materialize"; break;
      case AccessorKind::IsAddressor: Kind = ".addressor"; break;
      case AccessorKind::IsMutableAddressor: Kind = ".mutableAddressor"; break;
      }

      SmallVector<char, 64> Buf;
      StringRef Name = (VD->getName().str() + Twine(Kind)).toStringRef(Buf);
      return BumpAllocatedString(Name);
    }

  if (FD.hasName())
    return FD.getName().str();

  return StringRef();
}

/// Attempt to figure out the unmangled name of a function.
StringRef IRGenDebugInfo::getName(SILLocation L) {
  if (L.isNull())
    return StringRef();

  if (FuncDecl *FD = L.getAsASTNode<FuncDecl>())
    return getName(*FD);

  if (L.isASTNode<ConstructorDecl>())
    return "init";

  if (L.isASTNode<DestructorDecl>())
    return "deinit";

  return StringRef();
}

static CanSILFunctionType getFunctionType(SILType SILTy) {
  if (!SILTy)
    return CanSILFunctionType();

  auto FnTy = SILTy.getAs<SILFunctionType>();
  if (!FnTy) {
    DEBUG(llvm::dbgs() << "Unexpected function type: "; SILTy.dump();
          llvm::dbgs() << "\n");
    return CanSILFunctionType();
  }

  return FnTy;
}

/// Build the context chain for a given DeclContext.
llvm::DIScope *IRGenDebugInfo::getOrCreateContext(DeclContext *DC) {
  if (!DC)
    return TheCU;

  switch (DC->getContextKind()) {
  // TODO: Create a cache for functions.
  case DeclContextKind::AbstractClosureExpr:
  case DeclContextKind::AbstractFunctionDecl:

  // We don't model these in DWARF.
  case DeclContextKind::SerializedLocal:
  case DeclContextKind::Initializer:
  case DeclContextKind::ExtensionDecl:
    return getOrCreateContext(DC->getParent());

  case DeclContextKind::TopLevelCodeDecl:
    return cast<llvm::DIScope>(EntryPointFn);
  case DeclContextKind::Module:
    return getOrCreateModule({Module::AccessPathTy(), cast<ModuleDecl>(DC)});
  case DeclContextKind::FileUnit:
    // A module may contain multiple files.
    return getOrCreateContext(DC->getParent());
  case DeclContextKind::NominalTypeDecl: {
    auto CachedType = DITypeCache.find(
        cast<NominalTypeDecl>(DC)->getDeclaredType().getPointer());
    if (CachedType != DITypeCache.end()) {
      // Verify that the information still exists.
      if (llvm::Metadata *Val = CachedType->second)
        return cast<llvm::DIType>(Val);
    }

    // Create a Forward-declared type.
    auto *TyDecl = cast<NominalTypeDecl>(DC);
    auto Loc = getLoc(SM, TyDecl);
    auto File = getOrCreateFile(Loc.Filename);
    auto Line = Loc.Line;
    auto FwdDecl = DBuilder.createForwardDecl(
        llvm::dwarf::DW_TAG_structure_type, TyDecl->getName().str(),
        getOrCreateContext(DC->getParent()), File, Line,
        llvm::dwarf::DW_LANG_Swift, 0, 0);

    return FwdDecl;
  }
  }
  return TheCU;
}

/// Create a single parameter type and push it.
void IRGenDebugInfo::createParameterType(
    llvm::SmallVectorImpl<llvm::Metadata *> &Parameters, SILType type,
    DeclContext *DeclCtx) {
  // FIXME: This use of getSwiftType() is extremely suspect.
  DebugTypeInfo DbgTy(type.getSwiftType(), IGM.getTypeInfo(type), DeclCtx);
  Parameters.push_back(getOrCreateType(DbgTy));
}

/// Create the array of function parameters for FnTy. SIL Version.
llvm::DITypeRefArray
IRGenDebugInfo::createParameterTypes(SILType SILTy, DeclContext *DeclCtx) {
  if (!SILTy)
    return nullptr;
  return createParameterTypes(SILTy.castTo<SILFunctionType>(), DeclCtx);
}

/// Create the array of function parameters for a function type.
llvm::DITypeRefArray
IRGenDebugInfo::createParameterTypes(CanSILFunctionType FnTy,
                                     DeclContext *DeclCtx) {
  SmallVector<llvm::Metadata *, 16> Parameters;

  GenericsRAII scope(*this, FnTy->getGenericSignature());

  // The function return type is the first element in the list.
  createParameterType(Parameters, FnTy->getSemanticResultSILType(),
                      DeclCtx);

  // Actually, the input type is either a single type or a tuple
  // type. We currently represent a function with one n-tuple argument
  // as an n-ary function.
  for (auto Param : FnTy->getParameters())
    createParameterType(Parameters, Param.getSILType(), DeclCtx);

  return DBuilder.getOrCreateTypeArray(Parameters);
}

/// FIXME: replace this condition with something more sane.
static bool isAllocatingConstructor(SILFunctionTypeRepresentation Rep,
                                    DeclContext *DeclCtx) {
  return Rep != SILFunctionTypeRepresentation::Method
          && DeclCtx && isa<ConstructorDecl>(DeclCtx);
}

llvm::DISubprogram *IRGenDebugInfo::emitFunction(
    SILModule &SILMod, const SILDebugScope *DS, llvm::Function *Fn,
    SILFunctionTypeRepresentation Rep, SILType SILTy, DeclContext *DeclCtx) {
  // Returned a previously cached entry for an abstract (inlined) function.
  auto cached = ScopeCache.find(DS);
  if (cached != ScopeCache.end())
    return cast<llvm::DISubprogram>(cached->second);

  StringRef LinkageName;
  if (Fn)
    LinkageName = Fn->getName();
  else if (DS)
    LinkageName = DS->SILFn->getName();
  else
    llvm_unreachable("function has no mangled name");

  StringRef Name;
  if (DS) {
    if (DS->Loc.getKind() == SILLocation::SILFileKind)
      Name = DS->SILFn->getName();
    else
      Name = getName(DS->Loc);
  }

  Location L = {};
  unsigned ScopeLine = 0; /// The source line used for the function prologue.
  // Bare functions and thunks should not have any line numbers. This
  // is especially important for shared functions like reabstraction
  // thunk helpers, where getLocation() returns an arbitrary location
  // of whichever use was emitted first.
  if (DS && (!DS->SILFn || (!DS->SILFn->isBare() && !DS->SILFn->isThunk()))) {
    auto FL = getLocation(SM, DS->Loc);
    L = FL.Loc;
    ScopeLine = FL.LocForLinetable.Line;
  }

  auto File = getOrCreateFile(L.Filename);
  auto Scope = MainModule;
  auto Line = L.Line;

  // We know that main always comes from MainFile.
  if (LinkageName == SWIFT_ENTRY_POINT_FUNCTION) {
    if (!L.Filename)
      File = MainFile;
    Line = 1;
    Name = LinkageName;
  }

  CanSILFunctionType FnTy = getFunctionType(SILTy);
  auto Params = Opts.DebugInfoKind == IRGenDebugInfoKind::LineTables
    ? nullptr
    : createParameterTypes(SILTy, DeclCtx);
  llvm::DISubroutineType *DIFnTy = DBuilder.createSubroutineType(Params);
  llvm::DITemplateParameterArray TemplateParameters = nullptr;
  llvm::DISubprogram *Decl = nullptr;

  // Various flags
  bool IsLocalToUnit = Fn ? Fn->hasInternalLinkage() : true;
  bool IsDefinition = true;
  bool IsOptimized = Opts.Optimize;
  unsigned Flags = 0;

  // Mark everything that is not visible from the source code (i.e.,
  // does not have a Swift name) as artificial, so the debugger can
  // ignore it. Explicit closures are exempt from this rule. We also
  // make an exception for main, which, albeit it does not
  // have a Swift name, does appear prominently in the source code.
  if ((Name.empty() && LinkageName != SWIFT_ENTRY_POINT_FUNCTION &&
       !isExplicitClosure(DS)) ||
      // ObjC thunks should also not show up in the linetable, because we
      // never want to set a breakpoint there.
      (Rep == SILFunctionTypeRepresentation::ObjCMethod) ||
      isAllocatingConstructor(Rep, DeclCtx)) {
    Flags |= llvm::DINode::FlagArtificial;
    ScopeLine = 0;
  }

  if (FnTy && FnTy->getRepresentation()
        == SILFunctionType::Representation::Block)
    Flags |= llvm::DINode::FlagAppleBlock;

  llvm::DISubprogram *SP = DBuilder.createFunction(
      Scope, Name, LinkageName, File, Line, DIFnTy, IsLocalToUnit, IsDefinition,
      ScopeLine, Flags, IsOptimized, TemplateParameters, Decl);

  if (Fn && !Fn->isDeclaration())
    Fn->setSubprogram(SP);

  // RAUW the entry point function forward declaration with the real thing.
  if (LinkageName == SWIFT_ENTRY_POINT_FUNCTION) {
    assert(EntryPointFn->isTemporary() &&
           "more than one entry point function");
    EntryPointFn->replaceAllUsesWith(SP);
    llvm::MDNode::deleteTemporary(EntryPointFn);
    EntryPointFn = SP;
  }

  if (!DS)
    return nullptr;

  ScopeCache[DS] = llvm::TrackingMDNodeRef(SP);
  return SP;
}

/// TODO: This is no longer needed.
void IRGenDebugInfo::eraseFunction(llvm::Function *Fn) {}

/// The DWARF output for import decls is similar to that of a using
/// directive in C++:
///   import Foundation
///   -->
///   0: DW_TAG_imported_module
///        DW_AT_import(*1)
///   1: DW_TAG_module // instead of DW_TAG_namespace.
///        DW_AT_name("Foundation")
///
void IRGenDebugInfo::emitImport(ImportDecl *D) {
  if (Opts.DebugInfoKind == IRGenDebugInfoKind::LineTables)
    return;

  swift::Module *M = IGM.Context.getModule(D->getModulePath());
  if (!M &&
      D->getModulePath()[0].first == IGM.Context.TheBuiltinModule->getName())
    M = IGM.Context.TheBuiltinModule;
  if (!M) {
    assert(M && "Could not find module for import decl.");
    return;
  }
  auto DIMod = getOrCreateModule({D->getModulePath(), M});
  Location L = getLoc(SM, D);
  DBuilder.createImportedModule(getOrCreateFile(L.Filename), DIMod, L.Line);
}

llvm::DIModule *
IRGenDebugInfo::getOrCreateModule(ModuleDecl::ImportedModule M) {
  const char *fn = getFilenameFromDC(M.second);
  StringRef Path(fn ? fn : "");
  if (M.first.empty()) {
    StringRef Name = M.second->getName().str();
    return getOrCreateModule(Name, TheCU, Name, Path);
  }

  unsigned I = 0;
  SmallString<128> AccessPath;
  llvm::DIScope *Scope = TheCU;
  llvm::raw_svector_ostream OS(AccessPath);
  for (auto elt : M.first) {
    auto Component = elt.first.str();
    if (++I > 1)
      OS << '.';
    OS << Component;
    Scope = getOrCreateModule(AccessPath, Scope, Component, Path);
  }
  return cast<llvm::DIModule>(Scope);
}

/// Return a cached module for an access path or create a new one.
llvm::DIModule *IRGenDebugInfo::getOrCreateModule(StringRef Key,
                                                  llvm::DIScope *Parent,
                                                  StringRef Name,
                                                  StringRef IncludePath) {
  // Look in the cache first.
  auto Val = DIModuleCache.find(Key);
  if (Val != DIModuleCache.end())
    return cast<llvm::DIModule>(Val->second);

  StringRef ConfigMacros;
  StringRef Sysroot = IGM.Context.SearchPathOpts.SDKPath;
  auto M =
      DBuilder.createModule(Parent, Name, ConfigMacros, IncludePath, Sysroot);
  DIModuleCache.insert({Key, llvm::TrackingMDNodeRef(M)});
  return M;
}

llvm::DISubprogram *IRGenDebugInfo::emitFunction(SILFunction &SILFn,
                                                 llvm::Function *Fn) {
  auto *DS = SILFn.getDebugScope();
  assert(DS && "SIL function has no debug scope");
  (void) DS;
  return emitFunction(SILFn.getModule(), SILFn.getDebugScope(), Fn,
                      SILFn.getRepresentation(), SILFn.getLoweredType(),
                      SILFn.getDeclContext());
}

void IRGenDebugInfo::emitArtificialFunction(SILModule &SILMod,
                                            IRBuilder &Builder,
                                            llvm::Function *Fn, SILType SILTy) {
  RegularLocation ALoc = RegularLocation::getAutoGeneratedLocation();
  const SILDebugScope *Scope = new (SILMod) SILDebugScope(ALoc);
  emitFunction(SILMod, Scope, Fn, SILFunctionTypeRepresentation::Thin, SILTy);
  setCurrentLoc(Builder, Scope);
}

TypeAliasDecl *IRGenDebugInfo::getMetadataType() {
  if (!MetadataTypeDecl) {
    MetadataTypeDecl = new (IGM.Context) TypeAliasDecl(
        SourceLoc(), IGM.Context.getIdentifier("$swift.type"), SourceLoc(),
        TypeLoc::withoutLoc(IGM.Context.TheRawPointerType),
        IGM.Context.TheBuiltinModule);
    MetadataTypeDecl->computeType();
  }
  return MetadataTypeDecl;
}

void IRGenDebugInfo::emitTypeMetadata(IRGenFunction &IGF,
                                      llvm::Value *Metadata,
                                      StringRef Name) {
  if (Opts.DebugInfoKind == IRGenDebugInfoKind::LineTables)
    return;

  auto TName = BumpAllocatedString(("$swift.type." + Name).str());
  DebugTypeInfo DbgTy(getMetadataType(), Metadata->getType(),
                      (Size)CI.getTargetInfo().getPointerWidth(0),
                      (Alignment)CI.getTargetInfo().getPointerAlign(0));
  emitVariableDeclaration(IGF.Builder, Metadata, DbgTy, IGF.getDebugScope(),
                          TName, 0,
                          // swift.type is a already pointer type,
                          // having a shadow copy doesn't add another
                          // layer of indirection.
                          DirectValue, ArtificialValue);
}

void IRGenDebugInfo::emitStackVariableDeclaration(
    IRBuilder &B, ArrayRef<llvm::Value *> Storage, DebugTypeInfo DbgTy,
    const SILDebugScope *DS, StringRef Name, IndirectionKind Indirection) {
  emitVariableDeclaration(B, Storage, DbgTy, DS, Name, 0, Indirection,
                          RealValue);
}

void IRGenDebugInfo::emitArgVariableDeclaration(
    IRBuilder &Builder, ArrayRef<llvm::Value *> Storage, DebugTypeInfo DbgTy,
    const SILDebugScope *DS, StringRef Name, unsigned ArgNo,
    IndirectionKind Indirection, ArtificialKind IsArtificial) {
  assert(ArgNo > 0);
  if (Name == IGM.Context.Id_self.str())
    emitVariableDeclaration(Builder, Storage, DbgTy, DS, Name, ArgNo,
                            DirectValue, ArtificialValue);
  else
    emitVariableDeclaration(Builder, Storage, DbgTy, DS, Name, ArgNo,
                            Indirection, IsArtificial);
}

/// Return the DIFile that is the ancestor of Scope.
llvm::DIFile *IRGenDebugInfo::getFile(llvm::DIScope *Scope) {
  while (!isa<llvm::DIFile>(Scope)) {
    switch (Scope->getTag()) {
    case llvm::dwarf::DW_TAG_lexical_block:
      Scope = cast<llvm::DILexicalBlock>(Scope)->getScope();
      break;
    case llvm::dwarf::DW_TAG_subprogram: {
      // Scopes are not indexed by UID.
      llvm::DITypeIdentifierMap EmptyMap;
      Scope = cast<llvm::DISubprogram>(Scope)->getScope().resolve(EmptyMap);
      break;
    }
    default:
      return MainFile;
    }
    if (Scope)
      return MainFile;
  }
  return cast<llvm::DIFile>(Scope);
}

/// Return the storage size of an explosion value.
static uint64_t getSizeFromExplosionValue(const clang::TargetInfo &TI,
                                          llvm::Value *V) {
  llvm::Type *Ty = V->getType();
  if (unsigned PrimitiveSize = Ty->getPrimitiveSizeInBits())
    return PrimitiveSize;
  else if (Ty->isPointerTy())
    return TI.getPointerWidth(0);
  else
    llvm_unreachable("unhandled type of explosion value");
}

/// A generator that recursively returns the size of each element of a
/// composite type.
class ElementSizes {
  const TrackingDIRefMap &DIRefMap;
  llvm::SmallPtrSetImpl<const llvm::DIType *> &IndirectEnums;
  llvm::SmallVector<const llvm::DIType *, 12> Stack;
public:
  ElementSizes(const llvm::DIType *DITy, const TrackingDIRefMap &DIRefMap,
               llvm::SmallPtrSetImpl<const llvm::DIType *> &IndirectEnums)
    : DIRefMap(DIRefMap), IndirectEnums(IndirectEnums), Stack(1, DITy) {}

  struct SizeAlign {
    uint64_t SizeInBits, AlignInBits;
  };

  struct SizeAlign getNext() {
    if (Stack.empty())
      return {0, 0};

    auto *Cur = Stack.pop_back_val();
    if (isa<llvm::DICompositeType>(Cur) &&
        Cur->getTag() != llvm::dwarf::DW_TAG_subroutine_type) {
      auto *CTy = cast<llvm::DICompositeType>(Cur);
      auto Elts = CTy->getElements();
      unsigned N = Cur->getTag() == llvm::dwarf::DW_TAG_union_type
        ? std::min(1U, Elts.size()) // For unions, pick any one.
        : Elts.size();

      if (N) {
        // Push all elements in reverse order.
        // FIXME: With a little more state we don't need to actually
        // store them on the Stack.
        for (unsigned I = N; I > 0; --I)
          Stack.push_back(cast<llvm::DIType>(Elts[I - 1]));
        return getNext();
      }
    }
    switch (Cur->getTag()) {
    case llvm::dwarf::DW_TAG_member:
      // FIXME: Correctly handle the explosion value for enum types
      // with indirect members.
      if (IndirectEnums.count(Cur))
        return {0, 0};
      [[clang::fallthrough]];
    case llvm::dwarf::DW_TAG_typedef: {
      // Replace top of stack.
      auto *DTy = cast<llvm::DIDerivedType>(Cur);
      Stack.push_back(DTy->getBaseType().resolve(DIRefMap));
      return getNext();
    }
    default:
      return {Cur->getSizeInBits(), Cur->getAlignInBits()};
    }
  }
};

static Size
getStorageSize(const llvm::DataLayout &DL, ArrayRef<llvm::Value *> Storage) {
  unsigned size = 0;
  for (llvm::Value *Piece : Storage)
    size += DL.getTypeSizeInBits(Piece->getType());
  return Size(size);
}

void IRGenDebugInfo::emitVariableDeclaration(
    IRBuilder &Builder, ArrayRef<llvm::Value *> Storage, DebugTypeInfo DbgTy,
    const SILDebugScope *DS, StringRef Name, unsigned ArgNo,
    IndirectionKind Indirection, ArtificialKind Artificial) {
  // FIXME: Make this an assertion.
  // assert(DS && "variable has no scope");
  if (!DS)
    return;

  if (Opts.DebugInfoKind == IRGenDebugInfoKind::LineTables)
    return;

  if (!DbgTy.size)
    DbgTy.size = getStorageSize(IGM.DataLayout, Storage);

  auto *Scope = dyn_cast<llvm::DILocalScope>(getOrCreateScope(DS));
  assert(Scope && "variable has no local scope");
  Location Loc = getLoc(SM, DbgTy.getDecl());

  // FIXME: this should be the scope of the type's declaration.
  // If this is an argument, attach it to the current function scope.
  if (ArgNo > 0) {
    while (isa<llvm::DILexicalBlock>(Scope))
      Scope = cast<llvm::DILexicalBlock>(Scope)->getScope();
  }
  assert(Scope && isa<llvm::DIScope>(Scope) && "variable has no scope");
  llvm::DIFile *Unit = getFile(Scope);
  llvm::DIType *DITy = getOrCreateType(DbgTy);
  assert(DITy && "could not determine debug type of variable");

  unsigned Line = Loc.Line;
  unsigned Flags = 0;
  if (Artificial || DITy->isArtificial() || DITy == InternalType)
    Flags |= llvm::DINode::FlagArtificial;

  // Create the descriptor for the variable.
  llvm::DILocalVariable *Var = nullptr;
  llvm::DIExpression *Expr = DBuilder.createExpression();

  if (Indirection) {
    // Classes are always passed by reference.
    int64_t Addr[] = { llvm::dwarf::DW_OP_deref };
    Expr = DBuilder.createExpression(Addr);
    // FIXME: assert(Flags == 0 && "Complex variables cannot have flags");
  }
  /// This could be Opts.Optimize if we would also unique DIVariables here.
  bool Optimized = false;
  Var = (ArgNo > 0)
    ? DBuilder.createParameterVariable(Scope, Name, ArgNo, Unit, Line, DITy,
                                       Optimized, Flags)
    : DBuilder.createAutoVariable(Scope, Name, Unit, Line, DITy,
                                  Optimized, Flags);

  // Insert a debug intrinsic into the current block.
  unsigned OffsetInBits = 0;
  auto *BB = Builder.GetInsertBlock();
  bool IsPiece = Storage.size() > 1;
  uint64_t SizeOfByte = CI.getTargetInfo().getCharWidth();
  unsigned VarSizeInBits = getSizeInBits(Var, DIRefMap);
  ElementSizes EltSizes(DITy, DIRefMap, IndirectEnumCases);
  auto Dim = EltSizes.getNext();
  for (llvm::Value *Piece : Storage) {
    // There are variables without storage, such as "struct { func foo() {} }".
    // Emit them as constant 0.
    if (isa<llvm::UndefValue>(Piece))
      Piece = llvm::ConstantInt::get(llvm::Type::getInt64Ty(M.getContext()), 0);

    if (IsPiece) {
      // Try to get the size from the type if possible.
      auto StorageSize = getSizeFromExplosionValue(CI.getTargetInfo(), Piece);
      // FIXME: The TypeInfo for bound generic enum types reports a
      // type <{}> (with size 0) but a concrete instance may still
      // have storage allocated for it. rdar://problem/21470869
      if (!Dim.SizeInBits || (StorageSize && Dim.SizeInBits > StorageSize))
        Dim.SizeInBits = StorageSize;

      // FIXME: Occasionally we miss out that the Storage is acually a
      // refcount wrapper. Silently skip these for now.
      if (OffsetInBits+Dim.SizeInBits > VarSizeInBits)
        break;
      if (OffsetInBits == 0 && Dim.SizeInBits == VarSizeInBits)
        break;
      if (Dim.SizeInBits == 0)
        break;

      assert(Dim.SizeInBits < VarSizeInBits
             && "piece covers entire var");
      assert(OffsetInBits+Dim.SizeInBits <= VarSizeInBits && "pars > totum");
      SmallVector<uint64_t, 3> Elts;
      if (Indirection)
        Elts.push_back(llvm::dwarf::DW_OP_deref);
      Elts.push_back(llvm::dwarf::DW_OP_bit_piece);
      Elts.push_back(OffsetInBits);
      Elts.push_back(Dim.SizeInBits);
      Expr = DBuilder.createExpression(Elts);

      auto Size = Dim.SizeInBits;
      Dim = EltSizes.getNext();
      OffsetInBits +=
        llvm::RoundUpToAlignment(Size, Dim.AlignInBits ? Dim.AlignInBits
                                                       : SizeOfByte);
    }
    emitDbgIntrinsic(BB, Piece, Var, Expr, Line, Loc.Col, Scope, DS);
  }

  // Emit locationless intrinsic for variables that were optimized away.
  if (Storage.size() == 0) {
    auto *undef = llvm::UndefValue::get(DbgTy.StorageType);
    emitDbgIntrinsic(BB, undef, Var, Expr, Line, Loc.Col, Scope, DS);
  }
}

void IRGenDebugInfo::emitDbgIntrinsic(llvm::BasicBlock *BB,
                                      llvm::Value *Storage,
                                      llvm::DILocalVariable *Var,
                                      llvm::DIExpression *Expr, unsigned Line,
                                      unsigned Col, llvm::DILocalScope *Scope,
                                      const SILDebugScope *DS) {
  // Set the location/scope of the intrinsic.
  llvm::MDNode *InlinedAt = nullptr;
  if (DS && DS->InlinedCallSite) {
    assert(Scope && "Inlined location without a lexical scope");
    InlinedAt = createInlinedAt(DS);
  }
  auto DL = llvm::DebugLoc::get(Line, Col, Scope, InlinedAt);
  // An alloca may only be described by exactly one dbg.declare.
  if (isa<llvm::AllocaInst>(Storage) && llvm::FindAllocaDbgDeclare(Storage))
    return;

  // A dbg.declare is only meaningful if there is a single alloca for
  // the variable that is live throughout the function. With SIL
  // optimizations this is not guranteed and a variable can end up in
  // two allocas (for example, one function inlined twice).
  if (!Opts.Optimize &&
      (isa<llvm::AllocaInst>(Storage) ||
       isa<llvm::UndefValue>(Storage)))
    DBuilder.insertDeclare(Storage, Var, Expr, DL, BB);
  else
    DBuilder.insertDbgValueIntrinsic(Storage, 0, Var, Expr, DL, BB);
}


void IRGenDebugInfo::emitGlobalVariableDeclaration(llvm::GlobalValue *Var,
                                                   StringRef Name,
                                                   StringRef LinkageName,
                                                   DebugTypeInfo DbgTy,
                                                   Optional<SILLocation> Loc) {
  if (Opts.DebugInfoKind == IRGenDebugInfoKind::LineTables)
    return;

  llvm::DIType *Ty = getOrCreateType(DbgTy);
  if (Ty->isArtificial() || Ty == InternalType ||
      Var->getVisibility() == llvm::GlobalValue::HiddenVisibility)
    // FIXME: Really these should be marked as artificial, but LLVM
    // currently has no support for flags to be put on global
    // variables. In the mean time, elide these variables, they
    // would confuse both the user and LLDB.
    return;

  Location L = getStartLocation(SM, Loc);
  auto File = getOrCreateFile(L.Filename);

  // Emit it as global variable of the current module.
  DBuilder.createGlobalVariable(MainModule, Name, LinkageName, File, L.Line, Ty,
                                Var->hasInternalLinkage(), Var, nullptr);
}

/// Return the mangled name of any nominal type, including the global
/// _Tt prefix, which marks the Swift namespace for types in DWARF.
StringRef IRGenDebugInfo::getMangledName(DebugTypeInfo DbgTy) {
  if (MetadataTypeDecl && DbgTy.getDecl() == MetadataTypeDecl)
    return BumpAllocatedString(DbgTy.getDecl()->getName().str());

  llvm::SmallString<160> Buffer;
  {
    llvm::raw_svector_ostream S(Buffer);
    Mangle::Mangler M(S, /* DWARF */ true);
    M.mangleTypeForDebugger(DbgTy.getType(), DbgTy.getDeclContext());
  }
  assert(!Buffer.empty() && "mangled name came back empty");
  return BumpAllocatedString(Buffer);
}

/// Create a member of a struct, class, tuple, or enum.
llvm::DIDerivedType *
IRGenDebugInfo::createMemberType(DebugTypeInfo DbgTy, StringRef Name,
                                 unsigned &OffsetInBits, llvm::DIScope *Scope,
                                 llvm::DIFile *File, unsigned Flags) {
  unsigned SizeOfByte = CI.getTargetInfo().getCharWidth();
  auto *Ty = getOrCreateType(DbgTy);
  auto *DITy = DBuilder.createMemberType(
      Scope, Name, File, 0, SizeOfByte * DbgTy.size.getValue(),
      SizeOfByte * DbgTy.align.getValue(), OffsetInBits, Flags, Ty);
  OffsetInBits += getSizeInBits(Ty, DIRefMap);
  OffsetInBits = llvm::RoundUpToAlignment(OffsetInBits,
                                          SizeOfByte * DbgTy.align.getValue());
  return DITy;
}

/// Return an array with the DITypes for each of a tuple's elements.
llvm::DINodeArray IRGenDebugInfo::getTupleElements(
    TupleType *TupleTy, llvm::DIScope *Scope, llvm::DIFile *File,
    unsigned Flags, DeclContext *DeclContext, unsigned &SizeInBits) {
  SmallVector<llvm::Metadata *, 16> Elements;
  unsigned OffsetInBits = 0;
  for (auto ElemTy : TupleTy->getElementTypes()) {
    auto &elemTI =
      IGM.getTypeInfoForUnlowered(AbstractionPattern(CurGenerics,
                                                  ElemTy->getCanonicalType()),
                                  ElemTy);
    DebugTypeInfo DbgTy(ElemTy, elemTI, DeclContext);
    Elements.push_back(
        createMemberType(DbgTy, StringRef(), OffsetInBits, Scope, File, Flags));
  }
  SizeInBits = OffsetInBits;
  return DBuilder.getOrCreateArray(Elements);
}

/// Return an array with the DITypes for each of a struct's elements.
llvm::DINodeArray
IRGenDebugInfo::getStructMembers(NominalTypeDecl *D, Type BaseTy,
                                 llvm::DIScope *Scope, llvm::DIFile *File,
                                 unsigned Flags, unsigned &SizeInBits) {
  SmallVector<llvm::Metadata *, 16> Elements;
  unsigned OffsetInBits = 0;
  for (VarDecl *VD : D->getStoredProperties()) {
    auto memberTy =
        BaseTy->getTypeOfMember(IGM.SILMod->getSwiftModule(), VD, nullptr);
    DebugTypeInfo DbgTy(VD, IGM.getTypeInfoForUnlowered(
                                IGM.SILMod->Types.getAbstractionPattern(VD),
                                memberTy));
    Elements.push_back(createMemberType(DbgTy, VD->getName().str(),
                                        OffsetInBits, Scope, File, Flags));
  }
  if (OffsetInBits > SizeInBits)
    SizeInBits = OffsetInBits;
  return DBuilder.getOrCreateArray(Elements);
}

/// Create a temporary forward declaration for a struct and add it to
/// the type cache so we can safely build recursive types.
llvm::DICompositeType *IRGenDebugInfo::createStructType(
    DebugTypeInfo DbgTy, NominalTypeDecl *Decl, Type BaseTy,
    llvm::DIScope *Scope, llvm::DIFile *File, unsigned Line,
    unsigned SizeInBits, unsigned AlignInBits, unsigned Flags,
    llvm::DIType *DerivedFrom, unsigned RuntimeLang, StringRef UniqueID) {
  StringRef Name = Decl->getName().str();

  // Forward declare this first because types may be recursive.
  auto FwdDecl = llvm::TempDIType(
    DBuilder.createReplaceableCompositeType(
      llvm::dwarf::DW_TAG_structure_type, Name, Scope, File, Line,
        llvm::dwarf::DW_LANG_Swift, SizeInBits, AlignInBits, Flags, UniqueID));

#ifndef NDEBUG
  if (UniqueID.empty())
    assert(!Name.empty() && "no mangled name and no human readable name given");
  else
    assert(UniqueID.size() > 2 && UniqueID[0] == '_' && UniqueID[1] == 'T' &&
           "UID is not a mangled name");
#endif

  auto TH = llvm::TrackingMDNodeRef(FwdDecl.get());
  DITypeCache[DbgTy.getType()] = TH;
  auto Members = getStructMembers(Decl, BaseTy, Scope, File, Flags, SizeInBits);
  auto DITy = DBuilder.createStructType(
      Scope, Name, File, Line, SizeInBits, AlignInBits, Flags, DerivedFrom,
      Members, RuntimeLang, nullptr, UniqueID);
  DBuilder.replaceTemporary(std::move(FwdDecl), DITy);
  return DITy;
}

/// Return an array with the DITypes for each of an enum's elements.
llvm::DINodeArray IRGenDebugInfo::getEnumElements(DebugTypeInfo DbgTy,
                                                  EnumDecl *D,
                                                  llvm::DIScope *Scope,
                                                  llvm::DIFile *File,
                                                  unsigned Flags) {
  SmallVector<llvm::Metadata *, 16> Elements;

  for (auto *ElemDecl : D->getAllElements()) {
    // FIXME <rdar://problem/14845818> Support enums.
    // Swift Enums can be both like DWARF enums and DWARF unions.
    // They should probably be emitted as DW_TAG_variant_type.
    if (ElemDecl->hasType()) {
      // Use Decl as DeclContext.
      DebugTypeInfo ElemDbgTy;
      if (ElemDecl->hasArgumentType())
        ElemDbgTy = DebugTypeInfo(ElemDecl->getArgumentType(),
                                  DbgTy.StorageType,
                                  DbgTy.size, DbgTy.align, D);
      else
        if (D->hasRawType())
          ElemDbgTy = DebugTypeInfo(D->getRawType(), DbgTy.StorageType,
                                    DbgTy.size, DbgTy.align, D);
        else
          // Fallback to Int as the element type.
          ElemDbgTy = DebugTypeInfo(IGM.Context.getIntDecl()->getDeclaredType(),
                                    DbgTy.StorageType,
                                    DbgTy.size, DbgTy.align, D);
      unsigned Offset = 0;
      auto MTy = createMemberType(ElemDbgTy, ElemDecl->getName().str(), Offset,
                                  Scope, File, Flags);
      Elements.push_back(MTy);
      if (D->isIndirect() || ElemDecl->isIndirect())
        IndirectEnumCases.insert(MTy);
    }
  }
  return DBuilder.getOrCreateArray(Elements);
}

/// Create a temporary forward declaration for an enum and add it to
/// the type cache so we can safely build recursive types.
llvm::DICompositeType *IRGenDebugInfo::createEnumType(
    DebugTypeInfo DbgTy, EnumDecl *Decl, StringRef MangledName,
    llvm::DIScope *Scope, llvm::DIFile *File, unsigned Line, unsigned Flags) {
  unsigned SizeOfByte = CI.getTargetInfo().getCharWidth();
  unsigned SizeInBits = DbgTy.size.getValue() * SizeOfByte;
  unsigned AlignInBits = DbgTy.align.getValue() * SizeOfByte;

  // FIXME: Is DW_TAG_union_type the right thing here?
  // Consider using a DW_TAG_variant_type instead.
  auto FwdDecl = llvm::TempDIType(
    DBuilder.createReplaceableCompositeType(
      llvm::dwarf::DW_TAG_union_type, MangledName, Scope, File, Line,
        llvm::dwarf::DW_LANG_Swift, SizeInBits, AlignInBits, Flags,
        MangledName));

  auto TH = llvm::TrackingMDNodeRef(FwdDecl.get());
  DITypeCache[DbgTy.getType()] = TH;

  auto DITy = DBuilder.createUnionType(
      Scope, Decl->getName().str(), File, Line, SizeInBits, AlignInBits, Flags,
      getEnumElements(DbgTy, Decl, Scope, File, Flags),
      llvm::dwarf::DW_LANG_Swift, MangledName);

  DBuilder.replaceTemporary(std::move(FwdDecl), DITy);
  return DITy;
}

/// Return a DIType for Ty reusing any DeclContext found in DbgTy.
llvm::DIType *IRGenDebugInfo::getOrCreateDesugaredType(Type Ty,
                                                       DebugTypeInfo DbgTy) {
  DebugTypeInfo BlandDbgTy(Ty, DbgTy.StorageType, DbgTy.size, DbgTy.align,
                           DbgTy.getDeclContext());
  return getOrCreateType(BlandDbgTy);
}

uint64_t IRGenDebugInfo::getSizeOfBasicType(DebugTypeInfo DbgTy) {
  uint64_t SizeOfByte = CI.getTargetInfo().getCharWidth();
  uint64_t BitWidth = DbgTy.size.getValue() * SizeOfByte;
  llvm::Type *StorageType = DbgTy.StorageType
                                ? DbgTy.StorageType
                                : IGM.DataLayout.getSmallestLegalIntType(
                                      IGM.getLLVMContext(), BitWidth);

  if (StorageType)
    return IGM.DataLayout.getTypeSizeInBits(StorageType);

  // This type is too large to fit in a register.
  assert(BitWidth > IGM.DataLayout.getLargestLegalIntTypeSize());
  return BitWidth;
}

/// Convenience function that creates a forward declaration for PointeeTy.
llvm::DIType *IRGenDebugInfo::createPointerSizedStruct(
    llvm::DIScope *Scope, StringRef Name, llvm::DIFile *File, unsigned Line,
    unsigned Flags, StringRef MangledName) {
  auto FwdDecl = DBuilder.createForwardDecl(llvm::dwarf::DW_TAG_structure_type,
                                            Name, Scope, File, Line,
                                            llvm::dwarf::DW_LANG_Swift, 0, 0);
  return createPointerSizedStruct(Scope, Name, FwdDecl, File, Line, Flags,
                                  MangledName);
}

/// Create a pointer-sized struct with a mangled name and a single
/// member of PointeeTy.
llvm::DIType *IRGenDebugInfo::createPointerSizedStruct(
    llvm::DIScope *Scope, StringRef Name, llvm::DIType *PointeeTy,
    llvm::DIFile *File, unsigned Line, unsigned Flags, StringRef MangledName) {
  unsigned PtrSize = CI.getTargetInfo().getPointerWidth(0);
  unsigned PtrAlign = CI.getTargetInfo().getPointerAlign(0);
  auto PtrTy = DBuilder.createPointerType(PointeeTy, PtrSize, PtrAlign);
  llvm::Metadata *Elements[] = {
    DBuilder.createMemberType(Scope, "pointer", File, 0,
                              PtrSize, PtrAlign, 0, Flags, PtrTy)
  };
  return DBuilder.createStructType(
      Scope, Name, File, Line, PtrSize, PtrAlign, Flags,
      nullptr, // DerivedFrom
      DBuilder.getOrCreateArray(Elements), llvm::dwarf::DW_LANG_Swift,
      nullptr, MangledName);
}

/// Construct a DIType from a DebugTypeInfo object.
///
/// At this point we do not plan to emit full DWARF for all swift
/// types, the goal is to emit only the name and provenance of the
/// type, where possible. A can import the type definition directly
/// from the module/framework/source file the type is specified in.
/// For this reason we emit the fully qualified (=mangled) name for
/// each type whenever possible.
///
/// The ultimate goal is to emit something like a
/// DW_TAG_APPLE_ast_ref_type (an external reference) instead of a
/// local reference to the type.
llvm::DIType *IRGenDebugInfo::createType(DebugTypeInfo DbgTy,
                                         StringRef MangledName,
                                         llvm::DIScope *Scope,
                                         llvm::DIFile *File) {
  // FIXME: For SizeInBits, clang uses the actual size of the type on
  // the target machine instead of the storage size that is alloca'd
  // in the LLVM IR. For all types that are boxed in a struct, we are
  // emitting the storage size of the struct, but it may be necessary
  // to emit the (target!) size of the underlying basic type.
  uint64_t SizeOfByte = CI.getTargetInfo().getCharWidth();
  uint64_t SizeInBits = DbgTy.size.getValue() * SizeOfByte;
  // Prefer the actual storage size over the DbgTy.
  if (DbgTy.StorageType && DbgTy.StorageType->isSized()) {
    uint64_t Storage = IGM.DataLayout.getTypeSizeInBits(DbgTy.StorageType);
    if (Storage)
      SizeInBits = Storage;
  }
  uint64_t AlignInBits = DbgTy.align.getValue() * SizeOfByte;
  unsigned Encoding = 0;
  unsigned Flags = 0;

  TypeBase *BaseTy = DbgTy.getType();

  if (!BaseTy) {
    DEBUG(llvm::dbgs() << "Type without TypeBase: "; DbgTy.getType()->dump();
          llvm::dbgs() << "\n");
    if (!InternalType) {
      StringRef Name = "<internal>";
      InternalType = DBuilder.createForwardDecl(
          llvm::dwarf::DW_TAG_structure_type, Name, Scope, File,
          /*Line*/ 0, llvm::dwarf::DW_LANG_Swift, SizeInBits, AlignInBits);
    }
    return InternalType;
  }

  // Here goes!
  switch (BaseTy->getKind()) {
  case TypeKind::BuiltinInteger: {
    Encoding = llvm::dwarf::DW_ATE_unsigned;
    SizeInBits = getSizeOfBasicType(DbgTy);
    break;
  }

  case TypeKind::BuiltinFloat: {
    auto *FloatTy = BaseTy->castTo<BuiltinFloatType>();
    // Assuming that the bitwidth and FloatTy->getFPKind() are identical.
    SizeInBits = FloatTy->getBitWidth();
    Encoding = llvm::dwarf::DW_ATE_float;
    break;
  }

  case TypeKind::BuiltinUnknownObject: {
    // The builtin opaque Objective-C pointer type. Useful for pushing
    // an Objective-C type through swift.
    unsigned PtrSize = CI.getTargetInfo().getPointerWidth(0);
    unsigned PtrAlign = CI.getTargetInfo().getPointerAlign(0);
    auto IdTy = DBuilder.createForwardDecl(
      llvm::dwarf::DW_TAG_structure_type, MangledName, Scope, File, 0,
        llvm::dwarf::DW_LANG_ObjC, 0, 0);
    return DBuilder.createPointerType(IdTy, PtrSize, PtrAlign, MangledName);
  }

  case TypeKind::BuiltinNativeObject: {
    unsigned PtrSize = CI.getTargetInfo().getPointerWidth(0);
    unsigned PtrAlign = CI.getTargetInfo().getPointerAlign(0);
    auto PTy = DBuilder.createPointerType(nullptr, PtrSize, PtrAlign,
                                          MangledName);
    return DBuilder.createObjectPointerType(PTy);
  }

  case TypeKind::BuiltinBridgeObject: {
    unsigned PtrSize = CI.getTargetInfo().getPointerWidth(0);
    unsigned PtrAlign = CI.getTargetInfo().getPointerAlign(0);
    auto PTy = DBuilder.createPointerType(nullptr, PtrSize, PtrAlign,
                                          MangledName);
    return DBuilder.createObjectPointerType(PTy);
  }

  case TypeKind::BuiltinRawPointer: {
    unsigned PtrSize = CI.getTargetInfo().getPointerWidth(0);
    unsigned PtrAlign = CI.getTargetInfo().getPointerAlign(0);
    return DBuilder.createPointerType(nullptr, PtrSize, PtrAlign,
                                      MangledName);
  }

  case TypeKind::DynamicSelf: {
    // Self. We don't have a way to represent instancetype in DWARF,
    // so we emit the static type instead. This is similar to what we
    // do with instancetype in Objective-C.
    auto *DynamicSelfTy = BaseTy->castTo<DynamicSelfType>();
    auto SelfTy = getOrCreateDesugaredType(DynamicSelfTy->getSelfType(), DbgTy);
    return DBuilder.createTypedef(SelfTy, MangledName, File, 0, File);

  }

  // Even builtin swift types usually come boxed in a struct.
  case TypeKind::Struct: {
    auto *StructTy = BaseTy->castTo<StructType>();
    auto *Decl = StructTy->getDecl();
    Location L = getLoc(SM, Decl);
    return createStructType(DbgTy, Decl, StructTy, Scope,
                            getOrCreateFile(L.Filename), L.Line, SizeInBits,
                            AlignInBits, Flags,
                            nullptr, // DerivedFrom
                            llvm::dwarf::DW_LANG_Swift, MangledName);
  }

  case TypeKind::Class: {
    // Classes are represented as DW_TAG_structure_type. This way the
    // DW_AT_APPLE_runtime_class( DW_LANG_Swift ) attribute can be
    // used to differentiate them from C++ and ObjC classes.
    auto *ClassTy = BaseTy->castTo<ClassType>();
    auto *Decl = ClassTy->getDecl();
    Location L = getLoc(SM, Decl);
    if (auto *ClangDecl = Decl->getClangDecl()) {
      auto ClangSrcLoc = ClangDecl->getLocStart();
      clang::SourceManager &ClangSM =
          CI.getClangASTContext().getSourceManager();
      L.Line = ClangSM.getPresumedLineNumber(ClangSrcLoc);
      L.Filename = ClangSM.getBufferName(ClangSrcLoc);

      // Use "__ObjC" as default for implicit decls.
      // FIXME: Do something more clever based on the decl's mangled name.
      StringRef ModulePath;
      StringRef ModuleName = "__ObjC";
      if (auto *OwningModule = ClangDecl->getImportedOwningModule())
        ModuleName = OwningModule->getTopLevelModuleName();

      if (auto *SwiftModule = Decl->getParentModule())
        if (auto *ClangModule = SwiftModule->findUnderlyingClangModule()) {
          // FIXME: Clang submodules are not handled here.
          // FIXME: Clang module config macros are not handled here.
          ModuleName = ClangModule->getFullModuleName();
          // FIXME: A clang module's Directory is supposed to be the
          // directory containing the module map, but ClangImporter
          // sets it to the module cache directory.
          if (ClangModule->Directory)
            ModulePath = ClangModule->Directory->getName();
        }
      Scope = getOrCreateModule(ModuleName, TheCU, ModuleName, ModulePath);
    }
    return createPointerSizedStruct(Scope, Decl->getNameStr(),
                                    getOrCreateFile(L.Filename), L.Line, Flags,
                                    MangledName);
  }

  case TypeKind::Protocol: {
    auto *ProtocolTy = BaseTy->castTo<ProtocolType>();
    auto *Decl = ProtocolTy->getDecl();
    // FIXME: (LLVM branch) This should probably be a DW_TAG_interface_type.
    Location L = getLoc(SM, Decl);
    auto File = getOrCreateFile(L.Filename);
    return createPointerSizedStruct(Scope,
                                    Decl ? Decl->getNameStr() : MangledName,
                                    File, L.Line, Flags, MangledName);
  }

  case TypeKind::ProtocolComposition: {
    auto *Decl = DbgTy.getDecl();
    Location L = getLoc(SM, Decl);
    auto File = getOrCreateFile(L.Filename);

    // FIXME: emit types
    // auto ProtocolCompositionTy = BaseTy->castTo<ProtocolCompositionType>();
    return createPointerSizedStruct(Scope,
                                    Decl ? Decl->getNameStr() : MangledName,
                                    File, L.Line, Flags, MangledName);
  }

  case TypeKind::UnboundGeneric: {
    auto *UnboundTy = BaseTy->castTo<UnboundGenericType>();
    auto *Decl = UnboundTy->getDecl();
    Location L = getLoc(SM, Decl);
    return createPointerSizedStruct(Scope,
                                    Decl ? Decl->getNameStr() : MangledName,
                                    File, L.Line, Flags, MangledName);
  }

  case TypeKind::BoundGenericStruct: {
    auto *StructTy = BaseTy->castTo<BoundGenericStructType>();
    auto *Decl = StructTy->getDecl();
    Location L = getLoc(SM, Decl);
    return createPointerSizedStruct(Scope,
                                    Decl ? Decl->getNameStr() : MangledName,
                                    File, L.Line, Flags, MangledName);
  }

  case TypeKind::BoundGenericClass: {
    auto *ClassTy = BaseTy->castTo<BoundGenericClassType>();
    auto *Decl = ClassTy->getDecl();
    Location L = getLoc(SM, Decl);
    // TODO: We may want to peek at Decl->isObjC() and set this
    // attribute accordingly.
    return createPointerSizedStruct(Scope,
                                    Decl ? Decl->getNameStr() : MangledName,
                                    File, L.Line, Flags, MangledName);
  }

  case TypeKind::Tuple: {
    auto *TupleTy = BaseTy->castTo<TupleType>();
    // Tuples are also represented as structs.
    auto FwdDecl = llvm::TempDINode(
      DBuilder.createReplaceableCompositeType(
        llvm::dwarf::DW_TAG_structure_type, MangledName, Scope, File, 0,
          llvm::dwarf::DW_LANG_Swift, SizeInBits, AlignInBits, Flags,
          MangledName));
    
    DITypeCache[DbgTy.getType()] = llvm::TrackingMDNodeRef(FwdDecl.get());

    unsigned RealSize;
    auto Elements = getTupleElements(TupleTy, Scope, MainFile, Flags,
                                     DbgTy.getDeclContext(), RealSize);
    // FIXME: Handle %swift.opaque members and make this into an assertion.
    if (!RealSize)
      RealSize = SizeInBits;
    auto DITy = DBuilder.createStructType(
        Scope, MangledName, File, 0, RealSize, AlignInBits, Flags,
        nullptr, // DerivedFrom
        Elements, llvm::dwarf::DW_LANG_Swift, nullptr, MangledName);

    DBuilder.replaceTemporary(std::move(FwdDecl), DITy);
    return DITy;
  }

  case TypeKind::InOut: {
    // This is an inout type. Naturally we would be emitting them as
    // DW_TAG_reference_type types, but LLDB can deal better with pointer-sized
    // struct that has the appropriate mangled name.
    auto ObjectTy = BaseTy->castTo<InOutType>()->getObjectType();
    auto DT = getOrCreateDesugaredType(ObjectTy, DbgTy);
    return createPointerSizedStruct(Scope, MangledName, DT, File, 0, Flags,
                                    BaseTy->isUnspecializedGeneric()
                                    ? StringRef() : MangledName);
  }

  case TypeKind::Archetype: {
    auto *Archetype = BaseTy->castTo<ArchetypeType>();
    Location L = getLoc(SM, Archetype->getAssocType());
    auto Superclass = Archetype->getSuperclass();
    auto DerivedFrom = Superclass.isNull()
                           ? nullptr
                           : getOrCreateDesugaredType(Superclass, DbgTy);
    auto FwdDecl = llvm::TempDIType(
      DBuilder.createReplaceableCompositeType(
        llvm::dwarf::DW_TAG_structure_type, MangledName, Scope, File, L.Line,
          llvm::dwarf::DW_LANG_Swift, SizeInBits, AlignInBits, Flags,
          MangledName));

    // Emit the protocols the archetypes conform to.
    SmallVector<llvm::Metadata *, 4> Protocols;
    for (auto *ProtocolDecl : Archetype->getConformsTo()) {
      auto PTy = IGM.SILMod->Types.getLoweredType(ProtocolDecl->getType())
                     .getSwiftRValueType();
      auto PDbgTy = DebugTypeInfo(ProtocolDecl, IGM.getTypeInfoForLowered(PTy));
      auto PDITy = getOrCreateType(PDbgTy);
      Protocols.push_back(DBuilder.createInheritance(FwdDecl.get(),
                                                     PDITy, 0, Flags));
    }
    auto DITy = DBuilder.createStructType(
        Scope, MangledName, File, L.Line, SizeInBits, AlignInBits, Flags,
        DerivedFrom, DBuilder.getOrCreateArray(Protocols),
        llvm::dwarf::DW_LANG_Swift, nullptr,
        MangledName);

    DBuilder.replaceTemporary(std::move(FwdDecl), DITy);
    return DITy;
  }

  case TypeKind::ExistentialMetatype:
  case TypeKind::Metatype: {
    // Metatypes are (mostly) singleton type descriptors, often without storage.
    Flags |= llvm::DINode::FlagArtificial;
    Location L = getLoc(SM, DbgTy.getDecl());
    auto File = getOrCreateFile(L.Filename);
    return DBuilder.createStructType(
        Scope, MangledName, File, L.Line, SizeInBits, AlignInBits, Flags,
        nullptr, nullptr, llvm::dwarf::DW_LANG_Swift,
        nullptr, MangledName);
  }

  case TypeKind::SILFunction:
  case TypeKind::Function:
  case TypeKind::PolymorphicFunction:
  case TypeKind::GenericFunction: {
    auto FwdDecl = llvm::TempDINode(
      DBuilder.createReplaceableCompositeType(
        llvm::dwarf::DW_TAG_subroutine_type, MangledName, Scope, File, 0,
          llvm::dwarf::DW_LANG_Swift, SizeInBits, AlignInBits, Flags,
          MangledName));
     
    auto TH = llvm::TrackingMDNodeRef(FwdDecl.get());
    DITypeCache[DbgTy.getType()] = TH;

    CanSILFunctionType FunctionTy;
    if (auto *SILFnTy = dyn_cast<SILFunctionType>(BaseTy))
      FunctionTy = CanSILFunctionType(SILFnTy);
    // FIXME: Handling of generic parameters in SIL type lowering is in flux.
    // DebugInfo doesn't appear to care about the generic context, so just
    // throw it away before lowering.
    else if (isa<GenericFunctionType>(BaseTy) ||
             isa<PolymorphicFunctionType>(BaseTy)) {
      auto *fTy = cast<AnyFunctionType>(BaseTy);
      auto *nongenericTy = FunctionType::get(fTy->getInput(), fTy->getResult(),
                                            fTy->getExtInfo());

      FunctionTy = IGM.SILMod->Types.getLoweredType(nongenericTy)
                       .castTo<SILFunctionType>();
    } else
      FunctionTy =
          IGM.SILMod->Types.getLoweredType(BaseTy).castTo<SILFunctionType>();
    auto Params = createParameterTypes(FunctionTy, DbgTy.getDeclContext());

    // Functions are actually stored as a Pointer or a FunctionPairTy:
    // { i8*, %swift.refcounted* }
    auto FnTy = DBuilder.createSubroutineType(Params, Flags);
    auto DITy = createPointerSizedStruct(Scope, MangledName, FnTy,
                                         MainFile, 0, Flags, MangledName);
    DBuilder.replaceTemporary(std::move(FwdDecl), DITy);
    return DITy;
  }

  case TypeKind::Enum: {
    auto *EnumTy = BaseTy->castTo<EnumType>();
    auto *Decl = EnumTy->getDecl();
    Location L = getLoc(SM, Decl);
    return createEnumType(DbgTy, Decl, MangledName, Scope,
                          getOrCreateFile(L.Filename), L.Line, Flags);
  }

  case TypeKind::BoundGenericEnum: {
    auto *EnumTy = BaseTy->castTo<BoundGenericEnumType>();
    auto *Decl = EnumTy->getDecl();
    Location L = getLoc(SM, Decl);
    return createEnumType(DbgTy, Decl, MangledName, Scope,
                          getOrCreateFile(L.Filename), L.Line, Flags);
  }

  case TypeKind::BuiltinVector: {
    (void)MangledName; // FIXME emit the name somewhere.
    auto *BuiltinVectorTy = BaseTy->castTo<BuiltinVectorType>();
    DebugTypeInfo ElemDbgTy(BuiltinVectorTy->getElementType(),
                            DbgTy.StorageType,
                            DbgTy.size, DbgTy.align, DbgTy.getDeclContext());
    auto Subscripts = nullptr;
    return DBuilder.createVectorType(BuiltinVectorTy->getNumElements(),
                                     AlignInBits, getOrCreateType(ElemDbgTy),
                                     Subscripts);
  }

  // Reference storage types.
  case TypeKind::UnownedStorage:
  case TypeKind::UnmanagedStorage:
  case TypeKind::WeakStorage: {
    auto *ReferenceTy = cast<ReferenceStorageType>(BaseTy);
    auto CanTy = ReferenceTy->getReferentType();
    Location L = getLoc(SM, DbgTy.getDecl());
    auto File = getOrCreateFile(L.Filename);
    return DBuilder.createTypedef(getOrCreateDesugaredType(CanTy, DbgTy),
                                  MangledName, File, L.Line, File);
  }

  // Sugared types.

  case TypeKind::NameAlias: {

    auto *NameAliasTy = cast<NameAliasType>(BaseTy);
    auto *Decl = NameAliasTy->getDecl();
    Location L = getLoc(SM, Decl);
    auto AliasedTy = Decl->getUnderlyingType();
    auto File = getOrCreateFile(L.Filename);
    // For NameAlias types, the DeclContext for the aliasED type is
    // in the decl of the alias type.
    DebugTypeInfo AliasedDbgTy(AliasedTy, DbgTy.StorageType,
                               DbgTy.size, DbgTy.align, DbgTy.getDeclContext());
    return DBuilder.createTypedef(getOrCreateType(AliasedDbgTy), MangledName,
                                  File, L.Line, File);
  }

  case TypeKind::Substituted: {
    auto OrigTy = cast<SubstitutedType>(BaseTy)->getReplacementType();
    return getOrCreateDesugaredType(OrigTy, DbgTy);
  }

  case TypeKind::Paren: {
    auto Ty = cast<ParenType>(BaseTy)->getUnderlyingType();
    return getOrCreateDesugaredType(Ty, DbgTy);
  }

  // SyntaxSugarType derivations.
  case TypeKind::ArraySlice:
  case TypeKind::Optional:
  case TypeKind::ImplicitlyUnwrappedOptional: {
    auto *SyntaxSugarTy = cast<SyntaxSugarType>(BaseTy);
    auto *CanTy = SyntaxSugarTy->getSinglyDesugaredType();
    return getOrCreateDesugaredType(CanTy, DbgTy);
  }

  case TypeKind::Dictionary: {
    auto *DictionaryTy = cast<DictionaryType>(BaseTy);
    auto *CanTy = DictionaryTy->getDesugaredType();
    return getOrCreateDesugaredType(CanTy, DbgTy);
  }
    
  case TypeKind::GenericTypeParam: {
    auto *ParamTy = cast<GenericTypeParamType>(BaseTy);
    // FIXME: Provide a more meaningful debug type.
    return DBuilder.createUnspecifiedType(ParamTy->getName().str());
  }
  case TypeKind::DependentMember: {
    auto *MemberTy = cast<DependentMemberType>(BaseTy);
    // FIXME: Provide a more meaningful debug type.
    return DBuilder.createUnspecifiedType(MemberTy->getName().str());
  }

  // The following types exist primarily for internal use by the type
  // checker.
  case TypeKind::AssociatedType:
  case TypeKind::Error:
  case TypeKind::Unresolved:
  case TypeKind::LValue:
  case TypeKind::TypeVariable:
  case TypeKind::Module:
  case TypeKind::SILBlockStorage:
  case TypeKind::SILBox:
  case TypeKind::BuiltinUnsafeValueBuffer:

    DEBUG(llvm::errs() << "Unhandled type: "; DbgTy.getType()->dump();
          llvm::errs() << "\n");
    MangledName = "<unknown>";
  }
  return DBuilder.createBasicType(MangledName, SizeInBits, AlignInBits,
                                  Encoding);
}

/// Determine if there exists a name mangling for the given type.
static bool canMangle(TypeBase *Ty) {
  switch (Ty->getKind()) {
  case TypeKind::PolymorphicFunction: // Mangler crashes.
  case TypeKind::GenericFunction:     // Not yet supported.
  case TypeKind::SILBlockStorage:     // Not suported at all.
  case TypeKind::SILBox:
    return false;
  case TypeKind::InOut: {
    auto *ObjectTy = Ty->castTo<InOutType>()->getObjectType().getPointer();
    return canMangle(ObjectTy);
  }
  default:
    return true;
  }
}

/// Get the DIType corresponding to this DebugTypeInfo from the cache,
/// or build a fresh DIType otherwise.  There is the underlying
/// assumption that no two types that share the same canonical type
/// can have different storage size or alignment.
llvm::DIType *IRGenDebugInfo::getOrCreateType(DebugTypeInfo DbgTy) {
  // Is this an empty type?
  if (DbgTy.isNull())
    // We can't use the empty type as an index into DenseMap.
    return createType(DbgTy, "", TheCU, MainFile);

  // Look in the cache first.
  auto CachedType = DITypeCache.find(DbgTy.getType());
  if (CachedType != DITypeCache.end()) {
    // Verify that the information still exists.
    if (llvm::Metadata *Val = CachedType->second) {
      auto DITy = cast<llvm::DIType>(Val);
      return DITy;
    }
  }

  // Second line of defense: Look up the mangled name. TypeBase*'s are
  // not necessarily unique, but name mangling is too expensive to do
  // every time.
  StringRef MangledName;
  llvm::MDString *UID = nullptr;
  if (canMangle(DbgTy.getType())) {
    MangledName = getMangledName(DbgTy);
    UID = llvm::MDString::get(IGM.getLLVMContext(), MangledName);
    if (llvm::Metadata *CachedTy = DIRefMap.lookup(UID)) {
      auto DITy = cast<llvm::DIType>(CachedTy);
      return DITy;
    }
  }

  // Retrieve the context of the type, as opposed to the DeclContext
  // of the variable.
  //
  // FIXME: Builtin and qualified types in LLVM have no parent
  // scope. TODO: This can be fixed by extending DIBuilder.
  DeclContext *Context = DbgTy.getType()->getNominalOrBoundGenericNominal();
  if (Context)
    Context = Context->getParent();
  llvm::DIScope *Scope = getOrCreateContext(Context);
  llvm::DIType *DITy = createType(DbgTy, MangledName, Scope, getFile(Scope));

  // Incrementally build the DIRefMap.
  if (auto *CTy = dyn_cast<llvm::DICompositeType>(DITy)) {
#ifndef NDEBUG
    // Sanity check.
    if (llvm::Metadata *V = DIRefMap.lookup(UID)) {
      auto *CachedTy = cast<llvm::DIType>(V);
      assert(CachedTy == DITy && "conflicting types for one UID");
    }
#endif
    // If this type supports a UID, enter it to the cache.
    if (auto UID = CTy->getRawIdentifier()) {
      assert(UID->getString() == MangledName &&
             "Unique identifier is different from mangled name ");
      DIRefMap[UID] = llvm::TrackingMDNodeRef(DITy);
    }
  }

  // Store it in the cache.
  DITypeCache.insert({DbgTy.getType(), llvm::TrackingMDNodeRef(DITy)});

  return DITy;
}

void IRGenDebugInfo::finalize() {
  assert(LocationStack.empty() && "Mismatch of pushLoc() and popLoc().");

  // Finalize the DIBuilder.
  DBuilder.finalize();
}
