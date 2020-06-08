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

#include "MEM_guardedalloc.h"

#include <stdio.h>

#include "BKE_context.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_object.h"
#include "BKE_scene.h"

#include "BLI_iterator.h"
#include "BLI_linklist.h"
#include "BLI_math.h"
#include "BLI_vector.hh"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "IO_wavefront_obj.h"

#include "wavefront_obj.hh"
#include "wavefront_obj_exporter.hh"
#include "wavefront_obj_file_handler.hh"

namespace io {
namespace obj {

/**
 * Store the mesh vertex coordinates in data_to_export, in world coordinates.
 */
static void get_transformed_mesh_vertices(Mesh *me_eval,
                                          Object *ob_eval,
                                          OBJ_data_to_export *data_to_export)
{
  uint num_verts = data_to_export->tot_vertices = me_eval->totvert;
  float transformed_co[3];
  data_to_export->mvert = (MVert *)MEM_callocN(num_verts * sizeof(MVert),
                                               "OBJ vertex coordinates & normals");

  for (uint i = 0; i < num_verts; i++) {
    copy_v3_v3(transformed_co, me_eval->mvert[i].co);
    mul_m4_v3(ob_eval->obmat, transformed_co);
    copy_v3_v3(data_to_export->mvert[i].co, transformed_co);
  }
}

/**
 * Store the mesh vertex normals in data_to_export, in world coordinates.
 */
static void get_transformed_vertex_normals(Mesh *me_eval, OBJ_data_to_export *data_to_export)
{
  float transformed_normal[3];
  const MLoop *mloop;
  const MPoly *mpoly = me_eval->mpoly;
  const MVert *mvert = me_eval->mvert;

  for (uint i = 0; i < me_eval->totpoly; i++, mpoly++) {
    mloop = &me_eval->mloop[mpoly->loopstart];
    BKE_mesh_calc_poly_normal(mpoly, mloop, mvert, transformed_normal);
    normal_float_to_short_v3(data_to_export->mvert[i].no, transformed_normal);
  }
}

/**
 * Store a polygon's vertex indices and normal indices, indexing into the pre-defined
 * vertex coordinates and vertex normals list.
 */
static void get_polygon_vert_indices(Mesh *me_eval, OBJ_data_to_export *data_to_export)
{
  const MLoop *mloop;
  const MPoly *mpoly = me_eval->mpoly;

  data_to_export->tot_faces = me_eval->totpoly;
  data_to_export->polygon_list.increase_size_unchecked(me_eval->totpoly);

  for (uint i = 0; i < me_eval->totpoly; i++, mpoly++) {
    mloop = &me_eval->mloop[mpoly->loopstart];
    data_to_export->polygon_list[i].total_vertices_per_poly = mpoly->totloop;

    for (int j = 0; j < mpoly->totloop; j++) {
      data_to_export->polygon_list[i].vertex_index.append((mloop + j)->v + 1);
      data_to_export->polygon_list[i].face_normal_index.append(i + 1);
    }
  }
}

static void get_geometry_per_object(const OBJExportParams *export_params,
                                    OBJ_data_to_export *data_to_export)
{
  Depsgraph *depsgraph = data_to_export->depsgraph;
  Object *ob = CTX_data_active_object(data_to_export->C);
  Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
  Mesh *me_eval = BKE_object_get_evaluated_mesh(ob_eval);

  get_transformed_mesh_vertices(me_eval, ob_eval, data_to_export);
  get_transformed_vertex_normals(me_eval, data_to_export);
  get_polygon_vert_indices(me_eval, data_to_export);
}

/**
 * Central internal function to call geometry data preparation & writer functions.
 */
void exporter_main(bContext *C, const OBJExportParams *export_params)
{
  const char *filepath = export_params->filepath;
  struct OBJ_data_to_export data_to_export;
  data_to_export.C = C;
  data_to_export.depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  get_geometry_per_object(export_params, &data_to_export);

  write_obj_data(filepath, &data_to_export);
  MEM_freeN(data_to_export.mvert);
}
}  // namespace obj
}  // namespace io
