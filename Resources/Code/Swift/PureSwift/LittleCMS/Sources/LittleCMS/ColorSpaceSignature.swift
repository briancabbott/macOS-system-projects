//
//  ColorSpaceSignature.swift
//  LittleCMS
//
//  Created by Alsey Coleman Miller on 6/3/17.
//
//

import struct CLCMS.cmsColorSpaceSignature
import func CLCMS.cmsChannelsOf

public typealias ColorSpaceSignature = cmsColorSpaceSignature

public extension ColorSpaceSignature {
    
    public var numberOfComponents: UInt {
        
        return UInt(cmsChannelsOf(self))
    }
}
