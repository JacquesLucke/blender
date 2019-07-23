#pragma once

#include "FN_core.hpp"
#include "BLI_optional.hpp"

struct bNodeTree;

namespace FN {
namespace DataFlowNodes {

Optional<SharedFunction> generate_function(struct bNodeTree *btree);

}  // namespace DataFlowNodes
}  // namespace FN
