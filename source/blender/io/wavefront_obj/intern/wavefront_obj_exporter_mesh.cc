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

#include "BKE_customdata.h"
#include "BKE_deform.h"
#include "BKE_mesh_mapping.h"
#include "BKE_object.h"

#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_math.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "DEG_depsgraph_query.h"

#include "DNA_layer_types.h"
#include "DNA_material_types.h"
#include "DNA_modifier_types.h"

#include "wavefront_obj_exporter_mesh.hh"

namespace blender {
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
 * Store the product of export axes settings and an object's world transform matrix in
 * world_and_axes_transform[4][4].
 */
void OBJMesh::store_world_axes_transform()
{
  float axes_transform[3][3];
  unit_m3(axes_transform);
  /* -Y-forward and +Z-up are the default Blender axis settings. */
  mat3_from_axis_conversion(OBJ_AXIS_NEGATIVE_Y_FORWARD,
                            OBJ_AXIS_Z_UP,
                            _export_params->forward_axis,
                            _export_params->up_axis,
                            axes_transform);
  mul_m4_m3m4(_world_and_axes_transform, axes_transform, _export_object_eval->obmat);
  /* mul_m4_m3m4 does not copy last row of obmat, i.e. location data. */
  copy_v4_v4(_world_and_axes_transform[3], _export_object_eval->obmat[3]);
}

const char *OBJMesh::get_object_name()
{
  return _export_object_eval->id.name + 2;
}

const char *OBJMesh::get_object_data_name()
{
  return _export_mesh_eval->id.name + 2;
}

const char *OBJMesh::get_object_material_name(short mat_nr)
{
  Material *mat = BKE_object_material_get(_export_object_eval, mat_nr);
  return mat->id.name + 2;
}

void OBJMesh::ensure_normals()
{
  BKE_mesh_ensure_normals(_export_mesh_eval);
}

/** Return mat_nr-th material of the object. */
Material *OBJMesh::get_object_material(short mat_nr)
{
  return BKE_object_material_get(_export_object_eval, mat_nr);
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
void OBJMesh::calc_poly_vertex_indices(Vector<uint> &r_poly_vertex_indices, uint poly_index)
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
void OBJMesh::store_uv_coords_and_indices(Vector<std::array<float, 2>> &r_uv_coords,
                                          Vector<Vector<uint>> &r_uv_indices)
{
  const MPoly *mpoly = _export_mesh_eval->mpoly;
  const MLoop *mloop = _export_mesh_eval->mloop;
  const int totpoly = _export_mesh_eval->totpoly;
  const int totvert = _export_mesh_eval->totvert;
  const MLoopUV *mloopuv = (MLoopUV *)CustomData_get_layer(&_export_mesh_eval->ldata, CD_MLOOPUV);
  if (!mloopuv) {
    _tot_uv_vertices = 0;
    return;
  }
  const float limit[2] = {STD_UV_CONNECT_LIMIT, STD_UV_CONNECT_LIMIT};

  UvVertMap *uv_vert_map = BKE_mesh_uv_vert_map_create(
      mpoly, mloop, mloopuv, totpoly, totvert, limit, false, false);

  r_uv_indices.resize(totpoly);
  /* We know that at least totvert many vertices in a mesh will be present in its texture map. So
   * reserve them in the start to let append be less time costly later. */
  r_uv_coords.reserve(totvert);

  _tot_uv_vertices = -1;
  for (int vertex_index = 0; vertex_index < totvert; vertex_index++) {
    const UvMapVert *uv_vert = BKE_mesh_uv_vert_map_get_vert(uv_vert_map, vertex_index);
    while (uv_vert != NULL) {
      if (uv_vert->separate) {
        _tot_uv_vertices += 1;
      }
      const uint vertices_in_poly = mpoly[uv_vert->poly_index].totloop;

      /* Fill up UV vertices' coordinates. */
      r_uv_coords.resize(_tot_uv_vertices + 1);
      const int loopstart = mpoly[uv_vert->poly_index].loopstart;
      const float *vert_uv_coords = mloopuv[loopstart + uv_vert->loop_of_poly_index].uv;
      r_uv_coords[_tot_uv_vertices][0] = vert_uv_coords[0];
      r_uv_coords[_tot_uv_vertices][1] = vert_uv_coords[1];

      /* Fill up UV vertex index. The indices in OBJ are 1-based, so added 1. */
      r_uv_indices[uv_vert->poly_index].resize(vertices_in_poly);
      r_uv_indices[uv_vert->poly_index][uv_vert->loop_of_poly_index] = _tot_uv_vertices;

      uv_vert = uv_vert->next;
    }
  }
  /* Needed to update the index offsets after a mesh is written. */
  _tot_uv_vertices += 1;
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
void OBJMesh::calc_poly_normal_indices(Vector<uint> &r_normal_indices, uint poly_index)
{
  r_normal_indices.resize(_export_mesh_eval->mpoly[poly_index].totloop);
  for (uint i = 0; i < r_normal_indices.size(); i++) {
    r_normal_indices[i] = poly_index + 1;
  }
}

/**
 * Find the name of the group to which maximum number of vertices of a poly belong.
 * If no vertex belongs to any group, name is "off".
 * If there's a tie between two or more vertices, group name depends on the implementation
 * of max_element.
 */
const char *OBJMesh::get_object_deform_vert(const MPoly &mpoly)
{
  const MLoop *mloop = &_export_mesh_eval->mloop[mpoly.loopstart];
  Vector<int> deform_group_indices;
  deform_group_indices.resize(mpoly.totloop, 0);
  bool has_group = false;
  for (uint loop_index = 0; loop_index < mpoly.totloop; loop_index++) {
    const MDeformVert dvert = _export_mesh_eval->dvert[(mloop + loop_index)->v];
    const MDeformWeight *curr_weight = dvert.dw;
    if (curr_weight) {
      bDeformGroup *vertex_group = (bDeformGroup *)BLI_findlink(
          (ListBase *)(&_export_object_eval->defbase), curr_weight->def_nr);
      if (vertex_group) {
        deform_group_indices[curr_weight->def_nr] += 1;
        has_group = true;
      }
    }
  }
  if (!has_group) {
    return "off";
  }
  int max_idx = *std::max_element(deform_group_indices.begin(), deform_group_indices.end());
  bDeformGroup *vertex_group = (bDeformGroup *)BLI_findlink(
      (ListBase *)(&_export_object_eval->defbase), deform_group_indices[max_idx]);
  return vertex_group->name;
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

}  // namespace obj
}  // namespace io
}  // namespace blender
