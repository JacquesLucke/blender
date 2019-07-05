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

class BooleanSocket(bpy.types.NodeSocket, DataSocket):
    bl_idname = "fn_BooleanSocket"
    bl_label = "Boolean Socket"
    socket_color = (0.3, 0.3, 0.3, 1)

    value: BoolProperty(
        name="Value",
        default=False,
    )

    def draw_property(self, layout, node, text):
        layout.prop(self, "value", text=text)

    def get_state(self):
        return self.value

    def restore_state(self, state):
        self.value = state

class CustomColoredSocket(bpy.types.NodeSocket, DataSocket):
    bl_idname = "fn_CustomColoredSocket"
    bl_label = "Custom Colored Socket"

class EmitterSocket(bpy.types.NodeSocket, BaseSocket):
    bl_idname = "bp_EmitterSocket"
    bl_label = "Emitter Socket"

    def draw_color(self, context, node):
        return (1, 1, 1, 1)

class EventSocket(bpy.types.NodeSocket, BaseSocket):
    bl_idname = "bp_EventSocket"
    bl_label = "Event Socket"

    def draw_color(self, context, node):
        return (0.2, 0.8, 0.2, 1)

class ControlFlowSocket(bpy.types.NodeSocket, BaseSocket):
    bl_idname = "bp_ControlFlowSocket"
    bl_label = "Control Flow Socket"

    def draw_color(self, context, node):
        return (0.8, 0.2, 0.2, 1)
