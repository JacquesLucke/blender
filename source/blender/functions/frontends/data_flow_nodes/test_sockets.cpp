#include "registry.hpp"

#include "FN_types.hpp"
#include "FN_functions.hpp"

#include "RNA_access.h"

namespace FN { namespace DataFlowNodes {

	static Socket insert_float_socket(
		Builder &builder,
		const BuilderContext &ctx,
		struct bNodeSocket *bsocket)
	{
		auto fn = Functions::float_socket_input(ctx.btree(), bsocket);
		Node *node = builder.insert_function(fn);
		return node->output(0);
	}

	static Socket insert_vector_socket(
		Builder &builder,
		const BuilderContext &ctx,
		struct bNodeSocket *bsocket)
	{
		auto fn = Functions::vector_socket_input(ctx.btree(), bsocket);
		Node *node = builder.insert_function(fn);
		return node->output(0);
	}

	void initialize_socket_inserters(GraphInserters &inserters)
	{
		inserters.reg_socket_inserter("fn_FloatSocket", insert_float_socket);
		inserters.reg_socket_inserter("fn_VectorSocket", insert_vector_socket);
	}

} }