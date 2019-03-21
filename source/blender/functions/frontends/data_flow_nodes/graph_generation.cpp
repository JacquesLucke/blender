#include "graph_generation.hpp"

#include "inserters.hpp"
#include "util_wrappers.hpp"

#include "DNA_node_types.h"
#include "FN_types.hpp"

namespace FN { namespace DataFlowNodes {

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

	static void insert_input_node(
		Builder &builder, const BuilderContext &ctx, bNode *bnode)
	{
		OutputParameters outputs;
		for (bNodeSocket *bsocket : bSocketList(&bnode->outputs)) {
			if (ctx.is_data_socket(bsocket)) {
				SharedType &type = ctx.type_of_socket(bsocket);
				outputs.append(OutputParameter(bsocket->name, type));
			}
		}

		auto fn = SharedFunction::New("Function Input", Signature({}, outputs));
		Node *node = builder.insert_function(fn);
		builder.map_data_sockets(node, bnode, ctx);
	}

	static void insert_output_node(
		Builder &builder, const BuilderContext &ctx, bNode *bnode)
	{
		InputParameters inputs;
		for (bNodeSocket *bsocket : bSocketList(&bnode->inputs)) {
			if (ctx.is_data_socket(bsocket)) {
				SharedType &type = ctx.type_of_socket(bsocket);
				inputs.append(InputParameter(bsocket->name, type));
			}
		}

		auto fn = SharedFunction::New("Function Output", Signature(inputs, {}));
		Node *node = builder.insert_function(fn);
		builder.map_data_sockets(node, bnode, ctx);
	}

	Optional<FunctionGraph> generate_function_graph(struct bNodeTree *btree)
	{
		auto graph = SharedDataFlowGraph::New();
		SocketMap socket_map;

		Builder builder(graph, socket_map);
		BuilderContext ctx(btree);
		GraphInserters &inserters = get_standard_inserters();

		bNode *input_node;
		bNode *output_node;
		find_interface_nodes(btree, &input_node, &output_node);

		for (bNode *bnode : bNodeList(&btree->nodes)) {
			if (bnode == input_node || bnode == output_node) {
				continue;
			}

			if (!inserters.insert_node(builder, ctx, bnode)) {
				return {};
			}
		}

		SmallSocketVector input_sockets;
		SmallSocketVector output_sockets;

		if (input_node != nullptr) {
			insert_input_node(builder, ctx, input_node);
			for (bNodeSocket *bsocket : bSocketList(&input_node->outputs)) {
				if (ctx.is_data_socket(bsocket)) {
					input_sockets.append(socket_map.lookup(bsocket));
				}
			}
		}
		if (output_node != nullptr) {
			insert_output_node(builder, ctx, output_node);
			for (bNodeSocket *bsocket : bSocketList(&output_node->inputs)) {
				if (ctx.is_data_socket(bsocket)) {
					output_sockets.append(socket_map.lookup(bsocket));
				}
			}
		}

		for (bNodeLink *blink : bLinkList(&btree->links)) {
			if (!inserters.insert_link(builder, ctx, blink)) {
				return {};
			}
		}

		BSockets unlinked_inputs;
		SmallSocketVector node_inputs;
		for (bNode *bnode : bNodeList(&btree->nodes)) {
			for (bNodeSocket *bsocket : bSocketList(&bnode->inputs)) {
				if (ctx.is_data_socket(bsocket)) {
					Socket socket = socket_map.lookup(bsocket);
					if (!socket.is_linked()) {
						unlinked_inputs.append(bsocket);
						node_inputs.append(socket);
					}
				}
			}
		}

		SmallSocketVector new_origins = inserters.insert_sockets(builder, ctx, unlinked_inputs);
		BLI_assert(unlinked_inputs.size() == new_origins.size());

		for (uint i = 0; i < unlinked_inputs.size(); i++) {
			builder.insert_link(new_origins[i], node_inputs[i]);
		}

		graph->freeze();
		FunctionGraph fgraph(graph, input_sockets, output_sockets);
		return fgraph;
	}

} } /* namespace FN::DataFlowNodes */