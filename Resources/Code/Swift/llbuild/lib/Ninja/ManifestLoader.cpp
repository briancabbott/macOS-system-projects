//===-- ManifestLoader.cpp ------------------------------------------------===//
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

#include "llbuild/Ninja/ManifestLoader.h"

#include "llbuild/Basic/LLVM.h"
#include "llbuild/Basic/ShellUtility.h"
#include "llbuild/Ninja/Lexer.h"
#include "llbuild/Ninja/Parser.h"

#include "llvm/Support/MemoryBuffer.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/raw_ostream.h"

#include <cstdlib>
#include <vector>

using namespace llbuild;
using namespace llbuild::ninja;

#pragma mark - ManifestLoaderActions

ManifestLoaderActions::~ManifestLoaderActions() {
}

#pragma mark - ManifestLoader Implementation

/// Manifest loader implementation.
///
/// For simplicity, we just directly implement the parser actions interface.
class ManifestLoader::ManifestLoaderImpl: public ParseActions {
  struct IncludeEntry {
    /// An owning reference to the buffer consumed by the parser.
    std::unique_ptr<llvm::MemoryBuffer> data;
    /// The parser for the file.
    std::unique_ptr<Parser> parser;
    /// The active scope.
    Scope& scope;

    IncludeEntry(std::unique_ptr<llvm::MemoryBuffer> data,
                 std::unique_ptr<Parser> parser, Scope& scope)
      : data(std::move(data)), parser(std::move(parser)), scope(scope) {}
  };

  StringRef workingDirectory;
  StringRef mainFilename;
  ManifestLoaderActions& actions;
  std::unique_ptr<Manifest> manifest;
  llvm::SmallVector<IncludeEntry, 4> includeStack;

  // Cached buffers for temporary expansion of possibly large strings. These are
  // lifted out of the function body to ensure we don't blow up the stack
  // unnecesssarily.
  SmallString<10 * 1024> buildCommand;
  SmallString<10 * 1024> buildDescription;

public:
  ManifestLoaderImpl(StringRef workingDirectory, StringRef mainFilename,
                     ManifestLoaderActions& actions)
    : workingDirectory(workingDirectory), mainFilename(mainFilename),
      actions(actions), manifest(nullptr) {}

  std::unique_ptr<Manifest> load() {
    // Create the manifest.
    manifest.reset(new Manifest);

    // Enter the main file.
    if (!enterFile(mainFilename, manifest->getRootScope()))
      return nullptr;

    // Run the parser.
    assert(includeStack.size() == 1);
    getCurrentParser()->parse();
    assert(includeStack.size() == 0);

    return std::move(manifest);
  }

  bool enterFile(StringRef filename, Scope& scope,
                 const Token* forToken = nullptr) {
    SmallString<256> path(filename);
    llvm::sys::fs::make_absolute(workingDirectory, path);

    // Load the file data.
    StringRef forFilename = includeStack.empty() ? filename :
        getCurrentFilename();
    std::unique_ptr<llvm::MemoryBuffer> buffer = actions.readFile(
        path, forFilename, forToken);
    if (!buffer)
      return false;

    // Push a new entry onto the include stack.
    auto parser = llvm::make_unique<Parser>(buffer->getBuffer(), *this);
    includeStack.emplace_back(std::move(buffer), std::move(parser),
                              scope);

    return true;
  }

  void exitCurrentFile() {
    includeStack.pop_back();
  }

  ManifestLoaderActions& getActions() { return actions; }
  Parser* getCurrentParser() const {
    assert(!includeStack.empty());
    return includeStack.back().parser.get();
  }
  StringRef getCurrentFilename() const {
    assert(!includeStack.empty());
    return includeStack.back().data->getBufferIdentifier();
  }
  Scope& getCurrentScope() const {
    assert(!includeStack.empty());
    return includeStack.back().scope;
  }

  void evalString(void* userContext, StringRef string, raw_ostream& result,
                  std::function<void(void*, StringRef, raw_ostream&)> lookup,
                  std::function<void(const std::string&)> error) {
    // Scan the string for escape sequences or variable references, accumulating
    // output pieces as we go.
    const char* pos = string.begin();
    const char* end = string.end();
    while (pos != end) {
      // Find the next '$'.
      const char* pieceStart = pos;
      for (; pos != end; ++pos) {
        if (*pos == '$')
          break;
      }

      // Add the current piece, if non-empty.
      if (pos != pieceStart)
        result << StringRef(pieceStart, pos - pieceStart);

      // If we are at the end, we are done.
      if (pos == end)
        break;

      // Otherwise, we have a '$' character to handle.
      ++pos;
      if (pos == end) {
        error("invalid '$'-escape at end of string");
        break;
      }

      // If this is a newline continuation, skip it and all leading space.
      int c = *pos;
      if (c == '\n') {
        ++pos;
        while (pos != end && isspace(*pos))
          ++pos;
        continue;
      }

      // If this is single character escape, honor it.
      if (c == ' ' || c == ':' || c == '$') {
        result << char(c);
        ++pos;
        continue;
      }

      // If this is a braced variable reference, expand it.
      if (c == '{') {
        // Scan until the end of the reference, checking validity of the
        // identifier name as we go.
        ++pos;
        const char* varStart = pos;
        bool isValid = true;
        while (true) {
          // If we reached the end of the string, this is an error.
          if (pos == end) {
            error(
                "invalid variable reference in string (missing trailing '}')");
            break;
          }

          // If we found the end of the reference, resolve it.
          int c = *pos;
          if (c == '}') {
            // If this identifier isn't valid, emit an error.
            if (!isValid) {
              error("invalid variable name in reference");
            } else {
              lookup(userContext, StringRef(varStart, pos - varStart),
                     result);
            }
            ++pos;
            break;
          }

          // Track whether this is a valid identifier.
          if (!Lexer::isIdentifierChar(c))
            isValid = false;

          ++pos;
        }
        continue;
      }

      // If this is a simple variable reference, expand it.
      if (Lexer::isSimpleIdentifierChar(c)) {
        const char* varStart = pos;
        // Scan until the end of the simple identifier.
        ++pos;
        while (pos != end && Lexer::isSimpleIdentifierChar(*pos))
          ++pos;
        lookup(userContext, StringRef(varStart, pos-varStart), result);
        continue;
      }

      // Otherwise, we have an invalid '$' escape.
      error("invalid '$'-escape (literal '$' should be written as '$$')");
      break;
    }
  }

  /// Given a string template token, evaluate it against the given \arg Bindings
  /// and return the resulting string.
  void evalString(const Token& value, const Scope& scope,
                  SmallVectorImpl<char>& storage) {
    assert(value.tokenKind == Token::Kind::String && "invalid token kind");
    
    llvm::raw_svector_ostream result(storage);
    evalString(nullptr, StringRef(value.start, value.length), result,
               /*Lookup=*/ [&](void*, StringRef name, raw_ostream& result) {
                 result << scope.lookupBinding(name);
               },
               /*Error=*/ [this, &value](const std::string& msg) {
                 error(msg, value);
               });
  }

  /// @name Parse Actions Interfaces
  /// @{

  virtual void initialize(ninja::Parser* parser) override { }

  virtual void error(StringRef message, const Token& at) override {
    actions.error(getCurrentFilename(), message, at);
  }

  virtual void actOnBeginManifest(StringRef name) override { }

  virtual void actOnEndManifest() override {
    exitCurrentFile();
  }

  virtual void actOnBindingDecl(const Token& nameTok,
                                const Token& valueTok) override {
    // Extract the name string.
    StringRef name(nameTok.start, nameTok.length);

    // Evaluate the value string with the current bindings.
    SmallString<256> value;
    evalString(valueTok, getCurrentScope(), value);

    getCurrentScope().insertBinding(name, value.str());
  }

  virtual void actOnDefaultDecl(ArrayRef<Token> nameToks) override {
    // Resolve all of the inputs and outputs.
    for (const auto& nameTok: nameToks) {
      StringRef name(nameTok.start, nameTok.length);
      Node* node = manifest->findNode(workingDirectory, name);

      if (node == nullptr) {
        error("unknown target name", nameTok);
        continue;
      }

      manifest->getDefaultTargets().push_back(node);
    }
  }

  virtual void actOnIncludeDecl(bool isInclude,
                                const Token& pathTok) override {
    SmallString<256> path;
    evalString(pathTok, getCurrentScope(), path);

    // Enter the new file, with a new binding scope if this is a "subninja"
    // decl.
    if (isInclude) {
      if (enterFile(path.str(), getCurrentScope(), &pathTok)) {
        // Run the parser for the included file.
        getCurrentParser()->parse();
      }
    } else {
      // Establish a local binding set and use that to contain the bindings for
      // the subninja.
      Scope subninjaScope(&getCurrentScope());
      if (enterFile(path.str(), subninjaScope, &pathTok)) {
        // Run the parser for the included file.
        getCurrentParser()->parse();
      }
    }
  }

  virtual BuildResult
  actOnBeginBuildDecl(const Token& nameTok,
                      ArrayRef<Token> outputTokens,
                      ArrayRef<Token> inputTokens,
                      unsigned numExplicitInputs,
                      unsigned numImplicitInputs) override {
    StringRef name(nameTok.start, nameTok.length);

    // Resolve the rule.
    auto it = getCurrentScope().getRules().find(name);
    Rule* rule;
    if (it == getCurrentScope().getRules().end()) {
      error("unknown rule", nameTok);

      // Ensure we always have a rule for each command.
      rule = manifest->getPhonyRule();
    } else {
      rule = it->second;
    }

    // Resolve all of the inputs and outputs.
    SmallVector<Node*, 8> outputs;
    SmallVector<Node*, 8> inputs;
    for (const auto& token: outputTokens) {
      // Evaluate the token string.
      SmallString<256> path;
      evalString(token, getCurrentScope(), path);
      if (path.empty()) {
        error("empty output path", token);
      }
      outputs.push_back(manifest->findOrCreateNode(workingDirectory, path));
    }
    for (const auto& token: inputTokens) {
      // Evaluate the token string.
      SmallString<256> path;
      evalString(token, getCurrentScope(), path);
      if (path.empty()) {
        error("empty input path", token);
      }
      inputs.push_back(manifest->findOrCreateNode(workingDirectory, path));
    }

    Command* decl = new (manifest->getAllocator())
      Command(rule, outputs, inputs, numExplicitInputs, numImplicitInputs);
    manifest->getCommands().push_back(decl);

    return decl;
  }

  virtual void actOnBuildBindingDecl(BuildResult abstractDecl,
                                     const Token& nameTok,
                                     const Token& valueTok) override {
    Command* decl = static_cast<Command*>(abstractDecl);

    StringRef name(nameTok.start, nameTok.length);

    // FIXME: It probably should be an error to assign to the same parameter
    // multiple times, but Ninja doesn't diagnose this.

    // The value in a build decl is always evaluated immediately, but only in
    // the context of the top-level bindings.
    SmallString<256> value;
    evalString(valueTok, getCurrentScope(), value);
    
    decl->getParameters()[name] = value.str();
  }

  struct LookupContext {
    ManifestLoaderImpl& loader;
    Command* decl;
    const Token& startTok;
    bool shellEscapeInAndOut;
  };
  static void lookupBuildParameter(void* userContext, StringRef name,
                                   raw_ostream& result) {
    LookupContext* context = static_cast<LookupContext*>(userContext);
    context->loader.lookupBuildParameterImpl(context, name, result);
  }
  void lookupBuildParameterImpl(LookupContext* context, StringRef name,
                                raw_ostream& result) {
    auto decl = context->decl;
      
    // FIXME: Mange recursive lookup? Ninja crashes on it.
      
    // Support "in", "in_newline" and "out".
    if (name == "in" || name == "in_newline") {
      const auto separator = name == "in" ? ' ' : '\n';
      for (unsigned i = 0, ie = decl->getNumExplicitInputs(); i != ie; ++i) {
        if (i != 0)
          result << separator;
        auto& path = decl->getInputs()[i]->getScreenPath();
        result << (context->shellEscapeInAndOut ? basic::shellEscaped(path)
                                                : path);
      }
      return;
    } else if (name == "out") {
      for (unsigned i = 0, ie = decl->getOutputs().size(); i != ie; ++i) {
        if (i != 0)
          result << " ";
        auto& path = decl->getOutputs()[i]->getScreenPath();
        result << (context->shellEscapeInAndOut ? basic::shellEscaped(path)
                                                : path);
      }
      return;
    }

    auto it = decl->getParameters().find(name);
    if (it != decl->getParameters().end()) {
      result << it->second;
      return;
    }
    auto it2 = decl->getRule()->getParameters().find(name);
    if (it2 != decl->getRule()->getParameters().end()) {
      evalString(context, it2->second, result, lookupBuildParameter,
                 /*Error=*/ [&](const std::string& msg) {
                   error(msg + " during evaluation of '" + name.str() + "'",
                         context->startTok);
                 });
      return;
    }
      
    result << context->loader.getCurrentScope().lookupBinding(name);
  }
  StringRef lookupNamedBuildParameter(Command* decl, const Token& startTok,
                                      StringRef name,
                                      SmallVectorImpl<char>& storage) {
    LookupContext context{*this, decl, startTok,
                          /*shellEscapeInAndOut*/ name == "command"};
    llvm::raw_svector_ostream os(storage);
    lookupBuildParameter(&context, name, os);
    return os.str();
  }
  
  virtual void actOnEndBuildDecl(BuildResult abstractDecl,
                                const Token& startTok) override {
    Command* decl = static_cast<Command*>(abstractDecl);

    // Resolve the build decl parameters by evaluating in the context of the
    // rule and parameter overrides.
    //
    // FIXME: Eventually, we should evaluate whether it would be more efficient
    // to lazily bind all of these by only storing the parameters for the
    // commands. This would let us delay the computation of all of the "command"
    // strings until right before the command is run, which would then be
    // parallelized and could also be more memory efficient. However, that would
    // also requires us to expose more of the string evaluation machinery, as
    // well as ensure that the recursive binding sets used by "subninja" decls
    // are properly stored.

    // FIXME: There is no need to store the parameters in the build decl anymore
    // once this is all complete.

    // Evaluate the build parameters.
    buildCommand.clear();
    decl->setCommandString(lookupNamedBuildParameter(
                               decl, startTok, "command", buildCommand));
    buildDescription.clear();
    decl->setDescription(lookupNamedBuildParameter(
                             decl, startTok, "description", buildDescription));

    // Set the dependency style.
    SmallString<256> deps;
    lookupNamedBuildParameter(decl, startTok, "deps", deps);
    SmallString<256> depfile;
    lookupNamedBuildParameter(decl, startTok, "depfile", depfile);
    Command::DepsStyleKind depsStyle = Command::DepsStyleKind::None;
    if (deps.str() == "") {
      if (!depfile.empty())
        depsStyle = Command::DepsStyleKind::GCC;
    } else if (deps.str() == "gcc") {
      depsStyle = Command::DepsStyleKind::GCC;
    } else if (deps.str() == "msvc") {
      depsStyle = Command::DepsStyleKind::MSVC;
    } else {
      error("invalid 'deps' style '" + deps.str().str() + "'", startTok);
    }
    decl->setDepsStyle(depsStyle);

    if (!depfile.str().empty()) {
      if (depsStyle != Command::DepsStyleKind::GCC) {
        error("invalid 'depfile' attribute with selected 'deps' style",
              startTok);
      } else {
        decl->setDepsFile(depfile.str());
      }
    } else {
      if (depsStyle == Command::DepsStyleKind::GCC) {
        error("missing 'depfile' attribute with selected 'deps' style",
              startTok);
      }
    }

    SmallString<256> poolName;
    lookupNamedBuildParameter(decl, startTok, "pool", poolName);
    if (!poolName.empty()) {
      const auto& it = manifest->getPools().find(poolName.str());
      if (it == manifest->getPools().end()) {
        error("unknown pool '" + poolName.str().str() + "'", startTok);
      } else {
        decl->setExecutionPool(it->second);
      }
    }

    SmallString<256> generator;
    lookupNamedBuildParameter(decl, startTok, "generator", generator);
    decl->setGeneratorFlag(!generator.str().empty());

    SmallString<256> restat;
    lookupNamedBuildParameter(decl, startTok, "restat", restat);
    decl->setRestatFlag(!restat.str().empty());

    // Handle rspfile attributes.
    SmallString<256> rspfile;
    lookupNamedBuildParameter(decl, startTok, "rspfile", rspfile);
    if (rspfile.str().empty())
      return;
    if (!Manifest::normalize_path(workingDirectory, rspfile))
      return;
    decl->setRspFile(rspfile);

    SmallString<256> rspfileContent;
    lookupNamedBuildParameter(decl, startTok, "rspfile_content", rspfileContent);
    decl->setRspFileContent(rspfileContent);
  }

  virtual PoolResult actOnBeginPoolDecl(const Token& nameTok) override {
    StringRef name(nameTok.start, nameTok.length);

    // Find the hash slot.
    auto& result = manifest->getPools()[name];

    // Diagnose if the pool already exists (we still create a new one).
    if (result) {
      // The pool already exists.
      error("duplicate pool", nameTok);
    }

    // Insert the new pool.
    Pool* decl = new (manifest->getAllocator()) Pool(name);
    result = decl;
    return static_cast<PoolResult>(decl);
  }

  virtual void actOnPoolBindingDecl(PoolResult abstractDecl,
                                    const Token& nameTok,
                                    const Token& valueTok) override {
    Pool* decl = static_cast<Pool*>(abstractDecl);

    StringRef name(nameTok.start, nameTok.length);

    // Evaluate the value string with the current top-level bindings.
    SmallString<256> value;
    evalString(valueTok, getCurrentScope(), value);

    if (name == "depth") {
      long intValue;
      if (value.str().getAsInteger(10, intValue) || intValue <= 0) {
        error("invalid depth", valueTok);
      } else {
        decl->setDepth(static_cast<uint32_t>(intValue));
      }
    } else {
      error("unexpected variable", nameTok);
    }
  }

  virtual void actOnEndPoolDecl(PoolResult abstractDecl,
                                const Token& startTok) override {
    Pool* decl = static_cast<Pool*>(abstractDecl);

    // It is an error to not specify the pool depth.
    if (decl->getDepth() == 0) {
      error("missing 'depth' variable assignment", startTok);
    }
  }

  virtual RuleResult actOnBeginRuleDecl(const Token& nameTok) override {
    StringRef name(nameTok.start, nameTok.length);

    // Find the hash slot.
    auto& result = getCurrentScope().getRules()[name];

    // Diagnose if the rule already exists (we still create a new one).
    if (result) {
      // The rule already exists.
      error("duplicate rule", nameTok);
    }

    // Insert the new rule.
    Rule* decl = new (manifest->getAllocator()) Rule(name);
    result = decl;
    return static_cast<RuleResult>(decl);
  }

  virtual void actOnRuleBindingDecl(RuleResult abstractDecl,
                                    const Token& nameTok,
                                    const Token& valueTok) override {
    Rule* decl = static_cast<Rule*>(abstractDecl);

    StringRef name(nameTok.start, nameTok.length);
    // FIXME: It probably should be an error to assign to the same parameter
    // multiple times, but Ninja doesn't diagnose this.
    if (Rule::isValidParameterName(name)) {
      decl->getParameters()[name] = StringRef(valueTok.start, valueTok.length);
    } else {
      error("unexpected variable", nameTok);
    }
  }

  virtual void actOnEndRuleDecl(RuleResult abstractDecl,
                                const Token& startTok) override {
    Rule* decl = static_cast<Rule*>(abstractDecl);

    if (!decl->getParameters().count("command")) {
      error("missing 'command' variable assignment", startTok);
    }
  }

  /// @}
};

#pragma mark - ManifestLoader

ManifestLoader::ManifestLoader(StringRef workingDirectory,
                               StringRef filename,
                               ManifestLoaderActions& actions)
  : impl(new ManifestLoaderImpl(workingDirectory, filename, actions)) {}

ManifestLoader::~ManifestLoader() = default;

std::unique_ptr<Manifest> ManifestLoader::load() {
  // Initialize the actions.
  impl->getActions().initialize(this);
  return impl->load();
}

const Parser* ManifestLoader::getCurrentParser() const {
  return impl->getCurrentParser();
}
