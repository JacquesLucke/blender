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

#include "node_geometry_util.hh"

#include "BLI_rand.hh"

#include "DNA_mesh_types.h"
#include "DNA_pointcloud_types.h"

static bNodeSocketTemplate geo_node_random_attribute_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {SOCK_STRING, N_("Attribute")},
    {SOCK_VECTOR, N_("Min"), 0.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX},
    {SOCK_VECTOR, N_("Max"), 1.0f, 1.0f, 1.0f, 0.0f, -FLT_MAX, FLT_MAX},
    {SOCK_FLOAT, N_("Min"), 0.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX},
    {SOCK_FLOAT, N_("Max"), 1.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX},
    {SOCK_INT, N_("Seed"), 0, 0, 0, 0, -10000, 10000},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_random_attribute_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

static void geo_node_random_attribute_init(bNodeTree *UNUSED(tree), bNode *node)
{
  node->custom1 = CD_PROP_FLOAT;
}

static void geo_node_random_attribute_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  bNodeSocket *sock_min_vector = (bNodeSocket *)BLI_findlink(&node->inputs, 2);
  bNodeSocket *sock_max_vector = sock_min_vector->next;
  bNodeSocket *sock_min_float = sock_max_vector->next;
  bNodeSocket *sock_max_float = sock_min_float->next;

  const int data_type = node->custom1;

  nodeSetSocketAvailability(sock_min_vector, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(sock_max_vector, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(sock_min_float, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(sock_max_float, data_type == CD_PROP_FLOAT);
}

namespace blender::nodes {

static void randomize_attribute(FloatWriteAttribute &attribute,
                                float min,
                                float max,
                                RandomNumberGenerator &rng)
{
  for (const int i : IndexRange(attribute.size())) {
    const float value = rng.get_float() * (max - min) + min;
    attribute.set(i, value);
  }
}

static void randomize_attribute(Float3WriteAttribute &attribute,
                                float3 min,
                                float3 max,
                                RandomNumberGenerator &rng)
{
  for (const int i : IndexRange(attribute.size())) {
    const float x = rng.get_float();
    const float y = rng.get_float();
    const float z = rng.get_float();
    const float3 value = float3(x, y, z) * (max - min) + min;
    attribute.set(i, value);
  }
}

static void geo_random_attribute_exec(GeoNodeExecParams params)
{
  const bNode &node = params.node();
  const int data_type = node.custom1;
  const AttributeDomain domain = static_cast<AttributeDomain>(node.custom2);

  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  const std::string attribute_name = params.extract_input<std::string>("Attribute");
  const float3 min_value = params.extract_input<float3>("Min");
  const float3 max_value = params.extract_input<float3>("Max");
  const int seed = params.extract_input<int>("Seed");

  RandomNumberGenerator rng;
  rng.seed_random(seed);

  const CPPType &attribute_type = *bke::custom_data_type_to_cpp_type(data_type);

  MeshComponent &mesh_component = geometry_set.get_component_for_write<MeshComponent>();
  Mesh *mesh = mesh_component.get_for_write();
  if (mesh != nullptr) {
    WriteAttributePtr attribute = mesh_component.attribute_get_for_write(attribute_name);
    if (attribute && attribute->cpp_type() != attribute_type) {
      if (mesh_component.attribute_delete(attribute_name) == AttributeDeleteStatus::Deleted) {
        attribute = {};
      }
    }
    if (!attribute) {
      BKE_id_attribute_new(&mesh->id, attribute_name.c_str(), data_type, domain, nullptr);
      attribute = mesh_component.attribute_get_for_write(attribute_name);
    }

    if (attribute) {
      if (attribute->cpp_type().is<float>()) {
        FloatWriteAttribute float_attribute = std::move(attribute);
        randomize_attribute(float_attribute, min_value.x, max_value.x, rng);
      }
      else if (attribute->cpp_type().is<float3>()) {
        Float3WriteAttribute float3_attribute = std::move(attribute);
        randomize_attribute(float3_attribute, min_value, max_value, rng);
      }
    }
  }

  PointCloudComponent &pointcloud_component =
      geometry_set.get_component_for_write<PointCloudComponent>();
  PointCloud *pointcloud = pointcloud_component.get_for_write();
  if (pointcloud != nullptr) {
    WriteAttributePtr attribute = pointcloud_component.attribute_get_for_write(attribute_name);
    if (attribute && attribute->cpp_type() != attribute_type) {
      if (pointcloud_component.attribute_delete(attribute_name) ==
          AttributeDeleteStatus::Deleted) {
        attribute = {};
      }
    }
    if (!attribute) {
      BKE_id_attribute_new(&pointcloud->id, attribute_name.c_str(), data_type, domain, nullptr);
      attribute = pointcloud_component.attribute_get_for_write(attribute_name);
    }

    if (attribute) {
      if (attribute->cpp_type().is<float>()) {
        FloatWriteAttribute float_attribute = std::move(attribute);
        randomize_attribute(float_attribute, min_value.x, max_value.x, rng);
      }
      else if (attribute->cpp_type().is<float3>()) {
        Float3WriteAttribute float3_attribute = std::move(attribute);
        randomize_attribute(float3_attribute, min_value, max_value, rng);
      }
    }
  }

  params.set_output("Geometry", geometry_set);
}

}  // namespace blender::nodes

void register_node_type_geo_random_attribute()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_RANDOM_ATTRIBUTE, "Random Attribute", 0, 0);
  node_type_socket_templates(&ntype, geo_node_random_attribute_in, geo_node_random_attribute_out);
  node_type_init(&ntype, geo_node_random_attribute_init);
  node_type_update(&ntype, geo_node_random_attribute_update);
  ntype.geometry_node_execute = blender::nodes::geo_random_attribute_exec;
  nodeRegisterType(&ntype);
}
