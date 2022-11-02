//
//  ColorTransform.swift
//  LittleCMS
//
//  Created by Alsey Coleman Miller on 6/3/17.
//
//

import struct Foundation.Data
import CLCMS

public final class ColorTransform {
    
    // MARK: - Properties
    
    internal let internalPointer: cmsHTRANSFORM
    
    public let context: Context?
    
    /// Other CMS object wrappers / reference-backed value types to retain (besides the context)
    private let retain: [Any]
    
    // MARK: - Initialization
    
    deinit {
        
        cmsDeleteTransform(internalPointer)
    }
    
    /// Creates a color transform for translating bitmaps.
    public init?(input: (profile: Profile, format: cmsUInt32Number),
                output: (profile: Profile, format: cmsUInt32Number),
                intent: Intent,
                flags: cmsUInt32Number = 0,
                context: Context? = nil) {
        
        guard let internalPointer = cmsCreateTransformTHR(context?.internalPointer,
                                                     input.profile.internalReference.reference.internalPointer,
                                                     input.format,
                                                     output.profile.internalReference.reference.internalPointer,
                                                     output.format,
                                                     intent.rawValue,
                                                     flags)
            else { return nil }
        
        self.internalPointer = internalPointer
        self.context = context
        self.retain = [input.profile, output.profile]
    }
    
    // MARK: - Methods
    
    /// Translates bitmaps according of parameters setup when creating the color transform.
    public func transform(_ bitmap: Data) -> Data {
        
        let internalPointer = self.internalPointer
        
        var output = Data(count: bitmap.count)
        
        bitmap.withUnsafeBytes { (inputBytes: UnsafePointer<UInt8>) in
            
            let inputBytes = UnsafeRawPointer(inputBytes)
            
            output.withUnsafeMutableBytes { (outputBytes: UnsafeMutablePointer<UInt8>) in
                
                let outputBytes = UnsafeMutableRawPointer(outputBytes)
                
                cmsDoTransform(internalPointer, inputBytes, outputBytes, cmsUInt32Number(bitmap.count))
            }
        }
        
        return output
    }
}

extension ColorTransform: ContextualHandle {
    static var cmsGetContextID: cmsGetContextIDFunction { return cmsGetTransformContextID }
}
