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

#include "BLI_map.hh"
#include "BLI_string_map.h"

struct StringMap {
  blender::Map<blender::StringRefNull, void *> map;
};

StringMap *BLI_stringmap_new(const char *UNUSED(info))
{
  return new StringMap();
}

void BLI_stringmap_free(StringMap *map)
{
  delete map;
}

void BLI_stringmap_add_new(StringMap *map, const char *key, void *value)
{
  map->map.add_new(key, value);
}

void BLI_stringmap_add(StringMap *map, const char *key, void *value)
{
  map->map.add(key, value);
}

void BLI_stringmap_remove(StringMap *map, const char *key)
{
  map->map.remove(key);
}

bool BLI_stringmap_contains(StringMap *map, const char *key)
{
  return map->map.contains(key);
}

void *BLI_stringmap_lookup_or_null(StringMap *map, const char *key)
{
  return map->map.lookup_default(key, nullptr);
}
