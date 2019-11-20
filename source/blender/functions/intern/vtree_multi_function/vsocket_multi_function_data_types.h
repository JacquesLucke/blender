#pragma once

#include "BKE_virtual_node_tree.h"

#include "FN_multi_function_data_type.h"

#include "BLI_array_cxx.h"
#include "BLI_optional.h"

namespace FN {

using BKE::VirtualNodeTree;
using BKE::VSocket;
using BLI::Array;
using BLI::Optional;

class VSocketMFDataTypes {
 private:
  const VirtualNodeTree &m_vtree;
  Array<Optional<MFDataType>> m_data_type_by_vsocket_id;

 public:
  VSocketMFDataTypes(const VirtualNodeTree &vtree,
                     Array<Optional<MFDataType>> data_type_by_vsocket_id)
      : m_vtree(vtree), m_data_type_by_vsocket_id(std::move(data_type_by_vsocket_id))
  {
    BLI_assert(m_vtree.socket_count() == m_data_type_by_vsocket_id.size());
  }

  Optional<MFDataType> try_lookup(const VSocket &vsocket) const
  {
    BLI_assert(&vsocket.tree() == &m_vtree);
    return m_data_type_by_vsocket_id[vsocket.id()];
  }

  MFDataType lookup(const VSocket &vsocket) const
  {
    return this->try_lookup(vsocket).value();
  }

  bool is_data_socket(const VSocket &vsocket) const
  {
    return this->try_lookup(vsocket).has_value();
  }
};

}  // namespace FN
