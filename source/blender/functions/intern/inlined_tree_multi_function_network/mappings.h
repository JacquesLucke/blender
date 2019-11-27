#pragma once

#include "BLI_map.h"
#include "BLI_string_map.h"
#include "BLI_resource_collector.h"

#include "FN_inlined_tree_multi_function_network.h"

namespace FN {

using BLI::Map;
using BLI::ResourceCollector;
using BLI::StringMap;

struct VTreeMultiFunctionMappings;
class VTreeMFNetworkBuilder;
class VNodeMFNetworkBuilder;
class VSocketMFNetworkBuilder;

using InsertVNodeFunction = std::function<void(VNodeMFNetworkBuilder &builder)>;
using InsertVSocketFunction = std::function<void(VSocketMFNetworkBuilder &builder)>;
using InsertImplicitConversionFunction = std::function<
    std::pair<MFBuilderInputSocket *, MFBuilderOutputSocket *>(VTreeMFNetworkBuilder &builder)>;

struct VTreeMultiFunctionMappings {
  StringMap<MFDataType> data_type_by_idname;
  StringMap<const CPPType *> cpp_type_by_type_name;
  StringMap<MFDataType> data_type_by_type_name;
  Map<const CPPType *, std::string> type_name_from_cpp_type;
  StringMap<InsertVNodeFunction> xnode_inserters;
  StringMap<InsertVSocketFunction> xsocket_inserters;
  Map<std::pair<std::string, std::string>, InsertImplicitConversionFunction> conversion_inserters;
};

void add_inlined_tree_socket_mapping_info(VTreeMultiFunctionMappings &mappings);
void add_inlined_tree_node_mapping_info(VTreeMultiFunctionMappings &mappings);

const VTreeMultiFunctionMappings &get_inlined_tree_multi_function_mappings();

}  // namespace FN
