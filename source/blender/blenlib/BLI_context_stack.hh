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

  uint64_t hash() const
  {
    return v1;
  }

  friend bool operator==(const ContextStackHash &a, const ContextStackHash &b)
  {
    return a.v1 == b.v1 && a.v2 == b.v2;
  }

  void mix_in(Span<std::byte> data);
  void mix_in(StringRef a, StringRef b);

  friend std::ostream &operator<<(std::ostream &stream, const ContextStackHash &hash);
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

  void print_stack(std::ostream &stream, StringRef name) const;
  virtual void print_current_in_line(std::ostream &stream) const = 0;

  friend std::ostream &operator<<(std::ostream &stream, const ContextStack &context_stack);
};

}  // namespace blender
