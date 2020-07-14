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

struct UniqueMeshDeleter {
  void operator()(Mesh *mesh)
  {
    BKE_id_free(nullptr, mesh);
  }
};

struct UniqueBMeshDeleter {
  void operator()(BMesh *t)
  {
    BM_mesh_free(t);
  }
};

using unique_mesh_ptr = std::unique_ptr<Mesh, UniqueMeshDeleter>;
using unique_bmesh_ptr = std::unique_ptr<BMesh, UniqueBMeshDeleter>;

class OBJMeshFromRaw : NonMovable, NonCopyable {
 private:
  unique_mesh_ptr mesh_from_bm_;

 public:
  OBJMeshFromRaw(const class OBJRawObject &curr_object);

  unique_mesh_ptr mover()
  {
    return std::move(mesh_from_bm_);
  }
};

}  // namespace blender::io::obj

#endif
