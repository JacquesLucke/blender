/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup pythonintern
 */

#include <Python.h>

#include "BLI_profile.h"

#include "BPY_extern.h"
#include "bpy_app_profile.h"

PyDoc_STRVAR(bpy_app_profile_enable_doc, ".. function:: enable()\n");
static PyObject *bpy_app_profile_enable(PyObject *UNUSED(self),
                                        PyObject *UNUSED(args),
                                        PyObject *UNUSED(kwargs))
{
  BLI_profile_enable();
  Py_RETURN_NONE;
}

PyDoc_STRVAR(bpy_app_profile_disable_doc, ".. function:: disable()\n");
static PyObject *bpy_app_profile_disable(PyObject *UNUSED(self),
                                         PyObject *UNUSED(args),
                                         PyObject *UNUSED(kwargs))
{
  BLI_profile_disable();
  Py_RETURN_NONE;
}

PyDoc_STRVAR(bpy_app_profile_is_enabled_doc,
             ".. function:: is_enabled()\n"
             "\n"
             "   Check if profiling is enabled.\n"
             "\n"
             "   :return: True when profiling is enabled, otherwise False.\n"
             "   :rtype: bool\n");
static PyObject *bpy_app_profile_is_enabled(PyObject *UNUSED(self),
                                            PyObject *UNUSED(args),
                                            PyObject *UNUSED(kwargs))
{
  const bool enabled = BLI_profile_is_enabled();
  return PyBool_FromLong(enabled);
}

PyDoc_STRVAR(bpy_app_profile_clear_doc,
             ".. function:: clear()\n"
             "\n"
             "   Delete recorded profile.\n");
static PyObject *bpy_app_profile_clear(PyObject *UNUSED(self),
                                       PyObject *UNUSED(args),
                                       PyObject *UNUSED(kwargs))
{
  BLI_profile_clear();
  Py_RETURN_NONE;
}

static struct PyMethodDef M_AppProfile_methods[] = {
    {"enable",
     (PyCFunction)bpy_app_profile_enable,
     METH_VARARGS | METH_KEYWORDS,
     bpy_app_profile_enable_doc},
    {"disable",
     (PyCFunction)bpy_app_profile_disable,
     METH_VARARGS | METH_KEYWORDS,
     bpy_app_profile_disable_doc},
    {"is_enabled",
     (PyCFunction)bpy_app_profile_is_enabled,
     METH_VARARGS | METH_KEYWORDS,
     bpy_app_profile_is_enabled_doc},
    {"clear",
     (PyCFunction)bpy_app_profile_clear,
     METH_VARARGS | METH_KEYWORDS,
     bpy_app_profile_clear_doc},
    {nullptr, nullptr, 0, nullptr},
};

static struct PyModuleDef M_AppProfile_module_def = {
    PyModuleDef_HEAD_INIT,
    "bpy.app.profile",    /* m_name */
    nullptr,              /* m_doc */
    0,                    /* m_size */
    M_AppProfile_methods, /* m_methods */
    nullptr,              /* m_reload */
    nullptr,              /* m_traverse */
    nullptr,              /* m_clear */
    nullptr,              /* m_free */
};

PyObject *BPY_app_profile_module()
{
  PyObject *sys_modules = PyImport_GetModuleDict();
  PyObject *mod = PyModule_Create(&M_AppProfile_module_def);
  PyDict_SetItem(sys_modules, PyModule_GetNameObject(mod), mod);
  return mod;
}
