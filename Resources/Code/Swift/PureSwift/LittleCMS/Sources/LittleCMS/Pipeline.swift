//
//  Pipeline.swift
//  LittleCMS
//
//  Created by Alsey Coleman Miller on 6/4/17.
//
//

import CLCMS

/// Pipelines are a convenient way to model complex operations on image data.
/// Each pipeline may contain an arbitrary number of stages. 
/// Each stage performs a single operation. 
/// Pipelines may be optimized to be executed on a certain format (8 bits, for example)
/// and can be saved as LUTs in ICC profiles.
public struct Pipeline {
    
    // MARK: - Properties
    
    internal private(set) var internalReference: CopyOnWrite<Reference>
    
    // MARK: - Initialization
    
    internal init(_ internalReference: Reference) {
        
        self.internalReference = CopyOnWrite(internalReference)
    }
    
    /// Creates an empty pipeline. 
    ///
    /// Final Input and output channels must be specified at creation time.
    public init?(channels: (input: UInt, output: UInt), context: Context? = nil) {
        
        guard let internalReference = Reference(channels: channels, context: context)
            else { return  nil}
        
        self.internalReference = CopyOnWrite(internalReference)
    }
    
    // MARK: - Methods
    
    /// Evaluates a pipeline using floating point numbers.
    public func evaluate(_ input: [Float]) -> [Float] {
        
        return internalReference.reference.evaluate(input)
    }
    
    /// Evaluates a pipeline usin 16-bit numbers, optionally using the optimized path.
    public func evaluate(_ input: [UInt16]) -> [UInt16] {
        
        return internalReference.reference.evaluate(input)
    }
}

// MARK: - Reference Type

internal extension Pipeline {
    
    internal final class Reference {
        
        // MARK: - Properties
        
        internal let internalPointer: OpaquePointer
        
        let context: Context?
        
        // MARK: - Initialization
        
        deinit {
            
            // deallocate profile
            cmsPipelineFree(internalPointer)
        }
        
        @inline(__always)
        init(_ internalPointer: OpaquePointer) {
            
            self.internalPointer = internalPointer
            self.context = Reference.context(for: internalPointer) // get swift object from internal pointer
        }
        
        @inline(__always)
        init?(channels: (input: UInt, output: UInt), context: Context? = nil) {
            
            guard let internalPointer = cmsPipelineAlloc(context?.internalPointer,
                                                         cmsUInt32Number(channels.input),
                                                         cmsUInt32Number(channels.output))
                else { return  nil}
            
            self.internalPointer = internalPointer
            self.context = context
        }
        
        // MARK: - Methods
        
        @inline(__always)
        internal func evaluate<T: ExpressibleByIntegerLiteral>(_ input: [T], _ function: (UnsafePointer<T>?, UnsafeMutablePointer<T>?, InternalPointer?) -> ()) -> [T] {
            
            var input = input
            
            var output = [T].init(repeating: 0, count: input.count)
            
            function(&input, &output, internalPointer)
            
            return output
        }
        
        func evaluate(_ input: [Float]) -> [Float] {
            
            return evaluate(input, cmsPipelineEvalFloat)
        }
        
        func evaluate(_ input: [UInt16]) -> [UInt16] {
            
            return evaluate(input, cmsPipelineEval16)
        }
    }
}

// MARK: - Internal Protocols

extension Pipeline.Reference: DuplicableHandle {
    static var cmsDuplicate: cmsDuplicateFunction { return cmsPipelineDup }
}

extension Pipeline.Reference: ContextualHandle {
    static var cmsGetContextID: cmsGetContextIDFunction { return cmsGetPipelineContextID }
}
