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
class OBJImporter {
 private:
  const OBJImportParams &import_params_;
  std::ifstream infile_;
  uint index_offsets[2] = {0, 0};

 public:
  OBJImporter(const OBJImportParams &import_params);

  void parse_and_store(Vector<std::unique_ptr<OBJRawObject>> &list_of_objects);
  void print_obj_data(Vector<std::unique_ptr<OBJRawObject>> &list_of_objects);
  void raw_to_blender_objects(Main *bmain,
                              Scene *scene,
                              Vector<std::unique_ptr<OBJRawObject>> &list_of_objects);
};

}  // namespace blender::io::obj
#endif
