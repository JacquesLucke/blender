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
  fprintf(_outfile, "f ");
  for (int j = 0; j < poly_to_write.totloop; j++) {
    fprintf(_outfile,
            "%d/%d/%d ",
            vert_indices[j] + _index_offset[vertex_off],
            uv_indices[j] + _index_offset[uv_vertex_off],
            normal_indices[j] + _index_offset[normal_off]);
  }
  fprintf(_outfile, "\n");
}

/** Write one line of polygon indices as f v1//vn1 v2//vn2 .... */
void OBJWriter::write_vert_normal_indices(blender::Vector<uint> &vert_indices,
                                          blender::Vector<uint> &normal_indices,
                                          const MPoly &poly_to_write)
{
  fprintf(_outfile, "f ");
  for (int j = 0; j < poly_to_write.totloop; j++) {
    fprintf(_outfile,
            "%d//%d ",
            vert_indices[j] + _index_offset[vertex_off],
            normal_indices[j] + _index_offset[normal_off]);
  }
  fprintf(_outfile, "\n");
}

/** Write one line of polygon indices as f v1/vt1 v2/vt2 .... */
void OBJWriter::write_vert_uv_indices(blender::Vector<uint> &vert_indices,
                                      blender::Vector<uint> &uv_indices,
                                      const MPoly &poly_to_write)
{
  fprintf(_outfile, "f ");
  for (int j = 0; j < poly_to_write.totloop; j++) {
    fprintf(_outfile,
            "%d/%d ",
            vert_indices[j] + _index_offset[vertex_off],
            uv_indices[j] + 1 + _index_offset[uv_vertex_off]);
  }
  fprintf(_outfile, "\n");
}

/** Write one line of polygon indices as f v1 v2 .... */
void OBJWriter::write_vert_indices(blender::Vector<uint> &vert_indices, const MPoly &poly_to_write)
{
  fprintf(_outfile, "f ");
  for (int j = 0; j < poly_to_write.totloop; j++) {
    fprintf(_outfile, "%d ", vert_indices[j] + _index_offset[vertex_off]);
  }
  fprintf(_outfile, "\n");
}

/** Open the file and write file header. */
bool OBJWriter::init_writer()
{
  _outfile = fopen(_export_params->filepath, "w");
  if (!_outfile) {
    return false;
  }
  /** Write Blender version and website as a comment in the file. */
  fprintf(_outfile, "# Blender %s\nwww.blender.org\n", BKE_blender_version_string());
  return true;
}

/** Write object name as it appears in the outliner. */
void OBJWriter::write_object_name(OBJMesh &obj_mesh_data)
{
  const char *object_name;
  obj_mesh_data.get_object_name(&object_name);
  fprintf(_outfile, "o %s\n", object_name);
}

/** Write vertex coordinates for all vertices as v x y z */
void OBJWriter::write_vertex_coords(OBJMesh &obj_mesh_data)
{
  float vertex[3];
  for (uint i = 0; i < obj_mesh_data.tot_vertices(); i++) {
    obj_mesh_data.calc_vertex_coords(vertex, i);
    fprintf(_outfile, "v %f %f %f\n", vertex[0], vertex[1], vertex[2]);
  }
}

/** Write UV vertex coordinates for all vertices as vt u v
 * \note UV indices are stored here, but written later.
 */
void OBJWriter::write_uv_coords(OBJMesh &obj_mesh_data,
                                blender::Vector<blender::Vector<uint>> &uv_indices)
{
  blender::Vector<std::array<float, 2>> uv_coords;

  obj_mesh_data.store_uv_coords_and_indices(uv_coords, uv_indices);
  for (uint i = 0; i < uv_coords.size(); i++) {
    const std::array<float, 2> &uv_vertex = uv_coords[i];
    fprintf(_outfile, "vt %f %f\n", uv_vertex[0], uv_vertex[1]);
  }
}

/** Write face normals for all polygons as vn x y z */
void OBJWriter::write_poly_normals(OBJMesh &obj_mesh_data)
{
  float poly_normal[3];
  BKE_mesh_ensure_normals(obj_mesh_data.export_mesh_eval());
  for (uint i = 0; i < obj_mesh_data.tot_poly_normals(); i++) {
    obj_mesh_data.calc_poly_normal(poly_normal, i);
    fprintf(_outfile, "vn %f %f %f\n", poly_normal[0], poly_normal[1], poly_normal[2]);
  }
}

/** Define and write a face with at least vertex indices, and conditionally with UV vertex indices
 * and face normal indices.
 * \note UV indices are stored while writing UV vertices.
 */
void OBJWriter::write_poly_indices(OBJMesh &obj_mesh_data,
                                   blender::Vector<blender::Vector<uint>> &uv_indices)
{
  blender::Vector<uint> vertex_indices;
  blender::Vector<uint> normal_indices;

  if (_export_params->export_normals) {
    if (_export_params->export_uv) {
      /* Write both normals and UV indices. */
      for (uint i = 0; i < obj_mesh_data.tot_poly_normals(); i++) {
        obj_mesh_data.calc_poly_vertex_indices(vertex_indices, i);
        obj_mesh_data.calc_poly_normal_indices(normal_indices, i);
        const MPoly &poly_to_write = obj_mesh_data.get_ith_poly(i);
        write_vert_uv_normal_indices(vertex_indices, uv_indices[i], normal_indices, poly_to_write);
      }
    }
    else {
      /* Write normals indices. */
      for (uint i = 0; i < obj_mesh_data.tot_poly_normals(); i++) {
        obj_mesh_data.calc_poly_vertex_indices(vertex_indices, i);
        obj_mesh_data.calc_poly_normal_indices(normal_indices, i);
        const MPoly &poly_to_write = obj_mesh_data.get_ith_poly(i);

        write_vert_normal_indices(vertex_indices, normal_indices, poly_to_write);
      }
    }
  }
  else {
    /* Write UV indices. */
    if (_export_params->export_uv) {
      for (uint i = 0; i < obj_mesh_data.tot_poly_normals(); i++) {
        obj_mesh_data.calc_poly_vertex_indices(vertex_indices, i);
        const MPoly &poly_to_write = obj_mesh_data.get_ith_poly(i);

        write_vert_uv_indices(vertex_indices, uv_indices[i], poly_to_write);
      }
    }
    else {
      /* Write neither normals nor UV indices. */
      for (uint i = 0; i < obj_mesh_data.tot_poly_normals(); i++) {
        obj_mesh_data.calc_poly_vertex_indices(vertex_indices, i);
        const MPoly &poly_to_write = obj_mesh_data.get_ith_poly(i);

        write_vert_indices(vertex_indices, poly_to_write);
      }
    }
  }
}

/** Define and write an edge of a curve converted to mesh or a primitive circle as l v1 v2 */
void OBJWriter::write_curve_edges(OBJMesh &obj_mesh_data)
{
  uint vertex_indices[2];
  for (uint edge_index = 0; edge_index < obj_mesh_data.tot_edges(); edge_index++) {
    obj_mesh_data.calc_edge_vert_indices(vertex_indices, edge_index);
    fprintf(_outfile,
            "l %d %d\n",
            vertex_indices[0] + _index_offset[vertex_off],
            vertex_indices[1] + _index_offset[vertex_off]);
  }
}

void OBJWriter::write_nurbs_info(OBJNurbs &ob_nurbs)
{
  // todo object name max is 64.
  Nurb *nurb = (Nurb *)ob_nurbs.curve->nurb.first;
  uint tot_points = nurb->pntsv * nurb->pntsu;
  float point_coord[3];
  for (uint point_idx = 0; point_idx < tot_points; point_idx++) {
    ob_nurbs.calc_vertex_coords(point_coord, point_idx);
    fprintf(outfile, "v %f %f %f\n", point_coord[0], point_coord[1], point_coord[2]);
  }

  const char *nurbs_name;
  int nurbs_degree;
  /** Number of vertices in the curve + degree of the curve if it is cyclic. */
  int curv_num;
  nurbs_name = ob_nurbs.get_curve_info(&nurbs_degree, &curv_num);

  fprintf(outfile, "g %s\ncstype bspline\n deg %d\n", nurbs_name, nurbs_degree);

  /**
   * curv_num refers to the vertices above written in relative indices.
   * 0.0 1.0 -1 -2 -3 -4 for a non-cyclic curve with 4 points.
   * 0.0 1.0 -1 -2 -3 -4 -1 -2 -3 for a cyclic curve with 4 points.
   */
  fprintf(outfile, "curv 0.0 1.0 ");
  for (int i = 0; i < curv_num; i++) {
    fprintf(outfile, "%d ", -1 * ((i % tot_points) + 1));
  }
  fprintf(outfile, "\n");

  /**
   * In parm u, between 0 and 1, curv_num + 2 equidistant numbers are inserted.
   */
  fprintf(outfile, "parm u 0.000000 ");
  for (int i = 1; i <= curv_num + 2; i++) {
    fprintf(outfile, "%f ", 1.0f * i / (curv_num + 2 + 1));
  }
  fprintf(outfile, "1.000000\n");

  fprintf(outfile, "end\n");
}

/** When there are multiple objects in a frame, the indices of previous objects' coordinates or
 * normals add up.
 */
void OBJWriter::update_index_offsets(OBJMesh &obj_mesh_data)
{
  _index_offset[vertex_off] += obj_mesh_data.tot_vertices();
  _index_offset[uv_vertex_off] += obj_mesh_data.tot_uv_vertices();
  _index_offset[normal_off] += obj_mesh_data.tot_poly_normals();
}
}  // namespace obj
}  // namespace io
