#ifndef __FN_MULTI_FUNCTION_DATA_TYPE_H__
#define __FN_MULTI_FUNCTION_DATA_TYPE_H__

#include "FN_cpp_type.h"

namespace FN {

struct MFDataType {
 public:
  enum Category {
    None,
    Single,
    Vector,
  };

  MFDataType() = default;

  MFDataType(Category category, const CPPType &type) : m_category(category), m_base_type(&type)
  {
  }

  static MFDataType ForNone()
  {
    return MFDataType{};
  }

  template<typename T> static MFDataType ForSingle()
  {
    return MFDataType(Category::Single, CPP_TYPE<T>());
  }

  template<typename T> static MFDataType ForVector()
  {
    return MFDataType(Category::Vector, CPP_TYPE<T>());
  }

  bool is_none() const
  {
    return m_category == Category::None;
  }

  bool is_single() const
  {
    return m_category == Category::Single;
  }

  bool is_vector() const
  {
    return m_category == Category::Vector;
  }

  Category category() const
  {
    return m_category;
  }

  const CPPType &type() const
  {
    BLI_assert(m_category == Category::Single);
    return *m_base_type;
  }

  const CPPType &base_type() const
  {
    BLI_assert(m_category == Category::Vector);
    return *m_base_type;
  }

  friend bool operator==(MFDataType a, MFDataType b)
  {
    return a.m_category == b.m_category && a.m_base_type == b.m_base_type;
  }

  friend bool operator!=(MFDataType a, MFDataType b)
  {
    return !(a == b);
  }

 private:
  Category m_category = Category::None;
  const CPPType *m_base_type = nullptr;
};

}  // namespace FN

#endif /* __FN_MULTI_FUNCTION_DATA_TYPE_H__ */
