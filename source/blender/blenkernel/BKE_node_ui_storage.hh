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

#pragma once

#include <mutex>

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_hash.hh"
#include "BLI_map.hh"
#include "BLI_session_uuid.h"
#include "BLI_set.hh"

#include "DNA_ID.h"
#include "DNA_customdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_session_uuid_types.h"

#include "BKE_attribute.h"

struct ModifierData;
struct Object;
struct bNode;
struct bNodeTree;
struct bNodeSocket;
struct bContext;

enum class NodeWarningType {
  Error,
  Warning,
  Info,
};

struct NodeWarning {
  NodeWarningType type;
  std::string message;
};

class NodeTreeUIStorage;

namespace node_tree_ui_storage {

struct SocketIdentifier {
  std::string node_name;
  int socket_index;
  bool is_input;
};
struct GeometryAttributeInfo {
  std::string name;
  AttributeDomain domain;
  CustomDataType data_type;
};
struct GeometryAttributes : public SocketIdentifier {
  blender::Vector<GeometryAttributeInfo> attributes;
};
}  // namespace node_tree_ui_storage

class LocalNodeTreeUIStorage {
 private:
 public:
  blender::Vector<node_tree_ui_storage::GeometryAttributes> geometry_attributes_;
  void add_geometry_attributes(node_tree_ui_storage::GeometryAttributes attributes)
  {
    geometry_attributes_.append(std::move(attributes));
  }
};

class NodeTreeUIStorage {
 private:
 public:
  blender::threading::EnumerableThreadSpecific<LocalNodeTreeUIStorage> storages_;
  LocalNodeTreeUIStorage &get()
  {
    return storages_.local();
  }
};

NodeTreeUIStorage &BKE_node_tree_ui_storage_ensure(const bNodeTree &ntree);
