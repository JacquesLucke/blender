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

#ifndef __BKE_ID_HANDLE_H__
#define __BKE_ID_HANDLE_H__

#include "BLI_map.hh"
#include "BLI_utildefines.h"

struct ID;
struct Object;

namespace blender::bke {

class IDHandleMap;

class IDHandle {
 private:
  int handle_;

  friend IDHandleMap;

 protected:
  IDHandle(int handle) : handle_(handle)
  {
  }

 public:
  IDHandle() : handle_(-1)
  {
  }

  friend bool operator==(const IDHandle &a, const IDHandle &b)
  {
    return a.handle_ == b.handle_;
  }

  friend bool operator!=(const IDHandle &a, const IDHandle &b)
  {
    return !(a == b);
  }

  friend std::ostream &operator<<(std::ostream &stream, const IDHandle &a)
  {
    stream << a.handle_;
    return stream;
  }

  uint64_t hash() const
  {
    return handle_;
  }
};

class ObjectIDHandle : public IDHandle {
 private:
  friend IDHandleMap;

  ObjectIDHandle(int handle) : IDHandle(handle)
  {
  }

 public:
  ObjectIDHandle() : IDHandle()
  {
  }
};

class IDHandleMap {
 private:
  Map<int, const ID *> id_by_handle_;
  Map<const ID *, int> handle_by_id_;

 public:
  void add(const ID &id, int handle);
  IDHandle lookup(const ID *id) const;
  ObjectIDHandle lookup(const Object *object) const;
  const ID *lookup(const IDHandle &handle) const;
  const Object *lookup(const ObjectIDHandle &handle) const;
};

}  // namespace blender::bke

#endif /*  __BKE_ID_HANDLE_H__ */
