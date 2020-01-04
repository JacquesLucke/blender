#ifndef __FN_VTREE_MULTI_FUNCTION_NETWORK_GENERATION_H__
#define __FN_VTREE_MULTI_FUNCTION_NETWORK_GENERATION_H__

#include "FN_node_tree_multi_function_network.h"
#include "BLI_resource_collector.h"
#include "intern/multi_functions/network.h"

namespace FN {
namespace MFGeneration {

using BLI::ResourceCollector;

std::unique_ptr<FunctionTreeMFNetwork> generate_node_tree_multi_function_network(
    const FunctionNodeTree &function_tree, ResourceCollector &resources);

std::unique_ptr<MF_EvaluateNetwork> generate_node_tree_multi_function(
    const FunctionNodeTree &function_tree, ResourceCollector &resources);

}  // namespace MFGeneration
}  // namespace FN

#endif /* __FN_VTREE_MULTI_FUNCTION_NETWORK_GENERATION_H__ */
