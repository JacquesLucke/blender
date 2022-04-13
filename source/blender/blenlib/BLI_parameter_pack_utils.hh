/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * C++ has a feature called "parameter packs" which allow building variadic templates.
 * This file has some utilities to work with such parameter packs.
 */

#include <tuple>

#include "BLI_utildefines.h"

namespace blender {

/**
 * A type that encodes a specific value.
 */
template<typename T, T Element> struct TypeForValue {
  static constexpr T value = Element;
};

/**
 * A type that encodes a list of values of the same type.
 * This is similar to #std::integer_sequence, but a bit more general. It's main purpose it to also
 * support enums instead of just ints.
 */
template<typename T, T... Elements> struct ValueSequence {
  /**
   * Get the number of elements in the sequence.
   */
  static constexpr size_t size() noexcept
  {
    return sizeof...(Elements);
  }

  /**
   * Get the element at a specific index.
   */
  template<size_t I> static constexpr T at_index()
  {
    static_assert(I < sizeof...(Elements));
    return std::tuple_element_t<I, std::tuple<TypeForValue<T, Elements>...>>::value;
  }
};

/**
 * A type that encodes a list of types.
 * #std::tuple can also encode a list of types, but has a much more complex implementation.
 */
template<typename... T> struct TypeSequence {
  /**
   * Get the number of types in the sequence.
   */
  static constexpr size_t size() noexcept
  {
    return sizeof...(T);
  }

  /**
   * Get the type at a specific index.
   */
  template<size_t I> using at_index = std::tuple_element_t<I, std::tuple<T...>>;
};

namespace detail {

template<typename T, T Element, size_t... I>
ValueSequence<T, ((I == 0) ? Element : Element)...> make_value_sequence_impl(
    std::index_sequence<I...> /* indices */)
{
  return {};
}

}  // namespace detail

/**
 * Utility to create a #ValueSequence that has the same value at every index.
 *
 * Example:
 *   `make_value_sequence<MyEnum, MyEnum::A, 3>` becomes
 *   `ValueSequence<MyEnum::A, MyEnum::A, MyEnum::A>`
 */
template<typename T, T Element, size_t Size>
using make_value_sequence = decltype(detail::make_value_sequence_impl<T, Element>(
    std::make_index_sequence<Size>()));

}  // namespace blender
