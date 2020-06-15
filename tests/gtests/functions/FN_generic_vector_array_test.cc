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
 */

#include "FN_cpp_types.hh"
#include "FN_generic_vector_array.hh"

#include "testing/testing.h"

namespace blender {
namespace fn {

TEST(generic_vector_array, Constructor)
{
  GVectorArray vectors{CPPType_int32, 3};
  EXPECT_EQ(vectors.size(), 3);
  EXPECT_EQ(vectors.lengths().size(), 3);
  EXPECT_EQ(vectors.starts().size(), 3);
  EXPECT_EQ(vectors.lengths()[0], 0);
  EXPECT_EQ(vectors.lengths()[1], 0);
  EXPECT_EQ(vectors.lengths()[2], 0);
  EXPECT_EQ(vectors.type(), CPPType_int32);
}

TEST(generic_vector_array, Append)
{
  GVectorArray vectors{CPPType_string, 3};
  std::string value = "hello";
  vectors.append(0, &value);
  value = "world";
  vectors.append(0, &value);
  vectors.append(2, &value);

  EXPECT_EQ(vectors.lengths()[0], 2);
  EXPECT_EQ(vectors.lengths()[1], 0);
  EXPECT_EQ(vectors.lengths()[2], 1);
  EXPECT_EQ(vectors[0].size(), 2);
  EXPECT_EQ(vectors[0].typed<std::string>()[0], "hello");
  EXPECT_EQ(vectors[0].typed<std::string>()[1], "world");
  EXPECT_EQ(vectors[2].typed<std::string>()[0], "world");
}

TEST(generic_vector_array, AsArraySpan)
{
  GVectorArray vectors{CPPType_int32, 3};
  int value = 3;
  vectors.append(0, &value);
  vectors.append(0, &value);
  value = 5;
  vectors.append(2, &value);
  vectors.append(2, &value);
  vectors.append(2, &value);

  GVArraySpan span = vectors;
  EXPECT_EQ(span.type(), CPPType_int32);
  EXPECT_EQ(span.size(), 3);
  EXPECT_EQ(span[0].size(), 2);
  EXPECT_EQ(span[1].size(), 0);
  EXPECT_EQ(span[2].size(), 3);
  EXPECT_EQ(span[0].typed<int>()[1], 3);
  EXPECT_EQ(span[2].typed<int>()[0], 5);
}

TEST(generic_vector_array, Typed)
{
  GVectorArray vectors{CPPType_int32, 4};
  GVectorArray::Typed<int> typed = vectors.typed<int>();
  typed.append(0, 2);
  typed.append(0, 6);
  typed.append(0, 7);
  typed.append(2, 1);
  typed.append(2, 1);
  typed.append(3, 5);
  typed.append(3, 6);

  EXPECT_EQ(typed[0].size(), 3);
  EXPECT_EQ(vectors[0].size(), 3);
  EXPECT_EQ(typed[0][0], 2);
  EXPECT_EQ(typed[0][1], 6);
  EXPECT_EQ(typed[0][2], 7);
  EXPECT_EQ(typed[1].size(), 0);
  EXPECT_EQ(typed[2][0], 1);
  EXPECT_EQ(typed[2][1], 1);
  EXPECT_EQ(typed[3][0], 5);
  EXPECT_EQ(typed[3][1], 6);
}

}  // namespace fn
}  // namespace blender
