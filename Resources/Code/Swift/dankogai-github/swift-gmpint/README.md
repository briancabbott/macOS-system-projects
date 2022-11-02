swift-gmpint
============

Swift binding to GMP Integer

Prereqisite:
------------

[GMP].  The project file is configured for GMP installed via [MacPorts].  In other words, the prefix for includes and library is `/opt/local`.  If you are using different prefix, adjust the project setting accordingly.

[GMP]: https://gmplib.org/
[MacPorts]: http://www.macports.org/

Bugs and Workarounds for GMP prior to 6.0.0_1
---------------------------------------------

GMP 6.0.0 with MacPorts 2.3.1 build with XCode 6 Beta 2 had a strange bug on division operations where the denominator fits `unsigned int`.  It fails like this.

````
libdyld.dylib`stack_not_16_byte_aligned_error:
````

To workaround it, this project left-shifts both numerator and denominator 64 bits to make sure the operation is _true bigint_ on _true bigint_.  That yields the same quotient with left-shifted remainder.  The remainder is then right-shifted 64 bits.  Therefore the division operation is not as optimal as it should be.

This bug is fixed in 6.0.0_1 so the workaround was also removed.
