'''
Python mapping for the DictionaryServices framework.

This module does not contain docstrings for the wrapped code, check Apple's
documentation for details on how to use these functions and classes.
'''
import sys
import objc
import Foundation

from DictionaryServices import _metadata

sys.modules['DictionaryServices'] = mod = objc.ObjCLazyModule('DictionaryServices',
    "com.apple.DictionaryServices",
    objc.pathForFramework("/System/Library/Frameworks/CoreServices.framework/Frameworks/DictionaryServices.framework"),
    _metadata.__dict__, None, {
       '__doc__': __doc__,
       '__path__': __path__,
       'objc': objc,
    }, ( Foundation,))
