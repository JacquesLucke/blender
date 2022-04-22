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

template<typename... ParamTags, size_t... I, typename ElementFn, typename MaskT, typename... Args>
void execute_array(TypeSequence<ParamTags...> /* param_tags */,
                   std::index_sequence<I...> /* indices */,
                   ElementFn element_fn,
                   MaskT mask,
                   Args &&...args)
{
  for (const int64_t i : mask) {
    element_fn([&] {
      using ParamTag = typename TypeSequence<ParamTags...>::at_index<I>;
      if constexpr (ParamTag::category == MFParamCategory::SingleInput) {
        return args[i];
      }
      else if constexpr (ParamTag::category == MFParamCategory::SingleOutput) {
        return &args[i];
      }
    }()...);
  }
}

template<typename... ParamTags,
         typename ElementFn,
         typename InMask,
         typename OutMask,
         typename... Chunks>
void execute_materialized_impl(TypeSequence<ParamTags...> /* param_tags */,
                               const ElementFn element_fn,
                               InMask in_mask,
                               OutMask out_mask,
                               Chunks &&__restrict... chunks)
{
  BLI_assert(in_mask.size() == out_mask.size());
  for (const int64_t i : IndexRange(in_mask.size())) {
    const int64_t in_i = in_mask[i];
    const int64_t out_i = out_mask[i];
    element_fn([&]() -> decltype(auto) {
      using ParamTag = ParamTags;
      if constexpr (ParamTag::category == MFParamCategory::SingleInput) {
        return chunks[in_i];
      }
      else if constexpr (ParamTag::category == MFParamCategory::SingleOutput) {
        return &chunks[out_i];
      }
    }()...);
  }
}

template<typename... ParamTags, size_t... I, typename ElementFn, typename... Args>
void execute_materialized(TypeSequence<ParamTags...> /* param_tags */,
                          std::index_sequence<I...> /* indices */,
                          const ElementFn element_fn,
                          const IndexMask mask,
                          Args &&...args)
{
  enum class ArgMode {
    Unknown,
    Single,
    Span,
    Materialized,
  };

  static constexpr int64_t MaxChunkSize = 32;
  const int64_t mask_size = mask.size();
  const int64_t buffer_size = std::min(mask_size, MaxChunkSize);
  std::tuple<TypedBuffer<typename ParamTags::base_type, MaxChunkSize>...> buffers_owner;
  std::tuple<MutableSpan<typename ParamTags::base_type>...> buffers = {
      MutableSpan{std::get<I>(buffers_owner).ptr(), buffer_size}...};
  std::array<ArgMode, sizeof...(ParamTags)> arg_modes;
  arg_modes.fill(ArgMode::Unknown);

  (
      [&] {
        using ParamTag = ParamTags;
        using T = typename ParamTag::base_type;
        if constexpr (ParamTag::category == MFParamCategory::SingleInput) {
          VArray<T> &varray = *args;
          if (varray.is_single()) {
            MutableSpan<T> in_chunk{std::get<I>(buffers_owner).ptr(), buffer_size};
            const T in_single = varray.get_internal_single();
            uninitialized_fill_n(in_chunk.data(), in_chunk.size(), in_single);
            std::get<I>(buffers) = in_chunk;
            arg_modes[I] = ArgMode::Single;
          }
        }
      }(),
      ...);

  for (int64_t chunk_start = 0; chunk_start < mask_size; chunk_start += MaxChunkSize) {
    const int64_t chunk_size = std::min(mask_size - chunk_start, MaxChunkSize);
    const IndexMask sliced_mask = mask.slice(chunk_start, chunk_size);
    const bool sliced_mask_is_range = sliced_mask.is_range();

    execute_materialized_impl(
        TypeSequence<ParamTags...>(), element_fn, IndexRange(chunk_size), sliced_mask, [&] {
          using ParamTag = ParamTags;
          using T = typename ParamTag::base_type;
          if constexpr (ParamTag::category == MFParamCategory::SingleInput) {
            if (arg_modes[I] == ArgMode::Single) {
              return Span<T>(std::get<I>(buffers));
            }
            else {
              const VArray<T> &varray = *args;
              if (sliced_mask_is_range) {
                if (varray.is_span()) {
                  const IndexRange sliced_mask_range = sliced_mask.as_range();
                  const Span<T> data = varray.get_internal_span();
                  arg_modes[I] = ArgMode::Span;
                  return data.slice(sliced_mask_range);
                }
              }
              MutableSpan<T> in_chunk{std::get<I>(buffers_owner).ptr(), chunk_size};
              varray.materialize_compressed_to_uninitialized(sliced_mask, in_chunk);
              arg_modes[I] = ArgMode::Materialized;
              return Span<T>(in_chunk);
            }
          }
          else if constexpr (ParamTag::category == MFParamCategory::SingleOutput) {
            return args->data();
          }
        }()...);

    (
        [&] {
          using ParamTag = ParamTags;
          using T = typename ParamTag::base_type;
          if constexpr (ParamTag::category == MFParamCategory::SingleInput) {
            if (arg_modes[I] == ArgMode::Materialized) {
              T *in_chunk = std::get<I>(buffers_owner).ptr();
              destruct_n(in_chunk, chunk_size);
            }
          }
        }(),
        ...);
  }

  (
      [&] {
        using ParamTag = ParamTags;
        using T = typename ParamTag::base_type;
        if constexpr (ParamTag::category == MFParamCategory::SingleInput) {
          if (arg_modes[I] == ArgMode::Single) {
            MutableSpan<T> in_chunk = std::get<I>(buffers);
            destruct_n(in_chunk.data(), in_chunk.size());
          }
        }
      }(),
      ...);

  UNUSED_VARS(element_fn);
}

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

    fn_ = [element_fn, devi_fn](IndexMask mask, MFParams params) {
      execute(element_fn, devi_fn, mask, params, std::make_index_sequence<TagsSequence::size()>());
    };
  }

  template<typename ElementFn, typename DeviFn, size_t... I>
  static void execute(ElementFn element_fn,
                      DeviFn devi_fn,
                      IndexMask mask,
                      MFParams params,
                      std::index_sequence<I...> /* indices */)
  {
    UNUSED_VARS(element_fn, mask, devi_fn);
    std::tuple<typename ParamTags::array_type...> retrieved_params;
    (
        [&]() {
          using ParamTag = typename TagsSequence::template at_index<I>;
          using T = typename ParamTag::base_type;

          if constexpr (ParamTag::category == MFParamCategory::SingleInput) {
            std::get<I>(retrieved_params) = params.readonly_single_input<T>(I);
          }
          if constexpr (ParamTag::category == MFParamCategory::SingleOutput) {
            std::get<I>(retrieved_params) = params.uninitialized_single_output<T>(I);
          }
        }(),
        ...);

    execute_materialized(
        TypeSequence<ParamTags...>(), std::index_sequence<I...>(), element_fn, mask, [&] {
          return &std::get<I>(retrieved_params);
        }()...);

    // auto array_executor = [&](auto &&...args) {
    //   execute_array(TagsSequence(),
    //                 std::make_index_sequence<TagsSequence::size()>(),
    //                 element_fn,
    //                 std::forward<decltype(args)>(args)...);
    // };

    // devi::Devirtualizer<decltype(array_executor), IndexMask, typename ParamTags::array_type...>
    //     devirtualizer{array_executor, &mask, [&] { return &std::get<I>(retrieved_params);
    //     }()...};
    // devirtualizer.execute_fallback();
  }

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
