#pragma once

#include "FN_multi_function.h"

namespace FN {

template<typename T> class MF_ConstantValue : public MultiFunction {
 private:
  T m_value;

 public:
  MF_ConstantValue(T value) : m_value(std::move(value))
  {
    MFSignatureBuilder signature = this->get_builder("Constant " + CPP_TYPE<T>().name());
    std::string name = output_name_from_value(m_value);
    signature.single_output<T>(name);
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    MutableArrayRef<T> output = params.uninitialized_single_output<T>(0);

    mask.foreach_index([&](uint i) { new (output.begin() + i) T(m_value); });
  }

 private:
  static std::string output_name_from_value(const T &UNUSED(value))
  {
    return "Value";
  }
};

template<> inline std::string MF_ConstantValue<float>::output_name_from_value(const float &value)
{
  return std::to_string(value);
}

template<> inline std::string MF_ConstantValue<int>::output_name_from_value(const int &value)
{
  return std::to_string(value);
}

template<> inline std::string MF_ConstantValue<bool>::output_name_from_value(const bool &value)
{
  return (value) ? "true" : "false";
}

template<>
inline std::string MF_ConstantValue<std::string>::output_name_from_value(const std::string &value)
{
  return "\"" + value + "\"";
}

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
