//===--- GenDecl.cpp - IR Generation for Declarations ---------------------===//
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
//  This file implements IR generation for local and global
//  declarations in Swift.
//
//===----------------------------------------------------------------------===//

#include "swift/AST/ASTContext.h"
#include "swift/AST/Attr.h"
#include "swift/AST/Decl.h"
#include "swift/AST/DiagnosticEngine.h"
#include "swift/AST/DiagnosticsIRGen.h"
#include "swift/AST/IRGenOptions.h"
#include "swift/AST/Module.h"
#include "swift/AST/NameLookup.h"
#include "swift/AST/Pattern.h"
#include "swift/AST/TypeMemberVisitor.h"
#include "swift/AST/Types.h"
#include "swift/Basic/Fallthrough.h"
#include "swift/ClangImporter/ClangModule.h"
#include "swift/SIL/FormalLinkage.h"
#include "swift/SIL/SILDebugScope.h"
#include "swift/SIL/SILModule.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/TypeBuilder.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/GlobalAlias.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/ConvertUTF.h"

#include "CallingConvention.h"
#include "Explosion.h"
#include "FixedTypeInfo.h"
#include "GenClass.h"
#include "GenObjC.h"
#include "GenOpaque.h"
#include "GenMeta.h"
#include "GenType.h"
#include "IRGenDebugInfo.h"
#include "IRGenFunction.h"
#include "IRGenModule.h"
#include "Linking.h"

using namespace swift;
using namespace irgen;

static bool isTypeMetadataEmittedLazily(CanType type) {
  // All classes have eagerly-emitted metadata (at least for now).
  if (type.getClassOrBoundGenericClass()) return false;

  // Non-nominal metadata (e.g. for builtins) is provided by the runtime and
  // doesn't need lazy instantiation.
  auto nom = type->getAnyNominal();
  if (!nom)
    return false;

  switch (getDeclLinkage(nom)) {
  case FormalLinkage::PublicUnique:
  case FormalLinkage::HiddenUnique:
  case FormalLinkage::Private:
    // Maybe this *should* be lazy for private types?
    return false;
  case FormalLinkage::PublicNonUnique:
  case FormalLinkage::HiddenNonUnique:
    return true;
  }
  llvm_unreachable("bad formal linkage");
}

namespace {
  
/// Add methods, properties, and protocol conformances from a JITed extension
/// to an ObjC class using the ObjC runtime.
///
/// This must happen after ObjCProtocolInitializerVisitor if any @objc protocols
/// were defined in the TU.
class CategoryInitializerVisitor
  : public ClassMemberVisitor<CategoryInitializerVisitor>
{
  IRGenFunction &IGF;
  IRGenModule &IGM = IGF.IGM;
  IRBuilder &Builder = IGF.Builder;
  
  llvm::Constant *class_replaceMethod;
  llvm::Constant *class_addProtocol;
  
  llvm::Constant *classMetadata;
  llvm::Constant *metaclassMetadata;
  
public:
  CategoryInitializerVisitor(IRGenFunction &IGF, ExtensionDecl *ext)
    : IGF(IGF)
  {
    class_replaceMethod = IGM.getClassReplaceMethodFn();
    class_addProtocol = IGM.getClassAddProtocolFn();

    CanType origTy = ext->getDeclaredTypeOfContext()->getCanonicalType();
    classMetadata = tryEmitConstantHeapMetadataRef(IGM, origTy);
    assert(classMetadata &&
           "extended objc class doesn't have constant metadata?!");
    classMetadata = llvm::ConstantExpr::getBitCast(classMetadata,
                                                   IGM.ObjCClassPtrTy);
    metaclassMetadata = IGM.getAddrOfMetaclassObject(
                                       origTy.getClassOrBoundGenericClass(),
                                                         NotForDefinition);
    metaclassMetadata = llvm::ConstantExpr::getBitCast(metaclassMetadata,
                                                   IGM.ObjCClassPtrTy);

    // We need to make sure the Objective C runtime has initialized our
    // class. If you try to add or replace a method to a class that isn't
    // initialized yet, the Objective C runtime will crash in the calls
    // to class_replaceMethod or class_addProtocol.
    Builder.CreateCall(IGM.getGetInitializedObjCClassFn(), classMetadata);

    // Register ObjC protocol conformances.
    for (auto *p : ext->getLocalProtocols()) {
      if (!p->isObjC())
        continue;
      
      llvm::Value *protoRef = IGM.getAddrOfObjCProtocolRef(p, NotForDefinition);
      auto proto = Builder.CreateLoad(protoRef, IGM.getPointerAlignment());
      Builder.CreateCall(class_addProtocol, {classMetadata, proto});
    }
  }
  
  void visitMembers(ExtensionDecl *ext) {
    for (Decl *member : ext->getMembers())
      visit(member);
  }

  void visitTypeDecl(TypeDecl *type) {
    // We'll visit nested types separately if necessary.
  }
  
  void visitFuncDecl(FuncDecl *method) {
    if (!requiresObjCMethodDescriptor(method)) return;

    // Don't emit getters/setters for @NSManaged methods.
    if (method->getAttrs().hasAttribute<NSManagedAttr>())
      return;

    llvm::Constant *name, *imp, *types;
    emitObjCMethodDescriptorParts(IGM, method,
                                  /*extended*/false,
                                  /*concrete*/true,
                                  name, types, imp);
    
    // When generating JIT'd code, we need to call sel_registerName() to force
    // the runtime to unique the selector.
    llvm::Value *sel = Builder.CreateCall(IGM.getObjCSelRegisterNameFn(),
                                          name);
    
    llvm::Value *args[] = {
      method->isStatic() ? metaclassMetadata : classMetadata,
      sel,
      imp,
      types
    };
    
    Builder.CreateCall(class_replaceMethod, args);
  }

  // Can't be added in an extension.
  void visitDestructorDecl(DestructorDecl *dtor) {}

  void visitConstructorDecl(ConstructorDecl *constructor) {
    if (!requiresObjCMethodDescriptor(constructor)) return;
    llvm::Constant *name, *imp, *types;
    emitObjCMethodDescriptorParts(IGM, constructor, /*extended*/false,
                                  /*concrete*/true,
                                  name, types, imp);

    // When generating JIT'd code, we need to call sel_registerName() to force
    // the runtime to unique the selector.
    llvm::Value *sel = Builder.CreateCall(IGM.getObjCSelRegisterNameFn(),
                                          name);

    llvm::Value *args[] = {
      classMetadata,
      sel,
      imp,
      types
    };

    Builder.CreateCall(class_replaceMethod, args);
  }

  void visitPatternBindingDecl(PatternBindingDecl *binding) {
    // Ignore the PBD and just handle the individual vars.
  }
  
  void visitVarDecl(VarDecl *prop) {
    if (!requiresObjCPropertyDescriptor(IGM, prop)) return;

    // FIXME: register property metadata in addition to the methods.
    // ObjC doesn't have a notion of class properties, so we'd only do this
    // for instance properties.

    // Don't emit getters/setters for @NSManaged properties.
    if (prop->getAttrs().hasAttribute<NSManagedAttr>())
      return;

    llvm::Constant *name, *imp, *types;
    emitObjCGetterDescriptorParts(IGM, prop,
                                  name, types, imp);
    // When generating JIT'd code, we need to call sel_registerName() to force
    // the runtime to unique the selector.
    llvm::Value *sel = Builder.CreateCall(IGM.getObjCSelRegisterNameFn(),
                                          name);
    auto theClass = prop->isStatic() ? metaclassMetadata : classMetadata;
    llvm::Value *getterArgs[] = {theClass, sel, imp, types};
    Builder.CreateCall(class_replaceMethod, getterArgs);

    if (prop->isSettable(prop->getDeclContext())) {
      emitObjCSetterDescriptorParts(IGM, prop,
                                    name, types, imp);
      sel = Builder.CreateCall(IGM.getObjCSelRegisterNameFn(),
                               name);
      llvm::Value *setterArgs[] = {theClass, sel, imp, types};
      
      Builder.CreateCall(class_replaceMethod, setterArgs);
    }
  }

  void visitSubscriptDecl(SubscriptDecl *subscript) {
    assert(!subscript->isStatic() && "objc doesn't support class subscripts");
    if (!requiresObjCSubscriptDescriptor(IGM, subscript)) return;
    
    llvm::Constant *name, *imp, *types;
    emitObjCGetterDescriptorParts(IGM, subscript,
                                  name, types, imp);
    // When generating JIT'd code, we need to call sel_registerName() to force
    // the runtime to unique the selector.
    llvm::Value *sel = Builder.CreateCall(IGM.getObjCSelRegisterNameFn(),
                                          name);
    llvm::Value *getterArgs[] = {classMetadata, sel, imp, types};
    Builder.CreateCall(class_replaceMethod, getterArgs);

    if (subscript->isSettable()) {
      emitObjCSetterDescriptorParts(IGM, subscript,
                                    name, types, imp);
      sel = Builder.CreateCall(IGM.getObjCSelRegisterNameFn(),
                               name);
      llvm::Value *setterArgs[] = {classMetadata, sel, imp, types};
      
      Builder.CreateCall(class_replaceMethod, setterArgs);
    }
  }
};

/// Create a descriptor for JITed @objc protocol using the ObjC runtime.
class ObjCProtocolInitializerVisitor
  : public ClassMemberVisitor<ObjCProtocolInitializerVisitor>
{
  IRGenFunction &IGF;
  IRGenModule &IGM = IGF.IGM;
  IRBuilder &Builder = IGF.Builder;

  llvm::Constant *objc_getProtocol,
                 *objc_allocateProtocol,
                 *objc_registerProtocol,
                 *protocol_addMethodDescription,
                 *protocol_addProtocol;
  
  llvm::Value *NewProto = nullptr;
  
public:
  ObjCProtocolInitializerVisitor(IRGenFunction &IGF)
    : IGF(IGF)
  {
    objc_getProtocol = IGM.getGetObjCProtocolFn();
    objc_allocateProtocol = IGM.getAllocateObjCProtocolFn();
    objc_registerProtocol = IGM.getRegisterObjCProtocolFn();
    protocol_addMethodDescription = IGM.getProtocolAddMethodDescriptionFn();
    protocol_addProtocol = IGM.getProtocolAddProtocolFn();
  }
  
  void visitMembers(ProtocolDecl *proto) {
    // Check if the ObjC runtime already has a descriptor for this
    // protocol. If so, use it.
    SmallString<32> buf;
    auto protocolName
      = IGM.getAddrOfGlobalString(proto->getObjCRuntimeName(buf));
    
    auto existing = Builder.CreateCall(objc_getProtocol, protocolName);
    auto isNull = Builder.CreateICmpEQ(existing,
                   llvm::ConstantPointerNull::get(IGM.ProtocolDescriptorPtrTy));

    auto existingBB = IGF.createBasicBlock("existing_protocol");
    auto newBB = IGF.createBasicBlock("new_protocol");
    auto contBB = IGF.createBasicBlock("cont");
    Builder.CreateCondBr(isNull, newBB, existingBB);
    
    // Nothing to do if there's already a descriptor.
    Builder.emitBlock(existingBB);
    Builder.CreateBr(contBB);
    
    Builder.emitBlock(newBB);
    
    // Allocate the protocol descriptor.
    NewProto = Builder.CreateCall(objc_allocateProtocol, protocolName);
    
    // Add the parent protocols.
    for (auto inherited : proto->getInherited()) {
      SmallVector<ProtocolDecl*, 4> protocols;
      if (!inherited.getType()->isAnyExistentialType(protocols))
        continue;
      for (auto parentProto : protocols) {
        if (!parentProto->isObjC())
          continue;
        llvm::Value *parentRef
          = IGM.getAddrOfObjCProtocolRef(parentProto, NotForDefinition);
        parentRef = IGF.Builder.CreateBitCast(parentRef,
                                   IGM.ProtocolDescriptorPtrTy->getPointerTo());
        auto parent = Builder.CreateLoad(parentRef,
                                             IGM.getPointerAlignment());
        Builder.CreateCall(protocol_addProtocol, {NewProto, parent});
      }
    }
    
    // Add the members.
    for (Decl *member : proto->getMembers())
      visit(member);
    
    // Register it.
    Builder.CreateCall(objc_registerProtocol, NewProto);
    Builder.CreateBr(contBB);
    
    // Store the reference to the runtime's idea of the protocol descriptor.
    Builder.emitBlock(contBB);
    auto result = Builder.CreatePHI(IGM.ProtocolDescriptorPtrTy, 2);
    result->addIncoming(existing, existingBB);
    result->addIncoming(NewProto, newBB);
    
    llvm::Value *ref = IGM.getAddrOfObjCProtocolRef(proto, NotForDefinition);
    ref = IGF.Builder.CreateBitCast(ref,
                                  IGM.ProtocolDescriptorPtrTy->getPointerTo());

    Builder.CreateStore(result, ref, IGM.getPointerAlignment());
  }

  void visitTypeDecl(TypeDecl *type) {
    // We'll visit nested types separately if necessary.
  }

  void visitAbstractFunctionDecl(AbstractFunctionDecl *method) {
    llvm::Constant *name, *imp, *types;
    emitObjCMethodDescriptorParts(IGM, method, /*extended*/true,
                                  /*concrete*/false,
                                  name, types, imp);
    
    // When generating JIT'd code, we need to call sel_registerName() to force
    // the runtime to unique the selector.
    llvm::Value *sel = Builder.CreateCall(IGM.getObjCSelRegisterNameFn(), name);

    llvm::Value *args[] = {
      NewProto, sel, types,
      // required?
      llvm::ConstantInt::get(IGM.ObjCBoolTy,
                             !method->getAttrs().hasAttribute<OptionalAttr>()),
      // instance?
      llvm::ConstantInt::get(IGM.ObjCBoolTy,
                   isa<ConstructorDecl>(method) || method->isInstanceMember()),
    };
    
    Builder.CreateCall(protocol_addMethodDescription, args);
  }
  
  void visitPatternBindingDecl(PatternBindingDecl *binding) {
    // Ignore the PBD and just handle the individual vars.
  }
  
  void visitAbstractStorageDecl(AbstractStorageDecl *prop) {
    // TODO: Add properties to protocol.
    
    llvm::Constant *name, *imp, *types;
    emitObjCGetterDescriptorParts(IGM, prop,
                                  name, types, imp);
    // When generating JIT'd code, we need to call sel_registerName() to force
    // the runtime to unique the selector.
    llvm::Value *sel = Builder.CreateCall(IGM.getObjCSelRegisterNameFn(), name);
    llvm::Value *getterArgs[] = {
      NewProto, sel, types,
      // required?
      llvm::ConstantInt::get(IGM.ObjCBoolTy,
                             !prop->getAttrs().hasAttribute<OptionalAttr>()),
      // instance?
      llvm::ConstantInt::get(IGM.ObjCBoolTy,
                             prop->isInstanceMember()),
    };
    Builder.CreateCall(protocol_addMethodDescription, getterArgs);
    
    if (prop->isSettable(nullptr)) {
      emitObjCSetterDescriptorParts(IGM, prop, name, types, imp);
      sel = Builder.CreateCall(IGM.getObjCSelRegisterNameFn(), name);
      llvm::Value *setterArgs[] = {
        NewProto, sel, types,
        // required?
        llvm::ConstantInt::get(IGM.ObjCBoolTy,
                               !prop->getAttrs().hasAttribute<OptionalAttr>()),
        // instance?
        llvm::ConstantInt::get(IGM.ObjCBoolTy,
                               prop->isInstanceMember()),
      };
      Builder.CreateCall(protocol_addMethodDescription, setterArgs);
    }
  }
};

} // end anonymous namespace

namespace {
class PrettySourceFileEmission : public llvm::PrettyStackTraceEntry {
  const SourceFile &SF;
public:
  explicit PrettySourceFileEmission(const SourceFile &SF) : SF(SF) {}

  virtual void print(raw_ostream &os) const override {
    os << "While emitting IR for source file " << SF.getFilename() << '\n';
  }
};
} // end anonymous namespace

/// Emit all the top-level code in the source file.
void IRGenModule::emitSourceFile(SourceFile &SF, unsigned StartElem) {
  PrettySourceFileEmission StackEntry(SF);

  // Emit types and other global decls.
  for (unsigned i = StartElem, e = SF.Decls.size(); i != e; ++i)
    emitGlobalDecl(SF.Decls[i]);
  for (auto *localDecl : SF.LocalTypeDecls)
    emitGlobalDecl(localDecl);

  SF.forAllVisibleModules([&](swift::Module::ImportedModule import) {
    swift::Module *next = import.second;
    if (next->getName() == SF.getParentModule()->getName())
      return;

    next->collectLinkLibraries([this](LinkLibrary linkLib) {
      this->addLinkLibrary(linkLib);
    });
  });

  if (ObjCInterop)
    this->addLinkLibrary(LinkLibrary("objc", LibraryKind::Library));
}

/// Emit a global list, i.e. a global constant array holding all of a
/// list of values.  Generally these lists are for various LLVM
/// metadata or runtime purposes.
static llvm::GlobalVariable *
emitGlobalList(IRGenModule &IGM, ArrayRef<llvm::WeakVH> handles,
                           StringRef name, StringRef section,
                           llvm::GlobalValue::LinkageTypes linkage,
                           llvm::Type *eltTy,
                           bool isConstant) {
  // Do nothing if the list is empty.
  if (handles.empty()) return nullptr;

  // For global lists that actually get linked (as opposed to notional
  // ones like @llvm.used), it's important to set an explicit alignment
  // so that the linker doesn't accidentally put padding in the list.
  Alignment alignment = IGM.getPointerAlignment();

  // We have an array of value handles, but we need an array of constants.
  SmallVector<llvm::Constant*, 8> elts;
  elts.reserve(handles.size());
  for (auto &handle : handles) {
    auto elt = cast<llvm::Constant>(&*handle);
    if (elt->getType() != eltTy)
      elt = llvm::ConstantExpr::getBitCast(elt, eltTy);
    elts.push_back(elt);
  }

  auto varTy = llvm::ArrayType::get(eltTy, elts.size());
  auto init = llvm::ConstantArray::get(varTy, elts);
  auto var = new llvm::GlobalVariable(IGM.Module, varTy, isConstant, linkage,
                                      init, name);
  var->setSection(section);
  var->setAlignment(alignment.getValue());

  // Mark the variable as used if doesn't have external linkage.
  // (Note that we'd specifically like to not put @llvm.used in itself.)
  if (llvm::GlobalValue::isLocalLinkage(linkage))
    IGM.addUsedGlobal(var);
  return var;
}

void IRGenModule::emitRuntimeRegistration() {
  // Duck out early if we have nothing to register.
  if (ProtocolConformances.empty()
      && (!ObjCInterop || (ObjCProtocols.empty() &&
                           ObjCClasses.empty() &&
                           ObjCCategoryDecls.empty())))
    return;
  
  // Find the entry point.
  SILFunction *EntryPoint = SILMod->lookUpFunction(SWIFT_ENTRY_POINT_FUNCTION);
  
  // If we're debugging, we probably don't have a main. Find a
  // function marked with the LLDBDebuggerFunction attribute instead.
  if (!EntryPoint && Context.LangOpts.DebuggerSupport) {
    for (SILFunction &SF : *SILMod) {
      if (SF.hasLocation()) {
        if (Decl* D = SF.getLocation().getAsASTNode<Decl>()) {
          if (FuncDecl *FD = dyn_cast<FuncDecl>(D)) {
            if (FD->getAttrs().hasAttribute<LLDBDebuggerFunctionAttr>()) {
              EntryPoint = &SF;
              break;
            }
          }
        }
      }
    }
  }
  
  if (!EntryPoint)
    return;
    
  llvm::Function *EntryFunction = Module.getFunction(EntryPoint->getName());
  if (!EntryFunction)
    return;
  
  // Create a new function to contain our logic.
  auto fnTy = llvm::FunctionType::get(VoidTy, /*varArg*/ false);
  auto RegistrationFunction = llvm::Function::Create(fnTy,
                                           llvm::GlobalValue::PrivateLinkage,
                                           "runtime_registration",
                                           getModule());
  RegistrationFunction->setAttributes(constructInitialAttributes());
  
  // Insert a call into the entry function.
  {
    llvm::BasicBlock *EntryBB = &EntryFunction->getEntryBlock();
    llvm::BasicBlock::iterator IP = EntryBB->getFirstInsertionPt();
    IRBuilder Builder(getLLVMContext());
    Builder.llvm::IRBuilderBase::SetInsertPoint(EntryBB, IP);
    Builder.CreateCall(RegistrationFunction, {});
  }
  
  IRGenFunction RegIGF(*this, RegistrationFunction);
  
  // Register ObjC protocols, classes, and extensions we added.
  if (ObjCInterop) {
    if (!ObjCProtocols.empty()) {
      // We need to initialize ObjC protocols in inheritance order, parents
      // first.
      
      llvm::DenseSet<ProtocolDecl*> protos;
      for (auto &proto : ObjCProtocols)
        protos.insert(proto.first);
      
      llvm::SmallVector<ProtocolDecl*, 4> protoInitOrder;
      
      std::function<void(ProtocolDecl*)> orderProtocol
        = [&](ProtocolDecl *proto) {
          // Recursively put parents first.
          for (auto &inherited : proto->getInherited()) {
            SmallVector<ProtocolDecl*, 4> parents;
            if (!inherited.getType()->isAnyExistentialType(parents))
              continue;
            for (auto parent : parents)
              orderProtocol(parent);
          }
          // Skip if we don't need to reify this protocol.
          auto found = protos.find(proto);
          if (found == protos.end())
            return;
          protos.erase(found);
          protoInitOrder.push_back(proto);
        };
      
      while (!protos.empty()) {
        orderProtocol(*protos.begin());
      }

      // Visit the protocols in the order we established.
      for (auto *proto : protoInitOrder) {
        ObjCProtocolInitializerVisitor(RegIGF)
          .visitMembers(proto);
      }
    }
    
    for (llvm::WeakVH &ObjCClass : ObjCClasses) {
      RegIGF.Builder.CreateCall(getInstantiateObjCClassFn(), {ObjCClass});
    }
      
    for (ExtensionDecl *ext : ObjCCategoryDecls) {
      CategoryInitializerVisitor(RegIGF, ext).visitMembers(ext);
    }
  }
  // Register Swift protocol conformances if we added any.
  if (!ProtocolConformances.empty()) {

    llvm::Constant *conformances = emitProtocolConformances();

    llvm::Constant *beginIndices[] = {
      llvm::ConstantInt::get(Int32Ty, 0),
      llvm::ConstantInt::get(Int32Ty, 0),
    };
    auto begin = llvm::ConstantExpr::getGetElementPtr(
        /*Ty=*/nullptr, conformances, beginIndices);
    llvm::Constant *endIndices[] = {
      llvm::ConstantInt::get(Int32Ty, 0),
      llvm::ConstantInt::get(Int32Ty, ProtocolConformances.size()),
    };
    auto end = llvm::ConstantExpr::getGetElementPtr(
        /*Ty=*/nullptr, conformances, endIndices);
    
    RegIGF.Builder.CreateCall(getRegisterProtocolConformancesFn(), {begin, end});
  }
  RegIGF.Builder.CreateRetVoid();
}

/// Add the given global value to @llvm.used.
///
/// This value must have a definition by the time the module is finalized.
void IRGenModule::addUsedGlobal(llvm::GlobalValue *global) {
  LLVMUsed.push_back(global);
}

/// Add the given global value to the Objective-C class list.
void IRGenModule::addObjCClass(llvm::Constant *classPtr, bool nonlazy) {
  ObjCClasses.push_back(classPtr);
  if (nonlazy)
    ObjCNonLazyClasses.push_back(classPtr);
}

/// Add the given protocol conformance to the list of conformances for which
/// runtime records will be emitted in this translation unit.
void IRGenModule::addProtocolConformanceRecord(
                                       NormalProtocolConformance *conformance) {
  ProtocolConformances.push_back(conformance);
}

void IRGenModule::emitGlobalLists() {
  if (ObjCInterop) {
    assert(TargetInfo.OutputObjectFormat == llvm::Triple::MachO);
    // Objective-C class references go in a variable with a meaningless
    // name but a magic section.
    emitGlobalList(*this, ObjCClasses, "objc_classes",
                   "__DATA, __objc_classlist, regular, no_dead_strip",
                   llvm::GlobalValue::InternalLinkage,
                   Int8PtrTy,
                   false);
    // So do categories.
    emitGlobalList(*this, ObjCCategories, "objc_categories",
                   "__DATA, __objc_catlist, regular, no_dead_strip",
                   llvm::GlobalValue::InternalLinkage,
                   Int8PtrTy,
                   false);

    // Emit nonlazily realized class references in a second magic section to make
    // sure they are realized by the Objective-C runtime before any instances
    // are allocated.
    emitGlobalList(*this, ObjCNonLazyClasses, "objc_non_lazy_classes",
                   "__DATA, __objc_nlclslist, regular, no_dead_strip",
                   llvm::GlobalValue::InternalLinkage,
                   Int8PtrTy,
                   false);
  }

  // @llvm.used

  // Collect llvm.used globals already in the module (coming from ClangCodeGen).
  auto *ExistingLLVMUsed = Module.getGlobalVariable("llvm.used");
  if (ExistingLLVMUsed) {
    auto *Globals =
        cast<llvm::ConstantArray>(ExistingLLVMUsed->getInitializer());
    for (auto &Use : Globals->operands()) {
      auto *Global = Use.get();
      LLVMUsed.push_back(Global);
    }
    ExistingLLVMUsed->eraseFromParent();
  }

  assert(std::all_of(LLVMUsed.begin(), LLVMUsed.end(),
                     [](const llvm::WeakVH &global) {
    return !isa<llvm::GlobalValue>(global) ||
           !cast<llvm::GlobalValue>(global)->isDeclaration();
  }) && "all globals in the 'used' list must be definitions");
  emitGlobalList(*this, LLVMUsed, "llvm.used", "llvm.metadata",
                 llvm::GlobalValue::AppendingLinkage,
                 Int8PtrTy,
                 false);
}

void IRGenModuleDispatcher::emitGlobalTopLevel() {
  // Generate order numbers for the functions in the SIL module that
  // correspond to definitions in the LLVM module.
  unsigned nextOrderNumber = 0;
  for (auto &silFn : PrimaryIGM->SILMod->getFunctions()) {
    // Don't bother adding external declarations to the function order.
    if (!silFn.isDefinition()) continue;
    FunctionOrder.insert(std::make_pair(&silFn, nextOrderNumber++));
  }

  for (SILGlobalVariable &v : PrimaryIGM->SILMod->getSILGlobals()) {
    Decl *decl = v.getDecl();
    CurrentIGMPtr IGM = getGenModule(decl ? decl->getDeclContext() : nullptr);
    IGM->emitSILGlobalVariable(&v);
  }
  PrimaryIGM->emitCoverageMapping();
  
  // Emit SIL functions.
  bool isWholeModule = PrimaryIGM->SILMod->isWholeModule();
  for (SILFunction &f : *PrimaryIGM->SILMod) {
    // Only eagerly emit functions that are externally visible.
    if (!isPossiblyUsedExternally(f.getLinkage(), isWholeModule))
      continue;

    CurrentIGMPtr IGM = getGenModule(&f);
    IGM->emitSILFunction(&f);
  }

  // Emit static initializers.
  for (auto Iter : *this) {
    IRGenModule *IGM = Iter.second;
    IGM->emitSILStaticInitializer();
  }

  // Emit witness tables.
  for (SILWitnessTable &wt : PrimaryIGM->SILMod->getWitnessTableList()) {
    CurrentIGMPtr IGM = getGenModule(wt.getConformance()->getDeclContext());
    IGM->emitSILWitnessTable(&wt);
  }
  
  for (auto Iter : *this) {
    IRGenModule *IGM = Iter.second;
    IGM->finishEmitAfterTopLevel();
  }
}

void IRGenModule::finishEmitAfterTopLevel() {
  // Emit the implicit import of the swift standard libary.
  if (DebugInfo) {
    std::pair<swift::Identifier, swift::SourceLoc> AccessPath[] = {
      { Context.StdlibModuleName, swift::SourceLoc() }
    };

    auto Imp = ImportDecl::create(Context,
                                  SILMod->getSwiftModule(),
                                  SourceLoc(),
                                  ImportKind::Module, SourceLoc(),
                                  AccessPath);
    DebugInfo->emitImport(Imp);
  }

  // Emit external definitions used by this module.
  for (auto def : Context.ExternalDefinitions) {
    emitExternalDefinition(def);
  }

  // Let ClangCodeGen emit its global data structures (llvm.used, debug info,
  // etc.)
  finalizeClangCodeGen();
}

static void emitLazyTypeMetadata(IRGenModule &IGM, CanType type) {
  auto decl = type.getAnyNominal();
  assert(decl);

  if (auto sd = dyn_cast<StructDecl>(decl)) {
    return emitStructMetadata(IGM, sd);
  } else if (auto ed = dyn_cast<EnumDecl>(decl)) {
    emitEnumMetadata(IGM, ed);
  } else if (auto pd = dyn_cast<ProtocolDecl>(decl)) {
    IGM.emitProtocolDecl(pd);
  } else {
    llvm_unreachable("should not have enqueued a class decl here!");
  }
}

void IRGenModuleDispatcher::emitProtocolConformances() {
  for (auto &m : *this) {
    m.second->emitProtocolConformances();
  }
}

/// Emit any lazy definitions (of globals or functions or whatever
/// else) that we require.
void IRGenModuleDispatcher::emitLazyDefinitions() {
  while (!LazyTypeMetadata.empty() ||
         !LazyFunctionDefinitions.empty() ||
         !LazyFieldTypeAccessors.empty()) {

    // Emit any lazy type metadata we require.
    while (!LazyTypeMetadata.empty()) {
      CanType type = LazyTypeMetadata.pop_back_val();
      assert(isTypeMetadataEmittedLazily(type));
      auto nom = type->getAnyNominal();
      CurrentIGMPtr IGM = getGenModule(nom->getDeclContext());
      emitLazyTypeMetadata(*IGM.get(), type);
    }
    while (!LazyFieldTypeAccessors.empty()) {
      auto accessor = LazyFieldTypeAccessors.pop_back_val();
      emitFieldTypeAccessor(*accessor.IGM, accessor.type, accessor.fn,
                            accessor.fieldTypes);
    }

    // Emit any lazy function definitions we require.
    while (!LazyFunctionDefinitions.empty()) {
      SILFunction *f = LazyFunctionDefinitions.pop_back_val();
      CurrentIGMPtr IGM = getGenModule(f);
      assert(!isPossiblyUsedExternally(f->getLinkage(),
                                       IGM->SILMod->isWholeModule())
             && "function with externally-visible linkage emitted lazily?");
      IGM->emitSILFunction(f);
    }
  }
}

/// Emit symbols for eliminated dead methods, which can still be referenced
/// from other modules. This happens e.g. if a public class contains a (dead)
/// private method.
void IRGenModule::emitVTableStubs() {
  llvm::Function *stub = nullptr;
  for (auto I = SILMod->zombies_begin(); I != SILMod->zombies_end(); ++I) {
    const SILFunction &F = *I;
    if (! F.isExternallyUsedSymbol())
      continue;
    
    if (!stub) {
      // Create a single stub function which calls swift_reportMissingMethod().
      stub = llvm::Function::Create(llvm::FunctionType::get(VoidTy, false),
                                    llvm::GlobalValue::LinkOnceODRLinkage,
                                    "_swift_dead_method_stub");
      stub->setAttributes(constructInitialAttributes());
      Module.getFunctionList().push_back(stub);
      stub->setVisibility(llvm::GlobalValue::HiddenVisibility);
      stub->setCallingConv(RuntimeCC);
      auto *entry = llvm::BasicBlock::Create(getLLVMContext(), "entry", stub);
      auto *errorFunc = getDeadMethodErrorFn();
      llvm::CallInst::Create(errorFunc, ArrayRef<llvm::Value *>(), "", entry);
      new llvm::UnreachableInst(getLLVMContext(), entry);
    }
    // For each eliminated method symbol create an alias to the stub.
    llvm::GlobalAlias::create(llvm::GlobalValue::ExternalLinkage, F.getName(),
                              stub);
  }
}

void IRGenModule::emitTypeVerifier() {
  // Look up the types to verify.
  
  SmallVector<CanType, 4> TypesToVerify;
  for (auto name : Opts.VerifyTypeLayoutNames) {
    // Look up the name in the module.
    SmallVector<ValueDecl*, 1> lookup;
    swift::Module *M = SILMod->getSwiftModule();
    M->lookupMember(lookup, M, DeclName(Context.getIdentifier(name)),
                    Identifier());
    if (lookup.empty()) {
      Context.Diags.diagnose(SourceLoc(), diag::type_to_verify_not_found,
                             name);
      continue;
    }
    
    TypeDecl *typeDecl = nullptr;
    for (auto decl : lookup) {
      if (auto td = dyn_cast<TypeDecl>(decl)) {
        if (typeDecl) {
          Context.Diags.diagnose(SourceLoc(), diag::type_to_verify_ambiguous,
                                 name);
          goto next;
        }
        typeDecl = td;
        break;
      }
    }
    if (!typeDecl) {
      Context.Diags.diagnose(SourceLoc(), diag::type_to_verify_not_found, name);
      continue;
    }
    
    {
      auto type = typeDecl->getDeclaredInterfaceType();
      if (type->hasTypeParameter()) {
        Context.Diags.diagnose(SourceLoc(), diag::type_to_verify_dependent,
                               name);
        continue;
      }
      
      TypesToVerify.push_back(type->getCanonicalType());
    }
  next:;
  }
  if (TypesToVerify.empty())
    return;

  // Find the entry point.
  SILFunction *EntryPoint = SILMod->lookUpFunction(SWIFT_ENTRY_POINT_FUNCTION);

  if (!EntryPoint)
    return;
  
  llvm::Function *EntryFunction = Module.getFunction(EntryPoint->getName());
  if (!EntryFunction)
    return;
  
  // Create a new function to contain our logic.
  auto fnTy = llvm::FunctionType::get(VoidTy, /*varArg*/ false);
  auto VerifierFunction = llvm::Function::Create(fnTy,
                                             llvm::GlobalValue::PrivateLinkage,
                                             "type_verifier",
                                             getModule());
  VerifierFunction->setAttributes(constructInitialAttributes());
  
  // Insert a call into the entry function.
  {
    llvm::BasicBlock *EntryBB = &EntryFunction->getEntryBlock();
    llvm::BasicBlock::iterator IP = EntryBB->getFirstInsertionPt();
    IRBuilder Builder(getLLVMContext());
    Builder.llvm::IRBuilderBase::SetInsertPoint(EntryBB, IP);
    Builder.CreateCall(VerifierFunction, {});
  }

  IRGenFunction VerifierIGF(*this, VerifierFunction);
  emitTypeLayoutVerifier(VerifierIGF, TypesToVerify);
  VerifierIGF.Builder.CreateRetVoid();
}

/// Get SIL-linkage for something that's not required to be visible
/// and doesn't actually need to be uniqued.
static SILLinkage getNonUniqueSILLinkage(FormalLinkage linkage,
                                         ForDefinition_t forDefinition) {
  switch (linkage) {
  case FormalLinkage::PublicUnique:
  case FormalLinkage::PublicNonUnique:
    return (forDefinition ? SILLinkage::Shared : SILLinkage::PublicExternal);

  case FormalLinkage::HiddenUnique:
  case FormalLinkage::HiddenNonUnique:
    return (forDefinition ? SILLinkage::Shared : SILLinkage::HiddenExternal);

  case FormalLinkage::Private:
    return SILLinkage::Private;
  }
  llvm_unreachable("bad formal linkage");
}

static SILLinkage getConformanceLinkage(IRGenModule &IGM,
                                        const ProtocolConformance *conf) {
  auto wt = IGM.SILMod->lookUpWitnessTable(conf);
  if (wt.first) {
    return wt.first->getLinkage();
  } else {
    return SILLinkage::PublicExternal;
  }
}

SILLinkage LinkEntity::getLinkage(IRGenModule &IGM,
                                  ForDefinition_t forDefinition) const {
  switch (getKind()) {
  // Most type metadata depend on the formal linkage of their type.
  case Kind::ValueWitnessTable:
  case Kind::TypeMangling:
    return getSILLinkage(getTypeLinkage(getType()), forDefinition);

  case Kind::TypeMetadata:
    switch (getMetadataAddress()) {
    case TypeMetadataAddress::FullMetadata:
      // The full metadata object is private to the containing module.
      return SILLinkage::Private;
    case TypeMetadataAddress::AddressPoint:
      return getSILLinkage(getTypeLinkage(getType()), forDefinition);
    }

  // ...but we don't actually expose individual value witnesses (right now).
  case Kind::ValueWitness:
    return getNonUniqueSILLinkage(getTypeLinkage(getType()), forDefinition);

  // Foreign type metadata candidates are always shared; the runtime
  // does the uniquing.
  case Kind::ForeignTypeMetadataCandidate:
    return SILLinkage::Shared;

  case Kind::TypeMetadataAccessFunction:
  case Kind::TypeMetadataLazyCacheVariable:
    switch (getTypeMetadataAccessStrategy(IGM, getType(),
                                          /*preferDirectAccess=*/false)) {
    case MetadataAccessStrategy::PublicUniqueAccessor:
      return getSILLinkage(FormalLinkage::PublicUnique, forDefinition);
    case MetadataAccessStrategy::HiddenUniqueAccessor:
      return getSILLinkage(FormalLinkage::HiddenUnique, forDefinition);
    case MetadataAccessStrategy::PrivateAccessor:
      return getSILLinkage(FormalLinkage::Private, forDefinition);
    case MetadataAccessStrategy::NonUniqueAccessor:
      return SILLinkage::Shared;
    case MetadataAccessStrategy::Direct:
      llvm_unreachable("metadata accessor for type with direct access?");
    }
    llvm_unreachable("bad metadata access kind");

  case Kind::WitnessTableOffset:
  case Kind::Function:
  case Kind::Other:
  case Kind::ObjCClass:
  case Kind::ObjCMetaclass:
  case Kind::SwiftMetaclassStub:
  case Kind::FieldOffset:
  case Kind::NominalTypeDescriptor:
  case Kind::ProtocolDescriptor:
    return getSILLinkage(getDeclLinkage(getDecl()), forDefinition);

  case Kind::DirectProtocolWitnessTable:
  case Kind::ProtocolWitnessTableAccessFunction:
  case Kind::DependentProtocolWitnessTableGenerator:
    return getConformanceLinkage(IGM, getProtocolConformance());

  case Kind::ProtocolWitnessTableLazyAccessFunction:
  case Kind::ProtocolWitnessTableLazyCacheVariable:
    if (getTypeLinkage(getType()) == FormalLinkage::Private ||
        getConformanceLinkage(IGM, getProtocolConformance())
          == SILLinkage::Private) {
      return SILLinkage::Private;
    } else {
      return SILLinkage::Shared;
    }

  case Kind::DependentProtocolWitnessTableTemplate:
    return SILLinkage::Private;
  
  case Kind::SILFunction:
    return getSILFunction()->getEffectiveSymbolLinkage();
      
  case Kind::SILGlobalVariable:
    return getSILGlobalVariable()->getLinkage();
  }
  llvm_unreachable("bad link entity kind");
}

bool LinkEntity::isFragile(IRGenModule &IGM) const {
  switch (getKind()) {
    case Kind::SILFunction:
      return getSILFunction()->isFragile();
      
    case Kind::SILGlobalVariable:
      return getSILGlobalVariable()->isFragile();
      
    case Kind::DirectProtocolWitnessTable:
    case Kind::DependentProtocolWitnessTableGenerator: {
      auto wt = IGM.SILMod->lookUpWitnessTable(getProtocolConformance());
      if (wt.first) {
        return wt.first->isFragile();
      } else {
        return false;
      }
    }
      
    default:
      break;
  }
  return false;
}


static std::pair<llvm::GlobalValue::LinkageTypes,
                 llvm::GlobalValue::VisibilityTypes>
getIRLinkage(IRGenModule &IGM,
             SILLinkage linkage, bool isFragile, ForDefinition_t isDefinition,
             bool isWeakImported) {
  
#define RESULT(LINKAGE, VISIBILITY)        \
{ llvm::GlobalValue::LINKAGE##Linkage, \
llvm::GlobalValue::VISIBILITY##Visibility }
  
  if (isFragile) {
    // Fragile functions/globals must be visible from outside, regardless of
    // their accessibility. If a caller is also fragile and inlined into another
    // module it must be able to access this (not-inlined) function/global.
    switch (linkage) {
      case SILLinkage::Hidden:
      case SILLinkage::Private:
        linkage = SILLinkage::Public;
        break;

      case SILLinkage::Public:
      case SILLinkage::Shared:
      case SILLinkage::HiddenExternal:
      case SILLinkage::PrivateExternal:
      case SILLinkage::PublicExternal:
      case SILLinkage::SharedExternal:
        break;
    }
  }
  
  switch (linkage) {
  case SILLinkage::Public: return RESULT(External, Default);
  case SILLinkage::Shared:
  case SILLinkage::SharedExternal: return RESULT(LinkOnceODR, Hidden);
  case SILLinkage::Hidden: return RESULT(External, Hidden);
  case SILLinkage::Private:
    if (IGM.dispatcher.hasMultipleIGMs()) {
      // In case of multiple llvm modules (in multi-threaded compilation) all
      // private decls must be visible from other files.
      return RESULT(External, Hidden);
    }
    return RESULT(Internal, Default);
  case SILLinkage::PublicExternal:
    if (isDefinition) {
      return RESULT(AvailableExternally, Default);
    }

    if (isWeakImported)
      return RESULT(ExternalWeak, Default);
    return RESULT(External, Default);
  case SILLinkage::HiddenExternal:
  case SILLinkage::PrivateExternal: {
    auto visibility = isFragile ? llvm::GlobalValue::DefaultVisibility
                                : llvm::GlobalValue::HiddenVisibility;
    if (isDefinition) {
      return {llvm::GlobalValue::AvailableExternallyLinkage, visibility};
    }
    return {llvm::GlobalValue::ExternalLinkage, visibility};
  }
  }
  llvm_unreachable("bad SIL linkage");
}

/// Given that we're going to define a global value but already have a
/// forward-declaration of it, update its linkage.
static void updateLinkageForDefinition(IRGenModule &IGM,
                                       llvm::GlobalValue *global,
                                       const LinkEntity &entity) {
  // TODO: there are probably cases where we can avoid redoing the
  // entire linkage computation.
  auto linkage = getIRLinkage(
                   IGM,
                   entity.getLinkage(IGM, ForDefinition),
                   entity.isFragile(IGM),
                   ForDefinition,
                   entity.isWeakImported(IGM.SILMod->getSwiftModule()));
  global->setLinkage(linkage.first);
  global->setVisibility(linkage.second);

  // Everything externally visible is considered used in Swift.
  // That mostly means we need to be good at not marking things external.
  //
  // Exclude "main", because it should naturally be used, and because adding it
  // to llvm.used leaves a dangling use when the REPL attempts to discard
  // intermediate mains.
  if (linkage.first == llvm::GlobalValue::ExternalLinkage &&
      linkage.second == llvm::GlobalValue::DefaultVisibility &&
      global->getName() != SWIFT_ENTRY_POINT_FUNCTION) {
    IGM.addUsedGlobal(global);
  }
}

LinkInfo LinkInfo::get(IRGenModule &IGM, const LinkEntity &entity,
                       ForDefinition_t isDefinition) {
  LinkInfo result;

  entity.mangle(result.Name);

  std::tie(result.Linkage, result.Visibility) =
    getIRLinkage(IGM, entity.getLinkage(IGM, isDefinition),
                 entity.isFragile(IGM),
                 isDefinition,
                 entity.isWeakImported(IGM.SILMod->getSwiftModule()));

  result.ForDefinition = isDefinition;

  return result;
}

static bool isPointerTo(llvm::Type *ptrTy, llvm::Type *objTy) {
  return cast<llvm::PointerType>(ptrTy)->getElementType() == objTy;
}

/// Get or create an LLVM function with these linkage rules.
llvm::Function *LinkInfo::createFunction(IRGenModule &IGM,
                                         llvm::FunctionType *fnType,
                                         llvm::CallingConv::ID cc,
                                         const llvm::AttributeSet &attrs,
                                         llvm::Function *insertBefore) {
  llvm::Function *existing = IGM.Module.getFunction(getName());
  if (existing) {
    if (isPointerTo(existing->getType(), fnType))
      return cast<llvm::Function>(existing);

    IGM.error(SourceLoc(),
              "program too clever: function collides with existing symbol "
                + getName());

    // Note that this will implicitly unique if the .unique name is also taken.
    existing->setName(getName() + ".unique");
  }

  llvm::Function *fn
    = llvm::Function::Create(fnType, getLinkage(), getName());
  if (insertBefore) {
    IGM.Module.getFunctionList().insert(insertBefore->getIterator(), fn);
  } else {
    IGM.Module.getFunctionList().push_back(fn);
  }
  fn->setVisibility(getVisibility());
  fn->setCallingConv(cc);

  auto initialAttrs = IGM.constructInitialAttributes();
  // Merge initialAttrs with attrs.
  auto updatedAttrs = attrs.addAttributes(IGM.getLLVMContext(),
                        llvm::AttributeSet::FunctionIndex, initialAttrs);
  if (!updatedAttrs.isEmpty())
    fn->setAttributes(updatedAttrs);

  // Everything externally visible is considered used in Swift.
  // That mostly means we need to be good at not marking things external.
  //
  // Exclude "main", because it should naturally be used, and because adding it
  // to llvm.used leaves a dangling use when the REPL attempts to discard
  // intermediate mains.
  if (ForDefinition &&
      Linkage == llvm::GlobalValue::ExternalLinkage &&
      Visibility == llvm::GlobalValue::DefaultVisibility &&
      getName() != SWIFT_ENTRY_POINT_FUNCTION) {
    IGM.addUsedGlobal(fn);
  }

  return fn;
}

bool LinkInfo::isUsed() const {
  // Everything externally visible is considered used in Swift.
  // That mostly means we need to be good at not marking things external.
  return ForDefinition &&
      Linkage == llvm::GlobalValue::ExternalLinkage &&
      Visibility == llvm::GlobalValue::DefaultVisibility;
}

/// Get or create an LLVM global variable with these linkage rules.
llvm::GlobalVariable *LinkInfo::createVariable(IRGenModule &IGM,
                                               llvm::Type *storageType,
                                               Alignment alignment,
                                               DebugTypeInfo DebugType,
                                               Optional<SILLocation> DebugLoc,
                                               StringRef DebugName) {
  llvm::GlobalValue *existingValue = IGM.Module.getNamedGlobal(getName());
  if (existingValue) {
    auto existingVar = dyn_cast<llvm::GlobalVariable>(existingValue);
    if (existingVar && isPointerTo(existingVar->getType(), storageType))
      return existingVar;

    IGM.error(SourceLoc(),
              "program too clever: variable collides with existing symbol "
                + getName());

    // Note that this will implicitly unique if the .unique name is also taken.
    existingValue->setName(getName() + ".unique");
  }

  auto var = new llvm::GlobalVariable(IGM.Module, storageType,
                                      /*constant*/ false,
                                      getLinkage(), /*initializer*/ nullptr,
                                      getName());
  var->setVisibility(getVisibility());
  var->setAlignment(alignment.getValue());

  // Everything externally visible is considered used in Swift.
  // That mostly means we need to be good at not marking things external.
  if (isUsed()) {
    IGM.addUsedGlobal(var);
  }

  if (IGM.DebugInfo && ForDefinition)
    IGM.DebugInfo->
      emitGlobalVariableDeclaration(var,
                                    DebugName.empty() ? getName() : DebugName,
                                    getName(), DebugType, DebugLoc);

  return var;
}

/// Emit a global declaration.
void IRGenModule::emitGlobalDecl(Decl *D) {
  switch (D->getKind()) {
  case DeclKind::Extension:
    return emitExtension(cast<ExtensionDecl>(D));

  case DeclKind::Protocol:
    return emitProtocolDecl(cast<ProtocolDecl>(D));

  case DeclKind::PatternBinding:
    // The global initializations are in SIL.
    return;

  case DeclKind::Param:
    llvm_unreachable("there are no global function parameters");

  case DeclKind::Subscript:
    llvm_unreachable("there are no global subscript operations");
      
  case DeclKind::EnumCase:
  case DeclKind::EnumElement:
    llvm_unreachable("there are no global enum elements");

  case DeclKind::Constructor:
    llvm_unreachable("there are no global constructor");

  case DeclKind::Destructor:
    llvm_unreachable("there are no global destructor");

  case DeclKind::TypeAlias:
  case DeclKind::GenericTypeParam:
  case DeclKind::AssociatedType:
  case DeclKind::IfConfig:
    return;

  case DeclKind::Enum:
    return emitEnumDecl(cast<EnumDecl>(D));

  case DeclKind::Struct:
    return emitStructDecl(cast<StructDecl>(D));

  case DeclKind::Class:
    return emitClassDecl(cast<ClassDecl>(D));

  // These declarations are only included in the debug info.
  case DeclKind::Import:
    if (DebugInfo)
      DebugInfo->emitImport(cast<ImportDecl>(D));
    return;

  // We emit these as part of the PatternBindingDecl.
  case DeclKind::Var:
    return;

  case DeclKind::Func:
    // Handled in SIL.
    return;

  case DeclKind::TopLevelCode:
    // All the top-level code will be lowered separately.
    return;
      
  // Operator decls aren't needed for IRGen.
  case DeclKind::InfixOperator:
  case DeclKind::PrefixOperator:
  case DeclKind::PostfixOperator:
    return;

  case DeclKind::Module:
    return;
  }

  llvm_unreachable("bad decl kind!");
}

void IRGenModule::emitExternalDefinition(Decl *D) {
  switch (D->getKind()) {
  case DeclKind::Extension:
  case DeclKind::PatternBinding:
  case DeclKind::EnumCase:
  case DeclKind::EnumElement:
  case DeclKind::TopLevelCode:
  case DeclKind::TypeAlias:
  case DeclKind::GenericTypeParam:
  case DeclKind::AssociatedType:
  case DeclKind::Import:
  case DeclKind::Subscript:
  case DeclKind::Destructor:
  case DeclKind::InfixOperator:
  case DeclKind::PrefixOperator:
  case DeclKind::PostfixOperator:
  case DeclKind::IfConfig:
  case DeclKind::Param:
    llvm_unreachable("Not a valid external definition for IRgen");

  case DeclKind::Var:
    assert(D->getClangDecl() && "Not a valid external var for IRGen");
    return emitClangDecl(const_cast<clang::Decl *>(D->getClangDecl()));

  case DeclKind::Func:
    if (auto *clangDecl = D->getClangDecl())
      emitClangDecl(const_cast<clang::Decl *>(clangDecl));
    break;

  case DeclKind::Constructor:
    // Do nothing.
    break;

  // No need to eagerly emit Swift metadata for external types.
  case DeclKind::Struct:
  case DeclKind::Enum:
  case DeclKind::Class:
  case DeclKind::Protocol:
    break;

  case DeclKind::Module:
    break;
  }
}

Address IRGenModule::getAddrOfSILGlobalVariable(SILGlobalVariable *var,
                                                ForDefinition_t forDefinition) {
  LinkEntity entity = LinkEntity::forSILGlobalVariable(var);
  auto &unknownTI = getTypeInfo(var->getLoweredType());
  assert(isa<FixedTypeInfo>(unknownTI) &&
         "unsupported global variable of resilient type!");
  auto &ti = cast<FixedTypeInfo>(unknownTI);

  // Check whether we've created the global variable already.
  // FIXME: We should integrate this into the LinkEntity cache more cleanly.
  auto gvar = Module.getGlobalVariable(var->getName(), /*allowInternal*/ true);
  if (gvar) {
    if (forDefinition)
      updateLinkageForDefinition(*this, gvar, entity);

    llvm::Constant *addr = gvar;
    if (ti.getStorageType() != gvar->getType()->getElementType()) {
      auto *expectedTy = ti.StorageType->getPointerTo();
      addr = llvm::ConstantExpr::getBitCast(addr, expectedTy);
    }
    return Address(addr, Alignment(gvar->getAlignment()));
  }

  LinkInfo link = LinkInfo::get(*this, entity, forDefinition);
  if (var->getDecl()) {
    // If we have the VarDecl, use it for more accurate debugging information.
    DebugTypeInfo DbgTy(var->getDecl(),
                        var->getLoweredType().getSwiftType(), ti);
    gvar = link.createVariable(*this, ti.StorageType,
                               ti.getFixedAlignment(),
                               DbgTy, SILLocation(var->getDecl()),
                               var->getDecl()->getName().str());
  } else {
    // There is no VarDecl for a SILGlobalVariable, and thus also no context.
    DeclContext *DeclCtx = nullptr;
    DebugTypeInfo DbgTy(var->getLoweredType().getSwiftRValueType(), ti,
                        DeclCtx);

    Optional<SILLocation> loc;
    if (var->hasLocation())
      loc = var->getLocation();
    gvar = link.createVariable(*this, ti.StorageType, ti.getFixedAlignment(),
                               DbgTy, loc, var->getName());
  }
  
  // Set the alignment from the TypeInfo.
  Address gvarAddr = ti.getAddressForPointer(gvar);
  gvar->setAlignment(gvarAddr.getAlignment().getValue());
  
  return gvarAddr;
}

/// Return True if the function \p f is a 'readonly' function. Checking
/// for the SIL @effects(readonly) attribute is not enough because this
/// definition does not match the definition of the LLVM readonly function
/// attribute. In this function we do the actual check.
static bool isReadOnlyFunction(SILFunction *f) {
  // Check if the function has any 'owned' parameters. Owned parameters may
  // call the destructor of the object which could violate the readonly-ness
  // of the function.
  if (f->hasOwnedParameters())
    return false;

  auto Eff = f->getEffectsKind();
  return Eff == EffectsKind::ReadNone ||
         Eff == EffectsKind::ReadOnly;
}

/// Find the entry point for a SIL function.
llvm::Function *IRGenModule::getAddrOfSILFunction(SILFunction *f,
                                                  ForDefinition_t forDefinition) {
  LinkEntity entity = LinkEntity::forSILFunction(f);

  // Check whether we've created the function already.
  // FIXME: We should integrate this into the LinkEntity cache more cleanly.
  llvm::Function *fn = Module.getFunction(f->getName());
  if (fn) {
    if (forDefinition) updateLinkageForDefinition(*this, fn, entity);
    return fn;
  }

  bool hasOrderNumber = f->isDefinition();
  unsigned orderNumber = ~0U;
  llvm::Function *insertBefore = nullptr;

  // If the SIL function has a definition, we should have an order
  // number for it; make sure to insert it in that position relative
  // to other ordered functions.
  if (hasOrderNumber) {
    orderNumber = dispatcher.getFunctionOrder(f);
    if (auto emittedFunctionIterator
          = EmittedFunctionsByOrder.findLeastUpperBound(orderNumber))
      insertBefore = *emittedFunctionIterator;

    // Also, if we have a lazy definition for it, be sure to queue that up.
    if (!forDefinition &&
        !isPossiblyUsedExternally(f->getLinkage(), SILMod->isWholeModule()))
      dispatcher.addLazyFunction(f);
  }
    
  llvm::AttributeSet attrs;
  llvm::FunctionType *fnType = getFunctionType(f->getLoweredFunctionType(),
                                               attrs);
  
  auto cc = expandCallingConv(*this, f->getRepresentation());
  LinkInfo link = LinkInfo::get(*this, entity, forDefinition);

  if (f->getInlineStrategy() == NoInline) {
    attrs = attrs.addAttribute(fnType->getContext(),
                llvm::AttributeSet::FunctionIndex, llvm::Attribute::NoInline);
  }
  if (isReadOnlyFunction(f)) {
    attrs = attrs.addAttribute(fnType->getContext(),
                llvm::AttributeSet::FunctionIndex, llvm::Attribute::ReadOnly);
  }
  fn = link.createFunction(*this, fnType, cc, attrs, insertBefore);

  // If we have an order number for this function, set it up as appropriate.
  if (hasOrderNumber) {
    EmittedFunctionsByOrder.insert(orderNumber, fn);
  }
  return fn;
}

static llvm::GlobalVariable *createGOTEquivalent(IRGenModule &IGM,
                                                 llvm::Constant *global,
                                                 StringRef globalName) {

  auto gotEquivalent = new llvm::GlobalVariable(IGM.Module,
                                      global->getType(),
                                      /*constant*/ true,
                                      llvm::GlobalValue::PrivateLinkage,
                                      global,
                                      llvm::Twine("got.") + globalName);
  gotEquivalent->setUnnamedAddr(true);
  return gotEquivalent;
}

static llvm::Constant *getElementBitCast(llvm::Constant *ptr,
                                         llvm::Type *newEltType) {
  auto ptrType = cast<llvm::PointerType>(ptr->getType());
  if (ptrType->getElementType() == newEltType) {
    return ptr;
  } else {
    auto newPtrType = newEltType->getPointerTo(ptrType->getAddressSpace());
    return llvm::ConstantExpr::getBitCast(ptr, newPtrType);
  }
}

/// A convenient wrapper around getAddrOfLLVMVariable which uses the
/// default type as the definition type.
llvm::Constant *
IRGenModule::getAddrOfLLVMVariable(LinkEntity entity, Alignment alignment,
                                   ForDefinition_t forDefinition,
                                   llvm::Type *defaultType,
                                   DebugTypeInfo debugType) {
  llvm::Type *definitionType = (forDefinition ? defaultType : nullptr);
  return getAddrOfLLVMVariable(entity, alignment, definitionType,
                               defaultType, debugType);
}

/// Get or create a llvm::GlobalVariable.
///
/// If a definition type is given, the result will always be an
/// llvm::GlobalVariable of that type.  Otherwise, the result will
/// have type pointerToDefaultType and may involve bitcasts.
llvm::Constant *
IRGenModule::getAddrOfLLVMVariable(LinkEntity entity, Alignment alignment,
                                   llvm::Type *definitionType,
                                   llvm::Type *defaultType,
                                   DebugTypeInfo debugType) {
  // This function assumes that 'globals' only contains GlobalValue
  // values for the entities that it will look up.

  auto &entry = GlobalVars[entity];
  if (entry) {
    auto existing = cast<llvm::GlobalValue>(entry);

    // If we're looking to define something, we may need to replace a
    // forward declaration.
    if (definitionType) {
      assert(existing->isDeclaration() && "already defined");
      assert(entry->getType()->getPointerElementType() == defaultType);
      updateLinkageForDefinition(*this, existing, entity);

      // If the type is right, we're done.
      if (definitionType == defaultType)
        return entry;

      // Fall out to the case below, clearing the name so that
      // createVariable doesn't detect a collision.
      entry->setName("");

    // Otherwise, we have a previous declaration or definition which
    // we need to ensure has the right type.
    } else {
      return getElementBitCast(entry, defaultType);
    }
  }

  ForDefinition_t forDefinition = (ForDefinition_t) (definitionType != nullptr);
  LinkInfo link = LinkInfo::get(*this, entity, forDefinition);

  // Clang may have defined the variable already.
  if (auto existing = Module.getNamedGlobal(link.getName()))
    return getElementBitCast(existing, defaultType);

  // If we're not defining the object now, forward declare it with the default
  // type.
  if (!definitionType) definitionType = defaultType;

  // Create the variable.
  auto var = link.createVariable(*this, definitionType, alignment, debugType);

  // If we have an existing entry, destroy it, replacing it with the
  // new variable.
  if (entry) {
    auto existing = cast<llvm::GlobalValue>(entry);
    auto castVar = getElementBitCast(var, defaultType);
    existing->replaceAllUsesWith(castVar);
    existing->eraseFromParent();
  }

  // If there's also an existing GOT-equivalent entry, rewrite it too, since
  // LLVM won't recognize a global with bitcasts in its initializers as GOT-
  // equivalent. rdar://problem/22388190
  auto foundGOTEntry = GlobalGOTEquivalents.find(entity);
  if (foundGOTEntry != GlobalGOTEquivalents.end() && foundGOTEntry->second) {
    auto existingGOTEquiv = cast<llvm::GlobalVariable>(foundGOTEntry->second);

    // Make a new GOT equivalent referring to the new variable with its
    // definition type.
    auto newGOTEquiv = createGOTEquivalent(*this, var, var->getName());
    auto castGOTEquiv = llvm::ConstantExpr::getBitCast(newGOTEquiv,
                                                   existingGOTEquiv->getType());
    existingGOTEquiv->replaceAllUsesWith(castGOTEquiv);
    existingGOTEquiv->eraseFromParent();
    GlobalGOTEquivalents[entity] = newGOTEquiv;
  }

  // Cache and return.
  entry = var;
  return var;
}

/// Get or create a "GOT equivalent" llvm::GlobalVariable, if applicable.
///
/// Creates a private, unnamed constant containing the address of another
/// global variable. LLVM can replace relative references to this variable with
/// relative references to the GOT entry for the variable in the object file.
std::pair<llvm::Constant *, IRGenModule::DirectOrGOT>
IRGenModule::getAddrOfLLVMVariableOrGOTEquivalent(LinkEntity entity,
                                                  Alignment alignment,
                                                  llvm::Type *defaultType) {
  // Ensure the variable is at least forward-declared.
  if (entity.isForeignTypeMetadataCandidate()) {
    auto foreignCandidate
      = getAddrOfForeignTypeMetadataCandidate(entity.getType());
    (void)foreignCandidate;
  } else {
    getAddrOfLLVMVariable(entity, alignment, /*definitionType*/ nullptr,
                          defaultType, DebugTypeInfo());
  }

  // Guess whether a global entry is a definition from this TU. This isn't
  // bulletproof, but at the point we emit conformance tables, we're far enough
  // along that we should have emitted any metadata objects we were going to.
  auto isDefinition = [&](llvm::Constant *global) -> bool {
    // We only emit aliases for definitions. (An extern alias would be an
    // extern global.)
    if (isa<llvm::GlobalAlias>(global))
      return true;
    // Global vars are definitions if they have an initializer.
    if (auto var = dyn_cast<llvm::GlobalVariable>(global))
      return var->hasInitializer();
    // Assume anything else isn't a definition.
    return false;
  };

  // If the variable isn't public, or has already been defined in this TU,
  // then it definitely doesn't need a GOT entry, and we can
  // relative-reference it directly.
  //
  // TODO: Internal symbols from other TUs we know are destined to be linked
  // into the same image as us could use direct
  // relative references too, to avoid producing unnecessary GOT entries in
  // the final image.
  auto entry = GlobalVars[entity];
  if (!hasPublicVisibility(entity.getLinkage(*this, NotForDefinition))
      || isDefinition(entry)) {
    // FIXME: Relative references to aliases break MC on 32-bit Mach-O
    // platforms (rdar://problem/22450593 ), so substitute an alias with its
    // aliasee to work around that.
    if (auto alias = dyn_cast<llvm::GlobalAlias>(entry))
      entry = alias->getAliasee();
    return {entry, DirectOrGOT::Direct};
  }

  auto &gotEntry = GlobalGOTEquivalents[entity];
  if (gotEntry) {
    return {gotEntry, DirectOrGOT::GOT};
  }

  // Look up the global variable.
  auto global = cast<llvm::GlobalValue>(GlobalVars[entity]);
  // Use it as the initializer for an anonymous constant. LLVM can treat this as
  // equivalent to the global's GOT entry.
  llvm::SmallString<64> name;
  entity.mangle(name);
  auto gotEquivalent = createGOTEquivalent(*this, global, name);
  gotEntry = gotEquivalent;
  return {gotEquivalent, DirectOrGOT::GOT};
}

namespace {
struct TypeEntityInfo {
  unsigned flags;
  LinkEntity entity;
  llvm::Type *defaultTy, *defaultPtrTy;
};
} // end anonymous namespace

static TypeEntityInfo
getTypeEntityForProtocolConformanceRecord(IRGenModule &IGM,
                                       NormalProtocolConformance *conformance) {
  ProtocolConformanceTypeKind typeKind;
  Optional<LinkEntity> entity;
  llvm::Type *defaultTy, *defaultPtrTy;

  // TODO: Should use accessor kind for lazy conformances
  ProtocolConformanceReferenceKind conformanceKind
    = ProtocolConformanceReferenceKind::WitnessTable;

  auto conformingType = conformance->getType()->getCanonicalType();
  if (auto bgt = dyn_cast<BoundGenericType>(conformingType)) {
    // Conformances for generics are represented by referencing the metadata
    // pattern for the generic type.
    typeKind = ProtocolConformanceTypeKind::UniqueGenericPattern;
    entity = LinkEntity::forTypeMetadata(
                         bgt->getDecl()->getDeclaredType()->getCanonicalType(),
                         TypeMetadataAddress::AddressPoint,
                         /*isPattern*/ true);
    defaultTy = IGM.TypeMetadataPatternStructTy;
    defaultPtrTy = IGM.TypeMetadataPatternPtrTy;
  } else if (auto ct = dyn_cast<ClassType>(conformingType)) {
    auto clas = ct->getDecl();
    if (clas->isForeign()) {
      typeKind = ProtocolConformanceTypeKind::NonuniqueDirectType;
      entity = LinkEntity::forForeignTypeMetadataCandidate(conformingType);
      defaultTy = IGM.TypeMetadataStructTy;
      defaultPtrTy = IGM.TypeMetadataPtrTy;
    } else {
      // TODO: We should indirectly reference classes. For now directly
      // reference the class object, which is totally wrong for ObjC interop.

      typeKind = ProtocolConformanceTypeKind::UniqueDirectClass;
      if (hasKnownSwiftMetadata(IGM, clas))
        entity = LinkEntity::forTypeMetadata(
                         conformingType,
                         TypeMetadataAddress::AddressPoint,
                         /*isPattern*/ false);
      else
        entity = LinkEntity::forObjCClass(clas);
      defaultTy = IGM.TypeMetadataStructTy;
      defaultPtrTy = IGM.TypeMetadataPtrTy;
    }
  } else if (auto nom = conformingType->getNominalOrBoundGenericNominal()) {
    // Metadata for Clang types should be uniqued like foreign classes.
    if (nom->hasClangNode()) {
      typeKind = ProtocolConformanceTypeKind::NonuniqueDirectType;
      entity = LinkEntity::forForeignTypeMetadataCandidate(conformingType);
      defaultTy = IGM.TypeMetadataStructTy;
      defaultPtrTy = IGM.TypeMetadataPtrTy;
    } else {
      // We can reference the canonical metadata for native value types
      // directly.
      typeKind = ProtocolConformanceTypeKind::UniqueDirectType;
      entity = LinkEntity::forTypeMetadata(
                       conformingType,
                       TypeMetadataAddress::AddressPoint,
                       /*isPattern*/ false);
      defaultTy = IGM.TypeMetadataStructTy;
      defaultPtrTy = IGM.TypeMetadataPtrTy;
    }
  } else {
    // TODO: Universal and/or structural conformances
    llvm_unreachable("unhandled protocol conformance");
  }

  auto flags = ProtocolConformanceFlags()
    .withTypeKind(typeKind)
    .withConformanceKind(conformanceKind);

  return {flags.getValue(), *entity, defaultTy, defaultPtrTy};
}

/// Form an LLVM constant for the relative distance between a reference
/// (appearing at gep (0, arrayIndex, structIndex) of `base`) and
/// `target`.
static llvm::Constant *emitRelativeReference(IRGenModule &IGM,
                               std::pair<llvm::Constant *,
                                         IRGenModule::DirectOrGOT> target,
                               llvm::Constant *base,
                               unsigned arrayIndex,
                               unsigned structIndex) {
  auto targetAddr = llvm::ConstantExpr::getPtrToInt(target.first, IGM.SizeTy);

  llvm::Constant *indexes[] = {
    llvm::ConstantInt::get(IGM.Int32Ty, 0),
    llvm::ConstantInt::get(IGM.Int32Ty, arrayIndex),
    llvm::ConstantInt::get(IGM.Int32Ty, structIndex),
  };

  auto baseElt = llvm::ConstantExpr::getInBoundsGetElementPtr(
                       base->getType()->getPointerElementType(), base, indexes);

  auto baseAddr = llvm::ConstantExpr::getPtrToInt(baseElt, IGM.SizeTy);

  auto relativeAddr = llvm::ConstantExpr::getSub(targetAddr, baseAddr);

  // Relative addresses can be 32-bit even on 64-bit platforms.
  if (IGM.SizeTy != IGM.RelativeAddressTy)
    relativeAddr = llvm::ConstantExpr::getTrunc(relativeAddr,
                                                IGM.RelativeAddressTy);

  // If the reference is to a GOT entry, flag it by setting the low bit.
  // (All of the base, direct target, and GOT entry need to be pointer-aligned
  // for this to be OK.)
  if (target.second == IRGenModule::DirectOrGOT::GOT) {
    relativeAddr = llvm::ConstantExpr::getAdd(relativeAddr,
                                        llvm::ConstantInt::get(IGM.Int32Ty, 1));
  }

  return relativeAddr;
}

/// Emit the protocol conformance list and return it.
llvm::Constant *IRGenModule::emitProtocolConformances() {
  std::string sectionName;
  switch (TargetInfo.OutputObjectFormat) {
  case llvm::Triple::MachO:
    sectionName = "__TEXT, __swift2_proto, regular, no_dead_strip";
    break;
  case llvm::Triple::ELF:
    sectionName = ".swift2_protocol_conformances";
    break;
  default:
    llvm_unreachable("Don't know how to emit protocol conformances for "
                     "the selected object format.");
  }

  // Do nothing if the list is empty.
  if (ProtocolConformances.empty())
    return nullptr;

  // Define the global variable for the conformance list.
  // We have to do this before defining the initializer since the entries will
  // contain offsets relative to themselves.
  auto arrayTy = llvm::ArrayType::get(ProtocolConformanceRecordTy,
                                      ProtocolConformances.size());

  // FIXME: This needs to be a linker-local symbol in order for Darwin ld to
  // resolve relocations relative to it.
  auto var = new llvm::GlobalVariable(Module, arrayTy,
                                      /*isConstant*/ true,
                                      llvm::GlobalValue::PrivateLinkage,
                                      /*initializer*/ nullptr,
                                      "\x01l_protocol_conformances");

  SmallVector<llvm::Constant*, 8> elts;
  for (auto *conformance : ProtocolConformances) {
    auto descriptorRef = getAddrOfLLVMVariableOrGOTEquivalent(
                  LinkEntity::forProtocolDescriptor(conformance->getProtocol()),
                  getPointerAlignment(), ProtocolDescriptorStructTy);

    auto typeEntity
      = getTypeEntityForProtocolConformanceRecord(*this, conformance);

    // If the conformance is in this object's table, then the witness table
    // should also be in this object file, so we can always directly reference
    // it.
    // TODO: Produce a relative reference to a private generator function
    // if the witness table requires lazy initialization, instantiation, or
    // conditional conformance checking.
    std::pair<llvm::Constant*, DirectOrGOT> witnessTableRef;
    auto witnessTableVar = getAddrOfWitnessTable(conformance);
    witnessTableRef = std::make_pair(witnessTableVar,
                                     DirectOrGOT::Direct);

    auto typeRef = getAddrOfLLVMVariableOrGOTEquivalent(
      typeEntity.entity, getPointerAlignment(), typeEntity.defaultTy);

    llvm::Constant *recordFields[] = {
      emitRelativeReference(*this, descriptorRef,   var, elts.size(), 0),
      emitRelativeReference(*this, typeRef,         var, elts.size(), 1),
      emitRelativeReference(*this, witnessTableRef, var, elts.size(), 2),
      llvm::ConstantInt::get(Int32Ty, typeEntity.flags),
    };

    auto record = llvm::ConstantStruct::get(ProtocolConformanceRecordTy,
                                            recordFields);
    elts.push_back(record);
  }

  auto initializer = llvm::ConstantArray::get(arrayTy, elts);

  var->setInitializer(initializer);
  var->setSection(sectionName);
  var->setAlignment(getPointerAlignment().getValue());
  addUsedGlobal(var);
  return var;
}

/// Fetch a global reference to the given Objective-C class.  The
/// result is of type ObjCClassPtrTy.
llvm::Constant *IRGenModule::getAddrOfObjCClass(ClassDecl *theClass,
                                                ForDefinition_t forDefinition) {
  assert(ObjCInterop && "getting address of ObjC class in no-interop mode");
  LinkEntity entity = LinkEntity::forObjCClass(theClass);
  DebugTypeInfo DbgTy(theClass, ObjCClassPtrTy,
                      getPointerSize(), getPointerAlignment());
  auto addr = getAddrOfLLVMVariable(entity, getPointerAlignment(),
                                    forDefinition, ObjCClassStructTy, DbgTy);
  return addr;
}

/// Fetch a global reference to the given Objective-C metaclass.
/// The result is always a GlobalValue of ObjCClassPtrTy.
llvm::Constant *IRGenModule::getAddrOfObjCMetaclass(ClassDecl *theClass,
                                                ForDefinition_t forDefinition) {
  assert(ObjCInterop && "getting address of ObjC metaclass in no-interop mode");
  LinkEntity entity = LinkEntity::forObjCMetaclass(theClass);
  DebugTypeInfo DbgTy(theClass, ObjCClassPtrTy,
                      getPointerSize(), getPointerAlignment());
  auto addr = getAddrOfLLVMVariable(entity, getPointerAlignment(),
                                    forDefinition, ObjCClassStructTy, DbgTy);
  return addr;
}

/// Fetch the declaration of the metaclass stub for the given class type.
/// The result is always a GlobalValue of ObjCClassPtrTy.
llvm::Constant *IRGenModule::getAddrOfSwiftMetaclassStub(ClassDecl *theClass,
                                                ForDefinition_t forDefinition) {
  assert(ObjCInterop && "getting address of metaclass stub in no-interop mode");
  LinkEntity entity = LinkEntity::forSwiftMetaclassStub(theClass);
  DebugTypeInfo DbgTy(theClass, ObjCClassPtrTy,
                      getPointerSize(), getPointerAlignment());
  auto addr = getAddrOfLLVMVariable(entity, getPointerAlignment(),
                                    forDefinition, ObjCClassStructTy, DbgTy);
  return addr;
}

/// Fetch the declaration of a metaclass object.  This performs either
/// getAddrOfSwiftMetaclassStub or getAddrOfObjCMetaclass, depending
/// on whether the class is published as an ObjC class.
llvm::Constant *IRGenModule::getAddrOfMetaclassObject(ClassDecl *decl,
                                                ForDefinition_t forDefinition) {
  assert(!decl->isGenericContext()
         && "generic classes do not have a static metaclass object");
  if (decl->checkObjCAncestry() != ObjCClassKind::NonObjC ||
      decl->hasClangNode()) {
    return getAddrOfObjCMetaclass(decl, forDefinition);
  } else {
    return getAddrOfSwiftMetaclassStub(decl, forDefinition);
  }
}

/// Fetch the type metadata access function for a non-generic type.
llvm::Function *
IRGenModule::getAddrOfTypeMetadataAccessFunction(CanType type,
                                              ForDefinition_t forDefinition) {
  assert(!type->hasArchetype() && !type->hasTypeParameter());
  LinkEntity entity = LinkEntity::forTypeMetadataAccessFunction(type);
  llvm::Function *&entry = GlobalFuncs[entity];
  if (entry) {
    if (forDefinition) updateLinkageForDefinition(*this, entry, entity);
    return entry;
  }

  auto fnType = llvm::FunctionType::get(TypeMetadataPtrTy, false);
  LinkInfo link = LinkInfo::get(*this, entity, forDefinition);
  entry = link.createFunction(*this, fnType, RuntimeCC, llvm::AttributeSet());
  return entry;
}

/// Get or create a type metadata cache variable.  These are an
/// implementation detail of type metadata access functions.
llvm::Constant *
IRGenModule::getAddrOfTypeMetadataLazyCacheVariable(CanType type,
                                              ForDefinition_t forDefinition) {
  assert(!type->hasArchetype() && !type->hasTypeParameter());
  LinkEntity entity = LinkEntity::forTypeMetadataLazyCacheVariable(type);
  return getAddrOfLLVMVariable(entity, getPointerAlignment(), forDefinition,
                               TypeMetadataPtrTy, DebugTypeInfo());
}

/// Define the metadata for a type.
///
/// Some type metadata has information before the address point that the
/// public symbol for the metadata references. This function will rewrite any
/// existing external declaration to the address point as an alias into the
/// full metadata object.
llvm::GlobalValue *IRGenModule::defineTypeMetadata(CanType concreteType,
                                                   bool isIndirect,
                                                   bool isPattern,
                                                   bool isConstant,
                                                   llvm::Constant *init,
                                                   llvm::StringRef section) {
  assert(init);
  assert(!isIndirect && "indirect type metadata not used yet");

  /// For concrete metadata, we want to use the initializer on the
  /// "full metadata", and define the "direct" address point as an alias.
  /// For generic metadata patterns, the address point is always at the
  /// beginning of the template (for now...).
  TypeMetadataAddress addrKind;
  llvm::Type *defaultVarTy;
  unsigned adjustmentIndex;
  Alignment alignment = getPointerAlignment();

  if (isPattern) {
    addrKind = TypeMetadataAddress::AddressPoint;
    defaultVarTy = TypeMetadataPatternStructTy;
    adjustmentIndex = MetadataAdjustmentIndex::None;
  } else if (concreteType->getClassOrBoundGenericClass()) {
    addrKind = TypeMetadataAddress::FullMetadata;
    defaultVarTy = FullHeapMetadataStructTy;
    adjustmentIndex = MetadataAdjustmentIndex::Class;
  } else {
    addrKind = TypeMetadataAddress::FullMetadata;
    defaultVarTy = FullTypeMetadataStructTy;
    adjustmentIndex = MetadataAdjustmentIndex::ValueType;
  }

  auto entity = LinkEntity::forTypeMetadata(concreteType, addrKind, isPattern);

  auto DbgTy = DebugTypeInfo(MetatypeType::get(concreteType),
                             defaultVarTy->getPointerTo(),
                             0, 1, nullptr);

  // Define the variable.
  llvm::GlobalVariable *var = cast<llvm::GlobalVariable>(
      getAddrOfLLVMVariable(entity, alignment, init->getType(), defaultVarTy,
                            DbgTy));

  var->setInitializer(init);
  var->setConstant(isConstant);
  if (!section.empty())
    var->setSection(section);

  // For metadata patterns, we're done.
  if (isPattern)
    return var;

  // For concrete metadata, declare the alias to its address point.
  auto directEntity = LinkEntity::forTypeMetadata(concreteType,
                                              TypeMetadataAddress::AddressPoint,
                                              /*isPattern*/ false);

  llvm::Constant *addr = var;
  // Do an adjustment if necessary.
  if (adjustmentIndex) {
    llvm::Constant *indices[] = {
      llvm::ConstantInt::get(Int32Ty, 0),
      llvm::ConstantInt::get(Int32Ty, adjustmentIndex)
    };
    addr = llvm::ConstantExpr::getInBoundsGetElementPtr(/*Ty=*/nullptr,
                                                        addr, indices);
  }
  addr = llvm::ConstantExpr::getBitCast(addr, TypeMetadataPtrTy);

  // Check for an existing forward declaration of the address point.
  auto &directEntry = GlobalVars[directEntity];
  llvm::GlobalValue *existingVal = nullptr;
  if (directEntry) {
    existingVal = cast<llvm::GlobalValue>(directEntry);
    // Clear the existing value's name so we can steal it.
    existingVal->setName("");
  }

  LinkInfo link = LinkInfo::get(*this, directEntity, ForDefinition);
  auto *ptrTy = cast<llvm::PointerType>(addr->getType());
  auto *alias = llvm::GlobalAlias::create(
      ptrTy->getElementType(), ptrTy->getAddressSpace(), link.getLinkage(),
      link.getName(), addr, &Module);
  alias->setVisibility(link.getVisibility());

  // The full metadata is used based on the visibility of the address point,
  // not the metadata itself.
  if (link.isUsed()) {
    addUsedGlobal(var);
    addUsedGlobal(alias);
  }

  // Replace an existing external declaration for the address point.
  if (directEntry) {
    auto existingVal = cast<llvm::GlobalValue>(directEntry);

    // FIXME: MC breaks when emitting alias references on some platforms
    // (rdar://problem/22450593 ). Work around this by referring to the aliasee
    // instead.
    llvm::Constant *aliasCast = alias->getAliasee();
    aliasCast = llvm::ConstantExpr::getBitCast(aliasCast,
                                               directEntry->getType());
    existingVal->replaceAllUsesWith(aliasCast);
    existingVal->eraseFromParent();
  }
  directEntry = alias;

  return alias;
}

/// Fetch the declaration of the metadata (or metadata template) for a
/// type.
///
/// If the definition type is specified, the result will always be a
/// GlobalValue of the given type, which may not be at the
/// canonical address point for a type metadata.
///
/// If the definition type is not specified, then:
///   - if the metadata is indirect, then the result will not be adjusted
///     and it will have the type pointer-to-T, where T is the type
///     of a direct metadata;
///   - if the metadata is a pattern, then the result will not be
///     adjusted and it will have TypeMetadataPatternPtrTy;
///   - otherwise it will be adjusted to the canonical address point
///     for a type metadata and it will have type TypeMetadataPtrTy.
llvm::Constant *IRGenModule::getAddrOfTypeMetadata(CanType concreteType,
                                                   bool isPattern) {
  assert(isPattern || !isa<UnboundGenericType>(concreteType));

  llvm::Type *defaultVarTy;
  unsigned adjustmentIndex;
  Alignment alignment = getPointerAlignment();

  ClassDecl *ObjCClass = nullptr;
  
  // Patterns use the pattern type and no adjustment.
  if (isPattern) {
    defaultVarTy = TypeMetadataPatternStructTy;
    adjustmentIndex = 0;

  // Objective-C classes use the ObjC class object.
  } else if (isa<ClassType>(concreteType) &&
             !hasKnownSwiftMetadata(*this,
                                    cast<ClassType>(concreteType)->getDecl())) {
    defaultVarTy = TypeMetadataStructTy;
    adjustmentIndex = 0;
    ObjCClass = cast<ClassType>(concreteType)->getDecl();
  // The symbol for other nominal type metadata is generated at the address
  // point.
  } else if (isa<ClassType>(concreteType) ||
             isa<BoundGenericClassType>(concreteType)) {
    assert(!concreteType->getClassOrBoundGenericClass()->isForeign()
           && "metadata for foreign classes should be emitted as "
              "foreign candidate");
    defaultVarTy = TypeMetadataStructTy;
    adjustmentIndex = 0;
  } else if (concreteType->getAnyNominal()) {
    auto nom = concreteType->getNominalOrBoundGenericNominal();
    assert((!nom || !nom->hasClangNode())
           && "metadata for foreign type should be emitted as "
              "foreign candidate");
    (void)nom;
    
    defaultVarTy = TypeMetadataStructTy;
    adjustmentIndex = 0;
  } else {
    // FIXME: Non-nominal metadata provided by the C++ runtime is exported
    // with the address of the start of the full metadata object, since
    // Clang doesn't provide an easy way to emit symbols aliasing into the
    // middle of an object.
    defaultVarTy = FullTypeMetadataStructTy;
    adjustmentIndex = MetadataAdjustmentIndex::ValueType;
  }

  // If this is a use, and the type metadata is emitted lazily,
  // trigger lazy emission of the metadata.
  if (isTypeMetadataEmittedLazily(concreteType)) {
    dispatcher.addLazyTypeMetadata(concreteType);
  }

  LinkEntity entity
    = ObjCClass ? LinkEntity::forObjCClass(ObjCClass)
                : LinkEntity::forTypeMetadata(concreteType,
                                     TypeMetadataAddress::AddressPoint,
                                     isPattern);

  auto DbgTy = ObjCClass 
    ? DebugTypeInfo(ObjCClass, ObjCClassPtrTy,
                    getPointerSize(), getPointerAlignment())
    : DebugTypeInfo(MetatypeType::get(concreteType),
                    defaultVarTy->getPointerTo(),
                    0, 1, nullptr);

  auto addr = getAddrOfLLVMVariable(entity, alignment,
                                    nullptr, defaultVarTy, DbgTy);

  // FIXME: MC breaks when emitting alias references on some platforms
  // (rdar://problem/22450593 ). Work around this by referring to the aliasee
  // instead.
  if (auto alias = dyn_cast<llvm::GlobalAlias>(addr))
    addr = alias->getAliasee();

  // Adjust if necessary.
  if (adjustmentIndex) {
    llvm::Constant *indices[] = {
      llvm::ConstantInt::get(Int32Ty, 0),
      llvm::ConstantInt::get(Int32Ty, adjustmentIndex)
    };
    addr = llvm::ConstantExpr::getInBoundsGetElementPtr(
                                                 /*Ty=*/nullptr, addr, indices);
  }
  
  return addr;
}

/// Return the address of a foreign-type metadata candidate.  There is no
/// single translation unit which can uniquely emit foreign-type metadata,
/// and we don't want to force the dynamic linker to eagerly unique them
/// by symbol name, so instead we emit a "candidate" metadata and ask
/// the runtime to unique them.
llvm::Constant *
IRGenModule::getAddrOfForeignTypeMetadataCandidate(CanType type) {
  // What we save in GlobalVars is actually the offsetted value.
  auto entity = LinkEntity::forForeignTypeMetadataCandidate(type);
  if (auto entry = GlobalVars[entity])
    return entry;

  // Compute the constant initializer and the offset of the type
  // metadata candidate within it.
  Size addressPoint;
  auto init = emitForeignTypeMetadataInitializer(*this, type, addressPoint);

  // Create the global variable.
  LinkInfo link = LinkInfo::get(*this, entity, ForDefinition);
  auto var = link.createVariable(*this, init->getType(),
                                 getPointerAlignment());
  var->setInitializer(init);

  // Apply the offset.
  llvm::Constant *result = var;
  result = llvm::ConstantExpr::getBitCast(result, Int8PtrTy);
  result = llvm::ConstantExpr::getInBoundsGetElementPtr(
      Int8Ty, result, getSize(addressPoint));
  result = llvm::ConstantExpr::getBitCast(result, TypeMetadataPtrTy);

  // Only remember the offset.
  GlobalVars[entity] = result;

  return result;
}

/// Return the address of a nominal type descriptor.  Right now, this
/// must always be for purposes of defining it.
llvm::Constant *IRGenModule::getAddrOfNominalTypeDescriptor(NominalTypeDecl *D,
                                                  llvm::Type *definitionType) {
  assert(definitionType && "not defining nominal type descriptor?");
  auto entity = LinkEntity::forNominalTypeDescriptor(D);
  return getAddrOfLLVMVariable(entity, getPointerAlignment(),
                               definitionType, definitionType,
                               DebugTypeInfo());
}

llvm::Constant *IRGenModule::getAddrOfProtocolDescriptor(ProtocolDecl *D,
                                                ForDefinition_t forDefinition) {
  if (D->isObjC())
    return getAddrOfObjCProtocolRecord(D, forDefinition);
  
  auto entity = LinkEntity::forProtocolDescriptor(D);
  auto ty = ProtocolDescriptorStructTy;
  return getAddrOfLLVMVariable(entity, getPointerAlignment(),
                               forDefinition, ty, DebugTypeInfo());
}

/// Fetch the declaration of the ivar initializer for the given class.
Optional<llvm::Function*> IRGenModule::getAddrOfIVarInitDestroy(
                            ClassDecl *cd,
                            bool isDestroyer,
                            bool isForeign,
                            ForDefinition_t forDefinition) {
  SILDeclRef silRef(cd, 
                    isDestroyer? SILDeclRef::Kind::IVarDestroyer
                               : SILDeclRef::Kind::IVarInitializer, 
                    ResilienceExpansion::Minimal,
                    SILDeclRef::ConstructAtNaturalUncurryLevel, 
                    isForeign);

  // Find the SILFunction for the ivar initializer or destroyer.
  if (auto silFn = SILMod->lookUpFunction(silRef)) {
    return getAddrOfSILFunction(silFn, forDefinition);
  }

  return None;
}

/// Returns the address of a value-witness function.
llvm::Function *IRGenModule::getAddrOfValueWitness(CanType abstractType,
                                                   ValueWitness index,
                                                ForDefinition_t forDefinition) {
  // We shouldn't emit value witness symbols for generic type instances.
  assert(!isa<BoundGenericType>(abstractType) &&
         "emitting value witness for generic type instance?!");
  
  LinkEntity entity = LinkEntity::forValueWitness(abstractType, index);

  llvm::Function *&entry = GlobalFuncs[entity];
  if (entry) {
    if (forDefinition) updateLinkageForDefinition(*this, entry, entity);
    return entry;
  }

  // Find the appropriate function type.
  llvm::FunctionType *fnType =
    cast<llvm::FunctionType>(
      cast<llvm::PointerType>(getValueWitnessTy(index))
        ->getElementType());
  LinkInfo link = LinkInfo::get(*this, entity, forDefinition);
  entry = link.createFunction(*this, fnType, RuntimeCC, llvm::AttributeSet());
  return entry;
}

/// Returns the address of a value-witness table.  If a definition
/// type is provided, the table is created with that type; the return
/// value will be an llvm::GlobalValue.  Otherwise, the result will
/// have type WitnessTablePtrTy.
llvm::Constant *IRGenModule::getAddrOfValueWitnessTable(CanType concreteType,
                                                  llvm::Type *definitionType) {
  LinkEntity entity = LinkEntity::forValueWitnessTable(concreteType);
  DebugTypeInfo DbgTy(concreteType, WitnessTablePtrTy,
                      getPointerSize(), getPointerAlignment(),
                      nullptr);
  return getAddrOfLLVMVariable(entity, getPointerAlignment(), definitionType,
                               WitnessTableTy, DbgTy);
}

static Address getAddrOfSimpleVariable(IRGenModule &IGM,
                            llvm::DenseMap<LinkEntity, llvm::Constant*> &cache,
                                       LinkEntity entity,
                                       llvm::Type *type,
                                       Alignment alignment,
                                       ForDefinition_t forDefinition) {
  // Check whether it's already cached.
  llvm::Constant *&entry = cache[entity];
  if (entry) {
    auto existing = cast<llvm::GlobalValue>(entry);
    assert(alignment == Alignment(existing->getAlignment()));
    if (forDefinition) updateLinkageForDefinition(IGM, existing, entity);
    return Address(entry, alignment);
  }

  // Otherwise, we need to create it.
  LinkInfo link = LinkInfo::get(IGM, entity, forDefinition);
  auto addr = link.createVariable(IGM, type, alignment);
  addr->setConstant(true);

  entry = addr;
  return Address(addr, alignment);
}

/// getAddrOfWitnessTableOffset - Get the address of the global
/// variable which contains an offset within a witness table for the
/// value associated with the given function.
Address IRGenModule::getAddrOfWitnessTableOffset(SILDeclRef code,
                                                ForDefinition_t forDefinition) {
  LinkEntity entity =
    LinkEntity::forWitnessTableOffset(code.getDecl(),
                                      code.getResilienceExpansion(),
                                      code.uncurryLevel);
  return getAddrOfSimpleVariable(*this, GlobalVars, entity,
                                 SizeTy, getPointerAlignment(),
                                 forDefinition);
}

/// getAddrOfWitnessTableOffset - Get the address of the global
/// variable which contains an offset within a witness table for the
/// value associated with the given member variable..
Address IRGenModule::getAddrOfWitnessTableOffset(VarDecl *field,
                                                ForDefinition_t forDefinition) {
  LinkEntity entity =
    LinkEntity::forWitnessTableOffset(field, ResilienceExpansion::Minimal, 0);
  return ::getAddrOfSimpleVariable(*this, GlobalVars, entity,
                                   SizeTy, getPointerAlignment(),
                                   forDefinition);
}

/// getAddrOfFieldOffset - Get the address of the global variable
/// which contains an offset to apply to either an object (if direct)
/// or a metadata object in order to find an offset to apply to an
/// object (if indirect).
///
/// The result is always a GlobalValue.
Address IRGenModule::getAddrOfFieldOffset(VarDecl *var, bool isIndirect,
                                          ForDefinition_t forDefinition) {
  LinkEntity entity = LinkEntity::forFieldOffset(var, isIndirect);
  return getAddrOfSimpleVariable(*this, GlobalVars, entity,
                                 SizeTy, getPointerAlignment(),
                                 forDefinition);
}

void IRGenModule::emitNestedTypeDecls(DeclRange members) {
  for (Decl *member : members) {
    switch (member->getKind()) {
    case DeclKind::Import:
    case DeclKind::TopLevelCode:
    case DeclKind::Protocol:
    case DeclKind::Extension:
    case DeclKind::InfixOperator:
    case DeclKind::PrefixOperator:
    case DeclKind::PostfixOperator:
    case DeclKind::Param:
    case DeclKind::Module:
      llvm_unreachable("decl not allowed in type context");

    case DeclKind::IfConfig:
      continue;

    case DeclKind::PatternBinding:
    case DeclKind::Var:
    case DeclKind::Subscript:
    case DeclKind::Func:
    case DeclKind::Constructor:
    case DeclKind::Destructor:
    case DeclKind::EnumCase:
    case DeclKind::EnumElement:
      // Skip non-type members.
      continue;

    case DeclKind::AssociatedType:
    case DeclKind::GenericTypeParam:
      // Do nothing.
      continue;

    case DeclKind::TypeAlias:
      // Do nothing.
      continue;

    case DeclKind::Enum:
      emitEnumDecl(cast<EnumDecl>(member));
      continue;
    case DeclKind::Struct:
      emitStructDecl(cast<StructDecl>(member));
      continue;
    case DeclKind::Class:
      emitClassDecl(cast<ClassDecl>(member));
      continue;
    }
  }
}

static bool shouldEmitCategory(IRGenModule &IGM, ExtensionDecl *ext) {
  for (auto conformance : ext->getLocalConformances()) {
    if (conformance->getProtocol()->isObjC())
      return true;
  }

  for (auto member : ext->getMembers()) {
    if (auto func = dyn_cast<FuncDecl>(member)) {
      if (requiresObjCMethodDescriptor(func))
        return true;
    } else if (auto constructor = dyn_cast<ConstructorDecl>(member)) {
      if (requiresObjCMethodDescriptor(constructor))
        return true;
    } else if (auto var = dyn_cast<VarDecl>(member)) {
      if (requiresObjCPropertyDescriptor(IGM, var))
        return true;
    } else if (auto subscript = dyn_cast<SubscriptDecl>(member)) {
      if (requiresObjCSubscriptDescriptor(IGM, subscript))
        return true;
    }
  }

  return false;
}

void IRGenModule::emitExtension(ExtensionDecl *ext) {
  emitNestedTypeDecls(ext->getMembers());

  // Generate a category if the extension either introduces a
  // conformance to an ObjC protocol or introduces a method
  // that requires an Objective-C entry point.
  ClassDecl *origClass = ext->getExtendedType()->getClassOrBoundGenericClass();
  if (!origClass)
    return;

  if (shouldEmitCategory(*this, ext)) {
    assert(origClass && !origClass->isForeign() &&
           "CF types cannot have categories emitted");
    llvm::Constant *category = emitCategoryData(*this, ext);
    category = llvm::ConstantExpr::getBitCast(category, Int8PtrTy);
    ObjCCategories.push_back(category);
    ObjCCategoryDecls.push_back(ext);
  }
}


/// Create an allocation on the stack.
Address IRGenFunction::createAlloca(llvm::Type *type,
                                    Alignment alignment,
                                    const llvm::Twine &name) {
  llvm::AllocaInst *alloca = new llvm::AllocaInst(type, name, AllocaIP);
  alloca->setAlignment(alignment.getValue());
  return Address(alloca, alignment);
}

/// Allocate a fixed-size buffer on the stack.
Address IRGenFunction::createFixedSizeBufferAlloca(const llvm::Twine &name) {
  return createAlloca(IGM.getFixedBufferTy(),
                      getFixedBufferAlignment(IGM),
                      name);
}

/// Get or create a global string constant.
///
/// \returns an i8* with a null terminator; note that embedded nulls
///   are okay
llvm::Constant *IRGenModule::getAddrOfGlobalString(StringRef data) {
  // Check whether this string already exists.
  auto &entry = GlobalStrings[data];
  if (entry) return entry;

  // If not, create it.  This implicitly adds a trailing null.
  auto init = llvm::ConstantDataArray::getString(LLVMContext, data);
  auto global = new llvm::GlobalVariable(Module, init->getType(), true,
                                         llvm::GlobalValue::PrivateLinkage,
                                         init);
  global->setUnnamedAddr(true);

  // Drill down to make an i8*.
  auto zero = llvm::ConstantInt::get(SizeTy, 0);
  llvm::Constant *indices[] = { zero, zero };
  auto address = llvm::ConstantExpr::getInBoundsGetElementPtr(
      global->getValueType(), global, indices);

  // Cache and return.
  entry = address;
  return address;
}

/// Get or create a global UTF-16 string constant.
///
/// \returns an i16* with a null terminator; note that embedded nulls
///   are okay
llvm::Constant *IRGenModule::getAddrOfGlobalUTF16String(StringRef utf8) {
  // Check whether this string already exists.
  auto &entry = GlobalUTF16Strings[utf8];
  if (entry) return entry;

  // If not, first transcode it to UTF16.
  SmallVector<UTF16, 128> buffer(utf8.size() + 1); // +1 for ending nulls.
  const UTF8 *fromPtr = (const UTF8 *) utf8.data();
  UTF16 *toPtr = &buffer[0];
  (void) ConvertUTF8toUTF16(&fromPtr, fromPtr + utf8.size(),
                            &toPtr, toPtr + utf8.size(),
                            strictConversion);

  // The length of the transcoded string in UTF-8 code points.
  size_t utf16Length = toPtr - &buffer[0];

  // Null-terminate the UTF-16 string.
  *toPtr = 0;
  ArrayRef<UTF16> utf16(&buffer[0], utf16Length + 1);

  auto init = llvm::ConstantDataArray::get(LLVMContext, utf16);
  auto global = new llvm::GlobalVariable(Module, init->getType(), true,
                                         llvm::GlobalValue::PrivateLinkage,
                                         init);
  global->setUnnamedAddr(true);

  // Drill down to make an i16*.
  auto zero = llvm::ConstantInt::get(SizeTy, 0);
  llvm::Constant *indices[] = { zero, zero };
  auto address = llvm::ConstantExpr::getInBoundsGetElementPtr(
      global->getValueType(), global, indices);

  // Cache and return.
  entry = address;
  return address;
}

/// Mangle the name of a type.
StringRef IRGenModule::mangleType(CanType type, SmallVectorImpl<char> &buffer) {
  LinkEntity::forTypeMangling(type).mangle(buffer);
  return StringRef(buffer.data(), buffer.size());
}

/// Is the given declaration resilient?
bool IRGenModule::isResilient(Decl *D, ResilienceScope scope) {
  auto NTD = dyn_cast<NominalTypeDecl>(D);
  if (!NTD)
    return false;

  switch (scope) {
  case ResilienceScope::Component:
    return !NTD->hasFixedLayout(SILMod->getSwiftModule());
  case ResilienceScope::Universal:
    return !NTD->hasFixedLayout();
  }

  llvm_unreachable("Bad resilience scope");
}

/// Fetch the witness table access function for a protocol conformance.
llvm::Function *
IRGenModule::getAddrOfWitnessTableAccessFunction(
                                      const NormalProtocolConformance *conf,
                                              ForDefinition_t forDefinition) {
  LinkEntity entity = LinkEntity::forProtocolWitnessTableAccessFunction(conf);
  llvm::Function *&entry = GlobalFuncs[entity];
  if (entry) {
    if (forDefinition) updateLinkageForDefinition(*this, entry, entity);
    return entry;
  }

  llvm::FunctionType *fnType;
  if (conf->getDeclContext()->isGenericContext()) {
    fnType = llvm::FunctionType::get(WitnessTablePtrTy, {TypeMetadataPtrTy},
                                     false);
  } else {
    fnType = llvm::FunctionType::get(WitnessTablePtrTy, false);
  }

  LinkInfo link = LinkInfo::get(*this, entity, forDefinition);
  entry = link.createFunction(*this, fnType, RuntimeCC, llvm::AttributeSet());
  return entry;
}

/// Fetch the lazy witness table access function for a protocol conformance.
llvm::Function *
IRGenModule::getAddrOfWitnessTableLazyAccessFunction(
                                      const NormalProtocolConformance *conf,
                                              CanType conformingType,
                                              ForDefinition_t forDefinition) {
  LinkEntity entity =
    LinkEntity::forProtocolWitnessTableLazyAccessFunction(conf, conformingType);
  llvm::Function *&entry = GlobalFuncs[entity];
  if (entry) {
    if (forDefinition) updateLinkageForDefinition(*this, entry, entity);
    return entry;
  }

  llvm::FunctionType *fnType
    = llvm::FunctionType::get(WitnessTablePtrTy, false);

  LinkInfo link = LinkInfo::get(*this, entity, forDefinition);
  entry = link.createFunction(*this, fnType, RuntimeCC, llvm::AttributeSet());
  return entry;
}

/// Get or create a witness table cache variable.  These are an
/// implementation detail of witness table lazy access functions.
llvm::Constant *
IRGenModule::getAddrOfWitnessTableLazyCacheVariable(
                                      const NormalProtocolConformance *conf,
                                              CanType conformingType,
                                              ForDefinition_t forDefinition) {
  assert(!conformingType->hasArchetype());
  LinkEntity entity =
    LinkEntity::forProtocolWitnessTableLazyCacheVariable(conf, conformingType);
  return getAddrOfLLVMVariable(entity, getPointerAlignment(),
                               forDefinition, WitnessTablePtrTy,
                               DebugTypeInfo());
}

/// Look up the address of a witness table.
///
/// TODO: This needs to take a flag for the access mode of the witness table,
/// which may be direct, lazy, or a runtime instantiation template.
/// TODO: Use name from witness table here to lookup witness table instead of
/// recomputing it.
llvm::Constant*
IRGenModule::getAddrOfWitnessTable(const NormalProtocolConformance *conf,
                                   llvm::Type *storageTy) {
  auto entity = LinkEntity::forDirectProtocolWitnessTable(conf);
  return getAddrOfLLVMVariable(entity, getPointerAlignment(), storageTy,
                               WitnessTableTy, DebugTypeInfo());
}

/// Should we be defining the given helper function?
static llvm::Function *shouldDefineHelper(IRGenModule &IGM,
                                          llvm::Constant *fn) {
  llvm::Function *def = dyn_cast<llvm::Function>(fn);
  if (!def) return nullptr;
  if (!def->empty()) return nullptr;

  def->setLinkage(llvm::Function::LinkOnceODRLinkage);
  def->setVisibility(llvm::Function::HiddenVisibility);
  def->setDoesNotThrow();
  def->setCallingConv(IGM.RuntimeCC);
  return def;
}

/// Get or create a helper function with the given name and type, lazily
/// using the given generation function to fill in its body.
///
/// The helper function will be shared between translation units within the
/// current linkage unit, so choose the name carefully to ensure that it
/// does not collide with any other helper function.  In general, it should
/// be a Swift-specific C reserved name; that is, it should start with
//  "__swift".
llvm::Constant *
IRGenModule::getOrCreateHelperFunction(StringRef fnName, llvm::Type *resultTy,
                                       ArrayRef<llvm::Type*> paramTys,
                        llvm::function_ref<void(IRGenFunction &IGF)> generate) {
  llvm::FunctionType *fnTy =
    llvm::FunctionType::get(resultTy, paramTys, false);

  llvm::Constant *fn = Module.getOrInsertFunction(fnName, fnTy);

  if (llvm::Function *def = shouldDefineHelper(*this, fn)) {
    IRGenFunction IGF(*this, def);
    if (DebugInfo)
      DebugInfo->emitArtificialFunction(IGF, def);
    generate(IGF);
  }

  return fn;
}

