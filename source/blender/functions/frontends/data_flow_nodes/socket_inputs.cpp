#include "nodes.hpp"

#include "RNA_access.h"

namespace FN { namespace DataFlowNodes {

	using namespace Types;

	class FloatSocketInput : public FN::TupleCallBody {
	private:
		bNodeTree *m_btree;
		bNodeSocket *m_bsocket;

	public:
		FloatSocketInput(bNodeTree *btree, bNodeSocket *bsocket)
			: m_btree(btree), m_bsocket(bsocket) {}

		virtual void call(const Tuple &UNUSED(fn_in), Tuple &fn_out) const
		{
			PointerRNA ptr;
			RNA_pointer_create(&m_btree->id, &RNA_NodeSocket, m_bsocket, &ptr);
			float value = RNA_float_get(&ptr, "value");
			fn_out.set<float>(0, value);
		}
	};

	class VectorSocketInput : public FN::TupleCallBody {
	private:
		bNodeTree *m_btree;
		bNodeSocket *m_bsocket;

	public:
		VectorSocketInput(bNodeTree *btree, bNodeSocket *bsocket)
			: m_btree(btree), m_bsocket(bsocket) {}

		virtual void call(const Tuple &UNUSED(fn_in), Tuple &fn_out) const
		{
			PointerRNA ptr;
			RNA_pointer_create(&m_btree->id, &RNA_NodeSocket, m_bsocket, &ptr);

			float vector[3];
			RNA_float_get_array(&ptr, "value", vector);
			fn_out.set<Vector>(0, Vector(vector));
		}
	};

	static Socket insert_float_socket(
		bNodeTree *btree,
		bNodeSocket *bsocket,
		SharedDataFlowGraph &graph)
	{
		auto fn = SharedFunction::New("Float Input", Signature({}, {
			OutputParameter("Value", get_float_type()),
		}));
		fn->add_body(new FloatSocketInput(btree, bsocket));
		const Node *node = graph->insert(fn);
		return node->output(0);
	}

	static Socket insert_vector_socket(
		bNodeTree *btree,
		bNodeSocket *bsocket,
		SharedDataFlowGraph &graph)
	{
		auto fn = SharedFunction::New("Vector Input", Signature({}, {
			OutputParameter("Vector", get_fvec3_type()),
		}));
		fn->add_body(new VectorSocketInput(btree, bsocket));
		const Node *node = graph->insert(fn);
		return node->output(0);
	}

	void initialize_socket_inserters()
	{
		register_socket_inserter("fn_FloatSocket", insert_float_socket);
		register_socket_inserter("fn_VectorSocket", insert_vector_socket);
	}

} }