#include "FN_functions.hpp"
#include "DNA_node_types.h"

namespace FN::FunctionNodes {

	class FunctionNodeTree {
	private:
		bNodeTree *m_tree;

	public:
		FunctionNodeTree(bNodeTree *tree)
			: m_tree(tree) {}

		SharedDataFlowGraph to_data_flow_graph() const;
	};

} /* namespace FN::FunctionNodes */