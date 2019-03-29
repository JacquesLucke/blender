import bpy
from .. base import FunctionNode

class CombineVectorNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_CombineVectorNode"
    bl_label = "Combine Vector"

    def declaration(self, builder):
        builder.fixed_input("x", "X", "Float")
        builder.fixed_input("y", "Y", "Float")
        builder.fixed_input("z", "Z", "Float")
        builder.fixed_output("result", "Result", "Vector")