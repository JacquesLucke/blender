#pragma once

#include "FN_multi_function.h"

#include <sstream>

namespace FN {

/**
 * The value buffer passed into the constructor should have a longer lifetime than the
 * function itself.
 */
class MF_GenericConstantValue : public MultiFunction {
 private:
  const void *m_value;

 public:
  MF_GenericConstantValue(const CPPType &type, const void *value);
  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override;

  static void value_to_string(std::stringstream &ss, const CPPType &type, const void *value);
};

/**
 * The passed in buffer has to have a longer lifetime than the function itself.
 */
class MF_GenericConstantVector : public MultiFunction {
 private:
  GenericArrayRef m_array;

 public:
  MF_GenericConstantVector(GenericArrayRef array);
  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override;
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
