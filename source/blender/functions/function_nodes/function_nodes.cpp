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
		bNodeSocket *m_bsocket;

	public:
		FloatSocketInput(bNodeSocket *bsocket)
			: m_bsocket(bsocket) {}

		virtual void call(const Tuple &UNUSED(fn_in), Tuple &fn_out) const
		{
			// PointerRNA ptr;
			// RNA_pointer_create(m_btree, &RNA_NodeSocket, m_bsocket, &ptr);
			// float value = RNA_float_get(&ptr, "value");
			fn_out.set<float>(0, 0.0f);
		}
	};

	class VectorSocketInput : public FN::TupleCallBody {
	private:
		bNodeSocket *m_bsocket;

	public:
		VectorSocketInput(bNodeSocket *socket)
			: m_bsocket(socket) {}

		virtual void call(const Tuple &UNUSED(fn_in), Tuple &fn_out) const
		{
			fn_out.set<Vector>(0, Vector());
		}
	};

	static SharedType &get_type_of_socket(bNodeSocket *bsocket)
	{
		if (STREQ(bsocket->idname, "fn_FloatSocket")) {
			return Types::get_float_type();
		}
		else if (STREQ(bsocket->idname, "fn_VectorSocket")) {
			return Types::get_fvec3_type();
		}
		else {
			BLI_assert(false);
			return *(SharedType *)nullptr;
		}
	}

	static const Node *get_input_node_for_socket(
		SharedDataFlowGraph &graph,
		bNodeSocket *bsocket)
	{
		SharedType &type = get_type_of_socket(bsocket);

		if (type == Types::get_float_type()) {
			auto fn = SharedFunction::New("Float Input", Signature(
				{}, {OutputParameter("Value", Types::get_float_type())}));
			fn->add_body(new FloatSocketInput(bsocket));
			return graph->insert(fn);
		}
		else if (type == Types::get_fvec3_type()) {
			auto fn = SharedFunction::New("Vector Input", Signature(
				{}, {OutputParameter("Value", Types::get_fvec3_type())}));
			fn->add_body(new VectorSocketInput(bsocket));
			return graph->insert(fn);
		}
		else {
			BLI_assert(false);
			return nullptr;
		}
	}

	static void insert_input_socket_node(
		SharedDataFlowGraph &graph,
		Socket socket,
		bNodeSocket *bsocket)
	{
		const Node *node = get_input_node_for_socket(graph, bsocket);
		graph->link(node->output(0), socket);
	}

	static SharedFunction get_output_function(const SmallTypeVector &types)
	{
		InputParameters inputs;
		for (SharedType &type : types) {
			inputs.append(InputParameter("Input", type));
		}
		return SharedFunction::New("Output Node", Signature(inputs, {}));
	}

	static void insert_output_node(
		SharedDataFlowGraph &graph,
		SocketMap &socket_map,
		bNode *bnode)
	{
		SmallTypeVector types;
		for (bNodeSocket *bsocket : bSocketList(&bnode->inputs)) {
			if (STREQ(bsocket->idname, "fn_VectorSocket")) {
				types.append(Types::get_fvec3_type());
			}
			else if (STREQ(bsocket->idname, "fn_FloatSocket")) {
				types.append(Types::get_float_type());
			}
			else {
				BLI_assert(false);
			}
		}
		SharedFunction fn = get_output_function(types);
		const Node *node = graph->insert(fn);

		bNodeSocket *bsocket;
		uint i;
		for (i = 0, bsocket = (bNodeSocket *)bnode->inputs.first;
			bsocket;
			i++, bsocket = bsocket->next)
		{
			socket_map.add(bsocket, node->input(i));
		}
	}

	static void insert_input_node(
		SharedDataFlowGraph &graph,
		SocketMap &socket_map,
		bNode *bnode)
	{
		for (bNodeSocket *bsocket : bSocketList(&bnode->outputs)) {
			const Node *node = get_input_node_for_socket(graph, bsocket);
			socket_map.add(bsocket, node->output(0));
		}
	}


	static bool is_input_node(const bNode *bnode)
	{
		return STREQ(bnode->idname, "fn_FunctionInputNode");
	}

	static bool is_output_node(const bNode *bnode)
	{
		return STREQ(bnode->idname, "fn_FunctionOutputNode");
	}

	FunctionGraph FunctionNodeTree::to_function_graph() const
	{
		SocketMap socket_map;

		SmallMap<std::string, InsertInGraphFunction> inserters;
		inserters.add("fn_AddFloatsNode", insert_add_floats_node);
		inserters.add("fn_CombineVectorNode", insert_combine_vector_node);
		inserters.add("fn_FunctionOutputNode", insert_output_node);
		inserters.add("fn_FunctionInputNode", insert_input_node);

		SharedDataFlowGraph graph = SharedDataFlowGraph::New();

		SmallSocketVector input_sockets;
		SmallSocketVector output_sockets;

		for (bNode *bnode : this->nodes()) {
			auto insert = inserters.lookup(bnode->idname);
			insert(graph, socket_map, bnode);

			if (is_input_node(bnode)) {
				for (bNodeSocket *bsocket : bSocketList(&bnode->outputs)) {
					Socket socket = socket_map.lookup(bsocket);
					input_sockets.append(socket);
				}
			}
			if (is_output_node(bnode)) {
				for (bNodeSocket *bsocket : bSocketList(&bnode->inputs)) {
					Socket socket = socket_map.lookup(bsocket);
					output_sockets.append(socket);
				}
			}
		}

		for (bNodeLink *blink : this->links()) {
			Socket from = socket_map.lookup(blink->fromsock);
			Socket to = socket_map.lookup(blink->tosock);
			graph->link(from, to);
		}

		for (bNode *bnode : this->nodes()) {
			for (bNodeSocket *bsocket : bSocketList(&bnode->inputs)) {
				Socket socket = socket_map.lookup(bsocket);
				if (!socket.is_linked()) {
					insert_input_socket_node(graph, socket, bsocket);
				}
			}
		}

		graph->freeze();
		FunctionGraph fgraph(graph, input_sockets, output_sockets);

		return fgraph;
	}

} /* FN::FunctionNodes */