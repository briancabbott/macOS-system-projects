// APPLE LOCAL file libstdc++ debug mode
// Multimap iterator invalidation tests

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

#include <debug/map>
#include <iterator>
#include <testsuite_hooks.h>
#include <utility>

using __gnu_debug::multimap;
using std::advance;

bool test = true;

// Assignment
void test01()
{
  multimap<int, int> v1;
  multimap<int, int> v2;

  v1.insert(std::make_pair(17, 42));

  multimap<int, int>::iterator start = v1.begin();
  multimap<int, int>::iterator finish = v1.end();
  VERIFY(start._M_dereferenceable());
  VERIFY(!finish._M_dereferenceable() && !finish._M_singular());

  v1 = v2;
  VERIFY(start._M_singular());
  VERIFY(!finish._M_dereferenceable() && !finish._M_singular());
}

// Erase
void test02()
{
  multimap<int, int> v;
  for (int i = 0; i < 20; ++i)
    v.insert(std::make_pair(i, 20-i));

  // Single element erase (middle)
  multimap<int, int>::iterator before = v.begin();
  multimap<int, int>::iterator at = before;
  advance(at, 3);
  multimap<int, int>::iterator after = at;
  ++after;
  v.erase(at);
  VERIFY(before._M_dereferenceable());
  VERIFY(at._M_singular());
  VERIFY(after._M_dereferenceable());

  // Multiple element erase
  before = v.begin();
  at = before;
  advance(at, 3);
  after = at;
  advance(after, 4);
  v.erase(at, after);
  VERIFY(before._M_dereferenceable());
  VERIFY(at._M_singular());

  // clear()
  before = v.begin();
  multimap<int, int>::iterator finish = v.end();
  VERIFY(before._M_dereferenceable());
  v.clear();
  VERIFY(before._M_singular());
  VERIFY(!finish._M_singular() && !finish._M_dereferenceable());
}

int main()
{
  test01();
  test02();
  return !test;
}
