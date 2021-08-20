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

#include "NOD_node_type.hh"

#include "node_geometry_util.hh"

namespace blender::nodes {

class InputMaterialNodeType : public NodeType {
 public:
  InputMaterialNodeType() : NodeType("Material", GEO_NODE_INPUT_MATERIAL, NODE_CLASS_INPUT)
  {
  }

  void build(NodeBuilder &b) const override
  {
    b.output<decl::Material>("Material");
  }

  void geometry_exec(GeoNodeExecParams params) const override
  {
    Material *material = (Material *)params.node().id;
    params.set_output("Material", material);
  }

  void draw(NodeDrawer &d) const override
  {
    uiItemR(d.layout, d.ptr, "material", 0, "", ICON_NONE);
  }
};

}  // namespace blender::nodes

void register_node_type_geo_input_material()
{
  static bNodeType ntype;
  static blender::nodes::InputMaterialNodeType type;

  geo_node_register(ntype, type);
}
