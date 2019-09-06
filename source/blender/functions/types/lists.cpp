#include "BLI_lazy_init.hpp"

#include "FN_cpp.hpp"
#include "FN_llvm.hpp"
#include "FN_types.hpp"

namespace FN {
namespace Types {

class ListTypeInfo : public CPPTypeInfoForType<SharedList> {
 private:
  Type *m_base_type;

 public:
  ListTypeInfo(Type *base_type) : m_base_type(std::move(base_type))
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

Type *new_list_type(Type *base_type)
{
  BLI_STATIC_ASSERT(sizeof(SharedList) == sizeof(List *), "");
  Type *type = new Type(base_type->name() + " List");
  type->add_extension<ListTypeInfo>(base_type);
  type->add_extension<SharedImmutablePointerLLVMTypeInfo<List>>();
  return type;
}

}  // namespace Types
}  // namespace FN
