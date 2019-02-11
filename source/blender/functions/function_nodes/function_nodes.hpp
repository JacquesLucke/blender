#include "FN_functions.hpp"
#include "DNA_node_types.h"

namespace FN::FunctionNodes {

	class FunctionNodeTree {
	private:
		bNodeTree *m_tree;

	public:
		FunctionNodeTree(bNodeTree *tree)
			: m_tree(tree) {}

		FunctionGraph to_function_graph() const;
	};

} /* namespace FN::FunctionNodes */