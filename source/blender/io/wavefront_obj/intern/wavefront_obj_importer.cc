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
#include "wavefront_obj_im_nurbs.hh"
#include "wavefront_obj_im_objects.hh"
#include "wavefront_obj_importer.hh"

namespace blender::io::obj {

/**
 * Only for debug purposes. Must not be in master.
 */
static void print_obj_data(Vector<std::unique_ptr<OBJRawObject>> &list_of_objects,
                           const GlobalVertices &global_vertices)
{
  for (const float3 &curr_vert : global_vertices.vertices) {
    print_v3("vert", curr_vert);
  }
  printf("\n");
  for (const float2 &curr_uv_vert : global_vertices.uv_vertices) {
    print_v2("vert", curr_uv_vert);
  }
  printf("\n");

  for (const auto &curr_ob : list_of_objects) {
    for (const int &curr_vert_idx : curr_ob->vertex_indices) {
      printf(" %d", curr_vert_idx);
    }
    printf("\nglobal_vert_index^\n");
    for (const int &curr_uv_vert_idx : curr_ob->uv_vertex_indices) {
      printf(" %d", curr_uv_vert_idx);
    }
    printf("\nglobal_uv_vert_index^\n");
    for (const OBJFaceElem &curr_face : curr_ob->face_elements) {
      for (OBJFaceCorner a : curr_face.face_corners) {
        printf(" %d/%d", a.vert_index, a.uv_vert_index);
      }
      printf("\n");
    }
    printf("\nvert_index/uv_vert_index^\n");
    for (StringRef b : curr_ob->material_name) {
      printf("%s ", b.data());
    }
    printf("\nmat names^\n");
    for (const int &t : curr_ob->nurbs_element.curv_indices) {
      printf(" %d", t);
    }
    printf("\nnurbs curv indces^\n");
    for (const float &t : curr_ob->nurbs_element.parm) {
      printf(" %f", t);
    }
    printf("\nnurbs parm values^\n");
  }
}

static void raw_to_blender_objects(Main *bmain,
                                   Scene *scene,
                                   Vector<std::unique_ptr<OBJRawObject>> &list_of_objects,
                                   const GlobalVertices global_vertices)
{
  OBJParentCollection parent{bmain, scene};
  for (std::unique_ptr<OBJRawObject> &curr_object : list_of_objects) {
    if (curr_object->object_type & OB_MESH) {
      OBJMeshFromRaw mesh_from_raw{*curr_object, global_vertices};
      parent.add_object_to_parent(curr_object.get(), mesh_from_raw.mover());
    }
    else if (curr_object->object_type & (OB_CURVE | CU_NURBS)) {
      OBJCurveFromRaw nurbs_from_raw(bmain, *curr_object, global_vertices);
      parent.add_object_to_parent(curr_object.get(), nurbs_from_raw.mover());
    }
  }
}

void importer_main(bContext *C, const OBJImportParams &import_params)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Vector<std::unique_ptr<OBJRawObject>> list_of_objects;
  OBJImporter importer{import_params};
  GlobalVertices global_vertices;

  importer.parse_and_store(list_of_objects, global_vertices);

  //  print_obj_data(list_of_objects, global_vertices);
  raw_to_blender_objects(bmain, scene, list_of_objects, global_vertices);
}
}  // namespace blender::io::obj
