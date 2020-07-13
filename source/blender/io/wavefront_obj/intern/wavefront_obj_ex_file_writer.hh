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

#ifndef __WAVEFRONT_OBJ_EX_FILE_WRITER_HH__
#define __WAVEFRONT_OBJ_EX_FILE_WRITER_HH__

#include "IO_wavefront_obj.h"

#include "wavefront_obj_ex_mesh.hh"
#include "wavefront_obj_ex_nurbs.hh"

namespace blender::io::obj {

/* Types of index offsets. */
enum index_offsets {
  VERTEX_OFF = 0,
  UV_VERTEX_OFF = 1,
  NORMAL_OFF = 2,
};

class OBJWriter {
 private:
  /**
   * Destination OBJ file for one frame, and one writer instance.
   */
  FILE *outfile_;
  const OBJExportParams &export_params_;
  /**
   * Vertex offset, UV vertex offset, face normal offset respetively.
   */
  uint index_offset_[3] = {0, 0, 0};

 public:
  OBJWriter(const OBJExportParams &export_params) : export_params_(export_params)
  {
  }
  ~OBJWriter()
  {
    fclose(outfile_);
  }

  bool init_writer(const char *filepath);

  void write_object_name(OBJMesh &obj_mesh_data);
  void write_mtllib(const char *obj_filepath);
  void write_vertex_coords(OBJMesh &obj_mesh_data);
  void write_uv_coords(OBJMesh &obj_mesh_data, Vector<Vector<uint>> &uv_indices);
  void write_poly_normals(OBJMesh &obj_mesh_data);
  void write_smooth_group(OBJMesh &obj_mesh_data,
                          int &r_last_face_smooth_group,
                          const int *poly_smooth_groups,
                          uint poly_index);
  void write_poly_material(OBJMesh &obj_mesh_data, short &r_last_face_mat_nr, uint poly_index);
  void write_vertex_group(OBJMesh &obj_mesh_data,
                          short &r_last_face_vertex_group,
                          uint poly_index);
  void write_poly_elements(OBJMesh &obj_mesh_data, Span<Vector<uint>> uv_indices);
  void write_curve_edges(OBJMesh &obj_mesh_data);
  void write_nurbs_curve(OBJNurbs &obj_nurbs_data);

  void update_index_offsets(OBJMesh &obj_mesh_data);

 private:
  void write_vert_indices(Span<uint> vert_indices, const MPoly &poly_to_write);
  void write_vert_normal_indices(Span<uint> vert_indices,
                                 Span<uint> normal_indices,
                                 const MPoly &poly_to_write);
  void write_vert_uv_indices(Span<uint> vert_indices,
                             Span<uint> uv_indices,
                             const MPoly &poly_to_write);
  void write_vert_uv_normal_indices(Span<uint> vert_indices,
                                    Span<uint> uv_indices,
                                    Span<uint> normal_indices,
                                    const MPoly &poly_to_write);
};
}  // namespace blender::io::obj
#endif
