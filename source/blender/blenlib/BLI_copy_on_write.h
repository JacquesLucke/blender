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

#include "BLI_assert.h"
#include "BLI_compiler_attrs.h"
#include "BLI_utility_mixins.hh"

#include "DNA_copy_on_write.h"

#ifdef __cplusplus
extern "C" {
#endif

bCopyOnWrite *BLI_cow_new(int user_count);
void BLI_cow_free(const bCopyOnWrite *cow);

void BLI_cow_init(const bCopyOnWrite *cow);

bool BLI_cow_is_shared(const bCopyOnWrite *cow);
bool BLI_cow_is_mutable(const bCopyOnWrite *cow);

void BLI_cow_user_add(const bCopyOnWrite *cow);
bool BLI_cow_user_remove(const bCopyOnWrite *cow) ATTR_WARN_UNUSED_RESULT;

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

namespace blender {

class bCopyOnWrite : public ::bCopyOnWrite, private NonCopyable, NonMovable {
 public:
  bCopyOnWrite()
  {
    BLI_cow_init(this);
  }

  ~bCopyOnWrite()
  {
    BLI_assert(this->is_mutable());
  }

  bool is_shared() const
  {
    return BLI_cow_is_shared(this);
  }

  bool is_mutable() const
  {
    return BLI_cow_is_mutable(this);
  }

  void user_add() const
  {
    BLI_cow_user_add(this);
  }

  bool user_remove() const ATTR_WARN_UNUSED_RESULT
  {
    return BLI_cow_user_remove(this);
  }
};

template<typename T> class COWUser {
 private:
  T *data_ = nullptr;

 public:
  COWUser() = default;

  COWUser(T *data) : data_(data)
  {
  }

  COWUser(const COWUser &other) : data_(other.data_)
  {
    this->user_add(data_);
  }

  COWUser(COWUser &&other) : data_(other.data_)
  {
    other.data_ = nullptr;
  }

  ~COWUser()
  {
    this->user_remove(data_);
  }

  COWUser &operator=(const COWUser &other)
  {
    if (this == &other) {
      return *this;
    }

    this->user_remove(data_);
    data_ = other.data_;
    this->user_add(data_);
    return *this;
  }

  COWUser &operator=(COWUser &&other)
  {
    if (this == &other) {
      return *this;
    }

    this->user_remove(data_);
    data_ = other.data_;
    other.data_ = nullptr;
    return *this;
  }

  T *operator->()
  {
    BLI_assert(data_ != nullptr);
    return data_;
  }

  const T *operator->() const
  {
    BLI_assert(data_ != nullptr);
    return data_;
  }

  T &operator*()
  {
    BLI_assert(data_ != nullptr);
    return *data_;
  }

  const T &operator*() const
  {
    BLI_assert(data_ != nullptr);
    return *data_;
  }

  operator bool() const
  {
    return data_ != nullptr;
  }

  T *get()
  {
    return data_;
  }

  const T *get() const
  {
    return data_;
  }

  T *release()
  {
    T *data = data_;
    data_ = nullptr;
    return data;
  }

  void reset()
  {
    this->user_remove(data_);
    data_ = nullptr;
  }

  bool has_value() const
  {
    return data_ != nullptr;
  }

  uint64_t hash() const
  {
    return get_default_hash(data_);
  }

  friend bool operator==(const COWUser &a, const COWUser &b)
  {
    return a.data_ == b.data_;
  }

 private:
  static void user_add(T *data)
  {
    if (data != nullptr) {
      data->cow().user_add();
    }
  }

  static void user_remove(T *data)
  {
    if (data != nullptr) {
      if (data->cow().user_remove()) {
        data->cow_delete_self();
      }
    }
  }
};

}  // namespace blender

#endif
