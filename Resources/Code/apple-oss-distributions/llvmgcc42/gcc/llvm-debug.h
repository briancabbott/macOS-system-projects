/* LLVM LOCAL begin (ENTIRE FILE!)  */
/* Internal interfaces between the LLVM backend components
Copyright (C) 2006 Free Software Foundation, Inc.
Contributed by Jim Laskey  (jlaskey@apple.com)

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

//===----------------------------------------------------------------------===//
// This is a C++ header file that defines the debug interfaces shared among
// the llvm-*.cpp files.
//===----------------------------------------------------------------------===//

#ifndef LLVM_DEBUG_H
#define LLVM_DEBUG_H

#include "llvm-internal.h"
#include "llvm/Analysis/DebugInfo.h"
#include "llvm/Support/Dwarf.h"
#include "llvm/Support/ValueHandle.h"
#include "llvm/Support/Allocator.h"

extern "C" {
#include "llvm.h"
}  

#include <string>
#include <map>
#include <vector>

namespace llvm {

// Forward declarations
class AllocaInst;
class BasicBlock;
class CallInst;
class Function;
class Module;

/// DebugInfo - This class gathers all debug information during compilation and
/// is responsible for emitting to llvm globals or pass directly to the backend.
class DebugInfo {
private:
  Module *M;                            // The current module.
  DIFactory DebugFactory;               
  const char *CurFullPath;              // Previous location file encountered.
  int CurLineNo;                        // Previous location line# encountered.
  const char *PrevFullPath;             // Previous location file encountered.
  int PrevLineNo;                       // Previous location line# encountered.
  BasicBlock *PrevBB;                   // Last basic block encountered.
  DICompileUnit TheCU;                  // The compile unit.

  // Current GCC lexical block (or enclosing FUNCTION_DECL).
  tree_node *CurrentGCCLexicalBlock;	
  
  // This counter counts debug info for forward referenced subroutine types.
  // This counter is used to create unique name for such types so that their 
  // debug info (through MDNodes) is not shared accidently.
  unsigned FwdTypeCount;

  std::map<tree_node *, WeakVH > TypeCache;
                                        // Cache of previously constructed 
                                        // Types.
  std::map<tree_node *, WeakVH > SPCache;
                                        // Cache of previously constructed 
                                        // Subprograms.
  std::map<tree_node *, WeakVH> NameSpaceCache;
                                        // Cache of previously constructed name 
                                        // spaces.

  SmallVector<WeakVH, 4> RegionStack;
                                        // Stack to track declarative scopes.
  
  std::map<tree_node *, WeakVH> RegionMap;

  // Starting at the 'desired' BLOCK, recursively walk back to the
  // 'grand' context, and return pushing regions to make 'desired' the
  // current context.  'desired' should be a GCC lexical BLOCK.
  // 'grand' may be a BLOCK or a FUNCTION_DECL, and it's presumed to
  // be an ancestor of 'desired'.
  void push_regions(tree_node *desired, tree_node *grand);

  /// FunctionNames - This is a storage for function names that are
  /// constructed on demand. For example, C++ destructors, C++ operators etc..
  llvm::BumpPtrAllocator FunctionNames;

 public:
  DebugInfo(Module *m);

  /// Initialize - Initialize debug info by creating compile unit for
  /// main_input_filename. This must be invoked after language dependent
  /// initialization is done.
  void Initialize();

  // Accessors.
  void setLocationFile(const char *FullPath) { CurFullPath = FullPath; }
  void setLocationLine(int LineNo)           { CurLineNo = LineNo; }
  
  tree_node *getCurrentLexicalBlock() { return CurrentGCCLexicalBlock; }
  void setCurrentLexicalBlock(tree_node *lb) { CurrentGCCLexicalBlock = lb; }

  // Pop the current region/lexical-block back to 'grand', then push
  // regions to arrive at 'desired'.  This was inspired (cribbed from)
  // by GCC's cfglayout.c:change_scope().
  void change_regions(tree_node *desired, tree_node *grand);

  /// CreateSubprogramFromFnDecl - Constructs the debug code for entering a function -
  /// "llvm.dbg.func.start."
  DISubprogram CreateSubprogramFromFnDecl(tree_node *FnDecl);

  /// EmitFunctionStart - Constructs the debug code for entering a function -
  /// "llvm.dbg.func.start", and pushes it onto the RegionStack.
  void EmitFunctionStart(tree_node *FnDecl);

  /// EmitFunctionEnd - Constructs the debug code for exiting a declarative
  /// region - "llvm.dbg.region.end."
  void EmitFunctionEnd(BasicBlock *CurBB, bool EndFunction);

  /// EmitDeclare - Constructs the debug code for allocation of a new variable.
  /// region - "llvm.dbg.declare."
  void EmitDeclare(tree_node *decl, unsigned Tag, const char *Name,
                   tree_node *type, Value *AI, LLVMBuilder &Builder);

  /// EmitStopPoint - Emit a call to llvm.dbg.stoppoint to indicate a change of 
  /// source line.
  void EmitStopPoint(Function *Fn, BasicBlock *CurBB, LLVMBuilder &Builder);
                     
  /// EmitGlobalVariable - Emit information about a global variable.
  ///
  void EmitGlobalVariable(GlobalVariable *GV, tree_node *decl);

  /// getOrCreateType - Get the type from the cache or create a new type if
  /// necessary.
  DIType getOrCreateType(tree_node *type);

  /// createBasicType - Create BasicType.
  DIType createBasicType(tree_node *type);

  /// createMethodType - Create MethodType.
  DIType createMethodType(tree_node *type);

  /// createPointerType - Create PointerType.
  DIType createPointerType(tree_node *type);

  /// createArrayType - Create ArrayType.
  DIType createArrayType(tree_node *type);

  /// createEnumType - Create EnumType.
  DIType createEnumType(tree_node *type);

  /// createStructType - Create StructType for struct or union or class.
  DIType createStructType(tree_node *type);

  /// createVarinatType - Create variant type or return MainTy.
  DIType createVariantType(tree_node *type, DIType MainTy);

  /// getOrCreateCompileUnit - Create a new compile unit.
  DICompileUnit getOrCreateCompileUnit(const char *FullPath,
                                       bool isMain = false);

  /// getOrCreateFile - Get DIFile descriptor.
  DIFile getOrCreateFile(const char *FullPath);

  /// findRegion - Find tree_node N's region.
  DIDescriptor findRegion(tree_node *n);
  
  /// getOrCreateNameSpace - Get name space descriptor for the tree node.
  DINameSpace getOrCreateNameSpace(tree_node *Node, DIDescriptor Context);

  /// getFunctionName - Get function name for the given FnDecl. If the
  /// name is constructred on demand (e.g. C++ destructor) then the name
  /// is stored on the side.
  StringRef getFunctionName(tree_node *FnDecl);
};

} // end namespace llvm

#endif /* LLVM_DEBUG_H */
/* LLVM LOCAL end (ENTIRE FILE!)  */
