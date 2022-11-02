//===----------------------------------------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2020 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

@_exported import Foundation // Clang module


/**
 `Notification` encapsulates information broadcast to observers via a `NotificationCenter`.
*/
public struct Notification : ReferenceConvertible, Equatable, Hashable {
    public typealias ReferenceType = NSNotification
    
    /// A tag identifying the notification.
    public var name: Name
    
    /// An object that the poster wishes to send to observers.
    ///
    /// Typically this is the object that posted the notification.
    public var object: Any?
    
    /// Storage for values or objects related to this notification.
    public var userInfo: [AnyHashable : Any]?
    
    /// Initialize a new `Notification`.
    ///
    /// The default value for `userInfo` is nil.
    public init(name: Name, object: Any? = nil, userInfo: [AnyHashable : Any]? = nil) {
        self.name = name
        self.object = object
        self.userInfo = userInfo
    }

    public func hash(into hasher: inout Hasher) {
        hasher.combine(name)

        // FIXME: We should feed `object` to the hasher, too. However, this
        // cannot be safely done if its value happens not to be Hashable.

        // FIXME: == compares `userInfo` by bridging it to NSDictionary, so its
        // contents should be fully hashed here.  However, to prevent stability
        // issues with userInfo dictionaries that include non-Hashable values,
        // we only feed the keys to the hasher here.
        //
        // FIXME: This makes for weak hashes. We really should hash everything
        // that's compared in ==.
        //
        // The algorithm below is the same as the one used by Dictionary.
        var commutativeKeyHash = 0
        if let userInfo = userInfo {
            for (k, _) in userInfo {
                var nestedHasher = hasher
                nestedHasher.combine(k)
                commutativeKeyHash ^= nestedHasher.finalize()
            }
        }
        hasher.combine(commutativeKeyHash)
    }

    public var description: String {
        return "name = \(name.rawValue), object = \(String(describing: object)), userInfo = \(String(describing: userInfo))"
    }
    
    public var debugDescription: String {
        return description
    }

    // FIXME: Handle directly via API Notes
    public typealias Name = NSNotification.Name

    /// Compare two notifications for equality.
    public static func ==(lhs: Notification, rhs: Notification) -> Bool {
        if lhs.name.rawValue != rhs.name.rawValue {
            return false
        }
        if let lhsObj = lhs.object {
            if let rhsObj = rhs.object {
                // FIXME: Converting an arbitrary value to AnyObject won't use a
                // stable address when the value needs to be boxed.  Therefore,
                // this comparison makes == non-reflexive, violating Equatable
                // requirements. (rdar://problem/49797185)
                if lhsObj as AnyObject !== rhsObj as AnyObject {
                    return false
                }
            } else {
                return false
            }
        } else if rhs.object != nil {
            return false
        }
        if lhs.userInfo != nil {
            if rhs.userInfo != nil {
                // user info must be compared in the object form since the userInfo in swift is not comparable

                // FIXME: This violates reflexivity when userInfo contains any
                // non-Hashable values, for the same reason as described above.
                return lhs._bridgeToObjectiveC() == rhs._bridgeToObjectiveC()
            } else {
                return false
            }
        } else if rhs.userInfo != nil {
            return false
        }
        return true
    }
}

extension Notification: CustomReflectable {
    public var customMirror: Mirror {
        var children: [(label: String?, value: Any)] = []
        children.append((label: "name", value: self.name.rawValue))
        if let o = self.object {
            children.append((label: "object", value: o))
        }
        if let u = self.userInfo {
            children.append((label: "userInfo", value: u))
        }
        let m = Mirror(self, children:children, displayStyle: Mirror.DisplayStyle.class)
        return m
    }
}

extension Notification : _ObjectiveCBridgeable {
    public static func _getObjectiveCType() -> Any.Type {
        return NSNotification.self
    }
    
    @_semantics("convertToObjectiveC")
    public func _bridgeToObjectiveC() -> NSNotification {
        return NSNotification(name: name, object: object, userInfo: userInfo)
    }
    
    public static func _forceBridgeFromObjectiveC(_ x: NSNotification, result: inout Notification?) {
        if !_conditionallyBridgeFromObjectiveC(x, result: &result) {
            fatalError("Unable to bridge type")
        }
    }
    
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSNotification, result: inout Notification?) -> Bool {
        result = Notification(name: x.name, object: x.object, userInfo: x.userInfo)
        return true
    }

    @_effects(readonly)
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSNotification?) -> Notification {
        guard let src = source else { return Notification(name: Notification.Name("")) }
        return Notification(name: src.name, object: src.object, userInfo: src.userInfo)
    }
}

extension NSNotification : _HasCustomAnyHashableRepresentation {
    // Must be @nonobjc to avoid infinite recursion during bridging.
    @nonobjc
    public func _toCustomAnyHashable() -> AnyHashable? {
        return AnyHashable(self as Notification)
    }
}

