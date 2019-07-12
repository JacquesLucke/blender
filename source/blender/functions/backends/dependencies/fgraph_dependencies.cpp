#include "fgraph_dependencies.hpp"

namespace FN {

class FGraphDependencies : public DependenciesBody {
 private:
  FunctionGraph m_fgraph;
  SharedDataFlowGraph m_graph;

 public:
  FGraphDependencies(FunctionGraph &function_graph)
      : m_fgraph(function_graph), m_graph(m_fgraph.graph())
  {
  }

  void dependencies(ExternalDependenciesBuilder &deps) const override
  {
    SmallSetVector<Object *> transform_dependencies;
    for (uint i = 0; i < m_fgraph.outputs().size(); i++) {
      DFGraphSocket socket = m_fgraph.outputs()[i];
      SmallVector<Object *> outputs = this->find_deps_and_outputs(
          socket, transform_dependencies, deps);
      deps.set_output_objects(i, outputs);
    }
    deps.depends_on_transforms_of(transform_dependencies);
  }

  SmallVector<Object *> find_deps_and_outputs(DFGraphSocket socket,
                                              SmallSetVector<Object *> &transform_dependencies,
                                              ExternalDependenciesBuilder &deps) const
  {
    if (m_fgraph.inputs().contains(socket)) {
      return deps.get_input_objects(m_fgraph.inputs().index(socket));
    }
    else if (socket.is_input()) {
      return this->find_deps_and_outputs(
          m_graph->origin_of_input(socket), transform_dependencies, deps);
    }
    else {
      uint node_id = m_graph->node_id_of_output(socket);
      SharedFunction &fn = m_graph->function_of_node(node_id);
      DependenciesBody *body = fn->body<DependenciesBody>();
      if (body == nullptr) {
        for (auto input_socket : m_graph->inputs_of_node(node_id)) {
          this->find_deps_and_outputs(input_socket, transform_dependencies, deps);
        }
        return {};
      }
      else {
        SmallMultiMap<uint, Object *> inputs;
        for (uint i = 0; i < fn->input_amount(); i++) {
          inputs.add_multiple_new(
              i,
              this->find_deps_and_outputs(
                  m_graph->socket_of_node_input(node_id, i), transform_dependencies, deps));
        }
        ExternalDependenciesBuilder builder(inputs);
        body->dependencies(builder);
        transform_dependencies.add_multiple(builder.get_transform_dependencies());
        return builder.get_output_objects(m_graph->index_of_output(socket));
      }
    }
  }
};

void fgraph_add_DependenciesBody(SharedFunction &fn, FunctionGraph &fgraph)
{
  fn->add_body<FGraphDependencies>(fgraph);
}

}  // namespace FN
