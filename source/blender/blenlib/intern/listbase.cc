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

#include "BLI_listbase.h"
#include "BLI_stack.hh"

#include "MEM_guardedalloc.h"

void BLI_listbase_iter4(const ListBase *list, void (*callback)(void *ptr))
{
  if (list->first == nullptr) {
    return;
  }

  BLI::Vector<Link *> links;
  Link *front = (Link *)list->first;
  Link *back = (Link *)list->last;

  while (true) {
    callback((void *)front);

    if (front == back) {
      break;
    }
    if (front->next == back) {
      links.append(back);
      break;
    }

    links.append(back);
    front = front->next;
    back = back->prev;
  }

  for (uint i = links.size(); i--;) {
    callback((void *)links[i]);
  }
}
