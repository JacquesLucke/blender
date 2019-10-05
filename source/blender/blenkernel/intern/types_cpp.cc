#include "BKE_types_cpp.h"
#include "BKE_generic_array_ref.h"

#include "DNA_object_types.h"

#include "BLI_math_cxx.h"
#include "BLI_vector.h"

namespace BKE {

using BLI::Vector;

static Vector<TypeCPP *, 4, BLI::RawAllocator> allocated_types;

void free_data_types()
{
  for (TypeCPP *type : allocated_types) {
    delete type;
  }
}

template<typename T> void ConstructDefault_CB(const TypeCPP *UNUSED(self), void *ptr)
{
  BLI::construct_default((T *)ptr);
}

template<typename T, bool IsDefaultConstructible> struct DefaultConstructor;
template<typename T> struct DefaultConstructor<T, true> {
  static TypeCPP::ConstructDefaultF get_callback()
  {
    return ConstructDefault_CB<T>;
  }
};
template<typename T> struct DefaultConstructor<T, false> {
  static TypeCPP::ConstructDefaultF get_callback()
  {
    return nullptr;
  }
};

template<typename T> void Destruct_CB(void *ptr)
{
  BLI::destruct((T *)ptr);
}
template<typename T> void CopyToInitialized_CB(void *src, void *dst)
{
  *(T *)src = *(T *)dst;
}
template<typename T> void CopyToUninitialized_CB(void *src, void *dst)
{
  BLI::uninitialized_copy_n((T *)src, 1, (T *)dst);
}
template<typename T> void RelocateToInitialized_CB(void *src, void *dst)
{
  BLI::relocate((T *)src, (T *)dst);
}
template<typename T> void RelocateToUninitialized_CB(void *src, void *dst)
{
  BLI::uninitialized_relocate((T *)src, (T *)dst);
}

#define CPP_TYPE_DECLARE(IDENTIFIER) static TypeCPP *TYPE_##IDENTIFIER = nullptr

CPP_TYPE_DECLARE(float);
CPP_TYPE_DECLARE(bool);
CPP_TYPE_DECLARE(ObjectPtr);
CPP_TYPE_DECLARE(int32);
CPP_TYPE_DECLARE(rgba_f);
CPP_TYPE_DECLARE(string);
CPP_TYPE_DECLARE(GenericArrayRef);

#undef CPP_TYPE_DECLARE

void init_data_types()
{

#define CPP_TYPE_CONSTRUCTION(IDENTIFIER, TYPE_NAME) \
  TYPE_##IDENTIFIER = new TypeCPP( \
      STRINGIFY(IDENTIFIER), \
      sizeof(TYPE_NAME), \
      alignof(TYPE_NAME), \
      std::is_trivially_destructible<TYPE_NAME>::value, \
      DefaultConstructor<TYPE_NAME, \
                         std::is_default_constructible<TYPE_NAME>::value>::get_callback(), \
      Destruct_CB<TYPE_NAME>, \
      CopyToInitialized_CB<TYPE_NAME>, \
      CopyToUninitialized_CB<TYPE_NAME>, \
      RelocateToInitialized_CB<TYPE_NAME>, \
      RelocateToUninitialized_CB<TYPE_NAME>, \
      nullptr); \
  allocated_types.append(TYPE_##IDENTIFIER)

  CPP_TYPE_CONSTRUCTION(float, float);
  CPP_TYPE_CONSTRUCTION(bool, bool);
  CPP_TYPE_CONSTRUCTION(ObjectPtr, Object *);
  CPP_TYPE_CONSTRUCTION(int32, int32_t);
  CPP_TYPE_CONSTRUCTION(rgba_f, BLI::rgba_f);
  CPP_TYPE_CONSTRUCTION(string, std::string);
  CPP_TYPE_CONSTRUCTION(GenericArrayRef, GenericArrayRef);

#undef CPP_TYPE_CONSTRUCTION
}

#define CPP_TYPE_GETTER(IDENTIFIER, TYPE_NAME) \
  template<> TypeCPP &get_type_cpp<TYPE_NAME>() \
  { \
    return *TYPE_##IDENTIFIER; \
  }

CPP_TYPE_GETTER(float, float)
CPP_TYPE_GETTER(bool, bool)
CPP_TYPE_GETTER(ObjectPtr, Object *)
CPP_TYPE_GETTER(int32, int32_t)
CPP_TYPE_GETTER(rgba_f, BLI::rgba_f)
CPP_TYPE_GETTER(string, std::string)
CPP_TYPE_GETTER(GenericArrayRef, GenericArrayRef);

#undef CPP_TYPE_GETTER

}  // namespace BKE