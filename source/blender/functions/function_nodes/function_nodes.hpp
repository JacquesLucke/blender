#include "FN_functions.hpp"
#include "DNA_node_types.h"
#include "BLI_listbase_wrapper.hpp"

namespace FN::FunctionNodes {

	using bNodeList = ListBaseWrapper<bNode, true>;
	using bLinkList = ListBaseWrapper<bNodeLink, true>;
	using bSocketList = ListBaseWrapper<bNodeSocket, true>;

	class FunctionNodeTree {
	private:
		bNodeTree *m_tree;

	public:
		FunctionNodeTree(bNodeTree *tree)
			: m_tree(tree) {}

		bNodeList nodes() const
		{
			return bNodeList(&m_tree->nodes);
		}

		bLinkList links() const
		{
			return bLinkList(&m_tree->links);
		}

		FunctionGraph to_function_graph() const;
	};

} /* namespace FN::FunctionNodes */