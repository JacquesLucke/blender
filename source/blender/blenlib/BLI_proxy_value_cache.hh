/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include <memory>

namespace blender::proxy_value_cache {

class ProxyValue {
 private:
  uint64_t key1_;
  uint64_t key2_;

 public:
  uint64_t hash() const
  {
    return key1_;
  }
};

class CachedValue {
 private:
  virtual uint64_t estimate_memory_usage_in_bytes() const = 0;
};

void update_memory_limit(uint64_t bytes);
std::shared_ptr<CachedValue> lookup(const ProxyValue &proxy);
void force_cache(const ProxyValue &proxy, std::shared_ptr<CachedValue> value);

}  // namespace blender::proxy_value_cache
