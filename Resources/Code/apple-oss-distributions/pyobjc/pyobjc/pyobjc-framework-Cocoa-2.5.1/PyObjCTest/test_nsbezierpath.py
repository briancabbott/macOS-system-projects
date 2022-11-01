# Test special methods of NSBezierPath
# TODO: Test implementing these in python.
from PyObjCTools.TestSupport import *
import objc

from AppKit import *

class TestNSBezierPath(TestCase):
    if not hasattr(TestCase, 'assertAlmostEquals'):
        def assertAlmostEquals(self, val1, val2):
            self.assert_(abs(val1 - val2) < 0.000001)

    def assertPointEquals(self, point1, point2):
        self.assertAlmostEquals(point1[0], point2[0])
        self.assertAlmostEquals(point1[1], point2[1])

    def test_creation(self):
        p = NSBezierPath.bezierPath()
        self.assert_(p is not None)
        self.assertEqual(p.elementCount(), 0)

        p = NSBezierPath.bezierPathWithOvalInRect_(((0, 0), (100, 50)))
        self.assert_(p is not None)
        self.assertEqual(p.elementCount(), 5)

    def test_appendPoints(self):
        p = NSBezierPath.bezierPath()
        self.assert_(p is not None)
        self.assertEqual(p.elementCount(), 0)

        points = [ (0, 0), (100, 0), (100, 100), (0, 0) ]
        p.appendBezierPathWithPoints_count_(points, 3)
        self.assertEqual(p.elementCount(), 3)

    def test_setLineDash(self):
        p = NSBezierPath.bezierPath()
        p.setLineDash_count_phase_((10, 10, 20, 5), 4, 45.0)

        pattern, count, phase = p.getLineDash_count_phase_(objc.NULL, 0, None)
        #self.assertEqual(pattern, None)
        self.assertEqual(pattern, objc.NULL)
        self.assertEqual(count, 4)
        self.assertAlmostEquals(phase, 45.0)

        pattern, count, phase = p.getLineDash_count_phase_(None, 4, None)
        self.assertAlmostEquals(pattern[0], 10)
        self.assertAlmostEquals(pattern[1], 10)
        self.assertAlmostEquals(pattern[2], 20)
        self.assertAlmostEquals(pattern[3], 5)
        self.assertEqual(count, 4)
        self.assertAlmostEquals(phase, 45.0)

    def test_elementAtIndex(self):
        p = NSBezierPath.bezierPath()
        p.moveToPoint_((10, 10))
        p.lineToPoint_((20, 30))
        p.lineToPoint_((30, 20))
        p.curveToPoint_controlPoint1_controlPoint2_((40, 41), (10, 11), (20, 21))
        p.closePath()

        self.assertEqual(p.elementAtIndex_(0), NSMoveToBezierPathElement)
        self.assertEqual(p.elementAtIndex_(1), NSLineToBezierPathElement)
        self.assertEqual(p.elementAtIndex_(2), NSLineToBezierPathElement)
        self.assertEqual(p.elementAtIndex_(3), NSCurveToBezierPathElement)
        self.assertEqual(p.elementAtIndex_(4), NSClosePathBezierPathElement)

        tp, points = p.elementAtIndex_associatedPoints_(0)
        self.assertEqual(tp, NSMoveToBezierPathElement)
        self.assertEqual(len(points), 1)
        self.assertPointEquals(points[0], (10, 10))

        tp, points = p.elementAtIndex_associatedPoints_(1)
        self.assertEqual(tp, NSLineToBezierPathElement)
        self.assertEqual(len(points), 1)
        self.assertPointEquals(points[0], (20, 30))

        tp, points = p.elementAtIndex_associatedPoints_(2)
        self.assertEqual(tp, NSLineToBezierPathElement)
        self.assertEqual(len(points), 1)
        self.assertPointEquals(points[0], (30, 20))

        tp, points = p.elementAtIndex_associatedPoints_(3)
        self.assertEqual(tp, NSCurveToBezierPathElement)
        self.assertEqual(len(points), 3)
        self.assertPointEquals(points[0], (10, 11)) # control point 1
        self.assertPointEquals(points[1], (20, 21)) # control point 2
        self.assertPointEquals(points[2], (40, 41)) # end point

        tp, points = p.elementAtIndex_associatedPoints_(4)
        self.assertEqual(tp, NSClosePathBezierPathElement)
        self.assertEqual(len(points), 0)

    def test_setAssociatedPoints(self):
        p = NSBezierPath.bezierPath()
        p.moveToPoint_((10, 10))
        p.lineToPoint_((20, 30))
        p.lineToPoint_((30, 20))
        p.curveToPoint_controlPoint1_controlPoint2_((40, 41), (10, 11), (20, 21))
        p.closePath()

        p.setAssociatedPoints_atIndex_([(0, 1)], 0)
        tp, points = p.elementAtIndex_associatedPoints_(0)
        self.assertEqual(tp, NSMoveToBezierPathElement)
        self.assertEqual(len(points), 1)
        self.assertPointEquals(points[0], (0, 1))

        p.setAssociatedPoints_atIndex_([(0, 1), (2,3), (3,4)], 3)
        tp, points = p.elementAtIndex_associatedPoints_(3)
        self.assertEqual(tp, NSCurveToBezierPathElement)
        self.assertEqual(len(points), 3)
        self.assertPointEquals(points[0], (0, 1)) # control point 1
        self.assertPointEquals(points[1], (2, 3)) # control point 2
        self.assertPointEquals(points[2], (3, 4)) # end point


    def testConstants(self):
        self.assertEqual(NSButtLineCapStyle, 0)
        self.assertEqual(NSRoundLineCapStyle, 1)
        self.assertEqual(NSSquareLineCapStyle, 2)
        self.assertEqual(NSMiterLineJoinStyle, 0)
        self.assertEqual(NSRoundLineJoinStyle, 1)
        self.assertEqual(NSBevelLineJoinStyle, 2)
        self.assertEqual(NSNonZeroWindingRule, 0)
        self.assertEqual(NSEvenOddWindingRule, 1)
        self.assertEqual(NSMoveToBezierPathElement, 0)
        self.assertEqual(NSLineToBezierPathElement, 1)
        self.assertEqual(NSCurveToBezierPathElement, 2)
        self.assertEqual(NSClosePathBezierPathElement, 3)


    def testMethods(self):
        self.assertResultIsBOOL(NSBezierPath.isEmpty)
        self.assertResultIsBOOL(NSBezierPath.containsPoint_)
        self.assertResultIsBOOL(NSBezierPath.cachesBezierPath)
        self.assertArgIsBOOL(NSBezierPath.setCachesBezierPath_, 0)


if __name__ == '__main__':
    main( )
