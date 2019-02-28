#pragma once

#include "FN_all.hpp"
#include "BKE_node.h"
#include "BLI_listbase_wrapper.hpp"
#include "BLI_optional.hpp"
#include <functional>

namespace FN { namespace DataFlowNodes {

	using bNodeList = ListBaseWrapper<bNode, true>;
	using bLinkList = ListBaseWrapper<bNodeLink, true>;
	using bSocketList = ListBaseWrapper<bNodeSocket, true>;
	using SocketMap = SmallMap<bNodeSocket *, Socket>;

	typedef std::function<void (
		bNodeTree *,
		bNode *,
		SharedDataFlowGraph &,
		SocketMap &)> InsertNode;

	typedef std::function<Socket (
		bNodeTree *,
		bNodeSocket *,
		SharedDataFlowGraph &)> InsertSocket;

	typedef std::function<SharedFunction ()> NodeFunctionGetter_NoArg;

	Optional<InsertNode> get_node_inserter(const std::string &idname);
	Optional<InsertSocket> get_socket_inserter(const std::string &idname);


	void register_node_inserter(
		std::string node_idname,
		InsertNode inserter);

	void register_socket_inserter(
		std::string socket_idname,
		InsertSocket inserter);

	void register_node_function_getter__no_arg(
		std::string node_idname,
		NodeFunctionGetter_NoArg getter);

	void map_node_sockets(
		SocketMap &socket_map,
		bNode *bnode,
		const Node *node);

	void initialize();


	void initialize_node_inserters();
	void initialize_socket_inserters();

} }