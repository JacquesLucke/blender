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
				Builder &builder,
				const BuilderContext &ctx,
				bNode *bnode)
			{
				SharedFunction fn = getter();
				Node *node = builder.insert_function(fn, ctx.btree(), bnode);
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
				Builder &builder,
				const BuilderContext &UNUSED(ctx),
				Socket from,
				Socket to)
			{
				auto fn = getter();
				Node *node = builder.insert_function(fn);
				builder.insert_link(from, node->input(0));
				builder.insert_link(node->output(0), to);
			};
		this->reg_conversion_inserter(from_type, to_type, inserter);
	}

	bool GraphInserters::insert_node(
		Builder &builder,
		const BuilderContext &ctx,
		bNode *bnode)
	{
		NodeInserter *inserter = m_node_inserters.lookup_ptr(bnode->idname);
		if (inserter == nullptr) {
			return false;
		}
		(*inserter)(builder, ctx, bnode);

		BLI_assert(builder.verify_data_sockets_mapped(bnode, ctx));
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
				PointerRNA ptr;
				bNodeSocket *bsocket = m_bsockets[i];
				auto loader = m_loaders[i];

				RNA_pointer_create(&m_btree->id,
					&RNA_NodeSocket, bsocket, &ptr);
				loader(&ptr, fn_out, i);
			}
		}
	};

	SmallSocketVector GraphInserters::insert_sockets(
		Builder &builder,
		const BuilderContext &ctx,
		BSockets &bsockets)
	{
		SmallVector<SocketLoader> loaders;
		OutputParameters outputs;
		for (auto *bsocket : bsockets) {
			PointerRNA ptr;
			RNA_pointer_create(
				ctx.btree_id(), &RNA_NodeSocket,
				bsocket, &ptr);

			char data_type[64];
			RNA_string_get(&ptr, "data_type", data_type);

			SocketLoader loader = m_socket_loaders.lookup(data_type);
			loaders.append(loader);
			outputs.append(OutputParameter(bsocket->name, ctx.type_of_socket(bsocket)));
		}

		auto fn = SharedFunction::New("Input Sockets", Signature({}, outputs));
		fn->add_body(new SocketLoaderBody(ctx.btree(), bsockets, loaders));
		Node *node = builder.insert_function(fn);

		SmallSocketVector sockets;
		for (Socket output : node->outputs()) {
			sockets.append(output);
		}
		return sockets;
	}

	bool GraphInserters::insert_link(
		Builder &builder,
		const BuilderContext &ctx,
		struct bNodeLink *blink)
	{
		BLI_assert(ctx.is_data_socket(blink->fromsock));
		BLI_assert(ctx.is_data_socket(blink->tosock));

		Socket from_socket = builder.lookup_socket(blink->fromsock);
		Socket to_socket = builder.lookup_socket(blink->tosock);

		std::string from_type = ctx.socket_type_string(blink->fromsock);
		std::string to_type = ctx.socket_type_string(blink->tosock);

		if (from_type == to_type) {
			builder.insert_link(from_socket, to_socket);
			return true;
		}

		auto key = std::pair<std::string, std::string>(from_type, to_type);
		if (m_conversion_inserters.contains(key)) {
			auto inserter = m_conversion_inserters.lookup(key);
			inserter(builder, ctx, from_socket, to_socket);
			return true;
		}

		return false;
	}

} } /* namespace FN::DataFlowNodes */