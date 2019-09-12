#pragma once

#include "FN_core.hpp"
#include "BLI_value_or_error.h"

struct bNodeTree;

namespace FN {
namespace DataFlowNodes {

using BLI::ValueOrError;

ValueOrError<SharedFunction> generate_function(struct bNodeTree *btree);

}  // namespace DataFlowNodes
}  // namespace FN
