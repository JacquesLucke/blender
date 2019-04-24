#include "fgraph_dependencies.hpp"

namespace FN {

class FGraphDependencies : public DependenciesBody {
 private:
  SharedCompactDataFlowGraph m_graph;

 public:
  FGraphDependencies(CompactFunctionGraph &function_graph) : m_graph(function_graph.graph())
  {
  }

  void dependencies(Dependencies &deps) const override
  {
    for (uint node_id : m_graph->node_ids()) {
      DependenciesBody *body = m_graph->function_of_node(node_id)->body<DependenciesBody>();
      if (body) {
        body->dependencies(deps);
      }
    }
  }
};

void fgraph_add_DependenciesBody(SharedFunction &fn, CompactFunctionGraph &fgraph)
{
  fn->add_body(new FGraphDependencies(fgraph));
}

}  // namespace FN
