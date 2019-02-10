#pragma once

#include "data_flow_graph.hpp"
#include "cpu.hpp"

namespace FN {

	TupleCallBody *function_graph_to_callable(
		const FunctionGraph &function_graph);

} /* namespace FN */