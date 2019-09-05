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

#include "BLI_range.hpp"
#include "BLI_array_ref.hpp"

namespace BLI {

static bool array_is_initialized = false;
static uint array[RANGE_AS_ARRAY_REF_MAX_LEN];

ArrayRef<uint> IndexRange::as_array_ref() const
{
  BLI_assert(m_one_after_last <= RANGE_AS_ARRAY_REF_MAX_LEN);
  if (!array_is_initialized) {
    for (uint i = 0; i < RANGE_AS_ARRAY_REF_MAX_LEN; i++) {
      array[i] = i;
    }
    array_is_initialized = true;
  }
  return ArrayRef<uint>(array + m_start, this->size());
}

}  // namespace BLI
