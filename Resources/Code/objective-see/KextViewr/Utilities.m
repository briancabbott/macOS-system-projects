//
//  Utilities.m
//  DHS
//
//  Created by Patrick Wardle on 2/7/15.
//  Copyright (c) 2015 Objective-See. All rights reserved.
//

#import "Consts.h"
#import "Utilities.h"

#import <signal.h>
#import <unistd.h>
#import <syslog.h>
#import <libproc.h>
#import <sys/sysctl.h>
#import <Security/Security.h>
#import <Foundation/Foundation.h>
#import <CommonCrypto/CommonDigest.h>
#import <SystemConfiguration/SystemConfiguration.h>

//check if OS is supported
// ->Lion and newer
BOOL isSupportedOS()
{
    //return
    BOOL isSupported = NO;
    
    //major version
    SInt32 versionMajor = 0;
    
    //minor version
    SInt32 versionMinor = 0;
    
    //get major version
    versionMajor = getVersion(gestaltSystemVersionMajor);
    
    //get minor version
    versionMinor = getVersion(gestaltSystemVersionMinor);
    
    //sanity check
    if( (-1 == versionMajor) ||
        (-1 == versionMinor) )
    {
        //err
        goto bail;
    }
    
    //check that OS is supported
    // ->10.8+ ?
    if( (versionMajor == OS_MAJOR_VERSION_X) &&
        (versionMinor >= OS_MINOR_VERSION_LION) )
    {
        //set flag
        isSupported = YES;
    }
    
//bail
bail:
    
    return isSupported;
}

//get OS's major or minor version
SInt32 getVersion(OSType selector)
{
    //version
    // ->major or minor
    SInt32 version = -1;
    
    //get version info
    if(noErr != Gestalt(selector, &version))
    {
        //reset version
        version = -1;
        
        //err
        goto bail;
    }
    
//bail
bail:
    
    return version;
}


//get the signing info of a file
NSDictionary* extractSigningInfo(NSString* path)
{
    //info dictionary
    NSMutableDictionary* signingStatus = nil;
    
    //code
    SecStaticCodeRef staticCode = NULL;
    
    //status
    OSStatus status = !STATUS_SUCCESS;
    
    //signing information
    CFDictionaryRef signingInformation = NULL;
    
    //cert chain
    NSArray* certificateChain = nil;
    
    //index
    NSUInteger index = 0;
    
    //cert
    SecCertificateRef certificate = NULL;
    
    //common name on chert
    CFStringRef commonName = NULL;
    
    //init signing status
    signingStatus = [NSMutableDictionary dictionary];
    
    //create static code
    status = SecStaticCodeCreateWithPath((__bridge CFURLRef)([NSURL fileURLWithPath:path]), kSecCSDefaultFlags, &staticCode);
    
    //save signature status
    signingStatus[KEY_SIGNATURE_STATUS] = [NSNumber numberWithInt:status];
    
    //sanity check
    if(STATUS_SUCCESS != status)
    {
        //err msg
        //syslog(LOG_ERR, "OBJECTIVE-SEE ERROR: SecStaticCodeCreateWithPath() failed on %s with %d", [path UTF8String], status);
        
        //bail
        goto bail;
    }
    
    //check signature
    status = SecStaticCodeCheckValidityWithErrors(staticCode, kSecCSDoNotValidateResources, NULL, NULL);
    
    //(re)save signature status
    signingStatus[KEY_SIGNATURE_STATUS] = [NSNumber numberWithInt:status];
    
    //if file is signed
    // ->grab signing authorities
    if(STATUS_SUCCESS == status)
    {
        //grab signing authorities
        status = SecCodeCopySigningInformation(staticCode, kSecCSSigningInformation, &signingInformation);
        
        //sanity check
        if(STATUS_SUCCESS != status)
        {
            //err msg
            syslog(LOG_ERR, "OBJECTIVE-SEE ERROR: SecCodeCopySigningInformation() failed on %s with %d", [path UTF8String], status);
            
            //bail
            goto bail;
        }
        
        //determine if binary is signed by Apple
        signingStatus[KEY_SIGNING_IS_APPLE] = [NSNumber numberWithBool:isApple(path)];
    }
    //error
    // ->not signed, or something else, so no need to check cert's names
    else
    {
        //err msg
        syslog(LOG_ERR, "OBJECTIVE-SEE ERROR: SecStaticCodeCheckValidityWithErrors() failed on %s with %d", [path UTF8String], status);
        
        //bail
        goto bail;
    }
    
    //init array for certificate names
    signingStatus[KEY_SIGNING_AUTHORITIES] = [NSMutableArray array];
    
    //get cert chain
    certificateChain = [(__bridge NSDictionary*)signingInformation objectForKey:(__bridge NSString*)kSecCodeInfoCertificates];
    
    //handle case there is no cert chain
    // ->adhoc? (/Library/Frameworks/OpenVPN.framework/Versions/Current/bin/openvpn-service)
    if(0 == certificateChain.count)
    {
        //set
        [signingStatus[KEY_SIGNING_AUTHORITIES] addObject:@"signed, but no signing authorities (adhoc?)"];
    }
    
    //got cert chain
    // ->add each to list
    else
    {
        //get name of all certs
        for(index = 0; index < certificateChain.count; index++)
        {
            //extract cert
            certificate = (__bridge SecCertificateRef)([certificateChain objectAtIndex:index]);
            
            //get common name
            status = SecCertificateCopyCommonName(certificate, &commonName);
            
            //skip ones that error out
            if( (STATUS_SUCCESS != status) ||
                (NULL == commonName))
            {
                //skip
                continue;
            }
            
            //save
            [signingStatus[KEY_SIGNING_AUTHORITIES] addObject:(__bridge NSString*)commonName];
            
            //release name
            CFRelease(commonName);
        }
    }
    
//bail
bail:
    
    //free signing info
    if(NULL != signingInformation)
    {
        //free
        CFRelease(signingInformation);
    }
    
    //free static code
    if(NULL != staticCode)
    {
        //free
        CFRelease(staticCode);
    }
    
    return signingStatus;
}

//determine if a file is signed by Apple proper
BOOL isApple(NSString* path)
{
    //flag
    BOOL isApple = NO;
    
    //code
    SecStaticCodeRef staticCode = NULL;
    
    //signing reqs
    SecRequirementRef requirementRef = NULL;
    
    //status
    OSStatus status = -1;
    
    //create static code
    status = SecStaticCodeCreateWithPath((__bridge CFURLRef)([NSURL fileURLWithPath:path]), kSecCSDefaultFlags, &staticCode);
    if(STATUS_SUCCESS != status)
    {
        //err msg
        syslog(LOG_ERR, "OBJECTIVE-SEE ERROR: SecStaticCodeCreateWithPath() failed on %s with %d", [path UTF8String], status);
        
        //bail
        goto bail;
    }
    
    //create req string w/ 'anchor apple'
    // (3rd party: 'anchor apple generic')
    status = SecRequirementCreateWithString(CFSTR("anchor apple"), kSecCSDefaultFlags, &requirementRef);
    if( (STATUS_SUCCESS != status) ||
        (requirementRef == NULL) )
    {
        //err msg
        syslog(LOG_ERR, "OBJECTIVE-SEE ERROR: SecRequirementCreateWithString() failed on %s with %d", [path UTF8String], status);
        
        //bail
        goto bail;
    }
    
    //check if file is signed by apple by checking if it conforms to req string
    // note: ignore 'errSecCSBadResource' as lots of signed apple files return this issue :/
    status = SecStaticCodeCheckValidity(staticCode, kSecCSDefaultFlags, requirementRef);
    if( (STATUS_SUCCESS != status) &&
        (errSecCSBadResource != status) )
    {
        //bail
        // ->just means app isn't signed by apple
        goto bail;
    }
    
    //ok, happy (SecStaticCodeCheckValidity() didn't fail)
    // ->file is signed by Apple
    isApple = YES;
    
//bail
bail:
    
    //free req reference
    if(NULL != requirementRef)
    {
        //free
        CFRelease(requirementRef);
    }

    //free static code
    if(NULL != staticCode)
    {
        //free
        CFRelease(staticCode);
    }

    
    return isApple;
}

//hash a file
// ->md5 and sha1
NSDictionary* hashFile(NSString* filePath)
{
    //file hashes
    NSDictionary* hashes = nil;
    
    //file's contents
    NSData* fileContents = nil;
    
    //hash digest (md5)
    uint8_t digestMD5[CC_MD5_DIGEST_LENGTH] = {0};
    
    //md5 hash as string
    NSMutableString* md5 = nil;
    
    //hash digest (sha1)
    uint8_t digestSHA1[CC_SHA1_DIGEST_LENGTH] = {0};
    
    //sha1 hash as string
    NSMutableString* sha1 = nil;
    
    //index var
    NSUInteger index = 0;
    
    //init md5 hash string
    md5 = [NSMutableString string];
    
    //init sha1 hash string
    sha1 = [NSMutableString string];
    
    //load file
    if(nil == (fileContents = [NSData dataWithContentsOfFile:filePath]))
    {
        //bail
        goto bail;
    }
    
    //md5 it
    CC_MD5(fileContents.bytes, (unsigned int)fileContents.length, digestMD5);
    
    //convert to NSString
    // ->iterate over each bytes in computed digest and format
    for(index=0; index < CC_MD5_DIGEST_LENGTH; index++)
    {
        //format/append
        [md5 appendFormat:@"%02lX", (unsigned long)digestMD5[index]];
    }
    
    //sha1 it
    CC_SHA1(fileContents.bytes, (unsigned int)fileContents.length, digestSHA1);
    
    //convert to NSString
    // ->iterate over each bytes in computed digest and format
    for(index=0; index < CC_SHA1_DIGEST_LENGTH; index++)
    {
        //format/append
        [sha1 appendFormat:@"%02lX", (unsigned long)digestSHA1[index]];
    }
    
    //init hash dictionary
    hashes = @{KEY_HASH_MD5: md5, KEY_HASH_SHA1: sha1};
    
//bail
bail:
    
    return hashes;
}

//get app's version
// ->extracted from Info.plist
NSString* getAppVersion()
{
    //read and return 'CFBundleVersion' from bundle
    return [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleVersion"];
}

//convert a textview to a clickable hyperlink
void makeTextViewHyperlink(NSTextField* textField, NSURL* url)
{
    //hyperlink
    NSMutableAttributedString *hyperlinkString = nil;
    
    //range
    NSRange range = {0};
    
    //init hyper link
    hyperlinkString = [[NSMutableAttributedString alloc] initWithString:textField.stringValue];
    
    //init range
    range = NSMakeRange(0, [hyperlinkString length]);
   
    //start editing
    [hyperlinkString beginEditing];
    
    //add url
    [hyperlinkString addAttribute:NSLinkAttributeName value:url range:range];
    
    //make it blue
    [hyperlinkString addAttribute:NSForegroundColorAttributeName value:[NSColor blueColor] range:NSMakeRange(0, [hyperlinkString length])];
    
    //underline
    [hyperlinkString addAttribute:
     NSUnderlineStyleAttributeName value:[NSNumber numberWithInt:NSSingleUnderlineStyle] range:NSMakeRange(0, [hyperlinkString length])];
    
    //done editing
    [hyperlinkString endEditing];
    
    //set text
    [textField setAttributedStringValue:hyperlinkString];
    
    return;
}

//set the color of an attributed string
NSMutableAttributedString* setStringColor(NSAttributedString* string, NSColor* color)
{
    //colored string
    NSMutableAttributedString *coloredString = nil;

    //alloc/init colored string from existing one
    coloredString = [[NSMutableAttributedString alloc] initWithAttributedString:string];
    
    //set color
    [coloredString addAttribute:NSForegroundColorAttributeName value:color range:NSMakeRange(0, [coloredString length])];
    
    return coloredString;
}

//exec a process and grab it's output
NSData* execTask(NSString* binaryPath, NSArray* arguments)
{
    //task
    NSTask *task = nil;
    
    //output pipe
    NSPipe *outPipe = nil;
    
    //read handle
    NSFileHandle* readHandle = nil;
    
    //output
    NSMutableData *output = nil;
    
    //init task
    task = [NSTask new];
    
    //init output pipe
    outPipe = [NSPipe pipe];
    
    //init read handle
    readHandle = [outPipe fileHandleForReading];
    
    //init output buffer
    output = [NSMutableData data];
    
    //set task's path
    [task setLaunchPath:binaryPath];
    
    //set task's args
    [task setArguments:arguments];
    
    //set task's output
    [task setStandardOutput:outPipe];

    //wrap task launch
    @try {
        
        //launch
        [task launch];
    }
    @catch(NSException *exception)
    {
        //err msg
        //syslog(LOG_ERR, "OBJECTIVE-SEE ERROR: taskExec(%s) failed with %s", [binaryPath UTF8String], [[exception description] UTF8String]);
        
        //bail
        goto bail;
    }
    
    //read in output
    while(YES == [task isRunning])
    {
        //accumulate output
        [output appendData:[readHandle readDataToEndOfFile]];
    }
    
    //grab any left over data
    [output appendData:[readHandle readDataToEndOfFile]];
    
//bail
bail:
    
    return output;
}

//wait until a window is non nil
// ->then make it modal
void makeModal(NSWindowController* windowController)
{
    //wait up to 1 second window to be non-nil
    // ->then make modal
    for(int i=0; i<20; i++)
    {
        //can make it modal once we have a window
        if(nil != windowController.window)
        {
            //make modal on main thread
            dispatch_sync(dispatch_get_main_queue(), ^{
                
                //modal
                [[NSApplication sharedApplication] runModalForWindow:windowController.window];
        
            });
            
            //all done
            break;
        }
        
        //nap
        [NSThread sleepForTimeInterval:0.05f];

    }//until 1 second
    
    return;
}

//check if computer has network connection
BOOL isNetworkConnected()
{
    //flag
    BOOL isConnected = NO;
    
    //sock addr stuct
    struct sockaddr zeroAddress = {0};
    
    //reachability ref
    SCNetworkReachabilityRef reachabilityRef = NULL;
    
    //reachability flags
    SCNetworkReachabilityFlags flags = 0;
    
    //reachable flag
    BOOL isReachable = NO;
    
    //connection required flag
    BOOL connectionRequired = NO;
    
    //ensure its cleared out
    bzero(&zeroAddress, sizeof(zeroAddress));
    
    //set size
    zeroAddress.sa_len = sizeof(zeroAddress);
    
    //set family
    zeroAddress.sa_family = AF_INET;
    
    //create reachability ref
    reachabilityRef = SCNetworkReachabilityCreateWithAddress(NULL, (const struct sockaddr*)&zeroAddress);
    
    //sanity check
    if(NULL == reachabilityRef)
    {
        //bail
        goto bail;
    }
    
    //get flags
    if(TRUE != SCNetworkReachabilityGetFlags(reachabilityRef, &flags))
    {
        //bail
        goto bail;
    }
    
    //set reachable flag
    isReachable = ((flags & kSCNetworkFlagsReachable) != 0);
    
    //set connection required flag
    connectionRequired = ((flags & kSCNetworkFlagsConnectionRequired) != 0);
    
    //finally
    // ->determine if network is available
    isConnected = (isReachable && !connectionRequired) ? YES : NO;
    
//bail
bail:
    
    //cleanup
    if(NULL != reachabilityRef)
    {
        //release
        CFRelease(reachabilityRef);
    }
    
    return isConnected;
}

//set or unset button's highlight
void buttonAppearance(NSTableView* table, NSEvent* event, BOOL shouldReset)
{
    //mouse point
    NSPoint mousePoint = {0};
    
    //row index
    NSUInteger rowIndex = -1;
    
    //current row
    NSTableCellView* currentRow = nil;
    
    //tag
    NSUInteger tag = 0;
    
    //button
    NSButton* button = nil;
    
    //image name
    NSString* imageName =  nil;
    
    //extract tag
    tag = [((NSDictionary*)event.userData)[@"tag"] unsignedIntegerValue];
    
    //restore button back to default (visual) state
    if(YES == shouldReset)
    {
        //set image name
        // ->'info'
        if(TABLE_ROW_INFO_BUTTON == tag)
        {
            //set
            imageName = @"info";
        }
        //set image name
        // ->'info'
        else if(TABLE_ROW_SHOW_BUTTON == tag)
        {
            //set
            imageName = @"show";
        }
    }
    //highlight button
    else
    {
        //set image name
        // ->'info'
        if(TABLE_ROW_INFO_BUTTON == tag)
        {
            //set
            imageName = @"infoOver";
        }
        //set image name
        // ->'info'
        else if(TABLE_ROW_SHOW_BUTTON == tag)
        {
            //set
            imageName = @"showOver";
        }
    }
    
    //grab mouse point
    mousePoint = [table convertPoint:[event locationInWindow] fromView:nil];
    
    //compute row indow
    rowIndex = [table rowAtPoint:mousePoint];
    
    //sanity check
    if(-1 == rowIndex)
    {
        //bail
        goto bail;
    }
    
    //get row that's about to be selected
    currentRow = [table viewAtColumn:0 row:rowIndex makeIfNecessary:YES];
    
    //get button
    // ->tag id of button, passed in userData var
    button = [currentRow viewWithTag:[((NSDictionary*)event.userData)[@"tag"] unsignedIntegerValue]];
    
    //restore default button image
    // ->for 'info' and 'show' buttons
    if(nil != imageName)
    {
        //set image
        [button setImage:[NSImage imageNamed:imageName]];
    }
    
//bail
bail:
    
    return;
}
    

