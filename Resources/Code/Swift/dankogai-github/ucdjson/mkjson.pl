#!/usr/bin/env perl

use strict;
use warnings;
use XML::Simple;
use JSON;
use File::Path qw(make_path);

my $version = shift or die "usage: $0 version";
my $flat = 'ucd.all.flat';
my $xmlfn = "$version.xml";
my $url = "https://www.unicode.org/Public/$version/ucdxml/$flat.zip";


if ( !-f $xmlfn ) {
    my $err;
    $err = system qw/curl -fOR/, $url;
    die "download failed" if $err;
    $err = system qw/unzip/, "$flat.zip";
    die "unzip failed" if $err;
    unlink "$flat.zip";
    rename "$flat.xml", $xmlfn or die !$;
}

warn "reading $xmlfn...\n";
my $ucd = XMLin($xmlfn) or die $!;
my $jx  = JSON->new->canonical(1);

{
    my @aref;
    my $aref = $ucd->{repertoire}{char};
    warn 0 + @$aref, " char found.\n";
    for (@$aref) {
        my ( $dir, $path );
        if ( !$_->{cp} ) {
            push @aref, $_;
            next;
        }
        my $cp = sprintf "%06X", hex $_->{cp};
        $cp =~ /\A(..)(..)(..)/;
        $dir  = "ucd/$version/$1/$2";
        $path = "$dir/$3.json";
        if ( !-d $dir ) {
            make_path($dir);
            warn $dir;
        }
        open my $wh, '>:utf8', $path or die "$path:$!";
        print $wh $jx->encode($_);
    }
    $ucd->{repertoire}{char} = \@aref;
}
{
    for my $k (keys %$ucd) {
        my $dir = "ucd/$version";
        my $path = "$dir/$k.json";
        if ( !-d $dir ) {
            make_path($dir);
            warn $dir;
        }
        open my $wh, '>:utf8', $path or die "$path:$!";
        warn $path;
        print $wh $jx->encode($ucd->{$k});
    }
}
