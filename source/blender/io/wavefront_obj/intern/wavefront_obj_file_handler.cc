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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup obj
 */

#include <fstream>
#include <iostream>

#include "wavefront_obj.hh"
#include "wavefront_obj_file_handler.hh"

namespace IO {
namespace OBJ {
/**
 * File writer to the OBJ file at filepath.
 * data_to_export is filled in obj_exporter.cc.
 */
void write_obj_data(const char *filepath, OBJ_data_to_export *data_to_export)
{
  std::ofstream outfile(filepath, std::ios::binary);
  outfile << "# Blender 2.90 \n";

  /** Write v x y z for all vertices. */
  for (int i = 0; i < data_to_export->tot_vertices; i++) {
    outfile << "v ";
    outfile << data_to_export->mvert[i].co[0] << " " << data_to_export->mvert[i].co[1] << " "
            << data_to_export->mvert[i].co[2] << "\n";
  }

  /** Write vn n1 n2 n3 for all face normals. */
  for (int i = 0; i < data_to_export->tot_faces; i++) {
    outfile << "vn ";
    outfile << data_to_export->mvert[i].no[0] << " " << data_to_export->mvert[i].no[1] << " "
            << data_to_export->mvert[i].no[2] << "\n";
  }

  /** Write f v1/vt1/vn1 .. total_vertices_per_poly , for all polygons. */
  for (int i = 0; i < data_to_export->tot_faces; i++) {
    outfile << "f ";
    for (int j = 0; j < data_to_export->polygon_list[i].total_vertices_per_poly; j++) {
      outfile << data_to_export->polygon_list[i].vertex_index[j] << "//"
              << data_to_export->polygon_list[i].face_normal_index[j] << " ";
    }
    outfile << "\n";
  }
}

}  // namespace OBJ
}  // namespace IO
