#ifndef __BLI_STRING_MULTI_MAP_H__
#define __BLI_STRING_MULTI_MAP_H__

#include "BLI_string_ref.h"
#include "BLI_string_map.h"
#include "BLI_vector.h"

namespace BLI {

template<typename ValueT> class StringMultiMap {
 private:
  StringMap<Vector<ValueT>> m_map;

 public:
  StringMultiMap() = default;
  ~StringMultiMap() = default;

  uint key_amount() const
  {
    return m_map.size();
  }

  uint value_amount(StringRef key) const
  {
    return m_map.lookup(key).size();
  }

  bool add(StringRef key, const ValueT &value)
  {
    if (m_map.contains(key)) {
      m_map.lookup(key).append(value);
      return false;
    }
    else {
      m_map.add_new(key, Vector<ValueT>({value}));
      return true;
    }
  }

  void add_multiple(StringRef key, ArrayRef<ValueT> values)
  {
    if (m_map.contains(key)) {
      m_map.lookup(key).extend(values);
    }
    else {
      m_map.add_new(key, values);
    }
  }

  void add_multiple(const StringMultiMap<ValueT> &other)
  {
    other.foreach_item(
        [&](StringRefNull key, ArrayRef<ValueT> values) { this->add_multiple(key, values); });
  }

  ArrayRef<ValueT> lookup(StringRef key) const
  {
    return m_map.lookup(key);
  }

  ArrayRef<ValueT> lookup_default(StringRef key,
                                  ArrayRef<ValueT> default_array = ArrayRef<ValueT>()) const
  {
    const Vector<ValueT> *values = m_map.lookup_ptr(key);
    if (values == nullptr) {
      return default_array;
    }
    else {
      return *values;
    }
  }

  template<typename FuncT> void foreach_item(const FuncT &func) const
  {
    m_map.foreach_item([&](StringRefNull key, ArrayRef<ValueT> vector) { func(key, vector); });
  }
};

}  // namespace BLI

#endif /* __BLI_STRING_MULTI_MAP_H__ */
