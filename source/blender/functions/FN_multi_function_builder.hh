/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup fn
 *
 * This file contains several utilities to create multi-functions with less redundant code.
 */

#include <functional>

#include "BLI_devirtualize_parameters_presets.hh"

#include "FN_multi_function.hh"

namespace blender::fn {

namespace devi = devirtualize_arrays;

template<typename... ParamTags> class CustomMF : public MultiFunction {
 private:
  std::function<void(IndexMask mask, MFParams params)> fn_;
  MFSignature signature_;

  using TagsSequence = TypeSequence<ParamTags...>;

 public:
  template<typename ElementFn, typename DeviFn = devi::presets::None>
  CustomMF(const char *name, ElementFn element_fn, DeviFn devi_fn = devi::presets::None())
  {
    UNUSED_VARS(element_fn, devi_fn);
    MFSignatureBuilder signature{name};
    add_signature_parameters(signature, std::make_index_sequence<TagsSequence::size()>());
    signature_ = signature.build();
    this->set_signature(&signature_);

    // fn_ = [element_fn, devi_fn](IndexMask mask, MFParams params) {
    //   execute(element_fn, devi_fn, mask, params,
    //   std::make_index_sequence<TagsSequence::size()>());
    // };
  }

  // template<typename ElementFn, typename DeviFn, size_t... I>
  // static void execute(ElementFn element_fn,
  //                     DeviFn devi_fn,
  //                     IndexMask mask,
  //                     MFParams params,
  //                     std::index_sequence<I...> /* indices */)
  // {
  //   std::tuple<typename ParamTags::ArrayType...> retrieved_params;
  //   (
  //       [&]() {
  //         using ParamTag = typename TagsSequence::template at_index<I>;
  //         using T = typename ParamTag::BaseType;

  //         if constexpr (std::is_base_of_v<devi::tags::Input, ParamTag>) {
  //           std::get<I>(retrieved_params) = params.readonly_single_input<T>(I);
  //         }
  //         if constexpr (std::is_base_of_v<devi::tags::Output, ParamTag>) {
  //           std::get<I>(retrieved_params) = params.uninitialized_single_output<T>(I);
  //         }
  //       }(),
  //       ...);

  //   devi::Devirtualizer devirtualizer =
  //       devi::devirtualizer_from_element_fn<decltype(element_fn), ParamTags...>(
  //           element_fn, &mask, &std::get<I>(retrieved_params)...);
  //   devi_fn(devirtualizer);
  // }

  template<size_t... I>
  static void add_signature_parameters(MFSignatureBuilder &signature,
                                       std::index_sequence<I...> /* indices */)
  {
    (
        [&] {
          using ParamTag = typename TagsSequence::template at_index<I>;
          signature.add(ParamTag(), "");
        }(),
        ...);
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    fn_(mask, params);
  }
};

/**
 * Generates a multi-function with the following parameters:
 * 1. single input (SI) of type In1
 * 2. single output (SO) of type Out1
 *
 * This example creates a function that adds 10 to the incoming values:
 * `CustomMF_SI_SO<int, int> fn("add 10", [](int value) { return value + 10; });`
 */
template<typename In1, typename Out1>
class CustomMF_SI_SO : public CustomMF<MFParamTag<MFParamCategory::SingleInput, In1>,
                                       MFParamTag<MFParamCategory::SingleOutput, Out1>> {
 public:
  template<typename ElementFn, typename DeviFn = devi::presets::None>
  CustomMF_SI_SO(const char *name, ElementFn element_fn, DeviFn devi_fn = devi::presets::None())
      : CustomMF<MFParamTag<MFParamCategory::SingleInput, In1>,
                 MFParamTag<MFParamCategory::SingleOutput, Out1>>(
            name,
            [element_fn](const In1 &in1, Out1 *out1) { new (out1) Out1(element_fn(in1)); },
            devi_fn)
  {
  }
};

/**
 * Generates a multi-function with the following parameters:
 * 1. single input (SI) of type In1
 * 2. single input (SI) of type In2
 * 3. single output (SO) of type Out1
 */
template<typename In1, typename In2, typename Out1>
class CustomMF_SI_SI_SO : public CustomMF<MFParamTag<MFParamCategory::SingleInput, In1>,
                                          MFParamTag<MFParamCategory::SingleInput, In2>,
                                          MFParamTag<MFParamCategory::SingleOutput, Out1>> {
 public:
  template<typename ElementFn, typename DeviFn = devi::presets::None>
  CustomMF_SI_SI_SO(const char *name, ElementFn element_fn, DeviFn devi_fn = devi::presets::None())
      : CustomMF<MFParamTag<MFParamCategory::SingleInput, In1>,
                 MFParamTag<MFParamCategory::SingleInput, In2>,
                 MFParamTag<MFParamCategory::SingleOutput, Out1>>(
            name,
            [element_fn](const In1 &in1, const In2 &in2, Out1 *out1) {
              new (out1) Out1(element_fn(in1, in2));
            },
            devi_fn)
  {
  }
};

/**
 * Generates a multi-function with the following parameters:
 * 1. single input (SI) of type In1
 * 2. single input (SI) of type In2
 * 3. single input (SI) of type In3
 * 4. single output (SO) of type Out1
 */
template<typename In1, typename In2, typename In3, typename Out1>
class CustomMF_SI_SI_SI_SO : public CustomMF<MFParamTag<MFParamCategory::SingleInput, In1>,
                                             MFParamTag<MFParamCategory::SingleInput, In2>,
                                             MFParamTag<MFParamCategory::SingleInput, In3>,
                                             MFParamTag<MFParamCategory::SingleOutput, Out1>> {
 public:
  template<typename ElementFn, typename DeviFn = devi::presets::None>
  CustomMF_SI_SI_SI_SO(const char *name,
                       ElementFn element_fn,
                       DeviFn devi_fn = devi::presets::None())
      : CustomMF<MFParamTag<MFParamCategory::SingleInput, In1>,
                 MFParamTag<MFParamCategory::SingleInput, In2>,
                 MFParamTag<MFParamCategory::SingleInput, In3>,
                 MFParamTag<MFParamCategory::SingleOutput, Out1>>(
            name,
            [element_fn](const In1 &in1, const In2 &in2, const In3 &in3, Out1 *out1) {
              new (out1) Out1(element_fn(in1, in2, in3));
            },
            devi_fn)
  {
  }
};

/**
 * Generates a multi-function with the following parameters:
 * 1. single input (SI) of type In1
 * 2. single input (SI) of type In2
 * 3. single input (SI) of type In3
 * 4. single input (SI) of type In4
 * 5. single output (SO) of type Out1
 */
template<typename In1, typename In2, typename In3, typename In4, typename Out1>
class CustomMF_SI_SI_SI_SI_SO : public CustomMF<MFParamTag<MFParamCategory::SingleInput, In1>,
                                                MFParamTag<MFParamCategory::SingleInput, In2>,
                                                MFParamTag<MFParamCategory::SingleInput, In3>,
                                                MFParamTag<MFParamCategory::SingleInput, In4>,
                                                MFParamTag<MFParamCategory::SingleOutput, Out1>> {
 public:
  template<typename ElementFn, typename DeviFn = devi::presets::None>
  CustomMF_SI_SI_SI_SI_SO(const char *name,
                          ElementFn element_fn,
                          DeviFn devi_fn = devi::presets::None())
      : CustomMF<MFParamTag<MFParamCategory::SingleInput, In1>,
                 MFParamTag<MFParamCategory::SingleInput, In2>,
                 MFParamTag<MFParamCategory::SingleInput, In3>,
                 MFParamTag<MFParamCategory::SingleInput, In4>,
                 MFParamTag<MFParamCategory::SingleOutput, Out1>>(
            name,
            [element_fn](
                const In1 &in1, const In2 &in2, const In3 &in3, const In4 &in4, Out1 *out1) {
              new (out1) Out1(element_fn(in1, in2, in3, in4));
            },
            devi_fn)
  {
  }
};

/**
 * Generates a multi-function with the following parameters:
 * 1. single mutable (SM) of type Mut1
 */
template<typename Mut1> class CustomMF_SM : public MultiFunction {
 private:
  using FunctionT = std::function<void(IndexMask, MutableSpan<Mut1>)>;
  FunctionT function_;
  MFSignature signature_;

 public:
  CustomMF_SM(const char *name, FunctionT function) : function_(std::move(function))
  {
    MFSignatureBuilder signature{name};
    signature.single_mutable<Mut1>("Mut1");
    signature_ = signature.build();
    this->set_signature(&signature_);
  }

  template<typename ElementFuncT>
  CustomMF_SM(const char *name, ElementFuncT element_fn)
      : CustomMF_SM(name, CustomMF_SM::create_function(element_fn))
  {
  }

  template<typename ElementFuncT> static FunctionT create_function(ElementFuncT element_fn)
  {
    return [=](IndexMask mask, MutableSpan<Mut1> mut1) {
      mask.to_best_mask_type([&](const auto &mask) {
        for (const int64_t i : mask) {
          element_fn(mut1[i]);
        }
      });
    };
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    MutableSpan<Mut1> mut1 = params.single_mutable<Mut1>(0);
    function_(mask, mut1);
  }
};

/**
 * A multi-function that outputs the same value every time. The value is not owned by an instance
 * of this function. If #make_value_copy is false, the caller is responsible for destructing and
 * freeing the value.
 */
class CustomMF_GenericConstant : public MultiFunction {
 private:
  const CPPType &type_;
  const void *value_;
  MFSignature signature_;
  bool owns_value_;

  template<typename T> friend class CustomMF_Constant;

 public:
  CustomMF_GenericConstant(const CPPType &type, const void *value, bool make_value_copy);
  ~CustomMF_GenericConstant();
  void call(IndexMask mask, MFParams params, MFContext context) const override;
  uint64_t hash() const override;
  bool equals(const MultiFunction &other) const override;
};

/**
 * A multi-function that outputs the same array every time. The array is not owned by in instance
 * of this function. The caller is responsible for destructing and freeing the values.
 */
class CustomMF_GenericConstantArray : public MultiFunction {
 private:
  GSpan array_;
  MFSignature signature_;

 public:
  CustomMF_GenericConstantArray(GSpan array);
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

/**
 * Generates a multi-function that outputs a constant value.
 */
template<typename T> class CustomMF_Constant : public MultiFunction {
 private:
  T value_;
  MFSignature signature_;

 public:
  template<typename U> CustomMF_Constant(U &&value) : value_(std::forward<U>(value))
  {
    MFSignatureBuilder signature{"Constant"};
    signature.single_output<T>("Value");
    signature_ = signature.build();
    this->set_signature(&signature_);
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    MutableSpan<T> output = params.uninitialized_single_output<T>(0);
    mask.to_best_mask_type([&](const auto &mask) {
      for (const int64_t i : mask) {
        new (&output[i]) T(value_);
      }
    });
  }

  uint64_t hash() const override
  {
    return get_default_hash(value_);
  }

  bool equals(const MultiFunction &other) const override
  {
    const CustomMF_Constant *other1 = dynamic_cast<const CustomMF_Constant *>(&other);
    if (other1 != nullptr) {
      return value_ == other1->value_;
    }
    const CustomMF_GenericConstant *other2 = dynamic_cast<const CustomMF_GenericConstant *>(
        &other);
    if (other2 != nullptr) {
      const CPPType &type = CPPType::get<T>();
      if (type == other2->type_) {
        return type.is_equal_or_false(static_cast<const void *>(&value_), other2->value_);
      }
    }
    return false;
  }
};

class CustomMF_DefaultOutput : public MultiFunction {
 private:
  int output_amount_;
  MFSignature signature_;

 public:
  CustomMF_DefaultOutput(Span<MFDataType> input_types, Span<MFDataType> output_types);
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

class CustomMF_GenericCopy : public MultiFunction {
 private:
  MFSignature signature_;

 public:
  CustomMF_GenericCopy(MFDataType data_type);
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

}  // namespace blender::fn
