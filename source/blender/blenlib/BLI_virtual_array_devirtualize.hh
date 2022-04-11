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

enum class DevirtualizeMode {
  None = 0,
  Span = (1 << 0),
  Single = (1 << 1),
};
ENUM_OPERATORS(DevirtualizeMode, DevirtualizeMode::Single);

template<DevirtualizeMode... Mode>
using DevirtualizeModeSequence = EnumSequence<DevirtualizeMode, Mode...>;

template<typename TagsTuple, size_t I>
using BaseType = typename std::tuple_element_t<I, TagsTuple>::BaseType;

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
    return this->try_execute_devirtualized_impl(
        DevirtualizeModeSequence<>(),
        make_enum_sequence<DevirtualizeMode,
                           DevirtualizeMode::Span | DevirtualizeMode::Single,
                           sizeof...(Args)>());
  }

  void execute_materialized()
  {
    BLI_assert(!executed_);
    this->execute_materialized_impl(std::make_index_sequence<sizeof...(Args)>{});
  }

 private:
  template<size_t... I> void execute_materialized_impl(std::index_sequence<I...> /* indices */)
  {
    static constexpr int64_t MaxChunkSize = 32;
    const int64_t mask_size = mask_.size();
    std::tuple<TypedBuffer<BaseType<TagsTuple, I>, MaxChunkSize>...> buffers_owner;
    std::tuple<MutableSpan<BaseType<TagsTuple, I>>...> buffers = {
        MutableSpan{std::get<I>(buffers_owner).ptr(), std::min(mask_size, MaxChunkSize)}...};

    (
        [&]() {
          using ParamTag = std::tuple_element_t<I, TagsTuple>;
          using T = typename ParamTag::BaseType;
          if constexpr (std::is_base_of_v<SingleInputTagBase, ParamTag>) {
            MutableSpan in_chunk = std::get<I>(buffers);
            if (varray_is_single_[I]) {
              const VArray<T> *varray = std::get<I>(params_);
              const T in_single = varray->get_internal_single();
              in_chunk.fill(in_single);
            }
          }
        }(),
        ...);

    for (int64_t chunk_start = 0; chunk_start < mask_size; chunk_start += MaxChunkSize) {
      const int64_t chunk_size = std::min(mask_size - chunk_start, MaxChunkSize);
      const IndexMask sliced_mask = mask_.slice(chunk_start, chunk_size);
      const int64_t sliced_mask_size = sliced_mask.size();
      (
          [&]() {
            using ParamTag = std::tuple_element_t<I, TagsTuple>;
            using T = typename ParamTag::BaseType;
            if constexpr (std::is_base_of_v<SingleInputTagBase, ParamTag>) {
              if (!varray_is_single_[I]) {
                MutableSpan in_chunk = std::get<I>(buffers).take_front(sliced_mask_size);
                const VArray<T> *varray = std::get<I>(params_);
                varray->materialize_compressed_to_uninitialized(sliced_mask, in_chunk);
              }
            }
          }(),
          ...);

      fn_(IndexRange(sliced_mask_size), sliced_mask, [&]() {
        using ParamTag = std::tuple_element_t<I, TagsTuple>;
        using T = typename ParamTag::BaseType;
        if constexpr (std::is_base_of_v<SingleInputTagBase, ParamTag>) {
          MutableSpan<T> in_chunk = std::get<I>(buffers).take_front(sliced_mask_size);
          return in_chunk;
        }
        else if constexpr (std::is_base_of_v<SingleOutputTagBase, ParamTag>) {
          MutableSpan<T> out_span = *std::get<I>(params_);
          return out_span.data();
        }
      }()...);

      (
          [&]() {
            using ParamTag = std::tuple_element_t<I, TagsTuple>;
            using T = typename ParamTag::BaseType;
            if constexpr (std::is_base_of_v<SingleInputTagBase, ParamTag>) {
              MutableSpan<T> in_chunk = std::get<I>(buffers);
              destruct_n(in_chunk.data(), sliced_mask_size);
            }
          }(),
          ...);
    }
  }

  template<DevirtualizeMode... Mode>
  bool try_execute_devirtualized_impl(DevirtualizeModeSequence<Mode...> /* modes */,
                                      DevirtualizeModeSequence<> /* allowed_modes */)
  {
    static_assert(sizeof...(Mode) == sizeof...(Args));
    this->try_execute_devirtualized_impl_call(DevirtualizeModeSequence<Mode...>(),
                                              std::make_index_sequence<sizeof...(Args)>());
    return true;
  }

  template<DevirtualizeMode... Mode,
           DevirtualizeMode AllowedModes,
           DevirtualizeMode... RemainingAllowedModes>
  bool try_execute_devirtualized_impl(
      DevirtualizeModeSequence<Mode...> /* modes */,
      DevirtualizeModeSequence<AllowedModes, RemainingAllowedModes...> /* allowed_modes */)
  {
    static_assert(sizeof...(Mode) + sizeof...(RemainingAllowedModes) + 1 == sizeof...(Args));

    constexpr size_t I = sizeof...(Mode);
    using ParamTag = std::tuple_element_t<I, TagsTuple>;
    if constexpr (std::is_base_of_v<SingleInputTagBase, ParamTag>) {
      if constexpr ((AllowedModes & DevirtualizeMode::Single) != DevirtualizeMode::None) {
        if (varray_is_single_[I]) {
          return this->try_execute_devirtualized_impl(
              DevirtualizeModeSequence<Mode..., DevirtualizeMode::Single>(),
              DevirtualizeModeSequence<RemainingAllowedModes...>());
        }
      }
      if constexpr ((AllowedModes & DevirtualizeMode::Span) != DevirtualizeMode::None) {
        if (varray_is_span_[I]) {
          return this->try_execute_devirtualized_impl(
              DevirtualizeModeSequence<Mode..., DevirtualizeMode::Span>(),
              DevirtualizeModeSequence<RemainingAllowedModes...>());
        }
      }
      return false;
    }
    else {
      return this->try_execute_devirtualized_impl(
          DevirtualizeModeSequence<Mode..., DevirtualizeMode::None>(),
          DevirtualizeModeSequence<RemainingAllowedModes...>());
    }
  }

  template<DevirtualizeMode... Mode, size_t... I>
  void try_execute_devirtualized_impl_call(DevirtualizeModeSequence<Mode...> /* modes */,
                                           std::index_sequence<I...> /* indices */)
  {
    mask_.to_best_mask_type(
        [&](auto mask) { fn_(mask, mask, this->get_execute_param<I, Mode>()...); });
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
    fn_(mask_, mask_, this->get_execute_param<I, DevirtualizeMode::None>()...);
    executed_ = true;
  }

  template<size_t I, DevirtualizeMode Mode> auto get_execute_param()
  {
    using ParamTag = std::tuple_element_t<I, TagsTuple>;
    if constexpr (std::is_base_of_v<SingleInputTagBase, ParamTag>) {
      using T = typename ParamTag::BaseType;
      const VArray<T> *varray = std::get<I>(params_);
      if constexpr (Mode == DevirtualizeMode::None) {
        return *varray;
      }
      else if constexpr (Mode == DevirtualizeMode::Single) {
        return SingleAsSpan(*varray);
      }
      else if constexpr (Mode == DevirtualizeMode::Span) {
        return varray->get_internal_span();
      }
    }
    else if constexpr (std::is_base_of_v<SingleOutputTagBase, ParamTag>) {
      return std::get<I>(params_)->data();
    }
  }
};

}  // namespace blender
