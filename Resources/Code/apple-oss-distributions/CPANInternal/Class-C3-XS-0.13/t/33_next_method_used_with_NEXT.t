#!/usr/bin/perl

use strict;
use warnings;

use Test::More;

BEGIN {
    eval "use NEXT";
    plan skip_all => "NEXT required for this test" if $@;
    plan tests => 4;
}

use Class::C3::XS;

{
    package Foo;
    use strict;
    use warnings;
    
    sub foo { 'Foo::foo' }
    
    package Fuz;
    use strict;
    use warnings;
    use base 'Foo';

    sub foo { 'Fuz::foo => ' . (shift)->next::method }
        
    package Bar;
    use strict;
    use warnings;    
    use base 'Foo';

    sub foo { 'Bar::foo => ' . (shift)->next::method }
    
    package Baz;
    use strict;
    use warnings;    
    require NEXT; # load this as late as possible so we can catch the test skip

    use base 'Bar', 'Fuz';
    
    sub foo { 'Baz::foo => ' . (shift)->NEXT::foo }    
}

is(Foo->foo, 'Foo::foo', '... got the right value from Foo->foo');
is(Fuz->foo, 'Fuz::foo => Foo::foo', '... got the right value from Fuz->foo');
is(Bar->foo, 'Bar::foo => Foo::foo', '... got the right value from Bar->foo');

is(Baz->foo, 'Baz::foo => Bar::foo => Fuz::foo => Foo::foo', '... got the right value using NEXT in a subclass of a C3 class');

