//===-------------- EscapeAnalysis.cpp - SIL Escape Analysis --------------===//
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

#define DEBUG_TYPE "sil-escape"
#include "swift/SILAnalysis/EscapeAnalysis.h"
#include "swift/SILAnalysis/BasicCalleeAnalysis.h"
#include "swift/SILAnalysis/CallGraphAnalysis.h"
#include "swift/SILAnalysis/ArraySemantic.h"
#include "swift/SILPasses/PassManager.h"
#include "swift/SIL/SILArgument.h"
#include "llvm/Support/GraphWriter.h"
#include "llvm/Support/raw_ostream.h"

using namespace swift;

static bool isProjection(ValueBase *V) {
  switch (V->getKind()) {
    case ValueKind::IndexAddrInst:
    case ValueKind::IndexRawPointerInst:
    case ValueKind::StructElementAddrInst:
    case ValueKind::TupleElementAddrInst:
    case ValueKind::UncheckedTakeEnumDataAddrInst:
    case ValueKind::StructExtractInst:
    case ValueKind::TupleExtractInst:
    case ValueKind::UncheckedEnumDataInst:
    case ValueKind::MarkDependenceInst:
    case ValueKind::PointerToAddressInst:
      return true;
    default:
      return false;
  }
}

static bool isNonWritableMemoryAddress(ValueBase *V) {
  switch (V->getKind()) {
    case ValueKind::FunctionRefInst:
    case ValueKind::WitnessMethodInst:
    case ValueKind::ClassMethodInst:
    case ValueKind::SuperMethodInst:
    case ValueKind::StringLiteralInst:
      // These instructions return pointers to memory which can't be a
      // destination of a store.
      return true;
    default:
      return false;
  }
}

static ValueBase *skipProjections(ValueBase *V) {
  for (;;) {
    if (!isProjection(V))
      return V;
    V = cast<SILInstruction>(V)->getOperand(0).getDef();
  }
  llvm_unreachable("there is no escape from an infinite loop");
}

void EscapeAnalysis::ConnectionGraph::clear() {
  Values2Nodes.clear();
  Nodes.clear();
  ReturnNode = nullptr;
  UsePoints.clear();
  UsePointsComputed = false;
  NodeAllocator.DestroyAll();
  assert(ToMerge.empty());
}

EscapeAnalysis::CGNode *EscapeAnalysis::ConnectionGraph::
getOrCreateNode(ValueBase *V) {
  CGNode * &Node = Values2Nodes[V];
  if (!Node) {
    if (SILArgument *Arg = dyn_cast<SILArgument>(V)) {
      if (Arg->isFunctionArg()) {
        Node = allocNode(V, NodeType::Argument);
        Node->mergeEscapeState(EscapeState::Arguments);
      } else {
        Node = allocNode(V, NodeType::Value);
      }
    } else {
      Node = allocNode(V, NodeType::Value);
    }
  }
  return Node->getMergeTarget();
}

EscapeAnalysis::CGNode *EscapeAnalysis::ConnectionGraph::getContentNode(
                                                          CGNode *AddrNode) {
  // Do we already have a content node (which is not necessarliy an immediate
  // successor of AddrNode)?
  if (AddrNode->pointsTo)
    return AddrNode->pointsTo;

  CGNode *Node = allocNode(AddrNode->V, NodeType::Content);
  updatePointsTo(AddrNode, Node);
  assert(ToMerge.empty() &&
         "Initially setting pointsTo should not require any node merges");
  return Node;
}

bool EscapeAnalysis::ConnectionGraph::addDeferEdge(CGNode *From, CGNode *To) {
  if (!From->addDefered(To))
    return false;

  CGNode *FromPointsTo = From->pointsTo;
  CGNode *ToPointsTo = To->pointsTo;
  if (FromPointsTo != ToPointsTo) {
    if (!ToPointsTo) {
      updatePointsTo(To, FromPointsTo->getMergeTarget());
      assert(ToMerge.empty() &&
             "Initially setting pointsTo should not require any node merges");
    } else {
      // We are adding an edge between two pointers which point to different
      // content nodes. This will require to merge the content nodes (and maybe
      // other content nodes as well), because of the graph invariance 4).
      updatePointsTo(From, ToPointsTo->getMergeTarget());
    }
  }
  return true;
}

void EscapeAnalysis::ConnectionGraph::mergeAllScheduledNodes() {
  while (!ToMerge.empty()) {
    CGNode *From = ToMerge.pop_back_val();
    CGNode *To = From->mergeTo;
    assert(To && "Node scheduled to merge but no merge target set");
    assert(!From->isMerged && "Merge source is already merged");
    assert(From->Type == NodeType::Content && "Can only merge content nodes");
    assert(To->Type == NodeType::Content && "Can only merge content nodes");

    // Unlink the predecessors and redirect the incoming pointsTo edge.
    // Note: we don't redirect the defer-edges because we don't want to trigger
    // updatePointsTo (which is called by addDeferEdge) right now.
    for (Predecessor Pred : From->Preds) {
      CGNode *PredNode = Pred.getPointer();
      if (Pred.getInt() == EdgeType::PointsTo) {
        assert(PredNode->getPointsToEdge() == From &&
               "Incoming pointsTo edge not set in predecessor");
        if (PredNode != From)
          PredNode->setPointsTo(To);
      } else {
        assert(PredNode != From);
        auto Iter = PredNode->findDefered(From);
        assert(Iter != PredNode->defersTo.end() &&
               "Incoming defer-edge not found in predecessor's defer list");
        PredNode->defersTo.erase(Iter);
      }
    }
    // Unlink and redirect the outgoing pointsTo edge.
    if (CGNode *PT = From->getPointsToEdge()) {
      if (PT != From) {
        PT->removeFromPreds(Predecessor(From, EdgeType::PointsTo));
      } else {
        PT = To;
      }
      if (CGNode *ExistingPT = To->getPointsToEdge()) {
        // The To node already has an outgoing pointsTo edge, so the only thing
        // we can do is to merge both content nodes.
        scheduleToMerge(ExistingPT, PT);
      } else {
        To->setPointsTo(PT);
      }
    }
    // Unlink the outgoing defer edges.
    for (CGNode *Defers : From->defersTo) {
      assert(Defers != From && "defer edge may not form a self-cycle");
      Defers->removeFromPreds(Predecessor(From, EdgeType::Defer));
    }
    // Redirect the incoming defer edges. This may trigger other node merges.
    for (Predecessor Pred : From->Preds) {
      CGNode *PredNode = Pred.getPointer();
      if (Pred.getInt() == EdgeType::Defer) {
        assert(PredNode != From && "defer edge may not form a self-cycle");
        addDeferEdge(PredNode, To);
      }
    }
    // Redirect the outgoing defer edges, which may also trigger other node
    // merges.
    for (CGNode *Defers : From->defersTo) {
      addDeferEdge(To, Defers);
    }

    // Ensure that graph invariance 4) is kept. At this point there may be still
    // some violations because of the new adjacent edges of the To node.
    for (Predecessor Pred : To->Preds) {
      if (Pred.getInt() == EdgeType::PointsTo) {
        CGNode *PredNode = Pred.getPointer();
        for (Predecessor PredOfPred : PredNode->Preds) {
          if (PredOfPred.getInt() == EdgeType::Defer)
            updatePointsTo(PredOfPred.getPointer(), To);
        }
      }
    }
    if (CGNode *ToPT = To->getPointsToEdge()) {
      for (CGNode *ToDef : To->defersTo) {
        updatePointsTo(ToDef, ToPT);
      }
      for (Predecessor Pred : To->Preds) {
        if (Pred.getInt() == EdgeType::Defer)
          updatePointsTo(Pred.getPointer(), ToPT);
      }
    }
    To->mergeEscapeState(From->State);

    // Cleanup the merged node.
    From->isMerged = true;
    From->Preds.clear();
    From->defersTo.clear();
    From->pointsTo = nullptr;
  }
}

void EscapeAnalysis::ConnectionGraph::
updatePointsTo(CGNode *InitialNode, CGNode *pointsTo) {
  // Visit all nodes in the defer web, which don't have the right pointsTo set.
  llvm::SmallVector<CGNode *, 8> WorkList;
  WorkList.push_back(InitialNode);
  InitialNode->isInWorkList = true;
  for (unsigned Idx = 0; Idx < WorkList.size(); ++Idx) {
    auto *Node = WorkList[Idx];
    if (Node->pointsTo == pointsTo)
      continue;

    if (Node->pointsTo) {
      // Mismatching: we need to merge!
      scheduleToMerge(Node->pointsTo, pointsTo);
    }

    // If the node already has a pointsTo _edge_ we don't change it (we don't
    // want to change the structure of the graph at this point).
    if (!Node->pointsToIsEdge) {
      if (Node->defersTo.empty()) {
        // This node is the end of a defer-edge path with no pointsTo connected.
        // We create an edge to pointsTo (agreed, this changes the structure of
        // the graph but adding this edge is harmless).
        Node->setPointsTo(pointsTo);
      } else {
        Node->pointsTo = pointsTo;
      }
    }

    // Add all adjacent nodes to the WorkList.
    for (auto *Defered : Node->defersTo) {
      if (!Defered->isInWorkList) {
        WorkList.push_back(Defered);
        Defered->isInWorkList = true;
      }
    }
    for (Predecessor Pred : Node->Preds) {
      if (Pred.getInt() == EdgeType::Defer) {
        CGNode *PredNode = Pred.getPointer();
        if (!PredNode->isInWorkList) {
          WorkList.push_back(PredNode);
          PredNode->isInWorkList = true;
        }
      }
    }
  }
  clearWorkListFlags(WorkList);
}

void EscapeAnalysis::ConnectionGraph::propagateEscapeStates() {
  bool Changed = false;
  do {
    Changed = false;

    for (CGNode *Node : Nodes) {
      // Propagate the state to all successor nodes.
      if (Node->pointsTo) {
        Changed |= Node->pointsTo->mergeEscapeState(Node->State);
      }
      for (CGNode *Def : Node->defersTo) {
        Changed |= Def->mergeEscapeState(Node->State);
      }
    }
  } while (Changed);
}

void EscapeAnalysis::ConnectionGraph::computeUsePoints() {
  if (UsePointsComputed)
    return;

  // First scan the whole function and add relevant instructions as use-points.
  for (auto &BB : *F) {
    for (SILArgument *BBArg : BB.getBBArgs()) {
      /// In addition to releasing instructions (see below) we also add block
      /// arguments as use points. In case of loops, block arguments can
      /// "extend" the liferange of a reference in upward direction.
      if (CGNode *ArgNode = getNodeOrNull(BBArg)) {
        addUsePoint(ArgNode, BBArg);
      }
    }

    for (auto &I : BB) {
      switch (I.getKind()) {
        case ValueKind::StrongReleaseInst:
        case ValueKind::ReleaseValueInst:
        case ValueKind::UnownedReleaseInst:
        case ValueKind::ApplyInst:
        case ValueKind::TryApplyInst: {
          /// Actually we only add instructions which may release a reference.
          /// We need the use points only for getting the end of a reference's
          /// liferange. And that must be a releaseing instruction.
          int ValueIdx = -1;
          for (const Operand &Op : I.getAllOperands()) {
            ValueBase *OpV = Op.get().getDef();
            if (CGNode *OpNd = getNodeOrNull(skipProjections(OpV))) {
              if (ValueIdx < 0) {
                ValueIdx = addUsePoint(OpNd, &I);
              } else {
                OpNd->setUsePointBit(ValueIdx);
              }
            }
          }
          break;
        }
        default:
          break;
      }
    }
  }

  // Second, we propagate the use-point information through the graph.
  bool Changed = false;
  do {
    Changed = false;

    for (CGNode *Node : Nodes) {
      // Propagate the bits to all successor nodes.
      if (Node->pointsTo) {
        Changed |= Node->pointsTo->mergeUsePoints(Node);
      }
      for (CGNode *Def : Node->defersTo) {
        Changed |= Def->mergeUsePoints(Node);
      }
    }
  } while (Changed);

  UsePointsComputed = true;
}

bool EscapeAnalysis::ConnectionGraph::mergeFrom(ConnectionGraph *SourceGraph,
                                                CGNodeMap &Mapping) {
  // The main point of the merging algorithm is to map each content node in the
  // source graph to a content node in this (destination) graph. This may
  // require to create new nodes or to merge existing nodes in this graph.

  // First step: replicate the points-to edges and the content nodes of the
  // source graph in this graph.
  bool Changed = false;
  bool NodesMerged;
  do {
    NodesMerged = false;
    for (unsigned Idx = 0; Idx < Mapping.getMappedNodes().size(); ++Idx) {
      CGNode *SourceNd = Mapping.getMappedNodes()[Idx];
      CGNode *DestNd = Mapping.get(SourceNd);
      assert(DestNd);
      
      if (SourceNd->getEscapeState() >= EscapeState::Global) {
        // We don't need to merge the source subgraph of nodes which have the
        // global escaping state set.
        // Just set global escaping in the caller node and that's it.
        Changed |= DestNd->mergeEscapeState(EscapeState::Global);
        continue;
      }

      CGNode *SourcePT = SourceNd->pointsTo;
      if (!SourcePT)
        continue;

      CGNode *MappedDestPT = Mapping.get(SourcePT);
      if (!DestNd->pointsTo) {
        // The following getContentNode() will create a new content node.
        Changed = true;
      }
      CGNode *DestPT = getContentNode(DestNd);
      if (MappedDestPT) {
        // We already found the destination node through another path.
        if (DestPT != MappedDestPT) {
          // There are two content nodes in this graph which map to the same
          // content node in the source graph -> we have to merge them.
          scheduleToMerge(DestPT, MappedDestPT);
          mergeAllScheduledNodes();
          Changed = true;
          NodesMerged = true;
        }
        assert(SourcePT->isInWorkList);
      } else {
        // It's the first time we see the destination node, so we add it to the
        // mapping.
        Mapping.add(SourcePT, DestPT);
      }
    }
  } while (NodesMerged);

  clearWorkListFlags(Mapping.getMappedNodes());

  // Second step: add the source graph's defer edges to this graph.
  llvm::SmallVector<CGNode *, 8> WorkList;
  for (CGNode *SourceNd : Mapping.getMappedNodes()) {
    assert(WorkList.empty());
    WorkList.push_back(SourceNd);
    SourceNd->isInWorkList = true;
    CGNode *DestFrom = Mapping.get(SourceNd);
    assert(DestFrom && "node should have been merged to the graph");

    // Collect all nodes which are reachable from the SourceNd via a path
    // which only contains defer-edges.
    for (unsigned Idx = 0; Idx < WorkList.size(); ++Idx) {
      CGNode *SourceReachable = WorkList[Idx];
      CGNode *DestReachable = Mapping.get(SourceReachable);
      // Create the edge in this graph. Note: this may trigger merging of
      // content nodes.
      if (DestReachable)
        Changed |= defer(DestFrom, DestReachable);

      for (auto *Defered : SourceReachable->defersTo) {
        if (!Defered->isInWorkList) {
          WorkList.push_back(Defered);
          Defered->isInWorkList = true;
        }
      }
    }
    clearWorkListFlags(WorkList);
    WorkList.clear();
  }
  return Changed;
}

EscapeAnalysis::CGNode *EscapeAnalysis::ConnectionGraph::
getNode(ValueBase *V, EscapeAnalysis *EA) {
  if (isa<FunctionRefInst>(V))
    return nullptr;

  if (!V->hasValue())
    return nullptr;

  if (!EA->isPointer(V))
    return nullptr;

  V = skipProjections(V);

  return getOrCreateNode(V);
}

/// Returns true if \p V is a use of \p Node, i.e. V may (indirectly)
/// somehow refer to the Node's value.
/// Use-points are only values which are relevant for lifeness computation,
/// e.g. release or apply instructions.
bool EscapeAnalysis::ConnectionGraph::isUsePoint(ValueBase *V, CGNode *Node) {
  assert(Node->getEscapeState() < EscapeState::Global &&
         "Use points are only valid for non-escaping nodes");
  computeUsePoints();
  auto Iter = UsePoints.find(V);
  if (Iter == UsePoints.end())
    return false;
  int Idx = Iter->second;
  if (Idx >= (int)Node->UsePoints.size())
    return false;
  return Node->UsePoints.test(Idx);
}


//===----------------------------------------------------------------------===//
//                      Dumping, Viewing and Verification
//===----------------------------------------------------------------------===//

#ifndef NDEBUG

/// For the llvm's GraphWriter we copy the connection graph into CGForDotView.
/// This makes iterating over the edges easier.
struct CGForDotView {

  enum EdgeTypes {
    PointsTo,
    Defered
  };

  struct Node {
    EscapeAnalysis::CGNode *OrigNode;
    CGForDotView *Graph;
    SmallVector<Node *, 8> Children;
    SmallVector<EdgeTypes, 8> ChildrenTypes;
  };

  CGForDotView(const EscapeAnalysis::ConnectionGraph *CG);
  
  std::string getNodeLabel(const Node *Node) const;
  
  std::string getNodeAttributes(const Node *Node) const;

  std::vector<Node> Nodes;

  SILFunction *F;

  const EscapeAnalysis::ConnectionGraph *OrigGraph;

  // The same IDs as the SILPrinter uses.
  llvm::DenseMap<const ValueBase *, unsigned> InstToIDMap;

  typedef std::vector<Node>::iterator iterator;
  typedef SmallVectorImpl<Node *>::iterator child_iterator;
};

CGForDotView::CGForDotView(const EscapeAnalysis::ConnectionGraph *CG) :
    F(CG->F), OrigGraph(CG) {
  Nodes.resize(CG->Nodes.size());
  llvm::DenseMap<EscapeAnalysis::CGNode *, Node *> Orig2Node;
  int idx = 0;
  for (auto *OrigNode : CG->Nodes) {
    if (OrigNode->isMerged)
      continue;

    Orig2Node[OrigNode] = &Nodes[idx++];
  }
  Nodes.resize(idx);
  CG->F->numberValues(InstToIDMap);

  idx = 0;
  for (auto *OrigNode : CG->Nodes) {
    if (OrigNode->isMerged)
      continue;

    auto &Nd = Nodes[idx++];
    Nd.Graph = this;
    Nd.OrigNode = OrigNode;
    if (auto *PT = OrigNode->getPointsToEdge()) {
      Nd.Children.push_back(Orig2Node[PT]);
      Nd.ChildrenTypes.push_back(PointsTo);
    }
    for (auto *Def : OrigNode->defersTo) {
      Nd.Children.push_back(Orig2Node[Def]);
      Nd.ChildrenTypes.push_back(Defered);
    }
  }
}

std::string CGForDotView::getNodeLabel(const Node *Node) const {
  std::string Label;
  llvm::raw_string_ostream O(Label);
  if (ValueBase *V = Node->OrigNode->V)
    O << '%' << InstToIDMap.lookup(V) << '\n';
  
  switch (Node->OrigNode->Type) {
    case swift::EscapeAnalysis::NodeType::Content:
      O << "content";
      break;
    case swift::EscapeAnalysis::NodeType::Return:
      O << "return";
      break;
    default: {
      std::string Inst;
      llvm::raw_string_ostream OI(Inst);
      SILValue(Node->OrigNode->V).print(OI);
      size_t start = Inst.find(" = ");
      if (start != std::string::npos) {
        start += 3;
      } else {
        start = 2;
      }
      O << Inst.substr(start, 20);
      break;
    }
  }
  if (!Node->OrigNode->matchPointToOfDefers()) {
    O << "\nPT mismatch: ";
    if (Node->OrigNode->pointsTo) {
      if (ValueBase *V = Node->OrigNode->pointsTo->V)
        O << '%' << Node->Graph->InstToIDMap[V];
    } else {
      O << "null";
    }
  }
  O.flush();
  return Label;
}

std::string CGForDotView::getNodeAttributes(const Node *Node) const {
  auto *Orig = Node->OrigNode;
  std::string attr;
  switch (Orig->Type) {
    case swift::EscapeAnalysis::NodeType::Content:
      attr = "style=\"rounded\"";
      break;
    case swift::EscapeAnalysis::NodeType::Argument:
    case swift::EscapeAnalysis::NodeType::Return:
      attr = "style=\"bold\"";
      break;
    default:
      break;
  }
  if (Orig->getEscapeState() != swift::EscapeAnalysis::EscapeState::None &&
      !attr.empty())
    attr += ',';
  
  switch (Orig->getEscapeState()) {
    case swift::EscapeAnalysis::EscapeState::None:
      break;
    case swift::EscapeAnalysis::EscapeState::Return:
      attr += "color=\"green\"";
      break;
    case swift::EscapeAnalysis::EscapeState::Arguments:
      attr += "color=\"blue\"";
      break;
    case swift::EscapeAnalysis::EscapeState::Global:
      attr += "color=\"red\"";
      break;
  }
  return attr;
}

namespace llvm {


  /// GraphTraits specialization so the CGForDotView can be
  /// iterable by generic graph iterators.
  template <> struct GraphTraits<CGForDotView::Node *> {
    typedef CGForDotView::Node NodeType;
    typedef CGForDotView::child_iterator ChildIteratorType;

    static NodeType *getEntryNode(NodeType *N) { return N; }
    static inline ChildIteratorType child_begin(NodeType *N) {
      return N->Children.begin();
    }
    static inline ChildIteratorType child_end(NodeType *N) {
      return N->Children.end();
    }
  };

  template <> struct GraphTraits<CGForDotView *>
  : public GraphTraits<CGForDotView::Node *> {
    typedef CGForDotView *GraphType;

    static NodeType *getEntryNode(GraphType F) { return nullptr; }

    typedef CGForDotView::iterator nodes_iterator;
    static nodes_iterator nodes_begin(GraphType OCG) {
      return OCG->Nodes.begin();
    }
    static nodes_iterator nodes_end(GraphType OCG) { return OCG->Nodes.end(); }
    static unsigned size(GraphType CG) { return CG->Nodes.size(); }
  };

  /// This is everything the llvm::GraphWriter needs to write the call graph in
  /// a dot file.
  template <>
  struct DOTGraphTraits<CGForDotView *> : public DefaultDOTGraphTraits {

    DOTGraphTraits(bool isSimple = false) : DefaultDOTGraphTraits(isSimple) {}

    static std::string getGraphName(const CGForDotView *Graph) {
      return "CG for " + Graph->F->getName().str();
    }

    std::string getNodeLabel(const CGForDotView::Node *Node,
                             const CGForDotView *Graph) {
      return Graph->getNodeLabel(Node);
    }

    static std::string getNodeAttributes(const CGForDotView::Node *Node,
                                         const CGForDotView *Graph) {
      return Graph->getNodeAttributes(Node);
    }

    static std::string getEdgeAttributes(const CGForDotView::Node *Node,
                                         CGForDotView::child_iterator I,
                                         const CGForDotView *Graph) {
      unsigned ChildIdx = I - Node->Children.begin();
      switch (Node->ChildrenTypes[ChildIdx]) {
        case CGForDotView::PointsTo: return "";
        case CGForDotView::Defered: return "color=\"gray\"";
      }
    }
  };
} // end llvm namespace

#endif

void EscapeAnalysis::ConnectionGraph::viewCG() const {
  /// When asserts are disabled, this should be a NoOp.
#ifndef NDEBUG
  CGForDotView CGDot(this);
  llvm::ViewGraph(&CGDot, "connection-graph");
#endif
}

void EscapeAnalysis::CGNode::dump() const {
  llvm::errs() << getTypeStr();
  if (V)
    llvm::errs() << ": " << *V;
  else
    llvm::errs() << '\n';

  if (mergeTo) {
    llvm::errs() << "   -> merged to ";
    mergeTo->dump();
  }
}

const char *EscapeAnalysis::CGNode::getTypeStr() const {
  switch (Type) {
    case NodeType::Value:      return "Val";
    case NodeType::Content:    return "Con";
    case NodeType::Argument:   return "Arg";
    case NodeType::Return:     return "Ret";
  }
}

void EscapeAnalysis::ConnectionGraph::dump() const {
  print(llvm::errs());
}

void EscapeAnalysis::ConnectionGraph::print(llvm::raw_ostream &OS) const {
#ifndef NDEBUG
  OS << "CG of " << F->getName() << '\n';

  // Assign the same IDs to SILValues as the SILPrinter does.
  llvm::DenseMap<const ValueBase *, unsigned> InstToIDMap;
  InstToIDMap[nullptr] = (unsigned)-1;
  F->numberValues(InstToIDMap);

  // Assign consecutive subindices for nodes which map to the same value.
  llvm::DenseMap<const ValueBase *, unsigned> NumSubindicesPerValue;
  llvm::DenseMap<CGNode *, unsigned> Node2Subindex;

  // Sort by SILValue ID+Subindex. To make the output somehow consistent with
  // the output of the function's SIL.
  auto sortNodes = [&](llvm::SmallVectorImpl<CGNode *> &Nodes) {
    std::sort(Nodes.begin(), Nodes.end(),
      [&](CGNode *Nd1, CGNode *Nd2) -> bool {
        unsigned VIdx1 = InstToIDMap[Nd1->V];
        unsigned VIdx2 = InstToIDMap[Nd2->V];
        if (VIdx1 != VIdx2)
          return VIdx1 < VIdx2;
        return Node2Subindex[Nd1] < Node2Subindex[Nd2];
      });
  };

  auto NodeStr = [&](CGNode *Nd) -> std::string {
    std::string Str;
    if (Nd->V) {
      llvm::raw_string_ostream OS(Str);
      OS << '%' << InstToIDMap[Nd->V];
      unsigned Idx = Node2Subindex[Nd];
      if (Idx != 0)
        OS << '.' << Idx;
      OS.flush();
    }
    return Str;
  };

  llvm::SmallVector<CGNode *, 8> SortedNodes;
  for (CGNode *Nd : Nodes) {
    if (!Nd->isMerged) {
      unsigned &Idx = NumSubindicesPerValue[Nd->V];
      Node2Subindex[Nd] = Idx++;
      SortedNodes.push_back(Nd);
    }
  }
  sortNodes(SortedNodes);

  llvm::DenseMap<int, ValueBase *> Idx2UsePoint;
  for (auto Iter : UsePoints) {
    Idx2UsePoint[Iter.second] = Iter.first;
  }

  for (CGNode *Nd : SortedNodes) {
    OS << "  " << Nd->getTypeStr() << ' ' << NodeStr(Nd) << " Esc: ";
    switch (Nd->getEscapeState()) {
      case EscapeState::None: {
        const char *Separator = "";
        for (unsigned VIdx = Nd->UsePoints.find_first(); VIdx != -1u;
             VIdx = Nd->UsePoints.find_next(VIdx)) {
          ValueBase *V = Idx2UsePoint[VIdx];
          OS << Separator << '%' << InstToIDMap[V];
          Separator = ",";
        }
        break;
      }
      case EscapeState::Return:
        OS << 'R';
        break;
      case EscapeState::Arguments:
        OS << 'A';
        break;
      case EscapeState::Global:
        OS << 'G';
        break;
    }
    OS << ", Succ: ";
    const char *Separator = "";
    if (CGNode *PT = Nd->getPointsToEdge()) {
      OS << '(' << NodeStr(PT) << ')';
      Separator = ", ";
    }
    llvm::SmallVector<CGNode *, 8> SortedDefers = Nd->defersTo;
    sortNodes(SortedDefers);
    for (CGNode *Def : SortedDefers) {
      OS << Separator << NodeStr(Def);
      Separator = ", ";
    }
    OS << '\n';
  }
  OS << "End\n";
#endif
}

void EscapeAnalysis::ConnectionGraph::verify() const {
#ifndef NDEBUG
  verifyStructure();

  // Check graph invariance 4)
  for (CGNode *Nd : Nodes) {
    assert(Nd->matchPointToOfDefers());
  }
#endif
}

void EscapeAnalysis::ConnectionGraph::verifyStructure() const {
#ifndef NDEBUG
  for (CGNode *Nd : Nodes) {
    if (Nd->isMerged) {
      assert(Nd->mergeTo);
      assert(!Nd->pointsTo);
      assert(Nd->defersTo.empty());
      assert(Nd->Preds.empty());
      assert(Nd->Type == NodeType::Content);
      continue;
    }
    // Check if predecessor and successor edges are linked correctly.
    for (Predecessor Pred : Nd->Preds) {
      CGNode *PredNode = Pred.getPointer();
      if (Pred.getInt() == EdgeType::Defer) {
        assert(PredNode->findDefered(Nd) != PredNode->defersTo.end());
      } else {
        assert(Pred.getInt() == EdgeType::PointsTo);
        assert(PredNode->getPointsToEdge() == Nd);
      }
    }
    for (CGNode *Def : Nd->defersTo) {
      assert(Def->findPred(Predecessor(Nd, EdgeType::Defer)) != Def->Preds.end());
      assert(Def != Nd);
    }
    if (CGNode *PT = Nd->getPointsToEdge()) {
      assert(PT->Type == NodeType::Content);
      assert(PT->findPred(Predecessor(Nd, EdgeType::PointsTo)) != PT->Preds.end());
    }
  }
#endif
}

//===----------------------------------------------------------------------===//
//                          EscapeAnalysis
//===----------------------------------------------------------------------===//

EscapeAnalysis::EscapeAnalysis(SILModule *M) :
  SILAnalysis(AnalysisKind::Escape), M(M),
  ArrayType(M->getASTContext().getArrayDecl()),
  CGA(nullptr), shouldRecompute(true) {
}


void EscapeAnalysis::initialize(SILPassManager *PM) {
  BCA = PM->getAnalysis<BasicCalleeAnalysis>();
  CGA = PM->getAnalysis<CallGraphAnalysis>();
}

/// Returns true if we need to add defer edges for the arguments of a block.
static bool linkBBArgs(SILBasicBlock *BB) {
  // Don't need to handle function arguments.
  if (BB == &BB->getParent()->front())
    return false;
  // We don't need to link to the try_apply's normal result argument, because
  // we handle it separatly in setAllEscaping() and mergeCalleeGraph().
  if (SILBasicBlock *SinglePred = BB->getSinglePredecessor()) {
    auto *TAI = dyn_cast<TryApplyInst>(SinglePred->getTerminator());
    if (TAI && BB == TAI->getNormalBB())
      return false;
  }
  return true;
}

/// Returns true if the type \p Ty is a reference or transitively contains
/// a reference, i.e. if it is a "pointer" type.
static bool isOrContainsReference(SILType Ty, SILModule *Mod) {
  if (Ty.hasReferenceSemantics())
    return true;

  if (Ty.getSwiftType() == Mod->getASTContext().TheRawPointerType)
    return true;

  if (auto *Str = Ty.getStructOrBoundGenericStruct()) {
    for (auto *Field : Str->getStoredProperties()) {
      if (isOrContainsReference(Ty.getFieldType(Field, *Mod), Mod))
        return true;
    }
    return false;
  }
  if (auto TT = Ty.getAs<TupleType>()) {
    for (unsigned i = 0, e = TT->getNumElements(); i != e; ++i) {
      if (isOrContainsReference(Ty.getTupleElementType(i), Mod))
        return true;
    }
    return false;
  }
  if (auto En = Ty.getEnumOrBoundGenericEnum()) {
    for (auto *ElemDecl : En->getAllElements()) {
      if (ElemDecl->hasArgumentType() &&
          isOrContainsReference(Ty.getEnumElementType(ElemDecl, *Mod), Mod))
        return true;
    }
    return false;
  }
  return false;
}

bool EscapeAnalysis::isPointer(ValueBase *V) {
  assert(V->hasValue());
  SILType Ty = V->getType(0);
  auto Iter = isPointerCache.find(Ty);
  if (Iter != isPointerCache.end())
    return Iter->second;

  bool IP = (Ty.isAddress() || Ty.isLocalStorage() ||
             isOrContainsReference(Ty, M));
  isPointerCache[Ty] = IP;
  return IP;
}

void EscapeAnalysis::buildConnectionGraphs(FunctionInfo *FInfo) {
  ConnectionGraph *ConGraph = &FInfo->Graph;
  // We use a worklist for iteration to visit the blocks in dominance order.
  llvm::SmallPtrSet<SILBasicBlock*, 32> VisitedBlocks;
  llvm::SmallVector<SILBasicBlock *, 16> WorkList;
  VisitedBlocks.insert(&*ConGraph->F->begin());
  WorkList.push_back(&*ConGraph->F->begin());

  while (!WorkList.empty()) {
    SILBasicBlock *BB = WorkList.pop_back_val();

    // Create edges for the instructions.
    for (auto &I : *BB) {
      analyzeInstruction(&I, FInfo);
    }
    for (auto &Succ : BB->getSuccessors()) {
      if (VisitedBlocks.insert(Succ.getBB()).second)
        WorkList.push_back(Succ.getBB());
    }
  }

  // Second step: create defer-edges for block arguments.
  for (SILBasicBlock &BB : *ConGraph->F) {
    if (!linkBBArgs(&BB))
      continue;

    // Create defer-edges from the block arguments to it's values in the
    // predecessor's terminator instructions.
    for (SILArgument *BBArg : BB.getBBArgs()) {
      CGNode *ArgNode = ConGraph->getNode(BBArg, this);
      if (!ArgNode)
        continue;

      llvm::SmallVector<SILValue,4> Incoming;
      if (!BBArg->getIncomingValues(Incoming)) {
        // We don't know where the block argument comes from -> treat it
        // conservatively.
        ConGraph->setEscapesGlobal(ArgNode);
        continue;
      }

      for (SILValue Src : Incoming) {
        CGNode *SrcArg = ConGraph->getNode(Src, this);
        if (SrcArg) {
          ConGraph->defer(ArgNode, SrcArg);
        } else {
          ConGraph->setEscapesGlobal(ArgNode);
          break;
        }
      }
    }
  }

  ConGraph->propagateEscapeStates();
  mergeSummaryGraph(&FInfo->SummaryGraph, ConGraph);
  FInfo->Valid = true;
}

bool EscapeAnalysis::allCalleeFunctionsVisible(FullApplySite FAS) {
  auto Callees = BCA->getCalleeList(FAS);
  if (Callees.isIncomplete())
    return false;

  for (SILFunction *Callee : Callees) {
    if (Callee->isExternalDeclaration())
      return false;
  }
  return true;
}

void EscapeAnalysis::analyzeInstruction(SILInstruction *I,
                                        FunctionInfo *FInfo) {
  ConnectionGraph *ConGraph = &FInfo->Graph;
  FullApplySite FAS = FullApplySite::isa(I);
  if (FAS) {
    ArraySemanticsCall ASC(FAS.getInstruction());
    switch (ASC.getKind()) {
      case ArrayCallKind::kArrayPropsIsNative:
      case ArrayCallKind::kArrayPropsIsNativeTypeChecked:
      case ArrayCallKind::kCheckSubscript:
      case ArrayCallKind::kCheckIndex:
      case ArrayCallKind::kGetCount:
      case ArrayCallKind::kGetCapacity:
      case ArrayCallKind::kMakeMutable:
        // These array semantics calls do not capture anything.
        return;
      case ArrayCallKind::kArrayUninitialized:
        // array.uninitialized may have a first argument which is the
        // allocated array buffer. The call is like a struct(buffer)
        // instruction.
        if (CGNode *BufferNode = ConGraph->getNode(FAS.getArgument(0), this)) {
          ConGraph->defer(ConGraph->getNode(I, this), BufferNode);
        }
        return;
      case ArrayCallKind::kGetArrayOwner:
        if (CGNode *BufferNode = ConGraph->getNode(ASC.getSelf(), this)) {
          ConGraph->defer(ConGraph->getNode(I, this), BufferNode);
        }
        return;
      case ArrayCallKind::kGetElement:
        // This is like a load from a ref_element_addr.
        if (FAS.getArgument(0).getType().isAddress()) {
          if (CGNode *AddrNode = ConGraph->getNode(ASC.getSelf(), this)) {
            if (CGNode *DestNode = ConGraph->getNode(FAS.getArgument(0), this)) {
              // One content node for going from the array buffer pointer to
              // the element address (like ref_element_addr).
              CGNode *RefElement = ConGraph->getContentNode(AddrNode);
              // Another content node to actually load the element.
              CGNode *ArrayContent = ConGraph->getContentNode(RefElement);
              // The content of the destination address.
              CGNode *DestContent = ConGraph->getContentNode(DestNode);
              ConGraph->defer(DestContent, ArrayContent);
              return;
            }
          }
        }
        break;
      case ArrayCallKind::kGetElementAddress:
        // This is like a ref_element_addr.
        if (CGNode *SelfNode = ConGraph->getNode(ASC.getSelf(), this)) {
          ConGraph->defer(ConGraph->getNode(I, this),
                          ConGraph->getContentNode(SelfNode));
        }
        return;
      default:
        break;
    }

    if (allCalleeFunctionsVisible(FAS)) {
      // We handle this apply site afterwards by merging the callee graph(s)
      // into the caller graph.
      FInfo->KnownCallees.push_back(FAS);
      return;
    }

    if (auto *Fn = FAS.getCalleeFunction()) {
      if (Fn->getName() == "swift_bufferAllocate")
        // The call is a buffer allocation, e.g. for Array.
        return;
    }
  }
  if (isProjection(I))
    return;

  // Instructions which return the address of non-writable memory cannot have
  // an effect on escaping.
  if (isNonWritableMemoryAddress(I))
    return;

  switch (I->getKind()) {
    case ValueKind::AllocStackInst:
    case ValueKind::AllocRefInst:
    case ValueKind::AllocBoxInst:
    case ValueKind::DeallocStackInst:
    case ValueKind::StrongRetainInst:
    case ValueKind::StrongRetainUnownedInst:
    case ValueKind::RetainValueInst:
    case ValueKind::UnownedRetainInst:
    case ValueKind::BranchInst:
    case ValueKind::CondBranchInst:
    case ValueKind::SwitchEnumInst:
    case ValueKind::DebugValueInst:
    case ValueKind::DebugValueAddrInst:
      // These instructions don't have any effect on escaping.
      return;
    case ValueKind::StrongReleaseInst:
    case ValueKind::ReleaseValueInst:
    case ValueKind::UnownedReleaseInst: {
      SILValue OpV = I->getOperand(0);
      if (CGNode *AddrNode = ConGraph->getNode(OpV, this)) {
        // A release instruction may deallocate the pointer operand. This may
        // capture any content of the released object, but not the pointer to
        // the object itself (because it will be a dangling pointer after
        // deallocation).
        CGNode *CapturedByDeinit = ConGraph->getContentNode(AddrNode);
        CapturedByDeinit = ConGraph->getContentNode(CapturedByDeinit);
        if (isArrayOrArrayStorage(OpV)) {
          CapturedByDeinit = ConGraph->getContentNode(CapturedByDeinit);
        }
        ConGraph->setEscapesGlobal(CapturedByDeinit);
      }
      return;
    }
    case ValueKind::LoadInst:
    case ValueKind::LoadWeakInst:
    // We treat ref_element_addr like a load (see NodeType::Content).
    case ValueKind::RefElementAddrInst:
      if (isPointer(I)) {
        CGNode *AddrNode = ConGraph->getNode(I->getOperand(0), this);
        if (!AddrNode) {
          // A load from an address we don't handle -> be conservative.
          CGNode *ValueNode = ConGraph->getNode(I, this);
          ConGraph->setEscapesGlobal(ValueNode);
          return;
        }
        CGNode *PointsTo = ConGraph->getContentNode(AddrNode);
        // No need for a separate node for the load instruction:
        // just reuse the content node.
        ConGraph->setNode(I, PointsTo);
      }
      return;
    case ValueKind::StoreInst:
    case ValueKind::StoreWeakInst:
      if (CGNode *ValueNode = ConGraph->getNode(I->getOperand(StoreInst::Src), this)) {
        CGNode *AddrNode = ConGraph->getNode(I->getOperand(StoreInst::Dest), this);
        if (AddrNode) {
          // Create a defer-edge from the content to the stored value.
          CGNode *PointsTo = ConGraph->getContentNode(AddrNode);
          ConGraph->defer(PointsTo, ValueNode);
        } else {
          // A store to an address we don't handle -> be conservative.
          ConGraph->setEscapesGlobal(ValueNode);
        }
      }
      return;
    case ValueKind::PartialApplyInst: {
      // The result of a partial_apply is a thick function which stores the
      // boxed partial applied arguments. We create defer-edges from the
      // partial_apply values to the arguments.
      CGNode *ResultNode = ConGraph->getNode(I, this);
      assert(ResultNode && "thick functions must have a CG node");
      for (const Operand &Op : I->getAllOperands()) {
        if (CGNode *ArgNode = ConGraph->getNode(Op.get(), this)) {
          ConGraph->defer(ResultNode, ArgNode);
        }
      }
      return;
    }
    case ValueKind::StructInst:
    case ValueKind::TupleInst:
    case ValueKind::EnumInst: {
      // Aggregate composition is like assigning the aggregate fields to the
      // resulting aggregate value.
      CGNode *ResultNode = nullptr;
      for (const Operand &Op : I->getAllOperands()) {
        if (CGNode *FieldNode = ConGraph->getNode(Op.get(), this)) {
          if (!ResultNode) {
            // A small optimization to reduce the graph size: we re-use the
            // first field node as result node.
            ConGraph->setNode(I, FieldNode);
            ResultNode = FieldNode;
            assert(isPointer(I));
          } else {
            ConGraph->defer(ResultNode, FieldNode);
          }
        }
      }
      return;
    }
    case ValueKind::UncheckedRefCastInst:
      // A cast is almost like a projection.
      if (CGNode *OpNode = ConGraph->getNode(I->getOperand(0), this)) {
        ConGraph->setNode(I, OpNode);
      }
      break;
    case ValueKind::ReturnInst:
      if (CGNode *ValueNd = ConGraph->getNode(cast<ReturnInst>(I)->getOperand(), this)) {
        ConGraph->defer(ConGraph->getReturnNode(),
                                 ValueNd);
      }
      return;
    default:
      // We handle all other instructions conservatively.
      setAllEscaping(I, ConGraph);
      return;
  }
}

bool EscapeAnalysis::isArrayOrArrayStorage(SILValue V) {
  for (;;) {
    if (V.getType().getNominalOrBoundGenericNominal() == ArrayType)
      return true;

    if (!isProjection(V.getDef()))
      return false;

    V = dyn_cast<SILInstruction>(V.getDef())->getOperand(0);
  }
}

void EscapeAnalysis::setAllEscaping(SILInstruction *I,
                                    ConnectionGraph *ConGraph) {
  if (auto *TAI = dyn_cast<TryApplyInst>(I)) {
    setEscapesGlobal(ConGraph, TAI->getNormalBB()->getBBArg(0));
    setEscapesGlobal(ConGraph, TAI->getErrorBB()->getBBArg(0));
  }
  // Even if the instruction does not write memory we conservatively set all
  // operands to escaping, because they may "escape" to the result value in
  // an unspecified way. For example consider bit-casting a pointer to an int.
  // In this case we don't even create a node for the resulting int value.
  for (const Operand &Op : I->getAllOperands()) {
    SILValue OpVal = Op.get();
    if (!isNonWritableMemoryAddress(OpVal.getDef()))
      setEscapesGlobal(ConGraph, OpVal);
  }
  // Even if the instruction does not write memory it could e.g. return the
  // address of global memory. Therefore we have to define it as escaping.
  setEscapesGlobal(ConGraph, I);
}

void EscapeAnalysis::recompute() {

  // Did anything change since the last recompuation? (Probably yes)
  if (!shouldRecompute)
    return;

  DEBUG(llvm::dbgs() << "recompute escape analysis\n");

  Function2Info.clear();
  Allocator.DestroyAll();
  shouldRecompute = false;

  CallGraph &CG = CGA->getOrBuildCallGraph();

  // TODO: Remove this workaround when the bottom-up function order is
  // updated automatically.
  CG.invalidateBottomUpFunctionOrder();

  auto BottomUpFunctions = CG.getBottomUpFunctionOrder();
  std::vector<FunctionInfo *> FInfos;
  FInfos.reserve(BottomUpFunctions.size());

  // First step: create the initial connection graphs for all functions.
  for (SILFunction *F : BottomUpFunctions) {
    if (F->isExternalDeclaration())
      continue;

    DEBUG(llvm::dbgs() << "  build initial graph for " << F->getName() << '\n');

    auto *FInfo = new (Allocator.Allocate()) FunctionInfo(F);
    Function2Info[F] = FInfo;
    buildConnectionGraphs(FInfo);

    FInfos.push_back(FInfo);
    FInfo->NeedMergeCallees = true;
  }

  // Second step: propagate the connection graphs up the call tree until it
  // stabalizes.
  int Iteration = 0;
  bool Changed;
  do {
    DEBUG(llvm::dbgs() << "iteration " << Iteration << '\n');
    Changed = false;
    for (FunctionInfo *FInfo : FInfos) {
      if (!FInfo->NeedMergeCallees)
        continue;
      FInfo->NeedMergeCallees = false;

      // Limit the total number of iterations. First to limit compile time,
      // second to make sure that the loop terminates. Theoretically this
      // should always be the case, but who knows?
      if (Iteration >= MaxGraphMerges) {
        DEBUG(llvm::dbgs() << "  finalize " <<
              FInfo->Graph.F->getName() << '\n');
        finalizeGraphsConservatively(FInfo);
        continue;
      }

      DEBUG(llvm::dbgs() << "  merge into " <<
            FInfo->Graph.F->getName() << '\n');

      // Merge the callee-graphs into the graph of the current function.
      if (!mergeAllCallees(FInfo, CG))
        continue;

      FInfo->Graph.propagateEscapeStates();

      // Derive the summary graph of the current function. Even if the
      // complete graph of the function did change, it does not mean that the
      // summary graph will change.
      if (!mergeSummaryGraph(&FInfo->SummaryGraph, &FInfo->Graph))
        continue;

      // Trigger another graph merging action in all caller functions.
      auto &CallerSet = CG.getCallerEdges(FInfo->Graph.F);
      for (CallGraphEdge *CallerEdge : CallerSet) {
        if (!CallerEdge->canCallUnknownFunction()) {
          SILFunction *Caller = CallerEdge->getInstruction()->getFunction();
          FunctionInfo *CallerInfo = Function2Info.lookup(Caller);
          assert(CallerInfo && "We should have an info for all functions");
          CallerInfo->NeedMergeCallees = true;
        }
      }
      Changed = true;
    }
    Iteration++;
  } while (Changed);

  verify();
}

bool EscapeAnalysis::mergeAllCallees(FunctionInfo *FInfo, CallGraph &CG) {
  bool Changed = false;
  for (FullApplySite FAS : FInfo->KnownCallees) {
    auto Callees = CG.getCallees(FAS.getInstruction());
    assert(!Callees.canCallUnknownFunction() &&
           "knownCallees should not contain an unknown function");
    for (SILFunction *Callee : Callees) {
      DEBUG(llvm::dbgs() << "    callee " << Callee->getName() << '\n');
      FunctionInfo *CalleeInfo = Function2Info.lookup(Callee);
      assert(CalleeInfo && "We should have an info for all functions");
      Changed |= mergeCalleeGraph(FAS, &FInfo->Graph, &CalleeInfo->SummaryGraph);
    }
  }
  return Changed;
}

bool EscapeAnalysis::mergeCalleeGraph(FullApplySite FAS,
                                      ConnectionGraph *CallerGraph,
                                      ConnectionGraph *CalleeGraph) {
  CGNodeMap Callee2CallerMapping;

  // First map the callee parameters to the caller arguments.
  SILFunction *Callee = CalleeGraph->F;
  unsigned numCallerArgs = FAS.getNumArguments();
  unsigned numCalleeArgs = Callee->getArguments().size();
  assert(numCalleeArgs >= numCallerArgs);
  for (unsigned Idx = 0; Idx < numCalleeArgs; ++Idx) {
    // If there are more callee parameters than arguments it means that the
    // callee is the result of a partial_apply - a thick function. A thick
    // function also references the boxed partially applied arguments.
    // Therefore we map all the extra callee paramters to the callee operand
    // of the apply site.
    SILValue CallerArg = (Idx < numCallerArgs ? FAS.getArgument(Idx) :
                          FAS.getCallee());
    if (CGNode *CalleeNd = CalleeGraph->getNode(Callee->getArgument(Idx), this)) {
      Callee2CallerMapping.add(CalleeNd, CallerGraph->getNode(CallerArg, this));
    }
  }

  // Map the return value.
  if (CGNode *RetNd = CalleeGraph->getReturnNodeOrNull()) {
    ValueBase *CallerReturnVal = nullptr;
    if (auto *TAI = dyn_cast<TryApplyInst>(FAS.getInstruction())) {
      CallerReturnVal = TAI->getNormalBB()->getBBArg(0);
    } else {
      CallerReturnVal = FAS.getInstruction();
    }
    CGNode *CallerRetNd = CallerGraph->getNode(CallerReturnVal, this);
    Callee2CallerMapping.add(RetNd, CallerRetNd);
  }
  return CallerGraph->mergeFrom(CalleeGraph, Callee2CallerMapping);
}

bool EscapeAnalysis::mergeSummaryGraph(ConnectionGraph *SummaryGraph,
                                        ConnectionGraph *Graph) {

  // Make a 1-to-1 mapping of all arguments and the return value.
  CGNodeMap Mapping;
  for (SILArgument *Arg : Graph->F->getArguments()) {
    if (CGNode *ArgNd = Graph->getNode(Arg, this)) {
      Mapping.add(ArgNd, SummaryGraph->getNode(Arg, this));
    }
  }
  if (CGNode *RetNd = Graph->getReturnNodeOrNull()) {
    Mapping.add(RetNd, SummaryGraph->getReturnNode());
  }
  // Merging actually creates the summary graph.
  return SummaryGraph->mergeFrom(Graph, Mapping);
}


void EscapeAnalysis::finalizeGraphsConservatively(FunctionInfo *FInfo) {
  for (FullApplySite FAS : FInfo->KnownCallees) {
    setAllEscaping(FAS.getInstruction(), &FInfo->Graph);
  }
  FInfo->Graph.propagateEscapeStates();
  mergeSummaryGraph(&FInfo->SummaryGraph, &FInfo->Graph);
}

SILAnalysis *swift::createEscapeAnalysis(SILModule *M) {
  return new EscapeAnalysis(M);
}
