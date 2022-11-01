#
# Procedures that are used within DejaGnu
#
puts "DejaGnu=======1.3"

set frame_version	1.3
if ![info exists argv0] {
    send_error "Must use a version of Expect greater than 5.0\n"
    exit 1
}

#
# trap some signals so we know whats happening. These definitions are only
# temporary until we read in the library stuff
#
trap { send_user "\nterminated\n";             exit 1 } SIGTERM
trap { send_user "\ninterrupted by user\n";    exit 1 } SIGINT
trap { send_user "\nsegmentation violation\n"; exit 1 } SIGSEGV
trap { send_user "\nsigquit\n";                exit 1 } SIGQUIT


#
# Initialize a few global variables used by all tests.
# `reset_vars' resets several of these, we define them here to document their
# existence.  In fact, it would be nice if all globals used by some interface
# of dejagnu proper were documented here.
#
# Keep these all lowercase.  Interface variables used by the various
# testsuites (eg: the gcc testsuite) should be in all capitals
# (eg: TORTURE_OPTIONS).
#
set mail_logs   0		;# flag for mailing of summary and diff logs
set psum_file   "latest"	;# file name of previous summary to diff against
set testcnt	0		;# number of testcases that ran
set passcnt	0		;# number of testcases that passed
set failcnt	0		;# number of testcases that failed
set xfailcnt	0		;# number of testcases expected to fail which did
set xpasscnt	0		;# number of testcases that passed unexpectedly
set warncnt     0               ;# number of warnings
set errcnt      0               ;# number of errors
set unsupportedcnt 0		;# number of testcases that can't run
set unresolvedcnt 0		;# number of testcases whose result is unknown
set untestedcnt 0		;# number of untested testcases
set exit_status	0		;# exit code returned by this program
set xfail_flag  0
set xfail_prms	0
set sum_file	""		;# name of the file that contains the summary log
set base_dir	""		;# the current working directory
set logname     ""		;# the users login name
set passwd      ""
set prms_id	0               ;# GNATS prms id number
set bug_id	0               ;# optional bug id number
set dir		""		;# temp variable for directory names
set srcdir      "."		;# source directory containing the test suite
set ignoretests ""		;# list of tests to not execute
set objdir	"."		;# directory where test case binaries live
set makevars	""		;# FIXME: Is this used anywhere?
set reboot      0
set configfile  site.exp	;# (local to this file)
set multipass   ""		;# list of passes and var settings
set target_abbrev "unix"	;# environment (unix, sim, vx, etc.).
set errno	"";		;# 
#
# set communication parameters here
#
set netport     ""
set targetname  ""
set connectmode ""
set serialport  ""
set baud        ""
#
# These describe the host and target environments.
#
set build_triplet  ""		;# type of architecture to run tests on
set build_os	   ""		;# type of os the tests are running on
set build_vendor   ""		;# vendor name of the OS or workstation the test are running on
set build_cpu      ""		;# type of the cpu tests are running on
set host_triplet   ""		;# type of architecture to run tests on, sometimes remotely
set host_os	   ""		;# type of os the tests are running on
set host_vendor    ""		;# vendor name of the OS or workstation the test are running on
set host_cpu       ""		;# type of the cpu tests are running on
set target_triplet ""		;# type of architecture to run tests on, final remote
set target_os	   ""		;# type of os the tests are running on
set target_vendor  ""		;# vendor name of the OS or workstation the test are running on
set target_cpu     ""		;# type of the cpu tests are running on
set target_alias   ""		;# standard abbreviation of target

#
# some convenience abbreviations
#
if ![info exists hex] {
    set hex "0x\[0-9A-Fa-f\]+"
}
if ![info exists decimal] {
    set decimal "\[0-9\]+"
}

#
# set the base dir (current working directory)
#
set base_dir [pwd]

#
# These are tested in case they are not initialized in $configfile. They are
# tested here instead of the init module so they can be overridden by command
# line options.
#
if ![info exists all_flag] {
    set all_flag 0
}
if ![info exists binpath] {
    set binpath ""
}
if ![info exists debug] {
    set debug 0
}
if 0 {
    if ![info exists options] {
	set options ""
    }
}
if ![info exists outdir] {
    set outdir "."
}
if ![info exists reboot] {
    set reboot 1
}
if ![info exists all_runtests] {
    # FIXME: Can we create an empty array?
    # we don't have to (JWN 20 March 1998)
    #set all_runtests(empty) ""
}
if ![info exists tracelevel] {
    set tracelevel 0
}
if ![info exists verbose] {
    set verbose 0
}

#
# verbose [-n] [-log] [--] message [level]
#
# Print MESSAGE if the verbose level is >= LEVEL.
# The default value of LEVEL is 1.
# "-n" says to not print a trailing newline.
# "-log" says to add the text to the log file even if it won't be printed.
# Note that the apparent behaviour of `send_user' dictates that if the message
# is printed it is also added to the log file.
# Use "--" if MESSAGE begins with "-".
#
# This is defined here rather than in framework.exp so we can use it
# while still loading in the support files.
#
proc verbose { args } {
    global verbose
    set newline 1
    set logfile 0

    set i 0
    if { [string index [lindex $args 0] 0] == "-" } {
	for { set i 0 } { $i < [llength $args] } { incr i } {
	    if { [lindex $args $i] == "--" } {
		incr i
		break
	    } elseif { [lindex $args $i] == "-n" } {
		set newline 0
	    } elseif { [lindex $args $i] == "-log" } {
		set logfile 1
	    } elseif { [string index [lindex $args $i] 0] == "-" } {
		clone_output "ERROR: verbose: illegal argument: [lindex $args $i]"
		return
	    } else {
		break
	    }
	}
	if { [llength $args] == $i } {
	    clone_output "ERROR: verbose: nothing to print"
	    return
	}
    }

    set level 1
    if { [llength $args] > $i + 1 } {
	set level [lindex $args [expr $i+1]]
    }
    set message [lindex $args $i]
    
    if { $verbose >= $level } {
	# There is no need for the "--" argument here, but play it safe.
	# We assume send_user also sends the text to the log file (which
	# appears to be the case though the docs aren't clear on this).
	if { $newline } {
	    send_user -- "$message\n"
	} else {
	    send_user -- "$message"
	}
    } elseif { $logfile } {
	if { $newline } {
	    send_log "$message\n"
	} else {
	    send_log "$message"
	}
    }
}

#
# Transform a tool name to get the installed name.
# target_triplet is the canonical target name.  target_alias is the
# target name used when configure was run.
#
proc transform { name } {
    global target_triplet
    global target_alias
    global host_triplet
    
    if [string match $target_triplet $host_triplet] {
	return $name
    }
    if [string match "native" $target_triplet] {
	return $name
    }
    if [string match "" $target_triplet] {
	return $name
    } else {
	set tmp ${target_alias}-${name}
	verbose "Transforming $name to $tmp"
	return $tmp
    }
}

#
# findfile arg0 [arg1] [arg2]
#
# Find a file and see if it exists. If you only care about the false
# condition, then you'll need to pass a null "" for arg1.
#	arg0 is the filename to look for. If the only arg,
#            then that's what gets returned. If this is the
#            only arg, then if it exists, arg0 gets returned.
#            if it doesn't exist, return only the prog name.
#       arg1 is optional, and it's what gets returned if
#	     the file exists.
#       arg2 is optional, and it's what gets returned if
#            the file doesn't exist.
#
proc findfile { args } {    
    # look for the file
    verbose "Seeing if [lindex $args 0] exists." 2
    if [file exists [lindex $args 0]] {
	if { [llength $args] > 1 } {
	    verbose "Found file, returning [lindex $args 1]"
	    return [lindex $args 1]
	} else {
	    verbose "Found file, returning [lindex $args 0]"
	    return [lindex $args 0]
	}
    } else {
	if { [llength $args] > 2 } {
	    verbose "Didn't find file, returning [lindex $args 2]"
	    return [lindex $args 2]
	} else {
	    verbose "Didn't find file, returning [file tail [lindex $args 0]]"
	    return [transform [file tail [lindex $args 0]]]
	}
    }
}

#
# load_file [-1] [--] file1 [ file2 ... ]
#
# Utility to source a file.  All are sourced in order unless the flag "-1"
# is given in which case we stop after finding the first one.
# The result is 1 if a file was found, 0 if not.
# If a tcl error occurs while sourcing a file, we print an error message
# and exit.
#
# ??? Perhaps add an optional argument of some descriptive text to add to
# verbose and error messages (eg: -t "library file" ?).
#
proc load_file { args } {
    set i 0
    set only_one 0
    if { [lindex $args $i] == "-1" } {
	set only_one 1
	incr i
    }
    if { [lindex $args $i] == "--" } {
	incr i
    }

    set found 0
    foreach file [lrange $args $i end] {
	verbose "Looking for $file" 2
	if [file exists $file] {
	    set found 1
	    verbose "Found $file"
	    if { [catch "uplevel #0 source $file"] == 1 } {
		send_error "ERROR: tcl error sourcing $file.\n"
		global errorInfo
		if [info exists errorInfo] {
		    send_error "$errorInfo\n"
		}
		exit 1
	    }
	    if $only_one {
		break
	    }
	}
    }
    return $found
}

#
# Parse the arguments the first time looking for these.  We will ultimately
# parse them twice.  Things are complicated because:
# - we want to parse --verbose early on
# - we don't want config files to override command line arguments
#   (eg: $base_dir/$configfile vs --host/--target; $DEJAGNU vs --baud,
#   --connectmode, and --name)
# - we need some command line arguments before we can process some config files
#   (eg: --objdir before $objdir/$configfile, --host/--target before $DEJAGNU)
# The use of `arg_host_triplet' and `arg_target_triplet' lets us avoid parsing
# the arguments three times.
#

set arg_host_triplet ""
set arg_target_triplet ""
set arg_build_triplet ""
set argc [ llength $argv ]
for { set i 0 } { $i < $argc } { incr i } {
    set option [lindex $argv $i]

    # make all options have two hyphens
    switch -glob -- $option {
        "--*" {
        }
        "-*" {
	    set option "-$option"
        }
    }

    # split out the argument for options that take them
    switch -glob -- $option {
	"--*=*" {
	    set optarg [lindex [split $option =] 1]
	}
	"--ba*" -
	"--bu*" -
	"--co*" -
	"--ho*" -
	"--i*"  -
	"--m*"  -
	"--n*"  -
	"--ob*" -
	"--ou*" -
	"--sr*" -
	"--st*" -
        "--ta*" -
	"--to*" {
	    incr i
	    set optarg [lindex $argv $i]
	}
    }

    switch -glob -- $option {
	"--bu*" {			# (--build) the build host configuration
	    set arg_build_triplet $optarg
	    continue
	}
	
	"--ho*" {			# (--host) the host configuration
	    set arg_host_triplet $optarg
	    continue
	}
	
	"--ob*" {			# (--objdir) where the test case object code lives
	    set objdir $optarg
	    continue
	}

	"--sr*" {			# (--srcdir) where the testsuite source code lives
	    set srcdir $optarg
	    continue
	}
	
	"--ta*" {			# (--target) the target configuration
	    set arg_target_triplet $optarg
	    continue
	}

	"--to*" {			# (--tool) specify tool name
	    set tool $optarg
	    continue
        }
		
	"--v" -
	"--verb*" {			# (--verbose) verbose output
	    incr verbose
	    continue
	}
    }
}
verbose "Verbose level is $verbose"

#
# get the users login name
#
if [string match "" $logname] {
    if [info exists env(USER)] {
	set logname $env(USER)
    } else {
	if [info exists env(LOGNAME)] {
	    set logname $env(LOGNAME)
	} else {
	    # try getting it with whoami
	    catch "set logname [exec whoami]" tmp
	    if [string match "*couldn't find*to execute*" $tmp] {
		# try getting it with who am i
		unset tmp
		catch "set logname [exec who am i]" tmp
		if [string match "*Command not found*" $tmp] {	
		    send_user "ERROR: couldn't get the users login name\n"
		    set logname "Unknown"
		} else {
		    set logname [lindex [split $logname " !"] 1]
		}
	    }
	}
    }
}
verbose "Login name is $logname"

#
# Begin sourcing the config files.
# All are sourced in order.
#
# Search order:
#	$HOME/.dejagnurc -> $base_dir/$configfile -> $objdir/$configfile
#	-> installed -> $DEJAGNU
#
# ??? It might be nice to do $HOME last as it would allow it to be the
# ultimate override.  Though at present there is still $DEJAGNU.
#
# For the normal case, we rely on $base_dir/$configfile to set
# host_triplet and target_triplet.
#

load_file ~/.dejagnurc $base_dir/$configfile

#
# If objdir didn't get set in $base_dir/$configfile, set it to $base_dir.
# Make sure we source $objdir/$configfile in case $base_dir/$configfile doesn't
# exist and objdir was given on the command line.
#

if [expr [string match "." $objdir] || [string match $srcdir $objdir]] {
    set objdir $base_dir
} else {
    load_file $objdir/$configfile
}
verbose "Using test sources in $srcdir"
verbose "Using test binaries in $objdir"

set execpath [file dirname $argv0]
set libdir   [file dirname $execpath]/bluegnu
if [info exists env(BLUEGNULIBS)] {
    set libdir $env(BLUEGNULIBS)
}
verbose "Using $libdir to find libraries"

#
# If the host or target was given on the command line, override the above
# config files.  We allow $DEJAGNU to massage them though in case it would
# ever want to do such a thing.
#
if { $arg_host_triplet != "" } {
    set host_triplet $arg_host_triplet
}
if { $arg_build_triplet != "" } {
    set build_triplet $arg_build_triplet
}

# if we only specify --host, then that must be the build machne too, and we're
# stuck using the old functionality of a simple cross test
if [expr { $build_triplet == ""  &&  $host_triplet != "" } ] {
    set build_triplet $host_triplet
}
# if we only specify --build, then we'll use that as the host too
if [expr { $build_triplet != "" && $host_triplet == "" } ] {
    set host_triplet $build_triplet
}
unset arg_host_triplet arg_build_triplet

#
# If the build machine type hasn't been specified by now, use config.guess.
#

if [expr  { $build_triplet == ""  &&  $host_triplet == ""} ] {
    # find config.guess
    foreach dir "$libdir $libdir/.. $srcdir/.. $srcdir/../.." {
	verbose "Looking for $dir" 2
	if [file exists $dir/config.guess] {
	    set config_guess $dir/config.guess
	    verbose "Found $dir/config.guess"
	    break
	}
    }
    
    # get the canonical config name
    if ![info exists config_guess] {
	send_error "ERROR: Couldn't guess configuration.\n"
	exit 1
    }
    catch "exec $config_guess" build_triplet
    case $build_triplet in {
	{ "No uname command or uname output not recognized" "Unable to guess system type" } {
	    verbose "WARNING: Uname output not recognized"
	    set build_triplet unknown
	}
    }
    verbose "Assuming build host is $build_triplet"
    if { $host_triplet == "" } {
	set host_triplet $build_triplet
    }

}

#
# Figure out the target. If the target hasn't been specified, then we have to assume
# we are native.
#
if { $arg_target_triplet != "" } {
    set target_triplet $arg_target_triplet
} elseif { $target_triplet == "" } {
    set target_triplet $build_triplet
    verbose "Assuming native target is $target_triplet" 2
}
unset arg_target_triplet
#
# Default target_alias to target_triplet.
#
if ![info exists target_alias] {
    set target_alias $target_triplet
}

#
# Find and load the global config file if it exists.
# The global config file is used to set the connect mode and other
# parameters specific to each particular target.
# These files assume the host and target have been set.
#

if { [load_file -- $libdir/$configfile] == 0 } {
    # If $DEJAGNU isn't set either then there isn't any global config file.
    # Warn the user as there really should be one.
    if { ! [info exists env(DEJAGNU)] } {
	send_error "WARNING: Couldn't find the global config file.\n"
    }
}

if [info exists env(DEJAGNU)] {
    if { [load_file -- $env(DEJAGNU)] == 0 } {
	# It may seem odd to only issue a warning if there isn't a global
	# config file, but issue an error if $DEJAGNU is erroneously defined.
	# Since $DEJAGNU is set there is *supposed* to be a global config file,
	# so the current behaviour seems reasonable.
	send_error "ERROR: global config file $env(DEJAGNU) not found.\n"
	exit 1
    }
}

#
# parse out the config parts of the triplet name
#

# build values
if { $build_cpu == "" } {
    regsub -- "-.*-.*" ${build_triplet} "" build_cpu
}
if { $build_vendor == "" } {
    regsub -- "^\[a-z0-9\]*-" ${build_triplet} "" build_vendor
    regsub -- "-.*" ${build_vendor} "" build_vendor
}
if { $build_os == "" } {
    regsub -- ".*-.*-" ${build_triplet} "" build_os
}

# host values
if { $host_cpu == "" } {
    regsub -- "-.*-.*" ${host_triplet} "" host_cpu
}
if { $host_vendor == "" } {
    regsub -- "^\[a-z0-9\]*-" ${host_triplet} "" host_vendor
    regsub -- "-.*" ${host_vendor} "" host_vendor
}
if { $host_os == "" } {
    regsub -- ".*-.*-" ${host_triplet} "" host_os
}

# target values
if { $target_cpu == "" } {
    regsub -- "-.*-.*" ${target_triplet} "" target_cpu
}
if { $target_vendor == "" } {
    regsub -- "^\[a-z0-9\]*-" ${target_triplet} "" target_vendor
    regsub -- "-.*" ${target_vendor} "" target_vendor
}
if { $target_os == "" } {
    regsub -- ".*-.*-" ${target_triplet} "" target_os
}

#
# Parse the command line arguments.
#

set argc [ llength $argv ]
for { set i 0 } { $i < $argc } { incr i } {
    set option [ lindex $argv $i ]

    # make all options have two hyphens
    switch -glob -- $option {
        "--*" {
        }
        "-*" {
	    set option "-$option"
        }
    }

    # split out the argument for options that take them
    switch -glob -- $option {
	"--*=*" {
	    set optarg [lindex [split $option =] 1]
	}
	"--ba*"  -
	"--bu*"  -
	"--co*" -
	"--ho*" -
	"--i*"  -
	"--m*"  -
	"--n*"  -
	"--ob*" -
	"--ou*" -
	"--sr*" -
	"--st*" -

        "--ta*" -
	"--to*" {
	    incr i
	    set optarg [lindex $argv $i]
	}
    }

    switch -glob -- $option {
	"--V*" -
	"--vers*" {			# (--version) version numbers
	    send_user "Expect version is\t[exp_version]\n"
	    send_user "Tcl version is\t\t[ info tclversion ]\n"
	    send_user "Framework version is\t$frame_version\n"
	    exit
	}

	"--v*" {			# (--verbose) verbose output
	    # Already parsed.
	    continue
	}

	"--bu*" {			# (--build) the build host configuration
	    # Already parsed (and don't set again).  Let $DEJAGNU rename it.
	    continue
	}
	
	"--ho*" {			# (--host) the host configuration
	    # Already parsed (and don't set again).  Let $DEJAGNU rename it.
	    continue
	}
	
	"--ta*" {			# (--target) the target configuration
	    # Already parsed (and don't set again).  Let $DEJAGNU rename it.
	    continue
	}
	
	"--a*" {			# (--all) print all test output to screen
	    set all_flag 1
	    verbose "Print all test output to screen"
	    continue
	}
	
        "--ba*" {			# (--baud) the baud to use for a serial line
	    set baud $optarg
	    verbose "The baud rate is now $baud"
	    continue
	}
	
	"--co*" {			# (--connect) the connection mode to use
	    set connectmode $optarg
	    verbose "Comm method is $connectmode"
	    continue
	}
	
	"--d*" {			# (--debug) expect internal debugging
	    if [file exists ./dbg.log] {
		catch "exec rm -f ./dbg.log"
	    }
	    if { $verbose > 2 } {
		exp_internal -f dbg.log 1
	    } else {
		exp_internal -f dbg.log 0
	    }
	    verbose "Expect Debugging is ON"
	    continue
	}
	
	"--D[01]" {			# (-Debug) turn on Tcl debugger
	    verbose "Tcl debugger is ON"
	    continue
	}
	
	"--m*" {			# (--mail) mail the output
	    set mailing_list $optarg
            set mail_logs 1
	    verbose "Mail results to $mailing_list"
	    continue
	}
	
	"--r*" {			# (--reboot) reboot the target
	    set reboot 1
	    verbose "Will reboot the target (if supported)"
	    continue
	}
	
	"--ob*" {			# (--objdir) where the test case object code lives
	    # Already parsed, but parse again to make sure command line
	    # options override any config file.
	    set objdir $optarg
	    verbose "Using test binaries in $objdir"
	    continue
	}
	
	"--ou*" {			# (--outdir) where to put the output files
	    set outdir $optarg
	    verbose "Test output put in $outdir"
	    continue
	}
	
	"*.exp" {			#  specify test names to run
	    set all_runtests($option) ""
	    verbose "Running only tests $option"
	    continue
	}

	"*.exp=*" {			#  specify test names to run
	    set j [string first "=" $option]
	    set tmp [list [string range $option 0 [expr $j - 1]] \
		    [string range $option [expr $j + 1] end]]
	    set all_runtests([lindex $tmp 0]) [lindex $tmp 1]
	    verbose "Running only tests $option"
	    unset tmp j
	    continue
	}
	
	"--i*" {			#  (--ignore) specify test names to exclude
	    set ignoretests $optarg
	    verbose "Ignoring test $ignoretests"
	    continue
	}

	"--sr*" {			# (--srcdir) where the testsuite source code lives
	    # Already parsed, but parse again to make sure command line
	    # options override any config file.
	    
	    set srcdir $optarg
	    continue
	}
	
	"--st*" {			# (--strace) expect trace level
	    set tracelevel $optarg
	    strace $tracelevel
	    verbose "Source Trace level is now $tracelevel"
	    continue
	}
	
	"--n*" {			# (--name) the target's name
	    # ??? `targetname' is a confusing word to use here.
	    set targetname $optarg
	    verbose "Target name is now $targetname"
	    continue
	}
	
	"--to*" {			# (--tool) specify tool name
	    set tool $optarg
	    verbose "Testing $tool"
	    continue
        }
		
	"[A-Z]*=*" { # process makefile style args like CC=gcc, etc...
	    if [regexp "^(\[A-Z_\]+)=(.*)$" $option junk var val] {
		if {0 > [lsearch -exact $makevars $var]} {
		    lappend makevars "$var"
		    set $var $val
		} else {
		    set $var [concat [set $var] $val]
		}
		verbose "$var is now [set $var]"
		#append makevars "set $var $val;" ;# FIXME: Used anywhere?
		unset junk var val
	    } else {
		send_error "Illegal variable specification:\n"
		send_error "$option\n"
	    }
	    continue
	}
	
	"--he*" {			# (--help) help text
	    send_user "USAGE: runtest \[options...\]\n"
	    send_user "\t--all (-a)\t\tPrint all test output to screen\n"
	    send_user "\t--baud (-ba)\t\tThe baud rate\n"
	    send_user "\t--build \[string\]\t\tThe canonical config name of the build machine\n"
	    send_user "\t--host \[string\]\t\tThe canonical config name of the host machine\n"
	    send_user "\t--target \[string\]\tThe canonical config name of the target board\n"
	    send_user "\t--connect (-co)\t\[type\]\tThe type of connection to use\n"
	    send_user "\t--debug (-de)\t\tSet expect debugging ON\n"
	    send_user "\t--help (-he)\t\tPrint help text\n"
	    send_user "\t--ignore \[name(s)\]\tThe names of specific tests to ignore\n"
	    send_user "\t--mail \[name(s)\]\tWho to mail the results to\n"
	    send_user "\t--name \[name\]\t\tThe hostname of the target board\n"
	    send_user "\t--objdir \[name\]\t\tThe test suite binary directory\n"
	    send_user "\t--outdir \[name\]\t\tThe directory to put logs in\n"
	    send_user "\t--reboot \[name\]\t\tReboot the target (if supported)\n"
	    send_user "\t--srcdir \[name\]\t\tThe test suite source code directory\n"
	    send_user "\t--strace \[number\]\tSet expect tracing ON\n"
	    send_user "\t--tool\[name(s)\]\t\tRun tests on these tools\n"
	    send_user "\t--verbose (-v)\t\tEmit verbose output\n"
	    send_user "\t--version (-V)\t\tEmit all version numbers\n"
	    send_user "\t--D\[0-1\]\t\tTcl debugger\n"
	    send_user "\tscript.exp\[=arg(s)\]\tRun these tests only\n"
	    send_user "\tMakefile style arguments can also be used, ex. CC=gcc\n\n"
	    exit 0	
	}
	
	default {
	    send_error "\nIllegal Argument \"$option\"\n"
	    send_error "try \"runtest --help\" for option list\n"
	    exit 1
	}
	
    }
}

#
# check for a few crucial variables
#
if ![info exists tool] {
    send_error "WARNING: No tool specified\n"
    set tool ""
}

#
# initialize a few Tcl variables to something other than their default
#
if { $verbose > 2 } {
    log_user 1
} else {
    log_user 0
}

set timeout 10

#
# load_lib -- load a library by sourcing it
#
# If there a multiple files with the same name, stop after the first one found.
# The order is first look in the install dir, then in a parallel dir in the
# source tree, (up one or two levels), then in the current dir.
#
proc load_lib { file } {
    global verbose libdir srcdir base_dir execpath tool

    # ??? We could use `load_file' here but then we'd lose the "library file"
    # specific text in verbose and error messages.  Worth it?
    set found 0
    foreach dir "$libdir $libdir/lib [file dirname [file dirname $srcdir]]/bluegnu/lib $srcdir/lib . [file dirname [file dirname [file dirname $srcdir]]]/bluegnu/lib" {
	verbose "Looking for library file $dir/$file" 2
	if [file exists $dir/$file] {
	    set found 1
 	    verbose "Loading library file $dir/$file"
	    if { [catch "uplevel #0 source $dir/$file"] == 1 } {
		send_error "ERROR: tcl error sourcing library file $dir/$file.\n"
		global errorInfo
		if [info exists errorInfo] {
		    send_error "$errorInfo\n"
		}
		exit 1
	    }
	    break
	}
    }
    if { $found == 0 } {
	send_error "ERROR: Couldn't find library file $file.\n"
	exit 1
    }
}

#
# load the testing framework libraries
#
load_lib utils.exp
load_lib framework.exp
load_lib debugger.exp
load_lib remote.exp
load_lib target.exp

#
# open log files
#
open_logs

# print the config info
clone_output "Test Run By $logname on [timestamp -format %c]"
if [is3way]  {
    clone_output "Target is $target_triplet"
    clone_output "Host   is $host_triplet"  
    clone_output "Build  is $build_triplet"
} else {
    if [isnative] {
	clone_output "Native configuration is $target_triplet"
    } else {
	clone_output "Target is $target_triplet"
	clone_output "Host   is $host_triplet"
    }
}

clone_output "\n\t\t=== $tool tests ===\n"

#
# Find the tool init file. This is in the config directory of the tool's
# testsuite directory. These used to all be named $target_abbrev-$tool.exp,
# but as the $tool variable goes away, it's now just $target_abbrev.exp.
# First we look for a file named with both the abbrev and the tool names.
# Then we look for one named with just the abbrev name. Finally, we look for
# a file called default, which is the default actions, as some tools could
# be purely host based. Unknown is mostly for error trapping.
#

set found 0 
if ![info exists target_abbrev] {
    set target_abbrev "unix"
}
foreach dir "${srcdir}/config ${srcdir}/../config ${srcdir}/../../config ${srcdir}/../../../config" {
    foreach initfile "${target_abbrev}-${tool}.exp ${target_abbrev}.exp ${target_os}.exp default.exp unknown.exp" {
	verbose "Looking for tool init file ${dir}/${initfile}" 2
	if [file exists ${dir}/${initfile}] {
	    set found 1
	    verbose "Using ${dir}/${initfile} as tool init file."
	    if [catch "uplevel #0 source ${dir}/${initfile}"]==1 {
		send_error "ERROR: tcl error sourcing tool init file ${dir}/${initfile}.\n"
		if [info exists errorInfo] {
		    send_error "$errorInfo\n"
		}
		exit 1
	    }
	    break
	}
    }
    if $found {
	break
    }
}

if { $found == 0 } {
    send_error "ERROR: Couldn't find tool init file.\n"
    exit 1
}
unset found

#
# Trap some signals so we know what's happening.  These replace the previous
# ones because we've now loaded the library stuff.
#
if ![exp_debug] {
    foreach sig "{SIGTERM {terminated}} \
             {SIGINT  {interrupted by user}} \
             {SIGQUIT {interrupted by user}} \
             {SIGSEGV {segmentation violation}}" {
	trap { send_error "Got a [trap -name] signal, [lindex $sig 1]\n"; \
		   log_summary } [lindex $sig 0]
	verbose "setting trap for [lindex $sig 0] to \"[lindex $sig 1]\"" 1
    }
}
unset sig

#
# Setup for main test execution loop
#

if [info exists errorInfo] {
    unset errorInfo
}
reset_vars
# FIXME: The trailing '/' is deprecated and will go away at some point.
# Do not assume $srcdir has a trailing '/'.
append srcdir "/"
# make sure we have only single path delimiters
regsub -all "//*" $srcdir "/" srcdir


# If multiple passes requested, set them up.  Otherwise prepare just one.
# The format of `MULTIPASS' is a list of elements containing
# "{ name var1=value1 ... }" where `name' is a generic name for the pass and
# currently has no other meaning.

if { [info exists MULTIPASS] } {
    set multipass $MULTIPASS
}
if { $multipass == "" } {
    set multipass { "" }
}

# Pass varaibale passed as arguments into the queue
#
foreach var $makevars {
    if {[string compare $var "MULTIPASS"] != 0} {
	appendQueue Q0 "./tools/setVariable.exp=$var=[set $var]"
    }
}

foreach pass $multipass {
    # multipass_name is set for `record_test' to use (see framework.exp).
    if { [lindex $pass 0] != "" } {
	set multipass_name [lindex $pass 0]
	clone_output "Running pass `$multipass_name' ..."
	# Pass MULTIPASS into queue
	appendQueue Q0 "./tools/setVariable.exp=MULTIPASS=$pass"
    } else {
	set multipass_name ""
    }
    set restore ""
    foreach varval [lrange $pass 1 end] {
	# FIXME: doesn't handle a=b=c.
	set tmp [split $varval "="]
	set var [lindex $tmp 0]
	# Save previous value.
	if [info exists $var] {
	    lappend restore "$var [list [eval concat \$$var]]"
	} else {
	    lappend restore "$var"
	}
	# Handle "CFLAGS=$CFLAGS foo".
	# FIXME: Do we need to `catch' this?
	eval set $var \[concat [lindex $tmp 1]\]
	verbose "$var is now [eval concat \$$var]"
	unset tmp var
    }

    # look for the top level testsuites. if $tool doesn't
    # exist and there are no subdirectories in $srcdir, then
    # we default to srcdir.
    set test_top_dirs [lsort [getdirs ${srcdir} "$tool*"]]
    if { ${test_top_dirs} == "" } {
	set test_top_dirs ${srcdir}
    }
    verbose "Top level testsuite dirs are ${test_top_dirs}" 2
    foreach dir "${test_top_dirs}" {
	foreach test_name [lsort [find ${dir} *.exp]] {
	    if { ${test_name} == "" } {
		continue
	    }
	    # Ignore this one if asked to.
	    if ![string match "" ${ignoretests}] {
		if { 0 <= [lsearch ${ignoretests} [file tail ${test_name}]]} {
		    continue
		}
	    }
	    # Get the path after the $srcdir so we know the subdir we're in.
	    set subdir ""
	    regsub $srcdir [file dirname $test_name] "" subdir
	    if { "$srcdir" == "$subdir/" } {
		set subdir ""
	    }
	    # Check to see if the range of tests is limited,
	    # set `runtests' to a list of two elements: the script name
	    # and any arguments ("" if none).
	    if { [array size all_runtests] > 0 } {
		if { 0 > [lsearch [array names all_runtests] [file tail $test_name]]} {
		    continue
		}
		set runtests [list [file tail $test_name] $all_runtests([file tail $test_name])]
	    } else {
		set runtests [list [file tail $test_name] ""]
	    }
	    clone_output "Running $test_name ..."
	    ####################################################
	    #
	    # Append test to queue
	    #
	    if {[string length [lindex $runtests 1]] == 0} {
		appendQueue Q0 $test_name
	    } else {
		appendQueue Q0 [join [list $test_name \
			[lindex $runtests 1]] "="]
	    }
	    #
	    ####################################################
	}
    }

    # Restore the variables set by this pass.
    foreach varval $restore {
	if { [llength $varval] > 1 } {
	    verbose "Restoring [lindex $varval 0] to [lindex $varval 1]" 4
	    set [lindex $varval 0] [lindex $varval 1]
	} else {
	    verbose "Restoring [lindex $varval 0] to `unset'" 4
	    unset [lindex $varval 0]
	}
    }
}
#
# do quite a bit of cleaning
#
unset restore i
unset ignoretests
foreach var $makevars {
    unset $var
}
catch {unset tmp}
catch {unset makevars}
catch {unset pass}
catch {unset multipass}
catch {unset var}
catch {unset varval}
puts "======= DejaGnu"
