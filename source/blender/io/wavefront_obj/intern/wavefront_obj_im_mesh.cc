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

#include "BKE_customdata.h"

#include "BLI_array.hh"

#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "ED_mesh.h"

#include "wavefront_obj_im_mesh.hh"
#include "wavefront_obj_im_objects.hh"

namespace blender::io::obj {
OBJMeshFromRaw::OBJMeshFromRaw(const OBJRawObject &curr_object)
{
  uint tot_verts_object{curr_object.vertices.size()};
  uint tot_face_elems{curr_object.face_elements.size()};
  mesh_from_ob_.reset(
      BKE_mesh_new_nomain(tot_verts_object, 0, 0, curr_object.tot_loop, tot_face_elems));

  for (int i = 0; i < tot_verts_object; ++i) {
    copy_v3_v3(mesh_from_ob_->mvert[i].co, curr_object.vertices[i].co);
  }

  int tot_loop_idx = 0;
  for (int poly_idx = 0; poly_idx < tot_face_elems; ++poly_idx) {
    const OBJFaceElem &curr_face = curr_object.face_elements[poly_idx];
    MPoly &mpoly = mesh_from_ob_->mpoly[poly_idx];
    mpoly.totloop = curr_face.face_corners.size();
    mpoly.loopstart = tot_loop_idx;
    if (curr_face.shaded_smooth) {
      mpoly.flag |= ME_SMOOTH;
    }

    for (int loop_of_poly_idx = 0; loop_of_poly_idx < mpoly.totloop; ++loop_of_poly_idx) {
      MLoop *mloop = &mesh_from_ob_->mloop[tot_loop_idx];
      tot_loop_idx++;
      mloop->v = curr_face.face_corners[loop_of_poly_idx].vert_index;
    }
  }

  BKE_mesh_calc_edges(mesh_from_ob_.get(), false, false);

  /* TODO ankitm merge the face iteration loops. Kept separate for ease of debugging. */
  if (curr_object.tot_uv_verts > 0 && curr_object.texture_vertices.size() > 0) {
    MLoopUV *mluv_dst = (MLoopUV *)CustomData_add_layer(&mesh_from_ob_->ldata,
                                                        CD_MLOOPUV,
                                                        CD_DUPLICATE,
                                                        mesh_from_ob_->mloopuv,
                                                        curr_object.tot_loop);
    int tot_loop_idx = 0;
    for (const OBJFaceElem &curr_face : curr_object.face_elements) {
      for (const OBJFaceCorner &curr_corner : curr_face.face_corners) {
        if (curr_corner.tex_vert_index < 0 ||
            curr_corner.tex_vert_index >= curr_object.tot_uv_verts) {
          continue;
        }
        const MLoopUV *mluv_src = &curr_object.texture_vertices[curr_corner.tex_vert_index];
        copy_v2_v2(mluv_dst[tot_loop_idx].uv, mluv_src->uv);
        tot_loop_idx++;
      }
    }
  }

  BKE_mesh_validate(mesh_from_ob_.get(), false, true);
}
}  // namespace blender::io::obj
