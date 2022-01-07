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

#include "BKE_global.h"

#include "BLI_string.h"
#include "BLI_string_search.h"

#include "MEM_guardedalloc.h"

void BKE_global_recent_search_add(const char *search_str)
{
  /* If the search string is in the list already, move it to the tail. */
  LISTBASE_FOREACH_MUTABLE (RecentSearch *, recent_search, &G.recent_searches) {
    if (STREQ(recent_search->str, search_str)) {
      BLI_remlink(&G.recent_searches, recent_search);
      BLI_addtail(&G.recent_searches, recent_search);
      return;
    }
  }

  /* The search string did not exist yet. Add a new list entry. */
  RecentSearch *recent_search = MEM_cnew<RecentSearch>(__func__);
  recent_search->str = BLI_strdup(search_str);
  BLI_addtail(&G.recent_searches, recent_search);
}
