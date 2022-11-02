
typealias AUGraph = COpaquePointer
typealias AUNode = Int32
var kAUGraphErr_NodeNotFound: OSStatus { get }
var kAUGraphErr_InvalidConnection: OSStatus { get }
var kAUGraphErr_OutputNodeErr: OSStatus { get }
var kAUGraphErr_CannotDoInCurrentContext: OSStatus { get }
var kAUGraphErr_InvalidAudioUnit: OSStatus { get }
@available(OSX 10.0, *)
func NewAUGraph(_ outGraph: UnsafeMutablePointer<AUGraph>) -> OSStatus
@available(OSX 10.0, *)
func DisposeAUGraph(_ inGraph: AUGraph) -> OSStatus
@available(OSX 10.5, *)
func AUGraphAddNode(_ inGraph: AUGraph, _ inDescription: UnsafePointer<AudioComponentDescription>, _ outNode: UnsafeMutablePointer<AUNode>) -> OSStatus
@available(OSX 10.0, *)
func AUGraphRemoveNode(_ inGraph: AUGraph, _ inNode: AUNode) -> OSStatus
@available(OSX 10.0, *)
func AUGraphGetNodeCount(_ inGraph: AUGraph, _ outNumberOfNodes: UnsafeMutablePointer<UInt32>) -> OSStatus
@available(OSX 10.0, *)
func AUGraphGetIndNode(_ inGraph: AUGraph, _ inIndex: UInt32, _ outNode: UnsafeMutablePointer<AUNode>) -> OSStatus
@available(OSX 10.5, *)
func AUGraphNodeInfo(_ inGraph: AUGraph, _ inNode: AUNode, _ outDescription: UnsafeMutablePointer<AudioComponentDescription>, _ outAudioUnit: UnsafeMutablePointer<AudioUnit>) -> OSStatus
@available(OSX 10.2, *)
func AUGraphNewNodeSubGraph(_ inGraph: AUGraph, _ outNode: UnsafeMutablePointer<AUNode>) -> OSStatus
@available(OSX 10.2, *)
func AUGraphGetNodeInfoSubGraph(_ inGraph: AUGraph, _ inNode: AUNode, _ outSubGraph: UnsafeMutablePointer<AUGraph>) -> OSStatus
@available(OSX 10.2, *)
func AUGraphIsNodeSubGraph(_ inGraph: AUGraph, _ inNode: AUNode, _ outFlag: UnsafeMutablePointer<DarwinBoolean>) -> OSStatus
var kAUNodeInteraction_Connection: UInt32 { get }
var kAUNodeInteraction_InputCallback: UInt32 { get }
struct AudioUnitNodeConnection {
  var sourceNode: AUNode
  var sourceOutputNumber: UInt32
  var destNode: AUNode
  var destInputNumber: UInt32
  init()
  init(sourceNode sourceNode: AUNode, sourceOutputNumber sourceOutputNumber: UInt32, destNode destNode: AUNode, destInputNumber destInputNumber: UInt32)
}
typealias AUNodeConnection = AudioUnitNodeConnection
struct AUNodeRenderCallback {
  var destNode: AUNode
  var destInputNumber: AudioUnitElement
  var cback: AURenderCallbackStruct
  init()
  init(destNode destNode: AUNode, destInputNumber destInputNumber: AudioUnitElement, cback cback: AURenderCallbackStruct)
}
struct AUNodeInteraction {
  struct __Unnamed_union_nodeInteraction {
    var connection: AUNodeConnection
    var inputCallback: AUNodeRenderCallback
    init(connection connection: AUNodeConnection)
    init(inputCallback inputCallback: AUNodeRenderCallback)
    init()
  }
  var nodeInteractionType: UInt32
  var nodeInteraction: AUNodeInteraction.__Unnamed_union_nodeInteraction
  init()
  init(nodeInteractionType nodeInteractionType: UInt32, nodeInteraction nodeInteraction: AUNodeInteraction.__Unnamed_union_nodeInteraction)
}
@available(OSX 10.0, *)
func AUGraphConnectNodeInput(_ inGraph: AUGraph, _ inSourceNode: AUNode, _ inSourceOutputNumber: UInt32, _ inDestNode: AUNode, _ inDestInputNumber: UInt32) -> OSStatus
@available(OSX 10.5, *)
func AUGraphSetNodeInputCallback(_ inGraph: AUGraph, _ inDestNode: AUNode, _ inDestInputNumber: UInt32, _ inInputCallback: UnsafePointer<AURenderCallbackStruct>) -> OSStatus
@available(OSX 10.0, *)
func AUGraphDisconnectNodeInput(_ inGraph: AUGraph, _ inDestNode: AUNode, _ inDestInputNumber: UInt32) -> OSStatus
@available(OSX 10.0, *)
func AUGraphClearConnections(_ inGraph: AUGraph) -> OSStatus
@available(OSX 10.5, *)
func AUGraphGetNumberOfInteractions(_ inGraph: AUGraph, _ outNumInteractions: UnsafeMutablePointer<UInt32>) -> OSStatus
@available(OSX 10.5, *)
func AUGraphGetInteractionInfo(_ inGraph: AUGraph, _ inInteractionIndex: UInt32, _ outInteraction: UnsafeMutablePointer<AUNodeInteraction>) -> OSStatus
@available(OSX 10.5, *)
func AUGraphCountNodeInteractions(_ inGraph: AUGraph, _ inNode: AUNode, _ outNumInteractions: UnsafeMutablePointer<UInt32>) -> OSStatus
@available(OSX 10.5, *)
func AUGraphGetNodeInteractions(_ inGraph: AUGraph, _ inNode: AUNode, _ ioNumInteractions: UnsafeMutablePointer<UInt32>, _ outInteractions: UnsafeMutablePointer<AUNodeInteraction>) -> OSStatus
@available(OSX 10.0, *)
func AUGraphUpdate(_ inGraph: AUGraph, _ outIsUpdated: UnsafeMutablePointer<DarwinBoolean>) -> OSStatus
@available(OSX 10.0, *)
func AUGraphOpen(_ inGraph: AUGraph) -> OSStatus
@available(OSX 10.0, *)
func AUGraphClose(_ inGraph: AUGraph) -> OSStatus
@available(OSX 10.0, *)
func AUGraphInitialize(_ inGraph: AUGraph) -> OSStatus
@available(OSX 10.0, *)
func AUGraphUninitialize(_ inGraph: AUGraph) -> OSStatus
@available(OSX 10.0, *)
func AUGraphStart(_ inGraph: AUGraph) -> OSStatus
@available(OSX 10.0, *)
func AUGraphStop(_ inGraph: AUGraph) -> OSStatus
@available(OSX 10.0, *)
func AUGraphIsOpen(_ inGraph: AUGraph, _ outIsOpen: UnsafeMutablePointer<DarwinBoolean>) -> OSStatus
@available(OSX 10.0, *)
func AUGraphIsInitialized(_ inGraph: AUGraph, _ outIsInitialized: UnsafeMutablePointer<DarwinBoolean>) -> OSStatus
@available(OSX 10.0, *)
func AUGraphIsRunning(_ inGraph: AUGraph, _ outIsRunning: UnsafeMutablePointer<DarwinBoolean>) -> OSStatus
@available(OSX 10.1, *)
func AUGraphGetCPULoad(_ inGraph: AUGraph, _ outAverageCPULoad: UnsafeMutablePointer<Float32>) -> OSStatus
@available(OSX 10.3, *)
func AUGraphGetMaxCPULoad(_ inGraph: AUGraph, _ outMaxLoad: UnsafeMutablePointer<Float32>) -> OSStatus
@available(OSX 10.2, *)
func AUGraphAddRenderNotify(_ inGraph: AUGraph, _ inCallback: AURenderCallback, _ inRefCon: UnsafeMutablePointer<Void>) -> OSStatus
@available(OSX 10.2, *)
func AUGraphRemoveRenderNotify(_ inGraph: AUGraph, _ inCallback: AURenderCallback, _ inRefCon: UnsafeMutablePointer<Void>) -> OSStatus
