//===-- PrintAsObjC.cpp - Emit a header file for a Swift AST --------------===//
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

#include "swift/PrintAsObjC/PrintAsObjC.h"
#include "swift/Strings.h"
#include "swift/AST/AST.h"
#include "swift/AST/ASTVisitor.h"
#include "swift/AST/ForeignErrorConvention.h"
#include "swift/AST/PrettyStackTrace.h"
#include "swift/AST/TypeVisitor.h"
#include "swift/AST/Comment.h"
#include "swift/Basic/Version.h"
#include "swift/ClangImporter/ClangImporter.h"
#include "swift/Frontend/Frontend.h"
#include "swift/Frontend/PrintingDiagnosticConsumer.h"
#include "swift/IDE/CommentConversion.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclObjC.h"
#include "clang/Basic/Module.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

using namespace swift;

static bool isNSObject(ASTContext &ctx, Type type) {
  if (auto classDecl = type->getClassOrBoundGenericClass()) {
    return classDecl->getName() == ctx.Id_NSObject &&
           classDecl->getModuleContext()->getName() == ctx.Id_ObjectiveC;
  }

  return false;
}

/// Returns true if \p name matches a keyword in any Clang language mode.
static bool isClangKeyword(Identifier name) {
  static const llvm::StringSet<> keywords = []{
    llvm::StringSet<> set;
    // FIXME: clang::IdentifierInfo /nearly/ has the API we need to do this
    // in a more principled way, but not quite.
#define KEYWORD(SPELLING, FLAGS) \
    set.insert(#SPELLING);
#define CXX_KEYWORD_OPERATOR(SPELLING, TOK) \
    set.insert(#SPELLING);
#include "clang/Basic/TokenKinds.def"
    return set;
  }();

  if (name.empty())
    return false;
  return keywords.find(name.str()) != keywords.end();
}


namespace {
  enum CustomNamesOnly_t : bool {
    Normal = false,
    CustomNamesOnly = true,
  };
}

static Identifier getNameForObjC(const NominalTypeDecl *NTD,
                                 CustomNamesOnly_t customNamesOnly = Normal) {
  // FIXME: Should we support renaming for enums too?
  assert(isa<ClassDecl>(NTD) || isa<ProtocolDecl>(NTD));
  if (auto objc = NTD->getAttrs().getAttribute<ObjCAttr>()) {
    if (auto name = objc->getName()) {
      assert(name->getNumSelectorPieces() == 1);
      return name->getSelectorPieces().front();
    }
  }

  return customNamesOnly ? Identifier() : NTD->getName();
}


namespace {
class ObjCPrinter : private DeclVisitor<ObjCPrinter>,
                    private TypeVisitor<ObjCPrinter, void, 
                                        Optional<OptionalTypeKind>> {
  friend ASTVisitor;
  friend TypeVisitor;

  llvm::DenseMap<std::pair<Identifier, Identifier>, std::pair<StringRef, bool>>
    specialNames;
  Identifier ID_CFTypeRef;

  Module &M;
  raw_ostream &os;

  SmallVector<const FunctionType *, 4> openFunctionTypes;

  Accessibility minRequiredAccess;
  bool protocolMembersOptional = false;

  friend ASTVisitor<ObjCPrinter>;
  friend TypeVisitor<ObjCPrinter>;

public:
  explicit ObjCPrinter(Module &mod, raw_ostream &out, Accessibility access)
    : M(mod), os(out), minRequiredAccess(access) {}

  void print(const Decl *D) {
    PrettyStackTraceDecl trace("printing", D);
    visit(const_cast<Decl *>(D));
  }

  bool shouldInclude(const ValueDecl *VD) {
    return VD->isObjC() && VD->getFormalAccess() >= minRequiredAccess &&
      !(isa<ConstructorDecl>(VD) && 
        cast<ConstructorDecl>(VD)->hasStubImplementation());
  }

private:
  using ASTVisitor::visit;

  /// Prints a protocol adoption list: <code>&lt;NSCoding, NSCopying&gt;</code>
  ///
  /// This method filters out non-ObjC protocols, along with the special
  /// AnyObject protocol.
  void printProtocols(ArrayRef<ProtocolDecl *> protos) {
    SmallVector<ProtocolDecl *, 4> protosToPrint;
    std::copy_if(protos.begin(), protos.end(),
                 std::back_inserter(protosToPrint),
                 [this](const ProtocolDecl *PD) -> bool {
      if (!shouldInclude(PD))
        return false;
      auto knownProtocol = PD->getKnownProtocolKind();
      if (!knownProtocol)
        return true;
      return *knownProtocol != KnownProtocolKind::AnyObject;
    });

    if (protosToPrint.empty())
      return;

    os << " <";
    interleave(protosToPrint,
               [this](const ProtocolDecl *PD) { os << getNameForObjC(PD); },
               [this] { os << ", "; });
    os << ">";
  }

  /// Prints the members of a class, extension, or protocol.
  void printMembers(DeclRange members) {
    for (auto member : members) {
      auto VD = dyn_cast<ValueDecl>(member);
      if (!VD || !shouldInclude(VD) || isa<TypeDecl>(VD))
        continue;
      if (auto FD = dyn_cast<FuncDecl>(VD))
        if (FD->isAccessor())
          continue;
      if (VD->getAttrs().hasAttribute<OptionalAttr>() != protocolMembersOptional) {
        protocolMembersOptional = VD->getAttrs().hasAttribute<OptionalAttr>();
        os << (protocolMembersOptional ? "@optional\n" : "@required\n");
      }
      visit(VD);
    }
  }

  void printDocumentationComment(Decl *D) {
    llvm::markup::MarkupContext MC;
    auto DC = getDocComment(MC, D);
    if (DC.hasValue())
      ide::getDocumentationCommentAsDoxygen(DC.getValue(), os);
  }

  // Ignore other declarations.
  void visitDecl(Decl *D) {}

  void visitClassDecl(ClassDecl *CD) {
    printDocumentationComment(CD);

    Identifier customName = getNameForObjC(CD, CustomNamesOnly);
    if (customName.empty()) {
      llvm::SmallString<32> scratch;
      os << "SWIFT_CLASS(\"" << CD->getObjCRuntimeName(scratch) << "\")\n"
         << "@interface " << CD->getName();
    } else {
      os << "SWIFT_CLASS_NAMED(\"" << CD->getName() << "\")\n"
         << "@interface " << customName;
    }

    if (Type superTy = CD->getSuperclass())
      os << " : " << getNameForObjC(superTy->getClassOrBoundGenericClass());
    printProtocols(CD->getLocalProtocols(ConformanceLookupKind::OnlyExplicit));
    os << "\n";
    printMembers(CD->getMembers());
    os << "@end\n";
  }

  void visitExtensionDecl(ExtensionDecl *ED) {
    auto baseClass = ED->getExtendedType()->getClassOrBoundGenericClass();

    os << "@interface " << getNameForObjC(baseClass)
       << " (SWIFT_EXTENSION(" << ED->getModuleContext()->getName() << "))";
    printProtocols(ED->getLocalProtocols(ConformanceLookupKind::OnlyExplicit));
    os << "\n";
    printMembers(ED->getMembers());
    os << "@end\n";
  }

  void visitProtocolDecl(ProtocolDecl *PD) {
    printDocumentationComment(PD);

    Identifier customName = getNameForObjC(PD, CustomNamesOnly);
    if (customName.empty()) {
      llvm::SmallString<32> scratch;
      os << "SWIFT_PROTOCOL(\"" << PD->getObjCRuntimeName(scratch) << "\")\n"
         << "@protocol " << PD->getName();
    } else {
      os << "SWIFT_PROTOCOL_NAMED(\"" << PD->getName() << "\")\n"
         << "@protocol " << customName;
    }

    printProtocols(PD->getInheritedProtocols(nullptr));
    os << "\n";
    assert(!protocolMembersOptional && "protocols start required");
    printMembers(PD->getMembers());
    protocolMembersOptional = false;
    os << "@end\n";
  }
  
  void visitEnumDecl(EnumDecl *ED) {
    printDocumentationComment(ED);
    llvm::SmallString<32> scratch;
    os << "typedef SWIFT_ENUM(";
    print(ED->getRawType(), OTK_None);
    os << ", " << ED->getName() << ") {\n";
    for (auto Elt : ED->getAllElements()) {
      printDocumentationComment(Elt);

      // Print the cases as the concatenation of the enum name with the case
      // name.
      os << "  " << ED->getName() << Elt->getName();
      
      if (auto ILE = cast_or_null<IntegerLiteralExpr>(Elt->getRawValueExpr())) {
        os << " = ";
        if (ILE->isNegative())
          os << "-";
        os << ILE->getDigitsText();
      }
      os << ",\n";
    }
    os << "};\n";
  }

  void printSingleMethodParam(StringRef selectorPiece,
                              const Pattern *param,
                              const clang::ParmVarDecl *clangParam,
                              bool isNSUIntegerSubscript,
                              bool isLastPiece) {
    os << selectorPiece << ":(";
    if ((isNSUIntegerSubscript && isLastPiece) ||
        (clangParam && isNSUInteger(clangParam->getType()))) {
      os << "NSUInteger";
    } else {
      this->print(param->getType(), OTK_None);
    }
    os << ")";

    if (isa<AnyPattern>(param)) {
      os << "_";
    } else {
      Identifier name = cast<NamedPattern>(param)->getBodyName();
      os << name;
      if (isClangKeyword(name))
        os << "_";
    }
  }

  template <typename T>
  static const T *findClangBase(const T *member) {
    while (member) {
      if (member->getClangDecl())
        return member;
      member = member->getOverriddenDecl();
    }
    return nullptr;
  }

  /// Returns true if \p clangTy is the typedef for NSUInteger.
  bool isNSUInteger(clang::QualType clangTy) {
    const auto *typedefTy = dyn_cast<clang::TypedefType>(clangTy);
    if (!typedefTy)
      return false;

    const clang::IdentifierInfo *nameII = typedefTy->getDecl()->getIdentifier();
    if (!nameII)
      return false;
    if (nameII->getName() != "NSUInteger")
      return false;

    return true;
  }

  void printAbstractFunction(AbstractFunctionDecl *AFD, bool isClassMethod,
                             bool isNSUIntegerSubscript = false) {
    printDocumentationComment(AFD);
    if (isClassMethod)
      os << "+ (";
    else
      os << "- (";

    const clang::ObjCMethodDecl *clangMethod = nullptr;
    if (!isNSUIntegerSubscript) {
      if (const AbstractFunctionDecl *clangBase = findClangBase(AFD)) {
        clangMethod =
            dyn_cast_or_null<clang::ObjCMethodDecl>(clangBase->getClangDecl());
      }
    }

    Type rawMethodTy = AFD->getType()->castTo<AnyFunctionType>()->getResult();
    auto methodTy = rawMethodTy->castTo<FunctionType>();

    // A foreign error convention can affect the result type as seen in
    // Objective-C.
    Optional<ForeignErrorConvention> errorConvention
      = AFD->getForeignErrorConvention();
    Type resultTy = methodTy->getResult();
    if (errorConvention) {
      switch (errorConvention->getKind()) {
      case ForeignErrorConvention::ZeroResult:
      case ForeignErrorConvention::NonZeroResult:
        // The error convention provides the result type.
        resultTy = errorConvention->getResultType();
        break;

      case ForeignErrorConvention::NilResult:
        // Errors are propagated via 'nil' returns.
        resultTy = OptionalType::get(resultTy);
        break;

      case ForeignErrorConvention::NonNilError:
      case ForeignErrorConvention::ZeroPreservedResult:
        break;
      }
    }

    // Constructors and methods returning DynamicSelf return
    // instancetype.    
    if (isa<ConstructorDecl>(AFD) ||
        (isa<FuncDecl>(AFD) && cast<FuncDecl>(AFD)->hasDynamicSelf())) {
      if (errorConvention && errorConvention->stripsResultOptionality()) {
        printNullability(OTK_Optional, NullabilityPrintKind::ContextSensitive);
      } else if (auto ctor = dyn_cast<ConstructorDecl>(AFD)) {
        printNullability(ctor->getFailability(),
                         NullabilityPrintKind::ContextSensitive);
      } else {
        auto func = cast<FuncDecl>(AFD);
        OptionalTypeKind optionalKind;
        (void)func->getResultType()->getAnyOptionalObjectType(optionalKind);
        printNullability(optionalKind,
                         NullabilityPrintKind::ContextSensitive);
      }

      os << "instancetype";
    } else if (resultTy->isVoid() &&
               AFD->getAttrs().hasAttribute<IBActionAttr>()) {
      os << "IBAction";
    } else if (clangMethod && isNSUInteger(clangMethod->getReturnType())) {
      os << "NSUInteger";
    } else {
      print(resultTy, OTK_None);
    }

    os << ")";

    auto bodyPatterns = AFD->getBodyParamPatterns();
    assert(bodyPatterns.size() == 2 && "not an ObjC-compatible method");

    llvm::SmallString<128> selectorBuf;
    ArrayRef<Identifier> selectorPieces
      = AFD->getObjCSelector().getSelectorPieces();
    const TuplePattern *paramTuple
      = dyn_cast<TuplePattern>(bodyPatterns.back());
    const ParenPattern *paramParen
      = dyn_cast<ParenPattern>(bodyPatterns.back());
    assert((paramTuple || paramParen) && "Bad body parameters?");
    unsigned paramIndex = 0;
    for (unsigned i = 0, n = selectorPieces.size(); i != n; ++i) {
      if (i > 0) os << ' ';

      // Retrieve the selector piece.
      StringRef piece = selectorPieces[i].empty() ? StringRef("")
                                                  : selectorPieces[i].str();

      // If we have an error convention and this is the error
      // parameter, print it.
      if (errorConvention && i == errorConvention->getErrorParameterIndex()) {
        os << piece << ":(";
        print(errorConvention->getErrorParameterType(), OTK_None);
        os << ")error";
        continue;
      }

      // Zero-parameter initializers with a long selector.
      if (isa<ConstructorDecl>(AFD) &&
          cast<ConstructorDecl>(AFD)->isObjCZeroParameterWithLongSelector()) {
        os << piece;
        continue;
      }

      // Single-parameter methods.
      if (paramParen) {
        assert(paramIndex == 0);
        auto clangParam = clangMethod ? clangMethod->parameters()[0] : nullptr;
        printSingleMethodParam(piece,
                               paramParen->getSemanticsProvidingPattern(),
                               clangParam,
                               isNSUIntegerSubscript,
                               i == n-1);
        paramIndex = 1;
        continue;
      }

      // Zero-parameter methods.
      if (paramTuple->getNumElements() == 0) {
        assert(paramIndex == 0);
        os << piece;
        paramIndex = 1;
        continue;
      }

      // Multi-parameter methods.
      const clang::ParmVarDecl *clangParam = nullptr;
      if (clangMethod)
        clangParam = clangMethod->parameters()[paramIndex];

      const TuplePatternElt &param = paramTuple->getElements()[paramIndex];
      auto pattern = param.getPattern()->getSemanticsProvidingPattern();
      printSingleMethodParam(piece, pattern, clangParam,
                             isNSUIntegerSubscript,
                             i == n-1);
      ++paramIndex;
    }

    // Swift designated initializers are Objective-C designated initializers.
    if (auto ctor = dyn_cast<ConstructorDecl>(AFD)) {
      if (ctor->isDesignatedInit() &&
          !isa<ProtocolDecl>(ctor->getDeclContext())) {
        os << " OBJC_DESIGNATED_INITIALIZER";
      }
    }

    os << ";\n";
  }

  void visitFuncDecl(FuncDecl *FD) {
    assert(FD->getDeclContext()->isTypeContext() &&
           "cannot handle free functions right now");
    printAbstractFunction(FD, FD->isStatic());
  }

  void visitConstructorDecl(ConstructorDecl *CD) {
    printAbstractFunction(CD, false);
  }

  bool maybePrintIBOutletCollection(Type ty) {
    if (auto unwrapped = ty->getAnyOptionalObjectType())
      ty = unwrapped;

    auto genericTy = ty->getAs<BoundGenericStructType>();
    if (!genericTy || genericTy->getDecl() != M.getASTContext().getArrayDecl())
      return false;

    assert(genericTy->getGenericArgs().size() == 1);

    auto argTy = genericTy->getGenericArgs().front();
    if (auto classDecl = argTy->getClassOrBoundGenericClass())
      os << "IBOutletCollection(" << getNameForObjC(classDecl) << ") ";
    else
      os << "IBOutletCollection(id) ";
    return true;
  }

  bool isCFTypeRef(Type ty) {
    if (ID_CFTypeRef.empty())
      ID_CFTypeRef = M.getASTContext().getIdentifier("CFTypeRef");

    const TypeAliasDecl *TAD = nullptr;
    while (auto aliasTy = dyn_cast<NameAliasType>(ty.getPointer())) {
      TAD = aliasTy->getDecl();
      ty = TAD->getUnderlyingType();
    }

    return TAD && TAD->getName() == ID_CFTypeRef && TAD->hasClangNode();
  }

  void visitVarDecl(VarDecl *VD) {
    assert(VD->getDeclContext()->isTypeContext() &&
           "cannot handle global variables right now");

    printDocumentationComment(VD);

    if (VD->isStatic()) {
      // Objective-C doesn't have class properties. Just print the accessors.
      printAbstractFunction(VD->getGetter(), true);
      if (auto setter = VD->getSetter())
        printAbstractFunction(setter, true);
      return;
    }

    // For now, never promise atomicity.
    os << "@property (nonatomic";

    ASTContext &ctx = M.getASTContext();
    bool isSettable = VD->isSettable(nullptr);
    if (isSettable && ctx.LangOpts.EnableAccessControl)
      isSettable = (VD->getSetterAccessibility() >= minRequiredAccess);
    if (!isSettable)
      os << ", readonly";

    // Print the ownership semantics, if relevant.
    // We treat "unowned" as "assign" (even though it's more like
    // "safe_unretained") because we want people to think twice about
    // allowing that object to disappear.
    Type ty = VD->getType();
    if (auto weakTy = ty->getAs<WeakStorageType>()) {
      auto innerTy = weakTy->getReferentType()->getAnyOptionalObjectType();
      auto innerClass = innerTy->getClassOrBoundGenericClass();
      if ((innerClass && !innerClass->isForeign()) ||
          (innerTy->isObjCExistentialType() && !isCFTypeRef(innerTy))) {
        os << ", weak";
      }
    } else if (ty->is<UnownedStorageType>()) {
      os << ", assign";
    } else if (ty->is<UnmanagedStorageType>()) {
      os << ", unsafe_unretained";
    } else {
      Type copyTy = ty;
      OptionalTypeKind optionalType;
      if (auto unwrappedTy = copyTy->getAnyOptionalObjectType(optionalType))
        copyTy = unwrappedTy;
      auto nominal = copyTy->getNominalOrBoundGenericNominal();
      if (dyn_cast_or_null<StructDecl>(nominal)) {
        if (nominal == ctx.getArrayDecl() ||
            nominal == ctx.getDictionaryDecl() ||
            nominal == ctx.getSetDecl() ||
            nominal == ctx.getStringDecl()) {
          os << ", copy";
        } else if (nominal == ctx.getUnmanagedDecl()) {
          os << ", unsafe_unretained";
          // Don't print unsafe_unretained twice.
          if (auto boundTy = copyTy->getAs<BoundGenericType>()) {
            ty = boundTy->getGenericArgs().front();
            if (optionalType != OTK_None)
              ty = OptionalType::get(optionalType, ty);
          }
        }
      } else if (auto fnTy = copyTy->getAs<FunctionType>()) {
        switch (fnTy->getRepresentation()) {
        case FunctionTypeRepresentation::Block:
        case FunctionTypeRepresentation::Swift:
          os << ", copy";
          break;
        case FunctionTypeRepresentation::Thin:
        case FunctionTypeRepresentation::CFunctionPointer:
          break;
        }
      } else if ((dyn_cast_or_null<ClassDecl>(nominal) &&
                  !cast<ClassDecl>(nominal)->isForeign()) ||
                 (copyTy->isObjCExistentialType() && !isCFTypeRef(copyTy))) {
        os << ", strong";
      }
    }

    Identifier objCName = VD->getObjCPropertyName();
    bool hasReservedName = isClangKeyword(objCName);

    // Handle custom accessor names.
    llvm::SmallString<64> buffer;
    if (hasReservedName ||
        VD->getObjCGetterSelector() !=
          VarDecl::getDefaultObjCGetterSelector(ctx, objCName)) {
      os << ", getter=" << VD->getObjCGetterSelector().getString(buffer);
    }
    if (VD->isSettable(nullptr)) {
      if (hasReservedName ||
          VD->getObjCSetterSelector() !=
            VarDecl::getDefaultObjCSetterSelector(ctx, objCName)) {
        buffer.clear();
        os << ", setter=" << VD->getObjCSetterSelector().getString(buffer);
      }
    }

    os << ") ";
    if (VD->getAttrs().hasAttribute<IBOutletAttr>()) {
      if (!maybePrintIBOutletCollection(ty))
        os << "IBOutlet ";
    }

    clang::QualType clangTy;
    if (const VarDecl *base = findClangBase(VD))
      if (auto prop = dyn_cast<clang::ObjCPropertyDecl>(base->getClangDecl()))
        clangTy = prop->getType();

    if (!clangTy.isNull() && isNSUInteger(clangTy)) {
      os << "NSUInteger " << objCName;
    } else {
      print(ty, OTK_None, objCName.str());
    }

    if (hasReservedName)
      os << "_";

    os << ";\n";
  }

  void visitSubscriptDecl(SubscriptDecl *SD) {
    assert(SD->isInstanceMember() && "static subscripts not supported");

    bool isNSUIntegerSubscript = false;
    if (auto clangBase = findClangBase(SD)) {
      const auto *clangGetter =
          cast<clang::ObjCMethodDecl>(clangBase->getClangDecl());
      const auto *indexParam = clangGetter->parameters().front();
      isNSUIntegerSubscript = isNSUInteger(indexParam->getType());
    }

    printAbstractFunction(SD->getGetter(), false, isNSUIntegerSubscript);
    if (auto setter = SD->getSetter())
      printAbstractFunction(setter, false, isNSUIntegerSubscript);
  }

  /// Visit part of a type, such as the base of a pointer type.
  ///
  /// If a full type is being printed, use print() instead.
  void visitPart(Type ty, Optional<OptionalTypeKind> optionalKind) {
    TypeVisitor::visit(ty, optionalKind);
  }

  /// Where nullability information should be printed.
  enum class NullabilityPrintKind {
    Before,
    After,
    ContextSensitive,
  };

  void printNullability(Optional<OptionalTypeKind> kind,
                        NullabilityPrintKind printKind
                          = NullabilityPrintKind::After) {
    if (!kind)
      return;

    if (printKind == NullabilityPrintKind::After)
      os << ' ';

    if (printKind != NullabilityPrintKind::ContextSensitive)
      os << "__";

    switch (*kind) {
    case OTK_None:
      os << "nonnull";
      break;

    case OTK_Optional:
      os << "nullable";
      break;

    case OTK_ImplicitlyUnwrappedOptional:
      os << "null_unspecified";
      break;
    }

    if (printKind != NullabilityPrintKind::After)
      os << ' ';
  }

  /// If "name" is one of the standard library types used to map in Clang
  /// primitives and basic types, print out the appropriate spelling and
  /// return true.
  ///
  /// This handles typealiases and structs provided by the standard library
  /// for interfacing with C and Objective-C.
  bool printIfKnownTypeName(Identifier moduleName, Identifier name,
                            Optional<OptionalTypeKind> optionalKind) {
    if (specialNames.empty()) {
      ASTContext &ctx = M.getASTContext();
#define MAP(SWIFT_NAME, CLANG_REPR, NEEDS_NULLABILITY)                       \
      specialNames[{ctx.StdlibModuleName, ctx.getIdentifier(#SWIFT_NAME)}] = \
        { CLANG_REPR, NEEDS_NULLABILITY}

      MAP(CBool, "bool", false);

      MAP(CChar, "char", false);
      MAP(CWideChar, "wchar_t", false);
      MAP(CChar16, "char16_t", false);
      MAP(CChar32, "char32_t", false);

      MAP(CSignedChar, "signed char", false);
      MAP(CShort, "short", false);
      MAP(CInt, "int", false);
      MAP(CLong, "long", false);
      MAP(CLongLong, "long long", false);

      MAP(CUnsignedChar, "unsigned char", false);
      MAP(CUnsignedShort, "unsigned short", false);
      MAP(CUnsignedInt, "unsigned int", false);
      MAP(CUnsignedLong, "unsigned long", false);
      MAP(CUnsignedLongLong, "unsigned long long", false);

      MAP(CFloat, "float", false);
      MAP(CDouble, "double", false);

      MAP(Int8, "int8_t", false);
      MAP(Int16, "int16_t", false);
      MAP(Int32, "int32_t", false);
      MAP(Int64, "int64_t", false);
      MAP(UInt8, "uint8_t", false);
      MAP(UInt16, "uint16_t", false);
      MAP(UInt32, "uint32_t", false);
      MAP(UInt64, "uint64_t", false);

      MAP(Float, "float", false);
      MAP(Double, "double", false);
      MAP(Float32, "float", false);
      MAP(Float64, "double", false);

      MAP(Int, "NSInteger", false);
      MAP(UInt, "NSUInteger", false);
      MAP(Bool, "BOOL", false);

      MAP(COpaquePointer, "void *", true);

      Identifier ID_ObjectiveC = ctx.Id_ObjectiveC;
      specialNames[{ID_ObjectiveC, ctx.getIdentifier("ObjCBool")}] 
        = { "BOOL", false};
      specialNames[{ID_ObjectiveC, ctx.getIdentifier("Selector")}] 
        = { "SEL", true };
      specialNames[{ID_ObjectiveC, ctx.getIdentifier("NSZone")}] 
        = { "struct _NSZone *", true };

      specialNames[{ctx.Id_Darwin, ctx.getIdentifier("DarwinBoolean")}]
        = { "Boolean", false};

      // Use typedefs we set up for SIMD vector types.
#define MAP_SIMD_TYPE(BASENAME, __) \
      specialNames[{ctx.Id_simd, ctx.getIdentifier(#BASENAME "2")}] \
        = { "swift_" #BASENAME "2", false };                        \
      specialNames[{ctx.Id_simd, ctx.getIdentifier(#BASENAME "3")}] \
        = { "swift_" #BASENAME "3", false };                        \
      specialNames[{ctx.Id_simd, ctx.getIdentifier(#BASENAME "4")}] \
        = { "swift_" #BASENAME "4", false };
#include "swift/ClangImporter/SIMDMappedTypes.def"
      static_assert(SWIFT_MAX_IMPORTED_SIMD_ELEMENTS == 4,
                    "must add or remove special name mappings if max number of "
                    "SIMD elements is changed");
    }

    auto iter = specialNames.find({moduleName, name});
    if (iter == specialNames.end())
      return false;

    os << iter->second.first;
    if (iter->second.second) {
      // FIXME: Selectors and pointers should be nullable.
      printNullability(OptionalTypeKind::OTK_ImplicitlyUnwrappedOptional);
    }
    return true;
  }

  void visitType(TypeBase *Ty, Optional<OptionalTypeKind> optionalKind) {
    assert(Ty->getDesugaredType() == Ty && "unhandled sugared type");
    os << "/* ";
    Ty->print(os);
    os << " */";
  }

  bool isClangObjectPointerType(const clang::TypeDecl *clangTypeDecl) const {
    ASTContext &ctx = M.getASTContext();
    auto &clangASTContext = ctx.getClangModuleLoader()->getClangASTContext();
    clang::QualType clangTy = clangASTContext.getTypeDeclType(clangTypeDecl);
    return clangTy->isObjCRetainableType();
  }

  bool isClangPointerType(const clang::TypeDecl *clangTypeDecl) const {
    ASTContext &ctx = M.getASTContext();
    auto &clangASTContext = ctx.getClangModuleLoader()->getClangASTContext();
    clang::QualType clangTy = clangASTContext.getTypeDeclType(clangTypeDecl);
    return clangTy->isPointerType();
  }

  void visitNameAliasType(NameAliasType *aliasTy,
                          Optional<OptionalTypeKind> optionalKind) {
    const TypeAliasDecl *alias = aliasTy->getDecl();
    if (printIfKnownTypeName(alias->getModuleContext()->getName(),
                             alias->getName(),
                             optionalKind))
      return;

    if (alias->hasClangNode()) {
      auto *clangTypeDecl = cast<clang::TypeDecl>(alias->getClangDecl());
      os << clangTypeDecl->getName();

      // Print proper nullability for CF types, but __null_unspecified for
      // all other non-object Clang pointer types.
      if (aliasTy->hasReferenceSemantics() ||
          isClangObjectPointerType(clangTypeDecl)) {
        printNullability(optionalKind);
      } else if (isClangPointerType(clangTypeDecl)) {
        printNullability(OptionalTypeKind::OTK_ImplicitlyUnwrappedOptional);
      }
      return;
    }

    if (alias->isObjC()) {
      os << alias->getName();
      return;
    }

    visitPart(alias->getUnderlyingType(), optionalKind);
  }

  void maybePrintTagKeyword(const NominalTypeDecl *NTD) {
    if (isa<EnumDecl>(NTD) && !NTD->hasClangNode()) {
      os << "enum ";
      return;
    }

    auto clangDecl = dyn_cast_or_null<clang::TagDecl>(NTD->getClangDecl());
    if (!clangDecl)
      return;

    if (clangDecl->getTypedefNameForAnonDecl())
      return;

    ASTContext &ctx = M.getASTContext();
    auto importer = static_cast<ClangImporter *>(ctx.getClangModuleLoader());
    if (importer->hasTypedef(clangDecl))
      return;

    os << clangDecl->getKindName() << " ";
  }

  void visitStructType(StructType *ST, 
                       Optional<OptionalTypeKind> optionalKind) {
    const StructDecl *SD = ST->getStructOrBoundGenericStruct();
    if (SD == M.getASTContext().getStringDecl()) {
      os << "NSString *";
      printNullability(optionalKind);
      return;
    }

    if (printIfKnownTypeName(SD->getModuleContext()->getName(), SD->getName(),
                             optionalKind))
      return;

    maybePrintTagKeyword(SD);
    os << SD->getName();
  }

  /// Print a collection element type using Objective-C generics syntax.
  ///
  /// This will print the type as bridged to Objective-C.
  void printCollectionElement(Type ty) {
    ASTContext &ctx = M.getASTContext();

    // Use the type as bridged to Objective-C unless the element type is itself
    // a collection.
    const StructDecl *SD = ty->getStructOrBoundGenericStruct();
    if (SD != ctx.getArrayDecl() &&
        SD != ctx.getDictionaryDecl() &&
        SD != ctx.getSetDecl()) {
      ty = *ctx.getBridgedToObjC(&M, ty, /*resolver*/nullptr);
    }

    print(ty, None);
  }

  /// If \p BGT represents a generic struct used to import Clang types, print
  /// it out.
  bool printIfKnownGenericStruct(const BoundGenericStructType *BGT,
                                 Optional<OptionalTypeKind> optionalKind) {
    StructDecl *SD = BGT->getDecl();
    if (!SD->getModuleContext()->isStdlibModule())
      return false;

    ASTContext &ctx = M.getASTContext();

    if (SD == ctx.getArrayDecl()) {
      if (!BGT->getGenericArgs()[0]->isAnyObject()) {
        os << "NSArray<";
        printCollectionElement(BGT->getGenericArgs()[0]);
        os << "> *";
      } else {
        os << "NSArray *";
      }

      printNullability(optionalKind);
      return true;
    }

    if (SD == ctx.getDictionaryDecl()) {
      if (!isNSObject(ctx, BGT->getGenericArgs()[0]) ||
          !BGT->getGenericArgs()[1]->isAnyObject()) {
        os << "NSDictionary<";
        printCollectionElement(BGT->getGenericArgs()[0]);
        os << ", ";
        printCollectionElement(BGT->getGenericArgs()[1]);
        os << "> *";
      } else {
        os << "NSDictionary *";
      }

      printNullability(optionalKind);
      return true;
    }

    if (SD == ctx.getSetDecl()) {
      if (!isNSObject(ctx, BGT->getGenericArgs()[0])) {
        os << "NSSet<";
        printCollectionElement(BGT->getGenericArgs()[0]);
        os << "> *";
      } else {
        os << "NSSet *";
      }

      printNullability(optionalKind);
      return true;
    }

    if (SD == ctx.getUnmanagedDecl()) {
      auto args = BGT->getGenericArgs();
      assert(args.size() == 1);
      visitPart(args.front(), optionalKind);
      os << " __unsafe_unretained";
      return true;
    }

    // Everything from here on is some kind of pointer type.
    bool isConst;
    if (SD == ctx.getUnsafePointerDecl()) {
      isConst = true;
    } else if (SD == ctx.getAutoreleasingUnsafeMutablePointerDecl() ||
               SD == ctx.getUnsafeMutablePointerDecl()) {
      isConst = false;
    } else {
      // Not a pointer.
      return false;
    }

    auto args = BGT->getGenericArgs();
    assert(args.size() == 1);
    visitPart(args.front(), OTK_None);
    if (isConst)
      os << " const";
    os << " *";
    // FIXME: Pointer types should be nullable.
    printNullability(OptionalTypeKind::OTK_ImplicitlyUnwrappedOptional);
    return true;
  }

  void visitBoundGenericStructType(BoundGenericStructType *BGT,
                                   Optional<OptionalTypeKind> optionalKind) {
    if (printIfKnownGenericStruct(BGT, optionalKind))
      return;
    visitBoundGenericType(BGT, optionalKind);
  }

  void visitBoundGenericType(BoundGenericType *BGT,
                             Optional<OptionalTypeKind> optionalKind) {
    OptionalTypeKind innerOptionalKind;
    if (auto underlying = BGT->getAnyOptionalObjectType(innerOptionalKind)) {
      visitPart(underlying, innerOptionalKind);
    } else
      visitType(BGT, optionalKind);
  }

  void visitEnumType(EnumType *ET, Optional<OptionalTypeKind> optionalKind) {
    const EnumDecl *ED = ET->getDecl();
    maybePrintTagKeyword(ED);
    os << ED->getName();
  }

  void visitClassType(ClassType *CT, Optional<OptionalTypeKind> optionalKind) {
    const ClassDecl *CD = CT->getClassOrBoundGenericClass();
    assert(CD->isObjC());
    auto clangDecl = dyn_cast_or_null<clang::NamedDecl>(CD->getClangDecl());
    if (clangDecl) {
      if (isa<clang::ObjCInterfaceDecl>(clangDecl)) {
        os << clangDecl->getName() << " *";
        printNullability(optionalKind);
      } else {
        maybePrintTagKeyword(CD);
        os << clangDecl->getName();
        printNullability(optionalKind);
      }
    } else {
      os << getNameForObjC(CD) << " *";
      printNullability(optionalKind);
    }
  }

  void visitProtocolType(ProtocolType *PT, 
                         Optional<OptionalTypeKind> optionalKind, 
                         bool isMetatype = false) {
    os << (isMetatype ? "Class" : "id");

    auto proto = PT->getDecl();
    assert(proto->isObjC());
    if (auto knownKind = proto->getKnownProtocolKind()) {
      if (*knownKind == KnownProtocolKind::AnyObject) {
        printNullability(optionalKind);
        return;
      }
    }

    printProtocols(proto);
    printNullability(optionalKind);
  }

  void visitProtocolCompositionType(ProtocolCompositionType *PCT, 
                                    Optional<OptionalTypeKind> optionalKind,
                                    bool isMetatype = false) {
    CanType canonicalComposition = PCT->getCanonicalType();
    if (auto singleProto = dyn_cast<ProtocolType>(canonicalComposition))
      return visitProtocolType(singleProto, optionalKind, isMetatype);
    PCT = cast<ProtocolCompositionType>(canonicalComposition);

    os << (isMetatype ? "Class" : "id");

    SmallVector<ProtocolDecl *, 4> protos;
    std::transform(PCT->getProtocols().begin(), PCT->getProtocols().end(),
                   std::back_inserter(protos),
                   [] (Type ty) -> ProtocolDecl * {
      return ty->castTo<ProtocolType>()->getDecl();
    });
    printProtocols(protos);
    printNullability(optionalKind);
  }

  void visitExistentialMetatypeType(ExistentialMetatypeType *MT, 
                                    Optional<OptionalTypeKind> optionalKind) {
    Type instanceTy = MT->getInstanceType();
    if (auto protoTy = instanceTy->getAs<ProtocolType>()) {
      visitProtocolType(protoTy, optionalKind, /*isMetatype=*/true);
    } else if (auto compTy = instanceTy->getAs<ProtocolCompositionType>()) {
      visitProtocolCompositionType(compTy, optionalKind, /*isMetatype=*/true);
    } else {
      visitType(MT, optionalKind);
    }
  }

  void visitMetatypeType(MetatypeType *MT, 
                         Optional<OptionalTypeKind> optionalKind) {
    Type instanceTy = MT->getInstanceType();
    if (auto classTy = instanceTy->getAs<ClassType>()) {
      const ClassDecl *CD = classTy->getDecl();
      if (CD->isObjC())
        os << "SWIFT_METATYPE(" << getNameForObjC(CD) << ")";
      else
        os << "Class";
      printNullability(optionalKind);
    } else {
      visitType(MT, optionalKind);
    }
  }
                      
  void printFunctionType(FunctionType *FT, char pointerSigil,
                         Optional<OptionalTypeKind> optionalKind) {
    visitPart(FT->getResult(), OTK_None);
    os << " (" << pointerSigil;
    printNullability(optionalKind);
    openFunctionTypes.push_back(FT);
  }

  void visitFunctionType(FunctionType *FT, 
                         Optional<OptionalTypeKind> optionalKind) {
    switch (FT->getRepresentation()) {
    case AnyFunctionType::Representation::Thin:
      llvm_unreachable("can't represent thin functions in ObjC");
    // Native Swift function types bridge to block types.
    case AnyFunctionType::Representation::Swift:
    case AnyFunctionType::Representation::Block:
      printFunctionType(FT, '^', optionalKind);
      break;
    case AnyFunctionType::Representation::CFunctionPointer:
      printFunctionType(FT, '*', optionalKind);
    }
  }

  /// Print the part of a function type that appears after where the variable
  /// name would go.
  ///
  /// This is necessary to handle C's awful declarator syntax.
  /// "(A) -> ((B) -> C)" becomes "C (^ (^)(A))(B)".
  void finishFunctionType(const FunctionType *FT) {
    os << ")(";
    Type paramsTy = FT->getInput();
    if (auto tupleTy = paramsTy->getAs<TupleType>()) {
      if (tupleTy->getNumElements() == 0) {
        os << "void";
      } else {
        interleave(tupleTy->getElementTypes(),
                   [this](Type ty) { print(ty, OTK_None); },
                   [this] { os << ", "; });
      }
    } else {
      print(paramsTy, OTK_None);
    }
    os << ")";
  }

  void visitTupleType(TupleType *TT, Optional<OptionalTypeKind> optionalKind) {
    assert(TT->getNumElements() == 0);
    os << "void";
  }

  void visitParenType(ParenType *PT, Optional<OptionalTypeKind> optionalKind) {
    visitPart(PT->getSinglyDesugaredType(), optionalKind);
  }

  void visitSubstitutedType(SubstitutedType *ST, 
                            Optional<OptionalTypeKind> optionalKind) {
    visitPart(ST->getSinglyDesugaredType(), optionalKind);
  }

  void visitSyntaxSugarType(SyntaxSugarType *SST, 
                            Optional<OptionalTypeKind> optionalKind) {
    visitPart(SST->getSinglyDesugaredType(), optionalKind);
  }

  void visitDictionaryType(DictionaryType *DT, 
                           Optional<OptionalTypeKind> optionalKind) {
    visitPart(DT->getSinglyDesugaredType(), optionalKind);
  }

  void visitDynamicSelfType(DynamicSelfType *DST, 
                            Optional<OptionalTypeKind> optionalKind) {
    printNullability(optionalKind, NullabilityPrintKind::ContextSensitive);
    os << "instancetype";
  }

  void visitReferenceStorageType(ReferenceStorageType *RST, 
                                 Optional<OptionalTypeKind> optionalKind) {
    visitPart(RST->getReferentType(), optionalKind);
  }

  /// Print a full type, optionally declaring the given \p name.
  ///
  /// This will properly handle nested function types (see
  /// finishFunctionType()). If only a part of a type is being printed, use
  /// visitPart().
public:
  void print(Type ty, Optional<OptionalTypeKind> optionalKind, 
             StringRef name = "") {
    PrettyStackTraceType trace(M.getASTContext(), "printing", ty);

    decltype(openFunctionTypes) savedFunctionTypes;
    savedFunctionTypes.swap(openFunctionTypes);

    visitPart(ty, optionalKind);
    if (!name.empty())
      os << ' ' << name;
    while (!openFunctionTypes.empty()) {
      const FunctionType *openFunctionTy = openFunctionTypes.pop_back_val();
      finishFunctionType(openFunctionTy);
    }

    openFunctionTypes = std::move(savedFunctionTypes);
  }
};

class ReferencedTypeFinder : private TypeVisitor<ReferencedTypeFinder> {
  friend TypeVisitor;

  llvm::function_ref<void(ReferencedTypeFinder &, const TypeDecl *)> Callback;

  ReferencedTypeFinder(decltype(Callback) callback) : Callback(callback) {}

  void visitType(TypeBase *base) {
    assert(base->getDesugaredType() == base && "unhandled sugared type");
    return;
  }

  void visitNameAliasType(NameAliasType *aliasTy) {
    Callback(*this, aliasTy->getDecl());
  }

  void visitParenType(ParenType *parenTy) {
    visit(parenTy->getSinglyDesugaredType());
  }

  void visitTupleType(TupleType *tupleTy) {
    for (auto elemTy : tupleTy->getElementTypes())
      visit(elemTy);
  }

  void visitNominalType(NominalType *nominal) {
    Callback(*this, nominal->getDecl());
  }

  void visitMetatypeType(MetatypeType *metatype) {
    visit(metatype->getInstanceType());
  }

  void visitSubstitutedType(SubstitutedType *sub) {
    visit(sub->getSinglyDesugaredType());
  }

  void visitAnyFunctionType(AnyFunctionType *fnTy) {
    visit(fnTy->getInput());
    visit(fnTy->getResult());
  }

  void visitSyntaxSugarType(SyntaxSugarType *sugar) {
    visit(sugar->getSinglyDesugaredType());
  }

  void visitDictionaryType(DictionaryType *DT) {
    visit(DT->getSinglyDesugaredType());
  }

  void visitProtocolCompositionType(ProtocolCompositionType *composition) {
    for (auto proto : composition->getProtocols())
      visit(proto);
  }

  void visitLValueType(LValueType *lvalue) {
    visit(lvalue->getObjectType());
  }

  void visitInOutType(InOutType *inout) {
    visit(inout->getObjectType());
  }

  void visitBoundGenericType(BoundGenericType *boundGeneric) {
    for (auto argTy : boundGeneric->getGenericArgs())
      visit(argTy);
    // Ignore the base type; that can't be exposed to Objective-C. Every
    // bound generic type we care about gets mapped to a particular construct
    // in Objective-C we care about. (For example, Optional<NSFoo> is mapped to
    // NSFoo *.)
  }

public:
  using TypeVisitor::visit;

  static void walk(Type ty, decltype(Callback) callback) {
    ReferencedTypeFinder(callback).visit(ty);
  }
};

/// A generalization of llvm::SmallSetVector that allows a custom comparator.
template <typename T, unsigned N, typename C = std::less<T>>
using SmallSetVector =
  llvm::SetVector<T, SmallVector<T, N>, llvm::SmallSet<T, N, C>>;

/// A comparator for types with PointerLikeTypeTraits that sorts by opaque
/// void pointer representation.
template <typename T>
struct PointerLikeComparator {
  using Traits = llvm::PointerLikeTypeTraits<T>;
  bool operator()(T lhs, T rhs) {
    return std::less<void*>()(Traits::getAsVoidPointer(lhs),
                              Traits::getAsVoidPointer(rhs));
  }
};

class ModuleWriter {
  enum class EmissionState {
    DefinitionRequested = 0,
    DefinitionInProgress,
    Defined
  };

  llvm::DenseMap<const TypeDecl *, std::pair<EmissionState, bool>> seenTypes;
  std::vector<const Decl *> declsToWrite;

  using ImportModuleTy = PointerUnion<Module*, const clang::Module*>;
  SmallSetVector<ImportModuleTy, 8,
                 PointerLikeComparator<ImportModuleTy>> imports;

  std::string bodyBuffer;
  llvm::raw_string_ostream os{bodyBuffer};

  Module &M;
  StringRef bridgingHeader;
  ObjCPrinter printer;
public:
  ModuleWriter(Module &mod, StringRef header, Accessibility access)
    : M(mod), bridgingHeader(header), printer(M, os, access) {}

  /// Returns true if we added the decl's module to the import set, false if
  /// the decl is a local decl.
  ///
  /// The standard library is special-cased: we assume that any types from it
  /// will be handled explicitly rather than needing an explicit @import.
  bool addImport(const Decl *D) {
    Module *otherModule = D->getModuleContext();

    if (otherModule == &M)
      return false;
    if (otherModule->isStdlibModule())
      return true;
    // Don't need a module for SIMD types in C.
    if (otherModule->getName() == M.getASTContext().Id_simd)
      return true;

    // If there's a Clang node, see if it comes from an explicit submodule.
    // Import that instead, looking through any implicit submodules.
    if (auto clangNode = D->getClangNode()) {
      auto importer =
        static_cast<ClangImporter *>(M.getASTContext().getClangModuleLoader());
      if (const auto *clangModule = importer->getClangOwningModule(clangNode)) {
        while (clangModule && !clangModule->IsExplicit)
          clangModule = clangModule->Parent;
        if (clangModule) {
          imports.insert(clangModule);
          return true;
        }
      }
    }

    imports.insert(otherModule);
    return true;
  }

  bool require(const TypeDecl *D) {
    if (addImport(D)) {
      seenTypes[D] = { EmissionState::Defined, true };
      return true;
    }

    auto &state = seenTypes[D];
    switch (state.first) {
    case EmissionState::DefinitionRequested:
      declsToWrite.push_back(D);
      return false;
    case EmissionState::DefinitionInProgress:
      llvm_unreachable("circular requirements");
    case EmissionState::Defined:
      return true;
    }
  }

  void forwardDeclare(const NominalTypeDecl *NTD,
                      std::function<void (void)> Printer) {
    if (NTD->getModuleContext()->isStdlibModule())
      return;
    auto &state = seenTypes[NTD];
    if (state.second)
      return;
    Printer();
    state.second = true;
  }

  bool forwardDeclare(const ClassDecl *CD) {
    if (!CD->isObjC() || CD->isForeign())
      return false;
    forwardDeclare(CD, [&]{ os << "@class " << getNameForObjC(CD) << ";\n"; });
    return true;
  }

  void forwardDeclare(const ProtocolDecl *PD) {
    assert(PD->isObjC() ||
           *PD->getKnownProtocolKind() == KnownProtocolKind::AnyObject);
    forwardDeclare(PD, [&]{
      os << "@protocol " << getNameForObjC(PD) << ";\n";
    });
  }
  
  void forwardDeclare(const EnumDecl *ED) {
    assert(ED->isObjC() || ED->hasClangNode());
    
    forwardDeclare(ED, [&]{
      os << "enum " << ED->getName() << " : ";
      printer.print(ED->getRawType(), OTK_None);
      os << ";\n";
    });
  }

  void forwardDeclareMemberTypes(DeclRange members) {
    SmallVector<ValueDecl *, 4> nestedTypes;
    for (auto member : members) {
      auto VD = dyn_cast<ValueDecl>(member);
      if (!VD || !printer.shouldInclude(VD))
        continue;

      // Catch nested types and emit their definitions /after/ this class.
      if (isa<TypeDecl>(VD)) {
        // Don't emit nested types that are just implicitly @objc.
        // You should have to opt into this, since they are even less
        // namespaced than usual.
        if (std::any_of(VD->getAttrs().begin(), VD->getAttrs().end(),
                        [](const DeclAttribute *attr) {
                          return isa<ObjCAttr>(attr) && !attr->isImplicit();
                        })) {
          nestedTypes.push_back(VD);
        }
        continue;
      }

      ReferencedTypeFinder::walk(VD->getType(),
                                 [this](ReferencedTypeFinder &finder,
                                        const TypeDecl *TD) {
        if (auto CD = dyn_cast<ClassDecl>(TD)) {
          if (!forwardDeclare(CD)) {
            (void)addImport(CD);
          }
        } else if (auto PD = dyn_cast<ProtocolDecl>(TD))
          forwardDeclare(PD);
        else if (addImport(TD))
          return;
        else if (auto ED = dyn_cast<EnumDecl>(TD))
          forwardDeclare(ED);
        else if (auto TAD = dyn_cast<TypeAliasDecl>(TD))
          finder.visit(TAD->getUnderlyingType());
        else if (isa<AbstractTypeParamDecl>(TD))
          llvm_unreachable("should not see type params here");
        else
          assert(false && "unknown local type decl");
      });
    }

    declsToWrite.insert(declsToWrite.end()-1, nestedTypes.rbegin(),
                        nestedTypes.rend());

    // Separate forward declarations from the class itself.
    os << '\n';
  }

  bool writeClass(const ClassDecl *CD) {
    if (addImport(CD))
      return true;

    if (seenTypes[CD].first == EmissionState::Defined)
      return true;

    bool allRequirementsSatisfied = true;

    const ClassDecl *superclass = nullptr;
    if (Type superTy = CD->getSuperclass()) {
      superclass = superTy->getClassOrBoundGenericClass();
      allRequirementsSatisfied &= require(superclass);
    }
    for (auto proto : CD->getLocalProtocols(
                        ConformanceLookupKind::OnlyExplicit))
      if (printer.shouldInclude(proto))
        allRequirementsSatisfied &= require(proto);

    if (!allRequirementsSatisfied)
      return false;

    seenTypes[CD] = { EmissionState::Defined, true };
    forwardDeclareMemberTypes(CD->getMembers());
    printer.print(CD);
    return true;
  }

  bool writeProtocol(const ProtocolDecl *PD) {
    if (addImport(PD))
      return true;

    auto knownProtocol = PD->getKnownProtocolKind();
    if (knownProtocol && *knownProtocol == KnownProtocolKind::AnyObject)
      return true;

    if (seenTypes[PD].first == EmissionState::Defined)
      return true;

    bool allRequirementsSatisfied = true;

    for (auto proto : PD->getInheritedProtocols(nullptr)) {
      assert(proto->isObjC());
      allRequirementsSatisfied &= require(proto);
    }

    if (!allRequirementsSatisfied)
      return false;

    seenTypes[PD] = { EmissionState::Defined, true };
    forwardDeclareMemberTypes(PD->getMembers());
    printer.print(PD);
    return true;
  }

  bool writeExtension(const ExtensionDecl *ED) {
    bool allRequirementsSatisfied = true;

    const ClassDecl *CD = ED->getExtendedType()->getClassOrBoundGenericClass();
    allRequirementsSatisfied &= require(CD);
    for (auto proto : ED->getLocalProtocols())
      if (printer.shouldInclude(proto))
        allRequirementsSatisfied &= require(proto);

    if (!allRequirementsSatisfied)
      return false;

    forwardDeclareMemberTypes(ED->getMembers());
    printer.print(ED);
    return true;
  }
  
  bool writeEnum(const EnumDecl *ED) {
    if (addImport(ED))
      return true;
    
    if (seenTypes[ED].first == EmissionState::Defined)
      return true;
    
    seenTypes[ED] = {EmissionState::Defined, true};
    printer.print(ED);

    ASTContext &ctx = M.getASTContext();

    auto protos = ED->getAllProtocols();
    auto errorTypeProto = ctx.getProtocol(KnownProtocolKind::ErrorType);
    if (std::find(protos.begin(), protos.end(), errorTypeProto) !=
        protos.end()) {
      bool hasDomainCase = std::any_of(ED->getAllElements().begin(),
                                       ED->getAllElements().end(),
                                       [](const EnumElementDecl *elem) {
        return elem->getName().str() == "Domain";
      });
      if (!hasDomainCase) {
        os << "static NSString * __nonnull const " << ED->getName()
           << "Domain = @\"" << M.getName() << "." << ED->getName() << "\";\n";
      }
    }

    return true;
  }

  void writePrologue(raw_ostream &out) {
    out << "// Generated by " << version::getSwiftFullVersion() << "\n"
           "#pragma clang diagnostic push\n"
           "\n"
           "#if defined(__has_include) && "
             "__has_include(<swift/objc-prologue.h>)\n"
           "# include <swift/objc-prologue.h>\n"
           "#endif\n"
           "\n"
           "#pragma clang diagnostic ignored \"-Wauto-import\"\n"
           "#include <objc/NSObject.h>\n"
           "#include <stdint.h>\n"
           "#include <stddef.h>\n"
           "#include <stdbool.h>\n"
           "\n"
           "#if !defined(SWIFT_TYPEDEFS)\n"
           "# define SWIFT_TYPEDEFS 1\n"
           "# if defined(__has_include) && __has_include(<uchar.h>)\n"
           "#  include <uchar.h>\n"
           "# elif !defined(__cplusplus) || __cplusplus < 201103L\n"
           "typedef uint_least16_t char16_t;\n"
           "typedef uint_least32_t char32_t;\n"
           "# endif\n"
#define MAP_SIMD_TYPE(C_TYPE, _) \
           "typedef " #C_TYPE " swift_" #C_TYPE "2"       \
           "  __attribute__((__ext_vector_type__(2)));\n" \
           "typedef " #C_TYPE " swift_" #C_TYPE "3"       \
           "  __attribute__((__ext_vector_type__(3)));\n" \
           "typedef " #C_TYPE " swift_" #C_TYPE "4"       \
           "  __attribute__((__ext_vector_type__(4)));\n"
#include "swift/ClangImporter/SIMDMappedTypes.def"
           "#endif\n"
           "\n"
           "#if !defined(SWIFT_PASTE)\n"
           "# define SWIFT_PASTE_HELPER(x, y) x##y\n"
           "# define SWIFT_PASTE(x, y) SWIFT_PASTE_HELPER(x, y)\n"
           "#endif"
           "\n"
           "#if !defined(SWIFT_METATYPE)\n"
           "# define SWIFT_METATYPE(X) Class\n"
           "#endif\n"
           "\n"
           "#if defined(__has_attribute) && "
             "__has_attribute(objc_runtime_name)\n"
           "# define SWIFT_RUNTIME_NAME(X) "
             "__attribute__((objc_runtime_name(X)))\n"
           "#else\n"
           "# define SWIFT_RUNTIME_NAME(X)\n"
           "#endif\n"
           "#if defined(__has_attribute) && "
             "__has_attribute(swift_name)\n"
           "# define SWIFT_COMPILE_NAME(X) "
             "__attribute__((swift_name(X)))\n"
           "#else\n"
           "# define SWIFT_COMPILE_NAME(X)\n"
           "#endif\n"
           "#if !defined(SWIFT_CLASS_EXTRA)\n"
           "# define SWIFT_CLASS_EXTRA\n"
           "#endif\n"
           "#if !defined(SWIFT_PROTOCOL_EXTRA)\n"
           "# define SWIFT_PROTOCOL_EXTRA\n"
           "#endif\n"
           "#if !defined(SWIFT_ENUM_EXTRA)\n"
           "# define SWIFT_ENUM_EXTRA\n"
           "#endif\n"
           "#if !defined(SWIFT_CLASS)\n"
           "# if defined(__has_attribute) && "
             "__has_attribute(objc_subclassing_restricted) \n"
           "#  define SWIFT_CLASS(SWIFT_NAME) SWIFT_RUNTIME_NAME(SWIFT_NAME) "
             "__attribute__((objc_subclassing_restricted)) "
             "SWIFT_CLASS_EXTRA\n"
           "#  define SWIFT_CLASS_NAMED(SWIFT_NAME) "
             "__attribute__((objc_subclassing_restricted)) "
             "SWIFT_COMPILE_NAME(SWIFT_NAME) "
             "SWIFT_CLASS_EXTRA\n"
           "# else\n"
           "#  define SWIFT_CLASS(SWIFT_NAME) SWIFT_RUNTIME_NAME(SWIFT_NAME) "
             "SWIFT_CLASS_EXTRA\n"
           "#  define SWIFT_CLASS_NAMED(SWIFT_NAME) "
             "SWIFT_COMPILE_NAME(SWIFT_NAME) "
             "SWIFT_CLASS_EXTRA\n"
           "# endif\n"
           "#endif\n"
           "\n"
           "#if !defined(SWIFT_PROTOCOL)\n"
           "# define SWIFT_PROTOCOL(SWIFT_NAME) SWIFT_RUNTIME_NAME(SWIFT_NAME) "
             "SWIFT_PROTOCOL_EXTRA\n"
           "# define SWIFT_PROTOCOL_NAMED(SWIFT_NAME) "
             "SWIFT_COMPILE_NAME(SWIFT_NAME) "
             "SWIFT_PROTOCOL_EXTRA\n"
           "#endif\n"
           "\n"
           "#if !defined(SWIFT_EXTENSION)\n"
           "# define SWIFT_EXTENSION(M) SWIFT_PASTE(M##_Swift_, __LINE__)\n"
           "#endif\n"
           "\n"
           "#if !defined(OBJC_DESIGNATED_INITIALIZER)\n"
           "# if defined(__has_attribute) && "
             "__has_attribute(objc_designated_initializer)\n"
           "#  define OBJC_DESIGNATED_INITIALIZER "
             "__attribute__((objc_designated_initializer))\n"
           "# else\n"
           "#  define OBJC_DESIGNATED_INITIALIZER\n"
           "# endif\n"
           "#endif\n"
           "#if !defined(SWIFT_ENUM)\n"
           "# define SWIFT_ENUM(_type, _name) "
             "enum _name : _type _name; "
             "enum SWIFT_ENUM_EXTRA _name : _type\n"
           "#endif\n"
           ;
    static_assert(SWIFT_MAX_IMPORTED_SIMD_ELEMENTS == 4,
                "need to add SIMD typedefs here if max elements is increased");
  }

  bool isUnderlyingModule(Module *import) {
    if (bridgingHeader.empty())
      return import != &M && import->getName() == M.getName();

    auto importer =
      static_cast<ClangImporter *>(import->getASTContext()
                                     .getClangModuleLoader());
    return import == importer->getImportedHeaderModule();
  }

  void writeImports(raw_ostream &out) {
    out << "#if defined(__has_feature) && __has_feature(modules)\n";

    // Track printed names to handle overlay modules.
    llvm::SmallPtrSet<Identifier, 8> seenImports;
    bool includeUnderlying = false;
    for (auto import : imports) {
      if (auto *swiftModule = import.dyn_cast<Module *>()) {
        auto Name = swiftModule->getName();
        if (isUnderlyingModule(swiftModule)) {
          includeUnderlying = true;
          continue;
        }
        if (seenImports.insert(Name).second)
          out << "@import " << Name.str() << ";\n";
      } else {
        const auto *clangModule = import.get<const clang::Module *>();
        out << "@import ";
        // FIXME: This should be an API on clang::Module.
        SmallVector<StringRef, 4> submoduleNames;
        do {
          submoduleNames.push_back(clangModule->Name);
          clangModule = clangModule->Parent;
        } while (clangModule);
        interleave(submoduleNames.rbegin(), submoduleNames.rend(),
                   [&out](StringRef next) { out << next; },
                   [&out] { out << "."; });
        out << ";\n";
      }
    }

    out << "#endif\n\n";

    if (includeUnderlying) {
      if (bridgingHeader.empty())
        out << "#import <" << M.getName().str() << '/' << M.getName().str()
            << ".h>\n\n";
      else
        out << "#import \"" << bridgingHeader << "\"\n\n";
    }
  }

  bool writeToStream(raw_ostream &out) {
    SmallVector<Decl *, 64> decls;
    M.getTopLevelDecls(decls);

    auto newEnd = std::remove_if(decls.begin(), decls.end(),
                                 [this](const Decl *D) -> bool {
      if (auto VD = dyn_cast<ValueDecl>(D))
        return !printer.shouldInclude(VD);

      if (auto ED = dyn_cast<ExtensionDecl>(D)) {
        auto baseClass = ED->getExtendedType()->getClassOrBoundGenericClass();
        return !baseClass || !printer.shouldInclude(baseClass) ||
               baseClass->isForeign();
      }
      return true;
    });
    decls.erase(newEnd, decls.end());

    // REVERSE sort the decls, since we are going to copy them onto a stack.
    llvm::array_pod_sort(decls.begin(), decls.end(),
                         [](Decl * const *lhs, Decl * const *rhs) -> int {
      enum : int {
        Ascending = -1,
        Equivalent = 0,
        Descending = 1,
      };

      assert(*lhs != *rhs && "duplicate top-level decl");

      auto getSortName = [](const Decl *D) -> StringRef {
        if (auto VD = dyn_cast<ValueDecl>(D))
          return VD->getName().str();

        if (auto ED = dyn_cast<ExtensionDecl>(D)) {
          auto baseClass = ED->getExtendedType()->getClassOrBoundGenericClass();
          return baseClass->getName().str();
        }
        llvm_unreachable("unknown top-level ObjC decl");
      };

      // Sort by names.
      int result = getSortName(*rhs).compare(getSortName(*lhs));
      if (result != 0)
        return result;

      // Prefer value decls to extensions.
      assert(!(isa<ValueDecl>(*lhs) && isa<ValueDecl>(*rhs)));
      if (isa<ValueDecl>(*lhs) && !isa<ValueDecl>(*rhs))
        return Descending;
      if (!isa<ValueDecl>(*lhs) && isa<ValueDecl>(*rhs))
        return Ascending;

      // Break ties in extensions by putting smaller extensions last (in reverse
      // order).
      // FIXME: This will end up taking linear time.
      auto lhsMembers = cast<ExtensionDecl>(*lhs)->getMembers();
      auto rhsMembers = cast<ExtensionDecl>(*rhs)->getMembers();
      unsigned numLHSMembers = std::distance(lhsMembers.begin(), 
                                             lhsMembers.end());
      unsigned numRHSMembers = std::distance(rhsMembers.begin(), 
                                             rhsMembers.end());
      if (numLHSMembers != numRHSMembers)
        return numLHSMembers < numRHSMembers ? Descending : Ascending;

      // Or the extension with fewer protocols.
      auto lhsProtos = cast<ExtensionDecl>(*lhs)->getLocalProtocols();
      auto rhsProtos = cast<ExtensionDecl>(*rhs)->getLocalProtocols();
      if (lhsProtos.size() != rhsProtos.size())
        return lhsProtos.size() < rhsProtos.size() ? Descending : Ascending;

      // If that fails, arbitrarily pick the extension whose protocols are
      // alphabetically first.
      auto mismatch =
        std::mismatch(lhsProtos.begin(), lhsProtos.end(), rhsProtos.begin(),
                      [getSortName] (const ProtocolDecl *nextLHSProto,
                                     const ProtocolDecl *nextRHSProto) {
        return nextLHSProto->getName() != nextRHSProto->getName();
      });
      if (mismatch.first == lhsProtos.end())
        return Equivalent;
      StringRef lhsProtoName = (*mismatch.first)->getName().str();
      return lhsProtoName.compare((*mismatch.second)->getName().str());
    });

    assert(declsToWrite.empty());
    declsToWrite.assign(decls.begin(), decls.end());

    while (!declsToWrite.empty()) {
      const Decl *D = declsToWrite.back();
      bool success = true;

      if (isa<ValueDecl>(D)) {
        if (auto CD = dyn_cast<ClassDecl>(D))
          success = writeClass(CD);
        else if (auto PD = dyn_cast<ProtocolDecl>(D))
          success = writeProtocol(PD);
        else if (auto ED = dyn_cast<EnumDecl>(D))
          success = writeEnum(ED);
        else
          llvm_unreachable("unknown top-level ObjC value decl");

      } else if (auto ED = dyn_cast<ExtensionDecl>(D)) {
        success = writeExtension(ED);

      } else {
        llvm_unreachable("unknown top-level ObjC decl");
      }

      if (success) {
        assert(declsToWrite.back() == D);
        os << "\n";
        declsToWrite.pop_back();
      }
    }

    writePrologue(out);
    writeImports(out);
    out <<
        "#pragma clang diagnostic ignored \"-Wproperty-attribute-mismatch\"\n"
        "#pragma clang diagnostic ignored \"-Wduplicate-method-arg\"\n"
      << os.str()
      << "#pragma clang diagnostic pop\n";
    return false;
  }
};
}

bool swift::printAsObjC(llvm::raw_ostream &os, Module *M,
                        StringRef bridgingHeader,
                        Accessibility minRequiredAccess) {
  llvm::PrettyStackTraceString trace("While generating Objective-C header");
  return ModuleWriter(*M, bridgingHeader, minRequiredAccess).writeToStream(os);
}
