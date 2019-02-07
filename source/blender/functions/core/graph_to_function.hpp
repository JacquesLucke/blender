#pragma once

#include "data_flow_graph.hpp"

namespace FN {

	SharedFunction function_from_data_flow(
		const SharedDataFlowGraph &graph,
		const SmallSocketVector &inputs,
		const SmallSocketVector &outputs);

} /* namespace FN */