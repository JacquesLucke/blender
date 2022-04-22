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
  template<typename Fn, typename... ParamTypes>
  void operator()(Devirtualizer<Fn, ParamTypes...> &devirtualizer)
  {
    devirtualizer.template try_execute_devirtualized(
        make_two_value_sequence<DeviMode, Mode1, Mode2, sizeof...(ParamTypes), Mode1Indices...>());
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
