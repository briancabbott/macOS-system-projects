//
//  SocketProtocol.swift
//  
//
//  Created by Alsey Coleman Miller on 5/6/22.
//

import Socket

public enum CryptoSocketProtocol: Int32, SocketProtocol {
    
    case `default` = 0
    
    @_alwaysEmitIntoClient
    public static var family: SocketAddressFamily { .crypto }
    
    @_alwaysEmitIntoClient
    public var type: SocketType { .sequencedPacket }
}
