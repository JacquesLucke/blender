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

#include "MEM_guardedalloc.h"

#include <stdio.h>
#include <vector>

#include "BKE_curve.h"
#include "BKE_lib_id.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_runtime.h"
#include "BKE_scene.h"

#include "BLI_math.h"
#include "BLI_path_util.h"
#include "BLI_vector.hh"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "DNA_curve_types.h"
#include "DNA_layer_types.h"
#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"

#include "ED_object.h"

#include "IO_wavefront_obj.h"

#include "wavefront_obj.hh"
#include "wavefront_obj_exporter.hh"
#include "wavefront_obj_file_handler.hh"

namespace io {
namespace obj {

/**
 * Store the product of export axes settings and an object's world transform matrix in
 * world_and_axes_transform[4][4].
 */
void OBJMesh::store_world_axes_transform()
{
  float axes_transform[3][3];
  unit_m3(axes_transform);
  mat3_from_axis_conversion(DEFAULT_AXIS_FORWARD,
                            DEFAULT_AXIS_UP,
                            export_params->forward_axis,
                            export_params->up_axis,
                            axes_transform);
  mul_m4_m3m4(world_and_axes_transform, axes_transform, object->obmat);
  /* mul_m4_m3m4 does not copy last row of obmat, i.e. location data. */
  copy_v4_v4(world_and_axes_transform[3], object->obmat[3]);
}

/**
 * Calculate coordinates of the vertex at given index.
 */
void OBJMesh::calc_vertex_coords(float coords[3], uint vert_index)
{
  copy_v3_v3(coords, me_eval->mvert[vert_index].co);
  mul_m4_v3(world_and_axes_transform, coords);
  mul_v3_fl(coords, export_params->scaling_factor);
}

/**
 * Calculate vertex indices of all vertices of a polygon.
 */
void OBJMesh::calc_poly_vertex_indices(std::vector<uint> &poly_vertex_indices, uint poly_index)
{
  const MPoly &mpoly = me_eval->mpoly[poly_index];
  const MLoop *mloop = &me_eval->mloop[mpoly.loopstart];
  poly_vertex_indices.resize(mpoly.totloop);
  for (uint loop_index = 0; loop_index < mpoly.totloop; loop_index++) {
    poly_vertex_indices[loop_index] = (mloop + loop_index)->v + 1;
  }
}

/**
 * Store UV vertex coordinates as well as their indices.
 */
void OBJMesh::store_uv_coords_and_indices(std::vector<std::array<float, 2>> &uv_coords,
                                          std::vector<std::vector<uint>> &uv_indices)
{
  Mesh *me_eval = this->me_eval;
  OBJMesh *ob_mesh = this;
  const CustomData *ldata = &me_eval->ldata;

  for (int layer_idx = 0; layer_idx < ldata->totlayer; layer_idx++) {
    const CustomDataLayer *layer = &ldata->layers[layer_idx];
    if (layer->type != CD_MLOOPUV) {
      continue;
    }

    const MPoly *mpoly = me_eval->mpoly;
    const MLoop *mloop = me_eval->mloop;
    const MLoopUV *mloopuv = static_cast<MLoopUV *>(layer->data);
    const float limit[2] = {STD_UV_CONNECT_LIMIT, STD_UV_CONNECT_LIMIT};
    UvVertMap *uv_vert_map = BKE_mesh_uv_vert_map_create(
        mpoly, mloop, mloopuv, me_eval->totpoly, me_eval->totvert, limit, false, false);

    uv_indices.resize(me_eval->totpoly);
    /* TODO ankitm check if pre-emptively reserving space for uv coords improves timing or not.
     * It is guaranteed that there will be at least me_eval->totvert vertices.
     */
    ob_mesh->tot_uv_vertices = -1;

    for (int vertex_index = 0; vertex_index < me_eval->totvert; vertex_index++) {
      const UvMapVert *uv_vert = BKE_mesh_uv_vert_map_get_vert(uv_vert_map, vertex_index);
      while (uv_vert != NULL) {
        if (uv_vert->separate) {
          ob_mesh->tot_uv_vertices++;
        }
        const uint vertices_in_poly = me_eval->mpoly[uv_vert->poly_index].totloop;
        /* Resize UV vertices index list. */
        uv_indices[uv_vert->poly_index].resize(vertices_in_poly);
        /* Fill up UV vertex index for the polygon's one vertex. */
        uv_indices[uv_vert->poly_index][uv_vert->loop_of_poly_index] = ob_mesh->tot_uv_vertices +
                                                                       1;

        /* Fill up UV vertices' coordinates. We don't know how many unique vertices are there, so
         * need to push back everytime. */
        uv_coords.push_back(std::array<float, 2>());
        uv_coords[ob_mesh->tot_uv_vertices][0] =
            mloopuv[mpoly[uv_vert->poly_index].loopstart + uv_vert->loop_of_poly_index].uv[0];
        uv_coords[ob_mesh->tot_uv_vertices][1] =
            mloopuv[mpoly[uv_vert->poly_index].loopstart + uv_vert->loop_of_poly_index].uv[1];

        uv_vert = uv_vert->next;
      }
    }

    /* Actual number of total UV vertices is 1-based. */
    ob_mesh->tot_uv_vertices += 1;
    BKE_mesh_uv_vert_map_free(uv_vert_map);
    /* No need to go over other layers. */
    break;
  }
}

/**
 * Calculate face normal of the polygon at given index.
 */
void OBJMesh::calc_poly_normal(float poly_normal[3], uint poly_index)
{
  float sum[3] = {0, 0, 0};
  const MPoly &poly_to_write = me_eval->mpoly[poly_index];
  const MLoop *mloop = &me_eval->mloop[poly_to_write.loopstart];

  /* Sum all vertex normals to get a face normal. */
  for (uint i = 0; i < poly_to_write.totloop; i++) {
    sum[0] += me_eval->mvert[(mloop + i)->v].no[0];
    sum[1] += me_eval->mvert[(mloop + i)->v].no[1];
    sum[2] += me_eval->mvert[(mloop + i)->v].no[2];
  }

  mul_mat3_m4_v3(world_and_axes_transform, sum);
  copy_v3_v3(poly_normal, sum);
  normalize_v3(poly_normal);
}

/**
 * Calculate face normal indices of all polygons.
 */
void OBJMesh::calc_poly_normal_indices(std::vector<uint> &normal_indices, uint poly_index)
{
  normal_indices.resize(me_eval->mpoly[poly_index].totloop);
  for (uint i = 0; i < normal_indices.size(); i++) {
    normal_indices[i] = poly_index + 1;
  }
}

/**
 * Only for curve-like meshes: calculate vertex indices of one edge.
 */
void OBJMesh::calc_edge_vert_indices(std::array<uint, 2> &vert_indices, uint edge_index)
{
  vert_indices[0] = edge_index + 1;
  vert_indices[1] = edge_index + 2;
  /* TODO ankitm: check if this causes a slow down. If not, keep it here for consistency in the
   * writer. Last edge depends on whether the curve is cyclic or not.
   */
  if (edge_index == me_eval->totedge) {
    vert_indices[0] = edge_index + 1;
    vert_indices[1] = me_eval->totvert == me_eval->totedge ? 1 : me_eval->totvert;
  }
}

/**
 * Create a new mesh from given one and triangulate it.
 * \note The new mesh created here needs to be freed.
 * \return Pointer to the triangulated mesh.
 */
static Mesh *triangulate_mesh(Mesh *me_eval)
{
  struct BMeshCreateParams bm_create_params {
    .use_toolflags = false
  };
  struct BMeshFromMeshParams bm_convert_params {
    /* If calc_face_normal is false, it triggers BLI_assert(BM_face_is_normal_valid(f)). */
    .calc_face_normal = true, 0, 0, 0
  };
  /* Lower threshold where triangulation of a face starts, i.e. a quadrilateral will be
   * triangulated here. */
  int triangulate_min_verts = 4;

  BMesh *bmesh = BKE_mesh_to_bmesh_ex(me_eval, &bm_create_params, &bm_convert_params);
  BM_mesh_triangulate(bmesh,
                      MOD_TRIANGULATE_NGON_BEAUTY,
                      MOD_TRIANGULATE_QUAD_SHORTEDGE,
                      triangulate_min_verts,
                      false,
                      NULL,
                      NULL,
                      NULL);
  Mesh *triangulated = BKE_mesh_from_bmesh_for_eval_nomain(bmesh, NULL, me_eval);

  BM_mesh_free(bmesh);
  return triangulated;
}

/**
 * Store evaluated object and mesh pointers depending on object type.
 * New meshes are created for curves and triangulated meshes.
 */
void OBJMesh::get_mesh_eval()
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  object = DEG_get_evaluated_object(depsgraph, object);
  me_eval = BKE_object_get_evaluated_mesh(object);
  me_eval_needs_free = false;

  if (me_eval && me_eval->totpoly > 0) {
    if (export_params->export_triangulated_mesh) {
      me_eval = triangulate_mesh(me_eval);
      me_eval_needs_free = true;
    }
  }

  /* Curves need a new mesh to be exported in the form of vertices and edges.
   * For primitive circle, new mesh is redundant, but it behaves more like curves, so kept it here.
   */
  else if (object->type == OB_CURVE && !export_params->export_curves_as_nurbs) {
    me_eval = BKE_mesh_new_from_object(depsgraph, object, true);
    me_eval_needs_free = true;
  }
}

/**
 * Traverses over and exports a single frame to a single OBJ file.
 */
static void export_frame(bContext *C, const OBJExportParams *export_params, const char *filepath)
{
  std::vector<OBJMesh> export_mesh;
  /** TODO ankitm Unused now; to be done. */
  std::vector<OBJNurbs> export_nurbs;
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Base *base = static_cast<Base *>(view_layer->object_bases.first);

  for (; base; base = base->next) {
    Object *object_in_layer = base->object;
    switch (object_in_layer->type) {
      case OB_MESH:
        export_mesh.push_back(OBJMesh());
        export_mesh.back().object = object_in_layer;
        export_mesh.back().export_params = export_params;
        export_mesh.back().C = C;
        break;
      case OB_CURVE:
        /* TODO (ankitm) Conditionally push to export_nurbs too. */
        export_mesh.push_back(OBJMesh());
        export_mesh.back().object = object_in_layer;
        export_mesh.back().export_params = export_params;
        export_mesh.back().C = C;
      default:
        break;
    }
  }

  OBJWriter frame_writer;

  if (!frame_writer.open_file(export_params->filepath)) {
    fprintf(stderr, "Error in creating the file: %s\n", export_params->filepath);
    return;
  }

  frame_writer.write_header();

  for (uint ob_iter = 0; ob_iter < export_mesh.size(); ob_iter++) {
    OBJMesh &mesh_to_export = export_mesh[ob_iter];
    mesh_to_export.get_mesh_eval();
    mesh_to_export.store_world_axes_transform();

    frame_writer.write_object_name(mesh_to_export);
    frame_writer.write_vertex_coords(mesh_to_export);

    if (mesh_to_export.me_eval->totpoly == 0) {
      /* For curves and primitive circle. */
      frame_writer.write_curve_edges(mesh_to_export);
    }
    else {
      std::vector<std::vector<uint>> uv_indices;
      if (mesh_to_export.export_params->export_uv) {
        frame_writer.write_uv_coords(mesh_to_export, uv_indices);
      }
      if (mesh_to_export.export_params->export_normals) {
        frame_writer.write_poly_normals(mesh_to_export);
      }
      frame_writer.write_poly_indices(mesh_to_export, uv_indices);
    }
    frame_writer.update_index_offsets(mesh_to_export);

    if (mesh_to_export.me_eval_needs_free) {
      BKE_id_free(NULL, mesh_to_export.me_eval);
    }
  }
  frame_writer.close_file();
}

/**
 * Central internal function to call scene update & writer functions.
 */
void exporter_main(bContext *C, const OBJExportParams *export_params)
{
  ED_object_editmode_exit(C, EM_FREEDATA);
  Scene *scene = CTX_data_scene(C);
  const char *filepath = export_params->filepath;

  /* Single frame export, i.e. no amimation is to be exported. */
  if (!export_params->export_animation) {
    export_frame(C, export_params, filepath);
    printf("Writing to %s\n", filepath);
    return;
  }

  int start_frame = export_params->start_frame;
  int end_frame = export_params->end_frame;
  /* Insert frame number (with "-" if frame is negative) in the filename. */
  char filepath_with_frames[FILE_MAX];
  /* To reset the Scene to its original state. */
  int original_frame = CFRA;

  for (int frame = start_frame; frame <= end_frame; frame++) {
    BLI_strncpy(filepath_with_frames, filepath, FILE_MAX);
    /* 1 _ + 11 digits for frame number (INT_MAX + sign) + 4 for extension + 1 null. */
    char frame_ext[17];
    BLI_snprintf(frame_ext, 17, "_%d.obj", frame);
    bool filepath_ok = BLI_path_extension_replace(filepath_with_frames, FILE_MAX, frame_ext);
    if (filepath_ok == false) {
      printf("Error: File Path too long.\n%s\n", filepath_with_frames);
      return;
    }

    CFRA = frame;
    BKE_scene_graph_update_for_newframe(CTX_data_ensure_evaluated_depsgraph(C), CTX_data_main(C));
    printf("Writing to %s\n", filepath_with_frames);
    export_frame(C, export_params, filepath_with_frames);
  }
  CFRA = original_frame;
}
}  // namespace obj
}  // namespace io
