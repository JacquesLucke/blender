#include "fgraph_dependencies.hpp"

namespace FN {

class FGraphDependencies : public DepsBody {
 private:
  FunctionGraph m_fgraph;
  SharedDataFlowGraph m_graph;

 public:
  FGraphDependencies(FunctionGraph &function_graph)
      : m_fgraph(function_graph), m_graph(m_fgraph.graph())
  {
  }

  void build_deps(FunctionDepsBuilder &builder) const override
  {
    for (uint i = 0; i < m_fgraph.outputs().size(); i++) {
      DFGraphSocket socket = m_fgraph.outputs()[i];
      Vector<ID *> outputs = this->find_deps_and_outputs(socket, builder);
      builder.add_output_ids(i, outputs);
    }
  }

  Vector<ID *> find_deps_and_outputs(DFGraphSocket socket,
                                     FunctionDepsBuilder &parent_builder) const
  {
    if (m_fgraph.inputs().contains(socket)) {
      return parent_builder.get_input_ids(m_fgraph.inputs().index(socket));
    }
    else if (socket.is_input()) {
      return this->find_deps_and_outputs(m_graph->origin_of_input(socket), parent_builder);
    }
    else {
      uint node_id = m_graph->node_id_of_output(socket);
      SharedFunction &fn = m_graph->function_of_node(node_id);
      if (fn->has_body<DepsBody>()) {
        MultiMap<uint, ID *> input_ids;

        for (uint i = 0; i < fn->input_amount(); i++) {
          input_ids.add_multiple_new(
              i,
              this->find_deps_and_outputs(m_graph->socket_of_node_input(node_id, i),
                                          parent_builder));
        }

        MultiMap<uint, ID *> output_ids;
        FunctionDepsBuilder builder(input_ids, output_ids, parent_builder.dependency_components());
        DepsBody &body = fn->body<DepsBody>();
        body.build_deps(builder);
        return output_ids.lookup_default(m_graph->index_of_output(socket));
      }
      else {
        for (auto input_socket : m_graph->inputs_of_node(node_id)) {
          this->find_deps_and_outputs(input_socket, parent_builder);
        }
        return {};
      }
    }
  }
};

void fgraph_add_DependenciesBody(SharedFunction &fn, FunctionGraph &fgraph)
{
  fn->add_body<FGraphDependencies>(fgraph);
}

}  // namespace FN
