#ifndef __FN_MULTI_FUNCTION_DATA_TYPE_H__
#define __FN_MULTI_FUNCTION_DATA_TYPE_H__

#include "FN_cpp_type.h"

#include "BLI_hash_cxx.h"

namespace FN {

struct MFDataType {
 public:
  enum Category {
    Single,
    Vector,
  };

 private:
  MFDataType(Category category, const CPPType &type) : m_category(category), m_base_type(&type)
  {
  }

 public:
  MFDataType() = default;

  template<typename T> static MFDataType ForSingle()
  {
    return MFDataType::ForSingle(CPP_TYPE<T>());
  }

  template<typename T> static MFDataType ForVector()
  {
    return MFDataType::ForVector(CPP_TYPE<T>());
  }

  static MFDataType ForSingle(const CPPType &type)
  {
    return MFDataType(Category::Single, type);
  }

  static MFDataType ForVector(const CPPType &type)
  {
    return MFDataType(Category::Vector, type);
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

  const CPPType &single__cpp_type() const
  {
    BLI_assert(m_category == Category::Single);
    return *m_base_type;
  }

  const CPPType &vector__cpp_base_type() const
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

  std::string to_string() const
  {
    switch (m_category) {
      case Single:
        return m_base_type->name();
      case Vector:
        return m_base_type->name() + " Vector";
    }
    BLI_assert(false);
    return "";
  }

  friend std::ostream &operator<<(std::ostream &stream, MFDataType type)
  {
    stream << type.to_string();
    return stream;
  }

 private:
  Category m_category;
  const CPPType *m_base_type;

  friend BLI::DefaultHash<MFDataType>;
};

}  // namespace FN

namespace BLI {
template<> struct DefaultHash<FN::MFDataType> {
  uint32_t operator()(const FN::MFDataType &value) const
  {
    return DefaultHash<FN::CPPType *>{}(value.m_base_type) + 243523 * (uint)value.m_category;
  }
};

}  // namespace BLI

#endif /* __FN_MULTI_FUNCTION_DATA_TYPE_H__ */
