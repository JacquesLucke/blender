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

#include "DNA_object_types.h"

#include "wavefront_obj.hh"
#include "wavefront_obj_file_handler.hh"

namespace io {
namespace obj {

/**
 * Calculate a face normal's axis component by averaging over its vertex normals.
 */
MALWAYS_INLINE short face_normal_axis_component(const Polygon &poly_to_write,
                                                int axis,
                                                MVert *vertex_list)
{
  float sum = 0;
  for (int i = 0; i < poly_to_write.total_vertices_per_poly; i++) {
    sum += vertex_list[poly_to_write.vertex_index[i] - 1].no[axis];
  }
  return short(sum / poly_to_write.total_vertices_per_poly);
}

/**
 * Low level writer to the OBJ file at filepath.
 */
void write_obj_data(const char *filepath, OBJ_data_to_export *data_to_export)
{
  std::ofstream outfile(filepath, std::ios::binary | std::ios::trunc);
  if (outfile.is_open() == false) {
    printf("Error opening file.");
    return;
  }

  outfile << "# Blender 2.90 \n";

  /** Write object name, as seen in outliner. First two characters are ID code, so skipped. */
  outfile << "o " << data_to_export->ob_eval->id.name + 2 << "\n";

  /** Write v x y z for all vertices. */
  for (int i = 0; i < data_to_export->tot_vertices; i++) {
    MVert *vertex = &data_to_export->mvert[i];
    outfile << "v ";
    outfile << vertex->co[0] << " " << vertex->co[1] << " " << vertex->co[2] << "\n";
  }

  /** Write vn nx ny nz for all face normals. */
  for (int i = 0; i < data_to_export->tot_poly; i++) {
    MVert *vertex_list = data_to_export->mvert;
    const Polygon &polygon = data_to_export->polygon_list[i];
    outfile << "vn " << face_normal_axis_component(polygon, 0, vertex_list) << " "
            << face_normal_axis_component(polygon, 1, vertex_list) << " "
            << face_normal_axis_component(polygon, 2, vertex_list) << "\n";
  }

  /**
   * Write f v1/vt1/vn1 .. total_vertices_per_poly , for all polygons.
   * i-th vn is always i + 1, guaranteed by face normal loop above.
   * Both loop over the same polygon list.
   */
  for (int i = 0; i < data_to_export->tot_poly; i++) {
    const Polygon &polygon = data_to_export->polygon_list[i];
    outfile << "f ";
    for (int j = 0; j < polygon.total_vertices_per_poly; j++) {
      outfile << polygon.vertex_index[j] << "//" << i + 1 << " ";
    }
    outfile << "\n";
  }
}

/**
 * Same functionality as write_obj_data except it uses fprintf to write
 * to the file.
 */
void write_obj_data_fprintf(const char *filepath, OBJ_data_to_export *data_to_export)
{
  FILE *outfile = fopen(filepath, "w");
  if (outfile == NULL) {
    printf("Error in creating the file\n");
    return;
  }

  fprintf(outfile, "# Blender 2.90\n");
  fprintf(outfile, "o %s\n", data_to_export->ob_eval->id.name + 2);

  for (int i = 0; i < data_to_export->tot_vertices; i++) {
    MVert *vertex = &data_to_export->mvert[i];
    fprintf(outfile, "v ");
    fprintf(outfile, "%f ", vertex->co[0]);
    fprintf(outfile, "%f ", vertex->co[1]);
    fprintf(outfile, "%f\n", vertex->co[2]);
  }

  for (int i = 0; i < data_to_export->tot_poly; i++) {
    MVert *vertex_list = data_to_export->mvert;
    const Polygon &polygon = data_to_export->polygon_list[i];
    fprintf(outfile, "vn ");
    fprintf(outfile, "%hd ", face_normal_axis_component(polygon, 0, vertex_list));
    fprintf(outfile, "%hd ", face_normal_axis_component(polygon, 1, vertex_list));
    fprintf(outfile, "%hd \n", face_normal_axis_component(polygon, 2, vertex_list));
  }

  for (int i = 0; i < data_to_export->tot_poly; i++) {
    const Polygon &polygon = data_to_export->polygon_list[i];
    fprintf(outfile, "f ");
    for (int j = 0; j < polygon.total_vertices_per_poly; j++) {
      fprintf(outfile, "%d//%d ", polygon.vertex_index[j], i + 1);
    }
    fprintf(outfile, "\n");
  }
  fclose(outfile);
}

}  // namespace obj
}  // namespace io
