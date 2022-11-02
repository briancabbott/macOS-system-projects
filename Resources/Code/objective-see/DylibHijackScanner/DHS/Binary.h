//
//  Binary.h
//  DHS
//
//  Created by Patrick Wardle on 2/6/15.
//  Copyright (c) 2015 Objective-See, LLC. All rights reserved.
//

#import "MachO.h"
#import <Foundation/Foundation.h>

@interface Binary : NSObject
{
    
}

//path (on disk)
@property(nonatomic, retain)NSString* path;

//instance of mach-O parser
@property(nonatomic, retain)MachO* parserInstance;

//run paths
@property(nonatomic, retain)NSMutableArray* lcRPATHS;




//vulnerable
@property BOOL isVulnerable;

//hijacked
@property BOOL isHijacked;

//type of vulnerable/hijack
// ->either rpath or weak
@property NSUInteger issueType;

//path to dylib that could or is causing the issue
@property NSString* issueItem;

/* METHODS */

//init with a path
-(id)initWithPath:(NSString*)binaryPath;

//get the machO type from the machO parser instance
// ->just grab from first header (should all by the same)
-(uint32_t)getType;

//convert object to JSON string
-(NSString*)toJSON;


@end
