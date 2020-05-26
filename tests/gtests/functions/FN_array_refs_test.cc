/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "testing/testing.h"

#include "FN_array_refs.hh"
#include "FN_cpp_types.hh"

using namespace FN;

TEST(generic_array_ref, EmptyConstructor)
{
  GenericArrayRef ref(CPPType_float);
  EXPECT_EQ(ref.size(), 0);
  EXPECT_EQ(ref.type(), CPPType_float);
}

TEST(generic_array_ref, BufferConstructor)
{
  std::array<int, 3> values = {1, 2, 3};
  GenericArrayRef ref(CPPType_int32, (const void *)&values, values.size());
  EXPECT_EQ(ref.size(), 3);
  EXPECT_EQ(ref.type(), CPPType_int32);
  EXPECT_EQ(ref[0], (void *)&values[0]);
  EXPECT_EQ(ref[1], (void *)&values[1]);
  EXPECT_EQ(ref[2], (void *)&values[2]);
  EXPECT_EQ(ref.buffer(), (void *)&values);
}

TEST(generic_array_ref, ArrayRefConstructor)
{
  std::array<int, 3> values = {4, 5, 6};
  GenericArrayRef ref{ArrayRef<int>(values)};
  EXPECT_EQ(ref.size(), 3);
  EXPECT_EQ(ref.type(), CPPType_int32);
  EXPECT_EQ(ref[0], (void *)&values[0]);
  EXPECT_EQ(ref[1], (void *)&values[1]);
  EXPECT_EQ(ref[2], (void *)&values[2]);
  EXPECT_EQ(ref.buffer(), (void *)&values);
  EXPECT_EQ(ref.typed<int>()[1], 5);
}

TEST(generic_mutable_array_ref, EmptyConstructor)
{
  GenericMutableArrayRef ref(CPPType_float);
  EXPECT_EQ(ref.size(), 0);
  EXPECT_EQ(ref.type(), CPPType_float);
}

TEST(generic_mutable_array_ref, Modify)
{
  int array[10] = {0};
  GenericMutableArrayRef ref{MutableArrayRef<int>(array)};
  EXPECT_EQ(ref.size(), 10);
  EXPECT_EQ(array[3], 0);
  *(int *)ref[3] = 13;
  EXPECT_EQ(array[3], 13);
}

TEST(virtual_array_ref, FromSingle)
{
  int value = 12;
  VirtualArrayRef<int> ref = VirtualArrayRef<int>::FromSingle(&value, 10);
  EXPECT_EQ(ref.size(), 10);
  EXPECT_EQ(ref[0], 12);
  EXPECT_EQ(ref[3], 12);
  value = 3;
  EXPECT_EQ(ref[0], 3);
  EXPECT_EQ(ref[3], 3);
}

TEST(virtual_array_ref, FromFullArray)
{
  int array[5] = {4, 5, 6, 7, 8};
  VirtualArrayRef<int> ref{ArrayRef<int>(array)};
  EXPECT_EQ(ref.size(), 5);
  EXPECT_EQ(ref[0], 4);
  EXPECT_EQ(ref[3], 7);
  array[3] = 12;
  EXPECT_EQ(ref[3], 12);
}

TEST(virtual_array_ref, FromFullPointerArray)
{
  int x1 = 2;
  int x2 = 6;
  int x3 = 8;
  std::array<int *, 3> array = {&x1, &x2, &x3};
  VirtualArrayRef<int> ref{ArrayRef<int *>(array)};
  EXPECT_EQ(ref.size(), 3);
  EXPECT_EQ(ref[0], 2);
  EXPECT_EQ(ref[1], 6);
  EXPECT_EQ(ref[2], 8);
  x2 = 12;
  EXPECT_EQ(ref[1], 12);
}

TEST(generic_virtual_array_ref, FromSingle)
{
  int value = 12;
  GenericVirtualArrayRef ref = GenericVirtualArrayRef::FromSingle(
      CPPType_int32, (const void *)&value, 10);
  EXPECT_EQ(ref.size(), 10);
  EXPECT_EQ(ref[0], (void *)&value);
  EXPECT_EQ(ref[3], (void *)&value);
  EXPECT_EQ(*(int *)ref[3], 12);
  value = 3;
  EXPECT_EQ(*(int *)ref[0], 3);
  EXPECT_EQ(*(int *)ref[3], 3);
}

TEST(generic_virtual_array_ref, FromFullArray)
{
  int array[5] = {4, 5, 6, 7, 8};
  GenericVirtualArrayRef ref{ArrayRef<int>(array)};
  EXPECT_EQ(ref.type(), CPPType_int32);
  EXPECT_EQ(ref.size(), 5);
  EXPECT_EQ(ref[0], (void *)&array[0]);
  EXPECT_EQ(ref[3], (void *)&array[3]);
  EXPECT_EQ(*(int *)ref[3], 7);
  array[3] = 12;
  EXPECT_EQ(*(int *)ref[3], 12);
}

TEST(generic_virtual_array_ref, FromFullPointerArray)
{
  int x1 = 2;
  int x2 = 6;
  int x3 = 8;
  std::array<int *, 3> array = {&x1, &x2, &x3};
  GenericVirtualArrayRef ref = GenericVirtualArrayRef::FromFullPointerArray(
      CPPType_int32, (void **)&array, array.size());
  EXPECT_EQ(ref.size(), 3);
  EXPECT_EQ(ref[0], (void *)&x1);
  EXPECT_EQ(ref[1], (void *)&x2);
  EXPECT_EQ(ref[2], (void *)&x3);
  EXPECT_EQ(*(int *)ref[1], 6);
  x2 = 12;
  EXPECT_EQ(*(int *)ref[1], 12);
}
