#pragma once

#include "FN_vtree_multi_function_network_builder.h"

#include "BLI_map.h"
#include "BLI_owned_resources.h"

namespace FN {

using BLI::Map;
using BLI::OwnedResources;
using BLI::StringMap;

using InsertVNodeFunction = std::function<void(
    VTreeMFNetworkBuilder &builder, OwnedResources &resources, const VNode &vnode)>;
using InsertUnlinkedInputFunction = std::function<MFBuilderOutputSocket &(
    VTreeMFNetworkBuilder &builder, OwnedResources &resources, const VSocket &vsocket)>;
using InsertImplicitConversionFunction =
    std::function<std::pair<MFBuilderInputSocket *, MFBuilderOutputSocket *>(
        VTreeMFNetworkBuilder &builder, OwnedResources &resources)>;

struct VTreeMultiFunctionMappings {
  StringMap<MFDataType> data_type_by_idname;
  StringMap<const CPPType *> cpp_type_by_type_name;
  Map<const CPPType *, std::string> type_name_from_cpp_type;
  StringMap<InsertVNodeFunction> vnode_inserters;
  StringMap<InsertUnlinkedInputFunction> input_inserters;
  Map<std::pair<std::string, std::string>, InsertImplicitConversionFunction> conversion_inserters;
};

const VTreeMultiFunctionMappings &get_vtree_multi_function_mappings();

}  // namespace FN
