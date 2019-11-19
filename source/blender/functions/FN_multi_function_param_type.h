#ifndef __FN_MULTI_FUNCTION_PARAM_TYPE_H__
#define __FN_MULTI_FUNCTION_PARAM_TYPE_H__

#include "FN_multi_function_data_type.h"

namespace FN {

struct MFParamType {
 public:
  enum InterfaceType {
    Input,
    Output,
    Mutable,
  };

  enum Type {
    SingleInput,
    VectorInput,
    SingleOutput,
    VectorOutput,
    MutableSingle,
    MutableVector,
  };

  MFParamType(InterfaceType interface_type, MFDataType data_type)
      : m_interface_type(interface_type), m_data_type(data_type)
  {
  }

  static MFParamType ForSingleInput(const CPPType &type)
  {
    return MFParamType(InterfaceType::Input, MFDataType::ForSingle(type));
  }

  static MFParamType ForVectorInput(const CPPType &base_type)
  {
    return MFParamType(InterfaceType::Input, MFDataType::ForVector(base_type));
  }

  static MFParamType ForSingleOutput(const CPPType &type)
  {
    return MFParamType(InterfaceType::Output, MFDataType::ForSingle(type));
  }

  static MFParamType ForVectorOutput(const CPPType &base_type)
  {
    return MFParamType(InterfaceType::Output, MFDataType::ForVector(base_type));
  }

  static MFParamType ForSingleMutable(const CPPType &type)
  {
    return MFParamType(InterfaceType::Mutable, MFDataType::ForSingle(type));
  }

  static MFParamType ForVectorMutable(const CPPType &base_type)
  {
    return MFParamType(InterfaceType::Mutable, MFDataType::ForVector(base_type));
  }

  bool is_input() const
  {
    return m_interface_type == Input;
  }

  bool is_output() const
  {
    return m_interface_type == Output;
  }

  bool is_mutable() const
  {
    return m_interface_type == Mutable;
  }

  bool is_single_input() const
  {
    return m_interface_type == Input && m_data_type.is_single();
  }

  bool is_vector_input() const
  {
    return m_interface_type == Input && m_data_type.is_vector();
  }

  bool is_mutable_single() const
  {
    return m_interface_type == Mutable && m_data_type.is_single();
  }

  bool is_mutable_vector() const
  {
    return m_interface_type == Mutable && m_data_type.is_vector();
  }

  bool is_single_output() const
  {
    return m_interface_type == Output && m_data_type.is_single();
  }

  bool is_input_or_mutable() const
  {
    return ELEM(m_interface_type, Input, Mutable);
  }

  bool is_output_or_mutable() const
  {
    return ELEM(m_interface_type, Output, Mutable);
  }

  bool is_vector_output() const
  {
    return m_interface_type == Output && m_data_type.is_vector();
  }

  Type type() const
  {
    if (m_data_type.is_single()) {
      switch (m_interface_type) {
        case InterfaceType::Input:
          return SingleInput;
        case InterfaceType::Output:
          return SingleOutput;
        case InterfaceType::Mutable:
          return MutableSingle;
      }
    }
    else if (m_data_type.is_vector()) {
      switch (m_interface_type) {
        case InterfaceType::Input:
          return VectorInput;
        case InterfaceType::Output:
          return VectorOutput;
        case InterfaceType::Mutable:
          return MutableVector;
      }
    }
    BLI_assert(false);
    return Type::MutableSingle;
  }

  MFDataType data_type() const
  {
    return m_data_type;
  }

  InterfaceType interface_type() const
  {
    return m_interface_type;
  }

  friend bool operator==(MFParamType a, MFParamType b)
  {
    return a.m_interface_type == b.m_interface_type && a.m_data_type == b.m_data_type;
  }

 private:
  InterfaceType m_interface_type;
  MFDataType m_data_type;
};

}  // namespace FN

#endif /* __FN_MULTI_FUNCTION_PARAM_TYPE_H__ */
