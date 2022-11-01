// 2001-09-17 Benjamin Kosnik  <bkoz@redhat.com>

// Copyright (C) 2001, 2002, 2003, 2004 Free Software Foundation
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

// 22.2.5.3.1 time_put members

#include <locale>
#include <sstream>
#include <testsuite_hooks.h>

void test07()
{
  using namespace std;
  typedef ostreambuf_iterator<char> iterator_type;
  typedef char_traits<char> traits;

  bool test __attribute__((unused)) = true;

  // create "C" time objects
  tm time1 = { 0, 0, 12, 4, 3, 71, 0, 93, 0 };
  const char* date = "%A, the second of %B";
  const char* date_ex = "%Ex";

  // basic construction and sanity check
  locale loc_c = locale::classic();
  locale loc_hk = __gnu_test::try_named_locale("en_HK");
  VERIFY( loc_hk != loc_c );

  // create an ostream-derived object, cache the time_put facet
  const string empty;
  ostringstream oss;
  oss.imbue(loc_hk);
  const time_put<char>& tim_put = use_facet<time_put<char> >(oss.getloc()); 

  iterator_type os_it09 = tim_put.put(oss.rdbuf(), oss, '*', &time1, 
				      date, date + traits::length(date));
  string result9 = oss.str();
  VERIFY( result9 == "Sunday, the second of April");
  iterator_type os_it10 = tim_put.put(oss.rdbuf(), oss, '*', &time1, 
				      date_ex, date_ex + traits::length(date));
  string result10 = oss.str();
  VERIFY( result10 != result9 );
}

int main()
{
  test07();
  return 0;
}
