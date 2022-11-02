
@available(OSX 10.9, *)
class GCGamepad : NSObject {
  weak var controller: @sil_weak GCController? { get }
  var valueChangedHandler: GCGamepadValueChangedHandler?
  func saveSnapshot() -> GCGamepadSnapshot
  var dpad: GCControllerDirectionPad { get }
  var buttonA: GCControllerButtonInput { get }
  var buttonB: GCControllerButtonInput { get }
  var buttonX: GCControllerButtonInput { get }
  var buttonY: GCControllerButtonInput { get }
  var leftShoulder: GCControllerButtonInput { get }
  var rightShoulder: GCControllerButtonInput { get }
}
typealias GCGamepadValueChangedHandler = (GCGamepad, GCControllerElement) -> Void
