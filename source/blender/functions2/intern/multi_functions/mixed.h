#pragma once

#include "FN_multi_function.h"

namespace FN {

class MultiFunction_AddFloats final : public MultiFunction {
 public:
  MultiFunction_AddFloats();
  void call(const MFMask &mask, MFParams &params, MFContext &context) const override;
};

class MultiFunction_AddFloat3s final : public MultiFunction {
 public:
  MultiFunction_AddFloat3s();
  void call(const MFMask &mask, MFParams &params, MFContext &context) const override;
};

class MultiFunction_CombineVector final : public MultiFunction {
 public:
  MultiFunction_CombineVector();
  void call(const MFMask &mask, MFParams &params, MFContext &context) const override;
};

class MultiFunction_SeparateVector final : public MultiFunction {
 public:
  MultiFunction_SeparateVector();
  void call(const MFMask &mask, MFParams &params, MFContext &context) const override;
};

class MultiFunction_VectorDistance final : public MultiFunction {
 public:
  MultiFunction_VectorDistance();
  void call(const MFMask &mask, MFParams &params, MFContext &context) const override;
};

class MultiFunction_FloatArraySum final : public MultiFunction {
 public:
  MultiFunction_FloatArraySum();
  void call(const MFMask &mask, MFParams &params, MFContext &context) const override;
};

class MultiFunction_FloatRange final : public MultiFunction {
 public:
  MultiFunction_FloatRange();
  void call(const MFMask &mask, MFParams &params, MFContext &context) const override;
};

class MultiFunction_ObjectWorldLocation final : public MultiFunction {
 public:
  MultiFunction_ObjectWorldLocation();
  void call(const MFMask &mask, MFParams &params, MFContext &context) const override;
};

class MultiFunction_TextLength final : public MultiFunction {
 public:
  MultiFunction_TextLength();
  void call(const MFMask &mask, MFParams &params, MFContext &context) const override;
};

template<typename T> class MultiFunction_ConstantValue : public MultiFunction {
 private:
  T m_value;

 public:
  MultiFunction_ConstantValue(T value) : m_value(std::move(value))
  {
    MFSignatureBuilder signature("Constant " + GET_TYPE<T>().name());
    signature.single_output<T>("Output");
    this->set_signature(signature);
  }

  void call(const MFMask &mask, MFParams &params, MFContext &UNUSED(context)) const override
  {
    MutableArrayRef<T> output = params.single_output<T>(0, "Output");

    mask.foreach_index([&](uint i) { new (output.begin() + i) T(m_value); });
  }
};

template<typename FromT, typename ToT> class MultiFunction_Convert : public MultiFunction {
 public:
  MultiFunction_Convert()
  {
    MFSignatureBuilder signature(GET_TYPE<FromT>().name() + " to " + GET_TYPE<ToT>().name());
    signature.readonly_single_input<FromT>("Input");
    signature.single_output<ToT>("Output");
    this->set_signature(signature);
  }

  void call(const MFMask &mask, MFParams &params, MFContext &UNUSED(context)) const override
  {
    VirtualListRef<FromT> inputs = params.readonly_single_input<FromT>(0, "Input");
    MutableArrayRef<ToT> outputs = params.single_output<ToT>(1, "Output");

    for (uint i : mask.indices()) {
      const FromT &from_value = inputs[i];
      new (outputs.begin() + i) ToT(from_value);
    }
  }
};

class MultiFunction_SimpleVectorize final : public MultiFunction {
 private:
  const MultiFunction &m_function;
  Vector<bool> m_input_is_vectorized;
  Vector<uint> m_vectorized_inputs;
  Vector<uint> m_output_indices;

 public:
  MultiFunction_SimpleVectorize(const MultiFunction &function, ArrayRef<bool> input_is_vectorized);
  void call(const MFMask &mask, MFParams &params, MFContext &context) const override;
};

}  // namespace FN
