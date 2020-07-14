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

#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "wavefront_obj_ex_file_writer.hh"
#include "wavefront_obj_im_file_reader.hh"

namespace blender::io::obj {

using std::string;

OBJImporter::OBJImporter(const OBJImportParams &import_params) : import_params_(import_params)
{
  infile_.open(import_params_.filepath);
}

static void split_by_char(string in_string, char delimiter, Vector<string> &r_out_list)
{
  std::stringstream stream(in_string);
  string word{};
  while (std::getline(stream, word, delimiter)) {
    if (word == "" || word[0] == ' ' || word[0] == '\0') {
      continue;
    }
    r_out_list.append(word);
  }
}

static string first_word_of_string(string in_string)
{
  size_t pos = in_string.find_first_of(' ');
  return pos == string::npos ? in_string.substr(0, 1) : in_string.substr(0, pos);
}

MALWAYS_INLINE void copy_string_to_float(float *dst, Span<string> src)
{
  dst[0] = std::stof(src[0]);
  dst[1] = std::stof(src[1]);
  if (src.size() == 3) {
    dst[2] = std::stof(src[2]);
  }
}

void OBJImporter::parse_and_store(Vector<std::unique_ptr<OBJRawObject>> &list_of_objects)
{
  string line;
  /* Non owning raw pointer to the unique_ptr to a raw object.
   * Needed to update object data in the same while loop.
   * TODO ankitm Try to move the rest of the data parsing code in a conditional depending on a
   * valid "o" object. */
  std::unique_ptr<OBJRawObject> *curr_ob;
  while (std::getline(infile_, line)) {
    string line_key = first_word_of_string(line);
    std::stringstream s_line(line.substr(line_key.size()));

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
      Vector<string> str_vert;
      split_by_char(s_line.str(), ' ', str_vert);
      copy_string_to_float(curr_vert.co, str_vert);
      (*curr_ob)->vertices.append(curr_vert);
    }
    else if (line_key == "vn") {
      (*curr_ob)->tot_normals++;
    }
    else if (line_key == "vt") {
      MLoopUV curr_tex_vert;
      Vector<string> str_vert;
      split_by_char(s_line.str(), ' ', str_vert);
      copy_string_to_float(curr_tex_vert.uv, str_vert);
      (*curr_ob)->texture_vertices.append(curr_tex_vert);
    }
    else if (line_key == "f") {
      OBJFaceElem curr_face;
      string str_corners_line = s_line.str();
      Vector<string> str_corners_split;
      split_by_char(str_corners_line, ' ', str_corners_split);
      for (auto str_corner : str_corners_split) {
        OBJFaceCorner corner;
        size_t n_slash = std::count(str_corner.begin(), str_corner.end(), '/');
        if (n_slash == 0) {
          corner.vert_index = std::stoi(str_corner);
        }
        else if (n_slash == 1) {
          Vector<std::string> vert_texture;
          split_by_char(str_corner, '/', vert_texture);
          corner.vert_index = std::stoi(vert_texture[0]);
          corner.tex_vert_index = vert_texture.size() >= 1 ? -1 : std::stoi(vert_texture[1]);
        }
        else if (n_slash == 2) {
          Vector<std::string> vert_normal;
          split_by_char(str_corner, '/', vert_normal);
          corner.vert_index = std::stoi(vert_normal[0]);
          /* Discard normals. They'll be calculated on the basis of smooth shading flag. */
        }
        corner.vert_index -= index_offsets[VERTEX_OFF] + 1;
        corner.tex_vert_index -= index_offsets[UV_VERTEX_OFF] + 1;

        curr_face.face_corners.append(corner);
      }

      (*curr_ob)->face_elements.append(curr_face);
      (*curr_ob)->tot_loop += curr_face.face_corners.size();
    }
    else if (line_key == "usemtl") {
      (*curr_ob)->material_name.append(s_line.str());
    }
    else if (line_key == "#") {
    }
  }
}
}  // namespace blender::io::obj
