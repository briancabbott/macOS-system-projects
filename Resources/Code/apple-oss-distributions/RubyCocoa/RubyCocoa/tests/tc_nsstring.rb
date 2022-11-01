#
#  $Id: tc_nsstring.rb 2263 2009-09-23 04:01:42Z kimuraw $
#
#  Copyright (c) 2005 kimura wataru
#  Copyright (c) 2001-2003 FUJIMOTO Hisakuni
#

require 'test/unit'
require 'osx/cocoa'
require 'kconv'

class TC_NSString < Test::Unit::TestCase
  include OSX

  def setup
    @teststr = "Hello World".freeze
    @eucstr = "���֥������Ȼظ�������ץȸ���Ruby".toeuc.freeze
    $KCODE = 'NONE'
  end

  def teardown
    $KCODE = 'NONE'
  end

  def test_s_alloc
    obj = NSString.alloc.init
    assert( obj.isKindOfClass?(NSString) )
  end

  def test_s_stringWithString
    obj = NSString.stringWithString(@teststr)
    assert_equal(@teststr, obj.to_s)
  end

  def test_initWithString
    obj = NSString.alloc.initWithString(@teststr)
    assert_equal(@teststr, obj.to_s)
  end

  def test_stringWithRubyString_euc
    $KCODE = 'EUC'
    nsstr = NSString.stringWithRubyString( @eucstr )
    assert_equal( @eucstr, nsstr.to_s.toeuc )
  end

  def test_stringWithRubyString_sjis
    $KCODE = 'EUC'
    nsstr = NSString.stringWithRubyString( @eucstr.tosjis )
    assert_equal( @eucstr, nsstr.to_s.toeuc )
  end

  def test_stringWithRubyString_jis
    $KCODE = 'EUC'
    nsstr = NSString.stringWithRubyString( @eucstr.tojis )
    assert_equal( @eucstr, nsstr.to_s.toeuc )
  end

  def test_dataUsingEncoding_euc
    nsstr = NSString.stringWithRubyString( @eucstr )
    data = nsstr.dataUsingEncoding( NSJapaneseEUCStringEncoding )
    bytes = "." * data.length
    data.getBytes_length( bytes )
    assert_equal( @eucstr, bytes )
  end

  def test_dataUsingEncoding_sjis
    nsstr = NSString.stringWithRubyString( @eucstr )
    data = nsstr.dataUsingEncoding( NSShiftJISStringEncoding )
    bytes = "." * data.length
    data.getBytes_length( bytes )
    assert_equal( @eucstr.tosjis, bytes )
  end

  def test_dataUsingEncoding_jis
    nsstr = NSString.stringWithRubyString( @eucstr )
    data = nsstr.dataUsingEncoding( NSISO2022JPStringEncoding )
    bytes = "." * data.length
    data.getBytes_length( bytes )
    assert_equal( @eucstr.tojis, bytes )
  end
  
  def test_copy
    assert_nothing_raised {
      a = NSString.stringWithString('abc')
      b = a.dup
      b += 'def'
      b = a.clone
      b += 'def'
    }
  end

  def test_hash
    str1 = NSString.stringWithString('abc')
    str2 = NSString.stringWithString('abc')
    assert(str1.hash == str2.hash)
  end
  
  def test_to_ns
    assert_equal(OSX::NSString.stringWithString('abc'), 'abc'.to_ns)
    assert_equal(nil, "\270\236\010\210\245".to_ns)
  end
end
