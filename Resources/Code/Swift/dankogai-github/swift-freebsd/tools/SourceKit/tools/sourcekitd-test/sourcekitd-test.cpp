//===--- sourcekitd-test.cpp ----------------------------------------------===//
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

#include "sourcekitd/sourcekitd.h"

#include "TestOptions.h"
#include "clang/Rewrite/Core/RewriteBuffer.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/FileSystem.h"
#include <fstream>
#include <unistd.h>
#include <sys/param.h>

// FIXME: Platform compatibility.
#include <dispatch/dispatch.h>

using namespace llvm;

using namespace sourcekitd_test;

static int handleTestInvocation(ArrayRef<const char *> Args, TestOptions &InitOpts);
static void printCursorInfo(sourcekitd_variant_t Info, StringRef Filename,
                            llvm::raw_ostream &OS);
static void printDocInfo(sourcekitd_variant_t Info, StringRef Filename);
static void printInterfaceGen(sourcekitd_variant_t Info, bool CheckASCII);
static void printSemanticInfo();
static void printRelatedIdents(sourcekitd_variant_t Info, StringRef Filename,
                               llvm::raw_ostream &OS);
static void printFoundInterface(sourcekitd_variant_t Info,
                                llvm::raw_ostream &OS);
static void printFoundUSR(sourcekitd_variant_t Info,
                          llvm::MemoryBuffer *SourceBuf,
                          llvm::raw_ostream &OS);
static void printNormalizedDocComment(sourcekitd_variant_t Info);
static void expandPlaceholders(llvm::MemoryBuffer *SourceBuf,
                               llvm::raw_ostream &OS);

static unsigned resolveFromLineCol(unsigned Line, unsigned Col,
                                   StringRef Filename);
static unsigned resolveFromLineCol(unsigned Line, unsigned Col,
                                   llvm::MemoryBuffer *InputBuf);
static std::pair<unsigned, unsigned> resolveToLineCol(unsigned Offset,
                                                      StringRef Filename);
static std::pair<unsigned, unsigned> resolveToLineCol(unsigned Offset,
                                                  llvm::MemoryBuffer *InputBuf);
static std::pair<unsigned, unsigned> resolveToLineColFromBuf(unsigned Offset,
                                                      const char *Buf);
static llvm::MemoryBuffer *getBufferForFilename(StringRef Filename);

static void notification_receiver(sourcekitd_response_t resp);

static SourceKitRequest ActiveRequest = SourceKitRequest::None;

static sourcekitd_uid_t KeyRequest;
static sourcekitd_uid_t KeyCompilerArgs;
static sourcekitd_uid_t KeyOffset;
static sourcekitd_uid_t KeySourceFile;
static sourcekitd_uid_t KeyModuleName;
static sourcekitd_uid_t KeyName;
static sourcekitd_uid_t KeyFilePath;
static sourcekitd_uid_t KeyModuleInterfaceName;
static sourcekitd_uid_t KeyLength;
static sourcekitd_uid_t KeySourceText;
static sourcekitd_uid_t KeyUSR;
static sourcekitd_uid_t KeyTypename;
static sourcekitd_uid_t KeyOverrides;
static sourcekitd_uid_t KeyRelatedDecls;
static sourcekitd_uid_t KeyAnnotatedDecl;
static sourcekitd_uid_t KeyDocFullAsXML;
static sourcekitd_uid_t KeyResults;
static sourcekitd_uid_t KeySyntaxMap;
static sourcekitd_uid_t KeyEnableSyntaxMap;
static sourcekitd_uid_t KeyEnableSubStructure;
static sourcekitd_uid_t KeySyntacticOnly;
static sourcekitd_uid_t KeyLine;
static sourcekitd_uid_t KeyFormatOptions;
static sourcekitd_uid_t KeyCodeCompleteOptions;
static sourcekitd_uid_t KeyAnnotations;
static sourcekitd_uid_t KeyDiagnostics;
static sourcekitd_uid_t KeyDiagnosticStage;
static sourcekitd_uid_t KeySubStructure;
static sourcekitd_uid_t KeyIsSystem;
static sourcekitd_uid_t KeyNotification;
static sourcekitd_uid_t KeyPopular;
static sourcekitd_uid_t KeyUnpopular;
static sourcekitd_uid_t KeyTypeInterface;

static sourcekitd_uid_t RequestIndex;
static sourcekitd_uid_t RequestCodeComplete;
static sourcekitd_uid_t RequestCodeCompleteOpen;
static sourcekitd_uid_t RequestCodeCompleteClose;
static sourcekitd_uid_t RequestCodeCompleteUpdate;
static sourcekitd_uid_t RequestCodeCompleteCacheOnDisk;
static sourcekitd_uid_t RequestCodeCompleteSetPopularAPI;
static sourcekitd_uid_t RequestCursorInfo;
static sourcekitd_uid_t RequestRelatedIdents;
static sourcekitd_uid_t RequestEditorOpen;
static sourcekitd_uid_t RequestEditorOpenInterface;
static sourcekitd_uid_t RequestEditorOpenSwiftSourceInterface;
static sourcekitd_uid_t RequestEditorOpenHeaderInterface;
static sourcekitd_uid_t RequestEditorExtractTextFromComment;
static sourcekitd_uid_t RequestEditorReplaceText;
static sourcekitd_uid_t RequestEditorFormatText;
static sourcekitd_uid_t RequestEditorExpandPlaceholder;
static sourcekitd_uid_t RequestEditorFindUSR;
static sourcekitd_uid_t RequestEditorFindInterfaceDoc;
static sourcekitd_uid_t RequestDocInfo;

static sourcekitd_uid_t SemaDiagnosticStage;

static sourcekitd_uid_t NoteDocUpdate;

static dispatch_semaphore_t semaSemaphore;
static sourcekitd_response_t semaResponse;
static const char *semaName;

static int skt_main(int argc, const char **argv);

int main(int argc, const char **argv) {
  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
    int ret = skt_main(argc, argv);
    exit(ret);
  });

  dispatch_main();
}

static int skt_main(int argc, const char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal();

  sourcekitd_initialize();

  sourcekitd_set_notification_handler(^(sourcekitd_response_t resp) {
    notification_receiver(resp);
  });

  KeyRequest = sourcekitd_uid_get_from_cstr("key.request");
  KeyCompilerArgs = sourcekitd_uid_get_from_cstr("key.compilerargs");
  KeyOffset = sourcekitd_uid_get_from_cstr("key.offset");
  KeySourceFile = sourcekitd_uid_get_from_cstr("key.sourcefile");
  KeyModuleName = sourcekitd_uid_get_from_cstr("key.modulename");
  KeyName = sourcekitd_uid_get_from_cstr("key.name");
  KeyFilePath = sourcekitd_uid_get_from_cstr("key.filepath");
  KeyModuleInterfaceName = sourcekitd_uid_get_from_cstr("key.module_interface_name");
  KeyLength = sourcekitd_uid_get_from_cstr("key.length");
  KeySourceText = sourcekitd_uid_get_from_cstr("key.sourcetext");
  KeyUSR = sourcekitd_uid_get_from_cstr("key.usr");
  KeyTypename = sourcekitd_uid_get_from_cstr("key.typename");
  KeyOverrides = sourcekitd_uid_get_from_cstr("key.overrides");
  KeyRelatedDecls = sourcekitd_uid_get_from_cstr("key.related_decls");
  KeyAnnotatedDecl = sourcekitd_uid_get_from_cstr("key.annotated_decl");
  KeyDocFullAsXML = sourcekitd_uid_get_from_cstr("key.doc.full_as_xml");
  KeyResults = sourcekitd_uid_get_from_cstr("key.results");
  KeySyntaxMap = sourcekitd_uid_get_from_cstr("key.syntaxmap");
  KeyEnableSyntaxMap = sourcekitd_uid_get_from_cstr("key.enablesyntaxmap");
  KeyEnableSubStructure = sourcekitd_uid_get_from_cstr("key.enablesubstructure");
  KeySyntacticOnly = sourcekitd_uid_get_from_cstr("key.syntactic_only");
  KeyLine = sourcekitd_uid_get_from_cstr("key.line");
  KeyFormatOptions = sourcekitd_uid_get_from_cstr("key.editor.format.options");
  KeyCodeCompleteOptions =
      sourcekitd_uid_get_from_cstr("key.codecomplete.options");
  KeyAnnotations = sourcekitd_uid_get_from_cstr("key.annotations");
  KeyDiagnostics = sourcekitd_uid_get_from_cstr("key.diagnostics");
  KeyDiagnosticStage = sourcekitd_uid_get_from_cstr("key.diagnostic_stage");
  KeySubStructure = sourcekitd_uid_get_from_cstr("key.substructure");
  KeyIsSystem = sourcekitd_uid_get_from_cstr("key.is_system");
  KeyNotification = sourcekitd_uid_get_from_cstr("key.notification");
  KeyPopular = sourcekitd_uid_get_from_cstr("key.popular");
  KeyUnpopular = sourcekitd_uid_get_from_cstr("key.unpopular");
  KeyTypeInterface = sourcekitd_uid_get_from_cstr("key.typeinterface");

  SemaDiagnosticStage = sourcekitd_uid_get_from_cstr("source.diagnostic.stage.swift.sema");

  NoteDocUpdate = sourcekitd_uid_get_from_cstr("source.notification.editor.documentupdate");

  semaSemaphore = dispatch_semaphore_create(0);

  RequestIndex = sourcekitd_uid_get_from_cstr("source.request.indexsource");
  RequestCodeComplete = sourcekitd_uid_get_from_cstr("source.request.codecomplete");
  RequestCodeCompleteOpen = sourcekitd_uid_get_from_cstr("source.request.codecomplete.open");
  RequestCodeCompleteClose = sourcekitd_uid_get_from_cstr("source.request.codecomplete.close");
  RequestCodeCompleteUpdate = sourcekitd_uid_get_from_cstr("source.request.codecomplete.update");
  RequestCodeCompleteCacheOnDisk = sourcekitd_uid_get_from_cstr("source.request.codecomplete.cache.ondisk");
  RequestCodeCompleteSetPopularAPI = sourcekitd_uid_get_from_cstr("source.request.codecomplete.setpopularapi");
  RequestCursorInfo = sourcekitd_uid_get_from_cstr("source.request.cursorinfo");
  RequestRelatedIdents = sourcekitd_uid_get_from_cstr("source.request.relatedidents");
  RequestEditorOpen = sourcekitd_uid_get_from_cstr("source.request.editor.open");
  RequestEditorOpenInterface = sourcekitd_uid_get_from_cstr("source.request.editor.open.interface");
  RequestEditorOpenSwiftSourceInterface = sourcekitd_uid_get_from_cstr("source.request.editor.open.interface.swiftsource");
  RequestEditorOpenHeaderInterface = sourcekitd_uid_get_from_cstr("source.request.editor.open.interface.header");
  RequestEditorExtractTextFromComment = sourcekitd_uid_get_from_cstr("source.request.editor.extract.comment");
  RequestEditorReplaceText = sourcekitd_uid_get_from_cstr("source.request.editor.replacetext");
  RequestEditorFormatText = sourcekitd_uid_get_from_cstr("source.request.editor.formattext");
  RequestEditorExpandPlaceholder = sourcekitd_uid_get_from_cstr("source.request.editor.expand_placeholder");
  RequestEditorFindUSR = sourcekitd_uid_get_from_cstr("source.request.editor.find_usr");
  RequestEditorFindInterfaceDoc = sourcekitd_uid_get_from_cstr("source.request.editor.find_interface_doc");
  RequestDocInfo = sourcekitd_uid_get_from_cstr("source.request.docinfo");

  // A test invocation may initialize the options to be used for subsequent
  // invocations.
  TestOptions InitOpts;
  auto Args = llvm::makeArrayRef(argv+1, argc-1);
  while (1) {
    unsigned i = 0;
    for (auto Arg: Args) {
      if (StringRef(Arg) == "==")
        break;
      ++i;
    }
    if (i == Args.size())
      break;
    int ret = handleTestInvocation(Args.slice(0, i), InitOpts);
    if (ret) {
      sourcekitd_shutdown();
      return ret;
    }
    Args = Args.slice(i+1);
  }

  int ret = handleTestInvocation(Args, InitOpts);
  sourcekitd_shutdown();
  return ret;
}

static inline const char *getInterfaceGenDocumentName() {
  // Absolute "path" so that handleTestInvocation doesn't try to make it
  // absolute.
  return "/<interface-gen>";
}

static int printAnnotations();
static int printDiags();

static void getSemanticInfo(sourcekitd_variant_t Info, StringRef Filename);

static void addCodeCompleteOptions(sourcekitd_object_t Req, TestOptions &Opts) {
  if (!Opts.RequestOptions.empty()) {
    sourcekitd_object_t CCOpts =
        sourcekitd_request_dictionary_create(nullptr, nullptr, 0);
    for (auto &Opt : Opts.RequestOptions) {
      auto KeyValue = StringRef(Opt).split('=');
      std::string KeyStr("key.codecomplete.");
      KeyStr.append(KeyValue.first);
      sourcekitd_uid_t Key = sourcekitd_uid_get_from_cstr(KeyStr.c_str());

      // FIXME: more robust way to determine the option type.
      if (KeyValue.first == "filtertext") {
        sourcekitd_request_dictionary_set_stringbuf(
            CCOpts, Key, KeyValue.second.data(), KeyValue.second.size());
      } else {
        int64_t Value = 0;
        KeyValue.second.getAsInteger(0, Value);
        sourcekitd_request_dictionary_set_int64(CCOpts, Key, Value);
      }
    }
    sourcekitd_request_dictionary_set_value(Req, KeyCodeCompleteOptions,
                                            CCOpts);
    sourcekitd_request_release(CCOpts);
  }
}

static bool readPopularAPIList(StringRef filename,
                               std::vector<std::string> &result) {
  std::ifstream in(filename);
  if (!in.is_open()) {
    llvm::errs() << "error opening '" << filename << "'\n";
    return true;
  }

  std::string line;
  while (std::getline(in, line)) {
    result.emplace_back();
    std::swap(result.back(), line);
  }
  return false;
}

static int handleJsonRequestPath(StringRef QueryPath) {
  auto Buffer = getBufferForFilename(QueryPath)->getBuffer();
  char *Err = nullptr;
  auto Req = sourcekitd_request_create_from_yaml(Buffer.data(), &Err);
  if (!Req) {
    assert(Err);
    llvm::errs() << Err;
    free(Err);
    return 1;
  }
  sourcekitd_request_description_dump(Req);
  sourcekitd_response_t Resp = sourcekitd_send_request_sync(Req);
  auto Error = sourcekitd_response_is_error(Resp);
  sourcekitd_response_description_dump_filedesc(Resp, STDOUT_FILENO);
  return Error ? 1 : 0;
}

static int handleTestInvocation(ArrayRef<const char *> Args,
                                TestOptions &InitOpts) {

  unsigned Optargc = 0;
  for (auto Arg: Args) {
    if (StringRef(Arg) == "--")
      break;
    ++Optargc;
  }

  TestOptions Opts = InitOpts;
  if (Opts.parseArgs(Args.slice(0, Optargc)))
    return 1;

  if (!Opts.JsonRequestPath.empty())
    return handleJsonRequestPath(Opts.JsonRequestPath);

  if (Optargc < Args.size())
    Opts.CompilerArgs = Args.slice(Optargc+1);

  std::string SourceFile = Opts.SourceFile;
  if (!SourceFile.empty()) {
    llvm::SmallString<64> AbsSourceFile;
    AbsSourceFile += SourceFile;
    llvm::sys::fs::make_absolute(AbsSourceFile);
    SourceFile = AbsSourceFile.str();
  }

  if (!Opts.TextInputFile.empty()) {
    auto Buf = getBufferForFilename(Opts.TextInputFile);
    Opts.SourceText = Buf->getBuffer();
  }

  std::unique_ptr<llvm::MemoryBuffer> SourceBuf;
  if (Opts.SourceText.hasValue()) {
    SourceBuf = llvm::MemoryBuffer::getMemBuffer(*Opts.SourceText, Opts.SourceFile);
  } else if (!SourceFile.empty()) {
    SourceBuf = llvm::MemoryBuffer::getMemBuffer(
          getBufferForFilename(SourceFile)->getBuffer(), SourceFile);
  }

  unsigned ByteOffset = Opts.Offset;
  if (Opts.Line != 0) {
    ByteOffset = resolveFromLineCol(Opts.Line, Opts.Col, SourceBuf.get());
  }

  sourcekitd_object_t Req = sourcekitd_request_dictionary_create(nullptr,
                                                                 nullptr, 0);
  ActiveRequest = Opts.Request;
  switch (Opts.Request) {
  case SourceKitRequest::None:
    llvm::errs() << "request is not set\n";
    llvm::cl::PrintHelpMessage();
    return 1;

  case SourceKitRequest::Index:
    sourcekitd_request_dictionary_set_uid(Req, KeyRequest, RequestIndex);
    break;

  case SourceKitRequest::CodeComplete:
    sourcekitd_request_dictionary_set_uid(Req, KeyRequest, RequestCodeComplete);
    sourcekitd_request_dictionary_set_int64(Req, KeyOffset, ByteOffset);
    break;

  case SourceKitRequest::CodeCompleteOpen:
    sourcekitd_request_dictionary_set_uid(Req, KeyRequest,
                                          RequestCodeCompleteOpen);
    sourcekitd_request_dictionary_set_int64(Req, KeyOffset, ByteOffset);
    sourcekitd_request_dictionary_set_string(Req, KeyName, SourceFile.c_str());
    addCodeCompleteOptions(Req, Opts);
    break;

  case SourceKitRequest::CodeCompleteClose:
    sourcekitd_request_dictionary_set_uid(Req, KeyRequest,
                                          RequestCodeCompleteClose);
    sourcekitd_request_dictionary_set_int64(Req, KeyOffset, ByteOffset);
    sourcekitd_request_dictionary_set_string(Req, KeyName, SourceFile.c_str());
    break;

  case SourceKitRequest::CodeCompleteUpdate:
    sourcekitd_request_dictionary_set_uid(Req, KeyRequest,
                                          RequestCodeCompleteUpdate);
    sourcekitd_request_dictionary_set_int64(Req, KeyOffset, ByteOffset);
    sourcekitd_request_dictionary_set_string(Req, KeyName, SourceFile.c_str());
    addCodeCompleteOptions(Req, Opts);
    break;

  case SourceKitRequest::CodeCompleteCacheOnDisk:
    sourcekitd_request_dictionary_set_uid(Req, KeyRequest,
                                          RequestCodeCompleteCacheOnDisk);
    sourcekitd_request_dictionary_set_string(Req, KeyName,
                                             Opts.CachePath.c_str());
    break;

  case SourceKitRequest::CodeCompleteSetPopularAPI: {
    sourcekitd_request_dictionary_set_uid(Req, KeyRequest,
                                          RequestCodeCompleteSetPopularAPI);

    auto addPopularList = [&Req](StringRef filename, sourcekitd_uid_t key) {
      std::vector<std::string> names;
      if (readPopularAPIList(filename, names))
        return true;

      sourcekitd_object_t popular = sourcekitd_request_array_create(nullptr, 0);
      for (auto name : names)
        sourcekitd_request_array_set_string(popular, SOURCEKITD_ARRAY_APPEND,
                                            name.c_str());
      sourcekitd_request_dictionary_set_value(Req, key, popular);
      return false;
    };

    for (auto &Opt : Opts.RequestOptions) {
      auto KeyValue = StringRef(Opt).split('=');
      auto key = llvm::StringSwitch<sourcekitd_uid_t>(KeyValue.first)
        .Case("popular", KeyPopular)
        .Case("unpopular", KeyUnpopular)
        .Default(nullptr);
      if (!key) {
        llvm::errs() << "invalid key '" << KeyValue.first << "' in -req-opts\n";
        return 1;
      }

      if (addPopularList(KeyValue.second, key))
        return 1;
    }

    break;
  }

  case SourceKitRequest::CursorInfo:
    sourcekitd_request_dictionary_set_uid(Req, KeyRequest, RequestCursorInfo);
    sourcekitd_request_dictionary_set_int64(Req, KeyOffset, ByteOffset);
    break;

  case SourceKitRequest::RelatedIdents:
    sourcekitd_request_dictionary_set_uid(Req, KeyRequest, RequestRelatedIdents);
    sourcekitd_request_dictionary_set_int64(Req, KeyOffset, ByteOffset);
    break;

  case SourceKitRequest::SyntaxMap:
    sourcekitd_request_dictionary_set_uid(Req, KeyRequest, RequestEditorOpen);
    sourcekitd_request_dictionary_set_string(Req, KeyName, SourceFile.c_str());
    sourcekitd_request_dictionary_set_int64(Req, KeyEnableSyntaxMap, true);
    sourcekitd_request_dictionary_set_int64(Req, KeyEnableSubStructure, false);
    sourcekitd_request_dictionary_set_int64(Req, KeySyntacticOnly, !Opts.UsedSema);
    break;

  case SourceKitRequest::Structure:
    sourcekitd_request_dictionary_set_uid(Req, KeyRequest, RequestEditorOpen);
    sourcekitd_request_dictionary_set_string(Req, KeyName, SourceFile.c_str());
    sourcekitd_request_dictionary_set_int64(Req, KeyEnableSyntaxMap, false);
    sourcekitd_request_dictionary_set_int64(Req, KeyEnableSubStructure, true);
    sourcekitd_request_dictionary_set_int64(Req, KeySyntacticOnly, !Opts.UsedSema);
    break;

  case SourceKitRequest::Format:
    sourcekitd_request_dictionary_set_uid(Req, KeyRequest, RequestEditorOpen);
    sourcekitd_request_dictionary_set_string(Req, KeyName, SourceFile.c_str());
    sourcekitd_request_dictionary_set_int64(Req, KeyEnableSyntaxMap, false);
    sourcekitd_request_dictionary_set_int64(Req, KeyEnableSubStructure, false);
    sourcekitd_request_dictionary_set_int64(Req, KeySyntacticOnly, !Opts.UsedSema);
    break;
      
  case SourceKitRequest::ExpandPlaceholder:
    sourcekitd_request_dictionary_set_uid(Req, KeyRequest, RequestEditorOpen);
    sourcekitd_request_dictionary_set_string(Req, KeyName, SourceFile.c_str());
    sourcekitd_request_dictionary_set_int64(Req, KeyEnableSyntaxMap, false);
    sourcekitd_request_dictionary_set_int64(Req, KeyEnableSubStructure, false);
    sourcekitd_request_dictionary_set_int64(Req, KeySyntacticOnly, !Opts.UsedSema);
    break;

  case SourceKitRequest::DocInfo:
    sourcekitd_request_dictionary_set_uid(Req, KeyRequest, RequestDocInfo);
    break;

  case SourceKitRequest::SemanticInfo:
    InitOpts.UsedSema = true;
    sourcekitd_request_dictionary_set_uid(Req, KeyRequest, RequestEditorOpen);
    sourcekitd_request_dictionary_set_string(Req, KeyName, SourceFile.c_str());
    break;

  case SourceKitRequest::Open:
    sourcekitd_request_dictionary_set_uid(Req, KeyRequest, RequestEditorOpen);
    sourcekitd_request_dictionary_set_string(Req, KeyName, SourceFile.c_str());
    break;

  case SourceKitRequest::Edit:
    sourcekitd_request_dictionary_set_uid(Req, KeyRequest,
                                          RequestEditorReplaceText);
    sourcekitd_request_dictionary_set_string(Req, KeyName, SourceFile.c_str());
    sourcekitd_request_dictionary_set_int64(Req, KeyOffset, ByteOffset);
    sourcekitd_request_dictionary_set_int64(Req, KeyLength, Opts.Length);
    sourcekitd_request_dictionary_set_string(Req, KeySourceText,
                                       Opts.ReplaceText.getValue().c_str());
    break;

  case SourceKitRequest::PrintAnnotations:
    return printAnnotations();
  case SourceKitRequest::PrintDiags:
    return printDiags();
  case SourceKitRequest::ExtractComment:
    if (Opts.SourceFile.empty()) {
      llvm::errs() << "Missing '<source-file>' \n";
      return 1;
    }
    sourcekitd_request_dictionary_set_uid(Req, KeyRequest,
                                          RequestEditorExtractTextFromComment);
    break;

  case SourceKitRequest::InterfaceGen:
  case SourceKitRequest::InterfaceGenOpen:
    if (Opts.ModuleName.empty() && Opts.HeaderPath.empty() && Opts.SourceFile.empty()) {
      llvm::errs() << "Missing '-module <module name>' or '-header <path>' or '<source-file>' \n";
      return 1;
    }
    if (!Opts.ModuleName.empty()) {
      sourcekitd_request_dictionary_set_uid(Req, KeyRequest,
                                            RequestEditorOpenInterface);
    } else if (Opts.SourceFile.empty()){
      sourcekitd_request_dictionary_set_uid(Req, KeyRequest,
                                            RequestEditorOpenHeaderInterface);
    } else {
      sourcekitd_request_dictionary_set_uid(Req, KeyRequest,
                                            RequestEditorOpenSwiftSourceInterface);
    }
    sourcekitd_request_dictionary_set_string(Req, KeyName, getInterfaceGenDocumentName());
    break;

  case SourceKitRequest::FindInterfaceDoc:
    if (Opts.ModuleName.empty()) {
      llvm::errs() << "Missing '-module <module name>'\n";
      return 1;
    }
    sourcekitd_request_dictionary_set_uid(Req, KeyRequest, RequestEditorFindInterfaceDoc);
    break;

  case SourceKitRequest::FindUSR:
    if (Opts.USR.empty()) {
      llvm::errs() << "Missing '-usr <USR string>'\n";
      return 1;
    }
    sourcekitd_request_dictionary_set_uid(Req, KeyRequest, RequestEditorFindUSR);
    sourcekitd_request_dictionary_set_string(Req, KeyUSR, Opts.USR.c_str());
    break;
  }

  if (!SourceFile.empty()) {
    if (Opts.PassAsSourceText) {
      auto Buf = getBufferForFilename(SourceFile);
      sourcekitd_request_dictionary_set_string(Req, KeySourceText,
                                               Buf->getBufferStart());
    }
    sourcekitd_request_dictionary_set_string(Req, KeySourceFile,
                                             SourceFile.c_str());
  }

  if (Opts.SourceText) {
    sourcekitd_request_dictionary_set_string(Req, KeySourceText,
                                             Opts.SourceText->c_str());
  }

  if (!Opts.CompilerArgs.empty()) {
    sourcekitd_object_t Args = sourcekitd_request_array_create(nullptr, 0);
    for (auto Arg : Opts.CompilerArgs)
      sourcekitd_request_array_set_string(Args, SOURCEKITD_ARRAY_APPEND, Arg);
    sourcekitd_request_dictionary_set_value(Req, KeyCompilerArgs, Args);
    sourcekitd_request_release(Args);
  }

  if (!Opts.ModuleName.empty()) {
    sourcekitd_request_dictionary_set_string(Req, KeyModuleName,
                                             Opts.ModuleName.c_str());
  }
  if (!Opts.HeaderPath.empty()) {
    sourcekitd_request_dictionary_set_string(Req, KeyFilePath,
                                             Opts.HeaderPath.c_str());
  }

  sourcekitd_request_description_dump(Req);
  sourcekitd_response_t Resp = sourcekitd_send_request_sync(Req);
  bool KeepResponseAlive = false;
  bool IsError = sourcekitd_response_is_error(Resp);
  if (IsError) {
    sourcekitd_response_description_dump(Resp);
  } else {
    sourcekitd_variant_t Info = sourcekitd_response_get_value(Resp);
    switch (Opts.Request) {
    case SourceKitRequest::None:
      llvm_unreachable("request should be set");
    case SourceKitRequest::PrintAnnotations:
    case SourceKitRequest::PrintDiags:
      llvm_unreachable("print-annotations/print-diags is handled elsewhere");

    case SourceKitRequest::Open:
    case SourceKitRequest::Edit:
      getSemanticInfo(Info, SourceFile);
      KeepResponseAlive = true;
      break;

    case SourceKitRequest::Index:
    case SourceKitRequest::CodeComplete:
    case SourceKitRequest::CodeCompleteOpen:
    case SourceKitRequest::CodeCompleteClose:
    case SourceKitRequest::CodeCompleteUpdate:
    case SourceKitRequest::CodeCompleteCacheOnDisk:
    case SourceKitRequest::CodeCompleteSetPopularAPI:
      sourcekitd_response_description_dump_filedesc(Resp, STDOUT_FILENO);
      break;

    case SourceKitRequest::RelatedIdents:
      printRelatedIdents(Info, SourceFile, llvm::outs());
      break;

    case SourceKitRequest::CursorInfo:
      printCursorInfo(Info, SourceFile, llvm::outs());
      break;

    case SourceKitRequest::DocInfo:
      printDocInfo(Info, SourceFile);
      break;

    case SourceKitRequest::SemanticInfo:
      getSemanticInfo(Info, SourceFile);
      printSemanticInfo();
      break;

    case SourceKitRequest::InterfaceGen:
      printInterfaceGen(Info, Opts.CheckInterfaceIsASCII);
      break;

    case SourceKitRequest::ExtractComment:
      printNormalizedDocComment(Info);
      break;

    case SourceKitRequest::InterfaceGenOpen:
      // Just initialize the options for the subsequent request.
      InitOpts.SourceFile = getInterfaceGenDocumentName();
      InitOpts.SourceText =
          sourcekitd_variant_dictionary_get_string(Info, KeySourceText);
      break;

    case SourceKitRequest::FindInterfaceDoc:
      printFoundInterface(Info, llvm::outs());
      break;

    case SourceKitRequest::FindUSR:
      printFoundUSR(Info, SourceBuf.get(), llvm::outs());
      break;

    case SourceKitRequest::SyntaxMap:
    case SourceKitRequest::Structure:
      sourcekitd_response_description_dump_filedesc(Resp, STDOUT_FILENO);
      if (Opts.ReplaceText.hasValue()) {
        unsigned Offset = resolveFromLineCol(Opts.Line, Opts.Col, SourceFile);
        unsigned Length = Opts.Length;
        sourcekitd_object_t EdReq = sourcekitd_request_dictionary_create(nullptr,
                                                                    nullptr, 0);
        sourcekitd_request_dictionary_set_uid(EdReq, KeyRequest,
                                              RequestEditorReplaceText);
        sourcekitd_request_dictionary_set_string(EdReq, KeyName,
                                                 SourceFile.c_str());
        sourcekitd_request_dictionary_set_int64(EdReq, KeyOffset, Offset);
        sourcekitd_request_dictionary_set_int64(EdReq, KeyLength, Length);
        sourcekitd_request_dictionary_set_string(EdReq, KeySourceText,
                                           Opts.ReplaceText.getValue().c_str());
        bool EnableSyntaxMax = Opts.Request == SourceKitRequest::SyntaxMap;
        bool EnableSubStructure = Opts.Request == SourceKitRequest::Structure;
        sourcekitd_request_dictionary_set_int64(EdReq, KeyEnableSyntaxMap,
                                                EnableSyntaxMax);
        sourcekitd_request_dictionary_set_int64(EdReq, KeyEnableSubStructure,
                                                EnableSubStructure);
        sourcekitd_request_dictionary_set_int64(EdReq, KeySyntacticOnly, !Opts.UsedSema);

        sourcekitd_response_t EdResp = sourcekitd_send_request_sync(EdReq);
        sourcekitd_response_description_dump_filedesc(EdResp, STDOUT_FILENO);
        sourcekitd_response_dispose(EdResp);
        sourcekitd_request_release(EdReq);
      }
      break;

    case SourceKitRequest::Format:
      {
        sourcekitd_object_t Fmt = sourcekitd_request_dictionary_create(nullptr,
                                                                    nullptr, 0);
        sourcekitd_request_dictionary_set_uid(Fmt, KeyRequest,
                                              RequestEditorFormatText);
        sourcekitd_request_dictionary_set_string(Fmt, KeyName,
                                                 SourceFile.c_str());
        sourcekitd_request_dictionary_set_string(Fmt, KeySourceText, "");
        sourcekitd_request_dictionary_set_int64(Fmt, KeyLine, Opts.Line);
        sourcekitd_request_dictionary_set_int64(Fmt, KeyLength, Opts.Length);

        if (!Opts.RequestOptions.empty()) {
          sourcekitd_object_t FO = sourcekitd_request_dictionary_create(nullptr,
                                                                    nullptr, 0);
          for (auto &FmtOpt : Opts.RequestOptions) {
            auto KeyValue = StringRef(FmtOpt).split('=');
            std::string KeyStr("key.editor.format.");
            KeyStr.append(KeyValue.first);
            sourcekitd_uid_t Key = sourcekitd_uid_get_from_cstr(KeyStr.c_str());
            int64_t Value = 0;
            KeyValue.second.getAsInteger(0, Value);
            sourcekitd_request_dictionary_set_int64(FO, Key, Value);
          }
          sourcekitd_request_dictionary_set_value(Fmt, KeyFormatOptions, FO);
          sourcekitd_request_release(FO);
        }
        
        sourcekitd_response_t FmtResp = sourcekitd_send_request_sync(Fmt);
        sourcekitd_response_description_dump_filedesc(FmtResp, STDOUT_FILENO);
        sourcekitd_response_dispose(FmtResp);
        sourcekitd_request_release(Fmt);
      }
      break;

      case SourceKitRequest::ExpandPlaceholder:
        expandPlaceholders(SourceBuf.get(), llvm::outs());
        break;
    }
  }

  if (!KeepResponseAlive)
    sourcekitd_response_dispose(Resp);
  sourcekitd_request_release(Req);
  return IsError;
}

sourcekitd_variant_t LatestSemaAnnotations = {{0,0,0}};
sourcekitd_variant_t LatestSemaDiags = {{0,0,0}};

static void getSemanticInfoImpl(sourcekitd_variant_t Info) {
  sourcekitd_variant_t annotations =
    sourcekitd_variant_dictionary_get_value(Info, KeyAnnotations);
  sourcekitd_variant_t diagnosticStage =
    sourcekitd_variant_dictionary_get_value(Info, KeyDiagnosticStage);

  auto hasSemaInfo = [&]{
    if (sourcekitd_variant_get_type(annotations) != SOURCEKITD_VARIANT_TYPE_NULL)
      return true;
    if (sourcekitd_variant_get_type(diagnosticStage) == SOURCEKITD_VARIANT_TYPE_UID) {
      return sourcekitd_variant_uid_get_value(diagnosticStage) == SemaDiagnosticStage;
    }
    return false;
  };

  if (hasSemaInfo()) {
    LatestSemaAnnotations =
      sourcekitd_variant_dictionary_get_value(Info, KeyAnnotations);
    LatestSemaDiags =
      sourcekitd_variant_dictionary_get_value(Info, KeyDiagnostics);
  }
}

static void getSemanticInfo(sourcekitd_variant_t Info, StringRef Filename) {
  getSemanticInfoImpl(Info);

  // Wait for the notification that semantic info is available.
  // But only for 1 min.
  dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 60);
  bool expired = dispatch_semaphore_wait(semaSemaphore, when);
  if (expired) {
    llvm::report_fatal_error("Never got notification for semantic info");
  }

  if (Filename != semaName){
    llvm::report_fatal_error(
      llvm::Twine("Got notification for different doc name: ") + semaName);
  }
  getSemanticInfoImpl(sourcekitd_response_get_value(semaResponse));
}

static int printAnnotations() {
  sourcekitd_variant_description_dump_filedesc(LatestSemaAnnotations,
                                               STDOUT_FILENO);
  return 0;
}

static int printDiags() {
  sourcekitd_variant_description_dump_filedesc(LatestSemaDiags, STDOUT_FILENO);
  return 0;
}

static void printSemanticInfo() {
  printAnnotations();
  if (sourcekitd_variant_get_type(LatestSemaDiags) != SOURCEKITD_VARIANT_TYPE_NULL)
    printDiags();
}

static void notification_receiver(sourcekitd_response_t resp) {
  if (sourcekitd_response_is_error(resp)) {
    sourcekitd_response_description_dump(resp);
    exit(1);
  }

  sourcekitd_variant_t payload = sourcekitd_response_get_value(resp);
  sourcekitd_uid_t note =
      sourcekitd_variant_dictionary_get_uid(payload, KeyNotification);
  semaName = sourcekitd_variant_dictionary_get_string(payload, KeyName);

  if (note == NoteDocUpdate) {
    sourcekitd_object_t edReq = sourcekitd_request_dictionary_create(nullptr,
                                                                nullptr, 0);
    sourcekitd_request_dictionary_set_uid(edReq, KeyRequest,
                                          RequestEditorReplaceText);
    sourcekitd_request_dictionary_set_string(edReq, KeyName, semaName);
    sourcekitd_request_dictionary_set_string(edReq, KeySourceText, "");
    semaResponse = sourcekitd_send_request_sync(edReq);
    sourcekitd_request_release(edReq);
    dispatch_semaphore_signal(semaSemaphore);
  }
}

static void printCursorInfo(sourcekitd_variant_t Info, StringRef FilenameIn,
                            llvm::raw_ostream &OS) {
  sourcekitd_uid_t KindUID = sourcekitd_variant_dictionary_get_uid(Info,
                                      sourcekitd_uid_get_from_cstr("key.kind"));
  if (KindUID == nullptr) {
    OS << "<empty cursor info>\n";
    return;
  }

  std::string Filename = FilenameIn;
  char full_path[MAXPATHLEN];
  if (const char *path = realpath(Filename.c_str(), full_path))
    Filename = path;

  const char *Kind = sourcekitd_uid_get_string_ptr(KindUID);
  const char *USR = sourcekitd_variant_dictionary_get_string(Info, KeyUSR);
  const char *Name = sourcekitd_variant_dictionary_get_string(Info, KeyName);
  const char *Typename = sourcekitd_variant_dictionary_get_string(Info,
                                                                  KeyTypename);
  const char *ModuleName = sourcekitd_variant_dictionary_get_string(Info,
                                                              KeyModuleName);
  const char *ModuleInterfaceName =
      sourcekitd_variant_dictionary_get_string(Info, KeyModuleInterfaceName);
  const char *TypeInterface =
      sourcekitd_variant_dictionary_get_string(Info, KeyTypeInterface);
  bool IsSystem = sourcekitd_variant_dictionary_get_bool(Info, KeyIsSystem);
  const char *AnnotDecl = sourcekitd_variant_dictionary_get_string(Info,
                                                              KeyAnnotatedDecl);
  const char *DocFullAsXML =
      sourcekitd_variant_dictionary_get_string(Info, KeyDocFullAsXML);
  sourcekitd_variant_t OffsetObj =
      sourcekitd_variant_dictionary_get_value(Info, KeyOffset);
  llvm::Optional<int64_t> Offset;
  unsigned Length = 0;
  if (sourcekitd_variant_get_type(OffsetObj) != SOURCEKITD_VARIANT_TYPE_NULL) {
    Offset = sourcekitd_variant_int64_get_value(OffsetObj);
    Length = sourcekitd_variant_dictionary_get_int64(Info, KeyLength);
  }
  const char *FilePath = sourcekitd_variant_dictionary_get_string(Info, KeyFilePath);

  std::vector<const char *> OverrideUSRs;
  sourcekitd_variant_t OverridesObj =
      sourcekitd_variant_dictionary_get_value(Info, KeyOverrides);
  for (unsigned i = 0, e = sourcekitd_variant_array_get_count(OverridesObj);
         i != e; ++i) {
    sourcekitd_variant_t Entry =
      sourcekitd_variant_array_get_value(OverridesObj, i);
    OverrideUSRs.push_back(sourcekitd_variant_dictionary_get_string(Entry, KeyUSR));
  }

  std::vector<const char *> RelatedDecls;
  sourcekitd_variant_t RelatedDeclsObj =
  sourcekitd_variant_dictionary_get_value(Info, KeyRelatedDecls);
  for (unsigned i = 0, e = sourcekitd_variant_array_get_count(RelatedDeclsObj);
       i != e; ++i) {
    sourcekitd_variant_t Entry =
    sourcekitd_variant_array_get_value(RelatedDeclsObj, i);
    RelatedDecls.push_back(sourcekitd_variant_dictionary_get_string(Entry,
                                                             KeyAnnotatedDecl));
  }

  OS << Kind << " (";
  if (Offset.hasValue()) {
    if (Filename != FilePath)
      OS << FilePath << ":";
    auto LineCol = resolveToLineCol(Offset.getValue(), FilePath);
    OS << LineCol.first << ':' << LineCol.second;
    auto EndLineCol = resolveToLineCol(Offset.getValue()+Length, FilePath);
    OS << '-' << EndLineCol.first << ':' << EndLineCol.second;
  }
  OS << ")\n";
  OS << Name << '\n';
  if (USR)
    OS << USR << '\n';
  if (Typename)
    OS << Typename << '\n';
  if (ModuleName)
    OS << ModuleName << '\n';
  if (ModuleInterfaceName)
    OS << ModuleInterfaceName << '\n';
  if (IsSystem)
    OS << "SYSTEM\n";
  if (AnnotDecl)
    OS << AnnotDecl << '\n';
  if (DocFullAsXML)
    OS << DocFullAsXML << '\n';
  OS << "OVERRIDES BEGIN\n";
  for (auto OverUSR : OverrideUSRs)
    OS << OverUSR << '\n';
  OS << "OVERRIDES END\n";
  OS << "RELATED BEGIN\n";
  for (auto RelDecl : RelatedDecls)
    OS << RelDecl << '\n';
  OS << "RELATED END\n";
  OS << "TYPE INTERFACE BEGIN\n";
  if (TypeInterface)
    OS << TypeInterface << '\n';
  OS << "TYPE INTERFACE END\n";
}

static void printFoundInterface(sourcekitd_variant_t Info,
                                llvm::raw_ostream &OS) {
  const char *Name = sourcekitd_variant_dictionary_get_string(Info,
                                                        KeyModuleInterfaceName);
  OS << "DOC: (";
  if (Name)
    OS << Name;
  OS << ")\n";

  sourcekitd_variant_t ArgObj =
      sourcekitd_variant_dictionary_get_value(Info, KeyCompilerArgs);

  OS << "ARGS: [";
  if (sourcekitd_variant_get_type(ArgObj) != SOURCEKITD_VARIANT_TYPE_NULL) {
    for (unsigned i = 0, e = sourcekitd_variant_array_get_count(ArgObj);
           i != e; ++i) {
      OS << sourcekitd_variant_array_get_string(ArgObj, i);
      OS << ' ';
    }
  }
  OS << "]\n";
}

static void printFoundUSR(sourcekitd_variant_t Info,
                          llvm::MemoryBuffer *SourceBuf,
                          llvm::raw_ostream &OS) {
  sourcekitd_variant_t OffsetObj =
      sourcekitd_variant_dictionary_get_value(Info, KeyOffset);
  llvm::Optional<int64_t> Offset;
  if (sourcekitd_variant_get_type(OffsetObj) != SOURCEKITD_VARIANT_TYPE_NULL)
    Offset = sourcekitd_variant_int64_get_value(OffsetObj);

  if (!Offset.hasValue()) {
    OS << "USR NOT FOUND\n";
    return;
  }

  int64_t Length = sourcekitd_variant_dictionary_get_int64(Info, KeyLength);

  auto LineCol1 = resolveToLineCol(Offset.getValue(), SourceBuf);
  auto LineCol2 = resolveToLineCol(Offset.getValue() + Length, SourceBuf);
  OS << '(' << LineCol1.first << ':' << LineCol1.second << '-'
            << LineCol2.first << ':' << LineCol2.second << ")\n";
}

static void printNormalizedDocComment(sourcekitd_variant_t Info) {
  sourcekitd_variant_t Source =
    sourcekitd_variant_dictionary_get_value(Info, KeySourceText);
  sourcekitd_variant_description_dump_filedesc(Source, STDOUT_FILENO);
}

static void printDocInfo(sourcekitd_variant_t Info, StringRef Filename) {
  const char *text =
      sourcekitd_variant_dictionary_get_string(Info, KeySourceText);
  llvm::raw_fd_ostream OS(STDOUT_FILENO, /*shouldClose=*/false);
  if (text) {
    OS << text << '\n';
    OS.flush();
  }

  sourcekitd_variant_t annotations =
      sourcekitd_variant_dictionary_get_value(Info, KeyAnnotations);
  sourcekitd_variant_t entities =
      sourcekitd_variant_dictionary_get_value(Info,
                                  sourcekitd_uid_get_from_cstr("key.entities"));
  sourcekitd_variant_t diags =
      sourcekitd_variant_dictionary_get_value(Info, KeyDiagnostics);

  sourcekitd_variant_description_dump_filedesc(annotations, STDOUT_FILENO);
  sourcekitd_variant_description_dump_filedesc(entities, STDOUT_FILENO);

  if (sourcekitd_variant_get_type(diags) != SOURCEKITD_VARIANT_TYPE_NULL)
    sourcekitd_variant_description_dump_filedesc(diags, STDOUT_FILENO);
}

static void checkTextIsASCII(const char *Text) {
  for (const char *p = Text; *p; ++p) {
    if (*p & 0x80) {
      auto LineCol = resolveToLineColFromBuf(p - Text, Text);

      llvm::errs() << "!!Interface text is non-ascii!!\n"
                   << "@ " << LineCol.first << ":" << LineCol.second << "\n";
      exit(1);
    }
  }
}

static void printInterfaceGen(sourcekitd_variant_t Info, bool CheckASCII) {
  const char *text =
      sourcekitd_variant_dictionary_get_string(Info, KeySourceText);

  if (text) {
    llvm::raw_fd_ostream OS(STDOUT_FILENO, /*shouldClose=*/false);
    OS << text << '\n';
  }

  if (CheckASCII) {
    checkTextIsASCII(text);
  }

  sourcekitd_variant_t syntaxmap =
      sourcekitd_variant_dictionary_get_value(Info, KeySyntaxMap);
  sourcekitd_variant_description_dump_filedesc(syntaxmap, STDOUT_FILENO);
  sourcekitd_variant_t annotations =
      sourcekitd_variant_dictionary_get_value(Info, KeyAnnotations);
  sourcekitd_variant_description_dump_filedesc(annotations, STDOUT_FILENO);
  sourcekitd_variant_t structure =
      sourcekitd_variant_dictionary_get_value(Info, KeySubStructure);
  sourcekitd_variant_description_dump_filedesc(structure, STDOUT_FILENO);
}

static void printRelatedIdents(sourcekitd_variant_t Info, StringRef Filename,
                               llvm::raw_ostream &OS) {
  OS << "START RANGES\n";
  sourcekitd_variant_t Res =
      sourcekitd_variant_dictionary_get_value(Info, KeyResults);
  for (unsigned i=0, e = sourcekitd_variant_array_get_count(Res); i != e; ++i) {
    sourcekitd_variant_t Range = sourcekitd_variant_array_get_value(Res, i);
    int64_t Offset = sourcekitd_variant_dictionary_get_int64(Range, KeyOffset);
    int64_t Length = sourcekitd_variant_dictionary_get_int64(Range, KeyLength);
    auto LineCol = resolveToLineCol(Offset, Filename);
    OS << LineCol.first << ':' << LineCol.second << " - " << Length << '\n';
  }
  OS << "END RANGES\n";
}

static void initializeRewriteBuffer(StringRef Input,
                                    clang::RewriteBuffer &RewriteBuf) {
  RewriteBuf.Initialize(Input);
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

static std::vector<std::pair<unsigned, unsigned>>
getPlaceholderRanges(StringRef Source) {
  const char *StartPtr = Source.data();
  std::vector<std::pair<unsigned, unsigned>> Ranges;
  while (true) {
    size_t Pos = Source.find("<#");
    if (Pos == StringRef::npos)
      break;
    unsigned OffsetStart = Source.data() + Pos - StartPtr;
    Source = Source.substr(Pos+2);
    Pos = Source.find("#>");
    if (Pos == StringRef::npos)
      break;
    unsigned OffsetEnd = Source.data() + Pos + 2 - StartPtr;
    Source = Source.substr(Pos+2);
    Ranges.emplace_back(OffsetStart, OffsetEnd-OffsetStart);
  }
  return Ranges;
}

static void expandPlaceholders(llvm::MemoryBuffer *SourceBuf,
                               llvm::raw_ostream &OS) {
  clang::RewriteBuffer RewriteBuf;
  initializeRewriteBuffer(SourceBuf->getBuffer(), RewriteBuf);
  auto Ranges = getPlaceholderRanges(SourceBuf->getBuffer());
  for (auto Range : Ranges) {
    unsigned Offset = Range.first;
    unsigned Length = Range.second;
    sourcekitd_object_t Exp = sourcekitd_request_dictionary_create(nullptr,
                                                                   nullptr, 0);
    sourcekitd_request_dictionary_set_uid(Exp, KeyRequest,
                                          RequestEditorExpandPlaceholder);
    sourcekitd_request_dictionary_set_string(Exp, KeyName,
                                             SourceBuf->getBufferIdentifier());
    sourcekitd_request_dictionary_set_string(Exp, KeySourceText, "");
    sourcekitd_request_dictionary_set_int64(Exp, KeyOffset, Offset);
    sourcekitd_request_dictionary_set_int64(Exp, KeyLength, Length);

    sourcekitd_response_t Resp = sourcekitd_send_request_sync(Exp);
    if (sourcekitd_response_is_error(Resp)) {
      sourcekitd_response_description_dump(Resp);
      exit(1);
    }
    sourcekitd_request_release(Exp);
    sourcekitd_variant_t Info = sourcekitd_response_get_value(Resp);
    const char *Text = sourcekitd_variant_dictionary_get_string(Info, KeySourceText);
    if (!Text) {
      sourcekitd_response_dispose(Resp);
      continue;
    }
    unsigned EditOffset = sourcekitd_variant_dictionary_get_int64(Info, KeyOffset);
    unsigned EditLength = sourcekitd_variant_dictionary_get_int64(Info, KeyLength);
    RewriteBuf.ReplaceText(EditOffset, EditLength, Text);
    sourcekitd_response_dispose(Resp);
  }

  RewriteBuf.write(OS);
}

static std::pair<unsigned, unsigned>
resolveToLineCol(unsigned Offset, StringRef Filename) {
  return resolveToLineCol(Offset, getBufferForFilename(Filename));
}

static std::pair<unsigned, unsigned>
resolveToLineCol(unsigned Offset, llvm::MemoryBuffer *InputBuf) {
  if (Offset >= InputBuf->getBufferSize()) {
    llvm::errs() << "offset " << Offset << " for filename '"
        << InputBuf->getBufferIdentifier() << "' is too large\n";
    exit(1);
  }
  return resolveToLineColFromBuf(Offset, InputBuf->getBufferStart());
}

static std::pair<unsigned, unsigned>
resolveToLineColFromBuf(unsigned Offset, const char *Ptr) {
  const char *End = Ptr+Offset;

  unsigned Line = 1;
  const char *LineStart = Ptr;
  for (; Ptr < End; ++Ptr) {
    if (*Ptr == '\n') {
      ++Line;
      LineStart = Ptr+1;
    }
  }
  unsigned Col = Ptr-LineStart + 1;

  return { Line, Col };
}

static unsigned resolveFromLineCol(unsigned Line, unsigned Col,
                                   StringRef Filename) {
  return resolveFromLineCol(Line, Col, getBufferForFilename(Filename));
}

static unsigned resolveFromLineCol(unsigned Line, unsigned Col,
                                   llvm::MemoryBuffer *InputBuf) {
  if (Line == 0 || Col == 0) {
    llvm::errs() << "wrong pos format, line/col should start from 1\n";
    exit(1);
  }

  const char *Ptr = InputBuf->getBufferStart();
  const char *End = InputBuf->getBufferEnd();
  const char *LineStart = Ptr;
  for (; Ptr < End; ++Ptr) {
    if (*Ptr == '\n') {
      --Line;
      if (Line == 0)
        break;
      LineStart = Ptr+1;
    }
  }
  if (Line != 0) {
    llvm::errs() << "wrong pos format, line too large\n";
    exit(1);
  }
  Ptr = LineStart;
  for (; Ptr < End; ++Ptr) {
    --Col;
    if (Col == 0)
      return Ptr - InputBuf->getBufferStart();
    if (*Ptr == '\n')
      break;
  }

  llvm::errs() << "wrong pos format, column too large\n";
  exit(1);
}

static llvm::StringMap<llvm::MemoryBuffer*> Buffers;

static llvm::MemoryBuffer *getBufferForFilename(StringRef Filename) {
  auto It = Buffers.find(Filename);
  if (It != Buffers.end())
    return It->second;

  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> FileBufOrErr =
    llvm::MemoryBuffer::getFile(Filename);
  if (!FileBufOrErr) {
    llvm::errs() << "error opening input file '" << Filename << "' ("
                 << FileBufOrErr.getError().message() << ")\n";
    exit(1);
  }

  return Buffers[Filename] = FileBufOrErr.get().release();
}
