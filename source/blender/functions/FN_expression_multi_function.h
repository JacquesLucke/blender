#pragma once

#include "FN_multi_function.h"
#include "BLI_resource_collector.h"

namespace FN {
namespace Expr {

using BLI::ResourceCollector;

const MultiFunction &expression_to_multi_function(StringRef str, ResourceCollector &resources);

}  // namespace Expr
}  // namespace FN
