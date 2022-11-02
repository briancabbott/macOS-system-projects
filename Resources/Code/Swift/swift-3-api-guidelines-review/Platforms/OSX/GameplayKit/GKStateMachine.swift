
@available(OSX 10.11, *)
class GKStateMachine : NSObject {
  var currentState: GKState? { get }
  init(states states: [GKState])
  func update(withDeltaTime sec: NSTimeInterval)
  func canEnterState(_ stateClass: AnyClass) -> Bool
  func enterState(_ stateClass: AnyClass) -> Bool
}

@available(iOS 9.0, OSX 10.11, tvOS 9.0, *)
extension GKStateMachine {
  @warn_unused_result
  func stateForClass<StateType : GKState>(_ stateClass: StateType.Type) -> StateType?
}
