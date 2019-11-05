#ifndef __FN_VTREE_MULTI_FUNCTION_NETWORK_GENERATION_H__
#define __FN_VTREE_MULTI_FUNCTION_NETWORK_GENERATION_H__

#include "FN_vtree_multi_function_network.h"
#include "BLI_resource_collector.h"
#include "intern/multi_functions/network.h"

namespace FN {

using BLI::ResourceCollector;

std::unique_ptr<VTreeMFNetwork> generate_vtree_multi_function_network(
    const VirtualNodeTree &vtree, ResourceCollector &resources);

std::unique_ptr<MF_EvaluateNetwork> generate_vtree_multi_function(const VirtualNodeTree &vtree,
                                                                  ResourceCollector &resources);

}  // namespace FN

#endif /* __FN_VTREE_MULTI_FUNCTION_NETWORK_GENERATION_H__ */
