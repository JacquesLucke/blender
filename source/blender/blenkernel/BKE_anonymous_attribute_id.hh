/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <atomic>

#include "BLI_set.hh"
#include "BLI_string_ref.hh"
#include "BLI_user_counter.hh"

namespace blender::bke {

/**
 * An #AnonymousAttributeID contains information about a specific anonymous attribute.
 * Like normal attributes, anonymous attributes are also identified by their name. So one should
 * not have to compare #AnonymousAttributeID pointers.
 *
 * For the most part, anonymous attributes don't need additional information besides their name
 * with few exceptions:
 * - The name of anonymous attributes is generated automatically, so it is generally not human
 *   readable (just random characters). #AnonymousAttributeID can provide more context as where a
 *   specific anonymous attribute was created which can simplify debugging.
 * - [Not yet supported.] When anonymous attributes are contained in on-disk caches, we have to map
 *   those back to anonymous attributes at run-time. The issue is that (for various reasons) we
 *   might change how anonymous attribute names are generated in the future, which would lead to a
 *   mis-match between stored and new attribute names. To work around it, we should cache
 *   additional information for anonymous attributes on disk (like which node created it). This
 *   information can then be used to map stored attributes to their run-time counterpart.
 *
 * Once created, #AnonymousAttributeID is immutable. Also it is intrinsicly reference counted.
 * If possible, the #AutoAnonymousAttributeID wrapper should be used to avoid manual reference
 * counting.
 */
class AnonymousAttributeID {
 private:
  mutable std::atomic<int> users_ = 1;

 protected:
  std::string name_;

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

/** Wrapper for #AnonymousAttributeID that avoids manual reference counting. */
using AutoAnonymousAttributeID = UserCounter<const AnonymousAttributeID>;

/**
 * A set of anonymous attribute names that is passed around in geometry nodes.
 */
class AnonymousAttributeSet {
 public:
  std::shared_ptr<Set<std::string>> names;
};

/**
 * Can be passed to algorithms which propagate attributes. It can tell the algorithm which
 * anonymous attributes should be propagated and which should not.
 */
class AnonymousAttributePropagationInfo {
 public:
  std::shared_ptr<Set<std::string>> names;

  /**
   * Return true when the anonymous attribute should be propagated and false otherwise.
   */
  bool propagate(const AnonymousAttributeID &anonymous_id) const;
};

}  // namespace blender::bke
