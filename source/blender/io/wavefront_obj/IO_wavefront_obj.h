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

#ifndef __IO_WAVEFRONT_OBJ_H__
#define __IO_WAVEFRONT_OBJ_H__

#include "BKE_context.h"
#include "BLI_path_util.h"
#include "DEG_depsgraph.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  OBJ_AXIS_X_UP = 0,
  OBJ_AXIS_Y_UP = 1,
  OBJ_AXIS_Z_UP = 2,
  OBJ_AXIS_NEGATIVE_X_UP = 3,
  OBJ_AXIS_NEGATIVE_Y_UP = 4,
  OBJ_AXIS_NEGATIVE_Z_UP = 5,
} eTransformAxisUp;

typedef enum {
  OBJ_AXIS_X_FORWARD = 0,
  OBJ_AXIS_Y_FORWARD = 1,
  OBJ_AXIS_Z_FORWARD = 2,
  OBJ_AXIS_NEGATIVE_X_FORWARD = 3,
  OBJ_AXIS_NEGATIVE_Y_FORWARD = 4,
  OBJ_AXIS_NEGATIVE_Z_FORWARD = 5,
} eTransformAxisForward;

struct OBJExportParams {
  /** Full path to the destination OBJ file to export. */
  char filepath[FILE_MAX];

  /** Whether mutiple frames are to be exported or not. */
  bool export_animation;
  /** The first frame to be exported. */
  int start_frame;
  /** The last frame to be exported. */
  int end_frame;

  /** Geometry Transform options. */
  int forward_axis;
  int up_axis;
  float scaling_factor;

  /** File Write Options. */
  bool export_selected_objects;
  eEvaluationMode export_eval_mode;
  bool export_uv;
  bool export_normals;
  bool export_materials;
  bool export_triangulated_mesh;
  bool export_curves_as_nurbs;

  /** Grouping options. */
  bool export_object_groups;
  bool export_material_groups;
  bool export_vertex_groups;
  /**
   * Export vertex normals instead of face normals if mesh is shaded smooth and this option is
   * true.
   */
  bool export_smooth_groups;
  /**
   * If true, generate bitflags for smooth groups' IDs. Generates upto 32 but usually much less.
   */
  bool smooth_groups_bitflags;
};

struct OBJImportParams {
  /** Full path to the source OBJ file to import. */
  char filepath[FILE_MAX];
};

void OBJ_import(bContext *C, const struct OBJImportParams *import_params);

void OBJ_export(bContext *C, const struct OBJExportParams *export_params);

#ifdef __cplusplus
}
#endif

#endif /* __IO_WAVEFRONT_OBJ_H__ */
