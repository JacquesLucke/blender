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

#include "NOD_node_socket_declarations.hh"

#include "BKE_node.h"

namespace blender::nodes::decl {

bNodeSocket &Float::build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const
{
  bNodeSocket &socket = *nodeAddStaticSocket(
      &ntree, &node, in_out, SOCK_FLOAT, subtype_, name_.c_str(), name_.c_str());
  bNodeSocketValueFloat &value = *(bNodeSocketValueFloat *)socket.default_value;
  value.min = min_value_;
  value.max = max_value_;
  value.value = default_value_;
  return socket;
}

bNodeSocket &Int::build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const
{
  bNodeSocket &socket = *nodeAddStaticSocket(
      &ntree, &node, in_out, SOCK_INT, subtype_, name_.c_str(), name_.c_str());
  bNodeSocketValueInt &value = *(bNodeSocketValueInt *)socket.default_value;
  value.min = min_value_;
  value.max = max_value_;
  value.value = default_value_;
  return socket;
}

bNodeSocket &Geometry::build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const
{
  bNodeSocket &socket = *nodeAddSocket(
      &ntree, &node, in_out, "NodeSocketGeometry", name_.c_str(), name_.c_str());
  return socket;
}

}  // namespace blender::nodes::decl
