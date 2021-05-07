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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "MOD_nodes_evaluator.hh"

#include "NOD_geometry_exec.hh"
#include "NOD_type_conversions.hh"

#include "DEG_depsgraph_query.h"

#include "FN_generic_value_map.hh"
#include "FN_multi_function.hh"

#include "BLI_stack.hh"
#include "BLI_task.hh"
#include "BLI_vector_set.hh"

#include <atomic>
#include <tbb/enumerable_thread_specific.h>
#include <tbb/task_group.h>

namespace blender::modifiers::geometry_nodes {

using bke::PersistentCollectionHandle;
using bke::PersistentObjectHandle;
using fn::CPPType;
using fn::GValueMap;
using nodes::GeoNodeExecParams;
using namespace fn::multi_function_types;

enum class ValueUsage : uint8_t {
  /* The value is definitely used. */
  Required,
  /* The value may be used. */
  Maybe,
  /* The value will definitely not be used. */
  Unused,
};

struct SingleInputValue {
  /**
   * Points either to null or to a value of the type of input.
   */
  void *value = nullptr;
};

struct MultiInputValueItem {
  /**
   * The socket where this value is coming from. This is required to sort the inputs correctly
   * based on the link order later on.
   */
  DSocket origin;
  /**
   * Should only be null directly after construction. After that it should always point to a value
   * of the correct type.
   */
  void *value = nullptr;
};

struct MultiInputValue {
  /**
   * Collection of all the inputs that have been provided already. Note, the same origin can occure
   * multiple times. However, it is guaranteed that if two items have the same origin, they will
   * also have the same value (the pointer is different, but they point to values that would
   * compare equal).
   */
  Vector<MultiInputValueItem> items;
  /**
   * Number of items that need to be added until all inputs have been provided.
   */
  int expected_size = 0;
};

struct InputState {

  /**
   * Type of the socket. If this is null, the socket should just be ignored.
   */
  const CPPType *type = nullptr;

  /**
   * Value of this input socket. By default, the value is empty. When other nodes are done
   * computing their outputs, the computed values will be forwarded to linked input sockets.
   * The value will then live here until it is consumed by the node or it was found that the value
   * is not needed anymore.
   * Whether the `single` or `multi` value is used depends on the socket.
   */
  union {
    SingleInputValue *single;
    MultiInputValue *multi;
  } value;

  /**
   * How the node intends to use this input. By default all inputs may be used. Based on which
   * outputs are used, a node can tell the evaluator that an input will definitely be used or is
   * never used. This allows the evaluator to free values early, avoid copies and other unnecessary
   * computations.
   */
  ValueUsage usage = ValueUsage::Maybe;

  /**
   * True when this input is/was used for an evaluation. While a node is running, only the inputs
   * that have this set to true are allowed to be used. This makes sure that inputs created while
   * the node is running correctly trigger the node to run again. Furthermore, it gives the node a
   * consistent view of which inputs are available that does not change unexpectedly.
   *
   * While the node is running, this can be checked without a lock, because no one is writing to
   * it. If this is true, the value can be read without a lock as well, because the value is not
   * changed by others anymore.
   */
  bool was_ready_for_evaluation = false;
};

struct OutputState {
  /**
   * If this output has been computed and forwarded already. If this is true, the value is not
   * computed/forwarded again.
   */
  bool has_been_computed = false;

  /**
   * Keeps track of how the output value is used. If a connected input becomes required, this
   * output has to become required as well. The output becomes ignored when it has zero potential
   * users that are counted below.
   */
  ValueUsage output_usage = ValueUsage::Maybe;

  /**
   * This is a copy of `output_usage` that is done right before node evaluation starts. This is
   * done so that the node gets a consistent view of what outputs are used, even when this changes
   * while the node is running (the node might be reevaluated in that case).
   *
   * While the node is running, this can be checked without a lock, because no one is writing to
   * it.
   */
  ValueUsage output_usage_for_evaluation;

  /**
   * Counts how many times the value from this output might be used. If this number reaches zero,
   * the output is not needed anymore.
   */
  int potential_users = 0;
};

enum class NodeScheduleState {
  /**
   * Default state of every node.
   */
  NotScheduled,
  /**
   * The node has been added to the task group and will be executed by that in the future.
   */
  Scheduled,
  /**
   * The node is currently running.
   */
  Running,
  /**
   * The node is running and has been rescheduled while running. In this case the node will run
   * again. However, we don't add it to the task group immediately, because then the node might run
   * twice at the same time, which is not allowed. Instead, once the node is done running, it will
   * reschedule itself.
   */
  RunningAndRescheduled,
};

struct NodeState {
  /**
   * Needs to be locked when any data in this state is accessed that is not explicitely marked as
   * otherwise.
   */
  std::mutex mutex;

  /**
   * States of the individual input and output sockets. One can index into these arrays without
   * locking. However, to access the data inside a lock is generally necessary.
   *
   * These spans have to be indexed with the socket index. Unavailable sockets have a state as
   * well. Maybe we can handle unavailable sockets differently in Blender in general, so I did not
   * want to add complexity around it here.
   */
  MutableSpan<InputState> inputs;
  MutableSpan<OutputState> outputs;

  /**
   * The first run of a node is sometimes handled specially.
   */
  bool is_first_run = true;

  /**
   * Used to check that nodes that don't support lazyness do not run more than once.
   */
  bool has_been_executed = false;

  /**
   * Becomes true when the node will never be executed again and its inputs are destructed.
   * Generally, a node has finished once all of its outputs with (potential) users have been
   * computed.
   */
  bool node_has_finished = false;

  /**
   * Counts the number of values that still have to be forwarded to this node until it should run
   * again. It counts values from a multi input socket separately.
   * This is used as an optimization so that nodes are not scheduled unnecessarily in many cases.
   */
  int missing_required_inputs = 0;

  /**
   * A node is always in one specific schedule state. This helps to ensure that the same node does
   * not run twice at the same time accidentally.
   */
  NodeScheduleState schedule_state = NodeScheduleState::NotScheduled;
};

/**
 * Utility class that locks the state of a node. Having this is a separate class is useful because
 * it allows methods to communicate that they expect the node to be locked.
 */
class LockedNode {
 public:
  const DNode node;
  NodeState &node_state;

 private:
  std::lock_guard<std::mutex> lock_;

 public:
  LockedNode(const DNode node, NodeState &node_state)
      : node(node), node_state(node_state), lock_(node_state.mutex)
  {
  }
};

class GeometryNodesEvaluator;

/* TODO: Use a map data structure or so to make this faster. */
static DInputSocket get_input_by_identifier(const DNode node, const StringRef identifier)
{
  for (const InputSocketRef *socket : node->inputs()) {
    if (socket->identifier() == identifier) {
      return {node.context(), socket};
    }
  }
  return {};
}

static DOutputSocket get_output_by_identifier(const DNode node, const StringRef identifier)
{
  for (const OutputSocketRef *socket : node->outputs()) {
    if (socket->identifier() == identifier) {
      return {node.context(), socket};
    }
  }
  return {};
}

/** Implements the callbacks that might be called when a node is executed. */
class NodeParamsProvider : public nodes::GeoNodeExecParamsProvider {
 private:
  GeometryNodesEvaluator &evaluator_;
  NodeState *node_state_;

 public:
  NodeParamsProvider(GeometryNodesEvaluator &evaluator, DNode dnode);

  bool can_get_input(StringRef identifier) const override;
  bool can_set_output(StringRef identifier) const override;
  GMutablePointer extract_input(StringRef identifier) override;
  Vector<GMutablePointer> extract_multi_input(StringRef identifier) override;
  GPointer get_input(StringRef identifier) const override;
  GMutablePointer alloc_output_value(const CPPType &type) override;
  void set_output(StringRef identifier, GMutablePointer value) override;
  void require_input(StringRef identifier) override;
  void set_input_unused(StringRef identifier) override;
  bool output_may_be_required(StringRef identifier) const override;
  bool output_is_required(StringRef identifier) const override;
};

class GeometryNodesEvaluator {
 private:
  /**
   * This allocator lives on after the evaluator has been destructed. Therefore outputs of the
   * entire evaluator should be allocated here.
   */
  LinearAllocator<> &outer_allocator_;
  /**
   * A local linear allocator for each thread. Only use this for values that need to live longer
   * than the lifetime of the evaluator itself.
   * Considerations for the future:
   * - We could use an allocator that can free here, some temporary values don't live long.
   * - If we ever run into false sharing bottlenecks, we could use local allocators that allocate
   *   on cache line boundaries. Note, just because a value is allocated in one specific thread,
   *   does not mean that it will only be used by that thread.
   */
  tbb::enumerable_thread_specific<LinearAllocator<>> local_allocators_;

  GeometryNodesEvaluationParams &params_;
  const blender::nodes::DataTypeConversions &conversions_;

  Map<DNode, NodeState *> node_states_;
  tbb::task_group task_group_;

  friend NodeParamsProvider;

 public:
  GeometryNodesEvaluator(GeometryNodesEvaluationParams &params)
      : outer_allocator_(params.allocator),
        params_(params),
        conversions_(blender::nodes::get_implicit_type_conversions())
  {
  }

  Vector<GMutablePointer> execute()
  {
    this->create_states_for_reachable_nodes();
    this->forward_group_inputs();
    this->schedule_initial_nodes();
    task_group_.wait();
    Vector<GMutablePointer> output_values = this->extract_group_outputs();
    this->destruct_node_states();
    return output_values;
  }

  Vector<GMutablePointer> extract_group_outputs()
  {
    Vector<GMutablePointer> output_values;
    for (const DInputSocket &socket : params_.output_sockets) {
      BLI_assert(socket->is_available());
      BLI_assert(!socket->is_multi_input_socket());

      const DNode node = socket.node();
      NodeState &node_state = *node_states_.lookup(node);
      InputState &input_state = node_state.inputs[socket->index()];

      SingleInputValue &single_value = *input_state.value.single;
      void *value = single_value.value;

      /* The value should have been computed by now. If this assert is hit, it means that there was
       * some scheduling issue before. */
      BLI_assert(value != nullptr);

      /* Move value into memory owned by the outer allocator. */
      const CPPType &type = *input_state.type;
      void *buffer = outer_allocator_.allocate(type.size(), type.alignment());
      type.move_to_uninitialized(value, buffer);

      output_values.append({type, buffer});
    }
    return output_values;
  }

  void forward_group_inputs()
  {
    for (auto &&item : params_.input_values.items()) {
      const DOutputSocket socket = item.key;
      GMutablePointer value = item.value;
      this->log_socket_value(socket, value);

      const DNode node = socket.node();
      NodeState *node_state = node_states_.lookup_default(node, nullptr);
      if (node_state == nullptr) {
        /* The socket is not connected to any output. */
        value.destruct();
        continue;
      }
      this->forward_output(socket, value);
    }
  }

  void create_states_for_reachable_nodes()
  {
    /* Vector of all the inserted nodes, so that we can easily parallelize over this array
     * afterwards. */
    Vector<DNode> inserted_nodes;

    /* This does a depth first search for all the nodes that are reachable from the group outputs.
     * This finds all nodes that are relevant. */
    Stack<DNode> nodes_to_check;
    /* Start at the output sockets. */
    for (const DInputSocket &socket : params_.output_sockets) {
      nodes_to_check.push(socket.node());
    }
    /* Use a the local allocator because the states don't need to outlive the evaluator. */
    LinearAllocator<> &allocator = local_allocators_.local();
    while (!nodes_to_check.is_empty()) {
      const DNode node = nodes_to_check.pop();
      if (node_states_.contains(node)) {
        /* This node has been handled already. */
        continue;
      }
      /* Create a new state for the node. */
      NodeState &node_state = *allocator.construct<NodeState>().release();
      node_states_.add_new(node, &node_state);
      inserted_nodes.append(node);

      /* Push all linked origins on the stack. */
      for (const InputSocketRef *input_ref : node->inputs()) {
        const DInputSocket input{node.context(), input_ref};
        input.foreach_origin_socket(
            [&](const DSocket origin) { nodes_to_check.push(origin.node()); });
      }
    }

    /* Initialize the more complex parts of the node states in parallel. At this point no new node
     * states are added anymore, so it is safe to lookup states from `node_states_` from multiple
     * threads. */
    parallel_for(inserted_nodes.index_range(), 50, [&, this](const IndexRange range) {
      LinearAllocator<> &allocator = this->local_allocators_.local();
      for (const int i : range) {
        const DNode node = inserted_nodes[i];
        NodeState &node_state = *this->node_states_.lookup(node);
        this->initialize_node_state(node, node_state, allocator);
      }
    });
  }

  void initialize_node_state(const DNode node, NodeState &node_state, LinearAllocator<> &allocator)
  {
    /* Construct arrays of the correct size. */
    node_state.inputs = allocator.construct_array<InputState>(node->inputs().size());
    node_state.outputs = allocator.construct_array<OutputState>(node->outputs().size());

    /* Initialize input states. */
    for (const int i : node->inputs().index_range()) {
      InputState &input_state = node_state.inputs[i];
      const DInputSocket socket = node.input(i);
      if (!socket->is_available()) {
        /* Unavailable sockets should never be used. */
        input_state.type = nullptr;
        input_state.usage = ValueUsage::Unused;
        continue;
      }
      const CPPType *type = this->get_socket_type(socket);
      input_state.type = type;
      if (type == nullptr) {
        /* This is not a known data socket, it shouldn't be used. */
        input_state.usage = ValueUsage::Unused;
        continue;
      }
      /* Construct the correct struct that can hold the input(s). */
      if (socket->is_multi_input_socket()) {
        input_state.value.multi = allocator.construct<MultiInputValue>().release();
        /* Count how many values should be added until the socket is complete. */
        socket.foreach_origin_socket(
            [&](DSocket UNUSED(origin)) { input_state.value.multi->expected_size++; });
        /* If no links are connected, we do read the value from socket itself. */
        if (input_state.value.multi->expected_size == 0) {
          input_state.value.multi->expected_size = 1;
        }
      }
      else {
        input_state.value.single = allocator.construct<SingleInputValue>().release();
      }
    }
    /* Initialize output states. */
    for (const int i : node->outputs().index_range()) {
      OutputState &output_state = node_state.outputs[i];
      const DOutputSocket socket = node.output(i);
      if (!socket->is_available()) {
        /* Unavailable outputs should never be used. */
        output_state.output_usage = ValueUsage::Unused;
        continue;
      }
      const CPPType *type = this->get_socket_type(socket);
      if (type == nullptr) {
        /* Non data sockets should never be used. */
        output_state.output_usage = ValueUsage::Unused;
        continue;
      }
      /* Count the number of potential users for this socket. */
      socket.foreach_target_socket(
          [&, this](const DInputSocket target_socket) {
            const DNode target_node = target_socket.node();
            if (!this->node_states_.contains(target_node)) {
              /* The target node is not computed because it is not computed to the output. */
              return;
            }
            output_state.potential_users += 1;
          },
          {});
      if (output_state.potential_users == 0) {
        /* If it does not have any potential users, it is unused. */
        output_state.output_usage = ValueUsage::Unused;
      }
    }
  }

  void destruct_node_states()
  {
    /* Need to destruct stuff manually, because it's allocated by a custom allocator. */
    for (auto &&node_state_item : node_states_.items()) {
      const DNode node = node_state_item.key;
      NodeState &node_state = *node_state_item.value;

      for (const int i : node->inputs().index_range()) {
        InputState &input_state = node_state.inputs[i];
        if (input_state.type == nullptr) {
          continue;
        }
        const InputSocketRef &socket_ref = node->input(i);
        if (socket_ref.is_multi_input_socket()) {
          MultiInputValue &multi_value = *input_state.value.multi;
          for (MultiInputValueItem &item : multi_value.items) {
            input_state.type->destruct(item.value);
          }
          multi_value.~MultiInputValue();
        }
        else {
          SingleInputValue &single_value = *input_state.value.single;
          void *value = single_value.value;
          if (value != nullptr) {
            input_state.type->destruct(value);
          }
          single_value.~SingleInputValue();
        }
      }

      destruct_n(node_state.inputs.data(), node_state.inputs.size());
      destruct_n(node_state.outputs.data(), node_state.outputs.size());

      node_state.~NodeState();
    }
  }

  void schedule_initial_nodes()
  {
    for (const DInputSocket &socket : params_.output_sockets) {
      const DNode node = socket.node();
      NodeState &node_state = *node_states_.lookup(socket.node());
      LockedNode locked_node{node, node_state};
      /* Setting an input as required will schedule any linked node. */
      this->set_input_required(locked_node, socket);
    }
  }

  void set_input_required(LockedNode &locked_node, const DInputSocket input_socket)
  {
    BLI_assert(locked_node.node == input_socket.node());
    InputState &input_state = locked_node.node_state.inputs[input_socket->index()];

    /* Value set as unused cannot become used again. */
    BLI_assert(input_state.usage != ValueUsage::Unused);

    if (input_state.usage == ValueUsage::Required) {
      /* The value is already required, but the node might expect to be evaluated again. */
      this->schedule_node(locked_node);
      /* Returning here also ensure that the code below is executed at most once per input. */
      return;
    }
    input_state.usage = ValueUsage::Required;

    if (input_state.was_ready_for_evaluation) {
      /* The value was already ready, but the node might expect to be evaluated again. */
      this->schedule_node(locked_node);
      return;
    }

    /* Count how many values still have to be added to this input until it is "complete". */
    int missing_values = 0;
    if (input_socket->is_multi_input_socket()) {
      MultiInputValue &multi_value = *input_state.value.multi;
      missing_values = multi_value.expected_size - multi_value.items.size();
    }
    else {
      SingleInputValue &single_value = *input_state.value.single;
      if (single_value.value == nullptr) {
        missing_values = 1;
      }
    }
    if (missing_values == 0) {
      /* The input is fully available already, but the node might expect to be evaluated again. */
      this->schedule_node(locked_node);
      return;
    }
    /* Increase the total number of missing required inputs. This ensures that the node will be
     * scheduled correctly when all inputs have been provided. */
    locked_node.node_state.missing_required_inputs += missing_values;

    /* Get all origin sockets, because we have to tag those as required as well. */
    Vector<DSocket> origin_sockets;
    input_socket.foreach_origin_socket(
        [&, this](const DSocket origin_socket) { origin_sockets.append(origin_socket); });

    if (origin_sockets.is_empty()) {
      /* If there are no origin sockets, just load the value from the socket directly. */
      this->load_unlinked_input_value(locked_node, input_socket, input_state, input_socket);
      locked_node.node_state.missing_required_inputs -= 1;
      this->schedule_node(locked_node);
      return;
    }
    bool will_be_triggered_by_other_node = false;
    for (const DSocket origin_socket : origin_sockets) {
      if (origin_socket->is_input()) {
        /* Load the value directly from the origin socket. In most cases this is an unlinked group
         * input. */
        this->load_unlinked_input_value(locked_node, input_socket, input_state, origin_socket);
        locked_node.node_state.missing_required_inputs -= 1;
        return;
      }
      /* The value has not been computed yet, so when it will be forwarded by another node, this
       * node will be triggered. */
      will_be_triggered_by_other_node = true;

      const DNode origin_node = origin_socket.node();
      NodeState &origin_node_state = *this->node_states_.lookup(origin_node);
      OutputState &origin_socket_state = origin_node_state.outputs[origin_socket->index()];

      LockedNode locked_origin_node{origin_node, origin_node_state};

      if (origin_socket_state.output_usage == ValueUsage::Required) {
        /* Output is marked as required already. So the other node is scheduled already. */
        return;
      }
      /* The origin node needs to be scheduled so that it provides the requested input
       * eventually. */
      origin_socket_state.output_usage = ValueUsage::Required;
      this->schedule_node(locked_origin_node);
    }
    /* If this node will be triggered by another node, we don't have to schedule it now. */
    if (!will_be_triggered_by_other_node) {
      this->schedule_node(locked_node);
    }
  }

  void load_unlinked_input_value(LockedNode &UNUSED(locked_node),
                                 const DInputSocket input_socket,
                                 InputState &input_state,
                                 const DSocket origin_socket)
  {
    GMutablePointer value = this->get_value_from_socket(origin_socket, *input_state.type);
    if (input_socket->is_multi_input_socket()) {
      MultiInputValue &multi_value = *input_state.value.multi;
      multi_value.items.append({input_socket, value.get()});
    }
    else {
      SingleInputValue &single_value = *input_state.value.single;
      single_value.value = value.get();
    }
  }

  void set_input_unused(LockedNode &locked_node, const DInputSocket socket)
  {
    InputState &input_state = locked_node.node_state.inputs[socket->index()];
    BLI_assert(input_state.usage != ValueUsage::Required);

    if (input_state.usage == ValueUsage::Unused) {
      /* Nothing to do in this case. */
      return;
    }
    input_state.usage = ValueUsage::Unused;

    this->destruct_input_value(locked_node, socket);

    if (input_state.was_ready_for_evaluation) {
      /* If the value was already computed, we don't need to notify origin nodes. */
      return;
    }

    socket.foreach_origin_socket([&, this](const DSocket origin_socket) {
      if (origin_socket->is_input()) {
        return;
      }
      const DNode origin_node = origin_socket.node();
      NodeState &origin_node_state = *this->node_states_.lookup(origin_node);
      OutputState &origin_output_state = origin_node_state.outputs[origin_socket->index()];

      LockedNode locked_origin{origin_node, origin_node_state};
      origin_output_state.potential_users -= 1;
      if (origin_output_state.potential_users == 0) {
        /* The output socket has no users anymore. */
        origin_output_state.output_usage = ValueUsage::Unused;
        /* Schedule the origin node in case it wants to set its inputs as unused as well. */
        this->schedule_node(locked_origin);
      }
    });
  }

  void destruct_input_value(LockedNode &locked_node, const DInputSocket socket)
  {
    InputState &input_state = locked_node.node_state.inputs[socket->index()];
    if (socket->is_multi_input_socket()) {
      MultiInputValue &multi_value = *input_state.value.multi;
      for (MultiInputValueItem &item : multi_value.items) {
        input_state.type->destruct(item.value);
      }
      multi_value.items.clear();
    }
    else {
      SingleInputValue &single_value = *input_state.value.single;
      if (single_value.value != nullptr) {
        input_state.type->destruct(single_value.value);
        single_value.value = nullptr;
      }
    }
  }

  void forward_output(const DOutputSocket from_socket, GMutablePointer value_to_forward)
  {
    BLI_assert(value_to_forward.get() != nullptr);

    Vector<DInputSocket> to_sockets;

    auto handle_target_socket_fn = [&, this](const DInputSocket to_socket) {
      if (this->should_forward_to_socket(to_socket)) {
        to_sockets.append(to_socket);
      }
    };
    auto handle_skipped_socket_fn = [&, this](const DSocket socket) {
      this->log_socket_value(socket, value_to_forward);
    };
    from_socket.foreach_target_socket(handle_target_socket_fn, handle_skipped_socket_fn);

    LinearAllocator<> &allocator = local_allocators_.local();

    const CPPType &from_type = *value_to_forward.type();
    Vector<DInputSocket> to_sockets_same_type;
    for (const DInputSocket &to_socket : to_sockets) {
      const CPPType &to_type = *this->get_socket_type(to_socket);
      if (from_type == to_type) {
        to_sockets_same_type.append(to_socket);
        continue;
      }
      this->forward_to_socket_with_different_type(
          allocator, value_to_forward, from_socket, to_socket, to_type);
    }
    this->forward_to_sockets_with_same_type(
        allocator, to_sockets_same_type, value_to_forward, from_socket);
  }

  void log_socket_value(const DSocket socket, Span<GPointer> values)
  {
    if (params_.log_socket_value_fn) {
      params_.log_socket_value_fn(socket, values);
    }
  }

  void log_socket_value(const DSocket socket, Span<MultiInputValueItem> values)
  {
    Vector<GPointer, 16> value_pointers;
    value_pointers.reserve(values.size());
    const CPPType &type = *this->get_socket_type(socket);
    for (const MultiInputValueItem &item : values) {
      value_pointers.append({type, item.value});
    }
    this->log_socket_value(socket, value_pointers);
  }

  void log_socket_value(const DSocket socket, GPointer value)
  {
    this->log_socket_value(socket, Span<GPointer>(&value, 1));
  }

  bool should_forward_to_socket(const DInputSocket socket)
  {
    if (!socket->is_available()) {
      /* Unavailable sockets are never used. */
      return false;
    }
    const DNode to_node = socket.node();
    NodeState *target_node_state = this->node_states_.lookup_default(to_node, nullptr);
    if (target_node_state == nullptr) {
      /* If the socket belongs to a node that has no state, the entire node is not used. */
      return false;
    }
    InputState &target_input_state = target_node_state->inputs[socket->index()];

    std::lock_guard lock{target_node_state->mutex};
    return target_input_state.usage != ValueUsage::Unused;
  }

  void forward_to_socket_with_different_type(LinearAllocator<> &allocator,
                                             const GPointer value_to_forward,
                                             const DOutputSocket from_socket,
                                             const DInputSocket to_socket,
                                             const CPPType &to_type)
  {
    const CPPType &from_type = *value_to_forward.type();
    void *buffer = allocator.allocate(to_type.size(), to_type.alignment());
    if (conversions_.is_convertible(from_type, to_type)) {
      conversions_.convert_to_uninitialized(from_type, to_type, value_to_forward.get(), buffer);
    }
    else {
      /* Cannot convert, use default value instead. */
      to_type.copy_to_uninitialized(to_type.default_value(), buffer);
    }
    this->add_value_to_input_socket(to_socket, from_socket, {to_type, buffer});
  }

  void forward_to_sockets_with_same_type(LinearAllocator<> &allocator,
                                         Span<DInputSocket> to_sockets,
                                         GMutablePointer value_to_forward,
                                         const DOutputSocket from_socket)
  {
    if (to_sockets.is_empty()) {
      /* Value is not used anymore, so it can be destructed. */
      value_to_forward.destruct();
    }
    else if (to_sockets.size() == 1) {
      /* Value is only used by one input socket, no need to copy it. */
      const DInputSocket to_socket = to_sockets[0];
      this->add_value_to_input_socket(to_socket, from_socket, value_to_forward);
    }
    else {
      /* Multiple inputs use the value, make a copy for every input except for one. */
      /* First make the copies, so that the next node does not start modifying the value while we
       * are still making copies. */
      const CPPType &type = *value_to_forward.type();
      for (const DInputSocket &to_socket : to_sockets.drop_front(1)) {
        void *buffer = allocator.allocate(type.size(), type.alignment());
        type.copy_to_uninitialized(value_to_forward.get(), buffer);
        this->add_value_to_input_socket(to_socket, from_socket, {type, buffer});
      }
      /* Forward the original value to one of the targets. */
      const DInputSocket to_socket = to_sockets[0];
      this->add_value_to_input_socket(to_socket, from_socket, value_to_forward);
    }
  }

  void add_value_to_input_socket(const DInputSocket socket,
                                 const DOutputSocket origin,
                                 GMutablePointer value)
  {
    BLI_assert(socket->is_available());

    const DNode node = socket.node();
    NodeState &node_state = *node_states_.lookup(node);
    InputState &input_state = node_state.inputs[socket->index()];

    LockedNode locked_node{node, node_state};

    if (socket->is_multi_input_socket()) {
      MultiInputValue &multi_value = *input_state.value.multi;
      multi_value.items.append({origin, value.get()});
    }
    else {
      SingleInputValue &single_value = *input_state.value.single;
      BLI_assert(single_value.value == nullptr);
      single_value.value = value.get();
    }

    if (input_state.usage == ValueUsage::Required) {
      node_state.missing_required_inputs--;
      if (node_state.missing_required_inputs == 0) {
        /* Schedule node if all the required inputs have been provided. */
        this->schedule_node(locked_node);
      }
    }
  }

  const CPPType *get_socket_type(const DSocket socket) const
  {
    return nodes::socket_cpp_type_get(*socket->typeinfo());
  }

  const CPPType *get_socket_type(const SocketRef &socket) const
  {
    return nodes::socket_cpp_type_get(*socket.typeinfo());
  }

  void schedule_node(LockedNode &locked_node)
  {
    switch (locked_node.node_state.schedule_state) {
      case NodeScheduleState::NotScheduled: {
        /* Schedule the node now. */
        locked_node.node_state.schedule_state = NodeScheduleState::Scheduled;
        task_group_.run([this, node = locked_node.node]() { this->run_task(node); });
        break;
      }
      case NodeScheduleState::Scheduled: {
        /* Scheduled already, nothing to do. */
        break;
      }
      case NodeScheduleState::Running: {
        /* Reschedule node while it is running. The node will reschedule itself when it is done. */
        locked_node.node_state.schedule_state = NodeScheduleState::RunningAndRescheduled;
        break;
      }
      case NodeScheduleState::RunningAndRescheduled: {
        /* Scheduled already, nothing to do. */
        break;
      }
    }
  }

  void run_task(const DNode node)
  {
    if (node->is_group_input_node() || node->is_group_output_node()) {
      return;
    }

    NodeState &node_state = *node_states_.lookup(node);
    bool can_execute_node = false;
    {
      LockedNode locked_node{node, node_state};
      BLI_assert(node_state.schedule_state == NodeScheduleState::Scheduled);
      node_state.schedule_state = NodeScheduleState::Running;

      if (node_state.is_first_run) {
        this->first_node_run(locked_node);
        node_state.is_first_run = false;
      }
      can_execute_node = this->try_prepare_node_for_execution(locked_node);
    }

    if (can_execute_node) {
      this->execute_node(node, node_state);
    }

    {
      LockedNode locked_node{node, node_state};
      this->finish_node_if_remaining_outputs_are_unused(locked_node);
      const bool reschedule = (!node_state.node_has_finished &&
                               node_state.schedule_state ==
                                   NodeScheduleState::RunningAndRescheduled);
      node_state.schedule_state = NodeScheduleState::NotScheduled;
      if (reschedule) {
        this->schedule_node(locked_node);
      }
    }
  }

  bool try_prepare_node_for_execution(LockedNode &locked_node)
  {
    if (locked_node.node_state.node_has_finished) {
      return false;
    }
    this->finish_node_if_remaining_outputs_are_unused(locked_node);
    if (locked_node.node_state.node_has_finished) {
      return false;
    }
    bool evaluation_is_necessary = false;
    for (OutputState &output_state : locked_node.node_state.outputs) {
      output_state.output_usage_for_evaluation = output_state.output_usage;
      if (!output_state.has_been_computed) {
        if (output_state.output_usage == ValueUsage::Required) {
          /* Only evaluate when there is an output that is required but has not been computed. */
          evaluation_is_necessary = true;
        }
      }
    }
    if (!evaluation_is_necessary) {
      return false;
    }
    for (const int i : locked_node.node_state.inputs.index_range()) {
      InputState &input_state = locked_node.node_state.inputs[i];
      if (input_state.type == nullptr) {
        continue;
      }
      const DInputSocket socket = locked_node.node.input(i);
      const bool is_required = input_state.usage == ValueUsage::Required;

      /* No need to check this socket again. */
      if (input_state.was_ready_for_evaluation) {
        continue;
      }

      if (socket->is_multi_input_socket()) {
        MultiInputValue &multi_value = *input_state.value.multi;
        if (multi_value.items.size() == multi_value.expected_size) {
          input_state.was_ready_for_evaluation = true;
          this->log_socket_value(socket, multi_value.items);
        }
        else if (is_required) {
          /* The input is required but is not fully provided yet. Therefore the node cannot be
           * executed yet. */
          return false;
        }
      }
      else {
        SingleInputValue &single_value = *input_state.value.single;
        if (single_value.value != nullptr) {
          input_state.was_ready_for_evaluation = true;
          this->log_socket_value(socket, GPointer{input_state.type, single_value.value});
        }
        else if (is_required) {
          /* The input is required but has not been provided yet. Therefore the node cannot be
           * executed yet. */
          return false;
        }
      }
    }
    return true;
  }

  void finish_node_if_remaining_outputs_are_unused(LockedNode &locked_node)
  {
    bool has_remaining_output = false;
    for (OutputState &output_state : locked_node.node_state.outputs) {
      if (output_state.has_been_computed) {
        continue;
      }
      if (output_state.output_usage != ValueUsage::Unused) {
        has_remaining_output = true;
        break;
      }
    }
    if (!has_remaining_output) {
      for (const int i : locked_node.node->inputs().index_range()) {
        const DInputSocket socket = locked_node.node.input(i);
        InputState &input_state = locked_node.node_state.inputs[i];
        if (input_state.usage == ValueUsage::Maybe) {
          this->set_input_unused(locked_node, socket);
        }
        else if (input_state.usage == ValueUsage::Required) {
          this->destruct_input_value(locked_node, socket);
        }
      }
      locked_node.node_state.node_has_finished = true;
    }
  }

  void execute_node(const DNode node, NodeState &node_state)
  {
    const bNode &bnode = *node->bnode();

    if (node_state.has_been_executed) {
      if (!bnode.typeinfo->geometry_node_execute_supports_lazyness) {
        /* Nodes that don't support lazyness must not be executed more than once. */
        BLI_assert_unreachable();
      }
    }
    node_state.has_been_executed = true;

    /* Use the geometry node execute callback if it exists. */
    if (bnode.typeinfo->geometry_node_execute != nullptr) {
      this->execute_geometry_node(node);
      return;
    }

    /* Use the multi-function implementation if it exists. */
    const MultiFunction *multi_function = params_.mf_by_node->lookup_default(node, nullptr);
    if (multi_function != nullptr) {
      this->execute_multi_function_node(node, *multi_function, node_state);
      return;
    }

    this->execute_unknown_node(node);
  }

  void execute_geometry_node(const DNode node)
  {
    const bNode &bnode = *node->bnode();

    NodeParamsProvider params_provider{*this, node};
    GeoNodeExecParams params{params_provider};
    bnode.typeinfo->geometry_node_execute(params);
  }

  void execute_multi_function_node(const DNode node,
                                   const MultiFunction &fn,
                                   NodeState &node_state)
  {
    MFContextBuilder fn_context;
    MFParamsBuilder fn_params{fn, 1};
    LinearAllocator<> &allocator = local_allocators_.local();
    for (const int i : node->inputs().index_range()) {
      const InputSocketRef &socket_ref = node->input(i);
      if (!socket_ref.is_available()) {
        continue;
      }
      BLI_assert(!socket_ref.is_multi_input_socket());
      InputState &input_state = node_state.inputs[i];
      BLI_assert(input_state.was_ready_for_evaluation);
      SingleInputValue &single_value = *input_state.value.single;
      BLI_assert(single_value.value != nullptr);
      fn_params.add_readonly_single_input(GPointer{*input_state.type, single_value.value});
    }
    Vector<GMutablePointer> outputs;
    for (const int i : node->outputs().index_range()) {
      const OutputSocketRef &socket_ref = node->output(i);
      if (!socket_ref.is_available()) {
        continue;
      }
      const CPPType &type = *this->get_socket_type(socket_ref);
      void *buffer = allocator.allocate(type.size(), type.alignment());
      fn_params.add_uninitialized_single_output(GMutableSpan{type, buffer, 1});
      outputs.append({type, buffer});
    }

    fn.call(IndexRange(1), fn_params, fn_context);

    int output_index = 0;
    for (const int i : node->outputs().index_range()) {
      const OutputSocketRef &socket_ref = node->output(i);
      if (!socket_ref.is_available()) {
        continue;
      }
      OutputState &output_state = node_state.outputs[i];
      const DOutputSocket socket{node.context(), &socket_ref};
      GMutablePointer value = outputs[output_index];
      this->forward_output(socket, value);
      output_state.has_been_computed = true;
      output_index++;
    }
  }

  void execute_unknown_node(const DNode node)
  {
    NodeState &node_state = *node_states_.lookup(node);
    LinearAllocator<> &allocator = local_allocators_.local();
    for (const OutputSocketRef *socket : node->outputs()) {
      if (!socket->is_available()) {
        continue;
      }
      const CPPType *type = this->get_socket_type(*socket);
      if (type == nullptr) {
        continue;
      }
      OutputState &output_state = node_state.outputs[socket->index()];
      output_state.has_been_computed = true;
      void *buffer = allocator.allocate(type->size(), type->alignment());
      type->copy_to_uninitialized(type->default_value(), buffer);
      this->forward_output({node.context(), socket}, {*type, buffer});
    }
  }

  void first_node_run(LockedNode &locked_node)
  {
    const DNode node = locked_node.node;
    NodeState &node_state = locked_node.node_state;

    if (locked_node.node->typeinfo()->geometry_node_execute_supports_lazyness) {
      return;
    }

    for (const int i : node->inputs().index_range()) {
      const DInputSocket input_socket = node.input(i);
      if (!input_socket->is_available()) {
        continue;
      }
      InputState &input_state = node_state.inputs[i];
      if (input_state.type == nullptr) {
        continue;
      }
      this->set_input_required(locked_node, input_socket);
    }
  }

  GMutablePointer get_value_from_socket(const DSocket socket, const CPPType &required_type)
  {
    LinearAllocator<> &allocator = local_allocators_.local();

    bNodeSocket *bsocket = socket->bsocket();
    const CPPType &type = *this->get_socket_type(socket);
    void *buffer = allocator.allocate(type.size(), type.alignment());

    if (bsocket->type == SOCK_OBJECT) {
      Object *object = socket->default_value<bNodeSocketValueObject>()->value;
      PersistentObjectHandle object_handle = params_.handle_map->lookup(object);
      new (buffer) PersistentObjectHandle(object_handle);
    }
    else if (bsocket->type == SOCK_COLLECTION) {
      Collection *collection = socket->default_value<bNodeSocketValueCollection>()->value;
      PersistentCollectionHandle collection_handle = params_.handle_map->lookup(collection);
      new (buffer) PersistentCollectionHandle(collection_handle);
    }
    else {
      blender::nodes::socket_cpp_value_get(*bsocket, buffer);
    }

    if (type == required_type) {
      return {type, buffer};
    }
    if (conversions_.is_convertible(type, required_type)) {
      void *converted_buffer = allocator.allocate(required_type.size(), required_type.alignment());
      conversions_.convert_to_uninitialized(type, required_type, buffer, converted_buffer);
      type.destruct(buffer);
      return {required_type, converted_buffer};
    }
    void *default_buffer = allocator.allocate(required_type.size(), required_type.alignment());
    required_type.copy_to_uninitialized(required_type.default_value(), default_buffer);
    return {required_type, default_buffer};
  }
};

NodeParamsProvider::NodeParamsProvider(GeometryNodesEvaluator &evaluator, DNode dnode)
    : evaluator_(evaluator)
{
  this->dnode = dnode;
  this->handle_map = evaluator.params_.handle_map;
  this->self_object = evaluator.params_.self_object;
  this->modifier = &evaluator.params_.modifier_->modifier;
  this->depsgraph = evaluator.params_.depsgraph;

  node_state_ = evaluator.node_states_.lookup(dnode);
}

bool NodeParamsProvider::can_get_input(StringRef identifier) const
{
  const DInputSocket socket = get_input_by_identifier(this->dnode, identifier);
  BLI_assert(socket);

  InputState &input_state = node_state_->inputs[socket->index()];
  if (!input_state.was_ready_for_evaluation) {
    return false;
  }

  if (socket->is_multi_input_socket()) {
    MultiInputValue &multi_value = *input_state.value.multi;
    return multi_value.items.size() == multi_value.expected_size;
  }
  SingleInputValue &single_value = *input_state.value.single;
  return single_value.value != nullptr;
}

bool NodeParamsProvider::can_set_output(StringRef identifier) const
{
  const DOutputSocket socket = get_output_by_identifier(this->dnode, identifier);
  BLI_assert(socket);

  OutputState &output_state = node_state_->outputs[socket->index()];
  return !output_state.has_been_computed;
}

GMutablePointer NodeParamsProvider::extract_input(StringRef identifier)
{
  const DInputSocket socket = get_input_by_identifier(this->dnode, identifier);
  BLI_assert(socket);
  BLI_assert(!socket->is_multi_input_socket());
  BLI_assert(this->can_get_input(identifier));

  InputState &input_state = node_state_->inputs[socket->index()];
  SingleInputValue &single_value = *input_state.value.single;
  void *value = single_value.value;
  single_value.value = nullptr;
  return {*input_state.type, value};
}

Vector<GMutablePointer> NodeParamsProvider::extract_multi_input(StringRef identifier)
{
  const DInputSocket socket = get_input_by_identifier(this->dnode, identifier);
  BLI_assert(socket);
  BLI_assert(socket->is_multi_input_socket());
  BLI_assert(this->can_get_input(identifier));

  InputState &input_state = node_state_->inputs[socket->index()];
  MultiInputValue &multi_value = *input_state.value.multi;

  Vector<GMutablePointer> ret_values;
  socket.foreach_origin_socket([&](DSocket origin) {
    for (const MultiInputValueItem &item : multi_value.items) {
      if (item.origin == origin) {
        ret_values.append({*input_state.type, item.value});
        return;
      }
    }
    BLI_assert_unreachable();
  });
  multi_value.items.clear();
  return ret_values;
}

GPointer NodeParamsProvider::get_input(StringRef identifier) const
{
  const DInputSocket socket = get_input_by_identifier(this->dnode, identifier);
  BLI_assert(socket);
  BLI_assert(!socket->is_multi_input_socket());
  BLI_assert(this->can_get_input(identifier));

  InputState &input_state = node_state_->inputs[socket->index()];
  SingleInputValue &single_value = *input_state.value.single;
  return {*input_state.type, single_value.value};
}

GMutablePointer NodeParamsProvider::alloc_output_value(const CPPType &type)
{
  LinearAllocator<> &allocator = evaluator_.local_allocators_.local();
  return {type, allocator.allocate(type.size(), type.alignment())};
}

void NodeParamsProvider::set_output(StringRef identifier, GMutablePointer value)
{
  const DOutputSocket socket = get_output_by_identifier(this->dnode, identifier);
  BLI_assert(socket);

  evaluator_.log_socket_value(socket, value);

  OutputState &output_state = node_state_->outputs[socket->index()];
  BLI_assert(!output_state.has_been_computed);
  evaluator_.forward_output(socket, value);
  output_state.has_been_computed = true;
}

void NodeParamsProvider::require_input(StringRef identifier)
{
  const DInputSocket socket = get_input_by_identifier(this->dnode, identifier);
  LockedNode locked_node{this->dnode, *node_state_};
  evaluator_.set_input_required(locked_node, socket);
}

void NodeParamsProvider::set_input_unused(StringRef identifier)
{
  const DInputSocket socket = get_input_by_identifier(this->dnode, identifier);
  LockedNode locked_node{this->dnode, *node_state_};
  evaluator_.set_input_unused(locked_node, socket);
}

bool NodeParamsProvider::output_may_be_required(StringRef identifier) const
{
  const DOutputSocket socket = get_output_by_identifier(this->dnode, identifier);
  OutputState &output_state = node_state_->outputs[socket->index()];
  if (output_state.has_been_computed) {
    return false;
  }
  return output_state.output_usage_for_evaluation != ValueUsage::Unused;
}

bool NodeParamsProvider::output_is_required(StringRef identifier) const
{
  const DOutputSocket socket = get_output_by_identifier(this->dnode, identifier);
  OutputState &output_state = node_state_->outputs[socket->index()];
  if (output_state.has_been_computed) {
    return false;
  }
  return output_state.output_usage_for_evaluation == ValueUsage::Required;
}

void evaluate_geometry_nodes(GeometryNodesEvaluationParams &params)
{
  GeometryNodesEvaluator evaluator{params};
  params.r_output_values = evaluator.execute();
}

}  // namespace blender::modifiers::geometry_nodes
