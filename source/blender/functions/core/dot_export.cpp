#include "FN_core.hpp"

#include <sstream>
#include "WM_api.h"

namespace FN {
static std::string get_id(DFGB_Node *node)
{
  std::stringstream ss;
  ss << "\"";
  ss << (void *)node;
  ss << "\"";
  return ss.str();
}

static std::string get_id(DFGB_Socket socket)
{
  std::stringstream ss;
  ss << "\"";
  ss << std::to_string(socket.is_input());
  ss << std::to_string(socket.index());
  ss << "\"";
  return ss.str();
}

static std::string port_id(DFGB_Socket socket)
{
  std::string n = get_id(socket.node());
  std::string s = get_id(socket);
  return get_id(socket.node()) + ":" + get_id(socket);
}

static void insert_node_table(std::stringstream &ss, DFGB_Node *node)
{
  ss << "<table border=\"0\" cellspacing=\"3\">";

  /* Header */
  ss << "<tr><td colspan=\"3\" align=\"center\"><b>";
  ss << node->function()->name();
  ss << "</b></td></tr>";

  /* Sockets */
  uint inputs_amount = node->function()->input_amount();
  uint outputs_amount = node->function()->output_amount();
  uint socket_max_amount = std::max(inputs_amount, outputs_amount);
  for (uint i = 0; i < socket_max_amount; i++) {
    ss << "<tr>";
    if (i < inputs_amount) {
      DFGB_Socket socket = node->input(i);
      ss << "<td align=\"left\" port=" << get_id(socket) << ">";
      ss << socket.name();
      ss << "</td>";
    }
    else {
      ss << "<td></td>";
    }
    ss << "<td></td>";
    if (i < outputs_amount) {
      ss << "<td align=\"right\" port=" << get_id(node->output(i)) << ">";
      ss << node->output(i).name();
      ss << "</td>";
    }
    else {
      ss << "<td></td>";
    }
    ss << "</tr>";
  }

  ss << "</table>";
}

static void insert_node(std::stringstream &ss, DFGB_Node *node)
{
  ss << get_id(node) << " ";
  ss << "[style=\"filled\", fillcolor=\"#FFFFFF\", shape=\"box\"";
  ss << ", label=<";
  insert_node_table(ss, node);
  ss << ">]";
}

static void dot__insert_link(std::stringstream &ss, DFGB_Link link)
{
  ss << port_id(link.from()) << " -> " << port_id(link.to());
}

std::string DataFlowGraphBuilder::to_dot()
{
  std::stringstream ss;
  ss << "digraph MyGraph {" << std::endl;
  ss << "rankdir=LR" << std::endl;

  for (DFGB_Node *node : m_nodes) {
    insert_node(ss, node);
    ss << std::endl;
  }

  for (DFGB_Link link : this->links()) {
    dot__insert_link(ss, link);
    ss << std::endl;
  }

  ss << "}\n";
  return ss.str();
}

void DataFlowGraphBuilder::to_dot__clipboard()
{
  std::string dot = this->to_dot();
  WM_clipboard_text_set(dot.c_str(), false);
}
};  // namespace FN
