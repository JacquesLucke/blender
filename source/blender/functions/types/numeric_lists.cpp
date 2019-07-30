#include "numeric_lists.hpp"
#include "BLI_lazy_init.hpp"

#include "FN_tuple.hpp"
#include "FN_llvm.hpp"

namespace FN {
namespace Types {

template<typename T> static void *copy_func(void *value)
{
  List<T> *list = (List<T> *)value;
  list->new_user();
  return list;
}

template<typename T> static void free_func(void *value)
{
  List<T> *list = (List<T> *)value;
  list->remove_user();
}

template<typename T> static void *default_func()
{
  return new List<T>();
}

template<typename T> SharedType create_list_type(std::string name)
{
  BLI_STATIC_ASSERT(sizeof(SharedList<T>) == sizeof(List<T> *),
                    "Currently it is assumed that only a pointer to the list is stored");

  SharedType type = SharedType::New(name);
  type->add_extension<CPPTypeInfoForType<SharedList<T>>>();
  type->add_extension<PointerLLVMTypeInfo>(copy_func<T>, free_func<T>, default_func<T>);
  return type;
}

BLI_LAZY_INIT(SharedType, GET_TYPE_float_list)
{
  return create_list_type<float>("Float List");
}

BLI_LAZY_INIT(SharedType, GET_TYPE_float3_list)
{
  return create_list_type<float3>("Float3 List");
}

BLI_LAZY_INIT(SharedType, GET_TYPE_int32_list)
{
  return create_list_type<int32_t>("Int32 List");
}

BLI_LAZY_INIT(SharedType, GET_TYPE_bool_list)
{
  return create_list_type<bool>("Bool List");
}

BLI_LAZY_INIT(SharedType, GET_TYPE_object_list)
{
  return create_list_type<Object *>("Object List");
}

BLI_LAZY_INIT(SharedType, GET_TYPE_rgba_f_list)
{
  return create_list_type<rgba_f>("RGBA Float List");
}

}  // namespace Types
}  // namespace FN
