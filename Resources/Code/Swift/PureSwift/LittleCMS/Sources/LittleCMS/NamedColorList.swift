//
//  NamedColorList.swift
//  LittleCMS
//
//  Created by Alsey Coleman Miller on 6/3/17.
//
//

import struct Foundation.Data
import CLCMS

/// Specialized collection for dealing with named color profiles.
public struct NamedColorList {
    
    // MARK: - Properties
    
    internal private(set) var internalReference: CopyOnWrite<Reference>
    
    // MARK: - Initialization
    
    internal init(_ internalReference: Reference) {
        
        self.internalReference = CopyOnWrite(internalReference)
    }
    
    /// Allocates an empty named color dictionary.
    public init?(count: Int,
          colorantCount: Int,
          prefix: String,
          suffix: String,
          context: Context? = nil) {
        
        guard let internalReference = Reference(count: count,
                                                colorantCount: colorantCount,
                                                prefix: prefix,
                                                suffix: suffix,
                                                context: context)
            else { return nil }
        
        self.internalReference = CopyOnWrite(internalReference)
    }
    
    /// Retrieve a named color list from a given color transform.
    init?(transform: ColorTransform) {
        
        guard let internalReference = Reference(transform: transform)
            else { return nil }
        
        self.internalReference = CopyOnWrite(internalReference)
    }
    
    // MARK: - Accessors
    
    public var count: Int {
        
        return internalReference.reference.count
    }
    
    // MARK: - Methods
    
    /// Adds a new spot color to the list.
    ///
    /// If the number of elements in the list exceeds the initial storage,
    /// the list is realloc’ed to accommodate things.
    @discardableResult
    public mutating func append(name: String, profileColorSpace pcs: ProfileColorSpace, colorant: Colorant) -> Bool {
        
        return internalReference.mutatingReference.append(name: name, profileColorSpace: pcs, colorant:colorant)
    }
    
    public mutating func append(_ element: Element) {
        
        return internalReference.mutatingReference.append(element)
    }
    
    /// Performs a look-up in the dictionary and returns an index on the given color name.
    public func index(of name: String) -> Int? {
        
        return internalReference.reference.index(of: name)
    }
    
    // MARK: - Subscript
    
    public subscript (name: String) -> Element? {
        
        return internalReference.reference[name]
    }
    
    public subscript (index: Int) -> Element {
        
        return internalReference.reference[index]
    }
}

// MARK: - CustomStringConvertible

extension NamedColorList: CustomStringConvertible {
    
    public var description: String {
        
        /// Print just like an array would
        return "\(Array(self))"
    }
}

/// Reference Type Implementation

internal extension NamedColorList {
    
    /// Specialized collection for dealing with named color profiles.
    internal final class Reference {
        
        // MARK: - Properties
        
        internal let internalPointer: OpaquePointer
        
        // MARK: - Initialization
        
        deinit {
            
            // deallocate profile
            cmsFreeNamedColorList(internalPointer)
        }
        
        @inline(__always)
        internal init(_ internalPointer: OpaquePointer) {
            
            self.internalPointer = internalPointer
        }
        
        /// Allocates an empty named color dictionary.
        @inline(__always)
        init?(count: Int,
                     colorantCount: Int,
                     prefix: String,
                     suffix: String,
                     context: Context? = nil) {
            
            precondition(colorantCount <= Int(cmsMAXCHANNELS), "Invalid colorant count \(colorantCount) >= \(cmsMAXCHANNELS)")
            
            guard let internalPointer = cmsAllocNamedColorList(context?.internalPointer,
                                                               cmsUInt32Number(count),
                                                               cmsUInt32Number(colorantCount),
                                                               prefix, suffix)
                else { return nil }
            
            self.internalPointer = internalPointer
        }
        
        /// Retrieve a named color list from a given color transform.
        @inline(__always)
        init?(transform: ColorTransform) {
            
            guard let internalPointer = cmsGetNamedColorList(transform.internalPointer)
                else { return nil }
            
            self.internalPointer = internalPointer
        }
        
        // MARK: - Accessors
        
        var count: Int {
            
            @inline(__always)
            get { return Int(cmsNamedColorCount(internalPointer)) }
        }
        
        // MARK: - Methods
        
        /// Adds a new spot color to the list.
        ///
        /// If the number of elements in the list exceeds the initial storage,
        /// the list is realloc’ed to accommodate things.
        @discardableResult
        @inline(__always)
        func append(name: String, profileColorSpace pcs: ProfileColorSpace, colorant: Colorant) -> Bool {
            
            var colorant = colorant.rawValue
            
            var pcs = [pcs.0, pcs.1, pcs.2]
            
            precondition(Colorant.validate(colorant), "Invalid Colorant array")
            
            return cmsAppendNamedColor(internalPointer, name, &pcs, &colorant) > 0
        }
        
        @inline(__always)
        func append(_ element: Element) {
            
            guard append(name: element.name, profileColorSpace: element.profileColorSpace, colorant: element.colorant)
                else { fatalError("Could not append element \(element)") }
        }
        
        /// Performs a look-up in the dictionary and returns an index on the given color name.
        @inline(__always)
        func index(of name: String) -> Int? {
            
            // Index on name, or -1 if the spot color is not found.
            let index = Int(cmsNamedColorIndex(internalPointer, name))
            
            guard index != -1 else { return nil }
            
            return index
        }
        
        // MARK: - Subscript
        
        subscript (name: String) -> Element? {
            
            @inline(__always)
            get {
                
                guard let index = self.index(of: name)
                    else { return nil }
                
                return self[index]
            }
        }
        
        subscript (index: Int) -> Element {
            
            @inline(__always)
            get {
                
                var colorantValue = Colorant().rawValue
                
                var pcsBuffer = [cmsUInt16Number](repeating: 0, count: 3)
                
                var nameBytes = [CChar](repeating: 0, count: 256)
                
                let status = cmsNamedColorInfo(internalPointer,
                                               cmsUInt32Number(index),
                                               &nameBytes,
                                               nil, nil,
                                               &pcsBuffer,
                                               &colorantValue)
                
                assert(status > 0, "Invalid index")
                
                // get swift values
                
                let colorant = Colorant(rawValue: colorantValue)!
                
                let pcs = (pcsBuffer[0], pcsBuffer[1], pcsBuffer[2])
                
                let nameData = Data(bytes: unsafeBitCast(nameBytes, to: Array<UInt8>.self))
                
                let name = String(data: nameData, encoding: String.Encoding.utf8) ?? ""
                
                let element: Element = (name, pcs, colorant)
                
                return element
            }
        }
    }
}

// MARK: - Supporting Types

public extension NamedColorList {
    
    public typealias Element = (name: String, profileColorSpace: ProfileColorSpace, colorant: Colorant)
    
    public typealias ProfileColorSpace = (cmsUInt16Number, cmsUInt16Number, cmsUInt16Number)
    
    public struct Colorant: RawRepresentable {
        
        public let rawValue: [cmsUInt16Number]
        
        @inline(__always)
        public init?(rawValue: [cmsUInt16Number]) {
            
            guard Colorant.validate(rawValue)
                else { return nil }
            
            self.rawValue = rawValue
        }
        
        @inline(__always)
        public init() {
            
            self.rawValue = [cmsUInt16Number](repeating: 0, count: Int(cmsMAXCHANNELS))
            
            assert(Colorant.validate(rawValue))
        }
        
        @inline(__always)
        public static func validate(_ rawValue: RawValue) -> Bool {
            
            // Maximum number of channels in ICC is 16
            return rawValue.count == Int(cmsMAXCHANNELS)
        }
    }
}

// MARK: - Collection

extension NamedColorList: RandomAccessCollection {
    
    public subscript(bounds: Range<Int>) -> RandomAccessSlice<NamedColorList> {
        
        return RandomAccessSlice<NamedColorList>(base: self, bounds: bounds)
    }
    
    /// The start `Index`.
    public var startIndex: Int {
        
        return 0
    }
    
    /// The end `Index`.
    ///
    /// This is the "one-past-the-end" position, and will always be equal to the `count`.
    public var endIndex: Int {
        
        return count
    }
    
    public func index(before i: Int) -> Int {
        return i - 1
    }
    
    public func index(after i: Int) -> Int {
        return i + 1
    }
    
    public func makeIterator() -> IndexingIterator<NamedColorList> {
        
        return IndexingIterator(_elements: self)
    }
}

// MARK: - Internal Protocols

extension NamedColorList: ReferenceConvertible { }

extension NamedColorList.Reference: DuplicableHandle {
    static var cmsDuplicate: cmsDuplicateFunction { return cmsDupNamedColorList }
}
