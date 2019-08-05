#include "FN_tuple.hpp"
#include "FN_llvm.hpp"

namespace FN {
namespace Types {

class ListTypeInfo : CPPTypeInfoForType<List> {
 private:
  SharedType m_base_type;

 public:
  ListTypeInfo(SharedType base_type) : m_base_type(std::move(base_type))
  {
  }

  void construct_default(void *ptr) const override
  {
    new (ptr) List(m_base_type);
  }

  void construct_default_n(void *ptr, uint n) const override
  {
    for (uint i = 0; i < n; i++) {
      new ((List *)ptr + i) List(m_base_type);
    }
  }
};

SharedType new_list_type(SharedType &base_type)
{
  SharedType type = SharedType::New(base_type->name() + " List");
  type->add_extension<CPPTypeInfoForType<List>>();
  return type;
}

}  // namespace Types
}  // namespace FN
