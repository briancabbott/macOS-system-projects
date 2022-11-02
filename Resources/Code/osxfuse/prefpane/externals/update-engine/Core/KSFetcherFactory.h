// Copyright 2008 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#import <Foundation/Foundation.h>

@class GTMHTTPFetcher;

// A factory class that creates GTMHTTPFetcher objects.  We pass this
// to a KSUpdateChecker.  Since a KSUpdateChecker may need more than
// one GTMHTTPFetcher (depending on the KSServer implementation), we
// must pass in a factory instead of just a GTMHTTPFetcher.
@interface KSFetcherFactory : NSObject

// Returns an autoreleased instance of KSFetcherFactory
+ (KSFetcherFactory *)factory;

// Returns an autoreleased object compatible with GTMHTTPFetcher,
// initialized with the given NSURLRequest.
- (GTMHTTPFetcher *)createFetcherForRequest:(NSURLRequest *)request;

@end
