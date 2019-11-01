#pragma once

#include "FN_multi_function.h"

namespace FN {

class MF_GetListElement final : public MultiFunction {
 private:
  const CPPType &m_base_type;

 public:
  MF_GetListElement(const CPPType &base_type);
  void call(const MFMask &mask, MFParams &params, MFContext &context) const override;
};

class MF_ListLength final : public MultiFunction {
 private:
  const CPPType &m_base_type;

 public:
  MF_ListLength(const CPPType &base_type);
  void call(const MFMask &mask, MFParams &params, MFContext &context) const override;
};

class MF_PackList final : public MultiFunction {
 private:
  const CPPType &m_base_type;
  Vector<bool> m_input_list_status;

 public:
  MF_PackList(const CPPType &base_type, ArrayRef<bool> input_list_status);
  void call(const MFMask &mask, MFParams &params, MFContext &context) const override;

 private:
  bool input_is_list(uint index) const;
};

template<typename T> class MF_EmptyList : public MultiFunction {
 public:
  MF_EmptyList()
  {
    MFSignatureBuilder signature("Empty List - " + GET_TYPE<T>().name());
    signature.vector_output<T>("Output");
    this->set_signature(signature);
  }

  void call(const MFMask &UNUSED(mask),
            MFParams &UNUSED(params),
            MFContext &UNUSED(context)) const override
  {
  }
};

template<typename FromT, typename ToT> class MF_ConvertList : public MultiFunction {
 public:
  MF_ConvertList()
  {
    MFSignatureBuilder signature(GET_TYPE<FromT>().name() + " List to " + GET_TYPE<ToT>().name() +
                                 " List");
    signature.readonly_vector_input<FromT>("Inputs");
    signature.vector_output<ToT>("Outputs");
    this->set_signature(signature);
  }

  void call(const MFMask &mask, MFParams &params, MFContext &UNUSED(context)) const override
  {
    VirtualListListRef<FromT> inputs = params.readonly_vector_input<FromT>(0, "Inputs");
    GenericVectorArray::MutableTypedRef<ToT> outputs = params.vector_output<ToT>(1, "Outputs");

    for (uint index : mask.indices()) {
      VirtualListRef<FromT> input_list = inputs[index];

      for (uint i = 0; i < input_list.size(); i++) {
        const FromT &from_value = input_list[i];
        ToT to_value = static_cast<ToT>(from_value);
        outputs.append_single(index, to_value);
      }
    }
  }
};

template<typename T> class MF_SingleElementList : public MultiFunction {
 public:
  MF_SingleElementList()
  {
    MFSignatureBuilder signature("Single Element List - " + GET_TYPE<T>().name());
    signature.readonly_single_input<T>("Input");
    signature.vector_output<T>("Outputs");
    this->set_signature(signature);
  }

  void call(const MFMask &mask, MFParams &params, MFContext &UNUSED(context)) const override
  {
    VirtualListRef<T> inputs = params.readonly_single_input<T>(0, "Input");
    GenericVectorArray::MutableTypedRef<T> outputs = params.vector_output<T>(1, "Outputs");

    for (uint i : mask.indices()) {
      outputs.append_single(i, inputs[i]);
    }
  }
};

};  // namespace FN
