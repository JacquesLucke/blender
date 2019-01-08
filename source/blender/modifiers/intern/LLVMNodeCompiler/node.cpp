#include <sstream>

#include "core.hpp"

namespace LLVMNodeCompiler {

Node::~Node() { }

const NodeSockets &Node::inputs() const
{
	return this->m_inputs;
}
const NodeSockets &Node::outputs() const
{
	return this->m_outputs;
}

AnySocket Node::Input(const uint index) const
{
	return AnySocket::NewInput(this, index);
}
AnySocket Node::Output(const uint index) const
{
	return AnySocket::NewOutput(this, index);
}

void Node::addInput(std::string debug_name, Type *type)
{
	this->m_inputs.add(debug_name, type);
}
void Node::addOutput(std::string debug_name, Type *type)
{
	this->m_outputs.add(debug_name, type);
}

std::ostream &operator<<(std::ostream &stream, const Node &node)
{
	stream << node.debug_name();
	return stream;
}

std::string Node::str_id() const
{
	std::stringstream ss;
	ss << (void *)this;
	return ss.str();
}
std::string Node::debug_name() const
{
	return "no name";
}

} /* namespace LLVMNodeCompiler */