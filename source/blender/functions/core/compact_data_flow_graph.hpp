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
  MemMultiPool *m_source_info_pool;

 public:
  CompactDataFlowGraph(DataFlowGraph *orig_graph);
  CompactDataFlowGraph(CompactDataFlowGraph &other) = delete;

  ~CompactDataFlowGraph()
  {
    delete m_source_info_pool;
  }

  Range<uint> nodes() const
  {
    return Range<uint>(0, m_nodes.size());
  }

  SharedFunction &function_of_node(uint node)
  {
    return m_nodes[node].function;
  }

  Range<uint> inputs_of_node(uint node) const
  {
    MyNode &node_ = m_nodes[node];
    return Range<uint>(node_.inputs_start,
                       node_.inputs_start + node_.function->signature().inputs().size());
  }

  Range<uint> outputs_of_node(uint node) const
  {
    MyNode &node_ = m_nodes[node];
    return Range<uint>(node_.outputs_start,
                       node_.outputs_start + node_.function->signature().outputs().size());
  }

  SourceInfo *source_of_node(uint node) const
  {
    return m_nodes[node].source;
  }

  const char *name_of_node(uint node) const
  {
    return m_nodes[node].function->name().c_str();
  }

  uint origin(uint input_socket) const
  {
    return m_inputs[input_socket].origin;
  }

  FunctionSocket origin(FunctionSocket input_socket) const
  {
    BLI_assert(input_socket.is_input());
    return FunctionSocket::FromOutput(this->origin(input_socket.id()));
  }

  ArrayRef<uint> targets(uint output_socket) const
  {
    OutputSocket &data = m_outputs[output_socket];
    return ArrayRef<uint>(&m_targets[data.targets_start], data.targets_amount);
  }

  uint node_of_input(uint input_socket) const
  {
    return m_inputs[input_socket].node;
  }

  uint node_of_output(uint output_socket) const
  {
    return m_outputs[output_socket].node;
  }

  uint index_of_input(uint input_socket) const
  {
    return input_socket - m_nodes[m_inputs[input_socket].node].inputs_start;
  }

  uint index_of_output(uint output_socket) const
  {
    return output_socket - m_nodes[m_outputs[output_socket].node].outputs_start;
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
    uint node = this->node_of_input(input_socket);
    uint index = this->index_of_input(input_socket);
    return this->function_of_node(node)->signature().inputs()[index];
  }

  OutputParameter &output_parameter(uint output_socket)
  {
    uint node = this->node_of_output(output_socket);
    uint index = this->index_of_output(output_socket);
    return this->function_of_node(node)->signature().outputs()[index];
  }
};

using SharedCompactDataFlowGraph = AutoRefCount<CompactDataFlowGraph>;

}  // namespace FN
