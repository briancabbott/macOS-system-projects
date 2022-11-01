'''
Python mapping for the QuartzCore framework.

This module does not contain docstrings for the wrapped code, check Apple's
documentation for details on how to use these functions and classes.
'''
import sys
import objc
import Foundation

from Quartz.QuartzCore import _metadata

sys.modules['Quartz.QuartzCore'] = mod = objc.ObjCLazyModule('Quartz.QuartzCore',
    "com.apple.QuartzCore",
    objc.pathForFramework("/System/Library/Frameworks/QuartzCore.framework"),
    _metadata.__dict__, None, {
       '__doc__': __doc__,
       '__path__': __path__,
       'objc': objc,
    }, (Foundation,))
