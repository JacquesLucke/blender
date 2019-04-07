#include "fgraph_dependencies.hpp"

namespace FN {

	class FGraphDependencies : public DependenciesBody {
	private:
		SharedDataFlowGraph m_graph;

	public:
		FGraphDependencies(FunctionGraph &function_graph)
			: m_graph(function_graph.graph()) {}

		void dependencies(Dependencies &deps) const override
		{
			for (Node *node : m_graph->all_nodes()) {
				DependenciesBody *body = node->function()->body<DependenciesBody>();
				if (body) body->dependencies(deps);
			}
		}
	};

	void fgraph_add_DependenciesBody(
		SharedFunction &fn,
		FunctionGraph &fgraph)
	{
		fn->add_body(new FGraphDependencies(fgraph));
	}

} /* namespace FN */
