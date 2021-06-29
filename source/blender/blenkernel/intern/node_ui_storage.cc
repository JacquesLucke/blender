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

#include "CLG_log.h"

#include <mutex>

#include "BLI_map.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "DNA_node_types.h"
#include "DNA_object_types.h"

#include "BKE_context.h"
#include "BKE_node_ui_storage.hh"
#include "BKE_object.h"

static CLG_LogRef LOG = {"bke.node_ui_storage"};

/* Use a global mutex because otherwise it would have to be stored directly in the
 * bNodeTree struct in DNA. This could change if the node tree had a runtime struct. */
static std::mutex global_ui_storage_mutex;

NodeTreeUIStorage &BKE_node_tree_ui_storage_ensure(const bNodeTree &ntree)
{
  if (ntree.ui_storage == nullptr) {
    std::lock_guard lock{global_ui_storage_mutex};
    if (ntree.ui_storage == nullptr) {
      const_cast<bNodeTree &>(ntree).ui_storage = new NodeTreeUIStorage();
    }
  }
  return *ntree.ui_storage;
}
