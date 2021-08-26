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

#include <type_traits>

#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "DNA_node_types.h"

namespace blender::nodes {

class SocketDecl {
 protected:
  std::string name_;

 public:
  SocketDecl(std::string name) : name_(std::move(name))
  {
  }

  virtual bNodeSocket &build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const = 0;
};

class NodeSocketsBuilder;

class NodeSocketBuilderState {
 private:
  Vector<std::unique_ptr<SocketDecl>> inputs_;
  Vector<std::unique_ptr<SocketDecl>> outputs_;

  friend NodeSocketsBuilder;

 public:
  void build(bNodeTree &ntree, bNode &node) const;
};

class NodeSocketsBuilder {
 private:
  NodeSocketBuilderState &state_;

 public:
  NodeSocketsBuilder(NodeSocketBuilderState &state);

  template<typename DeclType> DeclType &add_input(StringRef name);
  template<typename DeclType> DeclType &add_output(StringRef name);
};

/* --------------------------------------------------------------------
 * NodeSocketsBuilder inline methods.
 */

inline NodeSocketsBuilder::NodeSocketsBuilder(NodeSocketBuilderState &state) : state_(state)
{
}

template<typename DeclType> inline DeclType &NodeSocketsBuilder::add_input(StringRef name)
{
  static_assert(std::is_base_of_v<SocketDecl, DeclType>);
  std::unique_ptr<DeclType> socket_decl = std::make_unique<DeclType>(name);
  DeclType &ref = *socket_decl;
  state_.inputs_.append(std::move(socket_decl));
  return ref;
}

template<typename DeclType> inline DeclType &NodeSocketsBuilder::add_output(StringRef name)
{
  static_assert(std::is_base_of_v<SocketDecl, DeclType>);
  std::unique_ptr<DeclType> socket_decl = std::make_unique<DeclType>(name);
  DeclType &ref = *socket_decl;
  state_.outputs_.append(std::move(socket_decl));
  return ref;
}

}  // namespace blender::nodes
