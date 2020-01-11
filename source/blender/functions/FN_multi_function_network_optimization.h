#ifndef __FN_MULTI_FUNCTION_NETWORK_OPTIMIZATION_H__
#define __FN_MULTI_FUNCTION_NETWORK_OPTIMIZATION_H__

#include "FN_multi_function_network.h"

#include "BLI_resource_collector.h"

namespace FN {

using BLI::ResourceCollector;

void optimize_network__constant_folding(MFNetworkBuilder &network, ResourceCollector &resources);
void optimize_network__remove_unused_nodes(MFNetworkBuilder &network_builder);

}  // namespace FN

#endif /* __FN_MULTI_FUNCTION_NETWORK_OPTIMIZATION_H__ */
