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

#ifndef __WAVEFRONT_OBJ_IM_MTL_HH__
#define __WAVEFRONT_OBJ_IM_MTL_HH__

#include "MEM_guardedalloc.h"
#include <memory>

#include "BKE_lib_id.h"
#include "BKE_node.h"

#include "BLI_map.hh"
#include "BLI_string_ref.hh"

#include "wavefront_obj_im_objects.hh"

namespace blender::io::obj {
struct UniqueNodeDeleter {
  void operator()(bNode *node)
  {
    MEM_freeN(node);
  }
};

struct UniqueNodetreeDeleter {
  void operator()(bNodeTree *node)
  {
    MEM_freeN(node);
  }
};

using unique_node_ptr = std::unique_ptr<bNode, UniqueNodeDeleter>;
using unique_nodetree_ptr = std::unique_ptr<bNodeTree, UniqueNodetreeDeleter>;

class ShaderNodetreeWrap {
 private:
  unique_nodetree_ptr nodetree_;
  unique_node_ptr bsdf_;
  unique_node_ptr shader_output_;
  const MTLMaterial *mtl_mat_;

 public:
  ShaderNodetreeWrap(Main *bmain, const MTLMaterial &mtl_mat);
  ~ShaderNodetreeWrap();

  bNodeTree *get_nodetree();

 private:
  bNode *add_node_to_tree(const int node_type);
  void link_sockets(unique_node_ptr src_node, StringRef src_id, bNode *dst_node, StringRef dst_id);
  void set_bsdf_socket_values();
  void add_image_textures(Main *bmain);
};
}  // namespace blender::io::obj

#endif
