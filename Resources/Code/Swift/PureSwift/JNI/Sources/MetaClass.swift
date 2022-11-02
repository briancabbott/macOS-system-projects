//
//  JNIClassSignature.swift
//  JNI
//
//  Created by Alsey Coleman Miller on 3/19/18.
//  Copyright © 2018 PureSwift. All rights reserved.
//

public struct JNIMetaClass: JNIClassNameComponent {
    
    public static let separator: Character = "$"
    
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

public extension JNIMetaClass {
    
    subscript(bounds: Range<Int>) -> Slice<JNIMetaClass> {
        
        return Slice<JNIMetaClass>(base: self, bounds: bounds)
    }
}
