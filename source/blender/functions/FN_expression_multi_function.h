#pragma once

#include "FN_multi_function.h"
#include "FN_multi_functions.h"

#include "BLI_resource_collector.h"
#include "BLI_map.h"
#include "BLI_string_map.h"
#include "BLI_string_multi_map.h"

namespace FN {
namespace Expr {

using BLI::Map;
using BLI::ResourceCollector;
using BLI::StringMap;
using BLI::StringMultiMap;

struct SingleConstant {
  const CPPType *type;
  void *buffer;
};

class SymbolTable {
 private:
  LinearAllocator<> m_allocator;
  StringMultiMap<const MultiFunction *> m_function_table;
  Map<std::pair<MFDataType, std::string>, const MultiFunction *> m_attribute_table;
  Map<std::pair<MFDataType, std::string>, const MultiFunction *> m_method_table;
  Map<std::pair<MFDataType, MFDataType>, const MultiFunction *> m_conversion_table;
  StringMap<SingleConstant> m_single_constants_table;

 public:
  ~SymbolTable()
  {
    m_single_constants_table.foreach_value(
        [&](SingleConstant &constant) { constant.type->destruct(constant.buffer); });
  }

  void add_function(StringRef name, const MultiFunction &fn)
  {
    m_function_table.add(name, &fn);
  }

  ArrayRef<const MultiFunction *> lookup_function_candidates(StringRef name) const
  {
    return m_function_table.lookup_default(name);
  }

  void add_attribute(MFDataType type, StringRef name, const MultiFunction &fn)
  {
    m_attribute_table.add_new({type, name}, &fn);
  }

  const MultiFunction *try_lookup_attribute(MFDataType type, StringRef name) const
  {
    return m_attribute_table.lookup_default({type, name}, nullptr);
  }

  void add_method(MFDataType type, StringRef name, const MultiFunction &fn)
  {
    m_method_table.add_new({type, name}, &fn);
  }

  const MultiFunction *try_lookup_method(MFDataType type, StringRef name) const
  {
    return m_method_table.lookup_default({type, name}, nullptr);
  }

  void add_conversion(MFDataType from, MFDataType to, const MultiFunction &fn)
  {
    m_conversion_table.add_new({from, to}, &fn);
  }

  template<typename FromT, typename ToT> void add_conversion(ResourceCollector &resources)
  {
    const MultiFunction &fn = resources.construct<MF_Convert<FromT, ToT>>("conversion fn");
    this->add_conversion(MFDataType::ForSingle<FromT>(), MFDataType::ForSingle<ToT>(), fn);
  }

  const MultiFunction *try_lookup_conversion(MFDataType from, MFDataType to) const
  {
    return m_conversion_table.lookup_default({from, to}, nullptr);
  }

  bool can_convert(MFDataType from, MFDataType to) const
  {
    return m_conversion_table.contains({from, to});
  }

  void add_single_constant(StringRef name, const CPPType &type, const void *buffer)
  {
    void *own_buffer = m_allocator.allocate(type.size(), type.alignment());
    type.copy_to_uninitialized(buffer, own_buffer);
    m_single_constants_table.add_new(name, {&type, own_buffer});
  }

  template<typename T> void add_single_constant(StringRef name, const T &value)
  {
    this->add_single_constant(name, CPP_TYPE<T>(), (const void *)&value);
  }

  Optional<SingleConstant> try_lookup_single_constant(StringRef name) const
  {
    return m_single_constants_table.try_lookup(name);
  }
};

const MultiFunction &expression_to_multi_function(StringRef str,
                                                  MFDataType output_type,
                                                  ResourceCollector &resources,
                                                  ArrayRef<StringRef> variable_names,
                                                  ArrayRef<MFDataType> variable_types,
                                                  const SymbolTable &symbols);

}  // namespace Expr
}  // namespace FN
