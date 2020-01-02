#ifndef __BLI_PARALLEL_H__
#define __BLI_PARALLEL_H__

#ifdef WITH_TBB
#  define TBB_SUPPRESS_DEPRECATED_MESSAGES 1
#  include "tbb/parallel_for.h"
#  include "tbb/parallel_invoke.h"
#endif

#include "BLI_index_range.h"
#include "BLI_multi_map.h"

namespace BLI {

/**
 * Call func for every index in the IndexRange. func has to receive a single uint parameter.
 */
template<typename FuncT> void parallel_for(IndexRange range, const FuncT &func)
{
  if (range.size() == 0) {
    return;
  }
#ifdef WITH_TBB
  tbb::parallel_for(range.first(), range.one_after_last(), func);
#else
  for (uint i : range) {
    func(i);
  }
#endif
}

/**
 * Call func for subranges of range. The size of the individual subranges is controlled by a
 * grain_size. func has to receive an IndexRange as parameter.
 */
template<typename FuncT>
void blocked_parallel_for(IndexRange range, uint grain_size, const FuncT &func)
{
  if (range.size() == 0) {
    return;
  }
#ifdef WITH_TBB
  tbb::parallel_for(
      tbb::blocked_range<uint>(range.first(), range.one_after_last(), grain_size),
      [&](const tbb::blocked_range<uint> &sub_range) { func(IndexRange(sub_range)); });
#else
  func(range);
#endif
}

/**
 * Invoke multiple functions in parallel.
 */
template<typename FuncT1, typename FuncT2>
void parallel_invoke(const FuncT1 &func1, const FuncT2 &func2)
{
#ifdef WITH_TBB
  tbb::parallel_invoke(func1, func2);
#else
  func1();
  func2();
#endif
}

template<typename FuncT1, typename FuncT2, typename FuncT3>
void parallel_invoke(const FuncT1 &func1, const FuncT2 &func2, const FuncT3 &func3)
{
#ifdef WITH_TBB
  tbb::parallel_invoke(func1, func2, func3);
#else
  func1();
  func2();
  func3();
#endif
}

template<typename KeyT, typename ValueT, uint N, typename FuncT>
void parallel_multi_map_items(const MultiMap<KeyT, ValueT, N> &multi_map, const FuncT &func)
{
  ScopedVector<const KeyT *> key_vector;
  ScopedVector<ArrayRef<ValueT>> values_vector;

  multi_map.foreach_item([&](const KeyT &key, ArrayRef<ValueT> values) {
    key_vector.append(&key);
    values_vector.append(values);
  });

  parallel_for(key_vector.index_range(), [&](uint index) {
    const KeyT &key = *key_vector[index];
    ArrayRef<ValueT> values = values_vector[index];

    func(key, values);
  });
}

}  // namespace BLI

#endif /* __BLI_PARALLEL_H__ */
