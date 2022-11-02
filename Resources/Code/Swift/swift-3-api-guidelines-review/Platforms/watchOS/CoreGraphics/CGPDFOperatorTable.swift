
typealias CGPDFOperatorTableRef = COpaquePointer
typealias CGPDFOperatorCallback = @convention(c) (CGPDFScannerRef, UnsafeMutablePointer<Void>) -> Void
@available(watchOS 2.0, *)
func CGPDFOperatorTableCreate() -> CGPDFOperatorTableRef
@available(watchOS 2.0, *)
func CGPDFOperatorTableRetain(_ table: CGPDFOperatorTableRef) -> CGPDFOperatorTableRef
@available(watchOS 2.0, *)
func CGPDFOperatorTableRelease(_ table: CGPDFOperatorTableRef)
@available(watchOS 2.0, *)
func CGPDFOperatorTableSetCallback(_ table: CGPDFOperatorTableRef, _ name: UnsafePointer<Int8>, _ callback: CGPDFOperatorCallback?)
