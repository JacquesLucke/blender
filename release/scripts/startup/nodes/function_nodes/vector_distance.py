import bpy
from .. base import FunctionNode

class VectorDistanceNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_VectorDistanceNode"
    bl_label = "Vector Distance"

    def declaration(self, builder):
        builder.fixed_input("a", "A", "Vector")
        builder.fixed_input("b", "B", "Vector")
        builder.fixed_output("distance", "Distance", "Float")
