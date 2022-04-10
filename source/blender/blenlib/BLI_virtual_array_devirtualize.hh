/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <tuple>

#include "BLI_virtual_array.hh"

namespace blender {

struct SingleInputTagBase {
};
template<typename T> struct SingleInputTag : public SingleInputTagBase {
  using BaseType = T;
};
struct SingleOutputTagBase {
};
template<typename T> struct SingleOutputTag : public SingleOutputTagBase {
  using BaseType = T;
};

template<typename T> struct ParamType {
};

template<typename T> struct ParamType<SingleInputTag<T>> {
  using type = VArray<T>;
};

template<typename T> struct ParamType<SingleOutputTag<T>> {
  using type = MutableSpan<T>;
};

struct DevirtualizeNone {
};
struct DevirtualizeSpan {
};
struct DevirtualizeSingle {
};

template<typename Fn, typename... Args> class ArrayDevirtualizer {
 private:
  using TagsTuple = std::tuple<Args...>;

  Fn fn_;
  IndexMask mask_;
  std::tuple<const typename ParamType<Args>::type *...> params_;

  std::array<bool, sizeof...(Args)> varray_is_span_;
  std::array<bool, sizeof...(Args)> varray_is_single_;

  bool executed_ = false;

 public:
  ArrayDevirtualizer(Fn fn, const IndexMask *mask, const typename ParamType<Args>::type *...params)
      : fn_(std::move(fn)), mask_(*mask), params_{params...}
  {
    this->init(std::make_index_sequence<sizeof...(Args)>{});
  }

  void execute_fallback()
  {
    BLI_assert(!executed_);
    this->execute_fallback_impl(std::make_index_sequence<sizeof...(Args)>{});
  }

  bool try_execute_devirtualized()
  {
    BLI_assert(!executed_);
    return this->try_execute_devirtualized_impl();
  }

 private:
  template<typename... Mode> bool try_execute_devirtualized_impl()
  {
    if constexpr (sizeof...(Mode) == sizeof...(Args)) {
      this->try_execute_devirtualized_impl_call(std::tuple<Mode...>(),
                                                std::make_index_sequence<sizeof...(Args)>());
      return true;
    }
    else {
      constexpr size_t I = sizeof...(Mode);
      using ParamTag = std::tuple_element_t<I, TagsTuple>;
      if constexpr (std::is_base_of_v<SingleInputTagBase, ParamTag>) {
        if (varray_is_single_[I]) {
          return this->try_execute_devirtualized_impl<Mode..., DevirtualizeSingle>();
        }
        else if (varray_is_span_[I]) {
          return this->try_execute_devirtualized_impl<Mode..., DevirtualizeSpan>();
        }
        else {
          return false;
        }
      }
      else {
        return this->try_execute_devirtualized_impl<Mode..., DevirtualizeNone>();
      }
    }
  }

  template<typename... Mode, size_t... I>
  void try_execute_devirtualized_impl_call(std::tuple<Mode...> /* modes */,
                                           std::index_sequence<I...> /* indices */)
  {
    mask_.to_best_mask_type([&](auto mask) {
      fn_(mask,
          mask,
          this->get_execute_param<I, std::tuple_element_t<I, std::tuple<Mode...>>>()...);
    });
    executed_ = true;
  }

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
    fn_(mask_, mask_, this->get_execute_param<I, DevirtualizeNone>()...);
    executed_ = true;
  }

  template<size_t I, typename Mode> auto get_execute_param()
  {
    using ParamTag = std::tuple_element_t<I, TagsTuple>;
    if constexpr (std::is_base_of_v<SingleInputTagBase, ParamTag>) {
      using T = typename ParamTag::BaseType;
      const VArray<T> *varray = std::get<I>(params_);
      if constexpr (std::is_same_v<Mode, DevirtualizeNone>) {
        return *varray;
      }
      else if constexpr (std::is_same_v<Mode, DevirtualizeSingle>) {
        return SingleAsSpan(*varray);
      }
      else if constexpr (std::is_same_v<Mode, DevirtualizeSpan>) {
        return varray->get_internal_span();
      }
    }
    else if constexpr (std::is_base_of_v<SingleOutputTagBase, ParamTag>) {
      return std::get<I>(params_)->data();
    }
  }
};

}  // namespace blender
