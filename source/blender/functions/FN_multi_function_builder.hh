/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup fn
 *
 * This file contains several utilities to create multi-functions with less redundant code.
 */

#include "BLI_array_function_evaluation.hh"

#include "FN_multi_function.hh"

namespace blender::fn::multi_function::build {

/**
 * These presets determine what code is generated for a #CustomMF. Different presets make different
 * trade-offs between run-time performance and compile-time/binary size.
 */
namespace exec_presets {

/** Method to execute a function in case devirtualization was not possible. */
enum class FallbackMode {
  /** Access all elements in virtual arrays through virtual function calls. */
  Simple,
  /** Process elements in chunks to reduce virtual function call overhead. */
  Materialized,
};

/**
 * The "naive" method for executing a #CustomMF. Every element is processed separately and input
 * values are retrieved from the virtual arrays one by one. This generates the least amount of
 * code, but is also the slowest method.
 */
struct Simple {
  static constexpr bool use_devirtualization = false;
  static constexpr FallbackMode fallback_mode = FallbackMode::Simple;
};

/**
 * This is an improvement over the #Simple method. It still generates a relatively small amount of
 * code, because the function is only instantiated once. It's generally faster than #Simple,
 * because inputs are retrieved from the virtual arrays in chunks, reducing virtual method call
 * overhead.
 */
struct Materialized {
  static constexpr bool use_devirtualization = false;
  static constexpr FallbackMode fallback_mode = FallbackMode::Materialized;
};

/**
 * The most efficient preset, but also potentially generates a lot of code (exponential in the
 * number of inputs of the function). It generates separate optimized loops for all combinations of
 * inputs. This should be used for small functions of which all inputs are likely to be single
 * values or spans, and the number of inputs is relatively small.
 */
struct AllSpanOrSingle {
  static constexpr bool use_devirtualization = true;
  static constexpr FallbackMode fallback_mode = FallbackMode::Materialized;

  template<typename... ParamTags, typename... LoadedParams, size_t... I>
  auto create_devirtualizers(TypeSequence<ParamTags...> /*param_tags*/,
                             std::index_sequence<I...> /*indices*/,
                             const IndexMask &mask,
                             const std::tuple<LoadedParams...> &loaded_params) const
  {
    return std::make_tuple(IndexMaskDevirtualizer<true, true>{mask}, [&]() {
      typedef ParamTags ParamTag;
      typedef typename ParamTag::base_type T;
      if constexpr (ParamTag::category == ParamCategory::SingleInput) {
        const GVArrayImpl &varray_impl = *std::get<I>(loaded_params);
        return GVArrayDevirtualizer<T, true, true>{varray_impl};
      }
      else if constexpr (ELEM(ParamTag::category,
                              ParamCategory::SingleOutput,
                              ParamCategory::SingleMutable)) {
        T *ptr = std::get<I>(loaded_params);
        return BasicDevirtualizer<T *>{ptr};
      }
    }()...);
  }
};

/**
 * A slightly weaker variant of #AllSpanOrSingle. It generates less code, because it assumes that
 * some of the inputs are most likely single values. It should be used for small functions which
 * have too many inputs to make #AllSingleOrSpan a reasonable choice.
 */
template<size_t... Indices> struct SomeSpanOrSingle {
  static constexpr bool use_devirtualization = true;
  static constexpr FallbackMode fallback_mode = FallbackMode::Materialized;

  template<typename... ParamTags, typename... LoadedParams, size_t... I>
  auto create_devirtualizers(TypeSequence<ParamTags...> /*param_tags*/,
                             std::index_sequence<I...> /*indices*/,
                             const IndexMask &mask,
                             const std::tuple<LoadedParams...> &loaded_params) const
  {
    return std::make_tuple(IndexMaskDevirtualizer<true, true>{mask}, [&]() {
      typedef ParamTags ParamTag;
      typedef typename ParamTag::base_type T;

      if constexpr (ParamTag::category == ParamCategory::SingleInput) {
        constexpr bool UseSpan = ValueSequence<size_t, Indices...>::template contains<I>();
        const GVArrayImpl &varray_impl = *std::get<I>(loaded_params);
        return GVArrayDevirtualizer<T, true, UseSpan>{varray_impl};
      }
      else if constexpr (ELEM(ParamTag::category,
                              ParamCategory::SingleOutput,
                              ParamCategory::SingleMutable)) {
        T *ptr = std::get<I>(loaded_params);
        return BasicDevirtualizer<T *>{ptr};
      }
    }()...);
  }
};

}  // namespace exec_presets

namespace detail {

template<typename ElementFn, typename ExecPreset, typename... ParamTags, size_t... I>
inline void execute_element_fn_as_multi_function(const ElementFn element_fn,
                                                 const ExecPreset exec_preset,
                                                 const IndexMask mask,
                                                 Params params,
                                                 TypeSequence<ParamTags...> /*param_tags*/,
                                                 std::index_sequence<I...> /*indices*/)
{

  /* Load parameters from #Params. */
  /* Contains `const GVArrayImpl *` for inputs and `T *` for outputs. */
  const auto loaded_params = std::make_tuple([&]() {
    /* Use `typedef` instead of `using` to work around a compiler bug. */
    typedef ParamTags ParamTag;
    typedef typename ParamTag::base_type T;

    if constexpr (ParamTag::category == ParamCategory::SingleInput) {
      return params.readonly_single_input(I).get_implementation();
    }
    else if constexpr (ParamTag::category == ParamCategory::SingleOutput) {
      return static_cast<T *>(params.uninitialized_single_output(I).data());
    }
    else if constexpr (ParamTag::category == ParamCategory::SingleMutable) {
      return static_cast<T *>(params.single_mutable(I).data());
    }
  }()...);

  /* Try execute devirtualized if enabled and the input types allow it. */
  bool executed_devirtualized = false;
  if constexpr (ExecPreset::use_devirtualization) {
    const auto devirtualizers = exec_preset.create_devirtualizers(
        TypeSequence<ParamTags...>(), std::index_sequence<I...>(), mask, loaded_params);
    executed_devirtualized = call_with_devirtualized_parameters(
        devirtualizers, [&](auto &&...args) {
          array_function_evaluation::execute_array(element_fn,
                                                   std::forward<decltype(args)>(args)...);
        });
  }

  /* If devirtualized execution was disabled or not possible, use a fallback method which is
   * slower but always works. */
  if (!executed_devirtualized) {
    /* The materialized method is most common because it avoids most virtual function overhead but
     * still instantiates the function only once. */
    if constexpr (ExecPreset::fallback_mode == exec_presets::FallbackMode::Materialized) {
      execute_materialized(element_fn, mask, std::index_sequence<I...>(), [&]() {
        /* Use `typedef` instead of `using` to work around a compiler bug. */
        typedef ParamTags ParamTag;
        typedef typename ParamTag::base_type T;
        if constexpr (ParamTag::category == ParamCategory::SingleInput) {
          const GVArrayImpl &varray_impl = *std::get<I>(loaded_params);
          return MaterializeGVArrayInput<T>{varray_impl, varray_impl.common_info()};
        }
        else if constexpr (ParamTag::category == ParamCategory::SingleMutable) {
          T *ptr = std::get<I>(loaded_params);
          return array_function_evaluation::ArrayMutable<T>{ptr};
        }
        else if constexpr (ParamTag::category == ParamCategory::SingleOutput) {
          T *ptr = std::get<I>(loaded_params);
          return array_function_evaluation::ArrayOutput<T>{ptr};
        }
      }()...);
    }
    else {
      /* This fallback is slower because it uses virtual method calls for every element. */
      array_function_evaluation::execute_array(element_fn, mask, [&]() {
        /* Use `typedef` instead of `using` to work around a compiler bug. */
        typedef ParamTags ParamTag;
        typedef typename ParamTag::base_type T;
        if constexpr (ParamTag::category == ParamCategory::SingleInput) {
          const GVArrayImpl &varray_impl = *std::get<I>(loaded_params);
          return GVArray(&varray_impl).typed<T>();
        }
        else if constexpr (ELEM(ParamTag::category,
                                ParamCategory::SingleOutput,
                                ParamCategory::SingleMutable)) {
          T *ptr = std::get<I>(loaded_params);
          return ptr;
        }
      }()...);
    }
  }
}

/**
 * `element_fn` is expected to return nothing and to have the following parameters:
 * - For single-inputs: const value or reference.
 * - For single-mutables: non-const reference.
 * - For single-outputs: non-const pointer.
 */
template<typename ElementFn, typename ExecPreset, typename... ParamTags>
inline auto build_multi_function_call_from_element_fn(const ElementFn element_fn,
                                                      const ExecPreset exec_preset,
                                                      TypeSequence<ParamTags...> /*param_tags*/)
{
  return [element_fn, exec_preset](const IndexMask mask, Params params) {
    execute_element_fn_as_multi_function(element_fn,
                                         exec_preset,
                                         mask,
                                         params,
                                         TypeSequence<ParamTags...>(),
                                         std::make_index_sequence<sizeof...(ParamTags)>());
  };
}

/**
 * A multi function that just invokes the provided function in its #call method.
 */
template<typename CallFn, typename... ParamTags> class CustomMF : public MultiFunction {
 private:
  Signature signature_;
  CallFn call_fn_;

 public:
  CustomMF(const char *name, CallFn call_fn, TypeSequence<ParamTags...> /*param_tags*/)
      : call_fn_(std::move(call_fn))
  {
    SignatureBuilder builder{name, signature_};
    /* Loop over all parameter types and add an entry for each in the signature. */
    ([&] { builder.add(ParamTags(), ""); }(), ...);
    this->set_signature(&signature_);
  }

  void call(IndexMask mask, Params params, Context /*context*/) const override
  {
    call_fn_(mask, params);
  }
};

template<typename Out, typename... In, typename ElementFn, typename ExecPreset>
inline auto build_multi_function_with_n_inputs_one_output(const char *name,
                                                          const ElementFn element_fn,
                                                          const ExecPreset exec_preset,
                                                          TypeSequence<In...> /*in_types*/)
{
  constexpr auto param_tags = TypeSequence<ParamTag<ParamCategory::SingleInput, In>...,
                                           ParamTag<ParamCategory::SingleOutput, Out>>();
  auto call_fn = build_multi_function_call_from_element_fn(
      [element_fn](const In &...in, Out &out) { new (&out) Out(element_fn(in...)); },
      exec_preset,
      param_tags);
  return CustomMF(name, call_fn, param_tags);
}

}  // namespace detail

/** Build multi-function with 1 single-input and 1 single-output parameter. */
template<typename In1,
         typename Out1,
         typename ElementFn,
         typename ExecPreset = exec_presets::Materialized>
inline auto SI1_SO(const char *name,
                   const ElementFn element_fn,
                   const ExecPreset exec_preset = exec_presets::Materialized())
{
  return detail::build_multi_function_with_n_inputs_one_output<Out1>(
      name, element_fn, exec_preset, TypeSequence<In1>());
}

/** Build multi-function with 2 single-input and 1 single-output parameter. */
template<typename In1,
         typename In2,
         typename Out1,
         typename ElementFn,
         typename ExecPreset = exec_presets::Materialized>
inline auto SI2_SO(const char *name,
                   const ElementFn element_fn,
                   const ExecPreset exec_preset = exec_presets::Materialized())
{
  return detail::build_multi_function_with_n_inputs_one_output<Out1>(
      name, element_fn, exec_preset, TypeSequence<In1, In2>());
}

/** Build multi-function with 3 single-input and 1 single-output parameter. */
template<typename In1,
         typename In2,
         typename In3,
         typename Out1,
         typename ElementFn,
         typename ExecPreset = exec_presets::Materialized>
inline auto SI3_SO(const char *name,
                   const ElementFn element_fn,
                   const ExecPreset exec_preset = exec_presets::Materialized())
{
  return detail::build_multi_function_with_n_inputs_one_output<Out1>(
      name, element_fn, exec_preset, TypeSequence<In1, In2, In3>());
}

/** Build multi-function with 4 single-input and 1 single-output parameter. */
template<typename In1,
         typename In2,
         typename In3,
         typename In4,
         typename Out1,
         typename ElementFn,
         typename ExecPreset = exec_presets::Materialized>
inline auto SI4_SO(const char *name,
                   const ElementFn element_fn,
                   const ExecPreset exec_preset = exec_presets::Materialized())
{
  return detail::build_multi_function_with_n_inputs_one_output<Out1>(
      name, element_fn, exec_preset, TypeSequence<In1, In2, In3, In4>());
}

/** Build multi-function with 5 single-input and 1 single-output parameter. */
template<typename In1,
         typename In2,
         typename In3,
         typename In4,
         typename In5,
         typename Out1,
         typename ElementFn,
         typename ExecPreset = exec_presets::Materialized>
inline auto SI5_SO(const char *name,
                   const ElementFn element_fn,
                   const ExecPreset exec_preset = exec_presets::Materialized())
{
  return detail::build_multi_function_with_n_inputs_one_output<Out1>(
      name, element_fn, exec_preset, TypeSequence<In1, In2, In3, In4, In5>());
}

/** Build multi-function with 6 single-input and 1 single-output parameter. */
template<typename In1,
         typename In2,
         typename In3,
         typename In4,
         typename In5,
         typename In6,
         typename Out1,
         typename ElementFn,
         typename ExecPreset = exec_presets::Materialized>
inline auto SI6_SO(const char *name,
                   const ElementFn element_fn,
                   const ExecPreset exec_preset = exec_presets::Materialized())
{
  return detail::build_multi_function_with_n_inputs_one_output<Out1>(
      name, element_fn, exec_preset, TypeSequence<In1, In2, In3, In4, In5, In6>());
}

/** Build multi-function with 1 single-mutable parameter. */
template<typename Mut1, typename ElementFn, typename ExecPreset = exec_presets::AllSpanOrSingle>
inline auto SM(const char *name,
               const ElementFn element_fn,
               const ExecPreset exec_preset = exec_presets::AllSpanOrSingle())
{
  constexpr auto param_tags = TypeSequence<ParamTag<ParamCategory::SingleMutable, Mut1>>();
  auto call_fn = detail::build_multi_function_call_from_element_fn(
      element_fn, exec_preset, param_tags);
  return detail::CustomMF(name, call_fn, param_tags);
}

}  // namespace blender::fn::multi_function::build

namespace blender::fn::multi_function {

/**
 * A multi-function that outputs the same value every time. The value is not owned by an instance
 * of this function. If #make_value_copy is false, the caller is responsible for destructing and
 * freeing the value.
 */
class CustomMF_GenericConstant : public MultiFunction {
 private:
  const CPPType &type_;
  const void *value_;
  Signature signature_;
  bool owns_value_;

  template<typename T> friend class CustomMF_Constant;

 public:
  CustomMF_GenericConstant(const CPPType &type, const void *value, bool make_value_copy);
  ~CustomMF_GenericConstant();
  void call(IndexMask mask, Params params, Context context) const override;
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
  Signature signature_;

 public:
  CustomMF_GenericConstantArray(GSpan array);
  void call(IndexMask mask, Params params, Context context) const override;
};

/**
 * Generates a multi-function that outputs a constant value.
 */
template<typename T> class CustomMF_Constant : public MultiFunction {
 private:
  T value_;
  Signature signature_;

 public:
  template<typename U> CustomMF_Constant(U &&value) : value_(std::forward<U>(value))
  {
    SignatureBuilder builder{"Constant", signature_};
    builder.single_output<T>("Value");
    this->set_signature(&signature_);
  }

  void call(IndexMask mask, Params params, Context /*context*/) const override
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
  Signature signature_;

 public:
  CustomMF_DefaultOutput(Span<DataType> input_types, Span<DataType> output_types);
  void call(IndexMask mask, Params params, Context context) const override;
};

class CustomMF_GenericCopy : public MultiFunction {
 private:
  Signature signature_;

 public:
  CustomMF_GenericCopy(DataType data_type);
  void call(IndexMask mask, Params params, Context context) const override;
};

}  // namespace blender::fn::multi_function
