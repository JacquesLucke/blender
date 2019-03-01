#include "inserters.hpp"
#include "registry.hpp"

#include "BLI_lazy_init.hpp"
#include "DNA_node_types.h"

namespace FN { namespace DataFlowNodes {

	static void initialize_standard_inserters(GraphInserters &inserters)
	{
		register_node_inserters(inserters);
		initialize_socket_inserters(inserters);
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
				const BuilderContext &UNUSED(ctx),
				bNode *bnode)
			{
				SharedFunction fn = getter();
				const Node *node = builder.insert_function(fn);
				builder.map_sockets(node, bnode);
			};
		this->reg_node_inserter(idname, inserter);
	}

	void GraphInserters::reg_socket_inserter(std::string idname, SocketInserter inserter)
	{
		BLI_assert(!m_node_inserters.contains(idname));
		m_socket_inserters.add(idname, inserter);
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
		return true;
	}

	Optional<Socket> GraphInserters::insert_socket(
		Builder &builder,
		const BuilderContext &ctx,
		struct bNodeSocket *bsocket)
	{
		SocketInserter *inserter = m_socket_inserters.lookup_ptr(bsocket->idname);
		if (inserter == nullptr) {
			return {};
		}
		Socket socket = (*inserter)(builder, ctx, bsocket);
		return socket;
	}

} } /* namespace FN::DataFlowNodes */