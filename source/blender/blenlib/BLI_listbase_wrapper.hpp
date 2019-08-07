#pragma once

/* The purpose of this wrapper is just to make it more
 * comfortable to iterate of ListBase instances, that
 * are used in many places in Blender.
 */

#include "BLI_listbase.h"
#include "DNA_listBase.h"

namespace BLI {

template<typename T> class IntrusiveListBaseWrapper {
 private:
  ListBase *m_listbase;

 public:
  IntrusiveListBaseWrapper(ListBase *listbase) : m_listbase(listbase)
  {
    BLI_assert(listbase);
  }

  IntrusiveListBaseWrapper(ListBase &listbase) : IntrusiveListBaseWrapper(&listbase)
  {
  }

  class Iterator {
   private:
    ListBase *m_listbase;
    T *m_current;

   public:
    Iterator(ListBase *listbase, T *current) : m_listbase(listbase), m_current(current)
    {
    }

    Iterator &operator++()
    {
      m_current = m_current->next;
      return *this;
    }

    Iterator operator++(int)
    {
      Iterator iterator = *this;
      ++*this;
      return iterator;
    }

    bool operator!=(const Iterator &iterator) const
    {
      return m_current != iterator.m_current;
    }

    T *operator*() const
    {
      return m_current;
    }
  };

  Iterator begin() const
  {
    return Iterator(m_listbase, (T *)m_listbase->first);
  }

  Iterator end() const
  {
    return Iterator(m_listbase, nullptr);
  }

  T get(uint index) const
  {
    void *ptr = BLI_findlink(m_listbase, index);
    BLI_assert(ptr);
    return (T *)ptr;
  }
};

} /* namespace BLI */
