//===--- MiscDiagnostics.cpp - AST-Level Diagnostics ----------------------===//
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
// This file implements AST-level diagnostics.
//
//===----------------------------------------------------------------------===//

#include "MiscDiagnostics.h"
#include "TypeChecker.h"
#include "swift/AST/ASTWalker.h"
#include "swift/AST/NameLookup.h"
#include "swift/AST/Pattern.h"
#include "swift/Basic/SourceManager.h"
#include "swift/Basic/StringExtras.h"
#include "swift/Parse/Lexer.h"
#include "llvm/ADT/MapVector.h"
using namespace swift;

//===--------------------------------------------------------------------===//
// Diagnose assigning variable to itself.
//===--------------------------------------------------------------------===//

static Decl *findSimpleReferencedDecl(const Expr *E) {
  if (auto *LE = dyn_cast<LoadExpr>(E))
    E = LE->getSubExpr();

  if (auto *DRE = dyn_cast<DeclRefExpr>(E))
    return DRE->getDecl();

  return nullptr;
}

static std::pair<Decl *, Decl *> findReferencedDecl(const Expr *E) {
  if (auto *LE = dyn_cast<LoadExpr>(E))
    E = LE->getSubExpr();

  if (auto *D = findSimpleReferencedDecl(E))
    return std::make_pair(nullptr, D);

  if (auto *MRE = dyn_cast<MemberRefExpr>(E)) {
    if (auto *BaseDecl = findSimpleReferencedDecl(MRE->getBase()))
      return std::make_pair(BaseDecl, MRE->getMember().getDecl());
  }

  return std::make_pair(nullptr, nullptr);
}

/// Diagnose assigning variable to itself.
static void diagSelfAssignment(TypeChecker &TC, const Expr *E) {
  auto *AE = dyn_cast<AssignExpr>(E);
  if (!AE)
    return;

  auto LHSDecl = findReferencedDecl(AE->getDest());
  auto RHSDecl = findReferencedDecl(AE->getSrc());
  if (LHSDecl.second && LHSDecl == RHSDecl) {
    TC.diagnose(AE->getLoc(), LHSDecl.first ? diag::self_assignment_prop
                                            : diag::self_assignment_var)
        .highlight(AE->getDest()->getSourceRange())
        .highlight(AE->getSrc()->getSourceRange());
  }
}


/// Diagnose syntactic restrictions of expressions.
///
///   - Module values may only occur as part of qualification.
///   - Metatype names cannot generally be used as values: they need a "T.self"
///     qualification unless used in narrow case (e.g. T() for construction).
///   - '_' may only exist on the LHS of an assignment expression.
///   - warn_unqualified_access values must not be accessed except via qualified
///     lookup.
///   - Partial application of some decls isn't allowed due to implementation
///     limitations.
///   - "&" (aka InOutExpressions) may only exist directly in function call
///     argument lists.
///   - 'self.init' and 'super.init' cannot be wrapped in a larger expression
///     or statement.
///
static void diagSyntacticUseRestrictions(TypeChecker &TC, const Expr *E,
                                         const DeclContext *DC,
                                         bool isExprStmt) {
  class DiagnoseWalker : public ASTWalker {
    SmallPtrSet<Expr*, 4> AlreadyDiagnosedMetatypes;
    SmallPtrSet<DeclRefExpr*, 4> AlreadyDiagnosedNoEscapes;
    
    // Keep track of acceptable DiscardAssignmentExpr's.
    SmallPtrSet<DiscardAssignmentExpr*, 2> CorrectDiscardAssignmentExprs;

    /// Keep track of InOutExprs
    SmallPtrSet<InOutExpr*, 2> AcceptableInOutExprs;

    bool IsExprStmt;

  public:
    TypeChecker &TC;
    const DeclContext *DC;

    DiagnoseWalker(TypeChecker &TC, const DeclContext *DC, bool isExprStmt)
      : IsExprStmt(isExprStmt), TC(TC), DC(DC) {}

    // Selector for the partial_application_of_function_invalid diagnostic
    // message.
    struct PartialApplication {
      unsigned level : 29;
      enum : unsigned {
        Function,
        MutatingMethod,
        ObjCProtocolMethod,
        SuperInit,
        SelfInit,
      };
      unsigned kind : 3;
    };

    // Partial applications of functions that are not permitted.  This is
    // tracked in post-order and unravelled as subsequent applications complete
    // the call (or not).
    llvm::SmallDenseMap<Expr*, PartialApplication,2> InvalidPartialApplications;

    ~DiagnoseWalker() {
      for (auto &unapplied : InvalidPartialApplications) {
        unsigned kind = unapplied.second.kind;
        TC.diagnose(unapplied.first->getLoc(),
                    diag::partial_application_of_function_invalid,
                    kind);
      }
    }

    /// If this is an application of a function that cannot be partially
    /// applied, arrange for us to check that it gets fully applied.
    void recordUnsupportedPartialApply(DeclRefExpr *expr) {
      bool requiresFullApply = false;
      unsigned kind;

      auto fn = dyn_cast<FuncDecl>(expr->getDecl());
      if (!fn)
        return;

      // @objc protocol methods cannot be partially applied.
      if (auto proto = fn->getDeclContext()->isProtocolOrProtocolExtensionContext()) {
        if (proto->isObjC()) {
          requiresFullApply = true;
          kind = PartialApplication::ObjCProtocolMethod;
        }
      }

      if (requiresFullApply) {
        // We need to apply all argument clauses.
        InvalidPartialApplications.insert({
          expr, {fn->getNaturalArgumentCount(), kind}
        });
      }
    }

    /// If this is an application of a function that cannot be partially
    /// applied, arrange for us to check that it gets fully applied.
    void recordUnsupportedPartialApply(ApplyExpr *expr, Expr *fnExpr) {

      if (isa<OtherConstructorDeclRefExpr>(fnExpr)) {
        auto kind = expr->getArg()->isSuperExpr()
                  ? PartialApplication::SuperInit
                  : PartialApplication::SelfInit;

        // Partial applications of delegated initializers aren't allowed, and
        // don't really make sense to begin with.
        InvalidPartialApplications.insert({ expr, {1, kind} });
        return;
      }

      auto fnDeclRef = dyn_cast<DeclRefExpr>(fnExpr);
      if (!fnDeclRef)
        return;

      recordUnsupportedPartialApply(fnDeclRef);

      auto fn = dyn_cast<FuncDecl>(fnDeclRef->getDecl());
      if (!fn)
        return;

      unsigned kind =
        fn->isInstanceMember() ? PartialApplication::MutatingMethod
                               : PartialApplication::Function;

      // Functions with inout parameters cannot be partially applied.
      if (expr->getArg()->getType()->hasInOut()) {
        // We need to apply all argument clauses.
        InvalidPartialApplications.insert({
          fnExpr, {fn->getNaturalArgumentCount(), kind}
        });
      }
    }

    /// This method is called in post-order over the AST to validate that
    /// methods are fully applied when they can't support partial application.
    void checkInvalidPartialApplication(Expr *E) {
      if (auto DRE = dyn_cast<DeclRefExpr>(E)) {
        recordUnsupportedPartialApply(DRE);
        return;
      }

      if (auto AE = dyn_cast<ApplyExpr>(E)) {
        Expr *fnExpr = AE->getFn()->getSemanticsProvidingExpr();
        if (auto forceExpr = dyn_cast<ForceValueExpr>(fnExpr))
          fnExpr = forceExpr->getSubExpr()->getSemanticsProvidingExpr();
        if (auto dotSyntaxExpr = dyn_cast<DotSyntaxBaseIgnoredExpr>(fnExpr))
          fnExpr = dotSyntaxExpr->getRHS();

        // Check to see if this is a potentially unsupported partial
        // application.
        recordUnsupportedPartialApply(AE, fnExpr);

        // If this is adding a level to an active partial application, advance
        // it to the next level.
        auto foundApplication = InvalidPartialApplications.find(fnExpr);
        if (foundApplication == InvalidPartialApplications.end())
          return;

        unsigned level = foundApplication->second.level;
        auto kind = foundApplication->second.kind;
        assert(level > 0);
        InvalidPartialApplications.erase(foundApplication);
        if (level > 1) {
          // We have remaining argument clauses.
          InvalidPartialApplications.insert({ AE, {level - 1, kind} });
        }
        return;
      }

    }

    // Not interested in going outside a basic expression.
    std::pair<bool, Stmt *> walkToStmtPre(Stmt *S) override {
      return { false, S };
    }
    std::pair<bool, Pattern*> walkToPatternPre(Pattern *P) override {
      return { false, P };
    }
    bool walkToDeclPre(Decl *D) override { return false; }
    bool walkToTypeReprPre(TypeRepr *T) override { return true; }

    std::pair<bool, Expr *> walkToExprPre(Expr *E) override {
      // See through implicit conversions of the expression.  We want to be able
      // to associate the parent of this expression with the ultimate callee.
      auto Base = E;
      while (auto Conv = dyn_cast<ImplicitConversionExpr>(Base))
        Base = Conv->getSubExpr();

      if (auto *DRE = dyn_cast<DeclRefExpr>(Base)) {
        // Verify metatype uses.
        if (isa<TypeDecl>(DRE->getDecl())) {
          if (isa<ModuleDecl>(DRE->getDecl()))
            checkUseOfModule(DRE);
          else
            checkUseOfMetaTypeName(Base);
        }

        // Verify noescape parameter uses.
        checkNoEscapeParameterUse(DRE, nullptr);

        // Verify warn_unqualified_access uses.
        checkUnqualifiedAccessUse(DRE);
      }
      if (auto *MRE = dyn_cast<MemberRefExpr>(Base)) {
        if (isa<TypeDecl>(MRE->getMember().getDecl()))
          checkUseOfMetaTypeName(Base);

        // Check whether there are needless words that could be omitted.
        TC.checkOmitNeedlessWords(MRE);
      }
      if (isa<TypeExpr>(Base))
        checkUseOfMetaTypeName(Base);

      if (auto *SE = dyn_cast<SubscriptExpr>(E)) {
        // Implicit InOutExpr's are allowed in the base of a subscript expr.
        if (auto *IOE = dyn_cast<InOutExpr>(SE->getBase()))
          if (IOE->isImplicit())
            AcceptableInOutExprs.insert(IOE);
      }


      // Check function calls, looking through implicit conversions on the
      // function and inspecting the arguments directly.
      if (auto *Call = dyn_cast<ApplyExpr>(E)) {
        // Check the callee, looking through implicit conversions.
        auto Base = Call->getFn();
        while (auto Conv = dyn_cast<ImplicitConversionExpr>(Base))
          Base = Conv->getSubExpr();
        if (auto *DRE = dyn_cast<DeclRefExpr>(Base))
          checkNoEscapeParameterUse(DRE, Call);

        auto *Arg = Call->getArg();

        // The argument could be shuffled if it includes default arguments,
        // label differences, or other exciting things like that.
        if (auto *TSE = dyn_cast<TupleShuffleExpr>(Arg))
          Arg = TSE->getSubExpr();

        // The argument is either a ParenExpr or TupleExpr.
        ArrayRef<Expr*> arguments;
        if (auto *TE = dyn_cast<TupleExpr>(Arg))
          arguments = TE->getElements();
        else if (auto *PE = dyn_cast<ParenExpr>(Arg))
          arguments = PE->getSubExpr();
        else
          arguments = Call->getArg();

        // Check each argument.
        for (auto arg : arguments) {
          // InOutExpr's are allowed in argument lists directly.
          if (auto *IOE = dyn_cast<InOutExpr>(arg)) {
            if (isa<CallExpr>(Call))
              AcceptableInOutExprs.insert(IOE);
          }
          // InOutExprs can be wrapped in some implicit casts.
          if (auto *ICO = dyn_cast<ImplicitConversionExpr>(arg)) {
            if (isa<InOutToPointerExpr>(ICO) ||
                isa<ArrayToPointerExpr>(ICO) ||
                isa<ErasureExpr>(ICO))
              if (auto *IOE = dyn_cast<InOutExpr>(ICO->getSubExpr()))
                AcceptableInOutExprs.insert(IOE);
          }

          while (1) {
            if (auto conv = dyn_cast<ImplicitConversionExpr>(arg))
              arg = conv->getSubExpr();
            else if (auto *PE = dyn_cast<ParenExpr>(arg))
              arg = PE->getSubExpr();
            else
              break;
          }

          if (auto *DRE = dyn_cast<DeclRefExpr>(arg))
            checkNoEscapeParameterUse(DRE, Call);
        }

        // Check whether there are needless words that could be omitted.
        TC.checkOmitNeedlessWords(Call);
      }
      
      // If we have an assignment expression, scout ahead for acceptable _'s.
      if (auto *AE = dyn_cast<AssignExpr>(E))
        markAcceptableDiscardExprs(AE->getDest());

      /// Diagnose a '_' that isn't on the immediate LHS of an assignment.
      if (auto *DAE = dyn_cast<DiscardAssignmentExpr>(E)) {
        if (!CorrectDiscardAssignmentExprs.count(DAE) &&
            !DAE->getType()->is<ErrorType>())
          TC.diagnose(DAE->getLoc(), diag::discard_expr_outside_of_assignment);
      }

      // Diagnose an '&' that isn't in an argument lists.
      if (auto *IOE = dyn_cast<InOutExpr>(E)) {
        if (!IOE->isImplicit() && !AcceptableInOutExprs.count(IOE) &&
            !IOE->getType()->is<ErrorType>())
          TC.diagnose(IOE->getLoc(), diag::inout_expr_outside_of_call)
            .highlight(IOE->getSubExpr()->getSourceRange());
      }

      // Diagnose 'self.init' or 'super.init' nested in another expression.
      if (auto *rebindSelfExpr = dyn_cast<RebindSelfInConstructorExpr>(E)) {
        if (!Parent.isNull() || !IsExprStmt) {
          bool isChainToSuper;
          (void)rebindSelfExpr->getCalledConstructor(isChainToSuper);
          TC.diagnose(E->getLoc(), diag::init_delegation_nested,
                      isChainToSuper, !IsExprStmt);
        }
      }

      return { true, E };
    }

    Expr *walkToExprPost(Expr *E) override {
      checkInvalidPartialApplication(E);
      return E;
    }

    /// Scout out the specified destination of an AssignExpr to recursively
    /// identify DiscardAssignmentExpr in legal places.  We can only allow them
    /// in simple pattern-like expressions, so we reject anything complex here.
    void markAcceptableDiscardExprs(Expr *E) {
      if (!E) return;
      
      if (auto *PE = dyn_cast<ParenExpr>(E))
        return markAcceptableDiscardExprs(PE->getSubExpr());
      if (auto *TE = dyn_cast<TupleExpr>(E)) {
        for (auto &elt : TE->getElements())
          markAcceptableDiscardExprs(elt);
        return;
      }
      if (auto *DAE = dyn_cast<DiscardAssignmentExpr>(E))
        CorrectDiscardAssignmentExprs.insert(DAE);

      // Otherwise, we can't support this.
    }
    

    void checkUseOfModule(DeclRefExpr *E) {
      // Allow module values as a part of:
      // - ignored base expressions;
      // - expressions that failed to type check.
      if (auto *ParentExpr = Parent.getAsExpr()) {
        if (isa<DotSyntaxBaseIgnoredExpr>(ParentExpr) ||
            isa<UnresolvedDotExpr>(ParentExpr))
          return;
      }

      TC.diagnose(E->getStartLoc(), diag::value_of_module_type);
    }

    /// The DRE argument is a reference to a noescape parameter.  Verify that
    /// its uses are ok.
    void checkNoEscapeParameterUse(DeclRefExpr *DRE, Expr *ParentExpr=nullptr) {
      // This only cares about declarations marked noescape.
      if (!DRE->getDecl()->getAttrs().hasAttribute<NoEscapeAttr>())
        return;

      // Only diagnose this once.  If we check and accept this use higher up in
      // the AST, don't recheck here.
      if (!AlreadyDiagnosedNoEscapes.insert(DRE).second)
        return;

      // The only valid use of the noescape parameter is an immediate call,
      // either as the callee or as an argument (in which case, the typechecker
      // validates that the noescape bit didn't get stripped off).
      if (ParentExpr && isa<ApplyExpr>(ParentExpr)) // param()
        return;

      TC.diagnose(DRE->getStartLoc(), diag::invalid_noescape_use,
                  DRE->getDecl()->getName());
      if (DRE->getDecl()->getAttrs().hasAttribute<AutoClosureAttr>() &&
          DRE->getDecl()->getAttrs().getAttribute<NoEscapeAttr>()->isImplicit())
        TC.diagnose(DRE->getDecl()->getLoc(), diag::noescape_autoclosure,
                    DRE->getDecl()->getName());
    }

    // Diagnose metatype values that don't appear as part of a property,
    // method, or constructor reference.
    void checkUseOfMetaTypeName(Expr *E) {
      // If we've already checked this at a higher level, we're done.
      if (!AlreadyDiagnosedMetatypes.insert(E).second)
        return;

      // Allow references to types as a part of:
      // - member references T.foo, T.Type, T.self, etc. (but *not* T.type)
      // - constructor calls T()
      if (auto *ParentExpr = Parent.getAsExpr()) {
        // Reject use of "T.dynamicType", it should be written as "T.self".
        if (auto metaExpr = dyn_cast<DynamicTypeExpr>(ParentExpr)) {
          // Add a fixit to replace '.dynamicType' with '.self'.
          TC.diagnose(E->getStartLoc(), diag::type_of_metatype)
            .fixItReplace(metaExpr->getMetatypeLoc(), "self");
          return;
        }

        // This is the white-list of accepted syntactic forms.
        if (isa<ErrorExpr>(ParentExpr) ||
            isa<DotSelfExpr>(ParentExpr) ||               // T.self
            isa<CallExpr>(ParentExpr) ||                  // T()
            isa<MemberRefExpr>(ParentExpr) ||             // T.foo
            isa<UnresolvedMemberExpr>(ParentExpr) ||
            isa<SelfApplyExpr>(ParentExpr) ||             // T.foo()  T()
            isa<UnresolvedDotExpr>(ParentExpr) ||
            isa<DotSyntaxBaseIgnoredExpr>(ParentExpr) ||
            isa<UnresolvedConstructorExpr>(ParentExpr) ||
            isa<UnresolvedSelectorExpr>(ParentExpr) ||
            isa<UnresolvedSpecializeExpr>(ParentExpr) ||
            isa<OpenExistentialExpr>(ParentExpr)) {
          return;
        }
      }

      // Is this a protocol metatype?

      TC.diagnose(E->getStartLoc(), diag::value_of_metatype_type);

      // Add fix-t to insert '()', unless this is a protocol metatype.
      bool isProtocolMetatype = false;
      if (auto metaTy = E->getType()->getAs<MetatypeType>())
        isProtocolMetatype = metaTy->getInstanceType()->is<ProtocolType>();
      if (!isProtocolMetatype) {
        TC.diagnose(E->getEndLoc(), diag::add_parens_to_type)
          .fixItInsertAfter(E->getEndLoc(), "()");
      }

      // Add fix-it to insert ".self".
      TC.diagnose(E->getEndLoc(), diag::add_self_to_type)
        .fixItInsertAfter(E->getEndLoc(), ".self");
    }

    void checkUnqualifiedAccessUse(const DeclRefExpr *DRE) {
      const Decl *D = DRE->getDecl();
      if (!D->getAttrs().hasAttribute<WarnUnqualifiedAccessAttr>())
        return;

      if (auto *parentExpr = Parent.getAsExpr()) {
        if (auto *ignoredBase = dyn_cast<DotSyntaxBaseIgnoredExpr>(parentExpr)){
          if (!ignoredBase->isImplicit())
            return;
        }
        if (auto *calledBase = dyn_cast<DotSyntaxCallExpr>(parentExpr)) {
          if (!calledBase->isImplicit())
            return;
        }
      }

      const auto *VD = cast<ValueDecl>(D);
      const TypeDecl *declParent =
          VD->getDeclContext()->isNominalTypeOrNominalTypeExtensionContext();
      if (!declParent) {
        assert(VD->getDeclContext()->isModuleScopeContext());
        declParent = VD->getDeclContext()->getParentModule();
      }

      TC.diagnose(DRE->getLoc(), diag::warn_unqualified_access,
                  VD->getName(), VD->getDescriptiveKind(),
                  declParent->getDescriptiveKind(), declParent->getFullName());
      TC.diagnose(VD, diag::decl_declared_here, VD->getName());

      if (VD->getDeclContext()->isTypeContext()) {
        TC.diagnose(DRE->getLoc(), diag::fix_unqualified_access_member)
          .fixItInsert(DRE->getStartLoc(), "self.");
      }

      DeclContext *topLevelContext = DC->getModuleScopeContext();
      UnqualifiedLookup lookup(VD->getBaseName(), topLevelContext, &TC,
                               /*knownPrivate*/true);

      // Group results by module. Pick an arbitrary result from each module.
      llvm::SmallDenseMap<const ModuleDecl*,const ValueDecl*,4> resultsByModule;
      for (auto &result : lookup.Results) {
        const ValueDecl *value = result.getValueDecl();
        resultsByModule.insert(std::make_pair(value->getModuleContext(),value));
      }

      // Sort by module name.
      using ModuleValuePair = std::pair<const ModuleDecl *, const ValueDecl *>;
      SmallVector<ModuleValuePair, 4> sortedResults{
        resultsByModule.begin(), resultsByModule.end()
      };
      llvm::array_pod_sort(sortedResults.begin(), sortedResults.end(),
                           [](const ModuleValuePair *lhs,
                              const ModuleValuePair *rhs) {
        return lhs->first->getName().compare(rhs->first->getName());
      });

      auto topLevelDiag = diag::fix_unqualified_access_top_level;
      if (sortedResults.size() > 1)
        topLevelDiag = diag::fix_unqualified_access_top_level_multi;

      for (const ModuleValuePair &pair : sortedResults) {
        DescriptiveDeclKind k = pair.second->getDescriptiveKind();

        SmallString<32> namePlusDot = pair.first->getName().str();
        namePlusDot.push_back('.');

        TC.diagnose(DRE->getLoc(), topLevelDiag,
                    namePlusDot, k, pair.first->getName())
          .fixItInsert(DRE->getStartLoc(), namePlusDot);
      }
    }
  };

  DiagnoseWalker Walker(TC, DC, isExprStmt);
  const_cast<Expr *>(E)->walk(Walker);
}


/// Diagnose recursive use of properties within their own accessors
static void diagRecursivePropertyAccess(TypeChecker &TC, const Expr *E,
                                        const DeclContext *DC) {
  auto fn = dyn_cast<FuncDecl>(DC);
  if (!fn || !fn->isAccessor())
    return;

  auto var = dyn_cast<VarDecl>(fn->getAccessorStorageDecl());
  if (!var)  // Ignore subscripts
    return;

  class DiagnoseWalker : public ASTWalker {
    TypeChecker &TC;
    VarDecl *Var;
    const FuncDecl *Accessor;

  public:
    explicit DiagnoseWalker(TypeChecker &TC, VarDecl *var,
                            const FuncDecl *Accessor)
      : TC(TC), Var(var), Accessor(Accessor) {}

    std::pair<bool, Expr *> walkToExprPre(Expr *E) override {
      Expr *subExpr;
      bool isStore = false;

      if (auto *AE = dyn_cast<AssignExpr>(E)) {
        subExpr = AE->getDest();
        
        // If we couldn't flatten this expression, don't explode.
        if (!subExpr)
          return { true, E };

        isStore = true;
      } else if (auto *IOE = dyn_cast<InOutExpr>(E)) {
        subExpr = IOE->getSubExpr();
        isStore = true;
      } else {
        subExpr = E;
      }

      if (auto *BOE = dyn_cast<BindOptionalExpr>(subExpr))
        subExpr = BOE;

      if (auto *DRE = dyn_cast<DeclRefExpr>(subExpr)) {
        if (DRE->getDecl() == Var) {
          // Handle local and top-level computed variables.
          if (DRE->getAccessSemantics() != AccessSemantics::DirectToStorage) {
            bool shouldDiagnose = false;
            // Warn about any property access in the getter.
            if (Accessor->isGetter())
              shouldDiagnose = !isStore;
            // Warn about stores in the setter, but allow loads.
            if (Accessor->isSetter())
              shouldDiagnose = isStore;

            // But silence the warning if the base was explicitly qualified.
            if (dyn_cast_or_null<DotSyntaxBaseIgnoredExpr>(Parent.getAsExpr()))
              shouldDiagnose = false;

            if (shouldDiagnose) {
              TC.diagnose(subExpr->getLoc(), diag::recursive_accessor_reference,
                          Var->getName(), Accessor->isSetter());
            }
          }
          
          // If this is a direct store in a "willSet", we reject this because
          // it is about to get overwritten.
          if (isStore &&
              DRE->getAccessSemantics() == AccessSemantics::DirectToStorage &&
              Accessor->getAccessorKind() == AccessorKind::IsWillSet) {
            TC.diagnose(E->getLoc(), diag::store_in_willset, Var->getName());
          }
        }


      } else if (auto *MRE = dyn_cast<MemberRefExpr>(subExpr)) {
        // Handle instance and type computed variables.
        // Find MemberRefExprs that have an implicit "self" base.
        if (MRE->getMember().getDecl() == Var &&
            isa<DeclRefExpr>(MRE->getBase()) &&
            MRE->getBase()->isImplicit()) {
          
          if (MRE->getAccessSemantics() != AccessSemantics::DirectToStorage) {
            bool shouldDiagnose = false;
            // Warn about any property access in the getter.
            if (Accessor->isGetter())
              shouldDiagnose = !isStore;
            // Warn about stores in the setter, but allow loads.
            if (Accessor->isSetter())
              shouldDiagnose = isStore;

            if (shouldDiagnose) {
              TC.diagnose(subExpr->getLoc(), diag::recursive_accessor_reference,
                          Var->getName(), Accessor->isSetter());
              TC.diagnose(subExpr->getLoc(),
                          diag::recursive_accessor_reference_silence)
              .fixItInsert(subExpr->getStartLoc(), "self.");
            }
          }

          // If this is a direct store in a "willSet", we reject this because
          // it is about to get overwritten.
          if (isStore &&
              MRE->getAccessSemantics() == AccessSemantics::DirectToStorage &&
              Accessor->getAccessorKind() == AccessorKind::IsWillSet) {
              TC.diagnose(subExpr->getLoc(), diag::store_in_willset,
                          Var->getName());
          }
        }

      }

      return { true, E };
    }
  };

  DiagnoseWalker walker(TC, var, fn);
  const_cast<Expr *>(E)->walk(walker);
}

/// Look for any property references in closures that lack a "self." qualifier.
/// Within a closure, we require that the source code contain "self." explicitly
/// because 'self' is captured, not the property value.  This is a common source
/// of confusion, so we force an explicit self.
static void diagnoseImplicitSelfUseInClosure(TypeChecker &TC, const Expr *E,
                                             const DeclContext *DC) {
  class DiagnoseWalker : public ASTWalker {
    TypeChecker &TC;
    unsigned InClosure;
  public:
    explicit DiagnoseWalker(TypeChecker &TC, bool isAlreadyInClosure)
        : TC(TC), InClosure(isAlreadyInClosure) {}

    /// Return true if this is an implicit reference to self.
    static bool isImplicitSelfUse(Expr *E) {
      auto *DRE = dyn_cast<DeclRefExpr>(E);
      return DRE && DRE->isImplicit() && DRE->getDecl()->hasName() &&
             DRE->getDecl()->getName().str() == "self" &&
             // Metatype self captures don't extend the lifetime of an object.
             !DRE->getType()->is<MetatypeType>();
    }

    /// Return true if this is a closure expression that will require "self."
    /// qualification of member references.
    static bool isClosureRequiringSelfQualification(
                  const AbstractClosureExpr *CE) {
      // If the closure's type was inferred to be noescape, then it doesn't
      // need qualification.
      return !AnyFunctionRef(const_cast<AbstractClosureExpr *>(CE))
               .isKnownNoEscape();
    }


    // Don't walk into nested decls.
    bool walkToDeclPre(Decl *D) override {
      return false;
    }

    std::pair<bool, Expr *> walkToExprPre(Expr *E) override {
      if (auto *CE = dyn_cast<AbstractClosureExpr>(E)) {
        if (!CE->hasSingleExpressionBody())
          return { false, E };

        // If this is a potentially-escaping closure expression, start looking
        // for references to self if we aren't already.
        if (isClosureRequiringSelfQualification(CE))
          ++InClosure;
      }


      // If we aren't in a closure, no diagnostics will be produced.
      if (!InClosure)
        return { true, E };

      // If we see a property reference with an implicit base from within a
      // closure, then reject it as requiring an explicit "self." qualifier.  We
      // do this in explicit closures, not autoclosures, because otherwise the
      // transparence of autoclosures is lost.
      if (auto *MRE = dyn_cast<MemberRefExpr>(E))
        if (isImplicitSelfUse(MRE->getBase())) {
          TC.diagnose(MRE->getLoc(),
                      diag::property_use_in_closure_without_explicit_self,
                      MRE->getMember().getDecl()->getName())
            .fixItInsert(MRE->getLoc(), "self.");
          return { false, E };
        }

      // Handle method calls with a specific diagnostic + fixit.
      if (auto *DSCE = dyn_cast<DotSyntaxCallExpr>(E))
        if (isImplicitSelfUse(DSCE->getBase()) &&
            isa<DeclRefExpr>(DSCE->getFn())) {
          auto MethodExpr = cast<DeclRefExpr>(DSCE->getFn());
          TC.diagnose(DSCE->getLoc(),
                      diag::method_call_in_closure_without_explicit_self,
                      MethodExpr->getDecl()->getName())
            .fixItInsert(DSCE->getLoc(), "self.");
          return { false, E };
        }

      // Catch any other implicit uses of self with a generic diagnostic.
      if (isImplicitSelfUse(E))
        TC.diagnose(E->getLoc(), diag::implicit_use_of_self_in_closure);

      return { true, E };
    }
    
    Expr *walkToExprPost(Expr *E) override {
      if (auto *CE = dyn_cast<AbstractClosureExpr>(E)) {
        if (isClosureRequiringSelfQualification(CE)) {
          assert(InClosure);
          --InClosure;
        }
      }
      
      return E;
    }
  };

  bool isAlreadyInClosure = false;
  if (DC->isLocalContext()) {
    while (DC->getParent()->isLocalContext() && !isAlreadyInClosure) {
      if (auto *closure = dyn_cast<AbstractClosureExpr>(DC))
        if (DiagnoseWalker::isClosureRequiringSelfQualification(closure))
          isAlreadyInClosure = true;
      DC = DC->getParent();
    }
  }
  const_cast<Expr *>(E)->walk(DiagnoseWalker(TC, isAlreadyInClosure));
}

//===--------------------------------------------------------------------===//
// Diagnose availability.
//===--------------------------------------------------------------------===//

static void tryFixPrintWithAppendNewline(const ValueDecl *D,
                                         const Expr *ParentExpr,
                                         InFlightDiagnostic &Diag) {
  if (!D || !ParentExpr)
    return;
  if (!D->getModuleContext()->isStdlibModule())
    return;

  DeclName Name = D->getFullName();
  if (Name.getBaseName().str() != "print")
    return;
  auto ArgNames = Name.getArgumentNames();
  if (ArgNames.size() != 2)
    return;
  if (ArgNames[1].str() != "appendNewline")
    return;

  // Go through the expr to determine if second parameter is boolean literal.
  auto *CE = dyn_cast_or_null<CallExpr>(ParentExpr);
  if (!CE)
    return;
  auto *TE = dyn_cast<TupleExpr>(CE->getArg());
  if (!TE)
    return;
  if (TE->getNumElements() != 2)
    return;
  auto *SCE = dyn_cast<CallExpr>(TE->getElement(1));
  if (!SCE || !SCE->isImplicit())
    return;
  auto *STE = dyn_cast<TupleExpr>(SCE->getArg());
  if (!STE || !STE->isImplicit())
    return;
  if (STE->getNumElements() != 1)
    return;
  auto *BE = dyn_cast<BooleanLiteralExpr>(STE->getElement(0));
  if (!BE)
    return;

  SmallString<20> termStr = StringRef("terminator: \"");
  if (BE->getValue())
    termStr += "\\n";
  termStr += "\"";

  SourceRange RangeToFix(TE->getElementNameLoc(1), BE->getEndLoc());
  Diag.fixItReplace(RangeToFix, termStr);
}

/// Emit a diagnostic for references to declarations that have been
/// marked as unavailable, either through "unavailable" or "obsoleted=".
bool TypeChecker::diagnoseExplicitUnavailability(const ValueDecl *D,
                                                 SourceRange R,
                                                 const DeclContext *DC,
                                                 const Expr *ParentExpr) {
  auto *Attr = AvailableAttr::isUnavailable(D);
  if (!Attr)
    return false;

  // Suppress the diagnostic if we are in synthesized code inside
  // a synthesized function and the reference is lexically
  // contained in a declaration that is itself marked unavailable.
  // The right thing to do here is to not synthesize that code in the
  // first place. rdar://problem/20491640
  if (R.isInvalid() && isInsideImplicitFunction(R, DC) &&
      isInsideUnavailableDeclaration(R, DC)) {
    return false;
  }

  SourceLoc Loc = R.Start;
  auto Name = D->getFullName();

  switch (Attr->getUnconditionalAvailability()) {
  case UnconditionalAvailabilityKind::Deprecated:
    break;

  case UnconditionalAvailabilityKind::None:
  case UnconditionalAvailabilityKind::Unavailable:
    if (!Attr->Rename.empty()) {
      if (Attr->Message.empty()) {
        diagnose(Loc, diag::availability_decl_unavailable_rename, Name,
                 Attr->Rename)
          .fixItReplace(R, Attr->Rename);
      } else {
        diagnose(Loc, diag::availability_decl_unavailable_rename_msg, Name,
                 Attr->Rename, Attr->Message)
          .fixItReplace(R, Attr->Rename);
      }
    } else if (Attr->Message.empty()) {
      diagnose(Loc, diag::availability_decl_unavailable, Name).highlight(R);
    } else {
      EncodedDiagnosticMessage EncodedMessage(Attr->Message);
      tryFixPrintWithAppendNewline(D, ParentExpr,
        diagnose(Loc, diag::availability_decl_unavailable_msg, Name,
                    EncodedMessage.Message).highlight(R));
    }
    break;

  case UnconditionalAvailabilityKind::UnavailableInSwift:
    if (Attr->Message.empty()) {
      diagnose(Loc, diag::availability_decl_unavailable_in_swift, Name)
        .highlight(R);
    } else {
      EncodedDiagnosticMessage EncodedMessage(Attr->Message);
      diagnose(Loc, diag::availability_decl_unavailable_in_swift_msg, Name,
                  EncodedMessage.Message).highlight(R);
    }
    break;
  }

  auto MinVersion = Context.LangOpts.getMinPlatformVersion();
  switch (Attr->getMinVersionAvailability(MinVersion)) {
  case MinVersionComparison::Available:
  case MinVersionComparison::PotentiallyUnavailable:
    llvm_unreachable("These aren't considered unavailable");

  case MinVersionComparison::Unavailable:
    diagnose(D, diag::availability_marked_unavailable, Name)
        .highlight(Attr->getRange());
    break;

  case MinVersionComparison::Obsoleted:
    // FIXME: Use of the platformString here is non-awesome for application
    // extensions.
    diagnose(D, diag::availability_obsoleted, Name,
             Attr->prettyPlatformString(),
             *Attr->Obsoleted).highlight(Attr->getRange());
    break;
  }
  return true;
}

/// Diagnose uses of unavailable declarations. Returns true if a diagnostic
/// was emitted.
static bool diagAvailability(TypeChecker &TC, const ValueDecl *D,
                             SourceRange R, const DeclContext *DC,
                             const Expr *ParentExpr = nullptr) {
  if (!D)
    return false;

  if (TC.diagnoseExplicitUnavailability(D, R, DC, ParentExpr))
    return true;

  // Diagnose for deprecation
  if (const AvailableAttr *Attr = TypeChecker::getDeprecated(D)) {
    TC.diagnoseDeprecated(R, DC, Attr, D->getFullName());
  }
  
  if (TC.getLangOpts().DisableAvailabilityChecking) {
    return false;
  }

  // Diagnose for potential unavailability
  auto maybeUnavail = TC.checkDeclarationAvailability(D, R.Start, DC);
  if (maybeUnavail.hasValue()) {
    TC.diagnosePotentialUnavailability(D, R, DC, maybeUnavail.getValue());
    return true;
  }
  return false;
}

namespace {
class AvailabilityWalker : public ASTWalker {
  /// Describes how the next member reference will be treated as we traverse
  /// the AST.
  enum class MemberAccessContext : unsigned {
    /// The member reference is in a context where an access will call
    /// the getter.
    Getter,

    /// The member reference is in a context where an access will call
    /// the setter.
    Setter,

    /// The member reference is in a context where it will be turned into
    /// an inout argument. (Once this happens, we have to conservatively assume
    /// that both the getter and setter could be called.)
    InOut
  };

  TypeChecker &TC;
  const DeclContext *DC;
  const MemberAccessContext AccessContext;
  SmallVector<const Expr *, 16> ExprStack;

public:
  AvailabilityWalker(
      TypeChecker &TC, const DeclContext *DC,
      MemberAccessContext AccessContext = MemberAccessContext::Getter)
      : TC(TC), DC(DC), AccessContext(AccessContext) {}

  virtual std::pair<bool, Expr *> walkToExprPre(Expr *E) override {
    ExprStack.push_back(E);

    auto visitChildren = [&]() { return std::make_pair(true, E); };
    auto skipChildren = [&]() {
      ExprStack.pop_back();
      return std::make_pair(false, E);
    };

    if (auto DR = dyn_cast<DeclRefExpr>(E))
      diagAvailability(TC, DR->getDecl(), DR->getSourceRange(), DC,
                       getParentForDeclRef());
    if (auto MR = dyn_cast<MemberRefExpr>(E)) {
      walkMemberRef(MR);
      return skipChildren();
    }
    if (auto OCDR = dyn_cast<OtherConstructorDeclRefExpr>(E))
      diagAvailability(TC, OCDR->getDecl(), OCDR->getConstructorLoc(), DC);
    if (auto DMR = dyn_cast<DynamicMemberRefExpr>(E))
      diagAvailability(TC, DMR->getMember().getDecl(), DMR->getNameLoc(), DC);
    if (auto DS = dyn_cast<DynamicSubscriptExpr>(E))
      diagAvailability(TC, DS->getMember().getDecl(), DS->getSourceRange(), DC);
    if (auto S = dyn_cast<SubscriptExpr>(E)) {
      if (S->hasDecl())
        diagAvailability(TC, S->getDecl().getDecl(), S->getSourceRange(), DC);
    }
    if (auto A = dyn_cast<AssignExpr>(E)) {
      walkAssignExpr(A);
      return skipChildren();
    }
    if (auto IO = dyn_cast<InOutExpr>(E)) {
      walkInOutExpr(IO);
      return skipChildren();
    }
    
    return visitChildren();
  }

  virtual Expr *walkToExprPost(Expr *E) override {
    assert(ExprStack.back() == E);
    ExprStack.pop_back();

    return E;
  }

private:
  /// Walk an assignment expression, checking for availability.
  void walkAssignExpr(AssignExpr *E) const {
    // We take over recursive walking of assignment expressions in order to
    // walk the destination and source expressions in different member
    // access contexts.
    Expr *Dest = E->getDest();
    if (!Dest) {
      return;
    }

    // Check the Dest expression in a setter context.
    // We have an implicit assumption here that the first MemberRefExpr
    // encountered walking (pre-order) is the Dest is the destination of the
    // write. For the moment this is fine -- but future syntax might violate
    // this assumption.
    walkInContext(Dest, MemberAccessContext::Setter);

    // Check RHS in getter context
    Expr *Source = E->getSrc();
    if (!Source) {
      return;
    }
    walkInContext(Source, MemberAccessContext::Getter);
  }
  
  /// Walk a member reference expression, checking for availability.
  void walkMemberRef(MemberRefExpr *E) {
    // Walk the base in a getter context.
    walkInContext(E->getBase(), MemberAccessContext::Getter);

    ValueDecl *D = E->getMember().getDecl();
    // Diagnose for the member declaration itself.
    if (diagAvailability(TC, D, E->getNameLoc(), DC)) {
      return;
    }

    if (TC.getLangOpts().DisableAvailabilityChecking) {
      return;
    }

    if (auto *ASD = dyn_cast<AbstractStorageDecl>(D)) {
      // Diagnose for appropriate accessors, given the access context.
      diagStorageAccess(ASD, E->getSourceRange(), DC);
    }
  }
  
  /// Walk an inout expression, checking for availability.
  void walkInOutExpr(InOutExpr *E) {
    walkInContext(E->getSubExpr(), MemberAccessContext::InOut);
  }

  /// Walk the given expression in the member access context.
  void walkInContext(Expr *E, MemberAccessContext AccessContext) const {
    E->walk(AvailabilityWalker(TC, DC, AccessContext));
  }

  /// Emit diagnostics, if necessary, for accesses to storage where
  /// the accessor for the AccessContext is not available.
  void diagStorageAccess(AbstractStorageDecl *D,
                         SourceRange ReferenceRange,
                         const DeclContext *ReferenceDC) const {
    if (!D->hasAccessorFunctions()) {
      return;
    }
    
    // Check availability of accessor functions
    switch (AccessContext) {
    case MemberAccessContext::Getter:
      diagAccessorAvailability(D->getGetter(), ReferenceRange, ReferenceDC,
                               /*ForInout=*/false);
      break;

    case MemberAccessContext::Setter:
      diagAccessorAvailability(D->getSetter(), ReferenceRange, ReferenceDC,
                               /*ForInout=*/false);
      break;

    case MemberAccessContext::InOut:
      diagAccessorAvailability(D->getGetter(), ReferenceRange, ReferenceDC,
                               /*ForInout=*/true);

      diagAccessorAvailability(D->getSetter(), ReferenceRange, ReferenceDC,
                               /*ForInout=*/true);
      break;
    }
  }

  /// Emit a diagnostic, if necessary for a potentially unavailable accessor.
  /// Returns true if a diagnostic was emitted.
  void diagAccessorAvailability(FuncDecl *D, SourceRange ReferenceRange,
                                const DeclContext *ReferenceDC,
                                bool ForInout) const {
    if (!D) {
      return;
    }
    auto MaybeUnavail = TC.checkDeclarationAvailability(D, ReferenceRange.Start,
                                                        DC);
    if (MaybeUnavail.hasValue()) {
      TC.diagnosePotentialAccessorUnavailability(D, ReferenceRange, ReferenceDC,
                                                 MaybeUnavail.getValue(),
                                                 ForInout);
    }
  }

  const Expr *getParentForDeclRef() {
    assert(isa<DeclRefExpr>(ExprStack.back()));
    ArrayRef<const Expr *> Stack = ExprStack;

    Stack = Stack.drop_back();
    if (Stack.empty())
      return nullptr;

    if (isa<DotSyntaxBaseIgnoredExpr>(Stack.back()))
      Stack = Stack.drop_back();
    if (Stack.empty())
      return nullptr;
    
    return Stack.back();
  }
};
}

/// Diagnose uses of unavailable declarations.
static void diagAvailability(TypeChecker &TC, const Expr *E,
                             const DeclContext *DC) {
  AvailabilityWalker walker(TC, DC);
  const_cast<Expr*>(E)->walk(walker);
}

//===--------------------------------------------------------------------===//
// High-level entry points.
//===--------------------------------------------------------------------===//

/// \brief Emit diagnostics for syntactic restrictions on a given expression.
void swift::performSyntacticExprDiagnostics(TypeChecker &TC, const Expr *E,
                                            const DeclContext *DC,
                                            bool isExprStmt) {
  diagSelfAssignment(TC, E);
  diagSyntacticUseRestrictions(TC, E, DC, isExprStmt);
  diagRecursivePropertyAccess(TC, E, DC);
  diagnoseImplicitSelfUseInClosure(TC, E, DC);
  diagAvailability(TC, E, DC);
}

void swift::performStmtDiagnostics(TypeChecker &TC, const Stmt *S) {
  TC.checkUnsupportedProtocolType(const_cast<Stmt *>(S));
}

//===--------------------------------------------------------------------===//
// Per func/init diagnostics
//===--------------------------------------------------------------------===//

namespace {
class VarDeclUsageChecker : public ASTWalker {
  TypeChecker &TC;
  
  // Keep track of some information about a variable.
  enum {
    RK_Read        = 1,      ///< Whether it was ever read.
    RK_Written     = 2,      ///< Whether it was ever written or passed inout.
    
    RK_CaptureList = 4       ///< Var is an entry in a capture list.
  };
  
  /// These are all of the variables that we are tracking.  VarDecls get added
  /// to this when the declaration is seen.  We use a MapVector to keep the
  /// diagnostics emission in deterministic order.
  llvm::SmallMapVector<VarDecl*, unsigned, 32> VarDecls;

  /// This is a mapping from an OpaqueValue to the expression that initialized
  /// it.
  llvm::SmallDenseMap<OpaqueValueExpr*, Expr*> OpaqueValueMap;
  
  /// This is a mapping from VarDecls to the if/while/guard statement that they
  /// occur in, when they are in a pattern in a StmtCondition.
  llvm::SmallDenseMap<VarDecl*, LabeledConditionalStmt*> StmtConditionForVD;
  
  bool sawError = false;
  
  VarDeclUsageChecker(const VarDeclUsageChecker &) = delete;
  void operator=(const VarDeclUsageChecker &) = delete;

public:
  VarDeclUsageChecker(TypeChecker &TC, AbstractFunctionDecl *AFD) : TC(TC) {
    // Track the parameters of the function.
    for (auto P : AFD->getBodyParamPatterns())
      P->forEachVariable([&](VarDecl *VD) {
        if (shouldTrackVarDecl(VD))
          VarDecls[VD] = 0;
      });
  }

  // After we have scanned the entire region, diagnose variables that could be
  // declared with a narrower usage kind.
  ~VarDeclUsageChecker();

  /// Check to see if the specified VarDecl is part of a larger
  /// PatternBindingDecl, where some other bound variable was mutated.  In this
  /// case we don't want to generate a "variable never mutated" warning, because
  /// it would require splitting up the destructuring of the tuple, which is
  ///  more code turmoil than the warning is worth.
  bool isVarDeclPartOfPBDThatHadSomeMutation(VarDecl *VD) {
    auto *PBD = VD->getParentPatternBinding();
    if (!PBD) return false;

    bool sawMutation = false;
    for (const auto &PBE : PBD->getPatternList()) {
      PBE.getPattern()->forEachVariable([&](VarDecl *VD) {
        auto it = VarDecls.find(VD);
        sawMutation |= it != VarDecls.end() && (it->second & RK_Written);
      });
    }
    return sawMutation;
  }

  bool shouldTrackVarDecl(VarDecl *VD) {
    // If the variable is implicit, ignore it.
    if (VD->isImplicit() || VD->getLoc().isInvalid())
      return false;
    
    // If the variable was invalid, ignore it and notice that the code is
    // malformed.
    if (VD->isInvalid() || !VD->hasType()) {
      sawError = true;
      return false;
    }
    
    // If the variable is already unnamed, ignore it.
    if (!VD->hasName() || VD->getName().str() == "_")
      return false;
    
    return true;
  }

  void addMark(Decl *D, unsigned Flag) {
    auto *vd = dyn_cast<VarDecl>(D);
    if (!vd) return;

    auto vdi = VarDecls.find(vd);
    if (vdi != VarDecls.end())
      vdi->second |= Flag;
  }
  
  void markBaseOfAbstractStorageDeclStore(Expr *E, ConcreteDeclRef decl);
  
  void markStoredOrInOutExpr(Expr *E, unsigned Flags);
  
  // We generally walk into declarations, other than types and nested functions.
  // FIXME: peek into capture lists of nested functions.
  bool walkToDeclPre(Decl *D) override {
    if (isa<TypeDecl>(D))
      return false;
      
    // If this is a VarDecl, then add it to our list of things to track.
    if (auto *vd = dyn_cast<VarDecl>(D))
      if (shouldTrackVarDecl(vd)) {
        unsigned defaultFlags = 0;
        // If this VarDecl is nested inside of a CaptureListExpr, remember that
        // fact for better diagnostics.
        if (dyn_cast_or_null<CaptureListExpr>(Parent.getAsExpr()))
          defaultFlags = RK_CaptureList;
        VarDecls[vd] |= defaultFlags;
      }

    if (auto *afd = dyn_cast<AbstractFunctionDecl>(D)) {
      // If this is a nested function with a capture list, mark any captured
      // variables.
      if (afd->isBodyTypeChecked()) {
        for (const auto &capture : afd->getCaptureInfo().getCaptures())
          addMark(capture.getDecl(), RK_Read|RK_Written);
      } else {
        // If the body hasn't been type checked yet, be super-conservative and
        // mark all variables as used.  This can be improved later, e.g. by
        // walking the untype-checked body to look for things that could
        // possibly be used.
        VarDecls.clear();
      }
      
      // Don't walk into it though, it may not even be type checked yet.
      return false;
    }

    
    // Note that we ignore the initialization behavior of PatternBindingDecls,
    // but we do want to walk into them, because we want to see any uses or
    // other things going on in the initializer expressions.
    return true;
  }
  
  /// The heavy lifting happens when visiting expressions.
  std::pair<bool, Expr *> walkToExprPre(Expr *E) override;

  /// handle #if directives.
  void handleIfConfig(IfConfigStmt *ICS);

  /// Custom handling for statements.
  std::pair<bool, Stmt *> walkToStmtPre(Stmt *S) override {
    // The body of #if statements are not walked into, we need custom processing
    // for them.
    if (auto *ICS = dyn_cast<IfConfigStmt>(S))
      handleIfConfig(ICS);
      
    // Keep track of an association between vardecls and the StmtCondition that
    // they are bound in for IfStmt, GuardStmt, WhileStmt, etc.
    if (auto LCS = dyn_cast<LabeledConditionalStmt>(S)) {
      for (auto &cond : LCS->getCond())
        if (auto pat = cond.getPatternOrNull()) {
          pat->forEachVariable([&](VarDecl *VD) {
            StmtConditionForVD[VD] = LCS;
          });
        }
    }
      
    return { true, S };
  }

};
}


// After we have scanned the entire region, diagnose variables that could be
// declared with a narrower usage kind.
VarDeclUsageChecker::~VarDeclUsageChecker() {
  // If we saw an ErrorExpr somewhere in the body, then we have a malformed AST
  // and we know stuff got dropped.  Instead of producing these diagnostics,
  // lets let the bigger issues get resolved first.
  if (sawError)
    return;
  
  for (auto elt : VarDecls) {
    auto *var = elt.first;
    unsigned access = elt.second;

    // If this is a 'let' value, any stores to it are actually initializations,
    // not mutations.
    if (var->isLet())
      access &= ~RK_Written;
    
    // If this variable has WeakStorageType, then it can be mutated in ways we
    // don't know.
    if (var->getType()->is<WeakStorageType>())
      access |= RK_Written;
    
    // If this is a vardecl with 'inout' type, then it is an inout argument to a
    // function, never diagnose anything related to it.
    if (var->getType()->is<InOutType>())
      continue;    
    
    // Consider parameters to always have been read.  It is common to name a
    // parameter and not use it (e.g. because you are an override or want the
    // named keyword, etc).  Warning to rewrite it to _ is more annoying than
    // it is useful.
    if (isa<ParamDecl>(var))
      access |= RK_Read;
    
    // Diagnose variables that were never used (other than their
    // initialization).
    //
    if ((access & (RK_Read|RK_Written)) == 0) {
      // If this is a member in a capture list, just say it is unused.  We could
      // produce a fixit hint with a parent map, but this is a lot of effort for
      // a narrow case.
      if (access & RK_CaptureList) {
        TC.diagnose(var->getLoc(), diag::capture_never_used,
                    var->getName());
        continue;
      }
      
      // If the source of the VarDecl is a trivial PatternBinding with only a
      // single binding, rewrite the whole thing into an assignment.
      //    let x = foo()
      //  ->
      //    _ = foo()
      if (auto *pbd = var->getParentPatternBinding())
        if (pbd->getSingleVar() == var && pbd->getInit(0) != nullptr) {
          unsigned varKind = var->isLet();
          TC.diagnose(var->getLoc(), diag::pbd_never_used,
                      var->getName(), varKind)
            .fixItReplace(SourceRange(pbd->getLoc(), var->getLoc()), "_");
          continue;
        }
      
      // If the variable is defined in a pattern in an if/while/guard statement,
      // see if we can produce a tuned fixit.  When we have something like:
      //
      //    if let x = <expr> {
      //
      // we prefer to rewrite it to:
      //
      //    if <expr> != nil {
      //
      if (auto SC = StmtConditionForVD[var]) {
        // We only handle the "if let" case right now, since it is vastly the
        // most common situation that people run into.
        if (SC->getCond().size() == 1) {
          auto pattern = SC->getCond()[0].getPattern();
          if (auto OSP = dyn_cast<OptionalSomePattern>(pattern))
            if (auto LP = dyn_cast<VarPattern>(OSP->getSubPattern()))
              if (isa<NamedPattern>(LP->getSubPattern())) {
                auto initExpr = SC->getCond()[0].getInitializer();
                auto beforeExprLoc =
                  initExpr->getStartLoc().getAdvancedLocOrInvalid(-1);
                if (beforeExprLoc.isValid()) {
                  unsigned noParens = initExpr->canAppendCallParentheses();
                  
                  // If the subexpr is an "as?" cast, we can rewrite it to
                  // be an "is" test.
                  bool isIsTest = false;
                  if (isa<ConditionalCheckedCastExpr>(initExpr) &&
                      !initExpr->isImplicit()) {
                    noParens = isIsTest = true;
                  }
                  
                  auto diagIF = TC.diagnose(var->getLoc(),
                                            diag::pbd_never_used_stmtcond,
                                            var->getName());
                  auto introducerLoc = SC->getCond()[0].getIntroducerLoc();
                  diagIF.fixItReplace(SourceRange(introducerLoc, beforeExprLoc),
                                      &"("[noParens]);
                  
                  if (isIsTest) {
                    // If this was an "x as? T" check, rewrite it to "x is T".
                    auto CCE = cast<ConditionalCheckedCastExpr>(initExpr);
                    diagIF.fixItReplace(SourceRange(CCE->getLoc(),
                                                    CCE->getQuestionLoc()),
                                        "is");
                  } else {
                    diagIF.fixItInsertAfter(initExpr->getEndLoc(),
                                            &") != nil"[noParens]);
                  }
                  continue;
                }
              }
        }
      }
      
      // Otherwise, this is something more complex, perhaps
      //    let (a,b) = foo()
      // Just rewrite the one variable with a _.
      unsigned varKind = var->isLet();
      TC.diagnose(var->getLoc(), diag::variable_never_used,
                  var->getName(), varKind)
        .fixItReplace(var->getLoc(), "_");
      continue;
    }
    
    // If this is a mutable 'var', and it was never written to, suggest
    // upgrading to 'let'.  We do this even for a parameter.
    if (!var->isLet() && (access & RK_Written) == 0 &&
        // Don't warn if we have something like "let (x,y) = ..." and 'y' was
        // never mutated, but 'x' was.
        !isVarDeclPartOfPBDThatHadSomeMutation(var)) {
      SourceLoc FixItLoc;
      
      // Try to find the location of the 'var' so we can produce a fixit.  If
      // this is a simple PatternBinding, use its location.
      if (auto *PBD = var->getParentPatternBinding()) {
        if (PBD->getSingleVar() == var)
          FixItLoc = PBD->getLoc();
      } else if (auto *pattern = var->getParentPattern()) {
        VarPattern *foundVP = nullptr;
        pattern->forEachNode([&](Pattern *P) {
          if (auto *VP = dyn_cast<VarPattern>(P))
            foundVP = VP;
        });
        
        if (foundVP && !foundVP->isLet())
          FixItLoc = foundVP->getLoc();
      }

      // If this is a parameter explicitly marked 'var', remove it.
      if (auto *param = dyn_cast<ParamDecl>(var))
        if (auto *pattern = param->getParamParentPattern())
          if (auto *vp = dyn_cast<VarPattern>(pattern)) {
            TC.diagnose(var->getLoc(), diag::variable_never_mutated,
                        var->getName(), /*param*/1)
              .fixItRemove(vp->getLoc());
            continue;
          }
      
      unsigned varKind = isa<ParamDecl>(var);
      // FIXME: fixit when we can find a pattern binding.
      if (FixItLoc.isInvalid())
        TC.diagnose(var->getLoc(), diag::variable_never_mutated,
                    var->getName(), varKind);
      else
        TC.diagnose(var->getLoc(), diag::variable_never_mutated,
                    var->getName(), varKind)
          .fixItReplace(FixItLoc, "let");
      continue;
    }
    
    // If this is a variable that was only written to, emit a warning.
    if ((access & RK_Read) == 0) {
      TC.diagnose(var->getLoc(), diag::variable_never_read, var->getName(),
                  isa<ParamDecl>(var));
      continue;
    }
  }
}

/// Handle a store to "x.y" where 'base' is the expression for x and 'decl' is
/// the decl for 'y'.
void VarDeclUsageChecker::
markBaseOfAbstractStorageDeclStore(Expr *base, ConcreteDeclRef decl) {
  // If the base is a class or an rvalue, then this store just loads the base.
  if (base->getType()->isAnyClassReferenceType() ||
      !(base->getType()->isLValueType() || base->getType()->is<InOutType>())) {
    base->walk(*this);
    return;
  }

  // If the store is to a non-mutating member, then this is just a load, even
  // if the base is an inout expr.
  auto *ASD = cast<AbstractStorageDecl>(decl.getDecl());
  if (ASD->isSettable(nullptr) && ASD->isSetterNonMutating()) {
    // Sema conservatively converts the base to inout expr when it is an lvalue.
    // Look through it because we know it isn't actually doing a load/store.
    if (auto *ioe = dyn_cast<InOutExpr>(base))
      base = ioe->getSubExpr();
    base->walk(*this);
    return;
  }
  
  // Otherwise this is a read and write of the base.
  return markStoredOrInOutExpr(base, RK_Written|RK_Read);
}


void VarDeclUsageChecker::markStoredOrInOutExpr(Expr *E, unsigned Flags) {
  // Sema leaves some subexpressions null, which seems really unfortunate.  It
  // should replace them with ErrorExpr.
  if (E == nullptr || !E->getType() || E->getType()->is<ErrorType>()) {
    sawError = true;
    return;
  }
  
  // Ignore parens and other easy cases.
  E = E->getSemanticsProvidingExpr();
  
  // If we found a decl that is being assigned to, then mark it.
  if (auto *DRE = dyn_cast<DeclRefExpr>(E)) {
    addMark(DRE->getDecl(), Flags);
    return;
  }
  
  if (auto *TE = dyn_cast<TupleExpr>(E)) {
    for (auto &elt : TE->getElements())
      markStoredOrInOutExpr(elt, Flags);
    return;
  }
  
  // If this is an assignment into a mutating subscript lvalue expr, then we
  // are mutating the base expression.  We also need to visit the index
  // expressions as loads though.
  if (auto *SE = dyn_cast<SubscriptExpr>(E)) {
    // The index of the subscript is evaluted as an rvalue.
    SE->getIndex()->walk(*this);
    if (SE->hasDecl())
      markBaseOfAbstractStorageDeclStore(SE->getBase(), SE->getDecl());
    else  // FIXME: Should not be needed!
      markStoredOrInOutExpr(SE->getBase(), RK_Written|RK_Read);
    
    return;
  }
  
  if (auto *ioe = dyn_cast<InOutExpr>(E))
    return markStoredOrInOutExpr(ioe->getSubExpr(), RK_Written|RK_Read);
  
  if (auto *MRE = dyn_cast<MemberRefExpr>(E)) {
    markBaseOfAbstractStorageDeclStore(MRE->getBase(), MRE->getMember());
    return;
  }
  
  if (auto *TEE = dyn_cast<TupleElementExpr>(E))
    return markStoredOrInOutExpr(TEE->getBase(), Flags);
  
  if (auto *FVE = dyn_cast<ForceValueExpr>(E))
    return markStoredOrInOutExpr(FVE->getSubExpr(), Flags);

  if (auto *BOE = dyn_cast<BindOptionalExpr>(E))
    return markStoredOrInOutExpr(BOE->getSubExpr(), Flags);
  
  // If this is an OpaqueValueExpr that we've seen a mapping for, jump to the
  // mapped value.
  if (auto *OVE = dyn_cast<OpaqueValueExpr>(E))
    if (auto *expr = OpaqueValueMap[OVE])
      return markStoredOrInOutExpr(expr, Flags);

  // If we don't know what kind of expression this is, assume it's a reference
  // and mark it as a read.
  E->walk(*this);
}

   

/// The heavy lifting happens when visiting expressions.
std::pair<bool, Expr *> VarDeclUsageChecker::walkToExprPre(Expr *E) {
  // Sema leaves some subexpressions null, which seems really unfortunate.  It
  // should replace them with ErrorExpr.
  if (E == nullptr || !E->getType() || E->getType()->is<ErrorType>()) {
    sawError = true;
    return { false, E };
  }

  // If this is a DeclRefExpr found in a random place, it is a load of the
  // vardecl.
  if (auto *DRE = dyn_cast<DeclRefExpr>(E))
    addMark(DRE->getDecl(), RK_Read);

  // If this is an AssignExpr, see if we're mutating something that we know
  // about.
  if (auto *assign = dyn_cast<AssignExpr>(E)) {
    markStoredOrInOutExpr(assign->getDest(), RK_Written);
    
    // Don't walk into the LHS of the assignment, only the RHS.
    assign->getSrc()->walk(*this);
    return { false, E };
  }
  
  // '&x' is a read and write of 'x'.
  if (auto *io = dyn_cast<InOutExpr>(E)) {
    markStoredOrInOutExpr(io->getSubExpr(), RK_Read|RK_Written);
    // Don't bother walking into this.
    return { false, E };
  }
  
  // If we see an OpenExistentialExpr, remember the mapping for its OpaqueValue.
  if (auto *oee = dyn_cast<OpenExistentialExpr>(E))
    OpaqueValueMap[oee->getOpaqueValue()] = oee->getExistentialValue();
  
  // If we saw an ErrorExpr, take note of this.
  if (isa<ErrorExpr>(E))
    sawError = true;
  
  return { true, E };
}

/// handle #if directives.  All of the active clauses are already walked by the
/// AST walker, but we also want to handle the inactive ones to avoid false
/// positives.
void VarDeclUsageChecker::handleIfConfig(IfConfigStmt *ICS) {
  struct ConservativeDeclMarker : public ASTWalker {
    VarDeclUsageChecker &VDUC;
    ConservativeDeclMarker(VarDeclUsageChecker &VDUC) : VDUC(VDUC) {}

    Expr *walkToExprPost(Expr *E) override {
      // If we see a bound reference to a decl in an inactive #if block, then
      // conservatively mark it read and written.  This will silence "variable
      // unused" and "could be marked let" warnings for it.
      if (auto *DRE = dyn_cast<DeclRefExpr>(E))
        VDUC.addMark(DRE->getDecl(), RK_Read|RK_Written);
      return E;
    }
  };

  for (auto &clause : ICS->getClauses()) {
    // Active clauses are handled by the normal AST walk.
    if (clause.isActive) continue;

    for (auto elt : clause.Elements)
      elt.walk(ConservativeDeclMarker(*this));
  }
}

/// Perform diagnostics for func/init/deinit declarations.
void swift::performAbstractFuncDeclDiagnostics(TypeChecker &TC,
                                               AbstractFunctionDecl *AFD) {
  assert(AFD->getBody() && "Need a body to check");
  
  // Don't produce these diagnostics for implicitly generated code.
  if (AFD->getLoc().isInvalid() || AFD->isImplicit() || AFD->isInvalid())
    return;
  
  // Check for unused variables, as well as variables that are could be
  // declared as constants.
  AFD->getBody()->walk(VarDeclUsageChecker(TC, AFD));
}




//===--------------------------------------------------------------------===//
// Utility functions
//===--------------------------------------------------------------------===//

void swift::fixItAccessibility(InFlightDiagnostic &diag, ValueDecl *VD,
                               Accessibility desiredAccess, bool isForSetter) {
  StringRef fixItString;
  switch (desiredAccess) {
  case Accessibility::Private:  fixItString = "private ";  break;
  case Accessibility::Internal: fixItString = "internal "; break;
  case Accessibility::Public:   fixItString = "public ";   break;
  }

  DeclAttributes &attrs = VD->getAttrs();
  DeclAttribute *attr;
  if (isForSetter) {
    attr = attrs.getAttribute<SetterAccessibilityAttr>();
    cast<AbstractStorageDecl>(VD)->overwriteSetterAccessibility(desiredAccess);
  } else {
    attr = attrs.getAttribute<AccessibilityAttr>();
    VD->overwriteAccessibility(desiredAccess);

    if (auto *ASD = dyn_cast<AbstractStorageDecl>(VD)) {
      if (auto *getter = ASD->getGetter())
        getter->overwriteAccessibility(desiredAccess);

      if (auto *setterAttr = attrs.getAttribute<SetterAccessibilityAttr>()) {
        if (setterAttr->getAccess() > desiredAccess)
          fixItAccessibility(diag, VD, desiredAccess, true);
      } else {
        ASD->overwriteSetterAccessibility(desiredAccess);
      }
    }
  }

  if (isForSetter && VD->getFormalAccess() == desiredAccess) {
    assert(attr);
    attr->setInvalid();
    // Remove the setter attribute.
    diag.fixItRemove(attr->Range);

  } else if (attr) {
    // This uses getLocation() instead of getRange() because we don't want to
    // replace the "(set)" part of a setter attribute.
    diag.fixItReplace(attr->getLocation(), fixItString.drop_back());
    attr->setInvalid();

  } else if (auto var = dyn_cast<VarDecl>(VD)) {
    if (auto PBD = var->getParentPatternBinding())
      diag.fixItInsert(PBD->getStartLoc(), fixItString);

  } else {
    diag.fixItInsert(VD->getStartLoc(), fixItString);
  }
}

/// Retrieve the type name to be used for determining whether we can
/// omit needless words.
static OmissionTypeName getTypeNameForOmission(Type type) {
  if (!type)
    return "";

  ASTContext &ctx = type->getASTContext();
  Type boolType;
  if (auto boolDecl = ctx.getBoolDecl())
    boolType = boolDecl->getDeclaredInterfaceType();
  Type objcBoolType;
  if (auto objcBoolDecl = ctx.getObjCBoolDecl())
    objcBoolType = objcBoolDecl->getDeclaredInterfaceType();

  /// Determine the options associated with the given type.
  auto getOptions = [&](Type type) {
    // Look for Boolean types.
    OmissionTypeOptions options;

    // Look for Boolean types.
    if (boolType && type->isEqual(boolType)) {
      // Swift.Bool
      options |= OmissionTypeFlags::Boolean;
    } else if (objcBoolType && type->isEqual(objcBoolType)) {
      // ObjectiveC.ObjCBool
      options |= OmissionTypeFlags::Boolean;
    }

    return options;
  };

  do {
    // Look through typealiases.
    if (auto aliasTy = dyn_cast<NameAliasType>(type.getPointer())) {
      type = aliasTy->getDecl()->getUnderlyingType();
      continue;
    }

    // Strip off lvalue/inout types.
    Type newType = type->getLValueOrInOutObjectType();
    if (newType.getPointer() != type.getPointer()) {
      type = newType;
      continue;
    }

    // Look through reference-storage types.
    newType = type->getReferenceStorageReferent();
    if (newType.getPointer() != type.getPointer()) {
      type = newType;
      continue;
    }

    // Look through parentheses.
    if (auto parenTy = dyn_cast<ParenType>(type.getPointer())) {
      type = parenTy->getUnderlyingType();
      continue;
    }

    // Look through optionals.
    if (auto optObjectTy = type->getAnyOptionalObjectType()) {
      type = optObjectTy;
      continue;
    }

    break;
  } while (true);

  // Nominal types.
  if (auto nominal = type->getAnyNominal()) {
    // If we have a collection, get the element type.
    if (auto bound = type->getAs<BoundGenericType>()) {
      ASTContext &ctx = nominal->getASTContext();
      auto args = bound->getGenericArgs();
      if (!args.empty() &&
          (bound->getDecl() == ctx.getArrayDecl() ||
           bound->getDecl() == ctx.getSetDecl())) {
        return OmissionTypeName(nominal->getName().str(),
                                getOptions(bound),
                                getTypeNameForOmission(args[0]).Name);
      }
    }

    // AnyObject -> "Object".
    if (type->isAnyObject())
      return "Object";

    return OmissionTypeName(nominal->getName().str(), getOptions(type));
  }

  // Generic type parameters.
  if (auto genericParamTy = type->getAs<GenericTypeParamType>()) {
    if (auto genericParam = genericParamTy->getDecl())
      return genericParam->getName().str();

    return "";
  }

  // Dependent members.
  if (auto dependentMemberTy = type->getAs<DependentMemberType>()) {
    return dependentMemberTy->getName().str();
  }

  // Archetypes.
  if (auto archetypeTy = type->getAs<ArchetypeType>()) {
    return archetypeTy->getName().str();
  }

  // Function types.
  if (auto funcTy = type->getAs<AnyFunctionType>()) {
    if (funcTy->getRepresentation() == AnyFunctionType::Representation::Block)
      return "Block";

    return "Function";
  }
  return "";
}

/// Attempt to omit needless words from the name of the given declaration.
static Optional<DeclName> omitNeedlessWords(AbstractFunctionDecl *afd) {
  auto &Context = afd->getASTContext();
  if (!Context.LangOpts.WarnOmitNeedlessWords)
    return None;

  if (afd->isInvalid())
    return None;

  DeclName name = afd->getFullName();
  if (!name)
    return None;

  // String'ify the arguments.
  StringRef baseNameStr = name.getBaseName().str();
  SmallVector<StringRef, 4> argNameStrs;
  for (auto arg : name.getArgumentNames()) {
    if (arg.empty())
      argNameStrs.push_back("");
    else
      argNameStrs.push_back(arg.str());
  }

  // String'ify the parameter types.
  SmallVector<OmissionTypeName, 4> paramTypes;
  Type functionType = afd->getInterfaceType();
  Type argumentType;
  for (unsigned i = 0, n = afd->getNaturalArgumentCount()-1; i != n; ++i)
    functionType = functionType->getAs<AnyFunctionType>()->getResult();
  argumentType = functionType->getAs<AnyFunctionType>()->getInput();
  if (auto tupleTy = argumentType->getAs<TupleType>()) {
    if (tupleTy->getNumElements() == argNameStrs.size()) {
      for (const auto &elt : tupleTy->getElements())
        paramTypes.push_back(getTypeNameForOmission(elt.getType())
                               .withDefaultArgument(elt.hasDefaultArg()));
    }
  }

  if (argNameStrs.size() == 1 && paramTypes.empty())
    paramTypes.push_back(getTypeNameForOmission(argumentType));

  // Handle contextual type, result type, and returnsSelf.
  Type contextType = afd->getDeclContext()->getDeclaredInterfaceType();
  Type resultType;
  bool returnsSelf = false;

  if (auto func = dyn_cast<FuncDecl>(afd)) {
    resultType = func->getResultType();
    returnsSelf = func->hasDynamicSelf();
  } else if (isa<ConstructorDecl>(afd)) {
    resultType = contextType;
    returnsSelf = true;
  }

  // Figure out the first parameter name.
  StringRef firstParamName;
  unsigned skipBodyPatterns = afd->getImplicitSelfDecl() ? 1 : 0;
  auto bodyPattern = afd->getBodyParamPatterns()[skipBodyPatterns];
  if (auto tuplePattern = dyn_cast<TuplePattern>(bodyPattern)) {
    if (tuplePattern->getNumElements() > 0) {
      auto firstParam = tuplePattern->getElement(0).getPattern();
      if (auto named = dyn_cast<NamedPattern>(
                         firstParam->getSemanticsProvidingPattern())) {
        if (!named->getBodyName().empty())
          firstParamName = named->getBodyName().str();
      }
    }
  }

  // Find the set of property names.
  const InheritedNameSet *allPropertyNames = nullptr;
  if (contextType) {
    if (auto classDecl = contextType->getClassOrBoundGenericClass()) {
      allPropertyNames = Context.getAllPropertyNames(classDecl,
                                                     afd->isInstanceMember());
    }
  }

  StringScratchSpace scratch;
  if (!swift::omitNeedlessWords(baseNameStr, argNameStrs, firstParamName,
                                getTypeNameForOmission(resultType),
                                getTypeNameForOmission(contextType),
                                paramTypes, returnsSelf, false,
                                allPropertyNames, scratch))
    return None;

  /// Retrieve a replacement identifier.
  auto getReplacementIdentifier = [&](StringRef name,
                                      Identifier old) -> Identifier{
    if (name.empty())
      return Identifier();

    if (!old.empty() && name == old.str())
      return old;

    return Context.getIdentifier(name);
  };

  Identifier newBaseName = getReplacementIdentifier(baseNameStr,
                                                    name.getBaseName());
  SmallVector<Identifier, 4> newArgNames;
  auto oldArgNames = name.getArgumentNames();
  for (unsigned i = 0, n = argNameStrs.size(); i != n; ++i) {
    newArgNames.push_back(getReplacementIdentifier(argNameStrs[i],
                                                   oldArgNames[i]));
  }

  return DeclName(Context, newBaseName, newArgNames);
}

/// Attempt to omit needless words from the name of the given declaration.
static Optional<Identifier> omitNeedlessWords(VarDecl *var) {
  auto &Context = var->getASTContext();

  if (var->isInvalid())
    return None;

  if (!Context.LangOpts.WarnOmitNeedlessWords)
    return None;

  if (var->getName().empty())
    return None;

  auto name = var->getName().str();

  // Dig out the context type.
  Type contextType = var->getDeclContext()->getDeclaredInterfaceType();
  if (!contextType)
    return None;

  // Dig out the type of the variable.
  Type type = var->getInterfaceType()->getReferenceStorageReferent()
  ->getLValueOrInOutObjectType();
  while (auto optObjectTy = type->getAnyOptionalObjectType())
    type = optObjectTy;

  // Find the set of property names.
  const InheritedNameSet *allPropertyNames = nullptr;
  if (contextType) {
    if (auto classDecl = contextType->getClassOrBoundGenericClass()) {
      allPropertyNames = Context.getAllPropertyNames(classDecl,
                                                     var->isInstanceMember());
    }
  }


  // Omit needless words.
  StringScratchSpace scratch;
  OmissionTypeName typeName = getTypeNameForOmission(var->getType());
  OmissionTypeName contextTypeName = getTypeNameForOmission(contextType);
  if (omitNeedlessWords(name, { }, "", typeName, contextTypeName, { },
                        /*returnsSelf=*/false, true, allPropertyNames,
                        scratch)) {
    return Context.getIdentifier(name);
  }

  return None;
}

void TypeChecker::checkOmitNeedlessWords(AbstractFunctionDecl *afd) {
  auto newName = ::omitNeedlessWords(afd);
  if (!newName)
    return;

  auto name = afd->getFullName();
  InFlightDiagnostic diag = diagnose(afd->getLoc(), diag::omit_needless_words,
                                     name, *newName);
  fixAbstractFunctionNames(diag, afd, *newName);
}

void TypeChecker::checkOmitNeedlessWords(VarDecl *var) {
  auto newName = ::omitNeedlessWords(var);
  if (!newName)
    return;

  auto name = var->getName();
  diagnose(var->getLoc(), diag::omit_needless_words, name, *newName)
    .fixItReplace(var->getLoc(), newName->str());
}

/// Determine the "fake" default argument provided by the given expression.
static Optional<StringRef> getDefaultArgForExpr(Expr *expr) {
  // Empty array literals, [].
  if (auto arrayExpr = dyn_cast<ArrayExpr>(expr)) {
    if (arrayExpr->getElements().empty())
      return StringRef("[]");

    return None;
  }

  // nil.
  if (auto call = dyn_cast<CallExpr>(expr)) {
    if (auto ctorRefCall = dyn_cast<ConstructorRefCallExpr>(call->getFn())) {
      if (auto ctorRef = dyn_cast<DeclRefExpr>(ctorRefCall->getFn())) {
        if (auto ctor = dyn_cast<ConstructorDecl>(ctorRef->getDecl())) {
          if (ctor->getFullName().getArgumentNames().size() == 1 &&
              ctor->getFullName().getArgumentNames()[0]
                == ctor->getASTContext().Id_nilLiteral)
            return StringRef("nil");
        }
      }
    }
  }

  return None;
}

namespace {
  struct CallEdit {
    enum {
      RemoveDefaultArg,
      Rename,
    } Kind;

    // The source range affected by this change.
    SourceRange Range;

    // The replacement text, for a rename.
    std::string Name;
  };

}

/// Find the source ranges of extraneous default arguments within a
/// call to the given function.
static bool hasExtraneousDefaultArguments(
              AbstractFunctionDecl *afd,
              Expr *arg,
              DeclName name,
              SmallVectorImpl<SourceRange> &ranges,
              SmallVectorImpl<unsigned> &removedArgs) {
  if (!afd->getClangDecl())
    return false;

  if (afd->isInvalid())
    return false;

  if (auto shuffle = dyn_cast<TupleShuffleExpr>(arg))
    arg = shuffle->getSubExpr();
    
  TupleExpr *argTuple = dyn_cast<TupleExpr>(arg);
  ParenExpr *argParen = dyn_cast<ParenExpr>(arg);

  ArrayRef<Pattern *> bodyPatterns = afd->getBodyParamPatterns();

  // Skip over the implicit 'self'.
  if (afd->getImplicitSelfDecl()) {
    bodyPatterns = bodyPatterns.slice(1);
  }
    
  ASTContext &ctx = afd->getASTContext();
  Pattern *bodyPattern = bodyPatterns[0];
  if (auto *tuple = dyn_cast<TuplePattern>(bodyPattern)) {
    Optional<unsigned> firstRemoved;
    Optional<unsigned> lastRemoved;
    unsigned numElementsInParens;
    if (argTuple) {
      numElementsInParens = (argTuple->getNumElements() -
                             argTuple->hasTrailingClosure());
    } else if (argParen) {
      numElementsInParens = 1 - argParen->hasTrailingClosure();
    } else {
      numElementsInParens = 0;
    }

    for (unsigned i = 0; i != numElementsInParens; ++i) {
      auto &elt = tuple->getElements()[i];
      if (elt.getDefaultArgKind() == DefaultArgumentKind::None)
        continue;

      auto defaultArg
        = elt.getPattern()->getType()->getInferredDefaultArgString();

      // Never consider removing the first argument for a "set" method
      // with an unnamed first argument.
      if (i == 0 &&
          !name.getBaseName().empty() &&
          camel_case::getFirstWord(name.getBaseName().str()) == "set" &&
          name.getArgumentNames().size() > 0 &&
          name.getArgumentNames()[0].empty())
        continue;

      SourceRange removalRange;
      if (argTuple && i < argTuple->getNumElements()) {
        // Check whether we have a default argument.
        auto exprArg = getDefaultArgForExpr(argTuple->getElement(i));
        if (!exprArg || defaultArg != *exprArg)
          continue;

        // Figure out where to start removing this argument.
        if (i == 0) {
          // Start removing right after the opening parenthesis.
          removalRange.Start = argTuple->getLParenLoc();
        } else {
          // Start removing right after the preceding argument, so we
          // consume the comma as well.
          removalRange.Start = argTuple->getElement(i-1)->getEndLoc();
        }

        // Adjust to the end of the starting token.
        removalRange.Start
          = Lexer::getLocForEndOfToken(ctx.SourceMgr, removalRange.Start);

        // Figure out where to finish removing this element.
        if (i == 0 && i < numElementsInParens - 1) {
          // We're the first of several arguments; consume the
          // following comma as well.
          removalRange.End = argTuple->getElementNameLoc(i+1);
          if (removalRange.End.isInvalid())
            removalRange.End = argTuple->getElement(i+1)->getStartLoc();
        } else if (i < numElementsInParens - 1) {
          // We're in the middle; consume through the end of this
          // element.
          removalRange.End
            = Lexer::getLocForEndOfToken(ctx.SourceMgr,
                                         argTuple->getElement(i)->getEndLoc());
        } else {
          // We're at the end; consume up to the closing parentheses.
          removalRange.End = argTuple->getRParenLoc();
        }
      } else if (argParen) {
        // Check whether we have a default argument.
        auto exprArg = getDefaultArgForExpr(argParen->getSubExpr());
        if (!exprArg || defaultArg != *exprArg)
          continue;

        removalRange = SourceRange(argParen->getSubExpr()->getStartLoc(),
                                   argParen->getRParenLoc());
      } else {
        continue;
      }

      if (removalRange.isInvalid())
        continue;

      // Note that we're removing this argument.
      removedArgs.push_back(i);

      // If we hadn't removed anything before, this is the first
      // removal.
      if (!firstRemoved) {
        ranges.push_back(removalRange);
        firstRemoved = i;
        lastRemoved = i;
        continue;
      }

      // If the previous removal range was the previous argument,
      // combine the ranges.
      if (*lastRemoved == i - 1) {
        ranges.back().End = removalRange.End;
        lastRemoved = i;
        continue;
      }

      // Otherwise, add this new removal range.
      ranges.push_back(removalRange);
      lastRemoved = i;
    }

    // If there is a single removal range that covers everything but
    // the trailing closure at the end, also zap the parentheses.
    if (ranges.size() == 1 &&
        *firstRemoved == 0 && *lastRemoved == tuple->getNumElements() - 2 &&
        argTuple && argTuple->hasTrailingClosure()) {
      ranges.front().Start = argTuple->getLParenLoc();
      ranges.front().End
        = Lexer::getLocForEndOfToken(ctx.SourceMgr, argTuple->getRParenLoc());
    }
  }

  return !ranges.empty();
}

void TypeChecker::checkOmitNeedlessWords(ApplyExpr *apply) {
  if (!Context.LangOpts.WarnOmitNeedlessWords)
    return;

  // Find the callee.
  ApplyExpr *innermostApply = apply;
  unsigned numApplications = 0;
  while (auto fnApply = dyn_cast<ApplyExpr>(
                          innermostApply->getFn()->getValueProvidingExpr())) {
    innermostApply = fnApply;
    ++numApplications;
  }
  if (numApplications != 1)
    return;

  DeclRefExpr *fnRef
    = dyn_cast<DeclRefExpr>(innermostApply->getFn()->getValueProvidingExpr());
  if (!fnRef)
    return;

  auto *afd = dyn_cast<AbstractFunctionDecl>(fnRef->getDecl());
  if (!afd)
    return;

  // Determine whether the callee has any needless words in it.
  auto newName = ::omitNeedlessWords(afd);

  bool renamed;
  if (!newName) {
    newName = afd->getFullName();
    renamed = false;
  } else {
    renamed = true;
  }

  // Determine whether there are any extraneous default arguments to be zapped.
  SmallVector<SourceRange, 2> removedDefaultArgRanges;
  SmallVector<unsigned, 2> removedArgs;
  bool anyExtraneousDefaultArgs
    = hasExtraneousDefaultArguments(afd, apply->getArg(), *newName,
                                    removedDefaultArgRanges,
                                    removedArgs);

  if (!renamed && !anyExtraneousDefaultArgs)
    return;

  // Make sure to apply the fix at the right application level.
  auto name = afd->getFullName();

  // Dig out the argument tuple.
  Expr *arg = apply->getArg();
  if (auto shuffle = dyn_cast<TupleShuffleExpr>(arg))
    arg = shuffle->getSubExpr();
  TupleExpr *argTuple = dyn_cast<TupleExpr>(arg);
  ParenExpr *argParen = dyn_cast<ParenExpr>(arg);

  if (argParen && !argTuple)
    arg = argParen->getSubExpr();

  InFlightDiagnostic diag
    = renamed ? diagnose(fnRef->getLoc(), diag::omit_needless_words,
                         name, *newName)
              : diagnose(fnRef->getLoc(), diag::extraneous_default_args_in_call,
                         name);

  // Fix the base name.
  if (newName->getBaseName() != name.getBaseName()) {
    diag.fixItReplace(fnRef->getLoc(), newName->getBaseName().str());
  }

  // Fix the argument names.
  auto oldArgNames = name.getArgumentNames();
  auto newArgNames = newName->getArgumentNames();
  unsigned currentRemovedArg = 0;
  if (argTuple) {
    for (unsigned i = 0, n = newArgNames.size(); i != n; ++i) {
      // If this argument was completely removed, don't emit any
      // Fix-Its for it.
      if (currentRemovedArg < removedArgs.size() &&
          removedArgs[currentRemovedArg] == i) {
        ++currentRemovedArg;
        continue;
      }

      // Check whether the name changed.
      auto newArgName = newArgNames[i];
      if (oldArgNames[i] == newArgName) continue;

      if (i >= argTuple->getNumElements()) break;
      if (argTuple->getElementName(i) != oldArgNames[i]) continue;

      auto nameLoc = argTuple->getElementNameLoc(i);
      if (nameLoc.isInvalid()) {
        // Add the argument label.
        diag.fixItInsert(argTuple->getElement(i)->getStartLoc(),
                         (newArgName.str() + ": ").str());
      } else if (newArgName.empty()) {
        // Delete the argument label.
        diag.fixItRemoveChars(nameLoc, argTuple->getElement(i)->getStartLoc());
      } else {
        // Fix the argument label.
        diag.fixItReplace(nameLoc, newArgName.str());
      }
    }
  } else if (newArgNames.size() > 0 && !newArgNames[0].empty() &&
             (!argParen || !argParen->hasTrailingClosure()) &&
             removedArgs.empty()) {
    // Add the argument label.
    auto newArgName = newArgNames[0];
    diag.fixItInsert(arg->getStartLoc(), (newArgName.str() + ": ").str());
  }

  // Remove all of the defaulted arguments.
  for (auto extraneous : removedDefaultArgRanges) {
    diag.fixItRemoveChars(extraneous.Start, extraneous.End);
  }
}

void TypeChecker::checkOmitNeedlessWords(MemberRefExpr *memberRef) {
  if (!Context.LangOpts.WarnOmitNeedlessWords)
    return;

  auto var = dyn_cast<VarDecl>(memberRef->getMember().getDecl());
  if (!var)
    return;

  // Check whether any needless words were omitted.
  auto newName = ::omitNeedlessWords(var);
  if (!newName)
    return;

  // Fix the name.
  auto name = var->getName();
  diagnose(memberRef->getNameLoc(), diag::omit_needless_words, name, *newName)
    .fixItReplace(memberRef->getNameLoc(), newName->str());
}
