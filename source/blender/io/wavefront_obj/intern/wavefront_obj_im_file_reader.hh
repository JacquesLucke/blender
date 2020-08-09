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

#include "IO_wavefront_obj.h"
#include "wavefront_obj_im_objects.hh"

namespace blender::io::obj {

class OBJParser {
 private:
  const OBJImportParams &import_params_;
  std::ifstream obj_file_;
  Vector<std::string> mtl_libraries_{};

 public:
  OBJParser(const OBJImportParams &import_params);

  void parse_and_store(Vector<std::unique_ptr<Geometry>> &all_geometries,
                       GlobalVertices &global_vertices);
  Span<std::string> mtl_libraries() const;
  void print_obj_data(Span<std::unique_ptr<Geometry>> all_geometries,
                      const GlobalVertices &global_vertices);
};

class MTLParser {
 private:
  StringRef mtl_library_;
  char mtl_file_path_[FILE_MAX]{};
  std::ifstream mtl_file_;

 public:
  MTLParser(StringRef mtl_library_, StringRefNull obj_filepath);

  void parse_and_store(Map<std::string, MTLMaterial> &mtl_materials);
};
}  // namespace blender::io::obj
