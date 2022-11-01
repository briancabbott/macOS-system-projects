#!/usr/bin/perl
#
# version.pl
# $Id: version.pl,v 1.1.1.1 2005-10-13 22:21:48 echristo Exp $
#
# Parse the NASM version file and produce appropriate macros
#
# The NASM version number is assumed to consist of:
#
# <major>.<minor>[.<subminor>][pl<patchlevel>]]<tail>
#
# ... where <tail> is not necessarily numeric.
#
# This defines the following macros:
#
# version.h:
# NASM_MAJOR_VER
# NASM_MINOR_VER
# NASM_SUBMINOR_VER	-- this is zero if no subminor
# NASM_PATCHLEVEL_VER	-- this is zero is no patchlevel
# NASM_VERSION_ID       -- version number encoded
# NASM_VER		-- whole version number as a string
#
# version.mac:
# __NASM_MAJOR__
# __NASM_MINOR__
# __NASM_SUBMINOR__
# __NASM_PATCHLEVEL__
# __NASM_VERSION_ID__
# __NASM_VER__
#

($what) = @ARGV;

$line = <STDIN>;
chomp $line;

undef $man, $min, $smin, $plvl, $tail;

if ( $line =~ /^([0-9]+)\.([0-9]+)/ ) {
    $maj  = $1;
    $min  = $2;
    $tail = $';
    if ( $tail =~ /^\.([0-9]+)/ ) {
	$smin = $1;
	$tail = $';
    }
    if ( $tail =~ /^(pl|\.)([0-9]+)/ ) {
	$plvl = $2;
	$tail = $';
    }
} else {
    die "$0: Invalid input format\n";
}

$nmaj = $maj+0;   $nmin = $min+0;
$nsmin = $smin+0; $nplvl = $plvl+0;

$nasm_id = ($nmaj << 24)+($nmin << 16)+($nsmin << 8)+$nplvl;

if ( $what eq 'h' ) {
    print  "#ifndef NASM_VERSION_H\n";
    print  "#define NASM_VERSION_H\n";
    printf "#define NASM_MAJOR_VER      %d\n", $nmaj;
    printf "#define NASM_MINOR_VER      %d\n", $nmin;
    printf "#define NASM_SUBMINOR_VER   %d\n", $nsmin;
    printf "#define NASM_PATCHLEVEL_VER %d\n", $nplvl;
    printf "#define NASM_VERSION_ID     0x%08x\n", $nasm_id;
    printf "#define NASM_VER            \"%s\"\n", $line;
    print  "#endif /* NASM_VERSION_H */\n";
} elsif ( $what eq 'mac' ) {
    printf "%%define __NASM_MAJOR__ %d\n", $nmaj;
    printf "%%define __NASM_MINOR__ %d\n", $nmin;
    printf "%%define __NASM_SUBMINOR__ %d\n", $nsmin;
    printf "%%define __NASM_PATCHLEVEL__ %d\n", $nplvl;
    printf "%%define __NASM_VERSION_ID__ 0%08Xh\n", $nasm_id;
    printf "%%define __NASM_VER__ \"%s\"\n", $line;
} elsif ( $what eq 'id' ) {
    print $nasm_id, "\n";	 # Print ID in decimal
} elsif ( $what eq 'xid' ) {
    printf "0x%08x\n", $nasm_id; # Print ID in hexadecimal
} else {
    die "$0: Unknown output: $what\n";
}

exit 0;

