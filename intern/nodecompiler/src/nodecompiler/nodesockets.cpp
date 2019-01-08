#include "core.hpp"

namespace LLVMNodeCompiler {

void NodeSockets::add(SocketInfo socket)
{
	this->sockets.push_back(socket);
}

void NodeSockets::add(std::string debug_name, Type *type)
{
	this->sockets.push_back(SocketInfo(debug_name, type));
}

uint NodeSockets::size() const
{
	return this->sockets.size();
}

const SocketInfo &NodeSockets::operator[](const int index) const
{
	return this->sockets[index];
}

NodeSockets::const_iterator NodeSockets::begin() const
{
	return this->sockets.begin();
}
NodeSockets::const_iterator NodeSockets::end() const
{
	return this->sockets.end();
}

} /* namespace LLVMNodeCompiler */