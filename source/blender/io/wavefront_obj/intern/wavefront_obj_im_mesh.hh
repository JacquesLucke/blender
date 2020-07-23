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

#ifndef __WAVEFRONT_OBJ_IM_MESH_HH__
#define __WAVEFRONT_OBJ_IM_MESH_HH__

#include "BKE_lib_id.h"
#include "BKE_mesh.h"

#include "BLI_float3.hh"
#include "BLI_utility_mixins.hh"

#include "bmesh.h"

namespace blender::io::obj {
class OBJRawObject;
struct GlobalVertices;
/**
 * An custom unique_ptr deleter for a Mesh object.
 */
struct UniqueMeshDeleter {
  void operator()(Mesh *mesh)
  {
    BKE_id_free(nullptr, mesh);
  }
};

/**
 * An unique_ptr to a Mesh with a custom deleter.
 */
using unique_mesh_ptr = std::unique_ptr<Mesh, UniqueMeshDeleter>;

class OBJMeshFromRaw : NonMovable, NonCopyable {
 private:
  unique_mesh_ptr mesh_from_ob_;

 public:
  OBJMeshFromRaw(const OBJRawObject &curr_object, const GlobalVertices global_vertices);

  unique_mesh_ptr mover()
  {
    return std::move(mesh_from_ob_);
  }

 private:
  void create_vertices(const OBJRawObject &curr_object,
                       const GlobalVertices &global_vertices,
                       int64_t tot_verts_object);
  void create_loops(const OBJRawObject &curr_object, int64_t tot_face_elems);
  void create_edges(const OBJRawObject &curr_object, int64_t tot_edges);
  void create_uv_verts(const OBJRawObject &curr_object, const GlobalVertices &global_vertices);
};

}  // namespace blender::io::obj

#endif
