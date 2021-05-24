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

#include "BLI_kdopbvh.h"
#include "BLI_kdtree.h"
#include "BLI_task.hh"
#include "BLI_timeit.hh"

#include "DNA_mesh_types.h"

#include "BKE_bvhutils.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

static void geo_node_attribute_processor_layout(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;
  uiLayout *row = uiLayoutRow(layout, true);
  uiTemplateIDBrowse(
      row, C, ptr, "node_tree", nullptr, nullptr, nullptr, UI_TEMPLATE_ID_FILTER_ALL, nullptr);
  uiItemStringO(row, "", ICON_PLUS, "node.new_attribute_processor_group", "node_name", node->name);

  uiItemR(layout, ptr, "domain", 0, "Domain", ICON_NONE);
}

static void geo_node_attribute_processor_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeGeometryAttributeProcessor *node_storage = (NodeGeometryAttributeProcessor *)MEM_callocN(
      sizeof(NodeGeometryAttributeProcessor), __func__);

  node->storage = node_storage;
}

namespace blender::nodes {

static void geo_node_attribute_processor_group_update(bNodeTree *ntree, bNode *node)
{
  if (node->id == nullptr) {
    nodeRemoveAllSockets(ntree, node);
    nodeAddSocket(ntree, node, SOCK_IN, "NodeSocketGeometry", "Geometry", "Geometry");
    nodeAddSocket(ntree, node, SOCK_OUT, "NodeSocketGeometry", "Geometry", "Geometry");
    return;
  }
  if ((ID_IS_LINKED(node->id) && (node->id->tag & LIB_TAG_MISSING))) {
    /* Missing datablock, leave sockets unchanged so that when it comes back
     * the links remain valid. */
    return;
  }
  nodeRemoveAllSockets(ntree, node);
  nodeAddSocket(ntree, node, SOCK_IN, "NodeSocketGeometry", "Geometry", "Geometry");
  nodeAddSocket(ntree, node, SOCK_OUT, "NodeSocketGeometry", "Geometry", "Geometry");

  bNodeTree *ngroup = (bNodeTree *)node->id;
  LISTBASE_FOREACH (bNodeSocket *, interface_sock, &ngroup->inputs) {
    nodeAddSocket(ntree,
                  node,
                  SOCK_IN,
                  interface_sock->idname,
                  interface_sock->identifier,
                  interface_sock->name);
  }
  LISTBASE_FOREACH (bNodeSocket *, interface_sock, &ngroup->outputs) {
    nodeAddSocket(ntree,
                  node,
                  SOCK_OUT,
                  interface_sock->idname,
                  interface_sock->identifier,
                  interface_sock->name);
  }
}

static void geo_node_attribute_processor_storage_free(bNode *node)
{
  NodeGeometryAttributeProcessor *storage = (NodeGeometryAttributeProcessor *)node->storage;

  LISTBASE_FOREACH_MUTABLE (AttributeProcessorInput *, input, &storage->inputs) {
    MEM_freeN(input->identifier);
    MEM_freeN(input);
  }
  BLI_listbase_clear(&storage->inputs);
  LISTBASE_FOREACH_MUTABLE (AttributeProcessorOutput *, output, &storage->outputs) {
    MEM_freeN(output->identifier);
    MEM_freeN(output);
  }
  BLI_listbase_clear(&storage->outputs);
  MEM_freeN(storage);
}

static void geo_node_attribute_processor_storage_copy(bNodeTree *UNUSED(dest_ntree),
                                                      bNode *dst_node,
                                                      const bNode *src_node)
{
  const NodeGeometryAttributeProcessor *src_storage = (const NodeGeometryAttributeProcessor *)
                                                          src_node->storage;
  NodeGeometryAttributeProcessor *dst_storage = (NodeGeometryAttributeProcessor *)MEM_callocN(
      sizeof(NodeGeometryAttributeProcessor), __func__);

  *dst_storage = *src_storage;

  BLI_listbase_clear(&dst_storage->inputs);
  LISTBASE_FOREACH (const AttributeProcessorInput *, src_input, &src_storage->inputs) {
    AttributeProcessorInput *dst_input = (AttributeProcessorInput *)MEM_callocN(
        sizeof(AttributeProcessorInput), __func__);
    *dst_input = *src_input;
    dst_input->identifier = BLI_strdup(src_input->identifier);
    BLI_addtail(&dst_storage->inputs, dst_input);
  }

  BLI_listbase_clear(&dst_storage->outputs);
  LISTBASE_FOREACH (const AttributeProcessorOutput *, src_output, &src_storage->outputs) {
    AttributeProcessorOutput *dst_output = (AttributeProcessorOutput *)MEM_callocN(
        sizeof(AttributeProcessorOutput), __func__);
    *dst_output = *src_output;
    dst_output->identifier = BLI_strdup(src_output->identifier);
    BLI_addtail(&dst_storage->outputs, dst_output);
  }

  dst_node->storage = dst_storage;
}

static void geo_node_attribute_processor_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  geometry_set = geometry_set_realize_instances(geometry_set);
  params.set_output("Geometry", geometry_set);
}

}  // namespace blender::nodes

void register_node_type_geo_attribute_processor()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_ATTRIBUTE_PROCESSOR, "Attribute Processor", NODE_CLASS_GROUP, 0);
  node_type_init(&ntype, geo_node_attribute_processor_init);
  node_type_storage(&ntype,
                    "NodeGeometryAttributeProcessor",
                    blender::nodes::geo_node_attribute_processor_storage_free,
                    blender::nodes::geo_node_attribute_processor_storage_copy);
  node_type_group_update(&ntype, blender::nodes::geo_node_attribute_processor_group_update);
  ntype.geometry_node_execute = blender::nodes::geo_node_attribute_processor_exec;
  ntype.draw_buttons = geo_node_attribute_processor_layout;
  nodeRegisterType(&ntype);
}
