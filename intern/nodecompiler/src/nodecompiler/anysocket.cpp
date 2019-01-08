#include <sstream>

#include "core.hpp"

namespace LLVMNodeCompiler {

bool AnySocket::is_output() const
{
	return this->m_is_output;
}

bool AnySocket::is_input() const
{
	return !this->m_is_output;
}

const Node *AnySocket::node() const
{
	return this->m_node;
}

uint AnySocket::index() const
{
	return this->m_index;
}

const SocketInfo *AnySocket::info() const
{
	if (this->is_input()) {
		return &this->node()->inputs()[this->index()];
	}
	else {
		return &this->node()->outputs()[this->index()];
	}
}

Type *AnySocket::type() const
{
	return this->info()->type;
}

std::string AnySocket::debug_name() const
{
	return this->info()->debug_name;
}

std::string AnySocket::str_id() const
{
	std::stringstream ss;
	ss << (void *)this->node() << this->is_output() << this->index();
	return ss.str();
}

AnySocket AnySocket::NewInput(const Node *node, uint index)
{
	return AnySocket(node, false, index);
}

AnySocket AnySocket::NewOutput(const Node *node, uint index)
{
	return AnySocket(node, true, index);
}

bool operator==(const AnySocket &left, const AnySocket &right)
{
	return (
			left.m_node == right.m_node
		&& left.m_is_output == right.m_is_output
		&& left.m_index == right.m_index);
}

std::ostream &operator<<(std::ostream &stream, const AnySocket &socket)
{
	stream << socket.debug_name();
	return stream;
}

AnySocket::AnySocket(const Node *node, bool is_output, uint index)
	: m_node(node), m_is_output(is_output), m_index(index) {}


} /* namespace LLVMNodeCompiler */