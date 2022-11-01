
from PyObjCTools.TestSupport import *
from AppKit import *

class TestNSDocumentController (TestCase):
    def testMethods(self):
        self.assertArgIsBOOL(NSDocumentController.openUntitledDocumentAndDisplay_error_, 0)
        self.assertArgIsOut(NSDocumentController.openUntitledDocumentAndDisplay_error_, 1)
        self.assertArgIsOut(NSDocumentController.makeUntitledDocumentOfType_error_, 1)
        self.assertArgIsBOOL(NSDocumentController.openDocumentWithContentsOfURL_display_error_, 1)
        self.assertArgIsOut(NSDocumentController.openDocumentWithContentsOfURL_display_error_, 2)
        self.assertArgIsOut(NSDocumentController.makeDocumentWithContentsOfURL_ofType_error_, 2)
        self.assertResultIsBOOL(NSDocumentController.reopenDocumentForURL_withContentsOfURL_error_)
        self.assertArgIsOut(NSDocumentController.reopenDocumentForURL_withContentsOfURL_error_, 2)
        self.assertArgIsOut(NSDocumentController.makeDocumentForURL_withContentsOfURL_ofType_error_, 3)
        self.assertResultIsBOOL(NSDocumentController.hasEditedDocuments)
        self.assertArgIsBOOL(NSDocumentController.reviewUnsavedDocumentsWithAlertTitle_cancellable_delegate_didReviewAllSelector_contextInfo_, 1)
        self.assertArgIsSEL(NSDocumentController.reviewUnsavedDocumentsWithAlertTitle_cancellable_delegate_didReviewAllSelector_contextInfo_, 3, b'v@:@'+objc._C_NSBOOL+b'^v')
        self.assertArgHasType(NSDocumentController.reviewUnsavedDocumentsWithAlertTitle_cancellable_delegate_didReviewAllSelector_contextInfo_, 4, b'^v')
        self.assertArgIsSEL(NSDocumentController.closeAllDocumentsWithDelegate_didCloseAllSelector_contextInfo_, 1, b'v@:@'+objc._C_NSBOOL+b'^v')
        self.assertArgHasType(NSDocumentController.closeAllDocumentsWithDelegate_didCloseAllSelector_contextInfo_, 2, b'^v')
        self.assertArgIsSEL(NSDocumentController.presentError_modalForWindow_delegate_didPresentSelector_contextInfo_, 3, b'v@:'+objc._C_NSBOOL+b'^v')
        self.assertArgHasType(NSDocumentController.presentError_modalForWindow_delegate_didPresentSelector_contextInfo_, 4, b'^v')
        self.assertResultIsBOOL(NSDocumentController.presentError_)
        self.assertArgIsOut(NSDocumentController.typeForContentsOfURL_error_, 1)
        self.assertResultIsBOOL(NSDocumentController.validateUserInterfaceItem_)
        self.assertArgIsBOOL(NSDocumentController.openDocumentWithContentsOfFile_display_, 1)
        self.assertArgIsBOOL(NSDocumentController.openDocumentWithContentsOfURL_display_, 1)
        self.assertArgIsBOOL(NSDocumentController.openUntitledDocumentOfType_display_, 1)
        self.assertArgIsBOOL(NSDocumentController.setShouldCreateUI_, 0)
        self.assertResultIsBOOL(NSDocumentController.shouldCreateUI)

    @min_os_level('10.7')
    def testMethods10_7(self):
        self.assertArgIsBOOL(NSDocumentController.openDocumentWithContentsOfURL_display_completionHandler_, 1)
        self.assertArgIsBlock(NSDocumentController.openDocumentWithContentsOfURL_display_completionHandler_, 2, b'v@')
        self.assertArgIsBOOL(NSDocumentController.reopenDocumentForURL_withContentsOfURL_display_completionHandler_, 2)
        self.assertArgIsBlock(NSDocumentController.reopenDocumentForURL_withContentsOfURL_display_completionHandler_, 3, b'v@' + objc._C_NSBOOL + b'@')
        self.assertArgIsBOOL(NSDocumentController.duplicateDocumentWithContentsOfURL_copying_displayName_error_, 1)
        self.assertArgIsOut(NSDocumentController.duplicateDocumentWithContentsOfURL_copying_displayName_error_, 3)

    @min_os_level('10.8')
    def testMethods10_8(self):
        self.assertArgIsBlock(NSDocumentController.beginOpenPanelWithCompletionHandler_, 0, b'v@')
        self.assertArgIsBlock(NSDocumentController.beginOpenPanel_forTypes_completionHandler_, 2, b'v' + objc._C_NSInteger)



if __name__ == "__main__":
    main()
