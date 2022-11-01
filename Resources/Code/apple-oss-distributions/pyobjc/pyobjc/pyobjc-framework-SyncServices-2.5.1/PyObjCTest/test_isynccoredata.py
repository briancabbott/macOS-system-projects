
from PyObjCTools.TestSupport import *
from SyncServices import *

class TestISyncCoreDataHelper (NSObject):
    def persistentStoreCoordinatorShouldStartSyncing_(self, v):
        return True

    def persistentStoreCoordinator_willDeleteRecordWithIdentifier_inSyncSession_(self, c, i, s):
        return True



class TestISyncCoreData (TestCase):
    @min_os_level("10.5")
    def testMethods(self):
        self.assertResultIsBOOL(NSPersistentStoreCoordinator.syncWithClient_inBackground_handler_error_)
        self.assertArgIsBOOL(NSPersistentStoreCoordinator.syncWithClient_inBackground_handler_error_, 1)
        self.assertArgIsOut(NSPersistentStoreCoordinator.syncWithClient_inBackground_handler_error_, 3)

    def testProtocols(self):
        self.assertResultIsBOOL(TestISyncCoreDataHelper.persistentStoreCoordinatorShouldStartSyncing_)
        self.assertResultIsBOOL(TestISyncCoreDataHelper.persistentStoreCoordinator_willDeleteRecordWithIdentifier_inSyncSession_)

if __name__ == "__main__":
    main()
