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

#include "BKE_blender_version.h"

#include "BLI_math_inline.h"

#include "DNA_object_types.h"

#include "wavefront_obj.hh"
#include "wavefront_obj_file_handler.hh"

namespace io {
namespace obj {

/**
 * Calculate a face normal by averaging over its vertex normals.
 */
MALWAYS_INLINE void face_no_from_vert_no(const Polygon &poly_to_write,
                                         short *face_no,
                                         MVert *vertex_list)
{
  /* Per axis sum of normals. */
  float sum[3] = {0, 0, 0};
  for (uint i = 0; i < poly_to_write.total_vertices_per_poly; i++) {
    sum[0] += vertex_list[poly_to_write.vertex_index[i] - 1].no[0];
    sum[1] += vertex_list[poly_to_write.vertex_index[i] - 1].no[1];
    sum[2] += vertex_list[poly_to_write.vertex_index[i] - 1].no[2];
  }
  /* Averaging the face normal. */
  face_no[0] = (short)sum[0] / poly_to_write.total_vertices_per_poly;
  face_no[1] = (short)sum[1] / poly_to_write.total_vertices_per_poly;
  face_no[2] = (short)sum[2] / poly_to_write.total_vertices_per_poly;
}

static void write_geomtery_per_mesh(FILE *outfile,
                                    const OBJ_obmesh_to_export &ob_mesh,
                                    const uint offset[3],
                                    const OBJExportParams *export_params)
{
  /** Write object name, as seen in outliner. */
  fprintf(outfile, "o %s\n", ob_mesh.object->id.name + 2);

  /** Write v x y z for all vertices. */
  for (uint i = 0; i < ob_mesh.tot_vertices; i++) {
    const MVert &vertex = ob_mesh.mvert[i];
    fprintf(outfile, "v %f %f %f\n", vertex.co[0], vertex.co[1], vertex.co[2]);
  }

  /**
   * Write texture coordinates, vt u v for all vertices in a object's texture space.
   */
  if (export_params->export_uv) {
    for (uint i = 0; i < ob_mesh.tot_uv_vertices; i++) {
      const std::array<float, 2> &uv_vertex = ob_mesh.uv_coords[i];
      fprintf(outfile, "vt %f %f\n", uv_vertex[0], uv_vertex[1]);
    }
  }

  /** Write vn nx ny nz for all face normals. */
  if (export_params->export_normals) {
    for (uint i = 0; i < ob_mesh.tot_poly; i++) {
      MVert *vertex_list = ob_mesh.mvert;
      const Polygon &polygon = ob_mesh.polygon_list[i];
      short face_no[3];
      face_no_from_vert_no(polygon, face_no, vertex_list),
          fprintf(outfile, "vn %hd %hd %d\n", face_no[0], face_no[1], face_no[2]);
    }
  }

  /**
   * Write f v1/vt1/vn1 .. total_vertices_per_poly , for all polygons.
   * i-th vn is always i + 1, guaranteed by face normal loop above.
   * Both loop over the same polygon list.
   */
  if (export_params->export_normals) {
    if (export_params->export_uv) {
      /* Write both normals and UV. f v1/vt1/vn1 */
      for (uint i = 0; i < ob_mesh.tot_poly; i++) {
        const Polygon &polygon = ob_mesh.polygon_list[i];
        fprintf(outfile, "f ");
        for (int j = 0; j < polygon.total_vertices_per_poly; j++) {
          fprintf(outfile,
                  "%d/%d/%d ",
                  polygon.vertex_index[j] + offset[0],
                  polygon.uv_vertex_index[j] + 1 + offset[1],
                  i + 1 + offset[2]);
        }
        fprintf(outfile, "\n");
      }
    }
    else {
      /* Write normals but not UV. f v1//vn1 */
      for (uint i = 0; i < ob_mesh.tot_poly; i++) {
        const Polygon &polygon = ob_mesh.polygon_list[i];
        fprintf(outfile, "f ");
        for (int j = 0; j < polygon.total_vertices_per_poly; j++) {
          fprintf(outfile, "%d//%d ", polygon.vertex_index[j] + offset[0], i + 1 + offset[2]);
        }
        fprintf(outfile, "\n");
      }
    }
  }
  else {
    if (export_params->export_uv) {
      /* Write UV but not normals. f v1/vt1 */
      for (uint i = 0; i < ob_mesh.tot_poly; i++) {
        const Polygon &polygon = ob_mesh.polygon_list[i];
        fprintf(outfile, "f ");
        for (int j = 0; j < polygon.total_vertices_per_poly; j++) {
          fprintf(outfile,
                  "%d/%d ",
                  polygon.vertex_index[j] + offset[0],
                  polygon.uv_vertex_index[j] + 1 + offset[1]);
        }
        fprintf(outfile, "\n");
      }
    }
    else {
      /* Write neither normals nor UV. f v1 */
      for (uint i = 0; i < ob_mesh.tot_poly; i++) {
        const Polygon &polygon = ob_mesh.polygon_list[i];
        fprintf(outfile, "f ");
        for (int j = 0; j < polygon.total_vertices_per_poly; j++) {
          fprintf(outfile, "%d ", polygon.vertex_index[j] + offset[0]);
        }
        fprintf(outfile, "\n");
      }
    }
  }
}

/**
 * Low level writer to the OBJ file at filepath.
 */
void write_mesh_objects(const char *filepath,
                        const std::vector<OBJ_obmesh_to_export> &meshes_to_export,
                        const OBJExportParams *export_params)
{
  FILE *outfile = fopen(filepath, "w");
  if (outfile == NULL) {
    printf("Error in creating the file: %s\n", filepath);
    return;
  }

  /**
   * index_offset[x]: All previous vertex, UV vertex and normal indices are added in subsequent
   * objects' indices.
   */
  uint index_offset[3] = {0, 0, 0};

  fprintf(outfile, "# Blender %s\n", BKE_blender_version_string());
  for (uint i = 0; i < meshes_to_export.size(); i++) {
    write_geomtery_per_mesh(outfile, meshes_to_export[i], index_offset, export_params);
    index_offset[0] += meshes_to_export[i].tot_vertices;
    index_offset[1] += meshes_to_export[i].tot_uv_vertices;
    index_offset[2] += meshes_to_export[i].tot_poly;
  }
  fclose(outfile);
}

}  // namespace obj
}  // namespace io
