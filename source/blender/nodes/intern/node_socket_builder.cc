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

#include "NOD_node_socket_builder.hh"

namespace blender::nodes {

void NodeSocketBuilderState::build(bNodeTree &ntree, bNode &node) const
{
  for (const std::unique_ptr<SocketDecl> &decl : inputs_) {
    decl->build(ntree, node, SOCK_IN);
  }
  for (const std::unique_ptr<SocketDecl> &decl : outputs_) {
    decl->build(ntree, node, SOCK_OUT);
  }
}

bool NodeSocketBuilderState::matches(const bNode &node) const
{
  auto check_sockets = [&](ListBase sockets, Span<std::unique_ptr<SocketDecl>> socket_decls) {
    const int tot_sockets = BLI_listbase_count(&sockets);
    if (tot_sockets != socket_decls.size()) {
      return false;
    }
    int i;
    LISTBASE_FOREACH_INDEX (const bNodeSocket *, socket, &sockets, i) {
      const SocketDecl &socket_decl = *socket_decls[i];
      if (!socket_decl.matches(*socket)) {
        return false;
      }
    }
    return true;
  };

  if (!check_sockets(node.inputs, inputs_)) {
    return false;
  }
  if (!check_sockets(node.outputs, outputs_)) {
    return false;
  }
  return true;
}

void SocketDecl::try_copy_value(bNodeSocket &UNUSED(dst_socket),
                                const bNodeSocket &UNUSED(src_socket)) const
{
}

}  // namespace blender::nodes
