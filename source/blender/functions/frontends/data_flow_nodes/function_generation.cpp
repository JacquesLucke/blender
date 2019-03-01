#include "function_generation.hpp"
#include "graph_generation.hpp"

#include "FN_tuple_call.hpp"
#include "FN_dependencies.hpp"
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
		fn->add_body(fgraph_tuple_call(fgraph));
		fn->add_body(fgraph_dependencies(fgraph));
		return fn;
	}

} } /* namespace FN::DataFlowNodes */