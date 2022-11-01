// 2003-07-09  Benjamin Kosnik  <bkoz@redhat.com>

// Copyright (C) 2003 Free Software Foundation, Inc.
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

#include <locale>
#include <sstream>
#include <ostream>
#include <stdexcept>
#include <ext/pod_char_traits.h>
#include <testsuite_hooks.h>

// Check for numpunct and ctype dependencies. Make sure that numpunct
// can be created without ctype.
void test01()
{
  using namespace std;
  using __gnu_test::pod_type;

  typedef numpunct<pod_type>::string_type 	string_type;
  typedef basic_ostringstream<pod_type> 		ostream_type;
  
  bool 		test = true;

  // Test formatted output.
  ostream_type 		os;
  const locale 	loc = locale::classic();
  os.imbue(loc);
  os.setf(ios_base::boolalpha);
  os.exceptions(ios_base::badbit);

  // 1: fail, no num_put.
  try
    {
      // Calls to num_put.put will fail, as there's no num_put facet.
      os << true;
      test = false;
    }
  catch(const bad_cast& obj)
    { }
  catch(...)
    { test = false; }
  VERIFY( test );

  // 2: fail, no ctype
  const locale 	loc2(loc, new num_put<pod_type>);
  os.clear();
  os.imbue(loc2);
  try
    {
      // Calls to ctype.widen will fail, as there's no ctype facet.
      os << true;
      test = false;
    }
  catch(const bad_cast& obj)
    { }
  catch(...)
    { test = false; }
  VERIFY( test );

  // 3: fail, no numpunct
  const locale 	loc3(loc2, new ctype<pod_type>);
  os.clear();
  os.imbue(loc3);
  try
    {
      // Formatted output fails as no numpunct.
      os << true;
      test = false;
    }
  catch(const bad_cast& obj)
    { }
  catch(...)
    { test = false; }
  VERIFY( test );

  // 4: works.
  const locale 	loc4(loc3, new numpunct<pod_type>);
  os.clear();
  os.imbue(loc4);
  try
    {
      os << long(500);
      string_type s = os.str();
      VERIFY( s.length() == 3 );

      VERIFY( os.narrow(s[0], char()) == '5' );
      VERIFY( os.narrow(s[1], char()) == '0' );
      VERIFY( os.narrow(s[2], char()) == '0' );
    }
  catch(const bad_cast& obj)
    { test = false; }
  catch(...)
    { test = false; }
  VERIFY( test );
}

int main()
{
  test01();
  return 0;
}
