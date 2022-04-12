/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <tuple>

#include "BLI_virtual_array.hh"

namespace blender::devirtualize_arrays {

namespace tags {

struct Input {
};

struct Output {
};

template<typename T> struct InVArray : public Input {
  using BaseType = T;
  using ArrayType = VArray<T>;
};

template<typename T> struct OutSpan : public Output {
  using BaseType = T;
  using ArrayType = MutableSpan<T>;
};

}  // namespace tags

enum class ParamMode {
  None = 0,
  Span = (1 << 0),
  Single = (1 << 1),
  VArray = (1 << 2),
  SpanAndSingle = Span | Single,
};
ENUM_OPERATORS(ParamMode, ParamMode::VArray);

enum class MaskMode {
  None = 0,
  Mask = (1 << 0),
  Range = (1 << 1),
  MaskAndRange = (Mask | Range),
};
ENUM_OPERATORS(MaskMode, MaskMode::Range);

template<ParamMode... Mode> using ParamModeSequence = ValueSequence<ParamMode, Mode...>;

template<typename Fn, typename... ParamTags> class Devirtualizer {
 private:
  template<size_t I> using tag_at_index = typename TypeSequence<ParamTags...>::at_index<I>;

  Fn fn_;
  IndexMask mask_;
  std::tuple<const typename ParamTags::ArrayType *...> params_;

  std::array<bool, sizeof...(ParamTags)> varray_is_span_;
  std::array<bool, sizeof...(ParamTags)> varray_is_single_;

  bool executed_ = false;

 public:
  Devirtualizer(Fn fn, const IndexMask *mask, const typename ParamTags::ArrayType *...params)
      : fn_(std::move(fn)), mask_(*mask), params_{params...}
  {
    this->init(std::make_index_sequence<sizeof...(ParamTags)>{});
  }

  void execute_fallback()
  {
    BLI_assert(!executed_);
    this->execute_fallback_impl(std::make_index_sequence<sizeof...(ParamTags)>{});
  }

  bool try_execute_devirtualized()
  {
    BLI_assert(!executed_);
    return this->try_execute_devirtualized_custom<MaskMode::MaskAndRange>(
        make_value_sequence<ParamMode,
                            ParamMode::Span | ParamMode::Single,
                            sizeof...(ParamTags)>());
  }

  template<MaskMode MaskMode, ParamMode... AllowedModes>
  bool try_execute_devirtualized_custom(ParamModeSequence<AllowedModes...> /* allowed_modes */)
  {
    BLI_assert(!executed_);
    static_assert(sizeof...(AllowedModes) == sizeof...(ParamTags));
    return this->try_execute_devirtualized_impl<MaskMode>(ParamModeSequence<>(),
                                                          ParamModeSequence<AllowedModes...>());
  }

  void execute_materialized()
  {
    BLI_assert(!executed_);
    this->execute_materialized_impl(std::make_index_sequence<sizeof...(ParamTags)>{});
  }

 private:
  template<size_t... I> void execute_materialized_impl(std::index_sequence<I...> /* indices */)
  {
    static constexpr int64_t MaxChunkSize = 32;
    const int64_t mask_size = mask_.size();
    std::tuple<TypedBuffer<typename tag_at_index<I>::BaseType, MaxChunkSize>...> buffers_owner;
    std::tuple<MutableSpan<typename tag_at_index<I>::BaseType>...> buffers = {
        MutableSpan{std::get<I>(buffers_owner).ptr(), std::min(mask_size, MaxChunkSize)}...};

    (
        [&]() {
          using ParamTag = tag_at_index<I>;
          using T = typename ParamTag::BaseType;
          if constexpr (std::is_base_of_v<tags::Input, ParamTag>) {
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
            using ParamTag = tag_at_index<I>;
            using T = typename ParamTag::BaseType;
            if constexpr (std::is_base_of_v<tags::Input, ParamTag>) {
              if (!varray_is_single_[I]) {
                MutableSpan in_chunk = std::get<I>(buffers).take_front(sliced_mask_size);
                const VArray<T> *varray = std::get<I>(params_);
                varray->materialize_compressed_to_uninitialized(sliced_mask, in_chunk);
              }
            }
          }(),
          ...);

      fn_(IndexRange(sliced_mask_size), sliced_mask, [&]() {
        using ParamTag = tag_at_index<I>;
        using T = typename ParamTag::BaseType;
        if constexpr (std::is_base_of_v<tags::Input, ParamTag>) {
          Span<T> in_chunk = std::get<I>(buffers).take_front(sliced_mask_size);
          return in_chunk;
        }
        else if constexpr (std::is_base_of_v<tags::Output, ParamTag>) {
          MutableSpan<T> out_span = *std::get<I>(params_);
          return out_span.data();
        }
      }()...);

      (
          [&]() {
            using ParamTag = tag_at_index<I>;
            using T = typename ParamTag::BaseType;
            if constexpr (std::is_base_of_v<tags::Input, ParamTag>) {
              MutableSpan<T> in_chunk = std::get<I>(buffers);
              destruct_n(in_chunk.data(), sliced_mask_size);
            }
          }(),
          ...);
    }
  }

  template<MaskMode MaskMode, ParamMode... Mode, ParamMode... AllowedModes>
  bool try_execute_devirtualized_impl(ParamModeSequence<Mode...> /* modes */,
                                      ParamModeSequence<AllowedModes...> /* allowed_modes */)
  {
    static_assert(sizeof...(AllowedModes) == sizeof...(ParamTags));
    if constexpr (sizeof...(Mode) == sizeof...(ParamTags)) {
      return this->try_execute_devirtualized_impl_call<MaskMode>(
          ParamModeSequence<Mode...>(), std::make_index_sequence<sizeof...(ParamTags)>());
    }
    else {
      constexpr size_t I = sizeof...(Mode);
      using ParamTag = tag_at_index<I>;
      if constexpr (std::is_base_of_v<tags::Input, ParamTag>) {
        constexpr ParamMode allowed_modes =
            ParamModeSequence<AllowedModes...>::template at_index<I>();
        if constexpr ((allowed_modes & ParamMode::Single) != ParamMode::None) {
          if (varray_is_single_[I]) {
            return this->try_execute_devirtualized_impl<MaskMode>(
                ParamModeSequence<Mode..., ParamMode::Single>(),
                ParamModeSequence<AllowedModes...>());
          }
        }
        if constexpr ((allowed_modes & ParamMode::Span) != ParamMode::None) {
          if (varray_is_span_[I]) {
            return this->try_execute_devirtualized_impl<MaskMode>(
                ParamModeSequence<Mode..., ParamMode::Span>(),
                ParamModeSequence<AllowedModes...>());
          }
        }
        if constexpr ((allowed_modes & ParamMode::VArray) != ParamMode::None) {
          return this->try_execute_devirtualized_impl<MaskMode>(
              ParamModeSequence<Mode..., ParamMode::VArray>(),
              ParamModeSequence<AllowedModes...>());
        }
        return false;
      }
      else {
        return this->try_execute_devirtualized_impl<MaskMode>(
            ParamModeSequence<Mode..., ParamMode::None>(), ParamModeSequence<AllowedModes...>());
      }
    }
  }

  template<MaskMode MaskMode, ParamMode... Mode, size_t... I>
  bool try_execute_devirtualized_impl_call(ParamModeSequence<Mode...> /* modes */,
                                           std::index_sequence<I...> /* indices */)
  {
    if constexpr ((MaskMode & MaskMode::Range) != MaskMode::None) {
      if (mask_.is_range()) {
        const IndexRange mask_range = mask_.as_range();
        fn_(mask_range, mask_range, this->get_execute_param<I, Mode>()...);
        return true;
      }
    }
    if constexpr ((MaskMode & MaskMode::Mask) != MaskMode::None) {
      fn_(mask_, mask_, this->get_execute_param<I, Mode>()...);
      return true;
    }
    return false;
  }

  template<size_t... I> void init(std::index_sequence<I...> /* indices */)
  {
    varray_is_span_.fill(false);
    varray_is_single_.fill(false);
    (this->init_param<I>(), ...);
  }

  template<size_t I> void init_param()
  {
    using ParamTag = tag_at_index<I>;
    if constexpr (std::is_base_of_v<tags::Input, ParamTag>) {
      const typename ParamTag::ArrayType *varray = std::get<I>(params_);
      varray_is_span_[I] = varray->is_span();
      varray_is_single_[I] = varray->is_single();
    }
  }

  template<size_t... I> void execute_fallback_impl(std::index_sequence<I...> /* indices */)
  {
    fn_(mask_, mask_, this->get_execute_param<I, ParamMode::None>()...);
    executed_ = true;
  }

  template<size_t I, ParamMode Mode> decltype(auto) get_execute_param()
  {
    using ParamTag = tag_at_index<I>;
    if constexpr (std::is_base_of_v<tags::Input, ParamTag>) {
      using T = typename ParamTag::BaseType;
      const VArray<T> *varray = std::get<I>(params_);
      if constexpr (ELEM(Mode, ParamMode::None, ParamMode::VArray)) {
        return *varray;
      }
      else if constexpr (Mode == ParamMode::Single) {
        return SingleAsSpan(*varray);
      }
      else if constexpr (Mode == ParamMode::Span) {
        return varray->get_internal_span();
      }
    }
    else if constexpr (std::is_base_of_v<tags::Output, ParamTag>) {
      return std::get<I>(params_)->data();
    }
  }
};

template<typename ElementFn, typename... ParamTags> struct ElementFnExecutor {
  template<size_t I> using tag_at_index = typename TypeSequence<ParamTags...>::at_index<I>;

  ElementFn element_fn;

  template<typename InIndices, typename OutIndices, size_t... I, typename... Args>
  void execute_element_fn(InIndices in_indices,
                          OutIndices out_indices,
                          std::index_sequence<I...> /* indices */,
                          Args &&__restrict... args)
  {
    BLI_assert(in_indices.size() == out_indices.size());
    for (const int64_t i : IndexRange(in_indices.size())) {
      const int64_t in_index = in_indices[i];
      const int64_t out_index = out_indices[i];
      element_fn([&]() -> decltype(auto) {
        using ParamTag = tag_at_index<I>;
        if constexpr (std::is_base_of_v<tags::Input, ParamTag>) {
          return args[in_index];
        }
        else {
          return args + out_index;
        }
      }()...);
    }
  }

  template<typename InIndices, typename OutIndices, typename... Args>
  void operator()(InIndices in_indices, OutIndices out_indices, Args &&...args)
  {
    this->execute_element_fn(in_indices,
                             out_indices,
                             std::make_index_sequence<sizeof...(ParamTags)>(),
                             std::forward<Args>(args)...);
  }
};

template<typename ElementFn, typename... ParamTags>
Devirtualizer<ElementFnExecutor<ElementFn, ParamTags...>, ParamTags...>
devirtualizer_from_element_fn(ElementFn element_fn,
                              const IndexMask *mask,
                              const typename ParamTags::ArrayType *...params)
{
  ElementFnExecutor<ElementFn, ParamTags...> executor{element_fn};
  return Devirtualizer<ElementFnExecutor<ElementFn, ParamTags...>, ParamTags...>{
      executor, mask, params...};
}

}  // namespace blender::devirtualize_arrays
