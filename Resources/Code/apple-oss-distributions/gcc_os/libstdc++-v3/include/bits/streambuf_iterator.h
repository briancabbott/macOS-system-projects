// Streambuf iterators

// Copyright (C) 1997, 1998, 1999, 2000, 2001, 2002, 2003
// Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License along
// with this library; see the file COPYING.  If not, write to the Free
// Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
// USA.

// As a special exception, you may use this file as part of a free software
// library without restriction.  Specifically, if other files instantiate
// templates or use macros or inline functions from this file, or you compile
// this file and link it with other files to produce an executable, this
// file does not by itself cause the resulting executable to be covered by
// the GNU General Public License.  This exception does not however
// invalidate any other reasons why the executable file might be covered by
// the GNU General Public License.

/** @file streambuf_iterator.h
 *  This is an internal header file, included by other library headers.
 *  You should not attempt to use it directly.
 */

#ifndef _CPP_BITS_STREAMBUF_ITERATOR_H
#define _CPP_BITS_STREAMBUF_ITERATOR_H 1

#pragma GCC system_header

#include <streambuf>

/* APPLE LOCAL begin libstdc++ debug mode */
#include <debug/debug.h>
/* APPLE LOCAL end libstdc++ debug mode */

// NB: Should specialize copy, find algorithms for streambuf iterators.

namespace std
{
  // 24.5.3 Template class istreambuf_iterator
  template<typename _CharT, typename _Traits>
    class istreambuf_iterator
    : public iterator<input_iterator_tag, _CharT, typename _Traits::off_type,
    		      _CharT*, _CharT&>
    {
    public:
      // Types:
      typedef _CharT                         		char_type;
      typedef _Traits                        		traits_type;
      typedef typename _Traits::int_type     		int_type;
      typedef basic_streambuf<_CharT, _Traits> 		streambuf_type;
      typedef basic_istream<_CharT, _Traits>         	istream_type;

    private:
      // 24.5.3 istreambuf_iterator 
      // p 1 
      // If the end of stream is reached (streambuf_type::sgetc()
      // returns traits_type::eof()), the iterator becomes equal to
      // the "end of stream" iterator value.
      mutable streambuf_type* 	_M_sbuf;  
      int_type 			_M_c;

    public:
      istreambuf_iterator() throw() 
      : _M_sbuf(0), _M_c(traits_type::eof()) { }
      
      istreambuf_iterator(istream_type& __s) throw()
      : _M_sbuf(__s.rdbuf()), _M_c(traits_type::eof()) { }

      istreambuf_iterator(streambuf_type* __s) throw()
      : _M_sbuf(__s), _M_c(traits_type::eof()) { }
       
      char_type 
      operator*() const
      { 
/* APPLE LOCAL begin libstdc++ debug mode */
#ifdef _GLIBCXX_DEBUG_PEDANTIC
	// Dereferencing a past-the-end istreambuf_iterator is a
	// libstdc++ extension
	__glibcxx_requires_cond(!_M_at_eof(),
				_M_message(__debug::__dbg_msg_deref_istreambuf)
				._M_iterator(*this)); 
#endif
/* APPLE LOCAL end libstdc++ debug mode */
	return traits_type::to_char_type(_M_get()); 
      }
	
      istreambuf_iterator& 
      operator++()
      { 
/* APPLE LOCAL begin libstdc++ debug mode */
	__glibcxx_requires_cond(!_M_at_eof(),
				_M_message(__debug::__dbg_msg_inc_istreambuf)
				._M_iterator(*this)); 
/* APPLE LOCAL end libstdc++ debug mode */
	const int_type __eof = traits_type::eof();
	if (_M_sbuf && traits_type::eq_int_type(_M_sbuf->sbumpc(), __eof))
	  _M_sbuf = 0;
	else
	  _M_c = __eof;
	return *this; 
      }

      istreambuf_iterator
      operator++(int)
      {
/* APPLE LOCAL begin libstdc++ debug mode */
	__glibcxx_requires_cond(!_M_at_eof(),
				_M_message(__debug::__dbg_msg_inc_istreambuf)
				._M_iterator(*this)); 
/* APPLE LOCAL end libstdc++ debug mode */
	const int_type __eof = traits_type::eof();
	istreambuf_iterator __old = *this;
	if (_M_sbuf
	    && traits_type::eq_int_type((__old._M_c = _M_sbuf->sbumpc()), 
					__eof))
	  _M_sbuf = 0;
	else
	  _M_c = __eof;
	return __old; 
      }

#ifdef _GLIBCPP_RESOLVE_LIB_DEFECTS
      // 110 istreambuf_iterator::equal not const
      // NB: there is also number 111 (NAD, Future) pending on this function.
      bool 
      equal(const istreambuf_iterator& __b) const
      {
	const int_type __eof = traits_type::eof();
	bool __thiseof = _M_at_eof();
	bool __beof = __b._M_at_eof();
	return (__thiseof && __beof || (!__thiseof && !__beof));
      }
#endif

    private:
      int_type 
      _M_get() const
      { 
	const int_type __eof = traits_type::eof();
	int_type __ret = __eof;
	if (_M_sbuf)
	  { 
	    if (!traits_type::eq_int_type(_M_c, __eof))
	      __ret = _M_c;
	    else 
	      if (traits_type::eq_int_type((__ret = _M_sbuf->sgetc()), __eof))
		_M_sbuf = 0;
	  }
	return __ret;
      }

      bool 
      _M_at_eof() const
      {
	const int_type __eof = traits_type::eof();
	return traits_type::eq_int_type(_M_get(), __eof);
      }
    };

  template<typename _CharT, typename _Traits>
    inline bool 
    operator==(const istreambuf_iterator<_CharT, _Traits>& __a,
	       const istreambuf_iterator<_CharT, _Traits>& __b)
    { return __a.equal(__b); }

  template<typename _CharT, typename _Traits>
    inline bool 
    operator!=(const istreambuf_iterator<_CharT, _Traits>& __a,
	       const istreambuf_iterator<_CharT, _Traits>& __b)
    { return !__a.equal(__b); }

  template<typename _CharT, typename _Traits>
    class ostreambuf_iterator
    : public iterator<output_iterator_tag, void, void, void, void>
    {
    public:
      // Types:
      typedef _CharT                           char_type;
      typedef _Traits                          traits_type;
      typedef basic_streambuf<_CharT, _Traits> streambuf_type;
      typedef basic_ostream<_CharT, _Traits>   ostream_type;

    private:
      streambuf_type* 	_M_sbuf;
      bool 		_M_failed;

    public:
      ostreambuf_iterator(ostream_type& __s) throw ()
      : _M_sbuf(__s.rdbuf()), _M_failed(!_M_sbuf) { }
      
      ostreambuf_iterator(streambuf_type* __s) throw ()
      : _M_sbuf(__s), _M_failed(!_M_sbuf) { }

      ostreambuf_iterator& 
      operator=(_CharT __c)
      {
	if (!_M_failed && 
	    _Traits::eq_int_type(_M_sbuf->sputc(__c), _Traits::eof()))
	  _M_failed = true;
	return *this;
      }

      ostreambuf_iterator& 
      operator*() throw()
      { return *this; }

      ostreambuf_iterator& 
      operator++(int) throw()
      { return *this; }

      ostreambuf_iterator& 
      operator++() throw()
      { return *this; }

      bool 
      failed() const throw()
      { return _M_failed; }

      ostreambuf_iterator& 
      _M_put(const _CharT* __ws, streamsize __len)
      {
	this->_M_sbuf->sputn(__ws, __len);
	return *this;
      }
    };
} // namespace std
#endif
