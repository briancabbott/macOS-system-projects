//
//  ClassName.swift
//  JNI
//
//  Created by Alsey Coleman Miller on 3/22/18.
//  Copyright © 2018 PureSwift. All rights reserved.
//

public struct JNIClassName {
    
    public let package: JNIPackage
    
    public let metaClass: JNIMetaClass
}

// MARK: - Equatable

extension JNIClassName: Equatable {
    
    public static func == (lhs: JNIClassName, rhs: JNIClassName) -> Bool {
        
        return lhs.package == rhs.package
            && lhs.metaClass == rhs.metaClass
    }
}

// MARK: - Hashable

extension JNIClassName: Hashable {
    
    public var hashValue: Int {
        
        return rawValue.hashValue
    }
}

// MARK: - RawRepresentable

extension JNIClassName: RawRepresentable {
    
    public init?(rawValue: String) {
        
        let elements = rawValue
            .split(separator: JNIPackage.separator, maxSplits: .max, omittingEmptySubsequences: true)
            .map { String($0) }
        
        guard elements.count >= 2,
            let className = elements.last
            else { return nil }
        
        let packageElements = Array(elements.dropLast())
        
        guard let package = JNIPackage(elements: packageElements),
            let metaClass = JNIMetaClass(rawValue: className)
            else { return nil }
        
        self.package = package
        self.metaClass = metaClass
    }
    
    public var rawValue: String {
        
        return package.rawValue + "/" + metaClass.rawValue
    }
}

// MARK: - CustomStringConvertible

extension JNIClassName: CustomStringConvertible {
    
    public var description: String {
        
        return rawValue
    }
}

// MARK: - Extensions

infix operator ☕️

public extension JNIPackage {
    
    static func ☕️ (package: JNIPackage, metaClass: JNIMetaClass) -> JNIClassName {
        
        return JNIClassName(package: package, metaClass: metaClass)
    }
    
    static func ☕️ (package: JNIPackage, metaClassName: String) -> JNIClassName? {
        
        guard let metaClass = JNIMetaClass(rawValue: metaClassName)
            else { return nil }
        
        return JNIClassName(package: package, metaClass: metaClass)
    }
}
