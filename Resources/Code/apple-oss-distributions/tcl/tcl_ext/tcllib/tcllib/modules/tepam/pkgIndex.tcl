if {![package vsatisfies [package provide Tcl] 8.3]} {return}
package ifneeded tepam   0.1.0 [list source [file join $dir tepam.tcl]]
