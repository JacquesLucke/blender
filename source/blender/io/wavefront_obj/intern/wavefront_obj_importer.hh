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

#ifndef __WAVEFRONT_OBJ_IMPORTER_HH__
#define __WAVEFRONT_OBJ_IMPORTER_HH__

#include <fstream>
#include <iostream>

#include "BKE_context.h"

#include "bmesh.h"

#include "BLI_float3.hh"
#include "BLI_float2.hh"
#include "BLI_string_ref.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

#include "DNA_meshdata_types.h"
#include "IO_wavefront_obj.h"

namespace blender::io::obj {

typedef struct OBJFaceCorner {
  int vert_index;
  int tex_vert_index = -1;
} OBJFaceCorner;

class OBJRawObject {
 public:
  OBJRawObject(StringRef ob_name) : object_name(ob_name.data()){};

  std::string object_name;
  Vector<MVert> vertices;
  Vector<MLoopUV> texture_vertices;
  Vector<Vector<OBJFaceCorner>> face_elements;
  uint tot_normals = 0;
  uint tot_loop = 0;
  bool is_shaded_smooth;
  Vector<std::string> material_name;
};

class OBJParentCollection {
 public:
  OBJParentCollection(Main *bmain, Scene *scene);
  void add_object_to_parent(OBJRawObject &ob_to_add, std::unique_ptr<Mesh> mesh);

 private:
  Main *bmain_;
  Scene *scene_;
  Collection *parent_collection_;
};

class OBJRawToBmesh : NonMovable, NonCopyable {
 public:
  OBJRawToBmesh(OBJRawObject &curr_object);
  ~OBJRawToBmesh();
  BMesh *getter_bmesh()
  {
    return bm_new_.get();
  }

 private:
  std::unique_ptr<Mesh> template_mesh_;
  std::unique_ptr<BMesh> bm_new_;
};

class OBJImporter {
 private:
  const OBJImportParams &import_params_;
  std::ifstream infile_;
  uint index_offsets[2] = {0, 0};

 public:
  OBJImporter(const OBJImportParams &import_params);

  void parse_and_store(Vector<std::unique_ptr<OBJRawObject>> &list_of_objects);
  void print_obj_data(Vector<std::unique_ptr<OBJRawObject>> &list_of_objects);
  void make_objects(Main *bmain,
                    Scene *scene,
                    Vector<std::unique_ptr<OBJRawObject>> &list_of_objects);
};

void importer_main(bContext *C, const OBJImportParams &import_params);
}  // namespace blender::io::obj

#endif
