#pragma once

#include "FN_multi_function.h"

namespace FN {

using BLI::LargeScopedVector;
using BLI::ScopedVector;

template<typename T, typename FuncT, typename EqualFuncT = std::equal_to<T>>
void group_indices_by_same_value(IndexMask indices,
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

  ScopedVector<T> seen_values;

  for (uint i : indices.index_range()) {
    uint index = indices[i];

    const T &value = values[index];
    if (seen_values.as_ref().any([&](const T &seen_value) { return equal(value, seen_value); })) {
      continue;
    }
    seen_values.append(value);

    LargeScopedVector<uint> indices_with_value;
    for (uint j : indices.indices().drop_front(i)) {
      if (equal(values[j], value)) {
        indices_with_value.append(j);
      }
    }

    IndexMask mask_with_same_value = indices_with_value.as_ref();
    func(value, mask_with_same_value);
  }
}

}  // namespace FN
