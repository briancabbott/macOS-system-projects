/* Copyright (c) 2007 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//
//  DocsSampleWindowController.m
//

#import "DocsSampleWindowController.h"

#import "GData/GTMOAuth2WindowController.h"

enum {
  // upload pop-up menu items
  kUploadAsGoogleDoc = 0,
  kUploadOriginal = 1,
  kUploadOCR = 2,
  kUploadDE = 3,
  kUploadJA = 4,
  kUploadEN = 5
};

@interface DocsSampleWindowController (PrivateMethods)
- (void)updateUI;
- (void)updateChangeFolderPopup;
- (void)updateSelectedDocumentThumbnailImage;
- (void)imageFetcher:(GTMHTTPFetcher *)fetcher finishedWithData:(NSData *)data error:(NSError *)error;

- (void)fetchDocList;
- (void)fetchRevisionsForSelectedDoc;

- (void)uploadFileAtPath:(NSString *)path;
- (void)showDownloadPanelForEntry:(GDataEntryBase *)entry suggestedTitle:(NSString *)title;
- (void)saveDocumentEntry:(GDataEntryBase *)docEntry toPath:(NSString *)path;
- (void)saveDocEntry:(GDataEntryBase *)entry toPath:(NSString *)savePath exportFormat:(NSString *)exportFormat authService:(GDataServiceGoogle *)service;

- (GDataServiceGoogleDocs *)docsService;
- (GDataEntryDocBase *)selectedDoc;
- (GDataEntryDocRevision *)selectedRevision;

- (GDataFeedDocList *)docListFeed;
- (void)setDocListFeed:(GDataFeedDocList *)feed;
- (NSError *)docListFetchError;
- (void)setDocListFetchError:(NSError *)error;
- (GDataServiceTicket *)docListFetchTicket;
- (void)setDocListFetchTicket:(GDataServiceTicket *)ticket;

- (GDataFeedDocRevision *)revisionFeed;
- (void)setRevisionFeed:(GDataFeedDocRevision *)feed;
- (NSError *)revisionFetchError;
- (void)setRevisionFetchError:(NSError *)error;
- (GDataServiceTicket *)revisionFetchTicket;
- (void)setRevisionFetchTicket:(GDataServiceTicket *)ticket;

- (GDataEntryDocListMetadata *)metadataEntry;
- (void)setMetadataEntry:(GDataEntryDocListMetadata *)entry;

- (GDataServiceTicket *)uploadTicket;
- (void)setUploadTicket:(GDataServiceTicket *)ticket;

- (void)displayAlert:(NSString *)title format:(NSString *)format, ...;
@end

@implementation DocsSampleWindowController

static NSString *const kKeychainItemName = @"DocsSample: Google Docs";

static DocsSampleWindowController* gDocsSampleWindowController = nil;

+ (DocsSampleWindowController *)sharedDocsSampleWindowController {

  if (!gDocsSampleWindowController) {
    gDocsSampleWindowController = [[DocsSampleWindowController alloc] init];
  }
  return gDocsSampleWindowController;
}


- (id)init {
  return [self initWithWindowNibName:@"DocsSampleWindow"];
}

- (void)windowDidLoad {
}

- (void)awakeFromNib {
  // Load the OAuth token from the keychain, if it was previously saved
  NSString *clientID = [mClientIDField stringValue];
  NSString *clientSecret = [mClientSecretField stringValue];

  GTMOAuth2Authentication *auth;
  auth = [GTMOAuth2WindowController authForGoogleFromKeychainForName:kKeychainItemName
                                                            clientID:clientID
                                                        clientSecret:clientSecret];
  [[self docsService] setAuthorizer:auth];

  // Set the result text field to have a distinctive color and mono-spaced font
  // to aid in understanding of each operation.
  [mDocListResultTextField setTextColor:[NSColor darkGrayColor]];

  NSFont *resultTextFont = [NSFont fontWithName:@"Monaco" size:9];
  [mDocListResultTextField setFont:resultTextFont];

  [self updateUI];
}

- (void)dealloc {
  [mDocListFeed release];
  [mDocListFetchTicket release];
  [mDocListFetchError release];

  [mRevisionFeed release];
  [mRevisionFetchTicket release];
  [mRevisionFetchError release];

  [mMetadataEntry release];

  [mUploadTicket cancelTicket];
  [mUploadTicket release];

  [super dealloc];
}

#pragma mark -

- (NSString *)signedInUsername {
  // Get the email address of the signed-in user
  GTMOAuth2Authentication *auth = [[self docsService] authorizer];
  BOOL isSignedIn = auth.canAuthorize;
  if (isSignedIn) {
    return auth.userEmail;
  } else {
    return nil;
  }
}

- (BOOL)isSignedIn {
  NSString *name = [self signedInUsername];
  return (name != nil);
}

- (void)runSigninThenInvokeSelector:(SEL)signInDoneSel {
  // Applications should have client ID and client secret strings
  // hardcoded into the source, but the sample application asks the
  // developer for the strings
  NSString *clientID = [mClientIDField stringValue];
  NSString *clientSecret = [mClientSecretField stringValue];

  if ([clientID length] == 0 || [clientSecret length] == 0) {
    // Remind the developer that client ID and client secret are needed
    [mClientIDButton performSelector:@selector(performClick:)
                          withObject:self
                          afterDelay:0.5];
    return;
  }

  // Show the OAuth 2 sign-in controller
  NSString *scope = [GTMOAuth2Authentication scopeWithStrings:
                     [GDataServiceGoogleDocs authorizationScope],
                     [GDataServiceGoogleSpreadsheet authorizationScope],
                     nil];

  NSBundle *frameworkBundle = [NSBundle bundleForClass:[GTMOAuth2WindowController class]];
  GTMOAuth2WindowController *windowController;
  windowController = [[[GTMOAuth2WindowController alloc] initWithScope:scope
                                                              clientID:clientID
                                                          clientSecret:clientSecret
                                                      keychainItemName:kKeychainItemName
                                                        resourceBundle:frameworkBundle] autorelease];

  [windowController setUserData:NSStringFromSelector(signInDoneSel)];
  [windowController signInSheetModalForWindow:[self window]
                                     delegate:self
                             finishedSelector:@selector(windowController:finishedWithAuth:error:)];
}

- (void)windowController:(GTMOAuth2WindowController *)windowController
        finishedWithAuth:(GTMOAuth2Authentication *)auth
                   error:(NSError *)error {
  // Callback from OAuth 2 sign-in
  if (error == nil) {
    [[self docsService] setAuthorizer:auth];

    NSString *selStr = [windowController userData];
    if (selStr) {
      [self performSelector:NSSelectorFromString(selStr)];
    }
  } else {
    [self setDocListFetchError:error];
    [self updateUI];
  }
}

#pragma mark -

- (void)updateUI {
  BOOL isSignedIn = [self isSignedIn];
  NSString *username = [self signedInUsername];
  [mSignedInButton setTitle:(isSignedIn ? @"Sign Out" : @"Sign In")];
  [mSignedInField setStringValue:(isSignedIn ? username : @"No")];

  // docList list display
  [mDocListTable reloadData];

  GDataEntryDocBase *selectedDoc = [self selectedDoc];

  // spin indicator when retrieving feed
  BOOL isFetchingDocList = (mDocListFetchTicket != nil);
  if (isFetchingDocList) {
    [mDocListProgressIndicator startAnimation:self];
  } else {
    [mDocListProgressIndicator stopAnimation:self];
  }
  [mDocListCancelButton setEnabled:isFetchingDocList];

  // show the doclist feed fetch result error or the selected entry
  NSString *docResultStr = @"";
  if (mDocListFetchError) {
    docResultStr = [mDocListFetchError description];
  } else {
    if (selectedDoc) {
      docResultStr = [selectedDoc description];
    }
  }
  [mDocListResultTextField setString:docResultStr];

  [self updateSelectedDocumentThumbnailImage];

  // revision list display
  [mRevisionsTable reloadData];

  GDataEntryDocRevision *selectedRevision = [self selectedRevision];

  // spin indicator when retrieving feed
  BOOL isFetchingRevisions = (mRevisionFetchTicket != nil);
  if (isFetchingRevisions) {
    [mRevisionsProgressIndicator startAnimation:self];
  } else {
    [mRevisionsProgressIndicator stopAnimation:self];
  }
  [mRevisionsCancelButton setEnabled:isFetchingRevisions];

  // show the revision feed fetch result error or the selected entry
  NSString *revisionsResultStr = @"";
  if (mRevisionFetchError) {
    revisionsResultStr = [mRevisionFetchError description];
  } else {
    if (selectedRevision) {
      revisionsResultStr = [selectedRevision description];
    }
  }
  [mRevisionsResultTextField setString:revisionsResultStr];

  BOOL isSelectedDocAStandardGDocsType =
    [selectedDoc isKindOfClass:[GDataEntryStandardDoc class]]
    || [selectedDoc isKindOfClass:[GDataEntrySpreadsheetDoc class]]
    || [selectedDoc isKindOfClass:[GDataEntryPresentationDoc class]];

  // enable the button for viewing the selected doc in a browser
  BOOL doesDocHaveHTMLLink = ([selectedDoc HTMLLink] != nil);
  [mViewSelectedDocButton setEnabled:doesDocHaveHTMLLink];

  BOOL doesRevisionHaveExportURL = ([[[selectedRevision content] sourceURI] length] > 0);
  [mDownloadSelectedRevisionButton setEnabled:doesRevisionHaveExportURL];

  BOOL doesDocHaveExportURL = ([[[selectedDoc content] sourceURI] length] > 0);
  [mDownloadSelectedDocButton setEnabled:doesDocHaveExportURL];

  BOOL doesDocHaveEditLink = ([selectedDoc editLink] != nil);
  [mDeleteSelectedDocButton setEnabled:doesDocHaveEditLink];

  [mDuplicateSelectedDocButton setEnabled:isSelectedDocAStandardGDocsType];

  // enable the publishing checkboxes when a publishable revision is selected
  BOOL isRevisionSelected = (selectedRevision != nil);
  BOOL isRevisionPublishable = isRevisionSelected
    && isSelectedDocAStandardGDocsType;

  [mPublishCheckbox setEnabled:isRevisionPublishable];
  [mAutoRepublishCheckbox setEnabled:isRevisionPublishable];
  [mPublishOutsideDomainCheckbox setEnabled:isRevisionPublishable];

  // enable the "Update Publishing" button when the selected revision is
  // publishable and the checkbox settings differ from the current publishing
  // setting for the selected revision
  BOOL isPublished = [[selectedRevision publish] boolValue];
  BOOL isPublishedChecked = ([mPublishCheckbox state] == NSOnState);

  BOOL isAutoRepublished = [[selectedRevision publishAuto] boolValue];
  BOOL isAutoRepublishedChecked = ([mAutoRepublishCheckbox state] == NSOnState);

  BOOL isExternalPublished = [[selectedRevision publishOutsideDomain] boolValue];
  BOOL isExternalPublishedChecked = ([mPublishOutsideDomainCheckbox state] == NSOnState);

  BOOL canUpdatePublishing = isRevisionPublishable
    && ((isPublished != isPublishedChecked)
        || (isAutoRepublished != isAutoRepublishedChecked)
        || (isExternalPublished != isExternalPublishedChecked));

  [mUpdatePublishingButton setEnabled:canUpdatePublishing];

  // enable uploading buttons
  BOOL isUploading = (mUploadTicket != nil);
  BOOL canPostToFeed = ([mDocListFeed postLink] != nil);

  [mUploadFileButton setEnabled:(canPostToFeed && !isUploading)];
  [mStopUploadButton setEnabled:isUploading];
  [mPauseUploadButton setEnabled:isUploading];
  [mCreateFolderButton setEnabled:canPostToFeed];

  BOOL isUploadPaused = [mUploadTicket isUploadPaused];
  NSString *pauseTitle = (isUploadPaused ? @"Resume" : @"Pause");
  [mPauseUploadButton setTitle:pauseTitle];

  // enable the "Upload Original Document" menu item only if the user metadata
  // indicates support for generic file uploads
  GDataDocFeature *feature = [mMetadataEntry featureForName:kGDataDocsFeatureNameUploadAny];
  BOOL canUploadGenericDocs = (feature != nil);

  NSMenuItem *genericMenuItem = [[mUploadPopup menu] itemWithTag:kUploadOriginal];
  [genericMenuItem setEnabled:canUploadGenericDocs];

  // fill in the add-to-folder pop-up for the selected doc
  [self updateChangeFolderPopup];

  // show the title of the file currently uploading
  NSString *uploadingStr = @"";
  NSString *uploadingTitle = [[(GDataEntryBase *)
    [mDocListFetchTicket postedObject] title] stringValue];

  if (uploadingTitle) {
    uploadingStr = [NSString stringWithFormat:@"Uploading: %@", uploadingTitle];
  }
  [mUploadingTextField setStringValue:uploadingStr];

  // Show or hide the text indicating that the client ID or client secret are
  // needed
  BOOL hasClientIDStrings = [[mClientIDField stringValue] length] > 0
    && [[mClientSecretField stringValue] length] > 0;
  [mClientIDRequiredTextField setHidden:hasClientIDStrings];
}

- (void)updateChangeFolderPopup {

  // replace all menu items in the button with the folder titles and pointers
  // of the feed's folder entries, but preserve the pop-up's "Change Folder"
  // title as the first item

  NSString *title = [mFolderMembershipPopup title];

  NSMenu *addMenu = [[[NSMenu alloc] initWithTitle:title] autorelease];
  [addMenu setAutoenablesItems:NO];
  [addMenu addItemWithTitle:title action:nil keyEquivalent:@""];
  [mFolderMembershipPopup setMenu:addMenu];

  // get all folder entries
  NSArray *folderEntries = [mDocListFeed entriesWithCategoryKind:kGDataCategoryFolderDoc];

  // get hrefs of folders that already contain the selected doc
  GDataEntryDocBase *doc = [self selectedDoc];
  NSArray *parentLinks = [doc parentLinks];
  NSArray *parentHrefs = [parentLinks valueForKey:@"href"];

  // disable the pop-up if a folder entry is selected
  BOOL isMovableDocSelected = (doc != nil)
    && ![doc isKindOfClass:[GDataEntryFolderDoc class]];
  [mFolderMembershipPopup setEnabled:isMovableDocSelected];

  if (isMovableDocSelected) {
    // step through the folders in this feed, add them to the
    // pop-up, and add a checkmark to the names of folders that
    // contain the selected document
    NSEnumerator *folderEnum = [folderEntries objectEnumerator];
    GDataEntryFolderDoc *folderEntry;
    while ((folderEntry = [folderEnum nextObject]) != nil) {

      NSString *title = [[folderEntry title] stringValue];
      NSMenuItem *item = [addMenu addItemWithTitle:title
                                            action:@selector(changeFolderSelected:)
                                     keyEquivalent:@""];
      [item setTarget:self];
      [item setRepresentedObject:folderEntry];

      NSString *folderHref = [[folderEntry selfLink] href];

      BOOL shouldCheckItem = (folderHref != nil)
        && [parentHrefs containsObject:folderHref];
      [item setState:shouldCheckItem];
    }
  }
}

- (void)updateSelectedDocumentThumbnailImage {
  static NSString* priorImageURLStr = nil;

  GDataEntryDocBase *doc = [self selectedDoc];
  GDataLink *thumbnailLink = [doc thumbnailLink];
  NSString *newImageURLStr = [thumbnailLink href];

  if (!AreEqualOrBothNil(newImageURLStr, priorImageURLStr)) {
    // the image has changed
    priorImageURLStr = newImageURLStr;

    [mDocListImageView setImage:nil];

    if ([newImageURLStr length] > 0) {
      GTMHTTPFetcher *fetcher = [GTMHTTPFetcher fetcherWithURLString:newImageURLStr];
      [fetcher beginFetchWithDelegate:self
                    didFinishSelector:@selector(imageFetcher:finishedWithData:error:)];
    }
  }
}

- (void)imageFetcher:(GTMHTTPFetcher *)fetcher
    finishedWithData:(NSData *)data
               error:(NSError *)error {
  if (error == nil) {
    NSImage *image = [[[NSImage alloc] initWithData:data] autorelease];
    [mDocListImageView setImage:image];
  } else {
    NSLog(@"Error %@ loading image %@", error, [[fetcher mutableRequest] URL]);
  }
}

- (void)displayAlert:(NSString *)title format:(NSString *)format, ... {
  NSString *result = format;
  if (format) {
    va_list argList;
    va_start(argList, format);
    result = [[[NSString alloc] initWithFormat:format
                                     arguments:argList] autorelease];
    va_end(argList);
  }
  NSBeginAlertSheet(title, nil, nil, nil, [self window], nil, nil,
                    nil, nil, result);
}

#pragma mark IBActions

- (IBAction)signInClicked:(id)sender {
  if (![self isSignedIn]) {
    // Sign in
    [self runSigninThenInvokeSelector:@selector(updateUI)];
  } else {
    // Sign out
    GDataServiceGoogleDocs *service = [self docsService];

    [GTMOAuth2WindowController removeAuthFromKeychainForName:kKeychainItemName];
    [service setAuthorizer:nil];
    [self updateUI];
  }
}

- (IBAction)getDocListClicked:(id)sender {
  if (![self isSignedIn]) {
    [self runSigninThenInvokeSelector:@selector(fetchDocList)];
  } else {
    [self fetchDocList];
  }
}

- (IBAction)cancelDocListFetchClicked:(id)sender {
  [mDocListFetchTicket cancelTicket];
  [self setDocListFetchTicket:nil];
  [self updateUI];
}

- (IBAction)cancelRevisionsFetchClicked:(id)sender {
  [mRevisionFetchTicket cancelTicket];
  [self setRevisionFetchTicket:nil];
  [self updateUI];
}

- (IBAction)viewSelectedDocClicked:(id)sender {

  NSURL *docURL = [[[self selectedDoc] HTMLLink] URL];

  if (docURL) {
    [[NSWorkspace sharedWorkspace] openURL:docURL];
  } else {
    NSBeep();
  }
}

#pragma mark -

- (IBAction)downloadSelectedDocClicked:(id)sender {

  GDataEntryDocBase *docEntry = [self selectedDoc];

  NSString *saveTitle = [[[self selectedDoc] title] stringValue];

  [self showDownloadPanelForEntry:docEntry
                   suggestedTitle:saveTitle];
}

- (IBAction)downloadSelectedRevisionClicked:(id)sender {

  GDataEntryDocRevision *revisionEntry = [self selectedRevision];

  GDataEntryDocBase *docEntry = [self selectedDoc];

  NSString *docName = [[docEntry title] stringValue];
  NSString *revisionName = [[revisionEntry title] stringValue];
  NSString *saveTitle = [NSString stringWithFormat:@"%@ (%@)",
                         docName, revisionName];

  // the revision entry doesn't tell us the kind of document being saved, so
  // we'll explicitly put it into a property of the entry
  Class documentClass = [docEntry class];
  [revisionEntry setProperty:documentClass
                      forKey:@"document class"];

  [self showDownloadPanelForEntry:revisionEntry
                   suggestedTitle:saveTitle];
}

- (void)showDownloadPanelForEntry:(GDataEntryBase *)entry
                   suggestedTitle:(NSString *)title {

  NSString *sourceURI = [[entry content] sourceURI];
  if (sourceURI) {
    BOOL isDrawing = [entry isKindOfClass:[GDataEntryDrawingDoc class]];
    NSString *fileExtension = (isDrawing ? @"pdf" : @"txt");
    NSString *filename = [NSString stringWithFormat:@"%@.%@",
                          title, fileExtension];

    SEL endSel = @selector(saveSheetDidEnd:returnCode:contextInfo:);

    NSSavePanel *savePanel = [NSSavePanel savePanel];
    [savePanel beginSheetForDirectory:nil
                                 file:filename
                       modalForWindow:[self window]
                        modalDelegate:self
                       didEndSelector:endSel
                          contextInfo:[entry retain]];
  } else {
    NSBeep();
  }
}

- (void)saveSheetDidEnd:(NSOpenPanel *)panel returnCode:(int)returnCode contextInfo:(void *)contextInfo {

  GDataEntryBase *entry = [(GDataEntryBase *)contextInfo autorelease];

  if (returnCode == NSOKButton) {
    // user clicked OK
    NSString *savePath = [panel filename];
    [self saveDocumentEntry:entry
                     toPath:savePath];
  }
}

// formerly saveSelectedDocumentToPath:
- (void)saveDocumentEntry:(GDataEntryBase *)docEntry
                   toPath:(NSString *)savePath {
  // downloading docs, per
  // http://code.google.com/apis/documents/docs/3.0/developers_guide_protocol.html#DownloadingDocs

  // when downloading a revision entry, we've added a property above indicating
  // the class of document for which this is a revision
  Class classProperty = [docEntry propertyForKey:@"document class"];
  if (!classProperty) {
    classProperty = [docEntry class];
  }

  // since the user has already fetched the doc list, the service object
  // has the proper authentication token.
  GDataServiceGoogleDocs *docsService = [self docsService];

  BOOL isDrawing = [classProperty isEqual:[GDataEntryDrawingDoc class]];
  NSString *exportFormat = (isDrawing ? @"pdf" : @"txt");
  [self saveDocEntry:docEntry
              toPath:savePath
        exportFormat:exportFormat
         authService:docsService];
}

- (void)saveDocEntry:(GDataEntryBase *)entry
              toPath:(NSString *)savePath
        exportFormat:(NSString *)exportFormat
         authService:(GDataServiceGoogle *)service {

  // the content src attribute is used for downloading
  NSURL *exportURL = [[entry content] sourceURL];
  if (exportURL != nil) {

    // we'll use GDataQuery as a convenient way to append the exportFormat
    // parameter of the docs export API to the content src URL
    GDataQuery *query = [GDataQuery queryWithFeedURL:exportURL];
    [query addCustomParameterWithName:@"exportFormat"
                                value:exportFormat];
    NSURL *downloadURL = [query URL];

    // create a file for saving the document
    NSError *error = nil;
    if ([[NSData data] writeToFile:savePath
                           options:NSAtomicWrite
                             error:&error]) {
      NSFileHandle *fileHandle = [NSFileHandle fileHandleForWritingAtPath:savePath];

      // read the document's contents asynchronously from the network

      // requestForURL:ETag:httpMethod: sets the user agent header of the
      // request and, when using ClientLogin, adds the authorization header
      NSURLRequest *request = [service requestForURL:downloadURL
                                                ETag:nil
                                          httpMethod:nil];

      GTMHTTPFetcher *fetcher = [GTMHTTPFetcher fetcherWithRequest:request];
      [fetcher setAuthorizer:[service authorizer]];
      [fetcher setDownloadFileHandle:fileHandle];
      [fetcher beginFetchWithDelegate:self
                    didFinishSelector:@selector(fetcher:finishedWithData:error:)];
    } else {
      NSLog(@"Error creating file at %@: %@", savePath, error);
    }
  }
}

- (void)fetcher:(GTMHTTPFetcher *)fetcher finishedWithData:(NSData *)data error:(NSError *)error {
  if (error == nil) {
    // successfully saved the document
  } else {
    NSLog(@"Error saving document: %@", error);
    NSBeep();
  }
}

/* When signing in with ClientLogin, we need to create a SpreadsheetService
   instance to do an authenticated download of spreadsheet documents.
 
   Since this sample signs in with OAuth 2, which allows multiple scopes,
   we do not need to use a SpreadsheetService, but here is what it looks
   like for ClientLogin.

- (void)saveSpreadsheet:(GDataEntrySpreadsheetDoc *)docEntry
                 toPath:(NSString *)savePath {
  // to download a spreadsheet document, we need a spreadsheet service object,
  // and we first need to fetch a feed or entry with the service object so that
  // it has a valid auth token
  GDataServiceGoogleSpreadsheet *spreadsheetService;
  spreadsheetService = [[[GDataServiceGoogleSpreadsheet alloc] init] autorelease];

  GDataServiceGoogleDocs *docsService = [self docsService];
  [spreadsheetService setUserAgent:[docsService userAgent]];
  [spreadsheetService setUserCredentialsWithUsername:[docsService username]
                                            password:[docsService password]];
  GDataServiceTicket *ticket;
  ticket = [spreadsheetService authenticateWithDelegate:self
                                didAuthenticateSelector:@selector(spreadsheetTicket:authenticatedWithError:)];

  // we'll hang on to the spreadsheet service object with a ticket property
  // since we need it to create an authorized NSURLRequest
  [ticket setProperty:docEntry forKey:@"docEntry"];
  [ticket setProperty:savePath forKey:@"savePath"];
}

- (void)spreadsheetTicket:(GDataServiceTicket *)ticket
   authenticatedWithError:(NSError *)error {
  if (error == nil) {
    GDataEntrySpreadsheetDoc *docEntry = [ticket propertyForKey:@"docEntry"];
    NSString *savePath = [ticket propertyForKey:@"savePath"];

    [self saveDocEntry:docEntry
                toPath:savePath
          exportFormat:@"tsv"
           authService:[ticket service]];
  } else {
    // failed to authenticate; give up
    NSLog(@"Spreadsheet authentication error: %@", error);
    return;
  }
}
*/

#pragma mark -

- (IBAction)uploadFileClicked:(id)sender {

  // ask the user to choose a file
  NSOpenPanel *openPanel = [NSOpenPanel openPanel];
  [openPanel setPrompt:@"Upload"];

  SEL endSel = @selector(openSheetDidEnd:returnCode:contextInfo:);
  [openPanel beginSheetForDirectory:nil
                               file:nil
                              types:nil // upload any file type
                     modalForWindow:[self window]
                      modalDelegate:self
                     didEndSelector:endSel
                      contextInfo:nil];
}

- (void)openSheetDidEnd:(NSOpenPanel *)panel returnCode:(int)returnCode contextInfo:(void *)contextInfo {

  if (returnCode == NSOKButton) {
    // user chose a file and clicked OK

    // start uploading (deferred to the main thread since we currently have
    // a sheet displayed)
    [self performSelectorOnMainThread:@selector(uploadFileAtPath:)
                           withObject:[panel filename]
                        waitUntilDone:NO];
  }
}

- (IBAction)pauseUploadClicked:(id)sender {
  if ([mUploadTicket isUploadPaused]) {
    [mUploadTicket resumeUpload];
  } else {
    [mUploadTicket pauseUpload];
  }
  [self updateUI];
}

- (IBAction)stopUploadClicked:(id)sender {
  [mUploadTicket cancelTicket];
  [self setUploadTicket:nil];

  [mUploadProgressIndicator setDoubleValue:0.0];
  [self updateUI];
}

#pragma mark -

- (IBAction)publishCheckboxClicked:(id)sender {
  // enable or disable the Update Publishing button
  [self updateUI];
}

- (IBAction)updatePublishingClicked:(id)sender {
  GDataServiceGoogleDocs *service = [self docsService];

  GDataEntryDocRevision *revisionEntry = [self selectedRevision];

  // update the revision elements to match the checkboxes
  //
  // we'll modify a copy of the selected entry so we don't leave an inaccurate
  // entry in the feed if our fetch fails
  GDataEntryDocRevision *revisionCopy = [[revisionEntry copy] autorelease];

  BOOL shouldPublish = ([mPublishCheckbox state] == NSOnState);
  [revisionCopy setPublish:[NSNumber numberWithBool:shouldPublish]];

  BOOL shouldAutoRepublish = ([mAutoRepublishCheckbox state] == NSOnState);
  [revisionCopy setPublishAuto:[NSNumber numberWithBool:shouldAutoRepublish]];

  BOOL shouldPublishExternally = ([mPublishOutsideDomainCheckbox state] == NSOnState);
  [revisionCopy setPublishOutsideDomain:[NSNumber numberWithBool:shouldPublishExternally]];

  [service fetchEntryByUpdatingEntry:revisionCopy
                            delegate:self
                   didFinishSelector:@selector(publishRevisionTicket:finishedWithEntry:error:)];
}

- (void)publishRevisionTicket:(GDataServiceTicket *)ticket
            finishedWithEntry:(GDataEntryDocRevision *)entry
                        error:(NSError *)error {
  if (error == nil) {
    [self displayAlert:@"Updated"
                format:@"Updated publish status for \"%@\"", [[entry title] stringValue]];

    // re-fetch the document list
    [self fetchRevisionsForSelectedDoc];
  } else {
    [self displayAlert:@"Updated failed"
                format:@"Failed to update publish status: %@", error];
  }
}

#pragma mark -

- (IBAction)createFolderClicked:(id)sender {

  GDataServiceGoogleDocs *service = [self docsService];

  GDataEntryFolderDoc *docEntry = [GDataEntryFolderDoc documentEntry];

  NSString *title = [NSString stringWithFormat:@"New Folder %@", [NSDate date]];
  [docEntry setTitleWithString:title];

  NSURL *postURL = [[mDocListFeed postLink] URL];

  [service fetchEntryByInsertingEntry:docEntry
                           forFeedURL:postURL
                             delegate:self
                    didFinishSelector:@selector(createFolderTicket:finishedWithEntry:error:)];
}

// folder create callback
- (void)createFolderTicket:(GDataServiceTicket *)ticket
         finishedWithEntry:(GDataEntryFolderDoc *)entry
                     error:(NSError *)error {
  if (error == nil) {
    [self displayAlert:@"Created folder"
                format:@"Created folder \"%@\"", [[entry title] stringValue]];

    // re-fetch the document list
    [self fetchDocList];
    [self updateUI];
  } else {
    [self displayAlert:@"Create failed"
                format:@"Folder create failed: %@", error];
  }
}

#pragma mark -

- (IBAction)deleteSelectedDocClicked:(id)sender {

  GDataEntryDocBase *doc = [self selectedDoc];
  if (doc) {
    // make the user confirm that the selected doc should be deleted
    NSBeginAlertSheet(@"Delete Document", @"Delete", @"Cancel", nil,
                      [self window], self,
                      @selector(deleteDocSheetDidEnd:returnCode:contextInfo:),
                      nil, nil, @"Delete the document \"%@\"?",
                      [[doc title] stringValue]);
  }
}

// delete dialog callback
- (void)deleteDocSheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo {

  if (returnCode == NSAlertDefaultReturn) {

    // delete the document entry
    GDataEntryDocBase *entry = [self selectedDoc];

    if (entry) {
      GDataServiceGoogleDocs *service = [self docsService];
      [service deleteEntry:entry
                  delegate:self
         didFinishSelector:@selector(deleteDocEntryTicket:deletedEntry:error:)];
    }
  }
}

// entry delete callback
- (void)deleteDocEntryTicket:(GDataServiceTicket *)ticket
                deletedEntry:(GDataEntryDocBase *)object
                       error:(NSError *)error {
  if (error == nil) {
    // note: object is nil in the delete callback
    [self displayAlert:@"Deleted Doc"
                format:@"Document deleted"];

    // re-fetch the document list
    [self fetchDocList];
    [self updateUI];
  } else {
    [self displayAlert:@"Delete failed"
                format:@"Document delete failed: %@", error];
  }
}

#pragma mark -

- (IBAction)duplicateSelectedDocClicked:(id)sender {

  GDataEntryDocBase *selectedDoc = [self selectedDoc];
  if (selectedDoc) {
    // make a new entry of the same class as the selected document entry,
    // with just the title set and an identifier equal to the selected
    // doc's resource ID
    GDataEntryDocBase *newEntry = [[selectedDoc class] documentEntry];

    [newEntry setIdentifier:[selectedDoc resourceID]];

    NSString *oldTitle = [[selectedDoc title] stringValue];
    NSString *newTitle = [oldTitle stringByAppendingString:@" copy"];
    [newEntry setTitleWithString:newTitle];

    GDataServiceGoogleDocs *service = [self docsService];
    NSURL *postURL = [[mDocListFeed postLink] URL];

    [service fetchEntryByInsertingEntry:newEntry
                             forFeedURL:postURL
                               delegate:self
                      didFinishSelector:@selector(duplicateDocEntryTicket:finishedWithEntry:error:)];
  }
}

// document copying callback
- (void)duplicateDocEntryTicket:(GDataServiceTicket *)ticket
              finishedWithEntry:(GDataEntryDocBase *)newEntry
                       error:(NSError *)error {
  if (error == nil) {
    [self displayAlert:@"Copied Doc"
                format:@"Document duplicate \"%@\" created", [[newEntry title] stringValue]];

    // re-fetch the document list
    [self fetchDocList];
    [self updateUI];
  } else {
    [self displayAlert:@"Copy failed"
                format:@"Document duplicate failed: %@", error];
  }
}

#pragma mark -

- (IBAction)changeFolderSelected:(id)sender {

  // the selected menu item represents a folder; fetch the folder's feed
  //
  // with the folder's feed, we can insert or remove the selected document
  // entry in the folder's feed

  GDataEntryFolderDoc *folderEntry = [sender representedObject];
  NSURL *folderFeedURL = [[folderEntry content] sourceURL];
  if (folderFeedURL != nil) {

    GDataServiceGoogleDocs *service = [self docsService];

    GDataServiceTicket *ticket;
    ticket = [service fetchFeedWithURL:folderFeedURL
                              delegate:self
                     didFinishSelector:@selector(fetchFolderTicket:finishedWithFeed:error:)];

    // save the selected doc in the ticket's userData
    GDataEntryDocBase *doc = [self selectedDoc];
    [ticket setUserData:doc];
  }
}

// folder feed fetch callback
- (void)fetchFolderTicket:(GDataServiceTicket *)ticket
         finishedWithFeed:(GDataFeedDocList *)feed
                    error:(NSError *)error {

  if (error == nil) {
    GDataEntryDocBase *docEntry = [ticket userData];

    GDataServiceGoogleDocs *service = [self docsService];
    GDataServiceTicket *ticket2;

    // if the entry is not in the folder's feed, insert it; otherwise, delete
    // it from the folder's feed
    GDataEntryDocBase *foundEntry = [feed entryForIdentifier:[docEntry identifier]];
    if (foundEntry == nil) {
      // the doc isn't currently in this folder's feed
      //
      // post the doc to the folder's feed
      NSURL *postURL = [[feed postLink] URL];

      ticket2 = [service fetchEntryByInsertingEntry:docEntry
                                         forFeedURL:postURL
                                           delegate:self
                                  didFinishSelector:@selector(addToFolderTicket:finishedWithEntry:error:)];
      [ticket2 setUserData:feed];
    } else {
      // the doc is alrady in the folder's feed, so remove it
      ticket2 = [service deleteEntry:foundEntry
                            delegate:self
                   didFinishSelector:@selector(removeFromFolderTicket:finishedWithEntry:error:)];
      [ticket2 setUserData:feed];
    }
  } else {
    // failed to fetch feed of folders
    [self displayAlert:@"Fetch failed"
                format:@"Fetch of folder feed failed: %@", error];

  }
}

// add to folder callback
- (void)addToFolderTicket:(GDataServiceTicket *)ticket
        finishedWithEntry:(GDataEntryDocBase *)entry
                    error:(NSError *)error {
  if (error == nil) {
    GDataFeedDocList *feed = [ticket userData];

    [self displayAlert:@"Added"
                format:@"Added document \"%@\" to feed \"%@\"",
     [[entry title] stringValue], [[feed title] stringValue]];

    // re-fetch the document list
    [self fetchDocList];
    [self updateUI];
  } else {
    [self displayAlert:@"Insert failed"
                format:@"Insert to folder feed failed: %@", error];
  }
}

// remove from folder callback
- (void)removeFromFolderTicket:(GDataServiceTicket *)ticket
             finishedWithEntry:(GDataEntryDocBase *)entry
                         error:(NSError *)error {
  if (error == nil) {
    GDataFeedDocList *feed = [ticket userData];

    [self displayAlert:@"Removed"
                format:@"Removed document from feed \"%@\"", [[feed title] stringValue]];

    // re-fetch the document list
    [self fetchDocList];
    [self updateUI];
  } else {
    [self displayAlert:@"Fetch failed"
                format:@"Remove from folder feed failed: %@", error];
  }
}

#pragma mark -

- (IBAction)APIConsoleClicked:(id)sender {
  NSURL *url = [NSURL URLWithString:@"https://code.google.com/apis/console"];
  [[NSWorkspace sharedWorkspace] openURL:url];
}

- (IBAction)loggingCheckboxClicked:(id)sender {
  [GTMHTTPFetcher setLoggingEnabled:[sender state]];
}

#pragma mark -

// get an docList service object with the current username/password
//
// A "service" object handles networking tasks.  Service objects
// contain user authentication information as well as networking
// state information (such as cookies and the "last modified" date for
// fetched data.)

- (GDataServiceGoogleDocs *)docsService {

  static GDataServiceGoogleDocs* service = nil;

  if (!service) {
    service = [[GDataServiceGoogleDocs alloc] init];

    [service setShouldCacheResponseData:YES];
    [service setServiceShouldFollowNextLinks:YES];
    [service setIsServiceRetryEnabled:YES];
  }

  return service;
}

// get the doc selected in the list, or nil if none
- (GDataEntryDocBase *)selectedDoc {

  int rowIndex = [mDocListTable selectedRow];
  if (rowIndex > -1) {
    GDataEntryDocBase *doc = [mDocListFeed entryAtIndex:rowIndex];
    return doc;
  }
  return nil;
}

// get the doc revision in the list, or nil if none
- (GDataEntryDocRevision *)selectedRevision {

  int rowIndex = [mRevisionsTable selectedRow];
  if (rowIndex > -1) {
    GDataEntryDocRevision *entry = [mRevisionFeed entryAtIndex:rowIndex];
    return entry;
  }
  return nil;
}

#pragma mark Fetch doc list user metadata

- (void)fetchMetadataEntry {
  [self setMetadataEntry:nil];

  NSURL *entryURL = [GDataServiceGoogleDocs metadataEntryURLForUserID:kGDataServiceDefaultUser];
  GDataServiceGoogleDocs *service = [self docsService];
  [service fetchEntryWithURL:entryURL
                    delegate:self
           didFinishSelector:@selector(metadataTicket:finishedWithEntry:error:)];
}

- (void)metadataTicket:(GDataServiceTicket *)ticket
     finishedWithEntry:(GDataEntryDocListMetadata *)entry
                 error:(NSError *)error {
  [self setMetadataEntry:entry];

  // enable or disable features
  [self updateUI];

  if (error != nil) {
    NSLog(@"Error fetching user metadata: %@", error);
  }
}

#pragma mark Fetch doc list

// begin retrieving the list of the user's docs
- (void)fetchDocList {

  [self setDocListFeed:nil];
  [self setDocListFetchError:nil];
  [self setDocListFetchTicket:nil];

  GDataServiceGoogleDocs *service = [self docsService];
  GDataServiceTicket *ticket;

  // Fetching a feed gives us 25 responses by default.  We need to use
  // the feed's "next" link to get any more responses.  If we want more than 25
  // at a time, instead of calling fetchDocsFeedWithURL, we can create a
  // GDataQueryDocs object, as shown here.

  NSURL *feedURL = [GDataServiceGoogleDocs docsFeedURL];

  GDataQueryDocs *query = [GDataQueryDocs documentQueryWithFeedURL:feedURL];
  [query setMaxResults:1000];
  [query setShouldShowFolders:YES];

  ticket = [service fetchFeedWithQuery:query
                              delegate:self
                     didFinishSelector:@selector(docListFetchTicket:finishedWithFeed:error:)];

  [self setDocListFetchTicket:ticket];

  // update our metadata entry for this user
  [self fetchMetadataEntry];

  [self updateUI];
}

// docList list fetch callback
- (void)docListFetchTicket:(GDataServiceTicket *)ticket
          finishedWithFeed:(GDataFeedDocList *)feed
                     error:(NSError *)error {
  [self setDocListFeed:feed];
  [self setDocListFetchError:error];
  [self setDocListFetchTicket:nil];

  [self updateUI];
}

#pragma mark Fetch revisions or content feed

- (void)fetchRevisionsForSelectedDoc {

  [self setRevisionFeed:nil];
  [self setRevisionFetchError:nil];
  [self setRevisionFetchTicket:nil];

  GDataEntryDocBase *selectedDoc = [self selectedDoc];
  GDataFeedLink *revisionFeedLink = [selectedDoc revisionFeedLink];
  NSURL *revisionFeedURL = [revisionFeedLink URL];
  if (revisionFeedURL) {

    GDataServiceGoogleDocs *service = [self docsService];
    GDataServiceTicket *ticket;
    ticket = [service fetchFeedWithURL:revisionFeedURL
                              delegate:self
                     didFinishSelector:@selector(revisionFetchTicket:finishedWithFeed:error:)];

    [self setRevisionFetchTicket:ticket];

  }

  [self updateUI];
}

// revisions list fetch callback
- (void)revisionFetchTicket:(GDataServiceTicket *)ticket
           finishedWithFeed:(GDataFeedDocRevision *)feed
                      error:(NSError *)error {

  [self setRevisionFeed:feed];
  [self setRevisionFetchError:error];
  [self setRevisionFetchTicket:nil];

  [self updateUI];
}

#pragma mark Upload

- (void)getMIMEType:(NSString **)mimeType andEntryClass:(Class *)class forExtension:(NSString *)extension {

  // Mac OS X's UTI database doesn't know MIME types for .doc and .xls
  // so GDataEntryBase's MIMETypeForFileAtPath method isn't helpful here

  struct MapEntry {
    NSString *extension;
    NSString *mimeType;
    NSString *className;
  };

  static struct MapEntry sMap[] = {
    { @"csv", @"text/csv", @"GDataEntryStandardDoc" },
    { @"doc", @"application/msword", @"GDataEntryStandardDoc" },
    { @"docx", @"application/vnd.openxmlformats-officedocument.wordprocessingml.document", @"GDataEntryStandardDoc" },
    { @"ods", @"application/vnd.oasis.opendocument.spreadsheet", @"GDataEntrySpreadsheetDoc" },
    { @"odt", @"application/vnd.oasis.opendocument.text", @"GDataEntryStandardDoc" },
    { @"pps", @"application/vnd.ms-powerpoint", @"GDataEntryPresentationDoc" },
    { @"ppt", @"application/vnd.ms-powerpoint", @"GDataEntryPresentationDoc" },
    { @"rtf", @"application/rtf", @"GDataEntryStandardDoc" },
    { @"sxw", @"application/vnd.sun.xml.writer", @"GDataEntryStandardDoc" },
    { @"txt", @"text/plain", @"GDataEntryStandardDoc" },
    { @"xls", @"application/vnd.ms-excel", @"GDataEntrySpreadsheetDoc" },
    { @"xlsx", @"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet", @"GDataEntrySpreadsheetDoc" },
    { @"jpg", @"image/jpeg", @"GDataEntryStandardDoc" },
    { @"jpeg", @"image/jpeg", @"GDataEntryStandardDoc" },
    { @"png", @"image/png", @"GDataEntryStandardDoc" },
    { @"bmp", @"image/bmp", @"GDataEntryStandardDoc" },
    { @"gif", @"image/gif", @"GDataEntryStandardDoc" },
    { @"html", @"text/html", @"GDataEntryStandardDoc" },
    { @"htm", @"text/html", @"GDataEntryStandardDoc" },
    { @"tsv", @"text/tab-separated-values", @"GDataEntryStandardDoc" },
    { @"tab", @"text/tab-separated-values", @"GDataEntryStandardDoc" },
    { @"pdf", @"application/pdf", @"GDataEntryPDFDoc" },
    { nil, nil, nil }
  };

  NSString *lowerExtn = [extension lowercaseString];

  for (int idx = 0; sMap[idx].extension != nil; idx++) {
    if ([lowerExtn isEqual:sMap[idx].extension]) {
      *mimeType = sMap[idx].mimeType;
      *class = NSClassFromString(sMap[idx].className);
      return;
    }
  }

  *mimeType = nil;
  *class = nil;
  return;
}

- (void)uploadFileAtPath:(NSString *)path {

  NSString *errorMsg = nil;

  // make a new entry for the file

  NSString *mimeType = nil;
  Class entryClass = nil;

  NSString *extn = [path pathExtension];
  [self getMIMEType:&mimeType andEntryClass:&entryClass forExtension:extn];

  if (!mimeType) {
    // for other file types, see if we can get the type from the Mac OS
    // and use a generic file document entry class
    mimeType = [GDataUtilities MIMETypeForFileAtPath:path
                                     defaultMIMEType:nil];
    entryClass = [GDataEntryFileDoc class];
  }

  if (!mimeType) {
    errorMsg = [NSString stringWithFormat:@"need MIME type for file %@", path];
  }

  if (mimeType && entryClass) {

    GDataEntryDocBase *newEntry = [entryClass documentEntry];

    NSString *title = [[NSFileManager defaultManager] displayNameAtPath:path];
    [newEntry setTitleWithString:title];

    NSFileHandle *uploadFileHandle = [NSFileHandle fileHandleForReadingAtPath:path];
    if (!uploadFileHandle) {
      errorMsg = [NSString stringWithFormat:@"cannot read file %@", path];
    }

    if (uploadFileHandle) {
      [newEntry setUploadFileHandle:uploadFileHandle];

      [newEntry setUploadMIMEType:mimeType];
      [newEntry setUploadSlug:[path lastPathComponent]];

      NSURL *uploadURL = [GDataServiceGoogleDocs docsUploadURL];

      // add the OCR or translation parameters, if the user set the pop-up
      // button appropriately
      int popupTag = [[mUploadPopup selectedItem] tag];
      if (popupTag != 0) {
        NSString *targetLanguage = nil;
        BOOL shouldConvertToGoogleDoc = YES;
        BOOL shouldOCR = NO;

        switch (popupTag) {
            // upload original file
          case kUploadOriginal: shouldConvertToGoogleDoc = NO; break;

            // OCR
          case kUploadOCR:      shouldOCR = YES; break;

            // translation
          case kUploadDE:       targetLanguage = @"de"; break; // german
          case kUploadJA:       targetLanguage = @"ja"; break; // japanese
          case kUploadEN:       targetLanguage = @"en"; break; // english

          default: break;
        }

        GDataQueryDocs *query = [GDataQueryDocs queryWithFeedURL:uploadURL];

        [query setShouldConvertUpload:shouldConvertToGoogleDoc];
        [query setShouldOCRUpload:shouldOCR];

        // we'll leave out the sourceLanguage parameter to get
        // auto-detection of the file's language
        //
        // language codes: http://www.loc.gov/standards/iso639-2/php/code_list.php
        [query setTargetLanguage:targetLanguage];

        uploadURL = [query URL];
      }

      // make service tickets call back into our upload progress selector
      GDataServiceGoogleDocs *service = [self docsService];

      // insert the entry into the docList feed
      GDataServiceTicket *ticket;
      ticket = [service fetchEntryByInsertingEntry:newEntry
                                        forFeedURL:uploadURL
                                          delegate:self
                                 didFinishSelector:@selector(uploadFileTicket:finishedWithEntry:error:)];
      SEL progressSel = @selector(ticket:hasDeliveredByteCount:ofTotalByteCount:);
      [ticket setUploadProgressSelector:progressSel];

      // we turned automatic retry on when we allocated the service, but we
      // could also turn it on just for this ticket

      [self setUploadTicket:ticket];
    }
  }

  if (errorMsg) {
    // we're currently in the middle of the file selection sheet, so defer our
    // error sheet
    [self displayAlert:@"Upload Error"
                format:@"%@", errorMsg];
  }

  [self updateUI];
}

// progress callback
- (void)ticket:(GDataServiceTicket *)ticket
   hasDeliveredByteCount:(unsigned long long)numberOfBytesRead
   ofTotalByteCount:(unsigned long long)dataLength {

  [mUploadProgressIndicator setMinValue:0.0];
  [mUploadProgressIndicator setMaxValue:(double)dataLength];
  [mUploadProgressIndicator setDoubleValue:(double)numberOfBytesRead];
}

// upload finished callback
- (void)uploadFileTicket:(GDataServiceTicket *)ticket
     finishedWithEntry:(GDataEntryDocBase *)entry
                   error:(NSError *)error {

  [self setUploadTicket:nil];
  [mUploadProgressIndicator setDoubleValue:0.0];

  if (error == nil) {
    // refetch the current doc list
    [self fetchDocList];

    // tell the user that the add worked
    [self displayAlert:@"Uploaded file"
                format:@"File uploaded: %@", [[entry title] stringValue]];
  } else {
    [self displayAlert:@"Upload failed"
                format:@"File upload failed: %@", error];
  }
  [self updateUI];
}

#pragma mark Client ID Sheet

// Client ID and Client Secret Sheet
//
// Sample apps need this sheet to ask for the client ID and client secret
// strings
//
// Your application will just hardcode the client ID and client secret strings
// into the source rather than ask the user for them.
//
// The string values are obtained from the API Console,
// https://code.google.com/apis/console

- (IBAction)clientIDClicked:(id)sender {
  // Show the sheet for developers to enter their client ID and client secret
  [NSApp beginSheet:mClientIDSheet
     modalForWindow:[self window]
      modalDelegate:self
     didEndSelector:@selector(clientIDSheetDidEnd:returnCode:contextInfo:)
        contextInfo:NULL];
}

- (IBAction)clientIDDoneClicked:(id)sender {
  [NSApp endSheet:mClientIDSheet returnCode:NSOKButton];
}

- (void)clientIDSheetDidEnd:(NSWindow *)sheet returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo {
  [sheet orderOut:self];
  [self updateUI];
}

#pragma mark TableView delegate methods

//
// table view delegate methods
//

- (void)tableViewSelectionDidChange:(NSNotification *)notification {
  if ([notification object] == mDocListTable) {
    // the user selected a document entry, so fetch its revisions
    [self fetchRevisionsForSelectedDoc];
  } else {
    // the user selected a revision entry
    //
    // update the publishing checkboxes to match the newly-selected revision

    GDataEntryDocRevision *selectedRevision = [self selectedRevision];

    BOOL isPublished = [[selectedRevision publish] boolValue];
    [mPublishCheckbox setState:(isPublished ? NSOnState : NSOffState)];

    BOOL isAutoRepublished = [[selectedRevision publishAuto] boolValue];
    [mAutoRepublishCheckbox setState:(isAutoRepublished ? NSOnState : NSOffState)];

    BOOL isExternalPublished = [[selectedRevision publishOutsideDomain] boolValue];
    [mPublishOutsideDomainCheckbox setState:(isExternalPublished ? NSOnState : NSOffState)];

    [self updateUI];
  }
}

// table view data source methods
- (int)numberOfRowsInTableView:(NSTableView *)tableView {
  if (tableView == mDocListTable) {
    return [[mDocListFeed entries] count];
  }

  if (tableView == mRevisionsTable) {
    return [[mRevisionFeed entries] count];
  }

  return 0;
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn row:(int)row {

  if (tableView == mDocListTable) {
    // get the docList entry's title, and the kind of document
    GDataEntryDocBase *doc = [mDocListFeed entryAtIndex:row];

    NSString *docKind = @"unknown";

    // the kind category for a doc entry includes a label like "document"
    // or "spreadsheet"
    NSArray *categories;
    categories = [GDataCategory categoriesWithScheme:kGDataCategoryScheme
                                      fromCategories:[doc categories]];
    if ([categories count] >= 1) {
      docKind = [[categories objectAtIndex:0] label];
    }

    // mark if the document is starred
    if ([doc isStarred]) {
      const UniChar kStarChar = 0x2605;
      docKind = [NSString stringWithFormat:@"%C, %@", kStarChar, docKind];
    }

    NSString *displayStr = [NSString stringWithFormat:@"%@ (%@)",
                            [[doc title] stringValue], docKind];
    return displayStr;
  }

  if (tableView == mRevisionsTable) {
    // get the revision entry
    GDataEntryDocRevision *revisionEntry;
    revisionEntry = [mRevisionFeed entryAtIndex:row];

    NSString *displayStr = [NSString stringWithFormat:@"%@ (edited %@)",
                            [[revisionEntry title] stringValue],
                            [[revisionEntry editedDate] date]];
    return displayStr;
  }
  return nil;
}

#pragma mark Setters and Getters

- (GDataFeedDocList *)docListFeed {
  return mDocListFeed;
}

- (void)setDocListFeed:(GDataFeedDocList *)feed {
  [mDocListFeed autorelease];
  mDocListFeed = [feed retain];
}

- (NSError *)docListFetchError {
  return mDocListFetchError;
}

- (void)setDocListFetchError:(NSError *)error {
  [mDocListFetchError release];
  mDocListFetchError = [error retain];
}

- (GDataServiceTicket *)docListFetchTicket {
  return mDocListFetchTicket;
}

- (void)setDocListFetchTicket:(GDataServiceTicket *)ticket {
  [mDocListFetchTicket release];
  mDocListFetchTicket = [ticket retain];
}


- (GDataFeedDocRevision *)revisionFeed {
  return mRevisionFeed;
}

- (void)setRevisionFeed:(GDataFeedDocRevision *)feed {
  [mRevisionFeed autorelease];
  mRevisionFeed = [feed retain];
}

- (NSError *)revisionFetchError {
  return mRevisionFetchError;
}

- (void)setRevisionFetchError:(NSError *)error {
  [mRevisionFetchError release];
  mRevisionFetchError = [error retain];
}

- (GDataServiceTicket *)revisionFetchTicket {
  return mRevisionFetchTicket;
}

- (void)setRevisionFetchTicket:(GDataServiceTicket *)ticket {
  [mRevisionFetchTicket release];
  mRevisionFetchTicket = [ticket retain];
}


- (GDataEntryDocListMetadata *)metadataEntry {
  return mMetadataEntry;
}

- (void)setMetadataEntry:(GDataEntryDocListMetadata *)entry {
  [mMetadataEntry release];
  mMetadataEntry = [entry retain];
}


- (GDataServiceTicket *)uploadTicket {
  return mUploadTicket;
}

- (void)setUploadTicket:(GDataServiceTicket *)ticket {
  [mUploadTicket release];
  mUploadTicket = [ticket retain];
}

@end
