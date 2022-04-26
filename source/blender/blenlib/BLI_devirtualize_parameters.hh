/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * In geometry nodes, many functions accept fields as inputs. For the implementation that means
 * that the inputs are virtual arrays. Usually those are backed by actual arrays or single values
 * but sometimes virtual arrays are used to compute values on demand or convert between data
 * formats.
 *
 * Using virtual arrays has the downside that individual elements are accessed through a virtual
 * method call, which has some overhead compared to normal array access. Whether this overhead is
 * negilible depends on the context. For very small functions (e.g. a single addition), the
 * overhead can make the function many times slower. Furthermore, it prevents the compiler from
 * doing some optimizations (e.g. loop unrolling and inserting SIMD instructions).
 *
 * The solution is to "devirtualize" the virtual arrays in cases when the overhead cannot be
 * ignored. That means that the function is instantiated multiple times at compile time for the
 * different cases. For example, there can be an optimized function that adds a span and a single
 * value, and another function that adds a span and another span. At run-time there is a dynamic
 * dispatch that executes the best function given the specific virtual arrays.
 *
 * The problem with this devirtualization is that it can result in exponentially increasing compile
 * times and binary sizes, depending on the number of parameters that are devirtualized separately.
 * So there is always a trade-off between run-time performance and compile-time/binary-size.
 *
 * This file provides a utility to devirtualize array parameters to a function using a high level
 * API. This makes it easy to experiment with different extremes of the mentioned trade-off and
 * allows finding a good compromise for each function.
 */

#include "BLI_parameter_pack_utils.hh"
#include "BLI_virtual_array.hh"

namespace blender::devirtualize_parameters {

template<bool AllowMask, bool AllowRange> struct DispatchIndexMask {
  template<typename Fn> static bool dispatch(const IndexMask &mask, const Fn &fn)
  {
    if constexpr (AllowRange) {
      if (mask.is_range()) {
        if (fn([](const IndexMask &mask) -> IndexRange { return mask.as_range(); })) {
          return true;
        }
      }
    }
    if constexpr (AllowMask) {
      if (fn([](const IndexMask &mask) -> const IndexMask & { return mask; })) {
        return true;
      }
    }
    return false;
  }
};

template<bool AllowSingle, bool AllowSpan> struct DispatchVArray {
  template<typename T, typename Fn> static bool dispatch(const VArray<T> &varray, const Fn &fn)
  {
    if constexpr (AllowSingle) {
      if (varray.is_single()) {
        if (fn([](const VArray<T> &varray) -> SingleAsSpan<T> { return SingleAsSpan(varray); })) {
          return true;
        }
      }
    }
    if constexpr (AllowSpan) {
      if (varray.is_span()) {
        if (fn([](const VArray<T> &varray) -> Span<T> { return varray.get_internal_span(); })) {
          return true;
        }
      }
    }
    return false;
  }
};

struct DispatchKeep {
  template<typename T, typename Fn> static bool dispatch(const T &value, const Fn &fn)
  {
    return fn([](const T &value) -> const T & { return value; });
  }
};

/**
 * Main class that performs the devirtualization.
 */
template<typename Fn, typename... SourceTypes> class Devirtualizer {
 private:
  /** Utility to get the tag of the I-th source type. */
  template<size_t I>
  using type_at_index = typename TypeSequence<SourceTypes...>::template at_index<I>;
  static constexpr size_t SourceTypesNum = sizeof...(SourceTypes);

  /** Function to devirtualize. */
  Fn fn_;

  /**
   * Source values that will be devirtualized. Note that these are stored as pointers to avoid
   * unnecessary copies. The caller is responsible for keeping the memory alive.
   */
  std::tuple<const SourceTypes *...> sources_;

  /** Keeps track of whether #fn_ has been called already to avoid calling it twice. */
  bool executed_ = false;

 public:
  Devirtualizer(Fn fn, const SourceTypes *...sources) : fn_(std::move(fn)), sources_{sources...}
  {
  }

  /**
   * Return true when the function passed to the constructor has been called already.
   */
  bool executed() const
  {
    return executed_;
  }

  /**
   * At compile time, generates multiple variants of the function, each optimized for a different
   * combination of devirtualized parameters. For every parameter, a bit flag is passed that
   * determines how it will be devirtualized. At run-time, if possible, one of the generated
   * functions is picked and executed.
   *
   * To check whether the function was called successfully, call #executed() afterwards.
   *
   * \note This generates an exponential amount of code in the final binary, depending on how many
   * to-be-virtualized parameters there are.
   */
  template<typename... Dispatchers>
  void try_execute_devirtualized(TypeSequence<Dispatchers...> /* dispatchers */)
  {
    BLI_assert(!executed_);
    static_assert(sizeof...(Dispatchers) == SourceTypesNum);
    this->try_execute_devirtualized_impl(std::make_tuple(), TypeSequence<Dispatchers...>());
  }

  /**
   * Execute the function and pass in the original parameters without doing any devirtualization.
   */
  void execute_without_devirtualization()
  {
    BLI_assert(!executed_);
    this->try_execute_devirtualized(make_type_sequence<DispatchKeep, SourceTypesNum>());
  }

 private:
  /**
   * A recursive method that generates all the combinations of devirtualized parameters that the
   * caller requested. A recursive function is necessary to achieve generating an exponential
   * number of function calls (which has to be used with care, but is expected here).
   *
   * At every recursive step, the #DeviMode of one parameter is determined. This is achieved by
   * extending #DeviModeSequence<Mode...> by one element in each step. The recursion ends once all
   * parameters are handled.
   *
   * \return True when the function has been executed.
   */
  template<typename... Converters, typename... Dispatchers>
  bool try_execute_devirtualized_impl(
      /* Initially empty, but then extended by one element in each recursive step.  */
      const std::tuple<Converters...> &converters,
      /* Bit flag for every parameter. */
      TypeSequence<Dispatchers...> /* dispatchers */)
  {
    static_assert(SourceTypesNum == sizeof...(Dispatchers));
    if constexpr (SourceTypesNum == sizeof...(Converters)) {
      /* End of recursion, now call the function with the determined #DeviModes. */
      this->execute_devirtualized_impl_call(converters,
                                            std::make_index_sequence<SourceTypesNum>());
      return true;
    }
    else {
      /* Index of the parameter that is checked in the current recursive step. */
      constexpr size_t I = sizeof...(Converters);
      /* Non-devirtualized parameter type. */
      using SourceType = type_at_index<I>;
      using Dispatcher = typename TypeSequence<Dispatchers...>::template at_index<I>;
      const SourceType &source_value = *std::get<I>(sources_);
      return Dispatcher::dispatch(source_value, [&](auto converter) {
        return this->try_execute_devirtualized_impl(
            std::tuple_cat(converters, std::make_tuple(converter)),
            TypeSequence<Dispatchers...>());
      });
    }
  }

  template<typename... Converters, size_t... I>
  void execute_devirtualized_impl_call(const std::tuple<Converters...> converters,
                                       std::index_sequence<I...> /* indices */)
  {
    BLI_assert(!executed_);
    fn_(std::get<I>(converters)(*std::get<I>(sources_))...);
    executed_ = true;
  }
};

}  // namespace blender::devirtualize_parameters

namespace blender {

/**
 * Generate multiple versions of the given function optimized for different virtual arrays.
 * One has to be careful with nesting multiple devirtualizations, because that results in an
 * exponential number of function instantiations (increasing compile time and binary size).
 *
 * Generally, this function should only be used when the virtual method call overhead to get an
 * element from a virtual array is significant.
 */
template<typename T, typename Func>
inline void devirtualize_varray(const VArray<T> &varray, const Func &func, bool enable = true)
{
  using namespace devirtualize_parameters;
  if (enable) {
    Devirtualizer<decltype(func), VArray<T>> devirtualizer(func, &varray);
    devirtualizer.try_execute_devirtualized(TypeSequence<DispatchVArray<true, true>>());
    if (devirtualizer.executed()) {
      return;
    }
  }
  func(varray);
}

/**
 * Same as `devirtualize_varray`, but devirtualizes two virtual arrays at the same time.
 * This is better than nesting two calls to `devirtualize_varray`, because it instantiates fewer
 * cases.
 */
template<typename T1, typename T2, typename Func>
inline void devirtualize_varray2(const VArray<T1> &varray1,
                                 const VArray<T2> &varray2,
                                 const Func &func,
                                 bool enable = true)
{
  using namespace devirtualize_parameters;
  if (enable) {
    Devirtualizer<decltype(func), VArray<T1>, VArray<T2>> devirtualizer(func, &varray1, &varray2);
    devirtualizer.try_execute_devirtualized(
        TypeSequence<DispatchVArray<true, true>, DispatchVArray<true, true>>());
    if (devirtualizer.executed()) {
      return;
    }
  }
  func(varray1, varray2);
}

}  // namespace blender
