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
				Node *node = builder.insert_function(fn);
				builder.map_sockets(node, bnode);
			};
		this->reg_node_inserter(idname, inserter);
	}

	void GraphInserters::reg_socket_loader(std::string idname, SocketLoader loader)
	{
		BLI_assert(!m_socket_loaders.contains(idname));
		m_socket_loaders.add(idname, loader);
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

		void call(Tuple &UNUSED(fn_in), Tuple &fn_out) const override
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
			SocketLoader loader = m_socket_loaders.lookup(bsocket->idname);
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

} } /* namespace FN::DataFlowNodes */