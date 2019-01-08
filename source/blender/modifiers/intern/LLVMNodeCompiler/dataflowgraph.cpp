#include "core.hpp"

namespace LLVMNodeCompiler {

void DataFlowGraph::addNode(Node *node)
{
	this->nodes.push_back(node);
}

void DataFlowGraph::addLink(AnySocket from, AnySocket to)
{
	this->links.links.push_back(Link(from, to));
}

void DataFlowGraph::generateCode(
	llvm::IRBuilder<> &builder,
	SocketArraySet &inputs, SocketArraySet &outputs, std::vector<llvm::Value *> &input_values,
	std::vector<llvm::Value *> &r_output_values)
{
	assert(outputs.size() > 0);
	assert(inputs.size() == input_values.size());

	SocketSet required_sockets = this->findRequiredSockets(inputs, outputs);

	SocketValueMap values;
	for (uint i = 0; i < inputs.size(); i++) {
		values.add(inputs[i], input_values[i]);
	}

	SocketSet forwarded_sockets;
	for (AnySocket socket : outputs) {
		this->generateCodeForSocket(builder, socket, values, required_sockets, forwarded_sockets);
		r_output_values.push_back(values.lookup(socket));
	}
}

void DataFlowGraph::generateCodeForSocket(
	llvm::IRBuilder<> &builder,
	AnySocket socket,
	SocketValueMap &values,
	SocketSet &required_sockets,
	SocketSet &forwarded_sockets)
{
	if (values.contains(socket)) {
		/* do nothing */
	}
	else if (socket.is_input()) {
		AnySocket origin = this->getOriginSocket(socket);
		this->generateCodeForSocket(builder, origin, values, required_sockets, forwarded_sockets);
		this->forwardOutputIfNecessary(builder, origin, values, required_sockets, forwarded_sockets);
	}
	else if (socket.is_output()) {
		Node *node = socket.node();
		std::vector<llvm::Value *> input_values;
		for (uint i = 0; i < node->inputs().size(); i++) {
			AnySocket input = node->Input(i);
			this->generateCodeForSocket(builder, input, values, required_sockets, forwarded_sockets);
			input_values.push_back(values.lookup(input));
		}

		std::vector<llvm::Value *> output_values;
		node->buildIR(builder, input_values, output_values);

		for (uint i = 0; i < node->outputs().size(); i++) {
			AnySocket output = node->Output(i);
			values.add(output, output_values[i]);
			this->forwardOutputIfNecessary(builder, output, values, required_sockets, forwarded_sockets);
		}
	}
	else {
		assert(!"should never happen");
	}
}

void DataFlowGraph::forwardOutputIfNecessary(
	llvm::IRBuilder<> &builder,
	AnySocket output,
	SocketValueMap &values,
	SocketSet &required_sockets,
	SocketSet &forwarded_sockets)
{
	if (!forwarded_sockets.contains(output)) {
		this->forwardOutput(builder, output, values, required_sockets);
		forwarded_sockets.add(output);
	}
}

void DataFlowGraph::forwardOutput(
	llvm::IRBuilder<> &builder,
	AnySocket output,
	SocketValueMap &values,
	SocketSet &required_sockets)
{
	llvm::Value *value_to_forward = values.lookup(output);
	Type *type = output.type();

	SocketArraySet targets;
	for (AnySocket target : this->getTargetSockets(output)) {
		if (required_sockets.contains(target) && !values.contains(target)) {
			assert(type == target.type());
			targets.add(target);
		}
	}

	if (targets.size() == 0) {
		type->buildFreeIR(builder, value_to_forward);
	}
	else if (targets.size() == 1) {
		values.add(targets[0], value_to_forward);
	}
	else {
		values.add(targets[0], value_to_forward);
		for (uint i = 1; i < targets.size(); i++) {
			AnySocket target = targets[i];
			llvm::Value *copied_value = type->buildCopyIR(builder, value_to_forward);
			values.add(target, copied_value);
		}
	}
}

SocketSet DataFlowGraph::findRequiredSockets(SocketSet &inputs, SocketSet &outputs)
{
	SocketSet required_sockets;

	for (AnySocket socket : outputs) {
		this->findRequiredSockets(socket, inputs, required_sockets);
	}

	return required_sockets;
}

void DataFlowGraph::findRequiredSockets(AnySocket socket, SocketSet &inputs, SocketSet &required_sockets)
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
		Node *node = socket.node();
		for (uint i = 0; i < node->inputs().size(); i++) {
			AnySocket input = AnySocket::NewInput(socket.node(), i);
			this->findRequiredSockets(input, inputs, required_sockets);
		}
		return;
	}
}

AnySocket DataFlowGraph::getOriginSocket(AnySocket socket) const
{
	return this->links.getOriginSocket(socket);
}

SocketSet DataFlowGraph::getTargetSockets(AnySocket socket) const
{
	return this->links.getTargetSockets(socket);
}

} /* namespace LLVMNodeCompiler */