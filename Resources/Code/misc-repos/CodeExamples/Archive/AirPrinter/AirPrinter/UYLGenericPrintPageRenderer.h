//
//  UYLGenericPrintPageRenderer.h
//
//  Created by Keith Harrison on 31 July 2011 http://useyourloaf.com
//  Copyright (c) 2011 Keith Harrison. All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are met:
//
//  Redistributions of source code must retain the above copyright
//  notice, this list of conditions and the following disclaimer.
//
//  Redistributions in binary form must reproduce the above copyright
//  notice, this list of conditions and the following disclaimer in the
//  documentation and/or other materials provided with the distribution.
//
//  Neither the name of Keith Harrison nor the names of its contributors
//  may be used to endorse or promote products derived from this software
//  without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER ''AS IS'' AND ANY
//  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
//  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
//  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
//  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
//  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
//  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@interface UYLGenericPrintPageRenderer : UIPrintPageRenderer;

@property (nonatomic, copy) NSString *headerText;
@property (nonatomic, copy) NSString *footerText;

#define HEADER_FONT_SIZE 14
#define HEADER_TOP_PADDING 5
#define HEADER_BOTTOM_PADDING 10
#define HEADER_RIGHT_PADDING 5
#define HEADER_LEFT_PADDING 5

#define FOOTER_FONT_SIZE 12
#define FOOTER_TOP_PADDING 10
#define FOOTER_BOTTOM_PADDING 5
#define FOOTER_RIGHT_PADDING 5
#define FOOTER_LEFT_PADDING 5

@end
