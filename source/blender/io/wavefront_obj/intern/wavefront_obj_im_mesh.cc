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

#include "BLI_array.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "wavefront_obj_im_mesh.hh"
#include "wavefront_obj_im_objects.hh"

namespace blender::io::obj {
OBJMeshFromRaw::OBJMeshFromRaw(const class OBJRawObject &curr_object)
{
  uint tot_verts_object = curr_object.vertices.size();
  mesh_from_bm_.reset(BKE_mesh_new_nomain(
      tot_verts_object, 0, 0, curr_object.tot_loop, curr_object.face_elements.size()));
  for (int i = 0; i < tot_verts_object; ++i) {
    copy_v3_v3(mesh_from_bm_->mvert[i].co, curr_object.vertices[i].co);
  }

  int curr_loop_idx = 0;
  for (int i = 0; i < curr_object.face_elements.size(); ++i) {
    const OBJFaceElem &curr_face = curr_object.face_elements[i];
    MPoly &mpoly = mesh_from_bm_->mpoly[i];
    mpoly.totloop = curr_face.face_corners.size();
    mpoly.loopstart = curr_loop_idx;

    for (int j = 0; j < mpoly.totloop; ++j) {
      MLoop *mloop = &mesh_from_bm_->mloop[curr_loop_idx];
      mloop->v = curr_face.face_corners[j].vert_index;
      curr_loop_idx++;
    }
  }
  BKE_mesh_calc_edges(mesh_from_bm_.get(), false, false);
  BKE_mesh_validate(mesh_from_bm_.get(), false, true);
}
}  // namespace blender::io::obj
