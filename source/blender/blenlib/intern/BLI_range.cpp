#include "BLI_range.hpp"

namespace BLI {

static bool array_is_initialized = false;
static uint array[RANGE_AS_ARRAY_REF_MAX_LEN];

template<typename T> ArrayRef<T> Range<T>::as_array_ref() const
{
  BLI_assert(m_start >= 0);
  BLI_assert(m_one_after_last <= RANGE_AS_ARRAY_REF_MAX_LEN);
  BLI_assert(sizeof(T) == sizeof(uint));
  if (!array_is_initialized) {
    for (uint i = 0; i < RANGE_AS_ARRAY_REF_MAX_LEN; i++) {
      array[i] = i;
    }
    array_is_initialized = true;
  }
  return ArrayRef<T>((T *)array + m_start, this->size());
}

template ArrayRef<uint> Range<uint>::as_array_ref() const;
template ArrayRef<int> Range<int>::as_array_ref() const;

}  // namespace BLI
