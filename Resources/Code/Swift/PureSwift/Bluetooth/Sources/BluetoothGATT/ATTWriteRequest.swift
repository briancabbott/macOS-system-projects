//
//  ATTWriteRequest.swift
//  Bluetooth
//
//  Created by Alsey Coleman Miller on 6/14/18.
//  Copyright © 2018 PureSwift. All rights reserved.
//

import Foundation

/// Write Request
///
/// The *Write Request* is used to request the server to write the value of an attribute
/// and acknowledge that this has been achieved in a *Write Response*.
@frozen
public struct ATTWriteRequest: ATTProtocolDataUnit, Equatable {
    
    public static var attributeOpcode: ATTOpcode { return .writeRequest }
    
    /// The handle of the attribute to be written.
    public var handle: UInt16
    
    /// The value to be written to the attribute.
    public var value: Data
    
    public init(handle: UInt16, value: Data) {
        
        self.handle = handle
        self.value = value
    }
}

public extension ATTWriteRequest {
    
    init?(data: Data) {
        
        guard data.count >= 3,
            type(of: self).validateOpcode(data)
            else { return nil }
        
        self.handle = UInt16(littleEndian: UInt16(bytes: (data[1], data[2])))
        self.value = data.suffixCheckingBounds(from: 3)
    }
    
    var data: Data {
        
        return Data(self)
    }
}

extension ATTWriteRequest: DataConvertible {
    
    var dataLength: Int {
        
        return 3 + value.count
    }
    
    static func += <T: DataContainer> (data: inout T, value: ATTWriteRequest) {
        
        data += attributeOpcode.rawValue
        data += value.handle.littleEndian
        data += value.value
    }
}
