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

#include "BKE_curve.h"
#include "BKE_customdata.h"
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
 * Store evaluated object and mesh pointers depending on object type.
 * New meshes are created for curves converted to meshes and triangulated meshes.
 */
void OBJMesh::init_export_mesh(Object *export_object)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(_C);
  _export_object_eval = DEG_get_evaluated_object(depsgraph, export_object);
  _export_mesh_eval = BKE_object_get_evaluated_mesh(_export_object_eval);
  _me_eval_needs_free = false;

  if (_export_mesh_eval && _export_mesh_eval->totpoly > 0) {
    if (_export_params->export_triangulated_mesh) {
      triangulate_mesh(_export_mesh_eval);
      _me_eval_needs_free = true;
    }
    _tot_vertices = _export_mesh_eval->totvert;
    _tot_poly_normals = _export_mesh_eval->totpoly;
  }
  /* Curves and nurbs surfaces need a new mesh when exported in the form of vertices and edges.
   * For primitive circle, new mesh is redundant, but it behaves more like curves, so kept it here.
   */
  else {
    _export_mesh_eval = BKE_mesh_new_from_object(depsgraph, _export_object_eval, true);
    _me_eval_needs_free = true;
    if (_export_object_eval->type == OB_CURVE || _export_mesh_eval->totpoly == 0) {
      /* Don't export polygon normals when there are no polygons. */
      _tot_poly_normals = 0;
      _tot_vertices = _export_mesh_eval->totvert;
      _tot_edges = _export_mesh_eval->totedge;
    }
    else if (_export_object_eval->type == OB_SURF) {
      _tot_vertices = _export_mesh_eval->totvert;
      _tot_poly_normals = _export_mesh_eval->totpoly;
    }
  }
  store_world_axes_transform();
}

/**
 * Triangulate given mesh and update _export_mesh_eval.
 * \note The new mesh created here needs to be freed.
 */
void OBJMesh::triangulate_mesh(Mesh *me_eval)
{
  struct BMeshCreateParams bm_create_params = {false};
  /* If calc_face_normal is false, it triggers BLI_assert(BM_face_is_normal_valid(f)). */
  struct BMeshFromMeshParams bm_convert_params = {true, 0, 0, 0};
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
  _export_mesh_eval = BKE_mesh_from_bmesh_for_eval_nomain(bmesh, NULL, me_eval);
  BM_mesh_free(bmesh);
}
/**
 * Initialise nurbs curve object.
 */
void OBJNurbs::init_nurbs_curve(Object *export_object)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(_C);
  _export_object_eval = DEG_get_evaluated_object(depsgraph, export_object);
  _export_curve = (Curve *)_export_object_eval->data;
}

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
                            _export_params->forward_axis,
                            _export_params->up_axis,
                            axes_transform);
  mul_m4_m3m4(_world_and_axes_transform, axes_transform, _export_object_eval->obmat);
  /* mul_m4_m3m4 does not copy last row of obmat, i.e. location data. */
  copy_v4_v4(_world_and_axes_transform[3], _export_object_eval->obmat[3]);
}

void OBJMesh::get_object_name(const char **r_object_name)
{
  *r_object_name = _export_object_eval->id.name + 2;
}

/**
 * Calculate coordinates of the vertex at given index.
 */
void OBJMesh::calc_vertex_coords(float r_coords[3], uint point_index)
{
  copy_v3_v3(r_coords, _export_mesh_eval->mvert[point_index].co);
  mul_m4_v3(_world_and_axes_transform, r_coords);
  mul_v3_fl(r_coords, _export_params->scaling_factor);
}

/**
 * Calculate vertex indices of all vertices of a polygon.
 */
void OBJMesh::calc_poly_vertex_indices(blender::Vector<uint> &r_poly_vertex_indices,
                                       uint poly_index)
{
  const MPoly &mpoly = _export_mesh_eval->mpoly[poly_index];
  const MLoop *mloop = &_export_mesh_eval->mloop[mpoly.loopstart];
  r_poly_vertex_indices.resize(mpoly.totloop);
  for (uint loop_index = 0; loop_index < mpoly.totloop; loop_index++) {
    r_poly_vertex_indices[loop_index] = (mloop + loop_index)->v + 1;
  }
}

/**
 * Store UV vertex coordinates as well as their indices.
 */
void OBJMesh::store_uv_coords_and_indices(blender::Vector<std::array<float, 2>> &r_uv_coords,
                                          blender::Vector<blender::Vector<uint>> &r_uv_indices)
{
  const MPoly *mpoly = _export_mesh_eval->mpoly;
  const MLoop *mloop = _export_mesh_eval->mloop;
  const int totpoly = _export_mesh_eval->totpoly;
  const int totvert = _export_mesh_eval->totvert;
  const MLoopUV *mloopuv = (MLoopUV *)CustomData_get_layer(&_export_mesh_eval->ldata, CD_MLOOPUV);
  const float limit[2] = {STD_UV_CONNECT_LIMIT, STD_UV_CONNECT_LIMIT};

  UvVertMap *uv_vert_map = BKE_mesh_uv_vert_map_create(
      mpoly, mloop, mloopuv, totpoly, totvert, limit, false, false);

  r_uv_indices.resize(totpoly);
  /* We know that at least totvert many vertices in a mesh will be present in its texture map. So
   * reserve them in the start to let append be less time costly later. */
  r_uv_coords.reserve(totvert);

  for (int vertex_index = 0; vertex_index < totvert; vertex_index++) {
    const UvMapVert *uv_vert = BKE_mesh_uv_vert_map_get_vert(uv_vert_map, vertex_index);
    while (uv_vert != NULL) {
      const uint vertices_in_poly = mpoly[uv_vert->poly_index].totloop;

      /* Fill up UV vertices' coordinates. */
      const int loopstart = mpoly[uv_vert->poly_index].loopstart;
      const float(&vert_uv_coords)[2] = mloopuv[loopstart + uv_vert->loop_of_poly_index].uv;
      r_uv_coords.append({vert_uv_coords[0], vert_uv_coords[1]});

      /* Fill up UV vertex index. The indices in OBJ are 1-based, so added 1. */
      r_uv_indices[uv_vert->poly_index].resize(vertices_in_poly);
      r_uv_indices[uv_vert->poly_index][uv_vert->loop_of_poly_index] = r_uv_coords.size() + 1;

      uv_vert = uv_vert->next;
    }
  }
  /* Needed to update the index offsets after a mesh is written. */
  _tot_uv_vertices = r_uv_coords.size();
  BKE_mesh_uv_vert_map_free(uv_vert_map);
}

/**
 * Calculate face normal of the polygon at given index.
 */
void OBJMesh::calc_poly_normal(float r_poly_normal[3], uint poly_index)
{
  /* TODO ankitm: Find an existing method to calculate a face's normals from its vertex normals. */
  const MPoly &poly_to_write = _export_mesh_eval->mpoly[poly_index];
  const MLoop *mloop = &_export_mesh_eval->mloop[poly_to_write.loopstart];

  /* Sum all vertex normals to get a face normal. */
  for (uint i = 0; i < poly_to_write.totloop; i++) {
    short(&vert_no)[3] = _export_mesh_eval->mvert[(mloop + i)->v].no;
    r_poly_normal[0] += vert_no[0];
    r_poly_normal[1] += vert_no[1];
    r_poly_normal[2] += vert_no[2];
  }

  mul_mat3_m4_v3(_world_and_axes_transform, r_poly_normal);
  normalize_v3(r_poly_normal);
}

/**
 * Calculate face normal indices of all polygons.
 */
void OBJMesh::calc_poly_normal_indices(blender::Vector<uint> &r_normal_indices, uint poly_index)
{
  r_normal_indices.resize(_export_mesh_eval->mpoly[poly_index].totloop);
  for (uint i = 0; i < r_normal_indices.size(); i++) {
    r_normal_indices[i] = poly_index + 1;
  }
}

/**
 * Only for curve converted to meshes and primitive circle: calculate vertex indices of one edge.
 */
void OBJMesh::calc_edge_vert_indices(uint r_vert_indices[2], uint edge_index)
{
  r_vert_indices[0] = edge_index + 1;
  r_vert_indices[1] = edge_index + 2;

  /* Last edge's second vertex depends on whether the curve is cyclic or not. */
  if (UNLIKELY(edge_index == _export_mesh_eval->totedge)) {
    r_vert_indices[0] = edge_index + 1;
    r_vert_indices[1] = _export_mesh_eval->totvert == _export_mesh_eval->totedge ?
                            1 :
                            _export_mesh_eval->totvert;
  }
}

void OBJNurbs::get_curve_name(const char **r_object_name)
{
  *r_object_name = _export_object_eval->id.name + 2;
}

  /** Get coordinates of a vertex at given point index. */
void OBJNurbs::calc_point_coords(float r_coords[3], uint vert_index, Nurb *nurb)
{
  BPoint *bpoint = nurb->bp;
  bpoint += vert_index;
  copy_v3_v3(r_coords, bpoint->vec);
}

  /** Get nurbs' degree and number of "curv" points of a nurb. */
void OBJNurbs::get_curve_info(int *r_nurbs_degree, int *r_curv_num, Nurb *nurb)
{
  *r_nurbs_degree = nurb->orderu - 1;
  /* "curv_num" are number of control points in a nurbs. If it is cyclic, degree also adds up. */
  *r_curv_num = nurb->pntsv * nurb->pntsu;
  if (nurb->flagu & CU_NURB_CYCLIC) {
    *r_curv_num += *r_nurbs_degree;
  }
}

/**
 * Traverses over and exports a single frame to a single OBJ file.
 */
static void export_frame(bContext *C, const OBJExportParams *export_params, const char *filepath)
{
  blender::Vector<OBJMesh> exportable_meshes;
  /** TODO ankitm Unused now; to be done. */
  blender::Vector<OBJNurbs> exportable_nurbs;
  ViewLayer *view_layer = CTX_data_view_layer(C);
  LISTBASE_FOREACH (Base *, base, &view_layer->object_bases) {
    Object *object_in_layer = base->object;
    switch (object_in_layer->type) {
      case OB_SURF:
      case OB_MESH: {
        exportable_meshes.append(OBJMesh(C, export_params, object_in_layer));
        break;
      }
      case OB_CURVE: {
        Curve *curve = (Curve *)object_in_layer->data;
        Nurb *nurb = (Nurb *)curve->nurb.first;
        if (nurb->type == CU_NURBS) {
          if (export_params->export_curves_as_nurbs) {
            exportable_nurbs.append(OBJNurbs(C, object_in_layer));
          }
          else {
            exportable_meshes.append(OBJMesh(C, export_params, object_in_layer));
          }
        }
        if (nurb->type == CU_BEZIER) {
          exportable_meshes.append(OBJMesh(C, export_params, object_in_layer));
        }
        /* Other types of curves are not supported.  */
        break;
      }
      default:
        break;
    }
  }

  OBJWriter frame_writer(export_params);
  if (!frame_writer.init_writer()) {
    fprintf(stderr, "Error in creating the file: %s\n", export_params->filepath);
    return;
  }

  for (uint ob_iter = 0; ob_iter < exportable_meshes.size(); ob_iter++) {
    OBJMesh &mesh_to_export = exportable_meshes[ob_iter];

    frame_writer.write_object_name(mesh_to_export);
    frame_writer.write_vertex_coords(mesh_to_export);

    /* For curves converted to mesh and primitive circle. */
    if (mesh_to_export.tot_poly_normals() == 0) {
      frame_writer.write_curve_edges(mesh_to_export);
    }
    else {
      blender::Vector<blender::Vector<uint>> uv_indices;
      if (export_params->export_normals) {
        frame_writer.write_poly_normals(mesh_to_export);
      }
      if (export_params->export_uv) {
        frame_writer.write_uv_coords(mesh_to_export, uv_indices);
      }
      frame_writer.write_poly_indices(mesh_to_export, uv_indices);
    }
    frame_writer.update_index_offsets(mesh_to_export);

    mesh_to_export.destruct();
  }
  /* Export nurbs in parm form, not as vertices and edges. */
  for (uint ob_iter = 0; ob_iter < exportable_nurbs.size(); ob_iter++) {
    OBJNurbs &nurbs_to_export = exportable_nurbs[ob_iter];
    frame_writer.write_nurbs_curve(nurbs_to_export);
  }
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
