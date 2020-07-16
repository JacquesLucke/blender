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
OBJMeshFromRaw::OBJMeshFromRaw(class OBJRawObject &curr_object)
{
  uint tot_verts_object = curr_object.vertices.size();
  uint tot_face_elems = curr_object.face_elements.size();
  mesh_from_bm_.reset(
      BKE_mesh_new_nomain(tot_verts_object, 0, 0, curr_object.tot_loop, tot_face_elems));

  for (int i = 0; i < tot_verts_object; ++i) {
    copy_v3_v3(mesh_from_bm_->mvert[i].co, curr_object.vertices[i].co);
  }

  int curr_loop_idx = 0;
  for (int i = 0; i < tot_face_elems; ++i) {
    const OBJFaceElem &curr_face = curr_object.face_elements[i];
    MPoly &mpoly = mesh_from_bm_->mpoly[i];
    mpoly.totloop = curr_face.face_corners.size();
    mpoly.loopstart = curr_loop_idx;
    if (curr_face.shaded_smooth) {
      mpoly.flag |= ME_SMOOTH;
    }

    for (int j = 0; j < mpoly.totloop; ++j) {
      MLoop *mloop = &mesh_from_bm_->mloop[curr_loop_idx];
      mloop->v = curr_face.face_corners[j].vert_index;
      curr_loop_idx++;
    }
  }

  int uv_vert_index = 0;
  if (curr_object.tot_uv_verts > 0) {
    ED_mesh_uv_texture_ensure(mesh_from_bm_.get(), nullptr);
  }
  for (int i = 0; i < tot_face_elems; ++i) {
    const OBJFaceElem curr_face = curr_object.face_elements[i];
    for (int j = 0; j < curr_face.face_corners.size(); ++j) {
      const OBJFaceCorner curr_corner = curr_face.face_corners[j];
      MLoopUV *mluv_dst = (MLoopUV *)CustomData_get_layer(&mesh_from_bm_->ldata, CD_MLOOPUV);
      if (!mluv_dst) {
        fprintf(stderr, "No UV layer found.\n");
        break;
      }
      if (curr_corner.tex_vert_index < 0 ||
          curr_corner.tex_vert_index >= curr_object.tot_uv_verts) {
        continue;
      }
      MLoopUV *mluv_src = &curr_object.texture_vertices[curr_corner.tex_vert_index];
      int &set_uv = mluv_src->flag;
      if (set_uv == false && uv_vert_index <= curr_object.tot_uv_verts) {
        copy_v2_v2(mluv_dst[uv_vert_index].uv, mluv_src->uv);
        uv_vert_index++;
        set_uv = true;
      }
    }
  }
  BKE_mesh_update_customdata_pointers(mesh_from_bm_.get(), false);
  BKE_mesh_calc_edges(mesh_from_bm_.get(), false, false);
  BKE_mesh_validate(mesh_from_bm_.get(), false, true);
}
}  // namespace blender::io::obj
