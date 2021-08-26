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

class NodeSocketsBuilder;

class SocketDecl {
 protected:
  std::string name_;
  std::string identifier_;

  friend NodeSocketsBuilder;

 public:
  virtual bNodeSocket &build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const = 0;
  virtual bool matches(const bNodeSocket &socket) const = 0;
  virtual void try_copy_value(bNodeSocket &dst_socket, const bNodeSocket &src_socket) const;
};

class NodeSocketBuilderState {
 private:
  Vector<std::unique_ptr<SocketDecl>> inputs_;
  Vector<std::unique_ptr<SocketDecl>> outputs_;

  friend NodeSocketsBuilder;

 public:
  void build(bNodeTree &ntree, bNode &node) const;
  bool matches(const bNode &node) const;

  Span<std::unique_ptr<SocketDecl>> inputs() const;
  Span<std::unique_ptr<SocketDecl>> outputs() const;
};

class NodeSocketsBuilder {
 private:
  NodeSocketBuilderState &state_;

 public:
  NodeSocketsBuilder(NodeSocketBuilderState &state);

  template<typename DeclType> DeclType &add_input(StringRef name, StringRef identifier = "");
  template<typename DeclType> DeclType &add_output(StringRef name, StringRef identifier = "");
};

/* --------------------------------------------------------------------
 * NodeSocketsBuilder inline methods.
 */

inline NodeSocketsBuilder::NodeSocketsBuilder(NodeSocketBuilderState &state) : state_(state)
{
}

template<typename DeclType>
inline DeclType &NodeSocketsBuilder::add_input(StringRef name, StringRef identifier)
{
  static_assert(std::is_base_of_v<SocketDecl, DeclType>);
  std::unique_ptr<DeclType> socket_decl = std::make_unique<DeclType>();
  DeclType &ref = *socket_decl;
  ref.name_ = name;
  ref.identifier_ = identifier.is_empty() ? name : identifier;
  state_.inputs_.append(std::move(socket_decl));
  return ref;
}

template<typename DeclType>
inline DeclType &NodeSocketsBuilder::add_output(StringRef name, StringRef identifier)
{
  static_assert(std::is_base_of_v<SocketDecl, DeclType>);
  std::unique_ptr<DeclType> socket_decl = std::make_unique<DeclType>();
  DeclType &ref = *socket_decl;
  ref.name_ = name;
  ref.identifier_ = identifier.is_empty() ? name : identifier;
  state_.outputs_.append(std::move(socket_decl));
  return ref;
}

/* --------------------------------------------------------------------
 * NodeSocketBuilderState inline methods.
 */

inline Span<std::unique_ptr<SocketDecl>> NodeSocketBuilderState::inputs() const
{
  return inputs_;
}

inline Span<std::unique_ptr<SocketDecl>> NodeSocketBuilderState::outputs() const
{
  return outputs_;
}

}  // namespace blender::nodes
