import bpy
from bpy.props import *
from .. base import FunctionNode

class ObjectTransformsNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_ObjectTransformsNode"
    bl_label = "Object Transforms"

    object: PointerProperty(
        name="Object",
        type=bpy.types.Object,
    )

    def declaration(self, builder):
        builder.fixed_output("location", "Location", "Vector")

    def draw(self, layout):
        layout.prop(self, "object", text="")