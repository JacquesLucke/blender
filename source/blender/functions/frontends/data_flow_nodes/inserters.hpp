#pragma once

#include "builder.hpp"
#include <functional>
#include "BLI_optional.hpp"

namespace FN { namespace DataFlowNodes {

	typedef std::function<void (
		Builder &builder,
		const BuilderContext &ctx,
		struct bNode *bnode)> NodeInserter;

	typedef std::function<Socket (
		Builder &builder,
		const BuilderContext &ctx,
		struct bNodeSocket *bsocket)> SocketInserter;

	typedef std::function<SharedFunction ()> FunctionGetter;

	class GraphInserters {
	private:
		SmallMap<std::string, NodeInserter> m_node_inserters;
		SmallMap<std::string, SocketInserter> m_socket_inserters;

	public:
		void reg_node_inserter(std::string idname, NodeInserter inserter);
		void reg_node_function(std::string idname, FunctionGetter getter);

		void reg_socket_inserter(std::string idname, SocketInserter inserter);

		bool insert_node(
			Builder &builder,
			const BuilderContext &ctx,
			struct bNode *bnode);

		Optional<Socket> insert_socket(
			Builder &builder,
			const BuilderContext &ctx,
			struct bNodeSocket *bsocket);
	};

	GraphInserters &get_standard_inserters();

} } /* namespace FN::DataFlowNodes */