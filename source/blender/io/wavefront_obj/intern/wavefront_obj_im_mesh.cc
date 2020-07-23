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

#include "wavefront_obj_im_mesh.hh"
#include "wavefront_obj_im_objects.hh"

namespace blender::io::obj {
/**
 * Make a Blender Mesh block from a raw object of OB_MESH type.
 * Use the mover function to own the mesh.
 */
OBJMeshFromRaw::OBJMeshFromRaw(const OBJRawObject &curr_object,
                               const GlobalVertices global_vertices)
{
  const int64_t tot_verts_object{curr_object.tot_verts()};
  const int64_t tot_edges{curr_object.tot_edges()};
  const int64_t tot_face_elems{curr_object.tot_face_elems()};
  mesh_from_ob_.reset(BKE_mesh_new_nomain(
      tot_verts_object, tot_edges, 0, curr_object.tot_loops(), tot_face_elems));

  create_vertices(curr_object, global_vertices, tot_verts_object);
  create_loops(curr_object, tot_face_elems);
  create_edges(curr_object, tot_edges);
  create_uv_verts(curr_object, global_vertices);

  BKE_mesh_validate(mesh_from_ob_.get(), false, true);
}

void OBJMeshFromRaw::create_vertices(const OBJRawObject &curr_object,
                                     const GlobalVertices &global_vertices,
                                     int64_t tot_verts_object)
{
  for (int i = 0; i < tot_verts_object; ++i) {
    /* Current object's vertex indices index into global list of vertex coordinates. */
    copy_v3_v3(mesh_from_ob_->mvert[i].co,
               global_vertices.vertices[curr_object.vertex_indices()[i]]);
  }
}

void OBJMeshFromRaw::create_loops(const OBJRawObject &curr_object, int64_t tot_face_elems)
{
  int tot_loop_idx = 0;
  for (int poly_idx = 0; poly_idx < tot_face_elems; ++poly_idx) {
    const OBJFaceElem &curr_face = curr_object.face_elements()[poly_idx];
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
}

void OBJMeshFromRaw::create_edges(const OBJRawObject &curr_object, int64_t tot_edges)
{
  for (int i = 0; i < tot_edges; ++i) {
    const auto &curr_edge = curr_object.edges()[i];
    mesh_from_ob_->medge[i].v1 = curr_edge.v1;
    mesh_from_ob_->medge[i].v2 = curr_edge.v2;
  }

  /* Set argument `update` to true so that existing explicitly imported edges can be merged
   * with the new ones created from polygons. */
  BKE_mesh_calc_edges(mesh_from_ob_.get(), true, false);
  BKE_mesh_calc_edges_loose(mesh_from_ob_.get());
}

void OBJMeshFromRaw::create_uv_verts(const OBJRawObject &curr_object,
                                     const GlobalVertices &global_vertices)
{
  if (curr_object.tot_uv_verts() > 0 && curr_object.tot_uv_vert_indices() > 0) {
    MLoopUV *mluv_dst = (MLoopUV *)CustomData_add_layer(&mesh_from_ob_->ldata,
                                                        CD_MLOOPUV,
                                                        CD_DUPLICATE,
                                                        mesh_from_ob_->mloopuv,
                                                        curr_object.tot_loops());
    int tot_loop_idx = 0;
    for (const OBJFaceElem &curr_face : curr_object.face_elements()) {
      for (const OBJFaceCorner &curr_corner : curr_face.face_corners) {
        if (curr_corner.uv_vert_index < 0 ||
            curr_corner.uv_vert_index >= curr_object.tot_uv_verts()) {
          continue;
        }
        /* Current corner's UV vertex index indices into current object's UV vertex indices, which
         * index into global list of UV vertex coordinates. */
        const float2 &mluv_src =
            global_vertices
                .uv_vertices[curr_object.uv_vertex_indices()[curr_corner.uv_vert_index]];
        copy_v2_v2(mluv_dst[tot_loop_idx].uv, mluv_src);
        tot_loop_idx++;
      }
    }
  }
}
}  // namespace blender::io::obj
