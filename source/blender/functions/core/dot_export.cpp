#include "data_flow_graph.hpp"

#include <sstream>

namespace FN {
	static std::string get_id(const Node *node)
	{
		std::stringstream ss;
		ss << "\"";
		ss << (void *)node;
		ss << "\"";
		return ss.str();
	}

	static std::string get_id(Socket socket)
	{
		std::stringstream ss;
		ss << "\"";
		ss << std::to_string(socket.is_input());
		ss << std::to_string(socket.index());
		ss << "\"";
		return ss.str();
	}

	static std::string port_id(Socket socket)
	{
		std::string n = get_id(socket.node());
		std::string s = get_id(socket);
		return get_id(socket.node()) + ":" + get_id(socket);
	}

	static void insert_node_table(std::stringstream &ss, const Node *node)
	{
		ss << "<table border=\"0\" cellspacing=\"3\">";

		/* Header */
		ss << "<tr><td colspan=\"3\" align=\"center\"><b>";
		ss << node->function()->name();
		ss << "</b></td></tr>";

		/* Sockets */
		const Signature &sig = node->signature();
		uint inputs_amount = sig.inputs().size();
		uint outputs_amount = sig.outputs().size();
		uint socket_max_amount = std::max(inputs_amount, outputs_amount);
		for (uint i = 0; i < socket_max_amount; i++) {
			ss << "<tr>";
			if (i < inputs_amount) {
				Socket socket = node->input(i);
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

	static void insert_node(std::stringstream &ss, const Node *node)
	{
		ss << get_id(node) << " ";
		ss << "[style=\"filled\", fillcolor=\"#FFFFFF\", shape=\"box\"";
		ss << ", label=<";
		insert_node_table(ss, node);
		ss << ">]";
	}

	static void insert_link(std::stringstream &ss, Link link)
	{
		ss << port_id(link.from()) << " -> " << port_id(link.to());
	}

	std::string DataFlowGraph::to_dot() const
	{
		std::stringstream ss;
		ss << "digraph MyGraph {" << std::endl;
		ss << "rankdir=LR" << std::endl;

		for (const Node *node : m_nodes) {
			insert_node(ss, node);
			ss << std::endl;
		}

		for (Link link : this->all_links()) {
			insert_link(ss, link);
			ss << std::endl;
		}

		ss << "}\n";
		return ss.str();
	}
};