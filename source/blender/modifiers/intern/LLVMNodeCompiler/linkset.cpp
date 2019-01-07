#include "core.hpp"

namespace LLVMNodeCompiler {

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



SocketSet LinkSet::getTargetSockets(AnySocket socket) const
{
	assert(socket.is_output());

	SocketSet targets;
	for (Link link : this->links) {
		if (link.from == socket) {
			targets.add(link.to);
		}
	}

	return targets;
}

} /* namespace LLVMNodeCompiler */