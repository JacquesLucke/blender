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

#include "BKE_node_tree_update.h"

void BKE_node_tree_update_tag(bNodeTree *tree)
{
  UNUSED_VARS(tree);
}

void BKE_node_tree_update_tag_node(bNodeTree *tree, bNode *node)
{
  UNUSED_VARS(tree, node);
}

void BKE_node_tree_update_tag_socket(bNodeTree *tree, bNodeSocket *socket)
{
  UNUSED_VARS(tree, socket);
}

void BKE_node_tree_update_tag_node_removed(bNodeTree *tree)
{
  UNUSED_VARS(tree);
}

void BKE_node_tree_update_tag_link_removed(bNodeTree *tree)
{
  UNUSED_VARS(tree);
}

void BKE_node_tree_update(Main *bmain, NodeTreeUpdateExtraParams *params)
{
  UNUSED_VARS(bmain, params);
}
