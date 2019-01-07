#include <sstream>

#include "core.hpp"

namespace LLVMNodeCompiler {

const static std::string indent1 = "  ";
const static std::string indent2 = indent1 + indent1;
const static std::string indent3 = indent2 + indent1;
const static std::string indent4 = indent2 + indent2;

static std::string dot_id(Node *node)
{
	return "\"" + node->str_id() + "\"";
}

static std::string dot_id(AnySocket socket)
{
	return "\"" + socket.str_id() + "\"";
}

static std::string dot_port_id(AnySocket socket)
{
	return dot_id(socket.node()) + ":" + dot_id(socket);
}

static void dot_InsertNode_WithSockets(std::stringstream &ss, Node *node)
{
	ss << indent1 << dot_id(node) << " [style=\"filled\", fillcolor=\"#FFFFFF\", shape=\"square\", label=<" << std::endl;
	ss << indent2 << "<table border=\"0\" cellspacing=\"3\">" << std::endl;
	ss << indent3 << "<tr><td colspan=\"3\" align=\"center\"><b>" << node->debug_name() << "</b></td></tr>" << std::endl;
	uint socket_amount_max = std::max(node->inputs().size(), node->outputs().size());
	for (uint i = 0; i < socket_amount_max; i++) {
		ss << indent3 << "<tr>" << std::endl;

		if (i < node->inputs().size()) {
			AnySocket socket = node->Input(i);
			ss << indent4 << "<td align=\"left\" port=" << dot_id(socket) << ">" << socket.debug_name() << "</td>" << std::endl;
		}
		else {
			ss << indent4 << "<td></td>" << std::endl;
		}

		ss << indent4 << "<td></td>" << std::endl;

		if (i < node->outputs().size()) {
			AnySocket socket = node->Output(i);
			ss << indent4 << "<td align=\"right\" port=" << dot_id(socket) << ">" << socket.debug_name() << "</td>" << std::endl;
		}
		else {
			ss << indent4 << "<td></td>" << std::endl;
		}

		ss << indent3 << "</tr>" << std::endl;
	}
	ss << indent2 << "</table>" << std::endl;
	ss << indent1 << ">]" << std::endl;
}

static void dot_MarkNode(std::stringstream &ss, Node *node)
{
	ss << indent1 << dot_id(node) << " [style=\"filled\", fillcolor=\"#FFAAAA\"]" << std::endl;
}

static void dot_InsertLink_WithSockets(std::stringstream &ss, Link link)
{
	ss << indent1 << dot_port_id(link.from) << " -> " << dot_port_id(link.to) << std::endl;
}

std::string DataFlowGraph::toDotFormat(std::vector<Node *> marked_nodes) const
{
	std::stringstream ss;
	ss << "digraph MyGraph {" << std::endl;
	ss << indent1 << "rankdir=LR" << std::endl;

	for (Node *node : this->nodes) {
		dot_InsertNode_WithSockets(ss, node);
	}

	for (Link link : this->links.links) {
		dot_InsertLink_WithSockets(ss, link);
	}

	for (Node *node : marked_nodes) {
		dot_MarkNode(ss, node);
	}

	ss << "}" << std::endl;
	return ss.str();
}

} /* namespace LLVMNodeCompiler */