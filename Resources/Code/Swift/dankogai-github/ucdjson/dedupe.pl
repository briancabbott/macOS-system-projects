#!/usr/bin/env perl
use strict;
use warnings;
use File::Find ();
use File::Compare;
my ($srcdir, $dstdir) = @ARGV;
die "usage: $0 srcdir dstdir" unless -d $srcdir && -d $dstdir;
# Set the variable $File::Find::dont_use_nlink if you're using AFS,
# since AFS cheats.
# for the convenience of &wanted calls, including -eval statements:

use vars qw/*name *dir *prune/;
*name   = *File::Find::name;
*dir    = *File::Find::dir;
*prune  = *File::Find::prune;

sub wanted;

# Traverse desired filesystems
File::Find::find({wanted => \&wanted}, $srcdir);
exit;


sub wanted {
    my @srcstat = lstat($_) or return;
    -f _ or return;
    my $dstname = $name; $dstname =~ s{$srcdir}{$ENV{PWD}/$dstdir};
    my @dststat = lstat($dstname) or return;
    $srcstat[0] == $dststat[0] or die "cross-device hard link unsupported";
    $srcstat[1] == $dststat[1] and return; # already linked
    $srcstat[7] == $dststat[7] or return; # different size, different file
    compare($_, $dstname) and return; # different content;
    unlink $dstname or die "$name:$!";
    link($_, $dstname) or die "$name:$!";
    print("$name\n");
}

