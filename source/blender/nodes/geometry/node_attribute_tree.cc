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

#include <cstring>

#include "BKE_node.h"

#include "MEM_guardedalloc.h"

#include "BLT_translation.h"

#include "RNA_access.h"

#include "NOD_geometry.h"

#include "node_common.h"

bNodeTreeType *ntreeType_Attribute;

static void attribute_node_tree_update(bNodeTree *ntree)
{
  ntreeSetOutput(ntree);

  /* Needed to give correct types to reroutes. */
  ntree_update_reroute_nodes(ntree);
}

static bool attribute_node_tree_socket_type_valid(eNodeSocketDatatype socket_type,
                                                  bNodeTreeType *UNUSED(ntreetype))
{
  return ELEM(
      socket_type, SOCK_FLOAT, SOCK_VECTOR, SOCK_RGBA, SOCK_BOOLEAN, SOCK_INT, SOCK_OBJECT);
}

void register_node_tree_type_attr()
{
  bNodeTreeType *tt = ntreeType_Attribute = static_cast<bNodeTreeType *>(
      MEM_callocN(sizeof(bNodeTreeType), "attribute node tree type"));
  tt->type = NTREE_ATTRIBUTE;
  strcpy(tt->idname, "AttributeNodeTree");
  strcpy(tt->ui_name, N_("Attribute Node Editor"));
  tt->ui_icon = 0; /* defined in drawnode.c */
  strcpy(tt->ui_description, N_("Attribute nodes"));
  tt->rna_ext.srna = &RNA_AttributeNodeTree;
  tt->update = attribute_node_tree_update;
  tt->valid_socket_type = attribute_node_tree_socket_type_valid;

  ntreeTypeAdd(tt);
}
