#include "nodes.hpp"

namespace FN { namespace Nodes {

	SmallMap<std::string, InsertNode> node_inserters;
	SmallMap<std::string, InsertSocket> socket_inserters;

	void initialize()
	{
		initialize_node_inserters();
		initialize_socket_inserters();
	}

	InsertNode get_node_inserter(const std::string &name)
	{
		return node_inserters.lookup_default(name, nullptr);
	}

	InsertSocket get_socket_inserter(const std::string &name)
	{
		return socket_inserters.lookup_default(name, nullptr);
	}

	void register_node_inserter(
		std::string node_idname,
		InsertNode inserter)
	{
		node_inserters.add(node_idname, inserter);
	}

	void register_socket_inserter(
		std::string socket_idname,
		InsertSocket inserter)
	{
		socket_inserters.add(socket_idname, inserter);
	}

	void register_node_function_getter__no_arg(
		std::string node_idname,
		NodeFunctionGetter_NoArg getter)
	{
		InsertNode inserter =
			[getter](bNodeTree *, bNode *bnode, SharedDataFlowGraph &graph, SocketMap &socket_map)
		{
			SharedFunction fn = getter();
			const Node *node = graph->insert(fn);
			map_node_sockets(socket_map, bnode, node);
		};

		register_node_inserter(node_idname, inserter);
	}

	void map_node_sockets(SocketMap &socket_map, bNode *bnode, const Node *node)
	{
		uint input_index = 0;
		for (bNodeSocket *bsocket : bSocketList(&bnode->inputs)) {
			socket_map.add(bsocket, node->input(input_index));
			input_index++;
		}

		uint output_index = 0;
		for (bNodeSocket *bsocket : bSocketList(&bnode->outputs)) {
			socket_map.add(bsocket, node->output(output_index));
			output_index++;
		}
	}

} }