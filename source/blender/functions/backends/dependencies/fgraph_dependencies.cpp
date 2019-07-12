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
    SmallSetVector<Object *> transform_dependencies;
    for (uint i = 0; i < m_fgraph.outputs().size(); i++) {
      DFGraphSocket socket = m_fgraph.outputs()[i];
      SmallVector<ID *> outputs = this->find_deps_and_outputs(
          socket, transform_dependencies, builder);
      builder.add_output_ids(i, outputs);
    }
    builder.add_transform_dependency(transform_dependencies);
  }

  SmallVector<ID *> find_deps_and_outputs(DFGraphSocket socket,
                                          SmallSetVector<Object *> &transform_dependencies,
                                          FunctionDepsBuilder &parent_builder) const
  {
    if (m_fgraph.inputs().contains(socket)) {
      return parent_builder.get_input_ids(m_fgraph.inputs().index(socket));
    }
    else if (socket.is_input()) {
      return this->find_deps_and_outputs(
          m_graph->origin_of_input(socket), transform_dependencies, parent_builder);
    }
    else {
      uint node_id = m_graph->node_id_of_output(socket);
      SharedFunction &fn = m_graph->function_of_node(node_id);
      DepsBody *body = fn->body<DepsBody>();
      if (body == nullptr) {
        for (auto input_socket : m_graph->inputs_of_node(node_id)) {
          this->find_deps_and_outputs(input_socket, transform_dependencies, parent_builder);
        }
        return {};
      }
      else {
        SmallMultiMap<uint, ID *> input_ids;

        for (uint i = 0; i < fn->input_amount(); i++) {
          input_ids.add_multiple_new(
              i,
              this->find_deps_and_outputs(m_graph->socket_of_node_input(node_id, i),
                                          transform_dependencies,
                                          parent_builder));
        }

        SmallMultiMap<uint, ID *> output_ids;
        FunctionDepsBuilder builder(input_ids, output_ids, transform_dependencies);
        body->build_deps(builder);
        return output_ids.lookup_default(m_graph->index_of_output(socket));
      }
    }
  }
};

void fgraph_add_DependenciesBody(SharedFunction &fn, FunctionGraph &fgraph)
{
  fn->add_body<FGraphDependencies>(fgraph);
}

}  // namespace FN
