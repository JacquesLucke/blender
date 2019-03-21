#include "builder.hpp"

#include "DNA_node_types.h"
#include "FN_types.hpp"
#include "util_wrappers.hpp"

#include "RNA_access.h"

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

	void Builder::map_data_sockets(Node *node, struct bNode *bnode, const BuilderContext &ctx)
	{
		uint input_index = 0;
		for (bNodeSocket *bsocket : bSocketList(&bnode->inputs)) {
			if (ctx.is_data_socket(bsocket)) {
				this->map_socket(node->input(input_index), bsocket);
				input_index++;
			}
		}

		uint output_index = 0;
		for (bNodeSocket *bsocket : bSocketList(&bnode->outputs)) {
			if (ctx.is_data_socket(bsocket)) {
				this->map_socket(node->output(output_index), bsocket);
				output_index++;
			}
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

	Socket Builder::lookup_socket(struct bNodeSocket *bsocket)
	{
		BLI_assert(m_socket_map.contains(bsocket));
		return m_socket_map.lookup(bsocket);
	}


	struct bNodeTree *BuilderContext::btree() const
	{
		return m_btree;
	}

	struct ID *BuilderContext::btree_id() const
	{
		return &m_btree->id;
	}

	bool BuilderContext::is_data_socket(bNodeSocket *bsocket) const
	{
		PointerRNA ptr;
		this->get_rna(bsocket, &ptr);
		return RNA_struct_find_property(&ptr, "data_type") != NULL;
	}

	SharedType &BuilderContext::type_by_name(const char *data_type) const
	{
		if (STREQ(data_type, "Float")) {
			return Types::get_float_type();
		}
		else if (STREQ(data_type, "Integer")) {
			return Types::get_int32_type();
		}
		else if (STREQ(data_type, "Vector")) {
			return Types::get_fvec3_type();
		}
		else if (STREQ(data_type, "Float List")) {
			return Types::get_float_list_type();
		}
		else if (STREQ(data_type, "Vector List")) {
			return Types::get_fvec3_list_type();
		}
		else if (STREQ(data_type, "Integer List")) {
			return Types::get_int32_list_type();
		}
		else {
			BLI_assert(false);
			return *(SharedType *)nullptr;
		}
	}

	SharedType &BuilderContext::type_of_socket(bNodeSocket *bsocket) const
	{
		std::string data_type = this->socket_type_string(bsocket);
		return this->type_by_name(data_type.c_str());
	}

	void BuilderContext::get_rna(bNode *bnode, PointerRNA *ptr) const
	{
		RNA_pointer_create(
			this->btree_id(), &RNA_Node,
			bnode, ptr);
	}

	void BuilderContext::get_rna(bNodeSocket *bsocket, PointerRNA *ptr) const
	{
		RNA_pointer_create(
			this->btree_id(), &RNA_NodeSocket,
			bsocket, ptr);
	}

	SharedType &BuilderContext::type_from_rna(bNode *bnode, const char *prop_name) const
	{
		PointerRNA ptr;
		this->get_rna(bnode, &ptr);
		char type_name[64];
		RNA_string_get(&ptr, prop_name, type_name);
		return this->type_by_name(type_name);
	}

	std::string BuilderContext::socket_type_string(bNodeSocket *bsocket) const
	{
		BLI_assert(this->is_data_socket(bsocket));
		PointerRNA ptr;
		this->get_rna(bsocket, &ptr);
		char type_name[64];
		RNA_string_get(&ptr, "data_type", type_name);
		return type_name;
	}

} } /* namespace FN::DataFlowNodes */