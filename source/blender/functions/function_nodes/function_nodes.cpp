#include "function_nodes.hpp"

#include "BLI_listbase.h"
#include "BKE_node.h"
#include "BKE_idprop.h"

namespace FN::FunctionNodes {

	using SocketMap = SmallMap<bNodeSocket *, Socket>;
	typedef void (*InsertInGraphFunction)(
		SharedDataFlowGraph &graph,
		SocketMap &map,
		bNode *bnode);

	class AddFloats : public FN::TupleCallBody {
		void call(const FN::Tuple &fn_in, FN::Tuple &fn_out) const override
		{
			float a = fn_in.get<float>(0);
			float b = fn_in.get<float>(1);
			fn_out.set<float>(0, a + b);
		}
	};

	static void insert_add_floats_node(
		SharedDataFlowGraph &graph,
		SocketMap &socket_map,
		bNode *bnode)
	{
		SharedType &float_ty = Types::get_float_type();

		auto fn = SharedFunction::New("Add Floats", Signature({
			InputParameter("A", float_ty),
			InputParameter("B", float_ty),
		}, {
			OutputParameter("Result", float_ty),
		}));
		fn->add_body(new AddFloats());
		const Node *node = graph->insert(fn);

		socket_map.add((bNodeSocket *)BLI_findlink(&bnode->inputs, 0), node->input(0));
		socket_map.add((bNodeSocket *)BLI_findlink(&bnode->inputs, 1), node->input(1));
		socket_map.add((bNodeSocket *)BLI_findlink(&bnode->outputs, 0), node->output(0));
	}


	struct Vector {
		float x, y, z;
	};

	class CombineVector : public FN::TupleCallBody {
		void call(const FN::Tuple &fn_in, FN::Tuple &fn_out) const override
		{
			Vector v;
			v.x = fn_in.get<float>(0);
			v.y = fn_in.get<float>(1);
			v.z = fn_in.get<float>(2);
			fn_out.set<Vector>(0, v);
		}
	};

	static void insert_combine_vector_node(
		SharedDataFlowGraph &graph,
		SocketMap &socket_map,
		bNode *bnode)
	{
		SharedType &float_ty = Types::get_float_type();
		SharedType &fvec3_ty = Types::get_fvec3_type();

		auto fn = SharedFunction::New("Combine Vector", Signature({
			InputParameter("X", float_ty),
			InputParameter("Y", float_ty),
			InputParameter("Z", float_ty),
		}, {
			OutputParameter("Result", fvec3_ty),
		}));
		fn->add_body(new CombineVector());
		const Node *node = graph->insert(fn);

		socket_map.add((bNodeSocket *)BLI_findlink(&bnode->inputs, 0), node->input(0));
		socket_map.add((bNodeSocket *)BLI_findlink(&bnode->inputs, 1), node->input(1));
		socket_map.add((bNodeSocket *)BLI_findlink(&bnode->inputs, 2), node->input(2));
		socket_map.add((bNodeSocket *)BLI_findlink(&bnode->outputs, 0), node->output(0));
	}


	class FloatSocketInput : public FN::TupleCallBody {
	private:
		bNodeSocket *m_socket;

	public:
		FloatSocketInput(bNodeSocket *socket)
			: m_socket(socket) {}

		virtual void call(const Tuple &UNUSED(fn_in), Tuple &fn_out) const
		{
			float value = IDP_Float(m_socket->prop);
			fn_out.set<float>(0, value);
		}
	};

	static void insert_input_node(
		SharedDataFlowGraph &graph,
		Socket socket,
		bNodeSocket *bsocket)
	{
		if (socket.type() == Types::get_float_type()) {
			auto fn = SharedFunction::New("Float Input", Signature(
				{}, {OutputParameter("Value", Types::get_float_type())}));
			fn->add_body(new FloatSocketInput(bsocket));
			const Node *node = graph->insert(fn);
			graph->link(node->output(0), socket);
		}
	}

	SharedDataFlowGraph FunctionNodeTree::to_data_flow_graph() const
	{
		SocketMap socket_map;

		SmallMap<std::string, InsertInGraphFunction> inserters;
		inserters.add("fn_AddFloatsNode", insert_add_floats_node);
		inserters.add("fn_CombineVectorNode", insert_combine_vector_node);

		SharedDataFlowGraph graph = SharedDataFlowGraph::New();

		for (bNode *bnode = (bNode *)m_tree->nodes.first; bnode; bnode = bnode->next) {
			auto insert = inserters.lookup(bnode->idname);
			insert(graph, socket_map, bnode);
		}

		for (bNodeLink *blink = (bNodeLink *)m_tree->links.first; blink; blink = blink->next) {
			Socket from = socket_map.lookup(blink->fromsock);
			Socket to = socket_map.lookup(blink->tosock);
			graph->link(from, to);
		}

		for (bNode *bnode = (bNode *)m_tree->nodes.first; bnode; bnode = bnode->next) {
			for (bNodeSocket *bsocket = (bNodeSocket *)bnode->inputs.first; bsocket; bsocket = bsocket->next) {
				Socket socket = socket_map.lookup(bsocket);
				if (!socket.is_linked()) {
					insert_input_node(graph, socket, bsocket);
				}
			}
		}

		return graph;
	}

} /* FN::FunctionNodes */