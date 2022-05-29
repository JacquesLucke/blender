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

std::ostream &operator<<(std::ostream &stream, const ContextStackHash &hash)
{
  std::stringstream ss;
  ss << "0x" << std::hex << hash.v1 << hash.v2;
  stream << ss.str();
  return stream;
}

void ContextStack::print_stack(std::ostream &stream, StringRef name) const
{
  Stack<const ContextStack *> stack;
  for (const ContextStack *current = this; current; current = current->parent_) {
    stack.push(current);
  }
  stream << "Context Stack: " << name << "\n";
  while (!stack.is_empty()) {
    const ContextStack *current = stack.pop();
    stream << "-> ";
    current->print_current_in_line(stream);
    const ContextStackHash &current_hash = current->hash_;
    stream << " \t(hash: " << current_hash << ")\n";
  }
}

std::ostream &operator<<(std::ostream &stream, const ContextStack &context_stack)
{
  context_stack.print_stack(stream, "");
  return stream;
}

}  // namespace blender
