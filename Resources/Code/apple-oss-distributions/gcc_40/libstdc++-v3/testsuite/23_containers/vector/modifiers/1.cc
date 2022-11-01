// 1999-11-09 bkoz

// Copyright (C) 1999, 2001, 2004 Free Software Foundation, Inc.
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

// 23.2.4.3 vector modifiers

#include <vector>
#include "testsuite_hooks.h"

bool test __attribute__((unused)) = true;

template<typename T>
  struct A { };

struct B { };

// vector::insert(iterator, inputiterator first, inputiterator last)
void
test01()
{
  // POD types
  typedef std::vector<int> 	vec_POD;
  vec_POD	vec01;
  int 		i01 = 5;
  int*		pi01 = &i01;
  vec01.insert(vec01.begin(), pi01, pi01 + 1);

  // non POD types
  typedef std::vector< A<B> >	vec_nonPOD;
  vec_nonPOD	vec02;
  A<B>		np01;
  A<B>*		pnp01 = &np01;
  vec02.insert(vec02.begin(), pnp01, pnp01 + 1);
}

#if !__GXX_WEAK__ && _MT_ALLOCATOR_H
// Explicitly instantiate for systems with no COMDAT or weak support.
template class __gnu_cxx::__mt_alloc<int>;
template class __gnu_cxx::__mt_alloc<A<B> >;
#endif

int main()
{
  test01();
  return 0;
}
