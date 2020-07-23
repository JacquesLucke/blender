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

#include "DNA_curve_types.h"

#include "wavefront_obj_im_nurbs.hh"
#include "wavefront_obj_im_objects.hh"

namespace blender::io::obj {
/**
 * Create a NURBS spline for the Curve converted from raw object.
 */
void OBJCurveFromRaw::create_nurbs(const OBJRawObject &curr_object,
                                   const GlobalVertices &global_vertices)
{
  const int64_t tot_vert{curr_object.nurbs_elem().curv_indices.size()};
  const OBJNurbsElem &raw_nurbs = curr_object.nurbs_elem();
  Nurb *nurb = (Nurb *)curve_from_raw_->nurb.first;

  nurb->type = CU_NURBS;
  nurb->next = nurb->prev = nullptr;
  /* BKE_nurb_points_add later on will update pntsu. If this were set to total curv points,
   * we get double the total points in viewport. */
  nurb->pntsu = 0;
  /* Total points = pntsu * pntsv. */
  nurb->pntsv = 1;
  nurb->orderu = nurb->orderv = raw_nurbs.degree + 1;
  nurb->resolu = 12;
  nurb->resolv = 12;

  BKE_nurb_points_add(nurb, tot_vert);
  for (int i = 0; i < tot_vert; i++) {
    BPoint &bpoint = nurb->bp[i];
    copy_v3_v3(bpoint.vec, global_vertices.vertices[raw_nurbs.curv_indices[i]]);
    bpoint.vec[3] = 1.0f;
    bpoint.weight = 1.0f;
  }
  BKE_nurb_knot_calc_u(nurb);

  bool do_endpoints = true;
  if (raw_nurbs.curv_indices.size() && raw_nurbs.parm.size() > raw_nurbs.degree + 1) {
    for (int i = 0; i < raw_nurbs.degree + 1; i++) {
      if (abs(raw_nurbs.parm[i] - raw_nurbs.curv_indices[0]) > 0.0001) {
        do_endpoints = false;
        break;
      }
      if (abs(raw_nurbs.parm[-(i + 1)] - raw_nurbs.curv_indices[1]) > 0.0001) {
        do_endpoints = false;
        break;
      }
    }
  }
  else {
    do_endpoints = false;
  }
  nurb->flag = CU_3D;
  if (do_endpoints) {
    nurb->flagu = CU_NURB_ENDPOINT;
  }
}

/**
 * Make a Blender NURBS Curve block from a raw object of OB_CURVE type.
 * Use the mover function to own the curve.
 */
OBJCurveFromRaw::OBJCurveFromRaw(Main *bmain,
                                 const OBJRawObject &curr_object,
                                 const GlobalVertices global_vertices)
{
  std::string ob_name = curr_object.object_name();
  if (ob_name.empty() && !curr_object.group().empty()) {
    ob_name = curr_object.group();
  }
  else {
    ob_name = "Untitled";
  }
  curve_from_raw_.reset(BKE_curve_add(bmain, curr_object.object_name().c_str(), OB_CURVE));
  curve_object_.reset(BKE_object_add_only_object(bmain, OB_CURVE, ob_name.c_str()));

  curve_from_raw_->flag = CU_3D;
  curve_from_raw_->resolu = curve_from_raw_->resolv = 12;
  /* Only one NURBS exists. */
  curve_from_raw_->actnu = 0;

  Nurb *nurb = (Nurb *)MEM_callocN(sizeof(Nurb), "OBJ import NURBS curve");
  BLI_addtail(BKE_curve_nurbs_get(curve_from_raw_.get()), nurb);
  create_nurbs(curr_object, global_vertices);

  curve_object_->data = curve_from_raw_.release();
}
}  // namespace blender::io::obj
