import bpy
from . base import DataSocket
from bpy.props import *

class FloatSocket(bpy.types.NodeSocket, DataSocket):
    bl_idname = "fn_FloatSocket"
    bl_label = "Float Socket"
    color = (0, 0, 0, 1)

    value: FloatProperty(
        name="Value",
        default=0.0,
    )

    def draw_property(self, layout, text, node):
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

    def draw_property(self, layout, text, node):
        layout.column().prop(self, "value", text=text)

bpy.utils.register_class(FloatSocket)
bpy.utils.register_class(VectorSocket)