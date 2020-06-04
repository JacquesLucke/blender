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
 */

/** \file
 * \ingroup obj
 */

#ifndef __IO_WAVEFRONT_OBJ_H__
#define __IO_WAVEFRONT_OBJ_H__

#include "BKE_context.h"

#ifdef __cplusplus
#  include <vector>
struct faces {
  int total_vertices_per_face;
  std::vector<int> vertex_references;
  std::vector<int> vertex_normal_references;
};

struct OBJ_data_to_export {
  int tot_vertices;
  std::vector<struct MVert> vertices;
  std::vector<std::array<float, 3>> normals;
  int tot_faces;
  std::vector<struct faces> faces_list;
};
extern "C" {
#endif
  
  struct OBJExportParams {
    const char *filepath;
    
    bContext *C;
    Depsgraph *depsgraph;
    Scene *scene;
    
    bool print_name;
    float number;
  };
  struct OBJImportParams {
    bool print_name;
    float number;
  };
  
  bool OBJ_import(struct bContext *C, const char *filepath, struct OBJImportParams *import_params);
  
  bool OBJ_export(struct bContext *C, struct OBJExportParams *export_params);
  
#ifdef __cplusplus
}
#endif

#endif /* __IO_WAVEFRONT_OBJ_H__ */
