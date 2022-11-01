#!/usr/bin/perl

use strict;
use warnings;

use Test::More;

use SOAP::Lite
  on_fault => sub {
    my $soap = shift;
    my $res = shift;
    ref $res ? warn(join " ", "SOAP FAULT:", $res->faultstring, "\n")
             : warn(join " ", "TRANSPORT ERROR:", $soap->transport->status, "\n");
    return new SOAP::SOM;
  }
;

my ($a, $s, $r);

my $proxy = 'http://services.soaplite.com/echo.cgi';

# ------------------------------------------------------
use SOAP::Test;

$s = SOAP::Lite->uri('http://something/somewhere')->proxy($proxy)->on_fault(sub{});
eval { $s->transport->timeout($SOAP::Test::TIMEOUT = $SOAP::Test::TIMEOUT) };
$r = $s->test_connection;

unless (defined $r && defined $r->envelope) {
  print "1..0 # Skip: ", $s->transport->status, "\n";
  exit;
}
# ------------------------------------------------------

plan tests => 16;

{
  print "Memory leaks test(s)...\n";

  SOAP::Lite->self(undef);

  my %calls = ();

  SOAP::Lite->import(trace => [objects => sub {
#    warn join ', ' , caller(2);
    my @caller = caller(2);
    $calls{$2}{$1}++ if ($caller[3] =~ /^(.+)::([^\:]+)$/);
  }]);
  {
    my $soap = SOAP::Lite
      -> uri("Echo")
      -> proxy($proxy)
      -> echo;
	use Data::Dumper;
#	find_cycle $soap;
  }
  use Data::Dumper;


  foreach (keys %{$calls{new}}) {
    print "default parser: $_\n";
    ok(exists $calls{DESTROY}{$_});
  }


  SKIP: {
      eval "require XML::Parser::Lite; 1;";
      skip "XML::Parser::Lite is required for this test", 8 if $@;

      %calls = ();
      {
          local $SOAP::Constants::DO_NOT_USE_XML_PARSER = 1;
          my $soap = SOAP::Lite
          -> uri("Echo")
          -> proxy($proxy)
          -> echo;
      }
      foreach (keys %{$calls{new}}) {
          print "XML::Parser::Lite: $_\n";
          ok(exists $calls{DESTROY}{$_});
      }
  }

  # SOAP::Lite->import(trace => '-objects');
}


