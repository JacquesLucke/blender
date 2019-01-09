#include <Python.h>
#include <iostream>
#include "nodecompiler/core.hpp"
#include "function_nodes/nodes/nodes.hpp"
#include "function_nodes/types/types.hpp"

#include "BLI_utildefines.h"

#include "functions_py_api.h"

namespace NC = LLVMNodeCompiler;

static int PyDict_GetNumberByString(PyObject *dict, const char *key)
{
	return PyLong_AsLong(PyDict_GetItemString(dict, key));
}

static bool PyDict_GetBoolByString(PyObject *dict, const char *key)
{
	return (PyDict_GetItemString(dict, key)) == Py_True;
}


PyDoc_STRVAR(set_function_graph_doc,
".. function:: set_function_graph(graph_json)\n"
);
static PyObject *set_function_graph(PyObject *UNUSED(self), PyObject *data)
{
	NC::DataFlowGraph graph;
	std::vector<NC::Node *> node_array;

	PyObject *nodes_py = PyDict_GetItemString(data, "nodes");
	for (int i = 0; i < PyList_Size(nodes_py); i++) {
		PyObject *node_py = PyList_GetItem(nodes_py, i);
		PyObject *node_type_py = PyDict_GetItemString(node_py, "type");

		NC::Node *node;
		if (_PyUnicode_EqualToASCIIString(node_type_py, "int_input")) {
			int number = PyDict_GetNumberByString(node_py, "number");
			node = new Int32InputNode(number);
		}
		else if (_PyUnicode_EqualToASCIIString(node_type_py, "add_ints")) {
			int amount = PyDict_GetNumberByString(node_py, "amount");
			node = new AddNumbersNode(amount, type_int32);
		}
		else {
			PyErr_SetString(PyExc_RuntimeError, "unknown node type");
			return NULL;
		}

		node_array.push_back(node);
		graph.addNode(node);
	}

	PyObject *links_py = PyDict_GetItemString(data, "links");
	for (int i = 0; i < PyList_Size(links_py); i++) {
		PyObject *link_py = PyList_GetItem(links_py, i);

		NC::Node *from_node = node_array[PyDict_GetNumberByString(link_py, "from_node")];
		NC::Node *to_node = node_array[PyDict_GetNumberByString(link_py, "to_node")];

		int from_index = PyDict_GetNumberByString(link_py, "from_index");
		int to_index = PyDict_GetNumberByString(link_py, "to_index");

		graph.addLink(from_node->Output(from_index), to_node->Input(to_index));
	}

	if (!graph.verify()) {
		PyErr_SetString(PyExc_RuntimeError, "not a valid graph");
		return NULL;
	}

	NC::SocketArraySet inputs;
	NC::SocketArraySet outputs;

	PyObject *inputs_py = PyDict_GetItemString(data, "inputs");
	for (int i = 0; i < PyList_Size(inputs_py); i++) {
		PyObject *input_py = PyList_GetItem(inputs_py, i);

		NC::Node *node = node_array[PyDict_GetNumberByString(input_py, "node")];
		bool is_output = PyDict_GetBoolByString(input_py, "is_output");
		int index = PyDict_GetNumberByString(input_py, "index");

		if (is_output) inputs.add(node->Output(index));
		else inputs.add(node->Input(index));
	}

	PyObject *outputs_py = PyDict_GetItemString(data, "outputs");
	for (int i = 0; i < PyList_Size(outputs_py); i++) {
		PyObject *output_py = PyList_GetItem(outputs_py, i);

		NC::Node *node = node_array[PyDict_GetNumberByString(output_py, "node")];
		bool is_output = PyDict_GetBoolByString(output_py, "is_output");
		int index = PyDict_GetNumberByString(output_py, "index");

		if (is_output) outputs.add(node->Output(index));
		else outputs.add(node->Input(index));
	}

	// std::string dot = graph.toDotFormat();
	// std::cout << dot << std::endl << std::endl;

	std::cout << "Inputs: " << inputs << std::endl;
	std::cout << "Outputs: " << outputs << std::endl;
	// NC::SocketSet sockets = graph.findRequiredSockets(inputs, outputs);
	// std::cout << sockets << std::endl;

	NC::CompiledLLVMFunction *function = NC::compileDataFlow(graph, inputs, outputs);
	function->printCode();

	Py_RETURN_NONE;
}

static struct PyMethodDef BPy_FN_methods[] = {
	{"set_function_graph", (PyCFunction)set_function_graph,
	 METH_O, set_function_graph_doc},
	{NULL, NULL, 0, NULL}
};

PyDoc_STRVAR(BPy_FN_doc,
"This module allows to create functions to be used by Blender.\n"
);
static struct PyModuleDef BPy_BM_module_def = {
	PyModuleDef_HEAD_INIT,
	"functions",  /* m_name */
	BPy_FN_doc,  /* m_doc */
	0,  /* m_size */
	BPy_FN_methods,  /* m_methods */
	NULL,  /* m_reload */
	NULL,  /* m_traverse */
	NULL,  /* m_clear */
	NULL,  /* m_free */
};

PyObject *BPyInit_functions(void)
{
	PyObject *mod;

	mod = PyModule_Create(&BPy_BM_module_def);

	return mod;
}
