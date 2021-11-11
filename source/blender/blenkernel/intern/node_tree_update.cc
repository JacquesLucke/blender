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

#include "DNA_node_types.h"

#include "BKE_node.h"
#include "BKE_node_tree_update.h"

void BKE_node_tree_update_tag(bNodeTree *tree)
{
  tree->changed_flag |= NTREE_CHANGED_ANY;
  UNUSED_VARS(tree);
}

void BKE_node_tree_update_tag_node(bNodeTree *tree, bNode *node)
{
  tree->changed_flag |= NTREE_CHANGED_NODE;
  node->changed_flag |= NODE_CHANGED_ANY;
  tree->update |= NTREE_UPDATE;
}

void BKE_node_tree_update_tag_socket(bNodeTree *tree, bNodeSocket *socket)
{
  tree->changed_flag |= NTREE_CHANGED_SOCKET;
  socket->changed_flag |= SOCK_CHANGED_ANY;
  tree->update |= NTREE_UPDATE;
}

void BKE_node_tree_update_tag_node_removed(bNodeTree *tree)
{
  tree->changed_flag |= NTREE_CHANGED_REMOVED_ANY;
  tree->update |= NTREE_UPDATE;
}

void BKE_node_tree_update_tag_link(bNodeTree *tree)
{
  tree->changed_flag |= NTREE_CHANGED_LINK;
  tree->update |= NTREE_UPDATE;
}

void BKE_node_tree_update_tag_node_added(bNodeTree *tree, bNode *node)
{
  BKE_node_tree_update_tag_node(tree, node);
}

void BKE_node_tree_update_tag_link_removed(bNodeTree *tree)
{
  BKE_node_tree_update_tag_link(tree);
}

void BKE_node_tree_update_tag_link_added(bNodeTree *tree, bNodeLink *UNUSED(link))
{
  BKE_node_tree_update_tag_link(tree);
}

void BKE_node_tree_update_tag_link_mute(bNodeTree *tree, bNodeLink *UNUSED(link))
{
  BKE_node_tree_update_tag_link(tree);
}

void BKE_node_tree_update_tag_missing_runtime_data(bNodeTree *tree)
{
  tree->changed_flag |= NTREE_CHANGED_MISSING_RUNTIME_DATA;
  tree->update |= NTREE_UPDATE;
}

void BKE_node_tree_update_tag_interface(bNodeTree *tree)
{
  tree->changed_flag |= NTREE_CHANGED_ANY;
  tree->update |= NTREE_UPDATE;
}

void BKE_node_tree_update(Main *bmain, NodeTreeUpdateExtraParams *params)
{
  FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
    ntreeUpdateTree(bmain, ntree);
    if (params->tree_changed_fn) {
      params->tree_changed_fn(id, ntree, params->user_data);
    }
    if (params->tree_interface_changed_fn) {
      params->tree_interface_changed_fn(id, ntree, params->user_data);
    }
    if (params->tree_output_changed_fn) {
      params->tree_output_changed_fn(id, ntree, params->user_data);
    }
  }
  FOREACH_NODETREE_END;
}
