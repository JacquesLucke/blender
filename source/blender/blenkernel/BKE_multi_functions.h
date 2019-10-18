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

class MultiFunction_AppendToList final : public MultiFunction {
 private:
  const CPPType &m_base_type;

 public:
  MultiFunction_AppendToList(const CPPType &base_type);
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

class MultiFunction_CombineLists final : public MultiFunction {
 private:
  const CPPType &m_base_type;

 public:
  MultiFunction_CombineLists(const CPPType &base_type);
  void call(ArrayRef<uint> mask_indices, MFParams &params, MFContext &context) const override;
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

};  // namespace BKE

#endif /* __BKE_MULTI_FUNCTIONS_H__ */
