#
#  OverdueTransformer.py
#  ToDos
#
#  Converted by u.fiedler on 09.02.05.
#
#  The original version was written in Objective-C by Malcolm Crawford
#  at http://homepage.mac.com/mmalc/CocoaExamples/controllers.html

from Foundation import *
from AppKit import *

class OverdueTransformer(NSValueTransformer):

    @classmethod
    def transformedValueClass(cls):
        return NSColor

    @classmethod
    def allowsReverseTransformation(cls):
        return False

    def transformedValue_(self, aDate):
        if aDate is None:
            return None
        if aDate.timeIntervalSinceNow() < 0:
            return NSColor.redColor()
        return NSColor.blackColor()
