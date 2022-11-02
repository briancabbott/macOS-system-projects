
class OBEXFileTransferServices : NSObject {
  unowned(unsafe) var delegate: @sil_unmanaged AnyObject!
  class func withOBEXSession(_ inOBEXSession: IOBluetoothOBEXSession!) -> Self!
  init!(obexSession inOBEXSession: IOBluetoothOBEXSession!)
  func currentPath() -> String!
  func isBusy() -> Bool
  func isConnected() -> Bool
  func connectToFTPService() -> OBEXError
  func connectToObjectPushService() -> OBEXError
  func disconnect() -> OBEXError
  func changeCurrentFolderToRoot() -> OBEXError
  func changeCurrentFolderBackward() -> OBEXError
  func changeCurrentFolderForward(toPath inDirName: String!) -> OBEXError
  func createFolder(_ inDirName: String!) -> OBEXError
  func removeItem(_ inItemName: String!) -> OBEXError
  func retrieveFolderListing() -> OBEXError
  func sendFile(_ inLocalPathAndName: String!) -> OBEXError
  func copyRemoteFile(_ inRemoteFileName: String!, toLocalPath inLocalPathAndName: String!) -> OBEXError
  func send(_ inData: NSData!, type inType: String!, name inName: String!) -> OBEXError
  func getDefaultVCard(_ inLocalPathAndName: String!) -> OBEXError
  func abort() -> OBEXError
}
extension NSObject {
  class func fileTransferServicesConnectionComplete(_ inServices: OBEXFileTransferServices!, error inError: OBEXError)
  func fileTransferServicesConnectionComplete(_ inServices: OBEXFileTransferServices!, error inError: OBEXError)
  class func fileTransferServicesDisconnectionComplete(_ inServices: OBEXFileTransferServices!, error inError: OBEXError)
  func fileTransferServicesDisconnectionComplete(_ inServices: OBEXFileTransferServices!, error inError: OBEXError)
  class func fileTransferServicesAbortComplete(_ inServices: OBEXFileTransferServices!, error inError: OBEXError)
  func fileTransferServicesAbortComplete(_ inServices: OBEXFileTransferServices!, error inError: OBEXError)
  class func fileTransferServicesRemoveItemComplete(_ inServices: OBEXFileTransferServices!, error inError: OBEXError, removedItem inItemName: String!)
  func fileTransferServicesRemoveItemComplete(_ inServices: OBEXFileTransferServices!, error inError: OBEXError, removedItem inItemName: String!)
  class func fileTransferServicesCreateFolderComplete(_ inServices: OBEXFileTransferServices!, error inError: OBEXError, folder inFolderName: String!)
  func fileTransferServicesCreateFolderComplete(_ inServices: OBEXFileTransferServices!, error inError: OBEXError, folder inFolderName: String!)
  class func fileTransferServicesPathChangeComplete(_ inServices: OBEXFileTransferServices!, error inError: OBEXError, finalPath inPath: String!)
  func fileTransferServicesPathChangeComplete(_ inServices: OBEXFileTransferServices!, error inError: OBEXError, finalPath inPath: String!)
  class func fileTransferServicesRetrieveFolderListingComplete(_ inServices: OBEXFileTransferServices!, error inError: OBEXError, listing inListing: [AnyObject]!)
  func fileTransferServicesRetrieveFolderListingComplete(_ inServices: OBEXFileTransferServices!, error inError: OBEXError, listing inListing: [AnyObject]!)
  class func fileTransferServicesFilePreparationComplete(_ inServices: OBEXFileTransferServices!, error inError: OBEXError)
  func fileTransferServicesFilePreparationComplete(_ inServices: OBEXFileTransferServices!, error inError: OBEXError)
  class func fileTransferServicesSendFileProgress(_ inServices: OBEXFileTransferServices!, transferProgress inProgressDescription: [NSObject : AnyObject]!)
  func fileTransferServicesSendFileProgress(_ inServices: OBEXFileTransferServices!, transferProgress inProgressDescription: [NSObject : AnyObject]!)
  class func fileTransferServicesSendFileComplete(_ inServices: OBEXFileTransferServices!, error inError: OBEXError)
  func fileTransferServicesSendFileComplete(_ inServices: OBEXFileTransferServices!, error inError: OBEXError)
  class func fileTransferServicesCopyRemoteFileProgress(_ inServices: OBEXFileTransferServices!, transferProgress inProgressDescription: [NSObject : AnyObject]!)
  func fileTransferServicesCopyRemoteFileProgress(_ inServices: OBEXFileTransferServices!, transferProgress inProgressDescription: [NSObject : AnyObject]!)
  class func fileTransferServicesCopyRemoteFileComplete(_ inServices: OBEXFileTransferServices!, error inError: OBEXError)
  func fileTransferServicesCopyRemoteFileComplete(_ inServices: OBEXFileTransferServices!, error inError: OBEXError)
}
var kFTSProgressBytesTransferredKey: Unmanaged<CFString>!
var kFTSProgressBytesTotalKey: Unmanaged<CFString>!
var kFTSProgressPercentageKey: Unmanaged<CFString>!
var kFTSProgressPrecentageKey: Unmanaged<CFString>!
var kFTSProgressEstimatedTimeKey: Unmanaged<CFString>!
var kFTSProgressTimeElapsedKey: Unmanaged<CFString>!
var kFTSProgressTransferRateKey: Unmanaged<CFString>!
var kFTSListingNameKey: Unmanaged<CFString>!
var kFTSListingTypeKey: Unmanaged<CFString>!
var kFTSListingSizeKey: Unmanaged<CFString>!
struct FTSFileType : RawRepresentable, Equatable {
  init(_ rawValue: UInt32)
  init(rawValue rawValue: UInt32)
  var rawValue: UInt32
}
var kFTSFileTypeFolder: FTSFileType { get }
var kFTSFileTypeFile: FTSFileType { get }
