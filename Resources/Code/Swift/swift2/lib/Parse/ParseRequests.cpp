//===--- ParseRequests.cpp - Parsing Requests -----------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2017 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// Lazy parsing requests
//
//===----------------------------------------------------------------------===//
#include "swift/AST/ParseRequests.h"
#include "swift/AST/ASTContext.h"
#include "swift/AST/Decl.h"
#include "swift/AST/DeclContext.h"
#include "swift/AST/Module.h"
#include "swift/AST/SourceFile.h"
#include "swift/Basic/Defer.h"
#include "swift/Parse/Parser.h"
#include "swift/Subsystems.h"

#if SWIFT_SWIFT_PARSER
#include "SwiftCompilerSupport.h"
#endif

using namespace swift;

namespace swift {
// Implement the type checker type zone (zone 10).
#define SWIFT_TYPEID_ZONE Parse
#define SWIFT_TYPEID_HEADER "swift/AST/ParseTypeIDZone.def"
#include "swift/Basic/ImplementTypeIDZone.h"
#undef SWIFT_TYPEID_ZONE
#undef SWIFT_TYPEID_HEADER
}

void swift::simple_display(llvm::raw_ostream &out,
                           const FingerprintAndMembers &value) {
  if (value.fingerprint)
    simple_display(out, *value.fingerprint);
  else
    out << "<no fingerprint>";
  out << ", ";
  simple_display(out, value.members);
}

FingerprintAndMembers
ParseMembersRequest::evaluate(Evaluator &evaluator,
                              IterableDeclContext *idc) const {
  SourceFile *sf = idc->getAsGenericContext()->getParentSourceFile();
  ASTContext &ctx = idc->getDecl()->getASTContext();
  auto fileUnit
    = dyn_cast<FileUnit>(idc->getAsGenericContext()->getModuleScopeContext());
  if (!sf) {
    // If there is no parent source file, this is a deserialized or synthesized
    // declaration context, in which case `getMembers()` has all of the members.
    // Filter out the implicitly-generated ones.
    SmallVector<Decl *, 4> members;
    for (auto decl : idc->getMembers()) {
      if (!decl->isImplicit()) {
        members.push_back(decl);
      }
    }

    Optional<Fingerprint> fp = None;
    if (!idc->getDecl()->isImplicit() && fileUnit) {
      fp = fileUnit->loadFingerprint(idc);
    }
    return FingerprintAndMembers{fp, ctx.AllocateCopy(members)};
  }

  unsigned bufferID = *sf->getBufferID();

  // Lexer diagnostics have been emitted during skipping, so we disable lexer's
  // diagnostic engine here.
  Parser parser(bufferID, *sf, /*No Lexer Diags*/nullptr, nullptr, nullptr);
  auto declsAndHash = parser.parseDeclListDelayed(idc);
  FingerprintAndMembers fingerprintAndMembers = {declsAndHash.second,
                                                 declsAndHash.first};
  return FingerprintAndMembers{
      fingerprintAndMembers.fingerprint,
      ctx.AllocateCopy(llvm::makeArrayRef(fingerprintAndMembers.members))};
}

BodyAndFingerprint
ParseAbstractFunctionBodyRequest::evaluate(Evaluator &evaluator,
                                           AbstractFunctionDecl *afd) const {
  using BodyKind = AbstractFunctionDecl::BodyKind;

  switch (afd->getBodyKind()) {
  case BodyKind::Deserialized:
  case BodyKind::SILSynthesize:
  case BodyKind::None:
  case BodyKind::Skipped:
    return {};

  case BodyKind::TypeChecked:
  case BodyKind::Parsed:
    return afd->BodyAndFP;

  case BodyKind::Synthesize: {
    BraceStmt *body;
    bool isTypeChecked;

    std::tie(body, isTypeChecked) = (afd->Synthesizer.Fn)(
        afd, afd->Synthesizer.Context);
    assert(body && "cannot synthesize a null body");
    afd->setBodyKind(isTypeChecked ? BodyKind::TypeChecked : BodyKind::Parsed);
    return {body, Fingerprint::ZERO()};
  }

  case BodyKind::Unparsed: {
    // FIXME: How do we configure code completion?
    SourceFile &sf = *afd->getDeclContext()->getParentSourceFile();
    SourceManager &sourceMgr = sf.getASTContext().SourceMgr;
    unsigned bufferID =
        sourceMgr.findBufferContainingLoc(afd->getBodySourceRange().Start);
    Parser parser(bufferID, sf, /*SIL*/ nullptr);
    auto result = parser.parseAbstractFunctionBodyDelayed(afd);
    afd->setBodyKind(BodyKind::Parsed);
    return result;
  }
  }
  llvm_unreachable("Unhandled BodyKind in switch");
}

//----------------------------------------------------------------------------//
// ParseSourceFileRequest computation.
//----------------------------------------------------------------------------//

/// A thunk that deletes an allocated PersistentParserState. This is needed for
/// us to be able to forward declare a unique_ptr to the state in the AST.
static void deletePersistentParserState(PersistentParserState *state) {
  delete state;
}

SourceFileParsingResult ParseSourceFileRequest::evaluate(Evaluator &evaluator,
                                                         SourceFile *SF) const {
  assert(SF);
  auto &ctx = SF->getASTContext();
  auto bufferID = SF->getBufferID();

  // If there's no buffer, there's nothing to parse.
  if (!bufferID)
    return {};

  // If we've been asked to silence warnings, do so now. This is needed for
  // secondary files, which can be parsed multiple times.
  auto &diags = ctx.Diags;
  auto didSuppressWarnings = diags.getSuppressWarnings();
  auto shouldSuppress = SF->getParsingOptions().contains(
      SourceFile::ParsingFlags::SuppressWarnings);
  diags.setSuppressWarnings(didSuppressWarnings || shouldSuppress);
  SWIFT_DEFER { diags.setSuppressWarnings(didSuppressWarnings); };

  // If this buffer is for code completion, hook up the state needed by its
  // second pass.
  PersistentParserState *state = nullptr;
  if (ctx.SourceMgr.getCodeCompletionBufferID() == bufferID) {
    state = new PersistentParserState();
    SF->setDelayedParserState({state, &deletePersistentParserState});
  }

  Parser parser(*bufferID, *SF, /*SIL*/ nullptr, state);
  PrettyStackTraceParser StackTrace(parser);

  SmallVector<ASTNode, 128> items;
  parser.parseTopLevelItems(items);

  Optional<ArrayRef<Token>> tokensRef;
  if (auto tokens = parser.takeTokenReceiver()->finalize())
    tokensRef = ctx.AllocateCopy(*tokens);

#if SWIFT_SWIFT_PARSER
  if ((ctx.LangOpts.hasFeature(Feature::ParserRoundTrip) ||
       ctx.LangOpts.hasFeature(Feature::ParserValidation)) &&
      ctx.SourceMgr.getCodeCompletionBufferID() != bufferID &&
      SF->Kind != SourceFileKind::SIL) {
    auto bufferRange = ctx.SourceMgr.getRangeForBuffer(*bufferID);
    unsigned int flags = 0;

    if (ctx.LangOpts.hasFeature(Feature::ParserRoundTrip) &&
        !parser.L->lexingCutOffOffset()) {
      flags |= SCC_RoundTrip;
    }

    if (!ctx.Diags.hadAnyError() &&
        ctx.LangOpts.hasFeature(Feature::ParserValidation))
      flags |= SCC_ParseDiagnostics;

    if (ctx.LangOpts.hasFeature(Feature::ParserSequenceFolding) &&
        !parser.L->lexingCutOffOffset())
      flags |= SCC_FoldSequences;

    if (flags) {
      SourceLoc startLoc =
          parser.SourceMgr.getLocForBufferStart(parser.L->getBufferID());
      struct ParserContext {
        SourceLoc startLoc;
        DiagnosticEngine *engine;
      } context{startLoc, &parser.Diags};
      int roundTripResult = swift_parser_consistencyCheck(
          bufferRange.str().data(), bufferRange.getByteLength(),
          SF->getFilename().str().c_str(), flags, static_cast<void *>(&context),
          [](ptrdiff_t off, const char *text, void *ctx) {
            auto *context = static_cast<ParserContext *>(ctx);
            SourceLoc loc = context->startLoc.getAdvancedLoc(off);
            context->engine->diagnose(loc, diag::foreign_diagnostic,
                                      StringRef(text));
          });

      if (roundTripResult)
        ctx.Diags.diagnose(SourceLoc(), diag::new_parser_failure);
    }
  }
#endif

  return SourceFileParsingResult{ctx.AllocateCopy(items), tokensRef,
                                 parser.CurrentTokenHash};
}

evaluator::DependencySource ParseSourceFileRequest::readDependencySource(
    const evaluator::DependencyRecorder &e) const {
  return std::get<0>(getStorage());
}

Optional<SourceFileParsingResult>
ParseSourceFileRequest::getCachedResult() const {
  auto *SF = std::get<0>(getStorage());
  auto items = SF->getCachedTopLevelItems();
  if (!items)
    return None;

  return SourceFileParsingResult{*items, SF->AllCollectedTokens,
                                 SF->InterfaceHasher};
}

void ParseSourceFileRequest::cacheResult(SourceFileParsingResult result) const {
  auto *SF = std::get<0>(getStorage());
  assert(!SF->Items);
  SF->Items = result.TopLevelItems;
  SF->AllCollectedTokens = result.CollectedTokens;
  SF->InterfaceHasher = result.InterfaceHasher;

  // Verify the parsed source file.
  verify(*SF);
}

ArrayRef<Decl *> ParseTopLevelDeclsRequest::evaluate(
    Evaluator &evaluator, SourceFile *SF) const {
  auto items = evaluateOrDefault(evaluator, ParseSourceFileRequest{SF}, {})
    .TopLevelItems;

  std::vector<Decl *> decls;
  for (auto item : items) {
    if (auto decl = item.dyn_cast<Decl *>())
      decls.push_back(decl);
  }

  return SF->getASTContext().AllocateCopy(decls);
}

//----------------------------------------------------------------------------//
// CodeCompletionSecondPassRequest computation.
//----------------------------------------------------------------------------//


void swift::simple_display(llvm::raw_ostream &out,
                           const CodeCompletionCallbacksFactory *factory) { }

evaluator::DependencySource
CodeCompletionSecondPassRequest::readDependencySource(
    const evaluator::DependencyRecorder &e) const {
  return std::get<0>(getStorage());
}

// Define request evaluation functions for each of the type checker requests.
static AbstractRequestFunction *parseRequestFunctions[] = {
#define SWIFT_REQUEST(Zone, Name, Sig, Caching, LocOptions)                    \
  reinterpret_cast<AbstractRequestFunction *>(&Name::evaluateRequest),
#include "swift/AST/ParseTypeIDZone.def"
#undef SWIFT_REQUEST
};

void swift::registerParseRequestFunctions(Evaluator &evaluator) {
  evaluator.registerRequestFunctions(Zone::Parse,
                                     parseRequestFunctions);
}
