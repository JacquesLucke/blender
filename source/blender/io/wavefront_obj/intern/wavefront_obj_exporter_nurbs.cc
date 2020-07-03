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

#include "BLI_math.h"
#include "BLI_vector.hh"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "wavefront_obj_exporter_nurbs.hh"

namespace blender::io::obj {
/**
 * Store NURBS curves that will be exported in parameter form, not converted to meshes.
 */
OBJNurbs::OBJNurbs(Depsgraph *depsgraph, Object *export_object)
    : depsgraph_(depsgraph), export_object_eval_(export_object)
{
  export_object_eval_ = DEG_get_evaluated_object(depsgraph_, export_object);
  export_curve_ = (Curve *)export_object_eval_->data;
}

const char *OBJNurbs::get_curve_name()
{
  return export_object_eval_->id.name + 2;
}

const ListBase *OBJNurbs::curve_nurbs()
{
  return &export_curve_->nurb;
}

/**
 * Get coordinates of a vertex at given point index.
 */
void OBJNurbs::calc_point_coords(float r_coords[3], int vert_index, const Nurb *nurb)
{
  BPoint *bpoint = nurb->bp;
  bpoint += vert_index;
  copy_v3_v3(r_coords, bpoint->vec);
}

/**
 * Get nurbs' degree and number of "curv" points of a nurb.
 */
void OBJNurbs::get_curve_info(int &r_nurbs_degree, int &r_curv_num, const Nurb *nurb)
{
  r_nurbs_degree = nurb->orderu - 1;
  /* "curv_num" is the number of control points in a nurbs.
   * If it is cyclic, degree also adds up. */
  r_curv_num = nurb->pntsv * nurb->pntsu;
  if (nurb->flagu & CU_NURB_CYCLIC) {
    r_curv_num += r_nurbs_degree;
  }
}

}  // namespace blender::io::obj
