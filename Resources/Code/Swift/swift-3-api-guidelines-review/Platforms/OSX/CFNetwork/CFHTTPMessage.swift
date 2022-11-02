
@available(OSX 10.1, *)
let kCFHTTPVersion1_0: CFString
@available(OSX 10.1, *)
let kCFHTTPVersion1_1: CFString
@available(OSX 10.10, *)
let kCFHTTPVersion2_0: CFString
@available(OSX 10.2, *)
let kCFHTTPAuthenticationSchemeBasic: CFString
@available(OSX 10.2, *)
let kCFHTTPAuthenticationSchemeDigest: CFString
@available(OSX 10.5, *)
let kCFHTTPAuthenticationSchemeNTLM: CFString
@available(OSX 10.5, *)
let kCFHTTPAuthenticationSchemeKerberos: CFString
@available(OSX 10.5, *)
let kCFHTTPAuthenticationSchemeNegotiate: CFString
@available(OSX 10.6, *)
let kCFHTTPAuthenticationSchemeNegotiate2: CFString
@available(OSX 10.6, *)
let kCFHTTPAuthenticationSchemeXMobileMeAuthToken: CFString
@available(OSX 10.9, *)
let kCFHTTPAuthenticationSchemeOAuth1: CFString
class CFHTTPMessage {
}
@available(OSX 10.1, *)
func CFHTTPMessageGetTypeID() -> CFTypeID
@available(OSX 10.1, *)
func CFHTTPMessageCreateRequest(_ alloc: CFAllocator?, _ requestMethod: CFString, _ url: CFURL, _ httpVersion: CFString) -> Unmanaged<CFHTTPMessage>
@available(OSX 10.1, *)
func CFHTTPMessageCreateResponse(_ alloc: CFAllocator?, _ statusCode: CFIndex, _ statusDescription: CFString?, _ httpVersion: CFString) -> Unmanaged<CFHTTPMessage>
@available(OSX 10.1, *)
func CFHTTPMessageCreateEmpty(_ alloc: CFAllocator?, _ isRequest: Bool) -> Unmanaged<CFHTTPMessage>
@available(OSX 10.1, *)
func CFHTTPMessageCreateCopy(_ alloc: CFAllocator?, _ message: CFHTTPMessage) -> Unmanaged<CFHTTPMessage>
@available(OSX 10.1, *)
func CFHTTPMessageIsRequest(_ message: CFHTTPMessage) -> Bool
@available(OSX 10.1, *)
func CFHTTPMessageCopyVersion(_ message: CFHTTPMessage) -> Unmanaged<CFString>
@available(OSX 10.1, *)
func CFHTTPMessageCopyBody(_ message: CFHTTPMessage) -> Unmanaged<CFData>?
@available(OSX 10.1, *)
func CFHTTPMessageSetBody(_ message: CFHTTPMessage, _ bodyData: CFData)
@available(OSX 10.1, *)
func CFHTTPMessageCopyHeaderFieldValue(_ message: CFHTTPMessage, _ headerField: CFString) -> Unmanaged<CFString>?
@available(OSX 10.1, *)
func CFHTTPMessageCopyAllHeaderFields(_ message: CFHTTPMessage) -> Unmanaged<CFDictionary>?
@available(OSX 10.1, *)
func CFHTTPMessageSetHeaderFieldValue(_ message: CFHTTPMessage, _ headerField: CFString, _ value: CFString?)
@available(OSX 10.1, *)
func CFHTTPMessageAppendBytes(_ message: CFHTTPMessage, _ newBytes: UnsafePointer<UInt8>, _ numBytes: CFIndex) -> Bool
@available(OSX 10.1, *)
func CFHTTPMessageIsHeaderComplete(_ message: CFHTTPMessage) -> Bool
@available(OSX 10.1, *)
func CFHTTPMessageCopySerializedMessage(_ message: CFHTTPMessage) -> Unmanaged<CFData>?
@available(OSX 10.1, *)
func CFHTTPMessageCopyRequestURL(_ request: CFHTTPMessage) -> Unmanaged<CFURL>?
@available(OSX 10.1, *)
func CFHTTPMessageCopyRequestMethod(_ request: CFHTTPMessage) -> Unmanaged<CFString>?
@available(OSX 10.1, *)
func CFHTTPMessageAddAuthentication(_ request: CFHTTPMessage, _ authenticationFailureResponse: CFHTTPMessage?, _ username: CFString, _ password: CFString, _ authenticationScheme: CFString?, _ forProxy: Bool) -> Bool
@available(OSX 10.1, *)
func CFHTTPMessageGetResponseStatusCode(_ response: CFHTTPMessage) -> CFIndex
@available(OSX 10.1, *)
func CFHTTPMessageCopyResponseStatusLine(_ response: CFHTTPMessage) -> Unmanaged<CFString>?
