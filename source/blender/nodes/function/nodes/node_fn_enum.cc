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

#include <cmath>

#include "RNA_enum_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_function_util.hh"

namespace blender::nodes {

static void fn_node_enum_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Float>(N_("Float"));
  b.add_output<decl::Int>(N_("Integer"));
};

static void fn_node_enum_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeFunctionEnum *data = (NodeFunctionEnum *)MEM_callocN(sizeof(NodeFunctionEnum), __func__);
  node->storage = data;
}

static void fn_node_enum_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "value", 0, "", ICON_NONE);
}

static const fn::MultiFunction *get_multi_function(bNode &UNUSED(bnode))
{
  return nullptr;
}

static void fn_node_enum_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const fn::MultiFunction *fn = get_multi_function(builder.node());
  builder.set_matching_fn(fn);
}

}  // namespace blender::nodes

void register_node_type_fn_enum()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_ENUM, "Enum", NODE_CLASS_SCRIPT, 0);
  node_type_storage(
      &ntype, "NodeFunctionEnum", node_free_standard_storage, node_copy_standard_storage);
  node_type_init(&ntype, blender::nodes::fn_node_enum_init);
  ntype.declare = blender::nodes::fn_node_enum_declare;
  ntype.build_multi_function = blender::nodes::fn_node_enum_build_multi_function;
  ntype.draw_buttons = blender::nodes::fn_node_enum_layout;
  nodeRegisterType(&ntype);
}
