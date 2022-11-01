
from PyObjCTools.TestSupport import *
from AppKit import *

try:
    unicode
except NameError:
    unicode = str

class TestNSKeyValueBindingHelper (NSObject):
    def commitEditingWithDelegate_didCommitSelector_contextInfo_(self, d, s, i):
        return None

    def commitEditingAndReturnError_(self, v): return 1

    def commitEditing(self): return 1


class TestNSKeyValueBinding (TestCase):
    def testConstants(self):
        self.assertIsInstance(NSMultipleValuesMarker, NSObject)
        self.assertIsInstance(NSNoSelectionMarker, NSObject)
        self.assertIsInstance(NSNotApplicableMarker, NSObject)

        self.assertIsInstance(NSObservedObjectKey, unicode)
        self.assertIsInstance(NSObservedKeyPathKey, unicode)
        self.assertIsInstance(NSOptionsKey, unicode)


        self.assertIsInstance(NSAlignmentBinding, unicode)
        self.assertIsInstance(NSAlternateImageBinding, unicode)
        self.assertIsInstance(NSAlternateTitleBinding, unicode)
        self.assertIsInstance(NSAnimateBinding, unicode)
        self.assertIsInstance(NSAnimationDelayBinding, unicode)
        self.assertIsInstance(NSArgumentBinding, unicode)
        self.assertIsInstance(NSAttributedStringBinding, unicode)
        self.assertIsInstance(NSContentArrayBinding, unicode)
        self.assertIsInstance(NSContentArrayForMultipleSelectionBinding, unicode)
        self.assertIsInstance(NSContentBinding, unicode)
        self.assertIsInstance(NSContentHeightBinding, unicode)
        self.assertIsInstance(NSContentObjectBinding, unicode)
        self.assertIsInstance(NSContentObjectsBinding, unicode)
        self.assertIsInstance(NSContentSetBinding, unicode)
        self.assertIsInstance(NSContentValuesBinding, unicode)
        self.assertIsInstance(NSContentWidthBinding, unicode)
        self.assertIsInstance(NSCriticalValueBinding, unicode)
        self.assertIsInstance(NSDataBinding, unicode)
        self.assertIsInstance(NSDisplayPatternTitleBinding, unicode)
        self.assertIsInstance(NSDisplayPatternValueBinding, unicode)
        self.assertIsInstance(NSDocumentEditedBinding, unicode)
        self.assertIsInstance(NSDoubleClickArgumentBinding, unicode)
        self.assertIsInstance(NSDoubleClickTargetBinding, unicode)
        self.assertIsInstance(NSEditableBinding, unicode)
        self.assertIsInstance(NSEnabledBinding, unicode)
        self.assertIsInstance(NSFilterPredicateBinding, unicode)
        self.assertIsInstance(NSFontBinding, unicode)
        self.assertIsInstance(NSFontBoldBinding, unicode)
        self.assertIsInstance(NSFontFamilyNameBinding, unicode)
        self.assertIsInstance(NSFontItalicBinding, unicode)
        self.assertIsInstance(NSFontNameBinding, unicode)
        self.assertIsInstance(NSFontSizeBinding, unicode)
        self.assertIsInstance(NSHeaderTitleBinding, unicode)
        self.assertIsInstance(NSHiddenBinding, unicode)
        self.assertIsInstance(NSImageBinding, unicode)
        self.assertIsInstance(NSIsIndeterminateBinding, unicode)
        self.assertIsInstance(NSLabelBinding, unicode)
        self.assertIsInstance(NSManagedObjectContextBinding, unicode)
        self.assertIsInstance(NSMaximumRecentsBinding, unicode)
        self.assertIsInstance(NSMaxValueBinding, unicode)
        self.assertIsInstance(NSMaxWidthBinding, unicode)
        self.assertIsInstance(NSMinValueBinding, unicode)
        self.assertIsInstance(NSMinWidthBinding, unicode)
        self.assertIsInstance(NSMixedStateImageBinding, unicode)
        self.assertIsInstance(NSOffStateImageBinding, unicode)
        self.assertIsInstance(NSOnStateImageBinding, unicode)
        self.assertIsInstance(NSPredicateBinding, unicode)
        self.assertIsInstance(NSRecentSearchesBinding, unicode)
        self.assertIsInstance(NSRepresentedFilenameBinding, unicode)
        self.assertIsInstance(NSRowHeightBinding, unicode)
        self.assertIsInstance(NSSelectedIdentifierBinding, unicode)
        self.assertIsInstance(NSSelectedIndexBinding, unicode)
        self.assertIsInstance(NSSelectedLabelBinding, unicode)
        self.assertIsInstance(NSSelectedObjectBinding, unicode)
        self.assertIsInstance(NSSelectedObjectsBinding, unicode)
        self.assertIsInstance(NSSelectedTagBinding, unicode)
        self.assertIsInstance(NSSelectedValueBinding, unicode)
        self.assertIsInstance(NSSelectedValuesBinding, unicode)
        self.assertIsInstance(NSSelectionIndexesBinding, unicode)
        self.assertIsInstance(NSSelectionIndexPathsBinding, unicode)
        self.assertIsInstance(NSSortDescriptorsBinding, unicode)
        self.assertIsInstance(NSTargetBinding, unicode)
        self.assertIsInstance(NSTextColorBinding, unicode)
        self.assertIsInstance(NSTitleBinding, unicode)
        self.assertIsInstance(NSToolTipBinding, unicode)
        self.assertIsInstance(NSValueBinding, unicode)
        self.assertIsInstance(NSValuePathBinding, unicode)
        self.assertIsInstance(NSValueURLBinding, unicode)
        self.assertIsInstance(NSVisibleBinding, unicode)
        self.assertIsInstance(NSWarningValueBinding, unicode)
        self.assertIsInstance(NSWidthBinding, unicode)

        self.assertIsInstance(NSAllowsEditingMultipleValuesSelectionBindingOption, unicode)
        self.assertIsInstance(NSAllowsNullArgumentBindingOption, unicode)
        self.assertIsInstance(NSAlwaysPresentsApplicationModalAlertsBindingOption, unicode)
        self.assertIsInstance(NSConditionallySetsEditableBindingOption, unicode)
        self.assertIsInstance(NSConditionallySetsEnabledBindingOption, unicode)
        self.assertIsInstance(NSConditionallySetsHiddenBindingOption, unicode)
        self.assertIsInstance(NSContinuouslyUpdatesValueBindingOption, unicode)
        self.assertIsInstance(NSCreatesSortDescriptorBindingOption, unicode)
        self.assertIsInstance(NSDeletesObjectsOnRemoveBindingsOption, unicode)
        self.assertIsInstance(NSDisplayNameBindingOption, unicode)
        self.assertIsInstance(NSDisplayPatternBindingOption, unicode)
        self.assertIsInstance(NSHandlesContentAsCompoundValueBindingOption, unicode)
        self.assertIsInstance(NSInsertsNullPlaceholderBindingOption, unicode)
        self.assertIsInstance(NSInvokesSeparatelyWithArrayObjectsBindingOption, unicode)
        self.assertIsInstance(NSMultipleValuesPlaceholderBindingOption, unicode)
        self.assertIsInstance(NSNoSelectionPlaceholderBindingOption, unicode)
        self.assertIsInstance(NSNotApplicablePlaceholderBindingOption, unicode)
        self.assertIsInstance(NSNullPlaceholderBindingOption, unicode)
        self.assertIsInstance(NSRaisesForNotApplicableKeysBindingOption, unicode)
        self.assertIsInstance(NSPredicateFormatBindingOption, unicode)
        self.assertIsInstance(NSSelectorNameBindingOption, unicode)
        self.assertIsInstance(NSSelectsAllWhenSettingContentBindingOption, unicode)
        self.assertIsInstance(NSValidatesImmediatelyBindingOption, unicode)
        self.assertIsInstance(NSValueTransformerNameBindingOption, unicode)
        self.assertIsInstance(NSValueTransformerBindingOption, unicode)

    @min_os_level("10.5")
    def testConstants10_5(self):
        self.assertIsInstance(NSContentDictionaryBinding, unicode)
        self.assertIsInstance(NSExcludedKeysBinding, unicode)
        self.assertIsInstance(NSIncludedKeysBinding, unicode)
        self.assertIsInstance(NSInitialKeyBinding, unicode)
        self.assertIsInstance(NSInitialValueBinding, unicode)
        self.assertIsInstance(NSLocalizedKeyDictionaryBinding, unicode)
        self.assertIsInstance(NSTransparentBinding, unicode)
        self.assertIsInstance(NSContentPlacementTagBindingOption, unicode)

    @min_os_level("10.7")
    def testConstants10_7(self):
        self.assertIsInstance(NSPositioningRectBinding, unicode)

    def testFunctions(self):
        o = NSObject.alloc().init()
        self.assertIs(NSIsControllerMarker(o), False)
        self.assertIs(NSIsControllerMarker(NSMultipleValuesMarker), True)

    def testMethods(self):
        o = TestNSKeyValueBindingHelper.alloc().init()
        m = o.commitEditingWithDelegate_didCommitSelector_contextInfo_.__metadata__()
        self.assertEqual(m['arguments'][3]['sel_of_type'], b'v@:@Z^v')

        self.assertResultIsBOOL(TestNSKeyValueBindingHelper.commitEditing)

    @min_os_level('10.7')
    @expectedFailure
    def testMethods10_7(self):
        self.assertResultIsBOOL(TestNSKeyValueBindingHelper.commitEditingAndReturnError_)
        self.assertArgIsOut(TestNSKeyValueBindingHelper.commitEditingAndReturnError_, 0)



if __name__ == "__main__":
    main()
