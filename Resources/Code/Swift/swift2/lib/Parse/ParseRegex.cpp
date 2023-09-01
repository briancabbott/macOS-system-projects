//===--- ParseRegex.cpp - Regular expression literal parsing --------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2021 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// Regular expression literal parsing
//
//===----------------------------------------------------------------------===//

#include "swift/Basic/BridgingUtils.h"
#include "swift/Parse/Parser.h"

// Regex parser delivered via Swift modules.
#include "swift/Parse/RegexParserBridging.h"
static RegexLiteralParsingFn regexLiteralParsingFn = nullptr;
void Parser_registerRegexLiteralParsingFn(RegexLiteralParsingFn fn) {
  regexLiteralParsingFn = fn;
}

using namespace swift;

ParserResult<Expr> Parser::parseExprRegexLiteral() {
  assert(Tok.is(tok::regex_literal));
  assert(regexLiteralParsingFn);

  auto regexText = Tok.getText();

  // Let the Swift library parse the contents, returning an error, or null if
  // successful.
  unsigned version = 0;
  auto capturesBuf = Context.AllocateUninitialized<uint8_t>(
      RegexLiteralExpr::getCaptureStructureSerializationAllocationSize(
          regexText.size()));
  bool hadError =
      regexLiteralParsingFn(regexText.str().c_str(), &version,
                            /*captureStructureOut*/ capturesBuf.data(),
                            /*captureStructureSize*/ capturesBuf.size(),
                            /*diagBaseLoc*/ Tok.getLoc(), Diags);
  auto loc = consumeToken();
  SourceMgr.recordRegexLiteralStartLoc(loc);

  if (hadError) {
    return makeParserResult(new (Context) ErrorExpr(loc));
  }
  assert(version >= 1);
  return makeParserResult(RegexLiteralExpr::createParsed(
      Context, loc, regexText, version, capturesBuf));
}
