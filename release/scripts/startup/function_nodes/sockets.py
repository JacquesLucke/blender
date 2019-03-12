import bpy
from . base import DataSocket
from bpy.props import *

class FloatSocket(bpy.types.NodeSocket, DataSocket):
    bl_idname = "fn_FloatSocket"
    bl_label = "Float Socket"

    value: FloatProperty(
        name="Value",
        default=0.0,
    )

    def draw_property(self, layout, node, text):
        layout.prop(self, "value", text=text)

    def draw_color(self, context, node):
        return (0, 0.3, 0.5, 1)

class IntegerSocket(bpy.types.NodeSocket, DataSocket):
    bl_idname = "fn_IntegerSocket"
    bl_label = "Integer Socket"

    value: IntProperty(
        name="Value",
        default=0,
    )

    def draw_property(self, layout, node, text):
        layout.prop(self, "value", text=text)

    def draw_color(self, context, node):
        return (0, 0.7, 0.5, 1)

class VectorSocket(bpy.types.NodeSocket, DataSocket):
    bl_idname = "fn_VectorSocket"
    bl_label = "Vector Socket"

    value: FloatVectorProperty(
        name="Value",
        size=3,
        default=(0.0, 0.0, 0.0),
    )

    def draw_property(self, layout, node, text):
        layout.column().prop(self, "value", text=text)

    def draw_color(self, context, node):
        return (0, 0, 0.5, 1)

class CustomColoredSocket(bpy.types.NodeSocket, DataSocket):
    bl_idname = "fn_CustomColoredSocket"
    bl_label = "Custom Colored Socket"

    color: FloatVectorProperty(
        size=4,
        subtype='COLOR',
        soft_min=0.0,
        soft_max=1.0)

    def draw_color(self, context, node):
        return self.color

class SocketBuilder:
    def build(self, node_sockets, name):
        raise NotImplementedError()

class UniqueSocketBuilder(SocketBuilder):
    def __init__(self, socket_cls):
        self.socket_cls = socket_cls

    def build(self, node_sockets, name):
        return node_sockets.new(self.socket_cls.bl_idname, name)

class ColoredSocketBuilder(SocketBuilder):
    def __init__(self, color):
        self.color = color

    def build(self, node_sockets, name):
        socket = node_sockets.new("fn_CustomColoredSocket", name)
        socket.color = self.color
        return socket

class DataTypesInfo:
    def __init__(self):
        self.data_types = set()
        self.builder_by_data_type = dict()
        self.list_by_base = dict()
        self.base_by_list = dict()

    def insert_data_type(self, data_type, builder):
        assert data_type not in self.data_types
        assert isinstance(builder, SocketBuilder)

        self.data_types.add(data_type)
        self.builder_by_data_type[data_type] = builder

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

    def to_builder(self, data_type):
        assert self.is_data_type(data_type)
        return self.builder_by_data_type[data_type]

    def build(self, data_type, node_sockets, name):
        builder = self.to_builder(data_type)
        socket = builder.build(node_sockets, name)
        socket.data_type = data_type
        return socket


info = DataTypesInfo()

info.insert_data_type("Float", UniqueSocketBuilder(FloatSocket))
info.insert_data_type("Vector", UniqueSocketBuilder(VectorSocket))
info.insert_data_type("Integer", UniqueSocketBuilder(VectorSocket))
info.insert_data_type("Float List", ColoredSocketBuilder((0, 0.3, 0.5, 0.5)))
info.insert_data_type("Vector List", ColoredSocketBuilder((0, 0, 0.5, 0.5)))

info.insert_list_relation("Float", "Float List")
info.insert_list_relation("Vector", "Vector List")