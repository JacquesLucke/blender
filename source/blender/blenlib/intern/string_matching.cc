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

#include "BLI_array.hh"
#include "BLI_span.hh"
#include "BLI_string_matching.h"
#include "BLI_string_ref.hh"

namespace blender::string_matching {

/**
 * Computes the minimum number of single character edits (insertions, deletions or substitutions)
 * required to get from one string to another.
 */
int levenshtein_distance(StringRef a, StringRef b)
{
  int length_a = a.size();
  int length_b = b.size();

  Array<int> v0_array(length_b + 1);
  Array<int> v1_array(length_b + 1);
  MutableSpan<int> v0 = v0_array;
  MutableSpan<int> v1 = v1_array;

  for (const int i : v0.index_range()) {
    v0[i] = i;
  }

  for (const int i : IndexRange(length_a)) {
    v1[0] = i + 1;

    for (const int j : IndexRange(length_b)) {
      const int deletion_cost = v0[j + 1] + 1;
      const int insertion_cost = v1[j] + 1;
      const int substitution_cost = v0[j] + (a[i] != b[j]);
      const int minimum_cost = std::min({deletion_cost, insertion_cost, substitution_cost});
      v1[j + 1] = minimum_cost;
    }

    std::swap(v0, v1);
  }

  return v0[length_b];
}

}  // namespace blender::string_matching
