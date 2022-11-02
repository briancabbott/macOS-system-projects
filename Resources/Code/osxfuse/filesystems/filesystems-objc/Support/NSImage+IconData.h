/*
 * Copyright (C) 2007 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#import <AppKit/NSImage.h>

@class NSData;

@interface NSImage (IconData)

// Creates the data for a .icns file from this NSImage.  You can use a width
// of 128, 256 or 512 pixels (128 and 512 only supported on Leopard and later).
- (NSData *)icnsDataWithWidth:(int)width;

@end
