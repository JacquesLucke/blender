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

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "NOD_geometry_exec.hh"

#include "node_function_util.hh"

namespace blender::nodes {

static void fn_node_enum_declare(NodeDeclarationBuilder &b)
{
  const bNode *node = b.node();
  if (node == nullptr) {
    return;
  }

  b.add_output<decl::Int>("Index");

  EnumPropertyItem *items = nullptr;
  int tot_items = 0;

  const NodeFunctionEnum *storage = (const NodeFunctionEnum *)node->storage;
  LISTBASE_FOREACH (const NodeFunctionEnumItem *, item, &storage->items) {
    b.add_output<decl::Bool>(item->name ? item->name : "Bool",
                             "item_" + std::to_string(item->value));
    EnumPropertyItem enum_item = {0};
    enum_item.identifier = BLI_strdup(item->name ? item->name : "");
    enum_item.name = enum_item.identifier;
    enum_item.description = BLI_strdup(item->description ? item->description : "");
    enum_item.value = item->value;
    RNA_enum_item_add(&items, &tot_items, &enum_item);
  }
  RNA_enum_item_end(&items, &tot_items);

  std::shared_ptr<EnumItems> socket_items = std::make_shared<EnumItems>(
      items, [tot_items, items]() {
        for (const int i : IndexRange(tot_items - 1)) {
          EnumPropertyItem &enum_item = items[i];
          MEM_freeN((void *)enum_item.identifier);
          MEM_freeN((void *)enum_item.description);
        }
        MEM_freeN(items);
      });

  b.add_input<decl::Enum>("Enum").dynamic_items(std::move(socket_items)).hide_label();
};

static bool fn_node_enum_draw_socket(uiLayout *layout,
                                     const bContext *UNUSED(C),
                                     bNodeTree *ntree,
                                     bNode *node,
                                     bNodeSocket *socket)
{
  const int socket_index = BLI_findindex(&node->outputs, socket);
  if (socket_index <= 0) {
    return false;
  }
  NodeFunctionEnum *storage = (NodeFunctionEnum *)node->storage;
  NodeFunctionEnumItem *item = (NodeFunctionEnumItem *)BLI_findlink(&storage->items,
                                                                    socket_index - 1);
  PointerRNA item_ptr;
  RNA_pointer_create(&ntree->id, &RNA_NodeFunctionEnumItem, item, &item_ptr);
  uiItemR(layout, &item_ptr, "name", 0, "", ICON_NONE);
  return true;
}

static void fn_node_enum_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeFunctionEnum *data = (NodeFunctionEnum *)MEM_callocN(sizeof(NodeFunctionEnum), __func__);
  data->owner_node = node;
  node->storage = data;
}

static void fn_node_enum_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;
  uiItemStringO(layout, "Add", ICON_PLUS, "node.enum_item_add", "node_name", node->name);
}

class EnumFunction : public fn::MultiFunction {
 private:
  Vector<int> enum_values_;
  fn::MFSignature signature_;

 public:
  EnumFunction(const bNode &node)
  {
    fn::MFSignatureBuilder signature{"Enum Function"};
    signature.single_input<EnumValue>("Enum");
    signature.single_output<int>("Index");

    const NodeFunctionEnum *storage = (NodeFunctionEnum *)node.storage;
    LISTBASE_FOREACH (const NodeFunctionEnumItem *, item, &storage->items) {
      signature.single_output<bool>(item->name);
      enum_values_.append(item->value);
    }

    signature_ = signature.build();
    this->set_signature(&signature_);
  }

  void call(IndexMask mask, fn::MFParams params, fn::MFContext UNUSED(context)) const override
  {
    const VArray<EnumValue> &enum_in = params.readonly_single_input<EnumValue>(0, "Enum");
    MutableSpan<int> r_indices = params.uninitialized_single_output_if_required<int>(1, "Index");

    if (!r_indices.is_empty()) {
      for (const int i : mask) {
        const int in_value = enum_in[i].value;
        r_indices[i] = enum_values_.first_index_of_try(in_value);
      }
    }

    for (const int enum_index : enum_values_.index_range()) {
      MutableSpan<bool> r_bools = params.uninitialized_single_output_if_required<bool>(2 +
                                                                                       enum_index);
      if (r_bools.is_empty()) {
        continue;
      }
      const int enum_value = enum_values_[enum_index];
      for (const int i : mask) {
        const int in_value = enum_in[i].value;
        r_bools[i] = in_value == enum_value;
      }
    }
  }
};

static void fn_node_enum_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  builder.construct_and_set_matching_fn<EnumFunction>(builder.node());
}

static void fn_node_enum_copy_storage(bNodeTree *UNUSED(dest_ntree),
                                      bNode *dst_node,
                                      const bNode *src_node)
{
  const NodeFunctionEnum *src_storage = (const NodeFunctionEnum *)src_node->storage;
  NodeFunctionEnum *dst_storage = (NodeFunctionEnum *)MEM_dupallocN(src_storage);
  dst_storage->owner_node = dst_node;
  BLI_listbase_clear(&dst_storage->items);
  LISTBASE_FOREACH (const NodeFunctionEnumItem *, src_item, &src_storage->items) {
    NodeFunctionEnumItem *dst_item = (NodeFunctionEnumItem *)MEM_dupallocN(src_item);
    dst_item->owner_node = dst_node;
    dst_item->name = (char *)MEM_dupallocN(src_item->name);
    dst_item->description = (char *)MEM_dupallocN(src_item->description);
    BLI_addtail(&dst_storage->items, dst_item);
  }
  dst_node->storage = dst_storage;
}

static void fn_node_enum_free_storage(bNode *node)
{
  NodeFunctionEnum *storage = (NodeFunctionEnum *)node->storage;
  LISTBASE_FOREACH_MUTABLE (NodeFunctionEnumItem *, item, &storage->items) {
    if (item->name) {
      MEM_freeN(item->name);
    }
    if (item->description) {
      MEM_freeN(item->description);
    }
    MEM_freeN(item);
  }
  MEM_freeN(storage);
}

}  // namespace blender::nodes

void register_node_type_fn_enum()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_ENUM, "Enum", NODE_CLASS_SCRIPT, 0);
  node_type_storage(&ntype,
                    "NodeFunctionEnum",
                    blender::nodes::fn_node_enum_free_storage,
                    blender::nodes::fn_node_enum_copy_storage);
  node_type_init(&ntype, blender::nodes::fn_node_enum_init);
  ntype.declare = blender::nodes::fn_node_enum_declare;
  ntype.declaration_is_dynamic = true;
  ntype.build_multi_function = blender::nodes::fn_node_enum_build_multi_function;
  ntype.draw_buttons = blender::nodes::fn_node_enum_layout;
  ntype.draw_socket = blender::nodes::fn_node_enum_draw_socket;
  nodeRegisterType(&ntype);
}
