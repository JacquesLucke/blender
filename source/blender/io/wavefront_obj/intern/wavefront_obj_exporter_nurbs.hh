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

#ifndef __WAVEFRONT_OBJ_EXPORTER_NURBS_HH__
#define __WAVEFRONT_OBJ_EXPORTER_NURBS_HH__

#include "BKE_context.h"
#include "BKE_curve.h"

#include "BLI_utility_mixins.hh"

#include "DNA_curve_types.h"

namespace blender::io::obj {
class OBJNurbs : NonMovable, NonCopyable {
private:
  Depsgraph *depsgraph_;
  Object *export_object_eval_;
  Curve *export_curve_;

 public:
  OBJNurbs(Depsgraph *depsgraph, Object *export_object);
  
  const char *get_curve_name();
  const ListBase *curve_nurbs();
  void calc_point_coords(float r_coords[3], int point_index, const Nurb *nurb);
  void get_curve_info(int &r_nurbs_degree, int &r_curv_num, const Nurb *nurb);
};

}  // namespace blender::io::obj
#endif
