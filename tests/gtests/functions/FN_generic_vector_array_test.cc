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
  GenericVectorArray vectors{CPPType_int32, 3};
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
  GenericVectorArray vectors{CPPType_string, 3};
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

}  // namespace fn
}  // namespace blender
