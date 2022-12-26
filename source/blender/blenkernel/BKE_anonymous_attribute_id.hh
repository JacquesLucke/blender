/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <atomic>

#include "BLI_set.hh"
#include "BLI_string_ref.hh"
#include "BLI_user_counter.hh"

namespace blender::bke {

class AnonymousAttributeID {
 protected:
  std::string name_;

 private:
  mutable std::atomic<int> users_ = 1;

 public:
  virtual ~AnonymousAttributeID() = default;

  StringRefNull name() const
  {
    return name_;
  }

  void user_add() const
  {
    users_.fetch_add(1);
  }

  void user_remove() const
  {
    const int new_users = users_.fetch_sub(1) - 1;
    if (new_users == 0) {
      delete this;
    }
  }
};

using AutoAnonymousAttributeID = UserCounter<const AnonymousAttributeID>;

class UniqueAnonymousAttributeID : public AnonymousAttributeID {
 public:
  UniqueAnonymousAttributeID();
};

class AnonymousAttributeSet {
 public:
  Set<std::string> names;
};

class AnonymousAttributePropagationInfo {
 public:
  Set<std::string> names;

  bool propagate(const AnonymousAttributeID & /*anonymous_id*/) const
  {
    return true;
    // return this->names.contains_as(anonymous_id.name());
  }
};

}  // namespace blender::bke
