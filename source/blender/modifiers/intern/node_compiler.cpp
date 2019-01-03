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
	llvm::IRBuilder<> *builder,
	std::vector<AnySocket> &inputs, std::vector<AnySocket> &outputs, std::vector<llvm::Value *> &input_values,
	llvm::IRBuilder<> **r_builder, std::vector<llvm::Value *> &r_output_values)
{
	assert(inputs.size() == input_values.size());

	SocketValueMap values;
	for (uint i = 0; i < inputs.size(); i++) {
		values.add(inputs[i], input_values[i]);
	}

	for (AnySocket socket : outputs) {
		llvm::IRBuilder<> *next_builder;

		llvm::Value *value = this->generateCodeForSocket(socket, builder, values, &next_builder);
		r_output_values.push_back(value);

		builder = next_builder;
	}

	*r_builder = builder;
}

llvm::Value *Graph::generateCodeForSocket(
	AnySocket socket,
	llvm::IRBuilder<> *builder,
	SocketValueMap &values,
	llvm::IRBuilder<> **r_builder)
{
	if (values.contains(socket)) {
		*r_builder = builder;
		return values.lookup(socket);
	}

	if (socket.is_input()) {
		AnySocket origin = this->getOriginSocket(socket);
		llvm::Value *value = this->generateCodeForSocket(origin, builder, values, r_builder);
		values.add(socket, value);
		return value;
	}

	if (socket.is_output()) {
		SimpleNode *node = socket.node();
		std::vector<llvm::Value *> input_values;
		for (uint i = 0; i < node->inputs.size(); i++) {
			llvm::IRBuilder<> *next_builder;

			llvm::Value *value = this->generateCodeForSocket(node->Input(i), builder, values, &next_builder);
			input_values.push_back(value);

			builder = next_builder;
		}

		std::vector<llvm::Value *> output_values;
		node->generateCode(input_values, builder, output_values, r_builder);

		for (uint i = 0; i < node->outputs.size(); i++) {
			values.add(node->Output(i), output_values[i]);
		}

		return values.lookup(socket);
	}

	assert(!"should never happen");
}

SocketSet Graph::findRequiredSockets(SocketSet &inputs, SocketSet &outputs)
{
	SocketSet required_sockets;

	for (AnySocket socket : outputs.elements()) {
		this->findRequiredSockets(socket, inputs, required_sockets);
	}

	return required_sockets;
}

void Graph::findRequiredSockets(AnySocket socket, SocketSet &inputs, SocketSet &required_sockets)
{
	if (required_sockets.contains(socket)) {
		return;
	}

	required_sockets.add(socket);

	if (inputs.contains(socket)) {
		return;
	}

	if (socket.is_input()) {
		AnySocket origin = this->getOriginSocket(socket);
		this->findRequiredSockets(origin, inputs, required_sockets);
		return;
	}

	if (socket.is_output()) {
		SimpleNode *node = socket.node();
		for (uint i = 0; i < node->inputs.size(); i++) {
			AnySocket input = AnySocket::NewInput(socket.node(), i);
			this->findRequiredSockets(input, inputs, required_sockets);
		}
		return;
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