#pragma once

#include "FN_core.hpp"
#include "BLI_optional.hpp"

struct bNodeTree;

namespace FN {
namespace DataFlowNodes {

Optional<CompactFunctionGraph> generate_function_graph(struct bNodeTree *btree);
}
}  // namespace FN
