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
  std::string node_name;
};

struct UIStorageAttributeInfo {
  std::string name;
  AttributeDomain domain;
  CustomDataType data_type;
};

struct UIStorageGeometryAttributes {
  std::string node_name;
  blender::Vector<UIStorageAttributeInfo> attributes;
};

class LocalNodeTreeUIStorage {
 public:
  blender::Vector<UIStorageGeometryAttributes> geometry_attributes;
};

class NodeTreeUIStorage {
 public:
  blender::threading::EnumerableThreadSpecific<LocalNodeTreeUIStorage> thread_locals;
};

NodeTreeUIStorage &BKE_node_tree_ui_storage_ensure(const bNodeTree &ntree);
