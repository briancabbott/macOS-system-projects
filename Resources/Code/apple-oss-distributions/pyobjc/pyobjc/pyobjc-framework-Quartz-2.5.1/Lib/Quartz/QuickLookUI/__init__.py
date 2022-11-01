'''
Python mapping for the QuickLookUI framework.

This module does not contain docstrings for the wrapped code, check Apple's
documentation for details on how to use these functions and classes.
'''
import sys
import objc
import Cocoa

from Quartz.QuickLookUI import _metadata

sys.modules['Quartz.QuickLookUI'] = mod = objc.ObjCLazyModule('Quartz.QuickLookUI',
    "com.apple.QuickLookUIFramework",
    objc.pathForFramework("/System/Library/Frameworks/Quartz.framework/Frameworks/QuickLookUI.framework"),
    _metadata.__dict__, None, {
       '__doc__': __doc__,
       '__path__': __path__,
       'objc': objc,
    }, ( Cocoa,))
