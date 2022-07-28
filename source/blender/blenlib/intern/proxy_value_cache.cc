/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_map.hh"
#include "BLI_proxy_value_cache.hh"
#include "BLI_set.hh"

#include <mutex>

namespace blender::proxy_value_cache {

using CacheMap = Map<ProxyValue, std::shared_ptr<CachedValue>>;

struct GlobalProxyValueCache {
  std::mutex mutex;
  uint64_t bytes_limit = 1024 * 1024;
  CacheMap cache_map;
  std::atomic<int> version_counter{0};
};

struct LocalProxyValueCache {
  int version = 0;
  Set<ProxyValue> cached_values;
};

GlobalProxyValueCache &get_global_cache()
{
  static GlobalProxyValueCache global_cache;
  return global_cache;
}

LocalProxyValueCache &get_local_cache()
{
  static thread_local LocalProxyValueCache local_cache;
  return local_cache;
}

void update_memory_limit(const uint64_t new_limit)
{
  GlobalProxyValueCache &global_cache = get_global_cache();
  std::lock_guard lock{global_cache.mutex};
  global_cache.bytes_limit = new_limit;
}

std::shared_ptr<CachedValue> lookup(const ProxyValue &proxy)
{
  GlobalProxyValueCache &global_cache = get_global_cache();
  LocalProxyValueCache &local_cache = get_local_cache();

  if (local_cache.version < global_cache.version_counter.load(std::memory_order_relaxed)) {
    local_cache.cached_values.clear();
    std::lock_guard lock{global_cache.mutex};
    for (const ProxyValue &proxy : global_cache.cache_map.keys()) {
      local_cache.cached_values.add(proxy);
    }
    local_cache.version = global_cache.version_counter;
  }
  if (!local_cache.cached_values.contains(proxy)) {
    return {};
  }
  std::lock_guard lock{global_cache.mutex};
  return global_cache.cache_map.lookup_default(proxy, {});
}

void force_cache(const ProxyValue &proxy, std::shared_ptr<CachedValue> value)
{
  GlobalProxyValueCache &global_cache = get_global_cache();
  std::lock_guard lock{global_cache.mutex};
  global_cache.cache_map.add_overwrite(proxy, std::move(value));
  global_cache.version_counter.fetch_add(1);
}

}  // namespace blender::proxy_value_cache
