#include <mutex>

#include "BLI_lazy_init_cxx.h"
#include "BLI_map.h"

#include "BKE_generic_array_ref.h"

namespace BKE {

using BLI::Map;

using ArrayRefTypeMapping = Map<const CPPType *, std::unique_ptr<ArrayRefCPPType>>;
using MutableArrayRefTypeMapping = Map<const CPPType *, std::unique_ptr<MutableArrayRefCPPType>>;

BLI_LAZY_INIT_STATIC(ArrayRefTypeMapping, get_type_mapping)
{
  return ArrayRefTypeMapping{};
}

BLI_LAZY_INIT_STATIC(MutableArrayRefTypeMapping, get_mutable_type_mapping)
{
  return MutableArrayRefTypeMapping{};
}

static std::mutex map_mutex__immutable;
static std::mutex map_mutex__mutable;

ArrayRefCPPType &GET_TYPE_array_ref(CPPType &base)
{
  std::lock_guard<std::mutex> lock(map_mutex__immutable);

  ArrayRefTypeMapping &mapping = get_type_mapping();

  auto &type = mapping.lookup_or_add(&base, [&]() {
    ArrayRefCPPType *new_type = new ArrayRefCPPType(base);
    return std::unique_ptr<ArrayRefCPPType>(new_type);
  });
  return *type;
}

MutableArrayRefCPPType &GET_TYPE_mutable_array_ref(CPPType &base)
{
  std::lock_guard<std::mutex> lock(map_mutex__mutable);

  MutableArrayRefTypeMapping &mapping = get_mutable_type_mapping();

  auto &type = mapping.lookup_or_add(&base, [&]() {
    MutableArrayRefCPPType *new_type = new MutableArrayRefCPPType(base);
    return std::unique_ptr<MutableArrayRefCPPType>(new_type);
  });

  return *type;
}

ArrayRefCPPType::ArrayRefCPPType(CPPType &base_type)
    : CPPType("GenericArrayRef for " + base_type.name(),
              GET_TYPE<GenericArrayRef>(),
              ArrayRefCPPType::ConstructDefaultCB),
      m_base_type(base_type)
{
}

MutableArrayRefCPPType::MutableArrayRefCPPType(CPPType &base_type)
    : CPPType("GenericMutableArrayRef for " + base_type.name(),
              GET_TYPE<GenericMutableArrayRef>(),
              MutableArrayRefCPPType::ConstructDefaultCB),
      m_base_type(base_type)
{
}

}  // namespace BKE