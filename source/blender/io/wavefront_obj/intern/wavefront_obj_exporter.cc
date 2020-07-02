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

#include "wavefront_obj_exporter.hh"
#include "wavefront_obj_exporter_mesh.hh"
#include "wavefront_obj_exporter_mtl.hh"
#include "wavefront_obj_exporter_nurbs.hh"
#include "wavefront_obj_file_handler.hh"

namespace blender {
namespace io {
namespace obj {

/**
 * Scan objects in a scene to find exportable objects, as per export_params and object types, and
 * add them to the given Vectors.
 */
static void filter_exportable_objects(bContext *C,
                                      const OBJExportParams *export_params,
                                      Vector<std::unique_ptr<OBJMesh>> &exportable_meshes,
                                      Vector<std::unique_ptr<OBJNurbs>> &exportable_nurbs)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  LISTBASE_FOREACH (Base *, base, &view_layer->object_bases) {
    Object *object_in_layer = base->object;
    if (export_params->export_selected_objects && !(object_in_layer->base_flag & BASE_SELECTED)) {
      continue;
    }
    switch (object_in_layer->type) {
      case OB_SURF:
      case OB_MESH: {
        OBJMesh *objmesh = new OBJMesh(C, export_params, object_in_layer);
        exportable_meshes.append(std::unique_ptr<OBJMesh>(objmesh));
        break;
      }
      case OB_CURVE: {
        Curve *curve = (Curve *)object_in_layer->data;
        Nurb *nurb = (Nurb *)curve->nurb.first;
        if (nurb->type == CU_NURBS) {
          if (export_params->export_curves_as_nurbs) {
            OBJNurbs *objnurb = new OBJNurbs(C, object_in_layer);
            exportable_nurbs.append(std::unique_ptr<OBJNurbs>(objnurb));
          }
          else {
            OBJMesh *objmesh = new OBJMesh(C, export_params, object_in_layer);
            exportable_meshes.append(std::unique_ptr<OBJMesh>(objmesh));
          }
        }
        if (nurb->type == CU_BEZIER) {
          OBJMesh *objmesh = new OBJMesh(C, export_params, object_in_layer);
          exportable_meshes.append(std::unique_ptr<OBJMesh>(objmesh));
        }
        /* Other types of curves are not supported.  */
        break;
      }
      default:
        break;
    }
  }
}

/**
 * Traverses over and exports a single frame to a single OBJ file.
 */
static void export_frame(bContext *C, const OBJExportParams *export_params, const char *filepath)
{
  OBJWriter frame_writer(export_params);
  if (!frame_writer.init_writer()) {
    fprintf(stderr, "Error in creating the file: %s\n", export_params->filepath);
    return;
  }

  Vector<std::unique_ptr<OBJMesh>> exportable_meshes;
  Vector<std::unique_ptr<OBJNurbs>> exportable_nurbs;
  filter_exportable_objects(C, export_params, exportable_meshes, exportable_nurbs);

  if (export_params->export_materials) {
    /* Write MTL filename to the OBJ file. Also create an empty MTL file of the same name as the
     * OBJ. */
    frame_writer.write_mtllib(filepath);
  }
  for (std::unique_ptr<OBJMesh> &mesh_to_export : exportable_meshes) {

    frame_writer.write_object_name(*mesh_to_export);
    frame_writer.write_vertex_coords(*mesh_to_export);

    /* For curves converted to mesh and primitive circle. */
    if (mesh_to_export->tot_poly_normals() == 0) {
      frame_writer.write_curve_edges(*mesh_to_export);
    }
    else {
      Vector<Vector<uint>> uv_indices;
      if (export_params->export_normals) {
        frame_writer.write_poly_normals(*mesh_to_export);
      }
      if (export_params->export_uv) {
        frame_writer.write_uv_coords(*mesh_to_export, uv_indices);
      }
      if (export_params->export_materials) {
        MTLWriter mtl_writer(filepath);
        mtl_writer.append_materials(*mesh_to_export);
      }
      frame_writer.write_poly_indices(*mesh_to_export, uv_indices);
    }
    frame_writer.update_index_offsets(*mesh_to_export);
  }
  /* Export nurbs in parm form, not as vertices and edges. */
  for (std::unique_ptr<OBJNurbs> &nurbs_to_export : exportable_nurbs) {
    frame_writer.write_nurbs_curve(*nurbs_to_export);
  }
}

/**
 * Central internal function to call scene update & writer functions.
 */
void exporter_main(bContext *C, const OBJExportParams *export_params)
{
  /* TODO ankitm: find a better way to exit edit mode that doesn't hit assert
   * https://hastebin.com/mitihetagi in file F8653460 */
  ED_object_editmode_exit(C, EM_FREEDATA);
  Scene *scene = CTX_data_scene(C);
  const char *filepath = export_params->filepath;

  /* Single frame export, i.e. no amimation is to be exported. */
  if (!export_params->export_animation) {
    fprintf(stderr, "Writing to %s\n", filepath);
    export_frame(C, export_params, filepath);
    return;
  }

  int start_frame = export_params->start_frame;
  int end_frame = export_params->end_frame;
  /* Insert frame number (with "-" if frame is negative) in the filename. */
  char filepath_with_frames[FILE_MAX];
  /* To reset the Scene to its original state. */
  int original_frame = CFRA;

  for (int frame = start_frame; frame <= end_frame; frame++) {
    BLI_strncpy(filepath_with_frames, filepath, FILE_MAX);
    /* 1 _ + 11 digits for frame number (INT_MAX + sign) + 4 for extension + 1 null. */
    char frame_ext[17];
    BLI_snprintf(frame_ext, 17, "_%d.obj", frame);
    bool filepath_ok = BLI_path_extension_replace(filepath_with_frames, FILE_MAX, frame_ext);
    if (!filepath_ok) {
      fprintf(stderr, "Error: File Path too long.\n%s\n", filepath_with_frames);
      return;
    }

    CFRA = frame;
    BKE_scene_graph_update_for_newframe(CTX_data_ensure_evaluated_depsgraph(C), CTX_data_main(C));
    fprintf(stderr, "Writing to %s\n", filepath_with_frames);
    export_frame(C, export_params, filepath_with_frames);
  }
  CFRA = original_frame;
}

}  // namespace obj
}  // namespace io
}  // namespace blender
