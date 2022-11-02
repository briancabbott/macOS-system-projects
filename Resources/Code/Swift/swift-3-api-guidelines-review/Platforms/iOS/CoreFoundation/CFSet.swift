
typealias CFSetRetainCallBack = @convention(c) (CFAllocator!, UnsafePointer<Void>) -> UnsafePointer<Void>
typealias CFSetReleaseCallBack = @convention(c) (CFAllocator!, UnsafePointer<Void>) -> Void
typealias CFSetCopyDescriptionCallBack = @convention(c) (UnsafePointer<Void>) -> Unmanaged<CFString>!
typealias CFSetEqualCallBack = @convention(c) (UnsafePointer<Void>, UnsafePointer<Void>) -> DarwinBoolean
typealias CFSetHashCallBack = @convention(c) (UnsafePointer<Void>) -> CFHashCode
struct CFSetCallBacks {
  var version: CFIndex
  var retain: CFSetRetainCallBack!
  var release: CFSetReleaseCallBack!
  var copyDescription: CFSetCopyDescriptionCallBack!
  var equal: CFSetEqualCallBack!
  var hash: CFSetHashCallBack!
  init()
  init(version version: CFIndex, retain retain: CFSetRetainCallBack!, release release: CFSetReleaseCallBack!, copyDescription copyDescription: CFSetCopyDescriptionCallBack!, equal equal: CFSetEqualCallBack!, hash hash: CFSetHashCallBack!)
}
let kCFTypeSetCallBacks: CFSetCallBacks
let kCFCopyStringSetCallBacks: CFSetCallBacks
typealias CFSetApplierFunction = @convention(c) (UnsafePointer<Void>, UnsafeMutablePointer<Void>) -> Void
class CFSet {
}
class CFMutableSet {
}
func CFSetGetTypeID() -> CFTypeID
func CFSetCreate(_ allocator: CFAllocator!, _ values: UnsafeMutablePointer<UnsafePointer<Void>>, _ numValues: CFIndex, _ callBacks: UnsafePointer<CFSetCallBacks>) -> CFSet!
func CFSetCreateCopy(_ allocator: CFAllocator!, _ theSet: CFSet!) -> CFSet!
func CFSetCreateMutable(_ allocator: CFAllocator!, _ capacity: CFIndex, _ callBacks: UnsafePointer<CFSetCallBacks>) -> CFMutableSet!
func CFSetCreateMutableCopy(_ allocator: CFAllocator!, _ capacity: CFIndex, _ theSet: CFSet!) -> CFMutableSet!
func CFSetGetCount(_ theSet: CFSet!) -> CFIndex
func CFSetGetCountOfValue(_ theSet: CFSet!, _ value: UnsafePointer<Void>) -> CFIndex
func CFSetContainsValue(_ theSet: CFSet!, _ value: UnsafePointer<Void>) -> Bool
func CFSetGetValue(_ theSet: CFSet!, _ value: UnsafePointer<Void>) -> UnsafePointer<Void>
func CFSetGetValueIfPresent(_ theSet: CFSet!, _ candidate: UnsafePointer<Void>, _ value: UnsafeMutablePointer<UnsafePointer<Void>>) -> Bool
func CFSetGetValues(_ theSet: CFSet!, _ values: UnsafeMutablePointer<UnsafePointer<Void>>)
func CFSetApplyFunction(_ theSet: CFSet!, _ applier: CFSetApplierFunction!, _ context: UnsafeMutablePointer<Void>)
func CFSetAddValue(_ theSet: CFMutableSet!, _ value: UnsafePointer<Void>)
func CFSetReplaceValue(_ theSet: CFMutableSet!, _ value: UnsafePointer<Void>)
func CFSetSetValue(_ theSet: CFMutableSet!, _ value: UnsafePointer<Void>)
func CFSetRemoveValue(_ theSet: CFMutableSet!, _ value: UnsafePointer<Void>)
func CFSetRemoveAllValues(_ theSet: CFMutableSet!)
