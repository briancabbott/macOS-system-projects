//===--- swift-ide-test.cpp - IDE functionality testing application -------===//
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

#include "XMLValidator.h"
#include "ModuleAPIDiff.h"
#include "swift/AST/ASTContext.h"
#include "swift/AST/ASTPrinter.h"
#include "swift/AST/ASTWalker.h"
#include "swift/AST/Comment.h"
#include "swift/AST/DebuggerClient.h"
#include "swift/AST/DiagnosticEngine.h"
#include "swift/AST/Mangle.h"
#include "swift/AST/PrintOptions.h"
#include "swift/AST/RawComment.h"
#include "swift/AST/USRGeneration.h"
#include "swift/Basic/Demangle.h"
#include "swift/Basic/DemangleWrappers.h"
#include "swift/Basic/DiagnosticConsumer.h"
#include "swift/Basic/LangOptions.h"
#include "swift/Basic/PrimitiveParsing.h"
#include "swift/Basic/LLVMInitialize.h"
#include "swift/Driver/FrontendUtil.h"
#include "swift/Frontend/Frontend.h"
#include "swift/Frontend/PrintingDiagnosticConsumer.h"
#include "swift/IDE/CodeCompletion.h"
#include "swift/IDE/CommentConversion.h"
#include "swift/IDE/ModuleInterfacePrinting.h"
#include "swift/IDE/REPLCodeCompletion.h"
#include "swift/IDE/SourceEntityWalker.h"
#include "swift/IDE/SyntaxModel.h"
#include "swift/IDE/Utils.h"
#include "swift/Markup/Markup.h"
#include "swift/Config.h"
#include "clang/APINotes/APINotesReader.h"
#include "clang/APINotes/APINotesWriter.h"
#include "clang/Rewrite/Core/RewriteBuffer.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/ManagedStatic.h"
#include <system_error>

#include <string>

using namespace swift;
using namespace ide;

namespace {

enum class ActionType {
  None,
  CodeCompletion,
  REPLCodeCompletion,
  DumpCompletionCache,
  DumpImporterLookupTable,
  SyntaxColoring,
  DumpAPI,
  DumpComments,
  Structure,
  Annotation,
  TestInputCompleteness,
  PrintASTNotTypeChecked,
  PrintASTTypeChecked,
  PrintModule,
  PrintHeader,
  PrintSwiftFileInterface,
  PrintTypes,
  PrintComments,
  PrintModuleComments,
  PrintModuleImports,
  PrintUSRs,
  PrintLocalTypes,
  PrintTypeInterface,
  TestCreateCompilerInvocation,
  CompilerInvocationFromModule,
  GenerateModuleAPIDescription,
  DiffModuleAPI,
};

class NullDebuggerClient : public DebuggerClient {
public:
  using DebuggerClient::DebuggerClient;

  bool shouldGlobalize(Identifier Name, DeclKind Kind) override {
    return false;
  }
  void didGlobalize(Decl *D) override {}
  bool lookupOverrides(Identifier Name, DeclContext *DC,
                       SourceLoc Loc, bool IsTypeLookup,
                       ResultVector &RV) override {
    return false;
  }

  bool lookupAdditions(Identifier Name, DeclContext *DC,
                       SourceLoc Loc, bool IsTypeLookup,
                       ResultVector &RV) override {
    return false;
  }

  SILDebuggerClient *getAsSILDebuggerClient() override {
    return nullptr;
  }
};

class PrivateDiscriminatorPreferenceClient : public NullDebuggerClient {
  Identifier Discriminator;
public:
  PrivateDiscriminatorPreferenceClient(ASTContext &C,
                                       StringRef DiscriminatorStr)
      : NullDebuggerClient(C),
        Discriminator(C.getIdentifier(DiscriminatorStr)) {}

  Identifier getPreferredPrivateDiscriminator() override {
    return Discriminator;
  }
};

} // end anonymous namespace

namespace options {

static llvm::cl::opt<ActionType>
Action(llvm::cl::desc("Mode:"), llvm::cl::init(ActionType::None),
       llvm::cl::values(
           clEnumValN(ActionType::CodeCompletion,
                      "code-completion", "Perform code completion"),
           clEnumValN(ActionType::REPLCodeCompletion,
                      "repl-code-completion", "Perform REPL-style code completion"),
           clEnumValN(ActionType::DumpCompletionCache,
                      "dump-completion-cache", "Dump a code completion cache file"),
           clEnumValN(ActionType::DumpImporterLookupTable,
                      "dump-importer-lookup-table", "Dump the Clang importer's lookup tables"),
           clEnumValN(ActionType::SyntaxColoring,
                      "syntax-coloring", "Perform syntax coloring"),
           clEnumValN(ActionType::DumpAPI,
                      "dump-api", "Dump the public API"),
           clEnumValN(ActionType::DumpComments,
                     "dump-comments", "Dump documentation comments attached to decls"),
           clEnumValN(ActionType::Structure,
                      "structure", "Perform document structure annotation"),
           clEnumValN(ActionType::Annotation,
                      "annotate", "Perform semantic annotation"),
           clEnumValN(ActionType::TestInputCompleteness,
                      "test-input-complete", "Check if input source is complete"),
           clEnumValN(ActionType::PrintASTNotTypeChecked,
                      "print-ast-not-typechecked", "Print the non-typechecked AST"),
           clEnumValN(ActionType::PrintASTTypeChecked,
                      "print-ast-typechecked", "Print the typechecked AST"),
           clEnumValN(ActionType::PrintModule,
                      "print-module", "Print visible declarations in a module"),
           clEnumValN(ActionType::PrintHeader,
                      "print-header", "Print visible declarations in a header file"),
           clEnumValN(ActionType::PrintSwiftFileInterface,
                      "print-swift-file-interface", "Print interface of a swift file"),
           clEnumValN(ActionType::PrintTypes,
                      "print-types", "Print types of all subexpressions and declarations in the AST"),
           clEnumValN(ActionType::PrintComments,
                      "print-comments", "Print documentation comments attached to decls"),
           clEnumValN(ActionType::PrintModuleComments,
                      "print-module-comments", "Given a module, print documentation comments attached to decls"),
           clEnumValN(ActionType::PrintModuleImports,
                      "print-module-imports", "Recursively print all imports visible from a particular module"),
           clEnumValN(ActionType::PrintUSRs,
                      "print-usrs", "Print USRs for all decls"),
           clEnumValN(ActionType::PrintLocalTypes,
                      "print-local-types", "Print local types and remanglings in a module"),
           clEnumValN(ActionType::TestCreateCompilerInvocation,
                      "test-createCompilerInvocation",
                      "Test swift::driver::createCompilerInvocation using the "
                      "arguments passed to swift-ide-test (must be specified "
                      "before all other arguments)"),
           clEnumValN(ActionType::CompilerInvocationFromModule,
                      "test-CompilerInvocation-from-module",
                      "Test CompilerInvocation::loadFromSerializedAST on the "
                      "\"source\" file"),
           clEnumValN(ActionType::GenerateModuleAPIDescription,
                      "generate-module-api-description",
                      "Generate a machine-readable description of module API"),
           clEnumValN(ActionType::DiffModuleAPI,
                      "diff-module-api",
                      "Compare machine-readable descriptions of module API"),
           clEnumValN(ActionType::PrintTypeInterface,
                      "print-type-interface",
                      "Print type-specific interface decl"),
           clEnumValEnd));

static llvm::cl::opt<std::string>
SourceFilename("source-filename", llvm::cl::desc("Name of the source file"));

static llvm::cl::list<std::string>
InputFilenames(llvm::cl::Positional, llvm::cl::desc("[input files...]"),
               llvm::cl::ZeroOrMore);

static llvm::cl::list<std::string>
BuildConfigs("D", llvm::cl::desc("Build configurations"));

static llvm::cl::opt<std::string>
SDK("sdk", llvm::cl::desc("path to the SDK to build against"));

static llvm::cl::opt<std::string>
Triple("target", llvm::cl::desc("target triple"));

static llvm::cl::opt<std::string>
ModuleCachePath("module-cache-path", llvm::cl::desc("Clang module cache path"));

static llvm::cl::opt<std::string>
    CompletionCachePath("completion-cache-path",
                        llvm::cl::desc("Code completion cache path"),
                        llvm::cl::ZeroOrMore);

static llvm::cl::list<std::string>
ImportPaths("I", llvm::cl::desc("add a directory to the import search path"));

static llvm::cl::list<std::string>
FrameworkPaths("F", llvm::cl::desc("add a directory to the framework search path"));

static llvm::cl::opt<std::string>
ResourceDir("resource-dir",
            llvm::cl::desc("The directory that holds the compiler resource files"));

static llvm::cl::opt<std::string>
ImportObjCHeader("import-objc-header", llvm::cl::desc("header to implicitly import"));

static llvm::cl::opt<bool>
EnableSourceImport("enable-source-import", llvm::cl::Hidden,
                   llvm::cl::init(false));

static llvm::cl::opt<bool>
SkipDeinit("skip-deinit",
                   llvm::cl::desc("Whether to skip printing destructors"),
                   llvm::cl::init(true));

static llvm::cl::opt<bool>
SkipImports("skip-imports",
            llvm::cl::desc("Whether to skip printing import declarations"),
            llvm::cl::init(false));

static llvm::cl::opt<bool>
SkipParameterNames("skip-parameter-names",
                   llvm::cl::desc("Whether to skip parameter names"),
                   llvm::cl::init(false));

static llvm::cl::opt<bool>
DisableAccessControl("disable-access-control",
    llvm::cl::desc("Disables access control, like a debugger"));

static llvm::cl::opt<bool> CodeCompleteInitsInPostfixExpr(
    "code-complete-inits-in-postfix-expr",
    llvm::cl::desc(
        "Include initializers when completing a postfix expression"));

static llvm::cl::opt<bool>
ObjCForwardDeclarations("enable-objc-forward-declarations",
    llvm::cl::desc("Import Objective-C forward declarations when possible"),
    llvm::cl::init(false));

static llvm::cl::opt<bool>
ImplicitProperties("enable-objc-implicit-properties",
                   llvm::cl::desc("Implicitly import Objective-C getter/setter pairs as properties"),
                   llvm::cl::init(false));

static llvm::cl::opt<bool>
InferDefaultArguments(
  "enable-infer-default-arguments",
  llvm::cl::desc("Infer default arguments for imported parameters"),
  llvm::cl::init(false));

static llvm::cl::opt<bool>
UseSwiftLookupTables(
  "enable-swift-name-lookup-tables",
  llvm::cl::desc("Use Swift-specific name lookup tables in the importer"),
  llvm::cl::init(false));

static llvm::cl::opt<bool>
OmitNeedlessWords("enable-omit-needless-words",
                   llvm::cl::desc("Omit needless words when importing Objective-C names"),
                   llvm::cl::init(false));

static llvm::cl::opt<bool>
DisableObjCAttrRequiresFoundationModule(
    "disable-objc-attr-requires-foundation-module",
    llvm::cl::desc("Allow @objc to be used freely"),
    llvm::cl::init(false));

static llvm::cl::opt<bool>
PrintStats("print-stats",
           llvm::cl::desc("Print statistics"),
           llvm::cl::init(false));

static llvm::cl::opt<std::string>
DebugForbidTypecheckPrefix("debug-forbid-typecheck-prefix",
  llvm::cl::desc("Triggers llvm fatal_error if typechecker tries to typecheck "
                 "a decl with the provided prefix name"));

// '-code-completion' options.

static llvm::cl::opt<std::string>
CodeCompletionToken("code-completion-token",
                    llvm::cl::desc("Code completion token name"));

static llvm::cl::opt<bool>
CodeCompletionDiagnostics("code-completion-diagnostics",
                          llvm::cl::desc("Print compiler diagnostics while "
                                         "doing code completion"),
                          llvm::cl::init(false));

static llvm::cl::opt<bool>
CodeCompletionKeywords("code-completion-keywords",
                       llvm::cl::desc("Include keywords in code completion results"),
                       llvm::cl::init(true));

static llvm::cl::opt<std::string>
DebugClientDiscriminator("debug-client-discriminator",
  llvm::cl::desc("A discriminator to prefer in lookups"));

// '-syntax-coloring' options.

static llvm::cl::opt<bool>
TerminalOutput("terminal",
               llvm::cl::desc("Use terminal color for source annotations"));

static llvm::cl::opt<bool>
Typecheck("typecheck",
          llvm::cl::desc("Type check the AST"),
          llvm::cl::init(false));

// AST printing options.

static llvm::cl::opt<bool>
FunctionDefinitions("function-definitions",
                    llvm::cl::desc("Print function bodies"),
                    llvm::cl::init(true));

static llvm::cl::opt<bool>
AbstractAccessors("abstract-accessors",
                  llvm::cl::desc("Hide the concrete accessors used to "
                                 "implement a property or subscript"),
                  llvm::cl::init(true));

static llvm::cl::opt<bool>
PreferTypeRepr("prefer-type-repr",
               llvm::cl::desc("When printing types, prefer printing TypeReprs"),
               llvm::cl::init(true));

static llvm::cl::opt<bool>
FullyQualifiedTypes("fully-qualified-types",
                    llvm::cl::desc("Print fully qualified types"),
                    llvm::cl::init(false));

static llvm::cl::opt<bool>
ExplodePatternBindingDecls(
    "explode-pattern-binding-decls",
    llvm::cl::desc("Separate pattern binding decls into individual var decls"),
    llvm::cl::init(false));

static llvm::cl::opt<std::string>
MangledNameToFind("find-mangled",
    llvm::cl::desc("Print the entity with the given mangled name"));

// Module printing options.

static llvm::cl::list<std::string>
ModuleToPrint("module-to-print",
              llvm::cl::desc("Name of the module to print"));

static llvm::cl::opt<bool>
ModulePrintSubmodules("module-print-submodules",
                      llvm::cl::desc("Recursively print submodules"),
                      llvm::cl::init(false));

static llvm::cl::opt<bool>
ModulePrintHidden("module-print-hidden",
                  llvm::cl::desc("Print non-exported imported or submodules"),
                  llvm::cl::init(false));

static llvm::cl::opt<bool>
ModulePrintSkipOverlay("module-print-skip-overlay",
                  llvm::cl::desc("Skip Swift overlay modules"),
                  llvm::cl::init(false));

static llvm::cl::opt<bool>
FullyQualifiedTypesIfAmbiguous(
    "fully-qualified-types-if-ambiguous",
    llvm::cl::desc("Print types fully-qualified if they would be ambiguous "
                   "otherwise"),
    llvm::cl::init(false));

static llvm::cl::opt<bool>
SynthesizeSugarOnTypes(
    "synthesize-sugar-on-types",
    llvm::cl::desc("Always print Array and Optional with sugar"),
    llvm::cl::init(false));

static llvm::cl::opt<bool>
AnnotatePrint("annotate-print",
               llvm::cl::desc("Annotate AST printing"),
               llvm::cl::init(false));

// AST and module printing options.

static llvm::cl::opt<bool>
PrintInterface("print-interface",
               llvm::cl::desc("Print with options set for interface printing, "
                              "overrides any other printing option"),
               llvm::cl::init(false));

static llvm::cl::opt<bool>
PrintInterfaceForDoc("print-interface-doc",
               llvm::cl::desc("Print with options set for interface printing, "
                        "for doc support; overrides any other printing option"),
               llvm::cl::init(false));

static llvm::cl::opt<bool>
PrintImplicitAttrs("print-implicit-attrs",
                   llvm::cl::desc("Print implicit attributes"),
                   llvm::cl::init(false));

static llvm::cl::opt<bool>
PrintAccessibility("print-accessibility",
                   llvm::cl::desc("Print accessibility for all values"),
                   llvm::cl::init(false));

static llvm::cl::opt<bool>
SkipUnavailable("skip-unavailable",
                llvm::cl::desc("Don't print unavailable declarations"),
                llvm::cl::init(false));

static llvm::cl::opt<Accessibility>
AccessibilityFilter(
    llvm::cl::desc("Accessibility filter:"),
    llvm::cl::init(Accessibility::Private),
    llvm::cl::values(
        clEnumValN(Accessibility::Private, "accessibility-filter-private",
            "Print all declarations"),
        clEnumValN(Accessibility::Internal, "accessibility-filter-internal",
            "Print internal and public declarations"),
        clEnumValN(Accessibility::Public, "accessibility-filter-public",
            "Print public declarations"),
        clEnumValEnd));

static llvm::cl::opt<bool>
SkipPrivateStdlibDecls("skip-private-stdlib-decls",
                llvm::cl::desc("Don't print declarations that start with '_'"),
                llvm::cl::init(false));

static llvm::cl::opt<bool>
SkipUnderscoredStdlibProtocols("skip-underscored-stdlib-protocols",
                llvm::cl::desc("Don't print protocols that start with '_'"),
                llvm::cl::init(false));

static llvm::cl::opt<bool>
PrintRegularComments("print-regular-comments",
             llvm::cl::desc("Print regular comments from clang module headers"),
             llvm::cl::init(false));

static llvm::cl::opt<bool>
PrintOriginalSourceText("print-original-source",
             llvm::cl::desc("print the original source text for applicable declarations"),
             llvm::cl::init(false));

static llvm::cl::opt<std::string>
CommentsXMLSchema("comments-xml-schema",
                  llvm::cl::desc("Filename of the RelaxNG schema for documentation comments"));

static llvm::cl::list<std::string>
ClangXCC("Xcc", llvm::cl::desc("option to pass to clang"));

static llvm::cl::list<std::string>
HeaderToPrint("header-to-print",
              llvm::cl::desc("Header filename to print swift interface for"));

static llvm::cl::opt<std::string>
LineColumnPair("pos", llvm::cl::desc("Line:Column pair"));
} // namespace options

static std::unique_ptr<llvm::MemoryBuffer>
removeCodeCompletionTokens(llvm::MemoryBuffer *Input,
                           StringRef TokenName,
                           unsigned *CodeCompletionOffset) {
  std::string CleanFile =
      ide::removeCodeCompletionTokens(Input->getBuffer(),
                                      TokenName,
                                      CodeCompletionOffset);
  return std::unique_ptr<llvm::MemoryBuffer>(
      llvm::MemoryBuffer::getMemBufferCopy(CleanFile,
                                           Input->getBufferIdentifier()));
}

static int doCodeCompletion(const CompilerInvocation &InitInvok,
                            StringRef SourceFilename,
                            StringRef CodeCompletionToken,
                            bool CodeCompletionDiagnostics,
                            bool CodeCompletionKeywords) {
  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> FileBufOrErr =
    llvm::MemoryBuffer::getFile(SourceFilename);
  if (!FileBufOrErr) {
    llvm::errs() << "error opening input file: "
                 << FileBufOrErr.getError().message() << '\n';
    return 1;
  }

  unsigned CodeCompletionOffset;

  std::unique_ptr<llvm::MemoryBuffer> CleanFile(
      removeCodeCompletionTokens(FileBufOrErr.get().get(), CodeCompletionToken,
                                 &CodeCompletionOffset));

  if (CodeCompletionOffset == ~0U) {
    llvm::errs() << "could not find code completion token \""
                 << CodeCompletionToken << "\"\n";
    return 1;
  }
  llvm::outs() << "found code completion token " << CodeCompletionToken
               << " at offset " << CodeCompletionOffset << "\n";
  llvm::errs() << "found code completion token " << CodeCompletionToken
               << " at offset " << CodeCompletionOffset << "\n";

  CompilerInvocation Invocation(InitInvok);
  Invocation.setCodeCompletionPoint(CleanFile.get(), CodeCompletionOffset);

  std::unique_ptr<ide::OnDiskCodeCompletionCache> OnDiskCache;
  if (!options::CompletionCachePath.empty()) {
    OnDiskCache = llvm::make_unique<ide::OnDiskCodeCompletionCache>(
        options::CompletionCachePath);
  }
  ide::CodeCompletionCache CompletionCache(OnDiskCache.get());
  ide::CodeCompletionContext CompletionContext(CompletionCache);

  // Create a CodeCompletionConsumer.
  std::unique_ptr<ide::CodeCompletionConsumer> Consumer(
      new ide::PrintingCodeCompletionConsumer(
          llvm::outs(), CodeCompletionKeywords));

  // Cerate a factory for code completion callbacks that will feed the
  // Consumer.
  std::unique_ptr<CodeCompletionCallbacksFactory> CompletionCallbacksFactory(
      ide::makeCodeCompletionCallbacksFactory(CompletionContext,
                                              *Consumer.get()));

  Invocation.setCodeCompletionFactory(CompletionCallbacksFactory.get());

  CompilerInstance CI;

  PrintingDiagnosticConsumer PrintDiags;
  if (CodeCompletionDiagnostics) {
    // Display diagnostics to stderr.
    CI.addDiagnosticConsumer(&PrintDiags);
  }
  if (CI.setup(Invocation))
    return 1;
  CI.performSema();
  return 0;
}

static int doREPLCodeCompletion(const CompilerInvocation &InitInvok,
                                StringRef SourceFilename) {
  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> FileBufOrErr =
    llvm::MemoryBuffer::getFile(SourceFilename);
  if (!FileBufOrErr) {
    llvm::errs() << "error opening input file: "
                 << FileBufOrErr.getError().message() << '\n';
    return 1;
  }

  StringRef BufferText = FileBufOrErr.get()->getBuffer();
  // Drop a single newline character from the buffer.
  if (BufferText.endswith("\n"))
    BufferText = BufferText.drop_back(1);

  CompilerInvocation Invocation(InitInvok);
  Invocation.setInputKind(InputFileKind::IFK_Swift_REPL);

  CompilerInstance CI;

  // Display diagnostics to stderr.
  PrintingDiagnosticConsumer PrintDiags;
  CI.addDiagnosticConsumer(&PrintDiags);
  if (CI.setup(Invocation))
    return 1;
  CI.performSema();

  SourceFile &SF = CI.getMainModule()->getMainSourceFile(SourceFileKind::REPL);

  REPLCompletions REPLCompl;
  REPLCompl.populate(SF, BufferText);
  llvm::outs() << "Begin completions\n";
  for (StringRef S : REPLCompl.getCompletionList()) {
    llvm::outs() << S << "\n";
  }
  llvm::outs() << "End completions\n";

  return 0;
}

//============================================================================//
// Syntax Coloring
//============================================================================//

namespace {

class PrintSyntaxColorWalker : public ide::SyntaxModelWalker {
  SourceManager &SM;
  unsigned BufferID;
  llvm::raw_ostream &OS;
  bool TerminalOutput;
  const char *BufStart;
  const char *BufEnd;
  const char *CurrBufPtr;

public:
  PrintSyntaxColorWalker(SourceManager &SM,
                         unsigned BufferID,
                         llvm::raw_ostream &OS,
                         bool TerminalOutput)
    : SM(SM), BufferID(BufferID), OS(OS), TerminalOutput(TerminalOutput) {
    CharSourceRange entireRange = SM.getRangeForBuffer(BufferID);
    StringRef Buffer = SM.extractText(entireRange);
    BufStart = Buffer.data();
    BufEnd = Buffer.data() + Buffer.size();
    CurrBufPtr = BufStart;
  }

  bool walkToNodePre(SyntaxNode Node) override {
    if (shouldIgnore(Node))
      return false;

    const char *LocPtr = getPtr(Node.Range.getStart());
    printSourceUntil(LocPtr);
    wrap(Node.Kind, /*Begin=*/true);
    return true;
  }

  bool walkToNodePost(SyntaxNode Node) override {
    if (shouldIgnore(Node))
      return true;

    const char *LocPtr = getPtr(Node.Range.getStart());
    unsigned Length = Node.Range.getByteLength();
    if (Node.Kind == SyntaxNodeKind::CommentLine) {
      if (LocPtr[Length-1] == '\n')
        --Length; // Wrapping should be in the same line.
    }
    printSourceUntil(LocPtr + Length);
    wrap(Node.Kind, /*Begin=*/false);
    return true;
  }

  void wrap(SyntaxNodeKind Kind, bool Begin) {
    if (TerminalOutput) {
      wrapForTerminal(Kind, Begin);
    } else {
      wrapForTest(Kind, Begin);
    }
  }

  bool shouldIgnore(SyntaxNode Node) const {
    const char *LocPtr = getPtr(Node.Range.getStart());
    if (Node.Kind == SyntaxNodeKind::CommentLine && !TerminalOutput) {
      // Ignore CHECK lines.
      if (StringRef(LocPtr, BufEnd - LocPtr).startswith("// CHECK"))
        return true;
    }
    return false;
  }

  const char *getPtr(SourceLoc Loc) const {
    return BufStart + SM.getLocOffsetInBuffer(Loc, BufferID);
  }

  void printSourceUntil(const char *Ptr) {
    assert(Ptr >= CurrBufPtr && Ptr <= BufEnd);
    StringRef Text = StringRef(CurrBufPtr, Ptr-CurrBufPtr);
    // Skip all "// CHECK" lines.
    while (true) {
      size_t Idx = Text.find("// CHECK");
      if (Idx == StringRef::npos)
        break;
      OS << Text.substr(0, Idx);
      Idx = Text.find('\n', Idx);
      Text = Idx == StringRef::npos ? StringRef() : Text.substr(Idx+1);
    }
    OS << Text;
    CurrBufPtr = Ptr;
  }

  void wrapForTest(SyntaxNodeKind Kind, bool Begin) {
    const char *Id = 0;
    switch (Kind) {
    case SyntaxNodeKind::Keyword: Id = "kw"; break;
    // Skip identifier.
    case SyntaxNodeKind::Identifier: return;
    case SyntaxNodeKind::DollarIdent: Id = "dollar"; break;
    case SyntaxNodeKind::Integer: Id = "int"; break;
    case SyntaxNodeKind::Floating: Id = "float"; break;
    case SyntaxNodeKind::String: Id = "str"; break;
    case SyntaxNodeKind::StringInterpolationAnchor: Id = "anchor"; break;
    case SyntaxNodeKind::CommentLine: Id = "comment-line"; break;
    case SyntaxNodeKind::CommentBlock: Id = "comment-block"; break;
    case SyntaxNodeKind::CommentMarker: Id = "comment-marker"; break;
    case SyntaxNodeKind::CommentURL: Id = "comment-url"; break;
    case SyntaxNodeKind::DocCommentLine: Id = "doc-comment-line"; break;
    case SyntaxNodeKind::DocCommentBlock: Id = "doc-comment-block"; break;
    case SyntaxNodeKind::DocCommentField: Id = "doc-comment-field"; break;
    case SyntaxNodeKind::TypeId: Id = "type"; break;
    case SyntaxNodeKind::BuildConfigKeyword: Id = "#kw"; break;
    case SyntaxNodeKind::BuildConfigId: Id = "#id"; break;
    case SyntaxNodeKind::AttributeId: Id = "attr-id"; break;
    case SyntaxNodeKind::AttributeBuiltin: Id = "attr-builtin"; break;
    case SyntaxNodeKind::EditorPlaceholder: Id = "placeholder"; break;
    case SyntaxNodeKind::ObjectLiteral: Id = "object-literal"; break;
    }

    OS << (Begin ? "<" : "</") << Id << '>';
  }

  void wrapForTerminal(SyntaxNodeKind Kind, bool Begin) {
    llvm::raw_ostream::Colors Col;
    switch (Kind) {
    case SyntaxNodeKind::Keyword: Col = llvm::raw_ostream::MAGENTA; break;
    // Skip identifier.
    case SyntaxNodeKind::Identifier: return;
    case SyntaxNodeKind::DollarIdent: Col = llvm::raw_ostream::MAGENTA; break;
    case SyntaxNodeKind::Integer: Col = llvm::raw_ostream::BLUE; break;
    case SyntaxNodeKind::Floating: Col = llvm::raw_ostream::BLUE; break;
    case SyntaxNodeKind::String: Col = llvm::raw_ostream::RED; break;
    case SyntaxNodeKind::StringInterpolationAnchor: Col = llvm::raw_ostream::CYAN; break;
    case SyntaxNodeKind::CommentLine: Col = llvm::raw_ostream::GREEN; break;
    case SyntaxNodeKind::CommentBlock: Col = llvm::raw_ostream::GREEN; break;
    case SyntaxNodeKind::CommentMarker: Col = llvm::raw_ostream::MAGENTA; break;
    case SyntaxNodeKind::DocCommentLine: Col = llvm::raw_ostream::CYAN; break;
    case SyntaxNodeKind::DocCommentBlock: Col = llvm::raw_ostream::CYAN; break;
    case SyntaxNodeKind::DocCommentField: Col = llvm::raw_ostream::WHITE; break;
    case SyntaxNodeKind::CommentURL: Col = llvm::raw_ostream::RED; break;
    case SyntaxNodeKind::TypeId: Col = llvm::raw_ostream::CYAN; break;
    case SyntaxNodeKind::BuildConfigKeyword: Col = llvm::raw_ostream::YELLOW; break;
    case SyntaxNodeKind::BuildConfigId: Col = llvm::raw_ostream::YELLOW; break;
    case SyntaxNodeKind::AttributeId: Col = llvm::raw_ostream::CYAN; break;
    case SyntaxNodeKind::AttributeBuiltin: Col = llvm::raw_ostream::MAGENTA; break;
    case SyntaxNodeKind::EditorPlaceholder: Col = llvm::raw_ostream::YELLOW; break;
    case SyntaxNodeKind::ObjectLiteral: return;
    }

    if (Begin) {
      if (const char *CStr =
          llvm::sys::Process::OutputColor(Col, false, false)) {
        OS << CStr;
      }
    } else {
      OS << llvm::sys::Process::ResetColor();
    }
  }

  void finished() {
    OS << StringRef(CurrBufPtr, BufEnd-CurrBufPtr);
  }
};

}

static int doSyntaxColoring(const CompilerInvocation &InitInvok,
                            StringRef SourceFilename,
                            bool TerminalOutput,
                            bool RunTypeChecker) {
  CompilerInvocation Invocation(InitInvok);
  Invocation.addInputFilename(SourceFilename);
  Invocation.getLangOptions().DisableAvailabilityChecking = false;

  CompilerInstance CI;

  // Display diagnostics to stderr.
  PrintingDiagnosticConsumer PrintDiags;
  CI.addDiagnosticConsumer(&PrintDiags);
  if (CI.setup(Invocation))
    return 1;
  if (!RunTypeChecker)
    CI.performParseOnly();
  else
    CI.performSema();

  unsigned BufID = CI.getInputBufferIDs().back();
  SourceFile *SF = nullptr;
  for (auto Unit : CI.getMainModule()->getFiles()) {
    SF = dyn_cast<SourceFile>(Unit);
    if (SF)
      break;
  }
  assert(SF && "no source file?");
  ide::SyntaxModelContext ColorContext(*SF);
  PrintSyntaxColorWalker ColorWalker(CI.getSourceMgr(), BufID, llvm::outs(),
                                     TerminalOutput);
  ColorContext.walk(ColorWalker);
  ColorWalker.finished();

  return 0;
}

//============================================================================//
// Dump API
//============================================================================//

namespace {

class DumpAPIWalker : public ASTWalker {
public:
  SmallVector<SourceRange, 1> SourceRangesToDelete;
  SmallVector<CharSourceRange, 1> CharSourceRangesToDelete;

  DumpAPIWalker() {}

  bool walkToDeclPre(Decl *D) override {
    const bool exploreChildren = true;
    const bool doneWithNode = false;

    // Ignore implicit Decls and don't descend into them; they often
    // have a bogus source range that collides with something we'll
    // want to keep.
    if (D->isImplicit())
      return doneWithNode;

    const auto *VD = dyn_cast<ValueDecl>(D);
    if (!VD)
      return exploreChildren; // Look for a nested ValueDecl.

    if (VD->getFormalAccess() == Accessibility::Public) {
      // Delete the bodies of public function and subscript decls
      if (auto *AFD = dyn_cast<AbstractFunctionDecl>(D)) {
        SourceRangesToDelete.push_back(AFD->getBodySourceRange());
        return doneWithNode;
      } else if (auto *SD = dyn_cast<SubscriptDecl>(D)) {
        SourceRangesToDelete.push_back(SD->getBracesRange());
        return doneWithNode;
      }
      return exploreChildren; // Handle nested decls.
    }

    // Delete entire non-public decls, including...
    for (const auto &Single : VD->getRawComment().Comments)
      CharSourceRangesToDelete.push_back(Single.Range); // attached comments

    VD->getAttrs().getAttrRanges(SourceRangesToDelete); // and attributes

    if (const auto *Var = dyn_cast<VarDecl>(VD)) {
      // VarDecls extend through their entire parent PatternBinding.
      SourceRangesToDelete.push_back(
          Var->getParentPatternBinding()->getSourceRange());
    } else {
      SourceRangesToDelete.push_back(VD->getSourceRange());
    }
    return doneWithNode;
  }
};
}

static int doDumpAPI(const CompilerInvocation &InitInvok,
                     StringRef SourceFilename) {
  CompilerInvocation Invocation(InitInvok);
  Invocation.addInputFilename(SourceFilename);
  Invocation.getLangOptions().DisableAvailabilityChecking = false;

  CompilerInstance CI;

  // Display diagnostics to stderr.
  PrintingDiagnosticConsumer PrintDiags;
  CI.addDiagnosticConsumer(&PrintDiags);
  if (CI.setup(Invocation))
    return 1;
  CI.performSema();

  unsigned BufID = CI.getInputBufferIDs().back();
  SourceFile *SF = nullptr;
  for (auto Unit : CI.getMainModule()->getFiles()) {
    SF = dyn_cast<SourceFile>(Unit);
    if (SF)
      break;
  }
  assert(SF && "no source file?");
  const auto &SM = CI.getSourceMgr();
  DumpAPIWalker Walker;
  CI.getMainModule()->walk(Walker);

  //===--- rewriting ------------------------------------------------------===//
  StringRef Input = SM.getLLVMSourceMgr().getMemoryBuffer(BufID)->getBuffer();
  clang::RewriteBuffer RewriteBuf;
  RewriteBuf.Initialize(Input);

  // Convert all the accumulated SourceRanges into CharSourceRanges
  auto &CharSourceRanges = Walker.CharSourceRangesToDelete;
  for (const auto &SR : Walker.SourceRangesToDelete) {
    auto End = Lexer::getLocForEndOfToken(SM, SR.End);
    CharSourceRanges.push_back(CharSourceRange(SM, SR.Start, End));
  }

  // Drop any invalid ranges
  CharSourceRanges.erase(
      std::remove_if(CharSourceRanges.begin(), CharSourceRanges.end(),
                     [](const CharSourceRange CSR) { return !CSR.isValid(); }),
      CharSourceRanges.end());

  // Sort them so we can easily handle overlaps; the SourceManager
  // doesn't cope well with overlapping deletions.
  std::sort(CharSourceRanges.begin(), CharSourceRanges.end(),
            [&](CharSourceRange LHS, CharSourceRange RHS) {
              return SM.isBeforeInBuffer(LHS.getStart(), RHS.getStart());
            });

  // A little function that we can re-use to delete a CharSourceRange
  const auto Del = [&](CharSourceRange CR) {
    const auto Start = SM.getLocOffsetInBuffer(CR.getStart(), BufID);
    const auto End = SM.getLocOffsetInBuffer(CR.getEnd(), BufID);
    RewriteBuf.RemoveText(Start, End - Start, true);
  };

  // Accumulate all overlapping ranges, deleting the previous one when
  // no overlap is detected.
  CharSourceRange Accumulator;
  for (const auto CR : CharSourceRanges) {
    if (!Accumulator.isValid()) {
      Accumulator = CR;
    } else if (Accumulator.overlaps(CR)) {
      Accumulator.widen(CR);
    } else {
      Del(Accumulator);
      Accumulator = CR;
    }
  }

  // The last valid range still needs to be deleted.
  if (Accumulator.isValid())
    Del(Accumulator);

  // Write the edited buffer to stdout
  RewriteBuf.write(llvm::outs());

  return 0;
}

static int doDumpImporterLookupTables(const CompilerInvocation &InitInvok,
                                      StringRef SourceFilename) {
  if (options::ImportObjCHeader.empty()) {
    llvm::errs() << "implicit header required\n";
    llvm::cl::PrintHelpMessage();
    return 1;
  }

  CompilerInvocation Invocation(InitInvok);
  Invocation.addInputFilename(SourceFilename);

  // We must use the Swift lookup tables.
  Invocation.getClangImporterOptions().UseSwiftLookupTables = true;

  CompilerInstance CI;

  // Display diagnostics to stderr.
  PrintingDiagnosticConsumer PrintDiags;
  CI.addDiagnosticConsumer(&PrintDiags);
  if (CI.setup(Invocation))
    return 1;
  CI.performSema();

  auto &Context = CI.getASTContext();
  auto &Importer = static_cast<ClangImporter &>(
                     *Context.getClangModuleLoader());
  Importer.dumpSwiftLookupTables();

  return 0;
}

//============================================================================//
// Structure Annotation
//============================================================================//

class StructureAnnotator : public ide::SyntaxModelWalker {
  SourceManager &SM;
  unsigned BufferID;
  clang::RewriteBuffer RewriteBuf;
  std::vector<SyntaxStructureNode> NodeStack;
  CharSourceRange LastPoppedNodeRange;

public:
  StructureAnnotator(SourceManager &SM, unsigned BufID)
    : SM(SM), BufferID(BufID) {
    StringRef Input = SM.getLLVMSourceMgr().getMemoryBuffer(BufID)->getBuffer();
    RewriteBuf.Initialize(Input);
    removeCheckLines(Input);
  }

  void printResult(raw_ostream &OS) {
    RewriteBuf.write(OS);
  }

private:
  bool walkToSubStructurePre(SyntaxStructureNode Node) override {
    const SyntaxStructureNode *Parent = nullptr;
    if (!NodeStack.empty())
      Parent = &NodeStack.back();
    checkNode(Node, Parent);
    NodeStack.push_back(Node);

    tagRange(Node.Range, getTagName(Node.Kind), Node);
    if (Node.NameRange.isValid())
      tagRange(Node.NameRange, "name", Node);
    for (auto &TyRange : Node.InheritedTypeRanges) {
      tagRange(TyRange, "inherited", Node);
    }
    for (auto &Elem : Node.Elements) {
      tagRange(Elem.Range, getTagName(Elem.Kind), Node);
    }

    return true;
  }

  void tagRange(CharSourceRange Range, StringRef tagName,
                const SyntaxStructureNode &Node) {
    checkRange(Range, &Node);
    std::string BeginTag;
    llvm::raw_string_ostream(BeginTag) << '<' << tagName << '>';
    std::string EndTag;
    llvm::raw_string_ostream(EndTag) << "</" << tagName << '>';

    unsigned Offset = SM.getLocOffsetInBuffer(Range.getStart(), BufferID);
    RewriteBuf.InsertTextAfter(Offset, BeginTag);
    RewriteBuf.InsertTextBefore(Offset+Range.getByteLength(), EndTag);
  }

  static StringRef getTagName(SyntaxStructureKind K) {
    switch (K) {
      case SyntaxStructureKind::Class: return "class";
      case SyntaxStructureKind::Struct: return "struct";
      case SyntaxStructureKind::Protocol: return "protocol";
      case SyntaxStructureKind::Enum: return "enum";
      case SyntaxStructureKind::Extension: return "extension";
      case SyntaxStructureKind::FreeFunction: return "ffunc";
      case SyntaxStructureKind::InstanceFunction: return "ifunc";
      case SyntaxStructureKind::StaticFunction: return "sfunc";
      case SyntaxStructureKind::ClassFunction: return "cfunc";
      case SyntaxStructureKind::GlobalVariable: return "gvar";
      case SyntaxStructureKind::InstanceVariable: return "property";
      case SyntaxStructureKind::StaticVariable: return "svar";
      case SyntaxStructureKind::ClassVariable: return "cvar";
      case SyntaxStructureKind::EnumCase: return "enum-case";
      case SyntaxStructureKind::EnumElement: return "enum-elem";
      case SyntaxStructureKind::Parameter: return "param";
      case SyntaxStructureKind::ForEachStatement: return "foreach";
      case SyntaxStructureKind::ForStatement: return "for";
      case SyntaxStructureKind::WhileStatement: return "while";
      case SyntaxStructureKind::RepeatWhileStatement: return "repeat-while";
      case SyntaxStructureKind::IfStatement: return "if";
      case SyntaxStructureKind::GuardStatement: return "guard";
      case SyntaxStructureKind::SwitchStatement: return "switch";
      case SyntaxStructureKind::CaseStatement: return "case";
      case SyntaxStructureKind::BraceStatement: return "brace";
      case SyntaxStructureKind::CallExpression: return "call";
      case SyntaxStructureKind::ArrayExpression: return "array";
      case SyntaxStructureKind::DictionaryExpression: return "dictionary";
      case SyntaxStructureKind::ObjectLiteralExpression:
        return "object-literal-expression";
    }
    llvm_unreachable("unhandled tag?");
  }

  static StringRef getTagName(SyntaxStructureElementKind K) {
    switch (K) {
      case SyntaxStructureElementKind::Id: return "elem-id";
      case SyntaxStructureElementKind::Expr: return "elem-expr";
      case SyntaxStructureElementKind::InitExpr: return "elem-initexpr";
      case SyntaxStructureElementKind::ConditionExpr: return "elem-condexpr";
      case SyntaxStructureElementKind::Pattern: return "elem-pattern";
      case SyntaxStructureElementKind::TypeRef: return "elem-typeref";
    }
    llvm_unreachable("unhandled tag?");
  }

  bool walkToSubStructurePost(SyntaxStructureNode Node) override {
    assert(!NodeStack.empty());
    LastPoppedNodeRange = NodeStack.back().Range;
    NodeStack.pop_back();
    return true;
  }

  void checkNode(const SyntaxStructureNode &Node,
                 const SyntaxStructureNode *Parent) {
    checkRange(Node.Range, Parent);
  }

  void checkRange(CharSourceRange Range, const SyntaxStructureNode *Parent) {
    assert(Range.isValid());
    if (Parent) {
      assert(Parent->Range.contains(Range));
    }
    if (LastPoppedNodeRange.isValid()) {
      // FIXME: Initializer expressions (like array literals) are not contained
      // within the global variables nodes.
      // assert(!SM.isBeforeInBuffer(Range.getStart(),
      //                             LastPoppedNodeRange.getEnd()));
    }
  }

  void removeCheckLines(StringRef Input) {
    StringRef CheckStr = "CHECK";
    size_t Pos = 0;
    while (true) {
      Pos = Input.find(CheckStr, Pos);
      if (Pos == StringRef::npos)
        break;
      Pos = Input.substr(0, Pos).rfind("//");
      assert(Pos != StringRef::npos);
      size_t EndLine = Input.find('\n', Pos);
      assert(EndLine != StringRef::npos);
      ++EndLine;
      RewriteBuf.RemoveText(Pos, EndLine-Pos);
      Pos = EndLine;
    }
  }
};

static int doStructureAnnotation(const CompilerInvocation &InitInvok,
                                 StringRef SourceFilename) {
  CompilerInvocation Invocation(InitInvok);
  Invocation.addInputFilename(SourceFilename);

  CompilerInstance CI;

  // Display diagnostics to stderr.
  PrintingDiagnosticConsumer PrintDiags;
  CI.addDiagnosticConsumer(&PrintDiags);
  if (CI.setup(Invocation))
    return 1;
  CI.performParseOnly();

  unsigned BufID = CI.getInputBufferIDs().back();
  ide::SyntaxModelContext StructureContext(
      CI.getMainModule()->getMainSourceFile(SourceFileKind::Main));
  StructureAnnotator Annotator(CI.getSourceMgr(), BufID);
  StructureContext.walk(Annotator);
  Annotator.printResult(llvm::outs());
  return 0;
}

//============================================================================//
// Semantic Annotation
//============================================================================//

namespace {

class AnnotationPrinter : public ide::SourceEntityWalker {
  SourceManager &SM;
  unsigned BufferID;
  llvm::raw_ostream &OS;
  bool TerminalOutput;
  const char *BufStart;
  const char *BufEnd;
  const char *CurrBufPtr;

public:
  AnnotationPrinter(SourceManager &SM,
                    unsigned BufferID,
                    llvm::raw_ostream &OS,
                    bool TerminalOutput)
    : SM(SM), BufferID(BufferID), OS(OS), TerminalOutput(TerminalOutput) {
    CharSourceRange entireRange = SM.getRangeForBuffer(BufferID);
    StringRef Buffer = SM.extractText(entireRange);
    BufStart = Buffer.data();
    BufEnd = Buffer.data() + Buffer.size();
    CurrBufPtr = BufStart;
  }

  void finished() {
    OS << StringRef(CurrBufPtr, BufEnd-CurrBufPtr);
  }

private:
  struct SemanticSourceEntity {
    CharSourceRange Range;
    ValueDecl *Dcl = nullptr;
    TypeDecl *CtorTyRef = nullptr;
    ModuleEntity Mod;
    bool IsRef = true;

    SemanticSourceEntity(CharSourceRange Range,
                         ValueDecl *Dcl,
                         TypeDecl *CtorTyRef,
                         bool IsRef)
      : Range(Range),
        Dcl(Dcl),
        CtorTyRef(CtorTyRef),
        IsRef(IsRef) {}

    SemanticSourceEntity(CharSourceRange Range,
                         ModuleEntity Mod)
      : Range(Range),
        Mod(Mod) {}
  };

  bool walkToDeclPre(Decl *D, CharSourceRange Range) override {
    if (Range.getByteLength() == 0)
      return true;
    if (ValueDecl *VD = dyn_cast<ValueDecl>(D))
      annotateSourceEntity({ Range, VD, nullptr, /*IsRef=*/false});
    return true;
  }

  bool visitDeclReference(ValueDecl *D, CharSourceRange Range,
                          TypeDecl *CtorTyRef, Type Ty) override {
    annotateSourceEntity({ Range, D, CtorTyRef, /*IsRef=*/true });
    return true;
  }

  bool visitSubscriptReference(ValueDecl *D, CharSourceRange Range,
                               bool IsOpenBracket) override {
    return visitDeclReference(D, Range, nullptr, Type());
  }

  bool visitCallArgName(Identifier Name, CharSourceRange Range,
                        ValueDecl *D) override {
    annotateSourceEntity({ Range, D, nullptr, /*IsRef=*/true });
    return true;
  }

  bool visitModuleReference(ModuleEntity Mod, CharSourceRange Range) override {
    annotateSourceEntity({ Range, Mod });
    return true;
  }

  void annotateSourceEntity(const SemanticSourceEntity &Entity) {
    const char *LocPtr =
        BufStart + SM.getLocOffsetInBuffer(Entity.Range.getStart(), BufferID);

    unsigned Length = Entity.Range.getByteLength();
    assert(LocPtr >= CurrBufPtr);
    printSourceUntil(LocPtr);
    StringRef NodeText = StringRef(LocPtr, Length);
    if (TerminalOutput) {
      if (!wrapForTerminal(Entity, NodeText))
        OS << NodeText;
    } else {
      if (!wrapForTest(Entity, StringRef(LocPtr, Length)))
        OS << NodeText;
    }
    CurrBufPtr = LocPtr + Length;
  }

  void printSourceUntil(const char *Ptr) {
    StringRef Text = StringRef(CurrBufPtr, Ptr-CurrBufPtr);
    // Skip all "// CHECK" lines.
    while (true) {
      size_t Idx = Text.find("// CHECK");
      if (Idx == StringRef::npos)
        break;
      OS << Text.substr(0, Idx);
      Idx = Text.find('\n', Idx);
      Text = Idx == StringRef::npos ? StringRef() : Text.substr(Idx+1);
    }
    OS << Text;
  }

  void printLoc(SourceLoc Loc, raw_ostream &OS) {
    OS << '@';
    if (Loc.isValid()) {
      auto LineCol = SM.getLineAndColumn(Loc, BufferID);
      OS  << LineCol.first << ':' << LineCol.second;
    }
  }

  bool wrapForTest(const SemanticSourceEntity &Entity, StringRef Text) {
    OS << '<';

    bool IsInSystemModule = false;
    ValueDecl *D = Entity.Dcl;
    if (D) {
      IsInSystemModule = D->getModuleContext()->isSystemModule();
      if (IsInSystemModule)
        OS << 'i';
      if (isa<ConstructorDecl>(D) && Entity.IsRef) {
        OS << "Ctor";
        printLoc(D->getLoc(), OS);
        if (Entity.CtorTyRef) {
          OS << '-';
          OS << Decl::getKindName(Entity.CtorTyRef->getKind());
          printLoc(Entity.CtorTyRef->getLoc(), OS);
        }
      } else {
        OS << Decl::getKindName(D->getKind());
        if (Entity.IsRef)
          printLoc(D->getLoc(), OS);
      }

    } else {
      if (Entity.Mod.isSystemModule())
        OS << 'i';
      OS << "Mod";
    }

    OS << '>';
    OS << Text;
    OS << "</";

    if (D) {
      if (IsInSystemModule)
        OS << 'i';
      if (isa<ConstructorDecl>(D) && Entity.IsRef) {
        OS << "Ctor";
      } else {
        OS << Decl::getKindName(D->getKind());
      }

    } else {
      if (Entity.Mod.isSystemModule())
        OS << 'i';
      OS << "Mod";
    }
    OS << '>';
    return true;
  }

  bool wrapForTerminal(const SemanticSourceEntity &Entity, StringRef Text) {
    llvm::raw_ostream::Colors Col;
    switch (Entity.Dcl->getKind()) {
    default:
      return false;

    case DeclKind::Var:
      Col = llvm::raw_ostream::GREEN;
      break;
    case DeclKind::Func:
    case DeclKind::Constructor:
    case DeclKind::Destructor:
      Col = llvm::raw_ostream::MAGENTA;
      break;
    case DeclKind::Class:
      Col = llvm::raw_ostream::RED;
      break;
    case DeclKind::Struct:
      Col = llvm::raw_ostream::BLUE;
      break;
    case DeclKind::Protocol:
      Col = llvm::raw_ostream::YELLOW;
      break;
    case DeclKind::TypeAlias:
    case DeclKind::AssociatedType:
    case DeclKind::GenericTypeParam:
      Col = llvm::raw_ostream::CYAN; break;
    }

    if (const char *CStr =
        llvm::sys::Process::OutputColor(Col, false, false)) {
      OS << CStr;
    }
    OS << Text;
    OS << llvm::sys::Process::ResetColor();
    return true;
  }
};

} // unnamed namespace

static int doSemanticAnnotation(const CompilerInvocation &InitInvok,
                                StringRef SourceFilename,
                                bool TerminalOutput) {
  CompilerInvocation Invocation(InitInvok);
  Invocation.addInputFilename(SourceFilename);

  CompilerInstance CI;

  // Display diagnostics to stderr.
  PrintingDiagnosticConsumer PrintDiags;
  CI.addDiagnosticConsumer(&PrintDiags);
  if (CI.setup(Invocation))
    return 1;
  CI.performSema();

  unsigned BufID = CI.getInputBufferIDs().back();
  AnnotationPrinter AnnotPrinter(CI.getSourceMgr(), BufID, llvm::outs(),
                                 TerminalOutput);
  AnnotPrinter.walk(*CI.getMainModule());
  AnnotPrinter.finished();
  return 0;
}

static int doInputCompletenessTest(StringRef SourceFilename) {
  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> FileBufOrErr =
    llvm::MemoryBuffer::getFile(SourceFilename);
  if (!FileBufOrErr) {
    llvm::errs() << "error opening input file: "
                 << FileBufOrErr.getError().message() << '\n';
    return 1;
  }

  llvm::raw_ostream &OS = llvm::outs();
  OS << SourceFilename << ": ";
  if (isSourceInputComplete(std::move(FileBufOrErr.get())).IsComplete) {
    OS << "IS_COMPLETE\n";
  } else {
    OS << "IS_INCOMPLETE\n";
  }
  return 0;
}

//============================================================================//
// AST printing
//============================================================================//

static Module *getModuleByFullName(ASTContext &Context, StringRef ModuleName) {
  SmallVector<std::pair<Identifier, SourceLoc>, 4>
      AccessPath;
  while (!ModuleName.empty()) {
    StringRef SubModuleName;
    std::tie(SubModuleName, ModuleName) = ModuleName.split('.');
    AccessPath.push_back(
        { Context.getIdentifier(SubModuleName), SourceLoc() });
  }
  return Context.getModule(AccessPath);
}

static Module *getModuleByFullName(ASTContext &Context, Identifier ModuleName) {
  return Context.getModule(std::make_pair(ModuleName, SourceLoc()));
}

static int doPrintAST(const CompilerInvocation &InitInvok,
                      StringRef SourceFilename,
                      bool RunTypeChecker,
                      const PrintOptions &Options,
                      StringRef MangledNameToFind,
                      StringRef DebugClientDiscriminator) {
  CompilerInvocation Invocation(InitInvok);
  Invocation.addInputFilename(SourceFilename);

  CompilerInstance CI;

  // Display diagnostics to stderr.
  PrintingDiagnosticConsumer PrintDiags;
  CI.addDiagnosticConsumer(&PrintDiags);
  if (CI.setup(Invocation))
    return 1;

  std::unique_ptr<DebuggerClient> DebuggerClient;
  if (!DebugClientDiscriminator.empty()) {
    DebuggerClient.reset(
      new PrivateDiscriminatorPreferenceClient(CI.getASTContext(),
                                               DebugClientDiscriminator)
    );
    CI.getMainModule()->setDebugClient(DebuggerClient.get());
  }

  if (!RunTypeChecker)
    CI.performParseOnly();
  else
    CI.performSema();

  if (MangledNameToFind.empty()) {
    Module *M = CI.getMainModule();
    M->getMainSourceFile(Invocation.getSourceFileKind()).print(llvm::outs(),
                                                               Options);
    return EXIT_SUCCESS;
  }

  // If we were given a mangled name, do a very simple form of LLDB's logic to
  // look up a type based on that name.
  Demangle::NodePointer node =
    demangle_wrappers::demangleSymbolAsNode(MangledNameToFind);
  using NodeKind = Demangle::Node::Kind;

  if (!node) {
    llvm::errs() << "Unable to demangle name.\n";
    return EXIT_FAILURE;
  }
  node = node->getFirstChild();

  // FIXME: Look up things other than types.
  if (node->getKind() != NodeKind::TypeMangling) {
    llvm::errs() << "Name does not refer to a type.\n";
    return EXIT_FAILURE;
  }
  node = node->getFirstChild();
  assert(node->getKind() == NodeKind::Type);
  node = node->getFirstChild();

  switch (node->getKind()) {
  case NodeKind::Class:
  case NodeKind::Enum:
  case NodeKind::Protocol:
  case NodeKind::Structure:
    break;
  default:
    llvm::errs() << "Name does not refer to a nominal type.\n";
    return EXIT_FAILURE;
  }

  ASTContext &ctx = CI.getASTContext();

  SmallVector<std::pair<DeclName, Identifier>, 4> identifiers;
  do {
    switch (node->getKind()) {
    case NodeKind::Class:
    case NodeKind::Enum:
    case NodeKind::Protocol:
    case NodeKind::Structure:
      break;
    default:
      llvm::errs() << "Name does not refer to a nominal type.\n";
      return EXIT_FAILURE;
    }

    auto nameNode = node->getChild(1);
    switch (nameNode->getKind()) {
    case NodeKind::Identifier:
      identifiers.push_back({ ctx.getIdentifier(nameNode->getText()),
                              Identifier() });
      break;
    case NodeKind::PrivateDeclName:
      identifiers.push_back({
        ctx.getIdentifier(nameNode->getChild(1)->getText()),
        ctx.getIdentifier(nameNode->getChild(0)->getText())
      });
      break;
    default:
      llvm::errs() << "Unsupported name kind.\n";
      return EXIT_FAILURE;
    }

    node = node->getChild(0);
  } while (node->getKind() != NodeKind::Module);

  Module *M = getModuleByFullName(ctx, node->getText());
  SmallVector<ValueDecl *, 4> results;
  M->lookupMember(results, M, identifiers.back().first,
                  identifiers.back().second);

  if (results.empty()) {
    llvm::errs() << "No matching declarations found for "
      << MangledNameToFind << ".\n";
    return EXIT_FAILURE;
  }

  // Progressively perform lookup into matching containers.
  for (auto member : reversed(llvm::makeArrayRef(identifiers).drop_back())) {
    decltype(results) prevResults;
    std::swap(results, prevResults);

    for (auto container : prevResults) {
      M->lookupMember(results, cast<NominalTypeDecl>(container),
                      member.first, member.second);
    }

    if (results.empty()) {
      llvm::errs() << "No matching declarations found for "
        << MangledNameToFind << ".\n";
      return EXIT_FAILURE;
    }
  }

  for (auto *VD : results)
    VD->print(llvm::outs(), Options);

  return EXIT_SUCCESS;
}

static int doPrintLocalTypes(const CompilerInvocation &InitInvok,
                             const std::vector<std::string> ModulesToPrint) {
  using NodeKind = Demangle::Node::Kind;

  CompilerInvocation Invocation(InitInvok);
  CompilerInstance CI;
  PrintingDiagnosticConsumer PrintDiags;
  CI.addDiagnosticConsumer(&PrintDiags);
  if (CI.setup(Invocation))
    return 1;

  auto &Context = CI.getASTContext();

  auto *Stdlib = getModuleByFullName(Context, Context.StdlibModuleName);
  if (!Stdlib)
    return 1;

  int ExitCode = 0;

  PrintOptions Options = PrintOptions::printEverything();

  for (StringRef ModuleName : ModulesToPrint) {
    auto *M = getModuleByFullName(Context, ModuleName);
    if (!M) {
      llvm::errs() << "Couldn't find module " << ModuleName << "\n";
      ExitCode = 1;
      continue;
    }

    SmallVector<TypeDecl *, 10> LocalTypeDecls;
    SmallVector<std::string, 10> MangledNames;

    // Sneak into the module and get all of the local type decls
    M->getLocalTypeDecls(LocalTypeDecls);

    // Simulate already having mangled names
    for (auto LTD : LocalTypeDecls) {
      SmallString<64> MangledName;
      llvm::raw_svector_ostream Buffer(MangledName);
      Mangle::Mangler Mangler(Buffer, /*DWARFMangling*/ true);
      Mangler.mangleTypeForDebugger(LTD->getDeclaredType(), LTD->getDeclContext());
      MangledNames.push_back(Buffer.str());
    }

    // Simulate the demangling / parsing process
    for (auto MangledName : MangledNames) {

      // Global
      auto node = demangle_wrappers::demangleSymbolAsNode(MangledName);

      // TypeMangling
      node = node->getFirstChild();

      // Type
      node = node->getFirstChild();
      auto typeNode = node;

      // Nominal Type
      node = node->getFirstChild();

      switch (node->getKind()) {
        case NodeKind::Structure:
        case NodeKind::Class:
        case NodeKind::Enum:
          break;

        default:
          llvm::errs() << "Expected a nominal type node in " <<
            MangledName << "\n";
          return EXIT_FAILURE;
      }

      while (node->getKind() != NodeKind::LocalDeclName)
        node = node->getChild(1); // local decl name

      // Now simulate the remangling process directly on the
      // LocalDeclName node.
      auto localDeclNameNode = node;

      auto remangled = Demangle::mangleNode(typeNode);

      auto LTD = M->lookupLocalType(remangled);

      if (!LTD) {
        llvm::errs() << "Couldn't find local type " << remangled << "\n";
        return EXIT_FAILURE;
      }

      llvm::outs() << remangled << "\n";

      auto Options = PrintOptions::printEverything();
      Options.PrintAccessibility = false;
      LTD->print(llvm::outs(), Options);
      llvm::outs() << "\n";
    }
  }

  return ExitCode;
}

namespace {
class AnnotatingPrinter : public StreamPrinter {
public:
  using StreamPrinter::StreamPrinter;

  void printDeclPre(const Decl *D) override {
    OS << "<decl:" << Decl::getKindName(D->getKind()) << '>';
  }
  void printDeclLoc(const Decl *D) override {
    OS << "<loc>";
  }
  void printDeclNameEndLoc(const Decl *D) override {
    OS << "</loc>";
  }
  void printDeclPost(const Decl *D) override {
    OS << "</decl>";
  }

  void printTypeRef(const TypeDecl *TD, Identifier Name) override {
    OS << "<ref:" << Decl::getKindName(TD->getKind()) << '>';
    StreamPrinter::printTypeRef(TD, Name);
    OS << "</ref>";
  }
  void printModuleRef(ModuleEntity Mod, Identifier Name) override {
    OS << "<ref:module>";
    StreamPrinter::printModuleRef(Mod, Name);
    OS << "</ref>";
  }
};
}

static int doPrintModules(const CompilerInvocation &InitInvok,
                          const std::vector<std::string> ModulesToPrint,
                          ide::ModuleTraversalOptions TraversalOptions,
                          const PrintOptions &Options,
                          bool AnnotatePrint) {
  CompilerInvocation Invocation(InitInvok);

  CompilerInstance CI;
  // Display diagnostics to stderr.
  PrintingDiagnosticConsumer PrintDiags;
  CI.addDiagnosticConsumer(&PrintDiags);
  if (CI.setup(Invocation))
    return 1;

  auto &Context = CI.getASTContext();

  // Load standard library so that Clang importer can use it.
  auto *Stdlib = getModuleByFullName(Context, Context.StdlibModuleName);
  if (!Stdlib) {
    llvm::errs() << "Failed loading stdlib\n";
    return 1;
  }

  int ExitCode = 0;

  std::unique_ptr<ASTPrinter> Printer;
  if (AnnotatePrint)
    Printer.reset(new AnnotatingPrinter(llvm::outs()));
  else
    Printer.reset(new StreamPrinter(llvm::outs()));

  for (StringRef ModuleToPrint : ModulesToPrint) {
    if (ModuleToPrint.empty()) {
      ExitCode = 1;
      continue;
    }

    // Get the (sub)module to print.
    auto *M = getModuleByFullName(Context, ModuleToPrint);
    if (!M) {
      ExitCode = 1;
      continue;
    }

    // Split the module path.
    std::vector<StringRef> ModuleName;
    while (!ModuleToPrint.empty()) {
      StringRef SubModuleName;
      std::tie(SubModuleName, ModuleToPrint) = ModuleToPrint.split('.');
      ModuleName.push_back(SubModuleName);
    }
    assert(!ModuleName.empty());

    // FIXME: If ModuleToPrint is a submodule, get its top-level module, which
    // will be the DeclContext for all of its Decls since we don't have first-
    // class submodules.
    if (ModuleName.size() > 1) {
      M = getModuleByFullName(Context, ModuleName[0]);
      if (!M) {
        ExitCode = 1;
        continue;
      }
    }

    printSubmoduleInterface(M, ModuleName, TraversalOptions, *Printer, Options);
  }

  return ExitCode;
}

static int doPrintHeaders(const CompilerInvocation &InitInvok,
                          const std::vector<std::string> HeadersToPrint,
                          const PrintOptions &Options,
                          bool AnnotatePrint) {
  CompilerInvocation Invocation(InitInvok);

  CompilerInstance CI;
  // Display diagnostics to stderr.
  PrintingDiagnosticConsumer PrintDiags;
  CI.addDiagnosticConsumer(&PrintDiags);
  if (CI.setup(Invocation))
    return 1;

  auto &Context = CI.getASTContext();

  // Load standard library so that Clang importer can use it.
  auto *Stdlib = getModuleByFullName(Context, Context.StdlibModuleName);
  if (!Stdlib) {
    llvm::errs() << "Failed loading stdlib\n";
    return 1;
  }

  auto &FEOpts = Invocation.getFrontendOptions();
  if (!FEOpts.ImplicitObjCHeaderPath.empty()) {
    auto &Importer = static_cast<ClangImporter &>(
                                              *Context.getClangModuleLoader());
    Importer.importBridgingHeader(FEOpts.ImplicitObjCHeaderPath,
                                  CI.getMainModule(),
                                  /*diagLoc=*/{},
                                  /*trackParsedSymbols=*/true);
  }

  int ExitCode = 0;

  std::unique_ptr<ASTPrinter> Printer;
  if (AnnotatePrint)
    Printer.reset(new AnnotatingPrinter(llvm::outs()));
  else
    Printer.reset(new StreamPrinter(llvm::outs()));

  for (StringRef HeaderToPrint : HeadersToPrint) {
    if (HeaderToPrint.empty()) {
      ExitCode = 1;
      continue;
    }

    printHeaderInterface(HeaderToPrint, Context, *Printer, Options);
  }

  return ExitCode;
}

static int doPrintSwiftFileInterface(const CompilerInvocation &InitInvok,
                                     StringRef SourceFilename,
                                     bool AnnotatePrint) {
  CompilerInvocation Invocation(InitInvok);
  Invocation.addInputFilename(SourceFilename);
  Invocation.getFrontendOptions().PrimaryInput = 0;
  Invocation.getLangOptions().AttachCommentsToDecls = true;
  CompilerInstance CI;
  // Display diagnostics to stderr.
  PrintingDiagnosticConsumer PrintDiags;
  CI.addDiagnosticConsumer(&PrintDiags);
  if (CI.setup(Invocation))
    return 1;
  CI.performSema();

  std::unique_ptr<ASTPrinter> Printer;
  if (AnnotatePrint)
    Printer.reset(new AnnotatingPrinter(llvm::outs()));
  else
    Printer.reset(new StreamPrinter(llvm::outs()));

  PrintOptions Options = PrintOptions::printSwiftFileInterface();
  if (options::PrintOriginalSourceText)
    Options.PrintOriginalSourceText = true;
  printSwiftSourceInterface(*CI.getPrimarySourceFile(), *Printer, Options);

  return 0;
}

namespace {
class ASTTypePrinter : public ASTWalker {
  raw_ostream &OS;
  SourceManager &SM;
  const PrintOptions &Options;

  unsigned IndentLevel = 0;

public:
  ASTTypePrinter(SourceManager &SM, const PrintOptions &Options)
      : OS(llvm::outs()), SM(SM), Options(Options) {}

  bool walkToDeclPre(Decl *D) override {
    if (auto *VD = dyn_cast<ValueDecl>(D)) {
      OS.indent(IndentLevel * 2);
      OS << Decl::getKindName(VD->getKind()) << "Decl '''"
         << VD->getName().str() << "''' ";
      VD->getType().print(OS, Options);
      OS << "\n";
    }
    IndentLevel++;
    return true;
  }

  bool walkToDeclPost(Decl *D) override {
    IndentLevel--;
    return true;
  }

  std::pair<bool, Expr *> walkToExprPre(Expr *E) override {
    StringRef SourceCode{ "<unknown>" };
    unsigned Line = ~0U;

    SourceRange SR = E->getSourceRange();
    if (SR.isValid()) {
      unsigned BufferID = SM.findBufferContainingLoc(SR.Start);
      SourceLoc EndCharLoc = Lexer::getLocForEndOfToken(SM, SR.End);
      SourceCode = SM.extractText({ SR.Start,
                                    SM.getByteDistance(SR.Start, EndCharLoc) });
      unsigned Column;
      std::tie(Line, Column) = SM.getLineAndColumn(SR.Start, BufferID);
    }

    OS.indent(IndentLevel * 2);
    OS << Expr::getKindName(E->getKind()) << "Expr";
    if (Line != ~0U)
      OS << ":" << Line;
    OS << " '''" << SourceCode << "''' ";
    E->getType().print(OS, Options);
    OS << "\n";
    IndentLevel++;
    return { true, E };
  }

  Expr *walkToExprPost(Expr *E) override {
    IndentLevel--;
    return E;
  }
};
} // unnamed namespace

static int doPrintTypes(const CompilerInvocation &InitInvok,
                        StringRef SourceFilename,
                        bool FullyQualifiedTypes) {
  CompilerInvocation Invocation(InitInvok);
  Invocation.addInputFilename(SourceFilename);

  CompilerInstance CI;
  // Display diagnostics to stderr.
  PrintingDiagnosticConsumer PrintDiags;
  CI.addDiagnosticConsumer(&PrintDiags);
  if (CI.setup(Invocation))
    return 1;
  CI.performSema();

  PrintOptions Options = PrintOptions::printEverything();
  Options.FullyQualifiedTypes = FullyQualifiedTypes;
  ASTTypePrinter Printer(CI.getSourceMgr(), Options);

  CI.getMainModule()->walk(Printer);

  return 0;
}

namespace {
class ASTDocCommentDumper : public ASTWalker {
  raw_ostream &OS;
public:
  ASTDocCommentDumper() : OS(llvm::outs()) {}

  bool walkToDeclPre(Decl *D) override {
    if (D->isImplicit())
      return true;

    llvm::markup::MarkupContext MC;
    auto DC = getDocComment(MC, D);
    if(DC.hasValue())
      llvm::markup::dump(DC.getValue()->getDocument(), OS);

    return true;
  }
};
} // end namespace

namespace {
class ASTCommentPrinter : public ASTWalker {
  raw_ostream &OS;
  SourceManager &SM;
  XMLValidator &TheXMLValidator;

public:
  ASTCommentPrinter(SourceManager &SM, XMLValidator &TheXMLValidator)
      : OS(llvm::outs()), SM(SM), TheXMLValidator(TheXMLValidator) {}

  StringRef getBufferIdentifier(SourceLoc Loc) {
    unsigned BufferID = SM.findBufferContainingLoc(Loc);
    return SM.getIdentifierForBuffer(BufferID);
  }

  void printWithEscaping(StringRef Str) {
    for (char C : Str) {
      switch (C) {
      case '\n': OS << "\\n"; break;
      case '\r': OS << "\\r"; break;
      case '\t': OS << "\\t"; break;
      case '\v': OS << "\\v"; break;
      case '\f': OS << "\\f"; break;
      default:   OS << C;     break;
      }
    }
  }

  void printDeclName(const ValueDecl *VD) {
    if (auto *NTD = dyn_cast<NominalTypeDecl>(VD->getDeclContext())) {
      Identifier Id = NTD->getName();
      if (!Id.empty())
        OS << Id.str() << ".";
    }
    Identifier Id = VD->getName();
    if (!Id.empty()) {
      OS << Id.str();
      return;
    }
    if (auto FD = dyn_cast<FuncDecl>(VD)) {
      if (auto *ASD = FD->getAccessorStorageDecl()) {
        switch (FD->getAccessorKind()) {
        case AccessorKind::NotAccessor:
          llvm_unreachable("is not an accessor?");
        case AccessorKind::IsGetter:
          OS << "<getter for ";
          break;
        case AccessorKind::IsSetter:
          OS << "<setter for ";
          break;
        case AccessorKind::IsWillSet:
          OS << "<willSet for ";
          break;
        case AccessorKind::IsDidSet:
          OS << "<didSet for ";
          break;
        case AccessorKind::IsAddressor:
          OS << "<addressor for ";
          break;
        case AccessorKind::IsMutableAddressor:
          OS << "<mutableAddressor for ";
          break;
        case AccessorKind::IsMaterializeForSet:
          OS << "<materializeForSet for ";
          break;
        }
        printDeclName(ASD);
        OS << ">";
        return;
      }
    }
    OS << "<anonymous>";
  }

  void printRawComment(const RawComment &RC) {
    OS << "RawComment=";
    if (RC.isEmpty()) {
      OS << "none";
      return;
    }
    OS << "[";
    for (auto &SRC : RC.Comments)
      printWithEscaping(SRC.RawText);
    OS << "]";
  }

  void printBriefComment(StringRef Brief) {
    OS << "BriefComment=";
    if (Brief.empty()) {
      OS << "none";
      return;
    }
    OS << "[";
    printWithEscaping(Brief);
    OS << "]";
  }

  void printDocComment(const Decl *D) {
    std::string XML;
    {
      llvm::raw_string_ostream OS(XML);
      getDocumentationCommentAsXML(D, OS);
    }
    OS << "DocCommentAsXML=";
    if (XML.empty()) {
      OS << "none";
      return;
    }
    OS << "[";
    printWithEscaping(XML);
    OS << "]";

    auto Status = TheXMLValidator.validate(XML);
    switch (Status.Code) {
    case XMLValidator::ErrorCode::Valid:
      OS << " CommentXMLValid";
      break;

    case XMLValidator::ErrorCode::NotCompiledIn:
      OS << " ValidationSkipped=[libxml is missing]";
      break;

    case XMLValidator::ErrorCode::NoSchema:
      OS << " ValidationSkipped=[schema is not set]";
      break;

    case XMLValidator::ErrorCode::BadSchema:
      OS << " CommentXMLInvalid=[bad schema file]";
      break;

    case XMLValidator::ErrorCode::NotWellFormed:
      OS << " CommentXMLInvalid=[not well-formed XML: " << Status.Message
         << "]";
      break;

    case XMLValidator::ErrorCode::NotValid:
      OS << " CommentXMLInvalid=[not valid XML: " << Status.Message << "]";
      break;

    case XMLValidator::ErrorCode::InternalError:
      OS << " CommentXMLInvalid=[libxml error]";
      break;
    }
  }

  bool walkToDeclPre(Decl *D) override {
    if (D->isImplicit())
      return true;

    if (auto *VD = dyn_cast<ValueDecl>(D)) {
      SourceLoc Loc = D->getLoc();
      if (Loc.isValid()) {
        auto LineAndColumn = SM.getLineAndColumn(Loc);
        OS << getBufferIdentifier(VD->getLoc())
           << ":" << LineAndColumn.first << ":" << LineAndColumn.second << ": ";
      }
      OS << Decl::getKindName(VD->getKind()) << "/";
      printDeclName(VD);

      OS << " ";
      printRawComment(D->getRawComment());
      OS << " ";
      printBriefComment(D->getBriefComment());
      OS << " ";
      printDocComment(D);
      OS << "\n";
    }
    return true;
  }
};
} // unnamed namespace

static int doDumpComments(const CompilerInvocation &InitInvok,
                          StringRef SourceFilename) {
  CompilerInvocation Invocation(InitInvok);
  Invocation.addInputFilename(SourceFilename);
  Invocation.getLangOptions().AttachCommentsToDecls = true;
  CompilerInstance CI;
  // Display diagnostics to stderr.
  PrintingDiagnosticConsumer PrintDiags;
  CI.addDiagnosticConsumer(&PrintDiags);
  if (CI.setup(Invocation))
    return 1;
  CI.performSema();

  ASTDocCommentDumper Dumper;
  CI.getMainModule()->walk(Dumper);

  llvm::outs() << "\n";

  return 0;
}

static int doPrintComments(const CompilerInvocation &InitInvok,
                           StringRef SourceFilename,
                           StringRef CommentsXMLSchema) {
  CompilerInvocation Invocation(InitInvok);
  Invocation.addInputFilename(SourceFilename);
  Invocation.getLangOptions().AttachCommentsToDecls = true;
  Invocation.getLangOptions().EnableObjCAttrRequiresFoundation = false;

  CompilerInstance CI;
  // Display diagnostics to stderr.
  PrintingDiagnosticConsumer PrintDiags;
  CI.addDiagnosticConsumer(&PrintDiags);
  if (CI.setup(Invocation))
    return 1;
  CI.performSema();

  XMLValidator TheXMLValidator;
  TheXMLValidator.setSchema(CommentsXMLSchema);

  ASTCommentPrinter Printer(CI.getSourceMgr(), TheXMLValidator);

  CI.getMainModule()->walk(Printer);

  return 0;
}

static int doPrintModuleComments(const CompilerInvocation &InitInvok,
                                 const std::vector<std::string> ModulesToPrint,
                                 StringRef CommentsXMLSchema) {
  CompilerInvocation Invocation(InitInvok);

  CompilerInstance CI;
  // Display diagnostics to stderr.
  PrintingDiagnosticConsumer PrintDiags;
  CI.addDiagnosticConsumer(&PrintDiags);
  if (CI.setup(Invocation))
    return 1;

  auto &Context = CI.getASTContext();

  // Load standard library so that Clang importer can use it.
  auto *Stdlib = getModuleByFullName(Context, Context.StdlibModuleName);
  if (!Stdlib)
    return 1;

  XMLValidator TheXMLValidator;
  TheXMLValidator.setSchema(CommentsXMLSchema);

  ASTCommentPrinter Printer(CI.getSourceMgr(), TheXMLValidator);

  int ExitCode = 0;
  for (StringRef ModuleToPrint : ModulesToPrint) {
    auto *M = getModuleByFullName(Context, ModuleToPrint);
    if (!M) {
      ExitCode = -1;
      continue;
    }


    M->walk(Printer);
  }

  return ExitCode;
}

static int doPrintModuleImports(const CompilerInvocation &InitInvok,
                                const std::vector<std::string> ModulesToPrint) {
  CompilerInvocation Invocation(InitInvok);

  CompilerInstance CI;
  // Display diagnostics to stderr.
  PrintingDiagnosticConsumer PrintDiags;
  CI.addDiagnosticConsumer(&PrintDiags);
  if (CI.setup(Invocation))
    return 1;

  auto &Context = CI.getASTContext();

  // Load standard library so that Clang importer can use it.
  auto *Stdlib = getModuleByFullName(Context, Context.StdlibModuleName);
  if (!Stdlib)
    return 1;

  int ExitCode = 0;
  for (StringRef ModuleToPrint : ModulesToPrint) {
    auto *M = getModuleByFullName(Context, ModuleToPrint);
    if (!M) {
      ExitCode = -1;
      continue;
    }

    auto isClangModule = [](const Module *M) -> bool {
      if (!M->getFiles().empty())
        if (M->getFiles().front()->getKind() == FileUnitKind::ClangModule)
          return true;
      return false;
    };

    SmallVector<Module::ImportedModule, 16> scratch;
    M->forAllVisibleModules({}, [&](const Module::ImportedModule &next) {
      llvm::outs() << next.second->getName();
      if (isClangModule(next.second))
        llvm::outs() << " (Clang)";
      llvm::outs() << ":\n";

      scratch.clear();
      next.second->getImportedModules(scratch, Module::ImportFilter::Public);
      for (auto &import : scratch) {
        llvm::outs() << "\t" << import.second->getName();
        for (auto accessPathPiece : import.first) {
          llvm::outs() << "." << accessPathPiece.first;
        }

        if (isClangModule(import.second))
          llvm::outs() << " (Clang)";
        llvm::outs() << "\n";
      }
    });
  }

  return ExitCode;
}


//============================================================================//
// Print type interfaces.
//============================================================================//
static int doPrintTypeInterface(const CompilerInvocation &InitInvok,
                                const StringRef FileName,
                                const StringRef LCPair) {
  auto Pair = parseLineCol(LCPair);
  if (!Pair.hasValue())
    return 1;
  CompilerInvocation Invocation(InitInvok);
  Invocation.addInputFilename(FileName);
  CompilerInstance CI;
  if (CI.setup(Invocation))
    return 1;
  CI.performSema();
  SourceFile *SF = nullptr;
  unsigned BufID = CI.getInputBufferIDs().back();
  for (auto Unit : CI.getMainModule()->getFiles()) {
    SF = dyn_cast<SourceFile>(Unit);
    if (SF)
      break;
  }
  assert(SF && "no source file?");
  SemaLocResolver Resolver(*SF);
  SourceManager &SM = SF->getASTContext().SourceMgr;
  auto Offset = SM.resolveFromLineCol(BufID, Pair.getValue().first,
                                      Pair.getValue().second);
  if (!Offset.hasValue()) {
    llvm::errs() << "Cannot resolve source location.\n";
    return 1;
  }
  SourceLoc Loc = Lexer::getLocForStartOfToken(SM, BufID, Offset.getValue());
  auto SemaT = Resolver.resolve(Loc);
  if (SemaT.isInvalid()) {
    llvm::errs() << "Cannot find sema token at the given location.\n";
    return 1;
  }
  if (SemaT.Ty.isNull()) {
    llvm::errs() << "Cannot get type of the sema token.\n";
    return 1;
  }
  ASTPrinter::printTypeInterface(SemaT.Ty, SemaT.DC, llvm::outs());
  return 0;
}

//============================================================================//
// Print USRs
//============================================================================//

namespace {

class USRPrinter : public ide::SourceEntityWalker {
  SourceManager &SM;
  unsigned BufferID;
  llvm::raw_ostream &OS;

public:
  USRPrinter(SourceManager &SM, unsigned BufferID, llvm::raw_ostream &OS)
    : SM(SM), BufferID(BufferID), OS(OS) { }

private:
  bool walkToDeclPre(Decl *D, CharSourceRange Range) override {
    if (ValueDecl *VD = dyn_cast<ValueDecl>(D))
      printUSR(VD, Range.getStart());
    return true;
  }

  bool walkToExprPre(Expr *E) override {
    if (auto *DRE = dyn_cast<DeclRefExpr>(E))
      printUSR(DRE->getDecl(), E->getLoc());
    return true;
  }

  void printUSR(const ValueDecl *VD, SourceLoc Loc) {
    printLoc(Loc);
    OS << ' ';
    if (ide::printDeclUSR(VD, OS))
      OS << "ERROR:no-usr";
    OS << '\n';
  }

  void printLoc(SourceLoc Loc) {
    if (Loc.isValid()) {
      auto LineCol = SM.getLineAndColumn(Loc, BufferID);
      OS << LineCol.first << ':' << LineCol.second;
    }
  }

  bool shouldWalkIntoFunctionGenericParams() override {
    return false;
  }
};

} // unnamed namespace

static int doPrintUSRs(const CompilerInvocation &InitInvok,
                       StringRef SourceFilename) {
  CompilerInvocation Invocation(InitInvok);
  Invocation.addInputFilename(SourceFilename);

  ClangImporterOptions &ImporterOpts = Invocation.getClangImporterOptions();
  ImporterOpts.DetailedPreprocessingRecord = true;

  CompilerInstance CI;
  // Display diagnostics to stderr.
  PrintingDiagnosticConsumer PrintDiags;
  CI.addDiagnosticConsumer(&PrintDiags);
  if (CI.setup(Invocation))
    return 1;
  CI.performSema();

  unsigned BufID = CI.getInputBufferIDs().back();
  USRPrinter Printer(CI.getSourceMgr(), BufID, llvm::outs());
  Printer.walk(*CI.getMainModule());
  return 0;
}

static int doTestCreateCompilerInvocation(ArrayRef<const char *> Args) {
  PrintingDiagnosticConsumer PDC;
  SourceManager SM;
  DiagnosticEngine Diags(SM);
  Diags.addConsumer(PDC);

  std::unique_ptr<CompilerInvocation> CI =
    driver::createCompilerInvocation(Args, Diags);

  if (!CI) {
    llvm::errs() << "error: unable to create a CompilerInvocation\n";
    return 1;
  }

  return 0;
}

static int doTestCompilerInvocationFromModule(StringRef ModuleFilePath) {
  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> FileBufOrErr =
      llvm::MemoryBuffer::getFile(ModuleFilePath);
  if (!FileBufOrErr) {
    llvm::errs() << "error opening input file: "
      << FileBufOrErr.getError().message() << '\n';
    return -1;
  }

  CompilerInvocation CI;
  StringRef Data = FileBufOrErr.get()->getBuffer();
  static_assert(static_cast<int>(serialization::Status::Valid) == 0,
                "Status::Valid should be a successful exit");
  return static_cast<int>(CI.loadFromSerializedAST(Data));
}

// This function isn't referenced outside its translation unit, but it
// can't use the "static" keyword because its address is used for
// getMainExecutable (since some platforms don't support taking the
// address of main, and some platforms can't implement getMainExecutable
// without being given the address of a function in the main executable).
void anchorForGetMainExecutable() {}

int main(int argc, char *argv[]) {
  INITIALIZE_LLVM(argc, argv);

  if (argc > 1) {
    // Handle integrated test tools which do not use
    // llvm::cl::ParseCommandLineOptions.
    StringRef FirstArg(argv[1]);
    if (FirstArg == "-test-createCompilerInvocation") {
      ArrayRef<const char *> Args(argv + 2, argc - 2);
      return doTestCreateCompilerInvocation(Args);
    }
  }

  // '--cc-args' separates swift-ide-test options from clang arguments.
  ArrayRef<const char *> CCArgs;
  for (int i = 1; i < argc; ++i) {
    if (StringRef(argv[i]) == "--cc-args") {
      CCArgs = llvm::makeArrayRef(argv+i+1, argc-i-1);
      argc = i;
    }
  }

  llvm::cl::ParseCommandLineOptions(argc, argv, "Swift IDE Test\n");

  if (options::Action == ActionType::None) {
    llvm::errs() << "action required\n";
    llvm::cl::PrintHelpMessage();
    return 1;
  }

  if (options::Action == ActionType::TestCreateCompilerInvocation) {
    llvm::errs() << "-test-createCompilerInvocation must be specified before "
                    "all other arguments\n";
    return 1;
  }

  if (options::Action == ActionType::GenerateModuleAPIDescription) {
    return doGenerateModuleAPIDescription(
        llvm::sys::fs::getMainExecutable(
            argv[0], reinterpret_cast<void *>(&anchorForGetMainExecutable)),
        options::InputFilenames);
  }

  if (options::Action == ActionType::GenerateModuleAPIDescription) {
    llvm::errs() << "unimplemented\n";
    return 1;
  }

  if (options::Action == ActionType::DumpCompletionCache) {
    if (options::InputFilenames.empty()) {
      llvm::errs() << "-dump-completin-cache requires an input file\n";
      return 1;
    }

    ide::PrintingCodeCompletionConsumer Consumer(
        llvm::outs(), options::CodeCompletionKeywords);
    for (StringRef filename : options::InputFilenames) {
      auto resultsOpt = ide::OnDiskCodeCompletionCache::getFromFile(filename);
      if (!resultsOpt) {
        // FIXME: error?
        continue;
      }
      Consumer.handleResults(resultsOpt->get()->Sink.Results);
    }

    return 0;
  }

  if (options::SourceFilename.empty()) {
    llvm::errs() << "source file required\n";
    llvm::cl::PrintHelpMessage();
    return 1;
  }

  if (options::Action == ActionType::CompilerInvocationFromModule) {
    return doTestCompilerInvocationFromModule(options::SourceFilename);
  }

  // If no SDK was specified via -sdk, check environment variable SDKROOT.
  if (options::SDK.getNumOccurrences() == 0) {
    const char *SDKROOT = getenv("SDKROOT");
    if (SDKROOT)
      options::SDK = SDKROOT;
  }

  if (options::PrintStats)
    llvm::EnableStatistics();

  CompilerInvocation InitInvok;

  for (auto &File : options::InputFilenames)
    InitInvok.addInputFilename(File);
  if (!options::InputFilenames.empty())
    InitInvok.setInputKind(InputFileKind::IFK_Swift_Library);

  InitInvok.setMainExecutablePath(
      llvm::sys::fs::getMainExecutable(argv[0],
          reinterpret_cast<void *>(&anchorForGetMainExecutable)));

  InitInvok.setModuleName("swift_ide_test");

  InitInvok.setSDKPath(options::SDK);
  if (!options::Triple.empty())
    InitInvok.setTargetTriple(options::Triple);
  InitInvok.getClangImporterOptions().ModuleCachePath =
    options::ModuleCachePath;
  InitInvok.setImportSearchPaths(options::ImportPaths);
  InitInvok.setFrameworkSearchPaths(options::FrameworkPaths);
  InitInvok.getFrontendOptions().EnableSourceImport |=
    options::EnableSourceImport;
  InitInvok.getFrontendOptions().ImplicitObjCHeaderPath =
    options::ImportObjCHeader;
  InitInvok.getLangOptions().EnableAccessControl =
    !options::DisableAccessControl;
  InitInvok.getLangOptions().CodeCompleteInitsInPostfixExpr |=
      options::CodeCompleteInitsInPostfixExpr;
  InitInvok.getClangImporterOptions().InferImplicitProperties |=
    options::ImplicitProperties;
  InitInvok.getClangImporterOptions().ImportForwardDeclarations |=
    options::ObjCForwardDeclarations;
  InitInvok.getClangImporterOptions().OmitNeedlessWords |=
    options::OmitNeedlessWords;
  InitInvok.getClangImporterOptions().InferDefaultArguments |=
    options::InferDefaultArguments;
  InitInvok.getClangImporterOptions().UseSwiftLookupTables |=
    options::UseSwiftLookupTables;

  if (!options::ResourceDir.empty()) {
    InitInvok.setRuntimeResourcePath(options::ResourceDir);
  }
  for (auto &Arg : options::ClangXCC) {
    InitInvok.getClangImporterOptions().ExtraArgs.push_back(Arg);
  }
  InitInvok.getLangOptions().DebugForbidTypecheckPrefix =
    options::DebugForbidTypecheckPrefix;
  InitInvok.getLangOptions().EnableObjCAttrRequiresFoundation =
    !options::DisableObjCAttrRequiresFoundationModule;

  for (auto ConfigName : options::BuildConfigs)
    InitInvok.getLangOptions().addBuildConfigOption(ConfigName);

  // Process the clang arguments last and allow them to override previously
  // set options.
  if (!CCArgs.empty()) {
    std::string Error;
    if (initInvocationByClangArguments(CCArgs, InitInvok, Error)) {
      llvm::errs() << "error initializing invocation with clang args: "
        << Error << '\n';
      return 1;
    }
  }

  PrintOptions PrintOpts;
  if (options::PrintInterface) {
    PrintOpts = PrintOptions::printInterface();
  } else if (options::PrintInterfaceForDoc) {
    PrintOpts = PrintOptions::printDocInterface();
  } else {
    PrintOpts = PrintOptions::printEverything();
    PrintOpts.FullyQualifiedTypesIfAmbiguous =
      options::FullyQualifiedTypesIfAmbiguous;
    PrintOpts.SynthesizeSugarOnTypes = options::SynthesizeSugarOnTypes;
    PrintOpts.AbstractAccessors = options::AbstractAccessors;
    PrintOpts.FunctionDefinitions = options::FunctionDefinitions;
    PrintOpts.PreferTypeRepr = options::PreferTypeRepr;
    PrintOpts.ExplodePatternBindingDecls = options::ExplodePatternBindingDecls;
    PrintOpts.PrintImplicitAttrs = options::PrintImplicitAttrs;
    PrintOpts.PrintAccessibility = options::PrintAccessibility;
    PrintOpts.AccessibilityFilter = options::AccessibilityFilter;
    PrintOpts.PrintRegularClangComments = options::PrintRegularComments;
    PrintOpts.SkipPrivateStdlibDecls = options::SkipPrivateStdlibDecls;
    PrintOpts.SkipUnavailable = options::SkipUnavailable;
    PrintOpts.SkipDeinit = options::SkipDeinit;
    PrintOpts.SkipImports = options::SkipImports;
    if (options::SkipParameterNames) {
      PrintOpts.ArgAndParamPrinting
        = PrintOptions::ArgAndParamPrintingMode::ArgumentOnly;
    }
  }
  if (options::SkipUnderscoredStdlibProtocols)
    PrintOpts.SkipUnderscoredStdlibProtocols = true;
  if (options::PrintOriginalSourceText)
    PrintOpts.PrintOriginalSourceText = true;

  if (PrintOpts.PrintDocumentationComments) {
    InitInvok.getLangOptions().AttachCommentsToDecls = true;
  }

  if (InitInvok.getClangImporterOptions().InferDefaultArguments)
    PrintOpts.PrintDefaultParameterPlaceholder = true;

  int ExitCode;

  switch (options::Action) {
  case ActionType::None:
  case ActionType::TestCreateCompilerInvocation:
  case ActionType::CompilerInvocationFromModule:
  case ActionType::GenerateModuleAPIDescription:
  case ActionType::DiffModuleAPI:
  case ActionType::DumpCompletionCache:
    llvm_unreachable("should be handled above");

  case ActionType::CodeCompletion:
    if (options::CodeCompletionToken.empty()) {
      llvm::errs() << "code completion token name required\n";
      return 1;
    }
    ExitCode = doCodeCompletion(InitInvok,
                                options::SourceFilename,
                                options::CodeCompletionToken,
                                options::CodeCompletionDiagnostics,
                                options::CodeCompletionKeywords);
    break;

  case ActionType::REPLCodeCompletion:
    ExitCode = doREPLCodeCompletion(InitInvok, options::SourceFilename);
    break;

  case ActionType::SyntaxColoring:
    ExitCode = doSyntaxColoring(InitInvok,
                                options::SourceFilename,
                                options::TerminalOutput,
                                options::Typecheck);
    break;

  case ActionType::DumpAPI:
    ExitCode = doDumpAPI(InitInvok, options::SourceFilename);
    break;

  case ActionType::DumpImporterLookupTable:
    ExitCode = doDumpImporterLookupTables(InitInvok, options::SourceFilename);
    break;

  case ActionType::Structure:
    ExitCode = doStructureAnnotation(InitInvok, options::SourceFilename);
    break;

  case ActionType::Annotation:
    ExitCode = doSemanticAnnotation(InitInvok,
                                    options::SourceFilename,
                                    options::TerminalOutput);
    break;

  case ActionType::TestInputCompleteness:
    ExitCode = doInputCompletenessTest(options::SourceFilename);
    break;

  case ActionType::PrintASTNotTypeChecked:
  case ActionType::PrintASTTypeChecked: {
    bool RunTypeChecker = (options::Action == ActionType::PrintASTTypeChecked);
    ExitCode = doPrintAST(InitInvok,
                          options::SourceFilename,
                          RunTypeChecker,
                          PrintOpts,
                          options::MangledNameToFind,
                          options::DebugClientDiscriminator);
    break;
  }
  case ActionType::PrintLocalTypes:
    ExitCode = doPrintLocalTypes(InitInvok, options::ModuleToPrint);
    break;

  case ActionType::PrintModule: {
    ide::ModuleTraversalOptions TraversalOptions;
    if (options::ModulePrintSubmodules)
      TraversalOptions |= ide::ModuleTraversal::VisitSubmodules;
    if (options::ModulePrintHidden)
      TraversalOptions |= ide::ModuleTraversal::VisitHidden;
    if (options::ModulePrintSkipOverlay)
      TraversalOptions |= ide::ModuleTraversal::SkipOverlay;

    ExitCode = doPrintModules(
        InitInvok, options::ModuleToPrint, TraversalOptions, PrintOpts,
        options::AnnotatePrint);
    break;
  }

  case ActionType::PrintHeader: {
    ExitCode = doPrintHeaders(
        InitInvok, options::HeaderToPrint, PrintOpts,
        options::AnnotatePrint);
    break;
  }

  case ActionType::PrintSwiftFileInterface: {
    ExitCode = doPrintSwiftFileInterface(
        InitInvok, options::SourceFilename,
        options::AnnotatePrint);
    break;
  }

  case ActionType::PrintTypes:
    ExitCode = doPrintTypes(InitInvok, options::SourceFilename,
                            options::FullyQualifiedTypes);
    break;

  case ActionType::PrintComments:
    ExitCode = doPrintComments(InitInvok, options::SourceFilename,
                               options::CommentsXMLSchema);
    break;

  case ActionType::DumpComments:
    ExitCode = doDumpComments(InitInvok, options::SourceFilename);
    break;

  case ActionType::PrintModuleComments:
    ExitCode = doPrintModuleComments(InitInvok, options::ModuleToPrint,
                                     options::CommentsXMLSchema);
    break;

  case ActionType::PrintModuleImports:
    ExitCode = doPrintModuleImports(InitInvok, options::ModuleToPrint);
    break;

  case ActionType::PrintUSRs:
    ExitCode = doPrintUSRs(InitInvok, options::SourceFilename);
    break;
  case ActionType::PrintTypeInterface:
    ExitCode = doPrintTypeInterface(InitInvok,
                                    options::SourceFilename,
                                    options::LineColumnPair);
    break;
  }

  if (options::PrintStats)
    llvm::PrintStatistics();

  return ExitCode;
}

