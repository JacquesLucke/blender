/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup obj
 */

#ifndef __WAVEFRONT_OBJ_IM_NURBS_HH__
#define __WAVEFRONT_OBJ_IM_NURBS_HH__

#include <memory>

#include "BKE_curve.h"

#include "BLI_utility_mixins.hh"

namespace blender::io::obj {
/* Avoid cyclic dependency build errors. */
class OBJRawObject;
struct GlobalVertices;

/** Free a curve's memory using Blender's memory management. */
struct UniqueCurveDeleter {
  void operator()(Curve *curve)
  {
    if (curve) {
      BKE_nurbList_free(&curve->nurb);
    }
  }
};

/** An unique_ptr to a Curve with a custom deleter. Don't let unique_ptr free a curve with a
 * different deallocator.
 */
using unique_curve_ptr = std::unique_ptr<Curve, UniqueCurveDeleter>;

class OBJCurveFromRaw : NonMovable, NonCopyable {
 private:
  unique_curve_ptr curve_from_ob_;

 public:
  OBJCurveFromRaw(Main *bmain,
                  const OBJRawObject &curr_object,
                  const GlobalVertices global_vertices);

  unique_curve_ptr mover()
  {
    return std::move(curve_from_ob_);
  }

 private:
  void create_nurbs(const OBJRawObject &curr_object, const GlobalVertices &global_vertices);
};
}  // namespace blender::io::obj
#endif
