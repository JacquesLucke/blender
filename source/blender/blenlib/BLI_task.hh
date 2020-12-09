/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

/** \file
 * \ingroup bli
 */

#ifdef WITH_TBB
/* Quiet top level deprecation message, unrelated to API usage here. */
#  define TBB_SUPPRESS_DEPRECATED_MESSAGES 1

#  if defined(WIN32) && !defined(NOMINMAX)
/* TBB includes Windows.h which will define min/max macros causing issues
 * when we try to use std::min and std::max later on. */
#    define NOMINMAX
#    define TBB_MIN_MAX_CLEANUP
#  endif
#  include <tbb/tbb.h>
#  ifdef WIN32
/* We cannot keep this defined, since other parts of the code deal with this on their own, leading
 * to multiple define warnings unless we un-define this, however we can only undefine this if we
 * were the ones that made the definition earlier. */
#    ifdef TBB_MIN_MAX_CLEANUP
#      undef NOMINMAX
#    endif
#  endif
#endif

#include "BLI_index_range.hh"
#include "BLI_utildefines.h"

#include <optional>

namespace blender {

template<typename Range, typename Function>
void parallel_for_each(Range &range, const Function &function)
{
#ifdef WITH_TBB
  tbb::parallel_for_each(range, function);
#else
  for (auto &value : range) {
    function(value);
  }
#endif
}

template<typename Function>
void parallel_for(IndexRange range, int64_t grain_size, const Function &function)
{
  if (range.size() == 0) {
    return;
  }
#ifdef WITH_TBB
  tbb::parallel_for(tbb::blocked_range<int64_t>(range.first(), range.one_after_last(), grain_size),
                    [&](const tbb::blocked_range<int64_t> &subrange) {
                      function(IndexRange(subrange.begin(), subrange.size()));
                    });
#else
  UNUSED_VARS(grain_size);
  function(range);
#endif
}

template<typename Function1, typename Function2>
void parallel_invoke(const Function1 &function1,
                     const Function2 &function2,
                     const bool use_threading = true)
{
#ifdef WITH_TBB
  if (use_threading) {
    tbb::parallel_invoke(function1, function2);
    return;
  }
#endif
  function1();
  function2();
}

template<typename T> class EnumerableThreadSpecific {
#ifdef WITH_TBB
 private:
  tbb::enumerable_thread_specific<T> tbb_data_;

 public:
  EnumerableThreadSpecific() = default;

  T &local()
  {
    return tbb_data_.local();
  }

  auto begin()
  {
    return tbb_data_.begin();
  }

  auto end()
  {
    return tbb_data_.end();
  }

#else /* WITH_TBB */

 private:
  std::optional<T> fallback_data_;

 public:
  EnumerableThreadSpecific() = default;

  T &local()
  {
    if (!fallback_data_.has_value()) {
      fallback_data_ = T();
    }
    return *fallback_data_;
  }

  T *begin()
  {
    if (fallback_data_.has_value()) {
      return &(*fallback_data_);
    }
    return nullptr;
  }

  T *end()
  {
    if (fallback_data_.has_value()) {
      return &(*fallback_data_) + 1;
    }
    return nullptr;
  }

#endif /* WITH_TBB */
};

}  // namespace blender
