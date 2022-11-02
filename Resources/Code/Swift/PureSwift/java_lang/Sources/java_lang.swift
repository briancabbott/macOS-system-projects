//
//  java_lang.swift
//  SwiftJava
//
//  Created by John Holdsworth on 18/07/2016.
//  Copyright (c) 2016 John Holdsworth. All rights reserved.
//

@_exported import java_swift

extension JavaObject {

    public var className: String {
        return getClass().getName()
    }

    public func validDownCast( toJavaClass: String, _ file: StaticString = #file, _ line: Int = #line ) -> Bool {
        do {
            if let targetClass = try JavaClass.forName( toJavaClass ) {
                let validCast = targetClass.isAssignableFrom( getClass() )
                if !validCast {
                    JNI.report( "Invalid cast from \(className) to \(toJavaClass)", file, line )
                }
                return validCast
            }
        } catch let notFound as ClassNotFoundException {
            notFound.printStackTrace();
        } catch {
            JNI.report("validDownCast: Uncaught exception \(error)")
        }
        return false;
    }

}

extension JavaThread {

    public convenience init( _ closure: @escaping () -> () )  {
        self.init( ClosureRunnable( closure ) )
    }

}

extension JNIType {

    private static var floatClass: jclass?
    private static var floatInitMethodID: jmethodID?

    public static func toJavaFloat( value: Float, locals: UnsafeMutablePointer<[jobject]> ) -> jvalue {
        var args: [jvalue] = [jvalue( f: value )]
        return jvalue( l: JNIMethod.NewObject( className: "java/lang/Float", classCache: &floatClass,
                                               methodSig: "(F)V", methodCache: &floatInitMethodID,
                                               args: &args, locals: locals ) )
    }

    public static func toSwiftFloat( from: jobject?, consume: Bool = true ) -> Float {
        guard let from = from else { return 0.0 }
        defer { if consume { JNI.DeleteLocalRef( from ) } }
        var args: [jvalue] = [jvalue()]
        var locals = [jobject]()
        var floatValueMethodID: jmethodID?
        return JNIMethod.CallFloatMethod( object: from, methodName: "floatValue", methodSig: "()F", methodCache: &floatValueMethodID,
                                          args: &args, locals: &locals )
    }

}
