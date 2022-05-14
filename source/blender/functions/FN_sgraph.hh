/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup fn
 *
 * A `SGraph` ("socket graph") is a graph data structure wrapper. It wraps graphs where each node
 * has input and output sockets and links only exist between an input and output of two different
 * nodes.
 *
 * In itself, it can not hold a graph. Instead different concrete graph data structures can
 * implement a "SGraphAccessor". This allows generic algorithms to work on different concrete graph
 * data structures.
 *
 * Generic algorithms don't work on the adapter directly, but on SGraph<Adapter>, which adds some
 * utilities on top of the adapter. This minimizes the amount of functionality that has to be
 * implemented by an adapter, while still making generic algorithms somewhat convenient to write.
 */

#include "BLI_hash.hh"
#include "BLI_type_traits.hh"
#include "BLI_utildefines.h"
#include "BLI_utility_mixins.hh"

namespace blender::fn::sgraph {

template<typename Accessor> class SGraphT : NonCopyable, NonMovable {
 public:
  using Graph = typename Accessor::Types::Graph;
  using Node = typename Accessor::Types::Node;
  using Link = typename Accessor::Types::Link;
  using Socket = typename Accessor::Types::Socket;
  using InSocket = typename Accessor::Types::InSocket;
  using OutSocket = typename Accessor::Types::OutSocket;

 private:
  Graph graph_;

 public:
  SGraphT(Graph graph) : graph_(std::move(graph))
  {
  }

  int node_inputs_num(const Node &node) const
  {
    return Accessor::node_inputs_num(graph_, node);
  }

  int node_outputs_num(const Node &node) const
  {
    return Accessor::node_outputs_num(graph_, node);
  }

  IndexRange node_inputs_range(const Node &node) const
  {
    return IndexRange{this->node_inputs_num(node)};
  }

  IndexRange node_outputs_range(const Node &node) const
  {
    return IndexRange{this->node_outputs_num(node)};
  }

  InSocket node_input(const Node &node, const int index) const
  {
    return Accessor::node_input(graph_, node, index);
  }

  OutSocket node_output(const Node &node, const int index) const
  {
    return Accessor::node_output(graph_, node, index);
  }

  template<typename F> void foreach_node(const F &f) const
  {
    static_assert(is_callable_v<F, void, Node>);
    Accessor::foreach_node(graph_, f);
  }

  template<typename F> void foreach_link_to_input(const InSocket &socket, const F &f) const
  {
    static_assert(is_callable_v<F, void, Link>);
    Accessor::foreach_link_to_input(graph_, socket, f);
  }

  template<typename F> void foreach_link_from_output(const OutSocket &socket, const F &f) const
  {
    static_assert(is_callable_v<F, void, Link>);
    Accessor::foreach_link_from_output(graph_, socket, f);
  }

  template<typename F> void foreach_link(const F &f) const
  {
    static_assert(is_callable_v<F, void, Link>);
    this->foreach_node([&](const Node &node) {
      for (const int i : this->node_outputs_range(node)) {
        const OutSocket socket = this->node_output(node, i);
        this->foreach_link_from_output(socket, f);
      }
    });
  }

  OutSocket link_from_socket(const Link &link) const
  {
    return Accessor::link_from_socket(graph_, link);
  }

  InSocket link_to_socket(const Link &link) const
  {
    return Accessor::link_to_socket(graph_, link);
  }

  Node link_from_node(const Link &link) const
  {
    return this->node_of_output(this->link_from_socket(link));
  }

  Node link_to_node(const Link &link) const
  {
    return this->node_of_input(this->link_to_socket(link));
  }

  Node node_of_input(const InSocket &socket) const
  {
    return Accessor::node_of_input(graph_, socket);
  }

  Node node_of_output(const OutSocket &socket) const
  {
    return Accessor::node_of_output(graph_, socket);
  }

  int index_of_input(const InSocket &socket) const
  {
    return Accessor::index_of_input(graph_, socket);
  }

  int index_of_output(const OutSocket &socket) const
  {
    return Accessor::index_of_output(graph_, socket);
  }

  std::string node_debug_name(const Node &node) const
  {
    return Accessor::node_debug_name(graph_, node);
  }

  std::string input_debug_name(const InSocket &socket) const
  {
    return Accessor::input_debug_name(graph_, socket);
  }

  std::string output_debug_name(const OutSocket &socket) const
  {
    return Accessor::output_debug_name(graph_, socket);
  }
};

}  // namespace blender::fn::sgraph
