/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_devirtualize_parameters.hh"

namespace blender::devirtualize_parameters::presets {

struct None {
  template<typename Fn, typename... ParamTypes>
  void operator()(Devirtualizer<Fn, ParamTypes...> &devirtualizer)
  {
    UNUSED_VARS(devirtualizer);
  }
};

template<DeviMode Mode> struct AllSame {
  template<typename Fn, typename... ParamTypes>
  void operator()(Devirtualizer<Fn, ParamTypes...> &devirtualizer)
  {
    devirtualizer.try_execute_devirtualized(
        make_value_sequence<DeviMode, Mode, sizeof...(ParamTypes)>());
  }
};

template<DeviMode Mode1, DeviMode Mode2, size_t... Mode1Indices> struct TwoModes {
  static constexpr DeviMode get_devi_mode(size_t I)
  {
    return ((I == Mode1Indices) || ...) ? Mode1 : Mode2;
  }

  template<size_t... I>
  static constexpr DeviModeSequence<get_devi_mode(I)...> get_devi_modes(
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

struct AllSpanOrSingle : public AllSame<DeviMode::Span | DeviMode::Single | DeviMode::Range> {
};

template<size_t... SpanIndices>
struct SomeSpanOtherSingle : public TwoModes<DeviMode::Span, DeviMode::Single, SpanIndices...> {
};

template<size_t SpanIndex>
struct OneSpanOtherSingle : public TwoModes<DeviMode::Span, DeviMode::Single, SpanIndex> {
};

}  // namespace blender::devirtualize_parameters::presets
