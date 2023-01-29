/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include <atomic>
#include <functional>

#include "BLI_compiler_attrs.h"
#include "BLI_utildefines.h"
#include "BLI_utility_mixins.hh"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bCopyOnWrite bCopyOnWrite;

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

struct bCopyOnWrite : blender::NonCopyable, blender::NonMovable {
 private:
  using DeleteFn = std::function<void(const bCopyOnWrite *cow)>;

  mutable std::atomic<int> users_;
  const void *data_;
  DeleteFn delete_fn_;

 public:
  bCopyOnWrite(const int initial_users, const void *data, DeleteFn delete_fn)
      : users_(initial_users), data_(data), delete_fn_(std::move(delete_fn))
  {
  }

  ~bCopyOnWrite()
  {
    BLI_assert(this->is_mutable());
  }

  const void *data() const
  {
    return data_;
  }

  bool is_shared() const
  {
    return users_.load(std::memory_order_relaxed) >= 2;
  }

  bool is_mutable() const
  {
    return !this->is_shared();
  }

  void user_add() const
  {
    users_.fetch_add(1, std::memory_order_relaxed);
  }

  bool user_remove_and_delete_if_last() const
  {
    const int old_user_count = users_.fetch_sub(1, std::memory_order_relaxed);
    BLI_assert(old_user_count >= 1);
    const bool was_last_user = old_user_count == 1;
    if (was_last_user) {
      delete_fn_(this);
    }
    return was_last_user;
  }
};

namespace blender {

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
    this->user_remove_and_delete_if_last(data_);
  }

  COWUser &operator=(const COWUser &other)
  {
    if (this == &other) {
      return *this;
    }

    this->user_remove_and_delete_if_last(data_);
    data_ = other.data_;
    this->user_add(data_);
    return *this;
  }

  COWUser &operator=(COWUser &&other)
  {
    if (this == &other) {
      return *this;
    }

    this->user_remove_and_delete_if_last(data_);
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
    this->user_remove_and_delete_if_last(data_);
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

  static void user_remove_and_delete_if_last(T *data)
  {
    if (data != nullptr) {
      data->cow().user_remove_and_delete_if_last();
    }
  }
};

}  // namespace blender

#endif
