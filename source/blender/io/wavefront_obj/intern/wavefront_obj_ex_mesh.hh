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

#ifndef __WAVEFRONT_OBJ_EX_MESH_HH__
#define __WAVEFRONT_OBJ_EX_MESH_HH__

#include "BKE_lib_id.h"
#include "BKE_material.h"
#include "BKE_mesh.h"

#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "IO_wavefront_obj.h"

namespace blender::io::obj {
class OBJMesh : NonMovable, NonCopyable {
 private:
  Depsgraph *depsgraph_;
  const OBJExportParams &export_params_;

  Object *export_object_eval_;
  Mesh *export_mesh_eval_;
  /**
   * For curves which are converted to mesh, and triangulated meshes, a new mesh is allocated
   * which needs to be freed later.
   */
  bool mesh_eval_needs_free_ = false;
  /**
   * Final transform of an object obtained from export settings (up_axis, forward_axis) and world
   * transform matrix.
   */
  float world_and_axes_transform_[4][4];

  /**
   * Total vertices in a mesh.
   */
  uint tot_vertices_;
  /**
   * Total polygons (and thus normals) in a mesh.
   */
  uint tot_poly_normals_;
  /**
   * Total UV vertices in a mesh's texture map.
   */
  uint tot_uv_vertices_;
  /**
   * Only for curve converted to meshes: total edges in a mesh.
   */
  uint tot_edges_;
  /**
   * Total smooth groups in an object.
   */
  uint tot_smooth_groups_ = 0;

 public:
  OBJMesh(Depsgraph *depsgraph, const OBJExportParams &export_params, Object *export_object);
  ~OBJMesh();

  const uint tot_vertices();
  const uint tot_polygons();
  const uint tot_uv_vertices();
  const uint tot_edges();
  const short tot_col();
  const uint tot_smooth_groups();

  void ensure_mesh_normals();
  Material *get_object_material(short mat_nr);
  const MPoly &get_ith_poly(uint i);

  const char *get_object_name();
  const char *get_object_data_name();
  const char *get_object_material_name(short mat_nr);

  void calc_vertex_coords(float r_coords[3], uint vert_index);
  void calc_poly_vertex_indices(Vector<uint> &r_poly_vertex_indices, uint poly_index);
  void store_uv_coords_and_indices(Vector<std::array<float, 2>> &r_uv_coords,
                                   Vector<Vector<uint>> &r_uv_indices);
  void calc_poly_normal(float r_poly_normal[3], uint poly_index);
  void calc_vertex_normal(float r_vertex_normal[3], uint vertex_index);
  void calc_poly_normal_indices(Vector<uint> &r_normal_indices, uint poly_index);
  int *calc_smooth_groups();
  const char *get_poly_deform_group_name(const MPoly &mpoly, short &r_last_vertex_group);
  void calc_edge_vert_indices(uint r_vert_indices[2], uint edge_index);

 private:
  void triangulate_mesh_eval();
  void store_world_axes_transform();
};
}  // namespace blender::io::obj

#endif
