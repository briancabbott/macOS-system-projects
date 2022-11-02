
@available(iOS 8.0, *)
class PHChange : NSObject {
  func changeDetails(for object: PHObject) -> PHObjectChangeDetails?
  func changeDetails(for object: PHFetchResult) -> PHFetchResultChangeDetails?
}
@available(iOS 8.0, *)
class PHObjectChangeDetails : NSObject {
  var objectBeforeChanges: PHObject { get }
  var objectAfterChanges: PHObject? { get }
  var assetContentChanged: Bool { get }
  var objectWasDeleted: Bool { get }
}
@available(iOS 8.0, *)
class PHFetchResultChangeDetails : NSObject {
  var fetchResultBeforeChanges: PHFetchResult { get }
  var fetchResultAfterChanges: PHFetchResult { get }
  var hasIncrementalChanges: Bool { get }
  var removedIndexes: NSIndexSet? { get }
  var removedObjects: [PHObject] { get }
  var insertedIndexes: NSIndexSet? { get }
  var insertedObjects: [PHObject] { get }
  var changedIndexes: NSIndexSet? { get }
  var changedObjects: [PHObject] { get }
  func enumerateMoves(_ handler: (Int, Int) -> Void)
  var hasMoves: Bool { get }
  convenience init(from fromResult: PHFetchResult, to toResult: PHFetchResult, changedObjects changedObjects: [PHObject])
}
