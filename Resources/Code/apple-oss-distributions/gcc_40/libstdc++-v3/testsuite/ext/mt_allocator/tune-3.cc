// 2004-08-25 Benjamin Kosnik <bkoz@redhat.com>
//
// Copyright (C) 2004 Free Software Foundation, Inc.
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

#include <testsuite_hooks.h>
#include <memory>
#include <ext/mt_allocator.h>

// Tune characteristics, two of same type
template<typename _Tp>
struct test_policy
{ static bool per_type() { return true; } };

template<bool _Thread>
struct test_policy<__gnu_cxx::__common_pool_policy<_Thread> >
{ 
  typedef __gnu_cxx::__common_pool_policy<_Thread> policy_type;
  static bool per_type() { return false; } 
};

// Tune characteristics, two of different types
template<typename _Tp, typename _Cp>
void test03()
{
  bool test __attribute__((unused)) = true;

  typedef __gnu_cxx::__pool_base::_Tune tune_type;
  typedef _Tp value_type;
  typedef _Cp policy_type;
  typedef __gnu_cxx::__mt_alloc<value_type, policy_type> allocator_type;

  tune_type t_opt(16, 5120, 32, 5120, 20, 10, false);
  tune_type t_single(16, 5120, 32, 5120, 1, 10, false);

  // First instances assured.
  allocator_type a;
  tune_type t_default = a._M_get_options();
  tune_type t1 = t_default;
  tune_type t2;
  if (test_policy<policy_type>::per_type())
    {
      VERIFY( t1._M_align == t_default._M_align );
      a._M_set_options(t_opt);
      t2 = a._M_get_options();
      VERIFY( t1._M_align != t2._M_align );
    }
  else
    t2 = t1;

  // Lock tune settings.
  typename allocator_type::pointer p1 = a.allocate(128);

  allocator_type a2;
  tune_type t3 = a2._M_get_options();  
  tune_type t4;
  VERIFY( t3._M_max_threads == t2._M_max_threads );

  typename allocator_type::pointer p2 = a2.allocate(5128);

  a2._M_set_options(t_single);
  t4 = a2._M_get_options();
  VERIFY( t4._M_max_threads != t_single._M_max_threads );
  VERIFY( t4._M_max_threads == t3._M_max_threads );

  a.deallocate(p1, 128);
  a2.deallocate(p2, 5128);
}

int main()
{
#ifdef __GTHREADS
  test03<int, __gnu_cxx::__per_type_pool_policy<int, true> >();
  test03<int, __gnu_cxx::__common_pool_policy<true> >();
#endif

  test03<int, __gnu_cxx::__common_pool_policy<false> >();
  test03<int, __gnu_cxx::__per_type_pool_policy<int, false> >();

  return 0;
}
