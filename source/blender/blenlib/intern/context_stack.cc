/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_context_stack.hh"
#include "BLI_hash_md5.h"

namespace blender {

void ContextStackHash::mix_in(const void *data, int64_t len)
{
  DynamicStackBuffer<> buffer_owner(HashSizeInBytes + len, 8);
  char *buffer = static_cast<char *>(buffer_owner.buffer());
  memcpy(buffer, this, HashSizeInBytes);
  memcpy(buffer + HashSizeInBytes, data, len);

  BLI_hash_md5_buffer(buffer, HashSizeInBytes + len, this);
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
