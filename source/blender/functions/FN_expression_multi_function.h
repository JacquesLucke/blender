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

class FunctionTable {
 private:
  StringMultiMap<const MultiFunction *> m_function_table;
  Map<std::pair<MFDataType, std::string>, const MultiFunction *> m_attribute_table;
  Map<std::pair<MFDataType, std::string>, const MultiFunction *> m_method_table;

 public:
  void add_function(StringRef name, const MultiFunction &fn)
  {
    m_function_table.add(name, &fn);
  }

  ArrayRef<const MultiFunction *> lookup_function(StringRef name) const
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
};

class ConversionTable {
 private:
  Map<std::pair<MFDataType, MFDataType>, const MultiFunction *> m_table;

 public:
  void add(MFDataType from, MFDataType to, const MultiFunction &fn)
  {
    m_table.add_new({from, to}, &fn);
  }

  template<typename FromT, typename ToT> void add(ResourceCollector &resources)
  {
    const MultiFunction &fn = resources.construct<MF_Convert<FromT, ToT>>("conversion fn");
    this->add(MFDataType::ForSingle<FromT>(), MFDataType::ForSingle<ToT>(), fn);
  }

  const MultiFunction *try_lookup(MFDataType from, MFDataType to) const
  {
    return m_table.lookup_default({from, to}, nullptr);
  }

  bool can_convert(MFDataType from, MFDataType to) const
  {
    return m_table.contains({from, to});
  }
};

struct SingleConstant {
  const CPPType *type;
  void *buffer;
};

class ConstantsTable {
 private:
  LinearAllocator<> m_allocator;
  StringMap<SingleConstant> m_table;

 public:
  ConstantsTable() = default;
  ~ConstantsTable()
  {
    m_table.foreach_value(
        [&](SingleConstant &constant) { constant.type->destruct(constant.buffer); });
  }

  void add_single(StringRef name, const CPPType &type, const void *buffer)
  {
    void *own_buffer = m_allocator.allocate(type.size(), type.alignment());
    type.copy_to_uninitialized(buffer, own_buffer);
    m_table.add_new(name, {&type, own_buffer});
  }

  template<typename T> void add_single(StringRef name, const T &value)
  {
    this->add_single(name, CPP_TYPE<T>(), (const void *)&value);
  }

  Optional<SingleConstant> try_lookup(StringRef name) const
  {
    return m_table.try_lookup(name);
  }
};

const MultiFunction &expression_to_multi_function(StringRef str,
                                                  ResourceCollector &resources,
                                                  ArrayRef<StringRef> variable_names,
                                                  ArrayRef<MFDataType> variable_types,
                                                  const ConstantsTable &constants_table,
                                                  const FunctionTable &function_table,
                                                  const ConversionTable &conversion_table);

}  // namespace Expr
}  // namespace FN
