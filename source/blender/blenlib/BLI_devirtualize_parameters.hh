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
enum class DeviMode {
  None = 0,
  Keep = (1 << 0),
  Span = (1 << 1),
  Single = (1 << 2),
  Range = (1 << 3),
  SpanAndSingle = Span | Single,
  SpanAndSingleAndRange = Span | Single | Range,
};
ENUM_OPERATORS(DeviMode, DeviMode::Range);

/** Utility to encode multiple #DeviMode in a type. */
template<DeviMode... Mode> using ParamModeSequence = ValueSequence<DeviMode, Mode...>;

/**
 * Main class that performs the devirtualization.
 */
template<typename Fn, typename... SourceTypes> class Devirtualizer {
 private:
  /** Utility to get the tag of the I-th parameter. */
  template<size_t I>
  using type_at_index = typename TypeSequence<SourceTypes...>::template at_index<I>;
  static constexpr size_t SourceTypesNum = sizeof...(SourceTypes);

  /** Function to devirtualize. */
  Fn fn_;
  std::tuple<const SourceTypes *...> sources_;

  bool executed_ = false;

 public:
  Devirtualizer(Fn fn, const SourceTypes *...sources) : fn_(std::move(fn)), sources_{sources...}
  {
  }

  bool executed() const
  {
    return executed_;
  }

  void execute_fallback()
  {
    BLI_assert(!executed_);
    this->try_execute_devirtualized_impl_call(
        make_value_sequence<DeviMode, DeviMode::Keep, SourceTypesNum>(),
        std::make_index_sequence<SourceTypesNum>());
  }

  bool try_execute_devirtualized()
  {
    BLI_assert(!executed_);
    return this->try_execute_devirtualized_custom(
        make_value_sequence<DeviMode, DeviMode::SpanAndSingleAndRange, SourceTypesNum>());
  }

  template<DeviMode... AllowedModes>
  bool try_execute_devirtualized_custom(ParamModeSequence<AllowedModes...> /* allowed_modes */)
  {
    BLI_assert(!executed_);
    static_assert(sizeof...(AllowedModes) == SourceTypesNum);
    return this->try_execute_devirtualized_impl(ParamModeSequence<>(),
                                                ParamModeSequence<AllowedModes...>());
  }

 private:
  template<DeviMode... Mode, DeviMode... AllowedModes>
  bool try_execute_devirtualized_impl(ParamModeSequence<Mode...> /* modes */,
                                      ParamModeSequence<AllowedModes...> /* allowed_modes */)
  {
    static_assert(SourceTypesNum == sizeof...(AllowedModes));
    if constexpr (SourceTypesNum == sizeof...(Mode)) {
      this->try_execute_devirtualized_impl_call(ParamModeSequence<Mode...>(),
                                                std::make_index_sequence<SourceTypesNum>());
      return true;
    }
    else {
      constexpr size_t I = sizeof...(Mode);
      using SourceType = type_at_index<I>;
      constexpr DeviMode allowed_modes =
          ParamModeSequence<AllowedModes...>::template at_index<I>();
      if constexpr (std::is_base_of_v<VArrayBase, SourceType>) {
        SourceType &varray = *std::get<I>(sources_);
        if constexpr ((allowed_modes & DeviMode::Single) != DeviMode::None) {
          if (varray.is_single()) {
            return this->try_execute_devirtualized_impl(
                ParamModeSequence<Mode..., DeviMode::Single>(),
                ParamModeSequence<AllowedModes...>());
          }
        }
        if constexpr ((allowed_modes & DeviMode::Span) != DeviMode::None) {
          if (varray.is_span()) {
            return this->try_execute_devirtualized_impl(
                ParamModeSequence<Mode..., DeviMode::Span>(),
                ParamModeSequence<AllowedModes...>());
          }
        }
        if constexpr ((allowed_modes & DeviMode::Keep) != DeviMode::None) {
          return this->try_execute_devirtualized_impl(ParamModeSequence<Mode..., DeviMode::Keep>(),
                                                      ParamModeSequence<AllowedModes...>());
        }
        return false;
      }
      else if constexpr (std::is_same_v<IndexMask, SourceType>) {
        if constexpr ((allowed_modes & DeviMode::Range) != DeviMode::None) {
          const IndexMask &mask = *std::get<I>(sources_);
          if (mask.is_range()) {
            return this->try_execute_devirtualized_impl(
                ParamModeSequence<Mode..., DeviMode::Range>(),
                ParamModeSequence<AllowedModes...>());
          }
        }
        if constexpr ((allowed_modes & DeviMode::Span) != DeviMode::None) {
          return this->try_execute_devirtualized_impl(ParamModeSequence<Mode..., DeviMode::Span>(),
                                                      ParamModeSequence<AllowedModes...>());
        }
        return false;
      }
      else {
        return this->try_execute_devirtualized_impl(ParamModeSequence<Mode..., DeviMode::Keep>(),
                                                    ParamModeSequence<AllowedModes...>());
      }
    }
  }

  template<DeviMode... Mode, size_t... I>
  void try_execute_devirtualized_impl_call(ParamModeSequence<Mode...> /* modes */,
                                           std::index_sequence<I...> /* indices */)
  {

    fn_(this->get_devirtualized_parameter<I, Mode>()...);
    executed_ = true;
  }

  template<size_t I, DeviMode Mode> decltype(auto) get_devirtualized_parameter()
  {
    using SourceType = type_at_index<I>;
    static_assert(Mode != DeviMode::None);
    if constexpr (Mode == DeviMode::Keep) {
      return *std::get<I>(sources_);
    }
    if constexpr (std::is_base_of_v<VArrayBase, SourceType>) {
      const SourceType &varray = *std::get<I>(sources_);
      if constexpr (Mode == DeviMode::Single) {
        return SingleAsSpan(varray);
      }
      else if constexpr (Mode == DeviMode::Span) {
        return varray.get_internal_span();
      }
    }
    else if constexpr (std::is_same_v<IndexMask, SourceType>) {
      const IndexMask &mask = *std::get<I>(sources_);
      if constexpr (ELEM(Mode, DeviMode::Span)) {
        return mask;
      }
      else if constexpr (Mode == DeviMode::Range) {
        return mask.as_range();
      }
    }
  }
};

}  // namespace blender::devirtualize_arrays
