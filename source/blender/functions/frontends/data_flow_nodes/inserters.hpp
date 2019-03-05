#pragma once

#include "builder.hpp"
#include <functional>
#include "BLI_optional.hpp"
#include "FN_tuple_call.hpp"
#include "util_wrappers.hpp"

struct PointerRNA;

namespace FN { namespace DataFlowNodes {

	typedef std::function<void (
		Builder &builder,
		const BuilderContext &ctx,
		struct bNode *bnode)> NodeInserter;

	typedef std::function<void (
		PointerRNA *socket_rna_ptr,
		Tuple &dst,
		uint index)> SocketLoader;

	typedef std::function<SharedFunction ()> FunctionGetter;

	class GraphInserters {
	private:
		SmallMap<std::string, NodeInserter> m_node_inserters;
		SmallMap<std::string, SocketLoader> m_socket_loaders;

	public:
		void reg_node_inserter(std::string idname, NodeInserter inserter);
		void reg_node_function(std::string idname, FunctionGetter getter);

		void reg_socket_loader(std::string idname, SocketLoader loader);

		bool insert_node(
			Builder &builder,
			const BuilderContext &ctx,
			struct bNode *bnode);

		SmallSocketVector insert_sockets(
			Builder &builder,
			const BuilderContext &ctx,
			BSockets &bsockets);
	};

	GraphInserters &get_standard_inserters();

} } /* namespace FN::DataFlowNodes */