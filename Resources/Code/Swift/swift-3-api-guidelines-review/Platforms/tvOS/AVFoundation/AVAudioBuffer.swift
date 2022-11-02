
@available(tvOS 8.0, *)
class AVAudioBuffer : NSObject, NSCopying, NSMutableCopying {
  var format: AVAudioFormat { get }
  var audioBufferList: UnsafePointer<AudioBufferList> { get }
  var mutableAudioBufferList: UnsafeMutablePointer<AudioBufferList> { get }
  @available(tvOS 8.0, *)
  func copy(with zone: NSZone = nil) -> AnyObject
  @available(tvOS 8.0, *)
  func mutableCopy(with zone: NSZone = nil) -> AnyObject
}
@available(tvOS 8.0, *)
class AVAudioPCMBuffer : AVAudioBuffer {
  init(pcmFormat format: AVAudioFormat, frameCapacity frameCapacity: AVAudioFrameCount)
  var frameCapacity: AVAudioFrameCount { get }
  var frameLength: AVAudioFrameCount
  var stride: Int { get }
  var floatChannelData: UnsafePointer<UnsafeMutablePointer<Float>> { get }
  var int16ChannelData: UnsafePointer<UnsafeMutablePointer<Int16>> { get }
  var int32ChannelData: UnsafePointer<UnsafeMutablePointer<Int32>> { get }
}
@available(tvOS 9.0, *)
class AVAudioCompressedBuffer : AVAudioBuffer {
  init(format format: AVAudioFormat, packetCapacity packetCapacity: AVAudioPacketCount, maximumPacketSize maximumPacketSize: Int)
  init(format format: AVAudioFormat, packetCapacity packetCapacity: AVAudioPacketCount)
  var packetCapacity: AVAudioPacketCount { get }
  var packetCount: AVAudioPacketCount
  var maximumPacketSize: Int { get }
  var data: UnsafeMutablePointer<Void> { get }
  var packetDescriptions: UnsafeMutablePointer<AudioStreamPacketDescription> { get }
}
