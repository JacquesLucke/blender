/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_devirtualize_arrays.hh"

namespace blender::devirtualize_arrays::presets {

struct None {
  template<typename Fn, typename... ParamTypes>
  void operator()(Devirtualizer<Fn, ParamTypes...> &devirtualizer)
  {
    devirtualizer.execute_fallback();
  }
};

struct Materialized {
  template<typename Fn, typename... ParamTypes>
  void operator()(Devirtualizer<Fn, ParamTypes...> &devirtualizer)
  {
    // devirtualizer.execute_materialized();
  }
};

struct AllSpanOrSingle {
  template<typename Fn, typename... ParamTypes>
  void operator()(Devirtualizer<Fn, ParamTypes...> &devirtualizer)
  {
    if (!devirtualizer.try_execute_devirtualized()) {
      // devirtualizer.execute_materialized();
    }
  }
};

template<size_t... SpanIndices> struct SomeSpanOtherSingle {
  static constexpr ParamMode get_param_mode(size_t I)
  {
    return ((I == SpanIndices) || ...) ? ParamMode::Span : ParamMode::Single;
  }

  template<size_t... I>
  static constexpr ParamModeSequence<get_param_mode(I)...> get_param_modes(
      std::index_sequence<I...> /* indices */)
  {
    return {};
  }

  template<typename Fn, typename... ParamTypes>
  void operator()(Devirtualizer<Fn, ParamTypes...> &devirtualizer)
  {
    if (!devirtualizer.template try_execute_devirtualized_custom(
            get_param_modes(std::make_index_sequence<sizeof...(ParamTypes)>()))) {
      // devirtualizer.execute_materialized();
    }
  }
};

template<size_t SpanIndex> struct OneSpanOtherSingle {
  template<size_t... I>
  static ParamModeSequence<((I == SpanIndex) ? ParamMode::Span : ParamMode::Single)...> get_modes(
      std::index_sequence<I...> /* indices */)
  {
    return {};
  }

  template<typename Fn, typename... ParamTypes>
  void operator()(Devirtualizer<Fn, ParamTypes...> &devirtualizer)
  {
    if (!devirtualizer.template try_execute_devirtualized_custom(
            get_modes(std::make_index_sequence<sizeof...(ParamTypes)>()))) {
      // devirtualizer.execute_materialized();
    }
  }
};

}  // namespace blender::devirtualize_arrays::presets
