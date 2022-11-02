
@available(watchOS 2.0, *)
let NSManagedObjectContextWillSaveNotification: String
@available(watchOS 2.0, *)
let NSManagedObjectContextDidSaveNotification: String
@available(watchOS 2.0, *)
let NSManagedObjectContextObjectsDidChangeNotification: String
@available(watchOS 2.0, *)
let NSInsertedObjectsKey: String
@available(watchOS 2.0, *)
let NSUpdatedObjectsKey: String
@available(watchOS 2.0, *)
let NSDeletedObjectsKey: String
@available(watchOS 2.0, *)
let NSRefreshedObjectsKey: String
@available(watchOS 2.0, *)
let NSInvalidatedObjectsKey: String
@available(watchOS 2.0, *)
let NSInvalidatedAllObjectsKey: String
@available(watchOS 2.0, *)
enum NSManagedObjectContextConcurrencyType : UInt {
  init?(rawValue rawValue: UInt)
  var rawValue: UInt { get }
  @available(watchOS, introduced=2.0, deprecated=2.0, message="Use another NSManagedObjectContextConcurrencyType")
  case confinementConcurrencyType
  case privateQueueConcurrencyType
  case mainQueueConcurrencyType
}
@available(watchOS 2.0, *)
class NSManagedObjectContext : NSObject, NSCoding {
  @available(watchOS 2.0, *)
  init(concurrencyType ct: NSManagedObjectContextConcurrencyType)
  @available(watchOS 2.0, *)
  func perform(_ block: () -> Void)
  @available(watchOS 2.0, *)
  func performBlockAndWait(_ block: () -> Void)
  var persistentStoreCoordinator: NSPersistentStoreCoordinator?
  @available(watchOS 2.0, *)
  var parent: NSManagedObjectContext?
  @available(watchOS 2.0, *)
  var name: String?
  var undoManager: NSUndoManager?
  var hasChanges: Bool { get }
  @available(watchOS 2.0, *)
  var userInfo: NSMutableDictionary { get }
  @available(watchOS 2.0, *)
  var concurrencyType: NSManagedObjectContextConcurrencyType { get }
  func objectRegistered(for objectID: NSManagedObjectID) -> NSManagedObject?
  func object(with objectID: NSManagedObjectID) -> NSManagedObject
  @available(watchOS 2.0, *)
  func existingObject(with objectID: NSManagedObjectID) throws -> NSManagedObject
  func execute(_ request: NSFetchRequest) throws -> [AnyObject]
  @available(watchOS 2.0, *)
  func count(for request: NSFetchRequest, error error: NSErrorPointer) -> Int
  @available(watchOS 2.0, *)
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
  @available(watchOS 2.0, *)
  func refreshAllObjects()
  var propagatesDeletesAtEndOfEvent: Bool
  var retainsRegisteredObjects: Bool
  @available(watchOS 2.0, *)
  var shouldDeleteInaccessibleFaults: Bool
  @available(watchOS 2.0, *)
  func shouldHandleInaccessibleFault(_ fault: NSManagedObject, for oid: NSManagedObjectID, triggeredByProperty property: NSPropertyDescription?) -> Bool
  var stalenessInterval: NSTimeInterval
  var mergePolicy: AnyObject
  @available(watchOS 2.0, *)
  func obtainPermanentIDs(for objects: [NSManagedObject]) throws
  @available(watchOS 2.0, *)
  func mergeChanges(fromContextDidSave notification: NSNotification)
  @available(watchOS 2.0, *)
  class func mergeChanges(fromRemoteContextSave changeNotificationData: [NSObject : AnyObject], into contexts: [NSManagedObjectContext])
  @available(watchOS 2.0, *)
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
