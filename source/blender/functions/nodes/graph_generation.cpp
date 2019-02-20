#include "graph_generation.hpp"

namespace FN { namespace Nodes {

	using namespace Types;

	static SharedType &get_type_of_socket(bNodeTree *UNUSED(btree), bNodeSocket *bsocket)
	{
		if (STREQ(bsocket->idname, "fn_FloatSocket")) {
			return get_float_type();
		}
		else if (STREQ(bsocket->idname, "fn_VectorSocket")) {
			return get_fvec3_type();
		}
		else {
			BLI_assert(false);
			return *(SharedType *)nullptr;
		}
	}


	static void insert_input_node(
		bNodeTree *btree,
		bNode *bnode,
		SharedDataFlowGraph &graph,
		SocketMap &socket_map)
	{
		OutputParameters outputs;
		for (bNodeSocket *bsocket : bSocketList(&bnode->outputs)) {
			SharedType &type = get_type_of_socket(btree, bsocket);
			outputs.append(OutputParameter(bsocket->name, type));
		}

		auto fn = SharedFunction::New("Function Input", Signature({}, outputs));
		const Node *node = graph->insert(fn);

		uint i = 0;
		for (bNodeSocket *bsocket : bSocketList(&bnode->outputs)) {
			socket_map.add(bsocket, node->output(i));
			i++;
		}
	}

	static void insert_output_node(
		bNodeTree *btree,
		bNode *bnode,
		SharedDataFlowGraph &graph,
		SocketMap &socket_map)
	{
		InputParameters inputs;
		for (bNodeSocket *bsocket : bSocketList(&bnode->inputs)) {
			SharedType &type = get_type_of_socket(btree, bsocket);
			inputs.append(InputParameter(bsocket->name, type));
		}

		auto fn = SharedFunction::New("Function Output", Signature(inputs, {}));
		const Node *node = graph->insert(fn);

		uint i = 0;
		for (bNodeSocket *bsocket : bSocketList(&bnode->inputs)) {
			socket_map.add(bsocket, node->input(i));
			i++;
		}
	}

	static bool is_input_node(const bNode *bnode)
	{
		return STREQ(bnode->idname, "fn_FunctionInputNode");
	}

	static bool is_output_node(const bNode *bnode)
	{
		return STREQ(bnode->idname, "fn_FunctionOutputNode");
	}

	static void find_interface_nodes(
		bNodeTree *btree,
		bNode **r_input,
		bNode **r_output)
	{
		bNode *input = nullptr;
		bNode *output = nullptr;

		for (bNode *bnode : bNodeList(&btree->nodes)) {
			if (is_input_node(bnode)) input = bnode;
			if (is_output_node(bnode)) output = bnode;
		}

		*r_input = input;
		*r_output = output;
	}

	FunctionGraph btree_to_graph(bNodeTree *btree)
	{
		SocketMap socket_map;
		auto graph = SharedDataFlowGraph::New();

		bNode *input_node;
		bNode *output_node;
		find_interface_nodes(btree, &input_node, &output_node);

		for (bNode *bnode : bNodeList(&btree->nodes)) {
			if (bnode == input_node || bnode == output_node) {
				continue;
			}

			auto inserter = get_node_inserter(bnode->idname);
			inserter(btree, bnode, graph, socket_map);
		}

		SmallSocketVector input_sockets;
		SmallSocketVector output_sockets;

		if (input_node != nullptr) {
			insert_input_node(btree, input_node, graph, socket_map);
			for (bNodeSocket *bsocket : bSocketList(&input_node->outputs)) {
				input_sockets.append(socket_map.lookup(bsocket));
			}
		}
		if (output_node != nullptr) {
			insert_output_node(btree, output_node, graph, socket_map);
			for (bNodeSocket *bsocket : bSocketList(&output_node->inputs)) {
				output_sockets.append(socket_map.lookup(bsocket));
			}
		}

		for (bNodeLink *blink : bLinkList(&btree->links)) {
			Socket from = socket_map.lookup(blink->fromsock);
			Socket to = socket_map.lookup(blink->tosock);
			graph->link(from, to);
		}

		for (bNode *bnode : bNodeList(&btree->nodes)) {
			for (bNodeSocket *bsocket : bSocketList(&bnode->inputs)) {
				Socket socket = socket_map.lookup(bsocket);
				if (!socket.is_linked()) {
					auto inserter = get_socket_inserter(bsocket->idname);
					Socket new_origin = inserter(btree, bsocket, graph);
					graph->link(new_origin, socket);
				}
			}
		}

		graph->freeze();
		FunctionGraph fgraph(graph, input_sockets, output_sockets);

		return fgraph;

	}

} }