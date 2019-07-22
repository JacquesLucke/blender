#pragma once

#include "BLI_string_map.hpp"
#include "FN_core.hpp"

namespace FN {
namespace DataFlowNodes {

StringMap<SharedType> &get_type_by_idname_map();
StringMap<SharedType> &get_type_by_data_type_map();

}  // namespace DataFlowNodes
}  // namespace FN
