/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_devirtualize_parameters.hh"

namespace blender::devirtualize_arrays::presets {

struct None {
  template<typename Fn, typename... ParamTypes>
  void operator()(Devirtualizer<Fn, ParamTypes...> &devirtualizer)
  {
    UNUSED_VARS(devirtualizer);
  }
};

struct AllSpanOrSingle {
  template<typename Fn, typename... ParamTypes>
  void operator()(Devirtualizer<Fn, ParamTypes...> &devirtualizer)
  {
    devirtualizer.try_execute_devirtualized(
        make_value_sequence<DeviMode,
                            DeviMode::Span | DeviMode::Single | DeviMode::Range,
                            sizeof...(ParamTypes)>());
  }
};

template<size_t... SpanIndices> struct SomeSpanOtherSingle {
  static constexpr DeviMode get_devi_mode(size_t I)
  {
    return ((I == SpanIndices) || ...) ? DeviMode::Span : DeviMode::Single;
  }

  template<size_t... I>
  static constexpr ParamModeSequence<get_devi_mode(I)...> get_devi_modes(
      std::index_sequence<I...> /* indices */)
  {
    return {};
  }

  template<typename Fn, typename... ParamTypes>
  void operator()(Devirtualizer<Fn, ParamTypes...> &devirtualizer)
  {
    devirtualizer.template try_execute_devirtualized(
        get_devi_modes(std::make_index_sequence<sizeof...(ParamTypes)>()));
  }
};

template<size_t SpanIndex> struct OneSpanOtherSingle {
  template<size_t... I>
  static ParamModeSequence<((I == SpanIndex) ? DeviMode::Span : DeviMode::Single)...> get_modes(
      std::index_sequence<I...> /* indices */)
  {
    return {};
  }

  template<typename Fn, typename... ParamTypes>
  void operator()(Devirtualizer<Fn, ParamTypes...> &devirtualizer)
  {
    devirtualizer.template try_execute_devirtualized(
        get_modes(std::make_index_sequence<sizeof...(ParamTypes)>()));
  }
};

}  // namespace blender::devirtualize_arrays::presets
