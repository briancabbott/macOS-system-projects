//
//  Tag.swift
//  LittleCMS
//
//  Created by Alsey Coleman Miller on 6/4/17.
//
//

import struct Foundation.Data
import CLCMS

// MARK: - Tag Signature

public typealias Tag = cmsTagSignature

public extension Tag {
    
    var isValid: Bool {
        
        return self.rawValue != 0
    }
}

// MARK: - Tag View

public extension Profile {
    
    /// Creates a new profile, replacing all tags with the specified new ones.
    public init(profile: Profile, tags: TagView) {
        
        self.internalReference = profile.internalReference
        self.tags = tags
    }
    
    /// A collection of the profile's tags.
    public var tags: TagView {
        
        get { return TagView(profile: self) }
        
        mutating set {
            
            // set new tags
            
        }
    }
    
    /// A representation of the profile's contents as a collection of tags.
    public struct TagView {
        
        // MARK: - Properties
        
        internal private(set) var internalReference: CopyOnWrite<Profile.Reference>
        
        // MARK: - Initialization
        
        internal init(_ internalReference: Profile.Reference) {
            
            self.internalReference = CopyOnWrite(internalReference)
        }
        
        /// Create from the specified profile.
        public init(profile: Profile) {
            
            self.init(profile.internalReference.reference)
        }
        
        // MARK: - Accessors
        
        public var count: Int {
            
            return internalReference.reference.tagCount
        }
        
        // MARK: - Methods
        
        // Returns `true` if a tag with signature sig is found on the profile.
        /// Useful to check if a profile contains a given tag.
        public func contains(_ tag: Tag) -> Bool {
            
            return internalReference.reference.contains(tag)
        }
        
        /// Creates a directory entry on tag sig that points to same location as tag destination.
        /// Using this function you can collapse several tag entries to the same block in the profile.
        public mutating func link(_ tag: Tag, to destination: Tag) -> Bool {
            
            return internalReference.mutatingReference.link(tag, to: destination)
        }
        
        /// Returns the tag linked to, in the case two tags are sharing same resource,
        /// or `nil` if the tag is not linked to any other tag.
        public func tagLinked(to tag: Tag) -> Tag? {
            
            return internalReference.reference.tagLinked(to: tag)
        }
        
        /// Returns the signature of a tag located at the specified index.
        public func tag(at index: Int) -> Tag? {
            
            return internalReference.reference.tag(at: index)
        }
        
        @inline(__always)
        fileprivate func pointer(for tag: Tag) -> UnsafeMutableRawPointer? {
            
            return cmsReadTag(internalReference.reference.internalPointer, tag)
        }
        
        /// Reads the tag value and attempts to get value from pointer.
        fileprivate func readCasting(_ tag: Tag, to valueType: TagValueConvertible.Type) -> TagValueConvertible? {
            
            guard let pointer = pointer(for: tag)
                else { return nil }
            
            return valueType.init(tagValue: pointer)
        }
        
        /// Reads the tag value and attempts to get value from pointer.
        fileprivate func readCasting<Value: TagValueConvertible>(_ tag: Tag) -> Value? {
            
            guard let pointer = pointer(for: tag)
                else { return nil }
            
            return Value(tagValue: pointer)
        }
        
        /// Reads the tag value and attempts to get value from pointer.
        public func read(_ tag: Tag) -> TagValue? {
            
            // invalid tag
            guard let valueType = tag.valueType as? TagValueConvertible.Type
                else { return nil }
            
            return readCasting(tag, to: valueType) as TagValue?
        }
    }
}

// MARK: - Collection

/* Sequence seems broken in Swift 4
extension Profile.TagView: RandomAccessCollection {
    
    public subscript(index: Int) -> TagValue {
        
        get {
            
            guard let tag = tag(at: index)
                else { fatalError("No tag at index \(index)") }
            
            guard let value = read(tag)
                else { fatalError("No value for tag \(tag) at index \(index)") }
            
            return value
        }
    }
    
    public subscript(bounds: Range<Int>) -> RandomAccessSlice<Profile.TagView> {
        
        return RandomAccessSlice<Profile.TagView>(base: self, bounds: bounds)
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
    
    public func makeIterator() -> IndexingIterator<Profile.TagView> {
        
        return IndexingIterator(_elements: self)
    }
}*/

// MARK: - Supporting Type

/// Any value that can be retrieved for a tag.
public protocol TagValue { }

fileprivate protocol TagValueConvertible: TagValue {
    
    init(tagValue pointer: UnsafeMutableRawPointer)
}

extension TagValueConvertible {
    
    /// Initializes value by casting pointer.
    init(tagValue pointer: UnsafeMutableRawPointer) {
        
        let pointer = pointer.assumingMemoryBound(to: Self.self)
        
        self = pointer[0]
    }
}

extension TagValueConvertible where Self: ReferenceConvertible, Self.Reference: DuplicableHandle {
    
    /// Initializes value by casting pointer to handle type, creating object wrapper, and then Swift struct.
    init(tagValue pointer: UnsafeMutableRawPointer) {
        
        let pointer = pointer.assumingMemoryBound(to: Self.Reference.InternalPointer.self)
        
        let internalPointer = pointer[0]
        
        // create copy to not corrupt profile handle internals
        guard let newInternalPointer = Self.Reference.cmsDuplicate(internalPointer)
            else { fatalError("Could not create duplicate \(Self.self)") }
        
        let reference = Self.Reference.init(newInternalPointer)
        
        self.init(reference)
    }
}

public extension Tag {
    
    /// The valye type for the specified tag.
    var valueType: TagValue.Type? {
        
        switch self {
            
        case cmsSigAToB0Tag: return Pipeline.self
        case cmsSigAToB1Tag: return Pipeline.self
        case cmsSigAToB2Tag: return Pipeline.self
        case cmsSigBlueColorantTag: return cmsCIEXYZ.self
        case cmsSigBlueMatrixColumnTag: return cmsCIEXYZ.self
            
        default: return nil
        }
    }
}

extension Pipeline: TagValueConvertible { }
extension cmsCIEXYZ: TagValueConvertible { }

// MARK: - Tag Properties

public extension Profile.TagView {
    
    // TODO: implement all tags
    
    public var aToB0: Pipeline? {
        
        return readCasting(cmsSigAToB0Tag)
    }
    
    public var blueColorant: cmsCIEXYZ? {
        
        return readCasting(cmsSigBlueColorantTag)
    }
}
