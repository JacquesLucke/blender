#pragma once

#include "BLI_map.h"
#include "BLI_string_map.h"
#include "BLI_resource_collector.h"

#include "FN_node_tree_multi_function_network.h"

namespace FN {
namespace MFGeneration {

using BLI::Map;
using BLI::ResourceCollector;
using BLI::StringMap;

class FNodeMFBuilder;
class VSocketMFBuilder;
class ConversionMFBuilder;

using FNodeInserter = std::function<void(FNodeMFBuilder &builder)>;
using VSocketInserter = std::function<void(VSocketMFBuilder &builder)>;
using ConversionInserter = std::function<void(ConversionMFBuilder &builder)>;

struct FunctionTreeMFMappings {
  StringMap<MFDataType> data_type_by_idname;
  StringMap<const CPPType *> cpp_type_by_type_name;
  StringMap<MFDataType> data_type_by_type_name;
  Map<const CPPType *, std::string> type_name_from_cpp_type;
  StringMap<FNodeInserter> fnode_inserters;
  StringMap<VSocketInserter> fsocket_inserters;
  Map<std::pair<std::string, std::string>, ConversionInserter> conversion_inserters;
};

void add_function_tree_socket_mapping_info(FunctionTreeMFMappings &mappings);
void add_function_tree_node_mapping_info(FunctionTreeMFMappings &mappings);

const FunctionTreeMFMappings &get_function_tree_multi_function_mappings();

}  // namespace MFGeneration
}  // namespace FN
