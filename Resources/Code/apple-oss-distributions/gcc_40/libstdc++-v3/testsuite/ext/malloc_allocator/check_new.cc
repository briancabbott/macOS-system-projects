// 2001-11-25  Phil Edwards  <pme@gcc.gnu.org>
//
// Copyright (C) 2001, 2003, 2004 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this library; see the file COPYING.  If not, write to the Free
// Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
// USA.

// 20.4.1.1 allocator members

#include <cstdlib>
#include <ext/malloc_allocator.h>
#include <testsuite_allocator.h>

using __gnu_cxx::malloc_allocator;

void* 
operator new(std::size_t n) throw(std::bad_alloc)
{
  new_called = true;
  requested = n;
  return std::malloc(n);
}

void
operator delete(void *v) throw()
{
  delete_called = true;
  return std::free(v);
}

// These just help tracking down error messages.
bool test01() 
{ 
  typedef malloc_allocator<unsigned int> allocator_type;
  return (__gnu_test::check_new<allocator_type, false>() == false); 
}

int main()
{
  return test01();
}

