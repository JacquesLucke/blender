#pragma once

#include "BLI_listbase_wrapper.hpp"

struct bNode;
struct bNodeLink;
struct bNodeSocket;

namespace FN { namespace DataFlowNodes {

	using bNodeList = ListBaseWrapper<struct bNode, true>;
	using bLinkList = ListBaseWrapper<struct bNodeLink, true>;
	using bSocketList = ListBaseWrapper<struct bNodeSocket, true>;

} } /* namespace FN::DataFlowNodes */