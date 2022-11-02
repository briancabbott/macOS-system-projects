
class CMMemoryPool {
}
@available(tvOS 6.0, *)
func CMMemoryPoolGetTypeID() -> CFTypeID
@available(tvOS 6.0, *)
let kCMMemoryPoolOption_AgeOutPeriod: CFString
@available(tvOS 6.0, *)
func CMMemoryPoolCreate(_ options: CFDictionary?) -> CMMemoryPool
@available(tvOS 6.0, *)
func CMMemoryPoolGetAllocator(_ pool: CMMemoryPool) -> CFAllocator
@available(tvOS 6.0, *)
func CMMemoryPoolFlush(_ pool: CMMemoryPool)
@available(tvOS 6.0, *)
func CMMemoryPoolInvalidate(_ pool: CMMemoryPool)
