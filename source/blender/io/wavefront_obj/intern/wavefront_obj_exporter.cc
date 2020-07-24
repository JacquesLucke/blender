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

#include <stdio.h>

#include "BKE_scene.h"

#include "BLI_path_util.h"
#include "BLI_vector.hh"

#include "DEG_depsgraph_query.h"

#include "DNA_scene_types.h"

#include "ED_object.h"

#include "wavefront_obj_ex_file_writer.hh"
#include "wavefront_obj_ex_mesh.hh"
#include "wavefront_obj_ex_mtl.hh"
#include "wavefront_obj_ex_nurbs.hh"
#include "wavefront_obj_exporter.hh"

namespace blender::io::obj {
/**
 * Scan objects in a scene to find exportable objects, as per export settings and object types, and
 * add them to the given Vectors.
 * \note Curves are also stored as OBJMesh if export settings specify so.
 */
static void find_exportable_objects(ViewLayer *view_layer,
                                    Depsgraph *depsgraph,
                                    const OBJExportParams &export_params,
                                    Vector<std::unique_ptr<OBJMesh>> &r_exportable_meshes,
                                    Vector<std::unique_ptr<OBJNurbs>> &r_exportable_nurbs)
{
  LISTBASE_FOREACH (Base *, base, &view_layer->object_bases) {
    Object *object_in_layer = base->object;
    if (export_params.export_selected_objects && !(object_in_layer->base_flag & BASE_SELECTED)) {
      continue;
    }
    switch (object_in_layer->type) {
      case OB_SURF:
        /* Export in mesh form: vertices and polygons. */
        ATTR_FALLTHROUGH;
      case OB_MESH: {
        r_exportable_meshes.append(
            std::unique_ptr<OBJMesh>(new OBJMesh(depsgraph, export_params, object_in_layer)));
        break;
      }
      case OB_CURVE: {
        Curve *curve = static_cast<Curve *>(object_in_layer->data);
        Nurb *nurb{static_cast<Nurb *>(curve->nurb.first)};
        switch (nurb->type) {
          case CU_NURBS: {
            if (export_params.export_curves_as_nurbs) {
              /* Export in parameter form: control points. */
              r_exportable_nurbs.append(
                  std::unique_ptr<OBJNurbs>(new OBJNurbs(depsgraph, object_in_layer)));
            }
            else {
              /* Export in mesh form: edges and vertices. */
              r_exportable_meshes.append(std::unique_ptr<OBJMesh>(
                  new OBJMesh(depsgraph, export_params, object_in_layer)));
            }
            break;
          }
          case CU_BEZIER: {
            /* Always export in mesh form: edges and vertices. */
            r_exportable_meshes.append(
                std::unique_ptr<OBJMesh>(new OBJMesh(depsgraph, export_params, object_in_layer)));
            break;
          }
          default: {
            /* Other curve types are not supported. */
            break;
          }
        }
      }
      default: {
        /* Other object types are not supported. */
        break;
      }
    }
  }
}

/**
 * Traverses over and exports a single frame to a single OBJ file.
 */
static void export_frame(ViewLayer *view_layer,
                         Depsgraph *depsgraph,
                         const OBJExportParams &export_params,
                         const char *filepath)
{
  OBJWriter frame_writer(export_params);
  if (!frame_writer.init_writer(filepath)) {
    fprintf(stderr, "Error in creating the file: %s\n", filepath);
    return;
  }

  /* Meshes, and curves to be exported in mesh form. */
  Vector<std::unique_ptr<OBJMesh>> exportable_as_mesh;
  /* NURBS to be exported in parameter form. */
  Vector<std::unique_ptr<OBJNurbs>> exportable_as_nurbs;
  find_exportable_objects(
      view_layer, depsgraph, export_params, exportable_as_mesh, exportable_as_nurbs);

  if (export_params.export_materials) {
    /* Create an empty MTL file in the beginning, to be appended later. */
    frame_writer.write_mtllib(filepath);
  }
  for (std::unique_ptr<OBJMesh> &mesh_to_export : exportable_as_mesh) {
    frame_writer.write_object_name(*mesh_to_export);
    frame_writer.write_vertex_coords(*mesh_to_export);

    /* Write edges of curves converted to mesh and primitive circle. */
    if (mesh_to_export->tot_polygons() == 0) {
      frame_writer.write_curve_edges(*mesh_to_export);
    }
    else {
      Vector<Vector<uint>> uv_indices;
      if (export_params.export_normals) {
        frame_writer.write_poly_normals(*mesh_to_export);
      }
      if (export_params.export_uv) {
        frame_writer.write_uv_coords(*mesh_to_export, uv_indices);
      }
      if (export_params.export_materials) {
        MTLWriter mtl_writer(filepath);
        mtl_writer.append_materials(*mesh_to_export);
      }
      frame_writer.write_poly_elements(*mesh_to_export, uv_indices);
    }
    frame_writer.update_index_offsets(*mesh_to_export);
  }
  /* Export nurbs in parm form, not as vertices and edges. */
  for (std::unique_ptr<OBJNurbs> &nurbs_to_export : exportable_as_nurbs) {
    frame_writer.write_nurbs_curve(*nurbs_to_export);
  }
}

/**
 * Insert frame number in OBJ filepath for animation export.
 */
static bool insert_frame_in_path(const char *filepath, char *r_filepath_with_frames, int frame)
{
  BLI_strncpy(r_filepath_with_frames, filepath, FILE_MAX);
  BLI_path_extension_replace(r_filepath_with_frames, FILE_MAX, "");
  int digits = frame == 0 ? 1 : integer_digits_i(abs(frame));
  BLI_path_frame(r_filepath_with_frames, frame, digits);
  bool filepath_ok = BLI_path_extension_replace(r_filepath_with_frames, FILE_MAX, ".obj");
  return filepath_ok;
}

/**
 * Central internal function to call scene update & writer functions.
 */
void exporter_main(bContext *C, const OBJExportParams &export_params)
{
  /* TODO ankitm: find a better way to exit edit mode that doesn't hit assert
   * https://hastebin.com/mitihetagi in file F8653460 */
  ED_object_editmode_exit(C, EM_FREEDATA);
  Scene *scene = CTX_data_scene(C);
  Main *bmain = CTX_data_main(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Depsgraph *depsgraph = export_params.export_eval_mode == DAG_EVAL_RENDER ?
                             DEG_graph_new(bmain, scene, view_layer, DAG_EVAL_RENDER) :
                             CTX_data_ensure_evaluated_depsgraph(C);
  const char *filepath = export_params.filepath;

  /* Single frame export, i.e. no amimation is to be exported. */
  if (!export_params.export_animation) {
    fprintf(stderr, "Writing to %s\n", filepath);
    export_frame(view_layer, depsgraph, export_params, filepath);
    return;
  }

  int start_frame = export_params.start_frame;
  int end_frame = export_params.end_frame;
  char filepath_with_frames[FILE_MAX];
  /* To reset the Scene to its original state. */
  int original_frame = CFRA;

  for (int frame = start_frame; frame <= end_frame; frame++) {
    bool filepath_ok = insert_frame_in_path(filepath, filepath_with_frames, frame);
    if (!filepath_ok) {
      fprintf(stderr, "Error: File Path too long.\n%s\n", filepath_with_frames);
      return;
    }

    CFRA = frame;
    BKE_scene_graph_update_for_newframe(depsgraph, bmain);
    fprintf(stderr, "Writing to %s\n", filepath_with_frames);
    export_frame(view_layer, depsgraph, export_params, filepath_with_frames);
  }
  CFRA = original_frame;
}
}  // namespace blender::io::obj
