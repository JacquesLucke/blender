#include "function_nodes.hpp"

#include "BLI_listbase.h"

#include "BKE_node.h"
#include "BKE_idprop.h"

#include "RNA_access.h"

#include "DNA_object_types.h"

namespace FN { namespace FunctionNodes {

	using SocketMap = SmallMap<bNodeSocket *, Socket>;
	typedef void (*InsertInGraphFunction)(
		const FunctionNodeTree &tree,
		SharedDataFlowGraph &graph,
		SocketMap &map,
		bNode *bnode);


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

	static Signature signature_from_node(bNode *bnode)
	{
		InputParameters inputs;
		for (bNodeSocket *bsocket : bSocketList(&bnode->inputs)) {
			inputs.append(InputParameter(bsocket->name, get_type_of_socket(bsocket)));
		}
		OutputParameters outputs;
		for (bNodeSocket *bsocket : bSocketList(&bnode->outputs)) {
			outputs.append(OutputParameter(bsocket->name, get_type_of_socket(bsocket)));
		}
		return Signature(inputs, outputs);
	}

	static void map_node_sockets(SocketMap &socket_map, bNode *bnode, const Node *node)
	{
		uint input_index = 0;
		for (bNodeSocket *bsocket : bSocketList(&bnode->inputs)) {
			socket_map.add(bsocket, node->input(input_index));
			input_index++;
		}

		uint output_index = 0;
		for (bNodeSocket *bsocket : bSocketList(&bnode->outputs)) {
			socket_map.add(bsocket, node->output(output_index));
			output_index++;
		}
	}

	using Types::Vector;

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

	class SeparateVector : public FN::TupleCallBody {
		void call(const FN::Tuple &fn_in, FN::Tuple &fn_out) const override
		{
			Vector v = fn_in.get<Vector>(0);
			fn_out.set<float>(0, v.x);
			fn_out.set<float>(1, v.y);
			fn_out.set<float>(2, v.z);
		}
	};

	class AddFloats : public FN::TupleCallBody {
		void call(const FN::Tuple &fn_in, FN::Tuple &fn_out) const override
		{
			float a = fn_in.get<float>(0);
			float b = fn_in.get<float>(1);
			fn_out.set<float>(0, a + b);
		}
	};

	class ObjectTransforms : public FN::TupleCallBody {
	private:
		Object *m_object;

	public:
		ObjectTransforms(Object *object)
			: m_object(object) {}

		void call(const FN::Tuple &UNUSED(fn_in), FN::Tuple &fn_out) const override
		{
			if (m_object) {
				Vector position = *(Vector *)m_object->loc;
				fn_out.set<Vector>(0, position);
			}
			else {
				fn_out.set<Vector>(0, Vector());
			}
		}

		void dependencies(Dependencies &deps) const override
		{
			deps.add_object_transform_dependency(m_object);
		}
	};


	static void insert_add_floats_node(
		const FunctionNodeTree &UNUSED(tree),
		SharedDataFlowGraph &graph,
		SocketMap &socket_map,
		bNode *bnode)
	{
		auto fn = SharedFunction::New("Add Floats", signature_from_node(bnode));
		fn->add_body(new AddFloats());
		const Node *node = graph->insert(fn);
		map_node_sockets(socket_map, bnode, node);
	}

	static void insert_combine_vector_node(
		const FunctionNodeTree &UNUSED(tree),
		SharedDataFlowGraph &graph,
		SocketMap &socket_map,
		bNode *bnode)
	{
		auto fn = SharedFunction::New("Combine Vector", signature_from_node(bnode));
		fn->add_body(new CombineVector());
		const Node *node = graph->insert(fn);
		map_node_sockets(socket_map, bnode, node);
	}

	static void insert_separate_vector_node(
		const FunctionNodeTree &UNUSED(tree),
		SharedDataFlowGraph &graph,
		SocketMap &socket_map,
		bNode *bnode)
	{
		auto fn = SharedFunction::New("Separate Vector", signature_from_node(bnode));
		fn->add_body(new SeparateVector());
		const Node *node = graph->insert(fn);
		map_node_sockets(socket_map, bnode, node);
	}

	static void insert_object_transforms_node(
		const FunctionNodeTree &tree,
		SharedDataFlowGraph &graph,
		SocketMap &socket_map,
		bNode *bnode)
	{
		PointerRNA ptr;
		RNA_pointer_create(&tree.orig_tree()->id, &RNA_Node, bnode, &ptr);
		Object *object = (Object *)RNA_pointer_get(&ptr, "object").id.data;

		auto fn = SharedFunction::New("Object Transforms", signature_from_node(bnode));
		fn->add_body(new ObjectTransforms(object));
		const Node *node = graph->insert(fn);
		map_node_sockets(socket_map, bnode, node);
	}


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
		bNodeSocket *m_bsocket;

	public:
		VectorSocketInput(bNodeSocket *socket)
			: m_bsocket(socket) {}

		virtual void call(const Tuple &UNUSED(fn_in), Tuple &fn_out) const
		{
			fn_out.set<Vector>(0, Vector());
		}
	};

	static const Node *get_input_node_for_socket(
		const FunctionNodeTree &tree,
		SharedDataFlowGraph &graph,
		bNodeSocket *bsocket)
	{
		SharedType &type = get_type_of_socket(bsocket);

		if (type == Types::get_float_type()) {
			auto fn = SharedFunction::New("Float Input", Signature(
				{}, {OutputParameter("Value", Types::get_float_type())}));
			fn->add_body(new FloatSocketInput(tree.orig_tree(), bsocket));
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
		const FunctionNodeTree &tree,
		SharedDataFlowGraph &graph,
		Socket socket,
		bNodeSocket *bsocket)
	{
		const Node *node = get_input_node_for_socket(tree, graph, bsocket);
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
		const FunctionNodeTree &UNUSED(tree),
		SharedDataFlowGraph &graph,
		SocketMap &socket_map,
		bNode *bnode)
	{
		SmallTypeVector types;
		for (bNodeSocket *bsocket : bSocketList(&bnode->inputs)) {
			types.append(get_type_of_socket(bsocket));
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
		const FunctionNodeTree &tree,
		SharedDataFlowGraph &graph,
		SocketMap &socket_map,
		bNode *bnode)
	{
		for (bNodeSocket *bsocket : bSocketList(&bnode->outputs)) {
			const Node *node = get_input_node_for_socket(tree, graph, bsocket);
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
		inserters.add("fn_SeparateVectorNode", insert_separate_vector_node);
		inserters.add("fn_FunctionOutputNode", insert_output_node);
		inserters.add("fn_FunctionInputNode", insert_input_node);
		inserters.add("fn_ObjectTransformsNode", insert_object_transforms_node);

		SharedDataFlowGraph graph = SharedDataFlowGraph::New();

		SmallSocketVector input_sockets;
		SmallSocketVector output_sockets;

		for (bNode *bnode : this->nodes()) {
			auto insert = inserters.lookup(bnode->idname);
			insert(*this, graph, socket_map, bnode);

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
					insert_input_socket_node(*this, graph, socket, bsocket);
				}
			}
		}

		graph->freeze();
		FunctionGraph fgraph(graph, input_sockets, output_sockets);

		return fgraph;
	}

} } /* FN::FunctionNodes */