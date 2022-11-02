//
//  main.swift
//  json
//
//  Created by Dan Kogai on 6/9/14.
//  Copyright (c) 2014 Dan Kogai. All rights reserved.
//

import Foundation

func fetch(u:String) -> String? {
     let url = NSURL.URLWithString(u)
     var enc:NSStringEncoding = NSUTF8StringEncoding
     var err:NSError?
     let str:String? = 
         NSString.stringWithContentsOfURL(url, usedEncoding:&enc, error:&err)
     if err { NSLog("error: %@", err!) }
     return str
}

if C_ARGC > 1 { 
  if let content = fetch(String.fromCString(C_ARGV[1])) {
    if let json:AnyObject? = JSON.parse(content) {
      println(json)
      println(JSON.stringify(json!))
      exit(0)
    }
  }
} else {
  println("\(C_ARGV[0]) url")
}
exit(-1)
