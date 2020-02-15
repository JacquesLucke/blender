#pragma once

#include <sstream>

#include "FN_multi_function.h"

namespace FN {

template<typename FromT, typename ToT> class MF_Convert : public MultiFunction {
 public:
  MF_Convert()
  {
    MFSignatureBuilder signature = this->get_builder(CPP_TYPE<FromT>().name() + " to " +
                                                     CPP_TYPE<ToT>().name());
    signature.single_input<FromT>("Input");
    signature.single_output<ToT>("Output");
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    VirtualListRef<FromT> inputs = params.readonly_single_input<FromT>(0, "Input");
    MutableArrayRef<ToT> outputs = params.uninitialized_single_output<ToT>(1, "Output");

    for (uint i : mask.indices()) {
      const FromT &from_value = inputs[i];
      new (outputs.begin() + i) ToT(from_value);
    }
  }
};

template<typename InT, typename OutT> class MF_Custom_In1_Out1 final : public MultiFunction {
 private:
  using FunctionT =
      std::function<void(IndexMask mask, VirtualListRef<InT>, MutableArrayRef<OutT>)>;
  FunctionT m_fn;

 public:
  MF_Custom_In1_Out1(StringRef name, FunctionT fn) : m_fn(std::move(fn))
  {
    MFSignatureBuilder signature = this->get_builder(name);
    signature.single_input<InT>("Input");
    signature.single_output<OutT>("Output");
  }

  template<typename ElementFuncT>
  MF_Custom_In1_Out1(StringRef name, ElementFuncT element_fn)
      : MF_Custom_In1_Out1(name, MF_Custom_In1_Out1::create_function(element_fn))
  {
  }

  template<typename ElementFuncT> static FunctionT create_function(ElementFuncT element_fn)
  {
    return [=](IndexMask mask, VirtualListRef<InT> inputs, MutableArrayRef<OutT> outputs) {
      if (inputs.is_non_single_full_array()) {
        ArrayRef<InT> in_array = inputs.as_full_array();
        mask.foreach_index([=](uint i) { outputs[i] = element_fn(in_array[i]); });
      }
      else if (inputs.is_single_element()) {
        InT in_single = inputs.as_single_element();
        outputs.fill_indices(mask.indices(), element_fn(in_single));
      }
      else {
        mask.foreach_index([=](uint i) { outputs[i] = element_fn(inputs[i]); });
      }
    };
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    VirtualListRef<InT> inputs = params.readonly_single_input<InT>(0);
    MutableArrayRef<OutT> outputs = params.uninitialized_single_output<OutT>(1);
    m_fn(mask, inputs, outputs);
  }
};

template<typename InT1, typename InT2, typename OutT>
class MF_Custom_In2_Out1 final : public MultiFunction {
 private:
  using FunctionT = std::function<void(
      IndexMask mask, VirtualListRef<InT1>, VirtualListRef<InT2>, MutableArrayRef<OutT>)>;

  FunctionT m_fn;

 public:
  MF_Custom_In2_Out1(StringRef name, FunctionT fn) : m_fn(std::move(fn))
  {
    MFSignatureBuilder signature = this->get_builder(name);
    signature.single_input<InT1>("Input 1");
    signature.single_input<InT2>("Input 2");
    signature.single_output<OutT>("Output");
  }

  template<typename ElementFuncT>
  MF_Custom_In2_Out1(StringRef name, ElementFuncT element_fn)
      : MF_Custom_In2_Out1(name, MF_Custom_In2_Out1::create_function(element_fn))
  {
  }

  template<typename ElementFuncT> static FunctionT create_function(ElementFuncT element_fn)
  {
    return [=](IndexMask mask,
               VirtualListRef<InT1> inputs1,
               VirtualListRef<InT2> inputs2,
               MutableArrayRef<OutT> outputs) -> void {
      if (inputs1.is_non_single_full_array() && inputs2.is_non_single_full_array()) {
        ArrayRef<InT1> in1_array = inputs1.as_full_array();
        ArrayRef<InT2> in2_array = inputs2.as_full_array();
        mask.foreach_index(
            [=](uint i) { new (&outputs[i]) OutT(element_fn(in1_array[i], in2_array[i])); });
      }
      else if (inputs1.is_non_single_full_array() && inputs2.is_single_element()) {
        ArrayRef<InT1> in1_array = inputs1.as_full_array();
        InT2 in2_single = inputs2.as_single_element();
        mask.foreach_index(
            [=](uint i) { new (&outputs[i]) OutT(element_fn(in1_array[i], in2_single)); });
      }
      else if (inputs1.is_single_element() && inputs2.is_non_single_full_array()) {
        InT1 in1_single = inputs1.as_single_element();
        ArrayRef<InT2> in2_array = inputs2.as_full_array();
        mask.foreach_index(
            [=](uint i) { new (&outputs[i]) OutT(element_fn(in1_single, in2_array[i])); });
      }
      else if (inputs1.is_single_element() && inputs2.is_single_element()) {
        InT1 in1_single = inputs1.as_single_element();
        InT2 in2_single = inputs2.as_single_element();
        OutT out_single = element_fn(in1_single, in2_single);
        outputs.fill_indices(mask.indices(), out_single);
      }
      else {
        mask.foreach_index(
            [=](uint i) { new (&outputs[i]) OutT(element_fn(inputs1[i], inputs2[i])); });
      }
    };
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    VirtualListRef<InT1> inputs1 = params.readonly_single_input<InT1>(0);
    VirtualListRef<InT2> inputs2 = params.readonly_single_input<InT2>(1);
    MutableArrayRef<OutT> outputs = params.uninitialized_single_output<OutT>(2);
    m_fn(mask, inputs1, inputs2, outputs);
  }
};

template<typename T> class MF_VariadicMath final : public MultiFunction {
 private:
  using FunctionT = std::function<void(
      IndexMask mask, VirtualListRef<T>, VirtualListRef<T>, MutableArrayRef<T>)>;

  uint m_input_amount;
  FunctionT m_fn;

 public:
  MF_VariadicMath(StringRef name, uint input_amount, FunctionT fn)
      : m_input_amount(input_amount), m_fn(fn)
  {
    BLI_STATIC_ASSERT(std::is_trivial<T>::value, "");
    BLI_assert(input_amount >= 1);
    MFSignatureBuilder signature = this->get_builder(name);
    for (uint i = 0; i < m_input_amount; i++) {
      signature.single_input<T>("Input");
    }
    signature.single_output<T>("Output");
  }

  template<typename ElementFuncT>
  MF_VariadicMath(StringRef name, uint input_amount, ElementFuncT element_func)
      : MF_VariadicMath(
            name, input_amount, MF_Custom_In2_Out1<T, T, T>::create_function(element_func))
  {
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    MutableArrayRef<T> outputs = params.uninitialized_single_output<T>(m_input_amount, "Output");

    if (m_input_amount == 1) {
      VirtualListRef<T> inputs = params.readonly_single_input<T>(0, "Input");
      for (uint i : mask.indices()) {
        outputs[i] = inputs[i];
      }
    }
    else {
      BLI_assert(m_input_amount >= 2);
      VirtualListRef<T> inputs0 = params.readonly_single_input<T>(0, "Input");
      VirtualListRef<T> inputs1 = params.readonly_single_input<T>(1, "Input");
      m_fn(mask, inputs0, inputs1, outputs);

      for (uint param_index = 2; param_index < m_input_amount; param_index++) {
        VirtualListRef<T> inputs = params.readonly_single_input<T>(param_index, "Input");
        m_fn(mask, VirtualListRef<T>::FromFullArray(outputs), inputs, outputs);
      }
    }
  }
};

}  // namespace FN
