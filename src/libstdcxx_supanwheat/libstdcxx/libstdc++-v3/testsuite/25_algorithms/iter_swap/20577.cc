// Copyright (C) 2005 Free Software Foundation, Inc.
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

#include <algorithm>
#include <vector>
#include <testsuite_hooks.h>

void
test1()
{
  bool test __attribute__((unused)) = true;

  std::vector<bool> v;
  v.push_back(true);
  v.push_back(false);
  std::iter_swap(v.begin(), v.begin() + 1);
  VERIFY( v[0] == false && v[1] == true );
}

void
test2()
{
  bool test __attribute__((unused)) = true;

  std::vector<int> v;
  v.push_back(1);
  v.push_back(2);
  std::iter_swap(v.begin(), v.begin() + 1);
  VERIFY( v[0] == 2 && v[1] == 1 );
}

int int_swap_count;

struct X {};
void swap(X& i, X& j)
{ ++int_swap_count; }

void
test3()
{
  bool test __attribute__((unused)) = true;

  int_swap_count = 0;
  X i, j;
  std::iter_swap(&i, &j);
  VERIFY( int_swap_count == 1 );
}

// libstdc++/20577
int main()
{
  test1();
  test2();
  test3();
  return 0;
}
