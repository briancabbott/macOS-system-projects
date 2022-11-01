# -*- perl -*-

# t/001_load.t - check module loading and create testing directory

use Test::More 0.88;

BEGIN { use_ok('DateTime::Format::Strptime'); }

my $object = DateTime::Format::Strptime->new( pattern => '%T' );
isa_ok( $object, 'DateTime::Format::Strptime' );

done_testing();
