#ifndef __FN_MULTI_FUNCTION_PARAM_TYPE_H__
#define __FN_MULTI_FUNCTION_PARAM_TYPE_H__

#include "FN_multi_function_data_type.h"

namespace FN {

struct MFParamType {
 public:
  enum Category {
    None,
    ReadonlySingleInput,
    SingleOutput,
    ReadonlyVectorInput,
    VectorOutput,
    MutableVector,
  };

  MFParamType(Category category, const CPPType *base_type = nullptr)
      : m_category(category), m_base_type(base_type)
  {
  }

  bool is_none() const
  {
    return m_category == MFParamType::None;
  }

  bool is_readonly_single_input() const
  {
    return m_category == ReadonlySingleInput;
  }

  bool is_readonly_vector_input() const
  {
    return m_category == ReadonlyVectorInput;
  }

  bool is_mutable_vector() const
  {
    return m_category == MutableVector;
  }

  bool is_single_output() const
  {
    return m_category == SingleOutput;
  }

  bool is_input_or_mutable() const
  {
    return ELEM(m_category, ReadonlySingleInput, ReadonlyVectorInput, MutableVector);
  }

  bool is_output_or_mutable() const
  {
    return ELEM(m_category, SingleOutput, VectorOutput, MutableVector);
  }

  bool is_vector_output() const
  {
    return m_category == VectorOutput;
  }

  MFDataType as_data_type() const
  {
    switch (m_category) {
      case None:
        return {};
      case ReadonlySingleInput:
      case SingleOutput:
        return {MFDataType::Single, *m_base_type};
      case ReadonlyVectorInput:
      case VectorOutput:
      case MutableVector:
        return {MFDataType::Vector, *m_base_type};
    }
    BLI_assert(false);
    return {};
  }

  Category category() const
  {
    return m_category;
  }

  const CPPType &type() const
  {
    BLI_assert(ELEM(m_category, Category::ReadonlySingleInput, Category::SingleOutput));
    return *m_base_type;
  }

  const CPPType &base_type() const
  {
    BLI_assert(ELEM(m_category,
                    Category::ReadonlyVectorInput,
                    Category::VectorOutput,
                    Category::MutableVector));
    return *m_base_type;
  }

 private:
  Category m_category = Category::None;
  const CPPType *m_base_type = nullptr;
};

}  // namespace FN

#endif /* __FN_MULTI_FUNCTION_PARAM_TYPE_H__ */
