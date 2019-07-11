#pragma once

#include "BLI_small_map.hpp"
#include "BLI_string_ref.hpp"

namespace BLI {

template<typename V> class StringMap {
 private:
  SmallMap<std::string, V> m_map;

 public:
  StringMap() = default;

  void add_new(StringRef key, const V &value)
  {
    m_map.add_new(key.to_std_string(), value);
  }

  V &lookup(StringRef key) const
  {
    return m_map.lookup(key);
  }

  decltype(m_map.items()) items() const
  {
    return m_map.items();
  }
};

}  // namespace BLI
