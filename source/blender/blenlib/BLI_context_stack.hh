/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_array.hh"
#include "BLI_stack.hh"
#include "BLI_string_ref.hh"

namespace blender {

struct ContextStackHash {
  uint64_t v1 = 0;
  uint64_t v2 = 0;

  void mix_in(Span<std::byte> data);
  void mix_in(StringRef a, StringRef b);
};

class ContextStack {
 private:
  const char *type_;
  const ContextStack *parent_ = nullptr;

 protected:
  ContextStackHash hash_;

 public:
  ContextStack(const char *type, const ContextStack *parent) : type_(type), parent_(parent)
  {
    if (parent != nullptr) {
      hash_ = parent_->hash_;
    }
  }

  const ContextStackHash &hash() const
  {
    return hash_;
  }

  const char *type() const
  {
    return type_;
  }

  const ContextStack *parent() const
  {
    return parent_;
  }

  virtual void print_current_in_line(std::ostream &stream) const = 0;

  void print_stack(std::ostream &stream, StringRef name) const
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
      stream << " (hash: " << std::hex << current_hash.v1 << ", " << current_hash.v2 << ")\n";
    }
  }

  friend std::ostream &operator<<(std::ostream &stream, const ContextStack &context_stack)
  {
    context_stack.print_stack(stream, "");
    return stream;
  }
};

}  // namespace blender
