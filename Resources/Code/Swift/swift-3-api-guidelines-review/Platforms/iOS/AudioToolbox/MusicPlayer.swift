
var kMusicEventType_NULL: UInt32 { get }
var kMusicEventType_ExtendedNote: UInt32 { get }
var kMusicEventType_ExtendedTempo: UInt32 { get }
var kMusicEventType_User: UInt32 { get }
var kMusicEventType_Meta: UInt32 { get }
var kMusicEventType_MIDINoteMessage: UInt32 { get }
var kMusicEventType_MIDIChannelMessage: UInt32 { get }
var kMusicEventType_MIDIRawData: UInt32 { get }
var kMusicEventType_Parameter: UInt32 { get }
var kMusicEventType_AUPreset: UInt32 { get }
typealias MusicEventType = UInt32
struct MusicSequenceLoadFlags : OptionSetType {
  init(rawValue rawValue: UInt32)
  let rawValue: UInt32
  static var smf_PreserveTracks: MusicSequenceLoadFlags { get }
  static var smf_ChannelsToTracks: MusicSequenceLoadFlags { get }
}
enum MusicSequenceType : UInt32 {
  init?(rawValue rawValue: UInt32)
  var rawValue: UInt32 { get }
  case beats
  case seconds
  case samples
}
enum MusicSequenceFileTypeID : UInt32 {
  init?(rawValue rawValue: UInt32)
  var rawValue: UInt32 { get }
  case anyType
  case midiType
  case iMelodyType
}
struct MusicSequenceFileFlags : OptionSetType {
  init(rawValue rawValue: UInt32)
  let rawValue: UInt32
  static var `default`: MusicSequenceFileFlags { get }
  static var eraseFile: MusicSequenceFileFlags { get }
}
typealias MusicTimeStamp = Float64
var kMusicTimeStamp_EndOfTrack: Double { get }
struct MIDINoteMessage {
  var channel: UInt8
  var note: UInt8
  var velocity: UInt8
  var releaseVelocity: UInt8
  var duration: Float32
  init()
  init(channel channel: UInt8, note note: UInt8, velocity velocity: UInt8, releaseVelocity releaseVelocity: UInt8, duration duration: Float32)
}
struct MIDIChannelMessage {
  var status: UInt8
  var data1: UInt8
  var data2: UInt8
  var reserved: UInt8
  init()
  init(status status: UInt8, data1 data1: UInt8, data2 data2: UInt8, reserved reserved: UInt8)
}
struct MIDIRawData {
  var length: UInt32
  var data: (UInt8)
  init()
  init(length length: UInt32, data data: (UInt8))
}
struct MIDIMetaEvent {
  var metaEventType: UInt8
  var unused1: UInt8
  var unused2: UInt8
  var unused3: UInt8
  var dataLength: UInt32
  var data: (UInt8)
  init()
  init(metaEventType metaEventType: UInt8, unused1 unused1: UInt8, unused2 unused2: UInt8, unused3 unused3: UInt8, dataLength dataLength: UInt32, data data: (UInt8))
}
struct MusicEventUserData {
  var length: UInt32
  var data: (UInt8)
  init()
  init(length length: UInt32, data data: (UInt8))
}
struct ExtendedNoteOnEvent {
  var instrumentID: MusicDeviceInstrumentID
  var groupID: MusicDeviceGroupID
  var duration: Float32
  var extendedParams: MusicDeviceNoteParams
  init()
  init(instrumentID instrumentID: MusicDeviceInstrumentID, groupID groupID: MusicDeviceGroupID, duration duration: Float32, extendedParams extendedParams: MusicDeviceNoteParams)
}
struct ParameterEvent {
  var parameterID: AudioUnitParameterID
  var scope: AudioUnitScope
  var element: AudioUnitElement
  var value: AudioUnitParameterValue
  init()
  init(parameterID parameterID: AudioUnitParameterID, scope scope: AudioUnitScope, element element: AudioUnitElement, value value: AudioUnitParameterValue)
}
struct ExtendedTempoEvent {
  var bpm: Float64
  init()
  init(bpm bpm: Float64)
}
struct AUPresetEvent {
  var scope: AudioUnitScope
  var element: AudioUnitElement
  var preset: Unmanaged<CFPropertyList>
}
struct CABarBeatTime {
  var bar: Int32
  var beat: UInt16
  var subbeat: UInt16
  var subbeatDivisor: UInt16
  var reserved: UInt16
  init()
  init(bar bar: Int32, beat beat: UInt16, subbeat subbeat: UInt16, subbeatDivisor subbeatDivisor: UInt16, reserved reserved: UInt16)
}
typealias MusicPlayer = COpaquePointer
typealias MusicSequence = COpaquePointer
typealias MusicTrack = COpaquePointer
typealias MusicEventIterator = COpaquePointer
typealias MusicSequenceUserCallback = @convention(c) (UnsafeMutablePointer<Void>, MusicSequence, MusicTrack, MusicTimeStamp, UnsafePointer<MusicEventUserData>, MusicTimeStamp, MusicTimeStamp) -> Void
var kAudioToolboxErr_InvalidSequenceType: OSStatus { get }
var kAudioToolboxErr_TrackIndexError: OSStatus { get }
var kAudioToolboxErr_TrackNotFound: OSStatus { get }
var kAudioToolboxErr_EndOfTrack: OSStatus { get }
var kAudioToolboxErr_StartOfTrack: OSStatus { get }
var kAudioToolboxErr_IllegalTrackDestination: OSStatus { get }
var kAudioToolboxErr_NoSequence: OSStatus { get }
var kAudioToolboxErr_InvalidEventType: OSStatus { get }
var kAudioToolboxErr_InvalidPlayerState: OSStatus { get }
var kAudioToolboxErr_CannotDoInCurrentContext: OSStatus { get }
var kAudioToolboxError_NoTrackDestination: OSStatus { get }
var kSequenceTrackProperty_LoopInfo: UInt32 { get }
var kSequenceTrackProperty_OffsetTime: UInt32 { get }
var kSequenceTrackProperty_MuteStatus: UInt32 { get }
var kSequenceTrackProperty_SoloStatus: UInt32 { get }
var kSequenceTrackProperty_AutomatedParameters: UInt32 { get }
var kSequenceTrackProperty_TrackLength: UInt32 { get }
var kSequenceTrackProperty_TimeResolution: UInt32 { get }
struct MusicTrackLoopInfo {
  var loopDuration: MusicTimeStamp
  var numberOfLoops: Int32
  init()
  init(loopDuration loopDuration: MusicTimeStamp, numberOfLoops numberOfLoops: Int32)
}
@available(iOS 5.0, *)
func NewMusicPlayer(_ outPlayer: UnsafeMutablePointer<MusicPlayer>) -> OSStatus
@available(iOS 5.0, *)
func DisposeMusicPlayer(_ inPlayer: MusicPlayer) -> OSStatus
@available(iOS 5.0, *)
func MusicPlayerSetSequence(_ inPlayer: MusicPlayer, _ inSequence: MusicSequence) -> OSStatus
@available(iOS 5.0, *)
func MusicPlayerGetSequence(_ inPlayer: MusicPlayer, _ outSequence: UnsafeMutablePointer<MusicSequence>) -> OSStatus
@available(iOS 5.0, *)
func MusicPlayerSetTime(_ inPlayer: MusicPlayer, _ inTime: MusicTimeStamp) -> OSStatus
@available(iOS 5.0, *)
func MusicPlayerGetTime(_ inPlayer: MusicPlayer, _ outTime: UnsafeMutablePointer<MusicTimeStamp>) -> OSStatus
@available(iOS 5.0, *)
func MusicPlayerGetHostTimeForBeats(_ inPlayer: MusicPlayer, _ inBeats: MusicTimeStamp, _ outHostTime: UnsafeMutablePointer<UInt64>) -> OSStatus
@available(iOS 5.0, *)
func MusicPlayerGetBeatsForHostTime(_ inPlayer: MusicPlayer, _ inHostTime: UInt64, _ outBeats: UnsafeMutablePointer<MusicTimeStamp>) -> OSStatus
@available(iOS 5.0, *)
func MusicPlayerPreroll(_ inPlayer: MusicPlayer) -> OSStatus
@available(iOS 5.0, *)
func MusicPlayerStart(_ inPlayer: MusicPlayer) -> OSStatus
@available(iOS 5.0, *)
func MusicPlayerStop(_ inPlayer: MusicPlayer) -> OSStatus
@available(iOS 5.0, *)
func MusicPlayerIsPlaying(_ inPlayer: MusicPlayer, _ outIsPlaying: UnsafeMutablePointer<DarwinBoolean>) -> OSStatus
@available(iOS 5.0, *)
func MusicPlayerSetPlayRateScalar(_ inPlayer: MusicPlayer, _ inScaleRate: Float64) -> OSStatus
@available(iOS 5.0, *)
func MusicPlayerGetPlayRateScalar(_ inPlayer: MusicPlayer, _ outScaleRate: UnsafeMutablePointer<Float64>) -> OSStatus
@available(iOS 5.0, *)
func NewMusicSequence(_ outSequence: UnsafeMutablePointer<MusicSequence>) -> OSStatus
@available(iOS 5.0, *)
func DisposeMusicSequence(_ inSequence: MusicSequence) -> OSStatus
@available(iOS 5.0, *)
func MusicSequenceNewTrack(_ inSequence: MusicSequence, _ outTrack: UnsafeMutablePointer<MusicTrack>) -> OSStatus
@available(iOS 5.0, *)
func MusicSequenceDisposeTrack(_ inSequence: MusicSequence, _ inTrack: MusicTrack) -> OSStatus
@available(iOS 5.0, *)
func MusicSequenceGetTrackCount(_ inSequence: MusicSequence, _ outNumberOfTracks: UnsafeMutablePointer<UInt32>) -> OSStatus
@available(iOS 5.0, *)
func MusicSequenceGetIndTrack(_ inSequence: MusicSequence, _ inTrackIndex: UInt32, _ outTrack: UnsafeMutablePointer<MusicTrack>) -> OSStatus
@available(iOS 5.0, *)
func MusicSequenceGetTrackIndex(_ inSequence: MusicSequence, _ inTrack: MusicTrack, _ outTrackIndex: UnsafeMutablePointer<UInt32>) -> OSStatus
@available(iOS 5.0, *)
func MusicSequenceGetTempoTrack(_ inSequence: MusicSequence, _ outTrack: UnsafeMutablePointer<MusicTrack>) -> OSStatus
@available(iOS 5.0, *)
func MusicSequenceSetAUGraph(_ inSequence: MusicSequence, _ inGraph: AUGraph) -> OSStatus
@available(iOS 5.0, *)
func MusicSequenceGetAUGraph(_ inSequence: MusicSequence, _ outGraph: UnsafeMutablePointer<AUGraph>) -> OSStatus
@available(iOS 5.0, *)
func MusicSequenceSetMIDIEndpoint(_ inSequence: MusicSequence, _ inEndpoint: MIDIEndpointRef) -> OSStatus
@available(iOS 5.0, *)
func MusicSequenceSetSequenceType(_ inSequence: MusicSequence, _ inType: MusicSequenceType) -> OSStatus
@available(iOS 5.0, *)
func MusicSequenceGetSequenceType(_ inSequence: MusicSequence, _ outType: UnsafeMutablePointer<MusicSequenceType>) -> OSStatus
@available(iOS 5.0, *)
func MusicSequenceFileLoad(_ inSequence: MusicSequence, _ inFileRef: CFURL, _ inFileTypeHint: MusicSequenceFileTypeID, _ inFlags: MusicSequenceLoadFlags) -> OSStatus
@available(iOS 5.0, *)
func MusicSequenceFileLoadData(_ inSequence: MusicSequence, _ inData: CFData, _ inFileTypeHint: MusicSequenceFileTypeID, _ inFlags: MusicSequenceLoadFlags) -> OSStatus
func MusicSequenceSetSMPTEResolution(_ fps: Int8, _ ticks: UInt8) -> Int16
func MusicSequenceGetSMPTEResolution(_ inRes: Int16, _ fps: UnsafeMutablePointer<Int8>, _ ticks: UnsafeMutablePointer<UInt8>)
@available(iOS 5.0, *)
func MusicSequenceFileCreate(_ inSequence: MusicSequence, _ inFileRef: CFURL, _ inFileType: MusicSequenceFileTypeID, _ inFlags: MusicSequenceFileFlags, _ inResolution: Int16) -> OSStatus
@available(iOS 5.0, *)
func MusicSequenceFileCreateData(_ inSequence: MusicSequence, _ inFileType: MusicSequenceFileTypeID, _ inFlags: MusicSequenceFileFlags, _ inResolution: Int16, _ outData: UnsafeMutablePointer<Unmanaged<CFData>?>) -> OSStatus
@available(iOS 5.0, *)
func MusicSequenceReverse(_ inSequence: MusicSequence) -> OSStatus
@available(iOS 5.0, *)
func MusicSequenceGetSecondsForBeats(_ inSequence: MusicSequence, _ inBeats: MusicTimeStamp, _ outSeconds: UnsafeMutablePointer<Float64>) -> OSStatus
@available(iOS 5.0, *)
func MusicSequenceGetBeatsForSeconds(_ inSequence: MusicSequence, _ inSeconds: Float64, _ outBeats: UnsafeMutablePointer<MusicTimeStamp>) -> OSStatus
@available(iOS 5.0, *)
func MusicSequenceSetUserCallback(_ inSequence: MusicSequence, _ inCallback: MusicSequenceUserCallback?, _ inClientData: UnsafeMutablePointer<Void>) -> OSStatus
@available(iOS 5.0, *)
func MusicSequenceBeatsToBarBeatTime(_ inSequence: MusicSequence, _ inBeats: MusicTimeStamp, _ inSubbeatDivisor: UInt32, _ outBarBeatTime: UnsafeMutablePointer<CABarBeatTime>) -> OSStatus
@available(iOS 5.0, *)
func MusicSequenceBarBeatTimeToBeats(_ inSequence: MusicSequence, _ inBarBeatTime: UnsafePointer<CABarBeatTime>, _ outBeats: UnsafeMutablePointer<MusicTimeStamp>) -> OSStatus
@available(iOS 5.0, *)
func MusicSequenceGetInfoDictionary(_ inSequence: MusicSequence) -> CFDictionary
@available(iOS 5.0, *)
func MusicTrackGetSequence(_ inTrack: MusicTrack, _ outSequence: UnsafeMutablePointer<MusicSequence>) -> OSStatus
@available(iOS 5.0, *)
func MusicTrackSetDestNode(_ inTrack: MusicTrack, _ inNode: AUNode) -> OSStatus
@available(iOS 5.0, *)
func MusicTrackSetDestMIDIEndpoint(_ inTrack: MusicTrack, _ inEndpoint: MIDIEndpointRef) -> OSStatus
@available(iOS 5.0, *)
func MusicTrackGetDestNode(_ inTrack: MusicTrack, _ outNode: UnsafeMutablePointer<AUNode>) -> OSStatus
@available(iOS 5.0, *)
func MusicTrackGetDestMIDIEndpoint(_ inTrack: MusicTrack, _ outEndpoint: UnsafeMutablePointer<MIDIEndpointRef>) -> OSStatus
@available(iOS 5.0, *)
func MusicTrackSetProperty(_ inTrack: MusicTrack, _ inPropertyID: UInt32, _ inData: UnsafeMutablePointer<Void>, _ inLength: UInt32) -> OSStatus
@available(iOS 5.0, *)
func MusicTrackGetProperty(_ inTrack: MusicTrack, _ inPropertyID: UInt32, _ outData: UnsafeMutablePointer<Void>, _ ioLength: UnsafeMutablePointer<UInt32>) -> OSStatus
@available(iOS 5.0, *)
func MusicTrackMoveEvents(_ inTrack: MusicTrack, _ inStartTime: MusicTimeStamp, _ inEndTime: MusicTimeStamp, _ inMoveTime: MusicTimeStamp) -> OSStatus
@available(iOS 5.0, *)
func MusicTrackClear(_ inTrack: MusicTrack, _ inStartTime: MusicTimeStamp, _ inEndTime: MusicTimeStamp) -> OSStatus
@available(iOS 5.0, *)
func MusicTrackCut(_ inTrack: MusicTrack, _ inStartTime: MusicTimeStamp, _ inEndTime: MusicTimeStamp) -> OSStatus
@available(iOS 5.0, *)
func MusicTrackCopyInsert(_ inSourceTrack: MusicTrack, _ inSourceStartTime: MusicTimeStamp, _ inSourceEndTime: MusicTimeStamp, _ inDestTrack: MusicTrack, _ inDestInsertTime: MusicTimeStamp) -> OSStatus
@available(iOS 5.0, *)
func MusicTrackMerge(_ inSourceTrack: MusicTrack, _ inSourceStartTime: MusicTimeStamp, _ inSourceEndTime: MusicTimeStamp, _ inDestTrack: MusicTrack, _ inDestInsertTime: MusicTimeStamp) -> OSStatus
@available(iOS 5.0, *)
func MusicTrackNewMIDINoteEvent(_ inTrack: MusicTrack, _ inTimeStamp: MusicTimeStamp, _ inMessage: UnsafePointer<MIDINoteMessage>) -> OSStatus
@available(iOS 5.0, *)
func MusicTrackNewMIDIChannelEvent(_ inTrack: MusicTrack, _ inTimeStamp: MusicTimeStamp, _ inMessage: UnsafePointer<MIDIChannelMessage>) -> OSStatus
@available(iOS 5.0, *)
func MusicTrackNewMIDIRawDataEvent(_ inTrack: MusicTrack, _ inTimeStamp: MusicTimeStamp, _ inRawData: UnsafePointer<MIDIRawData>) -> OSStatus
@available(iOS 5.0, *)
func MusicTrackNewExtendedNoteEvent(_ inTrack: MusicTrack, _ inTimeStamp: MusicTimeStamp, _ inInfo: UnsafePointer<ExtendedNoteOnEvent>) -> OSStatus
@available(iOS 5.0, *)
func MusicTrackNewParameterEvent(_ inTrack: MusicTrack, _ inTimeStamp: MusicTimeStamp, _ inInfo: UnsafePointer<ParameterEvent>) -> OSStatus
@available(iOS 5.0, *)
func MusicTrackNewExtendedTempoEvent(_ inTrack: MusicTrack, _ inTimeStamp: MusicTimeStamp, _ inBPM: Float64) -> OSStatus
@available(iOS 5.0, *)
func MusicTrackNewMetaEvent(_ inTrack: MusicTrack, _ inTimeStamp: MusicTimeStamp, _ inMetaEvent: UnsafePointer<MIDIMetaEvent>) -> OSStatus
@available(iOS 5.0, *)
func MusicTrackNewUserEvent(_ inTrack: MusicTrack, _ inTimeStamp: MusicTimeStamp, _ inUserData: UnsafePointer<MusicEventUserData>) -> OSStatus
@available(iOS 5.0, *)
func MusicTrackNewAUPresetEvent(_ inTrack: MusicTrack, _ inTimeStamp: MusicTimeStamp, _ inPresetEvent: UnsafePointer<AUPresetEvent>) -> OSStatus
@available(iOS 5.0, *)
func NewMusicEventIterator(_ inTrack: MusicTrack, _ outIterator: UnsafeMutablePointer<MusicEventIterator>) -> OSStatus
@available(iOS 5.0, *)
func DisposeMusicEventIterator(_ inIterator: MusicEventIterator) -> OSStatus
@available(iOS 5.0, *)
func MusicEventIteratorSeek(_ inIterator: MusicEventIterator, _ inTimeStamp: MusicTimeStamp) -> OSStatus
@available(iOS 5.0, *)
func MusicEventIteratorNextEvent(_ inIterator: MusicEventIterator) -> OSStatus
@available(iOS 5.0, *)
func MusicEventIteratorPreviousEvent(_ inIterator: MusicEventIterator) -> OSStatus
@available(iOS 5.0, *)
func MusicEventIteratorGetEventInfo(_ inIterator: MusicEventIterator, _ outTimeStamp: UnsafeMutablePointer<MusicTimeStamp>, _ outEventType: UnsafeMutablePointer<MusicEventType>, _ outEventData: UnsafeMutablePointer<UnsafePointer<Void>>, _ outEventDataSize: UnsafeMutablePointer<UInt32>) -> OSStatus
@available(iOS 5.0, *)
func MusicEventIteratorSetEventInfo(_ inIterator: MusicEventIterator, _ inEventType: MusicEventType, _ inEventData: UnsafePointer<Void>) -> OSStatus
@available(iOS 5.0, *)
func MusicEventIteratorSetEventTime(_ inIterator: MusicEventIterator, _ inTimeStamp: MusicTimeStamp) -> OSStatus
@available(iOS 5.0, *)
func MusicEventIteratorDeleteEvent(_ inIterator: MusicEventIterator) -> OSStatus
@available(iOS 5.0, *)
func MusicEventIteratorHasPreviousEvent(_ inIterator: MusicEventIterator, _ outHasPrevEvent: UnsafeMutablePointer<DarwinBoolean>) -> OSStatus
@available(iOS 5.0, *)
func MusicEventIteratorHasNextEvent(_ inIterator: MusicEventIterator, _ outHasNextEvent: UnsafeMutablePointer<DarwinBoolean>) -> OSStatus
@available(iOS 5.0, *)
func MusicEventIteratorHasCurrentEvent(_ inIterator: MusicEventIterator, _ outHasCurEvent: UnsafeMutablePointer<DarwinBoolean>) -> OSStatus
