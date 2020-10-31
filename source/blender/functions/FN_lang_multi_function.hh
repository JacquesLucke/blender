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

#include "BLI_linear_allocator.hh"
#include "BLI_map.hh"
#include "BLI_multi_value_map.hh"
#include "BLI_resource_collector.hh"

#include "FN_generic_pointer.hh"
#include "FN_multi_function.hh"
#include "FN_multi_function_builder.hh"

namespace blender::fn::lang {

class MFSymbolTable {
 private:
  LinearAllocator<> allocator_;
  MultiValueMap<std::string, const MultiFunction *> function_table_;
  Map<std::pair<MFDataType, std::string>, const MultiFunction *> attribute_table_;
  Map<std::pair<MFDataType, std::string>, const MultiFunction *> method_table_;
  Map<std::pair<MFDataType, MFDataType>, const MultiFunction *> conversion_table_;
  Map<std::string, GMutablePointer> single_constants_table_;

 public:
  ~MFSymbolTable()
  {
    for (GMutablePointer ptr : single_constants_table_.values()) {
      ptr.destruct();
    }
  }

  void add_function(StringRef name, const MultiFunction &fn)
  {
    function_table_.add_as(name, &fn);
  }

  Span<const MultiFunction *> lookup_function_candidates(StringRef name) const
  {
    return function_table_.lookup_as(name);
  }

  void add_attribute(MFDataType type, StringRef name, const MultiFunction &fn)
  {
    attribute_table_.add_new({type, name}, &fn);
  }

  const MultiFunction *try_lookup_attribute(MFDataType type, StringRef name) const
  {
    return attribute_table_.lookup_default({type, name}, nullptr);
  }

  void add_method(MFDataType type, StringRef name, const MultiFunction &fn)
  {
    method_table_.add_new({type, name}, &fn);
  }

  const MultiFunction *try_lookup_method(MFDataType type, StringRef name) const
  {
    return method_table_.lookup_default({type, name}, nullptr);
  }

  void add_conversion(MFDataType from, MFDataType to, const MultiFunction &fn)
  {
    conversion_table_.add_new({from, to}, &fn);
  }

  template<typename FromT, typename ToT> void add_conversion(ResourceCollector &resources)
  {
    const MultiFunction &fn = resources.construct<CustomMF_Convert<FromT, ToT>>(__func__);
    this->add_conversion(MFDataType::ForSingle<FromT>(), MFDataType::ForSingle<ToT>(), fn);
  }

  const MultiFunction *try_lookup_conversion(MFDataType from, MFDataType to) const
  {
    return conversion_table_.lookup_default({from, to}, nullptr);
  }

  bool can_convert(MFDataType from, MFDataType to) const
  {
    return conversion_table_.contains({from, to});
  }

  void add_single_constant(StringRef name, const CPPType &type, const void *buffer)
  {
    void *own_buffer = allocator_.allocate(type.size(), type.alignment());
    type.copy_to_uninitialized(buffer, own_buffer);
    single_constants_table_.add_new(name, {&type, own_buffer});
  }

  template<typename T> void add_single_constant(StringRef name, const T &value)
  {
    this->add_single_constant(name, CPPType::get<T>(), (const void *)&value);
  }

  const GMutablePointer *try_lookup_single_constant(StringRef name) const
  {
    return single_constants_table_.lookup_ptr_as(name);
  }
};

struct MFDataTypeWithName {
  MFDataType data_type;
  StringRef name;
};

const MultiFunction &expression_to_multi_function(StringRef expression,
                                                  const MFSymbolTable &symbols,
                                                  ResourceCollector &resources,
                                                  MFDataType return_type,
                                                  Span<MFDataTypeWithName> parameters = {});

}  // namespace blender::fn::lang
