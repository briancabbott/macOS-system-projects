//
//  SocketOption.swift
//  
//
//  Created by Alsey Coleman Miller on 5/6/22.
//

import Socket

public enum CryptoSocketOption: CInt, SocketOptionID {
    
    @_alwaysEmitIntoClient
    public static var optionLevel: SocketOptionLevel { .crypto }
    
    /// Crypto Key
    case key            = 1
    
    /// Crypto IV
    case iv             = 2
    
    /// Crypto Operation
    case operation      = 3
}

