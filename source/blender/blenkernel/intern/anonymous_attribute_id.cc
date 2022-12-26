/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_anonymous_attribute_id.hh"

namespace blender::bke {

UniqueAnonymousAttributeID::UniqueAnonymousAttributeID()
{
  static std::atomic<int> counter = 0;
  const int count = counter.fetch_add(1);
  name_ = ".a_" + std::to_string(count);
}

}  // namespace blender::bke
