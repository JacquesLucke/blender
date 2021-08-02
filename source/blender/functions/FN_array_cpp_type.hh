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

#pragma once

#include "BLI_array.hh"

#include "FN_cpp_type.hh"

namespace blender::fn {

template<typename T> struct ArrayCPPTypeParam {
};

class ArrayCPPType : public CPPType {
 private:
  const CPPType &element_type_;

 public:
  template<typename ElementT, int64_t InlineBufferCapacity, typename Allocator>
  ArrayCPPType(ArrayCPPTypeParam<Array<ElementT, InlineBufferCapacity, Allocator>> /* unused */,
               StringRef debug_name)
      : CPPType(
            CPPTypeParam<Array<ElementT, InlineBufferCapacity, Allocator>, CPPTypeFlags::None>(),
            debug_name),
        element_type_(CPPType::get<ElementT>())
  {
  }
};

}  // namespace blender::fn

#define MAKE_ARRAY_CPP_TYPE(IDENTIFIER, TYPE_NAME) \
  template<> const blender::fn::CPPType &blender::fn::CPPType::get_impl<TYPE_NAME>() \
  { \
    static blender::fn::ArrayCPPType cpp_type{blender::fn::ArrayCPPTypeParam<TYPE_NAME>(), \
                                              STRINGIFY(IDENTIFIER)}; \
    return cpp_type; \
  }
