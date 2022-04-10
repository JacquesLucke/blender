/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <tuple>

#include "BLI_virtual_array.hh"

namespace blender {

struct SingleInputTagBase {
};
template<typename T> struct SingleInputTag : public SingleInputTagBase {
};
struct SingleOutputTagBase {
};
template<typename T> struct SingleOutputTag : public SingleOutputTagBase {
};

template<typename T> struct ParamType {
};

template<typename T> struct ParamType<SingleInputTag<T>> {
  using type = VArray<T>;
};

template<typename T> struct ParamType<SingleOutputTag<T>> {
  using type = MutableSpan<T>;
};

template<typename Fn, typename... Args> class ArrayDevirtualizer {
 private:
  using TagsTuple = std::tuple<Args...>;

  Fn fn_;
  IndexMask mask_;
  std::tuple<const typename ParamType<Args>::type *...> params_;

  std::array<bool, sizeof...(Args)> varray_is_span_;
  std::array<bool, sizeof...(Args)> varray_is_single_;

 public:
  ArrayDevirtualizer(Fn fn, const IndexMask *mask, const typename ParamType<Args>::type *...params)
      : fn_(std::move(fn)), mask_(*mask), params_{params...}
  {
    this->init(std::make_index_sequence<sizeof...(Args)>{});
  }

  void execute_fallback()
  {
    this->execute_fallback_impl(std::make_index_sequence<sizeof...(Args)>{});
  }

 private:
  template<size_t... I> void init(std::index_sequence<I...> /* indices */)
  {
    varray_is_span_.fill(false);
    varray_is_single_.fill(false);
    (this->init_param<I>(), ...);
  }

  template<size_t I> void init_param()
  {
    using ParamTag = std::tuple_element_t<I, TagsTuple>;
    if constexpr (std::is_base_of_v<SingleInputTagBase, ParamTag>) {
      const typename ParamType<ParamTag>::type *varray = std::get<I>(params_);
      varray_is_span_[I] = varray->is_span();
      varray_is_single_[I] = varray->is_single();
    }
  }

  template<size_t... I> void execute_fallback_impl(std::index_sequence<I...> /* indices */)
  {
    fn_(mask_, mask_, *std::get<I>(params_)...);
  }
};

}  // namespace blender
