/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_anonymous_attribute_id.hh"

namespace blender::bke {

bool AnonymousAttributePropagationInfo::propagate(const AnonymousAttributeID &anonymous_id) const
{
  return this->names && this->names->contains_as(anonymous_id.name());
}

}  // namespace blender::bke
