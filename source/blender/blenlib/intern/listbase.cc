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

void BLI_listbase_iter6(const ListBase *list, void (*callback)(void *ptr))
{
  if (list->first == nullptr) {
    return;
  }

  BLI::Vector<Link *, 128> links;
  Link *front = (Link *)list->first;
  Link *back = (Link *)list->last;
  bool use_array = false;
  int index = -1;

  while (true) {
    Link *current;
    if (use_array) {
      if (index < 0) {
        break;
      }
      current = links[index];
      index--;
    }
    else {
      if (front == back) {
        links.append(front);
        use_array = true;
        index = (int)links.size() - 1;
        continue;
      }
      else if (front->next == back) {
        links.append(front);
        links.append(back);
        use_array = true;
        index = (int)links.size() - 1;
        continue;
      }
      current = front;
    }

    callback((void *)current);

    if (!use_array) {
      links.append(back);
      front = front->next;
      back = back->prev;
    }
  }
}

void BLI_listbase_iter7(const ListBase *list, void (*callback)(void *ptr))
{
  if (list->first == nullptr) {
    return;
  }

#define CAPACITY_OF_ARRAY(array_index) (1 << (array_index + 7))

  Link *local_buffer[CAPACITY_OF_ARRAY(0)];
  Link **link_arrays[32];
  link_arrays[0] = local_buffer;

  Link **current_link_array = local_buffer;
  int current_link_array_index = 0;
  int current_link_array_size = 0;
  int current_link_array_capacity = CAPACITY_OF_ARRAY(0);

  Link *front = (Link *)list->first;
  Link *back = (Link *)list->last;
  bool use_array = false;
  int index = -1;

#define DO_APPEND(link) \
  if (UNLIKELY(current_link_array_size == current_link_array_capacity)) { \
    current_link_array_index++; \
    current_link_array_capacity = CAPACITY_OF_ARRAY(current_link_array_index); \
    link_arrays[current_link_array_index] = (Link **)MEM_malloc_arrayN( \
        current_link_array_capacity, sizeof(Link *), __func__); \
    current_link_array = link_arrays[current_link_array_index]; \
    current_link_array_size = 0; \
  } \
  current_link_array[current_link_array_size] = link; \
  current_link_array_size++;

  while (true) {
    Link *current;
    if (use_array) {
      if (UNLIKELY(index < 0)) {
        if (current_link_array_index == 0) {
          break;
        }

        MEM_freeN(current_link_array);

        current_link_array_index--;
        current_link_array = link_arrays[current_link_array_index];
        index = CAPACITY_OF_ARRAY(current_link_array_index) - 1;
      }
      current = current_link_array[index];
      index--;
    }
    else {
      if (UNLIKELY(front == back)) {
        DO_APPEND(front);
        use_array = true;
        index = current_link_array_size - 1;
        continue;
      }
      else if (UNLIKELY(front->next == back)) {
        DO_APPEND(back);
        DO_APPEND(front);
        use_array = true;
        index = current_link_array_size - 1;
        continue;
      }
      current = front;
    }

    callback((void *)current);

    if (!use_array) {
      DO_APPEND(back);
      front = front->next;
      back = back->prev;
    }
  }

#undef DO_APPEND
#undef LOCAL_CAPACITY
}
