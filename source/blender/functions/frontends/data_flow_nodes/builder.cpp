#include "builder.hpp"

#include "DNA_node_types.h"
#include "FN_types.hpp"
#include "util_wrappers.hpp"

namespace FN { namespace DataFlowNodes {

	Node *Builder::insert_function(SharedFunction &function)
	{
		return m_graph->insert(function);
	}

	void Builder::insert_link(Socket a, Socket b)
	{
		m_graph->link(a, b);
	}

	void Builder::map_socket(Socket socket, bNodeSocket *bsocket)
	{
		m_socket_map.add(bsocket, socket);
	}

	void Builder::map_sockets(Node *node, struct bNode *bnode)
	{
		uint input_index = 0;
		for (bNodeSocket *bsocket : bSocketList(&bnode->inputs)) {
			this->map_socket(node->input(input_index), bsocket);
			input_index++;
		}

		uint output_index = 0;
		for (bNodeSocket *bsocket : bSocketList(&bnode->outputs)) {
			this->map_socket(node->output(output_index), bsocket);
			output_index++;
		}
	}

	void Builder::map_input(Socket socket, struct bNode *bnode, uint index)
	{
		BLI_assert(socket.is_input());
		auto bsocket = (bNodeSocket *)BLI_findlink(&bnode->inputs, index);
		this->map_socket(socket, bsocket);
	}

	void Builder::map_output(Socket socket, struct bNode *bnode, uint index)
	{
		BLI_assert(socket.is_output());
		auto bsocket = (bNodeSocket *)BLI_findlink(&bnode->outputs, index);
		this->map_socket(socket, bsocket);
	}


	struct bNodeTree *BuilderContext::btree() const
	{
		return m_btree;
	}

	struct ID *BuilderContext::btree_id() const
	{
		return &m_btree->id;
	}

	SharedType &BuilderContext::type_of_socket(bNodeSocket *bsocket) const
	{
		if (STREQ(bsocket->idname, "fn_FloatSocket")) {
			return Types::get_float_type();
		}
		else if (STREQ(bsocket->idname, "fn_IntegerSocket")) {
			return Types::get_int32_type();
		}
		else if (STREQ(bsocket->idname, "fn_VectorSocket")) {
			return Types::get_fvec3_type();
		}
		else if (STREQ(bsocket->idname, "fn_FloatListSocket")) {
			return Types::get_float_list_type();
		}
		else {
			BLI_assert(false);
			return *(SharedType *)nullptr;
		}
	}

} } /* namespace FN::DataFlowNodes */