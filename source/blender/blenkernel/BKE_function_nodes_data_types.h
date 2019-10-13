#ifndef __BKE_VIRTUAL_NODE_TREE_DATA_FLOW_H__
#define __BKE_VIRTUAL_NODE_TREE_DATA_FLOW_H__

#include "BKE_cpp_types.h"

#include "BLI_string_map.h"

namespace BKE {

using BLI::StringMap;

enum DataTypeCategory {
  Single,
  List,
};

struct SocketDataType {
  CPPType *type;
  DataTypeCategory category;

  static SocketDataType none_type()
  {
    return {nullptr, DataTypeCategory::Single};
  }
};

StringMap<SocketDataType> &get_function_nodes_data_types();

};  // namespace BKE

#endif /* __BKE_VIRTUAL_NODE_TREE_DATA_FLOW_H__ */
