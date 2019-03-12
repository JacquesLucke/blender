import bpy
from . base import DataSocket
from bpy.props import *

class FloatSocket(bpy.types.NodeSocket, DataSocket):
    bl_idname = "fn_FloatSocket"
    bl_label = "Float Socket"
    color = (0, 0.3, 0.5, 1)

    value: FloatProperty(
        name="Value",
        default=0.0,
    )

    def draw_property(self, layout, node, text):
        layout.prop(self, "value", text=text)

class IntegerSocket(bpy.types.NodeSocket, DataSocket):
    bl_idname = "fn_IntegerSocket"
    bl_label = "Integer Socket"
    color = (0, 0, 0, 1)

    value: IntProperty(
        name="Value",
        default=0,
    )

    def draw_property(self, layout, node, text):
        layout.prop(self, "value", text=text)

class VectorSocket(bpy.types.NodeSocket, DataSocket):
    bl_idname = "fn_VectorSocket"
    bl_label = "Vector Socket"
    color = (0, 0, 0.5, 1)

    value: FloatVectorProperty(
        name="Value",
        size=3,
        default=(0.0, 0.0, 0.0),
    )

    def draw_property(self, layout, node, text):
        layout.column().prop(self, "value", text=text)

class FloatListSocket(bpy.types.NodeSocket, DataSocket):
    bl_idname = "fn_FloatListSocket"
    bl_label = "Float List Socket"
    color = (0, 0.3, 0.5, 0.5)

class VectorListSocket(bpy.types.NodeSocket, DataSocket):
    bl_idname = "fn_VectorListSocket"
    bl_label = "Vector List Socket"
    color = (0, 0, 0.5, 0.5)

def socket_cls_from_data_type(data_type):
    return data_types[data_type]

def to_base_idname(value):
    return base_idnames[value]

def to_list_idname(value):
    return list_idnames[value]

base_idnames = {
    "Float List" : "fn_FloatSocket",
    "Vector List" : "fn_VectorSocket"
}

list_idnames = {
    "Float" : "fn_FloatListSocket",
    "Vector" : "fn_VectorListSocket",
}

list_relations = [
    (FloatSocket, FloatListSocket),
    (VectorSocket, VectorListSocket),
]

data_types = {
    "Float" : FloatSocket,
    "Integer" : IntegerSocket,
    "Vector" : VectorSocket,
    "Float List" : FloatListSocket,
    "Vector List" : VectorListSocket,
}