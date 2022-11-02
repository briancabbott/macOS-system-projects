
@available(iOS 5.0, *)
class NSSaveChangesRequest : NSPersistentStoreRequest {
  init(insertedObjects insertedObjects: Set<NSManagedObject>?, updatedObjects updatedObjects: Set<NSManagedObject>?, deletedObjects deletedObjects: Set<NSManagedObject>?, lockedObjects lockedObjects: Set<NSManagedObject>?)
  var insertedObjects: Set<NSManagedObject>? { get }
  var updatedObjects: Set<NSManagedObject>? { get }
  var deletedObjects: Set<NSManagedObject>? { get }
  var lockedObjects: Set<NSManagedObject>? { get }
}
