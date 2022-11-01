.. title:: clang-tidy - misc-bool-pointer-implicit-conversion

misc-bool-pointer-implicit-conversion
=====================================


Checks for conditions based on implicit conversion from a bool pointer to
bool.

Example:

.. code:: c++

  bool *p;
  if (p) {
    // Never used in a pointer-specific way.
  }

