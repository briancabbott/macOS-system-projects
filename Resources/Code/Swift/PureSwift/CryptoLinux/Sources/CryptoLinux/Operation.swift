//
//  Operation.swift
//  
//
//  Created by Alsey Coleman Miller on 5/7/22.
//

/// Linux Crypto Operation
public enum CryptoOperation: CInt {
    
    /// Decrypt
    case decrypt = 0 // ALG_OP_DECRYPT
    
    /// Encrypt
    case encrypt = 1 // ALG_OP_ENCRYPT
}

