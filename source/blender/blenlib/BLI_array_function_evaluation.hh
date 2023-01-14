/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_index_mask.hh"

namespace blender::array_function_evaluation {

/**
 * Executes #element_fn for all indices in the mask with the arguments at that index.
 */
template<typename MaskT, typename... Args, typename ElementFn>
/* Perform additional optimizations on this loop because it is a very hot loop. For example, the
 * math node in geometry nodes is processed here.  */
#if (defined(__GNUC__) && !defined(__clang__))
[[gnu::optimize("-funroll-loops")]] [[gnu::optimize("O3")]]
#endif
inline void
execute_array(ElementFn element_fn,
              MaskT mask,
              /* Use restrict to tell the compiler that pointer inputs do not alias each
               * other. This is important for some compiler optimizations. */
              Args &&__restrict... args)
{
  if constexpr (std::is_integral_v<MaskT>) {
    /* Having this explicit loop is necessary for MSVC to be able to vectorize this. */
    const int64_t end = int64_t(mask);
    for (int64_t i = 0; i < end; i++) {
      element_fn(args[i]...);
    }
  }
  else if constexpr (std::is_same_v<std::decay_t<MaskT>, IndexRange>) {
    /* Having this explicit loop is necessary for MSVC to be able to vectorize this. */
    const int64_t start = mask.start();
    const int64_t end = mask.one_after_last();
    for (int64_t i = start; i < end; i++) {
      element_fn(args[i]...);
    }
  }
  else {
    for (const int64_t i : mask) {
      element_fn(args[i]...);
    }
  }
}

enum class IOType {
  Input,
  Mutable,
  Output,
};

struct InputParam {
  static constexpr IOType io = IOType::Input;
  using value_type = int;

  bool is_single() const;
  bool is_span() const;
  const value_type &get_single() const;
  const value_type *get_span_begin() const;

  void load_to_span(IndexMask mask, value_type *dst) const;
};

struct MutableParam {
  static constexpr IOType io = IOType::Mutable;
  using value_type = int;

  bool is_span() const;
  value_type *get_span_begin() const;

  void load_to_span(IndexMask mask, value_type *dst);
  void relocate_from_span(IndexMask mask, value_type *src);
};

struct OutputParam {
  static constexpr IOType io = IOType::Output;
  using value_type = int;

  bool is_span() const;
  value_type *get_span_begin() const;

  void relocate_from_span(IndexMask mask, value_type *src);
};

template<typename T> struct SingleInput {
  using value_type = T;
  static constexpr IOType io = IOType::Input;

  const T &value;

  bool is_span() const
  {
    return false;
  }

  bool is_single() const
  {
    return true;
  }

  const T &get_single() const
  {
    return value;
  }

  const T *get_span_begin() const
  {
    BLI_assert_unreachable();
    return nullptr;
  }

  void load_to_span(const IndexMask mask, T *dst) const
  {
    BLI_assert_unreachable();
  }
};

template<typename T> struct ArrayOutput {
  using value_type = T;
  static constexpr IOType io = IOType::Output;

  T *ptr;

  bool is_span() const
  {
    return true;
  }

  T *get_span_begin() const
  {
    return this->ptr;
  }

  void relocate_from_span(const IndexMask mask, T *src) const
  {
    for (const int64_t i : IndexRange(mask.size())) {
      T &value = src[i];
      ptr[mask[i]] = std::move(value);
      std::destroy_at(&value);
    }
  }
};

template<typename T> struct ArrayMutable {
  using value_type = T;
  static constexpr IOType io = IOType::Output;

  T *ptr;

  bool is_span() const
  {
    return true;
  }

  T *get_span_begin() const
  {
    return this->ptr;
  }

  void load_to_span(const IndexMask mask, T *dst) const
  {
    for (const int64_t i : IndexRange(mask.size())) {
      dst[i] = std::move(ptr[mask[i]]);
    }
  }

  void relocate_from_span(const IndexMask mask, T *src) const
  {
    for (const int64_t i : IndexRange(mask.size())) {
      T &value = src[i];
      ptr[mask[i]] = std::move(value);
      std::destroy_at(&value);
    }
  }
};

template<typename T> struct GVArrayInput {
  using value_type = T;
  static constexpr IOType io = IOType::Input;

  const GVArrayImpl &varray_impl;
  CommonVArrayInfo varray_info;

  bool is_span() const
  {
    return this->varray_info.type == CommonVArrayInfo::Type::Span;
  }

  bool is_single() const
  {
    return this->varray_info.type == CommonVArrayInfo::Type::Single;
  }

  const T &get_single() const
  {
    BLI_assert(this->is_single());
    return *static_cast<const T *>(this->varray_info.data);
  }

  const T *get_span_begin() const
  {
    BLI_assert(this->is_span());
    return static_cast<const T *>(this->varray_info.data);
  }

  void load_to_span(const IndexMask mask, T *dst) const
  {
    this->varray_impl.materialize_compressed_to_uninitialized(mask, dst);
  }
};

enum class MaterializeArgMode {
  Unknown,
  Single,
  Span,
  Materialized,
};

template<typename Param> struct MaterializeArgInfo {
  MaterializeArgMode mode = MaterializeArgMode::Unknown;
};

/**
 * Executes #element_fn for all indices in #mask. However, instead of processing every element
 * separately, processing happens in chunks. This allows retrieving from input virtual arrays in
 * chunks, which reduces virtual function call overhead.
 */
template<typename ElementFn, size_t... I, typename... Params>
inline void execute_materialized(const ElementFn element_fn,
                                 const IndexMask mask,
                                 std::index_sequence<I...> /* indices */,
                                 Params &&...params)
{

  /* In theory, all elements could be processed in one chunk. However, that has the disadvantage
   * that large temporary arrays are needed. Using small chunks allows using small arrays, which
   * are reused multiple times, which improves cache efficiency. The chunk size also shouldn't be
   * too small, because then overhead of the outer loop over chunks becomes significant again. */
  static constexpr int64_t MaxChunkSize = 64;
  const int64_t mask_size = mask.size();
  const int64_t tmp_buffer_size = std::min(mask_size, MaxChunkSize);

  /* Local buffers that are used to temporarily store values for processing. */
  std::tuple<TypedBuffer<typename Params::value_type, MaxChunkSize>...> temporary_buffers;

  /* Information about every parameter. */
  std::tuple<MaterializeArgInfo<Params>...> args_info;

  (
      /* Setup information for all parameters. */
      [&] {
        /* Use `typedef` instead of `using` to work around a compiler bug. */
        typedef Params Param;
        typedef typename Param::value_type T;
        [[maybe_unused]] MaterializeArgInfo<Params> &arg_info = std::get<I>(args_info);
        const Param &param = params;
        if constexpr (Param::io == IOType::Input) {
          if (param.is_single()) {
            const T &in_single = param.get_single();
            T *tmp_buffer = std::get<I>(temporary_buffers).ptr();
            uninitialized_fill_n(tmp_buffer, tmp_buffer_size, in_single);
            arg_info.mode = MaterializeArgMode::Single;
          }
        }
      }(),
      ...);

  /* Outer loop over all chunks. */
  for (int64_t chunk_start = 0; chunk_start < mask_size; chunk_start += MaxChunkSize) {
    const int64_t chunk_end = std::min<int64_t>(chunk_start + MaxChunkSize, mask_size);
    const int64_t chunk_size = chunk_end - chunk_start;
    const IndexMask sliced_mask = mask.slice(chunk_start, chunk_size);
    const int64_t mask_start = sliced_mask[0];
    const bool sliced_mask_is_range = sliced_mask.is_range();

    /* Move mutable data into temporary array. */
    (
        [&] {
          /* Use `typedef` instead of `using` to work around a compiler bug. */
          typedef Params Param;
          typedef typename Param::value_type T;
          if constexpr (Param::io == IOType::Mutable) {
            Param &param = params;
            if (!sliced_mask_is_range || !param.is_span()) {
              T *tmp_buffer = std::get<I>(temporary_buffers).ptr();
              param.load_to_span(sliced_mask, tmp_buffer);
            }
          }
        }(),
        ...);

    array_function_evaluation::execute_array(
        element_fn,
        chunk_size,
        /* Prepare every parameter for this chunk. */
        [&] {
          using Param = Params;
          using T = typename Param::value_type;
          [[maybe_unused]] MaterializeArgInfo<Params> &arg_info = std::get<I>(args_info);
          T *tmp_buffer = std::get<I>(temporary_buffers);
          const Param &param = params;
          if constexpr (Param::io == IOType::Input) {
            if (arg_info.mode == MaterializeArgMode::Single) {
              /* The single value has been filled into a buffer already reused for every chunk. */
              return const_cast<const T *>(tmp_buffer);
            }
            if (sliced_mask_is_range && param.is_span()) {
              /* In this case we can just use an existing span instead of "compressing" it into
               * a new temporary buffer. */
              arg_info.mode = MaterializeArgMode::Span;
              return param.get_span_begin() + mask_start;
            }

            param.load_to_span(sliced_mask, tmp_buffer);
            /* Remember that this parameter has been materialized, so that the values are
             * destructed properly when the chunk is done. */
            arg_info.mode = MaterializeArgMode::Materialized;
            return const_cast<const T *>(tmp_buffer);
          }
          else {
            /* For outputs, just pass a pointer. This is important so that `__restrict` works. */
            if (sliced_mask_is_range && param.is_span()) {
              return param.get_span_begin() + mask_start;
            }
            else {
              /* Use the temporary buffer. The values will have to be copied out of that
               * buffer into the caller-provided buffer afterwards. */
              return const_cast<T *>(tmp_buffer);
            }
          }
        }()...);

    (
        [&] {
          /* Use `typedef` instead of `using` to work around a compiler bug. */
          typedef Params Param;
          typedef typename Param::value_type T;
          if constexpr (Param::io == IOType::Input) {
            MaterializeArgInfo<Params> &arg_info = std::get<I>(args_info);
            /* Destruct non-single materialized inputs. */
            if (arg_info.mode == MaterializeArgMode::Materialized) {
              T *tmp_buffer = std::get<I>(temporary_buffers).ptr();
              destruct_n(tmp_buffer, chunk_size);
            }
          }
          else {
            Param &param = params;
            if (!sliced_mask_is_range || !param.is_span()) {
              /* Relocate outputs from temporary buffers to buffers provided by caller. */
              T *tmp_buffer = std::get<I>(temporary_buffers).ptr();
              param.relocate_from_span(sliced_mask, tmp_buffer);
            }
          }
        }(),
        ...);
  }

  (
      [&] {
        /* Use `typedef` instead of `using` to work around a compiler bug. */
        typedef Params Param;
        typedef typename Param::value_type T;
        if constexpr (Param::io == IOType::Input) {
          /* Destruct buffers for single value inputs. */
          MaterializeArgInfo<Params> &arg_info = std::get<I>(args_info);
          if (arg_info.mode == MaterializeArgMode::Single) {
            T *tmp_buffer = std::get<I>(temporary_buffers).ptr();
            destruct_n(tmp_buffer, tmp_buffer_size);
          }
        }
      }(),
      ...);
}

}  // namespace blender::array_function_evaluation
