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

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

static bNodeSocketTemplate geo_node_attribute_in[] = {
    {SOCK_STRING, N_("Name")},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_attribute_out[] = {
    {SOCK_FLOAT, N_("Attribute")},
    {SOCK_INT, N_("Attribute")},
    {SOCK_BOOLEAN, N_("Attribute")},
    {SOCK_VECTOR, N_("Attribute")},
    {SOCK_RGBA, N_("Attribute")},
    {-1, ""},
};

static void geo_node_attribute_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "output_type", 0, "", ICON_NONE);
}

static void geo_node_attribute_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryAttribute *data = (NodeGeometryAttribute *)MEM_callocN(sizeof(NodeGeometryAttribute),
                                                                     __func__);
  data->output_type = SOCK_FLOAT;
  node->storage = data;
}

namespace blender::nodes {

static void geo_node_attribute_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeGeometryAttribute *node_storage = (NodeGeometryAttribute *)node->storage;

  LISTBASE_FOREACH (bNodeSocket *, socket, &node->outputs) {
    nodeSetSocketAvailability(socket,
                              socket->type == (eNodeSocketDatatype)node_storage->output_type);
  }
}

static const CPPType *get_cpp_type(const eNodeSocketDatatype data_type)
{
  switch (data_type) {
    case SOCK_FLOAT:
      return &CPPType::get<float>();
    case SOCK_VECTOR:
      return &CPPType::get<float3>();
    case SOCK_RGBA:
      return &CPPType::get<ColorGeometry4f>();
    case SOCK_BOOLEAN:
      return &CPPType::get<bool>();
    case SOCK_INT:
      return &CPPType::get<int>();
    default:
      return nullptr;
  }
}

static void geo_node_attribute_exec(GeoNodeExecParams params)
{
  NodeGeometryAttribute *node_storage = (NodeGeometryAttribute *)params.node().storage;
  std::string name = params.extract_input<std::string>("Name");

  const CPPType *cpp_type = get_cpp_type((eNodeSocketDatatype)node_storage->output_type);
  BLI_assert(cpp_type != nullptr);
  bke::FieldPtr field = new bke::GVArrayInputField<bke::AttributeFieldInputKey>(std::move(name),
                                                                                *cpp_type);
  if (cpp_type->is<float>()) {
    params.set_output("Attribute", bke::FieldRef<float>(std::move(field)));
  }
  else if (cpp_type->is<int>()) {
    params.set_output("Attribute_001", bke::FieldRef<int>(std::move(field)));
  }
  else if (cpp_type->is<bool>()) {
    params.set_output("Attribute_002", bke::FieldRef<bool>(std::move(field)));
  }
  else if (cpp_type->is<float3>()) {
    params.set_output("Attribute_003", bke::FieldRef<float3>(std::move(field)));
  }
  else if (cpp_type->is<ColorGeometry4f>()) {
    params.set_output("Attribute_004", bke::FieldRef<ColorGeometry4f>(std::move(field)));
  }
}

}  // namespace blender::nodes

void register_node_type_geo_attribute()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_ATTRIBUTE, "Attribute", NODE_CLASS_INPUT, 0);
  node_type_socket_templates(&ntype, geo_node_attribute_in, geo_node_attribute_out);
  node_type_init(&ntype, geo_node_attribute_init);
  node_type_update(&ntype, blender::nodes::geo_node_attribute_update);
  node_type_storage(
      &ntype, "NodeGeometryAttribute", node_free_standard_storage, node_copy_standard_storage);
  ntype.geometry_node_execute = blender::nodes::geo_node_attribute_exec;
  ntype.draw_buttons = geo_node_attribute_layout;
  nodeRegisterType(&ntype);
}
