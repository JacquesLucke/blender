#include "BLI_lazy_init.hpp"

#include "FN_tuple.hpp"
#include "FN_llvm.hpp"
#include "FN_types.hpp"

namespace FN {
namespace Types {

class ListTypeInfo : public CPPTypeInfoForType<SharedList> {
 private:
  SharedType m_base_type;

 public:
  ListTypeInfo(SharedType base_type) : m_base_type(std::move(base_type))
  {
  }

  void construct_default(void *ptr) const override
  {
    new (ptr) SharedList(new List(m_base_type));
  }

  void construct_default_n(void *ptr, uint n) const override
  {
    SharedList *ptr_ = static_cast<SharedList *>(ptr);
    for (uint i = 0; i < n; i++) {
      new (ptr_ + i) SharedList(new List(m_base_type));
    }
  }
};

SharedType new_list_type(SharedType &base_type)
{
  SharedType type = SharedType::New(base_type->name() + " List");
  type->add_extension<ListTypeInfo>(base_type);
  type->add_extension<PointerLLVMTypeInfo>(
      /* Copy list by incrementing the reference counter. */
      [](void *list) -> void * {
        List *list_ = static_cast<List *>(list);
        list_->incref();
        return static_cast<void *>(list);
      },
      /* Free list by decrementing the reference counter. */
      [](void *list) {
        List *list_ = static_cast<List *>(list);
        list_->decref();
      },
      /* Create a new empty list. */
      [base_type]() -> void * {
        List *list = new List(base_type);
        return static_cast<void *>(list);
      });
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
