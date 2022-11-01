// 2003-04-30  Petur Runolfsson <peturr02@ru.is>

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

#include <testsuite_hooks.h>
#include <cstdio>
#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

// Check that wcin.rdbuf()->sputbackc() puts characters back to stdin.
// If wcin.rdbuf() is a filebuf, this succeeds when stdin is a regular
// file, but fails otherwise, hence the named fifo.
void test01()
{
  using namespace std;
  using namespace __gnu_test;

  bool test __attribute__((unused)) = true;

  const char* name = "tmp_fifo5";

  signal(SIGPIPE, SIG_IGN);

  unlink(name);  
  try_mkfifo(name, S_IRWXU);
  
  int child = fork();
  VERIFY( child != -1 );

  if (child == 0)
    {
      sleep(1);
      FILE* file = fopen(name, "w");
      fputs("Whatever\n", file);
      fflush(file);
      sleep(2);
      fclose(file);
      exit(0);
    }
  
  freopen(name, "r", stdin);
  sleep(2);

  wint_t c1 = fgetwc(stdin);
  VERIFY( c1 != WEOF );
  wint_t c2 = wcin.rdbuf()->sputbackc(L'a');
  VERIFY( c2 != WEOF );
  VERIFY( c2 == L'a' );
  
  wint_t c3 = fgetwc(stdin);
  VERIFY( c3 != WEOF );
  VERIFY( c3 == c2 );
  wint_t c4 = ungetwc(L'b', stdin);
  VERIFY( c4 != WEOF );
  VERIFY( c4 == L'b' );
  
  wint_t c5 = wcin.rdbuf()->sgetc();
  VERIFY( c5 != WEOF );
  VERIFY( c5 == c4 );
}

int main()
{
  test01();
  return 0;
}
