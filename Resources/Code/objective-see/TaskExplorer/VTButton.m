//
//  vtButton.m
//  TaskExplorer
//
//  Created by Patrick Wardle on 3/26/15.
//  Copyright (c) 2015 Objective-See. All rights reserved.
//

#import "Consts.h"
#import "VTButton.h"
#import "Utilities.h"
#import "TaskTableController.h"

@implementation VTButton

@synthesize binary;
@synthesize delegate;
@synthesize mouseDown;
@synthesize mouseExit;

//automatically invoked
// ->create tracking area for mouse events
-(void)awakeFromNib
{
    //create the mouse-over tracking area
    [self createTrackingArea];
}

//create the mouse-over tracking area
-(void)createTrackingArea
{
    //tracking area
    NSTrackingArea *trackingArea = nil;
    
    //alloc/init tracking area
    trackingArea = [[NSTrackingArea alloc] initWithRect:NSZeroRect options:(NSTrackingInVisibleRect  | NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways) owner:self userInfo:nil];
    
    //add tracking area
    [self addTrackingArea:trackingArea];
    
    return;
}

//automatically invoked when mouse-down occurs
// ->set color to default or red
-(void)mouseDown:(NSEvent *)theEvent;
{
    //mouse down/over color
    NSColor* color = nil;
    
    //set flag
    self.mouseDown = YES;
    
    //flagged files
    // ->make em red!
    if( (nil != self.binary.vtInfo) &&
        (0 != [self.binary.vtInfo[VT_RESULTS_POSITIVES] unsignedIntegerValue]) )
    {
        //set color (light red)
        color = [NSColor colorWithCalibratedRed:(255/255.0f) green:(1.0/255.0f) blue:(1.0/255.0f) alpha:0.5];
    }
    //non-flagged files
    else
    {
        //default
        color = NSColor.controlTextColor;
    }
    
    //set string
    [self setAttributedTitle:setStringColor(self.attributedTitle, color)];
 
    return;
}

//automatically invoked when mouse-up occurs
// ->reset color to default or red and trigger mouse click logic (if necessary)
-(void)mouseUp:(NSEvent *)theEvent;
{
    //mouse up color
    NSColor* color = nil;
    
    //shoud treat at click?
    // ->mouse up inside in non-disabled button
    if( (YES == self.isEnabled) &&
        (YES != self.mouseExit) )
    {
        //show virus total window
        [self.delegate performSelector:@selector(showVTInfo:) withObject:self];
    }
    
    //reset flag
    self.mouseDown = NO;

    //flagged files
    // ->make em red!
    if( (nil != self.binary.vtInfo) &&
        (0 != [self.binary.vtInfo[VT_RESULTS_POSITIVES] unsignedIntegerValue]) )
    {
        //set color
        color = [NSColor redColor];
    }
    //non-flagged files
    // set (back) to default
    else
    {
        color = NSColor.controlTextColor;
    }
    
    //set string
    [self setAttributedTitle:setStringColor(self.attributedTitle, color)];
    
    return;
}

//automatically invoked when mouse enters
// ->set mouse over color
-(void)mouseEntered:(NSEvent*)theEvent
{
    //mouse entered color
    NSColor* color = nil;
    
    //set flag
    self.mouseExit = NO;
    
    //flagged files
    // ->make em red!
    if( (nil != self.binary.vtInfo) &&
        (0 != [self.binary.vtInfo[VT_RESULTS_POSITIVES] unsignedIntegerValue]) )
    {
        //set color (lightish red)
        color = [NSColor colorWithCalibratedRed:(255/255.0f) green:(1.0/255.0f) blue:(1.0/255.0f) alpha:0.66];
    }
    //non-flagged files
    // set (back) to default
    else
    {
        //default
        color = NSColor.controlTextColor;
    }
    
    //set string
    [self setAttributedTitle:setStringColor(self.attributedTitle, color)];
    
    return;
}

//automatically invoked when mouse exits
// ->reset color to black or red
-(void)mouseExited:(NSEvent*)theEvent
{
    //mouse exit color
    NSColor* color = nil;
 
    //set flag
    self.mouseExit = YES;
    
    //check if mouse is down
    // ->set color to default/red
    if(YES == self.mouseDown)
    {
        //flagged files
        // ->make em red!
        if( (nil != self.binary.vtInfo) &&
            (0 != [self.binary.vtInfo[VT_RESULTS_POSITIVES] unsignedIntegerValue]) )
        {
            //set color (lightish red)
            color = [NSColor colorWithCalibratedRed:(255/255.0f) green:(1.0/255.0f) blue:(1.0/255.0f) alpha:0.66];
        }
        //non-flagged files
        else
        {
            //default
            color = NSColor.controlTextColor;
        }
    }
    //mouse is up
    // ->reset color to black/red
    else
    {
        //flagged files
        // ->make em red!
        if( (nil != self.binary.vtInfo) &&
            (0 != [self.binary.vtInfo[VT_RESULTS_POSITIVES] unsignedIntegerValue]) )
        {
            //set color (light red)
            color = [NSColor redColor];
        }
        //non-flagged files
        // set (back) to default
        else
        {
            //default
            color = NSColor.controlTextColor;
        }
    }
    
    //set string
    [self setAttributedTitle:setStringColor(self.attributedTitle, color)];
    
    return;
}

@end
