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

namespace io {
namespace obj {

/**
 * Low level writer to the OBJ file at filepath.
 * data_to_export is filled in obj_exporter.cc.
 */
void write_obj_data(const char *filepath, OBJ_data_to_export *data_to_export)
{
  std::ofstream outfile(filepath, std::ios::binary);
  outfile << "# Blender 2.90 \n";

  /** Write v x y z for all vertices. */
  for (int i = 0; i < data_to_export->tot_vertices; i++) {
    MVert *vertex = &data_to_export->mvert[i];
    outfile << "v ";
    outfile << vertex[i].co[0] << " " << vertex[i].co[1] << " " << vertex[i].co[2] << "\n";
  }

  /** Write vn nx ny nz for all face normals. */
  for (int i = 0; i < data_to_export->tot_faces; i++) {
    MVert *vertex = &data_to_export->mvert[i];
    outfile << "vn ";
    outfile << vertex[i].no[0] << " " << vertex[i].no[1] << " " << vertex[i].no[2] << "\n";
  }

  /** Write f v1/vt1/vn1 .. total_vertices_per_poly , for all polygons. */
  for (int i = 0; i < data_to_export->tot_faces; i++) {
    const Polygon &polygon = data_to_export->polygon_list[i];
    outfile << "f ";
    for (int j = 0; j < polygon.total_vertices_per_poly; j++) {
      outfile << polygon.vertex_index[j] << "//" << polygon.face_normal_index[j] << " ";
    }
    outfile << "\n";
  }
}

}  // namespace obj
}  // namespace io
