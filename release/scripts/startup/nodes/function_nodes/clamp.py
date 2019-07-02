import bpy
from .. base import FunctionNode

class ClampNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_ClampNode"
    bl_label = "Clamp"

    def declaration(self, builder):
        builder.fixed_input("value", "Value", "Float")
        builder.fixed_input("min", "Min", "Float")
        builder.fixed_input("max", "Max", "Float")
        builder.fixed_output("result", "Result", "Float")
