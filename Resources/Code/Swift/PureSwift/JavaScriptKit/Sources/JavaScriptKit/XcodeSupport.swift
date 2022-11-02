import _CJavaScriptKit

/// Note:
/// Define all runtime functions stub which are imported from JavaScript environment.
/// SwiftPM doesn't support WebAssembly target yet, so we need to define them to
/// avoid link failure.
/// When running with JavaScript runtime library, they are ignored completely.
#if !arch(wasm32)
func _set_prop(
    _ _this: JavaScriptObjectRef,
    _ prop: UnsafePointer<Int8>!, _ length: Int32,
    _ kind: JavaScriptValueKind,
    _ payload1: JavaScriptPayload1,
    _ payload2: JavaScriptPayload2,
    _ payload3: JavaScriptPayload3) { fatalError() }
func _get_prop(
    _ _this: JavaScriptObjectRef,
    _ prop: UnsafePointer<Int8>!, _ length: Int32,
    _ kind: UnsafeMutablePointer<JavaScriptValueKind>!,
    _ payload1: UnsafeMutablePointer<JavaScriptPayload1>!,
    _ payload2: UnsafeMutablePointer<JavaScriptPayload2>!,
    _ payload3: UnsafeMutablePointer<JavaScriptPayload3>!) { fatalError() }
func _set_subscript(
    _ _this: JavaScriptObjectRef,
    _ index: Int32,
    _ kind: JavaScriptValueKind,
    _ payload1: JavaScriptPayload1,
    _ payload2: JavaScriptPayload2,
    _ payload3: JavaScriptPayload3) { fatalError() }
func _get_subscript(
    _ _this: JavaScriptObjectRef,
    _ index: Int32,
    _ kind: UnsafeMutablePointer<JavaScriptValueKind>!,
    _ payload1: UnsafeMutablePointer<JavaScriptPayload1>!,
    _ payload2: UnsafeMutablePointer<JavaScriptPayload2>!,
    _ payload3: UnsafeMutablePointer<JavaScriptPayload3>!) { fatalError() }
func _load_string(
    _ ref: JavaScriptObjectRef,
    _ buffer: UnsafeMutablePointer<UInt8>!) { fatalError() }
func _call_function(
    _ ref: JavaScriptObjectRef,
    _ argv: UnsafePointer<RawJSValue>!, _ argc: Int32,
    _ result_kind: UnsafeMutablePointer<JavaScriptValueKind>!,
    _ result_payload1: UnsafeMutablePointer<JavaScriptPayload1>!,
    _ result_payload2: UnsafeMutablePointer<JavaScriptPayload2>!,
    _ result_payload3: UnsafeMutablePointer<JavaScriptPayload3>!) { fatalError() }
func _call_function_with_this(
    _ _this: JavaScriptObjectRef,
    _ func_ref: JavaScriptObjectRef,
    _ argv: UnsafePointer<RawJSValue>!, _ argc: Int32,
    _ result_kind: UnsafeMutablePointer<JavaScriptValueKind>!,
    _ result_payload1: UnsafeMutablePointer<JavaScriptPayload1>!,
    _ result_payload2: UnsafeMutablePointer<JavaScriptPayload2>!,
    _ result_payload3: UnsafeMutablePointer<JavaScriptPayload3>!) { fatalError() }
func _call_new(
    _ ref: JavaScriptObjectRef,
    _ argv: UnsafePointer<RawJSValue>!, _ argc: Int32,
    _ result_obj: UnsafeMutablePointer<JavaScriptObjectRef>!) { fatalError() }
func _create_function(
    _ host_func_id: JavaScriptHostFuncRef,
    _ func_ref_ptr: UnsafePointer<JavaScriptObjectRef>!) { fatalError() }
func _destroy_ref(_ ref: JavaScriptObjectRef) { fatalError() }
#endif
