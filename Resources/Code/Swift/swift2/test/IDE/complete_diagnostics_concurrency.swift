// REQUIRES: concurrency 

// RUN: %empty-directory(%t)
// RUN: %target-swift-ide-test -batch-code-completion -source-filename %s -filecheck %raw-FileCheck -completion-output-dir %t/output -warn-concurrency

func asyncFunc() async {}
func syncFunc() {}

struct MySendable {}
class MyNonSendable {}


actor MyActor {
    func receiveSendable(arg: MySendable) {}
    func receiveNonSendable(arg: MyNonSendable) {}
}

func testAsyncContext() {
    #^SYNC_CONTEXT^#
// SYNC_CONTEXT: Begin completions
// SYNC_CONTEXT-DAG: Decl[FreeFunction]/CurrModule/NotRecommended: asyncFunc()[' async'][#Void#]; name=asyncFunc(); diagnostics=error:async 'asyncFunc()' used in a context that does not support concurrency{{$}}
// SYNC_CONTEXT-DAG: Decl[FreeFunction]/CurrModule:      syncFunc()[#Void#]; name=syncFunc(){{$}}
// SYNC_CONTEXT: End completions
}

func testActor(obj: MyActor) async {
    obj.#^ACTOR^#
// ACTOR: Begin completions
// ACTOR-DAG: Decl[InstanceMethod]/CurrNominal:   receiveSendable({#arg: MySendable#})[' async'][#Void#]; name=receiveSendable(arg:){{$}}
// ACTOR-DAG: Decl[InstanceMethod]/CurrNominal/NotRecommended: receiveNonSendable({#arg: MyNonSendable#})[' async'][#Void#]; name=receiveNonSendable(arg:); diagnostics=warning:actor-isolated 'receiveNonSendable(arg:)' should only be referenced from inside the actor{{$}}
// ACTOR: End completions
}

func testClosure(obj: (Int) async -> Void) {
  obj(#^CLOSURE_CALL^#)
// CLOSURE_CALL: Begin completions
// CLOSURE_CALL-DAG: Pattern/CurrModule/Flair[ArgLabels]/NotRecommended: ['(']{#Int#}[')'][' async'][#Void#]; name=; diagnostics=error:async function used in a context that does not support concurrency
// CLOSURE_CALL: End completions
}

func test() {
  struct Foo {
    var value: String? {
      get async { nil }
    }
  }

  var globalValue: String? {
    get async { nil }
  }

  let foo = Foo()
  foo.#^EXPLICITLY_ASYNC_PROPERTY^#
// EXPLICITLY_ASYNC_PROPERTY: Begin completions
// EXPLICITLY_ASYNC_PROPERTY-DAG: Decl[InstanceVar]/CurrNominal/NotRecommended: value[#String?#][' async']; name=value; diagnostics=error:async 'value' used in a context that does not support concurrency
// EXPLICITLY_ASYNC_PROPERTY: End completions

  #^EXPLICIT_GLOBAL_VAR^#
// EXPLICIT_GLOBAL_VAR: Begin completions
// EXPLICIT_GLOBAL_VAR-DAG: Decl[LocalVar]/Local/NotRecommended: globalValue[#String?#][' async']; name=globalValue; diagnostics=error:async 'globalValue' used in a context that does not support concurrency
// EXPLICIT_GLOBAL_VAR: End completions

}
