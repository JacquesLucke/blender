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

#include "BLI_math.h"
#include "BLI_vector.hh"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "wavefront_obj_exporter_nurbs.hh"

namespace blender {
namespace io {
namespace obj {

/**
 * Initialise nurbs curve object.
 */
void OBJNurbs::init_nurbs_curve(Object *export_object)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(_C);
  _export_object_eval = DEG_get_evaluated_object(depsgraph, export_object);
  _export_curve = (Curve *)_export_object_eval->data;
}

const char *OBJNurbs::get_curve_name()
{
  return _export_object_eval->id.name + 2;
}

/** Get coordinates of a vertex at given point index. */
void OBJNurbs::calc_point_coords(float r_coords[3], uint vert_index, Nurb *nurb)
{
  BPoint *bpoint = nurb->bp;
  bpoint += vert_index;
  copy_v3_v3(r_coords, bpoint->vec);
}

/** Get nurbs' degree and number of "curv" points of a nurb. */
void OBJNurbs::get_curve_info(int *r_nurbs_degree, int *r_curv_num, Nurb *nurb)
{
  *r_nurbs_degree = nurb->orderu - 1;
  /* "curv_num" are number of control points in a nurbs. If it is cyclic, degree also adds up. */
  *r_curv_num = nurb->pntsv * nurb->pntsu;
  if (nurb->flagu & CU_NURB_CYCLIC) {
    *r_curv_num += *r_nurbs_degree;
  }
}

}  // namespace obj
}  // namespace io
}  // namespace blender
