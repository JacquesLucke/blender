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

/** \file
 * \ingroup bke
 */

struct bNode;
struct bNodeSocket;
struct bNodeTree;
struct Main;

/**
 * Tag tree as changed without providing any more information about what has changed exactly.
 * The update process has to assume that everything may have changed.
 */
void BKE_node_tree_update_tag(struct bNodeTree *tree);

/**
 * More specialized tag functions that may result in a more efficient update.
 */
void BKE_node_tree_update_tag_node(struct bNodeTree *tree, struct bNode *node);
void BKE_node_tree_update_tag_socket(struct bNodeTree *tree, struct bNodeSocket *socket);
void BKE_node_tree_update_tag_node_removed(struct bNodeTree *tree);
void BKE_node_tree_update_tag_link_removed(struct bNodeTree *tree);

typedef struct NodeTreeUpdateExtraParams {
  /**
   * Hint to the update function that this is the only tree that has been tagged for update. Under
   * some circumstances checking the entirety of #bmain can be avoided with that.
   */
  struct bNodeTree *only_tagged_tree;

  /**
   * Called for every tree that has been changed during the update. This can be used to send
   * notifiers to trigger redraws or depsgraph updates.
   */
  void (*tree_changed_fn)(struct bNodeTree *);

  /**
   * Called for every tree whose interface (e.g. input sockets) changed in some way. Other
   * (non-node-tree) data blocks may have to update when that happens.
   */
  void (*tree_interface_changed_fn)(struct bNodeTree *);

  /**
   * Called for every tree whose output value may have changed based on the provided update tags.
   * This can be used to tag the depsgraph if necessary.
   */
  void (*tree_output_changed_fn)(struct bNodeTree *);
} NodeTreeUpdateExtraParams;

void BKE_node_tree_update(struct Main *bmain, struct NodeTreeUpdateExtraParams *params);
