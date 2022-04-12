/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_devirtualize_arrays.hh"

namespace blender::devirtualize_arrays::presets {

struct None {
  template<typename Fn, typename... ParamTags>
  void operator()(Devirtualizer<Fn, ParamTags...> &devirtualizer)
  {
    devirtualizer.execute_fallback();
  }
};

struct Materialized {
  template<typename Fn, typename... ParamTags>
  void operator()(Devirtualizer<Fn, ParamTags...> &devirtualizer)
  {
    devirtualizer.execute_materialized();
  }
};

struct AllSpanOrSingle {
  template<typename Fn, typename... ParamTags>
  void operator()(Devirtualizer<Fn, ParamTags...> &devirtualizer)
  {
    if (!devirtualizer.try_execute_devirtualized()) {
      devirtualizer.execute_materialized();
    }
  }
};

template<size_t... SpanIndices> struct SomeSpanOtherSingle {
  template<size_t I> static constexpr bool can_be_span()
  {
    return ((I == SpanIndices) || ...);
  };

  template<size_t... I>
  static constexpr ParamModeSequence<(can_be_span<I>() ? ParamMode::SpanAndSingle :
                                                         ParamMode::Single)...>
      get_modes(std::index_sequence<I...> /* indices */)
  {
    return {};
  }

  template<typename Fn, typename... ParamTags>
  void operator()(Devirtualizer<Fn, ParamTags...> &devirtualizer)
  {
    if (!devirtualizer.template try_execute_devirtualized_custom<MaskMode::Range>(
            get_modes(std::make_index_sequence<sizeof...(ParamTags)>()))) {
      devirtualizer.execute_materialized();
    }
  }
};

template<size_t SpanIndex, MaskMode MaskMode = MaskMode::Range> struct OneSpanOtherSingle {
  template<size_t... I>
  static ParamModeSequence<((I == SpanIndex) ? ParamMode::Span : ParamMode::Single)...> get_modes(
      std::index_sequence<I...> /* indices */)
  {
    return {};
  }

  template<typename Fn, typename... ParamTags>
  void operator()(Devirtualizer<Fn, ParamTags...> &devirtualizer)
  {
    if (!devirtualizer.template try_execute_devirtualized_custom<MaskMode>(
            get_modes(std::make_index_sequence<sizeof...(ParamTags)>()))) {
      devirtualizer.execute_materialized();
    }
  }
};

}  // namespace blender::devirtualize_arrays::presets
