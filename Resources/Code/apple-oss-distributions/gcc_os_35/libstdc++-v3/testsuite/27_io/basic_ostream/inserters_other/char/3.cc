// 1999-08-16 bkoz
// 1999-11-01 bkoz

// Copyright (C) 1999, 2000, 2001, 2002, 2003 Free Software Foundation
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

// 27.6.2.5.4 basic_ostream character inserters

#include <ostream>
#include <sstream>
#include <testsuite_hooks.h>

// libstdc++/3272
void test04()
{
  using namespace std;
  bool test __attribute__((unused)) = true;
  istringstream istr("inside betty carter");
  ostringstream ostr;
  ostr << istr.rdbuf() << endl;

  if (ostr.rdstate() & ios_base::eofbit) 
    test = false;

  VERIFY( test );
}

int 
main()
{
  test04(); 
  return 0;
}
