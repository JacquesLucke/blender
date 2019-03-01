#include "fgraph_dependencies.hpp"

namespace FN {

	class FGraphDependencies : public DependenciesBody {
	private:
		SharedDataFlowGraph m_graph;

	public:
		FGraphDependencies(const FunctionGraph &function_graph)
			: m_graph(function_graph.graph()) {}

		void dependencies(Dependencies &deps) const override
		{
			for (const Node *node : m_graph->all_nodes()) {
				const DependenciesBody *body = node->function()->body<DependenciesBody>();
				if (body) body->dependencies(deps);
			}
		}
	};

	DependenciesBody *fgraph_dependencies(
		const FunctionGraph &function_graph)
	{
		return new FGraphDependencies(function_graph);
	}

} /* namespace FN */