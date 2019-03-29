import bpy
from .. base import FunctionNode

class SeparateVectorNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_SeparateVectorNode"
    bl_label = "Separate Vector"

    def declaration(self, builder):
        builder.fixed_input("vector", "Vector", "Vector")
        builder.fixed_output("x", "X", "Float")
        builder.fixed_output("y", "Y", "Float")
        builder.fixed_output("z", "Z", "Float")
