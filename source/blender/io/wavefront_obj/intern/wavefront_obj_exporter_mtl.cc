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

#include "BKE_material.h"
#include "BKE_node.h"
#include "BKE_node_tree_ref.hh"

#include "BLI_map.hh"
#include "BLI_math.h"

#include "DNA_material_types.h"
#include "DNA_node_types.h"

#include "wavefront_obj_exporter_mtl.hh"

using namespace BKE;
namespace blender {
namespace io {
namespace obj {

/**
 * Find and initialiase the Principled-BSDF from the object's material.
 */
void MTLWriter::init_bsdf_node()
{
  if (_export_mtl->use_nodes == false) {
    fprintf(stderr, "No Principled-BSDF node found in the material Node tree.\n");
    _bsdf_node = nullptr;
    return;
  }
  ListBase *nodes = &_export_mtl->nodetree->nodes;
  LISTBASE_FOREACH (bNode *, curr_node, nodes) {
    if (curr_node->typeinfo->type == SH_NODE_BSDF_PRINCIPLED) {
      _bsdf_node = curr_node;
      return;
    }
  }
  fprintf(stderr, "No Principled-BSDF node found in the material Node tree.\n");
  _bsdf_node = nullptr;
}

/**
 * Copy the float property from the bNode to given buffer.
 */
void MTLWriter::float_property_from_node(float *r_property,
                                         const bNode *curr_node,
                                         const char *identifier)
{
  if (!_bsdf_node) {
    return;
  }
  bNodeSocket *socket = (bNodeSocket *)nodeFindSocket(_bsdf_node, SOCK_IN, identifier);
  if (socket) {
    bNodeSocketValueFloat *socket_def_value = (bNodeSocketValueFloat *)socket->default_value;
    *r_property = socket_def_value->value;
  }
}

/**
 * Copy the float3 property from the bNode to given buffer.
 * Mostly used for color, without alpha.
 */
void MTLWriter::float3_property_from_node(float *r_property,
                                          const bNode *curr_node,
                                          const char *identifier)
{
  if (!_bsdf_node) {
    return;
  }
  bNodeSocket *socket = (bNodeSocket *)nodeFindSocket(_bsdf_node, SOCK_IN, identifier);
  if (socket) {
    bNodeSocketValueRGBA *socket_def_value = (bNodeSocketValueRGBA *)socket->default_value;
    copy_v3_v3(r_property, socket_def_value->value);
  }
}

/**
 * Collect all the source sockets linked to the destination socket in a destination node.
 */
void MTLWriter::linked_sockets_to_dest_id(
    blender::Vector<const OutputSocketRef *> *r_linked_sockets,
    const bNode *dest_node,
    NodeTreeRef &node_tree,
    const char *dest_socket_id)
{
  if (!dest_node) {
    return;
  }
  Span<const NodeRef *> object_dest_nodes = node_tree.nodes_with_idname(dest_node->idname);
  Span<const InputSocketRef *> dest_inputs = object_dest_nodes.first()->inputs();
  const InputSocketRef *dest_socket = nullptr;
  for (const InputSocketRef *curr_socket : dest_inputs) {
    if (STREQ(curr_socket->bsocket()->identifier, dest_socket_id)) {
      dest_socket = curr_socket;
    }
  }
  if (dest_socket) {
    Span<const OutputSocketRef *> linked_sockets = dest_socket->directly_linked_sockets();
    r_linked_sockets->resize(linked_sockets.size());
    for (uint i = 0; i < linked_sockets.size(); i++) {
      (*r_linked_sockets)[i] = linked_sockets[i];
    }
  }
}

/**
 * From a list of sockets, get the parent node which is of the given node type.
 */
const bNode *MTLWriter::linked_node_of_type(
    const blender::Vector<const OutputSocketRef *> &sockets_list, uint sh_node_type)
{
  for (const SocketRef *sock : sockets_list) {
    const bNode *curr_node = sock->bnode();
    if (curr_node->typeinfo->type == sh_node_type) {
      return curr_node;
    }
  }
  return nullptr;
}

/**
 * From a texture image shader node, get the image's filepath.
 * Filepath is the exact string the node contains, relative or absolute.
 */
const char *MTLWriter::get_image_filepath(const bNode *tex_node)
{
  if (tex_node) {
    Image *tex_image = (Image *)tex_node->id;
    return tex_image->filepath;
  }
  return nullptr;
}

/** Append an object's material to the .mtl file. */
void MTLWriter::append_material(OBJMesh &mesh_to_export)
{
  _mtl_outfile = fopen(_mtl_filepath, "a");
  if (!_mtl_outfile) {
    fprintf(stderr, "Error in opening file at %s\n", _mtl_filepath);
    return;
  }

  _export_mtl = mesh_to_export.export_object_material();
  if (!_export_mtl) {
    fprintf(stderr, "No material active for the object.\n");
    return;
  }
  fprintf(_mtl_outfile, "\nnewmtl %s\n", _export_mtl->id.name + 2);

  init_bsdf_node();

  /* Empirical, and copied from original python exporter. */
  float spec_exponent = (1.0 - _export_mtl->roughness) * 30;
  spec_exponent *= spec_exponent;
  float specular = _export_mtl->spec;
  float_property_from_node(&specular, _bsdf_node, "Specular");
  float metallic = _export_mtl->metallic;
  float_property_from_node(&metallic, _bsdf_node, "Metallic");
  float refraction_index = 1.0f;
  float_property_from_node(&refraction_index, _bsdf_node, "IOR");
  float dissolved = _export_mtl->a;
  float_property_from_node(&dissolved, _bsdf_node, "Alpha");
  bool transparent = dissolved != 1.0f;

  float diffuse_col[3] = {_export_mtl->r, _export_mtl->g, _export_mtl->b};
  float_property_from_node(diffuse_col, _bsdf_node, "Base Color");
  float emission_col[3] = {0.0f, 0.0f, 0.0f};
  float3_property_from_node(emission_col, _bsdf_node, "Emission");

  /* See https://wikipedia.org/wiki/Wavefront_.obj_file for all possible values of illum. */
  /* Highlight on. */
  int illum = 2;
  if (!specular) {
    /* Color on and Ambient on. */
    illum = 1;
  }
  else if (metallic) {
    /* Metallic ~= Reflection. */
    if (transparent) {
      /* Transparency: Refraction on, Reflection: ~~Fresnel off and Ray trace~~ on. */
      illum = 6;
    }
    else {
      /* Reflection on and Ray trace on. */
      illum = 3;
    }
  }
  else if (transparent) {
    /* Transparency: Glass on, Reflection: Ray trace off */
    illum = 9;
  }

  fprintf(_mtl_outfile, "Ns %.6f\n", spec_exponent);
  fprintf(_mtl_outfile, "Ka %.6f %.6f %.6f\n", metallic, metallic, metallic);
  fprintf(_mtl_outfile, "Kd %.6f %.6f %.6f\n", diffuse_col[0], diffuse_col[1], diffuse_col[2]);
  fprintf(_mtl_outfile, "Ks %0.6f %0.6f %0.6f\n", specular, specular, specular);
  fprintf(
      _mtl_outfile, "Ke %0.6f %0.6f %0.6f\n", emission_col[0], emission_col[1], emission_col[2]);
  fprintf(_mtl_outfile, "Ni %0.6f\n", refraction_index);
  fprintf(_mtl_outfile, "d %.6f\n", dissolved);
  fprintf(_mtl_outfile, "illum %d\n", illum);

  /* Image Textures */
  blender::Map<const char *, const char *> texture_map_types;
  texture_map_types.add("map_Kd", "Base Color");
  texture_map_types.add("map_Ks", "Specular");
  texture_map_types.add("map_Ns", "Roughness");
  texture_map_types.add("map_d", "Alpha");
  texture_map_types.add("map_refl", "Metallic");
  texture_map_types.add("map_Ke", "Emission");

  const char *tex_image_filepath = nullptr;
  const bNode *tex_node;
  /* Need to create a NodeTreeRef for a faster way to find which two sockets are linked, as
   * compared to looping over all links in the node tree to match with two sockets of out interest.
   */
  NodeTreeRef node_tree(_export_mtl->nodetree);
  blender::Vector<const OutputSocketRef *> linked_sockets;
  for (auto const &map_type : texture_map_types.keys()) {
    /* Find sockets linked to destination socket of interest in p-bsdf node. */
    linked_sockets_to_dest_id(
        &linked_sockets, _bsdf_node, node_tree, texture_map_types.lookup(map_type));
    /* From the linked sockets, find Image Texture shader node. */
    tex_node = linked_node_of_type(linked_sockets, SH_NODE_TEX_IMAGE);
    tex_image_filepath = get_image_filepath(tex_node);
    if (tex_image_filepath) {
      fprintf(_mtl_outfile, "%s %s\n", map_type, tex_image_filepath);
    }
  }

  /* Find sockets linked to destination "Normal" socket in p-bsdf node. */
  linked_sockets_to_dest_id(&linked_sockets, _bsdf_node, node_tree, "Normal");
  /* From the linked sockets, find Normal Map shader node. */
  const bNode *normal_map_node = linked_node_of_type(linked_sockets, SH_NODE_NORMAL_MAP);
  /* Find sockets linked to "Color" socket in normal map node. */
  linked_sockets_to_dest_id(&linked_sockets, normal_map_node, node_tree, "Color");
  /* From the linked sockets, find Image Texture shader node. */
  tex_node = linked_node_of_type(linked_sockets, SH_NODE_TEX_IMAGE);
  tex_image_filepath = get_image_filepath(tex_node);
  if (tex_image_filepath) {
    fprintf(_mtl_outfile, "map_Bump %s", tex_image_filepath);
    float normal_map_strength = 1.0;
    float_property_from_node(&normal_map_strength, normal_map_node, "Strength");
    fprintf(_mtl_outfile, "-bm %f\n", normal_map_strength);
  }

  fclose(_mtl_outfile);
}

}  // namespace obj
}  // namespace io
}  // namespace blender
