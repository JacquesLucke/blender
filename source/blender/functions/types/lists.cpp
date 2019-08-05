#include "BLI_lazy_init.hpp"

#include "FN_tuple.hpp"
#include "FN_llvm.hpp"
#include "FN_types.hpp"

namespace FN {
namespace Types {

class ListTypeInfo : CPPTypeInfoForType<GenericList> {
 private:
  SharedType m_base_type;

 public:
  ListTypeInfo(SharedType base_type) : m_base_type(std::move(base_type))
  {
  }

  void construct_default(void *ptr) const override
  {
    new (ptr) GenericList(m_base_type);
  }

  void construct_default_n(void *ptr, uint n) const override
  {
    for (uint i = 0; i < n; i++) {
      new ((GenericList *)ptr + i) GenericList(m_base_type);
    }
  }
};

SharedType new_list_type(SharedType &base_type)
{
  SharedType type = SharedType::New(base_type->name() + " List");
  type->add_extension<CPPTypeInfoForType<GenericList>>();
  return type;
}

BLI_LAZY_INIT(SharedType, GET_TYPE_float_list)
{
  return new_list_type(GET_TYPE_float());
}

BLI_LAZY_INIT(SharedType, GET_TYPE_float3_list)
{
  return new_list_type(GET_TYPE_float3());
}

BLI_LAZY_INIT(SharedType, GET_TYPE_int32_list)
{
  return new_list_type(GET_TYPE_int32());
}

BLI_LAZY_INIT(SharedType, GET_TYPE_bool_list)
{
  return new_list_type(GET_TYPE_bool());
}

BLI_LAZY_INIT(SharedType, GET_TYPE_object_list)
{
  return new_list_type(GET_TYPE_object());
}

BLI_LAZY_INIT(SharedType, GET_TYPE_rgba_f_list)
{
  return new_list_type(GET_TYPE_rgba_f());
}

}  // namespace Types
}  // namespace FN
