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
#include "BLI_set.h"
#include "BLI_utility_mixins.h"

#include "BLI_dot_export_attribute_enums.h"

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
  AttributeList m_attributes;
  Vector<std::unique_ptr<Node>> m_nodes;
  Vector<std::unique_ptr<Cluster>> m_clusters;

  Set<Node *> m_top_level_nodes;
  Set<Cluster *> m_top_level_clusters;

  friend Cluster;
  friend Node;

 public:
  Node &new_node(StringRef label);
  Cluster &new_cluster(StringRef label = "");

  void export__declare_nodes_and_clusters(std::stringstream &ss) const;

  void set_attribute(StringRef key, StringRef value)
  {
    m_attributes.set(key, value);
  }

  void set_rankdir(Attr_rankdir::Enum rankdir)
  {
    this->set_attribute("rankdir", Attr_rankdir::to_string(rankdir));
  }

  void set_random_cluster_bgcolors();
};

class Cluster {
 private:
  AttributeList m_attributes;
  Graph &m_graph;
  Cluster *m_parent = nullptr;
  Set<Cluster *> m_children;
  Set<Node *> m_nodes;

  friend Graph;
  friend Node;

  Cluster(Graph &graph) : m_graph(graph)
  {
  }

 public:
  void export__declare_nodes_and_clusters(std::stringstream &ss) const;

  void set_attribute(StringRef key, StringRef value)
  {
    m_attributes.set(key, value);
  }

  void set_parent_cluster(Cluster *cluster);
  void set_parent_cluster(Cluster &cluster)
  {
    this->set_parent_cluster(&cluster);
  }

  void set_random_cluster_bgcolors();
};

class Node {
 private:
  AttributeList m_attributes;
  Graph &m_graph;
  Cluster *m_cluster = nullptr;

  friend Graph;

  Node(Graph &graph) : m_graph(graph)
  {
  }

 public:
  const AttributeList &attributes() const
  {
    return m_attributes;
  }

  AttributeList &attributes()
  {
    return m_attributes;
  }

  void set_parent_cluster(Cluster *cluster);
  void set_parent_cluster(Cluster &cluster)
  {
    this->set_parent_cluster(&cluster);
  }

  void set_attribute(StringRef key, StringRef value)
  {
    m_attributes.set(key, value);
  }

  void set_shape(Attr_shape::Enum shape)
  {
    this->set_attribute("shape", Attr_shape::to_string(shape));
  }

  /* See https://www.graphviz.org/doc/info/attrs.html#k:color. */
  void set_background_color(StringRef name)
  {
    this->set_attribute("fillcolor", name);
    this->set_attribute("style", "filled");
  }

  void export__as_id(std::stringstream &ss) const;

  void export__as_declaration(std::stringstream &ss) const;
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

  void set_arrowhead(Attr_arrowType::Enum type)
  {
    this->set_attribute("arrowhead", Attr_arrowType::to_string(type));
  }

  void set_arrowtail(Attr_arrowType::Enum type)
  {
    this->set_attribute("arrowtail", Attr_arrowType::to_string(type));
  }

  void set_dir(Attr_dirType::Enum type)
  {
    this->set_attribute("dir", Attr_dirType::to_string(type));
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

namespace Utils {

std::string color_attr_from_hsv(float h, float s, float v);

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
