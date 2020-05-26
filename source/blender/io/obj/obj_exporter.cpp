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

#include <stdio.h>

#include "obj_file_handler.h"
#include "obj_exporter.h"

#include "BKE_context.h"
#include "BLI_linklist.h"
#include "BKE_object.h"
#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"
#include "DNA_layer_types.h"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

bool exporter_main(bContext *C, const char *filepath){
  
  const Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object *ob = CTX_data_active_object(C);
  Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
  
  printf("\n%d\n", ob_eval->type & OB_MESH);
  Mesh *me_eval = BKE_object_get_evaluated_mesh(ob_eval);
  int numVerts = me_eval->totvert;
  for (int i=0; i < numVerts; i++) {
    for (int j=0 ; j<3; j++) {
      printf("%f ", me_eval->mvert[i].co[j]);
    }
    printf("\n");
  }
  
  return  true;
}
