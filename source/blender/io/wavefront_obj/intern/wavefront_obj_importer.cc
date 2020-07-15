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

#include <fstream>
#include <iostream>

#include "BLI_array.hh"
#include "BLI_float2.hh"
#include "BLI_float3.hh"
#include "BLI_string.h"
#include "BLI_string_ref.hh"

#include "bmesh.h"

#include "wavefront_obj_im_file_reader.hh"
#include "wavefront_obj_im_mesh.hh"
#include "wavefront_obj_im_objects.hh"
#include "wavefront_obj_importer.hh"

namespace blender::io::obj {

void OBJImporter::print_obj_data(Vector<std::unique_ptr<OBJRawObject>> &list_of_objects)
{
  for (std::unique_ptr<OBJRawObject> &curr_ob : list_of_objects) {
    for (const MVert &curr_vert : curr_ob->vertices) {
      print_v3("vert", curr_vert.co);
    }
    printf("\n");
    for (const MLoopUV &curr_tex_vert : curr_ob->texture_vertices) {
      print_v2("tex vert", curr_tex_vert.uv);
    }
    printf("\n");
    for (const OBJFaceElem &curr_face : curr_ob->face_elements) {
      for (OBJFaceCorner a : curr_face.face_corners) {
        printf("%d %d ", a.vert_index, a.tex_vert_index);
      }
      printf("\n");
    }
    printf("\n");
    for (StringRef b : curr_ob->material_name) {
      printf("%s", b.data());
    }
  }
}

void OBJImporter::raw_to_blender_objects(Main *bmain,
                                         Scene *scene,
                                         Vector<std::unique_ptr<OBJRawObject>> &list_of_objects)
{
  OBJParentCollection parent{bmain, scene};
  for (std::unique_ptr<OBJRawObject> &curr_object : list_of_objects) {
    OBJMeshFromRaw mesh_from_raw{*curr_object};
    parent.add_object_to_parent(curr_object->object_name, mesh_from_raw.mover());
  }
}

void importer_main(bContext *C, const OBJImportParams &import_params)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Vector<std::unique_ptr<OBJRawObject>> list_of_objects;
  OBJImporter importer{import_params};

  importer.parse_and_store(list_of_objects);
  //  importer.print_obj_data(list_of_objects);
  importer.raw_to_blender_objects(bmain, scene, list_of_objects);
}
}  // namespace blender::io::obj
