=======================
Coding style for PyObjC 
=======================

Introduction
------------

This document describes the coding style for PyObjC.  Please use this style for
new code and try apply this style to existing code while working on it.

The management summary: 4-space indents in Python code, 1-TAB indents in C
code.

Python code
-----------

The coding style for core Python is used (see :pep:`8`).  For consistency with
Cocoa we use mixed-case identifiers (like ``lookUpClass``).

PyObjC extensions to Apple frameworks should be clearly marked as such, 
preferably by prefixing names with ``PyObjC`` or ``pyobjc``.  This should make
it clear to users where they should look for documentation of an item: The
Apple documentation or ours.

C code
------

The coding style for core Python is used (see :pep:`7`).  We use ``PyObjC`` 
instead of ``Py`` as the prefix for globally visible symbols.

All (Objective-)C files in ``Modules/objc/`` should include ``"pyobjc.h"`` as
their first include.  The (Objective-)C files in the wrappers for frameworks
should include ``"pyobjc-api.h"`` and should not use other headers files from
``pyobjc-core``.

The ``setup.py`` for a framework wrapper should defer most work to 
``pyobjc_setup.py``, like so:

.. code-block:: python

   from pyobjc_setup import setup

   setup(
      name='pyobjc-framework-AddressBook',
      version="2.3"
      description = "Wrappers for the framework AddressBook on Mac OS X",
      packages = [ "AddressBook" ],
      install_requires = [
          'pyobjc-core>=2.3b1',
          'pyobjc-framework-Cocoa>=2.3b1',
      ],
   )

The framework wrappers do *not* include a copy of ``pyobjc-api.h``, but 
dynamicly fetch that at build time.

Documentation
-------------

All items exported by the objc module and all PyObjC extensions to Apple
frameworks (the AppKit and Foundation modules) should be documented using
docstrings.

All documentation-- both standalone documents and docstrings-- should be
marked up using reStructuredText_ [ReST].

ReST in DocStrings
++++++++++++++++++

reStructuredText_ can be used in doc strings.   ReST in DocStrings works
exactly like a standalone ReST document, but the ReST is broken up slightly
differently.

To format the DocStrings to be ReST compatible, make the following
changes/additions to the source.  These examples were taken from source found
in the DocUtils source tree. 

(1) Add appropriate ReST style fields to the top of the document as comments::

        # Author: David Goodger
        # Contact: goodger@users.sourceforge.net
        # Copyright: This module has been placed in the public domain.

(2) Add a module level variable that indicates that ReST is used to format
    the text contained in the docstrings::

        __docformat__ = 'reStructuredText'
    
(3) Format all other DocStrings as one normally would in Python.   Use ReST
    style markup where appropriate.   For example, bulleted lists and
    sections might commonly appear in the module or class docstrings.   The
    docstrings for individual methods may use example blocks, hyperlinks, or
    any other ReST markup.
        
.. _reStructuredText: http://docutils.sourceforge.net/rst.html
