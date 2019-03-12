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

class DataTypesInfo:
    def __init__(self):
        self.data_types = set()
        self.cls_by_data_type = dict()
        self.list_by_base = dict()
        self.base_by_list = dict()

    def insert_data_type(self, data_type, socket_cls):
        assert data_type not in self.data_types

        self.data_types.add(data_type)
        self.cls_by_data_type[data_type] = socket_cls

    def insert_list_relation(self, base_type, list_type):
        assert self.is_data_type(base_type)
        assert self.is_data_type(list_type)
        assert base_type not in self.list_by_base
        assert list_type not in self.base_by_list

        self.list_by_base[base_type] = list_type
        self.base_by_list[list_type] = base_type

    def is_data_type(self, data_type):
        return data_type in self.data_types

    def is_base(self, data_type):
        return data_type in self.list_by_base

    def is_list(self, data_type):
        return data_type in self.base_by_list

    def to_list(self, data_type):
        assert self.is_base(data_type)
        return self.list_by_base[data_type]

    def to_base(self, data_type):
        assert self.is_list(data_type)
        return self.base_by_list[data_type]

    def to_cls(self, data_type):
        assert self.is_data_type(data_type)
        return self.cls_by_data_type[data_type]

    def to_list_cls(self, data_type):
        return self.to_cls(self.to_list(data_type))

    def to_base_cls(self, data_type):
        return self.to_cls(self.to_base(data_type))

    def to_idname(self, data_type):
        return self.to_cls(data_type).bl_idname

    def to_list_idname(self, data_type):
        return self.to_list_cls(data_type).bl_idname

    def to_base_idname(self, data_type):
        return self.to_base_cls(data_type).bl_idname


info = DataTypesInfo()

info.insert_data_type("Float", FloatSocket)
info.insert_data_type("Vector", VectorSocket)
info.insert_data_type("Integer", VectorSocket)
info.insert_data_type("Float List", FloatListSocket)
info.insert_data_type("Vector List", VectorListSocket)

info.insert_list_relation("Float", "Float List")
info.insert_list_relation("Vector", "Vector List")