#pragma once

#include "FN_multi_function.h"

#include <sstream>

namespace FN {

/**
 * Note: The value buffer passed into the constructor should have a longer lifetime than the
 * function itself.
 */
class MF_GenericConstantValue : public MultiFunction {
 private:
  const void *m_value;

 public:
  MF_GenericConstantValue(const CPPType &type, const void *value) : m_value(value)
  {
    MFSignatureBuilder signature = this->get_builder("Constant " + type.name());
    std::stringstream ss;
    MF_GenericConstantValue::value_to_string(ss, type, value);
    signature.single_output(ss.str(), type);
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    GenericMutableArrayRef r_value = params.uninitialized_single_output(0);
    r_value.type().fill_uninitialized_indices(m_value, r_value.buffer(), mask);
  }

  static void value_to_string(std::stringstream &ss, const CPPType &type, const void *value);
};

template<typename T> class MF_ConstantValue : public MultiFunction {
 private:
  T m_value;

 public:
  MF_ConstantValue(T value) : m_value(std::move(value))
  {
    MFSignatureBuilder signature = this->get_builder("Constant " + CPP_TYPE<T>().name());
    std::stringstream ss;
    MF_GenericConstantValue::value_to_string(ss, CPP_TYPE<T>(), (const void *)&m_value);
    signature.single_output<T>(ss.str());
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    MutableArrayRef<T> output = params.uninitialized_single_output<T>(0);

    mask.foreach_index([&](uint i) { new (output.begin() + i) T(m_value); });
  }
};

}  // namespace FN
