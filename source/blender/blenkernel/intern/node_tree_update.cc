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

#include "BLI_map.hh"
#include "BLI_multi_value_map.hh"
#include "BLI_set.hh"
#include "BLI_stack.hh"
#include "BLI_vector_set.hh"

#include "DNA_modifier_types.h"
#include "DNA_node_types.h"

#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_node_tree_update.h"

#include "MOD_nodes.h"

#include "NOD_node_declaration.hh"
#include "NOD_node_tree_ref.hh"

using namespace blender::nodes;

namespace blender::bke {

namespace node_field_inferencing {

static bool is_field_socket_type(eNodeSocketDatatype type)
{
  return ELEM(type, SOCK_FLOAT, SOCK_INT, SOCK_BOOLEAN, SOCK_VECTOR, SOCK_RGBA);
}

static bool is_field_socket_type(const SocketRef &socket)
{
  return is_field_socket_type((eNodeSocketDatatype)socket.typeinfo()->type);
}

static bool update_field_inferencing(bNodeTree &btree);

static InputSocketFieldType get_interface_input_field_type(const NodeRef &node,
                                                           const InputSocketRef &socket)
{
  if (!is_field_socket_type(socket)) {
    return InputSocketFieldType::None;
  }
  if (node.is_reroute_node()) {
    return InputSocketFieldType::IsSupported;
  }
  if (node.is_group_output_node()) {
    /* Outputs always support fields when the data type is correct. */
    return InputSocketFieldType::IsSupported;
  }
  if (node.is_undefined()) {
    return InputSocketFieldType::None;
  }

  const NodeDeclaration *node_decl = node.declaration();

  /* Node declarations should be implemented for nodes involved here. */
  BLI_assert(node_decl != nullptr);

  /* Get the field type from the declaration. */
  const SocketDeclaration &socket_decl = *node_decl->inputs()[socket.index()];
  const InputSocketFieldType field_type = socket_decl.input_field_type();
  if (field_type == InputSocketFieldType::Implicit) {
    return field_type;
  }
  if (node_decl->is_function_node()) {
    /* In a function node, every socket supports fields. */
    return InputSocketFieldType::IsSupported;
  }
  return field_type;
}

static OutputFieldDependency get_interface_output_field_dependency(const NodeRef &node,
                                                                   const OutputSocketRef &socket)
{
  if (!is_field_socket_type(socket)) {
    /* Non-field sockets always output data. */
    return OutputFieldDependency::ForDataSource();
  }
  if (node.is_reroute_node()) {
    /* The reroute just forwards what is passed in. */
    return OutputFieldDependency::ForDependentField();
  }
  if (node.is_group_input_node()) {
    /* Input nodes get special treatment in #determine_group_input_states. */
    return OutputFieldDependency::ForDependentField();
  }
  if (node.is_undefined()) {
    return OutputFieldDependency::ForDataSource();
  }

  const NodeDeclaration *node_decl = node.declaration();

  /* Node declarations should be implemented for nodes involved here. */
  BLI_assert(node_decl != nullptr);

  if (node_decl->is_function_node()) {
    /* In a generic function node, all outputs depend on all inputs. */
    return OutputFieldDependency::ForDependentField();
  }

  /* Use the socket declaration. */
  const SocketDeclaration &socket_decl = *node_decl->outputs()[socket.index()];
  return socket_decl.output_field_dependency();
}

static FieldInferencingInterface get_dummy_field_inferencing_interface(const NodeRef &node)
{
  FieldInferencingInterface inferencing_interface;
  inferencing_interface.inputs.append_n_times(InputSocketFieldType::None, node.inputs().size());
  inferencing_interface.outputs.append_n_times(OutputFieldDependency::ForDataSource(),
                                               node.outputs().size());
  return inferencing_interface;
}

/**
 * Retrieves information about how the node interacts with fields.
 * In the future, this information can be stored in the node declaration. This would allow this
 * function to return a reference, making it more efficient.
 */
static FieldInferencingInterface get_node_field_inferencing_interface(const NodeRef &node)
{
  /* Node groups already reference all required information, so just return that. */
  if (node.is_group_node()) {
    bNodeTree *group = (bNodeTree *)node.bnode()->id;
    if (group == nullptr) {
      return FieldInferencingInterface();
    }
    if (!ntreeIsRegistered(group)) {
      /* This can happen when there is a linked node group that was not found (see T92799). */
      return get_dummy_field_inferencing_interface(node);
    }
    if (group->field_inferencing_interface == nullptr) {
      /* Update group recursively. */
      update_field_inferencing(*group);
    }
    return *group->field_inferencing_interface;
  }

  FieldInferencingInterface inferencing_interface;
  for (const InputSocketRef *input_socket : node.inputs()) {
    inferencing_interface.inputs.append(get_interface_input_field_type(node, *input_socket));
  }

  for (const OutputSocketRef *output_socket : node.outputs()) {
    inferencing_interface.outputs.append(
        get_interface_output_field_dependency(node, *output_socket));
  }
  return inferencing_interface;
}

/**
 * This struct contains information for every socket. The values are propagated through the
 * network.
 */
struct SocketFieldState {
  /* This socket starts a new field. */
  bool is_field_source = false;
  /* This socket can never become a field, because the node itself does not support it. */
  bool is_always_single = false;
  /* This socket is currently a single value. It could become a field though. */
  bool is_single = true;
  /* This socket is required to be a single value. This can be because the node itself only
   * supports this socket to be a single value, or because a node afterwards requires this to be a
   * single value. */
  bool requires_single = false;
};

static Vector<const InputSocketRef *> gather_input_socket_dependencies(
    const OutputFieldDependency &field_dependency, const NodeRef &node)
{
  const OutputSocketFieldType type = field_dependency.field_type();
  Vector<const InputSocketRef *> input_sockets;
  switch (type) {
    case OutputSocketFieldType::FieldSource:
    case OutputSocketFieldType::None: {
      break;
    }
    case OutputSocketFieldType::DependentField: {
      /* This output depends on all inputs. */
      input_sockets.extend(node.inputs());
      break;
    }
    case OutputSocketFieldType::PartiallyDependent: {
      /* This output depends only on a few inputs. */
      for (const int i : field_dependency.linked_input_indices()) {
        input_sockets.append(&node.input(i));
      }
      break;
    }
  }
  return input_sockets;
}

/**
 * Check what the group output socket depends on. Potentially traverses the node tree
 * to figure out if it is always a field or if it depends on any group inputs.
 */
static OutputFieldDependency find_group_output_dependencies(
    const InputSocketRef &group_output_socket,
    const Span<SocketFieldState> field_state_by_socket_id)
{
  if (!is_field_socket_type(group_output_socket)) {
    return OutputFieldDependency::ForDataSource();
  }

  /* Use a Set here instead of an array indexed by socket id, because we my only need to look at
   * very few sockets. */
  Set<const InputSocketRef *> handled_sockets;
  Stack<const InputSocketRef *> sockets_to_check;

  handled_sockets.add(&group_output_socket);
  sockets_to_check.push(&group_output_socket);

  /* Keeps track of group input indices that are (indirectly) connected to the output. */
  Vector<int> linked_input_indices;

  while (!sockets_to_check.is_empty()) {
    const InputSocketRef *input_socket = sockets_to_check.pop();

    for (const OutputSocketRef *origin_socket : input_socket->directly_linked_sockets()) {
      const NodeRef &origin_node = origin_socket->node();
      const SocketFieldState &origin_state = field_state_by_socket_id[origin_socket->id()];

      if (origin_state.is_field_source) {
        if (origin_node.is_group_input_node()) {
          /* Found a group input that the group output depends on. */
          linked_input_indices.append_non_duplicates(origin_socket->index());
        }
        else {
          /* Found a field source that is not the group input. So the output is always a field. */
          return OutputFieldDependency::ForFieldSource();
        }
      }
      else if (!origin_state.is_single) {
        const FieldInferencingInterface inferencing_interface =
            get_node_field_inferencing_interface(origin_node);
        const OutputFieldDependency &field_dependency =
            inferencing_interface.outputs[origin_socket->index()];

        /* Propagate search further to the left. */
        for (const InputSocketRef *origin_input_socket :
             gather_input_socket_dependencies(field_dependency, origin_node)) {
          if (!origin_input_socket->is_available()) {
            continue;
          }
          if (!field_state_by_socket_id[origin_input_socket->id()].is_single) {
            if (handled_sockets.add(origin_input_socket)) {
              sockets_to_check.push(origin_input_socket);
            }
          }
        }
      }
    }
  }
  return OutputFieldDependency::ForPartiallyDependentField(std::move(linked_input_indices));
}

static void propagate_data_requirements_from_right_to_left(
    const NodeTreeRef &tree, const MutableSpan<SocketFieldState> field_state_by_socket_id)
{
  const NodeTreeRef::ToposortResult toposort_result = tree.toposort(
      NodeTreeRef::ToposortDirection::RightToLeft);

  for (const NodeRef *node : toposort_result.sorted_nodes) {
    const FieldInferencingInterface inferencing_interface = get_node_field_inferencing_interface(
        *node);

    for (const OutputSocketRef *output_socket : node->outputs()) {
      SocketFieldState &state = field_state_by_socket_id[output_socket->id()];

      const OutputFieldDependency &field_dependency =
          inferencing_interface.outputs[output_socket->index()];

      if (field_dependency.field_type() == OutputSocketFieldType::FieldSource) {
        continue;
      }
      if (field_dependency.field_type() == OutputSocketFieldType::None) {
        state.requires_single = true;
        state.is_always_single = true;
        continue;
      }

      /* The output is required to be a single value when it is connected to any input that does
       * not support fields. */
      for (const InputSocketRef *target_socket : output_socket->directly_linked_sockets()) {
        state.requires_single |= field_state_by_socket_id[target_socket->id()].requires_single;
      }

      if (state.requires_single) {
        bool any_input_is_field_implicitly = false;
        const Vector<const InputSocketRef *> connected_inputs = gather_input_socket_dependencies(
            field_dependency, *node);
        for (const InputSocketRef *input_socket : connected_inputs) {
          if (!input_socket->is_available()) {
            continue;
          }
          if (inferencing_interface.inputs[input_socket->index()] ==
              InputSocketFieldType::Implicit) {
            if (!input_socket->is_logically_linked()) {
              any_input_is_field_implicitly = true;
              break;
            }
          }
        }
        if (any_input_is_field_implicitly) {
          /* This output isn't a single value actually. */
          state.requires_single = false;
        }
        else {
          /* If the output is required to be a single value, the connected inputs in the same node
           * must not be fields as well. */
          for (const InputSocketRef *input_socket : connected_inputs) {
            field_state_by_socket_id[input_socket->id()].requires_single = true;
          }
        }
      }
    }

    /* Some inputs do not require fields independent of what the outputs are connected to. */
    for (const InputSocketRef *input_socket : node->inputs()) {
      SocketFieldState &state = field_state_by_socket_id[input_socket->id()];
      if (inferencing_interface.inputs[input_socket->index()] == InputSocketFieldType::None) {
        state.requires_single = true;
        state.is_always_single = true;
      }
    }
  }
}

static void determine_group_input_states(
    const NodeTreeRef &tree,
    FieldInferencingInterface &new_inferencing_interface,
    const MutableSpan<SocketFieldState> field_state_by_socket_id)
{
  {
    /* Non-field inputs never support fields. */
    int index;
    LISTBASE_FOREACH_INDEX (bNodeSocket *, group_input, &tree.btree()->inputs, index) {
      if (!is_field_socket_type((eNodeSocketDatatype)group_input->type)) {
        new_inferencing_interface.inputs[index] = InputSocketFieldType::None;
      }
    }
  }
  /* Check if group inputs are required to be single values, because they are (indirectly)
   * connected to some socket that does not support fields. */
  for (const NodeRef *node : tree.nodes_by_type("NodeGroupInput")) {
    for (const OutputSocketRef *output_socket : node->outputs().drop_back(1)) {
      SocketFieldState &state = field_state_by_socket_id[output_socket->id()];
      if (state.requires_single) {
        new_inferencing_interface.inputs[output_socket->index()] = InputSocketFieldType::None;
      }
    }
  }
  /* If an input does not support fields, this should be reflected in all Group Input nodes. */
  for (const NodeRef *node : tree.nodes_by_type("NodeGroupInput")) {
    for (const OutputSocketRef *output_socket : node->outputs().drop_back(1)) {
      SocketFieldState &state = field_state_by_socket_id[output_socket->id()];
      const bool supports_field = new_inferencing_interface.inputs[output_socket->index()] !=
                                  InputSocketFieldType::None;
      if (supports_field) {
        state.is_single = false;
        state.is_field_source = true;
      }
      else {
        state.requires_single = true;
      }
    }
    SocketFieldState &dummy_socket_state = field_state_by_socket_id[node->outputs().last()->id()];
    dummy_socket_state.requires_single = true;
  }
}

static void propagate_field_status_from_left_to_right(
    const NodeTreeRef &tree, const MutableSpan<SocketFieldState> field_state_by_socket_id)
{
  const NodeTreeRef::ToposortResult toposort_result = tree.toposort(
      NodeTreeRef::ToposortDirection::LeftToRight);

  for (const NodeRef *node : toposort_result.sorted_nodes) {
    if (node->is_group_input_node()) {
      continue;
    }

    const FieldInferencingInterface inferencing_interface = get_node_field_inferencing_interface(
        *node);

    /* Update field state of input sockets, also taking into account linked origin sockets. */
    for (const InputSocketRef *input_socket : node->inputs()) {
      SocketFieldState &state = field_state_by_socket_id[input_socket->id()];
      if (state.is_always_single) {
        state.is_single = true;
        continue;
      }
      state.is_single = true;
      if (input_socket->directly_linked_sockets().is_empty()) {
        if (inferencing_interface.inputs[input_socket->index()] ==
            InputSocketFieldType::Implicit) {
          state.is_single = false;
        }
      }
      else {
        for (const OutputSocketRef *origin_socket : input_socket->directly_linked_sockets()) {
          if (!field_state_by_socket_id[origin_socket->id()].is_single) {
            state.is_single = false;
            break;
          }
        }
      }
    }

    /* Update field state of output sockets, also taking into account input sockets. */
    for (const OutputSocketRef *output_socket : node->outputs()) {
      SocketFieldState &state = field_state_by_socket_id[output_socket->id()];
      const OutputFieldDependency &field_dependency =
          inferencing_interface.outputs[output_socket->index()];

      switch (field_dependency.field_type()) {
        case OutputSocketFieldType::None: {
          state.is_single = true;
          break;
        }
        case OutputSocketFieldType::FieldSource: {
          state.is_single = false;
          state.is_field_source = true;
          break;
        }
        case OutputSocketFieldType::PartiallyDependent:
        case OutputSocketFieldType::DependentField: {
          for (const InputSocketRef *input_socket :
               gather_input_socket_dependencies(field_dependency, *node)) {
            if (!input_socket->is_available()) {
              continue;
            }
            if (!field_state_by_socket_id[input_socket->id()].is_single) {
              state.is_single = false;
              break;
            }
          }
          break;
        }
      }
    }
  }
}

static void determine_group_output_states(const NodeTreeRef &tree,
                                          FieldInferencingInterface &new_inferencing_interface,
                                          const Span<SocketFieldState> field_state_by_socket_id)
{
  for (const NodeRef *group_output_node : tree.nodes_by_type("NodeGroupOutput")) {
    /* Ignore inactive group output nodes. */
    if (!(group_output_node->bnode()->flag & NODE_DO_OUTPUT)) {
      continue;
    }
    /* Determine dependencies of all group outputs. */
    for (const InputSocketRef *group_output_socket : group_output_node->inputs().drop_back(1)) {
      OutputFieldDependency field_dependency = find_group_output_dependencies(
          *group_output_socket, field_state_by_socket_id);
      new_inferencing_interface.outputs[group_output_socket->index()] = std::move(
          field_dependency);
    }
    break;
  }
}

static void update_socket_shapes(const NodeTreeRef &tree,
                                 const Span<SocketFieldState> field_state_by_socket_id)
{
  const eNodeSocketDisplayShape requires_data_shape = SOCK_DISPLAY_SHAPE_CIRCLE;
  const eNodeSocketDisplayShape data_but_can_be_field_shape = SOCK_DISPLAY_SHAPE_DIAMOND_DOT;
  const eNodeSocketDisplayShape is_field_shape = SOCK_DISPLAY_SHAPE_DIAMOND;

  auto get_shape_for_state = [&](const SocketFieldState &state) {
    if (state.is_always_single) {
      return requires_data_shape;
    }
    if (!state.is_single) {
      return is_field_shape;
    }
    if (state.requires_single) {
      return requires_data_shape;
    }
    return data_but_can_be_field_shape;
  };

  for (const InputSocketRef *socket : tree.input_sockets()) {
    bNodeSocket *bsocket = socket->bsocket();
    const SocketFieldState &state = field_state_by_socket_id[socket->id()];
    bsocket->display_shape = get_shape_for_state(state);
  }
  for (const OutputSocketRef *socket : tree.output_sockets()) {
    bNodeSocket *bsocket = socket->bsocket();
    const SocketFieldState &state = field_state_by_socket_id[socket->id()];
    bsocket->display_shape = get_shape_for_state(state);
  }
}

static bool update_field_inferencing(bNodeTree &btree)
{
  using namespace blender::nodes;
  if (btree.type != NTREE_GEOMETRY) {
    return false;
  }

  /* Create new inferencing interface for this node group. */
  FieldInferencingInterface *new_inferencing_interface = new FieldInferencingInterface();
  new_inferencing_interface->inputs.resize(BLI_listbase_count(&btree.inputs),
                                           InputSocketFieldType::IsSupported);
  new_inferencing_interface->outputs.resize(BLI_listbase_count(&btree.outputs),
                                            OutputFieldDependency::ForDataSource());

  /* Create #NodeTreeRef to accelerate various queries on the node tree (e.g. linked sockets). */
  const NodeTreeRef tree{&btree};

  /* Keep track of the state of all sockets. The index into this array is #SocketRef::id(). */
  Array<SocketFieldState> field_state_by_socket_id(tree.sockets().size());

  propagate_data_requirements_from_right_to_left(tree, field_state_by_socket_id);
  determine_group_input_states(tree, *new_inferencing_interface, field_state_by_socket_id);
  propagate_field_status_from_left_to_right(tree, field_state_by_socket_id);
  determine_group_output_states(tree, *new_inferencing_interface, field_state_by_socket_id);
  update_socket_shapes(tree, field_state_by_socket_id);

  /* Update the previous group interface. */
  const bool group_interface_changed = btree.field_inferencing_interface == nullptr ||
                                       *btree.field_inferencing_interface !=
                                           *new_inferencing_interface;
  delete btree.field_inferencing_interface;
  btree.field_inferencing_interface = new_inferencing_interface;

  return group_interface_changed;
}

}  // namespace node_field_inferencing

using IDTreePair = std::pair<ID *, bNodeTree *>;
using TreeNodePair = std::pair<bNodeTree *, bNode *>;
using ObjectModifierPair = std::pair<Object *, ModifierData *>;

struct NodeTreeRelations {
 private:
  Main *bmain_;
  std::optional<Vector<IDTreePair>> all_trees_;
  std::optional<MultiValueMap<bNodeTree *, TreeNodePair>> group_node_users_;
  std::optional<MultiValueMap<bNodeTree *, ObjectModifierPair>> modifiers_users_;

 public:
  NodeTreeRelations(Main *bmain) : bmain_(bmain)
  {
  }

  void ensure_all_trees()
  {
    if (all_trees_.has_value()) {
      return;
    }
    all_trees_.emplace();
    if (bmain_ == nullptr) {
      return;
    }

    FOREACH_NODETREE_BEGIN (bmain_, ntree, id) {
      all_trees_->append({id, ntree});
    }
    FOREACH_NODETREE_END;
  }

  void ensure_group_node_users()
  {
    if (group_node_users_.has_value()) {
      return;
    }
    group_node_users_.emplace();
    if (bmain_ == nullptr) {
      return;
    }

    this->ensure_all_trees();

    for (const IDTreePair &pair : *all_trees_) {
      bNodeTree *ntree = pair.second;
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->id == nullptr) {
          continue;
        }
        ID *id = node->id;
        if (GS(id->name) == ID_NT) {
          bNodeTree *group = (bNodeTree *)id;
          group_node_users_->add(group, {ntree, node});
        }
      }
    }
  }

  void ensure_modifier_users()
  {
    if (modifiers_users_.has_value()) {
      return;
    }
    modifiers_users_.emplace();
    if (bmain_ == nullptr) {
      return;
    }

    LISTBASE_FOREACH (Object *, object, &bmain_->objects) {
      LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
        if (md->type == eModifierType_Nodes) {
          NodesModifierData *nmd = (NodesModifierData *)md;
          if (nmd->node_group != nullptr) {
            modifiers_users_->add(nmd->node_group, {object, md});
          }
        }
      }
    }
  }

  Span<ObjectModifierPair> get_modifier_users(bNodeTree *ntree)
  {
    BLI_assert(modifiers_users_.has_value());
    return modifiers_users_->lookup(ntree);
  }

  Span<TreeNodePair> get_group_node_users(bNodeTree *ntree)
  {
    BLI_assert(group_node_users_.has_value());
    return group_node_users_->lookup(ntree);
  }

  Span<IDTreePair> get_all_trees()
  {
    BLI_assert(all_trees_.has_value());
    return *all_trees_;
  }
};

struct TreeUpdateResult {
  bool interface_changed = false;
  bool output_changed = false;
};

class NodeTreeMainUpdater {
 private:
  Main *bmain_;
  NodeTreeUpdateExtraParams *params_;
  Map<bNodeTree *, TreeUpdateResult> update_result_by_tree_;
  NodeTreeRelations relations_;

 public:
  NodeTreeMainUpdater(Main *bmain, NodeTreeUpdateExtraParams *params)
      : bmain_(bmain), params_(params), relations_(bmain)
  {
  }

  void update()
  {
    Vector<bNodeTree *> changed_ntrees;
    FOREACH_NODETREE_BEGIN (bmain_, ntree, id) {
      if (ntree->changed_flag != NTREE_CHANGED_NONE) {
        changed_ntrees.append(ntree);
      }
    }
    FOREACH_NODETREE_END;
    this->update_rooted(changed_ntrees);
  }

  void update_rooted(Span<bNodeTree *> root_ntrees)
  {
    if (root_ntrees.is_empty()) {
      return;
    }

    bool is_single_tree_update = false;

    if (root_ntrees.size() == 1) {
      bNodeTree *ntree = root_ntrees[0];
      const TreeUpdateResult result = this->update_tree(*ntree);
      update_result_by_tree_.add_new(ntree, result);
      if (!result.interface_changed && !result.output_changed) {
        is_single_tree_update = true;
      }
    }

    if (!is_single_tree_update) {
      Vector<bNodeTree *> ntrees_in_order = this->get_tree_update_order(root_ntrees);
      for (bNodeTree *ntree : ntrees_in_order) {
        if (ntree->changed_flag == NTREE_CHANGED_NONE) {
          continue;
        }
        if (!update_result_by_tree_.contains(ntree)) {
          const TreeUpdateResult result = this->update_tree(*ntree);
          update_result_by_tree_.add_new(ntree, result);
        }
        const TreeUpdateResult result = update_result_by_tree_.lookup(ntree);
        if (result.output_changed || result.interface_changed) {
          Span<TreeNodePair> dependent_trees = relations_.get_group_node_users(ntree);
          for (const TreeNodePair &pair : dependent_trees) {
            BKE_node_tree_update_tag_node(pair.first, pair.second);
          }
        }
      }
    }

    for (const auto &item : update_result_by_tree_.items()) {
      bNodeTree *ntree = item.key;
      const TreeUpdateResult &result = item.value;
      /* TODO: Use owner id of embedded node trees. */
      ID *id = &ntree->id;

      this->reset_changed_flags(*ntree);

      if (result.interface_changed) {
        if (ntree->type == NTREE_GEOMETRY) {
          relations_.ensure_modifier_users();
          for (const ObjectModifierPair &pair : relations_.get_modifier_users(ntree)) {
            Object *object = pair.first;
            ModifierData *md = pair.second;

            if (md->type == eModifierType_Nodes) {
              MOD_nodes_update_interface(object, (NodesModifierData *)md);
            }
          }
        }
      }

      if (params_) {
        if (params_->tree_changed_fn) {
          params_->tree_changed_fn(id, ntree, params_->user_data);
        }
        if (params_->tree_interface_changed_fn && result.interface_changed) {
          params_->tree_interface_changed_fn(id, ntree, params_->user_data);
        }
        if (params_->tree_output_changed_fn && result.output_changed) {
          params_->tree_output_changed_fn(id, ntree, params_->user_data);
        }
      }
    }
  }

 private:
  enum class ToposortMark {
    None,
    Temporary,
    Permanent,
  };

  using ToposortMarkMap = Map<bNodeTree *, ToposortMark>;

  Vector<bNodeTree *> get_tree_update_order(Span<bNodeTree *> root_ntrees)
  {
    relations_.ensure_all_trees();
    relations_.ensure_group_node_users();

    Set<bNodeTree *> trees_to_update = get_trees_to_update(root_ntrees);

    Vector<bNodeTree *> sorted_ntrees;

    ToposortMarkMap marks;
    for (bNodeTree *ntree : trees_to_update) {
      marks.add_new(ntree, ToposortMark::None);
    }
    for (bNodeTree *ntree : trees_to_update) {
      if (marks.lookup(ntree) == ToposortMark::None) {
        const bool cycle_detected = !this->get_tree_update_order__visit_recursive(
            ntree, marks, sorted_ntrees);
        BLI_assert(!cycle_detected);
      }
    }

    std::reverse(sorted_ntrees.begin(), sorted_ntrees.end());

    return sorted_ntrees;
  }

  bool get_tree_update_order__visit_recursive(bNodeTree *ntree,
                                              ToposortMarkMap &marks,
                                              Vector<bNodeTree *> &sorted_ntrees)
  {
    ToposortMark &mark = marks.lookup(ntree);
    if (mark == ToposortMark::Permanent) {
      return true;
    }
    if (mark == ToposortMark::Temporary) {
      /* There is a dependency cycle. */
      return false;
    }

    mark = ToposortMark::Temporary;

    for (const TreeNodePair &pair : relations_.get_group_node_users(ntree)) {
      this->get_tree_update_order__visit_recursive(pair.first, marks, sorted_ntrees);
    }
    sorted_ntrees.append(ntree);

    mark = ToposortMark::Permanent;
    return true;
  }

  Set<bNodeTree *> get_trees_to_update(Span<bNodeTree *> root_ntrees)
  {
    relations_.ensure_group_node_users();

    Set<bNodeTree *> reachable_trees;
    VectorSet<bNodeTree *> trees_to_check = root_ntrees;

    while (!trees_to_check.is_empty()) {
      bNodeTree *ntree = trees_to_check.pop();
      if (reachable_trees.add(ntree)) {
        for (const TreeNodePair &pair : relations_.get_group_node_users(ntree)) {
          trees_to_check.add(pair.first);
        }
      }
    }

    return reachable_trees;
  }

  TreeUpdateResult update_tree(bNodeTree &ntree)
  {
    TreeUpdateResult result;

    if (ntree.changed_flag & NTREE_CHANGED_INTERFACE) {
      result.interface_changed = true;
    }

    this->update_input_socket_link_pointers(ntree);
    this->update_individual_nodes(ntree);

    if (ntree.typeinfo->update) {
      ntree.typeinfo->update(&ntree);
    }

    if (node_field_inferencing::update_field_inferencing(ntree)) {
      result.interface_changed = true;
    }

    this->update_input_socket_link_pointers(ntree);
    this->update_node_levels(ntree);
    this->update_link_validation(ntree);

    if (result.interface_changed) {
      ntreeInterfaceTypeUpdate(&ntree);
    }

    result.output_changed = true;
    return result;
  }

  void update_input_socket_link_pointers(bNodeTree &ntree)
  {
    LISTBASE_FOREACH (bNode *, node, &ntree.nodes) {
      LISTBASE_FOREACH (bNodeSocket *, socket, &node->inputs) {
        socket->link = nullptr;
      }
    }

    LISTBASE_FOREACH (bNodeLink *, link, &ntree.links) {
      link->tosock->link = link;
    }

    this->update_socket_used_tags(ntree);
  }

  void update_socket_used_tags(bNodeTree &ntree)
  {
    /* First clear flag. */
    LISTBASE_FOREACH (bNode *, node, &ntree.nodes) {
      LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
        sock->flag &= ~SOCK_IN_USE;
      }
      LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
        sock->flag &= ~SOCK_IN_USE;
      }
    }

    LISTBASE_FOREACH (bNodeLink *, link, &ntree.links) {
      link->fromsock->flag |= SOCK_IN_USE;
      if (!(link->flag & NODE_LINK_MUTED)) {
        link->tosock->flag |= SOCK_IN_USE;
      }
    }
  }

  void update_individual_nodes(bNodeTree &ntree)
  {
    LISTBASE_FOREACH (bNode *, node, &ntree.nodes) {
      if (ntree.changed_flag & NTREE_CHANGED_ANY || node->changed_flag & NODE_CHANGED_ANY ||
          ntree.changed_flag & NTREE_CHANGED_LINK) {
        this->update_individual_node(ntree, *node);
      }
    }
  }

  void update_individual_node(bNodeTree &ntree, bNode &node)
  {
    if (node.typeinfo->group_update_func) {
      node.typeinfo->group_update_func(&ntree, &node);
    }
    if (node.typeinfo->updatefunc) {
      node.typeinfo->updatefunc(&ntree, &node);
    }

    BLI_freelistN(&node.internal_links);
    if (node.typeinfo->update_internal_links) {
      node.typeinfo->update_internal_links(&ntree, &node);
    }
  }

  void update_node_levels(bNodeTree &ntree)
  {
    ntreeUpdateNodeLevels(&ntree);
  }

  void update_link_validation(bNodeTree &ntree)
  {
    LISTBASE_FOREACH (bNodeLink *, link, &ntree.links) {
      link->flag |= NODE_LINK_VALID;
      if (link->fromnode && link->tonode && link->fromnode->level <= link->tonode->level) {
        link->flag &= ~NODE_LINK_VALID;
      }
      else if (ntree.typeinfo->validate_link) {
        if (!ntree.typeinfo->validate_link(&ntree, link)) {
          link->flag &= ~NODE_LINK_VALID;
        }
      }
    }
  }

  void reset_changed_flags(bNodeTree &ntree)
  {
    ntree.changed_flag = NTREE_CHANGED_NONE;
    LISTBASE_FOREACH (bNode *, node, &ntree.nodes) {
      node->changed_flag = NODE_CHANGED_NONE;
      LISTBASE_FOREACH (bNodeSocket *, socket, &node->inputs) {
        socket->changed_flag = SOCK_CHANGED_NONE;
      }
      LISTBASE_FOREACH (bNodeSocket *, socket, &node->outputs) {
        socket->changed_flag = SOCK_CHANGED_NONE;
      }
    }
  }
};

}  // namespace blender::bke

void BKE_node_tree_update_tag(bNodeTree *ntree)
{
  ntree->changed_flag |= NTREE_CHANGED_ALL;
  ntree->update |= NTREE_UPDATE;
}

void BKE_node_tree_update_tag_node(bNodeTree *ntree, bNode *node)
{
  ntree->changed_flag |= NTREE_CHANGED_NODE;
  node->changed_flag |= NODE_CHANGED_ANY;
  ntree->update |= NTREE_UPDATE;
}

void BKE_node_tree_update_tag_socket(bNodeTree *ntree, bNodeSocket *socket)
{
  ntree->changed_flag |= NTREE_CHANGED_SOCKET;
  socket->changed_flag |= SOCK_CHANGED_ANY;
  ntree->update |= NTREE_UPDATE;
}

void BKE_node_tree_update_tag_node_removed(bNodeTree *ntree)
{
  ntree->changed_flag |= NTREE_CHANGED_REMOVED_ANY;
  ntree->update |= NTREE_UPDATE;
}

void BKE_node_tree_update_tag_link(bNodeTree *ntree)
{
  ntree->changed_flag |= NTREE_CHANGED_LINK;
  ntree->update |= NTREE_UPDATE;
}

void BKE_node_tree_update_tag_node_added(bNodeTree *ntree, bNode *node)
{
  BKE_node_tree_update_tag_node(ntree, node);
}

void BKE_node_tree_update_tag_link_removed(bNodeTree *ntree)
{
  BKE_node_tree_update_tag_link(ntree);
}

void BKE_node_tree_update_tag_link_added(bNodeTree *ntree, bNodeLink *UNUSED(link))
{
  BKE_node_tree_update_tag_link(ntree);
}

void BKE_node_tree_update_tag_link_mute(bNodeTree *ntree, bNodeLink *UNUSED(link))
{
  BKE_node_tree_update_tag_link(ntree);
}

void BKE_node_tree_update_tag_missing_runtime_data(bNodeTree *ntree)
{
  ntree->changed_flag |= NTREE_CHANGED_MISSING_RUNTIME_DATA;
  ntree->update |= NTREE_UPDATE;
}

void BKE_node_tree_update_tag_interface(bNodeTree *ntree)
{
  ntree->changed_flag |= NTREE_CHANGED_INTERFACE;
  ntree->update |= NTREE_UPDATE;
}

static bool is_updating = false;

void BKE_node_tree_update_main(Main *bmain, NodeTreeUpdateExtraParams *params)
{
  if (is_updating) {
    return;
  }

  is_updating = true;
  blender::bke::NodeTreeMainUpdater updater{bmain, params};
  updater.update();
  is_updating = false;
}

void BKE_node_tree_update_main_rooted(Main *bmain,
                                      bNodeTree *ntree,
                                      NodeTreeUpdateExtraParams *params)
{
  if (ntree == nullptr) {
    BKE_node_tree_update_main(bmain, params);
    return;
  }

  if (is_updating) {
    return;
  }

  is_updating = true;
  blender::bke::NodeTreeMainUpdater updater{bmain, params};
  updater.update_rooted({ntree});
  is_updating = false;
}
