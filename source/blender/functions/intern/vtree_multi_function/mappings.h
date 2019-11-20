#pragma once

#include "BLI_string_map.h"
#include "BLI_map.h"
#include "BLI_optional.h"

#include "FN_multi_function.h"

namespace FN {

using BLI::Map;
using BLI::Optional;
using BLI::StringMap;
using BLI::StringRef;

class VNodeMFWrapperBuilder;
class VSocketMFBuilder;

using BuildVNodeMFWrapperFunc = void (*)(VNodeMFWrapperBuilder &builder);
using BuildVSocketMFFunc = void (*)(VSocketMFBuilder &builder);

struct VTreeMFMappings {
  StringMap<BuildVNodeMFWrapperFunc> vnode_builders;
  StringMap<BuildVSocketMFFunc> vsocket_builders;
  Map<std::pair<std::string, std::string>, std::unique_ptr<MultiFunction>> conversion_functions;

  StringMap<MFDataType> data_type_by_idname;
  StringMap<const CPPType *> cpp_type_by_name;
  StringMap<MFDataType> data_type_by_name;
  Map<const CPPType *, std::string> name_from_cpp_type;
};

const VTreeMFMappings &get_vtree_mf_mappings();

}  // namespace FN
