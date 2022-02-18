/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "DNA_object_types.h"

#include "BKE_curves_sculpt.hh"

namespace blender::bke {

CurvesSculptSession &curves_sculpt_session_ensure(Object &object)
{
  if (object.runtime.curves_sculpt_session == nullptr) {
    object.runtime.curves_sculpt_session = MEM_new<CurvesSculptSession>(__func__);
  }
  return *object.runtime.curves_sculpt_session;
}

}  // namespace blender::bke
