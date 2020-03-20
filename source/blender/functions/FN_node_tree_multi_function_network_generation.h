#ifndef __FN_VTREE_MULTI_FUNCTION_NETWORK_GENERATION_H__
#define __FN_VTREE_MULTI_FUNCTION_NETWORK_GENERATION_H__

#include "BLI_resource_collector.h"
#include "FN_node_tree_multi_function_network.h"
#include "intern/multi_functions/network.h"

namespace FN {
namespace MFGeneration {

using BLI::ResourceCollector;

std::unique_ptr<FunctionTreeMFNetwork> generate_node_tree_multi_function_network(
    const FunctionTree &function_tree, ResourceCollector &resources);

std::unique_ptr<MF_EvaluateNetwork> generate_node_tree_multi_function(
    const FunctionTree &function_tree, ResourceCollector &resources);

}  // namespace MFGeneration
}  // namespace FN

#endif /* __FN_VTREE_MULTI_FUNCTION_NETWORK_GENERATION_H__ */
