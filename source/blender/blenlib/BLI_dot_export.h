#ifndef __BLI_DOT_EXPORT_H__
#define __BLI_DOT_EXPORT_H__

/**
 * Language grammar: https://www.graphviz.org/doc/info/lang.html
 * Attributes: https://www.graphviz.org/doc/info/attrs.html
 * Node Shapes: https://www.graphviz.org/doc/info/shapes.html
 * Preview: https://dreampuf.github.io/GraphvizOnline
 */

#include "BLI_vector.h"
#include "BLI_optional.h"
#include "BLI_string_map.h"
#include "BLI_map.h"
#include "BLI_utility_mixins.h"

#include <sstream>

namespace BLI {
namespace DotExport {

class Graph;
class DirectedGraph;
class UndirectedGraph;
class Node;
class NodePort;
class DirectedEdge;
class UndirectedEdge;
class Cluster;
class AttributeList;

class AttributeList {
 private:
  Map<std::string, std::string> m_attributes;

 public:
  void export__as_bracket_list(std::stringstream &ss) const;

  void set(StringRef key, StringRef value)
  {
    m_attributes.add_override(key, value);
  }
};

class Graph {
 private:
  Vector<std::unique_ptr<Node>> m_nodes;
  Vector<std::unique_ptr<Cluster>> m_clusters;

  friend Cluster;

 public:
  Node &new_node(StringRef label);
  Cluster &new_cluster();

  void export__declare_nodes_and_clusters(std::stringstream &ss) const;
};

class UndirectedGraph final : public Graph {
 private:
  Vector<std::unique_ptr<UndirectedEdge>> m_edges;

 public:
  std::string to_dot_string() const;

  UndirectedEdge &new_edge(NodePort a, NodePort b);
};

class DirectedGraph final : public Graph {
 private:
  Vector<std::unique_ptr<DirectedEdge>> m_edges;

 public:
  std::string to_dot_string() const;

  DirectedEdge &new_edge(NodePort from, NodePort to);
};

class Cluster {
 private:
  AttributeList m_attributes;
  Graph *m_graph;
  Cluster *m_parent;
  Vector<std::unique_ptr<Cluster>> m_clusters;
  Vector<std::unique_ptr<Node>> m_nodes;

 public:
  Cluster(Graph &graph, Cluster *parent) : m_graph(&graph), m_parent(parent)
  {
  }

  void export__declare_nodes_and_clusters(std::stringstream &ss) const;

  Cluster &new_cluster();

  Node &new_node(StringRef label);

  void set_attribute(StringRef key, StringRef value)
  {
    m_attributes.set(key, value);
  }
};

class NodePort {
 private:
  Node *m_node;
  Optional<std::string> m_port_name;

 public:
  NodePort(Node &node, Optional<std::string> port_name = {})
      : m_node(&node), m_port_name(std::move(port_name))
  {
  }

  void to_dot_string(std::stringstream &ss) const;
};

class Edge : BLI::NonCopyable, BLI::NonMovable {
 protected:
  AttributeList m_attributes;
  NodePort m_a;
  NodePort m_b;

 public:
  Edge(NodePort a, NodePort b) : m_a(std::move(a)), m_b(std::move(b))
  {
  }

  void set_attribute(StringRef key, StringRef value)
  {
    m_attributes.set(key, value);
  }
};

class DirectedEdge : public Edge {
 public:
  DirectedEdge(NodePort from, NodePort to) : Edge(std::move(from), std::move(to))
  {
  }

  void export__as_edge_statement(std::stringstream &ss) const;
};

class UndirectedEdge : public Edge {
 public:
  UndirectedEdge(NodePort a, NodePort b) : Edge(std::move(a), std::move(b))
  {
  }

  void export__as_edge_statement(std::stringstream &ss) const;
};

class Node {
 private:
  AttributeList m_attributes;

 public:
  const AttributeList &attributes() const
  {
    return m_attributes;
  }

  AttributeList &attributes()
  {
    return m_attributes;
  }

  void set_attribute(StringRef key, StringRef value)
  {
    m_attributes.set(key, value);
  }

  void export__as_id(std::stringstream &ss) const;

  void export__as_declaration(std::stringstream &ss) const;
};

namespace Utils {

class NodeWithSocketsWrapper {
 private:
  Node *m_node;

 public:
  NodeWithSocketsWrapper(Node &node,
                         StringRef name,
                         ArrayRef<std::string> input_names,
                         ArrayRef<std::string> output_names);

  NodePort input(uint index) const
  {
    std::string port = "\"in" + std::to_string(index) + "\"";
    return NodePort(*m_node, port);
  }

  NodePort output(uint index) const
  {
    std::string port = "\"out" + std::to_string(index) + "\"";
    return NodePort(*m_node, port);
  }
};

}  // namespace Utils

}  // namespace DotExport
}  // namespace BLI

#endif /* __BLI_DOT_EXPORT_H__ */
