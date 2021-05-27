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

#include "NOD_node_tree_multi_function.hh"

#include "FN_multi_function_network_evaluation.hh"

#include "node_geometry_util.hh"

static void geo_node_attribute_processor_layout(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;
  NodeGeometryAttributeProcessor *storage = (NodeGeometryAttributeProcessor *)node->storage;

  uiLayout *row = uiLayoutRow(layout, true);
  uiTemplateIDBrowse(
      row, C, ptr, "node_tree", nullptr, nullptr, nullptr, UI_TEMPLATE_ID_FILTER_ALL, nullptr);
  uiItemStringO(row, "", ICON_PLUS, "node.new_attribute_processor_group", "node_name", node->name);

  uiItemR(layout, ptr, "domain", 0, "Domain", ICON_NONE);

  bNodeTree *group = (bNodeTree *)node->id;
  if (group == nullptr) {
    return;
  }
  {
    uiLayout *box = uiLayoutBox(layout);
    bNodeSocket *interface_socket = (bNodeSocket *)group->inputs.first;
    AttributeProcessorInputSettings *input_settings = (AttributeProcessorInputSettings *)
                                                          storage->inputs_settings.first;
    for (; interface_socket && input_settings;
         interface_socket = interface_socket->next, input_settings = input_settings->next) {
      PointerRNA input_ptr;
      RNA_pointer_create(
          ptr->owner_id, &RNA_AttributeProcessorInputSettings, input_settings, &input_ptr);
      uiItemR(box, &input_ptr, "input_mode", 0, interface_socket->name, ICON_NONE);
    }
  }
  {
    uiLayout *box = uiLayoutBox(layout);
    bNodeSocket *interface_socket = (bNodeSocket *)group->outputs.first;
    AttributeProcessorOutputSettings *output_settings = (AttributeProcessorOutputSettings *)
                                                            storage->outputs_settings.first;
    for (; interface_socket && output_settings;
         interface_socket = interface_socket->next, output_settings = output_settings->next) {
      PointerRNA output_ptr;
      RNA_pointer_create(
          ptr->owner_id, &RNA_AttributeProcessorOutputSettings, output_settings, &output_ptr);
      uiItemR(box, &output_ptr, "output_mode", 0, interface_socket->name, ICON_NONE);
    }
  }
}

static void geo_node_attribute_processor_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeGeometryAttributeProcessor *node_storage = (NodeGeometryAttributeProcessor *)MEM_callocN(
      sizeof(NodeGeometryAttributeProcessor), __func__);

  node->storage = node_storage;
}

namespace blender::nodes {

static void free_group_input(AttributeProcessorInputSettings *input_settings)
{
  MEM_freeN(input_settings->identifier);
  MEM_freeN(input_settings);
}

static void free_group_output(AttributeProcessorOutputSettings *output_settings)
{
  MEM_freeN(output_settings->identifier);
  MEM_freeN(output_settings);
}

static void free_group_inputs(ListBase *inputs_settings)
{
  LISTBASE_FOREACH_MUTABLE (AttributeProcessorInputSettings *, input_settings, inputs_settings) {
    free_group_input(input_settings);
  }
  BLI_listbase_clear(inputs_settings);
}

static void free_group_outputs(ListBase *outputs_settings)
{
  LISTBASE_FOREACH_MUTABLE (
      AttributeProcessorOutputSettings *, output_settings, outputs_settings) {
    free_group_output(output_settings);
  }
  BLI_listbase_clear(outputs_settings);
}

static void geo_node_attribute_processor_storage_free(bNode *node)
{
  NodeGeometryAttributeProcessor *storage = (NodeGeometryAttributeProcessor *)node->storage;
  free_group_inputs(&storage->inputs_settings);
  free_group_outputs(&storage->outputs_settings);
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

  BLI_listbase_clear(&dst_storage->inputs_settings);
  LISTBASE_FOREACH (
      const AttributeProcessorInputSettings *, src_input_settings, &src_storage->inputs_settings) {
    AttributeProcessorInputSettings *dst_input_settings = (AttributeProcessorInputSettings *)
        MEM_callocN(sizeof(AttributeProcessorInputSettings), __func__);
    *dst_input_settings = *src_input_settings;
    dst_input_settings->identifier = BLI_strdup(src_input_settings->identifier);
    BLI_addtail(&dst_storage->inputs_settings, dst_input_settings);
  }

  BLI_listbase_clear(&dst_storage->outputs_settings);
  LISTBASE_FOREACH (const AttributeProcessorOutputSettings *,
                    src_output_settings,
                    &src_storage->outputs_settings) {
    AttributeProcessorOutputSettings *dst_output_settings = (AttributeProcessorOutputSettings *)
        MEM_callocN(sizeof(AttributeProcessorOutputSettings), __func__);
    *dst_output_settings = *src_output_settings;
    dst_output_settings->identifier = BLI_strdup(src_output_settings->identifier);
    BLI_addtail(&dst_storage->outputs_settings, dst_output_settings);
  }

  dst_node->storage = dst_storage;
}

static void geo_node_attribute_processor_group_update(bNodeTree *ntree, bNode *node)
{
  NodeGeometryAttributeProcessor *storage = (NodeGeometryAttributeProcessor *)node->storage;
  bNodeTree *ngroup = (bNodeTree *)node->id;

  if (ngroup == nullptr) {
    nodeRemoveAllSockets(ntree, node);
    nodeAddSocket(ntree, node, SOCK_IN, "NodeSocketGeometry", "Geometry", "Geometry");
    nodeAddSocket(ntree, node, SOCK_OUT, "NodeSocketGeometry", "Geometry", "Geometry");
    return;
  }
  if ((ID_IS_LINKED(ngroup) && (ngroup->id.tag & LIB_TAG_MISSING))) {
    /* Missing datablock, leave sockets unchanged so that when it comes back
     * the links remain valid. */
    return;
  }
  nodeRemoveAllSockets(ntree, node);
  nodeAddSocket(ntree, node, SOCK_IN, "NodeSocketGeometry", "Geometry", "Geometry");
  nodeAddSocket(ntree, node, SOCK_OUT, "NodeSocketGeometry", "Geometry", "Geometry");

  free_group_inputs(&storage->inputs_settings);
  free_group_outputs(&storage->outputs_settings);

  LISTBASE_FOREACH (bNodeSocket *, interface_sock, &ngroup->inputs) {
    AttributeProcessorInputSettings *input_settings = (AttributeProcessorInputSettings *)
        MEM_callocN(sizeof(AttributeProcessorInputSettings), __func__);
    input_settings->identifier = BLI_strdup(interface_sock->identifier);
    BLI_addtail(&storage->inputs_settings, input_settings);

    char identifier1[MAX_NAME];
    char identifier2[MAX_NAME];
    BLI_snprintf(identifier1, sizeof(identifier1), "inA%s", interface_sock->identifier);
    BLI_snprintf(identifier2, sizeof(identifier2), "inB%s", interface_sock->identifier);
    nodeAddSocket(ntree, node, SOCK_IN, interface_sock->idname, identifier1, interface_sock->name);
    nodeAddSocket(ntree, node, SOCK_IN, "NodeSocketString", identifier2, interface_sock->name);
  }
  LISTBASE_FOREACH (bNodeSocket *, interface_sock, &ngroup->outputs) {
    AttributeProcessorOutputSettings *output_settings = (AttributeProcessorOutputSettings *)
        MEM_callocN(sizeof(AttributeProcessorOutputSettings), __func__);
    output_settings->identifier = BLI_strdup(interface_sock->identifier);
    char identifier[MAX_NAME];
    BLI_snprintf(identifier, sizeof(identifier), "out%s", interface_sock->identifier);
    BLI_addtail(&storage->outputs_settings, output_settings);
    nodeAddSocket(ntree, node, SOCK_IN, "NodeSocketString", identifier, interface_sock->name);
  }
}

static void geo_node_attribute_processor_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeGeometryAttributeProcessor *storage = (NodeGeometryAttributeProcessor *)node->storage;
  bNodeTree *group = (bNodeTree *)node->id;
  if (group == nullptr) {
    return;
  }

  bNodeSocket *next_socket = (bNodeSocket *)node->inputs.first;
  /* Skip geometry socket. */
  next_socket = next_socket->next;
  LISTBASE_FOREACH (AttributeProcessorInputSettings *, input_settings, &storage->inputs_settings) {
    bNodeSocket *value_socket = next_socket;
    bNodeSocket *attribute_socket = value_socket->next;
    nodeSetSocketAvailability(value_socket,
                              input_settings->input_mode ==
                                  GEO_NODE_ATTRIBUTE_PROCESSOR_INPUT_MODE_CUSTOM_VALUE);
    nodeSetSocketAvailability(attribute_socket,
                              input_settings->input_mode ==
                                  GEO_NODE_ATTRIBUTE_PROCESSOR_INPUT_MODE_CUSTOM_ATTRIBUTE);
    next_socket = attribute_socket->next;
  }
  LISTBASE_FOREACH (
      AttributeProcessorOutputSettings *, output_settings, &storage->outputs_settings) {
    nodeSetSocketAvailability(next_socket,
                              output_settings->output_mode ==
                                  GEO_NODE_ATTRIBUTE_PROCESSOR_OUTPUT_MODE_CUSTOM);
    next_socket = next_socket->next;
  }
}

static CustomDataType get_custom_data_type(bNodeSocketType *typeinfo)
{
  switch (typeinfo->type) {
    case SOCK_FLOAT:
      return CD_PROP_FLOAT;
    case SOCK_VECTOR:
      return CD_PROP_FLOAT3;
    case SOCK_RGBA:
      return CD_PROP_COLOR;
    case SOCK_BOOLEAN:
      return CD_PROP_BOOL;
    case SOCK_INT:
      return CD_PROP_INT32;
  }
  BLI_assert_unreachable();
  return CD_PROP_FLOAT;
}

static void process_attributes(GeoNodeExecParams &geo_params, GeometrySet &geometry_set)
{
  const bNode &node = geo_params.node();
  const NodeGeometryAttributeProcessor &storage = *(NodeGeometryAttributeProcessor *)node.storage;
  bNodeTree *group = (bNodeTree *)node.id;
  const AttributeDomain domain = (AttributeDomain)storage.domain;

  if (group == nullptr) {
    return;
  }

  geometry_set = geometry_set_realize_instances(geometry_set);

  if (!geometry_set.has_mesh()) {
    return;
  }
  GeometryComponent &component = geometry_set.get_component_for_write<MeshComponent>();
  const int domain_size = component.attribute_domain_size(domain);
  if (domain_size == 0) {
    return;
  }

  NodeTreeRefMap tree_refs;
  DerivedNodeTree tree{*group, tree_refs};
  fn::MFNetwork network;
  ResourceScope scope;
  MFNetworkTreeMap network_map = insert_node_tree_into_mf_network(network, tree, scope);

  Vector<const fn::MFOutputSocket *> fn_input_sockets;
  Vector<const fn::MFInputSocket *> fn_output_sockets;

  const DTreeContext &root_context = tree.root_context();
  const NodeTreeRef &root_tree_ref = root_context.tree();

  Span<const NodeRef *> input_nodes = root_tree_ref.nodes_by_type("NodeGroupInput");
  Span<const NodeRef *> output_nodes = root_tree_ref.nodes_by_type("NodeGroupOutput");

  if (output_nodes.size() != 1) {
    return;
  }

  const DNode output_node{&root_context, output_nodes[0]};
  Vector<fn::MFInputSocket *> network_outputs;
  for (const InputSocketRef *socket_ref : output_node->inputs().drop_back(1)) {
    const DInputSocket socket{&root_context, socket_ref};
    network_outputs.append(network_map.lookup(socket).first());
  }

  VectorSet<const fn::MFOutputSocket *> network_inputs;
  VectorSet<const fn::MFInputSocket *> unlinked_inputs;
  network.find_dependencies(network_outputs, network_inputs, unlinked_inputs);
  BLI_assert(unlinked_inputs.is_empty());

  Vector<DOutputSocket> used_group_inputs;
  for (const fn::MFOutputSocket *dummy_socket : network_inputs) {
    const DOutputSocket dsocket = network_map.try_lookup(*dummy_socket);
    BLI_assert(dsocket);
    used_group_inputs.append(dsocket);
  }

  fn::MFNetworkEvaluator network_fn{Vector<const fn::MFOutputSocket *>(network_inputs.as_span()),
                                    Vector<const fn::MFInputSocket *>(network_outputs.as_span())};

  fn::MFParamsBuilder fn_params{network_fn, domain_size};
  fn::MFContextBuilder context;

  Vector<GVArrayPtr> input_gvarrays;

  for (const DOutputSocket &dsocket : used_group_inputs) {
    const int index = dsocket->index();
    const AttributeProcessorInputSettings *input_settings = (AttributeProcessorInputSettings *)
        BLI_findlink(&storage.inputs_settings, index);
    const bNodeSocket *interface_socket = (bNodeSocket *)BLI_findlink(&group->inputs, index);
    switch ((GeometryNodeAttributeProcessorInputMode)input_settings->input_mode) {
      case GEO_NODE_ATTRIBUTE_PROCESSOR_INPUT_MODE_DEFAULT: {
        const StringRefNull attribute_name = interface_socket->name;
        const CustomDataType type = get_custom_data_type(interface_socket->typeinfo);
        GVArrayPtr attribute = component.attribute_get_for_read(attribute_name, domain, type);
        fn_params.add_readonly_single_input(*attribute);
        input_gvarrays.append(std::move(attribute));
        break;
      }
      case GEO_NODE_ATTRIBUTE_PROCESSOR_INPUT_MODE_CUSTOM_ATTRIBUTE: {
        return;
        break;
      }
      case GEO_NODE_ATTRIBUTE_PROCESSOR_INPUT_MODE_CUSTOM_VALUE: {
        return;
        break;
      }
    }
  }

  Vector<std::unique_ptr<OutputAttribute>> output_attributes;
  for (const InputSocketRef *socket_ref : output_node->inputs().drop_back(1)) {
    const DInputSocket socket{&root_context, socket_ref};
    const int index = socket->index();
    const AttributeProcessorOutputSettings *output_settings = (AttributeProcessorOutputSettings *)
        BLI_findlink(&storage.outputs_settings, index);
    const bNodeSocket *interface_socket = (bNodeSocket *)BLI_findlink(&group->outputs, index);
    switch ((GeometryNodeAttributeProcessorOutputMode)output_settings->output_mode) {
      case GEO_NODE_ATTRIBUTE_PROCESSOR_OUTPUT_MODE_DEFAULT: {
        const StringRefNull attribute_name = interface_socket->name;
        const CustomDataType type = get_custom_data_type(interface_socket->typeinfo);
        auto attribute = std::make_unique<OutputAttribute>(
            component.attribute_try_get_for_output_only(attribute_name, domain, type));
        GMutableSpan attribute_span = attribute->as_span();
        /* Destruct because the function expects an uninitialized array. */
        attribute_span.type().destruct_n(attribute_span.data(), domain_size);
        fn_params.add_uninitialized_single_output(attribute_span);
        output_attributes.append(std::move(attribute));
        break;
      }
      case GEO_NODE_ATTRIBUTE_PROCESSOR_OUTPUT_MODE_CUSTOM: {
        return;
      }
    }
  }

  network_fn.call(IndexRange(domain_size), fn_params, context);

  for (std::unique_ptr<OutputAttribute> &output_attribute : output_attributes) {
    output_attribute->save();
  }
}

static void geo_node_attribute_processor_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  process_attributes(params, geometry_set);
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
  node_type_update(&ntype, blender::nodes::geo_node_attribute_processor_update);
  node_type_group_update(&ntype, blender::nodes::geo_node_attribute_processor_group_update);
  ntype.geometry_node_execute = blender::nodes::geo_node_attribute_processor_exec;
  ntype.draw_buttons = geo_node_attribute_processor_layout;
  nodeRegisterType(&ntype);
}
