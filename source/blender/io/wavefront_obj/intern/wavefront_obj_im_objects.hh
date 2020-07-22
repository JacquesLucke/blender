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

#ifndef __WAVEFRONT_OBJ_IM_OBJECTS_HH__
#define __WAVEFRONT_OBJ_IM_OBJECTS_HH__

#include "BLI_float2.hh"
#include "BLI_float3.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "DNA_collection_types.h"
#include "DNA_curve_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "wavefront_obj_im_mesh.hh"
#include "wavefront_obj_im_nurbs.hh"

namespace blender::io::obj {
struct GlobalVertices {
  Vector<float3> vertices{};
  Vector<float2> uv_vertices{};
};

typedef struct OBJFaceCorner {
  int vert_index;
  /* -1 is to indicate abscense of UV vertices. Only < 0 condition should be checked since
   * it can be less than -1 too. */
  int uv_vert_index = -1;
} OBJFaceCorner;

typedef struct OBJFaceElem {
  bool shaded_smooth = false;
  Vector<OBJFaceCorner> face_corners;
} OBJFaceElem;

/**
 * Contains data for one single NURBS curve in the OBJ file.
 */
struct NurbsElem {
  /**
   * For curves, groups may be used to specify multiple splines in the same curve object.
   * It may also serve as the name of the curve if not specified explicitly.
   */
  std::string group{};
  int degree = 0;
  /**
   * Indices into the global list of vertex coordinates. Must be non-negative.
   */
  Vector<int> curv_indices{};
  Vector<float> parm{};
};

class OBJRawObject {
 public:
  OBJRawObject(StringRef ob_name) : object_name(ob_name.data()){};

  int object_type = OB_MESH;
  std::string object_name{};
  Vector<int> vertex_indices{};
  Vector<int> uv_vertex_indices{};
  /**
   * Edges written in the file in addition to (or even without polygon) elements.
   */
  Vector<MEdge> edges{};
  Vector<OBJFaceElem> face_elements{};
  uint tot_normals = 0;
  uint tot_loop = 0;
  uint tot_uv_verts = 0;
  Vector<std::string> material_name{};

  NurbsElem nurbs_element;
};

class OBJParentCollection {
 private:
  Main *bmain_;
  Scene *scene_;
  Collection *parent_collection_;

 public:
  OBJParentCollection(Main *bmain, Scene *scene);

  void add_object_to_parent(OBJRawObject *object_to_add, unique_mesh_ptr mesh);
  void add_object_to_parent(OBJRawObject *object_to_add, unique_curve_ptr curve);
};
}  // namespace blender::io::obj

#endif
