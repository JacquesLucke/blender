#include "inserters.hpp"
#include "registry.hpp"

#include "BLI_lazy_init.hpp"
#include "DNA_node_types.h"

#include "RNA_access.h"

namespace FN { namespace DataFlowNodes {

	static void initialize_standard_inserters(GraphInserters &inserters)
	{
		register_node_inserters(inserters);
		initialize_socket_inserters(inserters);
		register_conversion_inserters(inserters);
	}

	LAZY_INIT_REF__NO_ARG(GraphInserters, get_standard_inserters)
	{
		GraphInserters inserters;
		initialize_standard_inserters(inserters);
		return inserters;
	}


	void GraphInserters::reg_node_inserter(std::string idname, NodeInserter inserter)
	{
		BLI_assert(!m_node_inserters.contains(idname));
		m_node_inserters.add(idname, inserter);
	}

	void GraphInserters::reg_node_function(std::string idname, FunctionGetter getter)
	{
		auto inserter = [getter](
				GraphBuilder &builder,
				bNode *bnode)
			{
				SharedFunction fn = getter();
				Node *node = builder.insert_function(fn, bnode);
				builder.map_sockets(node, bnode);
			};
		this->reg_node_inserter(idname, inserter);
	}

	void GraphInserters::reg_socket_loader(std::string idname, SocketLoader loader)
	{
		BLI_assert(!m_socket_loaders.contains(idname));
		m_socket_loaders.add(idname, loader);
	}

	void GraphInserters::reg_conversion_inserter(
		std::string from_type,
		std::string to_type,
		ConversionInserter inserter)
	{
		auto key = std::pair<std::string, std::string>(from_type, to_type);
		BLI_assert(!m_conversion_inserters.contains(key));
		m_conversion_inserters.add(key, inserter);
	}

	void GraphInserters::reg_conversion_function(
		std::string from_type,
		std::string to_type,
		FunctionGetter getter)
	{
		auto inserter = [getter](
				GraphBuilder &builder,
				Socket from,
				Socket to,
				struct bNodeLink *source_link)
			{
				auto fn = getter();
				Node *node;
				if (source_link == NULL) {
					node = builder.insert_function(fn);
				}
				else {
					node = builder.insert_function(fn, source_link);
				}
				builder.insert_link(from, node->input(0));
				builder.insert_link(node->output(0), to);
			};
		this->reg_conversion_inserter(from_type, to_type, inserter);
	}

	bool GraphInserters::insert_node(
		GraphBuilder &builder,
		bNode *bnode)
	{
		NodeInserter *inserter = m_node_inserters.lookup_ptr(bnode->idname);
		if (inserter == nullptr) {
			return false;
		}
		(*inserter)(builder, bnode);

		BLI_assert(builder.verify_data_sockets_mapped(bnode));
		return true;
	}

	class SocketLoaderBody : public TupleCallBody {
	private:
		bNodeTree *m_btree;
		BSockets m_bsockets;
		SmallVector<SocketLoader> m_loaders;

	public:
		SocketLoaderBody(
			bNodeTree *btree,
			BSockets &bsockets,
			SmallVector<SocketLoader> &loaders)
			: m_btree(btree), m_bsockets(bsockets), m_loaders(loaders) {}

		void call(Tuple &UNUSED(fn_in), Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
		{
			for (uint i = 0; i < m_bsockets.size(); i++) {
				PointerRNA rna;
				bNodeSocket *bsocket = m_bsockets[i];
				auto loader = m_loaders[i];

				RNA_pointer_create(&m_btree->id,
					&RNA_NodeSocket, bsocket, &rna);
				loader(&rna, fn_out, i);
			}
		}
	};

	SmallSocketVector GraphInserters::insert_sockets(
		GraphBuilder &builder,
		BSockets &bsockets,
		BNodes &UNUSED(bnodes))
	{
		SmallVector<SocketLoader> loaders;
		OutputParameters outputs;
		for (uint i = 0; i < bsockets.size(); i++) {
			bNodeSocket *bsocket = bsockets[i];

			PointerRNA rna = builder.get_rna(bsocket);

			char data_type[64];
			RNA_string_get(&rna, "data_type", data_type);

			SocketLoader loader = m_socket_loaders.lookup(data_type);
			loaders.append(loader);
			outputs.append(OutputParameter(
				builder.query_socket_name(bsocket),
				builder.query_socket_type(bsocket)));
		}

		auto fn = SharedFunction::New("Input Sockets", Signature({}, outputs));
		fn->add_body(new SocketLoaderBody(builder.btree(), bsockets, loaders));
		Node *node = builder.insert_function(fn);

		SmallSocketVector sockets;
		for (Socket output : node->outputs()) {
			sockets.append(output);
		}
		return sockets;
	}

	bool GraphInserters::insert_link(
		GraphBuilder &builder,
		struct bNodeSocket *from_bsocket,
		struct bNodeSocket *to_bsocket,
		struct bNodeLink *source_link)
	{
		BLI_assert(builder.is_data_socket(from_bsocket));
		BLI_assert(builder.is_data_socket(to_bsocket));

		Socket from_socket = builder.lookup_socket(from_bsocket);
		Socket to_socket = builder.lookup_socket(to_bsocket);

		std::string from_type = builder.query_socket_type_name(from_bsocket);
		std::string to_type = builder.query_socket_type_name(to_bsocket);

		if (from_type == to_type) {
			builder.insert_link(from_socket, to_socket);
			return true;
		}

		auto key = std::pair<std::string, std::string>(from_type, to_type);
		if (m_conversion_inserters.contains(key)) {
			auto inserter = m_conversion_inserters.lookup(key);
			inserter(builder, from_socket, to_socket, source_link);
			return true;
		}

		return false;
	}

} } /* namespace FN::DataFlowNodes */