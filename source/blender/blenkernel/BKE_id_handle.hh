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
};

class ObjectIDHandle : public IDHandle {
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
  IDHandle get_handle(const ID &id) const;
  const ID *get_id(const IDHandle &handle) const;
  const Object *get_object(const ObjectIDHandle &handle) const;
};

}  // namespace blender::bke

#endif /*  __BKE_ID_HANDLE_H__ */
