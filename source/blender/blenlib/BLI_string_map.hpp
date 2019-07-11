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

  void add_override(StringRef key, const V &value)
  {
    m_map.add_override(key.to_std_string(), value);
  }

  V lookup(StringRef key) const
  {
    return m_map.lookup(key.to_std_string());
  }

  V *lookup_ptr(StringRef key) const
  {
    return m_map.lookup_ptr(key.to_std_string());
  }

  V lookup_default(StringRef key, const V &default_value) const
  {
    return m_map.lookup_default(key.to_std_string(), default_value);
  }

  V &lookup_ref(StringRef key) const
  {
    return m_map.lookup_ref(key.to_std_string());
  }

  bool contains(StringRef key) const
  {
    return m_map.contains(key.to_std_string());
  }

  decltype(m_map.items()) items() const
  {
    return m_map.items();
  }

  decltype(m_map.keys()) keys() const
  {
    return m_map.keys();
  }

  decltype(m_map.values()) values() const
  {
    return m_map.values();
  }
};

}  // namespace BLI
