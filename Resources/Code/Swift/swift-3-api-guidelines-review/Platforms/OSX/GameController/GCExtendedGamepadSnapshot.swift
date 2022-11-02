
@available(OSX 10.9, *)
class GCExtendedGamepadSnapshot : GCExtendedGamepad {
  @NSCopying var snapshotData: NSData
  init(snapshotData data: NSData)
  init(controller controller: GCController, snapshotData data: NSData)
}
struct GCExtendedGamepadSnapShotDataV100 {
  var version: UInt16
  var size: UInt16
  var dpadX: Float
  var dpadY: Float
  var buttonA: Float
  var buttonB: Float
  var buttonX: Float
  var buttonY: Float
  var leftShoulder: Float
  var rightShoulder: Float
  var leftThumbstickX: Float
  var leftThumbstickY: Float
  var rightThumbstickX: Float
  var rightThumbstickY: Float
  var leftTrigger: Float
  var rightTrigger: Float
  init()
  init(version version: UInt16, size size: UInt16, dpadX dpadX: Float, dpadY dpadY: Float, buttonA buttonA: Float, buttonB buttonB: Float, buttonX buttonX: Float, buttonY buttonY: Float, leftShoulder leftShoulder: Float, rightShoulder rightShoulder: Float, leftThumbstickX leftThumbstickX: Float, leftThumbstickY leftThumbstickY: Float, rightThumbstickX rightThumbstickX: Float, rightThumbstickY rightThumbstickY: Float, leftTrigger leftTrigger: Float, rightTrigger rightTrigger: Float)
}
@available(OSX 10.9, *)
func GCExtendedGamepadSnapShotDataV100FromNSData(_ snapshotData: UnsafeMutablePointer<GCExtendedGamepadSnapShotDataV100>, _ data: NSData?) -> Bool
@available(OSX 10.9, *)
func NSDataFromGCExtendedGamepadSnapShotDataV100(_ snapshotData: UnsafeMutablePointer<GCExtendedGamepadSnapShotDataV100>) -> NSData?
