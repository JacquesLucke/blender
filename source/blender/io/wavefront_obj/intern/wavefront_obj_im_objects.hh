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

#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "DNA_collection_types.h"
#include "DNA_meshdata_types.h"

#include "wavefront_obj_im_mesh.hh"

namespace blender::io::obj {
typedef struct OBJFaceCorner {
  int vert_index;
  /* -1 is to indicate abscense of UV vertices. Only < 0 condition should be checked since
   * it can be less than -1 too. */
  int tex_vert_index = -1;
} OBJFaceCorner;

typedef struct OBJFaceElem {
  bool shaded_smooth = false;
  Vector<OBJFaceCorner> face_corners;
} OBJFaceElem;

class OBJRawObject {
 public:
  OBJRawObject(StringRef ob_name) : object_name(ob_name.data()){};

  std::string object_name;
  Vector<MVert> vertices{};
  Vector<MLoopUV> texture_vertices{};
  Vector<OBJFaceElem> face_elements{};
  uint tot_normals = 0;
  uint tot_loop = 0;
  uint tot_uv_verts = 0;
  Vector<std::string> material_name{};
};

class OBJParentCollection {
 private:
  Main *bmain_;
  Scene *scene_;
  Collection *parent_collection_;

 public:
  OBJParentCollection(Main *bmain, Scene *scene);

  void add_object_to_parent(StringRef ob_to_add_name, unique_mesh_ptr mesh);
};
}  // namespace blender::io::obj

#endif
