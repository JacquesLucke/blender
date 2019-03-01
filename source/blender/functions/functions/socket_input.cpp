#include "socket_input.hpp"

#include "FN_types.hpp"
#include "DNA_node_types.h"

namespace FN { namespace Functions {

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

	SharedFunction float_socket_input(
		struct bNodeTree *btree,
		struct bNodeSocket *bsocket)
	{
		auto fn = SharedFunction::New("Float Input", Signature({}, {
			OutputParameter("Value", get_float_type()),
		}));
		fn->add_body(new FloatSocketInput(btree, bsocket));
		return fn;
	}


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

	SharedFunction vector_socket_input(
		struct bNodeTree *btree,
		struct bNodeSocket *bsocket)
	{
		auto fn = SharedFunction::New("Vector Input", Signature({}, {
			OutputParameter("Vector", get_fvec3_type()),
		}));
		fn->add_body(new VectorSocketInput(btree, bsocket));
		return fn;
	}

} } /* namespace FN::Functions */