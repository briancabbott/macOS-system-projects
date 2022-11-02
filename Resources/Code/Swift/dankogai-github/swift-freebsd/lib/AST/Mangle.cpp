//===--- Mangle.cpp - Swift Name Mangling --------------------------------===//
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
//  This file implements declaration name mangling in Swift.
//
//===----------------------------------------------------------------------===//

#include "swift/AST/Mangle.h"
#include "swift/AST/ASTContext.h"
#include "swift/AST/ASTVisitor.h"
#include "swift/AST/Attr.h"
#include "swift/AST/Initializer.h"
#include "swift/AST/Module.h"
#include "swift/AST/ProtocolConformance.h"
#include "swift/Basic/Demangle.h"
#include "swift/Basic/Punycode.h"
#include "clang/Basic/CharInfo.h"
#include "clang/AST/Attr.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclObjC.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/SaveAndRestore.h"
#include "llvm/Support/raw_ostream.h"

using namespace swift;
using namespace Mangle;

static bool isNonAscii(StringRef str) {
  for (unsigned char c : str) {
    if (c >= 0x80)
      return true;
  }
  return false;
}

namespace {
  /// A helpful little wrapper for a value that should be mangled
  /// in a particular, compressed value.
  class Index {
    unsigned N;
  public:
    explicit Index(unsigned n) : N(n) {}
    friend raw_ostream &operator<<(raw_ostream &out, Index n) {
      if (n.N != 0) out << (n.N - 1);
      return (out << '_');
    }
  };        
}
        
/// Mangle a StringRef as an identifier into a buffer.
void Mangler::mangleIdentifier(StringRef str, OperatorFixity fixity,
                               bool isOperator) {
  auto operatorKind = [=]() -> Demangle::OperatorKind {
    if (!isOperator) return Demangle::OperatorKind::NotOperator;
    switch (fixity) {
    case OperatorFixity::NotOperator:return Demangle::OperatorKind::NotOperator;
    case OperatorFixity::Prefix: return Demangle::OperatorKind::Prefix;
    case OperatorFixity::Postfix: return Demangle::OperatorKind::Postfix;
    case OperatorFixity::Infix: return Demangle::OperatorKind::Infix;
    }
    llvm_unreachable("invalid operator fixity");
  }();

  std::string buf;
  Demangle::mangleIdentifier(str.data(), str.size(), operatorKind, buf,
                             UsePunycode);
  Buffer << buf;
}

/// Mangle an identifier into the buffer.
void Mangler::mangleIdentifier(Identifier ident, OperatorFixity fixity) {
  StringRef str = ident.str();
  assert(!str.empty() && "mangling an empty identifier!");
  return mangleIdentifier(str, fixity, ident.isOperator());
}

bool Mangler::tryMangleSubstitution(const void *ptr) {
  auto ir = Substitutions.find(ptr);
  if (ir == Substitutions.end()) return false;

  // substitution ::= 'S' integer? '_'

  unsigned index = ir->second;
  Buffer << 'S';
  if (index) Buffer << (index - 1);
  Buffer << '_';
  return true;
}

void Mangler::addSubstitution(const void *ptr) {
  Substitutions.insert(std::make_pair(ptr, Substitutions.size()));
}

/// Mangle the context of the given declaration as a <context.
/// This is the top-level entrypoint for mangling <context>.
void Mangler::mangleContextOf(const ValueDecl *decl, BindGenerics shouldBind) {
  auto clangDecl = decl->getClangDecl();

  // Classes and protocols implemented in Objective-C have a special context
  // mangling.
  //   known-context ::= 'So'
  if (isa<ClassDecl>(decl) && clangDecl) {
    assert(isa<clang::ObjCInterfaceDecl>(clangDecl) ||
           isa<clang::TypedefDecl>(clangDecl));
    Buffer << "So";
    return;
  }
  
  if (isa<ProtocolDecl>(decl) && clangDecl) {
    assert(isa<clang::ObjCProtocolDecl>(clangDecl));
    Buffer << "So";
    return; 
  }

  // Declarations provided by a C module have a special context mangling.
  //   known-context ::= 'SC'
  // Do a dance to avoid a layering dependency.
  if (auto file = dyn_cast<FileUnit>(decl->getDeclContext())) {
    if (file->getKind() == FileUnitKind::ClangModule) {
      Buffer << "SC";
      return;
    }
  }

  // Just mangle the decl's DC.
  mangleContext(decl->getDeclContext(), shouldBind);
}

namespace {
  class FindFirstVariable :
    public PatternVisitor<FindFirstVariable, VarDecl *> {
  public:
    VarDecl *visitNamedPattern(NamedPattern *P) {
      return P->getDecl();
    }

    VarDecl *visitTuplePattern(TuplePattern *P) {
      for (auto &elt : P->getElements()) {
        VarDecl *var = visit(elt.getPattern());
        if (var) return var;
      }
      return nullptr;
    }

    VarDecl *visitParenPattern(ParenPattern *P) {
      return visit(P->getSubPattern());
    }
    VarDecl *visitVarPattern(VarPattern *P) {
      return visit(P->getSubPattern());
    }
    VarDecl *visitTypedPattern(TypedPattern *P) {
      return visit(P->getSubPattern());
    }
    VarDecl *visitAnyPattern(AnyPattern *P) {
      return nullptr;
    }

    // Refutable patterns shouldn't ever come up.
#define REFUTABLE_PATTERN(ID, BASE)                                        \
    VarDecl *visit##ID##Pattern(ID##Pattern *P) {                          \
      llvm_unreachable("shouldn't be visiting a refutable pattern here!"); \
    }
#define PATTERN(ID, BASE)
#include "swift/AST/PatternNodes.def"
  };
}

/// Find the first identifier bound by the given binding.  This
/// assumes that field and global-variable bindings always bind at
/// least one name, which is probably a reasonable assumption but may
/// not be adequately enforced.
static VarDecl *findFirstVariable(PatternBindingDecl *binding) {
  for (auto entry : binding->getPatternList()) {
    auto var = FindFirstVariable().visit(entry.getPattern());
    if (var) return var;
  }
  llvm_unreachable("pattern-binding bound no variables?");
}

CanGenericSignature Mangler::getCanonicalSignatureOrNull(GenericSignature *sig,
                                                         ModuleDecl &M) {

  if (!sig)
    return nullptr;
  Mod = &M;
  return sig->getCanonicalManglingSignature(M);
}

void Mangler::mangleContext(const DeclContext *ctx, BindGenerics shouldBind) {
  switch (ctx->getContextKind()) {
  case DeclContextKind::Module:
    return mangleModule(cast<Module>(ctx));

  case DeclContextKind::FileUnit:
    assert(!isa<BuiltinUnit>(ctx) && "mangling member of builtin module!");
    mangleContext(ctx->getParent(), shouldBind);
    return;

  case DeclContextKind::SerializedLocal: {
    auto local = cast<SerializedLocalDeclContext>(ctx);
    switch (local->getLocalDeclContextKind()) {
    case LocalDeclContextKind::AbstractClosure:
      mangleClosureEntity(cast<SerializedAbstractClosureExpr>(local),
                          ResilienceExpansion::Minimal, /*uncurry*/ 0);
      return;
    case LocalDeclContextKind::DefaultArgumentInitializer: {
      auto argInit = cast<SerializedDefaultArgumentInitializer>(local);
      mangleDefaultArgumentEntity(ctx->getParent(), argInit->getIndex());
      return;
    }
    case LocalDeclContextKind::PatternBindingInitializer: {
      auto patternInit = cast<SerializedPatternBindingInitializer>(local);
      auto var = findFirstVariable(patternInit->getBinding());
      mangleInitializerEntity(var);
      return;
    }
    case LocalDeclContextKind::TopLevelCodeDecl:
      return mangleContext(local->getParent(), shouldBind);
    }
  }

  case DeclContextKind::NominalTypeDecl:
    mangleNominalType(cast<NominalTypeDecl>(ctx), ResilienceExpansion::Minimal,
                      shouldBind);
    return;

  case DeclContextKind::ExtensionDecl: {
    auto ExtD = cast<ExtensionDecl>(ctx);
    auto ExtTy = ExtD->getExtendedType();
    // Recover from erroneous extension.
    if (ExtTy.isNull() || ExtTy->is<ErrorType>())
      return mangleContext(ExtD->getDeclContext(), shouldBind);

    auto decl = ExtTy->getAnyNominal();
    assert(decl && "extension of non-nominal type?");
    // Mangle the module name if:
    // - the extension is defined in a different module from the actual nominal
    //   type decl,
    // - the extension is constrained, or
    // - the extension is to a protocol.
    // FIXME: In a world where protocol extensions are dynamically dispatched,
    // "extension is to a protocol" would no longer be a reason to use the
    // extension mangling, because an extension method implementation could be
    // resiliently moved into the original protocol itself.
    if (ExtD->getParentModule() != decl->getParentModule()
        || ExtD->isConstrainedExtension()
        || ExtD->getDeclaredInterfaceType()->isExistentialType()) {
      auto sig = ExtD->getGenericSignature();
      // If the extension is constrained, mangle the generic signature that
      // constrains it.
      bool mangleSignature = sig && ExtD->isConstrainedExtension();
      Buffer << (mangleSignature ? 'e' : 'E');
      mangleModule(ExtD->getParentModule());
      if (mangleSignature) {
        Mod = ExtD->getModuleContext();
        mangleGenericSignature(sig, ResilienceExpansion::Minimal);
      }
    }
    mangleNominalType(decl, ResilienceExpansion::Minimal, shouldBind,
                    getCanonicalSignatureOrNull(ExtD->getGenericSignature(),
                                                *ExtD->getParentModule()),
                    ExtD->getGenericParams());
    return;
  }

  case DeclContextKind::AbstractClosureExpr:
    return mangleClosureEntity(cast<AbstractClosureExpr>(ctx),
                               ResilienceExpansion::Minimal, /*uncurry*/ 0);

  case DeclContextKind::AbstractFunctionDecl: {
    auto fn = cast<AbstractFunctionDecl>(ctx);

    // Constructors and destructors as contexts are always mangled
    // using the non-(de)allocating variants.
    if (auto ctor = dyn_cast<ConstructorDecl>(fn)) {
      return mangleConstructorEntity(ctor, /*allocating*/ false,
                                     ResilienceExpansion::Minimal,
                                     /*uncurry*/ 0);
    }
    
    if (auto dtor = dyn_cast<DestructorDecl>(fn))
      return mangleDestructorEntity(dtor, /*deallocating*/ false);
    
    return mangleEntity(fn, ResilienceExpansion::Minimal, /*uncurry*/ 0);
  }

  case DeclContextKind::Initializer:
    switch (cast<Initializer>(ctx)->getInitializerKind()) {
    case InitializerKind::DefaultArgument: {
      auto argInit = cast<DefaultArgumentInitializer>(ctx);
      mangleDefaultArgumentEntity(ctx->getParent(),
                                  argInit->getIndex());
      return;
    }

    case InitializerKind::PatternBinding: {
      auto patternInit = cast<PatternBindingInitializer>(ctx);
      auto var = findFirstVariable(patternInit->getBinding());
      mangleInitializerEntity(var);
      return;
    }
    }
    llvm_unreachable("bad initializer kind");

  case DeclContextKind::TopLevelCodeDecl:
    // Mangle the containing module context.
    return mangleContext(ctx->getParent(), shouldBind);
  }

  llvm_unreachable("bad decl context");
}

void Mangler::mangleModule(const Module *module) {
  assert(!module->getParent() && "cannot mangle nested modules!");

  // Try the special 'swift' substitution.
  // context ::= known-module
  // known-module ::= 's'
  if (module->isStdlibModule()) {
    Buffer << "s";
    return;
  }

  // context ::= substitution
  if (tryMangleSubstitution(module)) return;

  // context ::= identifier
  mangleIdentifier(module->getName());

  addSubstitution(module);
}

/// Bind the generic parameters from the given list and its parents.
void Mangler::bindGenericParameters(CanGenericSignature sig,
                                    const GenericParamList *genericParams) {
  if (sig)
    CurGenericSignature = sig;
  assert(genericParams);
  SmallVector<const GenericParamList *, 2> paramLists;
  
  // Determine the depth our parameter list is at. We don't actually need to
  // emit the outer parameters because they should have been emitted as part of
  // the outer context.
  assert(ArchetypesDepth == genericParams->getDepth());
  ArchetypesDepth++;
  unsigned index = 0;
  for (auto archetype : genericParams->getPrimaryArchetypes()) {
    // Remember the current depth and level.
    ArchetypeInfo info;
    info.Depth = ArchetypesDepth;
    info.Index = index++;
    assert(!Archetypes.count(archetype) ||
           (Archetypes[archetype].Depth == info.Depth &&
            Archetypes[archetype].Index == info.Index));
    Archetypes.insert(std::make_pair(archetype, info));
  }
}

static OperatorFixity getDeclFixity(const ValueDecl *decl) {
  if (!decl->getName().isOperator())
    return OperatorFixity::NotOperator;

  switch (decl->getAttrs().getUnaryOperatorKind()) {
  case UnaryOperatorKind::Prefix: return OperatorFixity::Prefix;
  case UnaryOperatorKind::Postfix: return OperatorFixity::Postfix;
  case UnaryOperatorKind::None: return OperatorFixity::Infix;
  }
  llvm_unreachable("bad UnaryOperatorKind");
}

void Mangler::mangleDeclName(const ValueDecl *decl) {
  if (decl->getDeclContext()->isLocalContext()) {
    // Mangle local declarations with a numeric discriminator.
    // decl-name ::= 'L' index identifier
    Buffer << 'L' << Index(decl->getLocalDiscriminator());
    // Fall through to mangle the <identifier>.

  } else if (decl->hasAccessibility() &&
             decl->getFormalAccess() == Accessibility::Private) {
    // Mangle non-local private declarations with a textual discriminator
    // based on their enclosing file.
    // decl-name ::= 'P' identifier identifier

    // Don't bother to use the private discriminator if the enclosing context
    // is also private.
    auto isWithinPrivateNominal = [](const Decl *D) -> bool {
      const DeclContext *DC = D->getDeclContext();
      if (!DC->isTypeContext()) {
        assert((DC->isModuleScopeContext() || DC->isLocalContext()) &&
               "do we need a private discriminator for this context?");
        return false;
      }

      auto nominal = dyn_cast<NominalTypeDecl>(DC);
      if (!nominal)
        nominal = cast<ExtensionDecl>(DC)->getExtendedType()->getAnyNominal();
      return nominal->getFormalAccess() == Accessibility::Private;
    };

    if (!isWithinPrivateNominal(decl)) {
      // The first <identifier> is a discriminator string unique to the decl's
      // original source file.
      auto topLevelContext = decl->getDeclContext()->getModuleScopeContext();
      auto fileUnit = cast<FileUnit>(topLevelContext);

      Identifier discriminator =
        fileUnit->getDiscriminatorForPrivateValue(decl);
      assert(!discriminator.empty());
      assert(!isNonAscii(discriminator.str()) &&
             "discriminator contains non-ASCII characters");
      (void) isNonAscii;
      assert(!clang::isDigit(discriminator.str().front()) &&
             "not a valid identifier");

      Buffer << 'P';
      mangleIdentifier(discriminator);
    }
    // Fall through to mangle the name.
  }

  // decl-name ::= identifier
  mangleIdentifier(decl->getName(), getDeclFixity(decl));
}

static void bindAllGenericParameters(Mangler &mangler,
                                     CanGenericSignature sig,
                                     GenericParamList *generics) {
  if (!generics) {
    mangler.resetArchetypesDepth();
    return;
  }
  bindAllGenericParameters(mangler, nullptr, generics->getOuterParameters());
  mangler.bindGenericParameters(sig, generics);
}

void Mangler::mangleTypeForDebugger(Type Ty, const DeclContext *DC) {
  assert(DWARFMangling && "DWARFMangling expected whn mangling for debugger");

  // Polymorphic function types carry their own generic parameters and
  // manglePolymorphicType will bind them.
  bool BindGenericParams = Ty->getKind() != TypeKind::PolymorphicFunction;

  Buffer << "_Tt";

  // Move up to the innermost generic context.
  while (DC && !DC->isInnermostContextGeneric()) DC = DC->getParent();
  if (DC && BindGenericParams)
    bindAllGenericParameters(*this,
                             getCanonicalSignatureOrNull(
                               DC->getGenericSignatureOfContext(),
                               *DC->getParentModule()),
                             DC->getGenericParamsOfContext());
  DeclCtx = DC;

  mangleType(Ty, ResilienceExpansion::Minimal, /*uncurry*/ 0);
}

void Mangler::mangleDeclTypeForDebugger(const ValueDecl *decl) {
  assert(DWARFMangling && "DWARFMangling expected");
  Buffer << "_Tt";

  typedef std::pair<bool, BindGenerics> result_t;
  struct ClassifyDecl : swift::DeclVisitor<ClassifyDecl, result_t> {

    /// TypeAliasDecls need to be mangled.
    result_t visitTypeAliasDecl(TypeDecl *D) {
      llvm_unreachable("filtered out above");
    }
    /// Other TypeDecls don't need their types mangled in.
    result_t visitTypeDecl(TypeDecl *D) {
      return { false, BindGenerics::None };
    }

    /// Function-like declarations do, but they should have
    /// polymorphic type and therefore don't need specific binding.
    result_t visitFuncDecl(FuncDecl *D) {
      if (D->getDeclContext()->isTypeContext())
        return { true, BindGenerics::Enclosing };
      else
        return { true, BindGenerics::All };
    }
    result_t visitConstructorDecl(ConstructorDecl *D) {
      return { true, BindGenerics::Enclosing };
    }
    result_t visitDestructorDecl(DestructorDecl *D) {
      return { true, BindGenerics::Enclosing };
    }
    result_t visitEnumElementDecl(EnumElementDecl *D) {
      return { true, BindGenerics::Enclosing };
    }

    /// All other values need to have contextual archetypes bound.
    result_t visitVarDecl(VarDecl *D) {
      return { true, BindGenerics::All };
    }
    result_t visitParamDecl(ParamDecl *D) {
      return { true, BindGenerics::All };
    }
    result_t visitSubscriptDecl(SubscriptDecl *D) {
      return { true, BindGenerics::All };
    }

    /// Make sure we have a case for every ValueDecl.
    result_t visitValueDecl(ValueDecl *D) = delete;

    /// Everything else should be unreachable here.
    result_t visitDecl(Decl *D) {
      llvm_unreachable("not a ValueDecl");
    }
  };

  auto result = ClassifyDecl().visit(const_cast<ValueDecl *>(decl));
  if (!result.first) return;

  // Find the innermost generic context and stash it in DeclCtx.
  // Also track whether DC is just a type ancestor of the decl.
  DeclContext *DC = decl->getDeclContext();
  bool isNonTypeDC = false;
  while (DC && !DC->isInnermostContextGeneric()) {
    if (!isNonTypeDC) isNonTypeDC = !DC->isTypeContext();
    DC = DC->getParent();
  }
  DeclCtx = DC;

  // Bind the generic parameters from that.
  if (DC) {
    // But if the generics come from a type container, they may be
    // accounted for in the decl's type; skip them if so.
    if (result.second == BindGenerics::Enclosing && !isNonTypeDC) {
      while (DC->isTypeContext())
        DC = DC->getParent();
    }
    bindAllGenericParameters(*this,
                             getCanonicalSignatureOrNull(
                               DC->getGenericSignatureOfContext(),
                               *DC->getParentModule()),
                             DC->getGenericParamsOfContext());
  }

  mangleDeclType(decl, ResilienceExpansion::Minimal, /*uncurry*/ 0);
}

/// Is this declaration a method for mangling purposes? If so, we'll leave the
/// Self type out of its mangling.
static bool isMethodDecl(const Decl *decl) {
  return isa<AbstractFunctionDecl>(decl)
    && (isa<NominalTypeDecl>(decl->getDeclContext())
        || isa<ExtensionDecl>(decl->getDeclContext()));
}

static bool genericParamIsBelowDepth(Type type, unsigned methodDepth) {
  if (auto gp = type->getAs<GenericTypeParamType>()) {
    return gp->getDepth() >= methodDepth;
  }
  if (auto dm = type->getAs<DependentMemberType>()) {
    return genericParamIsBelowDepth(dm->getBase(), methodDepth);
  }
  // Non-dependent types in a same-type requirement don't affect whether we
  // mangle the requirement.
  return false;
}

Type Mangler::getDeclTypeForMangling(const ValueDecl *decl,
                                ArrayRef<GenericTypeParamType *> &genericParams,
                                unsigned &initialParamDepth,
                                ArrayRef<Requirement> &requirements,
                                SmallVectorImpl<Requirement> &requirementsBuf) {
  auto &C = decl->getASTContext();
  if (!decl->hasType())
    return ErrorType::get(C);

  Type type = decl->getInterfaceType();

  initialParamDepth = 0;
  CanGenericSignature sig;
  if (auto gft = type->getAs<GenericFunctionType>()) {
    assert(Mod);
    sig = gft->getGenericSignature()->getCanonicalManglingSignature(*Mod);
    CurGenericSignature = sig;
    genericParams = sig->getGenericParams();
    requirements = sig->getRequirements();

    type = FunctionType::get(gft->getInput(), gft->getResult(),
                             gft->getExtInfo());
  } else {
    genericParams = {};
    requirements = {};
  }

  // Shed the 'self' type and generic requirements from method manglings.
  if (C.LangOpts.DisableSelfTypeMangling
      && isMethodDecl(decl)
      && type && !type->is<ErrorType>()) {

    // Drop the Self argument clause from the type.
    type = type->castTo<AnyFunctionType>()->getResult();

    // Drop generic parameters and requirements from the method's context.
    if (auto parentGenericSig =
          decl->getDeclContext()->getGenericSignatureOfContext()) {
      // The method's depth starts below the depth of the context.
      if (!parentGenericSig->getGenericParams().empty())
        initialParamDepth =
          parentGenericSig->getGenericParams().back()->getDepth()+1;

      while (!genericParams.empty()) {
        if (genericParams.front()->getDepth() >= initialParamDepth)
          break;
        genericParams = genericParams.slice(1);
      }

      requirementsBuf.clear();
      for (auto &reqt : sig->getRequirements()) {
        switch (reqt.getKind()) {
        case RequirementKind::WitnessMarker:
          // Not needed for mangling.
          continue;

        case RequirementKind::Conformance:
          // We don't need the requirement if the constrained type is above the
          // method depth.
          if (!genericParamIsBelowDepth(reqt.getFirstType(), initialParamDepth))
            continue;
          break;
        case RequirementKind::SameType:
          // We don't need the requirement if both types are above the method
          // depth, or non-dependent.
          if (!genericParamIsBelowDepth(reqt.getFirstType(),initialParamDepth)&&
              !genericParamIsBelowDepth(reqt.getSecondType(),initialParamDepth))
            continue;
          break;
        }

        // If we fell through the switch, mangle the requirement.
        requirementsBuf.push_back(reqt);
      }
      requirements = requirementsBuf;
    }
  }

  return type;
}

void Mangler::mangleDeclType(const ValueDecl *decl,
                             ResilienceExpansion explosion,
                             unsigned uncurryLevel) {
  ArrayRef<GenericTypeParamType *> genericParams;
  unsigned initialParamDepth;
  ArrayRef<Requirement> requirements;
  SmallVector<Requirement, 4> requirementsBuf;
  Mod = decl->getModuleContext();
  Type type = getDeclTypeForMangling(decl,
                                     genericParams, initialParamDepth,
                                     requirements, requirementsBuf);

  // Mangle the generic signature, if any.
  if (!genericParams.empty() || !requirements.empty()) {
    Buffer << 'u';
    mangleGenericSignatureParts(genericParams, initialParamDepth,
                                requirements, explosion);
  }

  mangleType(type->getCanonicalType(), explosion, uncurryLevel);

  // Bind the declaration's generic context for nested decls.
  if (decl->getInterfaceType()
      && !decl->getInterfaceType()->is<ErrorType>()) {
    if (const auto context = dyn_cast<DeclContext>(decl)) {
      if (auto params = context->getGenericParamsOfContext()) {
        bindAllGenericParameters(*this, nullptr, params);
      }
    }
  }
}

void Mangler::mangleConstrainedType(CanType type,
                                    ResilienceExpansion expansion) {
  // The type constrained by a generic requirement should always be a
  // generic parameter or associated type thereof. Assuming this lets us save
  // an introducer character in the common case when a generic parameter is
  // constrained.
  assert(isa<DependentMemberType>(type) || isa<GenericTypeParamType>(type));

  if (auto gp = dyn_cast<GenericTypeParamType>(type)) {
    mangleGenericParamIndex(gp);
    return;
  }
  mangleType(type, expansion, 0);
}

void Mangler::mangleGenericSignatureParts(
                                        ArrayRef<GenericTypeParamType*> params,
                                        unsigned initialParamDepth,
                                        ArrayRef<Requirement> requirements,
                                        ResilienceExpansion expansion) {
  // Mangle the number of parameters.
  unsigned depth = 0;
  unsigned count = 0;
  
  // Since it's unlikely (but not impossible) to have zero generic parameters
  // at a depth, encode indexes starting from 1, and use a special mangling
  // for zero.
  auto mangleGenericParamCount = [&](unsigned depth, unsigned count) {
    if (depth < initialParamDepth)
      return;
    if (count == 0)
      Buffer << 'z';
    else
      Buffer << Index(count - 1);
  };
  
  // As a special case, mangle nothing if there's a single generic parameter
  // at the initial depth.
  if (params.size() == 1 && params[0]->getDepth() == initialParamDepth)
    goto mangle_requirements;
  
  for (auto param : params) {
    if (param->getDepth() != depth) {
      assert(param->getDepth() > depth && "generic params not ordered");
      while (depth < param->getDepth()) {
        mangleGenericParamCount(depth, count);
        ++depth;
        count = 0;
      }
    }
    assert(param->getIndex() == count && "generic params not ordered");
    ++count;
  }
  mangleGenericParamCount(depth, count);

mangle_requirements:
  bool didMangleRequirement = false;
  // Mangle the requirements.
  for (auto &reqt : requirements) {
    switch (reqt.getKind()) {
    case RequirementKind::WitnessMarker:
      break;
        
    case RequirementKind::Conformance: {
      if (!didMangleRequirement) {
        Buffer << 'R';
        didMangleRequirement = true;
      }
      SmallVector<ProtocolDecl *, 2> protocols;
      if (reqt.getSecondType()->isExistentialType(protocols)
          && protocols.size() == 1) {
        // Protocol constraints are the common case, so mangle them more
        // efficiently.
        // TODO: We could golf this a little more by assuming the first type
        // is a dependent type.
        mangleConstrainedType(reqt.getFirstType()->getCanonicalType(),
                              expansion);
        mangleProtocolName(protocols[0]);
        break;
      }
      mangleConstrainedType(reqt.getFirstType()->getCanonicalType(), expansion);
      mangleType(reqt.getSecondType()->getCanonicalType(), expansion, 0);
      break;
    }

    case RequirementKind::SameType:
      if (!didMangleRequirement) {
        Buffer << 'R';
        didMangleRequirement = true;
      }
      mangleConstrainedType(reqt.getFirstType()->getCanonicalType(), expansion);
      Buffer << 'z';
      mangleType(reqt.getSecondType()->getCanonicalType(), expansion, 0);
      break;
    }
  }
  // Mark end of requirements.
  Buffer << 'r';
}

void Mangler::mangleGenericSignature(const GenericSignature *sig,
                                     ResilienceExpansion expansion) {
  assert(Mod);
  auto canSig = sig->getCanonicalManglingSignature(*Mod);
  CurGenericSignature = canSig;
  mangleGenericSignatureParts(canSig->getGenericParams(), 0,
                              canSig->getRequirements(),
                              expansion);
}

static void mangleMetatypeRepresentation(raw_ostream &Buffer,
                                         MetatypeRepresentation Rep) {
  switch (Rep) {
  case MetatypeRepresentation::Thin:
    Buffer << 't';
    break;
  case MetatypeRepresentation::Thick:
    Buffer << 'T';
    break;
  case MetatypeRepresentation::ObjC:
    Buffer << 'o';
  }
}

void Mangler::mangleGenericParamIndex(GenericTypeParamType *paramTy) {
  if (paramTy->getDepth() > 0) {
    Buffer << 'd';
    Buffer << Index(paramTy->getDepth() - 1);
    Buffer << Index(paramTy->getIndex());
    return;
  }
  if (paramTy->getIndex() == 0) {
    Buffer << 'x';
    return;
  }
  Buffer << Index(paramTy->getIndex() - 1);
}

void Mangler::mangleAssociatedTypeName(DependentMemberType *dmt,
                                       bool canAbbreviate) {
  auto assocTy = dmt->getAssocType();

  if (tryMangleSubstitution(assocTy))
    return;


  // If the base type is known to have a single protocol conformance
  // in the current generic context, then we don't need to disambiguate the
  // associated type name by protocol.
  // FIXME: We ought to be able to get to the generic signature from a
  // dependent type, but can't yet. Shouldn't need this side channel.
  if (!canAbbreviate || !CurGenericSignature || !Mod
      || CurGenericSignature->getConformsTo(dmt->getBase(), *Mod).size() > 1) {
    Buffer << 'P';
    mangleProtocolName(assocTy->getProtocol());
  }
  mangleIdentifier(assocTy->getName());

  addSubstitution(assocTy);
}

/// Mangle a type into the buffer.
///
/// Type manglings should never start with [0-9dz_] or end with [0-9].
///
/// <type> ::= A <natural> <type>    # fixed-sized arrays
/// <type> ::= Bb                    # Builtin.UnsafeValueBuffer
/// <type> ::= Bf <natural> _        # Builtin.Float
/// <type> ::= Bi <natural> _        # Builtin.Integer
/// <type> ::= BO                    # Builtin.UnknownObject
/// <type> ::= Bo                    # Builtin.NativeObject
/// <type> ::= Bb                    # Builtin.BridgeObject
/// <type> ::= Bp                    # Builtin.RawPointer
/// <type> ::= Bv <natural> <type>   # Builtin.Vector
/// <type> ::= C <decl>              # class (substitutable)
/// <type> ::= D <type>              # dynamic Self return
/// <type> ::= ERR                   # Error type
/// <type> ::= 'a' <context> <identifier> # Type alias (DWARF only)
/// <type> ::= F <type> <type>       # function type
/// <type> ::= f <type> <type>       # uncurried function type
/// <type> ::= G <type> <type>+ _    # bound generic type
/// <type> ::= O <decl>              # enum (substitutable)
/// <type> ::= M <type>              # metatype
/// <type> ::= P <protocol-list> _   # protocol composition
/// <type> ::= PM <type>             # existential metatype
/// <type> ::= Q <index>             # archetype with depth=0, index=N
/// <type> ::= Qd <index> <index>    # archetype with depth=M+1, index=N
/// <type> ::= 'Qq' index context    # archetype+context (DWARF only)
///
/// <type> ::= R <type>              # inout
/// <type> ::= T <tuple-element>* _  # tuple
/// <type> ::= U <generic-parameter>+ _ <type>
/// <type> ::= V <decl>              # struct (substitutable)
/// <type> ::= Xo <type>             # unowned reference type
/// <type> ::= Xw <type>             # weak reference type
/// <type> ::= XF <impl-function-type> # SIL function type
///
/// <index> ::= _                    # 0
/// <index> ::= <natural> _          # N+1
///
/// <tuple-element> ::= <identifier>? <type>
void Mangler::mangleType(Type type, ResilienceExpansion explosion,
                         unsigned uncurryLevel) {
  assert((DWARFMangling || type->isCanonical()) &&
         "expecting canonical types when not mangling for the debugger");
  TypeBase *tybase = type.getPointer();
  switch (type->getKind()) {
  case TypeKind::TypeVariable:
    llvm_unreachable("mangling type variable");

  case TypeKind::Module:
    llvm_unreachable("Cannot mangle module type yet");

  case TypeKind::Error:
  case TypeKind::Unresolved:
    Buffer << "ERR";
    return;
      
  // We don't care about these types being a bit verbose because we
  // don't expect them to come up that often in API names.
  case TypeKind::BuiltinFloat:
    switch (cast<BuiltinFloatType>(tybase)->getFPKind()) {
    case BuiltinFloatType::IEEE16: Buffer << "Bf16_"; return;
    case BuiltinFloatType::IEEE32: Buffer << "Bf32_"; return;
    case BuiltinFloatType::IEEE64: Buffer << "Bf64_"; return;
    case BuiltinFloatType::IEEE80: Buffer << "Bf80_"; return;
    case BuiltinFloatType::IEEE128: Buffer << "Bf128_"; return;
    case BuiltinFloatType::PPC128: llvm_unreachable("ppc128 not supported");
    }
    llvm_unreachable("bad floating-point kind");
  case TypeKind::BuiltinInteger: {
    auto width = cast<BuiltinIntegerType>(tybase)->getWidth();
    if (width.isFixedWidth())
      Buffer << "Bi" << width.getFixedWidth() << '_';
    else if (width.isPointerWidth())
      Buffer << "Bw";
    else
      llvm_unreachable("impossible width value");
    return;
  }
  case TypeKind::BuiltinRawPointer:
    Buffer << "Bp";
    return;
  case TypeKind::BuiltinNativeObject:
    Buffer << "Bo";
    return;
  case TypeKind::BuiltinBridgeObject:
    Buffer << "Bb";
    return;
  case TypeKind::BuiltinUnknownObject:
    Buffer << "BO";
    return;
  case TypeKind::BuiltinUnsafeValueBuffer:
    Buffer << "BB";
    return;
  case TypeKind::BuiltinVector:
    Buffer << "Bv" << cast<BuiltinVectorType>(tybase)->getNumElements();
    mangleType(cast<BuiltinVectorType>(tybase)->getElementType(), explosion,
               uncurryLevel);
    return;

  case TypeKind::NameAlias: {
    assert(DWARFMangling && "sugared types are only legal for the debugger");
    auto NameAliasTy = cast<NameAliasType>(tybase);
    TypeAliasDecl *decl = NameAliasTy->getDecl();
    if (decl->getModuleContext() == decl->getASTContext().TheBuiltinModule)
      // It's not possible to mangle the context of the builtin module.
      return mangleType(decl->getUnderlyingType(), explosion, uncurryLevel);
    
    Buffer << "a";
    // For the DWARF output we want to mangle the type alias + context,
    // unless the type alias references a builtin type.
    ContextStack context(*this);
    while (DeclCtx && !DeclCtx->isInnermostContextGeneric())
      DeclCtx = DeclCtx->getParent();
    mangleContextOf(decl, BindGenerics::None);
    mangleIdentifier(decl->getName());
    return;
  }

  case TypeKind::Paren:
    return mangleSugaredType<ParenType>(type);
  case TypeKind::AssociatedType:
    return mangleSugaredType<AssociatedTypeType>(type);
  case TypeKind::Substituted:
    return mangleSugaredType<SubstitutedType>(type);
  case TypeKind::ArraySlice: /* fallthrough */
  case TypeKind::Optional:
    return mangleSugaredType<SyntaxSugarType>(type);
  case TypeKind::Dictionary:
    return mangleSugaredType<DictionaryType>(type);

  case TypeKind::ImplicitlyUnwrappedOptional: {
    assert(DWARFMangling && "sugared types are only legal for the debugger");
    auto *IUO = cast<ImplicitlyUnwrappedOptionalType>(tybase);
    auto implDecl = tybase->getASTContext().getImplicitlyUnwrappedOptionalDecl();
    auto GenTy = BoundGenericType::get(implDecl, Type(), IUO->getBaseType());
    return mangleType(GenTy, ResilienceExpansion::Minimal, 0);
  }

  case TypeKind::ExistentialMetatype: {
    ExistentialMetatypeType *EMT = cast<ExistentialMetatypeType>(tybase);
    if (EMT->hasRepresentation()) {
      Buffer << 'X' << 'P' << 'M';
      mangleMetatypeRepresentation(Buffer, EMT->getRepresentation());
    } else {
      Buffer << 'P' << 'M';
    }
    return mangleType(EMT->getInstanceType(),
                      ResilienceExpansion::Minimal, 0);
  }
  case TypeKind::Metatype: {
    MetatypeType *MT = cast<MetatypeType>(tybase);
    if (MT->hasRepresentation()) {
      Buffer << 'X' << 'M';
      mangleMetatypeRepresentation(Buffer, MT->getRepresentation());
    } else {
      Buffer << 'M';
    }
    return mangleType(MT->getInstanceType(),
                      ResilienceExpansion::Minimal, 0);
  }
  case TypeKind::LValue:
    llvm_unreachable("@lvalue types should not occur in function interfaces");
  case TypeKind::InOut:
    Buffer << 'R';
    return mangleType(cast<InOutType>(tybase)->getObjectType(),
                      ResilienceExpansion::Minimal, 0);

  case TypeKind::UnmanagedStorage:
    Buffer << "Xu";
    return mangleType(cast<UnmanagedStorageType>(tybase)->getReferentType(),
                      ResilienceExpansion::Minimal, 0);

  case TypeKind::UnownedStorage:
    Buffer << "Xo";
    return mangleType(cast<UnownedStorageType>(tybase)->getReferentType(),
                      ResilienceExpansion::Minimal, 0);

  case TypeKind::WeakStorage:
    Buffer << "Xw";
    return mangleType(cast<WeakStorageType>(tybase)->getReferentType(),
                      ResilienceExpansion::Minimal, 0);

  case TypeKind::Tuple: {
    auto tuple = cast<TupleType>(tybase);
    // type ::= 'T' tuple-field+ '_'  // tuple
    // type ::= 't' tuple-field+ '_'  // variadic tuple
    // tuple-field ::= identifier? type
    if (tuple->getNumElements() > 0
        && tuple->getElements().back().isVararg())
      Buffer << 't';
    else
      Buffer << 'T';
    for (auto &field : tuple->getElements()) {
      if (field.hasName())
        mangleIdentifier(field.getName());
      mangleType(field.getType(), explosion, 0);
    }
    Buffer << '_';
    return;
  }

  case TypeKind::Protocol:
    // Protocol type manglings have a variable number of protocol names
    // follow the 'P' sigil, so a trailing underscore is needed after the
    // type name, unlike protocols as contexts.
    Buffer << 'P';
    mangleProtocolList(type);
    Buffer << '_';
    return;

  case TypeKind::UnboundGeneric: {
    // We normally reject unbound types in IR-generation, but there
    // are several occasions in which we'd like to mangle them in the
    // abstract.
    ContextStack context(*this);
    mangleNominalType(cast<UnboundGenericType>(tybase)->getDecl(), explosion,
                      BindGenerics::None);
    return;
  }

  case TypeKind::Class:
  case TypeKind::Enum:
  case TypeKind::Struct: {
    ContextStack context(*this);
    return mangleNominalType(cast<NominalType>(tybase)->getDecl(), explosion,
                             BindGenerics::None);
  }

  case TypeKind::BoundGenericClass:
  case TypeKind::BoundGenericEnum:
  case TypeKind::BoundGenericStruct: {
    // type ::= 'G' <type> <type>+ '_'
    auto *boundType = cast<BoundGenericType>(tybase);
    Buffer << 'G';
    {
      ContextStack context(*this);
      mangleNominalType(boundType->getDecl(), explosion, BindGenerics::None);
    }
    for (auto arg : boundType->getGenericArgs()) {
      mangleType(arg, ResilienceExpansion::Minimal, /*uncurry*/ 0);
    }
    Buffer << '_';
    return;
  }

  case TypeKind::PolymorphicFunction:
    llvm_unreachable("should not be mangled");

  case TypeKind::SILFunction: {
    // <type> ::= 'XF' <impl-function-type>
    // <impl-function-type> ::= <impl-callee-convention>
    //                          <impl-function-attribute>* <generics>? '_'
    //                          <impl-parameter>* '_' <impl-result>* '_'
    // <impl-callee-convention> ::= 't'               // thin
    // <impl-callee-convention> ::= <impl-convention> // thick
    // <impl-convention> ::= 'a'                      // direct, autoreleased
    // <impl-convention> ::= 'd'                      // direct, no ownership transfer
    // <impl-convention> ::= 'g'                      // direct, guaranteed
    // <impl-convention> ::= 'e'                      // direct, deallocating
    // <impl-convention> ::= 'i'                      // indirect, ownership transfer
    // <impl-convention> ::= 'l'                      // indirect, inout
    // <impl-convention> ::= 'g'                      // direct, guaranteed
    // <impl-convention> ::= 'G'                      // indirect, guaranteed
    // <impl-convention> ::= 'z' <impl-convention>    // error result
    // <impl-function-attribute> ::= 'Cb'             // block invocation function
    // <impl-function-attribute> ::= 'Cc'             // C global function
    // <impl-function-attribute> ::= 'Cm'             // Swift method
    // <impl-function-attribute> ::= 'CO'             // ObjC method
    // <impl-function-attribute> ::= 'N'              // noreturn
    // <impl-function-attribute> ::= 'G'              // generic
    // <impl-parameter> ::= <impl-convention> <type>
    // <impl-result> ::= <impl-convention> <type>
    auto fn = cast<SILFunctionType>(tybase);
    Buffer << "XF";

    auto mangleParameterConvention = [](ParameterConvention conv) {
      // @in and @out are mangled the same because they're put in
      // different places.
      switch (conv) {
      case ParameterConvention::Indirect_In: return 'i';
      case ParameterConvention::Indirect_Out: return 'i';
      case ParameterConvention::Indirect_Inout: return 'l';
      case ParameterConvention::Indirect_In_Guaranteed: return 'G';
      case ParameterConvention::Direct_Owned: return 'o';
      case ParameterConvention::Direct_Unowned: return 'd';
      case ParameterConvention::Direct_Guaranteed: return 'g';
      case ParameterConvention::Direct_Deallocating: return 'e';
      }
      llvm_unreachable("bad parameter convention");
    };
    auto mangleResultConvention = [](ResultConvention conv) {
      switch (conv) {
      case ResultConvention::Owned: return 'o';
      case ResultConvention::Unowned: return 'd';
      case ResultConvention::UnownedInnerPointer: return 'D';
      case ResultConvention::Autoreleased: return 'a';
      }
      llvm_unreachable("bad result convention");
    };

    // <impl-callee-convention>
    if (!fn->getExtInfo().hasContext()) {
      Buffer << 't';
    } else {
      Buffer << mangleParameterConvention(fn->getCalleeConvention());
    }

    // <impl-function-attribute>*
    switch (fn->getRepresentation()) {
    case SILFunctionTypeRepresentation::Block:
      Buffer << "Cb";
      break;
    case SILFunctionTypeRepresentation::Thick:
    case SILFunctionTypeRepresentation::Thin:
      break;
    case SILFunctionTypeRepresentation::CFunctionPointer:
      Buffer << "Cc";
      break;
    case SILFunctionTypeRepresentation::ObjCMethod:
      Buffer << "CO";
      break;
    case SILFunctionTypeRepresentation::Method:
      Buffer << "Cm";
      break;
    case SILFunctionTypeRepresentation::WitnessMethod:
      Buffer << "Cw";
      break;
    }
    
    if (fn->isNoReturn()) Buffer << 'N';
    if (fn->isPolymorphic()) {
      Buffer << 'G';
      mangleGenericSignature(fn->getGenericSignature(), explosion);
    }
    Buffer << '_';

    auto mangleParameter = [&](SILParameterInfo param) {
      Buffer << mangleParameterConvention(param.getConvention());
      mangleType(param.getType(), ResilienceExpansion::Minimal, 0);
    };

    for (auto param : fn->getParametersWithoutIndirectResult()) {
      mangleParameter(param);
    }
    Buffer << '_';

    if (fn->hasIndirectResult()) {
      mangleParameter(fn->getIndirectResult());
    } else {
      auto result = fn->getResult();
      Buffer << mangleResultConvention(result.getConvention());
      mangleType(result.getType(), ResilienceExpansion::Minimal, 0);
    }
    
    if (fn->hasErrorResult()) {
      auto error = fn->getErrorResult();
      Buffer << 'z' << mangleResultConvention(error.getConvention());
      mangleType(error.getType(), ResilienceExpansion::Minimal, 0);
    }
    
    Buffer << '_';
    return;
  }

  // type ::= archetype
  case TypeKind::Archetype: {
    auto *archetype = cast<ArchetypeType>(tybase);
    
    // archetype ::= associated-type
    
    // associated-type ::= substitution
    if (tryMangleSubstitution(archetype))
      return;

    Buffer << 'Q';
    
    // associated-type ::= 'Q' archetype identifier
    // Mangle the associated type of a parent archetype.
    if (auto parent = archetype->getParent()) {
      assert(archetype->getAssocType()
             && "child archetype has no associated type?!");

      mangleType(parent, explosion, 0);
      mangleIdentifier(archetype->getName());
      addSubstitution(archetype);
      return;
    }
    
    // associated-type ::= 'Q' protocol-context
    // Mangle the Self archetype of a protocol.
    if (auto proto = archetype->getSelfProtocol()) {
      Buffer << 'P';
      mangleProtocolName(proto);
      addSubstitution(archetype);
      return;
    }
    
    // archetype ::= 'Q' <index>             # archetype with depth=0, index=N
    // archetype ::= 'Qd' <index> <index>    # archetype with depth=M+1, index=N
    // Mangle generic parameter archetypes.

    // Find the archetype information.
    const DeclContext *DC = DeclCtx;
    auto it = Archetypes.find(archetype);
    while (it == Archetypes.end()) {
      // This should be treated like an error, but we don't want
      // clients like lldb to crash because of corrupted input.
      assert(DC && "empty decl context");
      if (!DC) return;

      // This Archetype comes from an enclosing context -- proceed to
      // bind the generic params form all parent contexts.
      GenericParamList *GenericParams = nullptr;
      do { // Skip over empty parent contexts.
        DC = DC->getParent();
        assert(DC && "no decl context for archetype found");
        if (!DC) return;
        GenericParams = DC->getGenericParamsOfContext();
      } while (!GenericParams);

      bindGenericParameters(nullptr, GenericParams);
      it = Archetypes.find(archetype);
    }
    auto &info = it->second;
    assert(ArchetypesDepth >= info.Depth);
    unsigned relativeDepth = ArchetypesDepth - info.Depth;

    if (DWARFMangling) {
      Buffer << 'q' << Index(info.Index);
      // The DWARF output created by Swift is intentionally flat,
      // therefore archetypes are emitted with their DeclContext if
      // they appear at the top level of a type (_Tt).
      // Clone a new, non-DWARF Mangler for the DeclContext.
      Mangler ContextMangler(Buffer, /*DWARFMangling=*/false);
      SmallVector<const void *, 4> SortedSubsts(Substitutions.size());
      for (auto S : Substitutions) SortedSubsts[S.second] = S.first;
      for (auto S : SortedSubsts) ContextMangler.addSubstitution(S);
      for (; relativeDepth > 0; --relativeDepth)
        DC = DC->getParent();
      assert(DC && "no decl context for archetype found");
      if (!DC) return;
      ContextMangler.mangleContext(DC, BindGenerics::None);
    } else {
      if (relativeDepth != 0) {
        Buffer << 'd' << Index(relativeDepth - 1);
      }
      Buffer << Index(info.Index);
    }
    return;
  }

  case TypeKind::DynamicSelf: {
    auto dynamicSelf = cast<DynamicSelfType>(tybase);
    if (dynamicSelf->getSelfType()->getAnyNominal()) {
      Buffer << 'D';
      mangleType(dynamicSelf->getSelfType(), explosion, uncurryLevel);
    } else {
      // Mangle DynamicSelf as Self within a protocol.
      mangleType(dynamicSelf->getSelfType(), explosion, uncurryLevel);
    }
    return;
  }
    
  case TypeKind::GenericFunction: {
    auto genFunc = cast<GenericFunctionType>(tybase);
    Buffer << 'u';
    mangleGenericSignature(genFunc->getGenericSignature(), explosion);
    mangleFunctionType(genFunc, explosion, uncurryLevel);
    return;
  }

  case TypeKind::GenericTypeParam: {
    auto paramTy = cast<GenericTypeParamType>(tybase);
    // FIXME: Notion of depth is reversed from that for archetypes.

    // A special mangling for the very first generic parameter. This shows up
    // frequently because it corresponds to 'Self' in protocol requirement
    // generic signatures.
    if (paramTy->getDepth() == 0 && paramTy->getIndex() == 0) {
      Buffer << 'x';
      return;
    }

    Buffer << 'q';
    mangleGenericParamIndex(paramTy);
    return;
  }

  case TypeKind::DependentMember: {
    auto memTy = cast<DependentMemberType>(tybase);
    auto base = memTy->getBase()->getCanonicalType();

    // type ::= 'w' generic-param-index associated-type-name
    // 't_0_0.Member'
    if (auto gpBase = dyn_cast<GenericTypeParamType>(base)) {
      Buffer << 'w';
      mangleGenericParamIndex(gpBase);
      mangleAssociatedTypeName(memTy, /*canAbbreviate*/ true);
      return;
    }

    // type ::= 'W' generic-param-index associated-type-name+ '_'
    // 't_0_0.Member.Member...'
    SmallVector<DependentMemberType*, 2> path;
    path.push_back(memTy);
    while (auto dmBase = dyn_cast<DependentMemberType>(base)) {
      path.push_back(dmBase);
      base = dmBase.getBase();
    }
    if (auto gpRoot = dyn_cast<GenericTypeParamType>(base)) {
      Buffer << 'W';
      mangleGenericParamIndex(gpRoot);
      for (auto *member : reversed(path)) {
        mangleAssociatedTypeName(member, /*canAbbreviate*/ true);
      }
      Buffer << '_';
      return;
    }

    // type ::= 'q' type associated-type-name
    // Dependent members of non-generic-param types are not canonical, but
    // we may still want to mangle them for debugging or indexing purposes.
    Buffer << 'q';
    mangleType(memTy->getBase(), explosion, 0);
    mangleAssociatedTypeName(memTy, /*canAbbreviate*/false);
    return;
  }

  case TypeKind::Function:
    mangleFunctionType(cast<FunctionType>(tybase), explosion, uncurryLevel);
    return;

  case TypeKind::ProtocolComposition: {
    // We mangle ProtocolType and ProtocolCompositionType using the
    // same production:
    //   <type> ::= P <protocol-list> _

    auto protocols = cast<ProtocolCompositionType>(tybase)->getProtocols();
    Buffer << 'P';
    mangleProtocolList(protocols);
    Buffer << '_';
    return;
  }

  case TypeKind::SILBox:
  case TypeKind::SILBlockStorage:
    llvm_unreachable("should never be mangled");
  }
  llvm_unreachable("bad type kind");
}

/// Mangle a list of protocols.  Each protocol is a substitution
/// candidate.
///   <protocol-list> ::= <protocol-name>+
void Mangler::mangleProtocolList(ArrayRef<Type> protocols) {
  for (auto protoTy : protocols)
    if (auto composition = protoTy->getAs<ProtocolCompositionType>())
      mangleProtocolList(composition->getProtocols());
    else
      mangleProtocolName(protoTy->castTo<ProtocolType>()->getDecl());
}
void Mangler::mangleProtocolList(ArrayRef<ProtocolDecl*> protocols) {
  for (auto protocol : protocols)
    mangleProtocolName(protocol);
}

/// Mangle the name of a protocol as a substitution candidate.
void Mangler::mangleProtocolName(const ProtocolDecl *protocol) {
  //  <protocol-name> ::= <decl>      # substitutable
  // The <decl> in a protocol-name is the same substitution
  // candidate as a protocol <type>, but it is mangled without
  // the surrounding 'P'...'_'.
  ProtocolType *type = cast<ProtocolType>(protocol->getDeclaredType());
  if (tryMangleSubstitution(type))
    return;
  ContextStack context(*this);
  mangleContextOf(protocol, BindGenerics::None);
  mangleDeclName(protocol);
  addSubstitution(type);
}

static char getSpecifierForNominalType(const NominalTypeDecl *decl) {
  switch (decl->getKind()) {
#define NOMINAL_TYPE_DECL(id, parent)
#define DECL(id, parent) \
  case DeclKind::id:
#include "swift/AST/DeclNodes.def"
    llvm_unreachable("not a nominal type");

  case DeclKind::Protocol: return 'P';
  case DeclKind::Class: return 'C';
  case DeclKind::Enum: return 'O';
  case DeclKind::Struct: return 'V';
  }
  llvm_unreachable("bad decl kind");
}

void Mangler::mangleNominalType(const NominalTypeDecl *decl,
                                ResilienceExpansion explosion,
                                BindGenerics shouldBind,
                                CanGenericSignature extGenericSig,
                                const GenericParamList *extGenericParams) {
  auto bindGenericsIfDesired = [&] {
    if (shouldBind == BindGenerics::All) {
      const GenericParamList *generics =
          extGenericParams ? extGenericParams : decl->getGenericParams();
      auto sig = extGenericSig
                ? extGenericSig
                : getCanonicalSignatureOrNull(decl->getGenericSignature(),
                                              *decl->getParentModule());
      if (generics)
        bindGenericParameters(sig, generics);
    }
  };

  // Check for certain standard types.
  if (tryMangleStandardSubstitution(decl)) {
    bindGenericsIfDesired();
    return;
  }

  // For generic types, this uses the unbound type.
  TypeBase *key = decl->getDeclaredType().getPointer();

  // Try to mangle the entire name as a substitution.
  // type ::= substitution
  if (tryMangleSubstitution(key)) {
    bindGenericsIfDesired();
    return;
  }

  Buffer << getSpecifierForNominalType(decl);
  mangleContextOf(decl, shouldBind);
  bindGenericsIfDesired();
  mangleDeclName(decl);

  addSubstitution(key);
}

void Mangler::mangleProtocolDecl(const ProtocolDecl *protocol) {
  Buffer << 'P';
  ContextStack context(*this);
  mangleContextOf(protocol, BindGenerics::None);
  mangleDeclName(protocol);
  Buffer << '_';
}

bool Mangler::tryMangleStandardSubstitution(const NominalTypeDecl *decl) {
  // Bail out if our parent isn't the swift standard library.
  DeclContext *dc = decl->getDeclContext();
  if (!dc->isModuleScopeContext() || !dc->getParentModule()->isStdlibModule())
    return false;

  // Standard substitutions shouldn't start with 's' (because that's
  // reserved for the swift module itself) or a digit or '_'.

  StringRef name = decl->getName().str();
  if (name == "Int") {
    Buffer << "Si";
    return true;
  } else if (name == "UInt") {
    Buffer << "Su";
    return true;
  } else if (name == "Bool") {
    Buffer << "Sb";
    return true;
  } else if (name == "UnicodeScalar") {
    Buffer << "Sc";
    return true;
  } else if (name == "Double") {
    Buffer << "Sd";
    return true;
  } else if (name == "Float") {
    Buffer << "Sf";
    return true;
  } else if (name == "UnsafePointer") {
    Buffer << "SP";
    return true;
  } else if (name == "UnsafeMutablePointer") {
    Buffer << "Sp";
    return true;
  } else if (name == "Optional") {
    Buffer << "Sq";
    return true;
  } else if (name == "ImplicitlyUnwrappedOptional") {
    Buffer << "SQ";
    return true;
  } else if (name == "UnsafeBufferPointer") {
    Buffer << "SR";
    return true;
  } else if (name == "UnsafeMutableBufferPointer") {
    Buffer << "Sr";
    return true;
  } else if (name == "Array") {
    Buffer << "Sa";
    return true;
  } else if (name == "String") {
    Buffer << "SS";
    return true;
  } else {
    return false;
  }
}

void Mangler::mangleFunctionType(AnyFunctionType *fn,
                                 ResilienceExpansion explosion,
                                 unsigned uncurryLevel) {
  assert((DWARFMangling || fn->isCanonical()) &&
         "expecting canonical types when not mangling for the debugger");

  // type ::= 'F' type type (curried)
  // type ::= 'f' type type (uncurried)
  // type ::= 'b' type type (objc block)
  // type ::= 'c' type type (c function pointer)
  // type ::= 'Xf' type type (thin)
  // type ::= 'K' type type (auto closure)
  //
  // Note that we do not currently use thin representations in the AST
  // for the types of function decls.  This may need to change at some
  // point, in which case the uncurry logic can probably migrate to that
  // case.
  //
  // It would have been cleverer if we'd used 'f' for thin functions
  // and something else for uncurried functions, but oh well.
  //
  // Or maybe we can change the mangling at the same time we make
  // changes to better support thin functions.
  switch (fn->getRepresentation()) {
  case AnyFunctionType::Representation::Block:
    Buffer << 'b';
    break;
  case AnyFunctionType::Representation::Thin:
    Buffer << "Xf";
    break;
  case AnyFunctionType::Representation::Swift:
    if (fn->isAutoClosure())
      Buffer << 'K';
    else
      Buffer << (uncurryLevel > 0 ? 'f' : 'F');
    break;
  case AnyFunctionType::Representation::CFunctionPointer:
    Buffer << 'c';
    break;
  }
  
  if (fn->throws())
    Buffer << 'z';
  
  mangleType(fn->getInput(), explosion, 0);
  mangleType(fn->getResult(), explosion,
             (uncurryLevel > 0 ? uncurryLevel - 1 : 0));
}

void Mangler::mangleClosureComponents(Type Ty, unsigned discriminator,
                                      bool isImplicit,
                                      const DeclContext *parentContext,
                                      const DeclContext *localContext) {
  // entity-name ::= 'U' index type         // explicit anonymous closure
  // entity-name ::= 'u' index type         // implicit anonymous closure

  assert(discriminator != AbstractClosureExpr::InvalidDiscriminator
         && "closure must be marked correctly with discriminator");

  Buffer << 'F';
  mangleContext(parentContext, BindGenerics::All);
  Buffer << (isImplicit ? 'u' : 'U') << Index(discriminator);

  if (!Ty)
    Ty = ErrorType::get(localContext->getASTContext());

  if (!DeclCtx) DeclCtx = localContext;
  mangleType(Ty->getCanonicalType(), ResilienceExpansion::Minimal,
             /*uncurry*/ 0);
}

void Mangler::mangleClosureEntity(const SerializedAbstractClosureExpr *closure,
                                  ResilienceExpansion explosion,
                                  unsigned uncurryingLevel) {
  mangleClosureComponents(closure->getType(), closure->getDiscriminator(),
                          closure->isImplicit(), closure->getParent(),
                          closure->getLocalContext());
}

void Mangler::mangleClosureEntity(const AbstractClosureExpr *closure,
                                  ResilienceExpansion explosion,
                                  unsigned uncurryLevel) {
  mangleClosureComponents(closure->getType(), closure->getDiscriminator(),
                          isa<AutoClosureExpr>(closure), closure->getParent(),
                          closure->getLocalContext());
}

void Mangler::mangleConstructorEntity(const ConstructorDecl *ctor,
                                      bool isAllocating,
                                      ResilienceExpansion explosion,
                                      unsigned uncurryLevel) {
  Buffer << 'F';
  mangleContextOf(ctor, BindGenerics::Enclosing);
  Buffer << (isAllocating ? 'C' : 'c');
  mangleDeclType(ctor, explosion, uncurryLevel);
}

void Mangler::mangleDestructorEntity(const DestructorDecl *dtor,
                                     bool isDeallocating) {
  Buffer << 'F';
  mangleContextOf(dtor, BindGenerics::Enclosing);
  Buffer << (isDeallocating ? 'D' : 'd');
}

void Mangler::mangleIVarInitDestroyEntity(const ClassDecl *decl,
                                          bool isDestroyer) {
  Buffer << 'F';
  mangleContext(decl, BindGenerics::Enclosing);
  Buffer << (isDestroyer ? 'E' : 'e');
}

static StringRef getCodeForAccessorKind(AccessorKind kind,
                                        AddressorKind addressorKind) {
  switch (kind) {
  case AccessorKind::NotAccessor: llvm_unreachable("bad accessor kind!");
  case AccessorKind::IsGetter:    return "g";
  case AccessorKind::IsSetter:    return "s";
  case AccessorKind::IsWillSet:   return "w";
  case AccessorKind::IsDidSet:    return "W";
  case AccessorKind::IsAddressor:
    // 'l' is for location. 'A' was taken.
    switch (addressorKind) {
    case AddressorKind::NotAddressor: llvm_unreachable("bad combo");
    case AddressorKind::Unsafe: return "lu";
    case AddressorKind::Owning: return "lO";
    case AddressorKind::NativeOwning: return "lo";
    case AddressorKind::NativePinning: return "lp";
    }
    llvm_unreachable("bad addressor kind");
  case AccessorKind::IsMutableAddressor:
    switch (addressorKind) {
    case AddressorKind::NotAddressor: llvm_unreachable("bad combo");
    case AddressorKind::Unsafe: return "au";
    case AddressorKind::Owning: return "aO";
    case AddressorKind::NativeOwning: return "ao";
    case AddressorKind::NativePinning: return "ap";
    }
    llvm_unreachable("bad addressor kind");
  case AccessorKind::IsMaterializeForSet: return "m";
  }
  llvm_unreachable("bad accessor kind");
}

void Mangler::mangleAccessorEntity(AccessorKind kind,
                                   AddressorKind addressorKind,
                                   const AbstractStorageDecl *decl,
                                   ResilienceExpansion explosion) {
  assert(kind != AccessorKind::NotAccessor);
  Buffer << 'F';
  mangleContextOf(decl, BindGenerics::All);
  Buffer << getCodeForAccessorKind(kind, addressorKind);
  mangleDeclName(decl);
  mangleDeclType(decl, explosion, 0);
}

void Mangler::mangleAddressorEntity(const ValueDecl *decl) {
  Buffer << 'F';
  mangleContextOf(decl, BindGenerics::All);
  Buffer << "au";
  mangleDeclName(decl);
  mangleDeclType(decl, ResilienceExpansion::Minimal, 0);
}

void Mangler::mangleGlobalGetterEntity(ValueDecl *decl) {
  Buffer << 'F';
  mangleContextOf(decl, BindGenerics::All);
  Buffer << 'G';
  mangleDeclName(decl);
  mangleDeclType(decl, ResilienceExpansion::Minimal, 0);
}

void Mangler::mangleDefaultArgumentEntity(const DeclContext *func,
                                          unsigned index) {
  Buffer << 'I';
  mangleContext(func, BindGenerics::All);
  Buffer << 'A' << Index(index);
}

void Mangler::mangleInitializerEntity(const VarDecl *var) {
  // The initializer is its own entity whose context is the variable.
  Buffer << 'I';
  mangleEntity(var, ResilienceExpansion::Minimal, /*uncurry*/ 0);
  Buffer << 'i';
}

void Mangler::mangleEntity(const ValueDecl *decl,
                           ResilienceExpansion explosion,
                           unsigned uncurryLevel) {
  assert(!isa<ConstructorDecl>(decl));
  assert(!isa<DestructorDecl>(decl));
  
  // entity ::= static? entity-kind context entity-name
  if (decl->isStatic())
    Buffer << 'Z';

  // Handle accessors specially, they are mangled as modifiers on the accessed
  // declaration.
  if (auto func = dyn_cast<FuncDecl>(decl)) {
    auto accessorKind = func->getAccessorKind();
    if (accessorKind != AccessorKind::NotAccessor)
      return mangleAccessorEntity(accessorKind, func->getAddressorKind(),
                                  func->getAccessorStorageDecl(),
                                  explosion);
  }
  
  BindGenerics shouldBindParent = BindGenerics::All;

  if (isa<VarDecl>(decl)) {
    Buffer << 'v';
  } else if (isa<SubscriptDecl>(decl)) {
    Buffer << 'i';
  } else if (isa<GenericTypeParamDecl>(decl)) {
    shouldBindParent = BindGenerics::None;
    Buffer << 't';
  } else {
    assert(isa<AbstractFunctionDecl>(decl) ||
           isa<EnumElementDecl>(decl));
    Buffer << 'F';

    // If this is a method, then its formal type includes the
    // archetypes of its parent.
    if (decl->getDeclContext()->isTypeContext())
      shouldBindParent = BindGenerics::Enclosing;
  }

  if (!DeclCtx) DeclCtx = decl->getDeclContext();
  mangleContextOf(decl, shouldBindParent);
  mangleDeclName(decl);
  mangleDeclType(decl, explosion, uncurryLevel);
}

void Mangler::mangleDirectness(bool isIndirect) {
  Buffer << (isIndirect ? 'i': 'd');
}

void Mangler::mangleProtocolConformance(const ProtocolConformance *conformance){
  // protocol-conformance ::= ('u' generic-parameters '_')?
  //                          type protocol module
  // FIXME: explosion level?
  
  ContextStack context(*this);
  // If the conformance is generic, mangle its generic parameters.
  Mod = conformance->getDeclContext()->getParentModule();
  if (auto sig = conformance->getGenericSignature()) {
    Buffer << 'u';
    mangleGenericSignature(sig, ResilienceExpansion::Minimal);
  }
  mangleType(conformance->getInterfaceType()->getCanonicalType(),
             ResilienceExpansion::Minimal, 0);
  mangleProtocolName(conformance->getProtocol());
  mangleModule(conformance->getDeclContext()->getParentModule());
}

void Mangler::mangleFieldOffsetFull(const ValueDecl *decl, bool isIndirect) {
  Buffer << "_TWv";
  mangleDirectness(isIndirect);
  mangleEntity(decl, ResilienceExpansion::Minimal, 0);
}

void Mangler::mangleTypeFullMetadataFull(CanType ty) {
  Buffer << "_TMf";
  mangleType(ty, ResilienceExpansion::Minimal, 0);
}

void Mangler::mangleTypeMetadataFull(CanType ty, bool isPattern) {
  Buffer << "_TM";
  if (isPattern)
    Buffer << 'P';
  mangleType(ty, ResilienceExpansion::Minimal, 0);
}

void Mangler::mangleGlobalVariableFull(const VarDecl *decl) {
  // As a special case, Clang functions and globals don't get mangled at all.
  // FIXME: When we can import C++, use Clang's mangler.
  if (auto clangDecl =
        dyn_cast_or_null<clang::DeclaratorDecl>(decl->getClangDecl())) {
    if (auto asmLabel = clangDecl->getAttr<clang::AsmLabelAttr>()) {
      Buffer << '\01' << asmLabel->getLabel();
    } else {
      Buffer << clangDecl->getName();
    }
    return;
  }

  Buffer << "_T";
  mangleEntity(decl, ResilienceExpansion(0), 0);
}

void Mangler::mangleGlobalInit(const VarDecl *decl, int counter,
                               bool isInitFunc) {
  auto topLevelContext = decl->getDeclContext()->getModuleScopeContext();
  auto fileUnit = cast<FileUnit>(topLevelContext);
  Identifier discriminator = fileUnit->getDiscriminatorForPrivateValue(decl);
  assert(!discriminator.empty());
  assert(!isNonAscii(discriminator.str()) &&
         "discriminator contains non-ASCII characters");
  assert(!clang::isDigit(discriminator.str().front()) &&
         "not a valid identifier");
  
  Buffer << "globalinit_";
  mangleIdentifier(discriminator);
  Buffer << (isInitFunc ? "_func" : "_token");
  Buffer << counter;
}
