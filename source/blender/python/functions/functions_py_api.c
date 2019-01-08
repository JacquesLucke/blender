#include <Python.h>

#include "functions_py_api.h"

static struct PyMethodDef BPy_FN_methods[] = {
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
