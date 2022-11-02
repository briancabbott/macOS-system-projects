//
//  ToneCurve.swift
//  LittleCMS
//
//  Created by Alsey Coleman Miller on 6/3/17.
//
//

import CLCMS

public final class ToneCurve {
    
    // MARK: - Properties
    
    internal let internalPointer: OpaquePointer
    
    // MARK: - Initialization
    
    deinit {
        
        // deallocate profile
        cmsFreeToneCurve(internalPointer)
    }
    
    internal init(_ internalPointer: OpaquePointer) {
        
        self.internalPointer = internalPointer
    }
    
    public init?(gamma: Double, context: Context? = nil) {
        
        guard let internalPointer = cmsBuildGamma(context?.internalPointer, gamma)
            else { return nil }
        
        self.internalPointer = internalPointer
    }
    
    // MARK: - Accessors
    
    public var isLinear: Bool {
        
        return cmsIsToneCurveLinear(internalPointer) != 0
    }
}

// MARK: - Internal Protocols

extension ToneCurve: DuplicableHandle {
    static var cmsDuplicate: cmsDuplicateFunction { return cmsDupToneCurve }
}
