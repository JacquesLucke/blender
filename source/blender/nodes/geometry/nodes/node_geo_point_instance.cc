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

#include "DNA_collection_types.h"

#include "BLI_hash.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

static bNodeSocketTemplate geo_node_point_instance_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {SOCK_OBJECT, N_("Object")},
    {SOCK_COLLECTION, N_("Collection")},
    {SOCK_INT, N_("Seed"), 0, 0, 0, 0, -10000, 10000},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_point_instance_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

static void geo_node_point_instance_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "instance_type", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
  if (RNA_enum_get(ptr, "instance_type") == GEO_NODE_POINT_INSTANCE_TYPE_COLLECTION) {
    uiItemR(layout, ptr, "use_whole_collection", 0, nullptr, ICON_NONE);
  }
}

namespace blender::nodes {

static void geo_node_point_instance_update(bNodeTree *UNUSED(tree), bNode *node)
{
  bNodeSocket *object_socket = (bNodeSocket *)BLI_findlink(&node->inputs, 1);
  bNodeSocket *collection_socket = object_socket->next;
  bNodeSocket *seed_socket = collection_socket->next;

  NodeGeometryPointInstance *node_storage = (NodeGeometryPointInstance *)node->storage;
  GeometryNodePointInstanceType type = (GeometryNodePointInstanceType)node_storage->instance_type;
  const bool use_whole_collection = (node_storage->flag &
                                     GEO_NODE_POINT_INSTANCE_WHOLE_COLLECTION) != 0;

  nodeSetSocketAvailability(object_socket, type == GEO_NODE_POINT_INSTANCE_TYPE_OBJECT);
  nodeSetSocketAvailability(collection_socket, type == GEO_NODE_POINT_INSTANCE_TYPE_COLLECTION);
  nodeSetSocketAvailability(
      seed_socket, type == GEO_NODE_POINT_INSTANCE_TYPE_COLLECTION && !use_whole_collection);
}

static void get_instance_references__object(const GeoNodeExecParams &params,
                                            MutableSpan<InstanceReference> r_references)
{
  Object *object = params.get_input<Object *>("Object");
  if (object == params.self_object()) {
    object = nullptr;
  }
  if (object != nullptr) {
    r_references.fill(*object);
  }
}

static void get_instance_references__collection(const GeoNodeExecParams &params,
                                                const GeometryComponent &component,
                                                MutableSpan<InstanceReference> r_references)
{
  const bNode &node = params.node();
  NodeGeometryPointInstance *node_storage = (NodeGeometryPointInstance *)node.storage;

  Collection *collection = params.get_input<Collection *>("Collection");
  if (collection == nullptr) {
    return;
  }

  if (BLI_listbase_is_empty(&collection->children) &&
      BLI_listbase_is_empty(&collection->gobject)) {
    params.error_message_add(NodeWarningType::Info, TIP_("Collection is empty"));
    return;
  }

  const bool use_whole_collection = (node_storage->flag &
                                     GEO_NODE_POINT_INSTANCE_WHOLE_COLLECTION) != 0;
  if (use_whole_collection) {
    r_references.fill(*collection);
  }
  else {
    Vector<InstanceReference> possible_references;
    /* Direct child objects are instanced as objects. */
    LISTBASE_FOREACH (CollectionObject *, cob, &collection->gobject) {
      possible_references.append(*cob->ob);
    }
    /* Direct child collections are instanced as collections. */
    LISTBASE_FOREACH (CollectionChild *, child, &collection->children) {
      possible_references.append(*child->collection);
    }

    if (!possible_references.is_empty()) {
      const int seed = params.get_input<int>("Seed");
      Array<uint32_t> ids = get_geometry_element_ids_as_uints(component, ATTR_DOMAIN_POINT);
      for (const int i : r_references.index_range()) {
        const int index = BLI_hash_int_2d(ids[i], seed) % possible_references.size();
        r_references[i] = possible_references[index];
      }
    }
  }
}

static Array<InstanceReference> get_instance_references(const GeoNodeExecParams &params,
                                                        const GeometryComponent &component,
                                                        const int amount)
{
  const bNode &node = params.node();
  NodeGeometryPointInstance *node_storage = (NodeGeometryPointInstance *)node.storage;
  const GeometryNodePointInstanceType type = (GeometryNodePointInstanceType)
                                                 node_storage->instance_type;
  Array<InstanceReference> references(amount);

  switch (type) {
    case GEO_NODE_POINT_INSTANCE_TYPE_OBJECT: {
      get_instance_references__object(params, references);
      break;
    }
    case GEO_NODE_POINT_INSTANCE_TYPE_COLLECTION: {
      get_instance_references__collection(params, component, references);
      break;
    }
  }
  return references;
}

static void add_instances_from_geometry_component(InstancesComponent &instances,
                                                  const GeometryComponent &src_geometry,
                                                  const GeoNodeExecParams &params)
{
  const AttributeDomain domain = ATTR_DOMAIN_POINT;

  const int domain_size = src_geometry.attribute_domain_size(domain);
  Array<InstanceReference> references = get_instance_references(params, src_geometry, domain_size);

  GVArray_Typed<float3> positions = src_geometry.attribute_get_for_read<float3>(
      "position", domain, {0, 0, 0});
  GVArray_Typed<float3> rotations = src_geometry.attribute_get_for_read<float3>(
      "rotation", domain, {0, 0, 0});
  GVArray_Typed<float3> scales = src_geometry.attribute_get_for_read<float3>(
      "scale", domain, {1, 1, 1});
  GVArray_Typed<int> ids = src_geometry.attribute_get_for_read<int>("id", domain, -1);

  for (const int i : IndexRange(domain_size)) {
    const InstanceReference &reference = references[i];
    if (reference.type() != InstanceReference::Type::None) {
      const float4x4 matrix = float4x4::from_loc_eul_scale(positions[i], rotations[i], scales[i]);
      const int handle = instances.add_reference(reference);
      instances.add_instance(handle, matrix, ids[i]);
    }
  }
}

static void geo_node_point_instance_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  GeometrySet geometry_set_out;

  /* TODO: This node should be able to instance on the input instances component
   * rather than making the entire input geometry set real. */
  geometry_set = geometry_set_realize_instances(geometry_set);

  InstancesComponent &instances = geometry_set_out.get_component_for_write<InstancesComponent>();
  if (geometry_set.has<MeshComponent>()) {
    add_instances_from_geometry_component(
        instances, *geometry_set.get_component_for_read<MeshComponent>(), params);
  }
  if (geometry_set.has<PointCloudComponent>()) {
    add_instances_from_geometry_component(
        instances, *geometry_set.get_component_for_read<PointCloudComponent>(), params);
  }

  if (geometry_set.has<CurveComponent>()) {
    add_instances_from_geometry_component(
        instances, *geometry_set.get_component_for_read<CurveComponent>(), params);
  }

  params.set_output("Geometry", std::move(geometry_set_out));
}

static void geo_node_point_instance_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryPointInstance *data = (NodeGeometryPointInstance *)MEM_callocN(
      sizeof(NodeGeometryPointInstance), __func__);
  data->instance_type = GEO_NODE_POINT_INSTANCE_TYPE_OBJECT;
  data->flag |= GEO_NODE_POINT_INSTANCE_WHOLE_COLLECTION;
  node->storage = data;
}

}  // namespace blender::nodes

void register_node_type_geo_point_instance()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_POINT_INSTANCE, "Point Instance", NODE_CLASS_GEOMETRY, 0);
  node_type_socket_templates(&ntype, geo_node_point_instance_in, geo_node_point_instance_out);
  node_type_init(&ntype, blender::nodes::geo_node_point_instance_init);
  node_type_storage(
      &ntype, "NodeGeometryPointInstance", node_free_standard_storage, node_copy_standard_storage);
  ntype.draw_buttons = geo_node_point_instance_layout;
  node_type_update(&ntype, blender::nodes::geo_node_point_instance_update);
  ntype.geometry_node_execute = blender::nodes::geo_node_point_instance_exec;
  nodeRegisterType(&ntype);
}
