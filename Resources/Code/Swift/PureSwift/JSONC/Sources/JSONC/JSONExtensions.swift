//
//  JSONExtensions.swift
//  JSONC
//
//  Created by Alsey Coleman Miller on 12/19/15.
//  Copyright © 2015 PureSwift. All rights reserved.
//

import SwiftFoundation

public extension JSON.Value {
    
    /// Deserializes JSON from a string.
    ///
    /// - Note: Uses the [JSON-C](https://github.com/json-c/json-c) library.
    init?(string: Swift.String) {
        
        guard let value = JSONC.parse(string)
            else { return nil }
        
        self = value
    }
}