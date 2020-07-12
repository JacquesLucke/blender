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

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <optional>

#include "BKE_collection.h"
#include "BKE_customdata.h"
#include "BKE_object.h"

#include "BLI_array.hh"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_string_ref.hh"

#include "bmesh.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "wavefront_obj_file_handler.hh"
#include "wavefront_obj_importer.hh"

namespace blender::io::obj {
OBJImporter::OBJImporter(const OBJImportParams &import_params) : import_params_(import_params)
{
  infile_.open(import_params_.filepath);
}

void OBJImporter::parse_and_store(Vector<std::unique_ptr<OBJRawObject>> &list_of_objects)
{
  std::string line;
  std::unique_ptr<OBJRawObject> *curr_ob;
  while (std::getline(infile_, line)) {
    std::stringstream s_line(line);
    std::string line_key;
    s_line >> line_key;

    if (line_key == "o") {
      /* Update index offsets if an object has been processed already. */
      if (list_of_objects.size() > 0) {
        index_offsets[VERTEX_OFF] += (*curr_ob)->vertices.size();
        index_offsets[UV_VERTEX_OFF] += (*curr_ob)->texture_vertices.size();
      }
      list_of_objects.append(std::make_unique<OBJRawObject>(s_line.str()));
      curr_ob = &list_of_objects.last();
    }
    /* TODO ankitm Check that an object exists. */
    else if (line_key == "v") {
      MVert curr_vert;
      s_line >> curr_vert.co[0] >> curr_vert.co[1] >> curr_vert.co[2];
      (*curr_ob)->vertices.append(curr_vert);
    }
    else if (line_key == "vn") {
      (*curr_ob)->tot_normals++;
    }
    else if (line_key == "vt") {
      MLoopUV curr_tex_vert;
      s_line >> curr_tex_vert.uv[0] >> curr_tex_vert.uv[1];
      (*curr_ob)->texture_vertices.append(curr_tex_vert);
    }
    else if (line_key == "f") {
      Vector<OBJFaceCorner> curr_face;
      while (s_line) {
        OBJFaceCorner corner;
        if (!(s_line >> corner.vert_index)) {
          break;
        }
        /* Base 1 in OBJ to base 0 in C++. */
        corner.vert_index--;
        /* Adjust for index offset of previous objects. */
        corner.vert_index -= index_offsets[VERTEX_OFF];

        // TODO texture coords handling. It's mostly string manipulation. Normal indices will be
        // ignored and calculated depending on the smooth flag.
        // s_line >> corner.tex_vert_index;
        curr_face.append(corner);
      }
      (*curr_ob)->face_elements.append(curr_face);
      (*curr_ob)->tot_loop += curr_face.size();
    }
    else if (line_key == "usemtl") {
      (*curr_ob)->material_name.append(s_line.str());
    }
    else if (line_key == "#") {
    }
  }
}

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
    for (const Vector<OBJFaceCorner> &curr_face : curr_ob->face_elements) {
      for (OBJFaceCorner a : curr_face) {
        printf("%d ", a.vert_index);
      }
      printf("\n");
    }
    printf("\n");
    for (StringRef b : curr_ob->material_name) {
      printf("%s", b.data());
    }
  }
}

OBJBmeshFromRaw::OBJBmeshFromRaw(const OBJRawObject &curr_object)
{
  auto creator_mesh = [&]() {
    return BKE_mesh_new_nomain(0, 0, 0, curr_object.tot_loop, curr_object.face_elements.size());
  };
  auto creator_bmesh = [&]() {
    BMAllocTemplate bat = {0,
                           0,
                           static_cast<int>(curr_object.tot_loop),
                           static_cast<int>(curr_object.face_elements.size())};
    BMeshCreateParams bcp = {1};
    return BM_mesh_create(&bat, &bcp);
  };
  bm_new_.reset(creator_bmesh());
  struct BMeshFromMeshParams bm_convert_params = {true, 0, 0, 0};
  unique_mesh_ptr template_mesh{creator_mesh()};
  BM_mesh_bm_from_me(bm_new_.get(), template_mesh.get(), &bm_convert_params);
};

BMVert *OBJBmeshFromRaw::add_bmvert(float3 coords)
{
  return BM_vert_create(bm_new_.get(), coords, NULL, BM_CREATE_SKIP_CD);
}

void OBJBmeshFromRaw::add_polygon_from_verts(BMVert **verts_of_face, uint tot_verts_per_poly)
{
  BM_face_create_ngon_verts(
      bm_new_.get(), verts_of_face, tot_verts_per_poly, NULL, BM_CREATE_SKIP_CD, false, true);
}

static unique_mesh_ptr mesh_from_raw_obj(Main *bmain, const OBJRawObject &curr_object)
{

  OBJBmeshFromRaw bm_from_raw{curr_object};

  Array<BMVert *> all_vertices(curr_object.vertices.size());
  for (int i = 0; i < curr_object.vertices.size(); i++) {
    const MVert &curr_vert = curr_object.vertices[i];
    all_vertices[i] = bm_from_raw.add_bmvert(curr_vert.co);
  }

  for (const Vector<OBJFaceCorner> &curr_face : curr_object.face_elements) {
    /* Collect vertices of one face from a pool of BMesh vertices. */
    Array<BMVert *> verts_of_face(curr_face.size());
    for (int i = 0; i < curr_face.size(); i++) {
      verts_of_face[i] = all_vertices[curr_face[i].vert_index];
    }
    bm_from_raw.add_polygon_from_verts(&verts_of_face[0], curr_face.size());
  }

  unique_mesh_ptr bm_to_me{(Mesh *)BKE_id_new_nomain(ID_ME, nullptr)};
  BM_mesh_bm_to_me_for_eval(bm_from_raw.bm_getter(), bm_to_me.get(), nullptr);
  return bm_to_me;
}

OBJParentCollection::OBJParentCollection(Main *bmain, Scene *scene) : bmain_(bmain), scene_(scene)
{
  parent_collection_ = BKE_collection_add(
      bmain_, scene_->master_collection, "OBJ import collection");
}

void OBJParentCollection::add_object_to_parent(const OBJRawObject &ob_to_add, unique_mesh_ptr mesh)
{
  std::unique_ptr<Object> b_object{
      BKE_object_add_only_object(bmain_, OB_MESH, ob_to_add.object_name.c_str())};
  b_object->data = BKE_object_obdata_add_from_type(bmain_, OB_MESH, ob_to_add.object_name.c_str());

  //  BKE_mesh_validate(mesh, false, true);
  BKE_mesh_nomain_to_mesh(
      mesh.release(), (Mesh *)b_object->data, b_object.get(), &CD_MASK_EVERYTHING, true);

  BKE_collection_object_add(bmain_, parent_collection_, b_object.release());
  id_fake_user_set(&parent_collection_->id);

  DEG_id_tag_update(&parent_collection_->id, ID_RECALC_COPY_ON_WRITE);
  DEG_relations_tag_update(bmain_);
}

void OBJImporter::make_objects(Main *bmain,
                               Scene *scene,
                               Vector<std::unique_ptr<OBJRawObject>> &list_of_objects)
{
  OBJParentCollection parent{bmain, scene};
  for (std::unique_ptr<OBJRawObject> &curr_object : list_of_objects) {
    unique_mesh_ptr mesh{mesh_from_raw_obj(bmain, *curr_object)};

    parent.add_object_to_parent(*curr_object, std::move(mesh));
  }
}

void importer_main(bContext *C, const OBJImportParams &import_params)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Vector<std::unique_ptr<OBJRawObject>> list_of_objects;
  OBJImporter importer = OBJImporter(import_params);
  importer.parse_and_store(list_of_objects);
  //  importer.print_obj_data(list_of_objects);
  importer.make_objects(bmain, scene, list_of_objects);
}
}  // namespace blender::io::obj
