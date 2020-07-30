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

#ifndef __WAVEFRONT_OBJ_IM_FILE_READER_HH__
#define __WAVEFRONT_OBJ_IM_FILE_READER_HH__

#include "IO_wavefront_obj.h"
#include "wavefront_obj_im_objects.hh"

namespace blender::io::obj {

class OBJParser {
 private:
  const OBJImportParams &import_params_;
  std::ifstream obj_file_;
  /**
   * These two numbers VERTEX_OFF and UV_VERTEX_OFF respectively keep track of how many vertices
   * have been occupied by other objects. It is used when an index must stay local to an object,
   * not index into the global vertices list.
   */
  int index_offsets_[2] = {0, 0};

 public:
  OBJParser(const OBJImportParams &import_params);

  void parse_and_store(Vector<std::unique_ptr<OBJRawObject>> &list_of_objects,
                       GlobalVertices &global_vertices);
  void print_obj_data(Span<std::unique_ptr<OBJRawObject>> list_of_objects,
                      const GlobalVertices &global_vertices);

 private:
  void update_index_offsets(std::unique_ptr<OBJRawObject> *curr_ob);
};

}  // namespace blender::io::obj
#endif
