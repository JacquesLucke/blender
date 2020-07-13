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

#include "BKE_context.h"

#include "BLI_vector.hh"
#include "BLI_string_ref.hh"

#include "wavefront_obj_file_reader.hh"
#include "wavefront_obj_file_handler.hh"

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
}
