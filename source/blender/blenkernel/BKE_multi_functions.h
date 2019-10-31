#ifndef __BKE_MULTI_FUNCTIONS_H__
#define __BKE_MULTI_FUNCTIONS_H__

#include "BKE_multi_function.h"

namespace BKE {

class MultiFunction_AddFloats final : public MultiFunction {
 public:
  MultiFunction_AddFloats();
  void call(ArrayRef<uint> mask_indices, MFParams &params, MFContext &context) const override;
};

class MultiFunction_AddFloat3s final : public MultiFunction {
 public:
  MultiFunction_AddFloat3s();
  void call(ArrayRef<uint> mask_indices, MFParams &params, MFContext &context) const override;
};

class MultiFunction_CombineVector final : public MultiFunction {
 public:
  MultiFunction_CombineVector();
  void call(ArrayRef<uint> mask_indices, MFParams &params, MFContext &context) const override;
};

class MultiFunction_SeparateVector final : public MultiFunction {
 public:
  MultiFunction_SeparateVector();
  void call(ArrayRef<uint> mask_indices, MFParams &params, MFContext &context) const override;
};

class MultiFunction_VectorDistance final : public MultiFunction {
 public:
  MultiFunction_VectorDistance();
  void call(ArrayRef<uint> mask_indices, MFParams &params, MFContext &context) const override;
};

class MultiFunction_FloatArraySum final : public MultiFunction {
 public:
  MultiFunction_FloatArraySum();
  void call(ArrayRef<uint> mask_indices, MFParams &params, MFContext &context) const override;
};

class MultiFunction_FloatRange final : public MultiFunction {
 public:
  MultiFunction_FloatRange();
  void call(ArrayRef<uint> mask_indices, MFParams &params, MFContext &context) const override;
};

class MultiFunction_GetListElement final : public MultiFunction {
 private:
  const CPPType &m_base_type;

 public:
  MultiFunction_GetListElement(const CPPType &base_type);
  void call(ArrayRef<uint> mask_indices, MFParams &params, MFContext &context) const override;
};

class MultiFunction_ListLength final : public MultiFunction {
 private:
  const CPPType &m_base_type;

 public:
  MultiFunction_ListLength(const CPPType &base_type);
  void call(ArrayRef<uint> mask_indices, MFParams &params, MFContext &context) const override;
};

class MultiFunction_PackList final : public MultiFunction {
 private:
  const CPPType &m_base_type;
  Vector<bool> m_input_list_status;

 public:
  MultiFunction_PackList(const CPPType &base_type, ArrayRef<bool> input_list_status);
  void call(ArrayRef<uint> mask_indices, MFParams &params, MFContext &context) const override;

 private:
  bool input_is_list(uint index) const;
};

template<typename T> class MultiFunction_ConstantValue : public MultiFunction {
 private:
  T m_value;

 public:
  MultiFunction_ConstantValue(T value) : m_value(std::move(value))
  {
    MFSignatureBuilder signature;
    signature.single_output<T>("Output");
    this->set_signature(signature);
  }

  void call(ArrayRef<uint> mask_indices,
            MFParams &params,
            MFContext &UNUSED(context)) const override
  {
    MutableArrayRef<T> output = params.single_output<T>(0, "Output");

    for (uint i : mask_indices) {
      new (output.begin() + i) T(m_value);
    }
  }
};

template<typename T> class MultiFunction_EmptyList : public MultiFunction {
 public:
  MultiFunction_EmptyList()
  {
    MFSignatureBuilder signature;
    signature.vector_output<T>("Output");
    this->set_signature(signature);
  }

  void call(ArrayRef<uint> UNUSED(mask_indices),
            MFParams &UNUSED(params),
            MFContext &UNUSED(context)) const override
  {
  }
};

template<typename FromT, typename ToT> class MultiFunction_Convert : public MultiFunction {
 public:
  MultiFunction_Convert()
  {
    MFSignatureBuilder signature;
    signature.readonly_single_input<FromT>("Input");
    signature.single_output<ToT>("Output");
    this->set_signature(signature);
  }

  void call(ArrayRef<uint> mask_indices,
            MFParams &params,
            MFContext &UNUSED(context)) const override
  {
    VirtualListRef<FromT> inputs = params.readonly_single_input<FromT>(0, "Input");
    MutableArrayRef<ToT> outputs = params.single_output<ToT>(1, "Output");

    for (uint i : mask_indices) {
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
  void call(ArrayRef<uint> mask_indices, MFParams &params, MFContext &context) const override;
};

};  // namespace BKE

#endif /* __BKE_MULTI_FUNCTIONS_H__ */
