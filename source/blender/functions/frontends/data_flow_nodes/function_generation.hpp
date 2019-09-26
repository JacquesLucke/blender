#pragma once

#include "FN_core.hpp"
#include "BLI_optional.h"

struct bNodeTree;

namespace FN {
namespace DataFlowNodes {

using BLI::Optional;

Optional<SharedFunction> generate_function(struct bNodeTree *btree);

}  // namespace DataFlowNodes
}  // namespace FN
