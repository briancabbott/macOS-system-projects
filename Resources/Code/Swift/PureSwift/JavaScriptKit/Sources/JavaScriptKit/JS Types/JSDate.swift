//
//  JSDate.swift
//  JavaScriptKit
//
//  Created by Alsey Coleman Miller on 6/4/20.
//

/**
 JavaScript Date
 
 A JavaScript date is fundamentally specified as the number of milliseconds that have elapsed since midnight on January 1, 1970, UTC. This date and time is the same as the UNIX epoch, which is the predominant base value for computer-recorded date and time values.
 */
public final class JSDate: JSType {

    // MARK: - Properties

    public let jsObject: JSObjectRef

    // MARK: - Initialization

    public init?(_ jsObject: JSObjectRef) {
        self.jsObject = jsObject
    }
    
    /**
     Creates a JavaScript Date instance that represents a single moment in time in a platform-independent format. Date objects contain a Number that represents milliseconds since 1 January 1970 UTC.
     
     When no parameters are provided, the newly-created Date object represents the current date and time as of the time of instantiation.
     */
    public init() {
        self.jsObject = Self.classObject.new()
    }
    
    /**
     Creates a JavaScript Date instance that represents a single moment in time in a platform-independent format. Date objects contain a Number that represents milliseconds since 1 January 1970 UTC.
     
     - Parameter timeInterval: An integer value representing the number of milliseconds since January 1, 1970, 00:00:00 UTC (the ECMAScript epoch, equivalent to the UNIX epoch), with leap seconds ignored. Keep in mind that most UNIX Timestamp functions are only accurate to the nearest second.
     */
    public init(timeInterval: Double) {
        self.jsObject = Self.classObject.new(timeInterval)
    }
    
    /**
     Creates a JavaScript Date instance that represents a single moment in time in a platform-independent format. Date objects contain a Number that represents milliseconds since 1 January 1970 UTC.
     
     - Parameter _string: A string value representing a date, specified in a format recognized by the Date.parse() method. (These formats are IETF-compliant RFC 2822 timestamps, and also strings in a version of ISO8601.)
     */
    public init(_ dateString: String) {
        self.jsObject = Self.classObject.new(dateString)
    }
    
    /// The static `Date.now()` method returns the number of milliseconds elapsed since January 1, 1970 00:00:00 UTC.
    ///
    /// - Returns: A Number representing the milliseconds elapsed since the UNIX epoch.
    public static var now: Double {
        let function = classObject.now.function.assert()
        return function().number.assert()
    }
    
    // MARK: - Accessors
    
    /// Converts a date to a string following the ISO 8601 Extended Format.
    public func toISOString() -> String {
        let function = jsObject.toISOString.function.assert()
        return function.apply(this: jsObject).string.assert()
    }
    
    /// Returns a string representing the Date using `toISOString()`. Intended for use by JSON.stringify().
    public func toJSON() -> String {
        let function = jsObject.toJSON.function.assert()
        return function.apply(this: jsObject).string.assert()
    }
    
    /// Converts a date to a string using the UTC timezone.
    public func toUTCString() -> String {
        let function = jsObject.toUTCString.function.assert()
        return function.apply(this: jsObject).string.assert()
    }
}

// MARK: - Constants

internal extension JSDate {
    
    static let classObject = JSObjectRef.global.Date.function!
}

// MARK: - RawRepresentable

extension JSDate: RawRepresentable {
        
    public convenience init(rawValue: Double) {
        self.init(timeInterval: rawValue)
    }
    
    public var rawValue: Double {
        let function = jsObject.valueOf.function.assert("Invalid function \(#function)")
        return function.apply(this: jsObject).number.assert()
    }
}

// MARK: - CustomStringConvertible

extension JSDate: CustomStringConvertible { }
