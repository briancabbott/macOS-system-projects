// 1999-11-15 Kevin Ediger  <kediger@licor.com>
// test the floating point inserters (facet num_put)

// Copyright (C) 1999, 2002, 2003 Free Software Foundation, Inc.
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

#include <iomanip>
#include <sstream>
#include <testsuite_hooks.h>

using namespace std;

// libstdc++/3655
int
test04()
{
  stringbuf strbuf1, strbuf2;
  ostream o1(&strbuf1), o2(&strbuf2);
  bool test __attribute__((unused)) = true;

  o1 << hex << showbase << setw(6) << internal << 0xff;
  VERIFY( strbuf1.str() == "0x  ff" );
  
  // ... vs internal-adjusted const char*-type objects
  o2 << hex << showbase << setw(6) << internal << "0xff";
  VERIFY( strbuf2.str() == "  0xff" );

  return 0;
}

int 
main()
{
  test04();
  return 0;
}
