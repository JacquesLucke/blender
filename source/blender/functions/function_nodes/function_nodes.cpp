#include "function_nodes.hpp"
#include "nodes/nodes.hpp"

#include "BLI_listbase.h"

#include "BKE_node.h"
#include "BKE_idprop.h"

#include "RNA_access.h"

#include "DNA_object_types.h"

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

	static SharedFunction get_function_with_inputs(const SmallTypeVector &types)
	{
		InputParameters inputs;
		for (SharedType &type : types) {
			inputs.append(InputParameter("Input", type));
		}
		return SharedFunction::New("Inputs", Signature(inputs, {}));
	}

	static SharedFunction get_function_with_outputs(const SmallTypeVector &types)
	{
		OutputParameters outputs;
		for (SharedType &type : types) {
			outputs.append(OutputParameter("Output", type));
		}
		return SharedFunction::New("Outputs", Signature({}, outputs));
	}

	static void insert_output_node(
		bNodeTree *btree,
		bNode *bnode,
		SharedDataFlowGraph &graph,
		SocketMap &socket_map)
	{
		SmallTypeVector types;
		for (bNodeSocket *bsocket : bSocketList(&bnode->inputs)) {
			types.append(get_type_of_socket(btree, bsocket));
		}

		SharedFunction fn = get_function_with_inputs(types);
		const Node *node = graph->insert(fn);

		uint i = 0;
		for (bNodeSocket *bsocket : bSocketList(&bnode->inputs)) {
			socket_map.add(bsocket, node->input(i));
			i++;
		}
	}

	static void insert_input_node(
		bNodeTree *btree,
		bNode *bnode,
		SharedDataFlowGraph &graph,
		SocketMap &socket_map)
	{
		SmallTypeVector types;
		for (bNodeSocket *bsocket : bSocketList(&bnode->outputs)) {
			types.append(get_type_of_socket(btree, bsocket));
		}

		SharedFunction fn = get_function_with_outputs(types);
		const Node *node = graph->insert(fn);

		uint i = 0;
		for (bNodeSocket *bsocket : bSocketList(&bnode->outputs)) {
			socket_map.add(bsocket, node->output(i));
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

	FunctionGraph FunctionNodeTree::to_function_graph() const
	{
		bNodeTree *btree = this->orig_tree();

		SocketMap socket_map;

		SharedDataFlowGraph graph = SharedDataFlowGraph::New();

		SmallSocketVector input_sockets;
		SmallSocketVector output_sockets;

		for (bNode *bnode : this->nodes()) {
			if (is_input_node(bnode)) {
				insert_input_node(btree, bnode, graph, socket_map);
				for (bNodeSocket *bsocket : bSocketList(&bnode->outputs)) {
					Socket socket = socket_map.lookup(bsocket);
					input_sockets.append(socket);
				}
			}
			else if (is_output_node(bnode)) {
				insert_output_node(btree, bnode, graph, socket_map);
				for (bNodeSocket *bsocket : bSocketList(&bnode->inputs)) {
					Socket socket = socket_map.lookup(bsocket);
					output_sockets.append(socket);
				}
			}
			else {
				auto insert = get_node_inserter(bnode->idname);
				insert(btree, bnode, graph, socket_map);
			}
		}

		for (bNodeLink *blink : this->links()) {
			Socket from = socket_map.lookup(blink->fromsock);
			Socket to = socket_map.lookup(blink->tosock);
			graph->link(from, to);
		}

		for (bNode *bnode : this->nodes()) {
			for (bNodeSocket *bsocket : bSocketList(&bnode->inputs)) {
				Socket socket = socket_map.lookup(bsocket);
				if (!socket.is_linked()) {
					auto insert = get_socket_inserter(bsocket->idname);
					Socket new_origin = insert(btree, bsocket, graph);
					graph->link(new_origin, socket);
				}
			}
		}

		graph->freeze();
		FunctionGraph fgraph(graph, input_sockets, output_sockets);

		return fgraph;
	}

} } /* FN::Nodes */