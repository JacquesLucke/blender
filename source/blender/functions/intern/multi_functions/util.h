#pragma once

#include "FN_multi_function.h"

namespace FN {

using BLI::TemporaryVector;

template<typename T, typename FuncT, typename EqualFuncT = std::equal_to<T>>
void group_indices_by_same_value(ArrayRef<uint> indices,
                                 VirtualListRef<T> values,
                                 const FuncT &func,
                                 EqualFuncT equal = std::equal_to<T>())
{
  if (indices.size() == 0) {
    return;
  }
  if (values.is_single_element()) {
    const T &value = values[indices[0]];
    func(value, indices);
    return;
  }

  Vector<T> seen_values;

  for (uint i : indices.index_iterator()) {
    uint index = indices[i];

    const T &value = values[index];
    if (seen_values.as_ref().any([&](const T &seen_value) { return equal(value, seen_value); })) {
      continue;
    }
    seen_values.append(value);

    TemporaryVector<uint> indices_with_value;
    for (uint j : indices.drop_front(i)) {
      if (equal(values[j], value)) {
        indices_with_value.append(j);
      }
    }

    func(value, indices_with_value.as_ref());
  }
}

}  // namespace FN
