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
 */

/** \file
 * \ingroup obj
 */

#ifndef __WAVEFRONT_OBJ_HH__
#define __WAVEFRONT_OBJ_HH__

#include "BKE_context.h"
#include "BKE_mesh.h"
#include "BKE_object.h"

#include "BLI_array.hh"
#include "BLI_vector.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "IO_wavefront_obj.h"

namespace io {
namespace obj {

/* -Y */
#define DEFAULT_AXIS_FORWARD 4
/* Z */
#define DEFAULT_AXIS_UP 2

class OBJMesh {
 public:
  const OBJExportParams *export_params;
  bContext *C;

  Object *object;
  Mesh *me_eval;
  /** For curves and triangulated meshes, a new mesh is allocated which needs to be freed later. */
  bool me_eval_needs_free = false;

  /** Total vertices in a mesh. */
  uint tot_vertices;
  /** Total polygons (and thus normals) in a mesh. */
  uint tot_poly_normals;
  /** Total UV vertices in a mesh's texture map. */
  uint tot_uv_vertices;
  /** Only for curve-like meshes: total edges in a mesh. */
  uint tot_edges;
  /** Final transform of an object obtained from export settings (up_axis, forward_axis) and world
   * transform matrix.
   */
  float world_and_axes_transform[4][4];

  /**
   * Store evaluated object and mesh pointers depending on object type.
   * New meshes are created for curves and triangulated meshes.
   */
  void get_mesh_eval();
  /**
   * Store the product of export axes settings and an object's world transform matrix in
   * world_and_axes_transform[4][4].
   */
  void store_world_axes_transform();
  /**
   * Calculate coordinates of the vertex at given index.
   */
  void calc_vertex_coords(float coords[3], uint vert_index);
  /**
   * Calculate vertex indices of all vertices of a polygon.
   */
  void calc_poly_vertex_indices(std::vector<uint> &poly_vertex_indices, uint poly_index);
  /**
   * Store UV vertex coordinates as well as their indices.
   */
  void store_uv_coords_and_indices(std::vector<std::array<float, 2>> &uv_coords,
                                   std::vector<std::vector<uint>> &uv_indices);
  /**
   * Calculate face normal of the polygon at given index.
   */
  void calc_poly_normal(float poly_normal[3], uint poly_index);
  /**
   * Calculate face normal indices of all polygons.
   */
  void calc_poly_normal_indices(std::vector<uint> &normal_indices, uint poly_indices);
  /**
   * Only for curve-like meshes: calculate vertex indices of one edge.
   */
  void calc_edge_vert_indices(std::array<uint, 2> &vert_indices, uint edge_index);
};

class OBJNurbs {
  /*TODO ankitm to be done*/
 public:
};

}  // namespace obj
}  // namespace io

#endif /* __WAVEFRONT_OBJ_HH__ */
