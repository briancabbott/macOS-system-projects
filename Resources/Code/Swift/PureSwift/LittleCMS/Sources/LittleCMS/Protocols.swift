//
//  Protocols.swift
//  LittleCMS
//
//  Created by Alsey Coleman Miller on 6/4/17.
//
//

import struct CLCMS.cmsContext

// MARK: - Public Protocols

/// Any Swift object that can be copied.
public protocol Copyable: class {
    
    /// Creates a new instance or `nil` if copy failed.
    var copy: Self? { get }
}

// MARK: - Internal Protocols

/// The Swift class is a wrapper for a LittleCMS opaque type.
internal protocol HandleObject {
    
    associatedtype InternalPointer
    
    var internalPointer: InternalPointer { get }
}

/// Any Swift LittleCMS wrapper object that can be copied
internal protocol CopyableHandle: HandleObject, Copyable { }

/// The cms handle can be duplicated with a function.
internal protocol DuplicableHandle: CopyableHandle {
    
    typealias cmsDuplicateFunction = (InternalPointer!) -> InternalPointer!

    /// The Little CMS Function that creates a duplicate of the handler.
    static var cmsDuplicate: cmsDuplicateFunction { get }
    
    init(_ internalPointer: InternalPointer)
}

extension DuplicableHandle {
    
    // Protocol Oriented Programming implementation
    
    public var copy: Self? {
        
        @inline(__always)
        get {
            
            guard let newInternalPointer = Self.cmsDuplicate(self.internalPointer)
                else { return nil }
            
            return Self(newInternalPointer)
        }
    }
}

/// CMS handle that has a context attached
internal protocol ContextualHandle: HandleObject {
    
    typealias cmsGetContextIDFunction = (InternalPointer!) -> cmsContext!
    
    /// The Little CMS Function that gets the cms context ID of the handler.
    static var cmsGetContextID: cmsGetContextIDFunction { get }
}

extension ContextualHandle {
    
    /// Get context using cms function
    @inline(__always)
    static func context(for internalPointer: InternalPointer) -> Context? {
        
        guard let internalPointer = Self.cmsGetContextID(internalPointer)
            else { return nil }
        
        return cmsGetSwiftContext(internalPointer)
    }
}

// Swift struct wrapper for `HandleObject`.
internal protocol ReferenceConvertible {
    
    associatedtype Reference: CopyableHandle
    
    var internalReference: CopyOnWrite<Reference> { get }
    
    init(_ internalReference: Reference)
}

/// Encapsulates behavior surrounding value semantics and copy-on-write behavior
/// Modified version of https://github.com/klundberg/CopyOnWrite
internal struct CopyOnWrite<Reference: CopyableHandle> {
    
    /// Needed for `isKnownUniquelyReferenced`
    final class Box {
        
        let unbox: Reference
        
        @inline(__always)
        init(_ value: Reference) {
            unbox = value
        }
    }
    
    var _reference: Box
    
    /// Constructs the copy-on-write wrapper around the given reference and copy function
    ///
    /// - Parameters:
    ///   - reference: The object that is to be given value semantics
    ///   - copier: The function that is responsible for copying the reference if the 
    /// consumer of this API needs it to be copied. This function should create a new 
    /// instance of the referenced type; it should not return the original reference given to it.
    @inline(__always)
    init(_ reference: Reference) {
        self._reference = Box(reference)
    }
    
    /// Returns the reference meant for read-only operations.
    var reference: Reference {
        @inline(__always)
        get {
            return _reference.unbox
        }
    }
    
    /// Returns the reference meant for mutable operations. 
    ///
    /// If necessary, the reference is copied using the `copier` function
    /// or closure provided to the initializer before returning, in order to preserve value semantics.
    var mutatingReference: Reference {
        
        mutating get {
            
            // copy the reference only if necessary
            if !isUniquelyReferenced {
                
                guard let copy = _reference.unbox.copy
                    else { fatalError("Coult not duplicate internal reference type") }
                
                _reference = Box(copy)
            }
            
            return _reference.unbox
        }
    }
    
    /// Helper property to determine whether the reference is uniquely held. Used in tests as a sanity check.
    internal var isUniquelyReferenced: Bool {
        @inline(__always)
        mutating get {
            return isKnownUniquelyReferenced(&_reference)
        }
    }
}
