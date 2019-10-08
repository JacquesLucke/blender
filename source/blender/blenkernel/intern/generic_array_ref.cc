#include <mutex>

#include "BLI_lazy_init_cxx.h"
#include "BLI_map.h"

#include "BKE_generic_array_ref.h"

namespace BKE {

using BLI::Map;

using TypeMapping = Map<const CPPType *, std::unique_ptr<ArrayRefCPPType>>;

BLI_LAZY_INIT_STATIC(TypeMapping, get_mapping)
{
  return TypeMapping{};
}

static std::mutex mapping_mutex;

ArrayRefCPPType &get_generic_array_ref_cpp_type(CPPType &base)
{
  std::lock_guard<std::mutex> lock(mapping_mutex);

  TypeMapping &mapping = get_mapping();
  auto &type = mapping.lookup_or_add(&base, [&]() {
    CPPType &generalization = get_cpp_type<GenericArrayRef>();
    ArrayRefCPPType *new_type = new ArrayRefCPPType(base, generalization);
    return std::unique_ptr<ArrayRefCPPType>(new_type);
  });
  return *type;
}

ArrayRefCPPType::ArrayRefCPPType(CPPType &base_type, CPPType &generalization)
    : CPPType("GenericArrayRef for " + base_type.name(),
              generalization,
              ArrayRefCPPType::ConstructDefaultCB),
      m_base_type(base_type)
{
}

}  // namespace BKE