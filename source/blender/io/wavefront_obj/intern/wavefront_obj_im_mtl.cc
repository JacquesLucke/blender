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

#include "BKE_node.h"

#include "BLI_map.hh"
#include "BLI_string_utf8.h"

#include "DNA_node_types.h"

#include "NOD_shader.h"

#include "wavefront_obj_im_mtl.hh"

namespace blender::io::obj {

/**
 * Set the socket's (of given ID) value to the given number(s).
 * Only float value(s) can be set using this method.
 */
static void set_property_of_socket(eNodeSocketDatatype property_type,
                                   StringRef socket_id,
                                   Span<float> value,
                                   bNode *r_node)
{
  BLI_assert(r_node);
  bNodeSocket *socket{nodeFindSocket(r_node, SOCK_IN, socket_id.data())};
  BLI_assert(socket && socket->type == property_type);
  switch (property_type) {
    case SOCK_FLOAT: {
      BLI_assert(value.size() == 1);
      static_cast<bNodeSocketValueFloat *>(socket->default_value)->value = value[0];
      break;
    }
    case SOCK_RGBA: {
      /* Alpha will be manually added. It is not read from the MTL file either. */
      BLI_assert(value.size() == 3);
      copy_v3_v3(static_cast<bNodeSocketValueRGBA *>(socket->default_value)->value, value.data());
      static_cast<bNodeSocketValueRGBA *>(socket->default_value)->value[3] = 1.0f;
      break;
    }
    case SOCK_VECTOR: {
      BLI_assert(value.size() == 3);
      copy_v4_v4(static_cast<bNodeSocketValueVector *>(socket->default_value)->value,
                 value.data());
      break;
    }
    default: {
      BLI_assert(0);
      break;
    }
  }
}

/**
 * Set the filepath in the Image Texture Node.
 */
static void set_img_filepath(StringRef value, bNode *r_node)
{
  BLI_assert(r_node);
  Image *tex_image = reinterpret_cast<Image *>(r_node->id);
  BLI_assert(tex_image);
  BLI_strncpy_utf8(tex_image->filepath, value.data(), 1024);
}

/**
 * Initialises a nodetree with a p-BSDF node's BSDF socket connected to shader output node's
 * surface socket.
 */
ShaderNodetreeWrap::ShaderNodetreeWrap(const MTLMaterial &mtl_mat)
{
  nodetree_.reset(ntreeAddTree(nullptr, "Shader Nodetree", ntreeType_Shader->idname));
  bsdf_.reset(add_node_to_tree(SH_NODE_BSDF_PRINCIPLED));
  shader_output_.reset(add_node_to_tree(SH_NODE_OUTPUT_MATERIAL));

  set_bsdf_socket_values(mtl_mat);
  add_image_textures(mtl_mat);
  link_sockets(std::move(bsdf_), "BSDF", shader_output_.get(), "Surface");

  nodeSetActive(nodetree_.get(), shader_output_.get());
}

ShaderNodetreeWrap::~ShaderNodetreeWrap()
{
  /* If the destructor has been reached, we know that nodes and the nodetree
   * have been added to the scene. */
  shader_output_.release();
  if (nodetree_) {
    /* nodetree's ownership must be acquired by the caller. */
    BLI_assert(0);
  }
}

/**
 * Release nodetree for materials to own it. nodetree has its unique deleter
 * if destructor is not reached for some reason.
 */
bNodeTree *ShaderNodetreeWrap::get_nodetree()
{
  return nodetree_.release();
}

/**
 * Add a new static node to the tree.
 * No two nodes are linked here.
 */
bNode *ShaderNodetreeWrap::add_node_to_tree(const int node_type)
{
  return nodeAddStaticNode(nullptr, nodetree_.get(), node_type);
}

/**
 * Link two nodes by the sockets of given IDs.
 */
void ShaderNodetreeWrap::link_sockets(unique_node_ptr src_node,
                                      StringRef src_id,
                                      bNode *dst_node,
                                      StringRef dst_id)
{
  nodeAddLink(nodetree_.get(),
              src_node.get(),
              nodeFindSocket(src_node.get(), SOCK_OUT, src_id.data()),
              dst_node,
              nodeFindSocket(dst_node, SOCK_IN, dst_id.data()));
  src_node.release();
}

void ShaderNodetreeWrap::set_bsdf_socket_values(const MTLMaterial &mtl_mat)
{
  set_property_of_socket(SOCK_FLOAT, "Specular", {mtl_mat.Ns}, bsdf_.get());
  /* Only one value is taken for Metallic. */
  set_property_of_socket(SOCK_FLOAT, "Metallic", {mtl_mat.Ka[0]}, bsdf_.get());
  set_property_of_socket(SOCK_FLOAT, "IOR", {mtl_mat.Ni}, bsdf_.get());
  set_property_of_socket(SOCK_FLOAT, "Alpha", {mtl_mat.d}, bsdf_.get());
  set_property_of_socket(SOCK_RGBA, "Base Color", {mtl_mat.Kd, 3}, bsdf_.get());
  set_property_of_socket(SOCK_RGBA, "Emission", {mtl_mat.Ke, 3}, bsdf_.get());
}

void ShaderNodetreeWrap::add_image_textures(const MTLMaterial &mtl_mat)
{

  for (const eTextureMapType map_type : mtl_mat.all_tex_map_types()) {
    const tex_map_XX &texture_map = mtl_mat.tex_map_of_type(map_type);
    if (texture_map.image_path.empty()) {
    }
    unique_node_ptr tex_node{add_node_to_tree(SH_NODE_TEX_IMAGE)};
    unique_node_ptr vector_node{add_node_to_tree(SH_NODE_MAPPING)};
    unique_node_ptr normal_map_node = nullptr;
    if (map_type == MAP_BUMP) {
      normal_map_node.reset(add_node_to_tree(SH_NODE_MAPPING));
      set_property_of_socket(
          SOCK_FLOAT, "Strength", {mtl_mat.map_Bump_value}, normal_map_node.get());
    }

    set_img_filepath(texture_map.image_path, tex_node.get());
    set_property_of_socket(
        SOCK_VECTOR, "Location", {texture_map.translation, 3}, vector_node.get());
    set_property_of_socket(SOCK_VECTOR, "Scale", {texture_map.scale, 3}, vector_node.get());

    link_sockets(std::move(vector_node), "Mapping", tex_node.get(), "Vector");
    if (normal_map_node) {
      link_sockets(std::move(tex_node), "Color", normal_map_node.get(), "Color");
      link_sockets(std::move(normal_map_node), "Normal", bsdf_.get(), "Normal");
    }
    else {
      link_sockets(std::move(tex_node), "Color", bsdf_.get(), "Color");
    }
  }
}
}  // namespace blender::io::obj
