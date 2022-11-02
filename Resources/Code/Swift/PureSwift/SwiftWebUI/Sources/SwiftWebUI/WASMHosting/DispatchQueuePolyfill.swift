//
//  DispatchQueuePolyfill.swift
//  
//
//  Created by Carson Katri on 5/29/20.
//

public class DispatchQueue {
    public static let main = DispatchQueue()
    
    public func async(execute: () -> Void) {
        execute()
    }
}

public extension Error {
    var localizedDescription: String {
        String(describing: self)
    }
}
