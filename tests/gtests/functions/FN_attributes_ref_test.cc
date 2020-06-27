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

#include "FN_attributes_ref.hh"

#include "testing/testing.h"

namespace blender {
namespace fn {

TEST(attributes_info, BuildEmpty)
{
  AttributesInfoBuilder info_builder;
  AttributesInfo info{info_builder};

  EXPECT_EQ(info.size(), 0);
}

TEST(attributes_info, AddSameNameTwice)
{
  AttributesInfoBuilder info_builder;
  info_builder.add<int>("A", 4);
  info_builder.add<int>("A", 5);
  AttributesInfo info{info_builder};
  EXPECT_EQ(info.size(), 1);
  EXPECT_TRUE(info.has_attribute("A", CPPType::get<int>()));
  EXPECT_FALSE(info.has_attribute("B", CPPType::get<int>()));
  EXPECT_FALSE(info.has_attribute("A", CPPType::get<float>()));
  EXPECT_EQ(info.default_of<int>("A"), 4);
  EXPECT_EQ(info.name_of(0), "A");
  EXPECT_EQ(info.index_range().start(), 0);
  EXPECT_EQ(info.index_range().one_after_last(), 1);
}

TEST(attributes_info, BuildWithDefaultString)
{
  AttributesInfoBuilder info_builder;
  info_builder.add("A", CPPType::get<std::string>());
  AttributesInfo info{info_builder};
  EXPECT_EQ(info.default_of<std::string>("A"), "");
}

TEST(attributes_info, BuildWithGivenDefault)
{
  AttributesInfoBuilder info_builder;
  info_builder.add<std::string>("A", "hello world");
  AttributesInfo info{info_builder};
  const void *default_value = info.default_of("A");
  EXPECT_EQ(*(const std::string *)default_value, "hello world");
  EXPECT_EQ(info.type_of("A"), CPPType::get<std::string>());
}

}  // namespace fn
}  // namespace blender
