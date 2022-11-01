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

// 27.8.1.4 Overridden virtual functions

#include <fstream>
#include <locale>

#include <sys/types.h>
#include <sys/stat.h>

#include <testsuite_hooks.h>

// libstdc++/13171
void test01()
{
  bool test __attribute__((unused)) = true;
  using namespace std;
  using namespace __gnu_test;

  locale loc_fr(__gnu_test::try_named_locale("fr_FR"));
  locale loc_en(__gnu_test::try_named_locale("en_US"));

  const char* name = "tmp_fifo_13171-2";
  unlink(name);
  try_mkfifo(name, S_IRWXU);
  
  int child = fork();
  if (child == 0)
    {
      filebuf fb;
      fb.open(name, ios_base::out);
      fb.sputc('S');
      fb.pubsync();
      sleep(2);
      fb.close();
      exit(0);
    }

  filebuf fb;
  fb.pubimbue(loc_fr);
  fb.open(name, ios_base::in);
  sleep(1);
  VERIFY( fb.is_open() );
  fb.pubimbue(loc_en);
  filebuf::int_type c = fb.sgetc();
  fb.close();
  VERIFY( c == 'S' );
}

int main()
{
  test01();
  return 0;
}
