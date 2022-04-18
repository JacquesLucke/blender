/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * In geometry nodes, many functions accept fields as inputs. For the implementation that means
 * that the inputs are virtual arrays. Usually those are backed by actual arrays or single values.
 *
 * Using virtual arrays has to downside that individual elements are accessed through a virtual
 * method call, which has some overhead to normal array access. Whether this overhead is negilible
 * depends on the context. For very small functions (e.g. a single addition), the overhead can make
 * the function many times slower. Furthermore, it prevents the compiler from doing some
 * optimizations (e.g. loop unrolling and inserting simd instructions).
 *
 * The solution is to "devirtualize" the virtual arrays in cases when the overhead cannot be
 * ignored. That means that the function is instantiated multiple times at compile time for the
 * different cases. For example, there can be an optimized function that adds a span and a single
 * value, and another function that adds a span and another span. At run-time there is a dynamic
 * dispatch that executes the best function given the specific virtual arrays.
 *
 * The problem with this devirtualization is that it can result in exponentially increasing compile
 * times and binary sizes, depending on the number of parameters that are devirtualized separately.
 * So there is always a trade-off between run-time performance and compile-time/binary-size.
 *
 * This file provides a utility to devirtualize array parameters to a function using a high level
 * API. This makes it easy to experiment with different extremes of the mentioned trade-off and
 * allows finding a good compromise for each function.
 */

#include "BLI_parameter_pack_utils.hh"
#include "BLI_virtual_array.hh"

namespace blender::devirtualize_arrays {

/**
 * Bit flag that specifies how an individual parameter is or can be devirtualized.
 */
enum class ParamMode {
  None = 0,
  Span = (1 << 0),
  Single = (1 << 1),
  VArray = (1 << 2),
  Range = (1 << 3),
  SpanAndSingle = Span | Single,
  SpanAndSingleAndRange = Span | Single | Range,
};
ENUM_OPERATORS(ParamMode, ParamMode::Range);

/** Utility to encode multiple #ParamMode in a type. */
template<ParamMode... Mode> using ParamModeSequence = ValueSequence<ParamMode, Mode...>;

/**
 * Main class that performs the devirtualization.
 */
template<typename Fn, typename... ParamTypes> class Devirtualizer {
 private:
  /** Utility to get the tag of the I-th parameter. */
  template<size_t I>
  using type_at_index = typename TypeSequence<ParamTypes...>::template at_index<I>;

  /** Function to devirtualize. */
  Fn fn_;
  std::tuple<const ParamTypes *...> params_;

  std::array<bool, sizeof...(ParamTypes)> varray_is_span_;
  std::array<bool, sizeof...(ParamTypes)> varray_is_single_;

  bool executed_ = false;

 public:
  Devirtualizer(Fn fn, const ParamTypes *...params) : fn_(std::move(fn)), params_{params...}
  {
    this->init(std::make_index_sequence<sizeof...(ParamTypes)>{});
  }

  void execute_fallback()
  {
    BLI_assert(!executed_);
    this->execute_fallback_impl(std::make_index_sequence<sizeof...(ParamTypes)>{});
    this->try_execute_devirtualized_impl_call(
        make_value_sequence<ParamMode, ParamMode::None, sizeof...(ParamTypes)>(),
        std::make_index_sequence<sizeof...(ParamTypes)>());
  }

  bool try_execute_devirtualized()
  {
    BLI_assert(!executed_);
    return this->try_execute_devirtualized_custom(
        make_value_sequence<ParamMode, ParamMode::SpanAndSingleAndRange, sizeof...(ParamTypes)>());
  }

  template<ParamMode... AllowedModes>
  bool try_execute_devirtualized_custom(ParamModeSequence<AllowedModes...> /* allowed_modes */)
  {
    BLI_assert(!executed_);
    static_assert(sizeof...(AllowedModes) == sizeof...(ParamTypes));
    return this->try_execute_devirtualized_impl(ParamModeSequence<>(),
                                                ParamModeSequence<AllowedModes...>());
  }

 private:
  template<size_t... I> void init(std::index_sequence<I...> /* indices */)
  {
    varray_is_span_.fill(false);
    varray_is_single_.fill(false);
    (
        [&] {
          using ParamType = type_at_index<I>;
          if constexpr (std::is_base_of_v<VArrayBase, ParamType>) {
            const ParamType *varray = std::get<I>(params_);
            varray_is_span_[I] = varray->is_span();
            varray_is_single_[I] = varray->is_single();
          }
        }(),
        ...);
  }

  template<ParamMode... Mode, ParamMode... AllowedModes>
  bool try_execute_devirtualized_impl(ParamModeSequence<Mode...> /* modes */,
                                      ParamModeSequence<AllowedModes...> /* allowed_modes */)
  {
    static_assert(sizeof...(AllowedModes) == sizeof...(ParamTypes));
    if constexpr (sizeof...(Mode) == sizeof...(ParamTypes)) {
      this->try_execute_devirtualized_impl_call(ParamModeSequence<Mode...>(),
                                                std::make_index_sequence<sizeof...(ParamTypes)>());
      return true;
    }
    else {
      constexpr size_t I = sizeof...(Mode);
      using ParamType = type_at_index<I>;
      constexpr ParamMode allowed_modes =
          ParamModeSequence<AllowedModes...>::template at_index<I>();
      if constexpr (std::is_base_of_v<VArrayBase, ParamType>) {
        if constexpr ((allowed_modes & ParamMode::Single) != ParamMode::None) {
          if (varray_is_single_[I]) {
            return this->try_execute_devirtualized_impl(
                ParamModeSequence<Mode..., ParamMode::Single>(),
                ParamModeSequence<AllowedModes...>());
          }
        }
        if constexpr ((allowed_modes & ParamMode::Span) != ParamMode::None) {
          if (varray_is_span_[I]) {
            return this->try_execute_devirtualized_impl(
                ParamModeSequence<Mode..., ParamMode::Span>(),
                ParamModeSequence<AllowedModes...>());
          }
        }
        if constexpr ((allowed_modes & ParamMode::VArray) != ParamMode::None) {
          return this->try_execute_devirtualized_impl(
              ParamModeSequence<Mode..., ParamMode::VArray>(),
              ParamModeSequence<AllowedModes...>());
        }
        return false;
      }
      else if constexpr (std::is_same_v<IndexMask, ParamType>) {
        if constexpr ((allowed_modes & ParamMode::Range) != ParamMode::None) {
          const IndexMask &mask = *params_[I];
          if (mask.is_range()) {
            return this->try_execute_devirtualized_impl(
                ParamModeSequence<Mode..., ParamMode::Range>(),
                ParamModeSequence<AllowedModes...>());
          }
        }
        if constexpr ((allowed_modes & ParamMode::Span) != ParamMode::None) {
          return this->try_execute_devirtualized_impl(
              ParamModeSequence<Mode..., ParamMode::Span>(), ParamModeSequence<AllowedModes...>());
        }
      }
      else {
        return false;
      }
    }
  }

  template<ParamMode... Mode, size_t... I>
  void try_execute_devirtualized_impl_call(ParamModeSequence<Mode...> /* modes */,
                                           std::index_sequence<I...> /* indices */)
  {

    fn_(this->get_devirtualized_parameter<I, Mode>()...);
    executed_ = true;
  }

  template<size_t I, ParamMode Mode> decltype(auto) get_devirtualized_parameter()
  {
    using ParamType = type_at_index<I>;
    if constexpr (std::is_base_of_v<VArrayBase, ParamType>) {
      const ParamType &varray = *std::get<I>(params_);
      if constexpr (ELEM(Mode, ParamMode::None, ParamMode::VArray)) {
        return varray;
      }
      else if constexpr (Mode == ParamMode::Single) {
        return SingleAsSpan(varray);
      }
      else if constexpr (Mode == ParamMode::Span) {
        return varray.get_internal_span();
      }
    }
    else if constexpr (std::is_same_v<IndexMask, ParamType>) {
      const IndexMask &mask = *std::get<I>(params_);
      if constexpr (Mode == ParamMode::None) {
        return mask;
      }
      else if constexpr (Mode == ParamMode::Span) {
        return mask.as_range();
      }
    }
  }
};

}  // namespace blender::devirtualize_arrays
