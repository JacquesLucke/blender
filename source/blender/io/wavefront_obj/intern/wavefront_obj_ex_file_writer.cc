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

#include "BKE_blender_version.h"

#include "BLI_fileops.h"
#include "BLI_math_inline.h"

#include "DNA_object_types.h"

#include "wavefront_obj_ex_file_writer.hh"

namespace blender::io::obj {

/**
 * Write one line of polygon indices as f v1/vt1/vn1 v2/vt2/vn2 ... .
 */
void OBJWriter::write_vert_uv_normal_indices(Span<uint> vert_indices,
                                             Span<uint> uv_indices,
                                             Span<uint> normal_indices,
                                             const MPoly &poly_to_write)
{
  fprintf(outfile_, "f");
  for (uint j = 0; j < poly_to_write.totloop; j++) {
    fprintf(outfile_,
            " %u/%u/%u",
            vert_indices[j] + index_offset_[VERTEX_OFF],
            uv_indices[j] + index_offset_[UV_VERTEX_OFF],
            normal_indices[j] + index_offset_[NORMAL_OFF]);
  }
  fprintf(outfile_, "\n");
}

/**
 * Write one line of polygon indices as f v1//vn1 v2//vn2 ... .
 */
void OBJWriter::write_vert_normal_indices(Span<uint> vert_indices,
                                          Span<uint> normal_indices,
                                          const MPoly &poly_to_write)
{
  fprintf(outfile_, "f");
  for (uint j = 0; j < poly_to_write.totloop; j++) {
    fprintf(outfile_,
            " %u//%u",
            vert_indices[j] + index_offset_[VERTEX_OFF],
            normal_indices[j] + index_offset_[NORMAL_OFF]);
  }
  fprintf(outfile_, "\n");
}

/**
 * Write one line of polygon indices as f v1/vt1 v2/vt2 ... .
 */
void OBJWriter::write_vert_uv_indices(Span<uint> vert_indices,
                                      Span<uint> uv_indices,
                                      const MPoly &poly_to_write)
{
  fprintf(outfile_, "f");
  for (uint j = 0; j < poly_to_write.totloop; j++) {
    fprintf(outfile_,
            " %u/%u",
            vert_indices[j] + index_offset_[VERTEX_OFF],
            uv_indices[j] + index_offset_[UV_VERTEX_OFF]);
  }
  fprintf(outfile_, "\n");
}

/**
 *  Write one line of polygon indices as f v1 v2 ... .
 */
void OBJWriter::write_vert_indices(Span<uint> vert_indices, const MPoly &poly_to_write)
{
  fprintf(outfile_, "f");
  for (uint j = 0; j < poly_to_write.totloop; j++) {
    fprintf(outfile_, " %u", vert_indices[j] + index_offset_[VERTEX_OFF]);
  }
  fprintf(outfile_, "\n");
}

/**
 * Open the OBJ file and write file header.
 */
bool OBJWriter::init_writer(const char *filepath)
{
  outfile_ = fopen(filepath, "w");
  if (!outfile_) {
    return false;
  }
  fprintf(outfile_, "# Blender %s\n# www.blender.org\n", BKE_blender_version_string());
  return true;
}

/**
 * Write file name of Material Library in OBJ file.
 * Also create an empty Material Library file, or truncate the existing one.
 */
void OBJWriter::write_mtllib(const char *obj_filepath)
{
  char mtl_filepath[PATH_MAX];
  BLI_strncpy(mtl_filepath, obj_filepath, PATH_MAX);
  BLI_path_extension_replace(mtl_filepath, PATH_MAX, ".mtl");

  FILE *mtl_outfile = fopen(mtl_filepath, "w");
  if (!mtl_outfile) {
    fprintf(stderr, "Error opening Material Library file:%s", mtl_filepath);
    return;
  }
  fprintf(stderr, "Material Library: %s\n", mtl_filepath);
  fprintf(mtl_outfile, "# Blender %s\n# www.blender.org\n", BKE_blender_version_string());
  fclose(mtl_outfile);

  /* Split MTL file path into parent directory and filename. */
  char mtl_file_name[FILE_MAXFILE];
  char mtl_dir_name[FILE_MAXDIR];
  BLI_split_dirfile(mtl_filepath, mtl_dir_name, mtl_file_name, FILE_MAXDIR, FILE_MAXFILE);
  fprintf(outfile_, "mtllib %s\n", mtl_file_name);
}

/**
 * Write object name as it appears in the outliner.
 */
void OBJWriter::write_object_name(OBJMesh &obj_mesh_data)
{
  const char *object_name = obj_mesh_data.get_object_name();

  if (export_params_.export_object_groups) {
    const char *object_data_name = obj_mesh_data.get_object_data_name();
    fprintf(outfile_, "g %s_%s\n", object_name, object_data_name);
  }
  else {
    fprintf(outfile_, "o %s\n", object_name);
  }
}

/**
 * Write vertex coordinates for all vertices as v x y z .
 */
void OBJWriter::write_vertex_coords(OBJMesh &obj_mesh_data)
{
  float vertex[3];
  for (uint i = 0; i < obj_mesh_data.tot_vertices(); i++) {
    obj_mesh_data.calc_vertex_coords(vertex, i);
    fprintf(outfile_, "v %f %f %f\n", vertex[0], vertex[1], vertex[2]);
  }
}

/**
 * Write UV vertex coordinates for all vertices as vt u v .
 * \note UV indices are stored here, but written later.
 */
void OBJWriter::write_uv_coords(OBJMesh &obj_mesh_data, Vector<Vector<uint>> &uv_indices)
{
  Vector<std::array<float, 2>> uv_coords;

  obj_mesh_data.store_uv_coords_and_indices(uv_coords, uv_indices);
  for (const std::array<float, 2> &uv_vertex : uv_coords) {
    fprintf(outfile_, "vt %f %f\n", uv_vertex[0], uv_vertex[1]);
  }
}

/**
 * Write all face normals or all vertex normals as vn x y z .
 */
void OBJWriter::write_poly_normals(OBJMesh &obj_mesh_data)
{
  obj_mesh_data.ensure_mesh_normals();
  obj_mesh_data.calc_smooth_groups();
  if (export_params_.export_smooth_groups && obj_mesh_data.tot_smooth_groups() > 0) {
    float vertex_normal[3];
    for (uint i = 0; i < obj_mesh_data.tot_vertices(); i++) {
      obj_mesh_data.calc_vertex_normal(vertex_normal, i);
      fprintf(outfile_, "vn %f %f %f\n", vertex_normal[0], vertex_normal[1], vertex_normal[2]);
    }
  }
  else {
    float poly_normal[3];
    for (uint i = 0; i < obj_mesh_data.tot_polygons(); i++) {
      obj_mesh_data.calc_poly_normal(poly_normal, i);
      fprintf(outfile_, "vn %f %f %f\n", poly_normal[0], poly_normal[1], poly_normal[2]);
    }
  }
}

/**
 * Write smooth group if the polygon at given index is shaded smooth and export settings specify
 * so. If the polygon is not shaded smooth, write "off".
 */
void OBJWriter::write_smooth_group(OBJMesh &obj_mesh_data,
                                   int &r_last_face_smooth_group,
                                   uint poly_index)
{
  if (!export_params_.export_smooth_groups || !obj_mesh_data.tot_smooth_groups()) {
    return;
  }
  if (obj_mesh_data.get_ith_poly(poly_index).flag & ME_SMOOTH) {
    int curr_group = obj_mesh_data.ith_smooth_group(poly_index);
    if (curr_group == r_last_face_smooth_group) {
      return;
    }
    if (curr_group == 0) {
      fprintf(outfile_, "s off\n");
      r_last_face_smooth_group = curr_group;
      return;
    }
    fprintf(outfile_, "s %d\n", curr_group);
    r_last_face_smooth_group = curr_group;
  }
  else {
    if (r_last_face_smooth_group == 0) {
      return;
    }
    fprintf(outfile_, "s off\n");
    r_last_face_smooth_group = 0;
  }
}

/**
 * Write material name and material group of a face in the OBJ file.
 * \note It doesn't write to the material library.
 */
void OBJWriter::write_poly_material(OBJMesh &obj_mesh_data,
                                    short &r_last_face_mat_nr,
                                    uint poly_index)
{
  if (!export_params_.export_materials || obj_mesh_data.tot_col() <= 0) {
    return;
  }
  const MPoly &mpoly = obj_mesh_data.get_ith_poly(poly_index);
  short mat_nr = mpoly.mat_nr;
  /* Whenever a face with a new material is encountered, write its material and group, otherwise
   * pass. */
  if (UNLIKELY(r_last_face_mat_nr == mat_nr)) {
    const char *mat_name = obj_mesh_data.get_object_material_name(mat_nr + 1);
    if (export_params_.export_material_groups) {
      const char *object_name = obj_mesh_data.get_object_name();
      const char *object_data_name = obj_mesh_data.get_object_data_name();
      fprintf(outfile_, "g %s_%s_%s\n", object_name, object_data_name, mat_name);
    }
    fprintf(outfile_, "usemtl %s\n", mat_name);
    r_last_face_mat_nr = mat_nr;
  }
}

/**
 * Write the name of the deform group of a face. If no vertex group is found in the face, "off" is
 * written.
 */
void OBJWriter::write_vertex_group(OBJMesh &obj_mesh_data,
                                   short &last_face_vertex_group,
                                   uint poly_index)
{
  if (!export_params_.export_vertex_groups) {
    return;
  }
  const MPoly &mpoly = obj_mesh_data.get_ith_poly(poly_index);
  const char *def_group_name = obj_mesh_data.get_poly_deform_group_name(mpoly,
                                                                        last_face_vertex_group);
  if (!def_group_name) {
    /* Don't write the name of the group again. If set once, the group name changes only when a new
     * one is encountered. */
    return;
  }
  fprintf(outfile_, "g %s\n", def_group_name);
}

/**
 * Define and write face elements with at least vertex indices, and conditionally with UV vertex
 * indices and face normal indices. Also write groups: smooth, vertex, material.
 *  \note UV indices are stored while writing UV vertices.
 */
void OBJWriter::write_poly_elements(OBJMesh &obj_mesh_data, Span<Vector<uint>> uv_indices)
{
  Vector<uint> vertex_indices;
  Vector<uint> normal_indices;

  /* -1 has no significant value, it could have been any negative number. */
  int last_face_smooth_group = -1;
  /* -1 is used for a face having no vertex group. It could have been any _other_ negative
   * number. */
  short last_face_vertex_group = -2;
  /* -1 has no significant value, it could have been any negative number. */
  short last_face_mat_nr = -1;

  if (export_params_.export_normals) {
    if (export_params_.export_uv && (obj_mesh_data.tot_uv_vertices() > 0)) {
      /* Write both normals and UV indices. */
      for (uint i = 0; i < obj_mesh_data.tot_polygons(); i++) {
        obj_mesh_data.calc_poly_vertex_indices(vertex_indices, i);
        obj_mesh_data.calc_poly_normal_indices(normal_indices, i);
        const MPoly &poly_to_write = obj_mesh_data.get_ith_poly(i);

        write_smooth_group(obj_mesh_data, last_face_smooth_group, i);
        write_vertex_group(obj_mesh_data, last_face_vertex_group, i);
        write_poly_material(obj_mesh_data, last_face_mat_nr, i);
        write_vert_uv_normal_indices(vertex_indices, uv_indices[i], normal_indices, poly_to_write);
      }
    }
    else {
      /* Write normals indices. */
      for (uint i = 0; i < obj_mesh_data.tot_polygons(); i++) {
        obj_mesh_data.calc_poly_vertex_indices(vertex_indices, i);
        obj_mesh_data.calc_poly_normal_indices(normal_indices, i);
        const MPoly &poly_to_write = obj_mesh_data.get_ith_poly(i);

        write_smooth_group(obj_mesh_data, last_face_smooth_group, i);
        write_vertex_group(obj_mesh_data, last_face_vertex_group, i);
        write_poly_material(obj_mesh_data, last_face_mat_nr, i);
        write_vert_normal_indices(vertex_indices, normal_indices, poly_to_write);
      }
    }
  }
  else {
    /* Write UV indices. */
    if (export_params_.export_uv && (obj_mesh_data.tot_uv_vertices() > 0)) {
      for (uint i = 0; i < obj_mesh_data.tot_polygons(); i++) {
        obj_mesh_data.calc_poly_vertex_indices(vertex_indices, i);
        const MPoly &poly_to_write = obj_mesh_data.get_ith_poly(i);

        write_smooth_group(obj_mesh_data, last_face_smooth_group, i);
        write_vertex_group(obj_mesh_data, last_face_vertex_group, i);
        write_poly_material(obj_mesh_data, last_face_mat_nr, i);
        write_vert_uv_indices(vertex_indices, uv_indices[i], poly_to_write);
      }
    }
    else {
      /* Write neither normals nor UV indices. */
      for (uint i = 0; i < obj_mesh_data.tot_polygons(); i++) {
        obj_mesh_data.calc_poly_vertex_indices(vertex_indices, i);
        const MPoly &poly_to_write = obj_mesh_data.get_ith_poly(i);

        write_smooth_group(obj_mesh_data, last_face_smooth_group, i);
        write_vertex_group(obj_mesh_data, last_face_vertex_group, i);
        write_poly_material(obj_mesh_data, last_face_mat_nr, i);
        write_vert_indices(vertex_indices, poly_to_write);
      }
    }
  }
}

/**
 * Define and write an edge of a curve converted to mesh or a primitive circle as l v1 v2 .
 */
void OBJWriter::write_curve_edges(OBJMesh &obj_mesh_data)
{
  uint vertex_indices[2];
  for (uint edge_index = 0; edge_index < obj_mesh_data.tot_edges(); edge_index++) {
    obj_mesh_data.calc_edge_vert_indices(vertex_indices, edge_index);
    fprintf(outfile_,
            "l %u %u\n",
            vertex_indices[0] + index_offset_[VERTEX_OFF],
            vertex_indices[1] + index_offset_[VERTEX_OFF]);
  }
}

/**
 * Write a NURBS curve to the OBJ file in parameter form.
 */
void OBJWriter::write_nurbs_curve(OBJNurbs &obj_nurbs_data)
{
  const ListBase *nurbs = obj_nurbs_data.curve_nurbs();
  LISTBASE_FOREACH (const Nurb *, nurb, nurbs) {
    /* Total control points in a nurbs. */
    int tot_points = nurb->pntsv * nurb->pntsu;
    float point_coord[3];
    for (int point_idx = 0; point_idx < tot_points; point_idx++) {
      obj_nurbs_data.calc_point_coords(nurb, point_idx, point_coord);
      fprintf(outfile_, "v %f %f %f\n", point_coord[0], point_coord[1], point_coord[2]);
    }

    const char *nurbs_name = obj_nurbs_data.get_curve_name();
    int nurbs_degree = 0;
    /* Number of vertices in the curve + degree of the curve if it is cyclic. */
    int curv_num = 0;
    obj_nurbs_data.get_curve_info(nurb, nurbs_degree, curv_num);

    fprintf(outfile_, "g %s\ncstype bspline\ndeg %d\n", nurbs_name, nurbs_degree);
    /**
     * curv_num indices into the point vertices above, in relative indices.
     * 0.0 1.0 -1 -2 -3 -4 for a non-cyclic curve with 4 points.
     * 0.0 1.0 -1 -2 -3 -4 -1 -2 -3 for a cyclic curve with 4 points.
     */
    fprintf(outfile_, "curv 0.0 1.0");
    for (int i = 0; i < curv_num; i++) {
      fprintf(outfile_, " %d", -1 * ((i % tot_points) + 1));
    }
    fprintf(outfile_, "\n");

    /**
     * In parm u line: between 0 and 1, curv_num + 2 equidistant numbers are inserted.
     */
    fprintf(outfile_, "parm u 0.000000 ");
    for (int i = 1; i <= curv_num + 2; i++) {
      fprintf(outfile_, "%f ", 1.0f * i / (curv_num + 2 + 1));
    }
    fprintf(outfile_, "1.000000\n");

    fprintf(outfile_, "end\n");
  }
}

/**
 *  When there are multiple objects in a frame, the indices of previous objects' coordinates or
 * normals add up.
 */
void OBJWriter::update_index_offsets(OBJMesh &obj_mesh_data)
{
  index_offset_[VERTEX_OFF] += obj_mesh_data.tot_vertices();
  index_offset_[UV_VERTEX_OFF] += obj_mesh_data.tot_uv_vertices();
  index_offset_[NORMAL_OFF] += obj_mesh_data.tot_polygons();
}
}  // namespace blender::io::obj
