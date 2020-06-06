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

#include "BLI_array.hh"
#include "BLI_vector.hh"
#include "DNA_meshdata_types.h"

namespace io {
namespace obj {

/**
 * Polygon stores the data of one face of the mesh.
 * f v1/vt1/vn1 v2/vt2/vn2 .. (n)
 */
struct Polygon {
  /** Total vertices in one polgon face. n above. */
  uint total_vertices_per_poly;
  /**
   * Vertex indices of this polygon. v1, v2 .. above.
   * The index corresponds to the pre-defined vertex list.
   */
  BLI::Vector<uint> vertex_index;
  /**
   * Face normal indices of this polygon. vn1, vn2 .. above.
   * The index corresponds to the pre-defined face normal list.
   */
  BLI::Vector<uint> face_normal_index;
};

/**
 * Stores geometry of one object to be exported.
 * TODO (ankitm): Extend it to contain multiple objects' geometry.
 */
struct OBJ_data_to_export {
  bContext *C;
  Depsgraph *depsgraph;

  /** Vertices in a mesh to export. */
  MVert *mvert;
  /** Number of vertices in a mesh to export. */
  uint tot_vertices;

  /** Polygons in a mesh to export. */
  BLI::Vector<io::obj::Polygon> polygon_list;
  /** Number of polygons in a mesh to export. */
  uint tot_faces;
};
}  // namespace obj
}  // namespace io

#endif /* __WAVEFRONT_OBJ_HH__ */
