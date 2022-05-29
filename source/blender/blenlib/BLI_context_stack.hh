/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_array.hh"
#include "BLI_string_ref.hh"

namespace blender {

class ContextStack {
 private:
  const char *type_;
  ContextStack *parent_ = nullptr;
  uint64_t hash_;

 public:
  ContextStack(const char *type, ContextStack *parent) : type_(type), parent_(parent)
  {
  }

  uint64_t hash() const
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
};

}  // namespace blender
