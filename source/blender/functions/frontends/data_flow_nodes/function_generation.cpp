#include "function_generation.hpp"
#include "graph_generation.hpp"

#include "DNA_node_types.h"

namespace FN { namespace DataFlowNodes {

	Optional<SharedFunction> generate_function(bNodeTree *btree)
	{
		Optional<FunctionGraph> fgraph_ = generate_function_graph(btree);
		if (!fgraph_.has_value()) {
			return {};
		}

		FunctionGraph fgraph = fgraph_.value();

		auto fn = SharedFunction::New(btree->id.name, fgraph.signature());
		fn->add_body(function_graph_to_callable(fgraph));
		return fn;
	}

} } /* namespace FN::DataFlowNodes */