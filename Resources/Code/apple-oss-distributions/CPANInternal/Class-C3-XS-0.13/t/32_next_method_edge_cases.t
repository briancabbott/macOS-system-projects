#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 12;

BEGIN { use_ok('Class::C3::XS') }

{

    {
        package Foo;
        use strict;
        use warnings;
        sub new { bless {}, $_[0] }
        sub bar { 'Foo::bar' }
    }

    # call the submethod in the direct instance

    my $foo = Foo->new();
    isa_ok($foo, 'Foo');

    can_ok($foo, 'bar');
    is($foo->bar(), 'Foo::bar', '... got the right return value');    

    # fail calling it from a subclass

    {
        package Bar;
        use strict;
        use warnings;
        our @ISA = ('Foo');
    }  
    
    my $bar = Bar->new();
    isa_ok($bar, 'Bar');
    isa_ok($bar, 'Foo');    
    
    # test it working with with Sub::Name
    SKIP: {    
        eval 'use Sub::Name';
        skip "Sub::Name is required for this test", 3 if $@;
    
        my $m = sub { (shift)->next::method() };
        Sub::Name::subname('Bar::bar', $m);
        {
            no strict 'refs';
            *{'Bar::bar'} = $m;
        }

        can_ok($bar, 'bar');
        my $value = eval { $bar->bar() };
        ok(!$@, '... calling bar() succedded') || diag $@;
        is($value, 'Foo::bar', '... got the right return value too');
    }
    
    # test it failing without Sub::Name
    {
        package Baz;
        use strict;
        use warnings;
        our @ISA = ('Foo');
    }      
    
    my $baz = Baz->new();
    isa_ok($baz, 'Baz');
    isa_ok($baz, 'Foo');    
    
    {
        my $m = sub { (shift)->next::method() };
        {
            no strict 'refs';
            *{'Baz::bar'} = $m;
        }

        eval { $baz->bar() };
        ok($@, '... calling bar() with next::method failed') || diag $@;
    }    
}
