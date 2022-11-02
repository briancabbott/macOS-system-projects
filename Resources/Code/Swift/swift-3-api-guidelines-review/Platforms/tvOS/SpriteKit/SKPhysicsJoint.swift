
class SKPhysicsJoint : NSObject, NSCoding {
  var bodyA: SKPhysicsBody
  var bodyB: SKPhysicsBody
  var reactionForce: CGVector { get }
  var reactionTorque: CGFloat { get }
  func encode(with aCoder: NSCoder)
  init?(coder aDecoder: NSCoder)
}
class SKPhysicsJointPin : SKPhysicsJoint {
  class func joint(withBodyA bodyA: SKPhysicsBody, bodyB bodyB: SKPhysicsBody, anchor anchor: CGPoint) -> SKPhysicsJointPin
  var shouldEnableLimits: Bool
  var lowerAngleLimit: CGFloat
  var upperAngleLimit: CGFloat
  var frictionTorque: CGFloat
  var rotationSpeed: CGFloat
}
class SKPhysicsJointSpring : SKPhysicsJoint {
  class func joint(withBodyA bodyA: SKPhysicsBody, bodyB bodyB: SKPhysicsBody, anchorA anchorA: CGPoint, anchorB anchorB: CGPoint) -> SKPhysicsJointSpring
  var damping: CGFloat
  var frequency: CGFloat
}
class SKPhysicsJointFixed : SKPhysicsJoint {
  class func joint(withBodyA bodyA: SKPhysicsBody, bodyB bodyB: SKPhysicsBody, anchor anchor: CGPoint) -> SKPhysicsJointFixed
}
class SKPhysicsJointSliding : SKPhysicsJoint {
  class func joint(withBodyA bodyA: SKPhysicsBody, bodyB bodyB: SKPhysicsBody, anchor anchor: CGPoint, axis axis: CGVector) -> SKPhysicsJointSliding
  var shouldEnableLimits: Bool
  var lowerDistanceLimit: CGFloat
  var upperDistanceLimit: CGFloat
}
class SKPhysicsJointLimit : SKPhysicsJoint {
  var maxLength: CGFloat
  class func joint(withBodyA bodyA: SKPhysicsBody, bodyB bodyB: SKPhysicsBody, anchorA anchorA: CGPoint, anchorB anchorB: CGPoint) -> SKPhysicsJointLimit
}
