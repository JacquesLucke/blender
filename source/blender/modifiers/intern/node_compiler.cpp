#include "node_compiler.hpp"

#include <sstream>

namespace NodeCompiler {

AnySocket LinkSet::getOriginSocket(AnySocket socket) const
{
	assert(socket.is_input());

	for (Link link : this->links) {
		if (link.to == socket) {
			return link.from;
		}
	}

	assert(!"every input socket needs an origin");
}

AnySocket Graph::getOriginSocket(AnySocket socket) const
{
	return this->links.getOriginSocket(socket);
}

const SocketInfo *AnySocket::info() const
{
	if (this->is_input()) {
		return &this->node()->inputs[this->index()];
	}
	else {
		return &this->node()->outputs[this->index()];
	}
}

const llvm::Type *AnySocket::type() const
{
	return this->info()->type;
}

const std::string &AnySocket::debug_name() const
{
	return this->info()->debug_name;
}

std::string SimpleNode::debug_id() const
{
	std::stringstream ss;
	ss << this->debug_name << " at " << (void *)this;
	return ss.str();
}

void Graph::generateCode(
	llvm::IRBuilder<> &builder,
	std::vector<AnySocket> &inputs, std::vector<AnySocket> &outputs, std::vector<llvm::Value *> &input_values,
	llvm::IRBuilder<> *r_builder, std::vector<llvm::Value *> *r_output_values)
{

}

SocketSet Graph::findRequiredSockets(std::vector<AnySocket> &inputs, std::vector<AnySocket> &outputs)
{
	SocketSet required_sockets;

	for (AnySocket socket : outputs) {
		this->findRequiredSockets(socket, inputs, required_sockets);
	}

	return required_sockets;
}

void Graph::findRequiredSockets(AnySocket socket, std::vector<AnySocket> &inputs, SocketSet &required_sockets)
{
	if (required_sockets.find(socket) != required_sockets.end()) {
		// already found
		return;
	}

	required_sockets.insert(socket);

	if (std::find(inputs.begin(), inputs.end(), socket) != inputs.end()) {
		// is input
		return;
	}

	if (socket.is_input()) {
		AnySocket origin = this->getOriginSocket(socket);
		this->findRequiredSockets(origin, inputs, required_sockets);
	}
	else {
		for (uint i = 0; i < socket.node()->inputs.size(); i++) {
			AnySocket input = AnySocket::NewInput(socket.node(), i);
			this->findRequiredSockets(input, inputs, required_sockets);
		}
	}
}

std::string Graph::toDotFormat(std::vector<SimpleNode *> marked_nodes) const
{
	std::stringstream ss;
	ss << "digraph MyGraph {" << std::endl;

	for (SimpleNode *node : this->nodes) {
		ss << "    \"" << node->debug_id() << "\" [style=\"filled\", fillcolor=\"#FFFFFF\"]" << std::endl;
	}
	for (Link link : this->links.links) {
		ss << "    \"" << link.from.node()->debug_id() << "\" -> \"" << link.to.node()->debug_id() << "\"" << std::endl;
	}

	for (SimpleNode *node : marked_nodes) {
		ss << "    \"" << node->debug_id() << "\" [fillcolor=\"#FFAAAA\"]" << std::endl;
	}

	ss << "}" << std::endl;
	return ss.str();
}

} /* namespace NodeCompiler */