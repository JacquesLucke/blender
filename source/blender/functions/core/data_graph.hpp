#pragma once

/**
 * A data flow graph is the primary way to connect multiple functions to compose more powerful new
 * functions. It can be thought of as a normal node graph with the important constraint, that every
 * input socket has to be linked to some output.
 *
 * The graph itself does not represent a new function. Only when some sockets are selected as
 * inputs and outputs, a new function can be created from it.
 *
 * Every node in the graph contains one function. The inputs and outputs of the node correspond to
 * the inputs and outputs of the function.
 *
 * This data structure is immutable once it has been created. This allows it to implement very
 * efficient ways to iterate over it. To create a new data flow graph, the corresponding builder
 * should be used. That makes it much easier to dynamically add nodes and links at build-time.
 *
 * A data flow graph is reference counted and can therefore have multiple owners.
 *
 * Every node in the graph is identified by an integer. The identifiers are all in [0, #nodes - 1].
 * Similarly, every input and output socket is identified by an integer. However, an input and an
 * output socket can have the same identifier. So, to identify any socket, its ID and whether it is
 * an output or output has to be stored.
 */

#include "BLI_range.hpp"
#include "BLI_map.hpp"
#include "BLI_monotonic_allocator.hpp"

#include "function.hpp"
#include "source_info.hpp"

namespace FN {

using BLI::Map;
using BLI::MonotonicAllocator;

class DataGraphBuilder;

/**
 * Represents any socket in the graph by storing its ID and whether it is an input or output.
 */
struct DataSocket {
 private:
  bool m_is_output;
  uint m_id;

 public:
  DataSocket(bool is_output, uint id) : m_is_output(is_output), m_id(id)
  {
  }

  static DataSocket None()
  {
    return DataSocket(false, (uint)-1);
  }

  bool is_none() const
  {
    return m_id == (uint)-1;
  }

  static DataSocket FromInput(uint id)
  {
    return DataSocket(false, id);
  }

  static DataSocket FromOutput(uint id)
  {
    return DataSocket(true, id);
  }

  bool is_input() const
  {
    return !m_is_output;
  }

  bool is_output() const
  {
    return m_is_output;
  }

  uint id() const
  {
    return m_id;
  }

  friend inline bool operator==(const DataSocket &a, const DataSocket &b)
  {
    return a.m_id == b.m_id && a.m_is_output == b.m_is_output;
  }
};

/**
 * An iterator over sockets. This type should never appear in user code. Instead it is either used
 * directly in a range-for loop or it should be used with the auto keyword.
 */
template<typename IdIteratorT> class DataSocketIterator {
 private:
  bool m_is_output;
  IdIteratorT m_it;

 public:
  DataSocketIterator(bool is_output, IdIteratorT it) : m_is_output(is_output), m_it(it)
  {
  }

  DataSocketIterator &operator++()
  {
    ++m_it;
    return *this;
  }

  bool operator!=(const DataSocketIterator &other)
  {
    return m_it != other.m_it;
  }

  DataSocket operator*() const
  {
    return DataSocket(m_is_output, *m_it);
  }
};

/**
 * An iterator over sockets. This type should never appear in user code. Instead it is either used
 * directly in a range-for loop or it should be used with the auto keyword.
 */
template<typename SequenceT> class DataSocketSequence {
 private:
  bool m_is_output;
  SequenceT m_sequence;
  using IdIteratorT = decltype(m_sequence.begin());

 public:
  DataSocketSequence(bool is_output, SequenceT sequence)
      : m_is_output(is_output), m_sequence(sequence)
  {
  }

  DataSocketIterator<IdIteratorT> begin()
  {
    return DataSocketIterator<IdIteratorT>(m_is_output, m_sequence.begin());
  }

  DataSocketIterator<IdIteratorT> end()
  {
    return DataSocketIterator<IdIteratorT>(m_is_output, m_sequence.end());
  }

  uint size() const
  {
    return m_sequence.size();
  }
};

class DataGraph;
using SharedDataGraph = AutoRefCount<DataGraph>;

class DataGraph : public RefCounter {
 public:
  struct Node {
    SharedFunction function;
    SourceInfo *source_info;
    /* Index into m_origins. */
    uint inputs_start;
    /* Index into m_targets_info. */
    uint outputs_start;

    Node(SharedFunction fn, SourceInfo *source_info, uint inputs_start, uint outputs_start)
        : function(std::move(fn)),
          source_info(source_info),
          inputs_start(inputs_start),
          outputs_start(outputs_start)
    {
    }
  };

  struct InputSocket {
    uint node;
    uint origin;

    InputSocket(uint node, uint origin) : node(node), origin(origin)
    {
    }
  };

  struct OutputSocket {
    uint node;
    uint targets_start;
    uint targets_amount;

    OutputSocket(uint node, uint targets_start, uint targets_amount)
        : node(node), targets_start(targets_start), targets_amount(targets_amount)
    {
    }
  };

 private:
  Vector<Node> m_nodes;
  Vector<InputSocket> m_inputs;
  Vector<OutputSocket> m_outputs;
  Vector<uint> m_targets;
  std::unique_ptr<MonotonicAllocator> m_source_info_allocator;

 public:
  DataGraph(Vector<Node> nodes,
            Vector<InputSocket> inputs,
            Vector<OutputSocket> outputs,
            Vector<uint> targets,
            std::unique_ptr<MonotonicAllocator> source_info_allocator);

  DataGraph(DataGraph &other) = delete;
  ~DataGraph();

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("FN:DataGraph")
#endif

  Range<uint> node_ids() const
  {
    return Range<uint>(0, m_nodes.size());
  }

  SharedFunction &function_of_node(uint node_id) const
  {
    /* A function is mostly immutable anyway. */
    const SharedFunction &fn = m_nodes[node_id].function;
    return const_cast<SharedFunction &>(fn);
  }

  SharedFunction &function_of_input(uint input_id) const
  {
    return this->function_of_node(m_inputs[input_id].node);
  }

  SharedFunction &function_of_output(uint output_id) const
  {
    return this->function_of_node(m_outputs[output_id].node);
  }

  uint id_of_node_input(uint node_id, uint input_index) const
  {
    BLI_assert(input_index < this->input_ids_of_node(node_id).size());
    return m_nodes[node_id].inputs_start + input_index;
  }

  uint id_of_node_output(uint node_id, uint output_index) const
  {
    BLI_assert(output_index < this->output_ids_of_node(node_id).size());
    return m_nodes[node_id].outputs_start + output_index;
  }

  DataSocket socket_of_node_input(uint node_id, uint input_index) const
  {
    return DataSocket(false, this->id_of_node_input(node_id, input_index));
  }

  DataSocket socket_of_node_output(uint node_id, uint output_index) const
  {
    return DataSocket(true, this->id_of_node_output(node_id, output_index));
  }

  Range<uint> input_ids_of_node(uint node_id) const
  {
    const Node &node = m_nodes[node_id];
    return Range<uint>(node.inputs_start, node.inputs_start + node.function->input_amount());
  }

  DataSocketSequence<Range<uint>> inputs_of_node(uint node_id) const
  {
    return DataSocketSequence<Range<uint>>(false, this->input_ids_of_node(node_id));
  }

  Range<uint> output_ids_of_node(uint node_id) const
  {
    const Node &node = m_nodes[node_id];
    return Range<uint>(node.outputs_start, node.outputs_start + node.function->output_amount());
  }

  DataSocketSequence<Range<uint>> outputs_of_node(uint node_id) const
  {
    return DataSocketSequence<Range<uint>>(true, this->output_ids_of_node(node_id));
  }

  uint first_input_id_of_node(uint node_id) const
  {
    return m_nodes[node_id].inputs_start;
  }

  uint first_output_id_of_node(uint node_id) const
  {
    return m_nodes[node_id].outputs_start;
  }

  SourceInfo *source_info_of_node(uint node_id) const
  {
    return m_nodes[node_id].source_info;
  }

  const char *name_ptr_of_node(uint node_id) const
  {
    return m_nodes[node_id].function->name().data();
  }

  uint origin_of_input(uint input_id) const
  {
    return m_inputs[input_id].origin;
  }

  DataSocket origin_of_input(DataSocket input_socket) const
  {
    BLI_assert(input_socket.is_input());
    return DataSocket::FromOutput(this->origin_of_input(input_socket.id()));
  }

  ArrayRef<uint> targets_of_output(uint output_id) const
  {
    const OutputSocket &data = m_outputs[output_id];
    return ArrayRef<uint>(&m_targets[data.targets_start], data.targets_amount);
  }

  DataSocketSequence<ArrayRef<uint>> targets_of_output(DataSocket output_socket) const
  {
    BLI_assert(output_socket.is_output());
    return DataSocketSequence<ArrayRef<uint>>(false, this->targets_of_output(output_socket.id()));
  }

  uint node_id_of_socket(DataSocket socket) const
  {
    if (socket.is_input()) {
      return this->node_id_of_input(socket);
    }
    else {
      return this->node_id_of_output(socket);
    }
  }

  uint node_id_of_input(uint input_id) const
  {
    return m_inputs[input_id].node;
  }

  uint node_id_of_input(DataSocket input_socket) const
  {
    BLI_assert(input_socket.is_input());
    return this->node_id_of_input(input_socket.id());
  }

  uint node_id_of_output(uint output_id) const
  {
    return m_outputs[output_id].node;
  }

  uint node_id_of_output(DataSocket output_socket) const
  {
    BLI_assert(output_socket.is_output());
    return this->node_id_of_output(output_socket.id());
  }

  uint index_of_socket(DataSocket socket) const
  {
    if (socket.is_input()) {
      return this->index_of_input(socket);
    }
    else {
      return this->index_of_output(socket);
    }
  }

  uint index_of_input(uint input_id) const
  {
    return input_id - m_nodes[m_inputs[input_id].node].inputs_start;
  }

  uint index_of_input(DataSocket input_socket) const
  {
    BLI_assert(input_socket.is_input());
    return this->index_of_input(input_socket.id());
  }

  uint index_of_output(uint output_id) const
  {
    return output_id - m_nodes[m_outputs[output_id].node].outputs_start;
  }

  uint index_of_output(DataSocket output_socket) const
  {
    BLI_assert(output_socket.is_output());
    return this->index_of_output(output_socket.id());
  }

  StringRefNull name_of_socket(DataSocket socket) const
  {
    if (socket.is_input()) {
      return this->name_of_input(socket.id());
    }
    else {
      return this->name_of_output(socket.id());
    }
  }

  Type *type_of_socket(DataSocket socket) const
  {
    if (socket.is_input()) {
      return this->type_of_input(socket.id());
    }
    else {
      return this->type_of_output(socket.id());
    }
  }

  StringRefNull name_of_input(uint input_id) const
  {
    return this->function_of_input(input_id)->input_name(this->index_of_input(input_id));
  }

  StringRefNull name_of_output(uint output_id) const
  {
    return this->function_of_output(output_id)->output_name(this->index_of_output(output_id));
  }

  Type *type_of_input(uint input_id) const
  {
    return this->function_of_input(input_id)->input_type(this->index_of_input(input_id));
  }

  Type *type_of_output(uint output_id) const
  {
    return this->function_of_output(output_id)->output_type(this->index_of_output(output_id));
  }

  Type *type_of_input(DataSocket input_socket) const
  {
    BLI_assert(input_socket.is_input());
    return this->type_of_input(input_socket.id());
  }

  Type *type_of_output(DataSocket output_socket) const
  {
    BLI_assert(output_socket.is_output());
    return this->type_of_output(output_socket.id());
  }

  std::string to_dot();
  void to_dot__clipboard();

  void print_socket(DataSocket socket) const;

 private:
  void insert_in_builder(DataGraphBuilder &builder);
};

}  // namespace FN

namespace BLI {
template<> struct DefaultHash<FN::DataSocket> {
  uint32_t operator()(FN::DataSocket value) const
  {
    return value.id() + (int)value.is_input() * 12345;
  }
};
}  // namespace BLI
