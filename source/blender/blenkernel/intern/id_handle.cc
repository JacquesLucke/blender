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

#include "BKE_id_handle.hh"

#include "DNA_ID.h"

namespace blender::bke {

void IDHandleMap::add(const ID &id, int handle)
{
  BLI_assert(handle >= 0);
  handle_by_id_.add(&id, handle);
  id_by_handle_.add(handle, &id);
}

IDHandle IDHandleMap::lookup(const ID *id) const
{
  const int handle = handle_by_id_.lookup_default(id, -1);
  return IDHandle(handle);
}

ObjectIDHandle IDHandleMap::lookup(const Object *object) const
{
  const int handle = handle_by_id_.lookup_default((const ID *)object, -1);
  return ObjectIDHandle(handle);
}

const ID *IDHandleMap::lookup(const IDHandle &handle) const
{
  const ID *id = id_by_handle_.lookup_default(handle.handle_, nullptr);
  return id;
}

const Object *IDHandleMap::lookup(const ObjectIDHandle &handle) const
{
  const ID *id = this->lookup((const IDHandle &)handle);
  if (id == nullptr) {
    return nullptr;
  }
  if (GS(id->name) != ID_OB) {
    return nullptr;
  }
  return (const Object *)id;
}

}  // namespace blender::bke
