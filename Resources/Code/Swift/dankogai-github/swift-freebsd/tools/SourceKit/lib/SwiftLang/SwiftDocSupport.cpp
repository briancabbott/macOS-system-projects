//===--- SwiftDocSupport.cpp ----------------------------------------------===//
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

#include "SwiftASTManager.h"
#include "SwiftEditorDiagConsumer.h"
#include "SwiftLangSupport.h"
#include "SourceKit/Support/UIdent.h"

#include "swift/AST/ASTPrinter.h"
#include "swift/AST/ASTWalker.h"
#include "swift/Frontend/Frontend.h"
#include "swift/Frontend/PrintingDiagnosticConsumer.h"
#include "swift/IDE/CommentConversion.h"
#include "swift/IDE/ModuleInterfacePrinting.h"
#include "swift/IDE/SourceEntityWalker.h"
#include "swift/IDE/SyntaxModel.h"
// This is included only for createLazyResolver(). Move to different header ?
#include "swift/Sema/CodeCompletionTypeChecking.h"
#include "swift/Config.h"

#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"

using namespace SourceKit;
using namespace swift;
using namespace ide;

static Module *getModuleByFullName(ASTContext &Ctx, StringRef ModuleName) {
  SmallVector<std::pair<Identifier, SourceLoc>, 4>
      AccessPath;
  while (!ModuleName.empty()) {
    StringRef SubModuleName;
    std::tie(SubModuleName, ModuleName) = ModuleName.split('.');
    AccessPath.push_back({ Ctx.getIdentifier(SubModuleName), SourceLoc() });
  }
  return Ctx.getModule(AccessPath);
}

static Module *getModuleByFullName(ASTContext &Ctx, Identifier ModuleName) {
  return Ctx.getModule(std::make_pair(ModuleName, SourceLoc()));
}

namespace {
struct TextRange {
  unsigned Offset;
  unsigned Length;
};

struct TextEntity {
  const Decl *Dcl = nullptr;
  StringRef Argument;
  TextRange Range;
  unsigned LocOffset = 0;
  std::vector<TextEntity> SubEntities;

  TextEntity(const Decl *D, unsigned StartOffset)
    : Dcl(D), Range{StartOffset, 0} {}
  TextEntity(const Decl *D, TextRange TR, unsigned LocOffset)
    : Dcl(D), Range(TR), LocOffset(LocOffset) {}
  TextEntity(const Decl *D, StringRef Arg, TextRange TR, unsigned LocOffset)
    : Dcl(D), Argument(Arg), Range(TR), LocOffset(LocOffset) {}
};

struct TextReference {
  const ValueDecl *Dcl = nullptr;
  TextRange Range;
  const Type Ty;

  TextReference(const ValueDecl *D, unsigned Offset, unsigned Length,
                const Type Ty = Type()) : Dcl(D), Range{Offset, Length}, Ty(Ty) {}
};

class AnnotatingPrinter : public StreamPrinter {
public:
  std::vector<TextEntity> TopEntities;
  std::vector<TextEntity> EntitiesStack;
  std::vector<TextReference> References;

  using StreamPrinter::StreamPrinter;

  ~AnnotatingPrinter() {
    assert(EntitiesStack.empty());
  }

  void printDeclPre(const Decl *D) override {
    unsigned StartOffset = OS.tell();
    EntitiesStack.emplace_back(D, StartOffset);
  }

  void printDeclLoc(const Decl *D) override {
    if (EntitiesStack.back().Dcl == D) {
      unsigned LocOffset = OS.tell();
      EntitiesStack.back().LocOffset = LocOffset;
    }
  }

  void printDeclPost(const Decl *D) override {
    assert(EntitiesStack.back().Dcl == D);
    TextEntity Entity = std::move(EntitiesStack.back());
    EntitiesStack.pop_back();
    unsigned EndOffset = OS.tell();
    Entity.Range.Length = EndOffset - Entity.Range.Offset;
    if (EntitiesStack.empty())
      TopEntities.push_back(std::move(Entity));
    else
      EntitiesStack.back().SubEntities.push_back(std::move(Entity));
  }

  void printTypeRef(const TypeDecl *TD, Identifier Name) override {
    unsigned StartOffset = OS.tell();
    References.emplace_back(TD, StartOffset, Name.str().size());
    StreamPrinter::printTypeRef(TD, Name);
  }
};

struct SourceTextInfo {
  std::string Text;
  std::vector<TextEntity> TopEntities;
  std::vector<TextReference> References;
};

}

static void initDocGenericParams(const Decl *D, DocEntityInfo &Info) {
  GenericParamList *GenParams = nullptr;
  if (auto *NTD = dyn_cast<NominalTypeDecl>(D)) {
    GenParams = NTD->getGenericParams();
  } else if (auto *AFD = dyn_cast<AbstractFunctionDecl>(D)) {
    GenParams = AFD->getGenericParams();
  } else if (auto *ExtD = dyn_cast<ExtensionDecl>(D)) {
    GenParams = ExtD->getGenericParams();
  }

  if (!GenParams)
    return;

  for (auto *GP : GenParams->getParams()) {
    if (GP->isImplicit())
      continue;
    DocGenericParam Param;
    Param.Name = GP->getNameStr();
    if (!GP->getInherited().empty()) {
      llvm::raw_string_ostream OS(Param.Inherits);
      GP->getInherited()[0].getType().print(OS);
    }

    Info.GenericParams.push_back(Param);
  }
  for (auto &Req : GenParams->getRequirements()) {
    std::string ReqStr;
    llvm::raw_string_ostream OS(ReqStr);
    Req.printAsWritten(OS);
    OS.flush();
    Info.GenericRequirements.push_back(std::move(ReqStr));
  }
}

static bool initDocEntityInfo(const Decl *D, bool IsRef, DocEntityInfo &Info,
                              StringRef Arg = StringRef()) {
  if (!D || isa<ParamDecl>(D) ||
      (isa<VarDecl>(D) && D->getDeclContext()->isLocalContext())) {
    Info.Kind = SwiftLangSupport::getUIDForLocalVar(IsRef);
    if (D) {
      llvm::raw_svector_ostream OS(Info.Name);
      SwiftLangSupport::printDisplayName(cast<ValueDecl>(D), OS);
    } else {
      Info.Name = "_";
    }

    if (!Arg.empty())
      Info.Argument = Arg.str();

    return false;
  }

  Info.Kind = SwiftLangSupport::getUIDForDecl(D, IsRef);
  if (Info.Kind.isInvalid())
    return true;
  if (const ValueDecl *VD = dyn_cast<ValueDecl>(D)) {
    llvm::raw_svector_ostream NameOS(Info.Name);
    SwiftLangSupport::printDisplayName(VD, NameOS);

    llvm::raw_svector_ostream OS(Info.USR);
    SwiftLangSupport::printUSR(VD, OS);
  }
  Info.IsUnavailable = AvailableAttr::isUnavailable(D);
  Info.IsDeprecated = D->getAttrs().getDeprecated(D->getASTContext()) != nullptr;

  if (!IsRef) {
    llvm::raw_svector_ostream OS(Info.DocComment);
    ide::getDocumentationCommentAsXML(D, OS);

    initDocGenericParams(D, Info);
  }

  return false;
}

static bool initDocEntityInfo(const TextEntity &Entity,
                              DocEntityInfo &Info) {
  if (initDocEntityInfo(Entity.Dcl, /*IsRef=*/false, Info, Entity.Argument))
    return true;
  Info.Offset = Entity.Range.Offset;
  Info.Length = Entity.Range.Length;
  return false;
}

static const TypeDecl *getTypeDeclFromType(Type Ty) {
  if (auto Alias = dyn_cast<NameAliasType>(Ty.getPointer()))
    return Alias->getDecl();
  return Ty->getAnyNominal();
}

static void passInherits(const ValueDecl *D, DocInfoConsumer &Consumer) {
  DocEntityInfo EntInfo;
  if (initDocEntityInfo(D, /*IsRef=*/true, EntInfo))
    return;
  Consumer.handleInheritsEntity(EntInfo);
}
static void passConforms(const ValueDecl *D, DocInfoConsumer &Consumer) {
  DocEntityInfo EntInfo;
  if (initDocEntityInfo(D, /*IsRef=*/true, EntInfo))
    return;
  Consumer.handleConformsToEntity(EntInfo);
}
static void passInherits(ArrayRef<TypeLoc> InheritedTypes,
                         DocInfoConsumer &Consumer) {
  for (auto Inherited : InheritedTypes) {
    if (!Inherited.getType())
      continue;

    if (auto Proto = Inherited.getType()->getAs<ProtocolType>()) {
      passConforms(Proto->getDecl(), Consumer);
      continue;
    }

    if (auto ProtoComposition
               = Inherited.getType()->getAs<ProtocolCompositionType>()) {
      for (auto T : ProtoComposition->getProtocols())
        passInherits(TypeLoc::withoutLoc(T), Consumer);
      continue;
    }

    if (auto TD = getTypeDeclFromType(Inherited.getType())) {
      passInherits(TD, Consumer);
      continue;
    }
  }
}

static void passConforms(ArrayRef<ValueDecl *> Dcls,
                         DocInfoConsumer &Consumer) {
  for (auto D : Dcls)
    passConforms(D, Consumer);
}
static void passExtends(const ValueDecl *D, DocInfoConsumer &Consumer) {
  DocEntityInfo EntInfo;
  if (initDocEntityInfo(D, /*IsRef=*/true, EntInfo))
    return;
  Consumer.handleExtendsEntity(EntInfo);
}

static void reportRelated(ASTContext &Ctx,
                          const Decl *D,
                          DocInfoConsumer &Consumer) {
  if (!D || isa<ParamDecl>(D))
    return;
  if (const ExtensionDecl *ED = dyn_cast<ExtensionDecl>(D)) {
    if (Type T = ED->getExtendedType())
      if (auto TD = getTypeDeclFromType(T))
        passExtends(TD, Consumer);

    passInherits(ED->getInherited(), Consumer);

  } else if (const TypeDecl *TD = dyn_cast<TypeDecl>(D)) {
    passInherits(TD->getInherited(), Consumer);
    passConforms(TD->getSatisfiedProtocolRequirements(/*Sorted=*/true),
                 Consumer);
  } else if (auto *VD = dyn_cast<ValueDecl>(D)) {
    if (auto Overridden = VD->getOverriddenDecl())
      passInherits(Overridden, Consumer);
    passConforms(VD->getSatisfiedProtocolRequirements(/*Sorted=*/true),
                 Consumer);
  }
}

// Only reports @available.
// FIXME: Handle all attributes.
static void reportAttributes(ASTContext &Ctx,
                             const Decl *D,
                             DocInfoConsumer &Consumer) {
  static UIdent AvailableAttrKind("source.lang.swift.attribute.availability");
  static UIdent PlatformIOS("source.availability.platform.ios");
  static UIdent PlatformOSX("source.availability.platform.osx");
  static UIdent PlatformtvOS("source.availability.platform.tvos");
  static UIdent PlatformWatchOS("source.availability.platform.watchos");
  static UIdent PlatformIOSAppExt("source.availability.platform.ios_app_extension");
  static UIdent PlatformOSXAppExt("source.availability.platform.osx_app_extension");
  static UIdent PlatformtvOSAppExt("source.availability.platform.tvos_app_extension");
  static UIdent PlatformWatchOSAppExt("source.availability.platform.watchos_app_extension");

  for (auto Attr : D->getAttrs()) {
    if (auto Av = dyn_cast<AvailableAttr>(Attr)) {
      UIdent PlatformUID;
      switch (Av->Platform) {
      case PlatformKind::none:
        PlatformUID = UIdent(); break;
      case PlatformKind::iOS:
        PlatformUID = PlatformIOS; break;
      case PlatformKind::OSX:
        PlatformUID = PlatformOSX; break;
      case PlatformKind::tvOS:
        PlatformUID = PlatformtvOS; break;
      case PlatformKind::watchOS:
        PlatformUID = PlatformWatchOS; break;
      case PlatformKind::iOSApplicationExtension:
        PlatformUID = PlatformIOSAppExt; break;
      case PlatformKind::OSXApplicationExtension:
        PlatformUID = PlatformOSXAppExt; break;
      case PlatformKind::tvOSApplicationExtension:
        PlatformUID = PlatformtvOSAppExt; break;
      case PlatformKind::watchOSApplicationExtension:
        PlatformUID = PlatformWatchOSAppExt; break;
      }

      AvailableAttrInfo Info;
      Info.AttrKind = AvailableAttrKind;
      Info.IsUnavailable = Av->isUnconditionallyUnavailable();
      Info.IsDeprecated = Av->isUnconditionallyDeprecated();
      Info.Platform = PlatformUID;
      Info.Message = Av->Message;
      if (Av->Introduced)
        Info.Introduced = *Av->Introduced;
      if (Av->Deprecated)
        Info.Deprecated = *Av->Deprecated;
      if (Av->Obsoleted)
        Info.Obsoleted = *Av->Obsoleted;

      Consumer.handleAvailableAttribute(Info);
    }
  }
}

static void reportDocEntities(ASTContext &Ctx,
                              ArrayRef<TextEntity> Entities,
                              DocInfoConsumer &Consumer) {
  for (auto &Entity : Entities) {
    DocEntityInfo EntInfo;
    if (initDocEntityInfo(Entity, EntInfo))
      continue;
    Consumer.startSourceEntity(EntInfo);
    reportRelated(Ctx, Entity.Dcl, Consumer);
    reportDocEntities(Ctx, Entity.SubEntities, Consumer);
    reportAttributes(Ctx, Entity.Dcl, Consumer);
    Consumer.finishSourceEntity(EntInfo.Kind);
  }
}

namespace {
class DocSyntaxWalker : public SyntaxModelWalker {
  SourceManager &SM;
  unsigned BufferID;
  ArrayRef<TextReference> References;
  DocInfoConsumer &Consumer;
  SourceLoc LastArgLoc;
  SourceLoc LastParamLoc;

public:
  DocSyntaxWalker(SourceManager &SM, unsigned BufferID,
                  ArrayRef<TextReference> References,
                  DocInfoConsumer &Consumer)
    : SM(SM), BufferID(BufferID), References(References), Consumer(Consumer) {}

  bool walkToNodePre(SyntaxNode Node) override {
    unsigned Offset = SM.getLocOffsetInBuffer(Node.Range.getStart(), BufferID);
    unsigned Length = Node.Range.getByteLength();

    reportRefsUntil(Offset);
    if (!References.empty() && References.front().Range.Offset == Offset)
      return true;

    switch (Node.Kind) {
    case SyntaxNodeKind::EditorPlaceholder:
      return true;

    case SyntaxNodeKind::Keyword:
      if (Node.Range.getStart() == LastArgLoc ||
          Node.Range.getStart() == LastParamLoc)
        return true;
      break;

    case SyntaxNodeKind::Identifier:
    case SyntaxNodeKind::DollarIdent:
    case SyntaxNodeKind::Integer:
    case SyntaxNodeKind::Floating:
    case SyntaxNodeKind::String:
    case SyntaxNodeKind::StringInterpolationAnchor:
    case SyntaxNodeKind::CommentLine:
    case SyntaxNodeKind::CommentBlock:
    case SyntaxNodeKind::CommentMarker:
    case SyntaxNodeKind::CommentURL:
    case SyntaxNodeKind::DocCommentLine:
    case SyntaxNodeKind::DocCommentBlock:
    case SyntaxNodeKind::DocCommentField:
    case SyntaxNodeKind::TypeId:
    case SyntaxNodeKind::BuildConfigKeyword:
    case SyntaxNodeKind::BuildConfigId:
    case SyntaxNodeKind::AttributeId:
    case SyntaxNodeKind::AttributeBuiltin:
    case SyntaxNodeKind::ObjectLiteral:
      break;
    }

    DocEntityInfo Info;
    Info.Kind = SwiftLangSupport::getUIDForSyntaxNodeKind(Node.Kind);
    Info.Offset = Offset;
    Info.Length = Length;
    Consumer.handleAnnotation(Info);
    return true;
  }

  void finished() {
    reportRefsUntil(std::numeric_limits<unsigned>::max());
  }

  bool walkToSubStructurePre(SyntaxStructureNode Node) override {
    if (Node.Kind == SyntaxStructureKind::Parameter) {
      auto Param = dyn_cast<ParamDecl>(Node.Dcl);

      auto passAnnotation = [&](UIdent Kind, SourceLoc Loc, Identifier Name) {
        if (Loc.isInvalid())
          return;
        unsigned Offset = SM.getLocOffsetInBuffer(Loc, BufferID);
        unsigned Length = Name.empty() ? 1 : Name.getLength();
        reportRefsUntil(Offset);

        DocEntityInfo Info;
        Info.Kind = Kind;
        Info.Offset = Offset;
        Info.Length = Length;
        Consumer.handleAnnotation(Info);
      };

      // Argument
      static UIdent KindArgument("source.lang.swift.syntaxtype.argument");
      passAnnotation(KindArgument, Param->getArgumentNameLoc(),
                     Param->getArgumentName());
      LastArgLoc = Param->getArgumentNameLoc();

      // Parameter
      static UIdent KindParameter("source.lang.swift.syntaxtype.parameter");
      passAnnotation(KindParameter, Param->getNameLoc(), Param->getName());
      LastParamLoc = Param->getNameLoc();
    }

    return true;
  }

private:
  void reportRefsUntil(unsigned Offset) {
    while (!References.empty() && References.front().Range.Offset < Offset) {
      const TextReference &Ref = References.front();
      References = References.slice(1);
      DocEntityInfo Info;
      if (initDocEntityInfo(Ref.Dcl, /*IsRef=*/true, Info))
        continue;
      Info.Offset = Ref.Range.Offset;
      Info.Length = Ref.Range.Length;
      Info.Ty = Ref.Ty;
      Consumer.handleAnnotation(Info);
    }
  }
};
}

static bool makeParserAST(CompilerInstance &CI, StringRef Text) {
  CompilerInvocation Invocation;
  Invocation.setModuleName("main");
  Invocation.setInputKind(InputFileKind::IFK_Swift);

  std::unique_ptr<llvm::MemoryBuffer> Buf;
  Buf = llvm::MemoryBuffer::getMemBuffer(Text, "<module-interface>");
  Invocation.addInputBuffer(Buf.get());
  if (CI.setup(Invocation))
    return true;
  CI.performParseOnly();
  return false;
}

static void collectFuncEntities(std::vector<TextEntity> &Ents,
                                std::vector<TextEntity*> &FuncEntities) {
  for (TextEntity &Ent : Ents) {
    if (isa<AbstractFunctionDecl>(Ent.Dcl) || isa<SubscriptDecl>(Ent.Dcl)) {
      // We are getting the entities via a pointer and later adding to their
      // subentities; make sure it doesn't have subentities now or we are going
      // to invalidate the pointers.
      assert(Ent.SubEntities.empty());
      FuncEntities.push_back(&Ent);
    }
    collectFuncEntities(Ent.SubEntities, FuncEntities);
  }
}

static void addParameters(ArrayRef<Identifier> &ArgNames,
                          const Pattern *Pat,
                          TextEntity &Ent,
                          SourceManager &SM,
                          unsigned BufferID) {
  if (auto ParenPat = dyn_cast<ParenPattern>(Pat)) {
    addParameters(ArgNames, ParenPat->getSubPattern(), Ent, SM, BufferID);
    return;
  }

  if (auto Tuple = dyn_cast<TuplePattern>(Pat)) {
    for (const auto &Elt : Tuple->getElements())
      addParameters(ArgNames, Elt.getPattern(), Ent, SM, BufferID);

    return;
  }

  StringRef Arg;
  if (!ArgNames.empty()) {
    Identifier Id = ArgNames.front();
    Arg = Id.empty() ? "_" : Id.str();
    ArgNames = ArgNames.slice(1);
  }

  if (auto Typed = dyn_cast<TypedPattern>(Pat)) {
    VarDecl *VD = nullptr;
    if (auto Named = dyn_cast<NamedPattern>(Typed->getSubPattern())) {
      VD = Named->getDecl();
    }
    SourceRange TypeRange = Typed->getTypeLoc().getSourceRange();
    if (auto InOutTyR =
        dyn_cast_or_null<InOutTypeRepr>(Typed->getTypeLoc().getTypeRepr())) {
      TypeRange = InOutTyR->getBase()->getSourceRange();
    }
    if (TypeRange.isInvalid())
      return;
    unsigned StartOffs = SM.getLocOffsetInBuffer(TypeRange.Start, BufferID);
    unsigned EndOffs =
      SM.getLocOffsetInBuffer(Lexer::getLocForEndOfToken(SM, TypeRange.End),
                              BufferID);
    TextRange TR{ StartOffs, EndOffs-StartOffs };
    TextEntity Param(VD, Arg, TR, StartOffs);
    Ent.SubEntities.push_back(std::move(Param));
  }
}

static void addParameters(const AbstractFunctionDecl *FD,
                          TextEntity &Ent,
                          SourceManager &SM,
                          unsigned BufferID) {
  auto Pats = FD->getBodyParamPatterns();
  // Ignore 'self'.
  if (FD->getDeclContext()->isTypeContext() &&
      !Pats.empty() && isa<TypedPattern>(Pats.front())) {
    Pats = Pats.slice(1);
  }
  ArrayRef<Identifier> ArgNames;
  DeclName Name = FD->getFullName();
  if (Name) {
    ArgNames = Name.getArgumentNames();
  }
  for (auto Pat : Pats) {
    addParameters(ArgNames, Pat, Ent, SM, BufferID);
  }
}

static void addParameters(const SubscriptDecl *D,
                          TextEntity &Ent,
                          SourceManager &SM,
                          unsigned BufferID) {
  ArrayRef<Identifier> ArgNames;
  DeclName Name = D->getFullName();
  if (Name) {
    ArgNames = Name.getArgumentNames();
  }
  addParameters(ArgNames, D->getIndices(), Ent, SM, BufferID);
}

namespace {
class FuncWalker : public ASTWalker {
  SourceManager &SM;
  unsigned BufferID;
  llvm::MutableArrayRef<TextEntity*> FuncEnts;

public:
  FuncWalker(SourceManager &SM, unsigned BufferID,
             llvm::MutableArrayRef<TextEntity*> FuncEnts)
    : SM(SM), BufferID(BufferID), FuncEnts(FuncEnts) {}

  bool walkToDeclPre(Decl *D) override {
    if (D->isImplicit())
      return false; // Skip body.

    if (FuncEnts.empty())
      return false;

    if (!isa<AbstractFunctionDecl>(D) && !isa<SubscriptDecl>(D))
      return true;

    unsigned Offset = SM.getLocOffsetInBuffer(D->getLoc(), BufferID);
    auto Found = FuncEnts.end();
    if (FuncEnts.front()->LocOffset == Offset) {
      Found = FuncEnts.begin();
    } else {
      Found = std::lower_bound(FuncEnts.begin(), FuncEnts.end(), Offset,
        [](TextEntity *Ent, unsigned Offs) {
          return Ent->LocOffset < Offs;
        });
    }
    if (Found == FuncEnts.end() || (*Found)->LocOffset != Offset)
      return false;
    if (auto FD = dyn_cast<AbstractFunctionDecl>(D)) {
      addParameters(FD, **Found, SM, BufferID);
    } else {
      addParameters(cast<SubscriptDecl>(D), **Found, SM, BufferID);
    }
    FuncEnts = llvm::MutableArrayRef<TextEntity*>(Found+1, FuncEnts.end());
    return false; // skip body.
  }
};
}

static void addParameterEntities(CompilerInstance &CI,
                                 SourceTextInfo &IFaceInfo) {
  std::vector<TextEntity*> FuncEntities;
  collectFuncEntities(IFaceInfo.TopEntities, FuncEntities);
  llvm::MutableArrayRef<TextEntity*> FuncEnts(FuncEntities.data(),
                                              FuncEntities.size());
  for (auto Unit : CI.getMainModule()->getFiles()) {
    auto SF = dyn_cast<SourceFile>(Unit);
    if (!SF)
      continue;
    FuncWalker Walker(CI.getSourceMgr(), *SF->getBufferID(), FuncEnts);
    SF->walk(Walker);
  }
}

static void reportSourceAnnotations(const SourceTextInfo &IFaceInfo,
                                    CompilerInstance &CI,
                                    DocInfoConsumer &Consumer) {
  for (auto Unit : CI.getMainModule()->getFiles()) {
    auto SF = dyn_cast<SourceFile>(Unit);
    if (!SF)
      continue;

    SyntaxModelContext SyntaxContext(*SF);
    DocSyntaxWalker SyntaxWalker(CI.getSourceMgr(), *SF->getBufferID(),
                                 IFaceInfo.References, Consumer);
    SyntaxContext.walk(SyntaxWalker);
    SyntaxWalker.finished();
  }
}

static bool getModuleInterfaceInfo(ASTContext &Ctx, StringRef ModuleName,
                                   SourceTextInfo &Info) {
  // Load standard library so that Clang importer can use it.
  auto *Stdlib = getModuleByFullName(Ctx, Ctx.StdlibModuleName);
  if (!Stdlib)
    return true;

  auto *M = getModuleByFullName(Ctx, ModuleName);
  if (!M)
    return true;

  PrintOptions Options = PrintOptions::printDocInterface();
  ModuleTraversalOptions TraversalOptions = None;
  TraversalOptions |= ModuleTraversal::VisitSubmodules;
  TraversalOptions |= ModuleTraversal::VisitHidden;

  SmallString<128> Text;
  llvm::raw_svector_ostream OS(Text);
  AnnotatingPrinter Printer(OS);
  printModuleInterface(M, TraversalOptions, Printer, Options);

  Info.Text = OS.str();
  Info.TopEntities = std::move(Printer.TopEntities);
  Info.References = std::move(Printer.References);
  return false;
}

static bool reportModuleDocInfo(CompilerInvocation Invocation,
                                StringRef ModuleName,
                                DocInfoConsumer &Consumer) {
  CompilerInstance CI;
  // Display diagnostics to stderr.
  PrintingDiagnosticConsumer PrintDiags;
  CI.addDiagnosticConsumer(&PrintDiags);

  if (CI.setup(Invocation))
    return true;

  ASTContext &Ctx = CI.getASTContext();
  // Setup a typechecker for protocol conformance resolving.
  OwnedResolver TypeResolver = createLazyResolver(Ctx);

  SourceTextInfo IFaceInfo;
  if (getModuleInterfaceInfo(Ctx, ModuleName, IFaceInfo))
    return true;

  CompilerInstance ParseCI;
  if (makeParserAST(ParseCI, IFaceInfo.Text))
    return true;
  addParameterEntities(ParseCI, IFaceInfo);
  
  Consumer.handleSourceText(IFaceInfo.Text);
  reportDocEntities(Ctx, IFaceInfo.TopEntities, Consumer);
  reportSourceAnnotations(IFaceInfo, ParseCI, Consumer);

  return false;
}

namespace {
class SourceDocASTWalker : public SourceEntityWalker {
public:
  SourceManager &SM;
  unsigned BufferID;

  std::vector<TextEntity> TopEntities;
  std::vector<TextEntity> EntitiesStack;
  std::vector<TextReference> References;

  SourceDocASTWalker(SourceManager &SM, unsigned BufferID)
    : SM(SM), BufferID(BufferID) {}

  ~SourceDocASTWalker() {
    assert(EntitiesStack.empty());
  }

  bool walkToDeclPre(Decl *D, CharSourceRange Range) override {
    if (!isa<ValueDecl>(D) && !isa<ExtensionDecl>(D))
      return true;
    if (isLocal(D))
      return true;
    TextRange TR = getTextRange(D->getSourceRange());
    unsigned LocOffset = getOffset(Range.getStart());
    EntitiesStack.emplace_back(D, TR, LocOffset);
    return true;
  }

  bool walkToDeclPost(Decl *D) override {
    if (EntitiesStack.empty() || EntitiesStack.back().Dcl != D)
      return true;

    TextEntity Entity = std::move(EntitiesStack.back());
    EntitiesStack.pop_back();
    if (EntitiesStack.empty())
      TopEntities.push_back(Entity);
    else
      EntitiesStack.back().SubEntities.push_back(Entity);
    return true;
  }

  bool visitDeclReference(ValueDecl *D, CharSourceRange Range,
                          TypeDecl *CtorTyRef, Type Ty) override {
    unsigned StartOffset = getOffset(Range.getStart());
    References.emplace_back(D, StartOffset, Range.getByteLength(), Ty);
    return true;
  }

  bool visitSubscriptReference(ValueDecl *D, CharSourceRange Range,
                               bool IsOpenBracket) override {
    // Treat both open and close brackets equally
    return visitDeclReference(D, Range, nullptr, Type());
  }

  bool isLocal(Decl *D) const {
    return isa<ParamDecl>(D) || D->getDeclContext()->getLocalContext();
  }

  unsigned getOffset(SourceLoc Loc) const {
    return SM.getLocOffsetInBuffer(Loc, BufferID);
  }

  TextRange getTextRange(SourceRange R) const {
    unsigned Start = getOffset(R.Start);
    unsigned End = getOffset(R.End);
    return TextRange{ Start, End-Start };
  }
};
}

static bool getSourceTextInfo(CompilerInstance &CI,
                              SourceTextInfo &Info) {
  SourceManager &SM = CI.getSourceMgr();
  unsigned BufID = CI.getInputBufferIDs().back();

  SourceDocASTWalker Walker(SM, BufID);
  Walker.walk(*CI.getMainModule());

  CharSourceRange FullRange = SM.getRangeForBuffer(BufID);
  Info.Text = SM.extractText(FullRange);
  Info.TopEntities = std::move(Walker.TopEntities);
  Info.References = std::move(Walker.References);
  return false;
}

static bool reportSourceDocInfo(CompilerInvocation Invocation,
                                llvm::MemoryBuffer *InputBuf,
                                DocInfoConsumer &Consumer) {
  CompilerInstance CI;
  // Display diagnostics to stderr.
  PrintingDiagnosticConsumer PrintDiags;
  CI.addDiagnosticConsumer(&PrintDiags);

  EditorDiagConsumer DiagConsumer;
  CI.addDiagnosticConsumer(&DiagConsumer);

  Invocation.addInputBuffer(InputBuf);
  if (CI.setup(Invocation))
    return true;
  DiagConsumer.setInputBufferIDs(CI.getInputBufferIDs());

  ASTContext &Ctx = CI.getASTContext();
  CloseClangModuleFiles scopedCloseFiles(*Ctx.getClangModuleLoader());
  CI.performSema();

  // Setup a typechecker for protocol conformance resolving.
  OwnedResolver TypeResolver = createLazyResolver(Ctx);

  SourceTextInfo SourceInfo;
  if (getSourceTextInfo(CI, SourceInfo))
    return true;
  addParameterEntities(CI, SourceInfo);

  reportDocEntities(Ctx, SourceInfo.TopEntities, Consumer);
  reportSourceAnnotations(SourceInfo, CI, Consumer);
  for (auto &Diag : DiagConsumer.getDiagnosticsForBuffer(
                                                CI.getInputBufferIDs().back()))
    Consumer.handleDiagnostic(Diag);

  return false;
}

void SwiftLangSupport::getDocInfo(llvm::MemoryBuffer *InputBuf,
                                  StringRef ModuleName,
                                  ArrayRef<const char *> Args,
                                  DocInfoConsumer &Consumer) {
  CompilerInstance CI;
  // Display diagnostics to stderr.
  PrintingDiagnosticConsumer PrintDiags;
  CI.addDiagnosticConsumer(&PrintDiags);

  CompilerInvocation Invocation;
  std::string Error;
  bool Failed = getASTManager().initCompilerInvocation(Invocation, Args,
                                                       CI.getDiags(),
                                                       StringRef(),
                                                       Error);
  if (Failed) {
    Consumer.failed(Error);
    return;
  }
  Invocation.getClangImporterOptions().ImportForwardDeclarations = true;

  if (!ModuleName.empty()) {
    bool Error = reportModuleDocInfo(Invocation, ModuleName, Consumer);
    if (Error)
      Consumer.failed("Error occurred");
    return;
  }

  Failed = reportSourceDocInfo(Invocation, InputBuf, Consumer);
  if (Failed)
    Consumer.failed("Error occurred");
}
