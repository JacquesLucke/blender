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

#include "BKE_lib_id.h"
#include "BKE_object.h"

#include "BLI_float2.hh"
#include "BLI_float3.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "DNA_collection_types.h"
#include "DNA_curve_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "wavefront_obj_im_objects.hh"

namespace blender::io::obj {
/**
 * List of all vertex and UV vertex coordinates in an OBJ file accessible to any
 * raw object at any time.
 */
struct GlobalVertices {
  Vector<float3> vertices{};
  Vector<float2> uv_vertices{};
};

/**
 * A face's corner in an OBJ file. In Blender, it translates to a mloop vertex.
 */
struct OBJFaceCorner {
  /* This index should stay local to a raw object, & not index into the global list of vertices. */
  int vert_index;
  /* -1 is to indicate abscense of UV vertices. Only < 0 condition should be checked since
   * it can be less than -1 too. */
  int uv_vert_index = -1;
};

struct OBJFaceElem {
  std::string vertex_group{};
  bool shaded_smooth = false;
  Vector<OBJFaceCorner> face_corners;
};

/**
 * Contains data for one single NURBS curve in the OBJ file.
 */
struct OBJNurbsElem {
  /**
   * For curves, groups may be used to specify multiple splines in the same curve object.
   * It may also serve as the name of the curve if not specified explicitly.
   */
  std::string group_{};
  int degree = 0;
  /**
   * Indices into the global list of vertex coordinates. Must be non-negative.
   */
  Vector<int> curv_indices{};
  /* Values in the parm u/v line in a curve definition. */
  Vector<float> parm{};
};

class OBJRawObject {
 private:
  int object_type_ = OB_MESH;
  std::string object_name_{};
  Vector<std::string> material_name_{};
  /**
   * Vertex indices that index into the global list of vertex coordinates.
   * Lines that start with "v" are stored here, while the actual coordinates are in Global
   * vertices list.
   */
  Vector<int> vertex_indices_{};
  /**
   * UV Vertex indices that index into the global list of UV vertex coordinates.
   * Lines that start with "vn" are stored here, while the actual coordinates are in Global
   * vertices list.
   */
  Vector<int> uv_vertex_indices_{};
  /** Edges written in the file in addition to (or even without polygon) elements. */
  Vector<MEdge> edges_{};
  Vector<OBJFaceElem> face_elements_{};
  bool use_vertex_groups_ = false;
  OBJNurbsElem nurbs_element_;
  int tot_loops_ = 0;
  int tot_normals_ = 0;
  /** Total UV vertices referred to by an object's faces. */
  int tot_uv_verts_ = 0;

 public:
  OBJRawObject(StringRef ob_name) : object_name_(ob_name.data()){};

  int object_type() const;
  const std::string &object_name() const;
  Span<int> vertex_indices() const;
  int64_t tot_verts() const;
  Span<OBJFaceElem> face_elements() const;
  int64_t tot_face_elems() const;
  bool use_vertex_groups() const;
  Span<int> uv_vertex_indices() const;
  int64_t tot_uv_vert_indices() const;
  Span<MEdge> edges() const;
  int64_t tot_edges() const;
  int tot_loops() const;
  int tot_normals() const;
  int tot_uv_verts() const;

  const OBJNurbsElem &nurbs_elem() const;
  const std::string &group() const;

  /* Parser class edits all the parameters of the Raw object class. */
  friend class OBJParser;
};

enum eTextureMapType {
  MAP_KD = 1,
  MAP_KS = 2,
  MAP_KE = 3,
  MAP_D = 4,
  MAP_REFL = 5,
  MAP_NS = 6,
  MAP_BUMP = 7,
};

/**
 * Used for storing parameters for all kinds of texture maps from MTL file.
 */
struct tex_map_XX {
  float3 translation = {0.0f, 0.0f, 0.0f};
  float3 scale = {1.0f, 1.0f, 1.0f};
  std::string image_path{};
};

/**
 * Store material data parsed from MTL file.
 */
struct MTLMaterial {
  std::string name{};
  float Ns{1.0f};
  float3 Ka;
  float3 Kd;
  float3 Ks;
  float3 Ke;
  float Ni{1.0f};
  float d{1.0f};
  int illum{0};
  tex_map_XX map_Kd;
  tex_map_XX map_Ks;
  tex_map_XX map_Ke;
  tex_map_XX map_d;
  tex_map_XX map_refl;
  tex_map_XX map_Ns;
  tex_map_XX map_Bump;
  /** Only used for Normal Map node: map_Bump. */
  float map_Bump_value = 0.0f;
  Span<eTextureMapType> all_tex_map_types() const;
  const tex_map_XX &tex_map_of_type(eTextureMapType type) const;
};

struct UniqueObjectDeleter {
  void operator()(Object *object)
  {
    BKE_id_free(NULL, object);
  }
};

using unique_object_ptr = std::unique_ptr<Object, UniqueObjectDeleter>;

class OBJImportCollection {
 private:
  Main *bmain_;
  Scene *scene_;
  /**
   * The collection that holds all the imported objects.
   */
  Collection *obj_import_collection_;

 public:
  OBJImportCollection(Main *bmain, Scene *scene);

  void add_object_to_collection(unique_object_ptr b_object);
};
}  // namespace blender::io::obj

#endif
