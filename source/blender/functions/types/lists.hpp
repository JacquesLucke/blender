#pragma once

#include "BLI_shared_immutable.hpp"

namespace FN {
namespace Types {

template<typename T> class List;

template<typename T> using SharedList = AutoRefCount<List<T>>;

template<typename T> class List : public BLI::SharedImmutable {
 private:
  SmallVector<T> m_data;

  static constexpr bool DEBUG_ALLOCATIONS = false;

  ~List()
  {
    if (DEBUG_ALLOCATIONS) {
      std::cout << "List Freed:     " << (void *)this << std::endl;
    }
  }

 public:
  List() : BLI::SharedImmutable()
  {
    if (DEBUG_ALLOCATIONS) {
      std::cout << "List Allocated: " << (void *)this << std::endl;
    }
  }

  List(uint size) : List()
  {
    m_data = SmallVector<T>(size);
  }

  operator ArrayRef<T>() const
  {
    return m_data;
  }

  void append(T value)
  {
    this->assert_mutable();
    m_data.append(std::move(value));
  }

  void extend(List *other)
  {
    this->assert_mutable();
    m_data.extend(other->m_data);
  }

  List *copy() const
  {
    List *new_list = new List();
    for (T &value : m_data) {
      new_list->append(value);
    }
    BLI_assert(new_list->users() == 1);
    return new_list;
  }

  T *data_ptr() const
  {
    return m_data.begin();
  }

  uint size() const
  {
    return m_data.size();
  }

  T operator[](int index) const
  {
    return m_data[index];
  }

  T &operator[](int index)
  {
    this->assert_mutable();
    return m_data[index];
  }

  SharedList<T> get_mutable()
  {
    if (this->is_mutable()) {
      return SharedList<T>(this);
    }
    else {
      List *new_list = this->copy();
      BLI_assert(new_list->is_mutable());
      return SharedList<T>(new_list);
    }
  }

  T *begin() const
  {
    return m_data.begin();
  }

  T *end() const
  {
    return m_data.end();
  }
};

}  // namespace Types
}  // namespace FN
