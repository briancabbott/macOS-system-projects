import _CJavaScriptKit

@dynamicCallable
public class JSFunctionRef: JSObjectRef {

    @discardableResult
    public func dynamicallyCall(withArguments arguments: [JSValueConvertible]) -> JSValue {
        let result = arguments.withRawJSValues { rawValues -> RawJSValue in
            return rawValues.withUnsafeBufferPointer { bufferPointer -> RawJSValue in
                let argv = bufferPointer.baseAddress
                let argc = bufferPointer.count
                var result = RawJSValue()
                _call_function(
                    self.id, argv, Int32(argc),
                    &result.kind, &result.payload1, &result.payload2, &result.payload3
                )
                return result
            }
        }
        return result.jsValue()
    }

    @discardableResult
    public func apply(this: JSObjectRef, arguments: JSValueConvertible...) -> JSValue {
        return apply(this: this, argumentList: arguments)
    }
    
    @discardableResult
    public func apply(this: JSObjectRef, argumentList: [JSValueConvertible]) -> JSValue {
        let result = argumentList.withRawJSValues { rawValues in
            rawValues.withUnsafeBufferPointer { bufferPointer -> RawJSValue in
                let argv = bufferPointer.baseAddress
                let argc = bufferPointer.count
                var result = RawJSValue()
                _call_function_with_this(this.id,
                    self.id, argv, Int32(argc),
                    &result.kind, &result.payload1, &result.payload2, &result.payload3
                )
                return result
            }
        }
        return result.jsValue()
    }

    public func new(_ arguments: JSValueConvertible...) -> JSObjectRef {
        return arguments.withRawJSValues { rawValues in
            rawValues.withUnsafeBufferPointer { bufferPointer in
                let argv = bufferPointer.baseAddress
                let argc = bufferPointer.count
                var resultObj = JavaScriptObjectRef()
                _call_new(
                    self.id, argv, Int32(argc),
                    &resultObj
                )
                return JSObjectRef(id: resultObj)
            }
        }
    }
    
    @available(*, unavailable, message: "Please use JSClosure instead")
    public static func from(_ body: @escaping ([JSValue]) -> JSValue) -> JSFunctionRef {
        fatalError("unavailable")
    }
    
    // MARK: - JSValueConvertible

    public override func jsValue() -> JSValue {
        .function(self)
    }
}

public class JSClosure: JSFunctionRef {

    static var sharedFunctions: [JavaScriptHostFuncRef: ([JSValue]) -> JSValue] = [:]

    private var hostFuncRef: JavaScriptHostFuncRef = 0

    public init(_ body: @escaping ([JSValue]) -> JSValue) {
        super.init(id: 0)
        let objectId = ObjectIdentifier(self)
        let funcRef = JavaScriptHostFuncRef(bitPattern: Int32(objectId.hashValue))
        Self.sharedFunctions[funcRef] = body

        var objectRef: JavaScriptObjectRef = 0
        _create_function(funcRef, &objectRef)

        self.hostFuncRef = funcRef
        self.id = objectRef
    }

    public func release() {
        Self.sharedFunctions[hostFuncRef] = nil
    }
}

@_cdecl("swjs_prepare_host_function_call")
internal func _prepare_host_function_call(_ argc: Int32) -> UnsafeMutableRawPointer {
    let argumentSize = MemoryLayout<RawJSValue>.size * Int(argc)
    return malloc(Int(argumentSize))!
}

@_cdecl("swjs_cleanup_host_function_call")
internal func _cleanup_host_function_call(_ pointer: UnsafeMutableRawPointer) {
    free(pointer)
}

@_cdecl("swjs_call_host_function")
internal func _call_host_function(
    _ hostFuncRef: JavaScriptHostFuncRef,
    _ argv: UnsafePointer<RawJSValue>, _ argc: Int32,
    _ callbackFuncRef: JavaScriptObjectRef) {
    guard let hostFunc = JSClosure.sharedFunctions[hostFuncRef] else {
        fatalError("The function was already released")
    }
    let args = UnsafeBufferPointer(start: argv, count: Int(argc)).map {
        $0.jsValue()
    }
    let result = hostFunc(args)
    let callbackFuncRef = JSFunctionRef(id: callbackFuncRef)
    _ = callbackFuncRef(result)
}
