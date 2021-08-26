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

bool Float::matches(const bNodeSocket &socket) const
{
  if (socket.type != SOCK_FLOAT) {
    return false;
  }
  if (socket.typeinfo->subtype != subtype_) {
    return false;
  }
  if (socket.name != name_) {
    return false;
  }
  if (socket.identifier != name_) {
    return false;
  }
  bNodeSocketValueFloat &value = *(bNodeSocketValueFloat *)socket.default_value;
  if (value.min != min_value_) {
    return false;
  }
  if (value.max != max_value_) {
    return false;
  }
  return true;
}

void Float::try_copy_value(bNodeSocket &dst_socket, const bNodeSocket &src_socket) const
{
  bNodeSocketValueFloat &dst_value = *(bNodeSocketValueFloat *)dst_socket.default_value;
  if (src_socket.type == SOCK_FLOAT) {
    const bNodeSocketValueFloat &src_value = *(const bNodeSocketValueFloat *)
                                                  src_socket.default_value;
    dst_value.value = src_value.value;
  }
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

bool Int::matches(const bNodeSocket &socket) const
{
  if (socket.type != SOCK_INT) {
    return false;
  }
  if (socket.typeinfo->subtype != subtype_) {
    return false;
  }
  if (socket.name != name_) {
    return false;
  }
  if (socket.identifier != name_) {
    return false;
  }
  bNodeSocketValueInt &value = *(bNodeSocketValueInt *)socket.default_value;
  if (value.min != min_value_) {
    return false;
  }
  if (value.max != max_value_) {
    return false;
  }
  return true;
}

void Int::try_copy_value(bNodeSocket &dst_socket, const bNodeSocket &src_socket) const
{
  bNodeSocketValueInt &dst_value = *(bNodeSocketValueInt *)dst_socket.default_value;
  if (src_socket.type == SOCK_INT) {
    const bNodeSocketValueInt &src_value = *(const bNodeSocketValueInt *)src_socket.default_value;
    dst_value.value = src_value.value;
  }
}

bNodeSocket &Geometry::build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const
{
  bNodeSocket &socket = *nodeAddSocket(
      &ntree, &node, in_out, "NodeSocketGeometry", name_.c_str(), name_.c_str());
  return socket;
}

bool Geometry::matches(const bNodeSocket &socket) const
{
  if (socket.type != SOCK_GEOMETRY) {
    return false;
  }
  if (socket.name != name_) {
    return false;
  }
  if (socket.identifier != name_) {
    return false;
  }
  return true;
}

}  // namespace blender::nodes::decl
