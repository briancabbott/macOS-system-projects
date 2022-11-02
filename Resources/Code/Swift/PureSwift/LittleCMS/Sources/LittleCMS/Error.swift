//
//  Error.swift
//  LittleCMS
//
//  Created by Alsey Coleman Miller on 6/3/17.
//
//

import CLCMS

/// Errors raised by Little CMS
public enum LittleCMSError: UInt32, Error {
    
    case file = 1
    case range
    case `internal`
    case null
    case read
    case seek
    case write
    case unknownExtension
    case colorspaceCheck
    case alreadyDefined
    case badSignature
    case corruptionDetected
    case notSuitable
}
