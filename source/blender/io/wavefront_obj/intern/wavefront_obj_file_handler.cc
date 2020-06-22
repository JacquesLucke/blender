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

/** Write one line of polygon indices as f v1/vt1/vn1 v2/vt2/vn2 .... */
void OBJWriter::write_vert_uv_normal_indices(blender::Vector<uint> &vert_indices,
                                             blender::Vector<uint> &uv_indices,
                                             blender::Vector<uint> &normal_indices,
                                             const MPoly &poly_to_write)
{
  fprintf(outfile, "f ");
  for (int j = 0; j < poly_to_write.totloop; j++) {
    fprintf(outfile,
            "%d/%d/%d ",
            vert_indices[j] + index_offset[vertex_off],
            uv_indices[j] + index_offset[uv_vertex_off],
            normal_indices[j] + index_offset[normal_off]);
  }
  fprintf(outfile, "\n");
}

/** Write one line of polygon indices as f v1//vn1 v2//vn2 .... */
void OBJWriter::write_vert_normal_indices(blender::Vector<uint> &vert_indices,
                                          blender::Vector<uint> &normal_indices,
                                          const MPoly &poly_to_write)
{
  fprintf(outfile, "f ");
  for (int j = 0; j < poly_to_write.totloop; j++) {
    fprintf(outfile,
            "%d//%d ",
            vert_indices[j] + index_offset[vertex_off],
            normal_indices[j] + index_offset[normal_off]);
  }
  fprintf(outfile, "\n");
}

/** Write one line of polygon indices as f v1/vt1 v2/vt2 .... */
void OBJWriter::write_vert_uv_indices(blender::Vector<uint> &vert_indices,
                                      blender::Vector<uint> &uv_indices,
                                      const MPoly &poly_to_write)
{
  fprintf(outfile, "f ");
  for (int j = 0; j < poly_to_write.totloop; j++) {
    fprintf(outfile,
            "%d/%d ",
            vert_indices[j] + index_offset[vertex_off],
            uv_indices[j] + 1 + index_offset[uv_vertex_off]);
  }
  fprintf(outfile, "\n");
}

/** Write one line of polygon indices as f v1 v2 .... */
void OBJWriter::write_vert_indices(blender::Vector<uint> &vert_indices, const MPoly &poly_to_write)
{
  fprintf(outfile, "f ");
  for (int j = 0; j < poly_to_write.totloop; j++) {
    fprintf(outfile, "%d ", vert_indices[j] + index_offset[vertex_off]);
  }
  fprintf(outfile, "\n");
}

/** Try to open an empty OBJ file at filepath. */
bool OBJWriter::open_file(const char filepath[FILE_MAX])
{
  outfile = fopen(filepath, "w");
  if (!outfile) {
    return false;
  }
  return true;
}

/** Close the OBJ file. */
void OBJWriter::close_file()
{
  fclose(outfile);
}

/** Write Blender version as a comment in the file. */
void OBJWriter::write_header()
{
  fprintf(outfile, "# Blender %s\n", BKE_blender_version_string());
}

/** Write object name as it appears in the outliner. */
void OBJWriter::write_object_name(OBJMesh &ob_mesh)
{
  fprintf(outfile, "o %s\n", ob_mesh.object->id.name + 2);
}

/** Write vertex coordinates for all vertices as v x y z */
void OBJWriter::write_vertex_coords(OBJMesh &ob_mesh)
{
  ob_mesh.tot_vertices = ob_mesh.me_eval->totvert;
  float vertex[3];
  for (uint i = 0; i < ob_mesh.tot_vertices; i++) {
    ob_mesh.calc_vertex_coords(vertex, i);
    fprintf(outfile, "v %f %f %f\n", vertex[0], vertex[1], vertex[2]);
  }
}

/** Write UV vertex coordinates for all vertices as vt u v
 * \note UV indices are stored here, but written later.
 */
void OBJWriter::write_uv_coords(OBJMesh &ob_mesh, blender::Vector<blender::Vector<uint>> &uv_indices)
{
  blender::Vector<std::array<float, 2>> uv_coords;

  ob_mesh.store_uv_coords_and_indices(uv_coords, uv_indices);
  for (uint i = 0; i < ob_mesh.tot_uv_vertices; i++) {
    const std::array<float, 2> &uv_vertex = uv_coords[i];
    fprintf(outfile, "vt %f %f\n", uv_vertex[0], uv_vertex[1]);
  }
}

/** Write face normals for all polygons as vn x y z */
void OBJWriter::write_poly_normals(OBJMesh &ob_mesh)
{
  ob_mesh.tot_poly_normals = ob_mesh.me_eval->totpoly;
  float poly_normal[3];
  BKE_mesh_ensure_normals(ob_mesh.me_eval);
  for (uint i = 0; i < ob_mesh.tot_poly_normals; i++) {
    ob_mesh.calc_poly_normal(poly_normal, i);
    fprintf(outfile, "vn %f %f %f\n", poly_normal[0], poly_normal[1], poly_normal[2]);
  }
}

/** Define and write a face with at least vertex indices, and conditionally with UV vertex indices
 * and face normal indices.
 * \note UV indices are stored while writing UV vertices.
 */
void OBJWriter::write_poly_indices(OBJMesh &ob_mesh, blender::Vector<blender::Vector<uint>> &uv_indices)
{
  ob_mesh.tot_poly_normals = ob_mesh.me_eval->totpoly;
  blender::Vector<uint> vertex_indices;
  blender::Vector<uint> normal_indices;

  if (ob_mesh.export_params->export_normals) {
    if (ob_mesh.export_params->export_uv) {
      /* Write both normals and UV indices. */
      for (uint i = 0; i < ob_mesh.tot_poly_normals; i++) {
        ob_mesh.calc_poly_vertex_indices(vertex_indices, i);
        ob_mesh.calc_poly_normal_indices(normal_indices, i);
        const MPoly &poly_to_write = ob_mesh.me_eval->mpoly[i];

        write_vert_uv_normal_indices(vertex_indices, uv_indices[i], normal_indices, poly_to_write);
      }
    }
    else {
      /* Write normals indices. */
      for (uint i = 0; i < ob_mesh.tot_poly_normals; i++) {
        ob_mesh.calc_poly_vertex_indices(vertex_indices, i);
        ob_mesh.calc_poly_normal_indices(normal_indices, i);
        const MPoly &poly_to_write = ob_mesh.me_eval->mpoly[i];

        write_vert_normal_indices(vertex_indices, normal_indices, poly_to_write);
      }
    }
  }
  else {
    /* Write UV indices. */
    if (ob_mesh.export_params->export_uv) {
      for (uint i = 0; i < ob_mesh.tot_poly_normals; i++) {
        ob_mesh.calc_poly_vertex_indices(vertex_indices, i);
        const MPoly &poly_to_write = ob_mesh.me_eval->mpoly[i];

        write_vert_uv_indices(vertex_indices, uv_indices[i], poly_to_write);
      }
    }
    else {
      /* Write neither normals nor UV indices. */
      for (uint i = 0; i < ob_mesh.tot_poly_normals; i++) {
        ob_mesh.calc_poly_vertex_indices(vertex_indices, i);
        const MPoly &poly_to_write = ob_mesh.me_eval->mpoly[i];

        write_vert_indices(vertex_indices, poly_to_write);
      }
    }
  }
}

/** Define and write an edge of a curve or a "circle" mesh as l v1 v2 */
void OBJWriter::write_curve_edges(OBJMesh &ob_mesh)
{
  ob_mesh.tot_edges = ob_mesh.me_eval->totedge;
  blender::Array<uint, 2> vertex_indices;
  for (uint edge_index = 0; edge_index < ob_mesh.tot_edges; edge_index++) {
    ob_mesh.calc_edge_vert_indices(vertex_indices, edge_index);
    fprintf(outfile,
            "l %d %d\n",
            vertex_indices[0] + index_offset[vertex_off],
            vertex_indices[1] + index_offset[vertex_off]);
  }
}

/** When there are multiple objects in a frame, the indices of previous objects' coordinates or
 * normals add up.
 */
void OBJWriter::update_index_offsets(OBJMesh &ob_mesh)
{
  Mesh *exported_mesh = ob_mesh.me_eval;
  index_offset[vertex_off] += exported_mesh->totvert;
  index_offset[uv_vertex_off] += ob_mesh.tot_uv_vertices;
  index_offset[normal_off] += exported_mesh->totpoly;
}
}  // namespace obj
}  // namespace io
