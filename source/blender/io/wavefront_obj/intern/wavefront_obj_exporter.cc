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

#include "wavefront_obj_exporter.hh"
#include "wavefront_obj_file_handler.hh"

#include <array>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <vector>

#include "BKE_DerivedMesh.h"
#include "BKE_context.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_object.h"
#include "BKE_scene.h"

#include "BLI_linklist.h"
#include "BLI_math.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

void prepare_vertices(OBJExportParams *export_params, OBJ_data_to_export *data_to_export)
{

  Depsgraph *depsgraph = export_params->depsgraph;
  Object *ob = CTX_data_active_object(export_params->C);
  Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
  //  Scene *scene = export_params->scene;
  //  Mesh *me_eval = mesh_create_eval_final_view(depsgraph, scene, ob_eval, &CD_MASK_BAREMESH);
  //  causing memory leak in lite & assert in full.
  Mesh *me_eval = BKE_object_get_evaluated_mesh(ob_eval);

  int num_verts = data_to_export->tot_vertices = me_eval->totvert;
  MVert empty_mvert;
  float transformed_co[3];

  for (int i = 0; i < num_verts; i++) {
    data_to_export->vertices.push_back(empty_mvert);
    copy_v3_v3(transformed_co, me_eval->mvert[i].co);
    mul_m4_v3(ob_eval->obmat, transformed_co);
    for (int j = 0; j < 3; j++) {
      data_to_export->vertices[i].co[j] = transformed_co[j];
      me_eval->mvert[i].co[j] = transformed_co[j];
    }
  }
  float transformed_normals[3];
  const MLoop *ml;
  const MPoly *mp = me_eval->mpoly;
  const MVert *mvert = me_eval->mvert;
  struct faces empty_face;

  data_to_export->tot_faces = me_eval->totpoly;
  for (int i = 0; i < me_eval->totpoly; i++, mp++) {
    data_to_export->normals.push_back(std::array<float, 3>());
    data_to_export->faces_list.push_back(empty_face);

    ml = &me_eval->mloop[mp->loopstart];
    BKE_mesh_calc_poly_normal(mp, ml, mvert, transformed_normals);

    for (int j = 0; j < 3; j++) {
      data_to_export->normals[i][j] = transformed_normals[j];
    }

    data_to_export->faces_list[i].total_vertices_per_face = mp->totloop;
    for (int j = 0; j < mp->totloop; j++) {
      data_to_export->faces_list[i].vertex_references.push_back((ml + j)->v + 1);
      data_to_export->faces_list[i].vertex_normal_references.push_back(i + 1);
    }
  }
}

bool exporter_main(bContext *C, OBJExportParams *export_params)
{

  const char *filepath = export_params->filepath;

  export_params->C = C;
  export_params->depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  export_params->scene = CTX_data_scene(C);
  struct OBJ_data_to_export data_to_export;

  prepare_vertices(export_params, &data_to_export);
  write_prepared_data(filepath, &data_to_export);
  return true;
}
