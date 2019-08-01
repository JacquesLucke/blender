#include "FN_core.hpp"

#include <sstream>
#include "WM_api.h"

namespace FN {
static std::string get_id(BuilderNode *node)
{
  std::stringstream ss;
  ss << "\"";
  ss << (void *)node;
  ss << "\"";
  return ss.str();
}

static std::string get_id(BuilderSocket *socket)
{
  std::stringstream ss;
  ss << "\"";
  ss << (void *)socket;
  ss << "\"";
  return ss.str();
}

static std::string port_id(BuilderSocket *socket)
{
  return get_id(socket);
}

static void insert_node_table(std::stringstream &ss, BuilderNode *node)
{
  ss << "<table border=\"0\" cellspacing=\"3\">";

  /* Header */
  ss << "<tr><td colspan=\"3\" align=\"center\"><b>";
  ss << node->function()->name();
  ss << "</b></td></tr>";

  /* Sockets */
  auto inputs = node->inputs();
  auto outputs = node->outputs();
  uint socket_max_amount = std::max(inputs.size(), outputs.size());
  for (uint i = 0; i < socket_max_amount; i++) {
    ss << "<tr>";
    if (i < inputs.size()) {
      BuilderInputSocket *socket = inputs[i];
      ss << "<td align=\"left\" port=" << get_id(socket) << ">";
      ss << socket->name();
      ss << "</td>";
    }
    else {
      ss << "<td></td>";
    }
    ss << "<td></td>";
    if (i < outputs.size()) {
      BuilderOutputSocket *socket = outputs[i];
      ss << "<td align=\"right\" port=" << get_id(socket) << ">";
      ss << socket->name();
      ss << "</td>";
    }
    else {
      ss << "<td></td>";
    }
    ss << "</tr>";
  }

  ss << "</table>";
}

static void insert_node(std::stringstream &ss, BuilderNode *node)
{
  ss << get_id(node) << " ";
  ss << "[style=\"filled\", fillcolor=\"#FFFFFF\", shape=\"box\"";
  ss << ", label=<";
  insert_node_table(ss, node);
  ss << ">]";
}

static void dot__insert_link(std::stringstream &ss,
                             BuilderOutputSocket *from,
                             BuilderInputSocket *to)
{
  ss << port_id(from) << " -> " << port_id(to);
}

std::string DataGraphBuilder::to_dot()
{
  std::stringstream ss;
  ss << "digraph MyGraph {" << std::endl;
  ss << "rankdir=LR" << std::endl;

  for (BuilderNode *node : m_nodes) {
    insert_node(ss, node);
    ss << std::endl;
  }

  for (BuilderNode *node : m_nodes) {
    for (BuilderInputSocket *input : node->inputs()) {
      if (input->origin() != nullptr) {
        dot__insert_link(ss, input->origin(), input);
        ss << std::endl;
      }
    }
  }

  ss << "}\n";
  return ss.str();
}

void DataGraphBuilder::to_dot__clipboard()
{
  std::string dot = this->to_dot();
  WM_clipboard_text_set(dot.c_str(), false);
}
};  // namespace FN
