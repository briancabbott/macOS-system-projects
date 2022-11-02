//
//  Context.swift
//  LittleCMS
//
//  Created by Alsey Coleman Miller on 6/3/17.
//
//

import CLCMS

/// Keeps track of all plug-ins and static data.
///
/// There are situations where several instances of Little CMS engine have to coexist but on different conditions. 
/// For example, when the library is used as a DLL or a shared object, diverse applications may want to use
/// different plug-ins. Another example is when multiple threads are being used in same task and the
/// user wants to pass thread-dependent information to the memory allocators or the logging system. 
/// For all this use, Little CMS 2.6 and above implements context handling functions.
public final class Context {
    
    public typealias ErrorLog = (String, LittleCMSError?) -> ()
    
    // MARK: - Properties
    
    internal private(set) var internalPointer: cmsContext!
    
    // MARK: - Initialization
    
    deinit {
        
        if let internalPointer = self.internalPointer {
            
            cmsDeleteContext(internalPointer)
        }
    }
    
    /// Dummy initializer to satisfy Swift
    private init() { }
    
    //// Creates a new context with optional associated plug-ins.
    /// that will be forwarded to plug-ins and logger.
    public convenience init?(plugin: UnsafeMutableRawPointer? = nil) {
        
        self.init()
        
        let userData = cmsCreateContextUserData(self)
        
        guard let internalPointer = cmsCreateContext(plugin, userData)
            else { return nil }
        
        self.internalPointer = internalPointer
    }
    
    // MARK: - Accessors
    
    /// Duplicates a context with all associated plug-ins.
    /// Caller may specify an optional pointer to user-defined data
    /// that will be forwarded to plug-ins and logger.
    public var copy: Context? {
        
        let new = Context()
        
        let userData = cmsCreateContextUserData(new)
        
        guard let internalPointer = cmsDupContext(self.internalPointer, userData)
            else { return nil }
        
        new.internalPointer = internalPointer
        
        new.errorLog = errorLog
        
        return new
    }
    
    /// The handler for error logs.
    public var errorLog: ErrorLog? {
        
        didSet {
            
            let log: cmsLogErrorHandlerFunction?
            
            if errorLog != nil {
                
                log = logErrorHandler
                
            } else {
                
                log = nil
            }
            
            // set new error handler
            cmsSetLogErrorHandlerTHR(internalPointer, log)
        }
    }
}

// MARK: - Protocol Conformance

extension Context: Copyable { }

// MARK: - Private Functions

/// Creates the user data pointer for use with Little CMS functions.
@_silgen_name("_cmsCreateContextUserDataFromSwiftContext")
private func cmsCreateContextUserData(_ context: Context) -> UnsafeMutableRawPointer {
    
    let unmanaged = Unmanaged.passUnretained(context)
    
    let objectPointer = unmanaged.toOpaque()
    
    return objectPointer
}

/// Gets the Swift `Context` object from the Little CMS opaque type's associated user data.
/// This function will crash if the context was not originally created in Swift.
@_silgen_name("_cmsGetSwiftContext")
internal func cmsGetSwiftContext(_ contextID: cmsContext) -> Context? {
    
    guard let userData = cmsGetContextUserData(contextID)
        else { return nil }
    
    let unmanaged = Unmanaged<Context>.fromOpaque(userData)
    
    let context = unmanaged.takeUnretainedValue()
    
    return context
}

/// Function for logging
@_silgen_name("_cmsSwiftLogErrorHandler")
private func logErrorHandler(_ internalPointer: cmsContext?, _ error: cmsUInt32Number, _ messageBuffer: UnsafePointer<Int8>?) {
    
    // get swift context object
    let context = cmsGetSwiftContext(internalPointer!)!
    
    let error = LittleCMSError(rawValue: error)
    
    let message: String
    
    if let cString = messageBuffer {
        
        message = String(cString: cString)
        
    } else {
        
        message = ""
    }
    
    context.errorLog?(message, error)
}
