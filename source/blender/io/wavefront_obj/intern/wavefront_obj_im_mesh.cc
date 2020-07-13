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

#include "DNA_meshdata_types.h"

#include "wavefront_obj_im_mesh.hh"
#include "wavefront_obj_im_objects.hh"

namespace blender::io::obj {

OBJBmeshFromRaw::OBJBmeshFromRaw(const OBJRawObject &curr_object)
{
  auto creator_mesh = [&]() {
    return BKE_mesh_new_nomain(0,
                               0,
                               0,
                               static_cast<int>(curr_object.tot_loop),
                               static_cast<int>(curr_object.face_elements.size()));
  };
  auto creator_bmesh = [&]() {
    BMAllocTemplate bat{0,
                        0,
                        static_cast<int>(curr_object.tot_loop),
                        static_cast<int>(curr_object.face_elements.size())};
    BMeshCreateParams bcp{1};
    return BM_mesh_create(&bat, &bcp);
  };

  bm_new_.reset(creator_bmesh());
  unique_mesh_ptr template_mesh{creator_mesh()};
  BMeshFromMeshParams bm_convert_params{true, 0, 0, 0};
  BM_mesh_bm_from_me(bm_new_.get(), template_mesh.get(), &bm_convert_params);
};

BMVert *OBJBmeshFromRaw::add_bmvert(float3 coords)
{
  return BM_vert_create(bm_new_.get(), coords, nullptr, BM_CREATE_SKIP_CD);
}

void OBJBmeshFromRaw::add_polygon_from_verts(BMVert **verts_of_face, uint tot_verts_per_poly)
{
  BM_face_create_ngon_verts(
      bm_new_.get(), verts_of_face, tot_verts_per_poly, nullptr, BM_CREATE_SKIP_CD, false, true);
}

unique_mesh_ptr mesh_from_raw_obj(Main *bmain, const OBJRawObject &curr_object)
{
  OBJBmeshFromRaw bm_from_raw{curr_object};

  Array<BMVert *> all_vertices{curr_object.vertices.size()};
  for (int i = 0; i < curr_object.vertices.size(); i++) {
    all_vertices[i] = bm_from_raw.add_bmvert(curr_object.vertices[i].co);
  }

  for (const Vector<OBJFaceCorner> &curr_face : curr_object.face_elements) {
    /* Collect vertices of one face from a pool of BMesh vertices. */
    Array<BMVert *> verts_of_face{curr_face.size()};
    for (int i = 0; i < curr_face.size(); i++) {
      verts_of_face[i] = all_vertices[curr_face[i].vert_index];
    }
    bm_from_raw.add_polygon_from_verts(&verts_of_face[0], curr_face.size());
  }

  unique_mesh_ptr bm_to_me{(Mesh *)BKE_id_new_nomain(ID_ME, nullptr)};
  BM_mesh_bm_to_me_for_eval(bm_from_raw.bm_getter(), bm_to_me.get(), nullptr);
  BKE_mesh_validate(bm_to_me.get(), false, true);
  return bm_to_me;
}
}  // namespace blender::io::obj
