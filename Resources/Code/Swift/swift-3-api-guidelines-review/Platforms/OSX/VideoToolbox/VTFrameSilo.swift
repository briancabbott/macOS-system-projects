
class VTFrameSilo {
}
@available(OSX 10.10, *)
func VTFrameSiloGetTypeID() -> CFTypeID
@available(OSX 10.10, *)
func VTFrameSiloCreate(_ allocator: CFAllocator?, _ fileURL: CFURL?, _ timeRange: CMTimeRange, _ options: CFDictionary?, _ siloOut: UnsafeMutablePointer<VTFrameSilo?>) -> OSStatus
@available(OSX 10.10, *)
func VTFrameSiloAddSampleBuffer(_ silo: VTFrameSilo, _ sampleBuffer: CMSampleBuffer) -> OSStatus
@available(OSX 10.10, *)
func VTFrameSiloSetTimeRangesForNextPass(_ silo: VTFrameSilo, _ timeRangeCount: CMItemCount, _ timeRangeArray: UnsafePointer<CMTimeRange>) -> OSStatus
@available(OSX 10.10, *)
func VTFrameSiloGetProgressOfCurrentPass(_ silo: VTFrameSilo, _ progressOut: UnsafeMutablePointer<Float32>) -> OSStatus
@available(OSX 10.10, *)
func VTFrameSiloCallFunctionForEachSampleBuffer(_ silo: VTFrameSilo, _ timeRange: CMTimeRange, _ callbackInfo: UnsafeMutablePointer<Void>, _ callback: @convention(c) (UnsafeMutablePointer<Void>, CMSampleBuffer) -> OSStatus) -> OSStatus
@available(OSX 10.10, *)
func VTFrameSiloCallBlockForEachSampleBuffer(_ silo: VTFrameSilo, _ timeRange: CMTimeRange, _ handler: (CMSampleBuffer) -> OSStatus) -> OSStatus
