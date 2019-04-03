#include "builder.hpp"

#include "DNA_node_types.h"
#include "FN_types.hpp"
#include "util_wrappers.hpp"

#include "RNA_access.h"

#include <sstream>

#ifdef WITH_PYTHON
	#include <Python.h>

	extern "C" {
		PyObject *pyrna_struct_CreatePyObject(PointerRNA *ptr);
	}
#endif

#ifdef WITH_PYTHON
static PyObject *get_py_bnode(bNodeTree *btree, bNode *bnode)
{
	PointerRNA ptr;
	RNA_pointer_create(
		&btree->id, &RNA_Node,
		bnode, &ptr);
	return pyrna_struct_CreatePyObject(&ptr);
}
static PyObject *get_py_bsocket(bNodeTree *btree, bNodeSocket *bsocket)
{
	PointerRNA ptr;
	RNA_pointer_create(
		&btree->id, &RNA_NodeSocket,
		bsocket, &ptr);
	return pyrna_struct_CreatePyObject(&ptr);
}
#endif

namespace FN { namespace DataFlowNodes {

	class NodeSource : public SourceInfo {
	private:
		bNodeTree *m_btree;
		bNode *m_bnode;

	public:
		NodeSource(bNodeTree *btree, bNode *bnode)
			: m_btree(btree), m_bnode(bnode) {}

		std::string to_string() const override
		{
			std::stringstream ss;
			ss << "NodeTree \"" << m_btree->id.name + 2 << "\"";
			ss << " - Node \"" << m_bnode->name << "\"";
			return ss.str();
		}

		void handle_warning(std::string msg) const override
		{
#ifdef WITH_PYTHON
			PyGILState_STATE gilstate;
			gilstate = PyGILState_Ensure();

			PyObject *module = PyImport_ImportModule("function_nodes.problems");
			PyObject *globals = PyModule_GetDict(module);
			PyObject *function = PyDict_GetItemString(globals, "report_warning");

			PyObject *py_bnode = get_py_bnode(m_btree, m_bnode);
			PyObject *ret = PyObject_CallFunction(function, "Os", py_bnode, msg.c_str());
			Py_DECREF(ret);

			PyGILState_Release(gilstate);
#endif
		}
	};

	class LinkSource : public SourceInfo {
	private:
		bNodeTree *m_btree;
		bNodeLink *m_blink;

	public:
		LinkSource(bNodeTree *btree, bNodeLink *blink)
			: m_btree(btree), m_blink(blink) {}

		std::string to_string() const override
		{
			std::stringstream ss;
			ss << "NodeTree \"" << m_btree->id.name + 2 << "\"";
			ss << " - Link";
			return ss.str();
		}
	};

	Node *Builder::insert_function(SharedFunction &fn)
	{
		return m_graph->insert(fn);
	}

	Node *Builder::insert_function(SharedFunction &fn, bNodeTree *btree, bNode *bnode)
	{
		BLI_assert(btree != nullptr);
		BLI_assert(bnode != nullptr);
		NodeSource *source = m_graph->new_source_info<NodeSource>(btree, bnode);
		return m_graph->insert(fn, source);
	}

	Node *Builder::insert_function(SharedFunction &fn, bNodeTree *btree, bNodeLink *blink)
	{
		BLI_assert(btree != nullptr);
		BLI_assert(blink != nullptr);
		LinkSource *source = m_graph->new_source_info<LinkSource>(btree, blink);
		return m_graph->insert(fn, source);
	}

	void Builder::insert_link(Socket a, Socket b)
	{
		m_graph->link(a, b);
	}

	void Builder::map_socket(Socket socket, bNodeSocket *bsocket)
	{
		m_socket_map.add(bsocket, socket);
	}

	void Builder::map_sockets(Node *node, struct bNode *bnode)
	{
		BLI_assert(BLI_listbase_count(&bnode->inputs) == node->input_amount());
		BLI_assert(BLI_listbase_count(&bnode->outputs) == node->output_amount());

		uint input_index = 0;
		for (bNodeSocket *bsocket : bSocketList(&bnode->inputs)) {
			this->map_socket(node->input(input_index), bsocket);
			input_index++;
		}

		uint output_index = 0;
		for (bNodeSocket *bsocket : bSocketList(&bnode->outputs)) {
			this->map_socket(node->output(output_index), bsocket);
			output_index++;
		}
	}

	void Builder::map_data_sockets(Node *node, struct bNode *bnode, const BuilderContext &ctx)
	{
		uint input_index = 0;
		for (bNodeSocket *bsocket : bSocketList(&bnode->inputs)) {
			if (ctx.is_data_socket(bsocket)) {
				this->map_socket(node->input(input_index), bsocket);
				input_index++;
			}
		}

		uint output_index = 0;
		for (bNodeSocket *bsocket : bSocketList(&bnode->outputs)) {
			if (ctx.is_data_socket(bsocket)) {
				this->map_socket(node->output(output_index), bsocket);
				output_index++;
			}
		}
	}

	void Builder::map_input(Socket socket, struct bNode *bnode, uint index)
	{
		BLI_assert(socket.is_input());
		auto bsocket = (bNodeSocket *)BLI_findlink(&bnode->inputs, index);
		this->map_socket(socket, bsocket);
	}

	void Builder::map_output(Socket socket, struct bNode *bnode, uint index)
	{
		BLI_assert(socket.is_output());
		auto bsocket = (bNodeSocket *)BLI_findlink(&bnode->outputs, index);
		this->map_socket(socket, bsocket);
	}

	Socket Builder::lookup_socket(struct bNodeSocket *bsocket)
	{
		BLI_assert(m_socket_map.contains(bsocket));
		return m_socket_map.lookup(bsocket);
	}

	bool Builder::check_if_sockets_are_mapped(
		struct bNode *bnode,
		bSocketList bsockets,
		const BuilderContext &ctx) const
	{
		int index = 0;
		for (bNodeSocket *bsocket : bsockets) {
			if (ctx.is_data_socket(bsocket)) {
				if (!m_socket_map.contains(bsocket)) {
					std::cout << "Data Socket not mapped: " << std::endl;
					std::cout << "    Tree: " << ctx.btree()->id.name << std::endl;
					std::cout << "    Node: " << bnode->name << std::endl;
					if (bsocket->in_out == SOCK_IN) {
						std::cout << "    Input";
					}
					else {
						std::cout << "    Output";
					}
					std::cout << " Index: " << index << std::endl;
					return false;
				}
			}
			index++;
		}
		return true;
	}

	bool Builder::verify_data_sockets_mapped(struct bNode *bnode, const BuilderContext &ctx) const
	{
		return (
			this->check_if_sockets_are_mapped(bnode, bSocketList(&bnode->inputs), ctx) &&
			this->check_if_sockets_are_mapped(bnode, bSocketList(&bnode->outputs), ctx));
	}


	struct bNodeTree *BuilderContext::btree() const
	{
		return m_btree;
	}

	struct ID *BuilderContext::btree_id() const
	{
		return &m_btree->id;
	}

	bool BuilderContext::is_data_socket(bNodeSocket *bsocket) const
	{
		PointerRNA ptr;
		this->get_rna(bsocket, &ptr);
		return RNA_struct_find_property(&ptr, "data_type") != NULL;
	}

	SharedType &BuilderContext::type_by_name(const char *data_type) const
	{
		if (STREQ(data_type, "Float")) {
			return Types::get_float_type();
		}
		else if (STREQ(data_type, "Integer")) {
			return Types::get_int32_type();
		}
		else if (STREQ(data_type, "Vector")) {
			return Types::get_fvec3_type();
		}
		else if (STREQ(data_type, "Boolean")) {
			return Types::get_bool_type();
		}
		else if (STREQ(data_type, "Float List")) {
			return Types::get_float_list_type();
		}
		else if (STREQ(data_type, "Vector List")) {
			return Types::get_fvec3_list_type();
		}
		else if (STREQ(data_type, "Integer List")) {
			return Types::get_int32_list_type();
		}
		else {
			BLI_assert(false);
			return *(SharedType *)nullptr;
		}
	}

	SharedType &BuilderContext::type_of_socket(bNodeSocket *bsocket) const
	{
		std::string data_type = this->socket_type_string(bsocket);
		return this->type_by_name(data_type.c_str());
	}

	std::string BuilderContext::name_of_socket(bNode *bnode, bNodeSocket *bsocket) const
	{
#ifdef WITH_PYTHON
		PyGILState_STATE gilstate;
		gilstate = PyGILState_Ensure();

		PyObject *py_bnode = get_py_bnode(m_btree, bnode);
		PyObject *py_bsocket = get_py_bsocket(m_btree, bsocket);
		PyObject *ret = PyObject_CallMethod(py_bsocket, "get_name", "O", py_bnode);
		BLI_assert(PyUnicode_Check(ret));
		const char *name_ = PyUnicode_AsUTF8(ret);
		std::string name(name_);
		Py_DECREF(ret);

		PyGILState_Release(gilstate);
		return name;
#endif
		return bsocket->name;
	}

	void BuilderContext::get_rna(bNode *bnode, PointerRNA *ptr) const
	{
		RNA_pointer_create(
			this->btree_id(), &RNA_Node,
			bnode, ptr);
	}

	void BuilderContext::get_rna(bNodeSocket *bsocket, PointerRNA *ptr) const
	{
		RNA_pointer_create(
			this->btree_id(), &RNA_NodeSocket,
			bsocket, ptr);
	}

	SharedType &BuilderContext::type_from_rna(bNode *bnode, const char *prop_name) const
	{
		PointerRNA ptr;
		this->get_rna(bnode, &ptr);
		char type_name[64];
		RNA_string_get(&ptr, prop_name, type_name);
		return this->type_by_name(type_name);
	}

	std::string BuilderContext::socket_type_string(bNodeSocket *bsocket) const
	{
		BLI_assert(this->is_data_socket(bsocket));
		PointerRNA ptr;
		this->get_rna(bsocket, &ptr);
		char type_name[64];
		RNA_string_get(&ptr, "data_type", type_name);
		return type_name;
	}

} } /* namespace FN::DataFlowNodes */