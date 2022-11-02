//
//  JSPromise.swift
//  
//
//  Created by Alsey Coleman Miller on 6/3/20.
//

/**
 JavaScript Promise
 
 A Promise is a proxy for a value not necessarily known when the promise is created. It allows you to associate handlers with an asynchronous action's eventual success value or failure reason. This lets asynchronous methods return values like synchronous methods: instead of immediately returning the final value, the asynchronous method returns a promise to supply the value at some point in the future.
 */
public final class JSPromise<Success>: JSType where Success: JSValueConvertible, Success: JSValueConstructible {
    
    // MARK: - Properties
    
    public let jsObject: JSObjectRef
    
    // MARK: - Initialization

    public init?(_ jsObject: JSObjectRef) {
        self.jsObject = jsObject
        // validate if promise type
    }
    
    /**
     Initializes a Promise with the specified executor.
     
     - parameter executor: A function to be executed by the Promise.  The executor is custom code that ties an outcome to a promise. You, the programmer, write the executor.
     */
    public init(executor block: @escaping ((Success) -> (), (JSError) -> ()) -> ()) {
        let executor = JSClosure { (arguments) in
            let resolutionFunc = arguments[0].function
            let rejectionFunc = arguments[1].function
            block({ resolutionFunc?($0.jsValue()) }, { rejectionFunc?($0.jsValue()) })
            return .null
        }
        self.jsObject = JSPromiseClassObject.new(executor)
    }
    
    /**
     Initializes a Promise with the specified executor.
     
     - parameter executor: A function to be executed by the Promise.  The executor is custom code that ties an outcome to a promise. You, the programmer, write the executor.
     */
    public convenience init(executor block: @escaping ((Result<Success, JSError>) -> ()) -> ()) {
        self.init { (resolution, rejection) in
            block({
                switch $0 {
                case let .success(value):
                    resolution(value)
                case let .failure(error):
                    rejection(error)
                }
            })
        }
    }
    
    // MARK: - Accessors
    
    /// Promise State
    public var state: JSPromiseState {
        guard let value = jsObject.state.string.flatMap({ State(rawValue: $0) })
            else { fatalError("Invalid state: \(jsObject.state)") }
        return value
    }
    
    /// Promise result value.
    public var result: JSValue {
        return jsObject.result
    }
    
    // MARK: - Methods
    
    /**
     The `then()` method returns a Promise. It takes up to two arguments: callback functions for the success and failure cases of the Promise.
     
     - Parameter onFulfilled: A Function called if the Promise is fulfilled. This function has one argument, the fulfillment value. If it is not a function, it is internally replaced with an "Identity" function (it returns the received argument).
     
     - Parameter onRejected: A Function called if the Promise is rejected. This function has one argument, the rejection reason. If it is not a function, it is internally replaced with a "Thrower" function (it throws an error it received as argument).
     
     - Returns: Once a Promise is fulfilled or rejected, the respective handler function (onFulfilled or onRejected) will be called asynchronously (scheduled in the current thread loop). The behaviour of the handler function follows a specific set of rules.
     
     - Note: If a handler function:
     - returns a value, the promise returned by then gets resolved with the returned value as its value.
     - doesn't return anything, the promise returned by then gets resolved with an undefined value.
     - throws an error, the promise returned by then gets rejected with the thrown error as its value.
     - returns an already fulfilled promise, the promise returned by then gets fulfilled with that promise's value as its value.
     - returns an already rejected promise, the promise returned by then gets rejected with that promise's value as its value.
     - returns another pending promise object, the resolution/rejection of the promise returned by then will be subsequent to the resolution/rejection of  the promise returned by the handler. Also, the resolved value of the promise returned by then will be the same as the resolved value of the promise returned by the handler.
     */
    @discardableResult
    internal func _then(onFulfilled: @escaping (Success) -> (JSValue),
                        onRejected: @escaping (JSError) -> ()) -> JSValue {
        
        guard let function = jsObject.then.function
            else { fatalError("Invalid function \(#function)") }
        
        let success = JSClosure { (arguments) in
            if let value = arguments.first.flatMap({ Success.construct(from: $0) }) {
                return onFulfilled(value)
            } else {
                JSConsole.error("Unable to load success type \(String(reflecting: Success.self)) from ", arguments.first ?? "nil")
                return .undefined
            }
        }
        
        let errorFunction = JSClosure { (arguments) in
            if let value = arguments.first.flatMap({ JSError.construct(from: $0) }) {
                onRejected(value)
            } else {
                JSConsole.error("Unable to load error from ", arguments.first ?? "nil")
            }
            return .undefined
        }
        
        return function.apply(this: jsObject, argumentList: [success.jsValue(), errorFunction.jsValue()])
    }
    
    @discardableResult
    internal func _then(onFulfilled: @escaping (Success) -> (JSValue)) -> JSValue {
        
        guard let function = jsObject.then.function
            else { fatalError("Invalid function \(#function)") }
        
        let success = JSClosure { (arguments) in
            if let value = arguments.first.flatMap({ Success.construct(from: $0) }) {
                return onFulfilled(value)
            } else {
                JSConsole.error("Unable to load success type \(String(reflecting: Success.self)) from ", arguments.first ?? "nil")
                return .undefined
            }
        }
        
        return function.apply(this: jsObject, argumentList: [success.jsValue()])
    }
    
    /**
    The `then()` method returns a Promise. It takes up to two arguments: callback functions for the success and failure cases of the Promise.
    
    - Parameter onFulfilled: A Function called if the Promise is fulfilled. This function has one argument, the fulfillment value. If it is not a function, it is internally replaced with an "Identity" function (it returns the received argument).
    
    - Parameter onRejected: A Function called if the Promise is rejected. This function has one argument, the rejection reason. If it is not a function, it is internally replaced with a "Thrower" function (it throws an error it received as argument).
    
    - Returns: Once a Promise is fulfilled or rejected, the respective handler function (onFulfilled or onRejected) will be called asynchronously (scheduled in the current thread loop). The behaviour of the handler function follows a specific set of rules.
    
    - Note: If a handler function:
    - returns a value, the promise returned by then gets resolved with the returned value as its value.
    - doesn't return anything, the promise returned by then gets resolved with an undefined value.
    - throws an error, the promise returned by then gets rejected with the thrown error as its value.
    - returns an already fulfilled promise, the promise returned by then gets fulfilled with that promise's value as its value.
    - returns an already rejected promise, the promise returned by then gets rejected with that promise's value as its value.
    - returns another pending promise object, the resolution/rejection of the promise returned by then will be subsequent to the resolution/rejection of  the promise returned by the handler. Also, the resolved value of the promise returned by then will be the same as the resolved value of the promise returned by the handler.
    */
    public func then(onFulfilled: @escaping (Success) -> (),
                     onRejected: @escaping (JSError) -> ()) -> JSPromise<Success> {
        let result = _then(onFulfilled: {
            onFulfilled($0)
            return .null
        }, onRejected: onRejected)
        guard let promise = result.object.flatMap({ JSPromise<Success>($0) })
            else { fatalError("Invalid object \(result)") }
        return promise
    }
    
    /**
    The `then()` method returns a Promise. It takes up to two arguments: callback functions for the success and failure cases of the Promise.
    
    - Parameter onFulfilled: A Function called if the Promise is fulfilled. This function has one argument, the fulfillment value. If it is not a function, it is internally replaced with an "Identity" function (it returns the received argument).
    
    - Parameter onRejected: A Function called if the Promise is rejected. This function has one argument, the rejection reason. If it is not a function, it is internally replaced with a "Thrower" function (it throws an error it received as argument).
    
    - Returns: Once a Promise is fulfilled or rejected, the respective handler function (onFulfilled or onRejected) will be called asynchronously (scheduled in the current thread loop). The behaviour of the handler function follows a specific set of rules.
    
    - Note: If a handler function:
    - returns a value, the promise returned by then gets resolved with the returned value as its value.
    - doesn't return anything, the promise returned by then gets resolved with an undefined value.
    - throws an error, the promise returned by then gets rejected with the thrown error as its value.
    - returns an already fulfilled promise, the promise returned by then gets fulfilled with that promise's value as its value.
    - returns an already rejected promise, the promise returned by then gets rejected with that promise's value as its value.
    - returns another pending promise object, the resolution/rejection of the promise returned by then will be subsequent to the resolution/rejection of  the promise returned by the handler. Also, the resolved value of the promise returned by then will be the same as the resolved value of the promise returned by the handler.
    */
    public func then<T>(onFulfilled: @escaping (Success) -> (JSPromise<T>),
                        onRejected: @escaping (JSError) -> ()) -> JSPromise<T> where T: JSType {
        
        let result = _then(onFulfilled: {
            return onFulfilled($0).jsValue()
        }, onRejected: onRejected)
        guard let promise = result.object.flatMap({ JSPromise<T>($0) })
            else { fatalError("Invalid object \(result)") }
        return promise
    }
    
    /**
    The `then()` method returns a Promise. It takes up to two arguments: callback functions for the success and failure cases of the Promise.
    
    - Parameter onFulfilled: A Function called if the Promise is fulfilled. This function has one argument, the fulfillment value. If it is not a function, it is internally replaced with an "Identity" function (it returns the received argument).
    
    - Parameter onRejected: A Function called if the Promise is rejected. This function has one argument, the rejection reason. If it is not a function, it is internally replaced with a "Thrower" function (it throws an error it received as argument).
    
    - Returns: Once a Promise is fulfilled or rejected, the respective handler function (onFulfilled or onRejected) will be called asynchronously (scheduled in the current thread loop). The behaviour of the handler function follows a specific set of rules.
    
    - Note: If a handler function:
    - returns a value, the promise returned by then gets resolved with the returned value as its value.
    - doesn't return anything, the promise returned by then gets resolved with an undefined value.
    - throws an error, the promise returned by then gets rejected with the thrown error as its value.
    - returns an already fulfilled promise, the promise returned by then gets fulfilled with that promise's value as its value.
    - returns an already rejected promise, the promise returned by then gets rejected with that promise's value as its value.
    - returns another pending promise object, the resolution/rejection of the promise returned by then will be subsequent to the resolution/rejection of  the promise returned by the handler. Also, the resolved value of the promise returned by then will be the same as the resolved value of the promise returned by the handler.
    */
    public func then(onFulfilled: @escaping (Success) -> ()) -> JSPromise<Success> {
        let result = _then(onFulfilled: {
            onFulfilled($0)
            return .null
        })
        guard let promise = result.object.flatMap({ JSPromise<Success>($0) })
            else { fatalError("Invalid object \(result)") }
        return promise
    }
    
    /**
    The `then()` method returns a Promise. It takes up to two arguments: callback functions for the success and failure cases of the Promise.
    
    - Parameter onFulfilled: A Function called if the Promise is fulfilled. This function has one argument, the fulfillment value. If it is not a function, it is internally replaced with an "Identity" function (it returns the received argument).
    
    - Parameter onRejected: A Function called if the Promise is rejected. This function has one argument, the rejection reason. If it is not a function, it is internally replaced with a "Thrower" function (it throws an error it received as argument).
    
    - Returns: Once a Promise is fulfilled or rejected, the respective handler function (onFulfilled or onRejected) will be called asynchronously (scheduled in the current thread loop). The behaviour of the handler function follows a specific set of rules.
    
    - Note: If a handler function:
    - returns a value, the promise returned by then gets resolved with the returned value as its value.
    - doesn't return anything, the promise returned by then gets resolved with an undefined value.
    - throws an error, the promise returned by then gets rejected with the thrown error as its value.
    - returns an already fulfilled promise, the promise returned by then gets fulfilled with that promise's value as its value.
    - returns an already rejected promise, the promise returned by then gets rejected with that promise's value as its value.
    - returns another pending promise object, the resolution/rejection of the promise returned by then will be subsequent to the resolution/rejection of  the promise returned by the handler. Also, the resolved value of the promise returned by then will be the same as the resolved value of the promise returned by the handler.
    */
    public func then<T>(onFulfilled: @escaping (Success) -> (JSPromise<T>)) -> JSPromise<T> where T: JSType {
        
        let result = _then(onFulfilled: {
            return onFulfilled($0).jsValue()
        })
        guard let promise = result.object.flatMap({ JSPromise<T>($0) })
            else { fatalError("Invalid object \(result)") }
        return promise
    }
    
    public func `catch`(_ completion: @escaping (JSError) -> ()) {
        guard let function = jsObject.catch.function
            else { fatalError("Invalid function \(#function)") }
        let errorFunction = JSClosure { (arguments) in
            if let value = arguments.first.flatMap({ JSError.construct(from: $0) }) {
                completion(value)
            } else {
                JSConsole.error("Unable to load error from ", arguments.first ?? "nil")
            }
            return .undefined
        }
        function.apply(this: jsObject, arguments: errorFunction)
    }
    
    public func finally() {
        guard let function = jsObject.finally.function
            else { fatalError("Invalid function \(#function)") }
        function.apply(this: jsObject)
    }
}

private let JSPromiseClassObject = JSObjectRef.global.Promise.function!

// MARK: - Supporting Types

public extension JSPromise {
    
    typealias State = JSPromiseState
}

public enum JSPromiseState: String {
    
    /// Initial state, neither fulfilled nor rejected.
    case pending
    
    /// The operation completed successfully.
    case fulfilled
    
    /// the operation failed.
    case rejected
}

extension JSPromiseState: JSValueConstructible {
    
    public static func construct(from value: JSValue) -> JSPromiseState? {
        return value.string.flatMap { JSPromiseState(rawValue: $0) }
    }
}
