#include <mutex>

#include "BLI_lazy_init_cxx.h"
#include "BLI_map.h"

#include "BKE_generic_array_ref.h"

namespace BKE {

using BLI::Map;

using TypeMapping = Map<const TypeCPP *, std::unique_ptr<ArrayRefTypeCPP>>;

BLI_LAZY_INIT_STATIC(TypeMapping, get_mapping)
{
  return TypeMapping{};
}

static std::mutex mapping_mutex;

ArrayRefTypeCPP &get_generic_array_ref_cpp_type(TypeCPP &base)
{
  std::lock_guard<std::mutex> lock(mapping_mutex);

  TypeMapping &mapping = get_mapping();
  auto &type = mapping.lookup_or_add(&base, [&]() {
    TypeCPP &generalization = get_type_cpp<GenericArrayRef>();
    ArrayRefTypeCPP *new_type = new ArrayRefTypeCPP(base, generalization);
    return std::unique_ptr<ArrayRefTypeCPP>(new_type);
  });
  return *type;
}

ArrayRefTypeCPP::ArrayRefTypeCPP(TypeCPP &base_type, TypeCPP &generalization)
    : TypeCPP("GenericArrayRef for " + base_type.name(),
              generalization,
              ArrayRefTypeCPP::ConstructDefaultCB),
      m_base_type(base_type)
{
}

}  // namespace BKE