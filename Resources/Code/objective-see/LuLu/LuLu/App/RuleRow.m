//
//  file: RuleRow.m
//  project: lulu (main app)
//  description: row for 'rules' table
//
//  created by Patrick Wardle
//  copyright (c) 2017 Objective-See. All rights reserved.
//

#import "RuleRow.h"

@implementation RuleRow

//custom row selection
-(void)drawSelectionInRect:(NSRect)dirtyRect
{
    //selection rect
    NSRect selectionRect = {0};
    
    //selection path
    NSBezierPath* selectionPath = nil;
    
    //selection color
    NSColor* selectionColor = nil;
    
    //highlight selected rows
    if(self.selectionHighlightStyle != NSTableViewSelectionHighlightStyleNone)
    {
        //make selection rect
        selectionRect = NSInsetRect(self.bounds, 2.5, 2.5);
        
        //set color
        selectionColor = [NSColor unemphasizedSelectedContentBackgroundColor];
        
        //set stroke
        [selectionColor setStroke];
        
        //set fill
        [selectionColor setFill];
        
        //create selection path
        // with rounded corners (5x5)
        selectionPath = [NSBezierPath bezierPathWithRoundedRect:selectionRect xRadius:5 yRadius:5];
        
        //fill
        [selectionPath fill];
        
        //stroke
        [selectionPath stroke];
    }
    
    return;
}

@end
