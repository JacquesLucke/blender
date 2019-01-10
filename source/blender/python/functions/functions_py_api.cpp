#include <Python.h>
#include <iostream>
#include "nodecompiler/core.hpp"
#include "function_nodes/nodes/nodes.hpp"
#include "function_nodes/types/types.hpp"

#include "BLI_utildefines.h"

#include "functions_py_api.h"

namespace NC = LLVMNodeCompiler;

extern "C" {
	void WM_clipboard_text_set(const char *buf, bool selection);
}

static int PyDict_GetIntByString(PyObject *dict, const char *key)
{
	return PyLong_AsLong(PyDict_GetItemString(dict, key));
}

static float PyDict_GetFloatByString(PyObject *dict, const char *key)
{
	return (float)PyFloat_AsDouble(PyDict_GetItemString(dict, key));
}

static bool PyDict_GetBoolByString(PyObject *dict, const char *key)
{
	return (PyDict_GetItemString(dict, key)) == Py_True;
}

#define PyStringEQ _PyUnicode_EqualToASCIIString


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
		if (PyStringEQ(node_type_py, "int_input")) {
			int number = PyDict_GetIntByString(node_py, "number");
			node = new Int32InputNode(number);
		}
		else if (PyStringEQ(node_type_py, "float_input")) {
			float number = PyDict_GetFloatByString(node_py, "number");
			node = new FloatInputNode(number);
		}
		else if (PyStringEQ(node_type_py, "add_ints")) {
			int amount = PyDict_GetIntByString(node_py, "amount");
			node = new AddIntegersNode(amount, type_int32);
		}
		else if (PyStringEQ(node_type_py, "add_floats")) {
			int amount = PyDict_GetIntByString(node_py, "amount");
			node = new AddFloatsNode(amount, type_float);
		}
		else if (PyStringEQ(node_type_py, "vec3_input")) {
			float x = PyDict_GetFloatByString(node_py, "x");
			float y = PyDict_GetFloatByString(node_py, "y");
			float z = PyDict_GetFloatByString(node_py, "z");
			node = new VectorInputNode(x, y, z);
		}
		else if (PyStringEQ(node_type_py, "add_vec3")) {
			int amount = PyDict_GetIntByString(node_py, "amount");
			node = new AddVectorsNode(amount);
		}
		else if (PyStringEQ(node_type_py, "pass_through_float")) {
			node = new PassThroughNode(type_float);
		}
		else if (PyStringEQ(node_type_py, "pass_through_vec3")) {
			node = new PassThroughNode(type_vec3);
		}
		else if (PyStringEQ(node_type_py, "combine_vec3")) {
			node = new CombineVectorNode();
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

		NC::Node *from_node = node_array[PyDict_GetIntByString(link_py, "from_node")];
		NC::Node *to_node = node_array[PyDict_GetIntByString(link_py, "to_node")];

		int from_index = PyDict_GetIntByString(link_py, "from_index");
		int to_index = PyDict_GetIntByString(link_py, "to_index");

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

		NC::Node *node = node_array[PyDict_GetIntByString(input_py, "node")];
		bool is_output = PyDict_GetBoolByString(input_py, "is_output");
		int index = PyDict_GetIntByString(input_py, "index");

		if (is_output) inputs.add(node->Output(index));
		else inputs.add(node->Input(index));
	}

	PyObject *outputs_py = PyDict_GetItemString(data, "outputs");
	for (int i = 0; i < PyList_Size(outputs_py); i++) {
		PyObject *output_py = PyList_GetItem(outputs_py, i);

		NC::Node *node = node_array[PyDict_GetIntByString(output_py, "node")];
		bool is_output = PyDict_GetBoolByString(output_py, "is_output");
		int index = PyDict_GetIntByString(output_py, "index");

		if (is_output) outputs.add(node->Output(index));
		else outputs.add(node->Input(index));
	}

	std::string dot = graph.toDotFormat();
	WM_clipboard_text_set(dot.c_str(), false);
	// std::cout << dot << std::endl << std::endl;

	std::cout << "Inputs: " << inputs << std::endl;
	std::cout << "Outputs: " << outputs << std::endl;
	// NC::SocketSet sockets = graph.findRequiredSockets(inputs, outputs);
	// std::cout << sockets << std::endl;

	NC::CompiledLLVMFunction *function = NC::compileDataFlow(graph, inputs, outputs);
	function->printCode();

	Vector3 input = {1, 2, 3};
	Vector3 result;
	float value = 5;

	void *ptr = function->pointer();
	((void (*)(Vector3*, float*, Vector3*))ptr)(&input, &value, &result);

	std::cout << "Result: " << result.x << " " << result.y << " " << result.z << std::endl;

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
