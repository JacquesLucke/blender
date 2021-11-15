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
#include "BLI_multi_value_map.hh"

#include "DNA_modifier_types.h"
#include "DNA_node_types.h"

#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_node_tree_update.h"

#include "MOD_nodes.h"

namespace blender::bke {

using IDTreePair = std::pair<ID *, bNodeTree *>;
using TreeNodePair = std::pair<bNodeTree *, bNode *>;
using ObjectModifierPair = std::pair<Object *, ModifierData *>;

struct NodeTreeRelations {
 private:
  Main *bmain_;
  std::optional<MultiValueMap<bNodeTree *, TreeNodePair>> group_node_users_;
  std::optional<MultiValueMap<bNodeTree *, ObjectModifierPair>> modifiers_users_;

 public:
  NodeTreeRelations(Main *bmain) : bmain_(bmain)
  {
  }

  void ensure_group_node_users()
  {
    if (group_node_users_.has_value()) {
      return;
    }

    group_node_users_.emplace();

    if (bmain_ == nullptr) {
      return;
    }

    FOREACH_NODETREE_BEGIN (bmain_, ntree, id) {
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->id == nullptr) {
          continue;
        }
        ID *id = node->id;
        if (GS(id->name) == ID_NT) {
          bNodeTree *group = (bNodeTree *)id;
          group_node_users_->add(group, {ntree, node});
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  void ensure_modifier_users()
  {
    if (modifiers_users_.has_value()) {
      return;
    }

    modifiers_users_.emplace();

    if (bmain_ == nullptr) {
      return;
    }

    LISTBASE_FOREACH (Object *, object, &bmain_->objects) {
      LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
        if (md->type == eModifierType_Nodes) {
          NodesModifierData *nmd = (NodesModifierData *)md;
          if (nmd->node_group != nullptr) {
            modifiers_users_->add(nmd->node_group, {object, md});
          }
        }
      }
    }
  }

  Span<ObjectModifierPair> get_modifier_users(bNodeTree *ntree)
  {
    BLI_assert(modifiers_users_.has_value());
    return modifiers_users_->lookup(ntree);
  }

  Span<TreeNodePair> get_group_node_users(bNodeTree *ntree)
  {
    BLI_assert(group_node_users_);
    return group_node_users_->lookup(ntree);
  }
};

struct TreeUpdateResult {
  bool interface_changed = false;
  bool output_changed = false;
};

class NodeTreeMainUpdater {
 private:
  Main *bmain_;
  NodeTreeUpdateExtraParams *params_;
  Map<bNodeTree *, TreeUpdateResult> update_result_by_tree_;
  NodeTreeRelations relations_;

 public:
  NodeTreeMainUpdater(Main *bmain, NodeTreeUpdateExtraParams *params)
      : bmain_(bmain), params_(params), relations_(bmain)
  {
  }

  void update()
  {
    Vector<bNodeTree *> changed_ntrees;
    FOREACH_NODETREE_BEGIN (bmain_, ntree, id) {
      if (ntree->changed_flag != NTREE_CHANGED_NONE) {
        changed_ntrees.append(ntree);
      }
    }
    FOREACH_NODETREE_END;
    this->update_rooted(changed_ntrees);
  }

  void update_rooted(Span<bNodeTree *> root_ntrees)
  {
    if (root_ntrees.is_empty()) {
      return;
    }

    bool is_single_tree_update = false;

    if (root_ntrees.size() == 1) {
      bNodeTree *ntree = root_ntrees[0];
      const TreeUpdateResult result = this->update_tree(*ntree);
      update_result_by_tree_.add_new(ntree, result);
      if (!result.interface_changed && !result.output_changed) {
        is_single_tree_update = true;
      }
    }

    if (!is_single_tree_update) {
      Vector<bNodeTree *> ntrees_in_order = this->get_tree_update_order(root_ntrees);
      for (bNodeTree *ntree : ntrees_in_order) {
        if (ntree->changed_flag == NTREE_CHANGED_NONE) {
          continue;
        }
        if (!update_result_by_tree_.contains(ntree)) {
          const TreeUpdateResult result = this->update_tree(*ntree);
          update_result_by_tree_.add_new(ntree, result);
        }
        const TreeUpdateResult result = update_result_by_tree_.lookup(ntree);
        if (result.output_changed || result.interface_changed) {
          Span<TreeNodePair> dependent_trees = relations_.get_group_node_users(ntree);
          for (const TreeNodePair &pair : dependent_trees) {
            BKE_node_tree_update_tag_node(pair.first, pair.second);
          }
        }
      }
    }

    Vector<bNodeTree *> trees_with_visible_changes;
    for (bNodeTree *ntree : root_ntrees) {
      const TreeUpdateResult result = this->update_tree(*ntree);
      update_result_by_tree_.add_new(ntree, result);

      if (result.output_changed || result.interface_changed) {
        trees_with_visible_changes.append(ntree);
        relations_.ensure_group_node_users();
        if (ntree->type == NTREE_GEOMETRY) {
          relations_.ensure_modifier_users();
        }
      }
    }

    if (!trees_with_visible_changes.is_empty()) {
      relations_.ensure_group_node_users();
    }

    for (const auto &item : update_result_by_tree_.items()) {
      bNodeTree *ntree = item.key;
      const TreeUpdateResult &result = item.value;
      /* TODO: Use owner id of embedded node trees. */
      ID *id = &ntree->id;

      ntree->changed_flag = NTREE_CHANGED_NONE;

      if (result.interface_changed) {
        if (ntree->type == NTREE_GEOMETRY) {
          relations_.ensure_modifier_users();
          for (const ObjectModifierPair &pair : relations_.get_modifier_users(ntree)) {
            Object *object = pair.first;
            ModifierData *md = pair.second;

            if (md->type == eModifierType_Nodes) {
              MOD_nodes_update_interface(object, (NodesModifierData *)md);
            }
          }
        }
      }

      if (params_) {
        if (params_->tree_changed_fn) {
          params_->tree_changed_fn(id, ntree, params_->user_data);
        }
        if (params_->tree_interface_changed_fn && result.interface_changed) {
          params_->tree_interface_changed_fn(id, ntree, params_->user_data);
        }
        if (params_->tree_output_changed_fn && result.output_changed) {
          params_->tree_output_changed_fn(id, ntree, params_->user_data);
        }
      }
    }
  }

 private:
  Vector<bNodeTree *> get_tree_update_order(Span<bNodeTree *> root_ntrees)
  {
    relations_.ensure_group_node_users();
    return {};
  }

  TreeUpdateResult update_tree(bNodeTree &ntree)
  {
    ntreeUpdateTree(bmain_, &ntree);
    TreeUpdateResult result;
    result.interface_changed = true;
    result.output_changed = true;
    return result;
  }
};

}  // namespace blender::bke

void BKE_node_tree_update_tag(bNodeTree *ntree)
{
  ntree->changed_flag |= NTREE_CHANGED_ANY;
  ntree->update |= NTREE_UPDATE;
}

void BKE_node_tree_update_tag_node(bNodeTree *ntree, bNode *node)
{
  ntree->changed_flag |= NTREE_CHANGED_NODE;
  node->changed_flag |= NODE_CHANGED_ANY;
  ntree->update |= NTREE_UPDATE;
}

void BKE_node_tree_update_tag_socket(bNodeTree *ntree, bNodeSocket *socket)
{
  ntree->changed_flag |= NTREE_CHANGED_SOCKET;
  socket->changed_flag |= SOCK_CHANGED_ANY;
  ntree->update |= NTREE_UPDATE;
}

void BKE_node_tree_update_tag_node_removed(bNodeTree *ntree)
{
  ntree->changed_flag |= NTREE_CHANGED_REMOVED_ANY;
  ntree->update |= NTREE_UPDATE;
}

void BKE_node_tree_update_tag_link(bNodeTree *ntree)
{
  ntree->changed_flag |= NTREE_CHANGED_LINK;
  ntree->update |= NTREE_UPDATE;
}

void BKE_node_tree_update_tag_node_added(bNodeTree *ntree, bNode *node)
{
  BKE_node_tree_update_tag_node(ntree, node);
}

void BKE_node_tree_update_tag_link_removed(bNodeTree *ntree)
{
  BKE_node_tree_update_tag_link(ntree);
}

void BKE_node_tree_update_tag_link_added(bNodeTree *ntree, bNodeLink *UNUSED(link))
{
  BKE_node_tree_update_tag_link(ntree);
}

void BKE_node_tree_update_tag_link_mute(bNodeTree *ntree, bNodeLink *UNUSED(link))
{
  BKE_node_tree_update_tag_link(ntree);
}

void BKE_node_tree_update_tag_missing_runtime_data(bNodeTree *ntree)
{
  ntree->changed_flag |= NTREE_CHANGED_MISSING_RUNTIME_DATA;
  ntree->update |= NTREE_UPDATE;
}

void BKE_node_tree_update_tag_interface(bNodeTree *ntree)
{
  ntree->changed_flag |= NTREE_CHANGED_ANY;
  ntree->update |= NTREE_UPDATE;
}

static bool is_updating = false;

void BKE_node_tree_update_main(Main *bmain, NodeTreeUpdateExtraParams *params)
{
  if (is_updating) {
    return;
  }

  is_updating = true;
  blender::bke::NodeTreeMainUpdater updater{bmain, params};
  updater.update();
  is_updating = false;
}

void BKE_node_tree_update_main_rooted(Main *bmain,
                                      bNodeTree *ntree,
                                      NodeTreeUpdateExtraParams *params)
{
  if (ntree == nullptr) {
    BKE_node_tree_update_main(bmain, params);
    return;
  }

  if (is_updating) {
    return;
  }

  is_updating = true;
  blender::bke::NodeTreeMainUpdater updater{bmain, params};
  updater.update_rooted({{(ID *)ntree, ntree}});
  is_updating = false;
}
