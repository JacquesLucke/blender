/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_context_stack.hh"

#define XXH_INLINE_ALL 1
#include "xxhash.h"

namespace blender {

void ContextStackHash::mix_in(const Span<std::byte> data)
{
  XXH3_state_t state;
  XXH3_128bits_reset(&state);
  XXH3_128bits_update(&state, this, sizeof(ContextStackHash));
  XXH3_128bits_update(&state, data.data(), data.size());
  XXH128_hash_t new_hash = XXH3_128bits_digest(&state);
  static_assert(sizeof(XXH128_hash_t) == sizeof(ContextStackHash));
  memcpy(this, &new_hash, sizeof(ContextStackHash));
}

void ContextStackHash::mix_in(const StringRef a, const StringRef b)
{
  XXH3_state_t state;
  XXH3_128bits_reset(&state);
  XXH3_128bits_update(&state, this, sizeof(ContextStackHash));
  XXH3_128bits_update(&state, a.data(), a.size());
  XXH3_128bits_update(&state, b.data(), b.size());
  XXH128_hash_t new_hash = XXH3_128bits_digest(&state);
  static_assert(sizeof(XXH128_hash_t) == sizeof(ContextStackHash));
  memcpy(this, &new_hash, sizeof(ContextStackHash));
}

}  // namespace blender
