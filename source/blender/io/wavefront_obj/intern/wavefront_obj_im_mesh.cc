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

#include "DNA_scene_types.h" /* For eVGroupSelect. */

#include "BKE_customdata.h"
#include "BKE_material.h"
#include "BKE_object_deform.h"

#include "BLI_map.hh"
#include "BLI_vector_set.hh"

#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "wavefront_obj_im_mesh.hh"
#include "wavefront_obj_im_mtl.hh"
#include "wavefront_obj_im_objects.hh"

namespace blender::io::obj {
/**
 * Make a Blender Mesh Object from a raw object of OB_MESH type.
 * Use the mover function to own the mesh.
 */
OBJMeshFromRaw::OBJMeshFromRaw(Main *bmain,
                               const OBJRawObject &curr_object,
                               const GlobalVertices &global_vertices,
                               const Map<std::string, MTLMaterial> &materials)
    : curr_object_(&curr_object), global_vertices_(&global_vertices)
{
  std::string ob_name = curr_object_->object_name();
  if (ob_name.empty()) {
    ob_name = "Untitled";
  }
  const int64_t tot_verts_object{curr_object_->tot_verts()};
  const int64_t tot_edges{curr_object_->tot_edges()};
  const int64_t tot_face_elems{curr_object_->tot_face_elems()};
  const int64_t tot_loops{curr_object_->tot_loops()};

  mesh_from_raw_.reset(
      BKE_mesh_new_nomain(tot_verts_object, tot_edges, 0, tot_loops, tot_face_elems));
  mesh_object_.reset(BKE_object_add_only_object(bmain, OB_MESH, ob_name.c_str()));
  mesh_object_->data = BKE_object_obdata_add_from_type(bmain, OB_MESH, ob_name.c_str());

  create_vertices();
  create_polys_loops();
  create_edges();
  create_uv_verts();
  create_materials(bmain, materials);

  BKE_mesh_validate(mesh_from_raw_.get(), false, true);

  BKE_mesh_nomain_to_mesh(mesh_from_raw_.release(),
                          static_cast<Mesh *>(mesh_object_->data),
                          mesh_object_.get(),
                          &CD_MASK_EVERYTHING,
                          true);
}

void OBJMeshFromRaw::create_vertices()
{
  const int64_t tot_verts_object{curr_object_->tot_verts()};
  for (int i = 0; i < tot_verts_object; ++i) {
    /* Current object's vertex indices index into the global list of vertex coordinates. */
    copy_v3_v3(mesh_from_raw_->mvert[i].co,
               global_vertices_->vertices[curr_object_->vertex_indices()[i]]);
  }
}

void OBJMeshFromRaw::create_polys_loops()
{
  /* May not be used conditionally. */
  mesh_from_raw_->dvert = nullptr;
  float weight = 0.0f;
  if (curr_object_->tot_verts() && curr_object_->use_vertex_groups()) {
    mesh_from_raw_->dvert = static_cast<MDeformVert *>(CustomData_add_layer(
        &mesh_from_raw_->vdata, CD_MDEFORMVERT, CD_CALLOC, nullptr, curr_object_->tot_verts()));
    weight = 1.0f / curr_object_->tot_verts();
  }
  else {
    UNUSED_VARS(weight);
  }
  /* Do not remove elements from the VectorSet since order of insertion is required.
   * StringRef is fine since per-face deform group name outlives the VectorSet. */
  VectorSet<StringRef> group_names;
  const int64_t tot_face_elems{curr_object_->tot_face_elems()};
  int tot_loop_idx = 0;
  for (int poly_idx = 0; poly_idx < tot_face_elems; ++poly_idx) {
    const OBJFaceElem &curr_face = curr_object_->face_elements()[poly_idx];
    MPoly &mpoly = mesh_from_raw_->mpoly[poly_idx];
    mpoly.totloop = curr_face.face_corners.size();
    mpoly.loopstart = tot_loop_idx;
    if (curr_face.shaded_smooth) {
      mpoly.flag |= ME_SMOOTH;
    }

    for (const OBJFaceCorner &curr_corner : curr_face.face_corners) {
      MLoop *mloop = &mesh_from_raw_->mloop[tot_loop_idx];
      tot_loop_idx++;
      mloop->v = curr_corner.vert_index;
      if (mesh_from_raw_->dvert) {
        /* Iterating over mloop results in finding the same vertex multiple times.
         * Another way is to allocate memory for dvert while creating vertices and fill them here.
         */
        MDeformVert &def_vert = mesh_from_raw_->dvert[mloop->v];
        if (!def_vert.dw) {
          def_vert.dw = static_cast<MDeformWeight *>(
              MEM_callocN(sizeof(MDeformWeight), "OBJ Import Deform Weight"));
        }
        /* Every vertex in a face is assigned the same deform group. */
        int64_t pos_name{group_names.index_of_try(curr_face.vertex_group)};
        if (pos_name == -1) {
          group_names.add_new(curr_face.vertex_group);
          pos_name = group_names.size() - 1;
        }
        BLI_assert(pos_name >= 0);
        /* Deform group number (def_nr) must behave like an index into the names' list. */
        *(def_vert.dw) = {static_cast<unsigned int>(pos_name), weight};
      }
    }
  }

  if (!mesh_from_raw_->dvert) {
    return;
  }
  /* Add deform group(s) to the object's defbase. */
  for (StringRef name : group_names) {
    /* Adding groups in this order assumes that def_nr is an index into the names' list. */
    BKE_object_defgroup_add_name(mesh_object_.get(), name.data());
  }
}

void OBJMeshFromRaw::create_edges()
{
  const int64_t tot_edges{curr_object_->tot_edges()};
  for (int i = 0; i < tot_edges; ++i) {
    const MEdge &curr_edge = curr_object_->edges()[i];
    mesh_from_raw_->medge[i].v1 = curr_edge.v1;
    mesh_from_raw_->medge[i].v2 = curr_edge.v2;
  }

  /* Set argument `update` to true so that existing, explicitly imported edges can be merged
   * with the new ones created from polygons. */
  BKE_mesh_calc_edges(mesh_from_raw_.get(), true, false);
  BKE_mesh_calc_edges_loose(mesh_from_raw_.get());
}

void OBJMeshFromRaw::create_uv_verts()
{
  if (curr_object_->tot_uv_verts() > 0 && curr_object_->tot_uv_vert_indices() > 0) {
    MLoopUV *mluv_dst = static_cast<MLoopUV *>(CustomData_add_layer(
        &mesh_from_raw_->ldata, CD_MLOOPUV, CD_CALLOC, nullptr, curr_object_->tot_loops()));
    int tot_loop_idx = 0;
    for (const OBJFaceElem &curr_face : curr_object_->face_elements()) {
      for (const OBJFaceCorner &curr_corner : curr_face.face_corners) {
        if (curr_corner.uv_vert_index < 0 ||
            curr_corner.uv_vert_index >= curr_object_->tot_uv_verts()) {
          continue;
        }
        /* Current corner's UV vertex index indices into current object's UV vertex indices, which
         * index into global list of UV vertex coordinates. */
        const float2 &mluv_src =
            global_vertices_
                ->uv_vertices[curr_object_->uv_vertex_indices()[curr_corner.uv_vert_index]];
        copy_v2_v2(mluv_dst[tot_loop_idx].uv, mluv_src);
        tot_loop_idx++;
      }
    }
  }
}

void OBJMeshFromRaw::create_materials(Main *bmain,
                                      const OBJRawObject &curr_object,
                                      const Map<std::string, MTLMaterial> &materials)
{
  for (const Map<std::string, MTLMaterial>::Item &curr_mat : materials.items()) {
    Material *mat = BKE_material_add(bmain, curr_mat.key.c_str());
    mat->use_nodes = true;
    ShaderNodetreeWrap mat_wrap{curr_mat.value};
    mat->nodetree = mat_wrap.get_nodetree();
  }
}
}  // namespace blender::io::obj
