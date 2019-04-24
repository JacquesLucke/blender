#pragma once

#include "function.hpp"
#include "data_flow_graph.hpp"
#include "BLI_range.hpp"

namespace FN {

struct FunctionSocket {
 private:
  bool m_is_output;
  uint m_id;

  FunctionSocket(bool is_output, uint id) : m_is_output(is_output), m_id(id)
  {
  }

 public:
  static FunctionSocket FromInput(uint id)
  {
    return FunctionSocket(false, id);
  }

  static FunctionSocket FromOutput(uint id)
  {
    return FunctionSocket(true, id);
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
};

template<typename SequenceT> class FunctionSocketIterator {
 private:
  bool m_is_output;
  SequenceT m_sequence;

 public:
  FunctionSocketIterator(bool is_output, SequenceT sequence)
      : m_is_output(is_output), m_sequence(sequence)
  {
  }

  FunctionSocketIterator begin() const
  {
    return FunctionSocketIterator(m_is_output, m_sequence.begin());
  }

  FunctionSocketIterator end() const
  {
    return FunctionSocketIterator(m_is_output, m_sequence.end());
  }

  uint size() const
  {
    return m_sequence.size();
  }

  FunctionSocketIterator &operator++()
  {
    ++m_sequence;
    return *this;
  }

  bool operator!=(const FunctionSocketIterator &other)
  {
    return m_sequence != other.m_sequence;
  }

  FunctionSocket operator*() const
  {
    return FunctionSocket(m_is_output, *m_sequence);
  }
};

class CompactDataFlowGraph : public RefCountedBase {
 private:
  struct MyNode {
    SharedFunction function;
    SourceInfo *source;
    /* Index into m_origins. */
    uint inputs_start;
    /* Index into m_targets_info. */
    uint outputs_start;

    MyNode(SharedFunction fn, SourceInfo *source, uint inputs_start, uint outputs_start)
        : function(std::move(fn)),
          source(source),
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

  SmallVector<MyNode> m_nodes;
  SmallVector<InputSocket> m_inputs;
  SmallVector<OutputSocket> m_outputs;
  SmallVector<uint> m_targets;
  std::unique_ptr<DataFlowGraph> m_builder;

 public:
  CompactDataFlowGraph(std::unique_ptr<DataFlowGraph> orig_graph);
  CompactDataFlowGraph(CompactDataFlowGraph &other) = delete;

  Range<uint> node_ids() const
  {
    return Range<uint>(0, m_nodes.size());
  }

  SharedFunction &function_of_node(uint node_id)
  {
    return m_nodes[node_id].function;
  }

  Range<uint> input_ids_of_node(uint node_id) const
  {
    MyNode &node = m_nodes[node_id];
    return Range<uint>(node.inputs_start,
                       node.inputs_start + node.function->signature().inputs().size());
  }

  FunctionSocketIterator<Range<uint>> inputs_of_node(uint node_id) const
  {
    return FunctionSocketIterator<Range<uint>>(false, this->input_ids_of_node(node_id));
  }

  Range<uint> output_ids_of_node(uint node_id) const
  {
    MyNode &node = m_nodes[node_id];
    return Range<uint>(node.outputs_start,
                       node.outputs_start + node.function->signature().outputs().size());
  }

  FunctionSocketIterator<Range<uint>> outputs_of_node(uint node_id) const
  {
    return FunctionSocketIterator<Range<uint>>(true, this->output_ids_of_node(node_id));
  }

  SourceInfo *source_info_of_node(uint node_id) const
  {
    return m_nodes[node_id].source;
  }

  const char *name_ptr_of_node(uint node_id) const
  {
    return m_nodes[node_id].function->name().c_str();
  }

  uint origin_of_input(uint input_id) const
  {
    return m_inputs[input_id].origin;
  }

  FunctionSocket origin_of_input(FunctionSocket input_socket) const
  {
    BLI_assert(input_socket.is_input());
    return FunctionSocket::FromOutput(this->origin_of_input(input_socket.id()));
  }

  ArrayRef<uint> targets_of_output(uint output_id) const
  {
    OutputSocket &data = m_outputs[output_id];
    return ArrayRef<uint>(&m_targets[data.targets_start], data.targets_amount);
  }

  FunctionSocketIterator<ArrayRef<uint>> targets_of_output(FunctionSocket output_socket) const
  {
    BLI_assert(output_socket.is_output());
    return FunctionSocketIterator<ArrayRef<uint>>(false,
                                                  this->targets_of_output(output_socket.id()));
  }

  uint node_id_of_input(uint input_id) const
  {
    return m_inputs[input_id].node;
  }

  uint node_id_of_input(FunctionSocket input_socket) const
  {
    BLI_assert(input_socket.is_input());
    return this->node_id_of_input(input_socket.id());
  }

  uint node_id_of_output(uint output_id) const
  {
    return m_outputs[output_id].node;
  }

  uint node_id_of_output(FunctionSocket output_socket) const
  {
    BLI_assert(output_socket.is_output());
    return this->node_id_of_output(output_socket.id());
  }

  uint index_of_input(uint input_id) const
  {
    return input_id - m_nodes[m_inputs[input_id].node].inputs_start;
  }

  uint index_of_input(FunctionSocket input_socket) const
  {
    BLI_assert(input_socket.is_input());
    return this->index_of_input(input_socket.id());
  }

  uint index_of_output(uint output_id) const
  {
    return output_id - m_nodes[m_outputs[output_id].node].outputs_start;
  }

  uint index_of_output(FunctionSocket output_socket) const
  {
    BLI_assert(output_socket.is_output());
    return this->index_of_output(output_socket.id());
  }

  const std::string &name_of_socket(FunctionSocket socket)
  {
    if (socket.is_input()) {
      return this->name_of_input(socket.id());
    }
    else {
      return this->name_of_output(socket.id());
    }
  }

  SharedType &type_of_socket(FunctionSocket socket)
  {
    if (socket.is_input()) {
      return this->type_of_input(socket.id());
    }
    else {
      return this->type_of_output(socket.id());
    }
  }

  const std::string &name_of_input(uint input_socket)
  {
    return this->input_parameter(input_socket).name();
  }

  const std::string &name_of_output(uint output_socket)
  {
    return this->output_parameter(output_socket).name();
  }

  SharedType &type_of_input(uint input_socket)
  {
    return this->input_parameter(input_socket).type();
  }

  SharedType &type_of_output(uint output_socket)
  {
    return this->output_parameter(output_socket).type();
  }

  InputParameter &input_parameter(uint input_socket)
  {
    uint node = this->node_id_of_input(input_socket);
    uint index = this->index_of_input(input_socket);
    return this->function_of_node(node)->signature().inputs()[index];
  }

  OutputParameter &output_parameter(uint output_socket)
  {
    uint node = this->node_id_of_output(output_socket);
    uint index = this->index_of_output(output_socket);
    return this->function_of_node(node)->signature().outputs()[index];
  }
};

using SharedCompactDataFlowGraph = AutoRefCount<CompactDataFlowGraph>;

}  // namespace FN
