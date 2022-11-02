
@available(iOS 3.0, *)
let NSManagedObjectContextWillSaveNotification: String
@available(iOS 3.0, *)
let NSManagedObjectContextDidSaveNotification: String
@available(iOS 3.0, *)
let NSManagedObjectContextObjectsDidChangeNotification: String
@available(iOS 3.0, *)
let NSInsertedObjectsKey: String
@available(iOS 3.0, *)
let NSUpdatedObjectsKey: String
@available(iOS 3.0, *)
let NSDeletedObjectsKey: String
@available(iOS 3.0, *)
let NSRefreshedObjectsKey: String
@available(iOS 3.0, *)
let NSInvalidatedObjectsKey: String
@available(iOS 3.0, *)
let NSInvalidatedAllObjectsKey: String
@available(iOS 5.0, *)
enum NSManagedObjectContextConcurrencyType : UInt {
  init?(rawValue rawValue: UInt)
  var rawValue: UInt { get }
  @available(iOS, introduced=3.0, deprecated=9.0, message="Use another NSManagedObjectContextConcurrencyType")
  case confinementConcurrencyType
  case privateQueueConcurrencyType
  case mainQueueConcurrencyType
}
@available(iOS 3.0, *)
class NSManagedObjectContext : NSObject, NSCoding, NSLocking {
  @available(iOS 5.0, *)
  init(concurrencyType ct: NSManagedObjectContextConcurrencyType)
  @available(iOS 5.0, *)
  func perform(_ block: () -> Void)
  @available(iOS 5.0, *)
  func performBlockAndWait(_ block: () -> Void)
  var persistentStoreCoordinator: NSPersistentStoreCoordinator?
  @available(iOS 5.0, *)
  var parent: NSManagedObjectContext?
  @available(iOS 8.0, *)
  var name: String?
  var undoManager: NSUndoManager?
  var hasChanges: Bool { get }
  @available(iOS 5.0, *)
  var userInfo: NSMutableDictionary { get }
  @available(iOS 5.0, *)
  var concurrencyType: NSManagedObjectContextConcurrencyType { get }
  func objectRegistered(for objectID: NSManagedObjectID) -> NSManagedObject?
  func object(with objectID: NSManagedObjectID) -> NSManagedObject
  @available(iOS 3.0, *)
  func existingObject(with objectID: NSManagedObjectID) throws -> NSManagedObject
  func execute(_ request: NSFetchRequest) throws -> [AnyObject]
  @available(iOS 3.0, *)
  func count(for request: NSFetchRequest, error error: NSErrorPointer) -> Int
  @available(iOS 8.0, *)
  func execute(_ request: NSPersistentStoreRequest) throws -> NSPersistentStoreResult
  func insert(_ object: NSManagedObject)
  func delete(_ object: NSManagedObject)
  func refreshObject(_ object: NSManagedObject, mergeChanges flag: Bool)
  func detectConflicts(for object: NSManagedObject)
  func processPendingChanges()
  func assign(_ object: AnyObject, to store: NSPersistentStore)
  var insertedObjects: Set<NSManagedObject> { get }
  var updatedObjects: Set<NSManagedObject> { get }
  var deletedObjects: Set<NSManagedObject> { get }
  var registeredObjects: Set<NSManagedObject> { get }
  func undo()
  func redo()
  func reset()
  func rollback()
  func save() throws
  @available(iOS 8.3, *)
  func refreshAllObjects()
  @available(iOS, introduced=3.0, deprecated=8.0, message="Use a queue style context and -performBlockAndWait: instead")
  func lock()
  @available(iOS, introduced=3.0, deprecated=8.0, message="Use a queue style context and -performBlockAndWait: instead")
  func unlock()
  @available(iOS, introduced=3.0, deprecated=8.0, message="Use a queue style context and -performBlock: instead")
  func tryLock() -> Bool
  var propagatesDeletesAtEndOfEvent: Bool
  var retainsRegisteredObjects: Bool
  @available(iOS 9.0, *)
  var shouldDeleteInaccessibleFaults: Bool
  @available(iOS 9.0, *)
  func shouldHandleInaccessibleFault(_ fault: NSManagedObject, for oid: NSManagedObjectID, triggeredByProperty property: NSPropertyDescription?) -> Bool
  var stalenessInterval: NSTimeInterval
  var mergePolicy: AnyObject
  @available(iOS 3.0, *)
  func obtainPermanentIDs(for objects: [NSManagedObject]) throws
  @available(iOS 3.0, *)
  func mergeChanges(fromContextDidSave notification: NSNotification)
  @available(iOS 9.0, *)
  class func mergeChanges(fromRemoteContextSave changeNotificationData: [NSObject : AnyObject], into contexts: [NSManagedObjectContext])
  @available(iOS 3.0, *)
  func encode(with aCoder: NSCoder)
  init?(coder aDecoder: NSCoder)
}
struct _managedObjectContextFlags {
  var _registeredForCallback: UInt32
  var _propagatesDeletesAtEndOfEvent: UInt32
  var _exhaustiveValidation: UInt32
  var _processingChanges: UInt32
  var _useCommittedSnapshot: UInt32
  var _registeredUndoTransactionID: UInt32
  var _retainsAllRegisteredObjects: UInt32
  var _savingInProgress: UInt32
  var _wasDisposed: UInt32
  var _unprocessedChangesPending: UInt32
  var _isDirty: UInt32
  var _ignoreUndoCheckpoints: UInt32
  var _propagatingDeletes: UInt32
  var _isNSEditorEditing: UInt32
  var _isMainThreadBlessed: UInt32
  var _isImportContext: UInt32
  var _preflightSaveInProgress: UInt32
  var _disableDiscardEditing: UInt32
  var _isParentStoreContext: UInt32
  var _postSaveNotifications: UInt32
  var _isMerging: UInt32
  var _concurrencyType: UInt32
  var _deleteInaccessible: UInt32
  var _reservedFlags: UInt32
  init()
  init(_registeredForCallback _registeredForCallback: UInt32, _propagatesDeletesAtEndOfEvent _propagatesDeletesAtEndOfEvent: UInt32, _exhaustiveValidation _exhaustiveValidation: UInt32, _processingChanges _processingChanges: UInt32, _useCommittedSnapshot _useCommittedSnapshot: UInt32, _registeredUndoTransactionID _registeredUndoTransactionID: UInt32, _retainsAllRegisteredObjects _retainsAllRegisteredObjects: UInt32, _savingInProgress _savingInProgress: UInt32, _wasDisposed _wasDisposed: UInt32, _unprocessedChangesPending _unprocessedChangesPending: UInt32, _isDirty _isDirty: UInt32, _ignoreUndoCheckpoints _ignoreUndoCheckpoints: UInt32, _propagatingDeletes _propagatingDeletes: UInt32, _isNSEditorEditing _isNSEditorEditing: UInt32, _isMainThreadBlessed _isMainThreadBlessed: UInt32, _isImportContext _isImportContext: UInt32, _preflightSaveInProgress _preflightSaveInProgress: UInt32, _disableDiscardEditing _disableDiscardEditing: UInt32, _isParentStoreContext _isParentStoreContext: UInt32, _postSaveNotifications _postSaveNotifications: UInt32, _isMerging _isMerging: UInt32, _concurrencyType _concurrencyType: UInt32, _deleteInaccessible _deleteInaccessible: UInt32, _reservedFlags _reservedFlags: UInt32)
}
