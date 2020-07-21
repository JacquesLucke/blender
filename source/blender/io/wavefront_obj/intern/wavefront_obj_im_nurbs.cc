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
 * Edit the NURBS curve of the curve converted from raw object.
 */
void OBJCurveFromRaw::edit_nurbs(const OBJRawObject &curr_object,
                                 const GlobalVertices &global_vertices)
{
  const uint tot_vert = curr_object.nurbs_element.curv_indices.size();
  const NurbsElem &raw_nurbs = curr_object.nurbs_element;
  Nurb *nurb = (Nurb *)curve_from_ob_->nurb.first;

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
    copy_v3_v3(nurb->bp[i].vec, global_vertices.vertices[raw_nurbs.curv_indices[i]]);
    nurb->bp->vec[3] = 1.0f;
    nurb->bp->weight = 1.0f;
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
    nurb->flag = CU_NURB_ENDPOINT;
  }
}

OBJCurveFromRaw::OBJCurveFromRaw(Main *bmain,
                                 const OBJRawObject &curr_object,
                                 const GlobalVertices global_vertices)
{
  /* Set curve specific parameters. */
  curve_from_ob_.reset(BKE_curve_add(bmain, curr_object.object_name.c_str(), OB_CURVE));
  curve_from_ob_->flag = CU_3D;
  curve_from_ob_->resolu = curve_from_ob_->resolv = 12;

  Nurb *nurb = (Nurb *)MEM_callocN(sizeof(Nurb), "OBJ import NURBS curve");
  BLI_addtail(BKE_curve_nurbs_get(curve_from_ob_.get()), nurb);

  /* Set NURBS specific parameters. */
  edit_nurbs(curr_object, global_vertices);
}
}  // namespace blender::io::obj
