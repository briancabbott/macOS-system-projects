
struct NSFetchRequestResultType : OptionSetType {
  init(rawValue rawValue: UInt)
  let rawValue: UInt
  static var managedObjectResultType: NSFetchRequestResultType { get }
  static var managedObjectIDResultType: NSFetchRequestResultType { get }
  @available(tvOS 3.0, *)
  static var dictionaryResultType: NSFetchRequestResultType { get }
  @available(tvOS 3.0, *)
  static var countResultType: NSFetchRequestResultType { get }
}
@available(tvOS 3.0, *)
class NSFetchRequest : NSPersistentStoreRequest, NSCoding {
  @available(tvOS 4.0, *)
  convenience init(entityName entityName: String)
  var entity: NSEntityDescription?
  @available(tvOS 4.0, *)
  var entityName: String? { get }
  var predicate: NSPredicate?
  var sortDescriptors: [NSSortDescriptor]?
  var fetchLimit: Int
  @available(tvOS 3.0, *)
  var resultType: NSFetchRequestResultType
  @available(tvOS 3.0, *)
  var includesSubentities: Bool
  @available(tvOS 3.0, *)
  var includesPropertyValues: Bool
  @available(tvOS 3.0, *)
  var returnsObjectsAsFaults: Bool
  @available(tvOS 3.0, *)
  var relationshipKeyPathsForPrefetching: [String]?
  @available(tvOS 3.0, *)
  var includesPendingChanges: Bool
  @available(tvOS 3.0, *)
  var returnsDistinctResults: Bool
  @available(tvOS 3.0, *)
  var propertiesToFetch: [AnyObject]?
  @available(tvOS 3.0, *)
  var fetchOffset: Int
  @available(tvOS 3.0, *)
  var fetchBatchSize: Int
  @available(tvOS 5.0, *)
  var shouldRefreshRefetchedObjects: Bool
  @available(tvOS 5.0, *)
  var propertiesToGroupBy: [AnyObject]?
  @available(tvOS 5.0, *)
  var havingPredicate: NSPredicate?
  @available(tvOS 3.0, *)
  func encode(with aCoder: NSCoder)
  init?(coder aDecoder: NSCoder)
}
struct _fetchRequestFlags {
  var distinctValuesOnly: UInt32
  var includesSubentities: UInt32
  var includesPropertyValues: UInt32
  var resultType: UInt32
  var returnsObjectsAsFaults: UInt32
  var excludePendingChanges: UInt32
  var isInUse: UInt32
  var entityIsName: UInt32
  var refreshesRefetched: UInt32
  var propertiesValidated: UInt32
  var disableCaching: UInt32
  var _RESERVED: UInt32
  init()
  init(distinctValuesOnly distinctValuesOnly: UInt32, includesSubentities includesSubentities: UInt32, includesPropertyValues includesPropertyValues: UInt32, resultType resultType: UInt32, returnsObjectsAsFaults returnsObjectsAsFaults: UInt32, excludePendingChanges excludePendingChanges: UInt32, isInUse isInUse: UInt32, entityIsName entityIsName: UInt32, refreshesRefetched refreshesRefetched: UInt32, propertiesValidated propertiesValidated: UInt32, disableCaching disableCaching: UInt32, _RESERVED _RESERVED: UInt32)
}
