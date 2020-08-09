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

#pragma once

#include "BKE_lib_id.h"
#include "BKE_object.h"

#include "BLI_float2.hh"
#include "BLI_float3.hh"
#include "BLI_map.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "DNA_collection_types.h"
#include "DNA_curve_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "wavefront_obj_im_objects.hh"

namespace blender::io::obj {

/**
 * Keeps track of the vertices that belong to other objects.
 * Needed only for mloop->v which needs vertex indices ranging from (0 to total vertices in the
 * mesh) as opposed to the indices ranging from (0 to total vertices in the global list).
 */
struct VertexOffset {
 private:
  int vertex_offset_ = 0;

 public:
  int get_vertex_offset() const
  {
    return vertex_offset_;
  }
  void add_vertex_offset(const int vert_offset)
  {
    vertex_offset_ += vert_offset;
  }
};

/**
 * List of all vertex and UV vertex coordinates in an OBJ file accessible to any
 * Geometry instance at any time.
 */
struct GlobalVertices {
  Vector<float3> vertices{};
  Vector<float2> uv_vertices{};
  VertexOffset *vertex_offset = nullptr;
};

/**
 * A face's corner in an OBJ file. In Blender, it translates to a mloop vertex.
 */
struct FaceCorner {
  /* This index should stay local to a Geometry, & not index into the global list of vertices. */
  int vert_index;
  /* -1 is to indicate abscense of UV vertices. Only < 0 condition should be checked since
   * it can be less than -1 too. */
  int uv_vert_index = -1;
};

struct FaceElement {
  std::string vertex_group{};
  bool shaded_smooth = false;
  Vector<FaceCorner> face_corners;
};

/**
 * Contains data for one single NURBS curve in the OBJ file.
 */
struct NurbsElement {
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

enum eGeometryType {
  GEOM_MESH = OB_MESH,
  GEOM_CURVE = OB_CURVE,
};

class Geometry {
 private:
  const eGeometryType geom_type_ = GEOM_MESH;
  std::string geometry_name_{};
  Vector<std::string> material_name_{};
  /**
   * Keys range from zero to total vertices in the file. Values range from 0 to vertices
   * in a Geometry instance.
   */
  Map<int, int> vertex_indices_{};
  /** Edges written in the file in addition to (or even without polygon) elements. */
  Vector<MEdge> edges_{};
  Vector<FaceElement> face_elements_{};
  bool use_vertex_groups_ = false;
  NurbsElement nurbs_element_;
  int tot_loops_ = 0;
  int tot_normals_ = 0;

 public:
  Geometry(eGeometryType type, std::string_view ob_name)
      : geom_type_(type), geometry_name_(std::string(ob_name)){};

  eGeometryType geom_type() const;
  const std::string &geometry_name() const;
  int vertex_indices_lookup(const int key) const;
  int64_t tot_verts() const;
  Span<FaceElement> face_elements() const;
  int64_t tot_face_elems() const;
  bool use_vertex_groups() const;
  Span<MEdge> edges() const;
  int64_t tot_edges() const;
  int tot_loops() const;
  int tot_normals() const;

  const NurbsElement &nurbs_elem() const;
  const std::string &group() const;

  /* Parser class edits all the parameters of the Geometry class. */
  friend class OBJParser;
};

/**
 * Used for storing parameters for all kinds of texture maps from MTL file.
 */
struct tex_map_XX {
  tex_map_XX(StringRef to_socket_id) : dest_socket_id(to_socket_id){};

  const std::string dest_socket_id{};
  float3 translation = {0.0f, 0.0f, 0.0f};
  float3 scale = {1.0f, 1.0f, 1.0f};
  std::string image_path{};
};

/**
 * Store material data parsed from MTL file.
 */
struct MTLMaterial {
  MTLMaterial()
  {
    texture_maps.add("map_Kd", tex_map_XX("Base Color"));
    texture_maps.add("map_Ks", tex_map_XX("Specular"));
    texture_maps.add("map_Ns", tex_map_XX("Roughness"));
    texture_maps.add("map_d", tex_map_XX("Alpha"));
    texture_maps.add("map_refl", tex_map_XX("Metallic"));
    texture_maps.add("map_Ke", tex_map_XX("Emission"));
  }
  tex_map_XX &tex_map_of_type(StringRef map_string);

  std::string name{};
  float Ns{1.0f};
  float3 Ka{0.0f};
  float3 Kd{0.8f, 0.8f, 0.8f};
  float3 Ks{1.0f};
  float3 Ke{0.0f};
  float Ni{1.0f};
  float d{1.0f};
  int illum{0};
  Map<std::string, tex_map_XX> texture_maps;
  /** Only used for Normal Map node: map_Bump. */
  float map_Bump_strength = 0.0f;
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
