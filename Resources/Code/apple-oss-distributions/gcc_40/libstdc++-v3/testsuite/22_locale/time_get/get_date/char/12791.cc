// 2003-12-03  Paolo Carlini  <pcarlini@suse.de>

// Copyright (C) 2003 Free Software Foundation
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

// 22.2.5.1.1 time_get members

#include <locale>
#include <sstream>
#include <testsuite_hooks.h>

// libstdc++/12791
void test01()
{
  using namespace std;
  bool test __attribute__((unused)) = true;

  typedef istreambuf_iterator<char> iterator_type;

  // create an ostream-derived object, cache the time_get facet
  iterator_type end;

  istringstream iss;
  const time_get<char>& tim_get = use_facet<time_get<char> >(iss.getloc()); 

  const ios_base::iostate good = ios_base::goodbit;
  ios_base::iostate errorstate = good;

  iss.str("60/04/71");
  iterator_type is_it01(iss);
  tm time01;
  errorstate = good;
  tim_get.get_date(is_it01, end, iss, errorstate, &time01);
  VERIFY( errorstate == ios_base::failbit );
  VERIFY( *is_it01 == '6' );

  iss.str("04/38/71");
  iterator_type is_it02(iss);
  tm time02;
  errorstate = good;
  tim_get.get_date(is_it02, end, iss, errorstate, &time02);
  VERIFY( errorstate == ios_base::failbit );
  VERIFY( *is_it02 == '8' );
}

int main()
{
  test01();
  return 0;
}
