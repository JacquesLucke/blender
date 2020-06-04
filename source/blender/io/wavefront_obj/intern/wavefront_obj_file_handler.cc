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

#include <array>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <vector>

#include "wavefront_obj_file_handler.h"

#include "DNA_meshdata_types.h"

void write_prepared_data(const char *filepath, OBJ_data_to_export *data_to_export)
{
  std::ofstream outfile(filepath, std::ios::binary);
  outfile << "# Blender 2.90 \n";
  for (int i = 0; i < data_to_export->tot_vertices; i++) {
    outfile << "v ";
    outfile << data_to_export->vertices[i].co[0] << " " << data_to_export->vertices[i].co[1] << " "
            << data_to_export->vertices[i].co[2] << "\n";
  }
  for (int i = 0; i < data_to_export->tot_faces; i++) {
    outfile << "vn ";
    outfile << data_to_export->normals[i][0] << " " << data_to_export->normals[i][1] << " "
            << data_to_export->normals[i][2] << "\n";
  }
  for (int i = 0; i < data_to_export->tot_faces; i++) {
    outfile << "f ";
    for (int j = 0; j < data_to_export->faces_list[i].total_vertices_per_face; j++) {
      outfile << data_to_export->faces_list[i].vertex_references[j] << "//"
              << data_to_export->faces_list[i].vertex_normal_references[j] << " ";
    }
    outfile << "\n";
  }
}
