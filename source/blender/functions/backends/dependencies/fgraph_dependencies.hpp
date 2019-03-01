#pragma once

#include "FN_core.hpp"
#include "dependencies.hpp"

namespace FN {

	DependenciesBody *fgraph_dependencies(
		const FunctionGraph &function_graph);

} /* namespace FN */