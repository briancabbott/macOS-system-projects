
protocol NSFilePresenter : NSObjectProtocol {
  @NSCopying var presentedItemURL: NSURL? { get }
  @available(watchOS 2.0, *)
  var presentedItemOperationQueue: NSOperationQueue { get }
  optional func relinquishPresentedItem(toReader reader: ((() -> Void)?) -> Void)
  optional func relinquishPresentedItem(toWriter writer: ((() -> Void)?) -> Void)
  optional func savePresentedItemChanges(completionHandler completionHandler: (NSError?) -> Void)
  optional func accommodatePresentedItemDeletion(completionHandler completionHandler: (NSError?) -> Void)
  optional func presentedItemDidMove(to newURL: NSURL)
  optional func presentedItemDidChange()
  @available(watchOS 2.0, *)
  optional func presentedItemDidGainVersion(_ version: NSFileVersion)
  @available(watchOS 2.0, *)
  optional func presentedItemDidLose(_ version: NSFileVersion)
  @available(watchOS 2.0, *)
  optional func presentedItemDidResolveConflictVersion(_ version: NSFileVersion)
  optional func accommodatePresentedSubitemDeletion(at url: NSURL, completionHandler completionHandler: (NSError?) -> Void)
  optional func presentedSubitemDidAppear(at url: NSURL)
  optional func presentedSubitem(at oldURL: NSURL, didMoveTo newURL: NSURL)
  optional func presentedSubitemDidChange(at url: NSURL)
  @available(watchOS 2.0, *)
  optional func presentedSubitem(at url: NSURL, didGainVersion version: NSFileVersion)
  @available(watchOS 2.0, *)
  optional func presentedSubitem(at url: NSURL, didLose version: NSFileVersion)
  @available(watchOS 2.0, *)
  optional func presentedSubitem(at url: NSURL, didResolveConflictVersion version: NSFileVersion)
}
