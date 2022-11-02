//
//  JNIPackage.swift
//  JNI
//
//  Created by Alsey Coleman Miller on 3/22/18.
//  Copyright © 2018 PureSwift. All rights reserved.
//

public struct JNIPackage: JNIClassNameComponent {
    
    public static let separator: Character = "/"
    
    public let elements: [String]
    
    public init?(elements: [String]) {
        
        guard elements.isEmpty == false
            else { return nil }
        
        self.elements = elements
    }
    
    fileprivate init(_ elements: [String]) {
        
        assert(elements.isEmpty == false)
        
        self.elements = elements
    }
}

// MARK: - RandomAccessCollection

public extension JNIPackage {
    
    subscript(bounds: Range<Int>) -> Slice<JNIPackage> {
        
        return Slice<JNIPackage>(base: self, bounds: bounds)
    }
}

// MARK: - Extensions

public extension JNIPackage {
    
    static var java: JNIPackage {
        
        return ["java"]
    }
    
    static func java(_ elements: String...) -> JNIPackage {
        
        return JNIPackage(JNIPackage.java.elements + elements)
    }
    
    static func + (lhs: JNIPackage, rhs: JNIPackage) -> JNIPackage {
        
        return JNIPackage(lhs.elements + rhs.elements)
    }
    
    static func + (lhs: JNIPackage, rhs: [String]) -> JNIPackage {
        
        return JNIPackage(lhs.elements + rhs)
    }
}
