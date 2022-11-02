//
//  json.swift
//  json
//
//  Created by Dan Kogai on 6/9/14.
//  Copyright (c) 2014 Dan Kogai. All rights reserved.
//

import Foundation

class JSON {
    class func parse(str:String) -> AnyObject? {
        var err:NSError?
        let enc = NSUTF8StringEncoding
        var obj:AnyObject? = NSJSONSerialization.JSONObjectWithData(
            str.dataUsingEncoding(enc), options:nil, error:&err
        )
        if err { NSLog("error: %@", err!) }
        return obj
    }
    class func stringify(obj:AnyObject) -> String? {
        var err:NSError?
        let data = NSJSONSerialization.dataWithJSONObject(
            obj, options:nil, error:&err
        )
        if err {
            NSLog("error: %@", err!)
            return nil
        }
        return NSString(data:data, encoding:NSUTF8StringEncoding)
    }
}
