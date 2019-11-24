#include "BLI_dot_export.h"

namespace BLI {
namespace DotExport {

std::string DirectedGraph::to_dot_string() const
{
  std::stringstream ss;
  ss << "digraph {\n";
  this->export__declare_nodes_and_clusters(ss);
  ss << "\n";

  for (auto &edge : m_edges) {
    edge->export__as_edge_statement(ss);
    ss << "\n";
  }

  ss << "}\n";
  return ss.str();
}

std::string UndirectedGraph::to_dot_string() const
{
  std::stringstream ss;
  ss << "graph {\n";
  this->export__declare_nodes_and_clusters(ss);
  ss << "\n";

  for (auto &edge : m_edges) {
    edge->export__as_edge_statement(ss);
    ss << "\n";
  }

  ss << "}\n";
  return ss.str();
}

void Graph::export__declare_nodes_and_clusters(std::stringstream &ss) const
{
  for (auto &node : m_nodes) {
    node->export__as_declaration(ss);
  }

  for (auto &cluster : m_clusters) {
    cluster->export__declare_nodes_and_clusters(ss);
  }
}

void Cluster::export__declare_nodes_and_clusters(std::stringstream &ss) const
{
  ss << "subgraph cluster_" << (void *)this << " {\n";

  ss << "graph ";
  m_attributes.export__as_bracket_list(ss);
  ss << "\n\n";

  for (auto &node : m_nodes) {
    node->export__as_declaration(ss);
  }

  for (auto &cluster : m_clusters) {
    cluster->export__declare_nodes_and_clusters(ss);
  }

  ss << "}\n";
}

void DirectedEdge::export__as_edge_statement(std::stringstream &ss) const
{
  m_a.to_dot_string(ss);
  ss << " -> ";
  m_b.to_dot_string(ss);
  ss << " ";
  m_attributes.export__as_bracket_list(ss);
}

void UndirectedEdge::export__as_edge_statement(std::stringstream &ss) const
{
  m_a.to_dot_string(ss);
  ss << " -- ";
  m_b.to_dot_string(ss);
  ss << " ";
  m_attributes.export__as_bracket_list(ss);
}

void AttributeList::export__as_bracket_list(std::stringstream &ss) const
{
  ss << "[";
  for (auto item : m_attributes.items()) {
    if (StringRef(item.value).startswith("<")) {
      /* Don't draw the quotes, this is an html-like value. */
      ss << item.key << "=" << item.value << ", ";
    }
    else {
      ss << item.key << "=\"" << item.value << "\", ";
    }
  }
  ss << "]";
}

Node &Graph::new_node(StringRef label)
{
  Node *node = new Node();
  m_nodes.append(std::unique_ptr<Node>(node));
  node->set_attribute("label", label);
  return *node;
}

Cluster &Graph::new_cluster()
{
  Cluster *cluster = new Cluster(*this, nullptr);
  m_clusters.append(std::unique_ptr<Cluster>(cluster));
  return *cluster;
}

Node &Cluster::new_node(StringRef label)
{
  Node *node = new Node();
  m_nodes.append(std::unique_ptr<Node>(node));
  node->set_attribute("label", label);
  return *node;
}

UndirectedEdge &UndirectedGraph::new_edge(NodePort a, NodePort b)
{
  UndirectedEdge *edge = new UndirectedEdge(a, b);
  m_edges.append(std::unique_ptr<UndirectedEdge>(edge));
  return *edge;
}

DirectedEdge &DirectedGraph::new_edge(NodePort from, NodePort to)
{
  DirectedEdge *edge = new DirectedEdge(from, to);
  m_edges.append(std::unique_ptr<DirectedEdge>(edge));
  return *edge;
}

void Node::export__as_id(std::stringstream &ss) const
{
  ss << '"' << (const void *)this << '"';
}

void Node::export__as_declaration(std::stringstream &ss) const
{
  this->export__as_id(ss);
  ss << " ";
  m_attributes.export__as_bracket_list(ss);
  ss << "\n";
}

void NodePort::to_dot_string(std::stringstream &ss) const
{
  m_node->export__as_id(ss);
  if (m_port_name.has_value()) {
    ss << ":" << m_port_name.value();
  }
}

namespace Utils {

NodeWithSocketsWrapper::NodeWithSocketsWrapper(Node &node,
                                               StringRef name,
                                               ArrayRef<std::string> input_names,
                                               ArrayRef<std::string> output_names)
    : m_node(&node)
{
  std::stringstream ss;

  ss << "<<table border=\"0\" cellspacing=\"3\">";

  /* Header */
  ss << "<tr><td colspan=\"3\" align=\"center\"><b>";
  ss << name;
  ss << "</b></td></tr>";

  /* Sockets */
  uint socket_max_amount = std::max(input_names.size(), output_names.size());
  for (uint i = 0; i < socket_max_amount; i++) {
    ss << "<tr>";
    if (i < input_names.size()) {
      StringRef name = input_names[i];
      ss << "<td align=\"left\" port=\"in" << i << "\">";
      ss << name;
      ss << "</td>";
    }
    else {
      ss << "<td></td>";
    }
    ss << "<td></td>";
    if (i < output_names.size()) {
      StringRef name = output_names[i];
      ss << "<td align=\"right\" port=\"out" << i << "\">";
      ss << name;
      ss << "</td>";
    }
    else {
      ss << "<td></td>";
    }
    ss << "</tr>";
  }

  ss << "</table>>";

  m_node->set_attribute("label", ss.str());
  m_node->set_attribute("shape", "box");
}

}  // namespace Utils

}  // namespace DotExport
}  // namespace BLI
