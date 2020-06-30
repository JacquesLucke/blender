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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup obj
 */

#ifndef __WAVEFRONT_OBJ_EXPORTER_NURBS_HH__
#define __WAVEFRONT_OBJ_EXPORTER_NURBS_HH__

#include "BKE_context.h"
#include "BKE_curve.h"

#include "BLI_utility_mixins.hh"

#include "DNA_curve_types.h"

namespace blender {
namespace io {
namespace obj {

class OBJNurbs : NonMovable, NonCopyable {
 public:
  OBJNurbs(bContext *C, Object *export_object) : _C(C), _export_object_eval(export_object)
  {
    init_nurbs_curve(export_object);
  }

  const char *get_curve_name();
  /** Getter for export curve. Used to obtain a curve's nurbs in OBJWriter class. */
  const Curve *export_curve()
  {
    return _export_curve;
  }

  /** Get coordinates of a vertex at given point index. */
  void calc_point_coords(float r_coords[3], uint point_index, Nurb *nurb);
  /** Get nurbs' degree and number of "curv" points of a nurb. */
  void get_curve_info(int *r_nurbs_degree, int *r_curv_num, Nurb *nurb);

 private:
  bContext *_C;
  Object *_export_object_eval;
  Curve *_export_curve;
  void init_nurbs_curve(Object *export_object);
};

}  // namespace obj
}  // namespace io
}  // namespace blender
#endif
