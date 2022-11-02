//
//  Update.m
//  ReiKey
//
//  Created by Patrick Wardle on 12/24/18.
//  Copyright © 2018 Objective-See. All rights reserved.
//

#import "consts.h"
#import "Update.h"
#import "utilities.h"
#import "AppDelegate.h"

@implementation Update

//check for an update
// ->will invoke app delegate method to update UI when check completes
-(void)checkForUpdate:(void (^)(NSUInteger result, NSString* latestVersion))completionHandler
{
    //latest version
    __block NSString* latestVersion = nil;
    
    //result
    __block NSInteger result = -1;

    //get latest version in background
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        
        //grab latest version
        latestVersion = [self getLatestVersion];
        if(nil != latestVersion)
        {
            //check
            result = (NSOrderedAscending == [getAppVersion() compare:latestVersion options:NSNumericSearch]);
        }
        
        //invoke app delegate method
        // ->will update UI/show popup if necessart
        dispatch_async(dispatch_get_main_queue(),
        ^{
            completionHandler(result, latestVersion);
        });
        
    });
    
    return;
}

//query interwebz to get latest version
-(NSString*)getLatestVersion
{
    //product version(s) data
    NSData* productsVersionData = nil;
    
    //version dictionary
    NSDictionary* productsVersionDictionary = nil;
    
    //latest version
    NSString* latestVersion = nil;
    
    //get version from remote URL
    productsVersionData = [[NSData alloc] initWithContentsOfURL:[NSURL URLWithString:PRODUCT_VERSIONS_URL]];
    if(nil == productsVersionData)
    {
        //bail
        goto bail;
    }
    
    //convert JSON to dictionary
    // ->wrap as may throw exception
    @try
    {
        //convert
        productsVersionDictionary = [NSJSONSerialization JSONObjectWithData:productsVersionData options:0 error:nil];
        if(nil == productsVersionDictionary)
        {
            //bail
            goto bail;
        }
    }
    @catch(NSException* exception)
    {
        //bail
        goto bail;
    }
    
    //extract latest version
    latestVersion = [[productsVersionDictionary objectForKey:PRODUCT_NAME] objectForKey:@"version"];
    
bail:
    
    return latestVersion;
}

@end
