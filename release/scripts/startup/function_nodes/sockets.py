import bpy
from . base import DataSocket, BaseSocket
from bpy.props import *

class OperatorSocket(bpy.types.NodeSocket, BaseSocket):
    bl_idname = "fn_OperatorSocket"
    bl_label = "Operator Socket"

    def draw_color(self, context, node):
        return (0, 0, 0, 0)

class FloatSocket(bpy.types.NodeSocket, DataSocket):
    bl_idname = "fn_FloatSocket"
    bl_label = "Float Socket"
    socket_color = (0, 0.3, 0.5, 1)

    value: FloatProperty(
        name="Value",
        default=0.0,
    )

    def draw_property(self, layout, node, text):
        layout.prop(self, "value", text=text)

    def get_state(self):
        return self.value

    def restore_state(self, state):
        self.value = state

class IntegerSocket(bpy.types.NodeSocket, DataSocket):
    bl_idname = "fn_IntegerSocket"
    bl_label = "Integer Socket"
    socket_color = (0.3, 0.7, 0.5, 1)

    value: IntProperty(
        name="Value",
        default=0,
    )

    def draw_property(self, layout, node, text):
        layout.prop(self, "value", text=text)

    def get_state(self):
        return self.value

    def restore_state(self, state):
        self.value = state

class VectorSocket(bpy.types.NodeSocket, DataSocket):
    bl_idname = "fn_VectorSocket"
    bl_label = "Vector Socket"
    socket_color = (0, 0, 0.5, 1)

    value: FloatVectorProperty(
        name="Value",
        size=3,
        default=(0.0, 0.0, 0.0),
    )

    def draw_property(self, layout, node, text):
        layout.column().prop(self, "value", text=text)

    def get_state(self):
        return tuple(self.value)

    def restore_state(self, state):
        self.value = state

class CustomColoredSocket(bpy.types.NodeSocket, DataSocket):
    bl_idname = "fn_CustomColoredSocket"
    bl_label = "Custom Colored Socket"

    color: FloatVectorProperty(
        size=4,
        subtype='COLOR',
        soft_min=0.0,
        soft_max=1.0)

