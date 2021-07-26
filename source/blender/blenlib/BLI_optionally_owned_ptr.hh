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

#include <memory>

#include "BLI_utildefines.h"

namespace blender {

template<typename T, typename OwnedTPtr = std::unique_ptr<T>> class OptionallyOwnedPtr {
 private:
  OwnedTPtr owned_ptr_;
  T *ptr_ = nullptr;

 public:
  OptionallyOwnedPtr() = default;

  OptionallyOwnedPtr(T &ptr) : ptr_(&ptr)
  {
  }

  OptionallyOwnedPtr(OwnedTPtr owned_ptr) : owned_ptr_(std::move(owned_ptr)), ptr_(&*owned_ptr_)
  {
  }

  T *operator->()
  {
    BLI_assert(ptr_ != nullptr);
    return ptr_;
  }

  const T *operator->() const
  {
    BLI_assert(ptr_ != nullptr);
    return ptr_;
  }

  T &operator*()
  {
    BLI_assert(ptr_ != nullptr);
    return *ptr_;
  }

  const T &operator*() const
  {
    BLI_assert(ptr_ != nullptr);
    return *ptr_;
  }

  operator bool() const
  {
    return ptr_ != nullptr;
  }
};

}  // namespace blender
