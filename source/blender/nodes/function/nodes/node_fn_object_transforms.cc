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

#include "node_function_util.hh"

static bNodeSocketTemplate fn_node_object_transforms_in[] = {
    {SOCK_OBJECT, N_("Object")},
    {-1, ""},
};

static bNodeSocketTemplate fn_node_object_transforms_out[] = {
    {SOCK_VECTOR, N_("Location")},
    {-1, ""},
};

void register_node_type_fn_object_transforms()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_OBJECT_TRANSFORMS, "Object Transforms", 0, 0);
  node_type_socket_templates(&ntype, fn_node_object_transforms_in, fn_node_object_transforms_out);
  nodeRegisterType(&ntype);
}
